#include <actionlib/client/simple_action_client.h>
#include <geometry_msgs/Pose2D.h>
#include <ros/ros.h>
#include <std_msgs/Empty.h>
#include <std_msgs/Int8.h>
#include <std_msgs/String.h>
#include <TABDK/MoveAction.h>

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

typedef SimpleActionClient<MoveAction> Client;

const int KEY_UP = 65;
const int KEY_DOWN = 66;
const int KEY_RIGHT = 67;
const int KEY_LEFT = 68;
const int KEY_W = 119;
const double WAYPOINT_STEP = 0.1; // m

Pose2D waypoint_virtuel;
vector<Pose2D> trajectoire;
bool goal_active = false;
bool sequence_blocked = false;
bool mode_manual = false;
bool mode_received = false;
bool goal_termine = false;

void resetClient(Client *client, bool annuler_goal) {
  if (goal_active) {
    if (annuler_goal) client->cancelGoal();
    client->stopTrackingGoal();
  }
  waypoint_virtuel = Pose2D();
  trajectoire.clear();
  goal_active = false;
  goal_termine = false;
  sequence_blocked = false;
}

void resetCallback(const Empty::ConstPtr &, Client *client) {
  // Le serveur a déjà préempté le goal lors du reset global.
  resetClient(client, false);
}

void modeCallback(const String::ConstPtr &message, Client *client) {
  const bool nouveau_mode_manual = message->data == "manual";
  // Ne remettre à zéro qu'à la première réception ou à la sortie du manuel.
  if (!nouveau_mode_manual && (!mode_received || mode_manual)) {
    resetClient(client, true);
  }
  mode_manual = nouveau_mode_manual;
  mode_received = true;
}

// Les flèches déplacent la cible virtuelle
// 'w' ajoute une copie à la trajectoire.
void keyboardCallback(const Int8::ConstPtr &message) {
  if (!mode_received || !mode_manual) return;

  switch (message->data) {
  case KEY_UP:
    waypoint_virtuel.x += WAYPOINT_STEP;
    ROS_INFO("\nWaypoint virtuel deplace en avant\n > x=%.2f\n > y=%.2f\n",
             waypoint_virtuel.x, waypoint_virtuel.y);
    break;
  case KEY_DOWN:
    waypoint_virtuel.x -= WAYPOINT_STEP;
    ROS_INFO("\nWaypoint virtuel deplace en arriere\n > x=%.2f\n > y=%.2f\n",
             waypoint_virtuel.x, waypoint_virtuel.y);
    break;
  case KEY_LEFT:
    waypoint_virtuel.y -= WAYPOINT_STEP;
    ROS_INFO("\nWaypoint virtuel deplace à gauche\n > x=%.2f\n > y=%.2f\n",
             waypoint_virtuel.x, waypoint_virtuel.y);
    break;
  case KEY_RIGHT:
    waypoint_virtuel.y += WAYPOINT_STEP;
    ROS_INFO("\nWaypoint virtuel deplace à droite\n > x=%.2f\n > y=%.2f\n",
             waypoint_virtuel.x, waypoint_virtuel.y);
    break;
  case KEY_W: {
    trajectoire.push_back(waypoint_virtuel);
    string msg = "\nWaypoint ajoute à la fin de la trajectoire : \n";
    for (int i = 0; i < trajectoire.size(); i++) {
      msg += " > Waypoint " + to_string(i) + " : (x=" + to_string(trajectoire[i].x) + ", y=" + to_string(trajectoire[i].y) + ")\n";
    }
    ROS_INFO("%s\n", msg.c_str());
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
    ROS_INFO("\nWaypoint atteint\n > temps total : %.0f ms\n > distance parcourue : %.2f m\n > Position : (x=%.2f, y=%.2f)\n > Orientation : %.2f\n\n",
      result->temps_total,
      result->distance_parcourue,
      result->position.x, result->position.y,
      result->position.theta
    );
  } else {
    ROS_WARN("Waypoint non atteint : %s", state.toString().c_str());
  }
}

void activeCb() {
  ROS_INFO("Le goal est actif");
}

void feedbackCallback(const MoveFeedbackConstPtr &feedback) {
  // Le résultat et le feedback passent par des topics distincts.
  if (!goal_active || goal_termine) return;

  ROS_INFO("\nFeedback :\n > Position (x=%.2f, y=%.2f)\n > orientation : %.2f\n > vitesse lineaire (x=%.2f, y=%.2f, z=%.2f)\n > vitesse angulaire (x=%.2f, y=%.2f, z=%.2f)\n > distance restante=%.2f\n > temps ecoule=%.0f ms\n > mode=%s\n%s\n ---- \n",
    feedback->position.x, feedback->position.y,
    feedback->position.theta,
    feedback->vitesse.linear.x, feedback->vitesse.linear.y, feedback->vitesse.linear.z,
    feedback->vitesse.angular.x, feedback->vitesse.angular.y, feedback->vitesse.angular.z,
    feedback->distance_restante,
    feedback->temps_ecoule,
    feedback->mode.c_str(),
    feedback->pause ? "EN PAUSE\n" : ""
  );
}

int main(int argc, char **argv) {
  init(argc, argv, "planner");
  NodeHandle nh;

  Subscriber keyboard_sub = nh.subscribe("/keyboard_input", 1000, keyboardCallback);

  // Les callbacks d'action, du clavier et du reset partagent spinOnce().
  Client client("moving", false);
  Subscriber reset_sub = nh.subscribe<Empty>(
      "/reset", 10, boost::bind(&resetCallback, _1, &client));
  Subscriber mode_sub = nh.subscribe<String>(
      "/mode", 10, boost::bind(&modeCallback, _1, &client));
  Rate attente_serveur(5);
  while (ok() && !client.isServerConnected()) {
    spinOnce();
    attente_serveur.sleep();
  }
  if (!ok()) return 0;
  ROS_INFO("\n\nServeur 'moving' detecte.\nPret pour la saisie des waypoints.\n");

  Rate loop_rate(5); // 5 Hz
  while (ok()) {
    spinOnce(); // Maj keyboard

    if (mode_received && mode_manual && client.isServerConnected() &&
        !goal_active && !sequence_blocked &&
        !trajectoire.empty()) {
      MoveGoal goal;
      goal.position_x = trajectoire.front().x;
      goal.position_y = trajectoire.front().y;

      goal_termine = false;
      goal_active = true;
      client.sendGoal(goal, &doneCb, &activeCb, &feedbackCallback);
      
      ROS_INFO("\nEnvoi du waypoint\n > x=%.2f\n > y=%.2f", goal.position_x, goal.position_y);
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