#pragma once

#include "robot/grid_map.h"
#include "robot/types.h"

#include <cstdint>
#include <vector>

namespace robot {

struct SensorSample {
    double odom_dx = 0.0;
    double odom_dy = 0.0;
    double visual_x = 0.0;
    double visual_y = 0.0;
    double imu_yaw_rad = 0.0;
    double front_range_cells = 0.0;
    bool front_blocked = false;
};

struct FusedPose {
    std::uint32_t module_id = 0;
    double x = 0.0;
    double y = 0.0;
    double yaw_rad = 0.0;
    double confidence = 0.0;
    double error_cm = 0.0;
    int grid_x = 0;
    int grid_y = 0;
    bool front_blocked = false;
};

class SensorFusionFilter {
public:
    SensorFusionFilter() = default;
    explicit SensorFusionFilter(Point initial_position);

    void reset(Point initial_position);
    FusedPose update(std::uint32_t module_id,
                     Point truth,
                     const SensorSample& sample);
    FusedPose pose(std::uint32_t module_id) const;

private:
    double x_ = 0.0;
    double y_ = 0.0;
    double yaw_rad_ = 0.0;
    double confidence_ = 0.75;
    std::uint32_t updates_ = 0;
};

SensorSample simulate_sensor_sample(std::uint32_t module_id,
                                    int tick,
                                    Point previous,
                                    Point current,
                                    const GridMap& map);

} // namespace robot
