#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMutex>
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
    void appendLogToUI(const QString& message);
    void sliderValueChanged(int value);

private:
    Ui::MainWindow *ui;
    CollatzRunner runner;
};
#endif // MAINWINDOW_H
