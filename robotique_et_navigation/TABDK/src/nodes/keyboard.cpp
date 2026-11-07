#include "ros/ros.h"
#include "TABDK/set_mode.h"
#include "TABDK/global_control.h"
#include "std_msgs/Int8.h"
#include "termios.h"

using namespace ros;
using namespace TABDK;

const int KEY_M = 109;
const int KEY_P = 112;
const int KEY_R = 114;

const int FREQ_ACTUALISATION = 5; // Hz

int getch();
std_msgs::Int8 keyboard_input;
Publisher KeyInput_Pub;

int main(int argc, char **argv) {
  init(argc, argv, "key_input_node");

  NodeHandle nh;

  // Publishers
  KeyInput_Pub = nh.advertise<std_msgs::Int8>("/keyboard_input", 1000);
  ServiceClient client_set_mode =
      nh.serviceClient<set_mode>("/set_mode");
  ServiceClient client_global_control =
      nh.serviceClient<global_control>("/global_control");

  Rate loop_rate(FREQ_ACTUALISATION);

  while (ok()) {
    const int key = getch();
    switch (key) {
      case KEY_M: {
        // Appel du service /set_mode avec le mode "toggle"
        set_mode service;
        service.request.mode = "toggle";
        if (!client_set_mode.call(service)) {
          ROS_WARN("Impossible d'appeler le service /set_mode");
        } else if (!service.response.success) {
          ROS_WARN("Le service /set_mode a échoué");
        }
        break;
      }
      case KEY_P: {
        // Appel du service /global_control avec la commande "play_pause"
        global_control service;
        service.request.command = "play_pause";
        if (!client_global_control.call(service)) {
          ROS_WARN("Impossible d'appeler le service /global_control");
        } else if (!service.response.success) {
          ROS_WARN("Le service /global_control a échoué");
        }
        break;
      }
      case KEY_R: {
        // Appel du service /global_control avec la commande "reset"
        global_control service;
        service.request.command = "reset";
        if (!client_global_control.call(service)) {
          ROS_WARN("Impossible d'appeler le service /global_control");
        } else if (!service.response.success) {
          ROS_WARN("Le service /global_control a échoué");
        }
        break;
      }
      default: {
        keyboard_input.data = key;
        KeyInput_Pub.publish(keyboard_input);
        ROS_INFO("keyb = %d", keyboard_input.data);
        break;
      }
    }

    spinOnce();
    loop_rate.sleep();
  }
  return 0;
}

int getch() {
  static struct termios oldt, newt;
  tcgetattr(STDIN_FILENO, &oldt); // save old settings
  newt = oldt;
  newt.c_lflag &= ~(ICANON);               // disable buffering
  tcsetattr(STDIN_FILENO, TCSANOW, &newt); // apply new settings

  int c = getchar(); // read character (non-blocking)

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // restore old settings
  return c;
}
