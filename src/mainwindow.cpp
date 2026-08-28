#include "mainwindow.h"
#include "config.h"
#include "ui_slider.h"
#include "version.h"

#include <QCheckBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>

#include <iterator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_model(new Model(this)) {
  ui->setupUi(this);

  // Appended rather than hardcoded in the .ui, so the title carries the
  // exact tree a binary came from. Worth having on screen: this app will be
  // run over ssh from machines that may be a release or two behind.
  setWindowTitle(
      QString("%1 %2").arg(windowTitle(), QString::fromLatin1(CB_VERSION)));

  // Every mm widget carries the same precision as the readouts, so a value
  // written back from the model survives the trip unchanged.
  for (QDoubleSpinBox *box : {ui->doubleSpinBox_goPos, ui->doubleSpinBox_llim,
                              ui->doubleSpinBox_ulim}) {
    box->setDecimals(CB_MM_DIGITS);
    box->setSingleStep(CB_MM_PER_STEP);

    // Without this, valueChanged fires on every keystroke, so typing "120"
    // would command moves to 1 mm and 12 mm on the way.
    box->setKeyboardTracking(false);
  }

  // View -> model.
  connect(ui->pushButton_go, &QPushButton::clicked, m_model, &Model::go);
  connect(ui->pushButton_stop, &QPushButton::clicked, m_model, &Model::stop);
  connect(ui->pushButton_zero, &QPushButton::clicked, m_model, &Model::zero);

  connect(ui->horizontalSlider, &QSlider::valueChanged, m_model,
          &Model::setTargetSteps);
  connect(ui->doubleSpinBox_goPos,
          qOverload<double>(&QDoubleSpinBox::valueChanged), m_model,
          &Model::setTargetMm);
  connect(ui->doubleSpinBox_llim,
          qOverload<double>(&QDoubleSpinBox::valueChanged), m_model,
          &Model::setLlimMm);
  connect(ui->doubleSpinBox_ulim,
          qOverload<double>(&QDoubleSpinBox::valueChanged), m_model,
          &Model::setUlimMm);

  setUpJogColumns();

  ui->checkBox_lockLimits->setChecked(CB_LOCK_LIMITS_AT_STARTUP);
  connect(ui->checkBox_lockLimits, &QCheckBox::toggled, this,
          &MainWindow::updateControlStates);

  // Model -> view.
  connect(m_model, &Model::positionChanged, this, &MainWindow::showPosition);
  connect(m_model, &Model::targetChanged, this, &MainWindow::showTarget);
  connect(m_model, &Model::limitsChanged, this, &MainWindow::showLimits);
  connect(m_model, &Model::movingChanged, this, [this](bool moving) {
    m_moving = moving;
    updateControlStates();
  });

  // Fires after movingChanged(false), so this message is the one left
  // standing in the status bar.
  connect(m_model, &Model::travelInterrupted, this, [this] {
    statusBar()->showMessage(tr("Travel interrupted - held at %1 steps")
                                 .arg(m_model->stepCurrent()));
  });

  // Calibration, parked on the right of the status bar: always in view,
  // never in the way. Built from the constants rather than typed out, so it
  // cannot drift from the numbers the model actually uses.
  auto *calibration =
      new QLabel(tr("%1 steps/cm | %2 mm/step")
                     .arg(CB_STEPS_PER_CM, 0, 'g', 6)
                     .arg(CB_MM_PER_STEP, 0, 'f', CB_MM_DIGITS + 3),
                 this);

  calibration->setToolTip(tr("Measured rail calibration, from src/config.h"));
  statusBar()->addPermanentWidget(calibration);

  m_model->publishAll();
  updateControlStates();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::setUpJogColumns() {
  m_jog = {{
      {ui->pushButton_plusA, ui->pushButton_minusA, ui->spinBox_jogA,
       ui->label_jogMmA},
      {ui->pushButton_plusB, ui->pushButton_minusB, ui->spinBox_jogB,
       ui->label_jogMmB},
      {ui->pushButton_plusC, ui->pushButton_minusC, ui->spinBox_jogC,
       ui->label_jogMmC},
  }};

  static_assert(std::size(CB_JOG_DEFAULTS) == CB_JOG_COLUMNS,
                "CB_JOG_DEFAULTS must have one entry per jog column");

  for (int i = 0; i < CB_JOG_COLUMNS; i++) {
    const JogColumn &col = m_jog[i];

    col.steps->setRange(CB_JOG_MIN, CB_JOG_MAX);
    col.steps->setValue(CB_JOG_DEFAULTS[i]);
    col.steps->setToolTip(tr("Steps travelled per click of this column"));

    // Read at click time rather than captured, so retuning the column
    // takes effect immediately and there is no second copy of the
    // increment to keep in step with the box.
    connect(col.plus, &QPushButton::clicked, this,
            [this, &col] { m_model->jog(+col.steps->value()); });
    connect(col.minus, &QPushButton::clicked, this,
            [this, &col] { m_model->jog(-col.steps->value()); });
    connect(col.steps, qOverload<int>(&QSpinBox::valueChanged), this,
            [this, &col] { relabelJogColumn(col); });

    relabelJogColumn(col);
  }
}

void MainWindow::relabelJogColumn(const JogColumn &col) {
  const int steps = col.steps->value();
  const double mm = Model::mmFromSteps(steps);

  col.plus->setText(QString("+%1").arg(steps));
  col.minus->setText(QString("-%1").arg(steps));
  col.asMm->setText(QString("%1 mm").arg(mm, 0, 'f', CB_MM_DIGITS));

  const QString hint =
      tr("Jog %1 steps (%2 mm)").arg(steps).arg(mm, 0, 'f', CB_MM_DIGITS);

  col.plus->setToolTip(hint);
  col.minus->setToolTip(hint);
}

void MainWindow::showPosition(int steps, double mm) {
  ui->label_currentPos->setText(QString("%1 mm").arg(mm, 0, 'f', CB_MM_DIGITS));
  ui->label_currentSteps->setText(QString("%1 steps").arg(steps));
}

void MainWindow::showTarget(int steps, double mm) {
  // Blocked: these writes are an echo of a change the model already applied,
  // not a new command.
  {
    const QSignalBlocker block(ui->horizontalSlider);
    ui->horizontalSlider->setValue(steps);
  }
  {
    const QSignalBlocker block(ui->doubleSpinBox_goPos);
    ui->doubleSpinBox_goPos->setValue(mm);
  }
  ui->label_targetSteps->setText(QString("= %1 steps").arg(steps));
}

void MainWindow::showLimits(int loSteps, int hiSteps, double loMm,
                            double hiMm) {
  {
    const QSignalBlocker block(ui->horizontalSlider);
    ui->horizontalSlider->setRange(loSteps, hiSteps);
  }
  {
    const QSignalBlocker block(ui->doubleSpinBox_goPos);
    ui->doubleSpinBox_goPos->setRange(loMm, hiMm);
  }
  {
    const QSignalBlocker block(ui->doubleSpinBox_llim);
    ui->doubleSpinBox_llim->setValue(loMm);
  }
  {
    const QSignalBlocker block(ui->doubleSpinBox_ulim);
    ui->doubleSpinBox_ulim->setValue(hiMm);
  }
}

void MainWindow::updateControlStates() {
  const bool locked = ui->checkBox_lockLimits->isChecked();

  // Nothing may retarget the carriage mid-travel; Stop is the only way out.
  ui->pushButton_go->setEnabled(!m_moving);
  ui->horizontalSlider->setEnabled(!m_moving);

  for (const JogColumn &col : m_jog) {
    col.plus->setEnabled(!m_moving);
    col.minus->setEnabled(!m_moving);

    // The increment boxes stay live during travel: retuning one only
    // changes what the next click will do.
    col.steps->setEnabled(true);
  }

  ui->doubleSpinBox_goPos->setEnabled(!m_moving);

  ui->pushButton_stop->setEnabled(m_moving);

  // The limits and Zero are gated behind the lock as well. Zero belongs in
  // this group because it moves the travel window just as surely as editing
  // a limit does -- an accidental press silently redefines every position on
  // the rail.
  ui->doubleSpinBox_llim->setEnabled(!m_moving && !locked);
  ui->doubleSpinBox_ulim->setEnabled(!m_moving && !locked);
  ui->pushButton_zero->setEnabled(!m_moving && !locked);

  // The backend is named rather than assumed, so it is never a mystery
  // whether this window is driving a real rail.
  statusBar()->showMessage(
      QString("%1 - %2").arg(m_moving ? tr("Moving...") : tr("Idle"),
                             QString::fromLatin1(m_model->driverName())));
}
