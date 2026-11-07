#include <actionlib/client/simple_action_client.h>
#include <geometry_msgs/Pose2D.h>
#include <ros/ros.h>
#include <std_msgs/Int8.h>
#include <TABDK/MoveAction.h>

#include <vector>

using namespace actionlib;
using namespace geometry_msgs;
using namespace ros;
using namespace TABDK;
using std::vector;
using std_msgs::Int8;

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

// Les flèches déplacent la cible virtuelle ; w ajoute une copie à la trajectoire.
void keyboardCallback(const Int8::ConstPtr &message) {
  switch (message->data) {
  case KEY_UP:
    waypoint_virtuel.x += WAYPOINT_STEP;
    break;
  case KEY_DOWN:
    waypoint_virtuel.x -= WAYPOINT_STEP;
    break;
  case KEY_LEFT:
    waypoint_virtuel.y -= WAYPOINT_STEP;
    break;
  case KEY_RIGHT:
    waypoint_virtuel.y += WAYPOINT_STEP;
    break;
  case KEY_W:
    trajectoire.push_back(waypoint_virtuel);
    ROS_INFO("Waypoint ajoute : x=%f, y=%f",
             waypoint_virtuel.x, waypoint_virtuel.y);
    break;
  default:
    break;
  }
}

void doneCb(const SimpleClientGoalState &state,
            const MoveResultConstPtr &result) {
  if (state == SimpleClientGoalState::SUCCEEDED && result) {
    ROS_INFO("Waypoint atteint : %d", result->atteint);
  } else {
    ROS_WARN("Waypoint non atteint : %s", state.toString().c_str());
  }
}

void activeCb() {
  ROS_INFO("Le goal est actif");
}

void feedbackCallback(const MoveFeedbackConstPtr &feedback) {
  ROS_INFO("Position : x=%f, y=%f, distance restante=%f, mode=%s, pause=%d",
           feedback->position.x, feedback->position.y,
           feedback->distance_restante, feedback->mode.c_str(),
           feedback->pause);
}

int main(int argc, char **argv) {
  init(argc, argv, "planner");
  NodeHandle nh;

  Subscriber keyboard_sub = nh.subscribe("/keyboard_input", 1000, keyboardCallback);

  Client client("moving", true);
  client.waitForServer();
  ROS_INFO("Serveur 'moving' détecté. Prêt pour la saisie des waypoints.");

  Rate loop_rate(10); // 10 Hz
  while (ok()) {
    spinOnce(); // Maj keyboard

    if (!goal_active && !sequence_blocked && !trajectoire.empty()) {
      MoveGoal goal;
      goal.position_x = trajectoire.front().x;
      goal.position_y = trajectoire.front().y;

      client.sendGoal(goal, &doneCb, &activeCb, &feedbackCallback);
      goal_active = true;
      
      ROS_INFO("Envoi du waypoint : x=%f, y=%f", goal.position_x, goal.position_y);
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