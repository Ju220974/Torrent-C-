#include "AddTorrentDialog.h"
#include "../core/SessionManager.h"
#include "../core/Utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QStandardPaths>
#include <QDir>

AddTorrentDialog::AddTorrentDialog(SessionManager *sessionManager, QWidget *parent)
    : QDialog(parent), m_sessionManager(sessionManager)
{
    setWindowTitle(tr("Adicionar torrent"));
    resize(560, 480);
    buildUi();
}

void AddTorrentDialog::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);

    // --- Tab 1: from .torrent file ---
    auto *fileTab = new QWidget(this);
    auto *fileLayout = new QVBoxLayout(fileTab);
    auto *fileRow = new QHBoxLayout();
    m_filePathEdit = new QLineEdit(fileTab);
    m_filePathEdit->setPlaceholderText(tr("Nenhum arquivo selecionado"));
    m_filePathEdit->setReadOnly(true);
    auto *browseFileBtn = new QPushButton(tr("Procurar\u2026"), fileTab);
    fileRow->addWidget(m_filePathEdit, 1);
    fileRow->addWidget(browseFileBtn);
    fileLayout->addLayout(fileRow);

    m_previewLabel = new QLabel(tr("Selecione um arquivo .torrent para ver os detalhes."), fileTab);
    m_previewLabel->setWordWrap(true);
    fileLayout->addWidget(m_previewLabel);

    m_previewTree = new QTreeWidget(fileTab);
    m_previewTree->setHeaderLabels({tr("Arquivo"), tr("Tamanho")});
    m_previewTree->setColumnWidth(0, 340);
    m_previewTree->setRootIsDecorated(false);
    fileLayout->addWidget(m_previewTree, 1);

    connect(browseFileBtn, &QPushButton::clicked, this, &AddTorrentDialog::browseForTorrentFile);

    // --- Tab 2: magnet link ---
    auto *magnetTab = new QWidget(this);
    auto *magnetLayout = new QVBoxLayout(magnetTab);
    magnetLayout->addWidget(new QLabel(tr("Cole o link magnet abaixo:"), magnetTab));
    m_magnetEdit = new QLineEdit(magnetTab);
    m_magnetEdit->setPlaceholderText(QStringLiteral("magnet:?xt=urn:btih:..."));
    magnetLayout->addWidget(m_magnetEdit);
    magnetLayout->addStretch(1);

    m_tabs->addTab(fileTab, tr("Arquivo .torrent"));
    m_tabs->addTab(magnetTab, tr("Link magnet"));
    mainLayout->addWidget(m_tabs, 1);

    // --- Shared fields ---
    auto *form = new QFormLayout();

    auto *saveRow = new QHBoxLayout();
    m_savePathEdit = new QLineEdit(this);
    m_savePathEdit->setText(m_sessionManager->defaultSavePath());
    auto *browseSaveBtn = new QToolButton(this);
    browseSaveBtn->setText(QStringLiteral("\u2026"));
    saveRow->addWidget(m_savePathEdit, 1);
    saveRow->addWidget(browseSaveBtn);
    form->addRow(tr("Salvar em:"), saveRow);
    connect(browseSaveBtn, &QToolButton::clicked, this, &AddTorrentDialog::browseForSavePath);

    auto *catRow = new QHBoxLayout();
    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->setEditable(false);
    m_categoryCombo->addItem(tr("(sem categoria)"), QString());
    for (const QString &cat : m_sessionManager->categories()) m_categoryCombo->addItem(cat, cat);
    auto *newCatBtn = new QToolButton(this);
    newCatBtn->setText(QStringLiteral("+"));
    newCatBtn->setToolTip(tr("Nova categoria"));
    catRow->addWidget(m_categoryCombo, 1);
    catRow->addWidget(newCatBtn);
    form->addRow(tr("Categoria:"), catRow);
    connect(newCatBtn, &QToolButton::clicked, this, &AddTorrentDialog::createCategoryRequested);

    mainLayout->addLayout(form);

    m_startImmediatelyCheck = new QCheckBox(tr("Iniciar imediatamente"), this);
    m_startImmediatelyCheck->setChecked(true);
    mainLayout->addWidget(m_startImmediatelyCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void AddTorrentDialog::setInitialTorrentFile(const QString &path)
{
    m_torrentFilePath = path;
    m_filePathEdit->setText(path);
    m_tabs->setCurrentIndex(0);
    updatePreview();
}

void AddTorrentDialog::setInitialMagnetUri(const QString &uri)
{
    m_magnetEdit->setText(uri);
    m_tabs->setCurrentIndex(1);
}

void AddTorrentDialog::browseForTorrentFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Selecionar arquivo .torrent"), QDir::homePath(), tr("Arquivos torrent (*.torrent)"));
    if (path.isEmpty()) return;
    m_torrentFilePath = path;
    m_filePathEdit->setText(path);
    updatePreview();
}

void AddTorrentDialog::browseForSavePath()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, tr("Selecionar pasta de destino"), m_savePathEdit->text());
    if (!path.isEmpty()) m_savePathEdit->setText(path);
}

void AddTorrentDialog::updatePreview()
{
    m_previewTree->clear();
    if (m_torrentFilePath.isEmpty()) return;

    SessionManager::TorrentPreview preview = SessionManager::previewTorrentFile(m_torrentFilePath);
    if (!preview.valid) {
        m_previewLabel->setText(tr("Não foi possível ler o arquivo: %1").arg(preview.error));
        return;
    }

    m_previewLabel->setText(tr("%1  \u2014  %2  \u2014  %3 arquivo(s)")
        .arg(preview.name, Utils::formatSize(preview.size)).arg(preview.files.size()));

    for (const TorrentFileEntry &f : preview.files) {
        auto *item = new QTreeWidgetItem(m_previewTree);
        item->setText(0, f.path);
        item->setText(1, Utils::formatSize(f.size));
    }
}

void AddTorrentDialog::createCategoryRequested()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Nova categoria"), tr("Nome da categoria:"),
                                                QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    m_sessionManager->createCategory(name.trimmed());
    m_categoryCombo->addItem(name.trimmed(), name.trimmed());
    m_categoryCombo->setCurrentIndex(m_categoryCombo->count() - 1);
}

bool AddTorrentDialog::isMagnetMode() const
{
    return m_tabs->currentIndex() == 1;
}

QString AddTorrentDialog::magnetUri() const
{
    return m_magnetEdit->text().trimmed();
}

QString AddTorrentDialog::savePath() const
{
    return m_savePathEdit->text().trimmed();
}

QString AddTorrentDialog::category() const
{
    return m_categoryCombo->currentData().toString();
}

bool AddTorrentDialog::startImmediately() const
{
    return m_startImmediatelyCheck->isChecked();
}
