#include "DetailsPanel.h"
#include "SpeedGraphWidget.h"
#include "../core/SessionManager.h"
#include "../core/Utils.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QMenu>
#include <QAction>

DetailsPanel::DetailsPanel(SessionManager *sessionManager, QWidget *parent)
    : QWidget(parent), m_sessionManager(sessionManager)
{
    buildUi();
}

void DetailsPanel::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_tabs = new QTabWidget(this);

    // General
    m_generalLabel = new QLabel(tr("Selecione um torrent para ver os detalhes."), this);
    m_generalLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_generalLabel->setContentsMargins(12, 12, 12, 12);
    m_generalLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *generalHolder = new QWidget(this);
    auto *generalHolderLayout = new QVBoxLayout(generalHolder);
    generalHolderLayout->addWidget(m_generalLabel);
    generalHolderLayout->addStretch(1);

    // Trackers
    m_trackersTable = new QTableWidget(0, 3, this);
    m_trackersTable->setHorizontalHeaderLabels({tr("URL"), tr("Tier"), tr("Status")});
    m_trackersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_trackersTable->verticalHeader()->setVisible(false);
    m_trackersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_trackersTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Peers
    m_peersTable = new QTableWidget(0, 6, this);
    m_peersTable->setHorizontalHeaderLabels(
        {tr("Endereço"), tr("Cliente"), tr("Progresso"), tr("Download"), tr("Upload"), tr("Flags")});
    m_peersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_peersTable->verticalHeader()->setVisible(false);
    m_peersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_peersTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Files
    m_filesTree = new QTreeWidget(this);
    m_filesTree->setHeaderLabels({tr("Arquivo"), tr("Tamanho"), tr("Progresso"), tr("Prioridade")});
    m_filesTree->setRootIsDecorated(false);
    m_filesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_filesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_filesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(m_filesTree, &QTreeWidget::customContextMenuRequested,
            this, &DetailsPanel::showFilesContextMenu);

    // Speed
    m_speedGraph = new SpeedGraphWidget(this);

    m_tabs->addTab(generalHolder, tr("Geral"));
    m_tabs->addTab(m_trackersTable, tr("Rastreadores"));
    m_tabs->addTab(m_peersTable, tr("Peers"));
    m_tabs->addTab(m_filesTree, tr("Arquivos"));
    m_tabs->addTab(m_speedGraph, tr("Velocidade"));

    connect(m_tabs, &QTabWidget::currentChanged, this, &DetailsPanel::onTabChanged);

    layout->addWidget(m_tabs);
}

void DetailsPanel::setCurrentTorrent(const QString &id)
{
    if (m_currentId == id) return;
    m_currentId = id;
    m_speedGraph->clear();
    if (id.isEmpty()) {
        clearAll();
        return;
    }
    refreshTrackers();
    refreshPeers();
    refreshFiles();
}

void DetailsPanel::refresh(const TorrentRecord *record)
{
    if (!record) {
        if (!m_currentId.isEmpty()) {
            m_currentId.clear();
            clearAll();
        }
        return;
    }

    refreshGeneral(*record);
    m_speedGraph->pushSample(record->downloadRate, record->uploadRate);

    switch (m_tabs->currentIndex()) {
        case 1: refreshTrackers(); break;
        case 2: refreshPeers(); break;
        case 3: refreshFiles(); break;
        default: break;
    }
}

void DetailsPanel::onTabChanged(int index)
{
    if (m_currentId.isEmpty()) return;
    switch (index) {
        case 1: refreshTrackers(); break;
        case 2: refreshPeers(); break;
        case 3: refreshFiles(); break;
        default: break;
    }
}

void DetailsPanel::clearAll()
{
    m_generalLabel->setText(tr("Selecione um torrent para ver os detalhes."));
    m_trackersTable->setRowCount(0);
    m_peersTable->setRowCount(0);
    m_filesTree->clear();
}

void DetailsPanel::refreshGeneral(const TorrentRecord &rec)
{
    auto row = [](const QString &label, const QString &value) {
        return QStringLiteral("<tr><td style='padding-right:14px;'><b>%1</b></td><td>%2</td></tr>")
            .arg(label, value);
    };

    QString html = QStringLiteral("<table cellspacing='4'>");
    html += row(tr("Nome:"), rec.name.toHtmlEscaped());
    html += row(tr("Pasta:"), rec.savePath.toHtmlEscaped());
    html += row(tr("Tamanho total:"), Utils::formatSize(rec.totalSize));
    html += row(tr("Baixado:"), Utils::formatSize(rec.totalDownloaded));
    html += row(tr("Enviado:"), Utils::formatSize(rec.totalUploaded));
    html += row(tr("Proporção:"), Utils::formatRatio(rec.totalUploaded, rec.totalDownloaded));
    html += row(tr("Categoria:"), rec.category.isEmpty() ? tr("(sem categoria)") : rec.category.toHtmlEscaped());
    html += row(tr("Adicionado:"), Utils::formatTimestamp(rec.addedTime));
    html += row(tr("Identificador:"), rec.id);
    html += QStringLiteral("</table>");

    if (!rec.errorMessage.isEmpty()) {
        html += QStringLiteral("<p style='color:#d63c3c;'><b>%1</b> %2</p>")
            .arg(tr("Erro:"), rec.errorMessage.toHtmlEscaped());
    }

    m_generalLabel->setText(html);
}

void DetailsPanel::refreshTrackers()
{
    m_trackersTable->setRowCount(0);
    if (m_currentId.isEmpty()) return;
    const auto trackers = m_sessionManager->trackersForTorrent(m_currentId);
    m_trackersTable->setRowCount(trackers.size());
    for (int i = 0; i < trackers.size(); ++i) {
        m_trackersTable->setItem(i, 0, new QTableWidgetItem(trackers[i].url));
        m_trackersTable->setItem(i, 1, new QTableWidgetItem(QString::number(trackers[i].tier)));
        m_trackersTable->setItem(i, 2, new QTableWidgetItem(trackers[i].status));
    }
}

void DetailsPanel::refreshPeers()
{
    m_peersTable->setRowCount(0);
    if (m_currentId.isEmpty()) return;
    const auto peers = m_sessionManager->peersForTorrent(m_currentId);
    m_peersTable->setRowCount(peers.size());
    for (int i = 0; i < peers.size(); ++i) {
        m_peersTable->setItem(i, 0, new QTableWidgetItem(peers[i].address));
        m_peersTable->setItem(i, 1, new QTableWidgetItem(peers[i].client));
        m_peersTable->setItem(i, 2, new QTableWidgetItem(Utils::formatPercent(peers[i].progress)));
        m_peersTable->setItem(i, 3, new QTableWidgetItem(Utils::formatSpeed(peers[i].downloadRate)));
        m_peersTable->setItem(i, 4, new QTableWidgetItem(Utils::formatSpeed(peers[i].uploadRate)));
        m_peersTable->setItem(i, 5, new QTableWidgetItem(peers[i].flags));
    }
}

void DetailsPanel::refreshFiles()
{
    m_filesTree->clear();
    if (m_currentId.isEmpty()) return;
    const auto files = m_sessionManager->filesForTorrent(m_currentId);

    auto priorityLabel = [this](int p) -> QString {
        if (p <= 0) return tr("Pular");
        if (p <= 2) return tr("Baixa");
        if (p >= 6) return tr("Alta");
        return tr("Normal");
    };

    for (const TorrentFileEntry &f : files) {
        auto *item = new QTreeWidgetItem(m_filesTree);
        item->setText(0, f.path);
        item->setText(1, Utils::formatSize(f.size));
        const float prog = f.size > 0 ? float(f.downloaded) / float(f.size) : 1.0f;
        item->setText(2, Utils::formatPercent(prog));
        item->setText(3, priorityLabel(f.priority));
    }
}

void DetailsPanel::showFilesContextMenu(const QPoint &pos)
{
    if (m_filesTree->selectedItems().isEmpty()) return;
    QMenu menu(this);
    QAction *skip = menu.addAction(tr("Pular (não baixar)"));
    QAction *low = menu.addAction(tr("Prioridade baixa"));
    QAction *normal = menu.addAction(tr("Prioridade normal"));
    QAction *high = menu.addAction(tr("Prioridade alta"));
    QAction *chosen = menu.exec(m_filesTree->viewport()->mapToGlobal(pos));
    if (chosen == skip) applyPriorityToSelectedFiles(0);
    else if (chosen == low) applyPriorityToSelectedFiles(1);
    else if (chosen == normal) applyPriorityToSelectedFiles(4);
    else if (chosen == high) applyPriorityToSelectedFiles(7);
}

void DetailsPanel::applyPriorityToSelectedFiles(int priority)
{
    if (m_currentId.isEmpty()) return;
    const auto files = m_sessionManager->filesForTorrent(m_currentId);
    QVector<int> priorities;
    priorities.reserve(files.size());
    for (const auto &f : files) priorities.push_back(f.priority);

    const auto selected = m_filesTree->selectedItems();
    for (auto *item : selected) {
        const int row = m_filesTree->indexOfTopLevelItem(item);
        if (row >= 0 && row < priorities.size()) priorities[row] = priority;
    }
    m_sessionManager->setFilePriorities(m_currentId, priorities);
    refreshFiles();
}
