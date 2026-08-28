#include "motionhelper.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {
const char *stateName(MotionState state) {
  switch (state) {
  case MotionState::Idle:
    return "idle";
  case MotionState::Moving:
    return "moving";
  case MotionState::Stopping:
    return "stopping";
  case MotionState::Estopped:
    return "estopped";
  case MotionState::Fault:
    return "fault";
  }

  return "fault";
}

std::string detailToken(const std::string &detail) {
  if (detail.empty())
    return "none";

  std::string token = detail;

  for (char &ch : token)
    if (ch == ' ')
      ch = '_';

  return token;
}
} // namespace

int main() {
  try {
    MotionHelper helper;
    std::string line;

    while (std::getline(std::cin, line)) {
      std::istringstream input(line);
      std::string command;

      input >> command;

      if (command.empty())
        continue;

      if (command == "status") {
        const DriverStatus status = helper.status();

        std::cout << "status state=" << stateName(status.state)
                  << " pos=" << status.current << " target=" << status.target
                  << " known=" << (status.positionKnown ? 1 : 0)
                  << " detail=" << detailToken(status.detail) << '\n'
                  << std::flush;
        continue;
      }

      if (command == "quit") {
        std::cout << "ok\n" << std::flush;
        break;
      }

      std::string error;
      bool ok = false;

      if (command == "move" || command == "move_abs") {
        int target = 0;

        if (!(input >> target))
          error = "bad_args";
        else
          ok = helper.moveAbs(target, error);
      } else if (command == "stop") {
        ok = helper.stop(error);
      } else if (command == "estop") {
        ok = helper.estop(error);
      } else if (command == "zero_here") {
        ok = helper.zeroHere(error);
      } else {
        error = "bad_command";
      }

      if (ok)
        std::cout << "ok\n";
      else
        std::cout << "err " << detailToken(error) << '\n';

      std::cout << std::flush;
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  return 0;
}
