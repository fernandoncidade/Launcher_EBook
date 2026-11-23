#include "find_VersionEditor.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>
#include <QVBoxLayout>

#ifndef EBOOK_PROJECT_ROOT_PATH
#define EBOOK_PROJECT_ROOT_PATH ""
#endif

namespace mocks {
namespace {

QString normalizeVersion(const QString &raw)
{
    QString trimmed = raw.trimmed();
    if (trimmed.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        trimmed.remove(0, 1);
    return trimmed.trimmed();
}

bool isValidVersion(const QString &version)
{
    static const QRegularExpression pattern(QStringLiteral(R"(^[0-9]{4}\.[0-9]{1,2}\.[0-9]{1,2}\.[0-9]+$)"));
    return pattern.match(version).hasMatch();
}

QStringList excludedRelativePaths()
{
    return {
        QStringLiteral("assets/release/RELEASE NOTES_en_US.txt"),
        QStringLiteral("assets/release/RELEASE NOTES_pt_BR.txt")
    };
}

bool isExcludedFile(const QDir &root, const QString &filePath)
{
    const QString targetClean = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    for (const QString &rel : excludedRelativePaths()) {
        const QString excludedClean = QDir::cleanPath(QFileInfo(root.filePath(rel)).absoluteFilePath());
        if (targetClean.compare(excludedClean, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

QString replaceWithCapturedGroups(const QString &input,
                                  const QRegularExpression &regex,
                                  const QString &version,
                                  int prefixGroup,
                                  int suffixGroup = -1)
{
    QString result;
    result.reserve(input.size());

    qsizetype lastPos = 0;
    QRegularExpressionMatchIterator it = regex.globalMatch(input);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        result += input.mid(lastPos, match.capturedStart() - lastPos);
        result += match.captured(prefixGroup);
        result += version;
        if (suffixGroup > 0)
            result += match.captured(suffixGroup);
        lastPos = match.capturedEnd();
    }
    result += input.mid(lastPos);
    return result;
}

QStringList detectVersionsInText(const QString &text)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"((?<![A-Za-z0-9])[vV]?20[0-9]{2}\.[0-9]{1,2}\.[0-9]{1,2}\.[0-9]+(?![0-9]))"));
    QStringList found;
    QRegularExpressionMatchIterator it = pattern.globalMatch(text);
    while (it.hasNext()) {
        const QString captured = it.next().captured(0);
        if (!found.contains(captured))
            found << captured;
    }
    return found;
}

}  // namespace

VersionEditor::VersionEditor(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Editor de versao - Launcher EBook"));
    resize(820, 520);

    auto *root = new QVBoxLayout(this);

    auto *folderLayout = new QHBoxLayout();
    folderLayout->addWidget(new QLabel(QStringLiteral("Projeto:"), this));
    m_rootEdit = new QLineEdit(projectRoot(), this);
    folderLayout->addWidget(m_rootEdit, 1);
    auto *chooseButton = new QPushButton(QStringLiteral("Selecionar..."), this);
    folderLayout->addWidget(chooseButton);
    root->addLayout(folderLayout);

    auto *versionLayout = new QHBoxLayout();
    versionLayout->addWidget(new QLabel(QStringLiteral("Versao:"), this));
    const QDate today = QDate::currentDate();
    m_versionEdit = new QLineEdit(QStringLiteral("%1.%2.%3.0").arg(today.year()).arg(today.month()).arg(today.day()), this);
    versionLayout->addWidget(m_versionEdit);
    auto *inspectButton = new QPushButton(QStringLiteral("Verificar"), this);
    auto *saveButton = new QPushButton(QStringLiteral("Atualizar versoes"), this);
    versionLayout->addWidget(inspectButton);
    versionLayout->addWidget(saveButton);
    root->addLayout(versionLayout);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    root->addWidget(m_log, 1);

    connect(chooseButton, &QPushButton::clicked, this, &VersionEditor::chooseProjectRoot);
    connect(inspectButton, &QPushButton::clicked, this, &VersionEditor::inspectFiles);
    connect(saveButton, &QPushButton::clicked, this, &VersionEditor::applyVersion);

    inspectFiles();
}

QString VersionEditor::projectRoot() const
{
    const QString configured = QString::fromUtf8(EBOOK_PROJECT_ROOT_PATH);
    if (!configured.isEmpty())
        return QDir::cleanPath(configured);

    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString &rel : {QStringLiteral("/../.."), QStringLiteral("/../../.."), QStringLiteral("/../../../..")}) {
        const QString candidate = QDir::cleanPath(appDir + rel);
        if (QFileInfo::exists(QDir(candidate).filePath(QStringLiteral("CMakeLists.txt"))))
            return candidate;
    }
    return QDir::cleanPath(appDir + QStringLiteral("/../.."));
}

QStringList VersionEditor::targetFiles() const
{
    const QDir root(m_rootEdit->text().trimmed());
    const QStringList candidates = {
        root.filePath(QStringLiteral("assets/about/intellectual_property_pt_BR.txt")),
        root.filePath(QStringLiteral("assets/about/intellectual_property_en_US.txt")),
        root.filePath(QStringLiteral("assets/about/disclaimer_pt_BR.txt")),
        root.filePath(QStringLiteral("assets/about/disclaimer_en_US.txt")),
        root.filePath(QStringLiteral("CMakeLists.txt")),
        root.filePath(QStringLiteral("repo/Launcher_EBook.iss")),
        root.filePath(QStringLiteral("repo/Launcher_EBook_mingw.iss")),
        root.filePath(QStringLiteral("repo/Launcher_EBook_msvc.iss"))
    };

    QStringList files;
    files.reserve(candidates.size());
    for (const QString &candidate : candidates) {
        if (!isExcludedFile(root, candidate))
            files << candidate;
    }
    return files;
}

QString VersionEditor::readFile(const QString &path, bool *ok) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (ok)
            *ok = false;
        return {};
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    if (ok)
        *ok = true;
    return stream.readAll();
}

bool VersionEditor::writeFile(const QString &path, const QString &text, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << text;
    return true;
}

QString VersionEditor::replaceVersionLines(const QString &text, const QString &version) const
{
    const QString normalized = normalizeVersion(version);
    if (normalized.isEmpty())
        return text;

    static const QRegularExpression ptAboutRegex(
        QStringLiteral(R"(^(Vers[a\x{00E3}]o:[^\S\r\n]*[vV]?)[0-9]+(?:\.[0-9]+){2,3}[^\S\r\n]*$)"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression enAboutRegex(
        QStringLiteral(R"(^(Version:[^\S\r\n]*[vV]?)[0-9]+(?:\.[0-9]+){2,3}[^\S\r\n]*$)"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression cmakeRegex(
        QStringLiteral(R"((project\(\s*LauncherEBook\s+VERSION\s+[vV]?)[0-9]+(?:\.[0-9]+){2,3})"));
    static const QRegularExpression innoRegex(
        QStringLiteral(R"(^([^\S\r\n]*#define[^\S\r\n]+MyAppVersion[^\S\r\n]+"[vV]?)[0-9]+(?:\.[0-9]+){2,3}("))"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression calendarVersionRegex(
        QStringLiteral(R"((?<![A-Za-z0-9])([vV]?)20[0-9]{2}\.[0-9]{1,2}\.[0-9]{1,2}\.[0-9]+(?![0-9]))"));

    QString updated = text;
    updated = replaceWithCapturedGroups(updated, ptAboutRegex, normalized, 1);
    updated = replaceWithCapturedGroups(updated, enAboutRegex, normalized, 1);
    updated = replaceWithCapturedGroups(updated, cmakeRegex, normalized, 1);
    updated = replaceWithCapturedGroups(updated, innoRegex, normalized, 1, 2);
    updated = replaceWithCapturedGroups(updated, calendarVersionRegex, normalized, 1);
    return updated;
}

void VersionEditor::chooseProjectRoot()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Selecionar projeto"), m_rootEdit->text());
    if (!dir.isEmpty()) {
        m_rootEdit->setText(dir);
        inspectFiles();
    }
}

void VersionEditor::inspectFiles()
{
    const QDir root(m_rootEdit->text().trimmed());
    QStringList lines;
    lines << QStringLiteral("Arquivos monitorados:");
    for (const QString &path : targetFiles()) {
        if (!QFileInfo::exists(path)) {
            lines << QStringLiteral("[FALTA] %1").arg(QDir::toNativeSeparators(path));
            continue;
        }

        bool ok = false;
        const QString content = readFile(path, &ok);
        const QStringList versions = ok ? detectVersionsInText(content) : QStringList{};
        const QString suffix = versions.isEmpty()
                                   ? QString()
                                   : QStringLiteral(" (%1)").arg(versions.join(QStringLiteral(", ")));
        lines << QStringLiteral("[OK] %1%2").arg(QDir::toNativeSeparators(path), suffix);
    }

    lines << QString();
    lines << QStringLiteral("Arquivos preservados (excluidos da atualizacao):");
    for (const QString &rel : excludedRelativePaths())
        lines << QStringLiteral("[IGNORADO] %1").arg(QDir::toNativeSeparators(root.filePath(rel)));

    m_log->setPlainText(lines.join(QLatin1Char('\n')));
}

void VersionEditor::applyVersion()
{
    const QString normalized = normalizeVersion(m_versionEdit->text());
    if (normalized.isEmpty() || !isValidVersion(normalized)) {
        m_log->setPlainText(QStringLiteral("[ERRO] Versao invalida. Use o formato YYYY.M.D.N ou vYYYY.M.D.N."));
        return;
    }

    const QDir root(m_rootEdit->text().trimmed());
    QStringList lines;
    for (const QString &path : targetFiles()) {
        if (isExcludedFile(root, path)) {
            lines << QStringLiteral("[IGNORADO] %1").arg(QDir::toNativeSeparators(path));
            continue;
        }

        bool ok = false;
        const QString original = readFile(path, &ok);
        if (!ok) {
            lines << QStringLiteral("[FALTA] %1").arg(QDir::toNativeSeparators(path));
            continue;
        }

        const QString updated = replaceVersionLines(original, normalized);
        if (updated == original) {
            lines << QStringLiteral("[SEM ALTERACAO] %1").arg(QDir::toNativeSeparators(path));
            continue;
        }

        QString error;
        if (writeFile(path, updated, &error))
            lines << QStringLiteral("[OK] %1 -> %2").arg(QDir::toNativeSeparators(path), normalized);
        else
            lines << QStringLiteral("[ERRO] %1: %2").arg(QDir::toNativeSeparators(path), error);
    }
    m_log->setPlainText(lines.join(QLatin1Char('\n')));
}

}  // namespace mocks

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    mocks::VersionEditor editor;
    editor.show();
    return app.exec();
}
