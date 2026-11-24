#ifndef PARAMETRES_CPP
#define PARAMETRES_CPP

#include <cstdint>
#include <string>
#include <ros/ros.h>
#include "calculs.cpp"
using namespace ros;

// Paramètres partagés entre les 5 nœuds : valeurs par défaut écrasées par params.yaml (rapport § 3.2).
class Params {
public:
  double frequence = 5;

  double linear_speed       = 0.1;
  double angular_speed      = 0.1;
  double Kp_linear          = 1.0;
  double precision_linear   = 0.02;
  double Kp_pivot           = 2.0;
  double Kp_angular         = 2.0;
  double trigger_pivot      = 10; // deg
  double precision_angular  = 1; // deg

  double waypoint_step      = 0.1; // m
  bool move_smooth          = true;
  bool move_back            = true; // sans effet si move_smooth est false
  std::string default_mode  = "manual";

  int move_fwd_tm = 3000; // ms
  int move_bwd_tm = 4000; // ms
  int move_stp_tm = 2000; // ms
  int delay_collision = 1000; // ms
  float debut_collision_face = 0.12; // m
  float debut_collision_dos = 0.12; // m

  void load(NodeHandle &nh) {
    nh.getParam("frequence", frequence);
    nh.getParam("linear_speed", linear_speed);
    nh.getParam("angular_speed", angular_speed);
    nh.getParam("Kp_linear", Kp_linear);
    nh.getParam("precision_linear", precision_linear);
    nh.getParam("Kp_pivot", Kp_pivot);
    nh.getParam("Kp_angular", Kp_angular);
    nh.getParam("trigger_pivot", trigger_pivot);
    nh.getParam("precision_angular", precision_angular);
    nh.getParam("waypoint_step", waypoint_step);
    nh.getParam("move_smooth", move_smooth);
    nh.getParam("move_back", move_back);
    nh.getParam("default_mode", default_mode);
    nh.getParam("move_fwd_tm", move_fwd_tm);
    nh.getParam("move_bwd_tm", move_bwd_tm);
    nh.getParam("move_stp_tm", move_stp_tm);
    nh.getParam("delay_collision", delay_collision);
    nh.getParam("debut_collision_face", debut_collision_face);
    nh.getParam("debut_collision_dos", debut_collision_dos);

    // Réglés en degrés dans params.yaml pour plus de lisibilité, puis convertis en radians (rapport § 3.2).
    precision_angular = deg_to_rad(precision_angular);
    trigger_pivot = deg_to_rad(trigger_pivot);
  }

  // Convertit un temps en millisecondes en nombre de tics de la boucle
  int msToTicks(int ms) {
    return frequence * ms / 1000;
  }

  // Convertit les tics de la boucle en millisecondes.
  double ticksToMs(std::uint64_t ticks) const {
    return ticks * 1000.0 / frequence;
  }
};

#endif
