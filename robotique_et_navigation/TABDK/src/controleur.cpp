#include "ros/publisher.h"
#include <csignal>
#include <geometry_msgs/Twist.h>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <string>

#define FREQUENCY 1
#define MAX_LINEAR_SPEED 0.15
#define MAX_ANGULAR_SPEED 2.84
#define LINEAR_SPEED 0.1
#define ANGULAR_SPEED 0.1

#define MOVE_FWD_PER 3
#define MOVE_BWD_PER 4
#define MOVE_STP_PER 2

using namespace ros;
using namespace std_msgs;
using namespace geometry_msgs;

// Les prototypes des fonctions
void sigintHandler(int sig);

// Les variables globales
Publisher pub_msg;
Publisher pub_vel;
String msg;
Twist vel;
int move_counter = 0;
int move_fwd_per = MOVE_FWD_PER;
int move_bwd_per = MOVE_BWD_PER;
int move_stp_per = MOVE_STP_PER;
int move_tot_per = move_fwd_per + move_bwd_per + move_stp_per;
double linear_speed = LINEAR_SPEED;
double angular_speed = ANGULAR_SPEED;
double frequence = FREQUENCY;

// L'impl des fonctions
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

void move() {

  if (move_counter == 0) {
    move_counter = move_tot_per;
  }
  // ROS_INFO("move_counter %d", move_counter);
  if (move_counter > move_tot_per - move_fwd_per) {
    forward(linear_speed);
    move_counter--;
  } else if (move_counter > move_bwd_per) {
    stop();
    move_counter--;
  } else if (move_counter > 0) {
    backward(linear_speed);
    move_counter--;
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
  nh.getParam("move_fwd_per", move_fwd_per);
  nh.getParam("move_bwd_per", move_bwd_per);
  nh.getParam("move_stp_per", move_stp_per);
  move_tot_per = move_fwd_per + move_bwd_per + move_stp_per;

  msg.data = "un petit log";
  pub_msg = nh.advertise<String>("/log", 1000);

  pub_vel = nh.advertise<Twist>("/cmd_vel", 1000);

  Rate loop_rate(frequence);

  while (ok()) {

    // send_msg(msg);
    move();
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
