#pragma once

#include "robot/grid_map.h"
#include "robot/sensor_fusion.h"
#include "robot/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace robot {

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
                              const std::vector<std::string>& events);

} // namespace robot
