#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QStringList>
#include <QPair>
#include <memory>

#include "TorrentRecord.h"

// One entry in a torrent's file list (used by the Files tab and by the
// Add Torrent preview).
struct TorrentFileEntry {
    QString path;
    qint64 size = 0;
    qint64 downloaded = 0;
    int priority = 4;     // 0=skip, 1=low, 4=normal, 7=high
};

struct PeerRecord {
    QString address;
    QString client;
    float progress = 0.0f;
    int downloadRate = 0;
    int uploadRate = 0;
    QString flags;
};

struct TrackerRecord {
    QString url;
    int tier = 0;
    QString status;
};

// SessionManager owns the single libtorrent session for the process and
// is the only class in the project that includes libtorrent headers
// (see SessionManager.cpp). Everything above it - models, dialogs, the
// main window - talks exclusively in Qt types and the plain structs
// declared here and in TorrentRecord.h.
//
// Threading model: libtorrent's session is internally asynchronous, but
// this wrapper is deliberately single-threaded from the caller's point of
// view. MainWindow drives a QTimer that calls pollAlerts() roughly once a
// second; that call drains the alert queue and rebuilds the status cache
// that torrents()/filesForTorrent()/etc. read from. All public methods are
// expected to be called from the GUI thread.
class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager() override;

    void start();
    void shutdown();

    bool addTorrentFromFile(const QString &torrentPath, const QString &savePath,
                             const QString &category, bool startPaused, QString *errorOut = nullptr);
    bool addMagnet(const QString &magnetUri, const QString &savePath,
                   const QString &category, bool startPaused, QString *errorOut = nullptr);

    struct TorrentPreview {
        bool valid = false;
        QString name;
        qint64 size = 0;
        QVector<TorrentFileEntry> files;
        QString error;
    };
    static TorrentPreview previewTorrentFile(const QString &torrentPath);

    void pauseTorrent(const QString &id);
    void resumeTorrent(const QString &id);
    void removeTorrent(const QString &id, bool deleteFiles);
    void forceRecheck(const QString &id);
    void forceReannounce(const QString &id);
    void setCategory(const QString &id, const QString &category);
    void setFilePriorities(const QString &id, const QVector<int> &priorities);
    void setTorrentSpeedLimits(const QString &id, int downLimitBps, int upLimitBps);

    QVector<TorrentRecord> torrents() const;
    QVector<TorrentFileEntry> filesForTorrent(const QString &id) const;
    QVector<PeerRecord> peersForTorrent(const QString &id) const;
    QVector<TrackerRecord> trackersForTorrent(const QString &id) const;
    QString savePathForTorrent(const QString &id) const;
    QString magnetLinkForTorrent(const QString &id) const;

    QStringList categories() const;
    void createCategory(const QString &name);
    void deleteCategory(const QString &name);

    void setGlobalSpeedLimits(int downLimitBps, int upLimitBps);
    void setListenPort(int port);
    void setProtocolsEnabled(bool dht, bool lsd, bool upnp, bool natpmp);
    void setDefaultSavePath(const QString &path);
    QString defaultSavePath() const { return m_defaultSavePath; }
    int listenPort() const { return m_listenPort; }
    QPair<int,int> globalSpeedLimits() const { return {m_globalDownLimit, m_globalUpLimit}; }

    qint64 totalDownloadRate() const { return m_totalDownRate; }
    qint64 totalUploadRate() const { return m_totalUpRate; }
    qint64 totalDownloaded() const { return m_totalDownloaded; }
    qint64 totalUploaded() const { return m_totalUploaded; }

public slots:
    void pollAlerts();

signals:
    void torrentsChanged();
    void torrentFinished(const QString &name);
    void torrentErrorOccurred(const QString &name, const QString &error);

private:
    struct Impl;
    std::unique_ptr<Impl> d;

    QString m_defaultSavePath;
    int m_listenPort = 6881;
    int m_globalDownLimit = 0;
    int m_globalUpLimit = 0;
    qint64 m_totalDownRate = 0;
    qint64 m_totalUpRate = 0;
    qint64 m_totalDownloaded = 0;
    qint64 m_totalUploaded = 0;

    QString stateDir() const;
    QString resumeFilePath(const QString &id) const;
    void loadMeta();
    void saveMeta();
    void loadPersistedTorrents();
};
