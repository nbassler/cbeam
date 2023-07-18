#ifndef MODEL_H
#define MODEL_H

#include "ui_slider.h"
#include <QObject>

class Model: public QObject {
    Q_OBJECT

public:
    Model();
    void aaa();

public slots:
    void go();
    void zero();
    void set_step(int);
    void set_llim(int);
    void set_ulim(int);

private:
    float mm_per_step;
    int pos_step;  // current position in steps
    float pos_mm;  // current position in mm
    int step_llim; // lower step limit
    int step_ulim; // upper step limit
    int step_goto; // requested absulte step position, will be travelled to when go() function is called
};

#endif // MODEL_H
