#include <iostream>
#include "model.h"
#include "ui_slider.h"
#include "limits.h"

#include <QDebug>

using namespace std;

Model::Model()
{
    mm_per_step  = 40.0 / 1000.0;           // mm per step
    step_current = 0;
    step_goto    = 0;
    step_llim    = (int)LLIM / mm_per_step; // lower step limit
    step_ulim    = (int)ULIM / mm_per_step; // upper step limit
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

void Model::init()
{}

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
    emit updateHSliderPos(step_current);

    print_state();
}

void Model::set_step(int var)
{
    step_goto = var;
    emit updateSpinBoxPos(step_goto * mm_per_step);
    emit updateHSliderPos(var);
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
    emit updateHSliderLim(var, step_ulim);
}

void Model::set_step_ulim(int var)
{
    step_ulim = var;
    emit updateHSliderLim(step_llim, var);
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

double Model::get_step_llim(void)
{
    return step_llim;
}

double Model::get_step_ulim(void)
{
    return step_ulim;
}

double Model::get_step_current(void)
{
    return step_current;
}

double Model::get_mm_llim(void)
{
    return step_llim * mm_per_step;
}

double Model::get_mm_ulim(void)
{
    return step_ulim * mm_per_step;
}

void Model::updatePos()
{
    QString qstr =
        QString("%1 mm").arg(qFabs(step_current * mm_per_step), 0, 'f', 3);
    emit updatePosLabel(qstr);
}

void Model::print_state()
{
    qDebug() << "step_current" << step_current;
    qDebug() << "step_goto" << step_goto;
    qDebug() << "step_goto_mm" << step_goto * mm_per_step;
    qDebug() << "step_llim" << step_llim;
    qDebug() << "step_ulim" << step_ulim;
}
