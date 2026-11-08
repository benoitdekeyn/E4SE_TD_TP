#include "../include/path_generator/a_star.hpp"
#include <algorithm>
#include <geometry_msgs/Pose2D.h>
#include <nav_msgs/MapMetaData.h>
#include <nav_msgs/OccupancyGrid.h>

using namespace std::placeholders;

bool AStar::Vec2i::operator==(const Vec2i &coordinates_) {
  return (x == coordinates_.x && y == coordinates_.y);
}

bool AStar::Vec2i::operator!=(const Vec2i &coordinates_) {
  return !(x == coordinates_.x && y == coordinates_.y);
}

AStar::Vec2i operator+(const AStar::Vec2i &left_, const AStar::Vec2i &right_) {
  return {left_.x + right_.x, left_.y + right_.y};
}

AStar::Node::Node(Vec2i coordinates_, Node *parent_) {
  parent = parent_;
  coordinates = coordinates_;
  G = H = 0;
}

AStar::uint AStar::Node::getScore() { return G + H; }

AStar::Generator::Generator() {
  setDiagonalMovement(false);
  setHeuristic(&Heuristic::manhattan);
  direction = {{0, 1},   {1, 0}, {0, -1}, {-1, 0},
               {-1, -1}, {1, 1}, {-1, 1}, {1, -1}};
}

AStar::Vec2i AStar::Generator::xy_to_ij(float x, float y,
                                        nav_msgs::MapMetaData map_info_) {
  AStar::Vec2i ij;

  ij.x = (x - map_info_.origin.position.x) / map_info_.resolution;
  ij.y = (y - map_info_.origin.position.y) / map_info_.resolution;

  return ij;
}

geometry_msgs::Pose2D
AStar::Generator::ij_to_xy(int i, int j, nav_msgs::MapMetaData map_info_) {
  geometry_msgs::Pose2D pos;

  pos.x = (i + map_info_.origin.position.x / map_info_.resolution) *
          map_info_.resolution;
  pos.y = (j + map_info_.origin.position.y / map_info_.resolution) *
          map_info_.resolution;

  return pos;
}

void AStar::Generator::setWorldSize(Vec2i worldSize_) {
  worldSize = worldSize_;
}

void AStar::Generator::setEcart_obstacle(int ecart_obstacle_) {
  ecart_obstacle = ecart_obstacle_;
}

void AStar::Generator::setDiagonalMovement(bool enable_) {
  directions = (enable_ ? 8 : 4);
}

void AStar::Generator::setHeuristic(HeuristicFunction heuristic_) {
  heuristic =
      std::bind(heuristic_, std::placeholders::_1, std::placeholders::_2);
}

bool AStar::Generator::isCollision_in_OCG(Vec2i coordinates_) {
  if (coordinates_.x < 0 || coordinates_.x >= worldSize.x ||
      coordinates_.y < 0 || coordinates_.y >= worldSize.y)
    return true;

  if (ocg.data[coordinates_.x + coordinates_.y * worldSize.x] != 0)
    return true;
  else {

    for (int i = 0; i < 8; i++) {
      Vec2i new_coord = coordinates_ + direction[i];

      if (new_coord.x < 0 || new_coord.x >= worldSize.x || new_coord.y < 0 ||
          new_coord.y >= worldSize.y) {
        // ROS_INFO("\033[1;31m collision in x=%d y=%d  \033[0m", new_coord.x,
        //  new_coord.y);
        return true;
      }

      if (ocg.data[new_coord.x + new_coord.y * worldSize.x] != 0) {
        // ROS_INFO("\033[1;31m collision in x=%d y=%d  \033[0m", new_coord.x,
        //  new_coord.y);
        return true;
      }
    }
  }

  return false;
}

bool AStar::Generator::isCollision_in_OCG(Vec2i coordinates_, int size) {

  if (size == 0) {
    return isCollision_in_OCG(coordinates_);
  }

  for (int i = 0; i < 4; i++) {
    Vec2i new_coord = coordinates_ + direction[i];
    if (isCollision_in_OCG(new_coord, size - 1))
      return true;
  }

  return false;
}

AStar::CoordinateList AStar::Generator::findPath(Vec2i source_, Vec2i target_) {
  Node *current = nullptr;
  NodeSet openSet, closedSet;
  openSet.reserve(100);
  closedSet.reserve(100);
  openSet.push_back(new Node(source_));

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  while (!openSet.empty()) {
    auto current_it = openSet.begin();
    current = *current_it;

    for (auto it = openSet.begin(); it != openSet.end(); it++) {
      auto node = *it;
      if (node->getScore() <= current->getScore()) {
        current = node;
        current_it = it;
      }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_result =
        end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) * 1e-9;

    if (time_result > 1.0) { // time failure limit
      printf("time: %lf\n", time_result);
      break;
    }

    if (current->coordinates == target_) {
      break;
    }

    closedSet.push_back(current);
    openSet.erase(current_it);

    for (uint i = 0; i < directions; ++i) {
      Vec2i newCoordinates(current->coordinates + direction[i]);
      if (isCollision_in_OCG(newCoordinates) ||
          findNodeOnList(closedSet, newCoordinates)) {
        continue;
      }

      uint totalCost = current->G + ((i < 4) ? 10 : 14);

      Node *successor = findNodeOnList(openSet, newCoordinates);
      if (successor == nullptr) {
        successor = new Node(newCoordinates, current);
        successor->G = totalCost;
        successor->H = heuristic(successor->coordinates, target_);
        openSet.push_back(successor);
      } else if (totalCost < successor->G) {
        successor->parent = current;
        successor->G = totalCost;
      }
    }
  }

  CoordinateList path;

  if (current->coordinates == target_) {
    while (current != nullptr) {
      path.push_back(current->coordinates);
      current = current->parent;
    }
  }

  releaseNodes(openSet);
  releaseNodes(closedSet);

  return path;
}

AStar::Node *AStar::Generator::findNodeOnList(NodeSet &nodes_,
                                              Vec2i coordinates_) {
  for (auto node : nodes_) {
    if (node->coordinates == coordinates_) {
      return node;
    }
  }
  return nullptr;
}

void AStar::Generator::releaseNodes(NodeSet &nodes_) {
  for (auto it = nodes_.begin(); it != nodes_.end();) {
    delete *it;
    it = nodes_.erase(it);
  }
}

AStar::Vec2i AStar::Heuristic::getDelta(Vec2i source_, Vec2i target_) {
  return {abs(source_.x - target_.x), abs(source_.y - target_.y)};
}

AStar::uint AStar::Heuristic::manhattan(Vec2i source_, Vec2i target_) {
  auto delta = std::move(getDelta(source_, target_));
  return static_cast<uint>(10 * (delta.x + delta.y));
}

AStar::uint AStar::Heuristic::euclidean(Vec2i source_, Vec2i target_) {
  auto delta = std::move(getDelta(source_, target_));
  return static_cast<uint>(10 * sqrt(pow(delta.x, 2) + pow(delta.y, 2)));
}

AStar::uint AStar::Heuristic::octagonal(Vec2i source_, Vec2i target_) {
  auto delta = std::move(getDelta(source_, target_));
  return 10 * (delta.x + delta.y) + (-6) * std::min(delta.x, delta.y);
}
