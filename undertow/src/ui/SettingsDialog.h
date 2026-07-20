#pragma once

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QListWidget;
class SessionManager;

// Four tabs: General, Connection, Speed, Categories. Values are only
// pushed into SessionManager (and the returned UI-only flags) when the
// user presses OK; Cancel discards everything.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(SessionManager *sessionManager, QWidget *parent = nullptr);

    bool closeToTray() const;
    bool startMinimized() const;
    bool darkTheme() const;

    void setCloseToTray(bool v);
    void setStartMinimized(bool v);
    void setDarkTheme(bool v);

private slots:
    void browseSavePath();
    void addCategory();
    void removeCategory();
    void applyAndAccept();

private:
    void buildUi();
    void refreshCategoryList();

    SessionManager *m_sessionManager;

    QLineEdit *m_savePathEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QCheckBox *m_dhtCheck = nullptr;
    QCheckBox *m_lsdCheck = nullptr;
    QCheckBox *m_upnpCheck = nullptr;
    QCheckBox *m_natpmpCheck = nullptr;
    QSpinBox *m_downLimitSpin = nullptr;
    QSpinBox *m_upLimitSpin = nullptr;
    QCheckBox *m_closeToTrayCheck = nullptr;
    QCheckBox *m_startMinimizedCheck = nullptr;
    QCheckBox *m_darkThemeCheck = nullptr;
    QListWidget *m_categoryList = nullptr;
};
