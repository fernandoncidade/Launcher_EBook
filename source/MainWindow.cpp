#include "MainWindow.hpp"

#include "AppChooserDialog.hpp"
#include "AssociatedApps.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringConverter>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QVector>

#include <utility>

#ifndef EBOOKLAUNCHER_SOURCE_DIR
#define EBOOKLAUNCHER_SOURCE_DIR ""
#endif

#ifndef EBOOKLAUNCHER_VERSION
#define EBOOKLAUNCHER_VERSION "1.0.0"
#endif

namespace
{
class ExtensionChartWidget final : public QWidget
{
public:
    ExtensionChartWidget(QMap<QString, int> data, bool pieChart, QWidget *parent = nullptr)
        : QWidget(parent),
          m_data(std::move(data)),
          m_pieChart(pieChart)
    {
        setMinimumSize(520, 360);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), palette().window());

        if (m_data.isEmpty()) {
            painter.drawText(rect(), Qt::AlignCenter, tr("Nenhum dado para exibir."));
            return;
        }

        if (m_pieChart)
            paintPieChart(painter);
        else
            paintHistogram(painter);
    }

private:
    int totalCount() const
    {
        int total = 0;
        for (int value : m_data)
            total += value;
        return total;
    }

    QColor colorAt(int index) const
    {
        static const QVector<QColor> colors = {
            QColor(48, 105, 152),
            QColor(61, 156, 90),
            QColor(218, 124, 48),
            QColor(148, 89, 160),
            QColor(200, 72, 82),
            QColor(82, 157, 181),
            QColor(118, 118, 118),
            QColor(188, 166, 64)
        };
        return colors.at(index % colors.size());
    }

    void paintPieChart(QPainter &painter)
    {
        const int total = totalCount();
        const QRect chartRect(30, 30, qMin(width() / 2, height() - 70), qMin(width() / 2, height() - 70));
        QRectF pieRect(chartRect);
        pieRect.moveTop((height() - chartRect.height()) / 2);

        int startAngle = 0;
        int index = 0;
        for (auto it = m_data.cbegin(); it != m_data.cend(); ++it, ++index) {
            const int spanAngle = total > 0 ? static_cast<int>(360.0 * 16.0 * it.value() / total) : 0;
            painter.setBrush(colorAt(index));
            painter.setPen(Qt::white);
            painter.drawPie(pieRect, startAngle, spanAngle);
            startAngle += spanAngle;
        }

        const int legendX = pieRect.right() + 38;
        int legendY = 36;
        index = 0;
        painter.setPen(palette().text().color());
        for (auto it = m_data.cbegin(); it != m_data.cend(); ++it, ++index) {
            painter.fillRect(QRect(legendX, legendY + 4, 16, 16), colorAt(index));
            painter.drawText(legendX + 24, legendY + 18,
                             QStringLiteral(".%1: %2").arg(it.key()).arg(it.value()));
            legendY += 28;
        }
    }

    void paintHistogram(QPainter &painter)
    {
        const int left = 52;
        const int bottom = height() - 54;
        const int top = 28;
        const int right = width() - 24;
        const int chartHeight = bottom - top;

        painter.setPen(QPen(palette().text().color(), 1));
        painter.drawLine(left, top, left, bottom);
        painter.drawLine(left, bottom, right, bottom);

        int maxValue = 1;
        for (int value : m_data)
            maxValue = qMax(maxValue, value);

        const int count = m_data.size();
        const int availableWidth = qMax(1, right - left - 20);
        const int barWidth = qMax(18, availableWidth / qMax(1, count) - 12);

        int index = 0;
        for (auto it = m_data.cbegin(); it != m_data.cend(); ++it, ++index) {
            const int x = left + 14 + index * (barWidth + 12);
            const int barHeight = static_cast<int>(chartHeight * (static_cast<double>(it.value()) / maxValue));
            const QRect bar(x, bottom - barHeight, barWidth, barHeight);

            painter.fillRect(bar, colorAt(index));
            painter.setPen(palette().text().color());
            painter.drawText(QRect(x - 8, bottom + 6, barWidth + 16, 20),
                             Qt::AlignCenter,
                             QStringLiteral(".%1").arg(it.key()));
            painter.drawText(QRect(x - 8, bar.top() - 22, barWidth + 16, 18),
                             Qt::AlignCenter,
                             QString::number(it.value()));
        }
    }

    QMap<QString, int> m_data;
    bool m_pieChart = true;
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_supportedExtensions = QSet<QString>{
        "pdf", "epub", "mobi", "azw", "azw3", "fb2", "djvu", "xps",
        "doc", "docx", "odt", "rtf", "txt", "md", "markdown",
        "html", "htm", "xml", "tex", "csv", "log",
        "cbz", "cbr", "cb7", "cbt"
    };

    setupUi();
    setupMenus();
    loadSettings();
    updateWindowIcon();
    retranslateUi();
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();

    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupUi()
{
    resize(1100, 700);

    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    auto *topLayout = new QHBoxLayout();
    m_directoryEdit = new QLineEdit(this);

    m_chooseButton = new QPushButton(this);
    m_scanButton = new QPushButton(this);
    m_clearButton = new QPushButton(this);

    topLayout->addWidget(m_directoryEdit, 1);
    topLayout->addWidget(m_chooseButton);
    topLayout->addWidget(m_scanButton);
    topLayout->addWidget(m_clearButton);
    mainLayout->addLayout(topLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    mainLayout->addWidget(m_table, 1);

    setCentralWidget(central);

    m_statusLabel = new QLabel(this);
    statusBar()->addWidget(m_statusLabel, 1);

    connect(m_chooseButton, &QPushButton::clicked, this, &MainWindow::chooseDirectory);
    connect(m_scanButton, &QPushButton::clicked, this, &MainWindow::scanDirectory);
    connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::clearData);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &MainWindow::openFileAtRow);
}

void MainWindow::setupMenus()
{
    m_chooseDirectoryAction = new QAction(this);
    m_chooseDirectoryAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    connect(m_chooseDirectoryAction, &QAction::triggered, this, &MainWindow::chooseDirectory);

    m_scanDirectoryAction = new QAction(this);
    m_scanDirectoryAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    connect(m_scanDirectoryAction, &QAction::triggered, this, &MainWindow::scanDirectory);

    m_clearDataAction = new QAction(this);
    m_clearDataAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+L")));
    connect(m_clearDataAction, &QAction::triggered, this, &MainWindow::clearData);

    m_exitAction = new QAction(this);
    m_exitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    m_reportFileCountAction = new QAction(this);
    m_reportFileCountAction->setCheckable(true);
    m_reportFileCountAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+A")));
    connect(m_reportFileCountAction, &QAction::toggled, this, &MainWindow::reportFileCount);

    m_reportFolderCountAction = new QAction(this);
    m_reportFolderCountAction->setCheckable(true);
    m_reportFolderCountAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+P")));
    connect(m_reportFolderCountAction, &QAction::toggled, this, &MainWindow::reportFolderCount);

    m_languageActionGroup = new QActionGroup(this);
    m_languageActionGroup->setExclusive(true);

    m_portugueseAction = new QAction(this);
    m_portugueseAction->setCheckable(true);
    m_portugueseAction->setChecked(true);
    m_portugueseAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+1")));
    m_languageActionGroup->addAction(m_portugueseAction);
    connect(m_portugueseAction, &QAction::triggered, this, &MainWindow::setPortugueseLanguage);

    m_englishAction = new QAction(this);
    m_englishAction->setCheckable(true);
    m_englishAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+2")));
    m_languageActionGroup->addAction(m_englishAction);
    connect(m_englishAction, &QAction::triggered, this, &MainWindow::setEnglishLanguage);

    m_pieChartAction = new QAction(this);
    m_pieChartAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+G")));
    connect(m_pieChartAction, &QAction::triggered, this, &MainWindow::showPieChart);

    m_histogramAction = new QAction(this);
    m_histogramAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+H")));
    connect(m_histogramAction, &QAction::triggered, this, &MainWindow::showHistogram);

    m_intellectualPropertyAction = new QAction(this);
    m_intellectualPropertyAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+I")));
    connect(m_intellectualPropertyAction, &QAction::triggered, this, &MainWindow::showIntellectualProperty);

    m_disclaimerAction = new QAction(this);
    m_disclaimerAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+D")));
    connect(m_disclaimerAction, &QAction::triggered, this, &MainWindow::showDisclaimer);

    m_mitLicenseAction = new QAction(this);
    m_mitLicenseAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+M")));
    connect(m_mitLicenseAction, &QAction::triggered, this, &MainWindow::showMitLicense);

    m_filesMenu = menuBar()->addMenu(QString());
    m_filesMenu->addAction(m_chooseDirectoryAction);
    m_filesMenu->addAction(m_scanDirectoryAction);
    m_filesMenu->addSeparator();
    m_filesMenu->addAction(m_clearDataAction);
    m_filesMenu->addSeparator();
    m_filesMenu->addAction(m_exitAction);

    m_settingsMenu = menuBar()->addMenu(QString());
    m_settingsMenu->addAction(m_reportFileCountAction);
    m_settingsMenu->addAction(m_reportFolderCountAction);
    m_settingsMenu->addSeparator();
    m_languageMenu = m_settingsMenu->addMenu(QString());
    m_languageMenu->addAction(m_portugueseAction);
    m_languageMenu->addAction(m_englishAction);
    m_statisticsMenu = m_settingsMenu->addMenu(QString());
    m_chartsMenu = m_statisticsMenu->addMenu(QString());
    m_chartsMenu->addAction(m_pieChartAction);
    m_chartsMenu->addAction(m_histogramAction);

    m_aboutMenu = menuBar()->addMenu(QString());
    m_aboutMenu->addAction(m_intellectualPropertyAction);
    m_aboutMenu->addAction(m_disclaimerAction);
    m_aboutMenu->addAction(m_mitLicenseAction);
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("Biblioteca local de e-books"));
    m_directoryEdit->setPlaceholderText(tr("Selecione o diretório pai da biblioteca..."));
    m_chooseButton->setText(tr("Escolher diretório"));
    m_scanButton->setText(tr("Listar arquivos"));
    m_clearButton->setText(tr("Limpar dados"));
    m_table->setHorizontalHeaderLabels({tr("Nome"), tr("Extensão"), tr("MIME"), tr("Tamanho"), tr("Caminho")});

    m_filesMenu->setTitle(tr("Arquivos"));
    m_settingsMenu->setTitle(tr("Configurações"));
    m_languageMenu->setTitle(tr("Idiomas"));
    m_statisticsMenu->setTitle(tr("Estatísticas"));
    m_chartsMenu->setTitle(tr("Gráficos"));
    m_aboutMenu->setTitle(tr("Sobre"));

    m_chooseDirectoryAction->setText(tr("Escolher diretório"));
    m_scanDirectoryAction->setText(tr("Listar arquivos"));
    m_clearDataAction->setText(tr("Limpar dados"));
    m_exitAction->setText(tr("Sair"));
    m_reportFileCountAction->setText(tr("Reportar quantidade de arquivos"));
    m_reportFolderCountAction->setText(tr("Reportar quantidade de pastas"));
    m_portugueseAction->setText(tr("Português"));
    m_englishAction->setText(tr("Inglês"));
    m_pieChartAction->setText(tr("Pizza"));
    m_histogramAction->setText(tr("Histograma"));
    m_intellectualPropertyAction->setText(tr("Propriedade intelectual"));
    m_disclaimerAction->setText(tr("Termo de Isenção de Responsabilidade"));
    m_mitLicenseAction->setText(tr("Licença MIT"));

    updateStatusReport();
}

void MainWindow::updateWindowIcon()
{
    const QString iconPath = findRuntimeFile(QStringLiteral("assets/icon/launcherebook2_256-256.ico"));
    if (!iconPath.isEmpty())
        setWindowIcon(QIcon(iconPath));
}

void MainWindow::chooseDirectory()
{
    const QString dir = QFileDialog::getExistingDirectory(this,
                                                         tr("Selecionar diretório pai da biblioteca"),
                                                         m_directoryEdit->text());
    if (!dir.isEmpty()) {
        m_directoryEdit->setText(dir);
        m_lastFolderCount = countFolders(dir);
        updateStatusReport(tr("Diretório selecionado."));
    }
}

bool MainWindow::isSupportedFile(const QString &filePath) const
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return m_supportedExtensions.contains(suffix);
}

QString MainWindow::humanReadableSize(qint64 bytes) const
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unit = 0;
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        ++unit;
    }
    return QString::number(size, 'f', unit == 0 ? 0 : 2) + ' ' + units[unit];
}

void MainWindow::scanDirectory()
{
    const QString root = m_directoryEdit->text().trimmed();
    if (root.isEmpty() || !QFileInfo(root).isDir()) {
        QMessageBox::warning(this, tr("Diretório inválido"),
                             tr("Escolha um diretório pai válido antes de listar os arquivos."));
        return;
    }

    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);

    QMimeDatabase mimeDb;
    int count = 0;
    m_lastFolderCount = countFolders(root);

    QDirIterator iterator(root,
                          QDir::Files | QDir::Readable | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        const QString filePath = iterator.next();
        if (!isSupportedFile(filePath))
            continue;

        const QFileInfo info(filePath);
        const QMimeType mime = mimeDb.mimeTypeForFile(info);

        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *nameItem = new QTableWidgetItem(info.fileName());
        nameItem->setData(Qt::UserRole, filePath);

        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, new QTableWidgetItem(info.suffix().toLower()));
        m_table->setItem(row, 2, new QTableWidgetItem(mime.name()));
        m_table->setItem(row, 3, new QTableWidgetItem(humanReadableSize(info.size())));
        m_table->setItem(row, 4, new QTableWidgetItem(QDir::toNativeSeparators(info.absoluteFilePath())));

        ++count;
        if (count % 100 == 0) {
            m_lastFileCount = count;
            updateStatusReport(tr("%1 total de itens reconhecidos...").arg(m_lastFileCount + m_lastFolderCount));
            qApp->processEvents();
        }
    }

    m_lastFileCount = count;
    m_table->resizeColumnsToContents();
    m_table->setSortingEnabled(true);
    updateStatusReport(tr("%1 total de itens reconhecidos.").arg(m_lastFileCount + m_lastFolderCount));
}

void MainWindow::clearData()
{
    m_table->setRowCount(0);
    m_directoryEdit->clear();
    m_lastFileCount = 0;
    m_lastFolderCount = 0;
    updateStatusReport(tr("Dados limpos."));
}

void MainWindow::openFileAtRow(int row, int)
{
    if (row < 0)
        return;

    QTableWidgetItem *item = m_table->item(row, 0);
    if (!item)
        return;

    const QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, tr("Arquivo indisponível"),
                             tr("O arquivo selecionado não existe mais no caminho listado."));
        return;
    }

    const QVector<AssociatedApp> apps = AssociatedApps::appsForFile(filePath);
    AppChooserDialog dialog(filePath, apps, this);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const AssociatedApp app = dialog.selectedApp();
    QString error;
    if (!AssociatedApps::openWith(app, filePath, &error))
        QMessageBox::critical(this, tr("Falha ao abrir"), error);
}

void MainWindow::reportFileCount()
{
    m_lastFileCount = m_table->rowCount();
    updateStatusReport(m_reportFileCountAction->isChecked()
                           ? tr("Relatório de arquivos ativado.")
                           : tr("Relatório de arquivos desativado."));
    if (!m_loadingSettings)
        saveSettings();
}

void MainWindow::reportFolderCount()
{
    const QString root = m_directoryEdit->text().trimmed();
    if (!root.isEmpty() && QFileInfo(root).isDir())
        m_lastFolderCount = countFolders(root);

    updateStatusReport(m_reportFolderCountAction->isChecked()
                           ? tr("Relatório de pastas ativado.")
                           : tr("Relatório de pastas desativado."));
    if (!m_loadingSettings)
        saveSettings();
}

void MainWindow::setPortugueseLanguage()
{
    setLanguage(QStringLiteral("pt_BR"));
}

void MainWindow::setEnglishLanguage()
{
    setLanguage(QStringLiteral("en_US"));
}

void MainWindow::showPieChart()
{
    showChartDialog(tr("Gráfico de pizza por extensão"), true);
}

void MainWindow::showHistogram()
{
    showChartDialog(tr("Histograma por extensão"), false);
}

void MainWindow::showIntellectualProperty()
{
    showAboutDocument(tr("Propriedade intelectual"), QStringLiteral("intellectual_property"));
}

void MainWindow::showDisclaimer()
{
    showAboutDocument(tr("Termo de Isenção de Responsabilidade"), QStringLiteral("disclaimer"));
}

void MainWindow::showMitLicense()
{
    showAboutDocument(tr("Licença MIT"), QStringLiteral("mit_license"));
}

int MainWindow::countFolders(const QString &rootPath) const
{
    int count = 0;
    QDirIterator iterator(rootPath,
                          QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        ++count;
    }
    return count;
}

QMap<QString, int> MainWindow::extensionCounts() const
{
    QMap<QString, int> counts;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *item = m_table->item(row, 1);
        if (!item)
            continue;

        const QString extension = item->text().trimmed().toLower();
        if (!extension.isEmpty())
            counts[extension] += 1;
    }
    return counts;
}

QString MainWindow::findRuntimeFile(const QString &relativePath) const
{
    const QString cleanRelative = QDir::fromNativeSeparators(relativePath);
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString sourceDir = QString::fromUtf8(EBOOKLAUNCHER_SOURCE_DIR);

    const QStringList candidates = {
        QDir(appDir).filePath(cleanRelative),
        QDir(appDir).filePath(QStringLiteral("../") + cleanRelative),
        sourceDir.isEmpty() ? QString() : QDir(sourceDir).filePath(cleanRelative),
        QDir::current().filePath(cleanRelative)
    };

    for (const QString &candidate : candidates) {
        if (!candidate.isEmpty() && QFileInfo::exists(candidate))
            return QFileInfo(candidate).absoluteFilePath();
    }

    return {};
}

QString MainWindow::aboutFilePath(const QString &fileName) const
{
    const QString localized = QStringLiteral("assets/about/%1_%2.txt").arg(fileName, m_currentLanguage);
    QString path = findRuntimeFile(localized);
    if (!path.isEmpty())
        return path;

    return findRuntimeFile(QStringLiteral("assets/about/%1_pt_BR.txt").arg(fileName));
}

QString MainWindow::readTextFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return tr("Não foi possível carregar o arquivo: %1").arg(QDir::toNativeSeparators(filePath));

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

void MainWindow::showAboutDocument(const QString &title, const QString &fileName)
{
    const QString filePath = aboutFilePath(fileName);
    const QString text = filePath.isEmpty()
                             ? tr("Arquivo de texto não encontrado para esta seção.")
                             : readTextFile(filePath);

    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setWindowIcon(windowIcon());
    dialog.resize(760, 520);

    auto *layout = new QVBoxLayout(&dialog);
    auto *versionLabel = new QLabel(tr("<b>Launcher EBook</b><br>Versão: %1").arg(QStringLiteral(EBOOKLAUNCHER_VERSION)),
                                    &dialog);
    layout->addWidget(versionLabel);

    auto *textEdit = new QPlainTextEdit(&dialog);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(text);
    layout->addWidget(textEdit, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::showChartDialog(const QString &title, bool pieChart)
{
    const QMap<QString, int> counts = extensionCounts();
    if (counts.isEmpty()) {
        QMessageBox::information(this, title, tr("Liste arquivos antes de abrir os gráficos."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setWindowIcon(windowIcon());
    dialog.resize(680, 460);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new ExtensionChartWidget(counts, pieChart, &dialog), 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::setLanguage(const QString &localeName)
{
    setLanguageInternal(localeName, !m_loadingSettings);
}

bool MainWindow::setLanguageInternal(const QString &localeName, bool saveAfterChange)
{
    const QString normalizedLocale = localeName == QStringLiteral("en_US")
                                         ? QStringLiteral("en_US")
                                         : QStringLiteral("pt_BR");

    if (normalizedLocale == m_currentLanguage) {
        const QSignalBlocker portugueseBlocker(m_portugueseAction);
        const QSignalBlocker englishBlocker(m_englishAction);
        m_portugueseAction->setChecked(m_currentLanguage == QStringLiteral("pt_BR"));
        m_englishAction->setChecked(m_currentLanguage == QStringLiteral("en_US"));
        if (saveAfterChange)
            saveSettings();
        return true;
    }

    qApp->removeTranslator(&m_translator);
    m_currentLanguage = QStringLiteral("pt_BR");

    if (normalizedLocale != QStringLiteral("pt_BR")) {
        const QString qmPath = findRuntimeFile(QStringLiteral("locale/Launcher EBook_%1.qm").arg(normalizedLocale));
        if (!qmPath.isEmpty() && m_translator.load(qmPath)) {
            qApp->installTranslator(&m_translator);
            m_currentLanguage = normalizedLocale;
        } else {
            QMessageBox::warning(this, tr("Idioma indisponível"),
                                 tr("Não foi possível carregar o arquivo de idioma solicitado."));
        }
    }

    {
        const QSignalBlocker portugueseBlocker(m_portugueseAction);
        const QSignalBlocker englishBlocker(m_englishAction);
        m_portugueseAction->setChecked(m_currentLanguage == QStringLiteral("pt_BR"));
        m_englishAction->setChecked(m_currentLanguage == QStringLiteral("en_US"));
    }

    retranslateUi();

    if (saveAfterChange)
        saveSettings();

    return m_currentLanguage == normalizedLocale;
}

void MainWindow::loadSettings()
{
    m_loadingSettings = true;

    QFile file(settingsFilePath());
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject root = document.object();
            const QJsonObject settings = root.value(QStringLiteral("settings")).isObject()
                                             ? root.value(QStringLiteral("settings")).toObject()
                                             : root;

            {
                const QSignalBlocker fileCountBlocker(m_reportFileCountAction);
                const QSignalBlocker folderCountBlocker(m_reportFolderCountAction);
                m_reportFileCountAction->setChecked(settings.value(QStringLiteral("reportFileCount")).toBool(false));
                m_reportFolderCountAction->setChecked(settings.value(QStringLiteral("reportFolderCount")).toBool(false));
            }

            setLanguageInternal(settings.value(QStringLiteral("language")).toString(QStringLiteral("pt_BR")), false);
        } else {
            qWarning() << "Could not parse settings JSON:" << parseError.errorString();
        }
    }

    m_loadingSettings = false;
    saveSettings();
}

void MainWindow::saveSettings() const
{
    const QString path = settingsFilePath();
    QDir dir(QFileInfo(path).absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qWarning() << "Could not create settings directory:" << dir.absolutePath();
        return;
    }

    QJsonObject settings;
    settings.insert(QStringLiteral("reportFileCount"), m_reportFileCountAction && m_reportFileCountAction->isChecked());
    settings.insert(QStringLiteral("reportFolderCount"), m_reportFolderCountAction && m_reportFolderCountAction->isChecked());
    settings.insert(QStringLiteral("language"), m_currentLanguage);

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("settings"), settings);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not open settings file for writing:" << path;
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit())
        qWarning() << "Could not commit settings file:" << path;
}

QString MainWindow::settingsFilePath() const
{
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty())
        return QDir(localAppData).filePath(QStringLiteral("EBookLauncher") + QStringLiteral("Qt/settings.json"));

    QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (configDir.isEmpty())
        configDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config"));

    return QDir(configDir).filePath(QStringLiteral("EBookLauncher") + QStringLiteral("Qt/settings.json"));
}

void MainWindow::updateStatusReport(const QString &message)
{
    QStringList parts;
    if (!message.isEmpty())
        parts << message;

    if (m_reportFileCountAction && m_reportFileCountAction->isChecked())
        parts << tr("Arquivos: %1").arg(m_lastFileCount);

    if (m_reportFolderCountAction && m_reportFolderCountAction->isChecked())
        parts << tr("Pastas: %1").arg(m_lastFolderCount);

    if (parts.isEmpty())
        parts << tr("Pronto");

    m_statusLabel->setText(parts.join(QStringLiteral(" | ")));
}
