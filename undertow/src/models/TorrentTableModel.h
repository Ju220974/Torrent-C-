#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "../core/TorrentRecord.h"

class TorrentTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColName = 0,
        ColSize,
        ColProgress,
        ColStatus,
        ColDown,
        ColUp,
        ColSeeds,
        ColPeers,
        ColEta,
        ColRatio,
        ColCategory,
        ColumnCount
    };

    explicit TorrentTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Replaces the full snapshot. Called once per poll tick from MainWindow.
    void setRecords(const QVector<TorrentRecord> &records);

    const TorrentRecord *recordAt(int row) const;
    int rowForId(const QString &id) const;

    static QString stateLabel(TorrentState state);

private:
    QVector<TorrentRecord> m_records;
};
