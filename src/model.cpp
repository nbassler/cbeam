#include <iostream>
#include "model.h"
#include "ui_slider.h"

#include <QDebug>

using namespace std;

Model::Model()
{
    mm_per_step  = 40.0 / 1000.0;            // mm per step
    step_current = 0;
    step_goto    = 0;
    step_llim    = 0;                        // lower step limit
    step_ulim    = (int)300.0 / mm_per_step; // upper step limit
}

void go();
void zero();

void set_step(int);
void set_step_llim(int);
void set_step_ulim(int);
void set_step_delta(double);

void set_mm(double);
void set_mm_llim(double);
void set_mm_ulim(double);
void set_mm_delta(double);

void Model::aaa()
{
    std::cout << "hey ho aaa\n";
}

void Model::zero()
{
    std::cout << "hey ho bbb\n";
}

void Model::go()
{
    std::cout << "b";

    while (step_current != step_goto)
    {
        if (step_goto > step_current)
            step_current++;
        else
            step_current--;
        std::cout << "z";
    }

    std::cout << "b...\n";
    updatePos();
    print_state();
}

void Model::set_step(int var)
{
    step_goto = var;
    emit valueChanged(step_goto * mm_per_step);
}

void Model::set_step_delta(int var)
{
    step_goto = step_current + (var / mm_per_step);
}

void Model::set_step_p1()
{
    step_goto = step_current + 1;
    go();
}

void Model::set_step_m1()
{
    step_goto = step_current - 1;
    go();
}

void Model::set_step_p10()
{
    step_goto = step_current + 10;
    go();
}

void Model::set_step_m10()
{
    step_goto = step_current - 10;
    go();
}

void Model::set_step_llim(int var)
{
    step_llim = var;
    print_state();
}

void Model::set_step_ulim(int var)
{
    step_ulim = var;
    print_state();
}

void Model::set_mm(double var)
{
    set_step((int)(var / mm_per_step));
}

void Model::set_mm_delta(double var)
{
    set_step((int)(step_current + var / mm_per_step));
}

void Model::set_mm_llim(double var)
{
    set_step_llim((int)(var / mm_per_step));
}

void Model::set_mm_ulim(double var)
{
    set_step_ulim((int)(var / mm_per_step));
}

void Model::updatePos()
{
    QString qstr =
        QString("%1 mm").arg(qFabs(step_current * mm_per_step), 3, 'f');

    // QString str;


    qDebug() << qstr;
    emit updatePosLabel(qstr);
}

void Model::print_state()
{
    qDebug() << "step_current" << step_current;
    qDebug() << "step_goto" << step_goto;
    qDebug() << "step_llim" << step_llim;
    qDebug() << "step_ulim" << step_ulim;
}
