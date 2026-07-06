#pragma once

#include "robot/types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace robot {

enum class MessageType : std::uint16_t {
    Register = 1,
    Heartbeat = 2,
    StateReport = 3,
    TaskCommand = 4,
    Ack = 5,
    FaultReport = 6,
    TaskAssign = 7,
    TaskStatus = 8
};

enum class TaskType : std::uint16_t {
    Explore = 1
};

enum class TaskState : std::uint16_t {
    Idle = 0,
    Running = 1,
    Done = 2
};

constexpr std::uint16_t kProtocolMagic = 0xA55A;
constexpr std::size_t kProtocolHeaderSize = 14;
constexpr std::size_t kProtocolCrcSize = 2;
constexpr std::size_t kStatusPayloadSize = 12;
constexpr std::size_t kRolePayloadSize = 2;
constexpr std::size_t kAckPayloadSize = 4;
constexpr std::size_t kTaskAssignPayloadSize = 14;
constexpr std::size_t kTaskStatusPayloadSize = 8;

struct StatusPayload {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t battery = 0;
};

struct TaskAssignPayload {
    std::uint32_t task_id = 0;
    TaskType type = TaskType::Explore;
    std::int32_t target_x = 0;
    std::int32_t target_y = 0;
};

struct TaskStatusPayload {
    std::uint32_t task_id = 0;
    TaskState state = TaskState::Idle;
    std::uint16_t progress = 0;
};

struct Message {
    MessageType type = MessageType::Heartbeat;
    std::uint32_t module_id = 0;
    std::uint32_t sequence = 0;
    std::string payload;
    std::uint16_t received_crc = 0;
    std::uint16_t computed_crc = 0;
};

std::uint16_t crc16_ccitt(const std::string& bytes);

std::string encode_message(MessageType type,
                           std::uint32_t module_id,
                           std::uint32_t sequence,
                           const std::string& payload);

std::optional<Message> decode_message(const std::string& bytes);

std::string encode_status_payload(StatusPayload payload);
std::optional<StatusPayload> decode_status_payload(const std::string& payload);

std::string encode_role_payload(Role role);
std::optional<Role> decode_role_payload(const std::string& payload);

std::string encode_ack_payload(std::uint32_t ack_sequence);
std::optional<std::uint32_t> decode_ack_payload(const std::string& payload);

std::string encode_task_assign_payload(TaskAssignPayload payload);
std::optional<TaskAssignPayload> decode_task_assign_payload(const std::string& payload);

std::string encode_task_status_payload(TaskStatusPayload payload);
std::optional<TaskStatusPayload> decode_task_status_payload(const std::string& payload);

std::string message_type_name(MessageType type);
std::string task_type_name(TaskType type);
std::string task_state_name(TaskState state);

} // namespace robot
