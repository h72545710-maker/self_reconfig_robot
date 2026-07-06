#include "robot/sensor_fusion.h"

#include <algorithm>
#include <cmath>

namespace robot {

namespace {

double deterministic_noise(std::uint32_t module_id, int tick, int salt) {
    const int raw = static_cast<int>((module_id * 37 + tick * 17 + salt * 23) % 11) - 5;
    return static_cast<double>(raw) * 0.025;
}

Point heading_from_delta(Point previous, Point current) {
    const int dx = current.x - previous.x;
    const int dy = current.y - previous.y;
    if (std::abs(dx) >= std::abs(dy) && dx != 0) {
        return {dx > 0 ? 1 : -1, 0};
    }
    if (dy != 0) {
        return {0, dy > 0 ? 1 : -1};
    }
    return {1, 0};
}

double measure_front_range(Point current, Point heading, const GridMap& map) {
    constexpr int kMaxRange = 4;
    for (int step = 1; step <= kMaxRange; ++step) {
        const Point probe{current.x + heading.x * step, current.y + heading.y * step};
        if (!map.passable(probe)) {
            return static_cast<double>(step - 1);
        }
    }
    return static_cast<double>(kMaxRange);
}

} // namespace

SensorFusionFilter::SensorFusionFilter(Point initial_position) {
    reset(initial_position);
}

void SensorFusionFilter::reset(Point initial_position) {
    x_ = static_cast<double>(initial_position.x);
    y_ = static_cast<double>(initial_position.y);
    yaw_rad_ = 0.0;
    confidence_ = 0.75;
    updates_ = 0;
}

FusedPose SensorFusionFilter::update(std::uint32_t module_id,
                                     Point truth,
                                     const SensorSample& sample) {
    const double predicted_x = x_ + sample.odom_dx;
    const double predicted_y = y_ + sample.odom_dy;

    constexpr double kOdomWeight = 0.62;
    constexpr double kVisualWeight = 0.38;
    x_ = predicted_x * kOdomWeight + sample.visual_x * kVisualWeight;
    y_ = predicted_y * kOdomWeight + sample.visual_y * kVisualWeight;
    yaw_rad_ = yaw_rad_ * 0.7 + sample.imu_yaw_rad * 0.3;

    const double error = std::hypot(x_ - static_cast<double>(truth.x),
                                    y_ - static_cast<double>(truth.y));
    confidence_ = std::clamp(0.96 - error * 0.32 - (sample.front_blocked ? 0.03 : 0.0),
                             0.35,
                             0.98);
    ++updates_;

    FusedPose fused = pose(module_id);
    fused.error_cm = error * 100.0;
    fused.front_blocked = sample.front_blocked;
    return fused;
}

FusedPose SensorFusionFilter::pose(std::uint32_t module_id) const {
    FusedPose fused;
    fused.module_id = module_id;
    fused.x = x_;
    fused.y = y_;
    fused.yaw_rad = yaw_rad_;
    fused.confidence = confidence_;
    fused.grid_x = static_cast<int>(std::lround(x_));
    fused.grid_y = static_cast<int>(std::lround(y_));
    return fused;
}

SensorSample simulate_sensor_sample(std::uint32_t module_id,
                                    int tick,
                                    Point previous,
                                    Point current,
                                    const GridMap& map) {
    const Point heading = heading_from_delta(previous, current);
    const double front_range = measure_front_range(current, heading, map);

    SensorSample sample;
    sample.odom_dx = static_cast<double>(current.x - previous.x) +
                     deterministic_noise(module_id, tick, 1);
    sample.odom_dy = static_cast<double>(current.y - previous.y) +
                     deterministic_noise(module_id, tick, 2);
    sample.visual_x = static_cast<double>(current.x) +
                      deterministic_noise(module_id, tick, 3);
    sample.visual_y = static_cast<double>(current.y) +
                      deterministic_noise(module_id, tick, 4);
    sample.imu_yaw_rad = std::atan2(static_cast<double>(heading.y),
                                    static_cast<double>(heading.x));
    sample.front_range_cells = front_range;
    sample.front_blocked = front_range < 2.0;
    return sample;
}

} // namespace robot
