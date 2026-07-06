#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

#include <chrono>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

using namespace std::chrono_literals;

namespace {

std::optional<std::string> extract_json_value(const std::string& json, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    const auto key_pos = json.find(marker);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    const auto colon_pos = json.find(':', key_pos + marker.size());
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    auto value_start = colon_pos + 1;
    while (value_start < json.size() && std::isspace(static_cast<unsigned char>(json[value_start]))) {
        ++value_start;
    }
    if (value_start >= json.size()) {
        return std::nullopt;
    }

    const char open = json[value_start];
    if (open == '{' || open == '[') {
        const char close = open == '{' ? '}' : ']';
        int depth = 0;
        bool in_string = false;
        bool escaped = false;

        for (std::size_t i = value_start; i < json.size(); ++i) {
            const char ch = json[i];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                in_string = !in_string;
                continue;
            }
            if (in_string) {
                continue;
            }
            if (ch == open) {
                ++depth;
            } else if (ch == close) {
                --depth;
                if (depth == 0) {
                    return json.substr(value_start, i - value_start + 1);
                }
            }
        }
        return std::nullopt;
    }

    if (open == '"') {
        bool escaped = false;
        for (std::size_t i = value_start + 1; i < json.size(); ++i) {
            const char ch = json[i];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                return json.substr(value_start, i - value_start + 1);
            }
        }
        return std::nullopt;
    }

    auto value_end = value_start;
    while (value_end < json.size() && json[value_end] != ',' && json[value_end] != '\n' &&
           json[value_end] != '}') {
        ++value_end;
    }
    return json.substr(value_start, value_end - value_start);
}

std_msgs::msg::String make_message(const std::string& data) {
    auto message = std_msgs::msg::String();
    message.data = data;
    return message;
}

} // namespace

class ControlStatePublisher : public rclcpp::Node {
public:
    ControlStatePublisher() : Node("control_state_publisher") {
        state_file_ = declare_parameter<std::string>("state_file", "build/control_state.json");
        publish_period_ms_ = declare_parameter<int>("publish_period_ms", 200);
        publishers_["control_state"] =
            create_publisher<std_msgs::msg::String>("/self_reconfig/control_state", 10);
        publishers_["mission"] =
            create_publisher<std_msgs::msg::String>("/self_reconfig/mission", 10);
        publishers_["modules"] =
            create_publisher<std_msgs::msg::String>("/self_reconfig/modules", 10);
        publishers_["path"] =
            create_publisher<std_msgs::msg::String>("/self_reconfig/path", 10);
        publishers_["metrics"] =
            create_publisher<std_msgs::msg::String>("/self_reconfig/metrics", 10);
        publishers_["events"] =
            create_publisher<std_msgs::msg::String>("/self_reconfig/events", 10);
        timer_ = create_wall_timer(std::chrono::milliseconds(publish_period_ms_),
                                   [this]() { publish_state(); });
        RCLCPP_INFO(get_logger(), "publishing %s to /self_reconfig/* topics", state_file_.c_str());
    }

private:
    void publish_state() {
        std::ifstream input(state_file_);
        if (!input) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 3000,
                                 "waiting for state file: %s", state_file_.c_str());
            return;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        const std::string state = buffer.str();

        publishers_["control_state"]->publish(make_message(state));
        publish_slice(state, "mission");
        publish_slice(state, "modules");
        publish_slice(state, "path");
        publish_slice(state, "metrics");
        publish_slice(state, "events");
    }

    void publish_slice(const std::string& state, const std::string& key) {
        const auto value = extract_json_value(state, key);
        if (!value) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
                                 "state file has no '%s' field", key.c_str());
            return;
        }
        publishers_[key]->publish(make_message(*value));
    }

    std::string state_file_;
    int publish_period_ms_ = 200;
    std::unordered_map<std::string, rclcpp::Publisher<std_msgs::msg::String>::SharedPtr> publishers_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ControlStatePublisher>());
    rclcpp::shutdown();
    return 0;
}
