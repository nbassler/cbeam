// #include "model.h"
#include "mainwindow.h"
#include "ctrl.h"


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow   w;

    w.show();
    return app.exec();
}
