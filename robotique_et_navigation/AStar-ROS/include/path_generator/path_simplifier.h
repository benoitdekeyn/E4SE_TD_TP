#ifndef PATH_SIMPLIFIER_H
#define PATH_SIMPLIFIER_H

#include "a_star.hpp"

// The path must be ordered from source to target.
AStar::CoordinateList simplifyPath(const AStar::CoordinateList &path,
                                   AStar::Generator &generator);

#endif
