#ifndef CLAVIER_CPP
#define CLAVIER_CPP

#include <ros/ros.h>
#include <std_msgs/Int8.h>

using std_msgs::Int8;

#define KEY_UP 65
#define KEY_DOWN 66
#define KEY_RIGHT 67
#define KEY_LEFT 68
#define KEY_ZERO 48

enum Arrow { NONE, UP, DOWN, LEFT, RIGHT };

// Ordres issus du clavier, remis à zéro par le contrôleur une fois traités
struct KeyboardInputs {
  Arrow direction = NONE;
  bool reset_requested = false;
};

// Écoute /keyboard_input et garde les ordres reçus dans "inputs"
class Clavier {
public:
  KeyboardInputs inputs;

  void init(NodeHandle &nh) {
    sub_ = nh.subscribe("/keyboard_input", 1000, &Clavier::callback, this);
  }

  void reset() {
    inputs = KeyboardInputs();
  }

private:
  Subscriber sub_;

  void callback(const Int8::ConstPtr &msg) {
    switch (msg->data) {
      case KEY_UP:
        ROS_INFO("UP key received");
        inputs.direction = UP;
        break;
      case KEY_DOWN:
        ROS_INFO("DOWN key received");
        inputs.direction = DOWN;
        break;
      case KEY_LEFT:
        ROS_INFO("LEFT key received");
        inputs.direction = LEFT;
        break;
      case KEY_RIGHT:
        ROS_INFO("RIGHT key received");
        inputs.direction = RIGHT;
        break;
      case KEY_ZERO:
        ROS_INFO("ZERO key received");
        inputs.direction = NONE;
        break;
      default:
        break;
    }
  }
};

#endif
