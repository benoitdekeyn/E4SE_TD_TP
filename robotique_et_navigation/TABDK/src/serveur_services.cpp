#ifndef SERVEUR_SERVICES_CPP
#define SERVEUR_SERVICES_CPP

#include "controleur.cpp"

#include <TABDK/global_control.h>
#include <TABDK/set_mode.h>
#include <ros/ros.h>

using namespace ros;
using namespace TABDK;

class ServeurServices {
public:
  ServeurServices(NodeHandle &nh, Controleur &controleur)
      : controleur_(controleur) {
    srv_set_mode_ = nh.advertiseService(
        "/set_mode", &ServeurServices::setModeCallback, this);
    srv_global_control_ = nh.advertiseService(
        "/global_control", &ServeurServices::globalControlCallback, this);
  }

private:
  Controleur &controleur_;
  ServiceServer srv_set_mode_;
  ServiceServer srv_global_control_;

  bool setModeCallback(set_mode::Request &request,
                       set_mode::Response &response) {
    response.success = controleur_.changerMode(request.mode);
    response.mode_courant = controleur_.modeCourant();
    return true;
  }

  bool globalControlCallback(global_control::Request &request,
                             global_control::Response &response) {
    response.success = controleur_.commandeGlobale(request.command);
    response.mode_courant = controleur_.modeCourant();
    return true;
  }
};

#endif
