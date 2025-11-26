/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.9.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QPushButton *start;
    QTextEdit *textEdit;
    QPushButton *Exit;
    QComboBox *comboBox;
    QSlider *verticalSlider;
    QLabel *sliderLabel;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(800, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        start = new QPushButton(centralwidget);
        start->setObjectName("start");
        start->setGeometry(QRect(40, 460, 100, 41));
        textEdit = new QTextEdit(centralwidget);
        textEdit->setObjectName("textEdit");
        textEdit->setGeometry(QRect(360, 10, 311, 481));
        Exit = new QPushButton(centralwidget);
        Exit->setObjectName("Exit");
        Exit->setGeometry(QRect(160, 460, 100, 41));
        comboBox = new QComboBox(centralwidget);
        comboBox->setObjectName("comboBox");
        comboBox->setGeometry(QRect(10, 150, 291, 32));
        verticalSlider = new QSlider(centralwidget);
        verticalSlider->setObjectName("verticalSlider");
        verticalSlider->setGeometry(QRect(720, 40, 22, 451));
        verticalSlider->setOrientation(Qt::Orientation::Vertical);
        sliderLabel = new QLabel(centralwidget);
        sliderLabel->setObjectName("sliderLabel");
        sliderLabel->setGeometry(QRect(700, 10, 80, 20));
        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 37));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        start->setText(QCoreApplication::translate("MainWindow", "start", nullptr));
        Exit->setText(QCoreApplication::translate("MainWindow", "Exit", nullptr));
        sliderLabel->setText(QCoreApplication::translate("MainWindow", "Threads: 1", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
