#include "ros/ros.h"
#include "std_msgs/Bool.h"
#include "std_msgs/Empty.h"
#include "std_msgs/Int8.h"
#include "std_msgs/String.h"
#include "TABDK/global_control.h"
#include "TABDK/set_mode.h"

#include <string>

#include "../modes.cpp"
#include "../parametres.cpp"

using namespace ros;
using namespace TABDK;
using std::string;
using std_msgs::Bool;
using std_msgs::Empty;
using std_msgs::Int8;
using std_msgs::String;

Params params;

const int KEY_M = 109;
const int KEY_P = 112;
const int KEY_R = 114;

ServiceClient client_set_mode;
ServiceClient client_global_control;
string current_mode;
bool is_paused = false;
bool mode_received = false;
bool pause_received = false;

void resetCallback(const Empty::ConstPtr &) {
  current_mode.clear();
  is_paused = false;
  // Attendre les prochains états publiés par le contrôleur.
  mode_received = false;
  pause_received = false;
}

void modeCallback(const String::ConstPtr &message) {
  current_mode = message->data;
  mode_received = true;
}

void pauseCallback(const Bool::ConstPtr &message) {
  is_paused = message->data;
  pause_received = true;
}

bool callSetMode(const string &mode) {
  set_mode service;
  service.request.mode = mode;
  if (!client_set_mode.call(service)) {
    ROS_WARN("Impossible d'appeler le service /set_mode");
    return false;
  } else if (!service.response.success) {
    ROS_WARN("Le service /set_mode a echoue");
    return false;
  }
  return true;
}

bool callGlobalControl(const string &state) {
  global_control service;
  service.request.command = state;
  if (!client_global_control.call(service)) {
    ROS_WARN("Impossible d'appeler le service /global_control");
    return false;
  } else if (!service.response.success) {
    ROS_WARN("Le service /global_control a echoue");
    return false;
  }
  return true;
}

void keyboardCallback(const Int8::ConstPtr &message) {
  switch (message->data) {
  case KEY_M: {
    ROS_INFO("\n##    Touche 'm' pressee    ##\n");
    if (!mode_received) {
      ROS_WARN("Le mode courant n'a pas encore ete recu sur /mode");
      break;
    }
    string requested_mode;
    if (current_mode == AUTO) {
      requested_mode = MANUAL;
    }
    if (current_mode == MANUAL) {
      requested_mode = AUTO;
    }
    if (!requested_mode.empty()) {
      if (callSetMode(requested_mode)) {
        current_mode = requested_mode;
        ROS_INFO("\n\n###    Basculement en MODE %s    ###\n", current_mode.c_str());
      } else {
        ROS_WARN("Echec du changement de mode vers : %s", requested_mode.c_str());
      }
    } else {
      ROS_WARN("Mode courant non implemente : %s", current_mode.c_str());
    }
    break;
  }
  case KEY_P:
    ROS_INFO("\n##    Touche 'p' pressee    ##\n");
    if (!pause_received) {
      ROS_WARN("L'etat de pause n'a pas encore ete recu sur /pause");
      break;
    }
    if (callGlobalControl(is_paused ? "play" : "pause")) {
      is_paused = !is_paused;
      ROS_INFO("##    Mise en %s    ##", is_paused ? "pause" : "play");
    }
    break;
  case KEY_R:
    ROS_INFO("\n##    Touche 'r' pressee    ##\n");
    if (callGlobalControl("reset")) {
      ROS_INFO("\n###    Reinitialisation du Robot    ###\n");
    }
    break;
  default:
    break;
  }
}

int main(int argc, char **argv) {
  init(argc, argv, "client_services");
  NodeHandle nh;
  params.load(nh);

  Rate loop_rate(params.frequence);

  client_set_mode           = nh.serviceClient<set_mode>("/set_mode");
  client_global_control     = nh.serviceClient<global_control>("/global_control");
  Subscriber mode_sub       = nh.subscribe("/mode", 10, modeCallback);
  Subscriber pause_sub      = nh.subscribe("/pause", 10, pauseCallback);
  Subscriber reset_sub      = nh.subscribe("/reset", 10, resetCallback);
  Subscriber key_input_sub  = nh.subscribe("/keyboard_input", 1000, keyboardCallback);

  spin();
  return 0;
}
