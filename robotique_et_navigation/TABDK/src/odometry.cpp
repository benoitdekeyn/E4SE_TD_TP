#ifndef ODOMETRY_CPP
#define ODOMETRY_CPP

#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Pose2D.h>
#include <ros/ros.h>
#include <tf/tf.h>
#include <cmath>

using namespace ros;
using namespace tf;
using geometry_msgs::Pose2D;

// Écoute /odom et garde la dernière position du robot dans "position" de type Pose2D
class Odometry {
public:
  Pose2D position;

  void init(NodeHandle &nh) {
    odom_sub = nh.subscribe("/odom", 100, &Odometry::odomCallback, this);
  }

  double distanceToPosition(double x, double y) const {
    const double delta_x = x - position.x;
    const double delta_y = y - position.y;
    return std::sqrt(delta_x * delta_x + delta_y * delta_y);
  }

private:
  Subscriber odom_sub;

  void odomCallback(const nav_msgs::Odometry::ConstPtr &msg) {
    Quaternion quaternion_odom = Quaternion(
      msg->pose.pose.orientation.x,
      msg->pose.pose.orientation.y,
      msg->pose.pose.orientation.z,
      msg->pose.pose.orientation.w
    );
    position.x = msg->pose.pose.position.x;
    position.y = msg->pose.pose.position.y;
    position.theta = getYaw(quaternion_odom);
  }
};

#endif
