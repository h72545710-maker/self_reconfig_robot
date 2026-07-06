#include "robot/protocol.h"

namespace robot {

namespace {

void append_u16(std::string& out, std::uint16_t value) {
    out.push_back(static_cast<char>(value & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
}

void append_u32(std::string& out, std::uint32_t value) {
    out.push_back(static_cast<char>(value & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
    out.push_back(static_cast<char>((value >> 16) & 0xFF));
    out.push_back(static_cast<char>((value >> 24) & 0xFF));
}

std::uint16_t read_u16(const std::string& bytes, std::size_t offset) {
    const auto b0 = static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset]));
    const auto b1 = static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset + 1]));
    return static_cast<std::uint16_t>(b0 | (b1 << 8));
}

std::uint32_t read_u32(const std::string& bytes, std::size_t offset) {
    const auto b0 = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset]));
    const auto b1 = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 1]));
    const auto b2 = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 2]));
    const auto b3 = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 3]));
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

std::uint16_t role_to_wire(Role role) {
    switch (role) {
    case Role::Idle:
        return 0;
    case Role::Leader:
        return 1;
    case Role::Follower:
        return 2;
    }
    return 0;
}

std::optional<Role> wire_to_role(std::uint16_t value) {
    switch (value) {
    case 0:
        return Role::Idle;
    case 1:
        return Role::Leader;
    case 2:
        return Role::Follower;
    default:
        return std::nullopt;
    }
}

std::optional<TaskType> wire_to_task_type(std::uint16_t value) {
    switch (value) {
    case 1:
        return TaskType::Explore;
    default:
        return std::nullopt;
    }
}

std::optional<TaskState> wire_to_task_state(std::uint16_t value) {
    switch (value) {
    case 0:
        return TaskState::Idle;
    case 1:
        return TaskState::Running;
    case 2:
        return TaskState::Done;
    default:
        return std::nullopt;
    }
}

} // namespace

std::uint16_t crc16_ccitt(const std::string& bytes) {
    std::uint16_t crc = 0xFFFF;
    for (unsigned char ch : bytes) {
        crc ^= static_cast<std::uint16_t>(ch) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000) != 0) {
                crc = static_cast<std::uint16_t>((crc << 1) ^ 0x1021);
            } else {
                crc = static_cast<std::uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

std::string encode_message(MessageType type,
                           std::uint32_t module_id,
                           std::uint32_t sequence,
                           const std::string& payload) {
    const auto total_length = static_cast<std::uint16_t>(kProtocolHeaderSize + payload.size() + kProtocolCrcSize);

    std::string out;
    out.reserve(total_length);
    append_u16(out, kProtocolMagic);
    append_u16(out, total_length);
    append_u16(out, static_cast<std::uint16_t>(type));
    append_u32(out, module_id);
    append_u32(out, sequence);
    out.append(payload);
    append_u16(out, crc16_ccitt(out));
    return out;
}

std::optional<Message> decode_message(const std::string& bytes) {
    if (bytes.size() < kProtocolHeaderSize + kProtocolCrcSize) {
        return std::nullopt;
    }

    const std::uint16_t magic = read_u16(bytes, 0);
    const std::uint16_t length = read_u16(bytes, 2);
    if (magic != kProtocolMagic || length != bytes.size()) {
        return std::nullopt;
    }

    const std::uint16_t raw_type = read_u16(bytes, 4);
    switch (static_cast<MessageType>(raw_type)) {
    case MessageType::Register:
    case MessageType::Heartbeat:
    case MessageType::StateReport:
    case MessageType::TaskCommand:
    case MessageType::Ack:
    case MessageType::FaultReport:
    case MessageType::TaskAssign:
    case MessageType::TaskStatus:
        break;
    default:
        return std::nullopt;
    }

    const std::uint16_t received_crc = read_u16(bytes, bytes.size() - kProtocolCrcSize);
    const std::string crc_region = bytes.substr(0, bytes.size() - kProtocolCrcSize);
    const std::uint16_t computed_crc = crc16_ccitt(crc_region);
    if (received_crc != computed_crc) {
        return std::nullopt;
    }

    Message message;
    message.type = static_cast<MessageType>(raw_type);
    message.module_id = read_u32(bytes, 6);
    message.sequence = read_u32(bytes, 10);
    message.payload = bytes.substr(kProtocolHeaderSize, bytes.size() - kProtocolHeaderSize - kProtocolCrcSize);
    message.received_crc = received_crc;
    message.computed_crc = computed_crc;
    return message;
}

std::string encode_status_payload(StatusPayload payload) {
    std::string out;
    out.reserve(kStatusPayloadSize);
    append_u32(out, static_cast<std::uint32_t>(payload.x));
    append_u32(out, static_cast<std::uint32_t>(payload.y));
    append_u32(out, static_cast<std::uint32_t>(payload.battery));
    return out;
}

std::optional<StatusPayload> decode_status_payload(const std::string& payload) {
    if (payload.size() != kStatusPayloadSize) {
        return std::nullopt;
    }

    StatusPayload status;
    status.x = static_cast<std::int32_t>(read_u32(payload, 0));
    status.y = static_cast<std::int32_t>(read_u32(payload, 4));
    status.battery = static_cast<std::int32_t>(read_u32(payload, 8));
    return status;
}

std::string encode_role_payload(Role role) {
    std::string out;
    out.reserve(kRolePayloadSize);
    append_u16(out, role_to_wire(role));
    return out;
}

std::optional<Role> decode_role_payload(const std::string& payload) {
    if (payload.size() != kRolePayloadSize) {
        return std::nullopt;
    }
    return wire_to_role(read_u16(payload, 0));
}

std::string encode_ack_payload(std::uint32_t ack_sequence) {
    std::string out;
    out.reserve(kAckPayloadSize);
    append_u32(out, ack_sequence);
    return out;
}

std::optional<std::uint32_t> decode_ack_payload(const std::string& payload) {
    if (payload.size() != kAckPayloadSize) {
        return std::nullopt;
    }
    return read_u32(payload, 0);
}

std::string encode_task_assign_payload(TaskAssignPayload payload) {
    std::string out;
    out.reserve(kTaskAssignPayloadSize);
    append_u32(out, payload.task_id);
    append_u16(out, static_cast<std::uint16_t>(payload.type));
    append_u32(out, static_cast<std::uint32_t>(payload.target_x));
    append_u32(out, static_cast<std::uint32_t>(payload.target_y));
    return out;
}

std::optional<TaskAssignPayload> decode_task_assign_payload(const std::string& payload) {
    if (payload.size() != kTaskAssignPayloadSize) {
        return std::nullopt;
    }
    const auto task_type = wire_to_task_type(read_u16(payload, 4));
    if (!task_type) {
        return std::nullopt;
    }

    TaskAssignPayload task;
    task.task_id = read_u32(payload, 0);
    task.type = *task_type;
    task.target_x = static_cast<std::int32_t>(read_u32(payload, 6));
    task.target_y = static_cast<std::int32_t>(read_u32(payload, 10));
    return task;
}

std::string encode_task_status_payload(TaskStatusPayload payload) {
    std::string out;
    out.reserve(kTaskStatusPayloadSize);
    append_u32(out, payload.task_id);
    append_u16(out, static_cast<std::uint16_t>(payload.state));
    append_u16(out, payload.progress);
    return out;
}

std::optional<TaskStatusPayload> decode_task_status_payload(const std::string& payload) {
    if (payload.size() != kTaskStatusPayloadSize) {
        return std::nullopt;
    }
    const auto state = wire_to_task_state(read_u16(payload, 4));
    if (!state) {
        return std::nullopt;
    }

    TaskStatusPayload status;
    status.task_id = read_u32(payload, 0);
    status.state = *state;
    status.progress = read_u16(payload, 6);
    return status;
}

std::string message_type_name(MessageType type) {
    switch (type) {
    case MessageType::Register:
        return "REGISTER";
    case MessageType::Heartbeat:
        return "HEARTBEAT";
    case MessageType::StateReport:
        return "STATE";
    case MessageType::TaskCommand:
        return "ROLE";
    case MessageType::Ack:
        return "ACK";
    case MessageType::FaultReport:
        return "FAULT";
    case MessageType::TaskAssign:
        return "TASK_ASSIGN";
    case MessageType::TaskStatus:
        return "TASK_STATUS";
    }
    return "UNKNOWN";
}

std::string task_type_name(TaskType type) {
    switch (type) {
    case TaskType::Explore:
        return "EXPLORE";
    }
    return "UNKNOWN";
}

std::string task_state_name(TaskState state) {
    switch (state) {
    case TaskState::Idle:
        return "idle";
    case TaskState::Running:
        return "running";
    case TaskState::Done:
        return "done";
    }
    return "unknown";
}

} // namespace robot
