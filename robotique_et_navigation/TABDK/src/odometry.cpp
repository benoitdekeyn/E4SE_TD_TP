#ifndef ODOMETRY_CPP
#define ODOMETRY_CPP

#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Pose2D.h>
#include <geometry_msgs/Twist.h>
#include <ros/ros.h>
#include <tf/tf.h>
#include <cmath>

using namespace ros;
using namespace tf;
using geometry_msgs::Pose2D;
using geometry_msgs::Twist;

// Écoute /odom et conserve la dernière position et les dernières vitesses
class Odometry {
public:
  Pose2D position;
  Twist vitesse; 

  void init(NodeHandle &nh) {
    odom_sub = nh.subscribe("/odom", 100, &Odometry::odomCallback, this);
  }

  double distanceToPosition(double x, double y) const {
    const double delta_x = x - position.x;
    const double delta_y = y - position.y;
    return std::sqrt(delta_x * delta_x + delta_y * delta_y);
  }

  double distanceParcourue() const {
    return distance_parcourue;
  }

private:
  Subscriber odom_sub;
  double distance_parcourue = 0.0;
  bool position_recue = false;

  void odomCallback(const nav_msgs::Odometry::ConstPtr &msg) {
    // Additionne les déplacements entre deux mesures successives.
    if (position_recue) {
      distance_parcourue += distanceToPosition(msg->pose.pose.position.x,
                                              msg->pose.pose.position.y);
    }
    position_recue = true;

    Quaternion quaternion_odom = Quaternion(
      msg->pose.pose.orientation.x,
      msg->pose.pose.orientation.y,
      msg->pose.pose.orientation.z,
      msg->pose.pose.orientation.w
    );
    position.x = msg->pose.pose.position.x;
    position.y = msg->pose.pose.position.y;
    position.theta = getYaw(quaternion_odom);
    vitesse = msg->twist.twist;
  }
};

#endif
