#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace mocks {

class CompileTranslationsWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit CompileTranslationsWindow(QWidget *parent = nullptr);

private:
    QString defaultProjectRoot() const;
    QString defaultLocaleDir() const;
    QString defaultLreleasePath() const;
    QStringList translationSources() const;
    QString qmPathForTs(const QString &tsPath) const;
    void appendLog(const QString &text);
    bool compileOne(const QString &tsPath);

    void chooseLocaleDir();
    void chooseLrelease();
    void compileAll();
    void cleanQmFiles();

    QLineEdit *m_localeDirEdit = nullptr;
    QLineEdit *m_lreleaseEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QPushButton *m_compileButton = nullptr;
    QPushButton *m_cleanButton = nullptr;
};

}  // namespace mocks
