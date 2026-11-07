#ifndef STATES_CPP
#define STATES_CPP

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

#define DEFAULT_STATE STOP

class States {
public:
  void init(Params &params, Clavier &clavier, Mouvements &mouvements) {
    params_ = &params;
    clavier_ = &clavier;
    mouvements_ = &mouvements;
  }

  void initManual() {
    current_state_ = STOP;
    clavier_->inputs.direction = NONE;
  }

  void initAuto() {
    current_state_ = DEFAULT_STATE;
    ticks_counter_ = 0;
  }

  void stepAuto() {
    updateFSMAuto();
    applyState();
  }

  void stepManual() {
    updateFSMManual();
    applyState();
  }

  void stop() {
    mouvements_->stop();
  }

  void reset() {
    mouvements_->reset();
    current_state_ = DEFAULT_STATE;
    ticks_counter_ = 0;
    clavier_->reset();
  }

private:
  Params *params_ = nullptr;
  Clavier *clavier_ = nullptr;
  Mouvements *mouvements_ = nullptr;

  RobotState current_state_ = DEFAULT_STATE;
  int ticks_counter_ = 0;

  void applyState() {
    switch (current_state_) {
    case FORWARD:
      mouvements_->forward(params_->linear_speed);
      break;
    case PIVOTER_GAUCHE:
      mouvements_->pivoter(params_->angular_speed);
      break;
    case PIVOTER_DROITE:
      mouvements_->pivoter(-params_->angular_speed);
      break;
    case STOP:
      mouvements_->stop();
      break;
    case BACKWARD:
      mouvements_->backward(params_->linear_speed);
      break;
    case REACH_POSITION:
      mouvements_->reachPosition(mouvements_->targets.x, mouvements_->targets.y);
      break;
    case REACH_POSITION_SMOOTH:
      mouvements_->reachPositionSmooth(mouvements_->targets.x,
                                       mouvements_->targets.y);
      break;
    case REACH_POSITION_X:
      mouvements_->reachPositionX(mouvements_->targets.position_x);
      break;
    case REACH_ANGLE:
      mouvements_->reachAngle(mouvements_->targets.angle);
      break;
    default:
      mouvements_->stop();
      break;
    }
  }

  void updateFSMAuto() {
    switch (current_state_) {
    case FORWARD:
      if (params_->msToTicks(params_->move_fwd_tm) == ticks_counter_) {
        ticks_counter_ = 0;
        current_state_ = STOP;
      }
      break;
    case STOP:
      if (params_->msToTicks(params_->move_stp_tm) == ticks_counter_) {
        ticks_counter_ = 0;
        current_state_ = BACKWARD;
      }
      break;
    case BACKWARD:
      if (params_->msToTicks(params_->move_bwd_tm) == ticks_counter_) {
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
    switch (clavier_->inputs.direction) {
    case UP:
      mouvements_->targets.x += 0.1;
      current_state_ = REACH_POSITION_SMOOTH;
      break;
    case DOWN:
      mouvements_->targets.x -= 0.1;
      current_state_ = REACH_POSITION_SMOOTH;
      break;
    case LEFT:
      mouvements_->targets.y -= 0.1;
      current_state_ = REACH_POSITION_SMOOTH;
      break;
    case RIGHT:
      mouvements_->targets.y += 0.1;
      current_state_ = REACH_POSITION_SMOOTH;
      break;
    default:
      break;
    }
    clavier_->inputs.direction = NONE;
  }

};

#endif
