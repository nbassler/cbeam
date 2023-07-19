#include <iostream>
#include "ctrl.h"
#include "model.h"
#include "ui_slider.h"
#include <QDebug>

// Testwindow::Testwindow(QString text, QWidget *parent) : QMainWindow(parent)
Ctrl::Ctrl()
{
}


void Ctrl::aaa()
{
    std::cout << "ctrl hey ho aaa\n";
}


void Ctrl::zero()
{
    std::cout << "ctrl hey ho bbb\n";
}


void Ctrl::go()
{
    std::cout << "ctrl hey ho sss\n";
}


void Ctrl::set_step(int var)
{
    qDebug() << "ctrl hey ho var\n" << var;
    emit valueChanged(var);
}
