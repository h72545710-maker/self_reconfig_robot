cmake_minimum_required(VERSION 3.16)

project(self_reconfig_robot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(robot_core
    src/control_state_writer.cpp
    src/grid_map.cpp
    src/json_utils.cpp
    src/protocol.cpp
    src/sensor_fusion.cpp
    src/udp_socket.cpp
)

target_include_directories(robot_core PUBLIC include)

if (WIN32)
    target_link_libraries(robot_core PUBLIC ws2_32)
endif()

add_executable(robot_sim
    src/main.cpp
)

target_link_libraries(robot_sim PRIVATE robot_core)

add_executable(master_node
    src/master_node.cpp
)

target_link_libraries(master_node PRIVATE robot_core)

add_executable(module_node
    src/module_node.cpp
)

target_link_libraries(module_node PRIVATE robot_core)

if (UNIX)
    add_executable(stm32_bridge
        src/stm32_bridge.cpp
        src/serial_port.cpp
    )

    target_link_libraries(stm32_bridge PRIVATE robot_core)

    add_executable(mock_stm32
        src/mock_stm32.cpp
    )
endif()

if (BUILD_TESTING)
    add_executable(robot_core_tests
        tests/robot_core_tests.cpp
    )

    target_link_libraries(robot_core_tests PRIVATE robot_core)

    add_test(NAME robot_core_tests COMMAND robot_core_tests)
endif()

if (MSVC)
    target_compile_options(robot_core PRIVATE /W4)
    target_compile_options(robot_sim PRIVATE /W4)
    target_compile_options(master_node PRIVATE /W4)
    target_compile_options(module_node PRIVATE /W4)
else()
    target_compile_options(robot_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(robot_sim PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(master_node PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(module_node PRIVATE -Wall -Wextra -Wpedantic)
    if (UNIX)
        target_compile_options(stm32_bridge PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(mock_stm32 PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
