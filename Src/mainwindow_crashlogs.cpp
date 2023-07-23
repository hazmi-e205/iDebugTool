#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils.h"
#include "crashsymbolicator.h"
#include <QMessageBox>
#include <QDesktopServices>
#include <QFile>

void MainWindow::SetupCrashlogsUI()
{
    if (!m_stacktraceModel) {
        m_stacktraceModel = new QStandardItemModel();
        ui->stacktraceTable->setModel(m_stacktraceModel);
        ui->stacktraceTable->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        ui->stacktraceTable->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
        ui->stacktraceTable->setVerticalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
        ui->stacktraceTable->setHorizontalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
        ui->stacktraceTable->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        ui->stacktraceTable->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
        ui->stacktraceTable->setShowGrid(false);

        connect(ui->syncCrashlogsBtn, SIGNAL(pressed()), this, SLOT(OnSyncCrashlogsClicked()));
        connect(DeviceBridge::Get(), SIGNAL(CrashlogsStatusChanged(QString)), this, SLOT(OnCrashlogsStatusChanged(QString)));
        connect(ui->crashlogBtn, SIGNAL(pressed()), this, SLOT(OnCrashlogClicked()));
        connect(ui->crashlogsDirBtn, SIGNAL(pressed()), this, SLOT(OnCrashlogsDirClicked()));
        connect(ui->dsymBtn, SIGNAL(pressed()), this, SLOT(OnDsymClicked()));
        connect(ui->dwarfBtn, SIGNAL(pressed()), this, SLOT(OnDwarfClicked()));
        connect(ui->symbolicateBtn, SIGNAL(pressed()), this, SLOT(OnSymbolicateClicked()));
        connect(ui->saveSymbolicatedBtn, SIGNAL(pressed()), this, SLOT(OnSaveSymbolicatedClicked()));
        connect(CrashSymbolicator::Get(), SIGNAL(SymbolicateResult2(unsigned int,SymbolicatedData,bool)), this, SLOT(OnSymbolicateResult2(unsigned int,SymbolicatedData,bool)));
        connect(ui->threadEdit, SIGNAL(textActivated(QString)), this, SLOT(OnStacktraceThreadChanged(QString)));
    }
    m_stacktraceModel->setHorizontalHeaderItem(0, new QStandardItem("Binary"));
    m_stacktraceModel->setHorizontalHeaderItem(1, new QStandardItem("Line"));
    m_stacktraceModel->setHorizontalHeaderItem(2, new QStandardItem("Function"));
    ui->stacktraceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeMode::ResizeToContents);
    ui->stacktraceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);
    ui->stacktraceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeMode::ResizeToContents);
    ui->stacktraceTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
}

void MainWindow::OnSyncCrashlogsClicked()
{
    DeviceBridge::Get()->SyncCrashlogs(GetDirectory(DIRECTORY_TYPE::CRASHLOGS));
    ui->bottomWidget->setCurrentIndex(1);
}

void MainWindow::OnCrashlogsDirClicked()
{
    if (!QDesktopServices::openUrl(GetDirectory(DIRECTORY_TYPE::CRASHLOGS)))
    {
        ui->bottomWidget->setCurrentIndex(1);
        ui->outputEdit->appendPlainText("No synchronized crashlogs from device.");
    }
}

void MainWindow::OnCrashlogsStatusChanged(QString text)
{
    ui->outputEdit->appendPlainText(text);
}

void MainWindow::OnCrashlogClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_FILE, "Crashlog", this);
    ui->crashlogEdit->setText(filepath);
}

void MainWindow::OnDsymClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_DIR, "dSYM", this);
    ui->dsymEdit->setText(filepath);
}

void MainWindow::OnDwarfClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_FILE, "DWARF", this);
    ui->dsymEdit->setText(filepath);
}

void MainWindow::OnSymbolicateClicked()
{
    QString crashpath = ui->crashlogEdit->text();
    QString dsympath = ui->dsymEdit->text();
    CrashSymbolicator::Get()->Process(crashpath, dsympath);

    m_stacktraceModel->clear();
    ui->threadEdit->clear();
    SetupCrashlogsUI();
}

void MainWindow::OnSaveSymbolicatedClicked()
{
    QString data = m_lastStacktrace.rawString;
    if (data.isEmpty())
        return;

    QString filepath = ShowBrowseDialog(BROWSE_TYPE::SAVE_FILE, "Symbolicated", this, "Text File (*.txt)");
    if (!filepath.isEmpty()) {
        QFile f(filepath);
        if (f.open(QIODevice::WriteOnly)) {
            QTextStream stream(&f);
            stream << data;
            f.close();
        }
    }
}

void MainWindow::OnStacktraceThreadChanged(QString threadName)
{
    m_stacktraceModel->clear();
    SetupCrashlogsUI();

    foreach (auto& stack, m_lastStacktrace.stackTraces)
    {
        if (stack.threadName == threadName)
        {
            foreach (auto& line, stack.lines)
            {
                QList<QStandardItem*> rowData;
                rowData << new QStandardItem(line.binary);
                rowData << new QStandardItem(line.line);
                rowData << new QStandardItem(line.function);
                m_stacktraceModel->appendRow(rowData);
            }
            return;
        }
    }
}

void MainWindow::OnSymbolicateResult2(unsigned int progress, SymbolicatedData data, bool error)
{
    if (error)
    {
        QMessageBox::critical(this, "Error", data.rawString, QMessageBox::Ok);
        ui->statusbar->showMessage("Symbolication failed!");
    }
    else
    {
        if (progress == 100)
        {
            ui->statusbar->showMessage("Symbolication success!");
            m_lastStacktrace = data;
            foreach (auto& stack, data.stackTraces) {
                ui->threadEdit->addItem(stack.threadName);
            }
            OnStacktraceThreadChanged(ui->threadEdit->currentText());
            m_loading->close();
        }
        else
        {
            if (!m_loading->isActiveWindow())
                m_loading->ShowProgress("Symbolicate crashlog...");
            m_loading->SetProgress(progress, "Processing...");
        }
    }
}
