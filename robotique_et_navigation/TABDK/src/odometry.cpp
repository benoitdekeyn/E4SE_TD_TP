#ifndef ODOMETRY_CPP
#define ODOMETRY_CPP

#include <nav_msgs/Odometry.h>
#include <ros/ros.h>
#include <tf/tf.h>

struct Pose2D {
  double x = 0.0;
  double y = 0.0;
  double rotation = 0.0; // rad
};

// Écoute /odom et garde la dernière position du robot dans "pose"
class Odometry {
public:
  Pose2D pose;

  void init(ros::NodeHandle &nh) {
    sub_ = nh.subscribe("/odom", 1000, &Odometry::callback, this);
  }

private:
  ros::Subscriber sub_;

  void callback(const nav_msgs::Odometry::ConstPtr &msg) {
    pose.x = msg->pose.pose.position.x;
    pose.y = msg->pose.pose.position.y;
    tf::Quaternion q = tf::Quaternion(
        msg->pose.pose.orientation.x,
        msg->pose.pose.orientation.y,
        msg->pose.pose.orientation.z,
        msg->pose.pose.orientation.w);
    pose.rotation = tf::getYaw(q);
  }
};

#endif
