#include <iostream>
#include <QDebug>
#include "ui_slider.h"
#include "ctrl.h"
#include "model.h"
#include "mainwindow.h"


using namespace std;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    // init values
    ui->setupUi(this);

    // connect stuff here

    // Model *m = new Model();

    // Ctrl  *c = new Ctrl();
    Model *m = new Model();

    // any user change on GUI will be sent to controller, which then will update
    // the model
    //
    // QObject::connect(ui.pushButton_go, SIGNAL(clicked()), c, SLOT(go())); //
    // old syntax
    // QObject::connect(ui.horizontalSlider, SIGNAL(valueChanged()), c,
    // SLOT(set_step(var))); // old syntax
    QObject::connect(ui->pushButton_go,       &QPushButton::pressed,
                     m, &Model::go);
    QObject::connect(ui->horizontalSlider,    &QSlider::valueChanged,
                     m, &Model::set_mm);

    QObject::connect(ui->doubleSpinBox_goPos, &QDoubleSpinBox::valueChanged,
                     m, &Model::set_mm);
    QObject::connect(ui->doubleSpinBox_llim,  &QDoubleSpinBox::valueChanged,
                     m, &Model::set_mm_llim);
    QObject::connect(ui->doubleSpinBox_ulim,  &QDoubleSpinBox::valueChanged,
                     m, &Model::set_mm_ulim);

    QObject::connect(ui->pushButton_p1,       &QPushButton::pressed,
                     m, &Model::set_step_p1);
    QObject::connect(ui->pushButton_m1,       &QPushButton::pressed,
                     m, &Model::set_step_m1);
    QObject::connect(ui->pushButton_p10,      &QPushButton::pressed,
                     m, &Model::set_step_p10);
    QObject::connect(ui->pushButton_m10,      &QPushButton::pressed,
                     m, &Model::set_step_p10);


    // let the model be able to update the GUI
    // QObject::connect(
    //     m, &Model::valueChanged, ui->label_currentPos,
    //     static_cast<void (QLabel::*)(int)>(&QLabel::setNum) // amazingly ugly
    //     );

    QObject::connect(m, &Model::updatePosLabel,
                     ui->label_currentPos, &QLabel::setText);

    QObject::connect(m, &Model::updateHSliderPos,
                     ui->horizontalSlider, &QSlider::setValue);

    QObject::connect(m, &Model::updateHSliderLim,
                     ui->horizontalSlider, &QSlider::setRange);

    ui->doubleSpinBox_llim->setValue(m->get_step_llim());
    ui->doubleSpinBox_ulim->setValue(m->get_step_ulim());

    qDebug() << ui->horizontalSlider->minimum() << "min";
    qDebug() << ui->horizontalSlider->maximum() << "max";

    ui->horizontalSlider->setRange(m->get_step_llim(), m->get_step_ulim());
    ui->horizontalSlider->setValue(m->get_step_current());

    qDebug() << ui->horizontalSlider->minimum() << "min";
    qDebug() << ui->horizontalSlider->maximum() << "max";

    // m->init();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::update()
{}
