#ifndef MODES_CPP
#define MODES_CPP

#include <string>

#include "parametres.cpp"
#include "clavier.cpp"
#include "mouvements.cpp"
#include "states.cpp"

using std::string;

#define AUTO "auto"
#define MANUAL "manual"
#define DEFAULT_MODE MANUAL
const string MODES[] = {AUTO, MANUAL};
const int NBR_MODES = sizeof(MODES) / sizeof(MODES[0]);

class Modes {
public:
  void init(States &states) {
    states_ = &states;
  }

  void step() {
    if (paused_) {
      states_->stop();
      return;
    }

    if (current_mode_ == AUTO) {
      states_->stepAuto();
    } else if (current_mode_ == MANUAL) {
      states_->stepManual();
    } else {
      ROS_ERROR("Mode non pris en charge : %s", current_mode_.c_str());
      states_->stop();
    }
  }

  bool set_mode(string &mode) {
    if (!is_a_mode(mode)) {
      return false;
    }

    if (mode != current_mode_) {
      if (mode == AUTO) {
        states_->initAuto();
      } else if (mode == MANUAL) {
        states_->initManual();
      } else {
        ROS_ERROR("Mode non pris en charge : %s", mode.c_str());
        return false;
      }
      current_mode_ = mode;
    }
    return true;
  }

  bool set_pause(string &state) {
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
    states_->reset();
  }

  string mode() const {
    return current_mode_;
  }

  bool paused() const {
    return paused_;
  }

private:
  States *states_ = nullptr;
  string current_mode_ = DEFAULT_MODE;
  bool paused_ = false;

  bool is_a_mode(const string &mode) {
    for (int i = 0; i < NBR_MODES; i++) {
      if (mode == MODES[i]) {
        return true;
      }
    }
    return false;
  }
};

#endif
