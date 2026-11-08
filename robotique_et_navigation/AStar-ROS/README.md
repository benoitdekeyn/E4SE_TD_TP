# A* Path Planner

### Astar demo in ROS Rviz
![Demo](https://user-images.githubusercontent.com/31432135/184294267-b0fe5840-1d76-44a7-80a5-d5ba0d35c083.gif)
<br>

### Prerequisition
Install package
```
$ sudo apt install ros-${ROS_DISTRO}-map-server
```
### How to use
1. Clone Package
```
$ mkdir -p ~/catkin_ws/src
$ cd ~/catkin_ws/src
$ git clone https://github.com/zang09/AStar-ROS.git
```
2. Build Package
```
$ cd ..
$ catkin_make
$ source ~/catkin_ws/devel/setup.bash
```
3. Launch packages 
```
$ roslaunch path_generator run.launch
```
4. Send goal pose by rviz tool `2D Nav Goal`

### Path simplification

The planner simplifies the A* path into the minimum number of straight
segments whose endpoints are on the original path. This allows diagonal
segments while checking every touched map cell, including cells at corners.
The published waypoints are at the centers of the selected cells. The robot
still stops at each waypoint.

`ecart_obstacle` in `config/params.yaml` sets the clearance in map cells for
both A* and simplification. For example, 2 cells on a 0.05 m resolution map
correspond to 0.10 m; choose a value that includes the robot's radius and a
safety margin. Occupied and unknown cells, as well as map boundaries, are
treated as obstacles. Reload the map to apply a changed parameter.

This checks the static map only: it does not avoid newly detected obstacles or
guarantee that the controller's actual curved trajectory remains on the
checked segments.
