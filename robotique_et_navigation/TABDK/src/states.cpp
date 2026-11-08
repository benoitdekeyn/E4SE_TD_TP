#ifndef STATES_CPP
#define STATES_CPP

enum RobotState {
  FORWARD,
  BACKWARD,
  PIVOTER_GAUCHE,
  PIVOTER_DROITE,
  STOP,
  REACH_POSITION_X,
  REACH_POSITION,
  REACH_POSITION_SMOOTH,
  REACH_POSITION_SMOOTH_BACK,
  REACH_ANGLE
};

#define DEFAULT_STATE STOP

class States {
public:
  bool waypoint_active = false;
  // État utilisé pour rejoindre un waypoint, choisi par le serveur d'action.
  RobotState waypoint_state = REACH_POSITION_SMOOTH;

  void init(Params &params, Mouvements &mouvements) {
    this->params = &params;
    this->mouvements = &mouvements;
  }

  void initManual() {
    current_state = STOP;
    if (waypoint_active) {
      // La position peut avoir changé pendant le mode automatique.
      mouvements->position_atteinte = false;
    }
  }

  void initAuto() {
    current_state = DEFAULT_STATE;
    ticks_counter = 0;
  }

  void stop() {
    mouvements->stop();
  }

  void reset() {
    mouvements->reset();
    current_state = DEFAULT_STATE;
    ticks_counter = 0;
    waypoint_active = false;
  }

private:
  Params *params = nullptr;
  Mouvements *mouvements = nullptr;

  RobotState current_state = DEFAULT_STATE;
  int ticks_counter = 0;

public:
  // Les FSM choisissent l'état ; applyState() exécute ensuite son action.
  void updateFSMManual() {
    switch (current_state) {
    case STOP:
      if (waypoint_active) {
        current_state = waypoint_state;
      }
      break;
    case REACH_POSITION:
    case REACH_POSITION_SMOOTH:
    case REACH_POSITION_SMOOTH_BACK:
      if (!waypoint_active || mouvements->position_atteinte) {
        waypoint_active = false;
        mouvements->position_atteinte = false;
        current_state = STOP;
      }
      break;
    default:
      current_state = STOP;
      break;
    }
  }

  void applyState() {
    switch (current_state) {
    case FORWARD:
      mouvements->forward(params->linear_speed);
      break;
    case PIVOTER_GAUCHE:
      mouvements->pivoter(params->angular_speed);
      break;
    case PIVOTER_DROITE:
      mouvements->pivoter(-params->angular_speed);
      break;
    case STOP:
      mouvements->stop();
      break;
    case BACKWARD:
      mouvements->backward(params->linear_speed);
      break;
      case REACH_POSITION_X:
        mouvements->reachPositionX(mouvements->targets.position_x);
        break;
    case REACH_POSITION:
      mouvements->reachPosition(mouvements->targets.x, mouvements->targets.y);
      break;
    case REACH_POSITION_SMOOTH:
      mouvements->reachPositionSmooth(mouvements->targets.x,
                                      mouvements->targets.y);
      break;
    case REACH_POSITION_SMOOTH_BACK:
      mouvements->reachPositionSmoothBack(mouvements->targets.x,
                                          mouvements->targets.y);
      break;
    case REACH_ANGLE:
      mouvements->reachAngle(mouvements->targets.angle);
      break;
    default:
      mouvements->stop();
      break;
    }
  }

  void updateFSMAuto() {
    switch (current_state) {
    case FORWARD:
      if (params->msToTicks(params->move_fwd_tm) == ticks_counter) {
        ticks_counter = 0;
        current_state = STOP;
        ROS_INFO("\n###    STOP pour %d ms    ###\n", params->move_stp_tm);
      }
      break;
    case STOP:
      if (params->msToTicks(params->move_stp_tm) == ticks_counter) {
        ticks_counter = 0;
        current_state = BACKWARD;
        ROS_INFO("\n###    BACKWARD pour %d ms    ###\n", params->move_bwd_tm);
      }
      break;
    case BACKWARD:
      if (params->msToTicks(params->move_bwd_tm) == ticks_counter) {
        ticks_counter = 0;
        current_state = FORWARD;
        ROS_INFO("\n###    FORWARD pour %d ms    ###\n", params->move_fwd_tm);
      }
      break;
    default:
      current_state = DEFAULT_STATE;
      break;
    }
    ticks_counter++;
  }

};

#endif
