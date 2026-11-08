#include <actionlib/client/simple_action_client.h>
#include <geometry_msgs/Pose2D.h>
#include <nav_msgs/Path.h>
#include <ros/ros.h>
#include <std_msgs/Empty.h>
#include <std_msgs/Int8.h>
#include <std_msgs/String.h>
#include <TABDK/MoveAction.h>
#include "../calculs.cpp"

#include <cmath>
#include <vector>

using namespace actionlib;
using namespace geometry_msgs;
using namespace ros;
using namespace TABDK;
using std::vector;
using std::to_string;
using std::string;
using std_msgs::Empty;
using std_msgs::Int8;
using std_msgs::String;
using nav_msgs::Path;
using std_msgs::String;

typedef SimpleActionClient<MoveAction> Client;

const int KEY_UP = 65;
const int KEY_DOWN = 66;
const int KEY_RIGHT = 67;
const int KEY_LEFT = 68;
const int KEY_W = 119;
const int KEY_X = 120;
double WAYPOINT_STEP = 0.1; // m, surchargé par le paramètre waypoint_step

Pose2D waypoint_virtuel;
vector<Pose2D> trajectoire;
bool goal_active = false;
bool sequence_blocked = false;
bool mode_manual = false;
bool mode_auto = false;
bool mode_received = false;
bool goal_termine = false;
bool annulation_demandee = false;

void viderTrajectoire() {
  if (goal_active) {
    annulation_demandee = true;
  }
  trajectoire.clear();
  goal_active = false;
  goal_termine = false;
  sequence_blocked = false;
}

void resetClient() {
  viderTrajectoire();
  waypoint_virtuel = Pose2D();
}

void resetCallback(const Empty::ConstPtr &) {
  resetClient();
}

void modeCallback(const String::ConstPtr &message) {
  if (message->data == "manual") {
    mode_manual = true;
    mode_auto = false;
  } else if (message->data == "auto") {
    mode_manual = false;
    mode_auto = true;
  }
  mode_received = true;
  if (!mode_manual && !mode_auto) {
    ROS_WARN_THROTTLE(1.0, "Mode inconnu : %s", message->data.c_str());
  }
}

void printNouvelleTrajectoire() {
  string msg = "\nNouvelle trajectoire\n";
  msg += " --> " + to_string(trajectoire.size()) + " waypoints\n";
  for (int i = 0; i < trajectoire.size(); i++) {
    msg += " > Waypoint " + to_string(i) 
         + " : (x=" + to_string(trajectoire[i].x) 
         + ", y=" + to_string(trajectoire[i].y) + ")\n";
  }
  ROS_INFO("%s", msg.c_str());
}

void robotPathCallback(const Path::ConstPtr &message) {
  ROS_INFO("\n##    Nouvelle trajectoire recue    ##\n");

  if (!mode_received || !mode_auto) {
    ROS_WARN("Trajectoire ignoree : mode automatique non actif");
    return;
  }

  vector<Pose2D> nouvelle_trajectoire;
  for (const auto &pose : message->poses) {
    Pose2D waypoint;
    waypoint.x = pose.pose.position.x;
    waypoint.y = pose.pose.position.y;
    nouvelle_trajectoire.push_back(waypoint);
  }

  resetClient();
  trajectoire = nouvelle_trajectoire;
  printNouvelleTrajectoire();
}

// Les flèches déplacent la cible virtuelle, 'w' l'ajoute et 'x' vide la trajectoire.
void keyboardCallback(const Int8::ConstPtr &message) {
  if (!mode_received || !mode_manual) return;

  switch (message->data) {
  case KEY_UP:
    waypoint_virtuel.x += WAYPOINT_STEP;
    ROS_INFO("\nWaypoint virtuel deplace de +%.0fcm en x\n > x=%.2f\n > y=%.2f\n",
             WAYPOINT_STEP * 100, waypoint_virtuel.x, waypoint_virtuel.y);
    break;
  case KEY_DOWN:
    waypoint_virtuel.x -= WAYPOINT_STEP;
    ROS_INFO("\nWaypoint virtuel deplace de -%.0fcm en x\n > x=%.2f\n > y=%.2f\n",
             WAYPOINT_STEP * 100, waypoint_virtuel.x, waypoint_virtuel.y);
    break;
  case KEY_LEFT:
    waypoint_virtuel.y += WAYPOINT_STEP;
    ROS_INFO("\nWaypoint virtuel deplace de +%.0fcm en y\n > x=%.2f\n > y=%.2f\n",
             WAYPOINT_STEP * 100, waypoint_virtuel.x, waypoint_virtuel.y);
    break;
  case KEY_RIGHT:
    waypoint_virtuel.y -= WAYPOINT_STEP;
    ROS_INFO("\nWaypoint virtuel deplace de -%.0fcm en y\n > x=%.2f\n > y=%.2f\n",
             WAYPOINT_STEP * 100, waypoint_virtuel.x, waypoint_virtuel.y);
    break;
  case KEY_W: {
    trajectoire.push_back(waypoint_virtuel);
    ROS_INFO("\n##    Waypoint ajoute    ##\n");
    printNouvelleTrajectoire();
    break;
  }
  case KEY_X: {
    trajectoire.clear();
    ROS_INFO("\n##    Trajectoire videe    ##\n");
    break;
  }
  default:
    break;
  }
}

void doneCb(const SimpleClientGoalState &state,
            const MoveResultConstPtr &result) {
  goal_termine = true;
  if (state == SimpleClientGoalState::SUCCEEDED && result) {
    ROS_INFO("\nWaypoint atteint\n > temps total : %.0f ms\n > distance parcourue : %.2f m\n > Position : (x=%.2f, y=%.2f) m\n > Orientation : %.2f deg\n\n",
      result->temps_total,
      result->distance_parcourue,
      result->position.x, result->position.y,
      rad_to_deg(result->position.theta)
    );
  } else {
    ROS_WARN("Waypoint non atteint : %s", state.toString().c_str());
  }
}

void activeCb() {
  ROS_INFO("\n##    Goal active    ##\n");
}

void feedbackCallback(const MoveFeedbackConstPtr &feedback) {
  if (!goal_active || goal_termine) return;

  if (feedback->pause) {
    ROS_INFO_THROTTLE(1.0, "\n### EN PAUSE ###");
    return;
  }
  ROS_INFO_THROTTLE(1.0, "\nFeedback :\n > Position (x=%.2f, y=%.2f) m\n > Orientation : %.2f deg\n > Vitesse lineaire (x=%.2f, y=%.2f, z=%.2f) m/s\n > Vitesse angulaire (x=%.2f, y=%.2f, z=%.2f) deg/s\n > Distance restante : %.2f m\n > Temps ecoule : %.0f ms\n > Mode : %s\n",
    feedback->position.x, feedback->position.y,
    rad_to_deg(feedback->position.theta),
    feedback->vitesse.linear.x, feedback->vitesse.linear.y, feedback->vitesse.linear.z,
    rad_to_deg(feedback->vitesse.angular.x), rad_to_deg(feedback->vitesse.angular.y), rad_to_deg(feedback->vitesse.angular.z),
    feedback->distance_restante,
    feedback->temps_ecoule,
    feedback->mode.c_str()
  );
}

void traiterAnnulation(Client &client) {
  if (annulation_demandee) {
    client.cancelGoal();
    client.stopTrackingGoal();
    annulation_demandee = false;
    goal_termine = false;
  }
}

bool attendreServeur(Client &client) {
  Rate attente_serveur(5); // 5 Hz
  while (ok() && !client.isServerConnected()) {
    spinOnce();
    traiterAnnulation(client);
    attente_serveur.sleep();
  }
  if (!ok()) return false; // si c'est ros ok qui a fait sortir de la while

  ROS_INFO("\n\nServeur 'moving' detecte.\nPret pour la saisie des waypoints.\n");
  return true;
}

int main(int argc, char **argv) {
  init(argc, argv, "planner");
  NodeHandle nh;
  nh.getParam("waypoint_step", WAYPOINT_STEP);

  // Les callbacks d'action, du clavier et du reset partagent spinOnce().
  Client client("moving", false);
  Subscriber keyboard_sub = nh.subscribe("/keyboard_input", 1000, keyboardCallback);
  Subscriber reset_sub = nh.subscribe("/reset", 10, resetCallback);
  Subscriber mode_sub = nh.subscribe("/mode", 10, modeCallback);
  Subscriber robot_path_sub = nh.subscribe("/robot_path", 1, robotPathCallback);
  
  if (!attendreServeur(client)) return 0;

  // Boucle principale du noeud
  Rate loop_rate(5); // 5 Hz
  while (ok()) {
    spinOnce(); // Maj keyboard
    traiterAnnulation(client);

    if (!client.isServerConnected() && !attendreServeur(client)) break;

    if (mode_received && (mode_manual || mode_auto) && !goal_active && !sequence_blocked &&
        !trajectoire.empty()) {
      MoveGoal goal;
      goal.position_x = trajectoire[0].x;
      goal.position_y = trajectoire[0].y;

      goal_termine = false;
      goal_active = true;
      client.sendGoal(goal, &doneCb, &activeCb, &feedbackCallback);
      
      ROS_INFO("\nEnvoi du nouveau waypoint a suivre\n > x=%.2f\n > y=%.2f\n", goal.position_x, goal.position_y);
    }

    if (goal_active && client.getState().isDone()) {
      goal_active = false;
      if (client.getState() == SimpleClientGoalState::SUCCEEDED) {
        trajectoire.erase(trajectoire.begin());
      } else {
        sequence_blocked = true;
        ROS_WARN("Trajectoire conservee : envoi suspendu apres echec du goal");
      }
    }

    loop_rate.sleep();
  }

  client.cancelGoal();
  return 0;
}