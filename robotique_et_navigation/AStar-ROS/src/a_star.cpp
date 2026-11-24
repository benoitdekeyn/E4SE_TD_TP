#include "../include/path_generator/a_star.hpp"
#include <algorithm>
#include <cstdint>
#include <geometry_msgs/Pose2D.h>
#include <limits>
#include <nav_msgs/MapMetaData.h>
#include <nav_msgs/OccupancyGrid.h>
#include <queue>

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

// Dilate chaque obstacle de "ecart_obstacle" cases pour tenir le robot à distance des murs et coins (rapport § 7.2).
bool AStar::Generator::isCollision_in_OCG(Vec2i coordinates_) {
  if (coordinates_.x < 0 || coordinates_.x >= worldSize.x ||
      coordinates_.y < 0 || coordinates_.y >= worldSize.y)
    return true;

  for (int dy = -ecart_obstacle; dy <= ecart_obstacle; ++dy) {
    for (int dx = -ecart_obstacle; dx <= ecart_obstacle; ++dx) {
      int x = coordinates_.x + dx;
      int y = coordinates_.y + dy;
      if (x < 0 || x >= worldSize.x || y < 0 || y >= worldSize.y ||
          ocg.data[x + y * worldSize.x] != 0) {
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

// Recherche A* par file de priorité (coût 10 en droit, 14 en diagonal) pour gérer les contournements complexes (rapport § 7.2).
AStar::CoordinateList AStar::Generator::findPath(Vec2i source_, Vec2i target_) {
  CoordinateList path;
  if (worldSize.x <= 0 || worldSize.y <= 0 ||
      ocg.data.size() != static_cast<size_t>(worldSize.x) *
                             static_cast<size_t>(worldSize.y)) {
    ROS_ERROR("A* : carte invalide ou dimensions incompatibles");
    return path;
  }
  if (isCollision_in_OCG(source_)) {
    ROS_ERROR("A* : depart hors carte ou trop proche d'un obstacle");
    return path;
  }
  if (isCollision_in_OCG(target_)) {
    ROS_ERROR("A* : destination hors carte ou trop proche d'un obstacle");
    return path;
  }

  struct OpenEntry {
    size_t index;
    std::uint64_t cost;
    std::uint64_t estimate;
    uint remaining;
  };
  struct CompareEntry {
    bool operator()(const OpenEntry &left, const OpenEntry &right) const {
      if (left.estimate != right.estimate)
        return left.estimate > right.estimate;
      return left.remaining > right.remaining;
    }
  };

  const size_t width = static_cast<size_t>(worldSize.x);
  const size_t count = ocg.data.size();
  const std::uint64_t infinity = std::numeric_limits<std::uint64_t>::max();
  std::vector<std::uint64_t> costs(count, infinity);
  std::vector<size_t> parents(count, count);
  std::priority_queue<OpenEntry, std::vector<OpenEntry>, CompareEntry> open;
  const size_t source = static_cast<size_t>(source_.y) * width + source_.x;
  const size_t target = static_cast<size_t>(target_.y) * width + target_.x;
  uint initial_estimate = heuristic(source_, target_);
  costs[source] = 0;
  open.push({source, 0, initial_estimate, initial_estimate});

  while (!open.empty() && ros::ok()) {
    OpenEntry current = open.top();
    open.pop();
    // Ignore une entrée périmée si un chemin plus court vers cette case a déjà été trouvé.
    if (current.cost != costs[current.index])
      continue;

    if (current.index == target) {
      size_t index = target;
      while (true) {
        path.push_back({static_cast<int>(index % width),
                        static_cast<int>(index / width)});
        if (index == source)
          break;
        index = parents[index];
      }
      return path;
    }

    Vec2i position = {static_cast<int>(current.index % width),
                      static_cast<int>(current.index / width)};
    for (uint i = 0; i < directions; ++i) {
      Vec2i next = position + direction[i];
      if (isCollision_in_OCG(next))
        continue;
      // En déplacement diagonal, empêche de couper à travers le coin d'un obstacle.
      if (i >= 4 &&
          (isCollision_in_OCG({position.x + direction[i].x, position.y}) ||
           isCollision_in_OCG({position.x, position.y + direction[i].y})))
        continue;

      size_t next_index = static_cast<size_t>(next.y) * width + next.x;
      std::uint64_t next_cost = current.cost + ((i < 4) ? 10 : 14);
      if (next_cost < costs[next_index]) {
        costs[next_index] = next_cost;
        parents[next_index] = current.index;
        uint remaining = heuristic(next, target_);
        open.push({next_index, next_cost, next_cost + remaining, remaining});
      }
    }
  }

  if (ros::ok())
    ROS_WARN("A* : aucun chemin accessible avec la marge d'obstacle actuelle");
  else
    ROS_WARN("A* : recherche interrompue");
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
