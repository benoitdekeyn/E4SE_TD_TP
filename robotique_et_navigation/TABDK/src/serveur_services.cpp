#ifndef SERVEUR_SERVICES_CPP
#define SERVEUR_SERVICES_CPP

#include <TABDK/global_control.h>
#include <TABDK/set_mode.h>
#include <ros/ros.h>
#include <std_msgs/Empty.h>
#include <std_srvs/Empty.h>
#include <string>

#include "serveur_action.cpp"

using namespace ros;
using namespace TABDK;

// Expose /set_mode et /global_control pour séparer le mode de la pause et du reset (rapport § 6.1).
class ServeurServices {
public:
  void init(NodeHandle &nh, Modes &modes, ServeurAction &action_server,
            Publisher &pub_reset,
            ServiceClient &client_reset_sim) {
    this->modes = &modes;
    this->action_server = &action_server;
    this->pub_reset = &pub_reset;
    this->client_reset_sim = &client_reset_sim;
    srv_set_mode = nh.advertiseService(
        "/set_mode", &ServeurServices::setModeCallback, this);
    srv_global_control = nh.advertiseService(
        "/global_control", &ServeurServices::globalControlCallback, this);
  }

  // Ordre précis : annuler les goals, réinitialiser contrôleur et Gazebo, recréer le serveur d'action, puis prévenir les clients (rapport § 6.2).
  void resetRobot() {
    action_server->reset();
    modes->reset();

    std_srvs::Empty service;
    if (!client_reset_sim->call(service)) {
      ROS_WARN("Echec de l'appel au service /gazebo/reset_simulation");
    }
    // Oublie les dates des goals précédents après le reset de l'horloge.
    action_server->redemarrer();
    pub_reset->publish(std_msgs::Empty());
  }

private:
  Modes *modes = nullptr;
  ServeurAction *action_server = nullptr;
  Publisher *pub_reset = nullptr;
  ServiceClient *client_reset_sim = nullptr;
  ServiceServer srv_set_mode;
  ServiceServer srv_global_control;

  bool setModeCallback(set_mode::Request &request, set_mode::Response &response) {
    response.success = modes->set_mode(request.mode);
    if (!response.success) {
      ROS_WARN("Mode inconnu : %s", request.mode.c_str());
    }
    response.mode_courant = modes->mode();
    return true;
  }

  bool globalControlCallback(global_control::Request &request,
                             global_control::Response &response) {
    if (request.command == "reset") {
      resetRobot();
      response.success = true;
    } else {
      response.success = modes->set_pause(request.command);
      if (!response.success) {
        ROS_WARN("Commande globale inconnue : %s", request.command.c_str());
      }
    }
    response.mode_courant = modes->mode();
    return true;
  }
};

#endif
