cmake_minimum_required(VERSION 3.16)

project(self_reconfig_robot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(robot_sim
    src/main.cpp
    src/grid_map.cpp
    src/sensor_fusion.cpp
)

target_include_directories(robot_sim PRIVATE include)

add_executable(master_node
    src/master_node.cpp
    src/protocol.cpp
    src/udp_socket.cpp
)

target_include_directories(master_node PRIVATE include)

add_executable(module_node
    src/module_node.cpp
    src/protocol.cpp
    src/udp_socket.cpp
)

target_include_directories(module_node PRIVATE include)

if (UNIX)
    add_executable(stm32_bridge
        src/stm32_bridge.cpp
        src/protocol.cpp
        src/serial_port.cpp
        src/udp_socket.cpp
    )

    target_include_directories(stm32_bridge PRIVATE include)
endif()

if (WIN32)
    target_link_libraries(master_node PRIVATE ws2_32)
    target_link_libraries(module_node PRIVATE ws2_32)
endif()

if (MSVC)
    target_compile_options(robot_sim PRIVATE /W4)
    target_compile_options(master_node PRIVATE /W4)
    target_compile_options(module_node PRIVATE /W4)
else()
    target_compile_options(robot_sim PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(master_node PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(module_node PRIVATE -Wall -Wextra -Wpedantic)
    if (UNIX)
        target_compile_options(stm32_bridge PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
