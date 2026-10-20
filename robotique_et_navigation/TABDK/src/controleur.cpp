#include "ros/publisher.h"
#include <csignal>
#include <geometry_msgs/Twist.h>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include "std_msgs/Int8.h"
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
#define KEY_ZERO 48

using namespace ros;
using namespace std_msgs;
using namespace geometry_msgs;

// Les prototypes des fonctions
void sigintHandler(int sig);

//FSM 
enum RobotState {
  FORWARD,
  BACKWARD,
  PIVOTER_GAUCHE,
  PIVOTER_DROITE,
  STOP
};

enum RobotMode {
  AUTO,
  MANUAL,
  PAUSE,
};

enum Arrow {
  NONE,
  UP,
  DOWN,
  LEFT,
  RIGHT
};

struct KeyboardInputs {
  bool change_mode;
  Arrow direction;
  bool play_pause;
};

#define DEFAULT_STATE FORWARD
#define DEFAULT_MODE AUTO

RobotState current_state = DEFAULT_STATE;
RobotState previous_state = DEFAULT_STATE;
RobotMode current_mode = DEFAULT_MODE;
RobotMode previous_mode = DEFAULT_MODE;
int ticks_counter = 0;

//autres variables globales
Publisher pub_vel;
Subscriber sub_keyboard;

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

void keyboardCallback(const Int8::ConstPtr &msg){
  int key = msg->data;
  //ROS_INFO("Keyboard input : %d", key);
  switch (key) {
    case KEY_M:
      keyboard_inputs.change_mode = true;
      break;
    case KEY_UP:
      ROS_INFO("UP key received");
      keyboard_inputs.direction = UP;
      break;
    case KEY_DOWN:
      ROS_INFO("DOWN key received");
      keyboard_inputs.direction = DOWN;
      break;
    case KEY_LEFT:
      ROS_INFO("LEFT key received");
      keyboard_inputs.direction = LEFT;
      break;
    case KEY_RIGHT:
      ROS_INFO("RIGHT key received");
      keyboard_inputs.direction = RIGHT;
      break;
    case KEY_SPACE:
      ROS_INFO("SPACE key received");
      keyboard_inputs.play_pause = 1;
      break;
    case KEY_ZERO:
      ROS_INFO("ZERO key received");
      keyboard_inputs.direction = NONE;
      break;
    default:
      break;
  }
}

void stop() {
  vel = Twist();
  pub_vel.publish(vel);
}

void forward(double v) {
  vel.linear.x = v;
  pub_vel.publish(vel);
}

void pivoter(double w) {
  vel.angular.z = w;
  pub_vel.publish(vel);
}

void backward(double v) {
  vel.linear.x = -v;
  pub_vel.publish(vel);
}

void apply_state() {
  switch(current_state){
    case FORWARD:
      forward(linear_speed);
      break;
    case PIVOTER_GAUCHE:
      pivoter(angular_speed);
      break;
    case PIVOTER_DROITE:
      pivoter(-angular_speed);
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

void update_Mode() {
  switch(current_mode) {
    case AUTO:
      if (keyboard_inputs.change_mode) {
        previous_mode = current_mode;
        current_mode = MANUAL;
        ROS_INFO("Switching to MANUAL mode");
        // reset
        current_state = STOP;
        keyboard_inputs.direction = NONE;
      } else if (keyboard_inputs.play_pause) {
        previous_mode = current_mode;
        current_mode = PAUSE;
        ROS_INFO("Switching to PAUSE mode");
      }
      break;

    case MANUAL:
      if (keyboard_inputs.change_mode) {
        previous_mode = current_mode;
        current_mode = AUTO;
        ROS_INFO("Switching to AUTO mode");
        // reset
        current_state = DEFAULT_STATE;
        ticks_counter = 0;
      } else if (keyboard_inputs.play_pause) {
        previous_mode = current_mode;
        current_mode = PAUSE;
        ROS_INFO("Switching to PAUSE mode");
      }
      break;

    case PAUSE:
      if (keyboard_inputs.play_pause) {
        current_mode = previous_mode;
        ROS_INFO("Switching to %s mode", (current_mode == AUTO) ? "AUTO" : "MANUAL");
      }
      break;

    default:
      current_mode = DEFAULT_MODE;
      break;
  }

  keyboard_inputs.change_mode = false;
  keyboard_inputs.play_pause = false;
}

void update_FSM_auto() {
  switch(current_state) {
    case FORWARD:
      if (ms_to_ticks(move_fwd_tm) == ticks_counter) {
        ROS_INFO("Switching to STOP state");
        ticks_counter = 0;
        current_state = STOP;
      }
      break;
    case STOP:
      if (ms_to_ticks(move_stp_tm) == ticks_counter) {
        ROS_INFO("Switching to BACKWARD state");
        ticks_counter = 0;
        current_state = BACKWARD;
      }
      break;
    case BACKWARD:
      if (ms_to_ticks(move_bwd_tm) == ticks_counter) {
        ROS_INFO("Switching to FORWARD state");
        ticks_counter = 0;
        current_state = FORWARD;
      }
      break;
    default:
      current_state = DEFAULT_STATE;
      break;
  }
  ticks_counter++;
}

void update_FSM_manual() {
  if (current_mode != MANUAL)
    return;
  switch (keyboard_inputs.direction) {
    case UP:
      current_state = FORWARD;
      break;
    case DOWN:
      current_state = BACKWARD;
      break;
    case LEFT:
      current_state = PIVOTER_GAUCHE;
      break;
    case RIGHT:
      current_state = PIVOTER_DROITE;
      break;
    default:
      break;
  }
  keyboard_inputs.direction = NONE;
}


void update_FSM() {
  switch(current_mode) {
    case AUTO:
      update_FSM_auto();
      apply_state();
      break;
    case MANUAL:
      update_FSM_manual();
      apply_state();
      break;
    case PAUSE:
      stop();
      break;
    default:
      break;
  }
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

  pub_vel = nh.advertise<Twist>("/cmd_vel", 1000);

  sub_keyboard = nh.subscribe("/keyboard_input", 1000, keyboardCallback);

  Rate loop_rate(frequence);

  while (ok()) {
    
    update_Mode();
    update_FSM();

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
