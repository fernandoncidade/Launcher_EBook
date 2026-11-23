#include "AssociatedApps.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QUrl>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shlwapi.h>
#  include <shellapi.h>
#endif

namespace
{
QString normalizedExtensionForFile(const QString &filePath)
{
    QString suffix = QFileInfo(filePath).suffix().trimmed().toLower();
    if (suffix.isEmpty())
        return {};
    return "." + suffix;
}

void addUnique(QVector<AssociatedApp> &apps, QSet<QString> &seen, const AssociatedApp &app)
{
    QString key;
    if (app.kind == AssociatedApp::Kind::SystemDefault) {
        key = "__system_default__";
    } else {
        key = QFileInfo(app.executablePath).canonicalFilePath().toLower();
        if (key.isEmpty())
            key = app.executablePath.toLower();
    }

    if (key.isEmpty() || seen.contains(key))
        return;

    seen.insert(key);
    apps.push_back(app);
}

#ifdef Q_OS_WIN
std::wstring toStdWString(const QString &s)
{
    return std::wstring(reinterpret_cast<const wchar_t *>(s.utf16()), static_cast<size_t>(s.size()));
}

QString fromWide(const wchar_t *value)
{
    if (!value)
        return {};
    return QString::fromWCharArray(value);
}

QString expandEnvironmentStringsIfNeeded(const QString &text)
{
    if (!text.contains('%'))
        return text;

    std::wstring in = toStdWString(text);
    DWORD required = ExpandEnvironmentStringsW(in.c_str(), nullptr, 0);
    if (required == 0)
        return text;

    std::wstring out(required, L'\0');
    DWORD written = ExpandEnvironmentStringsW(in.c_str(), out.data(), required);
    if (written == 0 || written > required)
        return text;

    return fromWide(out.c_str());
}

QString readRegistryString(HKEY root, const QString &subKey, const QString &valueName = QString())
{
    std::wstring key = toStdWString(subKey);
    std::wstring value = toStdWString(valueName);

    DWORD type = 0;
    DWORD bytes = 0;
    LONG rc = RegGetValueW(root,
                           key.c_str(),
                           valueName.isEmpty() ? nullptr : value.c_str(),
                           RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                           &type,
                           nullptr,
                           &bytes);
    if (rc != ERROR_SUCCESS || bytes == 0)
        return {};

    std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
    rc = RegGetValueW(root,
                      key.c_str(),
                      valueName.isEmpty() ? nullptr : value.c_str(),
                      RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                      &type,
                      buffer.data(),
                      &bytes);
    if (rc != ERROR_SUCCESS)
        return {};

    QString result = fromWide(buffer.c_str()).trimmed();
    return expandEnvironmentStringsIfNeeded(result);
}

QStringList enumRegistryValueNames(HKEY root, const QString &subKey)
{
    QStringList names;
    HKEY keyHandle = nullptr;
    std::wstring key = toStdWString(subKey);

    if (RegOpenKeyExW(root, key.c_str(), 0, KEY_READ, &keyHandle) != ERROR_SUCCESS)
        return names;

    DWORD valueCount = 0;
    DWORD maxValueNameLen = 0;
    if (RegQueryInfoKeyW(keyHandle, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                         &valueCount, &maxValueNameLen, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        for (DWORD i = 0; i < valueCount; ++i) {
            std::wstring name(maxValueNameLen + 2, L'\0');
            DWORD nameLen = static_cast<DWORD>(name.size());
            DWORD type = 0;

            LONG rc = RegEnumValueW(keyHandle, i, name.data(), &nameLen,
                                    nullptr, &type, nullptr, nullptr);
            if (rc == ERROR_SUCCESS && nameLen > 0) {
                QString n = QString::fromWCharArray(name.data(), static_cast<int>(nameLen)).trimmed();
                if (!n.isEmpty() && n.compare("MRUList", Qt::CaseInsensitive) != 0)
                    names << n;
            }
        }
    }

    RegCloseKey(keyHandle);
    names.removeDuplicates();
    return names;
}

QStringList enumRegistryStringValues(HKEY root, const QString &subKey)
{
    QStringList values;
    HKEY keyHandle = nullptr;
    std::wstring key = toStdWString(subKey);

    if (RegOpenKeyExW(root, key.c_str(), 0, KEY_READ, &keyHandle) != ERROR_SUCCESS)
        return values;

    DWORD valueCount = 0;
    DWORD maxValueNameLen = 0;
    DWORD maxValueLen = 0;
    if (RegQueryInfoKeyW(keyHandle, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                         &valueCount, &maxValueNameLen, &maxValueLen, nullptr, nullptr) == ERROR_SUCCESS) {
        for (DWORD i = 0; i < valueCount; ++i) {
            std::wstring name(maxValueNameLen + 2, L'\0');
            DWORD nameLen = static_cast<DWORD>(name.size());
            DWORD type = 0;
            std::wstring data((maxValueLen / sizeof(wchar_t)) + 2, L'\0');
            DWORD dataBytes = static_cast<DWORD>(data.size() * sizeof(wchar_t));

            LONG rc = RegEnumValueW(keyHandle, i, name.data(), &nameLen,
                                    nullptr, &type, reinterpret_cast<LPBYTE>(data.data()), &dataBytes);
            if (rc == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
                QString n = QString::fromWCharArray(name.data(), static_cast<int>(nameLen));
                if (n.compare("MRUList", Qt::CaseInsensitive) == 0)
                    continue;
                QString value = fromWide(data.c_str()).trimmed();
                if (!value.isEmpty())
                    values << expandEnvironmentStringsIfNeeded(value);
            }
        }
    }

    RegCloseKey(keyHandle);
    values.removeDuplicates();
    return values;
}

QString executableFromCommand(const QString &command)
{
    QString c = expandEnvironmentStringsIfNeeded(command).trimmed();
    if (c.isEmpty())
        return {};

    if (c.startsWith('"')) {
        int endQuote = c.indexOf('"', 1);
        if (endQuote > 1)
            return c.mid(1, endQuote - 1).trimmed();
    }

    int exeIndex = c.toLower().indexOf(".exe");
    if (exeIndex >= 0)
        return c.left(exeIndex + 4).trimmed();

    int firstSpace = c.indexOf(' ');
    if (firstSpace > 0)
        return c.left(firstSpace).trimmed();

    return c;
}

QString defaultExecutableForExtension(const QString &extension)
{
    if (extension.isEmpty())
        return {};

    std::wstring ext = toStdWString(extension);
    DWORD required = 0;
    HRESULT hr = AssocQueryStringW(ASSOCF_NONE,
                                   ASSOCSTR_EXECUTABLE,
                                   ext.c_str(),
                                   L"open",
                                   nullptr,
                                   &required);
    if (hr != S_FALSE || required == 0)
        return {};

    std::wstring buffer(required, L'\0');
    hr = AssocQueryStringW(ASSOCF_NONE,
                           ASSOCSTR_EXECUTABLE,
                           ext.c_str(),
                           L"open",
                           buffer.data(),
                           &required);
    if (FAILED(hr))
        return {};

    return fromWide(buffer.c_str()).trimmed();
}

QString commandFromProgId(const QString &progId)
{
    if (progId.isEmpty())
        return {};

    const QString suffix = progId + "\\shell\\open\\command";
    QString command = readRegistryString(HKEY_CURRENT_USER, "Software\\Classes\\" + suffix);
    if (!command.isEmpty())
        return command;

    command = readRegistryString(HKEY_LOCAL_MACHINE, "Software\\Classes\\" + suffix);
    if (!command.isEmpty())
        return command;

    return readRegistryString(HKEY_CLASSES_ROOT, suffix);
}

QString friendlyNameFromProgId(const QString &progId)
{
    QString name = readRegistryString(HKEY_CURRENT_USER, "Software\\Classes\\" + progId);
    if (!name.isEmpty())
        return name;

    name = readRegistryString(HKEY_LOCAL_MACHINE, "Software\\Classes\\" + progId);
    if (!name.isEmpty())
        return name;

    name = readRegistryString(HKEY_CLASSES_ROOT, progId);
    if (!name.isEmpty())
        return name;

    return progId;
}

QString commandFromApplicationExeName(const QString &exeName)
{
    if (exeName.isEmpty())
        return {};

    const QString suffix = "Applications\\" + exeName + "\\shell\\open\\command";
    QString command = readRegistryString(HKEY_CURRENT_USER, "Software\\Classes\\" + suffix);
    if (!command.isEmpty())
        return command;

    command = readRegistryString(HKEY_LOCAL_MACHINE, "Software\\Classes\\" + suffix);
    if (!command.isEmpty())
        return command;

    return readRegistryString(HKEY_CLASSES_ROOT, suffix);
}

void addExecutableFromCommand(QVector<AssociatedApp> &apps,
                              QSet<QString> &seen,
                              const QString &name,
                              const QString &command,
                              const QString &source)
{
    QString exe = executableFromCommand(command);
    if (exe.isEmpty())
        return;

    QFileInfo exeInfo(exe);
    AssociatedApp app;
    app.kind = AssociatedApp::Kind::Executable;
    app.executablePath = exe;
    app.displayName = !name.trimmed().isEmpty()
                          ? name.trimmed()
                          : (!exeInfo.completeBaseName().isEmpty() ? exeInfo.completeBaseName() : exe);
    app.source = source;

    addUnique(apps, seen, app);
}
#endif
}

QVector<AssociatedApp> AssociatedApps::appsForFile(const QString &filePath)
{
    QVector<AssociatedApp> apps;
    QSet<QString> seen;

    AssociatedApp systemDefault;
    systemDefault.kind = AssociatedApp::Kind::SystemDefault;
    systemDefault.displayName = "Aplicativo padrão do sistema";
    systemDefault.source = "Associação padrão do sistema operacional";
    addUnique(apps, seen, systemDefault);

#ifdef Q_OS_WIN
    const QString extension = normalizedExtensionForFile(filePath);

    const QString defaultExe = defaultExecutableForExtension(extension);
    if (!defaultExe.isEmpty()) {
        QFileInfo info(defaultExe);
        AssociatedApp app;
        app.kind = AssociatedApp::Kind::Executable;
        app.displayName = info.completeBaseName().isEmpty() ? defaultExe : info.completeBaseName();
        app.executablePath = defaultExe;
        app.source = "Aplicativo padrão registrado no Windows";
        addUnique(apps, seen, app);
    }

    QStringList progIds;
    progIds << readRegistryString(HKEY_CURRENT_USER, "Software\\Classes\\" + extension);
    progIds << readRegistryString(HKEY_LOCAL_MACHINE, "Software\\Classes\\" + extension);
    progIds << readRegistryString(HKEY_CLASSES_ROOT, extension);
    progIds << enumRegistryValueNames(HKEY_CURRENT_USER,
                                      "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + extension + "\\OpenWithProgids");
    progIds << enumRegistryValueNames(HKEY_CURRENT_USER,
                                      "Software\\Classes\\" + extension + "\\OpenWithProgids");
    progIds << enumRegistryValueNames(HKEY_LOCAL_MACHINE,
                                      "Software\\Classes\\" + extension + "\\OpenWithProgids");
    progIds << enumRegistryValueNames(HKEY_CLASSES_ROOT,
                                      extension + "\\OpenWithProgids");
    progIds.removeAll(QString());
    progIds.removeDuplicates();

    for (const QString &progId : progIds) {
        const QString command = commandFromProgId(progId);
        addExecutableFromCommand(apps, seen, friendlyNameFromProgId(progId), command,
                                 "ProgID registrado: " + progId);
    }

    QStringList appExeNames;
    appExeNames << enumRegistryStringValues(HKEY_CURRENT_USER,
                                            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + extension + "\\OpenWithList");
    appExeNames << enumRegistryValueNames(HKEY_CLASSES_ROOT,
                                          extension + "\\OpenWithList");
    appExeNames.removeDuplicates();

    for (const QString &exeName : appExeNames) {
        const QString command = commandFromApplicationExeName(exeName);
        addExecutableFromCommand(apps, seen, QFileInfo(exeName).completeBaseName(), command,
                                 "Lista Abrir com do Windows");
    }
#endif

    return apps;
}

bool AssociatedApps::openWithSystemDefault(const QString &filePath, QString *errorMessage)
{
    const bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    if (!ok && errorMessage)
        *errorMessage = "Não foi possível abrir o arquivo com o aplicativo padrão do sistema.";
    return ok;
}

bool AssociatedApps::openWith(const AssociatedApp &app, const QString &filePath, QString *errorMessage)
{
    if (app.kind == AssociatedApp::Kind::SystemDefault)
        return openWithSystemDefault(filePath, errorMessage);

    if (app.executablePath.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = "Nenhum executável foi definido para abrir o arquivo.";
        return false;
    }

#ifdef Q_OS_WIN
    std::wstring exe = toStdWString(app.executablePath);
    std::wstring params = toStdWString('"' + QDir::toNativeSeparators(filePath) + '"');

    HINSTANCE result = ShellExecuteW(nullptr,
                                     L"open",
                                     exe.c_str(),
                                     params.c_str(),
                                     nullptr,
                                     SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        if (errorMessage)
            *errorMessage = "O Windows recusou a abertura do arquivo com o executável selecionado.";
        return false;
    }
    return true;
#else
    const bool ok = QProcess::startDetached(app.executablePath, QStringList{filePath});
    if (!ok && errorMessage)
        *errorMessage = "Não foi possível iniciar o aplicativo selecionado.";
    return ok;
#endif
}
