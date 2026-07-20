#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QVector>

class QTableView;
class QAction;
class QLabel;
class QTimer;
class QListWidget;
class QListWidgetItem;
class QSplitter;
class QCloseEvent;
class SessionManager;
class TorrentTableModel;
class DetailsPanel;
class AddTorrentDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Used both for CLI arguments at startup and (in the future) for a
    // single-instance IPC handler; see README for the current limitation
    // of one session per running process.
    void openPathOrUri(const QString &pathOrUri);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onAddTorrentFile();
    void onAddMagnet();
    void onPauseSelected();
    void onResumeSelected();
    void onRemoveSelected();
    void onRecheckSelected();
    void onReannounceSelected();
    void onSetCategoryForSelected();
    void onCopyMagnetForSelected();
    void onOpenContainingFolder();
    void onOpenSettings();
    void onSelectionChanged();
    void onTorrentsContextMenu(const QPoint &pos);
    void onTick();
    void onTorrentFinished(const QString &name);
    void onTorrentError(const QString &name, const QString &error);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void buildUi();
    void buildToolbar();
    void buildMenus();
    void buildTray();
    void refreshCategorySidebar();
    void updateStatusBar();
    void runAddDialog(AddTorrentDialog &dlg);
    QVector<QString> selectedIds() const;
    QString currentFilterCategory() const;

    SessionManager *m_sessionManager;
    TorrentTableModel *m_model;
    DetailsPanel *m_detailsPanel = nullptr;

    QTableView *m_tableView = nullptr;
    QListWidget *m_categorySidebar = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_verticalSplitter = nullptr;

    QSystemTrayIcon *m_trayIcon = nullptr;
    QTimer *m_pollTimer = nullptr;
    QLabel *m_statusSpeedLabel = nullptr;

    bool m_closeToTray = true;
    bool m_darkTheme = false;
    QString m_lastAddDir;
};
