#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSanitizeClicked();

private:
    QTableWidget *driveTable;
    QPushButton *sanitizeButton;
    void loadDrives();
};

#endif // MAINWINDOW_H
