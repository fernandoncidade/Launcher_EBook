#pragma once

#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QTranslator>

class QAction;
class QActionGroup;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QTableWidget;
class QEvent;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void chooseDirectory();
    void scanDirectory();
    void clearData();
    void openFileAtRow(int row, int column);
    void reportFileCount();
    void reportFolderCount();
    void setPortugueseLanguage();
    void setEnglishLanguage();
    void showPieChart();
    void showHistogram();
    void showIntellectualProperty();
    void showDisclaimer();
    void showMitLicense();

private:
    void setupUi();
    void setupMenus();
    void retranslateUi();
    void updateWindowIcon();
    bool isSupportedFile(const QString &filePath) const;
    QString humanReadableSize(qint64 bytes) const;
    int countFolders(const QString &rootPath) const;
    QMap<QString, int> extensionCounts() const;
    QString findRuntimeFile(const QString &relativePath) const;
    QString aboutFilePath(const QString &fileName) const;
    QString readTextFile(const QString &filePath) const;
    void showAboutDocument(const QString &title, const QString &fileName);
    void showChartDialog(const QString &title, bool pieChart);
    void setLanguage(const QString &localeName);
    bool setLanguageInternal(const QString &localeName, bool saveAfterChange);
    void loadSettings();
    void saveSettings() const;
    QString settingsFilePath() const;
    void updateStatusReport(const QString &message = QString());

    QLineEdit *m_directoryEdit = nullptr;
    QPushButton *m_chooseButton = nullptr;
    QPushButton *m_scanButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;

    QAction *m_chooseDirectoryAction = nullptr;
    QAction *m_scanDirectoryAction = nullptr;
    QAction *m_clearDataAction = nullptr;
    QAction *m_exitAction = nullptr;
    QAction *m_reportFileCountAction = nullptr;
    QAction *m_reportFolderCountAction = nullptr;
    QAction *m_portugueseAction = nullptr;
    QAction *m_englishAction = nullptr;
    QAction *m_pieChartAction = nullptr;
    QAction *m_histogramAction = nullptr;
    QAction *m_intellectualPropertyAction = nullptr;
    QAction *m_disclaimerAction = nullptr;
    QAction *m_mitLicenseAction = nullptr;
    QActionGroup *m_languageActionGroup = nullptr;

    QMenu *m_filesMenu = nullptr;
    QMenu *m_settingsMenu = nullptr;
    QMenu *m_languageMenu = nullptr;
    QMenu *m_statisticsMenu = nullptr;
    QMenu *m_chartsMenu = nullptr;
    QMenu *m_aboutMenu = nullptr;

    QSet<QString> m_supportedExtensions;
    QTranslator m_translator;
    QString m_currentLanguage = QStringLiteral("pt_BR");
    bool m_loadingSettings = false;
    int m_lastFileCount = 0;
    int m_lastFolderCount = 0;
};
