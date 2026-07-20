#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QTabWidget;
class QTreeWidget;
class QLabel;
class SessionManager;

// Covers both ways of adding a torrent (file or magnet link) in one
// dialog with two tabs, plus the fields every add shares: save path,
// category and whether to start right away.
class AddTorrentDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddTorrentDialog(SessionManager *sessionManager, QWidget *parent = nullptr);

    void setInitialTorrentFile(const QString &path);
    void setInitialMagnetUri(const QString &uri);

    bool isMagnetMode() const;
    QString torrentFilePath() const { return m_torrentFilePath; }
    QString magnetUri() const;
    QString savePath() const;
    QString category() const;
    bool startImmediately() const;

private slots:
    void browseForTorrentFile();
    void browseForSavePath();
    void updatePreview();
    void createCategoryRequested();

private:
    void buildUi();

    SessionManager *m_sessionManager;
    QString m_torrentFilePath;

    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_filePathEdit = nullptr;
    QLineEdit *m_magnetEdit = nullptr;
    QLineEdit *m_savePathEdit = nullptr;
    QComboBox *m_categoryCombo = nullptr;
    QCheckBox *m_startImmediatelyCheck = nullptr;
    QLabel *m_previewLabel = nullptr;
    QTreeWidget *m_previewTree = nullptr;
};
