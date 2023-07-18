#ifndef CTRL_H
#define CTRL_H

#include "ui_slider.h"
#include <QObject>

class Ctrl: public QObject {
    Q_OBJECT

public:
    Ctrl();
    void aaa();

    public slots:
        void go();
        void zero();
        void set_step(int);

    signals:
        void valueChanged(int);

private:
    int pos_step;
    float pos_mm;
};

#endif // CTRL_H
