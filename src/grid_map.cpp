#include "robot/grid_map.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <sstream>
#include <unordered_map>

namespace robot {

namespace {

int manhattan(Point a, Point b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

struct QueueItem {
    Point point{};
    int priority = 0;

    bool operator>(const QueueItem& other) const {
        return priority > other.priority;
    }
};

} // namespace

GridMap::GridMap(int width, int height) : width_(width), height_(height) {}

int GridMap::width() const {
    return width_;
}

int GridMap::height() const {
    return height_;
}

bool GridMap::in_bounds(Point p) const {
    return p.x >= 0 && p.y >= 0 && p.x < width_ && p.y < height_;
}

bool GridMap::is_obstacle(Point p) const {
    return obstacles_.find(p) != obstacles_.end();
}

bool GridMap::passable(Point p) const {
    return in_bounds(p) && !is_obstacle(p);
}

void GridMap::add_obstacle(Point p) {
    if (in_bounds(p)) {
        obstacles_.insert(p);
    }
}

std::vector<Point> GridMap::obstacles() const {
    std::vector<Point> result(obstacles_.begin(), obstacles_.end());
    std::sort(result.begin(), result.end(), [](Point lhs, Point rhs) {
        if (lhs.y != rhs.y) {
            return lhs.y < rhs.y;
        }
        return lhs.x < rhs.x;
    });
    return result;
}

std::vector<Point> GridMap::neighbors(Point p) const {
    const std::vector<Point> candidates{
        {p.x + 1, p.y},
        {p.x - 1, p.y},
        {p.x, p.y + 1},
        {p.x, p.y - 1},
    };

    std::vector<Point> result;
    for (const auto& item : candidates) {
        if (passable(item)) {
            result.push_back(item);
        }
    }
    return result;
}

std::vector<Point> GridMap::plan_path(Point start, Point goal) const {
    return plan_path(start, goal, {});
}

std::vector<Point> GridMap::plan_path(Point start,
                                      Point goal,
                                      const std::vector<Point>& blocked_cells) const {
    if (!passable(start) || !passable(goal)) {
        return {};
    }

    std::unordered_set<Point, PointHash> blocked;
    for (const auto& cell : blocked_cells) {
        if (cell != start && cell != goal) {
            blocked.insert(cell);
        }
    }

    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> frontier;
    std::unordered_map<Point, Point, PointHash> came_from;
    std::unordered_map<Point, int, PointHash> cost_so_far;

    frontier.push({start, 0});
    came_from[start] = start;
    cost_so_far[start] = 0;

    while (!frontier.empty()) {
        const Point current = frontier.top().point;
        frontier.pop();

        if (current == goal) {
            break;
        }

        for (const auto& next : neighbors(current)) {
            if (blocked.find(next) != blocked.end()) {
                continue;
            }
            const int new_cost = cost_so_far[current] + 1;
            const auto known = cost_so_far.find(next);
            if (known == cost_so_far.end() || new_cost < known->second) {
                cost_so_far[next] = new_cost;
                frontier.push({next, new_cost + manhattan(next, goal)});
                came_from[next] = current;
            }
        }
    }

    if (came_from.find(goal) == came_from.end()) {
        return {};
    }

    std::vector<Point> path;
    Point current = goal;
    while (current != start) {
        path.push_back(current);
        current = came_from[current];
    }
    path.push_back(start);
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<Point> GridMap::optimize_path(const std::vector<Point>& path) const {
    if (path.size() <= 2) {
        return path;
    }

    std::vector<Point> smoothed;
    smoothed.push_back(path.front());

    std::size_t anchor = 0;
    while (anchor + 1 < path.size()) {
        std::size_t farthest = anchor + 1;
        for (std::size_t probe = path.size() - 1; probe > anchor + 1; --probe) {
            if (line_of_sight(path[anchor], path[probe])) {
                farthest = probe;
                break;
            }
        }
        smoothed.push_back(path[farthest]);
        anchor = farthest;
    }

    return smoothed;
}

bool GridMap::line_of_sight(Point from, Point to) const {
    int x0 = from.x;
    int y0 = from.y;
    const int x1 = to.x;
    const int y1 = to.y;
    const int dx = std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    while (true) {
        if (!passable({x0, y0})) {
            return false;
        }
        if (x0 == x1 && y0 == y1) {
            return true;
        }
        const int e2 = 2 * error;
        if (e2 >= dy) {
            error += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

std::string GridMap::render(const std::vector<RobotState>& robots,
                            const std::vector<Point>& path,
                            Point goal) const {
    std::vector<std::string> canvas(static_cast<std::size_t>(height_),
                                    std::string(static_cast<std::size_t>(width_), '.'));

    for (const auto& obstacle : obstacles_) {
        canvas[static_cast<std::size_t>(obstacle.y)][static_cast<std::size_t>(obstacle.x)] = '#';
    }

    for (const auto& p : path) {
        if (passable(p)) {
            canvas[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] = '*';
        }
    }

    if (in_bounds(goal)) {
        canvas[static_cast<std::size_t>(goal.y)][static_cast<std::size_t>(goal.x)] = 'G';
    }

    for (const auto& r : robots) {
        if (!in_bounds(r.pos)) {
            continue;
        }
        char marker = static_cast<char>('0' + (r.id % 10));
        if (r.role == Role::Leader) {
            marker = 'L';
        } else if (r.status != ModuleStatus::Online) {
            marker = 'X';
        }
        canvas[static_cast<std::size_t>(r.pos.y)][static_cast<std::size_t>(r.pos.x)] = marker;
    }

    std::ostringstream out;
    for (const auto& row : canvas) {
        out << row << '\n';
    }
    return out.str();
}

} // namespace robot
