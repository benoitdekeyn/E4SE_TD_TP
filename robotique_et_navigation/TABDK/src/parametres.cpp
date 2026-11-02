#ifndef PARAMETRES_CPP
#define PARAMETRES_CPP

#include <ros/ros.h>

// Paramètres du contrôleur : valeurs par défaut, écrasées par params.yaml dans load()
class Params {
public:
  double frequence = 5;
  double linear_speed = 0.1;
  double angular_speed = 0.1;

  int move_fwd_tm = 3000; // ms
  int move_bwd_tm = 4000; // ms
  int move_stp_tm = 2000; // ms

  void load(ros::NodeHandle &nh) {
    nh.getParam("frequence", frequence);
    nh.getParam("linear_speed", linear_speed);
    nh.getParam("angular_speed", angular_speed);
    nh.getParam("move_fwd_tm", move_fwd_tm);
    nh.getParam("move_bwd_tm", move_bwd_tm);
    nh.getParam("move_stp_tm", move_stp_tm);
  }
};

#endif
