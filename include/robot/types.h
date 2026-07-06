#pragma once

#include <cstdint>
#include <string>

namespace robot {

struct Point {
    int x = 0;
    int y = 0;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Point& other) const {
        return !(*this == other);
    }
};

enum class Role {
    Idle,
    Leader,
    Follower
};

enum class ModuleStatus {
    Online,
    Offline,
    Fault
};

struct RobotState {
    std::uint32_t id = 0;
    Point pos{};
    int battery = 100;
    Role role = Role::Idle;
    ModuleStatus status = ModuleStatus::Online;
    std::uint32_t missed_heartbeats = 0;
};

inline std::string to_string(Role role) {
    switch (role) {
    case Role::Idle:
        return "idle";
    case Role::Leader:
        return "leader";
    case Role::Follower:
        return "follower";
    }
    return "unknown";
}

inline std::string to_string(ModuleStatus status) {
    switch (status) {
    case ModuleStatus::Online:
        return "online";
    case ModuleStatus::Offline:
        return "offline";
    case ModuleStatus::Fault:
        return "fault";
    }
    return "unknown";
}

} // namespace robot
