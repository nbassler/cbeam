#ifndef HELPERDRIVER_H
#define HELPERDRIVER_H

#include <QByteArray>
#include <QProcess>

#include <string>

#include "stepdriver.h"

class HelperDriver : public StepDriver {
public:
  HelperDriver();
  ~HelperDriver() override;

  HelperDriver(const HelperDriver &) = delete;
  HelperDriver &operator=(const HelperDriver &) = delete;

  StepOutcome step(int steps) override;
  void abort() override;
  const char *name() const override;

  bool controlsOwnMotion() const override { return true; }
  bool moveTo(int targetSteps) override;
  bool stopMotion() override;
  bool estop() override;
  bool zeroHere() override;
  bool poll(DriverStatus &status) override;
  std::string lastError() const override { return m_lastError; }

private:
  bool sendCommand(const QByteArray &command, QByteArray &reply);
  bool expectOk(const QByteArray &command);
  bool ensureRunning();
  void setError(const QString &message);

  QProcess m_process;
  std::string m_name;
  std::string m_lastError;
};

#endif // HELPERDRIVER_H
