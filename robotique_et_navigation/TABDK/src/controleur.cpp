#include "ros/publisher.h"
#include <csignal>
#include <geometry_msgs/Twist.h>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include "std_msgs/Int8.h"
#include <string>
#include <nav_msgs/Odometry.h>
#include <tf/tf.h>
#include <std_msgs/Empty.h>
#include <std_srvs/Empty.h>
#include <cmath>

#define X 0
#define Y 1

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
#define KEY_R 114

#define Kp_POS 1.0
#define Kd_POS 1.0
#define Ki_POS 1.0
#define SEUIL_PRECISION_POS 0.02 // m
#define Kp_ANG 2.0
#define Kd_ANG 1.0
#define Ki_ANG 1.0
#define SEUIL_PRECISION_ANG 2 // deg
#define SEUIL_NOUVELLE_RECHERCHE 4 // deg

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
  STOP,
  REACH_POSITION,
  REACH_POSITION_X,
  REACH_ANGLE
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

struct odometry {
  double x;
  double y;
  double rotation;
};

#define DEFAULT_STATE STOP
#define DEFAULT_MODE MANUAL

RobotState current_state = DEFAULT_STATE;
RobotState previous_state = DEFAULT_STATE;
RobotMode current_mode = DEFAULT_MODE;
RobotMode previous_mode = DEFAULT_MODE;
int ticks_counter = 0;

//autres variables globales
Publisher pub_vel;
Subscriber sub_keyboard;
Subscriber sub_odometry;
Publisher pub_reset;
ServiceClient client_reset_sim;

Twist robot_velocity;
odometry robot_odometry = {0.0, 0.0, 0.0};
int move_counter = 0;
int move_fwd_tm = MOVE_FWD_TM;
int move_bwd_tm = MOVE_BWD_TM;
int move_stp_tm = MOVE_STP_TM;
int move_tot_tm = move_fwd_tm + move_bwd_tm + move_stp_tm;
double linear_speed = LINEAR_SPEED;
double angular_speed = ANGULAR_SPEED;
double frequence = FREQUENCY;
double target_position_x = 0.0;
double target_angle = 0.0;
double target_position[2] = {0.0, 0.0};
double angle_target = 0.0;
double erreur_angle = 0.0;
double erreur_distance = 0.0;
double delta_x = 0.0;
double delta_y = 0.0;

// structure qui contient les ordres issus du clavier
KeyboardInputs keyboard_inputs = {false, NONE, false};

double rad_to_deg(double radians) {
  return radians * (180.0 / M_PI);
}

double deg_to_rad(double degrees) {
  return degrees * (M_PI / 180.0);
}

double cal_angle_offset(double current_angle, double offset) {
  double new_angle = current_angle + offset;
  while (new_angle > M_PI) new_angle -= 2 * M_PI;
  while (new_angle < -M_PI) new_angle += 2 * M_PI;
  return new_angle;
}

float cal_delta_Theta(float _theta, float _theta0) 
{
  float theta, theta0, delta_theta;

  if  (_theta < 0)  theta = _theta + 2 * M_PI;
  else theta = _theta;

  if  (_theta0 < 0)  theta0 = _theta0 + 2 * M_PI;
  else theta0 = _theta0;

  delta_theta = theta - theta0;

  if (delta_theta < -M_PI)
  {
      delta_theta = delta_theta + 2 * M_PI;
  } 
  else 
  {
    if (delta_theta >= M_PI) {
      delta_theta = delta_theta - 2 * M_PI;
    }
  }
  return delta_theta;
}

// L'impl des fonctionsm
int ms_to_ticks(int ms) {
  return frequence * (ms / 1000);
}


void odometryCallback(const nav_msgs::Odometry::ConstPtr &msg){
  robot_odometry.x = msg->pose.pose.position.x;
  robot_odometry.y = msg->pose.pose.position.y;
  tf::Quaternion q = tf::Quaternion(
      msg->pose.pose.orientation.x,
      msg->pose.pose.orientation.y,
      msg->pose.pose.orientation.z,
      msg->pose.pose.orientation.w
  );
  robot_odometry.rotation = tf::getYaw(q);
}

void stop() {
  robot_velocity = Twist();
  pub_vel.publish(robot_velocity);
}

void forward(double v) {
  robot_velocity = Twist();
  robot_velocity.linear.x = v;
  pub_vel.publish(robot_velocity);
}

void pivoter(double w) {
  robot_velocity = Twist();
  robot_velocity.angular.z = w;
  pub_vel.publish(robot_velocity);
}

void backward(double v) {
  robot_velocity = Twist();
  robot_velocity.linear.x = -v;
  pub_vel.publish(robot_velocity);
}

void move(double linear, double angular) {
  robot_velocity = Twist();
  robot_velocity.linear.x = linear;
  robot_velocity.angular.z = angular;
  pub_vel.publish(robot_velocity);
}

void reach_position_x(double x) {
  double error = x - robot_odometry.x;
  double control_signal = fabs(Kp_POS * error);
  control_signal = control_signal > linear_speed ? linear_speed : control_signal;

  if (error > SEUIL_PRECISION_POS) {
    forward(control_signal);
  } else if (error < -SEUIL_PRECISION_POS) {
    backward(control_signal);
  } else {
    stop();
  }
}

void reach_angle(double angle) {
  double error = cal_delta_Theta(angle, robot_odometry.rotation);
  double control_signal = fabs(Kp_ANG * error);
  control_signal = control_signal > angular_speed ? angular_speed : control_signal;

  if (error > deg_to_rad(SEUIL_PRECISION_ANG)) {
    pivoter(control_signal);
  } else if (error < -deg_to_rad(SEUIL_PRECISION_ANG)) {
    pivoter(-control_signal);
  } else {
    stop();
  }
}

void reach_position(double x, double y) {
  delta_x = x - robot_odometry.x;
  delta_y = y - robot_odometry.y;
  angle_target = atan2(delta_y, delta_x);
  erreur_distance = sqrt(pow(delta_x, 2) + pow(delta_y, 2));
  erreur_angle = cal_delta_Theta(angle_target, robot_odometry.rotation);

  if (fabs(erreur_angle) > deg_to_rad(SEUIL_PRECISION_ANG_)) {

  if (fabs(erreur_angle) > deg_to_rad(SEUIL_PRECISION_ANG)) {
    double control_signal_a = fabs(Kp_ANG * erreur_angle);
    control_signal_a = control_signal_a > angular_speed ? angular_speed : control_signal_a;

    if (erreur_angle > deg_to_rad(SEUIL_PRECISION_ANG)) {
      pivoter(control_signal_a);
    } else if (erreur_angle < -deg_to_rad(SEUIL_PRECISION_ANG)) {
      pivoter(-control_signal_a);
    }
  }

  else
  {
    double control_signal = fabs(Kp_POS * erreur_distance);
    control_signal = control_signal > linear_speed ? linear_speed : control_signal;

    if (erreur_distance > SEUIL_PRECISION_POS) {
      forward(control_signal);
    } else if (erreur_distance < -SEUIL_PRECISION_POS) {
      backward(control_signal);
    } else {
      stop();
    }
  }
}

void reset_robot() {
  // Robot réel : message vide sur /reset
  pub_reset.publish(std_msgs::Empty());
  // Simulation Gazebo
  std_srvs::Empty srv;
  if (!client_reset_sim.call(srv)) {
    ROS_WARN("Echec de l'appel au service /gazebo/reset_simulation");
  }
  stop();
  // Remise à zéro de l'état interne du contrôleur
  current_state = DEFAULT_STATE;
  previous_state = DEFAULT_STATE;
  current_mode = DEFAULT_MODE;
  previous_mode = DEFAULT_MODE;
  move_counter = 0;
  ticks_counter = 0;
  keyboard_inputs.direction = NONE;
  keyboard_inputs.play_pause = 0;
  target_position_x = 0.0;
  target_angle = 0.0;
  target_position[0] = 0.0;
  target_position[1] = 0.0;
  delta_x = 0.0;
  delta_y = 0.0;
  erreur_distance = 0.0;
  erreur_angle = 0.0;
  angle_target = 0.0;
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
    case KEY_R: {
      ROS_INFO("R key received : reset robot + simulation");
      reset_robot();
      break;
    }
    default:
      break;
  }
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
    case REACH_POSITION:
      reach_position(target_position[X], target_position[Y]);
      break;
    case REACH_POSITION_X:
      reach_position_x(target_position_x);
      break;
    case REACH_ANGLE:
      reach_angle(target_angle);
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
  switch (keyboard_inputs.direction) {
    case UP:
      target_position[X] += 0.1;
      current_state = REACH_POSITION;
      break;
    case DOWN:
      target_position[X] -= 0.1;
      current_state = REACH_POSITION;
      break;
    case LEFT:
      target_position[Y] -= 0.1;
      current_state = REACH_POSITION;
      break;
    case RIGHT:
      target_position[Y] += 0.1;
      current_state = REACH_POSITION;
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
      keyboard_inputs.direction = NONE;
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
  nh.getParam("angular_speed", angular_speed);
  nh.getParam("move_fwd_tm", move_fwd_tm);
  nh.getParam("move_bwd_tm", move_bwd_tm);
  nh.getParam("move_stp_tm", move_stp_tm);

  pub_vel = nh.advertise<Twist>("/cmd_vel", 1000);
  pub_reset = nh.advertise<std_msgs::Empty>("/reset", 1);
  client_reset_sim = nh.serviceClient<std_srvs::Empty>("/gazebo/reset_simulation");

  sub_keyboard = nh.subscribe("/keyboard_input", 1000, keyboardCallback);
  sub_odometry = nh.subscribe("/odom", 1000, odometryCallback);

  Rate loop_rate(frequence);

  while (ok()) {

    ROS_INFO_THROTTLE(1, "x=%.4f y=%.4f rotation=%.4f deg", robot_odometry.x, robot_odometry.y, rad_to_deg(robot_odometry.rotation));
    ROS_INFO_THROTTLE(1, "target_x=%.4f target_y=%.4f", target_position[X], target_position[Y]);
    ROS_INFO_THROTTLE(1, "angle_target=%.4f deg, erreur_angle=%.4f deg, erreur_distance=%.4f", rad_to_deg(angle_target), rad_to_deg(erreur_angle), erreur_distance);
    ROS_INFO_THROTTLE(1, "delta_x=%.4f delta_y=%.4f", delta_x, delta_y);
    
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

  reset_robot();

  // Kill all open subscriptions, publications, service calls, and service
  // servers
  shutdown();
}
