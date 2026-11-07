#include "ros/ros.h"
#include "std_msgs/Int8.h"
#include "TABDK/global_control.h"
#include "TABDK/set_mode.h"

using namespace ros;
using namespace TABDK;

const int KEY_M = 109;
const int KEY_P = 112;
const int KEY_R = 114;

ServiceClient client_set_mode;
ServiceClient client_global_control;

void keyboardCallback(const std_msgs::Int8::ConstPtr &message) {
  switch (message->data) {
  case KEY_M: {
    set_mode service;
    service.request.mode = "toggle";
    if (!client_set_mode.call(service)) {
      ROS_WARN("Impossible d'appeler le service /set_mode");
    } else if (!service.response.success) {
      ROS_WARN("Le service /set_mode a échoué");
    }
    break;
  }
  case KEY_P:
  case KEY_R: {
    global_control service;
    service.request.command =
        message->data == KEY_P ? "play_pause" : "reset";
    if (!client_global_control.call(service)) {
      ROS_WARN("Impossible d'appeler le service /global_control");
    } else if (!service.response.success) {
      ROS_WARN("Le service /global_control a échoué");
    }
    break;
  }
  default:
    break;
  }
}

int main(int argc, char **argv) {
  init(argc, argv, "client_services");
  NodeHandle nh;

  client_set_mode = nh.serviceClient<set_mode>("/set_mode");
  client_global_control =
      nh.serviceClient<global_control>("/global_control");
  Subscriber key_input_sub =
      nh.subscribe("/keyboard_input", 1000, keyboardCallback);

  spin();
  return 0;
}
