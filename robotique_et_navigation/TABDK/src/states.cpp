#ifndef STATES_CPP
#define STATES_CPP

// Regroupe tous les états créés au fil du projet ; en version finale seuls STOP et REACH_POSITION servent (rapport § 4.1 et § 4.3).
enum RobotState {
  FORWARD,
  BACKWARD,
  PIVOTER_GAUCHE,
  PIVOTER_DROITE,
  STOP,
  REACH_POSITION_X,
  REACH_POSITION,
  REACH_ANGLE
};

#define DEFAULT_STATE STOP

class States {
public:
  bool target_active = false;

  void init(Params &params, Mouvements &mouvements) {
    this->params = &params;
    this->mouvements = &mouvements;
  }

  void initManual() {
    current_state = STOP;
    if (target_active) {
      // La position peut avoir changé depuis le dernier contrôle de la cible.
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
    target_active = false;
  }

private:
  Params *params = nullptr;
  Mouvements *mouvements = nullptr;

  RobotState current_state = DEFAULT_STATE;
  int ticks_counter = 0;

public:
  // Suivi de cible utilisé en mode manuel comme en auto : seule l'origine des waypoints change (rapport § 4.1 et § 4.3).
  void updateFSMManual() {
    switch (current_state) {
    case STOP:
      if (target_active) {
        current_state = REACH_POSITION;
      }
      break;
    case REACH_POSITION:
      if (!target_active || mouvements->position_atteinte) {
        target_active = false;
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
      mouvements->reachPositionX(mouvements->targets.x);
      break;
    case REACH_ANGLE:
      mouvements->reachAngle(mouvements->targets.angle);
      break;
    case REACH_POSITION:
      if (!params->move_smooth) {
        mouvements->reachPosition(mouvements->targets.x, mouvements->targets.y);
      } else if (params->move_back) {
        mouvements->reachPositionSmoothBack(mouvements->targets.x, mouvements->targets.y);
      } else {
        mouvements->reachPositionSmooth(mouvements->targets.x, mouvements->targets.y);
      }
      break;
    default:
      mouvements->stop();
      break;
    }
  }

  // Ancienne séquence avancer/arrêter/reculer conservée pour référence, mais plus appelée en version finale (rapport § 3.1 et § 4.3).
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
