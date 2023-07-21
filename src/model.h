#ifndef MODEL_H
#define MODEL_H

#include "ui_slider.h"
#include <QObject>

class Model : public QObject {
    Q_OBJECT

public:

    Model();
    void   update_ui();
    double get_step_llim();
    double get_step_ulim();
    double get_mm_llim();
    double get_mm_ulim();
    double get_mm_per_step();
    double get_step_current();
    void   move();

public slots:

    void go();
    void zero();

    void set_step_goto(int);
    void set_step_llim(int);
    void set_step_ulim(int);

    // void set_step_delta(int);

    void set_step_p1(void);
    void set_step_m1(void);
    void set_step_p10(void);
    void set_step_m10(void);

    void set_mm_goto(double);
    void set_mm_llim(double);
    void set_mm_ulim(double);

    // void set_mm_delta(double);

    void print_state();

    /* UI stuff */
    void updatePosUI();
    void updateGotoUI();
    void updateLimUI();

private:

    double mm_per_step;
    int step_current;  // current position in steps
    int step_goto;     // requested absolute step position, will be travelled to
    // viewer data
    int step_goto_box; // requested step in goto_box
    int step_llim;     // lower step limit
    int step_ulim;     // upper step limit
    double myround(double,
                   int);

signals:

    void updateSpinBoxPos(double);
    void updatePosLabel(QString);
    void updateHSliderLim(int,
                          int);
    void updateHSliderPos(int);
    void updateGotoBoxLlim(double);
    void updateGotoBoxUlim(double);
    void updateDSLlim(double);
    void updateDSUlim(double);
};

#endif // MODEL_H
