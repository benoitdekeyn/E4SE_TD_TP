#include "controleur.cpp"

#include <csignal>
#include <ros/ros.h>

bool arret_demande = false;

// Ctrl+C : on lève juste un flag, la boucle principale fait le reset
void sigintHandler(int sig) {
  arret_demande = true;
}

int main(int argc, char **argv) {
  signal(SIGINT, sigintHandler);
  ros::init(argc, argv, "controleur", ros::init_options::NoSigintHandler);

  ros::NodeHandle nh;
  Controleur controleur;
  controleur.init(nh);

  ros::Rate loop_rate(controleur.frequence());

  while (ros::ok() && !arret_demande) {
    controleur.step();
    ros::spinOnce();
    loop_rate.sleep();
  }

  ROS_INFO("Exiting program gracefully ...");
  controleur.resetRobot();
  ros::shutdown();
  return 0;
}
