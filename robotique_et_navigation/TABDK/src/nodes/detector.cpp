#include <algorithm>
#include <ros/ros.h>
#include <string>
#include <vector>
#include <sensor_msgs/LaserScan.h>
#include <visualization_msgs/MarkerArray.h>
#include <geometry_msgs/Point.h>
#include <std_msgs/ColorRGBA.h>
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
ros::Publisher marker_pub;

#define SEUIL_OBSTACLE 1 //m

// PROTOTYPE DES FONCTIONS
float get_minimum(vector<float> data, int debut, int fin, int *indiceMin);
void filtrer_distances(vector<float>& data, float seuil_min, float seuil_max);


// FONCTIONS MARKERS
const char* FRAME_MARKERS = "base_scan";
const float OFFSET_LABEL = 0.18f; // m, décalage du texte vers l'extérieur

float angle_region_rad(int i) {
  return ((i + 0.5f) * ANGLE_REGION) * M_PI / 180.0f;
}

std_msgs::ColorRGBA get_color(bool is_danger) {
  std_msgs::ColorRGBA c;
  c.r = is_danger ? 1.0 : 0.0;
  c.g = is_danger ? 0.0 : 1.0;
  c.b = 0.0;
  c.a = 0.8;
  return c;
}

geometry_msgs::Point polar_to_point(float distance, float angle_rad) {
  geometry_msgs::Point p;
  p.x = distance * cos(angle_rad);
  p.y = distance * sin(angle_rad);
  p.z = 0.0;
  return p;
}

// "32 cm" si < 1 m, "1.25 m" sinon
std::string format_label(float distance) {
  char buf[32];
  if (distance < 1.0f)
    snprintf(buf, sizeof(buf), "%.0f cm", distance * 100.0f);
  else
    snprintf(buf, sizeof(buf), "%.2f m", distance);
  return buf;
}

visualization_msgs::Marker init_marker(const std::string& ns, int id, int type) {
  visualization_msgs::Marker m;
  m.header.frame_id = FRAME_MARKERS;
  m.header.stamp = ros::Time();
  m.ns = ns;
  m.id = id;
  m.type = type;
  m.action = visualization_msgs::Marker::ADD;
  m.pose.orientation.w = 1.0;
  return m;
}

// Un segment par région, perpendiculaire à la direction centrale de la région,
// passant par le point minimum. Rouge si l'obstacle est sous le seuil.
visualization_msgs::Marker make_stripline() {
  visualization_msgs::Marker line = init_marker("stripline", 0, visualization_msgs::Marker::LINE_LIST);
  line.scale.x = 0.05;
  line.color.a = 1.0; // remplacée par line.colors

  float demi_angle = (ANGLE_REGION / 2.0f) * M_PI / 180.0f;

  for (int i = 0; i < NBR_REGION; i++) {
    float d = min_distances[i];
    float centre = angle_region_rad(i);
    // Les extrémités sont sur les bords de la région, à d / cos(demi_angle),
    // ce qui rend le segment perpendiculaire à la direction centrale.
    float d_bord = d / cos(demi_angle);

    std_msgs::ColorRGBA color = get_color(d < SEUIL_OBSTACLE);
    line.points.push_back(polar_to_point(d_bord, centre - demi_angle));
    line.colors.push_back(color);
    line.points.push_back(polar_to_point(d_bord, centre + demi_angle));
    line.colors.push_back(color);
  }
  return line;
}

const float LONGUEUR_RAYON = 10.5f; // m

// 8 rayons gris partant du robot, aux jonctions entre régions
visualization_msgs::Marker make_rays() {
  visualization_msgs::Marker rays = init_marker("rays", 0, visualization_msgs::Marker::LINE_LIST);
  rays.scale.x = 0.03;
  rays.color.r = rays.color.g = rays.color.b = 0.5;
  rays.color.a = 0.6;

  geometry_msgs::Point origine = polar_to_point(0.0f, 0.0f);
  for (int i = 0; i < NBR_REGION; i++) {
    float angle = (i * ANGLE_REGION) * M_PI / 180.0f;
    rays.points.push_back(origine);
    rays.points.push_back(polar_to_point(LONGUEUR_RAYON, angle));
  }
  return rays;
}

// Angle de la jonction écrit au bout de chaque rayon
std::vector<visualization_msgs::Marker> make_ray_labels() {
  std::vector<visualization_msgs::Marker> labels;
  for (int i = 0; i < NBR_REGION; i++) {
    int angle_deg = i * ANGLE_REGION;
    visualization_msgs::Marker txt = init_marker("ray_labels", i, visualization_msgs::Marker::TEXT_VIEW_FACING);

    txt.pose.position = polar_to_point(LONGUEUR_RAYON + 0.8f, angle_deg * M_PI / 180.0f);
    txt.scale.z = 0.6;
    txt.color.r = txt.color.g = txt.color.b = 0.7;
    txt.color.a = 1.0;
    txt.text = std::to_string(angle_deg);
    labels.push_back(txt);
  }
  return labels;
}

// Distance écrite au milieu de chaque segment, côté extérieur (même direction que le centre de la région)
std::vector<visualization_msgs::Marker> make_labels() {
  std::vector<visualization_msgs::Marker> labels;
  for (int i = 0; i < NBR_REGION; i++) {
    float d = min_distances[i];
    visualization_msgs::Marker txt = init_marker("labels", i, visualization_msgs::Marker::TEXT_VIEW_FACING);

    txt.pose.position = polar_to_point(d + OFFSET_LABEL, angle_region_rad(i));
    txt.scale.z = d < 1.0f ? 0.15 : 0.3;
    txt.color = get_color(d < SEUIL_OBSTACLE);
    txt.color.a = 1.0;
    txt.text = format_label(d);
    labels.push_back(txt);
  }
  return labels;
}

void update_markers() {
  visualization_msgs::MarkerArray markers;

  markers.markers.push_back(make_rays());
  markers.markers.push_back(make_stripline());

  std::vector<visualization_msgs::Marker> labels = make_labels();
  markers.markers.insert(markers.markers.end(), labels.begin(), labels.end());

  std::vector<visualization_msgs::Marker> ray_labels = make_ray_labels();
  markers.markers.insert(markers.markers.end(), ray_labels.begin(), ray_labels.end());

  marker_pub.publish(markers);
}

// FONCTION CALLBACK
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
  update_markers();
}

int main(int argc, char **argv) {
  init(argc, argv, "detector");
  NodeHandle nh;

  Rate loop_rate(1); //Hz

  Subscriber scan_sub = nh.subscribe("/scan", 1, scanCallback);
  marker_pub = nh.advertise<visualization_msgs::MarkerArray>("/lidar_visualization", 0);

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