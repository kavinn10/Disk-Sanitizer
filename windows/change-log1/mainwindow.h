#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QProgressDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <vector>
#include "driveutils.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSanitizeClicked();
    void onRefreshClicked();   // new: reload drives

private:
    QTableWidget *driveTable;
    QPushButton *sanitizeButton;
    QPushButton *refreshButton;   // new: refresh
    QProgressDialog *progressDialog; // optional: busy indicator

    std::vector<DriveInfo> drives; // store full drive info (with devicePath)

    void loadDrives();
};

#endif // MAINWINDOW_H
