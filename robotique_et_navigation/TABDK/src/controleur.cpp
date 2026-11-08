#include <csignal>
#include <ros/ros.h>
#include <std_msgs/Bool.h>
#include <std_msgs/Empty.h>
#include <std_msgs/String.h>
#include <std_srvs/Empty.h>

#include "calculs.cpp"
#include "parametres.cpp"
#include "odometry.cpp"
#include "mouvements.cpp"
#include "states.cpp"
#include "modes.cpp"
#include "serveur_action.cpp"
#include "serveur_services.cpp"

using namespace ros;
using std_msgs::Bool;
using std_msgs::String;
using ResetMessage = std_msgs::Empty;
using ResetService = std_srvs::Empty;
using init_options::NoSigintHandler;

bool arret_demande = false;

void sigintHandler(int) {
  arret_demande = true;
}

void publishStatus(const Modes &modes, Publisher &pub_mode,
                   Publisher &pub_pause) {
  String mode_message;
  mode_message.data = modes.mode();
  pub_mode.publish(mode_message);

  Bool pause_message;
  pause_message.data = modes.paused();
  pub_pause.publish(pause_message);
}

int main(int argc, char **argv) {
  signal(SIGINT, sigintHandler);
  init(argc, argv, "controleur", NoSigintHandler);

  NodeHandle nh;

  Params params;
  params.load(nh);

  Odometry odometry;
  odometry.init(nh);

  Mouvements mouvements;
  mouvements.init(nh, &params, &odometry);

  Publisher pub_reset = nh.advertise<ResetMessage>("/reset", 1);
  Publisher pub_mode = nh.advertise<String>("/mode", 1, true);
  Publisher pub_pause = nh.advertise<Bool>("/pause", 1, true);
  ServiceClient client_reset_sim =
      nh.serviceClient<ResetService>("/gazebo/reset_simulation");

  States states;
  states.init(params, mouvements);

  Modes modes;
  modes.init(states, params.default_mode);

  ServeurAction serveur_action;
  serveur_action.init(nh, params, mouvements, modes, odometry, states);

  ServeurServices services;
  services.init(nh, modes, serveur_action, pub_reset, client_reset_sim);

  publishStatus(modes, pub_mode, pub_pause);
  Rate loop_rate(params.frequence);

  while (ok() && !arret_demande) {
    modes.step();
    serveur_action.step();
    spinOnce();
    publishStatus(modes, pub_mode, pub_pause);
    loop_rate.sleep();
  }

  ROS_INFO("Exiting program gracefully ...");
  services.resetRobot();
  publishStatus(modes, pub_mode, pub_pause);
  shutdown();
  return 0;
}
