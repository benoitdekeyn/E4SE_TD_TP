#ifndef CLAVIER_CPP
#define CLAVIER_CPP

#include <ros/ros.h>
#include <std_msgs/Int8.h>

#define KEY_M 109
#define KEY_SPACE 32
#define KEY_UP 65
#define KEY_DOWN 66
#define KEY_RIGHT 67
#define KEY_LEFT 68
#define KEY_ZERO 48
#define KEY_R 114

enum Arrow { NONE, UP, DOWN, LEFT, RIGHT };

// Ordres issus du clavier, remis à zéro par le contrôleur une fois traités
struct KeyboardInputs {
  bool change_mode = false;
  Arrow direction = NONE;
  bool play_pause = false;
  bool reset_requested = false;
};

// Écoute /keyboard_input et garde les ordres reçus dans "inputs"
class Clavier {
public:
  KeyboardInputs inputs;

  void init(ros::NodeHandle &nh) {
    sub_ = nh.subscribe("/keyboard_input", 1000, &Clavier::callback, this);
  }

  void reset() {
    inputs = KeyboardInputs();
  }

private:
  ros::Subscriber sub_;

  void callback(const std_msgs::Int8::ConstPtr &msg) {
    switch (msg->data) {
      case KEY_M:
        inputs.change_mode = true;
        break;
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
      case KEY_SPACE:
        ROS_INFO("SPACE key received");
        inputs.play_pause = true;
        break;
      case KEY_ZERO:
        ROS_INFO("ZERO key received");
        inputs.direction = NONE;
        break;
      case KEY_R:
        ROS_INFO("R key received : reset robot + simulation");
        // Le reset est exécuté par le contrôleur dans sa boucle
        inputs.reset_requested = true;
        break;
      default:
        break;
    }
  }
};

#endif
