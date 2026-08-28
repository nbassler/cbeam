#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <array>

#include "config.h"
#include "model.h"

namespace Ui {
class MainWindow;
}

class QLabel;
class QPushButton;
class QSpinBox;

// The view.  Holds no position state of its own: every widget is written from
// a Model signal, and every user edit goes straight back to the Model.
//
// All programmatic widget updates are wrapped in a QSignalBlocker.  That is
// what breaks the slider <-> spin box <-> model cycle, and it is the reason the
// model can emit its signals unconditionally.
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private:
  void showPosition(int steps, double mm);
  void showTarget(int steps, double mm);
  void showLimits(int loSteps, int hiSteps, double loMm, double hiMm);

  // Controls are gated by two independent things -- travel in progress, and
  // the limits lock -- so both are resolved in one place rather than by two
  // handlers taking turns overriding each other.
  void updateControlStates();

  // One retunable jog increment: a pair of buttons, the spin box that sets
  // how far they go, and the millimetre equivalent shown underneath.
  struct JogColumn {
    QPushButton *plus;
    QPushButton *minus;
    QSpinBox *steps;
    QLabel *asMm;
  };

  void setUpSimulationToggle();
  void setSimulation(bool simulated);
  void setUpJogColumns();
  void relabelJogColumn(const JogColumn &col);

  Ui::MainWindow *ui;
  Model *m_model;
  std::array<JogColumn, CB_JOG_COLUMNS> m_jog;
  bool m_moving = false;

#ifdef CBEAM_HAVE_GPIO
  static constexpr bool m_gpioAvailable = true;
#else
  static constexpr bool m_gpioAvailable = false;
#endif
};

#endif // MAINWINDOW_H
