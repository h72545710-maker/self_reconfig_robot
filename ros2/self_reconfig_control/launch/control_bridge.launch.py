from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    state_file = LaunchConfiguration("state_file")

    return LaunchDescription([
        DeclareLaunchArgument(
            "state_file",
            default_value="build/control_state.json",
            description="Path to the JSON state exported by robot_sim or master_node.",
        ),
        Node(
            package="self_reconfig_control",
            executable="control_state_publisher",
            name="control_state_publisher",
            output="screen",
            parameters=[{"state_file": state_file}],
        ),
    ])
