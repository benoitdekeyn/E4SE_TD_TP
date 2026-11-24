#ifndef MODES_CPP
#define MODES_CPP

#include <string>

#include "mouvements.cpp"
#include "states.cpp"

using std::string;

#define AUTO "auto"
#define MANUAL "manual"
const string MODES[] = {AUTO, MANUAL};
const int NBR_MODES = sizeof(MODES) / sizeof(MODES[0]);

// Gère le mode actif ("manual"/"auto") et la pause de façon indépendante (rapport § 4.1).
class Modes {
public:
  void init(States &states, const string &default_mode) {
    this->states = &states;
    this->default_mode = default_mode;
    if (!is_a_mode(this->default_mode)) {
      ROS_WARN("Mode par defaut invalide '%s', utilisation de '%s'",
               default_mode.c_str(), MANUAL);
      this->default_mode = MANUAL;
    }
    current_mode = this->default_mode;
    states.initManual();
  }

  void step() {
    // En pause, on coupe les moteurs et on fige la FSM sans bloquer la boucle ROS ni les services.
    if (pause_active) {
      states->stop();
      if (current_mode == AUTO) {
        ROS_INFO_THROTTLE(1.0, "\n### EN PAUSE ###");
      }
      return;
    }

    if (current_mode == AUTO || current_mode == MANUAL) {
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
      states->initManual();
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
    current_mode = default_mode;
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
  string default_mode = MANUAL;
  string current_mode = MANUAL;
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
