#pragma once

#include "AssociatedApps.hpp"

#include <QDialog>
#include <QVector>

class QLabel;
class QPushButton;
class QTableWidget;

class AppChooserDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AppChooserDialog(const QString &filePath,
                              const QVector<AssociatedApp> &apps,
                              QWidget *parent = nullptr);

    AssociatedApp selectedApp() const;

private slots:
    void chooseExecutableManually();
    void accept() override;

private:
    void populateTable();
    int selectedRow() const;

    QString m_filePath;
    QVector<AssociatedApp> m_apps;
    QTableWidget *m_table = nullptr;
    QPushButton *m_chooseOtherButton = nullptr;
};
