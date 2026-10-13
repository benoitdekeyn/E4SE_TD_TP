#include "ros/publisher.h"
#include <csignal>
#include <geometry_msgs/Twist.h>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <string>

#define FREQUENCY 5
#define MAX_LINEAR_SPEED 0.15
#define MAX_ANGULAR_SPEED 2.84
#define LINEAR_SPEED 0.1
#define ANGULAR_SPEED 0.1

#define MOVE_FWD_TM 3000
#define MOVE_BWD_TM 4000
#define MOVE_STP_TM 2000

#define KEY_M 109
#define KEY_SPACE 32
#define KEY_UP 65
#define KEY_DOWN 66
#define KEY_RIGHT 67
#define KEY_LEFT 68

using namespace ros;
using namespace std_msgs;
using namespace geometry_msgs;

// Les prototypes des fonctions
void sigintHandler(int sig);

//FSM 
enum RobotState {
  FORWARD,
  BACKWARD,
  STOP
};

enum RobotMode {
  AUTO,
  MANUAL
};

enum Direction {
  NONE,
  UP,
  DOWN,
  LEFT,
  RIGHT
};

struct KeyboardInputs {
  bool change_mode;
  Direction direction;
  bool stop;
};

#define DEFAULT_STATE FORWARD
#define DEFAULT_MODE AUTO

RobotState current_state = DEFAULT_STATE;
RobotMode current_mode = DEFAULT_MODE;
int ticks_counter = 0;

//autres variables globales
Publisher pub_msg;
Publisher pub_vel;
Subscriber sub_keyboard;

String msg;
Twist vel;
int move_counter = 0;
int move_fwd_tm = MOVE_FWD_TM;
int move_bwd_tm = MOVE_BWD_TM;
int move_stp_tm = MOVE_STP_TM;
int move_tot_tm = move_fwd_tm + move_bwd_tm + move_stp_tm;
double linear_speed = LINEAR_SPEED;
double angular_speed = ANGULAR_SPEED;
double frequence = FREQUENCY;

// structure qui contient les ordres issus du clavier
KeyboardInputs keyboard_inputs = {false, NONE, false};

// L'impl des fonctions
int ms_to_ticks(int ms) {
  return frequence * (ms / 1000);
}

void keyboardCallback(const String::ConstPtr &msg){
  ROS_INFO("Keyboard input : %d", msg->data);
  switch (msg->data) {
    case KEY_M:
      keyboard_inputs.change_mode = true;
      break;
    case KEY_UP:
      keyboard_inputs.direction = UP;
      break;
    case KEY_DOWN:
      keyboard_inputs.direction = DOWN;
      break;
    case KEY_LEFT:
      keyboard_inputs.direction = LEFT;
      break;
    case KEY_RIGHT:
      keyboard_inputs.direction = RIGHT;
      break;
    case KEY_SPACE:
      keyboard_inputs.stop = true;
      break;
    default:
      break;
  }
}

void stop() {
  ROS_INFO("Stop");
  vel.linear.x = 0;
  pub_vel.publish(vel);
}

void forward(double v) {
  ROS_INFO("Forward : %2f", v);
  vel.linear.x = v;
  pub_vel.publish(vel);
}

void backward(double v) {
  ROS_INFO("Backward : -%2f", v);
  vel.linear.x = -v;
  pub_vel.publish(vel);
}

void update_Mode() {
  switch(current_mode) {
    case AUTO:
      if (keyboard_inputs.change_mode) {
        current_mode = MANUAL;
      }
      break;
    case MANUAL:
      if (keyboard_inputs.change_mode) {
        current_mode = AUTO;
      }
      break;
    default:
      current_mode = DEFAULT_MODE;
      break;
  }
  keyboard_inputs.change_mode = false;
}

void update_FSM_auto() {
  switch(current_state) {
    case FORWARD:
      if (ms_to_ticks(move_fwd_tm) == ticks_counter) {
        ticks_counter = 0;
        current_state = STOP;
      }
      break;
    case STOP:
      if (ms_to_ticks(move_stp_tm) == ticks_counter) {
        ticks_counter = 0;
        current_state = BACKWARD;
      }
      break;
    case BACKWARD:
      if (ms_to_ticks(move_bwd_tm) == ticks_counter) {
        ticks_counter = 0;
        current_state = FORWARD;
      }
      break;
    default:
      current_state = DEFAULT_STATE;
      break;
  }
}

void update_FSM_manual() {
  if (keyboard_inputs.stop) {
    current_state = STOP;
    return;
  }

  switch (keyboard_inputs.direction) {
    case UP:
      current_state = FORWARD;
      break;
    case DOWN:
      current_state = BACKWARD;
      break;
    default:
      break;
  }
  keyboard_inputs.stop = false;
  keyboard_inputs.direction = NONE;
}

void update_FSM() {
  switch(current_mode) {
    case AUTO:
      update_FSM_auto();
      break;
    case MANUAL:
      update_FSM_manual();
      break;
    default:
      break;
  }
}

void apply_state() {
  switch(current_state){
    case FORWARD:
      forward(linear_speed);
      break;
    case STOP:
      stop();
      break;
    case BACKWARD:
      backward(linear_speed);
      break;
    default:
      stop();
      break; 
  }
}

void send_msg(String msg) {
  ROS_INFO("Publisher : %s", msg.data.c_str());
  pub_msg.publish(msg);
}

int main(int argc, char **argv) {
  signal(SIGINT, sigintHandler);
  ros::init(argc, argv, "controleur", ros::init_options::NoSigintHandler);

  NodeHandle nh;

  nh.getParam("frequence", frequence);
  nh.getParam("linear_speed", linear_speed);
  nh.getParam("move_fwd_tm", move_fwd_tm);
  nh.getParam("move_bwd_tm", move_bwd_tm);
  nh.getParam("move_stp_tm", move_stp_tm);

  msg.data = "un petit log";
  pub_msg = nh.advertise<String>("/log", 1000);

  pub_vel = nh.advertise<Twist>("/cmd_vel", 1000);

  sub_keyboard = nh.subscribe("/keyboard_input", 1000, keyboardCallback);

  Rate loop_rate(frequence);

  while (ok()) {

    // send_msg(msg);
    
    update_Mode();
    update_FSM();
    apply_state();
    ticks_counter++;

    spinOnce();
    loop_rate.sleep();
  }
  return 0;
}

/**
 *
 * SIGINT handler.
 * Exit code gracefully.
 *
 * @param sig - catched signal.
 *
 */

void sigintHandler(int sig) {
  // Log quit
  ROS_INFO("Exiting program gracefully ...");

  stop();
  // MESSAGE A PUBLIER le MESSAGE

  // Kill all open subscriptions, publications, service calls, and service
  // servers
  shutdown();
}
