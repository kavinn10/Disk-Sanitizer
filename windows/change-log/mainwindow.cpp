#include "certificateutils.h"
#include "mainwindow.h"
#include "driveutils.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QJsonObject>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    driveTable = new QTableWidget(this);
    driveTable->setColumnCount(5);
    driveTable->setHorizontalHeaderLabels({"Model", "Type", "Size (GB)", "Serial", "Firmware"});
    driveTable->horizontalHeader()->setStretchLastSection(true);
    driveTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    driveTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    sanitizeButton = new QPushButton("Sanitize / Wipe", this);
    refreshButton = new QPushButton("Refresh Drives", this); // new

    // Buttons layout
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(refreshButton);
    btnLayout->addWidget(sanitizeButton);

    layout->addWidget(driveTable);
    layout->addLayout(btnLayout);
    setCentralWidget(central);

    loadDrives();

    connect(sanitizeButton, &QPushButton::clicked, this, &MainWindow::onSanitizeClicked);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
}

MainWindow::~MainWindow() {}

void MainWindow::loadDrives()
{
    drives = DriveUtils::getDrives();
    qDebug() << "Drives found:" << (int)drives.size();

    driveTable->clearContents();
    driveTable->setRowCount(0);

    if (drives.empty())
    {
        QMessageBox::warning(this, "Warning", "No drives detected! Try running as Administrator.");
        return;
    }
    

    driveTable->setRowCount(static_cast<int>(drives.size()));
    for (int row = 0; row < static_cast<int>(drives.size()); ++row)
    {
        const auto &d = drives[row];
        d.debugPrint(); // prints to console
        driveTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(d.model)));
        driveTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(d.type)));
        driveTable->setItem(row, 2, new QTableWidgetItem(QString::number(d.sizeGB)));
        driveTable->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(d.serial)));
        driveTable->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(d.firmware)));
    }
}

void MainWindow::onSanitizeClicked()
{
    int row = driveTable->currentRow();
    if (row < 0)
    {
        QMessageBox::warning(this, "Error", "Select a drive first!");
        return;
    }

    // Use stored DriveInfo (so devicePath is available for future real-wipe)
    const DriveInfo &selected = drives[row];

    // Confirm destructive action
    QMessageBox::StandardButton reply;
    reply = QMessageBox::warning(this, "Confirm Wipe",
                                 QString::fromStdString("You are about to wipe drive: " + selected.model + " (" + selected.devicePath + ")\nALL DATA WILL BE LOST!\n\nContinue?"),
                                 QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    // For now call dummyWipe (phase 1). Later replace with wipeDrive(selected.devicePath, passes, verify)
    bool success = DriveUtils::dummyWipe(selected.model, selected.type);
    if (success)
    {
        QMessageBox::information(this, "Done", "Sanitization (dummy) completed.");

        // 🔹 Build metadata for certificate
        QJsonObject metadata;
        metadata["deviceModel"] = QString::fromStdString(selected.model);   // model of drive
        metadata["serialNumber"] = QString::fromStdString(selected.serial); // serial number of drive
        metadata["type"] = QString::fromStdString(selected.type);           // HDD or SSD
        metadata["firmware"] = QString::fromStdString(selected.firmware);   // firmware version of drive
        metadata["sizeGB"] = QString::number(selected.sizeGB);              // size of drive in gb
        metadata["wipeMethod"] = "NIST SP 800-88 Clear (Dummy)";            // wipe method used
        metadata["passes"] = "1";                                           // number of passes
        metadata["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        // 🔹 Generate certificates
        CertificateUtils::generateJsonCertificate(metadata, "wipe_certificate.json");
        CertificateUtils::generatePdfCertificate(metadata, "wipe_certificate.pdf");

        // 🔹 Verify JSON integrity
        bool valid = CertificateUtils::verifyJsonCertificate("wipe_certificate.json");
        if (valid)
        {
            QMessageBox::information(this, "Certificate", "Wipe certificate generated and verified successfully.");
        }
        else
        {
            QMessageBox::critical(this, "Certificate Error", "Failed to verify generated certificate!");
        }
    }
}

void MainWindow::onRefreshClicked()
{
    loadDrives();
    QMessageBox::information(this, "Drives Refreshed", "Drive list updated successfully!");
}