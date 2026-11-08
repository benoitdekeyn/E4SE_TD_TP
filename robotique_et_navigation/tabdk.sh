#!/usr/bin/env bash

ALIAS_FILE="$HOME/.bash_aliases"
touch "$ALIAS_FILE"

# Fonction pour ajouter un alias seulement s'il n'existe pas déjà
add_alias() {
  local line="$1"
  grep -qF "$line" "$ALIAS_FILE" || echo "$line" >> "$ALIAS_FILE"
}

# Configuration des alias
add_alias 'alias cm="cd ~/catkin_ws && catkin_make && source devel/setup.bash && cd"'
add_alias 'alias slam="roslaunch turtlebot3_slam turtlebot3_slam.launch"'
add_alias 'alias teleop="roslaunch turtlebot3_teleop turtlebot3_teleop_key.launch"'
add_alias 'alias robot_vide="roslaunch tb3_td turtlebot3_empty_world.launch"'
add_alias 'alias robot_maison="roslaunch turtlebot3_gazebo turtlebot3_house.launch"'
add_alias 'alias astar="roslaunch path_generator run.launch"'
add_alias 'alias tabdk="roslaunch TABDK run.launch"'
add_alias 'alias tabdk_rviz="roslaunch TABDK rviz_tb3.launch"'

echo "Init aliases done !"

# Source l'environnement ROS dans le terminal courant
source ~/catkin_ws/devel/setup.bash 2>/dev/null
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-key F42ED6FBAB17C654
sudo apt update
sudo apt install -y ros-noetic-turtlebot3-navigation
sudo apt install -y ros-noetic-turtlebot3-slam
