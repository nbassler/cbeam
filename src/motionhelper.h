#ifndef MOTIONHELPER_H
#define MOTIONHELPER_H

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "config.h"
#include "stepdriver.h"

class MotionHelper {
public:
  MotionHelper();
  ~MotionHelper();

  MotionHelper(const MotionHelper &) = delete;
  MotionHelper &operator=(const MotionHelper &) = delete;

  bool moveAbs(int target, std::string &error);
  bool stop(std::string &error);
  bool estop(std::string &error);
  bool zeroHere(std::string &error);
  DriverStatus status() const;

private:
  struct WaveSlot {
    int id = -1;
    int steps = 0;
    int direction = 1;
    double endRate = CB_RATE_START;
  };

  void plannerLoop();
  void retireFinishedLocked();
  bool queueNextWaveLocked();
  void clearQueuedLocked();
  void setFaultLocked(const std::string &detail);
  int plannedPositionLocked() const;
  int signedSteps(const WaveSlot &slot) const;

  int m_pi = -1;
  mutable std::mutex m_mutex;
  std::condition_variable m_cv;
  std::thread m_thread;
  bool m_exit = false;
  int m_position = 0;
  int m_target = 0;
  bool m_positionKnown = true;
  MotionState m_state = MotionState::Idle;
  std::string m_detail;
  double m_rate = CB_RATE_START;
  double m_planRate = CB_RATE_START;
  WaveSlot m_playing;
  WaveSlot m_queued;
};

#endif // MOTIONHELPER_H
