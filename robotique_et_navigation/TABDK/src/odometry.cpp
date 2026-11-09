#ifndef ODOMETRY_CPP
#define ODOMETRY_CPP

#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Pose2D.h>
#include <geometry_msgs/Twist.h>
#include <ros/ros.h>
#include <tf/tf.h>
#include <tf/transform_listener.h>
#include <cmath>

using namespace ros;
using namespace tf;
using geometry_msgs::Pose2D;
using geometry_msgs::Twist;

// Écoute /odom et conserve la dernière position et les dernières vitesses.
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
    // Additionne les déplacements (repère odom) entre deux mesures successives.
    const double ox = msg->pose.pose.position.x;
    const double oy = msg->pose.pose.position.y;
    if (position_recue) {
      distance_parcourue += std::hypot(ox - derniere_odom_x, oy - derniere_odom_y);
    }
    derniere_odom_x = ox;
    derniere_odom_y = oy;
    position_recue = true;

    Transform pose_odom(
      Quaternion(msg->pose.pose.orientation.x, msg->pose.pose.orientation.y,
                 msg->pose.pose.orientation.z, msg->pose.pose.orientation.w),
      Vector3(ox, oy, 0.0));

    // Les chemins sont dans le repère "map" : on applique la correction map->odom d'AMCL.
    try {
      StampedTransform map_odom;
      listener.lookupTransform("map", msg->header.frame_id, Time(0), map_odom);
      pose_odom = map_odom * pose_odom;
    } catch (const tf::TransformException &e) {
      ROS_WARN_THROTTLE(2.0, "Odometry : transform map->%s indisponible, repère odom utilisé (%s)",
                        msg->header.frame_id.c_str(), e.what());
    }

    position.x = pose_odom.getOrigin().x();
    position.y = pose_odom.getOrigin().y();
    position.theta = getYaw(pose_odom.getRotation());
    vitesse = msg->twist.twist;
  }

  TransformListener listener;
  double derniere_odom_x = 0.0;
  double derniere_odom_y = 0.0;
};

#endif
