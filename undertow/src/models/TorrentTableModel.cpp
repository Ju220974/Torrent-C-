#include "TorrentTableModel.h"
#include "../core/Utils.h"

#include <QColor>
#include <QMap>

TorrentTableModel::TorrentTableModel(QObject *parent) : QAbstractTableModel(parent) {}

int TorrentTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

int TorrentTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant TorrentTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size()) return QVariant();
    const TorrentRecord &r = m_records[index.row()];

    if (role == Qt::UserRole && index.column() == ColProgress) {
        return r.progress;
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
            case ColName:
            case ColCategory:
                return QVariant(Qt::AlignVCenter | Qt::AlignLeft);
            default:
                return QVariant(Qt::AlignCenter);
        }
    }

    if (role == Qt::ForegroundRole && index.column() == ColStatus && r.state == TorrentState::Error) {
        return QColor(220, 60, 60);
    }

    if (role != Qt::DisplayRole) return QVariant();

    switch (index.column()) {
        case ColName: return r.name;
        case ColSize: return Utils::formatSize(r.totalSize);
        case ColProgress: return Utils::formatPercent(r.progress);
        case ColStatus: return stateLabel(r.state);
        case ColDown: return Utils::formatSpeed(r.downloadRate);
        case ColUp: return Utils::formatSpeed(r.uploadRate);
        case ColSeeds:
            return r.numSeedsSwarm >= 0
                ? QStringLiteral("%1 (%2)").arg(r.numSeedsConnected).arg(r.numSeedsSwarm)
                : QString::number(r.numSeedsConnected);
        case ColPeers:
            return r.numPeersSwarm >= 0
                ? QStringLiteral("%1 (%2)").arg(r.numPeersConnected).arg(r.numPeersSwarm)
                : QString::number(r.numPeersConnected);
        case ColEta: return Utils::formatEta(r.etaSeconds);
        case ColRatio: return Utils::formatRatio(r.totalUploaded, r.totalDownloaded);
        case ColCategory: return r.category.isEmpty() ? QStringLiteral("\u2014") : r.category;
        default: return QVariant();
    }
}

QVariant TorrentTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return QVariant();
    switch (section) {
        case ColName: return tr("Nome");
        case ColSize: return tr("Tamanho");
        case ColProgress: return tr("Progresso");
        case ColStatus: return tr("Status");
        case ColDown: return tr("Download");
        case ColUp: return tr("Upload");
        case ColSeeds: return tr("Seeds");
        case ColPeers: return tr("Peers");
        case ColEta: return tr("ETA");
        case ColRatio: return tr("Proporção");
        case ColCategory: return tr("Categoria");
        default: return QVariant();
    }
}

void TorrentTableModel::setRecords(const QVector<TorrentRecord> &records)
{
    QMap<QString, int> incomingIndex;
    for (int i = 0; i < records.size(); ++i) incomingIndex[records[i].id] = i;

    for (int row = m_records.size() - 1; row >= 0; --row) {
        if (!incomingIndex.contains(m_records[row].id)) {
            beginRemoveRows(QModelIndex(), row, row);
            m_records.removeAt(row);
            endRemoveRows();
        }
    }

    QMap<QString, int> currentIndex;
    for (int i = 0; i < m_records.size(); ++i) currentIndex[m_records[i].id] = i;

    for (const TorrentRecord &rec : records) {
        auto it = currentIndex.find(rec.id);
        if (it != currentIndex.end()) {
            int row = it.value();
            m_records[row] = rec;
            emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        }
    }

    QVector<TorrentRecord> toAppend;
    for (const TorrentRecord &rec : records) {
        if (!currentIndex.contains(rec.id)) toAppend.push_back(rec);
    }
    if (!toAppend.isEmpty()) {
        const int first = m_records.size();
        beginInsertRows(QModelIndex(), first, first + toAppend.size() - 1);
        m_records += toAppend;
        endInsertRows();
    }
}

const TorrentRecord *TorrentTableModel::recordAt(int row) const
{
    if (row < 0 || row >= m_records.size()) return nullptr;
    return &m_records[row];
}

int TorrentTableModel::rowForId(const QString &id) const
{
    for (int i = 0; i < m_records.size(); ++i) {
        if (m_records[i].id == id) return i;
    }
    return -1;
}

QString TorrentTableModel::stateLabel(TorrentState state)
{
    switch (state) {
        case TorrentState::CheckingFiles: return tr("Verificando arquivos");
        case TorrentState::DownloadingMetadata: return tr("Obtendo metadados");
        case TorrentState::Downloading: return tr("Baixando");
        case TorrentState::Finished: return tr("Concluído");
        case TorrentState::Seeding: return tr("Semeando");
        case TorrentState::CheckingResumeData: return tr("Verificando dados");
        case TorrentState::Paused: return tr("Pausado");
        case TorrentState::QueuedDownload: return tr("Na fila (download)");
        case TorrentState::QueuedSeed: return tr("Na fila (semeando)");
        case TorrentState::Error: return tr("Erro");
    }
    return tr("Desconhecido");
}
