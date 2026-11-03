#ifndef CONTROLEUR_CPP
#define CONTROLEUR_CPP

#include "parametres.cpp"
#include "calculs.cpp"
#include "clavier.cpp"
#include "mouvements.cpp"
#include "odometry.cpp"

#include <ros/ros.h>
#include <std_msgs/Empty.h>
#include <std_srvs/Empty.h>

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

enum RobotMode { AUTO, MANUAL, PAUSE };

#define DEFAULT_STATE STOP
#define DEFAULT_MODE MANUAL

// Possède tous les modules, la machine à états et le reset
class Controleur {
public:
  void init(ros::NodeHandle &nh) {
    params_.load(nh);
    odom_.init(nh);
    clavier_.init(nh);
    mouvements_.init(nh, &params_, &odom_);

    pub_reset_ = nh.advertise<std_msgs::Empty>("/reset", 1);
    client_reset_sim_ = nh.serviceClient<std_srvs::Empty>("/gazebo/reset_simulation");
  }

  double frequence() {
    return params_.frequence;
  }

  // Une itération de la boucle de contrôle
  void step() {
    logStatus();

    if (clavier_.inputs.reset_requested) {
      resetRobot();
      return;
    }

    updateMode();
    updateFSM();
  }

  // Remet le robot, la simulation et le contrôleur à zéro
  void resetRobot() {
    // Robot réel : message vide sur /reset
    pub_reset_.publish(std_msgs::Empty());

    // Simulation Gazebo
    std_srvs::Empty srv;
    if (!client_reset_sim_.call(srv)) {
      ROS_WARN("Echec de l'appel au service /gazebo/reset_simulation");
    }

    // Remise à zéro des contrôles
    mouvements_.reset();
    resetFSM();
    clavier_.reset();
  }

private:
  Params params_;
  Odometry odom_;
  Clavier clavier_;
  Mouvements mouvements_;

  ros::Publisher pub_reset_;
  ros::ServiceClient client_reset_sim_;

  RobotState current_state_ = DEFAULT_STATE;
  RobotMode current_mode_ = DEFAULT_MODE;
  RobotMode previous_mode_ = DEFAULT_MODE;
  int ticks_counter_ = 0;

  void resetFSM() {
    current_state_ = DEFAULT_STATE;
    current_mode_ = DEFAULT_MODE;
    previous_mode_ = DEFAULT_MODE;
    ticks_counter_ = 0;
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
        mouvements_.reachPositionSmooth(mouvements_.targets.x, mouvements_.targets.y);
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

  void updateMode() {
    switch (current_mode_) {
      case AUTO:
        if (clavier_.inputs.change_mode) {
          previous_mode_ = current_mode_;
          current_mode_ = MANUAL;
          ROS_INFO("Switching to MANUAL mode");
          current_state_ = STOP;
          clavier_.inputs.direction = NONE;
        } else if (clavier_.inputs.play_pause) {
          previous_mode_ = current_mode_;
          current_mode_ = PAUSE;
          ROS_INFO("Switching to PAUSE mode");
        }
        break;

      case MANUAL:
        if (clavier_.inputs.change_mode) {
          previous_mode_ = current_mode_;
          current_mode_ = AUTO;
          ROS_INFO("Switching to AUTO mode");
          current_state_ = DEFAULT_STATE;
          ticks_counter_ = 0;
        } else if (clavier_.inputs.play_pause) {
          previous_mode_ = current_mode_;
          current_mode_ = PAUSE;
          ROS_INFO("Switching to PAUSE mode");
        }
        break;

      case PAUSE:
        if (clavier_.inputs.play_pause) {
          current_mode_ = previous_mode_;
          ROS_INFO("Switching to %s mode", (current_mode_ == AUTO) ? "AUTO" : "MANUAL");
        }
        break;

      default:
        current_mode_ = DEFAULT_MODE;
        break;
    }

    clavier_.inputs.change_mode = false;
    clavier_.inputs.play_pause = false;
  }

  void updateFSMAuto() {
    switch (current_state_) {
      case FORWARD:
        if (params_.msToTicks(params_.move_fwd_tm) == ticks_counter_) {
          ROS_INFO("Switching to STOP state");
          ticks_counter_ = 0;
          current_state_ = STOP;
        }
        break;
      case STOP:
        if (params_.msToTicks(params_.move_stp_tm) == ticks_counter_) {
          ROS_INFO("Switching to BACKWARD state");
          ticks_counter_ = 0;
          current_state_ = BACKWARD;
        }
        break;
      case BACKWARD:
        if (params_.msToTicks(params_.move_bwd_tm) == ticks_counter_) {
          ROS_INFO("Switching to FORWARD state");
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
    switch (current_mode_) {
      case AUTO:
        updateFSMAuto();
        applyState();
        break;
      case MANUAL:
        updateFSMManual();
        applyState();
        break;
      case PAUSE:
        mouvements_.stop();
        clavier_.inputs.direction = NONE;
        break;
      default:
        break;
    }
  }

  void logStatus() {
    ROS_INFO_THROTTLE(1, "x=%.4f y=%.4f rotation=%.4f deg", odom_.pose.x, odom_.pose.y, rad_to_deg(odom_.pose.rotation));
    ROS_INFO_THROTTLE(1, "target_x=%.4f target_y=%.4f", mouvements_.targets.x, mouvements_.targets.y);
    ROS_INFO_THROTTLE(1, "angle_target=%.4f deg, erreur_angle=%.4f deg, erreur_distance=%.4f",
                      rad_to_deg(mouvements_.debug.angle_target), rad_to_deg(mouvements_.debug.erreur_angle), mouvements_.debug.erreur_distance);
    ROS_INFO_THROTTLE(1, "delta_x=%.4f delta_y=%.4f", mouvements_.debug.delta_x, mouvements_.debug.delta_y);
    ROS_INFO_THROTTLE(1, "pivot_en_cours=%d", mouvements_.debug.pivot_en_cours);
    ROS_INFO_THROTTLE(1, " --- ");
  }
};

#endif
