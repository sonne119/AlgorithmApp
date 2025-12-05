#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "CollatzRunner.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void logMessageReceived(const QString& message);

private slots:
    void on_start_clicked();
    void exitClicked();
    void sliderValueChanged(int value);
    void appendLogToUI(const QString& message);

private:
    Ui::MainWindow *ui;
    CollatzRunner runner;
    int algorithmChoice = 0; // 0=Standard(8-Way), 1=SIMD, 2=16-Way
};
#endif // MAINWINDOW_H
