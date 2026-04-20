#include "mainwindow.h"
#include "driveutils.h"
#include <QMessageBox>
#include <QHeaderView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    driveTable = new QTableWidget(this);
    driveTable->setColumnCount(7);  // increased to fit new fields
    driveTable->setHorizontalHeaderLabels({
        "Model",
        "Type",
        "Size (GB)",
        "Serial",
        "Firmware",
        "Max LBA",
        "Rotation (RPM)"
    });
    driveTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    driveTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    driveTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    sanitizeButton = new QPushButton("Sanitize Selected Drive", this);

    layout->addWidget(driveTable);
    layout->addWidget(sanitizeButton);
    setCentralWidget(central);

    loadDrives();

    connect(sanitizeButton, &QPushButton::clicked, this, &MainWindow::onSanitizeClicked);
}

MainWindow::~MainWindow() {}

void MainWindow::loadDrives() {
    auto drives = DriveUtils::getDrives();
    driveTable->setRowCount(drives.size());

    for (int i = 0; i < drives.size(); i++) {
        driveTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(drives[i].model)));
        driveTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(drives[i].type)));
        driveTable->setItem(i, 2, new QTableWidgetItem(QString::number(drives[i].sizeGB)));
        driveTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(drives[i].serial)));
        driveTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(drives[i].firmware)));
        driveTable->setItem(i, 5, new QTableWidgetItem(QString::number(drives[i].maxLBA)));
        driveTable->setItem(i, 6, new QTableWidgetItem(QString::number(drives[i].rotationRPM)));
    }
}

void MainWindow::onSanitizeClicked() {
    int row = driveTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "Error", "Select a drive first!");
        return;
    }

    QString model = driveTable->item(row, 0)->text();
    QString type = driveTable->item(row, 1)->text();

    // Dummy wipe (Phase 1/2 placeholder)
    bool success = DriveUtils::dummyWipe(model.toStdString(), type.toStdString());
    if (success) {
        QMessageBox::information(this, "Done", "Sanitization (dummy) completed.");
    } else {
        QMessageBox::critical(this, "Failed", "Sanitization failed.");
    }
}
