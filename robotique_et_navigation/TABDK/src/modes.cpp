#ifndef MODES_CPP
#define MODES_CPP

#include <string>

enum RobotMode { AUTO, MANUAL };

#define DEFAULT_MODE MANUAL

class Modes {
public:
  explicit Modes(States &states) : states_(states) {}

  void step() {
    if (paused_) {
      states_.stop();
      return;
    }

    switch (current_mode_) {
    case AUTO:
      states_.stepAuto();
      break;
    case MANUAL:
      states_.stepManual();
      break;
    }
  }

  bool set_mode(const std::string &mode) {
    if (mode != "manual" && mode != "auto") {
      return false;
    }

    RobotMode requested_mode = mode == "auto" ? AUTO : MANUAL;
    if (requested_mode != current_mode_) {
      current_mode_ = requested_mode;
      if (current_mode_ == AUTO) {
        states_.initAuto();
      } else {
        states_.initManual();
      }
    }
    return true;
  }

  bool set_pause(const std::string &state) {
    if (state == "pause") {
      paused_ = true;
    } else if (state == "play") {
      paused_ = false;
    } else {
      return false;
    }
    return true;
  }

  void reset() {
    current_mode_ = DEFAULT_MODE;
    paused_ = false;
    states_.reset();
  }

  std::string mode() const {
    return current_mode_ == AUTO ? "auto" : "manual";
  }

  bool paused() const {
    return paused_;
  }

private:
  States &states_;
  RobotMode current_mode_ = DEFAULT_MODE;
  bool paused_ = false;
};

#endif
