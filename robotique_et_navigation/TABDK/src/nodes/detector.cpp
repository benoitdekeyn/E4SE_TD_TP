#include <algorithm>
#include <ros/ros.h>
#include <string>
#include <vector>
#include <sensor_msgs/LaserScan.h>
#include <cfloat>
#include <cmath>


using namespace ros;
using namespace std;

// VARIABLES GLOBALES
int NBR_REGION = 8;
int SCAN_SAMPLE = 360;
int ANGLE_REGION = SCAN_SAMPLE / NBR_REGION;
std::vector<float> distances;
std::vector<float> min_distances;

#define SEUIL_OBSTACLE 0.45 //m


// PROTOTYPE DES FONCTIONS
float get_minimum(vector<float> data, int debut, int fin, int *indiceMin);
void filtrer_distances(vector<float>& data, float seuil_min, float seuil_max);

// FONCTION CALLBACK
// A COMPLETER//
void scanCallback(const sensor_msgs::LaserScan::ConstPtr& scan) {
  ROS_INFO(" --- Mesures a 360 LIDAR --- ");

  distances = scan->ranges;
  int angle_deg_increment = scan->angle_increment * 180 / M_PI;
  filtrer_distances(distances, scan->range_min, scan->range_max);

  float min_region_i;
  int indice_min_i;
  min_distances.clear();

  for (int i = 0; i < NBR_REGION; i++) {
    int angle_debut = i * ANGLE_REGION;
    int angle_fin = (i + 1) * ANGLE_REGION;
    indice_min_i = -1;
    min_region_i = FLT_MAX;
    min_region_i = get_minimum(distances, angle_debut / angle_deg_increment, angle_fin / angle_deg_increment -1, &indice_min_i);
    min_distances.push_back(min_region_i);

    ROS_INFO("Region %d: min distance = %f a %d deg", i, min_region_i, indice_min_i * angle_deg_increment);

    if (min_region_i < SEUIL_OBSTACLE) {
      ROS_WARN("Obstacle detecte dans la region %d! (entre %d et %d deg)", i, angle_debut, angle_fin);
    }
  }
}

int main(int argc, char **argv) {
  init(argc, argv, "detector");
  NodeHandle nh;

  Rate loop_rate(1); //Hz

  // A COMPLETER
  Subscriber scan_sub = nh.subscribe("/scan", 1, scanCallback);

  spin();

  return 0;
}

float get_minimum(vector<float> data, int debut, int fin, int *indice_min) {

  float min = FLT_MAX;

  for (int i = debut; i <= fin; i++) {
    if (data[i]<min && data[i]> 0.0) {
      min = data[i];
      *indice_min = i;
    }
  }
  return std::min(10.0f, min);
}

void filtrer_distances(vector<float>& data, float seuil_min, float seuil_max) {
  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] < seuil_min || data[i] > seuil_max) {
      data[i] = FLT_MAX;
    }
  }
}