#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui_slider.h"
#include <QMainWindow>
#include <QtNetwork/QTcpSocket>

namespace Ui {
    class MainWindow;
}

class MainWindow: public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void update();

private:
    Ui::MainWindow *ui;
    QTcpSocket *tcpSocket;
    int blockSize;

};

#endif // MAINWINDOW_H
