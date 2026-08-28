#include "helperdriver.h"
#include "config.h"

#include <QCoreApplication>
#include <QDir>
#include <QStringList>

#include <stdexcept>

namespace {
QString helperProgramPath() {
  const QString override = qEnvironmentVariable("CBEAM_HELPER");

  if (!override.isEmpty())
    return override;

  return QDir(QCoreApplication::applicationDirPath()).filePath("cbeam-helper");
}

QString stateName(MotionState state) {
  switch (state) {
  case MotionState::Idle:
    return QStringLiteral("idle");
  case MotionState::Moving:
    return QStringLiteral("moving");
  case MotionState::Stopping:
    return QStringLiteral("stopping");
  case MotionState::Estopped:
    return QStringLiteral("estopped");
  case MotionState::Fault:
    return QStringLiteral("fault");
  }

  return QStringLiteral("fault");
}
} // namespace

HelperDriver::HelperDriver() {
  m_process.setProgram(helperProgramPath());
  m_process.setProcessChannelMode(QProcess::SeparateChannels);
  m_process.start();

  if (!m_process.waitForStarted(3000))
    throw std::runtime_error(
        QString("cannot start cbeam-helper at %1")
            .arg(m_process.program())
            .toStdString());

  DriverStatus status;

  if (!poll(status))
    throw std::runtime_error(m_lastError.empty() ? "cbeam-helper did not answer"
                                                 : m_lastError);

  m_name = "helper (GPIO " + std::to_string(CB_PIN_PULSE) + "/" +
           std::to_string(CB_PIN_DIR) + ")";
}

HelperDriver::~HelperDriver() {
  if (m_process.state() == QProcess::NotRunning)
    return;

  QByteArray reply;
  sendCommand("quit", reply);
  m_process.waitForFinished(1000);

  if (m_process.state() != QProcess::NotRunning)
    m_process.kill();
}

StepOutcome HelperDriver::step(int steps) {
  Q_UNUSED(steps)
  return {0, StepResult::Blocked};
}

void HelperDriver::abort() {
  if (m_process.state() != QProcess::NotRunning)
    estop();
}

const char *HelperDriver::name() const { return m_name.c_str(); }

bool HelperDriver::moveTo(int targetSteps) {
  return expectOk(QByteArray("move_abs ") + QByteArray::number(targetSteps));
}

bool HelperDriver::stopMotion() { return expectOk("stop"); }

bool HelperDriver::estop() { return expectOk("estop"); }

bool HelperDriver::zeroHere() { return expectOk("zero_here"); }

bool HelperDriver::poll(DriverStatus &status) {
  QByteArray reply;

  if (!sendCommand("status", reply))
    return false;

  const QList<QByteArray> tokens = reply.split(' ');

  if (tokens.isEmpty() || tokens.first() != "status") {
    setError(QString("unexpected helper reply: %1")
                 .arg(QString::fromLatin1(reply.constData())));
    return false;
  }

  DriverStatus parsed;

  for (int i = 1; i < tokens.size(); i++) {
    const int eq = tokens[i].indexOf('=');

    if (eq <= 0)
      continue;

    const QByteArray key = tokens[i].left(eq);
    const QByteArray value = tokens[i].mid(eq + 1);

    if (key == "state") {
      if (value == "idle")
        parsed.state = MotionState::Idle;
      else if (value == "moving")
        parsed.state = MotionState::Moving;
      else if (value == "stopping")
        parsed.state = MotionState::Stopping;
      else if (value == "estopped")
        parsed.state = MotionState::Estopped;
      else
        parsed.state = MotionState::Fault;
    } else if (key == "pos") {
      parsed.current = value.toInt();
    } else if (key == "target") {
      parsed.target = value.toInt();
    } else if (key == "known") {
      parsed.positionKnown = value == "1";
    } else if (key == "detail" && value != "none") {
      parsed.detail = value.toStdString();
    }
  }

  status = parsed;
  m_lastError.clear();
  return true;
}

bool HelperDriver::sendCommand(const QByteArray &command, QByteArray &reply) {
  reply.clear();

  if (!ensureRunning())
    return false;

  if (m_process.write(command + '\n') < 0 || !m_process.waitForBytesWritten(1000)) {
    setError(QString("cannot write '%1' to cbeam-helper")
                 .arg(QString::fromLatin1(command.constData())));
    return false;
  }

  while (true) {
    if (m_process.canReadLine()) {
      reply = m_process.readLine().trimmed();
      return true;
    }

    if (!m_process.waitForReadyRead(2000)) {
      const QString stderrText =
          QString::fromLocal8Bit(m_process.readAllStandardError()).trimmed();

      if (stderrText.isEmpty())
        setError(QString("timeout waiting for cbeam-helper after '%1'")
                     .arg(QString::fromLatin1(command.constData())));
      else
        setError(stderrText);

      return false;
    }
  }
}

bool HelperDriver::expectOk(const QByteArray &command) {
  QByteArray reply;

  if (!sendCommand(command, reply))
    return false;

  if (reply == "ok") {
    m_lastError.clear();
    return true;
  }

  if (reply.startsWith("err ")) {
    setError(QString::fromLatin1(reply.mid(4).constData()));
    return false;
  }

  setError(QString("unexpected helper reply: %1")
               .arg(QString::fromLatin1(reply.constData())));
  return false;
}

bool HelperDriver::ensureRunning() {
  if (m_process.state() == QProcess::Running)
    return true;

  const QString stderrText =
      QString::fromLocal8Bit(m_process.readAllStandardError()).trimmed();

  if (stderrText.isEmpty())
    setError(QString("cbeam-helper is not running (%1)")
                 .arg(stateName(MotionState::Fault)));
  else
    setError(stderrText);

  return false;
}

void HelperDriver::setError(const QString &message) {
  m_lastError = message.toStdString();
}
