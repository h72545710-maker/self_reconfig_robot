#include "robot/grid_map.h"
#include "robot/protocol.h"
#include "robot/sensor_fusion.h"
#include "robot/types.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {

using robot::ModuleStatus;
using robot::Point;
using robot::RobotState;
using robot::Role;

struct Config {
    int ticks = 60;
    int sleep_ms = 120;
    std::string state_file;
};

struct MissionGoal {
    std::uint32_t id = 1;
    std::string type;
    Point target{};
};

struct FormationAssignment {
    std::uint32_t module_id = 0;
    Point target{};
};

struct SubTask {
    std::uint32_t module_id = 0;
    Point target{};
    bool done = false;
};

struct TimelineEntry {
    int tick = 0;
    std::string mode;
    std::string label;
};

struct ControlSummary {
    std::uint32_t leader_switches = 0;
    std::uint32_t formation_switches = 0;
    std::uint32_t health_handoffs = 0;
    int fault_recovery_ticks = -1;
};

Config parse_args(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--ticks" && i + 1 < argc) {
            config.ticks = std::stoi(argv[++i]);
        } else if (arg == "--sleep-ms" && i + 1 < argc) {
            config.sleep_ms = std::stoi(argv[++i]);
        } else if (arg == "--state-file" && i + 1 < argc) {
            config.state_file = argv[++i];
        } else if (arg.rfind("--state-file=", 0) == 0) {
            config.state_file = arg.substr(std::string("--state-file=").size());
        }
    }
    return config;
}

int distance_score(Point a, Point b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

int leader_score(const RobotState& robot, Point goal) {
    return robot.battery * 4 - distance_score(robot.pos, goal) * 3;
}

void assign_roles(std::vector<RobotState>& robots, Point goal) {
    auto leader = std::max_element(
        robots.begin(), robots.end(), [&](const RobotState& lhs, const RobotState& rhs) {
            if (lhs.status != ModuleStatus::Online) {
                return true;
            }
            if (rhs.status != ModuleStatus::Online) {
                return false;
            }
            return leader_score(lhs, goal) < leader_score(rhs, goal);
        });

    for (auto& robot : robots) {
        robot.role = Role::Idle;
    }

    if (leader == robots.end() || leader->status != ModuleStatus::Online) {
        return;
    }

    leader->role = Role::Leader;
    for (auto& robot : robots) {
        if (robot.id != leader->id && robot.status == ModuleStatus::Online) {
            robot.role = Role::Follower;
        }
    }
}

RobotState* find_leader(std::vector<RobotState>& robots) {
    for (auto& robot : robots) {
        if (robot.role == Role::Leader && robot.status == ModuleStatus::Online) {
            return &robot;
        }
    }
    return nullptr;
}

std::vector<Point> online_occupied_cells(const std::vector<RobotState>& robots, std::uint32_t except_id = 0) {
    std::vector<Point> cells;
    for (const auto& robot : robots) {
        if (robot.id != except_id && robot.status != ModuleStatus::Offline) {
            cells.push_back(robot.pos);
        }
    }
    return cells;
}

bool contains_point(const std::vector<Point>& points, Point point) {
    return std::find(points.begin(), points.end(), point) != points.end();
}

int update_sensor_coverage(const robot::GridMap& map,
                           const std::vector<RobotState>& robots,
                           std::vector<Point>& covered_cells) {
    constexpr int kSensorRadius = 2;
    int newly_covered = 0;

    for (const auto& robot : robots) {
        if (robot.status != ModuleStatus::Online) {
            continue;
        }

        for (int dy = -kSensorRadius; dy <= kSensorRadius; ++dy) {
            for (int dx = -kSensorRadius; dx <= kSensorRadius; ++dx) {
                const Point cell{robot.pos.x + dx, robot.pos.y + dy};
                if (std::abs(dx) + std::abs(dy) > kSensorRadius || !map.passable(cell)) {
                    continue;
                }
                if (!contains_point(covered_cells, cell)) {
                    covered_cells.push_back(cell);
                    ++newly_covered;
                }
            }
        }
    }

    return newly_covered;
}

std::vector<Point> passable_covered_cells(const robot::GridMap& map,
                                          const std::vector<Point>& covered_cells) {
    std::vector<Point> result;
    for (const auto& cell : covered_cells) {
        if (map.passable(cell)) {
            result.push_back(cell);
        }
    }
    return result;
}

int coverage_percent(const robot::GridMap& map, const std::vector<Point>& covered_cells) {
    const int total_passable = map.width() * map.height() - static_cast<int>(map.obstacles().size());
    if (total_passable <= 0) {
        return 0;
    }
    const auto covered = passable_covered_cells(map, covered_cells);
    return static_cast<int>((covered.size() * 100) / static_cast<std::size_t>(total_passable));
}

Point nearest_passable_target(const robot::GridMap& map,
                              Point desired,
                              const std::vector<Point>& occupied) {
    std::vector<Point> candidates{desired};
    for (int radius = 1; radius <= 3; ++radius) {
        for (int dx = -radius; dx <= radius; ++dx) {
            candidates.push_back({desired.x + dx, desired.y - radius});
            candidates.push_back({desired.x + dx, desired.y + radius});
        }
        for (int dy = -radius + 1; dy <= radius - 1; ++dy) {
            candidates.push_back({desired.x - radius, desired.y + dy});
            candidates.push_back({desired.x + radius, desired.y + dy});
        }
    }

    Point best = desired;
    int best_score = std::numeric_limits<int>::max();
    for (const auto& candidate : candidates) {
        if (!map.passable(candidate)) {
            continue;
        }
        if (std::find(occupied.begin(), occupied.end(), candidate) != occupied.end()) {
            continue;
        }
        const int score = distance_score(candidate, desired);
        if (score < best_score) {
            best = candidate;
            best_score = score;
        }
    }
    return best;
}

Point formation_target(const robot::GridMap& map,
                       const RobotState& leader,
                       Point heading,
                       const std::string& mode,
                       int follower_index,
                       const std::vector<Point>& occupied) {
    if (heading.x == 0 && heading.y == 0) {
        heading = {1, 0};
    }

    const Point back{-heading.x, -heading.y};
    const Point lateral{-heading.y, heading.x};

    std::vector<Point> offsets;
    if (mode == "column") {
        offsets = {
            {back.x, back.y},
            {back.x * 2, back.y * 2},
            {back.x * 3, back.y * 3},
            {back.x * 4, back.y * 4},
        };
    } else if (mode == "line") {
        offsets = {
            {lateral.x, lateral.y},
            {-lateral.x, -lateral.y},
            {back.x, back.y},
            {back.x + lateral.x, back.y + lateral.y},
        };
    } else {
        offsets = {
            {back.x, back.y},
            {back.x + lateral.x, back.y + lateral.y},
            {back.x - lateral.x, back.y - lateral.y},
            {back.x * 2, back.y * 2},
            {back.x * 2 + lateral.x, back.y * 2 + lateral.y},
            {back.x * 2 - lateral.x, back.y * 2 - lateral.y},
        };
    }

    const Point offset = offsets[static_cast<std::size_t>(follower_index % static_cast<int>(offsets.size()))];
    const Point desired{leader.pos.x + offset.x, leader.pos.y + offset.y};
    return nearest_passable_target(map, desired, occupied);
}

std::vector<FormationAssignment> move_followers(robot::GridMap& map,
                                                std::vector<RobotState>& robots,
                                                const RobotState& leader,
                                                Point heading,
                                                const std::string& formation_mode) {
    std::vector<FormationAssignment> assignments;
    int follower_index = 0;
    std::vector<Point> reserved = online_occupied_cells(robots, leader.id);
    reserved.push_back(leader.pos);

    for (auto& robot : robots) {
        if (robot.role != Role::Follower || robot.status != ModuleStatus::Online) {
            continue;
        }

        std::vector<Point> occupied = reserved;
        occupied.erase(std::remove(occupied.begin(), occupied.end(), robot.pos), occupied.end());
        const Point target = formation_target(map, leader, heading, formation_mode, follower_index, occupied);
        assignments.push_back({robot.id, target});
        const auto path = map.plan_path(robot.pos, target, occupied);
        if (path.size() > 1) {
            robot.pos = path[1];
        }
        robot.battery = std::max(0, robot.battery - 1);

        reserved.push_back(robot.pos);
        ++follower_index;
    }

    return assignments;
}

std::string choose_formation_mode(const robot::GridMap& map,
                                  const RobotState& leader,
                                  Point goal,
                                  const std::vector<Point>& path) {
    if (distance_score(leader.pos, goal) <= 3) {
        return "line";
    }

    const std::vector<Point> around{
        {leader.pos.x + 1, leader.pos.y},
        {leader.pos.x - 1, leader.pos.y},
        {leader.pos.x, leader.pos.y + 1},
        {leader.pos.x, leader.pos.y - 1},
    };
    const int blocked_neighbors = static_cast<int>(std::count_if(around.begin(), around.end(), [&](Point p) {
        return !map.passable(p);
    }));

    if (blocked_neighbors >= 2 || (!path.empty() && path.size() > 12)) {
        return "column";
    }

    return "wedge";
}

void print_status(const std::vector<RobotState>& robots) {
    for (const auto& robot : robots) {
        std::cout << "module=" << robot.id
                  << " role=" << robot::to_string(robot.role)
                  << " status=" << robot::to_string(robot.status)
                  << " pos=(" << robot.pos.x << "," << robot.pos.y << ")"
                  << " battery=" << robot.battery << '\n';
    }
}

void write_points(std::ofstream& out, const std::vector<Point>& points) {
    out << "[";
    for (std::size_t i = 0; i < points.size(); ++i) {
        out << "{\"x\":" << points[i].x << ",\"y\":" << points[i].y << "}";
        if (i + 1 < points.size()) {
            out << ",";
        }
    }
    out << "]";
}

void write_subtasks(std::ofstream& out, const std::vector<SubTask>& subtasks) {
    out << "[";
    for (std::size_t i = 0; i < subtasks.size(); ++i) {
        out << "{\"moduleId\":" << subtasks[i].module_id
            << ",\"x\":" << subtasks[i].target.x
            << ",\"y\":" << subtasks[i].target.y
            << ",\"done\":" << (subtasks[i].done ? "true" : "false") << "}";
        if (i + 1 < subtasks.size()) {
            out << ",";
        }
    }
    out << "]";
}

void write_timeline(std::ofstream& out, const std::vector<TimelineEntry>& timeline) {
    out << "[";
    for (std::size_t i = 0; i < timeline.size(); ++i) {
        out << "{\"tick\":" << timeline[i].tick
            << ",\"mode\":\"" << timeline[i].mode
            << "\",\"label\":\"" << timeline[i].label << "\"}";
        if (i + 1 < timeline.size()) {
            out << ",";
        }
    }
    out << "]";
}

void write_sensor_fusion(std::ofstream& out, const std::vector<robot::FusedPose>& fused_poses) {
    out << "[";
    for (std::size_t i = 0; i < fused_poses.size(); ++i) {
        const auto& pose = fused_poses[i];
        out << "{\"moduleId\":" << pose.module_id
            << ",\"x\":" << pose.x
            << ",\"y\":" << pose.y
            << ",\"gridX\":" << pose.grid_x
            << ",\"gridY\":" << pose.grid_y
            << ",\"yawRad\":" << pose.yaw_rad
            << ",\"confidence\":" << pose.confidence
            << ",\"errorCm\":" << pose.error_cm
            << ",\"frontBlocked\":" << (pose.front_blocked ? "true" : "false") << "}";
        if (i + 1 < fused_poses.size()) {
            out << ",";
        }
    }
    out << "]";
}

int average_fusion_confidence_percent(const std::vector<robot::FusedPose>& fused_poses) {
    if (fused_poses.empty()) {
        return 0;
    }
    double sum = 0.0;
    for (const auto& pose : fused_poses) {
        sum += pose.confidence;
    }
    return static_cast<int>((sum * 100.0) / static_cast<double>(fused_poses.size()));
}

int max_fusion_error_cm(const std::vector<robot::FusedPose>& fused_poses) {
    double max_error = 0.0;
    for (const auto& pose : fused_poses) {
        max_error = std::max(max_error, pose.error_cm);
    }
    return static_cast<int>(max_error);
}

void write_goals(std::ofstream& out, const std::vector<MissionGoal>& goals) {
    out << "[";
    for (std::size_t i = 0; i < goals.size(); ++i) {
        out << "{\"id\":" << goals[i].id
            << ",\"type\":\"" << goals[i].type
            << "\",\"target\":{\"x\":" << goals[i].target.x
            << ",\"y\":" << goals[i].target.y << "}}";
        if (i + 1 < goals.size()) {
            out << ",";
        }
    }
    out << "]";
}

void write_control_state_json(const std::string& path,
                              const robot::GridMap& map,
                              const std::vector<RobotState>& robots,
                              std::uint32_t leader_id,
                              const std::vector<MissionGoal>& goals,
                              std::size_t current_goal_index,
                              int tick,
                              std::uint32_t replan_count,
                              const ControlSummary& summary,
                              const std::string& formation_mode,
                              const std::string& work_mode,
                              const std::vector<SubTask>& subtasks,
                              const std::vector<TimelineEntry>& timeline,
                              const std::vector<Point>& raw_path,
                              const std::vector<Point>& optimized_path,
                              const std::vector<Point>& leader_trail,
                              const std::vector<Point>& covered_cells,
                              const std::vector<robot::FusedPose>& fused_poses,
                              const std::vector<FormationAssignment>& formation,
                              const std::vector<std::string>& events) {
    if (path.empty()) {
        return;
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        std::cerr << "failed to open state file for writing: " << path << '\n';
        return;
    }

    const auto obstacles = map.obstacles();
    const auto covered_passable = passable_covered_cells(map, covered_cells);
    const int covered_percent = coverage_percent(map, covered_cells);
    const bool mission_done = current_goal_index >= goals.size();
    const MissionGoal active_goal = mission_done ? goals.back() : goals[current_goal_index];
    const std::size_t completed_goals = mission_done ? goals.size() : current_goal_index;
    const std::size_t completed_subtasks = static_cast<std::size_t>(
        std::count_if(subtasks.begin(), subtasks.end(), [](const SubTask& task) {
            return task.done;
        }));
    const std::size_t saved = raw_path.size() >= optimized_path.size()
                                  ? raw_path.size() - optimized_path.size()
                                  : 0;
    const int front_blocked_count = static_cast<int>(
        std::count_if(fused_poses.begin(), fused_poses.end(), [](const robot::FusedPose& pose) {
            return pose.front_blocked;
        }));

    out << "{\n";
    out << "  \"mode\": \"control-sim\",\n";
    out << "  \"tick\": " << tick << ",\n";
    out << "  \"leader\": " << leader_id << ",\n";
    out << "  \"map\": {\"width\": " << map.width()
        << ", \"height\": " << map.height()
        << ", \"target\": {\"x\": " << active_goal.target.x << ", \"y\": " << active_goal.target.y
        << "}, \"obstacles\": ";
    write_points(out, obstacles);
    out << "},\n";
    out << "  \"mission\": {\"currentGoal\": " << (mission_done ? goals.size() : current_goal_index + 1)
        << ", \"completedGoals\": " << completed_goals
        << ", \"totalGoals\": " << goals.size()
        << ", \"workMode\": \"" << work_mode
        << "\", \"completedSubtasks\": " << completed_subtasks
        << ", \"totalSubtasks\": " << subtasks.size()
        << ", \"goals\": ";
    write_goals(out, goals);
    out << "},\n";
    out << "  \"task\": {\"id\": " << active_goal.id
        << ", \"type\": \"" << active_goal.type
        << "\", \"assigned\": " << leader_id
        << ", \"state\": \"" << (mission_done ? "done" : "running")
        << "\", \"progress\": " << (mission_done ? 100 : static_cast<int>((completed_goals * 100) / goals.size()))
        << ", \"target\": {\"x\": " << active_goal.target.x << ", \"y\": " << active_goal.target.y << "}},\n";
    out << "  \"metrics\": {\"planner\": \"A* + line_of_sight_smoothing\""
        << ", \"rawPath\": " << raw_path.size()
        << ", \"optimizedWaypoints\": " << optimized_path.size()
        << ", \"savedWaypoints\": " << saved
        << ", \"replanCount\": " << replan_count
        << ", \"formationMode\": \"" << formation_mode
        << "\", \"leaderSwitches\": " << summary.leader_switches
        << ", \"formationSwitches\": " << summary.formation_switches
        << ", \"healthHandoffs\": " << summary.health_handoffs
        << ", \"faultRecoveryTicks\": " << summary.fault_recovery_ticks
        << ", \"trailLength\": " << (leader_trail.empty() ? 0 : leader_trail.size() - 1)
        << ", \"obstacleCount\": " << obstacles.size()
        << ", \"coveredCells\": " << covered_passable.size()
        << ", \"coveragePercent\": " << covered_percent
        << ", \"fusionConfidence\": " << average_fusion_confidence_percent(fused_poses)
        << ", \"fusionMaxErrorCm\": " << max_fusion_error_cm(fused_poses)
        << ", \"frontBlockedCount\": " << front_blocked_count << "},\n";
    out << "  \"subtasks\": ";
    write_subtasks(out, subtasks);
    out << ",\n";
    out << "  \"timeline\": ";
    write_timeline(out, timeline);
    out << ",\n";
    out << "  \"coveredCells\": ";
    write_points(out, covered_passable);
    out << ",\n";
    out << "  \"leaderTrail\": ";
    write_points(out, leader_trail);
    out << ",\n";
    out << "  \"path\": ";
    write_points(out, optimized_path);
    out << ",\n";
    out << "  \"formationTargets\": [\n";
    for (std::size_t i = 0; i < formation.size(); ++i) {
        out << "    {\"moduleId\": " << formation[i].module_id
            << ", \"x\": " << formation[i].target.x
            << ", \"y\": " << formation[i].target.y << "}";
        if (i + 1 < formation.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"sensorFusion\": ";
    write_sensor_fusion(out, fused_poses);
    out << ",\n";
    out << "  \"modules\": [\n";
    for (std::size_t i = 0; i < robots.size(); ++i) {
        const auto& robot = robots[i];
        out << "    {\"id\": " << robot.id
            << ", \"role\": \"" << robot::to_string(robot.role)
            << "\", \"status\": \"" << robot::to_string(robot.status)
            << "\", \"x\": " << robot.pos.x
            << ", \"y\": " << robot.pos.y
            << ", \"battery\": " << robot.battery << "}";
        if (i + 1 < robots.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"events\": [\n";
    for (std::size_t i = 0; i < events.size(); ++i) {
        out << "    \"" << events[i] << "\"";
        if (i + 1 < events.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    const Config config = parse_args(argc, argv);

    robot::GridMap map(18, 10);
    for (int y = 1; y < 8; ++y) {
        map.add_obstacle({7, y});
    }
    for (int x = 10; x < 15; ++x) {
        map.add_obstacle({x, 5});
    }

    std::vector<RobotState> robots{
        {1, {1, 1}, 95, Role::Idle, ModuleStatus::Online, 0},
        {2, {1, 2}, 88, Role::Idle, ModuleStatus::Online, 0},
        {3, {1, 3}, 76, Role::Idle, ModuleStatus::Online, 0},
        {4, {1, 4}, 82, Role::Idle, ModuleStatus::Online, 0},
    };
    std::vector<robot::SensorFusionFilter> fusion_filters;
    fusion_filters.reserve(robots.size());
    for (const auto& robot : robots) {
        fusion_filters.emplace_back(robot.pos);
    }

    const std::vector<MissionGoal> goals{
        {1, "RENDEZVOUS", {6, 8}},
        {2, "EXPLORE", {16, 8}},
        {3, "RETURN", {16, 2}},
    };
    std::size_t current_goal_index = 0;
    assign_roles(robots, goals[current_goal_index].target);
    std::vector<std::string> events{
        "[control] A* path planning enabled",
        "[control] waypoint optimization enabled",
        "[control] formation target tracking enabled",
        "[mission] 3-stage mission: RENDEZVOUS -> EXPLORE -> RETURN",
    };
    std::vector<Point> leader_trail;
    std::uint32_t replan_count = 0;
    std::string formation_mode = "wedge";
    std::string last_formation_mode;
    std::uint32_t last_leader_id = 0;
    int fault_tick = -1;
    ControlSummary summary;
    bool dynamic_obstacle_added = false;
    bool low_power_injected = false;
    std::vector<Point> covered_cells;
    int last_coverage_milestone = 0;
    std::vector<SubTask> subtasks{
        {2, {10, 8}, false},
        {3, {16, 8}, false},
        {4, {16, 6}, false},
    };
    std::string work_mode = "coupled";
    std::vector<TimelineEntry> timeline{
        {0, "coupled", "rendezvous"},
    };
    bool split_complete_reported = false;

    for (int tick = 0; tick < config.ticks; ++tick) {
        std::vector<Point> previous_positions;
        previous_positions.reserve(robots.size());
        for (const auto& robot : robots) {
            previous_positions.push_back(robot.pos);
        }

        if (tick == 8) {
            robots[0].status = ModuleStatus::Fault;
            robots[0].battery = 0;
            std::cout << "[fault-injection] module 1 reports motor fault\n";
            events.push_back("[fault-injection] module 1 reports motor fault");
            timeline.push_back({tick, "recovery", "module fault"});
            fault_tick = tick;
            assign_roles(robots, goals[current_goal_index].target);
        }

        if (tick == 18 && !dynamic_obstacle_added) {
            map.add_obstacle({11, 8});
            map.add_obstacle({12, 8});
            map.add_obstacle({13, 8});
            events.push_back("[dynamic-obstacle] new blocked cells at y=8, trigger detour");
            timeline.push_back({tick, "replan", "dynamic obstacle"});
            dynamic_obstacle_added = true;
        }

        if (tick == 24 && !low_power_injected) {
            for (auto& robot : robots) {
                if (robot.role == Role::Leader && robot.status == ModuleStatus::Online) {
                    robot.battery = std::min(robot.battery, 18);
                    events.push_back("[health] active leader battery low, trigger energy-aware handoff");
                    timeline.push_back({tick, "handoff", "low battery"});
                    ++summary.health_handoffs;
                    break;
                }
            }
            assign_roles(robots, goals[current_goal_index].target);
            low_power_injected = true;
        }

        RobotState* leader = find_leader(robots);
        if (leader == nullptr) {
            std::cout << "no online leader, mission paused\n";
            break;
        }

        if (current_goal_index >= goals.size()) {
            break;
        }

        Point goal = goals[current_goal_index].target;
        work_mode = (current_goal_index == 1) ? "split" : "coupled";
        const std::uint32_t leader_id = leader->id;
        if (last_leader_id != 0 && leader_id != last_leader_id) {
            ++summary.leader_switches;
            events.push_back("[recovery] leader switched " + std::to_string(last_leader_id) +
                             " -> " + std::to_string(leader_id));
        }
        if (fault_tick >= 0 && summary.fault_recovery_ticks < 0 && leader_id != 1) {
            summary.fault_recovery_ticks = std::max(1, tick - fault_tick);
            events.push_back("[recovery] fault recovery completed in " +
                             std::to_string(summary.fault_recovery_ticks) + " tick(s)");
        }
        last_leader_id = leader_id;

        const auto path = map.plan_path(leader->pos, goal, online_occupied_cells(robots, leader->id));
        if (!path.empty()) {
            ++replan_count;
        }
        formation_mode = choose_formation_mode(map, *leader, goal, path);
        if (!last_formation_mode.empty() && formation_mode != last_formation_mode) {
            ++summary.formation_switches;
            events.push_back("[formation] mode switch " + last_formation_mode +
                             " -> " + formation_mode);
        }
        last_formation_mode = formation_mode;

        Point heading{0, 0};
        if (work_mode == "coupled" && path.size() > 1) {
            heading = {path[1].x - leader->pos.x, path[1].y - leader->pos.y};
            leader->pos = path[1];
            leader->battery = std::max(0, leader->battery - 1);
        }
        if (leader_trail.empty() || leader_trail.back() != leader->pos) {
            leader_trail.push_back(leader->pos);
        }

        bool mission_complete = false;
        if (work_mode == "coupled" && leader->pos == goal) {
            events.push_back("[mission] goal " + std::to_string(goals[current_goal_index].id) +
                             " " + goals[current_goal_index].type + " reached");
            ++current_goal_index;
            if (current_goal_index >= goals.size()) {
                mission_complete = true;
            } else {
                goal = goals[current_goal_index].target;
                events.push_back("[mission] switch to goal " + std::to_string(goals[current_goal_index].id) +
                                 " " + goals[current_goal_index].type);
                timeline.push_back({tick, "split", "parallel explore"});
            }
        }

        std::vector<FormationAssignment> formation;
        if (work_mode == "split") {
            for (auto& task : subtasks) {
                auto robot_it = std::find_if(robots.begin(), robots.end(), [&](const RobotState& robot) {
                    return robot.id == task.module_id && robot.status == ModuleStatus::Online;
                });
                if (robot_it == robots.end()) {
                    continue;
                }

                formation.push_back({robot_it->id, task.target});
                const auto occupied = online_occupied_cells(robots, robot_it->id);
                const auto sub_path = map.plan_path(robot_it->pos, task.target, occupied);
                if (sub_path.size() > 1) {
                    robot_it->pos = sub_path[1];
                    robot_it->battery = std::max(0, robot_it->battery - 1);
                }
                if (robot_it->pos == task.target && !task.done) {
                    task.done = true;
                    events.push_back("[split-task] module " + std::to_string(task.module_id) +
                                     " reached local target");
                }
            }
        } else {
            formation = move_followers(map, robots, *leader, heading, formation_mode);
        }

        if (current_goal_index == 1 && !split_complete_reported &&
            std::all_of(subtasks.begin(), subtasks.end(), [](const SubTask& task) {
                return task.done;
            })) {
            events.push_back("[split-task] all local exploration subtasks complete");
            split_complete_reported = true;
            events.push_back("[mission] goal " + std::to_string(goals[current_goal_index].id) +
                             " " + goals[current_goal_index].type + " reached");
            ++current_goal_index;
            goal = goals[current_goal_index].target;
            work_mode = "coupled";
            events.push_back("[mission] switch to goal " + std::to_string(goals[current_goal_index].id) +
                             " " + goals[current_goal_index].type);
            timeline.push_back({tick, "coupled", "return"});
        }

        const int newly_covered = update_sensor_coverage(map, robots, covered_cells);
        (void)newly_covered;
        const int current_coverage = coverage_percent(map, covered_cells);
        while (last_coverage_milestone + 25 <= current_coverage && last_coverage_milestone < 75) {
            last_coverage_milestone += 25;
            events.push_back("[mapping] coverage reached " + std::to_string(last_coverage_milestone) + "%");
        }

        std::vector<robot::FusedPose> fused_poses;
        fused_poses.reserve(robots.size());
        for (std::size_t i = 0; i < robots.size(); ++i) {
            if (robots[i].status == ModuleStatus::Offline) {
                continue;
            }
            const auto sample = robot::simulate_sensor_sample(robots[i].id,
                                                              tick,
                                                              previous_positions[i],
                                                              robots[i].pos,
                                                              map);
            fused_poses.push_back(fusion_filters[i].update(robots[i].id, robots[i].pos, sample));
        }

        const auto latest_path = map.plan_path(leader->pos, goal, online_occupied_cells(robots, leader->id));
        const auto latest_optimized_path = map.optimize_path(latest_path);
        std::cout << "\n[tick " << tick << "] leader=" << leader->id << '\n';
        std::cout << "goal=" << (current_goal_index >= goals.size() ? goals.size() : current_goal_index + 1)
                  << "/" << goals.size()
                  << " formation=" << formation_mode << '\n';
        std::cout << "raw_path=" << latest_path.size()
                  << " optimized_waypoints=" << latest_optimized_path.size()
                  << " saved_waypoints=" << (latest_path.size() >= latest_optimized_path.size()
                                                ? latest_path.size() - latest_optimized_path.size()
                                                : 0)
                  << '\n';
        std::cout << map.render(robots, latest_optimized_path, goal);
        print_status(robots);
        write_control_state_json(config.state_file,
                                 map,
                                 robots,
                                 leader_id,
                                 goals,
                                 current_goal_index,
                                 tick,
                                 replan_count,
                                 summary,
                                 formation_mode,
                                 work_mode,
                                 subtasks,
                                 timeline,
                                 latest_path,
                                 latest_optimized_path,
                                 leader_trail,
                                 covered_cells,
                                 fused_poses,
                                 formation,
                                 events);

        if (mission_complete) {
            std::cout << "mission complete\n";
            events.push_back("[mission] complete");
            timeline.push_back({tick, "done", "mission complete"});
            write_control_state_json(config.state_file,
                                     map,
                                     robots,
                                     leader_id,
                                     goals,
                                     current_goal_index,
                                     tick,
                                     replan_count,
                                     summary,
                                     formation_mode,
                                     work_mode,
                                     subtasks,
                                     timeline,
                                     latest_path,
                                     latest_optimized_path,
                                     leader_trail,
                                     covered_cells,
                                     fused_poses,
                                     formation,
                                     events);
            break;
        }

        if (config.sleep_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config.sleep_ms));
        }
    }

    return 0;
}
