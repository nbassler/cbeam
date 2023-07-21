#include <iostream>
#include "model.h"
#include "ui_slider.h"
#include "limits.h"

#include <QDebug>

using namespace std;

Model::Model()
{
    mm_per_step  = CB_MM_PER_STEP;             // mm per step
    step_current = 0;
    step_goto    = 0;
    step_llim    = (int)CB_LLIM / mm_per_step; // lower step limit
    step_ulim    = (int)CB_ULIM / mm_per_step; // upper step limit
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


void Model::zero()
{
    step_current  = 0;
    step_goto     = 0;
    step_ulim    -= step_llim; // shift upper limit down accordingly
    step_llim     = 0;
    step_goto_box = 0;
    print_state();
    updatePosUI();
    updateLimUI();
    updateGotoUI();
}

void Model::move()
{
    std::cout << "b";

    if (step_goto < step_llim)
        step_goto = step_llim;

    if (step_goto > step_ulim)
        step_goto = step_ulim;

    while (step_current != step_goto)
    {
        if (step_goto > step_current)
            step_current++;
        else
            step_current--;
        std::cout << "z";
    }

    std::cout << "b...\n";
    updatePosUI();
    print_state();
}

void Model::set_step_goto(int var)
{
    step_goto_box = var;
    updateGotoUI();
}

// void Model::set_step_delta(int var)
// {
//     step_goto = step_current + (var / mm_per_step);
// }

void Model::go() // in case GO button was hit
{
    step_goto = step_goto_box;
    move();
}

void Model::set_step_p1()
{
    step_goto = step_current + 1;
    move();
}

void Model::set_step_m1()
{
    step_goto = step_current - 1;
    move();
}

void Model::set_step_p10()
{
    step_goto = step_current + 10;
    move();
}

void Model::set_step_m10()
{
    step_goto = step_current - 10;
    move();
}

void Model::set_step_llim(int var)
{
    step_llim = var;
    updateLimUI();
}

void Model::set_step_ulim(int var)
{
    step_ulim = var;
    updateLimUI();
}

void Model::set_mm_goto(double var)
{
    int step = round(var / mm_per_step);

    qDebug() << "set_mm_goto: " << step;
    set_step_goto(step);
}

// void Model::set_mm_delta(double var)
// {
//     set_step_goto((int)(step_current + var / mm_per_step));
// }

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

double Model::get_mm_per_step(void)
{
    return mm_per_step;
}

void Model::updatePosUI()
{
    QString qstr =
        QString("%1 mm").arg(qFabs(step_current * mm_per_step), 0, 'f',
                             CB_DIGITS);
    emit updatePosLabel(qstr);
}

void Model::updateGotoUI()
{
    double pos = myround((step_goto_box * mm_per_step), CB_DIGITS);
    emit   updateSpinBoxPos(pos);
    emit   updateHSliderPos(step_goto_box);
}

void Model::updateLimUI()
{
    double dl = myround((step_llim * mm_per_step), CB_DIGITS);
    double du = myround((step_ulim * mm_per_step), CB_DIGITS);
    emit   updateHSliderLim(step_llim, step_ulim);
    emit   updateGotoBoxLlim(dl);
    emit   updateGotoBoxUlim(du);
    emit   updateDSLlim(dl);
    emit   updateDSUlim(du);
}

void Model::print_state()
{
    qDebug() << "-------------------------------";
    qDebug() << "step_current" << step_current;
    qDebug() << "step_goto" << step_goto;
    qDebug() << "step_goto_mm" << step_goto * mm_per_step;
    qDebug() << "step_llim" << step_llim;
    qDebug() << "step_ulim" << step_ulim;
}

double Model::myround(double var, int digits)
{
    for (int i = 0; i < digits; i++)
    {
        var *= 10.0;
    }
    var = round(var);

    for (int i = 0; i < digits; i++)
    {
        var *= 0.1;
    }
    return var;
}
