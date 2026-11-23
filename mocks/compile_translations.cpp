#include "compile_translations.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

#ifndef EBOOK_PROJECT_ROOT_PATH
#define EBOOK_PROJECT_ROOT_PATH ""
#endif

#ifndef EBOOK_MOCK_QT_ROOT_PATH
#define EBOOK_MOCK_QT_ROOT_PATH ""
#endif

namespace mocks {

CompileTranslationsWindow::CompileTranslationsWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Compilar traducoes - Launcher EBook"));
    resize(820, 520);

    auto *root = new QVBoxLayout(this);

    auto *localeLayout = new QHBoxLayout();
    localeLayout->addWidget(new QLabel(QStringLiteral("Diretorio locale:"), this));
    m_localeDirEdit = new QLineEdit(defaultLocaleDir(), this);
    localeLayout->addWidget(m_localeDirEdit, 1);
    auto *chooseLocaleButton = new QPushButton(QStringLiteral("Selecionar..."), this);
    localeLayout->addWidget(chooseLocaleButton);
    root->addLayout(localeLayout);

    auto *toolLayout = new QHBoxLayout();
    toolLayout->addWidget(new QLabel(QStringLiteral("lrelease:"), this));
    m_lreleaseEdit = new QLineEdit(defaultLreleasePath(), this);
    toolLayout->addWidget(m_lreleaseEdit, 1);
    auto *chooseToolButton = new QPushButton(QStringLiteral("Selecionar..."), this);
    toolLayout->addWidget(chooseToolButton);
    root->addLayout(toolLayout);

    auto *buttons = new QHBoxLayout();
    m_compileButton = new QPushButton(QStringLiteral("Compilar .ts para .qm"), this);
    m_cleanButton = new QPushButton(QStringLiteral("Limpar .qm"), this);
    buttons->addWidget(m_compileButton);
    buttons->addWidget(m_cleanButton);
    buttons->addStretch(1);
    root->addLayout(buttons);

    m_statusLabel = new QLabel(QStringLiteral("Pronto"), this);
    root->addWidget(m_statusLabel);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    root->addWidget(m_log, 1);

    connect(chooseLocaleButton, &QPushButton::clicked, this, &CompileTranslationsWindow::chooseLocaleDir);
    connect(chooseToolButton, &QPushButton::clicked, this, &CompileTranslationsWindow::chooseLrelease);
    connect(m_compileButton, &QPushButton::clicked, this, &CompileTranslationsWindow::compileAll);
    connect(m_cleanButton, &QPushButton::clicked, this, &CompileTranslationsWindow::cleanQmFiles);
}

QString CompileTranslationsWindow::defaultProjectRoot() const
{
    const QString configured = QString::fromUtf8(EBOOK_PROJECT_ROOT_PATH);
    if (!configured.isEmpty())
        return QDir::cleanPath(configured);
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../.."));
}

QString CompileTranslationsWindow::defaultLocaleDir() const
{
    return QDir(defaultProjectRoot()).filePath(QStringLiteral("locale"));
}

QString CompileTranslationsWindow::defaultLreleasePath() const
{
    const QString configured = QString::fromUtf8(EBOOK_MOCK_QT_ROOT_PATH);
#ifdef Q_OS_WIN
    const QString exeName = QStringLiteral("lrelease.exe");
#else
    const QString exeName = QStringLiteral("lrelease");
#endif
    if (!configured.isEmpty()) {
        const QString candidate = QDir(configured).filePath(QStringLiteral("bin/") + exeName);
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return exeName;
}

QStringList CompileTranslationsWindow::translationSources() const
{
    const QDir localeDir(m_localeDirEdit->text().trimmed());
    return {
        localeDir.filePath(QStringLiteral("Launcher EBook_pt_BR.ts")),
        localeDir.filePath(QStringLiteral("Launcher EBook_en_US.ts"))
    };
}

QString CompileTranslationsWindow::qmPathForTs(const QString &tsPath) const
{
    QFileInfo info(tsPath);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".qm"));
}

void CompileTranslationsWindow::appendLog(const QString &text)
{
    m_log->appendPlainText(text);
}

bool CompileTranslationsWindow::compileOne(const QString &tsPath)
{
    if (!QFileInfo::exists(tsPath)) {
        appendLog(QStringLiteral("[ERRO] Nao encontrado: %1").arg(QDir::toNativeSeparators(tsPath)));
        return false;
    }

    const QString qmPath = qmPathForTs(tsPath);
    QProcess process;
    process.setProgram(m_lreleaseEdit->text().trimmed());
    process.setArguments({QStringLiteral("-silent"), tsPath, QStringLiteral("-qm"), qmPath});
    process.setWorkingDirectory(QFileInfo(tsPath).absolutePath());
    process.setProcessChannelMode(QProcess::MergedChannels);

    appendLog(QStringLiteral("Compilando %1").arg(QFileInfo(tsPath).fileName()));
    process.start();
    if (!process.waitForStarted(5000)) {
        appendLog(QStringLiteral("[ERRO] Falha ao iniciar lrelease: %1").arg(process.errorString()));
        return false;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        appendLog(QStringLiteral("[ERRO] Timeout ao compilar."));
        return false;
    }

    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    if (!output.isEmpty())
        appendLog(output);

    const bool ok = process.exitStatus() == QProcess::NormalExit &&
                    process.exitCode() == 0 &&
                    QFileInfo(qmPath).exists() &&
                    QFileInfo(qmPath).size() > 0;
    appendLog(ok ? QStringLiteral("[OK] %1").arg(QFileInfo(qmPath).fileName())
                 : QStringLiteral("[ERRO] Falha ao gerar %1").arg(QFileInfo(qmPath).fileName()));
    return ok;
}

void CompileTranslationsWindow::chooseLocaleDir()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Selecionar locale"), m_localeDirEdit->text());
    if (!dir.isEmpty())
        m_localeDirEdit->setText(dir);
}

void CompileTranslationsWindow::chooseLrelease()
{
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Selecionar lrelease"), m_lreleaseEdit->text());
    if (!file.isEmpty())
        m_lreleaseEdit->setText(file);
}

void CompileTranslationsWindow::compileAll()
{
    m_log->clear();
    appendLog(QStringLiteral("Inicio: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));

    int okCount = 0;
    const QStringList files = translationSources();
    for (const QString &file : files) {
        if (compileOne(file))
            ++okCount;
    }

    m_statusLabel->setText(QStringLiteral("Compilados: %1/%2").arg(okCount).arg(files.size()));
}

void CompileTranslationsWindow::cleanQmFiles()
{
    int removed = 0;
    for (const QString &tsPath : translationSources()) {
        const QString qmPath = qmPathForTs(tsPath);
        if (QFileInfo::exists(qmPath) && QFile::remove(qmPath)) {
            appendLog(QStringLiteral("[REMOVIDO] %1").arg(QFileInfo(qmPath).fileName()));
            ++removed;
        }
    }
    m_statusLabel->setText(QStringLiteral("Arquivos .qm removidos: %1").arg(removed));
}

}  // namespace mocks

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    mocks::CompileTranslationsWindow window;
    window.show();
    return app.exec();
}
