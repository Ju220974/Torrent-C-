#include "SettingsDialog.h"
#include "../core/SessionManager.h"

#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QListWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

SettingsDialog::SettingsDialog(SessionManager *sessionManager, QWidget *parent)
    : QDialog(parent), m_sessionManager(sessionManager)
{
    setWindowTitle(tr("Configurações"));
    resize(480, 420);
    buildUi();
}

void SettingsDialog::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    // --- Geral ---
    auto *generalTab = new QWidget(this);
    auto *generalForm = new QFormLayout(generalTab);

    auto *saveRow = new QHBoxLayout();
    m_savePathEdit = new QLineEdit(m_sessionManager->defaultSavePath(), generalTab);
    auto *browseBtn = new QToolButton(generalTab);
    browseBtn->setText(QStringLiteral("\u2026"));
    saveRow->addWidget(m_savePathEdit, 1);
    saveRow->addWidget(browseBtn);
    generalForm->addRow(tr("Pasta de download padrão:"), saveRow);
    connect(browseBtn, &QToolButton::clicked, this, &SettingsDialog::browseSavePath);

    m_closeToTrayCheck = new QCheckBox(tr("Minimizar para a bandeja ao fechar"), generalTab);
    m_startMinimizedCheck = new QCheckBox(tr("Iniciar minimizado"), generalTab);
    m_darkThemeCheck = new QCheckBox(tr("Tema escuro"), generalTab);
    generalForm->addRow(m_closeToTrayCheck);
    generalForm->addRow(m_startMinimizedCheck);
    generalForm->addRow(m_darkThemeCheck);

    tabs->addTab(generalTab, tr("Geral"));

    // --- Conexão ---
    auto *connTab = new QWidget(this);
    auto *connForm = new QFormLayout(connTab);

    m_portSpin = new QSpinBox(connTab);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(m_sessionManager->listenPort());
    connForm->addRow(tr("Porta de escuta:"), m_portSpin);

    m_dhtCheck = new QCheckBox(tr("DHT (rede distribuída sem tracker)"), connTab);
    m_lsdCheck = new QCheckBox(tr("LSD (descoberta na rede local)"), connTab);
    m_upnpCheck = new QCheckBox(tr("UPnP (abrir porta automaticamente no roteador)"), connTab);
    m_natpmpCheck = new QCheckBox(tr("NAT-PMP"), connTab);
    m_dhtCheck->setChecked(true);
    m_lsdCheck->setChecked(true);
    m_upnpCheck->setChecked(true);
    m_natpmpCheck->setChecked(true);
    connForm->addRow(m_dhtCheck);
    connForm->addRow(m_lsdCheck);
    connForm->addRow(m_upnpCheck);
    connForm->addRow(m_natpmpCheck);

    tabs->addTab(connTab, tr("Conexão"));

    // --- Velocidade ---
    auto *speedTab = new QWidget(this);
    auto *speedForm = new QFormLayout(speedTab);

    m_downLimitSpin = new QSpinBox(speedTab);
    m_downLimitSpin->setRange(0, 1000000);
    m_downLimitSpin->setSuffix(tr(" KB/s"));
    m_downLimitSpin->setSpecialValueText(tr("Ilimitado"));
    m_downLimitSpin->setValue(m_sessionManager->globalSpeedLimits().first / 1024);
    speedForm->addRow(tr("Limite de download:"), m_downLimitSpin);

    m_upLimitSpin = new QSpinBox(speedTab);
    m_upLimitSpin->setRange(0, 1000000);
    m_upLimitSpin->setSuffix(tr(" KB/s"));
    m_upLimitSpin->setSpecialValueText(tr("Ilimitado"));
    m_upLimitSpin->setValue(m_sessionManager->globalSpeedLimits().second / 1024);
    speedForm->addRow(tr("Limite de upload:"), m_upLimitSpin);

    tabs->addTab(speedTab, tr("Velocidade"));

    // --- Categorias ---
    auto *catTab = new QWidget(this);
    auto *catLayout = new QVBoxLayout(catTab);
    catLayout->addWidget(new QLabel(tr("Categorias usadas para organizar seus torrents:"), catTab));
    m_categoryList = new QListWidget(catTab);
    catLayout->addWidget(m_categoryList, 1);
    auto *catBtnRow = new QHBoxLayout();
    auto *addCatBtn = new QPushButton(tr("Adicionar"), catTab);
    auto *removeCatBtn = new QPushButton(tr("Remover"), catTab);
    catBtnRow->addWidget(addCatBtn);
    catBtnRow->addWidget(removeCatBtn);
    catBtnRow->addStretch(1);
    catLayout->addLayout(catBtnRow);
    connect(addCatBtn, &QPushButton::clicked, this, &SettingsDialog::addCategory);
    connect(removeCatBtn, &QPushButton::clicked, this, &SettingsDialog::removeCategory);
    refreshCategoryList();

    tabs->addTab(catTab, tr("Categorias"));

    mainLayout->addWidget(tabs, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::applyAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void SettingsDialog::refreshCategoryList()
{
    m_categoryList->clear();
    m_categoryList->addItems(m_sessionManager->categories());
}

void SettingsDialog::browseSavePath()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("Selecionar pasta"), m_savePathEdit->text());
    if (!path.isEmpty()) m_savePathEdit->setText(path);
}

void SettingsDialog::addCategory()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Nova categoria"), tr("Nome da categoria:"),
                                                QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    m_sessionManager->createCategory(name.trimmed());
    refreshCategoryList();
}

void SettingsDialog::removeCategory()
{
    auto *item = m_categoryList->currentItem();
    if (!item) return;
    const auto reply = QMessageBox::question(this, tr("Remover categoria"),
        tr("Remover a categoria \"%1\"? Os torrents nela ficarão sem categoria.").arg(item->text()));
    if (reply != QMessageBox::Yes) return;
    m_sessionManager->deleteCategory(item->text());
    refreshCategoryList();
}

void SettingsDialog::applyAndAccept()
{
    m_sessionManager->setDefaultSavePath(m_savePathEdit->text());
    m_sessionManager->setListenPort(m_portSpin->value());
    m_sessionManager->setProtocolsEnabled(m_dhtCheck->isChecked(), m_lsdCheck->isChecked(),
                                          m_upnpCheck->isChecked(), m_natpmpCheck->isChecked());
    m_sessionManager->setGlobalSpeedLimits(m_downLimitSpin->value() * 1024, m_upLimitSpin->value() * 1024);
    accept();
}

bool SettingsDialog::closeToTray() const { return m_closeToTrayCheck->isChecked(); }
bool SettingsDialog::startMinimized() const { return m_startMinimizedCheck->isChecked(); }
bool SettingsDialog::darkTheme() const { return m_darkThemeCheck->isChecked(); }

void SettingsDialog::setCloseToTray(bool v) { m_closeToTrayCheck->setChecked(v); }
void SettingsDialog::setStartMinimized(bool v) { m_startMinimizedCheck->setChecked(v); }
void SettingsDialog::setDarkTheme(bool v) { m_darkThemeCheck->setChecked(v); }
