#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QComboBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->comboBox->clear();
    ui->comboBox->addItem("1 Million", 1000000ULL);
    ui->comboBox->addItem("700 Million",700000000ULL);
    ui->comboBox->addItem("1 Billion", 1000000000ULL);
    ui->comboBox->addItem("5 Billion", 5000000000ULL);
    ui->comboBox->addItem("9 Billion", 9000000000ULL);
    ui->comboBox->setCurrentIndex(1);

    ui->verticalSlider->setMinimum(1);
    ui->verticalSlider->setMaximum(12);
    ui->verticalSlider->setValue(12);
    ui->sliderLabel->setText(QString("Threads: %1").arg(ui->verticalSlider->value()));
    
    ui->textEdit->setReadOnly(true);
    ui->textEdit->append("Collatz Ready\n");

    connect(ui->Exit, &QPushButton::clicked, this, &MainWindow::exitClicked);
    connect(ui->verticalSlider, &QSlider::valueChanged, this, [this](int value) {
        ui->sliderLabel->setText(QString("Threads: %1").arg(value));
    });
    connect(ui->verticalSlider, &QSlider::valueChanged,
            this, &MainWindow::sliderValueChanged);
    connect(this, &MainWindow::logMessageReceived,
            this, &MainWindow::appendLogToUI,
            Qt::QueuedConnection);

    connect(ui->radioSIMD, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            algorithmChoice = 1;
            ui->textEdit->append("Algorithm: SIMD Vector\n");
        }
    });
    connect(ui->radio16Way, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            algorithmChoice = 2;
            ui->textEdit->append("Algorithm: 16-Way Parallel\n");
        }
    });
}

void MainWindow::sliderValueChanged(int value)
{
    ui->sliderLabel->setText(QString("Threads: %1").arg(value));
    runner.threadCount = value;
}

void MainWindow::appendLogToUI(const QString& message)
{
    ui->textEdit->append(message);

    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textEdit->setTextCursor(cursor);
}

void MainWindow::on_start_clicked()
{
    uint64_t count = ui->comboBox->currentData().toULongLong();

    ui->start->setEnabled(false);
    ui->comboBox->setEnabled(false);
    ui->radioSIMD->setEnabled(false);
    ui->radio16Way->setEnabled(false);
    QString algoName = (algorithmChoice == 1) ? "SIMD Vector" : 
                       (algorithmChoice == 2) ? "16-Way Parallel" : "8-Way Parallel";
    ui->textEdit->append(QString("\nComputing %1 numbers using %2 algorithm...\n").arg(count).arg(algoName));

    auto *watcher = new QFutureWatcher<CollatzResult>(this);
    connect(watcher, &QFutureWatcher<CollatzResult>::finished, this, [this, watcher]() {
        CollatzResult result = watcher->result();

        QString output;
        output.append("\n============ Results ===========\n");
        output.append("Limit: " + QString::number(result.limit) + "\n");
        output.append("Seconds: " + QString::number(result.seconds, 'f', 3) + " s\n");
        output.append("Throughput: " + QString::number(result.throughput, 'f', 3) + " Billion/sec\n");
        output.append("Max Length: " + QString::number(result.longest_len) +
                      " (seed=" + QString::number(result.longest_seed) + ")\n");
        output.append("Max Peak: " + QString::number(result.max_peak) + "\n");

        if (result.first_overflow != (uint64_t)INT64_MAX) {
            output.append("Overflow Seed: " + QString::number(result.first_overflow) + "\n");
        } else {
            output.append("Overflow Seed: NONE\n");
        }

        ui->textEdit->append(output);
        ui->start->setEnabled(true);
        ui->comboBox->setEnabled(true);
        ui->radioSIMD->setEnabled(true);
        ui->radio16Way->setEnabled(true);
        watcher->deleteLater();
    });

    runner.limit = count;
    QFuture<CollatzResult> future = QtConcurrent::run([this]() {
        if (algorithmChoice == 1) {
            // SIMD Vector
            return runner.Compute_simd([this](const std::string& msg) {
                QString qmsg = QString::fromStdString(msg);
                QMetaObject::invokeMethod(this, "appendLogToUI",
                                          Qt::QueuedConnection,
                                          Q_ARG(QString, qmsg));
            });
        } else if (algorithmChoice == 2) {
            // 16-Way Parallel (currently uses same as 8-Way)
            return runner.Compute([this](const std::string& msg) {
                QString qmsg = QString::fromStdString(msg);
                QMetaObject::invokeMethod(this, "appendLogToUI",
                                          Qt::QueuedConnection,
                                          Q_ARG(QString, qmsg));
            });
        } else {
            // 8-Way Parallel (default)
            return runner.Compute([this](const std::string& msg) {
                QString qmsg = QString::fromStdString(msg);
                QMetaObject::invokeMethod(this, "appendLogToUI",
                                          Qt::QueuedConnection,
                                          Q_ARG(QString, qmsg));
            });
        }
    });

    watcher->setFuture(future);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::exitClicked()
{
    QApplication::quit();
}
