#include "controleur.cpp"

#include <csignal>
#include <ros/ros.h>

using namespace ros;

bool arret_demande = false;

// Ctrl+C : on lève juste un flag, la boucle principale fait le reset
void sigintHandler(int sig) {
  arret_demande = true;
}

int main(int argc, char **argv) {
  signal(SIGINT, sigintHandler);
  init(argc, argv, "controleur", init_options::NoSigintHandler);

  NodeHandle nh;
  Controleur controleur;
  controleur.init(nh);

  Rate loop_rate(controleur.frequence());

  while (ok() && !arret_demande) {
    controleur.step();
    spinOnce();
    loop_rate.sleep();
  }

  ROS_INFO("Exiting program gracefully ...");
  controleur.resetRobot();
  shutdown();
  return 0;
}
