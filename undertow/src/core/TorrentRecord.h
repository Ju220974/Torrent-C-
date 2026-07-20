#pragma once

#include <QString>
#include <QMetaType>

// Torrent lifecycle state, translated from libtorrent's state_t plus
// its paused/queued flags into something the UI can reason about directly.
enum class TorrentState {
    CheckingFiles,
    DownloadingMetadata,
    Downloading,
    Finished,
    Seeding,
    CheckingResumeData,
    Paused,
    QueuedDownload,
    QueuedSeed,
    Error
};

// Flat, Qt-friendly snapshot of one torrent's status. SessionManager
// rebuilds a vector of these on every poll tick from libtorrent's
// torrent_status objects, so nothing in the UI layer ever touches
// libtorrent types directly.
struct TorrentRecord {
    QString id;              // hex info-hash (v1 if present, else truncated v2) - stable unique key
    QString name;
    QString savePath;
    QString category;

    qint64 totalSize = 0;
    qint64 totalDone = 0;
    qint64 totalDownloaded = 0;  // all-time downloaded (payload)
    qint64 totalUploaded = 0;    // all-time uploaded (payload)

    float progress = 0.0f;       // 0..1
    int downloadRate = 0;        // bytes/sec
    int uploadRate = 0;          // bytes/sec

    int numPeersConnected = 0;
    int numSeedsConnected = 0;
    int numPeersSwarm = -1;      // -1 = unknown
    int numSeedsSwarm = -1;

    qint64 etaSeconds = -1;

    TorrentState state = TorrentState::CheckingFiles;
    QString errorMessage;

    qint64 addedTime = 0;        // unix seconds
};

Q_DECLARE_METATYPE(TorrentRecord)
