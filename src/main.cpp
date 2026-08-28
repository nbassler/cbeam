#include <QApplication>

#include "mainwindow.h"
#include "version.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  QCoreApplication::setApplicationName("cbeam");
  QCoreApplication::setApplicationVersion(CB_VERSION);

  MainWindow w;

  w.show();
  return app.exec();
}
