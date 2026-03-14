#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QListWidget>

namespace {
QStringList SelectedAttributes(QListWidget* list)
{
    QStringList attrs;
    const auto items = list->selectedItems();
    for (const auto* item : items)
        attrs.append(item->text());
    return attrs;
}
}

void MainWindow::SetupInstrumentUI()
{
    connect(ui->instrumentLoadSystemAttrsBtn, SIGNAL(pressed()), this, SLOT(OnInstrumentLoadSystemAttrsClicked()));
    connect(ui->instrumentLoadProcessAttrsBtn, SIGNAL(pressed()), this, SLOT(OnInstrumentLoadProcessAttrsClicked()));
    connect(ui->instrumentMonitorBtn, SIGNAL(pressed()), this, SLOT(OnInstrumentStartStopMonitorClicked()));
    connect(ui->instrumentGetProcessListBtn, SIGNAL(pressed()), this, SLOT(OnInstrumentGetProcessListClicked()));
    connect(ui->instrumentFpsBtn, SIGNAL(pressed()), this, SLOT(OnInstrumentStartStopFpsClicked()));
    connect(ui->instrumentClearLogBtn, SIGNAL(pressed()), this, SLOT(OnInstrumentClearLogClicked()));
    connect(DeviceBridge::Get(), SIGNAL(InstrumentLogReceived(QString)), this, SLOT(OnInstrumentLogReceived(QString)));

    ui->instrumentSystemAttrs->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->instrumentProcessAttrs->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->instrumentMonitorInterval->setText("1000");
    ui->instrumentFpsInterval->setText("1000");
}

void MainWindow::OnInstrumentLoadSystemAttrsClicked()
{
    ui->instrumentSystemAttrs->clear();
    const auto attrs = DeviceBridge::Get()->GetAttributes(DeviceBridge::AttrType::SYSTEM);
    ui->instrumentSystemAttrs->addItems(attrs);
    ui->instrumentLogEdit->appendPlainText(QString("Loaded %1 system attributes").arg(attrs.size()));
}

void MainWindow::OnInstrumentLoadProcessAttrsClicked()
{
    ui->instrumentProcessAttrs->clear();
    const auto attrs = DeviceBridge::Get()->GetAttributes(DeviceBridge::AttrType::PROCESS);
    ui->instrumentProcessAttrs->addItems(attrs);
    ui->instrumentLogEdit->appendPlainText(QString("Loaded %1 process attributes").arg(attrs.size()));
}

void MainWindow::OnInstrumentStartStopMonitorClicked()
{
    if (ui->instrumentMonitorBtn->text().contains("Start", Qt::CaseInsensitive))
    {
        bool ok = false;
        const int interval = ui->instrumentMonitorInterval->text().toInt(&ok);
        const auto systemAttrs = SelectedAttributes(ui->instrumentSystemAttrs);
        const auto processAttrs = SelectedAttributes(ui->instrumentProcessAttrs);
        if (!ok || interval <= 0)
        {
            ui->instrumentLogEdit->appendPlainText("Invalid monitor interval. Use a positive integer.");
            return;
        }
        DeviceBridge::Get()->StartMonitor(static_cast<unsigned int>(interval), systemAttrs, processAttrs);
        ui->instrumentMonitorBtn->setText("Stop Monitor");
        ui->instrumentLogEdit->appendPlainText(
            QString("Started monitor. Interval=%1 ms, systemAttrs=%2, processAttrs=%3")
                .arg(interval).arg(systemAttrs.size()).arg(processAttrs.size()));
        return;
    }

    DeviceBridge::Get()->StopMonitor();
    ui->instrumentMonitorBtn->setText("Start Monitor");
    ui->instrumentLogEdit->appendPlainText("Stopped monitor");
}

void MainWindow::OnInstrumentGetProcessListClicked()
{
    DeviceBridge::Get()->GetProcessList();
    ui->instrumentLogEdit->appendPlainText("Requested process list");
}

void MainWindow::OnInstrumentStartStopFpsClicked()
{
    if (ui->instrumentFpsBtn->text().contains("Start", Qt::CaseInsensitive))
    {
        bool ok = false;
        const int interval = ui->instrumentFpsInterval->text().toInt(&ok);
        if (!ok || interval <= 0)
        {
            ui->instrumentLogEdit->appendPlainText("Invalid FPS interval. Use a positive integer.");
            return;
        }
        DeviceBridge::Get()->StartFPS(static_cast<unsigned int>(interval));
        ui->instrumentFpsBtn->setText("Stop FPS");
        ui->instrumentLogEdit->appendPlainText(QString("Started FPS monitor. Interval=%1 ms").arg(interval));
        return;
    }

    DeviceBridge::Get()->StopFPS();
    ui->instrumentFpsBtn->setText("Start FPS");
    ui->instrumentLogEdit->appendPlainText("Stopped FPS monitor");
}

void MainWindow::OnInstrumentClearLogClicked()
{
    ui->instrumentLogEdit->clear();
}

void MainWindow::OnInstrumentLogReceived(QString logs)
{
    ui->instrumentLogEdit->appendPlainText(logs);
}
