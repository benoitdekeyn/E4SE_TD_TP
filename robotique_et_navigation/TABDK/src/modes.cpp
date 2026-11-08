#ifndef MODES_CPP
#define MODES_CPP

#include <string>

#include "parametres.cpp"
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
    this->states = &states;
  }

  void step() {
    if (pause_active) {
      states->stop();
      if (current_mode == AUTO) {
        ROS_INFO_THROTTLE(1.0, "\n### EN PAUSE ###");
      }
      return;
    }

    if (current_mode == AUTO) {
      states->updateFSMAuto();
    } else if (current_mode == MANUAL) {
      states->updateFSMManual();
    } else {
      ROS_ERROR("Mode non pris en charge : %s", current_mode.c_str());
      states->stop();
      return;
    }
    states->applyState();
  }

  bool set_mode(string &mode) {
    if (!is_a_mode(mode)) {
      return false;
    }

    if (mode != current_mode) {
      if (mode == AUTO) {
        states->initAuto();
      } else if (mode == MANUAL) {
        states->initManual();
      } else {
        ROS_ERROR("Mode non pris en charge : %s", mode.c_str());
        return false;
      }
      current_mode = mode;
    }
    return true;
  }

  bool set_pause(string &state) {
    if (state == "pause") {
      pause_active = true;
      states->stop();
    } else if (state == "play") {
      pause_active = false;
    } else {
      return false;
    }
    return true;
  }

  void reset() {
    current_mode = DEFAULT_MODE;
    pause_active = false;
    states->reset();
  }

  string mode() const {
    return current_mode;
  }

  bool paused() const {
    return pause_active;
  }

private:
  States *states = nullptr;
  string current_mode = DEFAULT_MODE;
  bool pause_active = false;
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
