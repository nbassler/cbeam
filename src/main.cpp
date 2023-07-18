#include "ui_slider.h"
// #include "model.h"
#include "ctrl.h"


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QMainWindow widget;
    Ui::MainWindow ui;

    ui.setupUi(&widget);

    // Model *m = new Model();
    Ctrl *c = new Ctrl();

    // any user change on GUI will be sent to controller, which then will update the model
    //
    // QObject::connect(ui.pushButton_go, SIGNAL(clicked()), c, SLOT(go())); // old syntax
    // QObject::connect(ui.horizontalSlider, SIGNAL(valueChanged()), c, SLOT(set_step(var))); // old syntax
    QObject::connect(ui.pushButton_go, &QPushButton::pressed, c, &Ctrl::go);
    QObject::connect(ui.horizontalSlider, &QSlider::valueChanged, c, &Ctrl::set_step);
    QObject::connect(ui.doubleSpinBox_goPos, &QDoubleSpinBox::valueChanged, c, &Ctrl::set_step);

    // let the model be able to update the GUI
    QObject::connect(
        c, &Ctrl::valueChanged, ui.label_currentPos,
        static_cast<void(QLabel::*)(int)>(&QLabel::setNum)  // amazingly ugly
    );

    widget.show();
    return app.exec();
}
