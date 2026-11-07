#ifndef STATES_MODES_CPP
#define STATES_MODES_CPP

#include <string>

using std::string;

enum RobotState {
  FORWARD,
  BACKWARD,
  PIVOTER_GAUCHE,
  PIVOTER_DROITE,
  STOP,
  REACH_POSITION,
  REACH_POSITION_X,
  REACH_POSITION_SMOOTH,
  REACH_ANGLE
};

enum RobotMode { AUTO, MANUAL };

#define DEFAULT_STATE STOP
#define DEFAULT_MODE MANUAL

class Controleur {
public:
  Controleur(Params &params, Clavier &clavier, Mouvements &mouvements)
      : params_(params), clavier_(clavier), mouvements_(mouvements) {}

  void step() {
    updateFSM();
  }

  bool set_mode(const string &mode) {
    if (mode != "manual" && mode != "auto") {
      return false;
    }

    RobotMode requested_mode = mode == "auto" ? AUTO : MANUAL;
    if (requested_mode != current_mode_) {
      current_mode_ = requested_mode;
      initModeState();
    }
    return true;
  }

  bool set_pause(const string &state) {
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
    current_state_ = DEFAULT_STATE;
    current_mode_ = DEFAULT_MODE;
    paused_ = false;
    ticks_counter_ = 0;
    clavier_.inputs.direction = NONE;
  }

  string mode() const {
    return current_mode_ == AUTO ? "auto" : "manual";
  }

  bool paused() const {
    return paused_;
  }

private:
  Params &params_;
  Clavier &clavier_;
  Mouvements &mouvements_;

  RobotState current_state_ = DEFAULT_STATE;
  RobotMode current_mode_ = DEFAULT_MODE;
  bool paused_ = false;
  int ticks_counter_ = 0;

  void initModeState() {
    switch (current_mode_) {
    case MANUAL:
      current_state_ = STOP;
      clavier_.inputs.direction = NONE;
      break;
    case AUTO:
      current_state_ = DEFAULT_STATE;
      ticks_counter_ = 0;
      break;
    }
  }

  void applyState() {
    switch (current_state_) {
    case FORWARD:
      mouvements_.forward(params_.linear_speed);
      break;
    case PIVOTER_GAUCHE:
      mouvements_.pivoter(params_.angular_speed);
      break;
    case PIVOTER_DROITE:
      mouvements_.pivoter(-params_.angular_speed);
      break;
    case STOP:
      mouvements_.stop();
      break;
    case BACKWARD:
      mouvements_.backward(params_.linear_speed);
      break;
    case REACH_POSITION:
      mouvements_.reachPosition(mouvements_.targets.x, mouvements_.targets.y);
      break;
    case REACH_POSITION_SMOOTH:
      mouvements_.reachPositionSmooth(mouvements_.targets.x,
                                      mouvements_.targets.y);
      break;
    case REACH_POSITION_X:
      mouvements_.reachPositionX(mouvements_.targets.position_x);
      break;
    case REACH_ANGLE:
      mouvements_.reachAngle(mouvements_.targets.angle);
      break;
    default:
      mouvements_.stop();
      break;
    }
  }

  void updateFSMAuto() {
    switch (current_state_) {
    case FORWARD:
      if (params_.msToTicks(params_.move_fwd_tm) == ticks_counter_) {
        ticks_counter_ = 0;
        current_state_ = STOP;
      }
      break;
    case STOP:
      if (params_.msToTicks(params_.move_stp_tm) == ticks_counter_) {
        ticks_counter_ = 0;
        current_state_ = BACKWARD;
      }
      break;
    case BACKWARD:
      if (params_.msToTicks(params_.move_bwd_tm) == ticks_counter_) {
        ticks_counter_ = 0;
        current_state_ = FORWARD;
      }
      break;
    default:
      current_state_ = DEFAULT_STATE;
      break;
    }
    ticks_counter_++;
  }

  void updateFSMManual() {
    switch (clavier_.inputs.direction) {
    case UP:
      mouvements_.targets.x += 0.1;
      current_state_ = REACH_POSITION_SMOOTH;
      break;
    case DOWN:
      mouvements_.targets.x -= 0.1;
      current_state_ = REACH_POSITION_SMOOTH;
      break;
    case LEFT:
      mouvements_.targets.y -= 0.1;
      current_state_ = REACH_POSITION_SMOOTH;
      break;
    case RIGHT:
      mouvements_.targets.y += 0.1;
      current_state_ = REACH_POSITION_SMOOTH;
      break;
    default:
      break;
    }
    clavier_.inputs.direction = NONE;
  }

  void updateFSM() {
    if (paused_) {
      mouvements_.stop();
      return;
    }

    switch (current_mode_) {
    case AUTO:
      updateFSMAuto();
      applyState();
      break;
    case MANUAL:
      updateFSMManual();
      applyState();
      break;
    }
  }
};

#endif
