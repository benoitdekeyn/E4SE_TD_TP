#include "../include/path_generator/path_simplifier.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {

bool hasLineOfSight(AStar::Vec2i start, AStar::Vec2i end,
                    AStar::Generator &generator) {
  int dx = end.x - start.x;
  int dy = end.y - start.y;
  int nx = std::abs(dx);
  int ny = std::abs(dy);
  int step_x = (dx > 0) - (dx < 0);
  int step_y = (dy > 0) - (dy < 0);
  int ix = 0;
  int iy = 0;
  AStar::Vec2i current = start;

  if (generator.isCollision_in_OCG(current))
    return false;

  while (ix < nx || iy < ny) {
    long long cross_x = (1LL + 2LL * ix) * ny;
    long long cross_y = (1LL + 2LL * iy) * nx;
    if (cross_x == cross_y) {
      // A segment crossing a cell corner touches both adjacent cells.
      AStar::Vec2i side_x = {current.x + step_x, current.y};
      AStar::Vec2i side_y = {current.x, current.y + step_y};
      if (generator.isCollision_in_OCG(side_x) ||
          generator.isCollision_in_OCG(side_y))
        return false;
      current.x += step_x;
      current.y += step_y;
      ++ix;
      ++iy;
    } else if (cross_x < cross_y) {
      current.x += step_x;
      ++ix;
    } else {
      current.y += step_y;
      ++iy;
    }
    if (generator.isCollision_in_OCG(current))
      return false;
  }
  return true;
}

} // namespace

AStar::CoordinateList simplifyPath(const AStar::CoordinateList &path,
                                   AStar::Generator &generator) {
  if (path.empty())
    return {};

  // Each edge connects two visible path points; breadth-first search finds
  // the fewest segments while keeping all waypoints on the original path.
  const size_t count = path.size();
  std::vector<size_t> parent(count, count);
  std::vector<size_t> queue;
  queue.push_back(0);
  parent[0] = 0;

  if (generator.isCollision_in_OCG(path[0]))
    return {};

  for (size_t head = 0; head < queue.size() && parent[count - 1] == count;
       ++head) {
    size_t from = queue[head];
    for (size_t to = count - 1; to > from; --to) {
      if (parent[to] == count &&
          hasLineOfSight(path[from], path[to], generator)) {
        parent[to] = from;
        queue.push_back(to);
        if (to == count - 1)
          break;
      }
    }
  }

  if (parent[count - 1] == count)
    return {};

  AStar::CoordinateList result;
  for (size_t index = count - 1;; index = parent[index]) {
    result.push_back(path[index]);
    if (index == 0)
      break;
  }
  std::reverse(result.begin(), result.end());
  return result;
}
