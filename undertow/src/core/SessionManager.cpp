#include "SessionManager.h"

#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/hex.hpp>
#include <libtorrent/span.hpp>

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QThread>
#include <QSet>

namespace lt = libtorrent;

namespace {

QString idFromStatus(const lt::torrent_status &st)
{
    if (!st.info_hashes.has_v1() && !st.info_hashes.has_v2()) return QString();
    const lt::sha1_hash hash = st.info_hashes.get_best();
    // NOTE: to_hex(std::string const&) is flagged deprecated upstream in favor of a
    // span<char const> overload that isn't actually exposed by this build of
    // libtorrent-rasterbar; the string overload remains fully functional.
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_GCC("-Wdeprecated-declarations")
    return QString::fromStdString(lt::to_hex(hash.to_string()));
    QT_WARNING_POP
}

QString idFromHandle(const lt::torrent_handle &h)
{
    if (!h.is_valid()) return QString();
    return idFromStatus(h.status());
}

TorrentState mapState(const lt::torrent_status &st)
{
    if (st.errc) return TorrentState::Error;

    const bool paused = bool(st.flags & lt::torrent_flags::paused);
    const bool autoManaged = bool(st.flags & lt::torrent_flags::auto_managed);

    if (paused && !autoManaged) return TorrentState::Paused;

    switch (st.state) {
        case lt::torrent_status::checking_files:
            return TorrentState::CheckingFiles;
        case lt::torrent_status::downloading_metadata:
            return TorrentState::DownloadingMetadata;
        case lt::torrent_status::downloading:
            return (paused && autoManaged) ? TorrentState::QueuedDownload : TorrentState::Downloading;
        case lt::torrent_status::finished:
            return (paused && autoManaged) ? TorrentState::QueuedSeed : TorrentState::Finished;
        case lt::torrent_status::seeding:
            return (paused && autoManaged) ? TorrentState::QueuedSeed : TorrentState::Seeding;
        case lt::torrent_status::checking_resume_data:
            return TorrentState::CheckingResumeData;
        default:
            return TorrentState::Downloading;
    }
}

void writeResumeFileFor(const QString &path, const lt::add_torrent_params &params)
{
    std::vector<char> buf = lt::write_resume_data_buf(params);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(buf.data(), qint64(buf.size()));
    }
}

} // namespace

struct SessionManager::Impl {
    std::unique_ptr<lt::session> session;
    QMap<QString, lt::torrent_handle> handles;
    QMap<QString, QString> categories;
    QMap<QString, qint64> addedTimes;
    QStringList categoryList;
    QSet<QString> pendingRemoval;
    QVector<TorrentRecord> cache;
    bool dhtEnabled = true;
    bool lsdEnabled = true;
    bool upnpEnabled = true;
    bool natpmpEnabled = true;
};

SessionManager::SessionManager(QObject *parent) : QObject(parent) {}

SessionManager::~SessionManager()
{
    shutdown();
}

QString SessionManager::stateDir() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + QStringLiteral("/.local/share/Undertow");
    return base;
}

QString SessionManager::resumeFilePath(const QString &id) const
{
    return stateDir() + QStringLiteral("/torrents/") + id + QStringLiteral(".fastresume");
}

void SessionManager::loadMeta()
{
    if (!d) return;
    QFile f(stateDir() + QStringLiteral("/meta.json"));
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return;
    QJsonObject root = doc.object();

    const QJsonObject torrentsObj = root.value(QStringLiteral("torrents")).toObject();
    for (auto it = torrentsObj.begin(); it != torrentsObj.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        d->categories[it.key()] = entry.value(QStringLiteral("category")).toString();
        d->addedTimes[it.key()] = qint64(entry.value(QStringLiteral("addedTime")).toDouble());
    }

    const QJsonArray cats = root.value(QStringLiteral("categories")).toArray();
    for (const QJsonValue &v : cats) {
        const QString name = v.toString();
        if (!name.isEmpty() && !d->categoryList.contains(name)) d->categoryList.append(name);
    }

    m_defaultSavePath = root.value(QStringLiteral("defaultSavePath")).toString(m_defaultSavePath);
    m_listenPort = root.value(QStringLiteral("listenPort")).toInt(m_listenPort);
    m_globalDownLimit = root.value(QStringLiteral("globalDownLimit")).toInt(0);
    m_globalUpLimit = root.value(QStringLiteral("globalUpLimit")).toInt(0);
    d->dhtEnabled = root.value(QStringLiteral("dht")).toBool(true);
    d->lsdEnabled = root.value(QStringLiteral("lsd")).toBool(true);
    d->upnpEnabled = root.value(QStringLiteral("upnp")).toBool(true);
    d->natpmpEnabled = root.value(QStringLiteral("natpmp")).toBool(true);
}

void SessionManager::saveMeta()
{
    if (!d) return;
    QJsonObject root;
    QJsonObject torrentsObj;
    for (auto it = d->categories.begin(); it != d->categories.end(); ++it) {
        QJsonObject entry;
        entry[QStringLiteral("category")] = it.value();
        entry[QStringLiteral("addedTime")] = double(d->addedTimes.value(it.key()));
        torrentsObj[it.key()] = entry;
    }
    root[QStringLiteral("torrents")] = torrentsObj;

    QJsonArray cats;
    for (const QString &c : d->categoryList) cats.append(c);
    root[QStringLiteral("categories")] = cats;

    root[QStringLiteral("defaultSavePath")] = m_defaultSavePath;
    root[QStringLiteral("listenPort")] = m_listenPort;
    root[QStringLiteral("globalDownLimit")] = m_globalDownLimit;
    root[QStringLiteral("globalUpLimit")] = m_globalUpLimit;
    root[QStringLiteral("dht")] = d->dhtEnabled;
    root[QStringLiteral("lsd")] = d->lsdEnabled;
    root[QStringLiteral("upnp")] = d->upnpEnabled;
    root[QStringLiteral("natpmp")] = d->natpmpEnabled;

    QFile f(stateDir() + QStringLiteral("/meta.json"));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

void SessionManager::loadPersistedTorrents()
{
    QDir dir(stateDir() + QStringLiteral("/torrents"));
    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.fastresume"), QDir::Files);
    for (const QString &fname : files) {
        QFile f(dir.filePath(fname));
        if (!f.open(QIODevice::ReadOnly)) continue;
        QByteArray data = f.readAll();
        f.close();
        if (data.isEmpty()) continue;

        try {
            lt::add_torrent_params params =
                lt::read_resume_data(lt::span<char const>(data.constData(), data.size()));
            lt::error_code ec;
            d->session->add_torrent(params, ec);
        } catch (const std::exception &) {
            continue;
        }
    }
}

void SessionManager::start()
{
    d = std::make_unique<Impl>();

    QDir().mkpath(stateDir());
    QDir().mkpath(stateDir() + QStringLiteral("/torrents"));

    loadMeta();

    if (m_defaultSavePath.isEmpty()) {
        m_defaultSavePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (m_defaultSavePath.isEmpty())
            m_defaultSavePath = QDir::homePath() + QStringLiteral("/Downloads");
    }
    QDir().mkpath(m_defaultSavePath);

    lt::settings_pack pack;
    pack.set_str(lt::settings_pack::user_agent, std::string("Undertow/1.0.0"));
    pack.set_str(lt::settings_pack::listen_interfaces,
        QStringLiteral("0.0.0.0:%1,[::]:%1").arg(m_listenPort).toStdString());
    pack.set_int(lt::settings_pack::alert_mask,
        int(lt::alert_category::status | lt::alert_category::error
            | lt::alert_category::storage | lt::alert_category::tracker));
    pack.set_bool(lt::settings_pack::enable_dht, d->dhtEnabled);
    pack.set_bool(lt::settings_pack::enable_lsd, d->lsdEnabled);
    pack.set_bool(lt::settings_pack::enable_upnp, d->upnpEnabled);
    pack.set_bool(lt::settings_pack::enable_natpmp, d->natpmpEnabled);
    pack.set_int(lt::settings_pack::active_downloads, 12);
    pack.set_int(lt::settings_pack::active_seeds, 12);
    if (m_globalDownLimit > 0) pack.set_int(lt::settings_pack::download_rate_limit, m_globalDownLimit);
    if (m_globalUpLimit > 0) pack.set_int(lt::settings_pack::upload_rate_limit, m_globalUpLimit);

    d->session = std::make_unique<lt::session>(std::move(pack));

    loadPersistedTorrents();
}

void SessionManager::shutdown()
{
    if (!d || !d->session) return;

    std::vector<lt::torrent_handle> handles = d->session->get_torrents();
    int outstanding = 0;
    for (auto &h : handles) {
        if (!h.is_valid()) continue;
        h.save_resume_data();
        ++outstanding;
    }

    QElapsedTimer timer;
    timer.start();
    while (outstanding > 0 && timer.elapsed() < 8000) {
        std::vector<lt::alert*> alerts;
        d->session->pop_alerts(&alerts);
        for (lt::alert *a : alerts) {
            if (auto *srd = lt::alert_cast<lt::save_resume_data_alert>(a)) {
                QString id = idFromHandle(srd->handle);
                if (!id.isEmpty()) writeResumeFileFor(resumeFilePath(id), srd->params);
                --outstanding;
            } else if (lt::alert_cast<lt::save_resume_data_failed_alert>(a)) {
                --outstanding;
            }
        }
        if (outstanding > 0) QThread::msleep(50);
    }

    saveMeta();
    d->session.reset();
    d->handles.clear();
}

bool SessionManager::addTorrentFromFile(const QString &torrentPath, const QString &savePath,
                                         const QString &category, bool startPaused, QString *errorOut)
{
    if (!d || !d->session) {
        if (errorOut) *errorOut = tr("A sessão do torrent ainda não foi iniciada.");
        return false;
    }

    lt::add_torrent_params params;
    try {
        params = lt::load_torrent_file(torrentPath.toStdString());
    } catch (const std::exception &e) {
        if (errorOut) *errorOut = QString::fromStdString(e.what());
        return false;
    }

    params.save_path = savePath.toStdString();
    if (startPaused) {
        params.flags |= lt::torrent_flags::paused;
        params.flags &= ~lt::torrent_flags::auto_managed;
    } else {
        params.flags &= ~lt::torrent_flags::paused;
        params.flags |= lt::torrent_flags::auto_managed;
    }

    lt::error_code ec;
    lt::torrent_handle h = d->session->add_torrent(params, ec);
    if (ec || !h.is_valid()) {
        if (errorOut) *errorOut = QString::fromStdString(ec.message());
        return false;
    }

    const QString id = idFromHandle(h);
    d->handles[id] = h;
    d->categories[id] = category;
    d->addedTimes[id] = QDateTime::currentSecsSinceEpoch();
    if (!category.isEmpty() && !d->categoryList.contains(category)) d->categoryList.append(category);
    saveMeta();
    return true;
}

bool SessionManager::addMagnet(const QString &magnetUri, const QString &savePath,
                                const QString &category, bool startPaused, QString *errorOut)
{
    if (!d || !d->session) {
        if (errorOut) *errorOut = tr("A sessão do torrent ainda não foi iniciada.");
        return false;
    }

    lt::error_code parseEc;
    lt::add_torrent_params params = lt::parse_magnet_uri(magnetUri.toStdString(), parseEc);
    if (parseEc) {
        if (errorOut) *errorOut = QString::fromStdString(parseEc.message());
        return false;
    }

    params.save_path = savePath.toStdString();
    if (startPaused) {
        params.flags |= lt::torrent_flags::paused;
        params.flags &= ~lt::torrent_flags::auto_managed;
    } else {
        params.flags &= ~lt::torrent_flags::paused;
        params.flags |= lt::torrent_flags::auto_managed;
    }

    lt::error_code ec;
    lt::torrent_handle h = d->session->add_torrent(params, ec);
    if (ec || !h.is_valid()) {
        if (errorOut) *errorOut = QString::fromStdString(ec.message());
        return false;
    }

    const QString id = idFromHandle(h);
    d->handles[id] = h;
    d->categories[id] = category;
    d->addedTimes[id] = QDateTime::currentSecsSinceEpoch();
    if (!category.isEmpty() && !d->categoryList.contains(category)) d->categoryList.append(category);
    saveMeta();
    return true;
}

SessionManager::TorrentPreview SessionManager::previewTorrentFile(const QString &torrentPath)
{
    TorrentPreview preview;
    try {
        lt::add_torrent_params params = lt::load_torrent_file(torrentPath.toStdString());
        if (!params.ti) {
            preview.error = tr("Arquivo .torrent inválido ou corrompido.");
            return preview;
        }
        const lt::torrent_info &ti = *params.ti;
        preview.valid = true;
        preview.name = QString::fromStdString(ti.name());
        preview.size = ti.total_size();

        const lt::file_storage &fs = ti.files();
        for (auto const &idx : fs.file_range()) {
            TorrentFileEntry entry;
            entry.path = QString::fromStdString(fs.file_path(idx));
            entry.size = fs.file_size(idx);
            entry.priority = 4;
            preview.files.push_back(entry);
        }
    } catch (const std::exception &e) {
        preview.error = QString::fromStdString(e.what());
    }
    return preview;
}

void SessionManager::pauseTorrent(const QString &id)
{
    if (!d || !d->handles.contains(id)) return;
    lt::torrent_handle h = d->handles.value(id);
    h.unset_flags(lt::torrent_flags::auto_managed);
    h.pause();
}

void SessionManager::resumeTorrent(const QString &id)
{
    if (!d || !d->handles.contains(id)) return;
    lt::torrent_handle h = d->handles.value(id);
    h.set_flags(lt::torrent_flags::auto_managed);
    h.resume();
}

void SessionManager::removeTorrent(const QString &id, bool deleteFiles)
{
    if (!d || !d->handles.contains(id)) return;
    lt::torrent_handle h = d->handles.value(id);
    d->pendingRemoval.insert(id);
    d->session->remove_torrent(h, deleteFiles ? lt::session_handle::delete_files : lt::remove_flags_t{});
    d->handles.remove(id);
    QFile::remove(resumeFilePath(id));
    d->categories.remove(id);
    d->addedTimes.remove(id);
    saveMeta();
}

void SessionManager::forceRecheck(const QString &id)
{
    if (!d || !d->handles.contains(id)) return;
    d->handles.value(id).force_recheck();
}

void SessionManager::forceReannounce(const QString &id)
{
    if (!d || !d->handles.contains(id)) return;
    d->handles.value(id).force_reannounce();
}

void SessionManager::setCategory(const QString &id, const QString &category)
{
    if (!d) return;
    d->categories[id] = category;
    if (!category.isEmpty() && !d->categoryList.contains(category)) d->categoryList.append(category);
    saveMeta();
}

void SessionManager::setFilePriorities(const QString &id, const QVector<int> &priorities)
{
    if (!d || !d->handles.contains(id)) return;
    std::vector<lt::download_priority_t> p;
    p.reserve(std::size_t(priorities.size()));
    for (int v : priorities) p.emplace_back(static_cast<std::uint8_t>(v));
    d->handles.value(id).prioritize_files(p);
}

void SessionManager::setTorrentSpeedLimits(const QString &id, int downLimitBps, int upLimitBps)
{
    if (!d || !d->handles.contains(id)) return;
    lt::torrent_handle h = d->handles.value(id);
    h.set_download_limit(downLimitBps);
    h.set_upload_limit(upLimitBps);
}

QVector<TorrentRecord> SessionManager::torrents() const
{
    return d ? d->cache : QVector<TorrentRecord>();
}

QVector<TorrentFileEntry> SessionManager::filesForTorrent(const QString &id) const
{
    QVector<TorrentFileEntry> result;
    if (!d || !d->handles.contains(id)) return result;
    lt::torrent_handle h = d->handles.value(id);
    if (!h.is_valid()) return result;

    std::shared_ptr<const lt::torrent_info> ti = h.torrent_file();
    if (!ti) return result;

    const lt::file_storage &fs = ti->files();
    std::vector<std::int64_t> progress = h.file_progress();
    std::vector<lt::download_priority_t> priorities = h.get_file_priorities();

    for (auto const &idx : fs.file_range()) {
        TorrentFileEntry entry;
        entry.path = QString::fromStdString(fs.file_path(idx));
        entry.size = fs.file_size(idx);
        const int i = static_cast<int>(idx);
        entry.downloaded = (i >= 0 && i < int(progress.size())) ? progress[std::size_t(i)] : 0;
        entry.priority = (i >= 0 && i < int(priorities.size())) ? int(priorities[std::size_t(i)]) : 4;
        result.push_back(entry);
    }
    return result;
}

QVector<PeerRecord> SessionManager::peersForTorrent(const QString &id) const
{
    QVector<PeerRecord> result;
    if (!d || !d->handles.contains(id)) return result;
    lt::torrent_handle h = d->handles.value(id);
    if (!h.is_valid()) return result;

    std::vector<lt::peer_info> peers;
    h.get_peer_info(peers);
    result.reserve(int(peers.size()));
    for (auto const &p : peers) {
        PeerRecord rec;
        rec.address = QString::fromStdString(p.ip.address().to_string())
                      + QStringLiteral(":") + QString::number(p.ip.port());
        rec.client = QString::fromStdString(p.client);
        rec.progress = p.progress;
        rec.downloadRate = p.down_speed;
        rec.uploadRate = p.up_speed;

        QStringList flags;
        if (p.flags & lt::peer_info::seed) flags << tr("Seed");
        if (p.flags & lt::peer_info::interesting) flags << tr("Interessado");
        if (p.flags & lt::peer_info::choked) flags << tr("Bloqueado");
        if (p.flags & lt::peer_info::utp_socket) flags << QStringLiteral("uTP");
        rec.flags = flags.join(QStringLiteral(", "));

        result.push_back(rec);
    }
    return result;
}

QVector<TrackerRecord> SessionManager::trackersForTorrent(const QString &id) const
{
    QVector<TrackerRecord> result;
    if (!d || !d->handles.contains(id)) return result;
    lt::torrent_handle h = d->handles.value(id);
    if (!h.is_valid()) return result;

    for (auto const &ae : h.trackers()) {
        TrackerRecord rec;
        rec.url = QString::fromStdString(ae.url);
        rec.tier = ae.tier;
        rec.status = tr("Configurado");
        result.push_back(rec);
    }
    return result;
}

QString SessionManager::savePathForTorrent(const QString &id) const
{
    if (!d || !d->handles.contains(id)) return QString();
    return QString::fromStdString(d->handles.value(id).status().save_path);
}

QString SessionManager::magnetLinkForTorrent(const QString &id) const
{
    if (!d || !d->handles.contains(id)) return QString();
    return QString::fromStdString(lt::make_magnet_uri(d->handles.value(id)));
}

QStringList SessionManager::categories() const
{
    return d ? d->categoryList : QStringList();
}

void SessionManager::createCategory(const QString &name)
{
    if (!d) return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || d->categoryList.contains(trimmed)) return;
    d->categoryList.append(trimmed);
    saveMeta();
}

void SessionManager::deleteCategory(const QString &name)
{
    if (!d) return;
    d->categoryList.removeAll(name);
    for (auto it = d->categories.begin(); it != d->categories.end(); ++it) {
        if (it.value() == name) it.value().clear();
    }
    saveMeta();
}

void SessionManager::setGlobalSpeedLimits(int downLimitBps, int upLimitBps)
{
    m_globalDownLimit = downLimitBps;
    m_globalUpLimit = upLimitBps;
    saveMeta();
    if (!d || !d->session) return;
    lt::settings_pack pack;
    pack.set_int(lt::settings_pack::download_rate_limit, downLimitBps);
    pack.set_int(lt::settings_pack::upload_rate_limit, upLimitBps);
    d->session->apply_settings(pack);
}

void SessionManager::setListenPort(int port)
{
    m_listenPort = port;
    saveMeta();
    if (!d || !d->session) return;
    lt::settings_pack pack;
    pack.set_str(lt::settings_pack::listen_interfaces,
        QStringLiteral("0.0.0.0:%1,[::]:%1").arg(port).toStdString());
    d->session->apply_settings(pack);
}

void SessionManager::setProtocolsEnabled(bool dht, bool lsd, bool upnp, bool natpmp)
{
    if (!d) return;
    d->dhtEnabled = dht;
    d->lsdEnabled = lsd;
    d->upnpEnabled = upnp;
    d->natpmpEnabled = natpmp;
    saveMeta();
    if (!d->session) return;
    lt::settings_pack pack;
    pack.set_bool(lt::settings_pack::enable_dht, dht);
    pack.set_bool(lt::settings_pack::enable_lsd, lsd);
    pack.set_bool(lt::settings_pack::enable_upnp, upnp);
    pack.set_bool(lt::settings_pack::enable_natpmp, natpmp);
    d->session->apply_settings(pack);
}

void SessionManager::setDefaultSavePath(const QString &path)
{
    m_defaultSavePath = path;
    saveMeta();
}

void SessionManager::pollAlerts()
{
    if (!d || !d->session) return;

    std::vector<lt::alert*> alerts;
    d->session->pop_alerts(&alerts);

    for (lt::alert *a : alerts) {
        if (auto *fin = lt::alert_cast<lt::torrent_finished_alert>(a)) {
            if (fin->handle.is_valid()) {
                emit torrentFinished(QString::fromStdString(fin->handle.status().name));
                fin->handle.save_resume_data();
            }
        } else if (auto *err = lt::alert_cast<lt::torrent_error_alert>(a)) {
            const QString name = err->handle.is_valid()
                ? QString::fromStdString(err->handle.status().name) : QString();
            emit torrentErrorOccurred(name, QString::fromStdString(err->message()));
        } else if (auto *srd = lt::alert_cast<lt::save_resume_data_alert>(a)) {
            const QString id = idFromHandle(srd->handle);
            if (!id.isEmpty()) writeResumeFileFor(resumeFilePath(id), srd->params);
        }
    }

    QVector<TorrentRecord> newCache;
    qint64 downRate = 0, upRate = 0, downloaded = 0, uploaded = 0;

    for (lt::torrent_handle &h : d->session->get_torrents()) {
        if (!h.is_valid()) continue;
        lt::torrent_status st = h.status();
        const QString id = idFromStatus(st);
        if (id.isEmpty() || d->pendingRemoval.contains(id)) continue;

        d->handles[id] = h;
        if (!d->addedTimes.contains(id)) d->addedTimes[id] = QDateTime::currentSecsSinceEpoch();

        TorrentRecord rec;
        rec.id = id;
        rec.name = st.name.empty() ? tr("(obtendo metadados\u2026)") : QString::fromStdString(st.name);
        rec.savePath = QString::fromStdString(st.save_path);
        rec.category = d->categories.value(id);
        rec.totalSize = st.total_wanted;
        rec.totalDone = st.total_wanted_done;
        rec.totalDownloaded = st.all_time_download;
        rec.totalUploaded = st.all_time_upload;
        rec.progress = st.progress;
        rec.downloadRate = st.download_payload_rate;
        rec.uploadRate = st.upload_payload_rate;
        rec.numPeersConnected = st.num_peers;
        rec.numSeedsConnected = st.num_seeds;
        rec.numPeersSwarm = st.num_incomplete;
        rec.numSeedsSwarm = st.num_complete;
        rec.state = mapState(st);
        rec.errorMessage = st.errc ? QString::fromStdString(st.errc.message()) : QString();
        rec.addedTime = d->addedTimes.value(id);

        if (rec.downloadRate > 0 && rec.totalSize > rec.totalDone) {
            rec.etaSeconds = qint64(rec.totalSize - rec.totalDone) / rec.downloadRate;
        } else {
            rec.etaSeconds = -1;
        }

        downRate += rec.downloadRate;
        upRate += rec.uploadRate;
        downloaded += rec.totalDownloaded;
        uploaded += rec.totalUploaded;

        newCache.push_back(rec);
    }

    d->cache = newCache;
    m_totalDownRate = downRate;
    m_totalUpRate = upRate;
    m_totalDownloaded = downloaded;
    m_totalUploaded = uploaded;

    emit torrentsChanged();
}
