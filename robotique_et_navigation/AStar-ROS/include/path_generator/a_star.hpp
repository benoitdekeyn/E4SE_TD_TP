/*
    Copyright (c) 2015, Damian Barczynski <daan.net@wp.eu>
    Following tool is licensed under the terms and conditions of the ISC
   license. For more information visit https://opensource.org/licenses/ISC.
*/
#ifndef ASTAR_HPP
#define ASTAR_HPP

#include <ros/ros.h>

#include <functional>
#include <geometry_msgs/Pose2D.h>
#include <math.h>
#include <nav_msgs/MapMetaData.h>
#include <nav_msgs/OccupancyGrid.h>
#include <set>
#include <vector>

namespace AStar {
struct Vec2i {
  int x, y;

  bool operator==(const Vec2i &coordinates_);
  bool operator!=(const Vec2i &coordinates_);
};

using uint = unsigned int;
using HeuristicFunction = std::function<uint(Vec2i, Vec2i)>;
using CoordinateList = std::vector<Vec2i>;

struct Node {
  uint G, H;
  Vec2i coordinates;
  Node *parent;

  Node(Vec2i coord_, Node *parent_ = nullptr);
  uint getScore();
};

using NodeSet = std::vector<Node *>;

class Generator {
  bool detectCollision(Vec2i coordinates_);
  Node *findNodeOnList(NodeSet &nodes_, Vec2i coordinates_);
  void releaseNodes(NodeSet &nodes_);

public:
  Generator();
  void setWorldSize(Vec2i worldSize_);
  void setDiagonalMovement(bool enable_);
  void setHeuristic(HeuristicFunction heuristic_);
  CoordinateList findPath(Vec2i source_, Vec2i target_);
  bool isCollision_in_OCG(Vec2i coordinates_, int size);
  bool isCollision_in_OCG(Vec2i coordinates_);
  void setEcart_obstacle(int ecart_obstacle_);
  Vec2i xy_to_ij(float x, float y, nav_msgs::MapMetaData map_info_);
  geometry_msgs::Pose2D ij_to_xy(int i, int j, nav_msgs::MapMetaData map_info_);

  nav_msgs::OccupancyGrid ocg;

private:
  HeuristicFunction heuristic;
  CoordinateList direction, walls;
  Vec2i worldSize;
  uint directions;
  int ecart_obstacle;
};

class Heuristic {
  static Vec2i getDelta(Vec2i source_, Vec2i target_);

public:
  static uint manhattan(Vec2i source_, Vec2i target_);
  static uint euclidean(Vec2i source_, Vec2i target_);
  static uint octagonal(Vec2i source_, Vec2i target_);
};
} // namespace AStar

#endif // ASTAR_HPP
