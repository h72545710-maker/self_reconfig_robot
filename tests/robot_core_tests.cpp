#include "robot/grid_map.h"
#include "robot/json_utils.h"
#include "robot/protocol.h"
#include "robot/sensor_fusion.h"
#include "robot/types.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << message << '\n';
    }
}

void test_protocol_roundtrip() {
    const robot::StatusPayload status{-3, 8, 91};
    const auto payload = robot::encode_status_payload(status);
    const auto frame = robot::encode_message(robot::MessageType::StateReport, 42, 7, payload);
    const auto decoded = robot::decode_message(frame);

    expect(decoded.has_value(), "valid protocol frame decodes");
    expect(decoded->type == robot::MessageType::StateReport, "message type roundtrips");
    expect(decoded->module_id == 42, "module id roundtrips");
    expect(decoded->sequence == 7, "sequence roundtrips");

    const auto decoded_status = robot::decode_status_payload(decoded->payload);
    expect(decoded_status.has_value(), "status payload decodes");
    expect(decoded_status->x == -3 && decoded_status->y == 8 && decoded_status->battery == 91,
           "status payload values roundtrip");

    std::string corrupted = frame;
    corrupted[corrupted.size() - 1] = static_cast<char>(corrupted.back() ^ 0x01);
    expect(!robot::decode_message(corrupted).has_value(), "CRC rejects corrupted frame");
}

void test_task_payloads() {
    const robot::TaskAssignPayload assign{17, robot::TaskType::Explore, 12, -4};
    const auto decoded_assign = robot::decode_task_assign_payload(robot::encode_task_assign_payload(assign));
    expect(decoded_assign.has_value(), "task assignment decodes");
    expect(decoded_assign->task_id == 17 &&
               decoded_assign->type == robot::TaskType::Explore &&
               decoded_assign->target_x == 12 &&
               decoded_assign->target_y == -4,
           "task assignment values roundtrip");

    const robot::TaskStatusPayload status{17, robot::TaskState::Running, 65};
    const auto decoded_status = robot::decode_task_status_payload(robot::encode_task_status_payload(status));
    expect(decoded_status.has_value(), "task status decodes");
    expect(decoded_status->task_id == 17 &&
               decoded_status->state == robot::TaskState::Running &&
               decoded_status->progress == 65,
           "task status values roundtrip");
}

void test_grid_map_planning() {
    robot::GridMap map(6, 5);
    map.add_obstacle({2, 0});
    map.add_obstacle({2, 1});
    map.add_obstacle({2, 2});
    map.add_obstacle({2, 3});

    const auto path = map.plan_path({0, 0}, {5, 0});
    expect(!path.empty(), "A* finds a path around partial wall");
    expect(path.front() == robot::Point{0, 0}, "path starts at requested point");
    expect(path.back() == robot::Point{5, 0}, "path ends at requested goal");
    expect(std::none_of(path.begin(), path.end(), [&](robot::Point point) {
               return map.is_obstacle(point);
           }),
           "path does not cross static obstacles");

    const auto smoothed = map.optimize_path(path);
    expect(!smoothed.empty(), "path smoothing keeps a non-empty path");
    expect(smoothed.size() <= path.size(), "path smoothing does not add waypoints");

    const auto blocked_path = map.plan_path({0, 0}, {5, 0}, {{1, 0}, {0, 1}});
    expect(blocked_path.empty(), "dynamic blocked cells can make a route unavailable");
}

void test_sensor_fusion() {
    robot::GridMap map(8, 4);
    map.add_obstacle({4, 1});

    robot::SensorFusionFilter filter({1, 1});
    const auto sample = robot::simulate_sensor_sample(3, 2, {1, 1}, {2, 1}, map);
    const auto pose = filter.update(3, {2, 1}, sample);

    expect(pose.module_id == 3, "fused pose keeps module id");
    expect(pose.grid_x >= 1 && pose.grid_x <= 3, "fused x remains near truth");
    expect(pose.grid_y >= 0 && pose.grid_y <= 2, "fused y remains near truth");
    expect(pose.confidence >= 0.35 && pose.confidence <= 0.98, "confidence is clamped");
}

void test_json_escape() {
    const std::string escaped = robot::json_escape("role=\"leader\"\npath\\ok");
    expect(escaped == "role=\\\"leader\\\"\\npath\\\\ok", "JSON string escaping handles quotes/newlines/backslashes");
}

} // namespace

int main() {
    test_protocol_roundtrip();
    test_task_payloads();
    test_grid_map_planning();
    test_sensor_fusion();
    test_json_escape();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "robot_core_tests passed\n";
    return EXIT_SUCCESS;
}
