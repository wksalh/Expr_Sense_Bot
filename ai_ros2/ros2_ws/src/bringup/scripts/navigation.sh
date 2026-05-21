#!/bin/bash
gnome-terminal \
--tab -e "zsh -c 'source $HOME/.zshrc;sudo systemctl stop start_app_node.service;rviz2 rviz2 -d $(dirname "$0")/../navigation/rviz/navigation_desktop.rviz'" \
--tab -e "zsh -c 'source $HOME/.zshrc;ros2 launch navigation navigation.launch.py map:=map_01'"
