#pragma once

#include <QWidget>
#include "../core/TorrentRecord.h"

class QTabWidget;
class QLabel;
class QTableWidget;
class QTreeWidget;
class SessionManager;
class SpeedGraphWidget;

// Shows everything about the currently-selected torrent. MainWindow calls
// setCurrentTorrent() on selection change and refresh() once per poll
// tick; when nothing is selected refresh(nullptr) clears the panel.
class DetailsPanel : public QWidget {
    Q_OBJECT
public:
    explicit DetailsPanel(SessionManager *sessionManager, QWidget *parent = nullptr);

    void setCurrentTorrent(const QString &id);
    void refresh(const TorrentRecord *record);

private slots:
    void showFilesContextMenu(const QPoint &pos);
    void onTabChanged(int index);

private:
    void buildUi();
    void refreshGeneral(const TorrentRecord &rec);
    void refreshTrackers();
    void refreshPeers();
    void refreshFiles();
    void applyPriorityToSelectedFiles(int priority);
    void clearAll();

    SessionManager *m_sessionManager;
    QString m_currentId;

    QTabWidget *m_tabs = nullptr;
    QLabel *m_generalLabel = nullptr;
    QTableWidget *m_trackersTable = nullptr;
    QTableWidget *m_peersTable = nullptr;
    QTreeWidget *m_filesTree = nullptr;
    SpeedGraphWidget *m_speedGraph = nullptr;
};
