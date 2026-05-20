import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='large_models',
            executable='qth_control_move',
            name='qth_control_move',
            output='screen',
            parameters=[],
            remappings=[]
        )
    ])
