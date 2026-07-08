#include "robot/control_state_writer.h"

#include "robot/json_utils.h"

#include <algorithm>
#include <fstream>
#include <iostream>

namespace robot {
namespace {

std::vector<Point> passable_covered_cells(const GridMap& map,
                                          const std::vector<Point>& covered_cells) {
    std::vector<Point> result;
    for (const auto& cell : covered_cells) {
        if (map.passable(cell)) {
            result.push_back(cell);
        }
    }
    return result;
}

int coverage_percent(const GridMap& map, const std::vector<Point>& covered_cells) {
    const int total_passable = map.width() * map.height() - static_cast<int>(map.obstacles().size());
    if (total_passable <= 0) {
        return 0;
    }
    const auto covered = passable_covered_cells(map, covered_cells);
    return static_cast<int>((covered.size() * 100) / static_cast<std::size_t>(total_passable));
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
            << ",\"mode\":\"" << json_escape(timeline[i].mode)
            << "\",\"label\":\"" << json_escape(timeline[i].label) << "\"}";
        if (i + 1 < timeline.size()) {
            out << ",";
        }
    }
    out << "]";
}

void write_sensor_fusion(std::ofstream& out, const std::vector<FusedPose>& fused_poses) {
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

int average_fusion_confidence_percent(const std::vector<FusedPose>& fused_poses) {
    if (fused_poses.empty()) {
        return 0;
    }
    double sum = 0.0;
    for (const auto& pose : fused_poses) {
        sum += pose.confidence;
    }
    return static_cast<int>((sum * 100.0) / static_cast<double>(fused_poses.size()));
}

int max_fusion_error_cm(const std::vector<FusedPose>& fused_poses) {
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
            << ",\"type\":\"" << json_escape(goals[i].type)
            << "\",\"target\":{\"x\":" << goals[i].target.x
            << ",\"y\":" << goals[i].target.y << "}}";
        if (i + 1 < goals.size()) {
            out << ",";
        }
    }
    out << "]";
}

} // namespace

void write_control_state_json(const std::string& path,
                              const GridMap& map,
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
                              const std::vector<FusedPose>& fused_poses,
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
        std::count_if(fused_poses.begin(), fused_poses.end(), [](const FusedPose& pose) {
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
        << ", \"workMode\": \"" << json_escape(work_mode)
        << "\", \"completedSubtasks\": " << completed_subtasks
        << ", \"totalSubtasks\": " << subtasks.size()
        << ", \"goals\": ";
    write_goals(out, goals);
    out << "},\n";
    out << "  \"task\": {\"id\": " << active_goal.id
        << ", \"type\": \"" << json_escape(active_goal.type)
        << "\", \"assigned\": " << leader_id
        << ", \"state\": \"" << (mission_done ? "done" : "running")
        << "\", \"progress\": " << (mission_done ? 100 : static_cast<int>((completed_goals * 100) / goals.size()))
        << ", \"target\": {\"x\": " << active_goal.target.x << ", \"y\": " << active_goal.target.y << "}},\n";
    out << "  \"metrics\": {\"planner\": \"A* + line_of_sight_smoothing\""
        << ", \"rawPath\": " << raw_path.size()
        << ", \"optimizedWaypoints\": " << optimized_path.size()
        << ", \"savedWaypoints\": " << saved
        << ", \"replanCount\": " << replan_count
        << ", \"formationMode\": \"" << json_escape(formation_mode)
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
            << ", \"role\": \"" << to_string(robot.role)
            << "\", \"status\": \"" << to_string(robot.status)
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
        out << "    \"" << json_escape(events[i]) << "\"";
        if (i + 1 < events.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

} // namespace robot
