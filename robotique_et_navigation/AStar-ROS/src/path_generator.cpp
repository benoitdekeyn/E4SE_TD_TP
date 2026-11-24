#include "../include/path_generator/path_generator.h"
#include "../include/path_generator/path_simplifier.h"
#include <algorithm>
#include <cmath>
#include <csignal>
#include <geometry_msgs/Pose2D.h>
#include <geometry_msgs/Quaternion.h>
#include <iostream>
#include <nav_msgs/Odometry.h>
#include <ros/ros.h>
#include <string>
#include <tf/tf.h>

using namespace nav_msgs;
using namespace ros;
using namespace std_msgs;
using namespace geometry_msgs;

float current_pos_x = 0.2;
float current_pos_y = 0.0;
Pose2D odom_pos;

PathGenerator::PathGenerator() { subscribeAndPublish(); }

PathGenerator::~PathGenerator() {}

void PathGenerator::subscribeAndPublish() {
  sub_grid_map_ = nh_.subscribe<nav_msgs::OccupancyGrid>(
      "map", 1, &PathGenerator::gridMapHandler, this);

  sub_initial_pose_ = nh_.subscribe<geometry_msgs::PoseWithCovarianceStamped>(
      "initialpose", 1, &PathGenerator::initialPoseHandler, this);

  sub_nav_goal_ = nh_.subscribe<geometry_msgs::PoseStamped>(
      "move_base_simple/goal", 1, &PathGenerator::navGoalHandler, this);

  pub_robot_path_ = nh_.advertise<nav_msgs::Path>("robot_path", 1, true);
}

void PathGenerator::gridMapHandler(const nav_msgs::OccupancyGrid map_msg) {
  ROS_INFO("reading map..");
  map_exsit_ = false;

  map_info_ = map_msg.info;

  ROS_INFO("map size => w = %d h = %d", (int)map_info_.width,
           (int)map_info_.height);
  int ecart_obstacle;
  nh_.param("ecart_obstacle", ecart_obstacle, 2);
  if (ecart_obstacle < 0) {
    ROS_ERROR("ecart_obstacle doit etre positif ou nul (en cases)");
    return;
  }
  // Generate Map, Options
  map_generator_.setWorldSize(
      {(int)map_info_.width, (int)map_info_.height}); //{x, y}
  map_generator_.setEcart_obstacle(ecart_obstacle);
  map_generator_.setHeuristic(AStar::Heuristic::manhattan);
  map_generator_.setDiagonalMovement(false);
  map_generator_.ocg = map_msg;

  ROS_INFO("Success build map!");
  map_exsit_ = true;
}

void PathGenerator::initialPoseHandler(
    const geometry_msgs::PoseWithCovarianceStamped init_msg) {
  ROS_INFO("\033[1;32m inital pose x = %f y = %f \033[0m",
           init_msg.pose.pose.position.x, init_msg.pose.pose.position.y);
}

void PathGenerator::navGoalHandler(
    const geometry_msgs::PoseStamped::ConstPtr &goal_msg) {
  if (!map_exsit_) {
    ROS_INFO("\033[1;32m map not exist ?? !\033[0m");
    return;
  }

  ROS_INFO("\033[1;32mGoal received!\033[0m");

  // Round goal coordinate
  float goal_x = round(goal_msg->pose.position.x * 10) / 10;
  float goal_y = round(goal_msg->pose.position.y * 10) / 10;

  ROS_INFO("\033[1;32mGoal x = %f y = %f \033[0m", goal_x, goal_y);
  ROS_INFO("\033[1;32mOrigin origin_x = %f origin_y = %f \033[0m",
           map_info_.origin.position.x, map_info_.origin.position.y);
  ROS_INFO("\033[1;32mOrigin resolution = %f  \033[0m", map_info_.resolution);

  // Le point de départ vient de /odom (et non d'AMCL), d'où l'alignement du reset sur l'origine de la carte (rapport § 7.2).
  AStar::Vec2i source;
  source = map_generator_.xy_to_ij(odom_pos.x, odom_pos.y, map_info_);
  ROS_INFO("\033[1;32m remap coordinante source x = %d y = %d \033[0m",
           source.x, source.y);

  AStar::Vec2i target;
  target = map_generator_.xy_to_ij(goal_x, goal_y, map_info_);

  ROS_INFO("\033[1;32m remap coordinate target  x = %d y = %d \033[0m",
           target.x, target.y);

  // Find Path
  auto path = map_generator_.findPath(source, target);

  nav_msgs::Path path_msg;
  if (path.empty()) {
    ROS_ERROR("Echec de la generation du chemin ; voir le diagnostic A*");
    return;
  }

  // A* renvoie le chemin de l'arrivée vers le départ : on l'inverse puis on supprime les waypoints intermédiaires inutiles (rapport § 7.2).
  std::reverse(path.begin(), path.end());
  size_t path_size = path.size();
  path = simplifyPath(path, map_generator_);
  if (path.empty()) {
    ROS_ERROR("Impossible de simplifier le chemin avec la marge d'obstacle");
    return;
  }
  ROS_INFO("Chemin simplifie : %zu points -> %zu waypoints", path_size,
           path.size());

  ROS_INFO("\033[1;31m generate path OK !\033[0m");
  int i = 0;
  for (const auto &coordinate : path) {
    geometry_msgs::PoseStamped point_pose;

    // Remmaping coordinate
    geometry_msgs::Pose2D pos =
        map_generator_.ij_to_xy(coordinate.x, coordinate.y, map_info_);
    point_pose.pose.position.x = pos.x + map_info_.resolution / 2.0;
    point_pose.pose.position.y = pos.y + map_info_.resolution / 2.0;
    path_msg.poses.push_back(point_pose);
    ROS_INFO("pos i=%d x =%f y=%f", i++, point_pose.pose.position.x,
             point_pose.pose.position.y);
  }

  path_msg.header.frame_id = "map";
  pub_robot_path_.publish(path_msg);

  ROS_INFO("\033[1;36mSuccess generate path!\033[0m");
}

void odomCallback(Odometry odom_msg) {
  // ROS_INFO("\033[1;32m----> ODOM \033[0m");

  odom_pos.x = odom_msg.pose.pose.position.x;
  odom_pos.y = odom_msg.pose.pose.position.y;
  odom_pos.theta = tf::getYaw(odom_msg.pose.pose.orientation);
}

void sigintHandler(int sig) {
  // Log quit
  ROS_INFO("Exiting program gracefully ...");

  // MESSAGE A PUBLIER le MESSAGE
  // Kill all open subscriptions, publications, service calls, and service
  // servers
  shutdown();
}

int main(int argc, char **argv) {
  signal(SIGINT, sigintHandler);
  ros::init(argc, argv, "path_generator", ros::init_options::NoSigintHandler);
  NodeHandle nh;

  // nh.getParam("ecart_obstacle", ecart_obstacle);
  ROS_INFO("\033[1;32m----> Path Generator Node is Started.\033[0m");
  Subscriber odom = nh.subscribe("/odom", 1000, odomCallback);

  PathGenerator PG;

  ros::spin();
  return 0;
}
