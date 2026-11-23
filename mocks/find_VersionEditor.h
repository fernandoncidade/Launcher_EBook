#pragma once

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

namespace mocks {

class VersionEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit VersionEditor(QWidget *parent = nullptr);

private:
    QString projectRoot() const;
    QStringList targetFiles() const;
    QString readFile(const QString &path, bool *ok) const;
    bool writeFile(const QString &path, const QString &text, QString *error) const;
    QString replaceVersionLines(const QString &text, const QString &version) const;

    void chooseProjectRoot();
    void inspectFiles();
    void applyVersion();

    QLineEdit *m_rootEdit = nullptr;
    QLineEdit *m_versionEdit = nullptr;
    QPlainTextEdit *m_log = nullptr;
};

}  // namespace mocks
