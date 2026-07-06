cmake_minimum_required(VERSION 3.16)
project(self_reconfig_control)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(control_state_publisher src/control_state_publisher.cpp)
ament_target_dependencies(control_state_publisher rclcpp std_msgs)

install(TARGETS control_state_publisher
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
