#pragma once

#include "robot/types.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace robot {

class GridMap {
public:
    GridMap(int width, int height);

    int width() const;
    int height() const;

    bool in_bounds(Point p) const;
    bool is_obstacle(Point p) const;
    bool passable(Point p) const;

    void add_obstacle(Point p);
    std::vector<Point> obstacles() const;
    std::vector<Point> neighbors(Point p) const;
    std::vector<Point> plan_path(Point start, Point goal) const;
    std::vector<Point> plan_path(Point start,
                                 Point goal,
                                 const std::vector<Point>& blocked_cells) const;
    std::vector<Point> optimize_path(const std::vector<Point>& path) const;
    bool line_of_sight(Point from, Point to) const;

    std::string render(const std::vector<RobotState>& robots,
                       const std::vector<Point>& path,
                       Point goal) const;

private:
    struct PointHash {
        std::size_t operator()(Point p) const {
            return static_cast<std::size_t>(p.x * 73856093) ^
                   static_cast<std::size_t>(p.y * 19349663);
        }
    };

    int width_ = 0;
    int height_ = 0;
    std::unordered_set<Point, PointHash> obstacles_;
};

} // namespace robot
