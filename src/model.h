#ifndef MODEL_H
#define MODEL_H

#include "ui_slider.h"
#include <QObject>

class Model : public QObject {
    Q_OBJECT

public:

    Model();
    void aaa();
    void update_ui();

public slots:

    void go();
    void zero();

    void set_step(int);
    void set_step_llim(int);
    void set_step_ulim(int);
    void set_step_delta(int);

    void set_step_p1(void);
    void set_step_m1(void);
    void set_step_p10(void);
    void set_step_m10(void);

    void set_mm(double);
    void set_mm_llim(double);
    void set_mm_ulim(double);
    void set_mm_delta(double);

    void print_state();

    /* UI stuff */
    void updatePos();

private:

    double mm_per_step;
    int step_current; // current position in steps
    int step_goto;    // requested absulte step position, will be travelled to
    int step_llim;    // lower step limit
    int step_ulim;    // upper step limit

signals:

    void valueChanged(int);
    void updatePosLabel(QString);
};

#endif // MODEL_H
