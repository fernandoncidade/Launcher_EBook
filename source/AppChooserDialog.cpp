#include "AppChooserDialog.hpp"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

AppChooserDialog::AppChooserDialog(const QString &filePath,
                                   const QVector<AssociatedApp> &apps,
                                   QWidget *parent)
    : QDialog(parent),
      m_filePath(filePath),
      m_apps(apps)
{
    setWindowTitle("Escolher leitor");
    resize(820, 420);

    auto *layout = new QVBoxLayout(this);

    const QFileInfo info(filePath);
    auto *label = new QLabel("Escolha o aplicativo para abrir:\n" + info.fileName(), this);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(label);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({"Aplicativo", "Origem", "Executável"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table);

    populateTable();

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { accept(); });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_chooseOtherButton = buttons->addButton("Escolher outro executável...", QDialogButtonBox::ActionRole);
    connect(m_chooseOtherButton, &QPushButton::clicked, this, &AppChooserDialog::chooseExecutableManually);
    connect(buttons, &QDialogButtonBox::accepted, this, &AppChooserDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &AppChooserDialog::reject);
    layout->addWidget(buttons);
}

AssociatedApp AppChooserDialog::selectedApp() const
{
    int row = selectedRow();
    if (row < 0 || row >= m_apps.size())
        return {};
    return m_apps[row];
}

void AppChooserDialog::populateTable()
{
    m_table->setRowCount(m_apps.size());
    for (int row = 0; row < m_apps.size(); ++row) {
        const AssociatedApp &app = m_apps[row];
        m_table->setItem(row, 0, new QTableWidgetItem(app.displayName));
        m_table->setItem(row, 1, new QTableWidgetItem(app.source));
        m_table->setItem(row, 2, new QTableWidgetItem(app.executablePath));
    }

    m_table->resizeColumnsToContents();
    if (!m_apps.isEmpty())
        m_table->selectRow(0);
}

int AppChooserDialog::selectedRow() const
{
    const QList<QTableWidgetSelectionRange> ranges = m_table->selectedRanges();
    if (ranges.isEmpty())
        return -1;
    return ranges.first().topRow();
}

void AppChooserDialog::chooseExecutableManually()
{
#ifdef Q_OS_WIN
    const QString filter = "Executáveis (*.exe);;Todos os arquivos (*)";
#else
    const QString filter = "Todos os arquivos (*)";
#endif

    const QString exe = QFileDialog::getOpenFileName(this,
                                                     "Escolher aplicativo leitor",
                                                     QString(),
                                                     filter);
    if (exe.isEmpty())
        return;

    QFileInfo exeInfo(exe);
    AssociatedApp app;
    app.kind = AssociatedApp::Kind::Executable;
    app.displayName = exeInfo.completeBaseName().isEmpty() ? exe : exeInfo.completeBaseName();
    app.executablePath = exe;
    app.source = "Selecionado manualmente pelo usuário";

    m_apps.push_back(app);
    populateTable();
    m_table->selectRow(m_apps.size() - 1);
}

void AppChooserDialog::accept()
{
    if (selectedRow() < 0) {
        QMessageBox::warning(this, "Nenhum aplicativo selecionado",
                             "Selecione um aplicativo para abrir o arquivo.");
        return;
    }
    QDialog::accept();
}
