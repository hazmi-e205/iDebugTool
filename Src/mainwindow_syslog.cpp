#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils.h"
#include <QFile>
#include <QTextDocument>
#include <QTextBlock>
#include <QScrollBar>

void MainWindow::SetupSyslogUI()
{
    ui->syslogEdit->setReadOnly(true);
    ui->syslogEdit->setWordWrapMode(QTextOption::NoWrap);
    QFont font;
    font.setFamily("monospace [Courier New]");
    font.setFixedPitch(true);
    font.setStyleHint(QFont::Monospace);
    ui->syslogEdit->setFont(font);

    connect(DeviceBridge::Get(), SIGNAL(SystemLogsReceived2(QString)), this, SLOT(OnSystemLogsReceived2(QString)));
    connect(DeviceBridge::Get(), SIGNAL(FilterStatusChanged(bool)), this, SLOT(OnFilterStatusChanged(bool)));
    connect(ui->searchEdit, SIGNAL(textChanged(QString)), this, SLOT(OnTextFilterChanged(QString)));
    connect(ui->pidEdit, SIGNAL(currentTextChanged(QString)), this, SLOT(OnPidFilterChanged(QString)));
    connect(ui->excludeEdit, SIGNAL(textChanged(QString)), this, SLOT(OnExcludeFilterChanged(QString)));
    connect(ui->stopCheck, SIGNAL(stateChanged(int)), this, SLOT(OnStopChecked(int)));
    connect(ui->clearBtn, SIGNAL(pressed()), this, SLOT(OnClearClicked()));
    connect(ui->saveBtn, SIGNAL(pressed()), this, SLOT(OnSaveClicked()));
    connect(ui->syslogEdit->verticalScrollBar(), SIGNAL(sliderMoved(int)), this, SLOT(OnSyslogSliderMoved(int)));

    DeviceBridge::Get()->SetMaxCachedLogs(m_maxCachedLogs);
    ui->maxShownLogs->setText(QString::number(m_maxCachedLogs));
    ui->pidEdit->addItems(QStringList() << "By user apps only" << "Related to user apps");
    ui->pidEdit->setCurrentIndex(0);
    DeviceBridge::Get()->LogsFilterByString(ui->searchEdit->text());
    DeviceBridge::Get()->LogsExcludeByString(ui->excludeEdit->text());
    DeviceBridge::Get()->LogsFilterByPID(ui->pidEdit->currentText());
}

void MainWindow::OnSyslogSliderMoved(int value)
{
    int max_value = ui->syslogEdit->verticalScrollBar()->maximum();
    if (ui->scrollCheck->isChecked())
    {
        if (value < max_value)
            ui->scrollCheck->setCheckState(Qt::CheckState::Unchecked);
    }
    else
    {
        if (value == max_value)
            ui->scrollCheck->setCheckState(Qt::CheckState::Checked);
    }
}

void MainWindow::OnSystemLogsReceived2(QString logs)
{
    ui->syslogEdit->appendPlainText(logs);
}

void MainWindow::OnFilterStatusChanged(bool isfiltering)
{
    if (isfiltering)
        ui->syslogEdit->setPlainText(QString("Filterring about %1 cached logs...").arg(m_maxCachedLogs));
    else
        ui->syslogEdit->clear();
}

void MainWindow::OnTextFilterChanged(QString text)
{
    ui->syslogEdit->clear();
    DeviceBridge::Get()->LogsFilterByString(text);
}

void MainWindow::OnPidFilterChanged(QString text)
{
    ui->syslogEdit->clear();
    DeviceBridge::Get()->LogsFilterByPID(text);
}

void MainWindow::OnExcludeFilterChanged(QString text)
{
    ui->syslogEdit->clear();
    DeviceBridge::Get()->LogsExcludeByString(text);
}

void MainWindow::OnStopChecked(int state)
{
    bool isStop = state == 0 ? false : true;
    DeviceBridge::Get()->CaptureSystemLogs(!isStop);
}

void MainWindow::OnClearClicked()
{
    bool is_stop = ui->stopCheck->isChecked();
    ui->stopCheck->setChecked(true);

    ui->syslogEdit->clear();
    DeviceBridge::Get()->ClearCachedLogs();

    ui->stopCheck->setChecked(is_stop);
}

void MainWindow::OnSaveClicked()
{
    bool is_stop = ui->stopCheck->isChecked();
    ui->stopCheck->setChecked(true);

    QString filepath = ShowBrowseDialog(BROWSE_TYPE::SAVE_FILE, "Log", this, "Text File (*.txt)");
    if (!filepath.isEmpty()) {
        QFile f(filepath);
        if (f.open(QIODevice::WriteOnly)) {
            QTextStream stream(&f);
            stream << ui->syslogEdit->toPlainText();
            f.close();
        }
    }
    ui->stopCheck->setChecked(is_stop);
}
