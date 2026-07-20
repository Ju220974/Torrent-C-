#include "MainWindow.h"
#include "AddTorrentDialog.h"
#include "SettingsDialog.h"
#include "DetailsPanel.h"
#include "Theme.h"
#include "../core/SessionManager.h"
#include "../core/Utils.h"
#include "../models/TorrentTableModel.h"
#include "../models/ProgressDelegate.h"

#include <QApplication>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QTableView>
#include <QHeaderView>
#include <QListWidget>
#include <QSplitter>
#include <QLabel>
#include <QTimer>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QInputDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QDesktopServices>
#include <QClipboard>
#include <QCloseEvent>
#include <QSettings>
#include <QStyle>

namespace {

bool matchesSmartFilter(const TorrentRecord &r, const QString &marker)
{
    if (marker == QStringLiteral("::downloading")) {
        return r.state == TorrentState::Downloading
            || r.state == TorrentState::DownloadingMetadata
            || r.state == TorrentState::QueuedDownload
            || r.state == TorrentState::CheckingFiles
            || r.state == TorrentState::CheckingResumeData;
    }
    if (marker == QStringLiteral("::seeding")) {
        return r.state == TorrentState::Seeding || r.state == TorrentState::QueuedSeed;
    }
    if (marker == QStringLiteral("::completed")) {
        return r.progress >= 0.999f;
    }
    if (marker == QStringLiteral("::paused")) {
        return r.state == TorrentState::Paused;
    }
    if (marker == QStringLiteral("::error")) {
        return r.state == TorrentState::Error;
    }
    return false;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_sessionManager(new SessionManager(this))
    , m_model(new TorrentTableModel(this))
{
    QSettings settings;
    m_closeToTray = settings.value(QStringLiteral("closeToTray"), true).toBool();
    m_darkTheme = settings.value(QStringLiteral("darkTheme"), false).toBool();
    m_lastAddDir = QDir::homePath();

    if (auto *app = qobject_cast<QApplication *>(QApplication::instance())) {
        Theme::apply(*app, m_darkTheme ? Theme::Mode::Dark : Theme::Mode::Light);
    }

    m_sessionManager->start();

    buildUi();
    buildToolbar();
    buildMenus();
    buildTray();

    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());

    refreshCategorySidebar();

    connect(m_sessionManager, &SessionManager::torrentFinished, this, &MainWindow::onTorrentFinished);
    connect(m_sessionManager, &SessionManager::torrentErrorOccurred, this, &MainWindow::onTorrentError);

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::onTick);
    m_pollTimer->start(1000);

    onTick();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Undertow"));
    resize(1180, 720);

    auto *central = new QWidget(this);
    auto *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_categorySidebar = new QListWidget(central);
    m_categorySidebar->setFixedWidth(190);
    m_categorySidebar->setFrameShape(QFrame::NoFrame);

    m_tableView = new QTableView(central);
    m_tableView->setModel(m_model);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->horizontalHeader()->setSectionResizeMode(TorrentTableModel::ColName, QHeaderView::Stretch);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setItemDelegateForColumn(TorrentTableModel::ColProgress, new ProgressDelegate(m_tableView));
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    m_detailsPanel = new DetailsPanel(m_sessionManager, central);

    m_verticalSplitter = new QSplitter(Qt::Vertical, central);
    m_verticalSplitter->addWidget(m_tableView);
    m_verticalSplitter->addWidget(m_detailsPanel);
    m_verticalSplitter->setStretchFactor(0, 3);
    m_verticalSplitter->setStretchFactor(1, 2);

    m_mainSplitter = new QSplitter(Qt::Horizontal, central);
    m_mainSplitter->addWidget(m_categorySidebar);
    m_mainSplitter->addWidget(m_verticalSplitter);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_mainSplitter);
    setCentralWidget(central);

    m_statusSpeedLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusSpeedLabel);

    connect(m_tableView, &QTableView::customContextMenuRequested, this, &MainWindow::onTorrentsContextMenu);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onSelectionChanged);
    connect(m_categorySidebar, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *, QListWidgetItem *) { onTick(); });
}

void MainWindow::buildToolbar()
{
    auto *toolbar = addToolBar(tr("Principal"));
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    QAction *addFileAction = toolbar->addAction(
        style()->standardIcon(QStyle::SP_FileIcon), tr("Adicionar Torrent"));
    connect(addFileAction, &QAction::triggered, this, &MainWindow::onAddTorrentFile);

    QAction *addMagnetAction = toolbar->addAction(
        style()->standardIcon(QStyle::SP_DriveNetIcon), tr("Adicionar Magnet"));
    connect(addMagnetAction, &QAction::triggered, this, &MainWindow::onAddMagnet);

    toolbar->addSeparator();

    QAction *startAction = toolbar->addAction(
        style()->standardIcon(QStyle::SP_MediaPlay), tr("Iniciar"));
    connect(startAction, &QAction::triggered, this, &MainWindow::onResumeSelected);

    QAction *pauseAction = toolbar->addAction(
        style()->standardIcon(QStyle::SP_MediaPause), tr("Pausar"));
    connect(pauseAction, &QAction::triggered, this, &MainWindow::onPauseSelected);

    QAction *removeAction = toolbar->addAction(
        style()->standardIcon(QStyle::SP_TrashIcon), tr("Remover"));
    connect(removeAction, &QAction::triggered, this, &MainWindow::onRemoveSelected);

    toolbar->addSeparator();

    QAction *settingsAction = toolbar->addAction(
        style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("Configurações"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onOpenSettings);
}

void MainWindow::buildMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&Arquivo"));
    fileMenu->addAction(tr("Adicionar torrent\u2026"), QKeySequence::Open,
                         this, &MainWindow::onAddTorrentFile);
    fileMenu->addAction(tr("Adicionar link magnet\u2026"), this, &MainWindow::onAddMagnet);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Sair"), QKeySequence::Quit, qApp, &QApplication::quit);

    auto *torrentMenu = menuBar()->addMenu(tr("&Torrent"));
    torrentMenu->addAction(tr("Iniciar"), this, &MainWindow::onResumeSelected);
    torrentMenu->addAction(tr("Pausar"), this, &MainWindow::onPauseSelected);
    torrentMenu->addSeparator();
    torrentMenu->addAction(tr("Forçar verificação"), this, &MainWindow::onRecheckSelected);
    torrentMenu->addAction(tr("Forçar reanúncio"), this, &MainWindow::onReannounceSelected);
    torrentMenu->addSeparator();
    torrentMenu->addAction(tr("Definir categoria\u2026"), this, &MainWindow::onSetCategoryForSelected);
    torrentMenu->addAction(tr("Copiar link magnet"), this, &MainWindow::onCopyMagnetForSelected);
    torrentMenu->addAction(tr("Abrir pasta de destino"), this, &MainWindow::onOpenContainingFolder);
    torrentMenu->addSeparator();
    torrentMenu->addAction(tr("Remover\u2026"), this, &MainWindow::onRemoveSelected);

    auto *viewMenu = menuBar()->addMenu(tr("Exi&bir"));
    viewMenu->addAction(tr("Configurações\u2026"), this, &MainWindow::onOpenSettings);

    auto *helpMenu = menuBar()->addMenu(tr("Aj&uda"));
    helpMenu->addAction(tr("Sobre o Undertow"), this, [this] {
        QMessageBox::about(this, tr("Sobre o Undertow"),
            tr("<b>Undertow</b> 1.0.0<br>Cliente BitTorrent construído com Qt6 e libtorrent-rasterbar."));
    });
}

void MainWindow::buildTray()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(style()->standardIcon(QStyle::SP_DriveNetIcon));
    m_trayIcon->setToolTip(QStringLiteral("Undertow"));

    auto *menu = new QMenu(this);
    QAction *showAction = menu->addAction(tr("Mostrar"));
    QAction *quitAction = menu->addAction(tr("Sair"));
    connect(showAction, &QAction::triggered, this, [this] { showNormal(); raise(); activateWindow(); });
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    m_trayIcon->setContextMenu(menu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    if (QSystemTrayIcon::isSystemTrayAvailable()) m_trayIcon->show();
}

void MainWindow::refreshCategorySidebar()
{
    const QString currentMarker = currentFilterCategory();
    m_categorySidebar->clear();

    auto addSmartItem = [this](const QString &label, const QString &marker) {
        auto *item = new QListWidgetItem(label, m_categorySidebar);
        item->setData(Qt::UserRole, marker);
    };
    addSmartItem(tr("Tudo"), QStringLiteral("::all"));
    addSmartItem(tr("Baixando"), QStringLiteral("::downloading"));
    addSmartItem(tr("Semeando"), QStringLiteral("::seeding"));
    addSmartItem(tr("Concluídos"), QStringLiteral("::completed"));
    addSmartItem(tr("Pausados"), QStringLiteral("::paused"));
    addSmartItem(tr("Com erro"), QStringLiteral("::error"));

    if (!m_sessionManager->categories().isEmpty()) {
        auto *sep = new QListWidgetItem(QStringLiteral("\u2500\u2500\u2500\u2500\u2500\u2500"), m_categorySidebar);
        sep->setFlags(Qt::NoItemFlags);
    }
    for (const QString &cat : m_sessionManager->categories()) {
        auto *item = new QListWidgetItem(cat, m_categorySidebar);
        item->setData(Qt::UserRole, cat);
    }

    for (int i = 0; i < m_categorySidebar->count(); ++i) {
        if (m_categorySidebar->item(i)->data(Qt::UserRole).toString() == currentMarker) {
            m_categorySidebar->setCurrentRow(i);
            return;
        }
    }
    m_categorySidebar->setCurrentRow(0);
}

QString MainWindow::currentFilterCategory() const
{
    auto *item = m_categorySidebar->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QStringLiteral("::all");
}

QVector<QString> MainWindow::selectedIds() const
{
    QVector<QString> ids;
    if (!m_tableView->selectionModel()) return ids;
    const auto rows = m_tableView->selectionModel()->selectedRows();
    for (const auto &idx : rows) {
        const TorrentRecord *rec = m_model->recordAt(idx.row());
        if (rec) ids.push_back(rec->id);
    }
    return ids;
}

void MainWindow::onSelectionChanged()
{
    const auto ids = selectedIds();
    m_detailsPanel->setCurrentTorrent(ids.size() == 1 ? ids.first() : QString());
}

void MainWindow::updateStatusBar()
{
    m_statusSpeedLabel->setText(tr("\u2193 %1    \u2191 %2    %3 torrent(s)")
        .arg(Utils::formatSpeed(m_sessionManager->totalDownloadRate()))
        .arg(Utils::formatSpeed(m_sessionManager->totalUploadRate()))
        .arg(m_model->rowCount()));
}

void MainWindow::onTick()
{
    m_sessionManager->pollAlerts();

    const QString filter = currentFilterCategory();
    const QVector<TorrentRecord> all = m_sessionManager->torrents();

    QVector<TorrentRecord> filtered;
    filtered.reserve(all.size());
    for (const TorrentRecord &r : all) {
        if (filter == QStringLiteral("::all")) filtered.push_back(r);
        else if (filter.startsWith(QStringLiteral("::"))) {
            if (matchesSmartFilter(r, filter)) filtered.push_back(r);
        } else if (r.category == filter) {
            filtered.push_back(r);
        }
    }

    m_model->setRecords(filtered);

    const auto selected = selectedIds();
    const TorrentRecord *currentRec = nullptr;
    if (selected.size() == 1) {
        for (const TorrentRecord &r : filtered) {
            if (r.id == selected.first()) { currentRec = &r; break; }
        }
    }
    m_detailsPanel->refresh(currentRec);

    updateStatusBar();
}

void MainWindow::runAddDialog(AddTorrentDialog &dlg)
{
    if (dlg.exec() != QDialog::Accepted) return;

    QDir().mkpath(dlg.savePath());

    QString error;
    bool ok;
    if (dlg.isMagnetMode()) {
        ok = m_sessionManager->addMagnet(dlg.magnetUri(), dlg.savePath(), dlg.category(),
                                          !dlg.startImmediately(), &error);
    } else {
        ok = m_sessionManager->addTorrentFromFile(dlg.torrentFilePath(), dlg.savePath(), dlg.category(),
                                                   !dlg.startImmediately(), &error);
    }

    if (!ok) {
        QMessageBox::critical(this, tr("Erro ao adicionar torrent"), error);
    }
    refreshCategorySidebar();
    onTick();
}

void MainWindow::onAddTorrentFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Selecionar arquivo .torrent"), m_lastAddDir, tr("Arquivos torrent (*.torrent)"));
    if (path.isEmpty()) return;
    m_lastAddDir = QFileInfo(path).absolutePath();

    AddTorrentDialog dlg(m_sessionManager, this);
    dlg.setInitialTorrentFile(path);
    runAddDialog(dlg);
}

void MainWindow::onAddMagnet()
{
    AddTorrentDialog dlg(m_sessionManager, this);
    dlg.setInitialMagnetUri(QString());
    runAddDialog(dlg);
}

void MainWindow::openPathOrUri(const QString &pathOrUri)
{
    AddTorrentDialog dlg(m_sessionManager, this);
    if (pathOrUri.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive)) {
        dlg.setInitialMagnetUri(pathOrUri);
    } else {
        dlg.setInitialTorrentFile(pathOrUri);
    }
    runAddDialog(dlg);
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::onPauseSelected()
{
    for (const QString &id : selectedIds()) m_sessionManager->pauseTorrent(id);
    onTick();
}

void MainWindow::onResumeSelected()
{
    for (const QString &id : selectedIds()) m_sessionManager->resumeTorrent(id);
    onTick();
}

void MainWindow::onRemoveSelected()
{
    const auto ids = selectedIds();
    if (ids.isEmpty()) return;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Remover torrent"));
    box.setText(ids.size() == 1
        ? tr("Remover o torrent selecionado?")
        : tr("Remover %1 torrents selecionados?").arg(ids.size()));
    QPushButton *removeOnly = box.addButton(tr("Remover apenas da lista"), QMessageBox::AcceptRole);
    QPushButton *removeWithData = box.addButton(tr("Remover e apagar arquivos"), QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();

    if (box.clickedButton() == removeOnly) {
        for (const QString &id : ids) m_sessionManager->removeTorrent(id, false);
    } else if (box.clickedButton() == removeWithData) {
        for (const QString &id : ids) m_sessionManager->removeTorrent(id, true);
    } else {
        return;
    }
    onTick();
}

void MainWindow::onRecheckSelected()
{
    for (const QString &id : selectedIds()) m_sessionManager->forceRecheck(id);
}

void MainWindow::onReannounceSelected()
{
    for (const QString &id : selectedIds()) m_sessionManager->forceReannounce(id);
}

void MainWindow::onSetCategoryForSelected()
{
    const auto ids = selectedIds();
    if (ids.isEmpty()) return;

    const QString noneLabel = tr("(sem categoria)");
    QStringList options;
    options << noneLabel << m_sessionManager->categories();

    bool ok = false;
    const QString chosen = QInputDialog::getItem(this, tr("Definir categoria"), tr("Categoria:"),
                                                  options, 0, false, &ok);
    if (!ok) return;
    const QString category = (chosen == noneLabel) ? QString() : chosen;
    for (const QString &id : ids) m_sessionManager->setCategory(id, category);
    refreshCategorySidebar();
    onTick();
}

void MainWindow::onCopyMagnetForSelected()
{
    const auto ids = selectedIds();
    if (ids.size() != 1) return;
    const QString uri = m_sessionManager->magnetLinkForTorrent(ids.first());
    if (!uri.isEmpty()) QGuiApplication::clipboard()->setText(uri);
}

void MainWindow::onOpenContainingFolder()
{
    const auto ids = selectedIds();
    if (ids.size() != 1) return;
    const QString path = m_sessionManager->savePathForTorrent(ids.first());
    if (!path.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::onTorrentsContextMenu(const QPoint &pos)
{
    if (!m_tableView->selectionModel() || m_tableView->selectionModel()->selectedRows().isEmpty()) return;

    QMenu menu(this);
    menu.addAction(tr("Iniciar"), this, &MainWindow::onResumeSelected);
    menu.addAction(tr("Pausar"), this, &MainWindow::onPauseSelected);
    menu.addSeparator();
    menu.addAction(tr("Forçar verificação"), this, &MainWindow::onRecheckSelected);
    menu.addAction(tr("Forçar reanúncio"), this, &MainWindow::onReannounceSelected);
    menu.addSeparator();
    menu.addAction(tr("Definir categoria\u2026"), this, &MainWindow::onSetCategoryForSelected);
    menu.addAction(tr("Copiar link magnet"), this, &MainWindow::onCopyMagnetForSelected);
    menu.addAction(tr("Abrir pasta de destino"), this, &MainWindow::onOpenContainingFolder);
    menu.addSeparator();
    menu.addAction(tr("Remover\u2026"), this, &MainWindow::onRemoveSelected);
    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dlg(m_sessionManager, this);
    dlg.setCloseToTray(m_closeToTray);
    dlg.setDarkTheme(m_darkTheme);
    if (dlg.exec() != QDialog::Accepted) return;

    m_closeToTray = dlg.closeToTray();
    const bool newDark = dlg.darkTheme();
    if (newDark != m_darkTheme) {
        m_darkTheme = newDark;
        if (auto *app = qobject_cast<QApplication *>(QApplication::instance())) {
            Theme::apply(*app, m_darkTheme ? Theme::Mode::Dark : Theme::Mode::Light);
        }
    }

    QSettings settings;
    settings.setValue(QStringLiteral("closeToTray"), m_closeToTray);
    settings.setValue(QStringLiteral("darkTheme"), m_darkTheme);

    refreshCategorySidebar();
    onTick();
}

void MainWindow::onTorrentFinished(const QString &name)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(tr("Download concluído"), name, QSystemTrayIcon::Information, 5000);
    }
}

void MainWindow::onTorrentError(const QString &name, const QString &error)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(tr("Erro no torrent"), QStringLiteral("%1: %2").arg(name, error),
                                 QSystemTrayIcon::Warning, 6000);
    }
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible() && !isMinimized()) {
            hide();
        } else {
            showNormal();
            raise();
            activateWindow();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_closeToTray && m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
        return;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("closeToTray"), m_closeToTray);
    settings.setValue(QStringLiteral("darkTheme"), m_darkTheme);

    m_sessionManager->shutdown();
    event->accept();
}
