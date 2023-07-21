#include <iostream>
#include <QDebug>

#include "ui_slider.h"
#include "model.h"
#include "mainwindow.h"


using namespace std;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    // init values
    ui->setupUi(this);

    Model *m = new Model();

    QObject::connect(ui->pushButton_go,       &QPushButton::pressed,
                     m, &Model::go);
    QObject::connect(ui->pushButton_zero,     &QPushButton::pressed,
                     m, &Model::zero);
    QObject::connect(ui->horizontalSlider,    &QSlider::valueChanged,
                     m, &Model::set_step_goto);

    QObject::connect(ui->doubleSpinBox_goPos, &QDoubleSpinBox::valueChanged,
                     m, &Model::set_mm_goto);
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
                     m, &Model::set_step_m10);


    // let the model be able to update the GUI
    // QObject::connect(
    //     m, &Model::valueChanged, ui->label_currentPos,
    //     static_cast<void (QLabel::*)(int)>(&QLabel::setNum) // amazingly ugly
    //     );

    QObject::connect(m, &Model::updatePosLabel,
                     ui->label_currentPos, &QLabel::setText);

    QObject::connect(m, &Model::updateHSliderPos,
                     ui->horizontalSlider, &QSlider::setValue);

    QObject::connect(m, &Model::updateSpinBoxPos,
                     ui->doubleSpinBox_goPos, &QDoubleSpinBox::setValue);

    QObject::connect(m, &Model::updateHSliderLim,
                     ui->horizontalSlider, &QSlider::setRange);

    QObject::connect(m, &Model::updateGotoBoxLlim,
                     ui->doubleSpinBox_goPos, &QDoubleSpinBox::setMinimum);
    QObject::connect(m, &Model::updateGotoBoxUlim,
                     ui->doubleSpinBox_goPos, &QDoubleSpinBox::setMaximum);

    QObject::connect(m, &Model::updateDSLlim,
                     ui->doubleSpinBox_llim, &QDoubleSpinBox::setValue);
    QObject::connect(m, &Model::updateDSUlim,
                     ui->doubleSpinBox_ulim, &QDoubleSpinBox::setValue);


    ui->doubleSpinBox_llim->setValue(m->get_step_llim());
    ui->doubleSpinBox_ulim->setValue(m->get_step_ulim());

    ui->doubleSpinBox_goPos->setMinimum(m->get_mm_llim());
    ui->doubleSpinBox_goPos->setMaximum(m->get_mm_ulim());
    ui->doubleSpinBox_goPos->setSingleStep(m->get_mm_per_step());

    qDebug() << m->get_mm_llim() << "mm_llim";
    qDebug() << m->get_mm_ulim() << "mm_ulim";

    qDebug() << ui->horizontalSlider->minimum() << "min";
    qDebug() << ui->horizontalSlider->maximum() << "max";

    ui->horizontalSlider->setRange(m->get_step_llim(), m->get_step_ulim());
    ui->horizontalSlider->setValue(m->get_step_current());

    qDebug() << ui->horizontalSlider->minimum() << "min";
    qDebug() << ui->horizontalSlider->maximum() << "max";
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::update()
{}
