#ifndef SERVEUR_ACTION_CPP
#define SERVEUR_ACTION_CPP

#include <actionlib/server/simple_action_server.h>
#include <TABDK/MoveAction.h>
#include <ros/ros.h>
#include "mouvements.cpp"
#include "modes.cpp"
#include "states.cpp"
#include "odometry.cpp"

using namespace TABDK;
using namespace actionlib;
using namespace ros;

class ServeurAction {
public:
  typedef SimpleActionServer<MoveAction> Server;

  void init(NodeHandle &nh, Mouvements &mouvements, Modes &modes, Odometry &odometry, States &states) {
    this->mouvements = &mouvements;
    this->modes = &modes;
    this->odometry = &odometry;
    this->states = &states;

    // Enregistrement des callbacks d'action
    server = new Server(nh, "moving", false);
    server->registerGoalCallback([this]() { execute(); });
    server->registerPreemptCallback([this]() { preempter(); });
    server->start();
  }

  // Destructeur pcq on a utilisé 'new' dans init()
  ~ServeurAction() {
    if (server) delete server;
  }

  // Le reset global annule les goals actifs ou en attente.
  void reset() {
    states->waypoint_active = false;
    if (server->isActive()) {
      server->setPreempted(MoveResult(), "Robot reset");
    }
    goal.reset();
    if (server->isNewGoalAvailable()) {
      server->acceptNewGoal();
      server->setPreempted(MoveResult(), "Robot reset");
    }
  }

  // Appelee  chaque tour de la while(ok) de ros
  void step() {
    if (!goal) return;

    // Feedback
    MoveFeedback fb;
    fb.position = odometry->position;
    fb.distance_restante = odometry->distanceToPosition(goal->position_x, goal->position_y);
    fb.mode = modes->mode();
    fb.pause = modes->paused();
    server->publishFeedback(fb);

    // Check de l'atteinte du goal en cours
    if (!states->waypoint_active) {
      MoveResult res;
      res.atteint = true;
      res.position = odometry->position;
      server->setSucceeded(res);
      goal.reset();
    }
  }

private:
  Server *server = nullptr;
  Mouvements *mouvements = nullptr;
  Modes *modes = nullptr;
  Odometry *odometry = nullptr;
  States *states = nullptr;
  
  MoveGoalConstPtr goal;

  // Déclenché automatiquement quand le client_action envoie un nouveau goal
  void execute() {
    goal = server->acceptNewGoal();
    
    if (server->isPreemptRequested()) {
      preempter();
      return;
    }

    mouvements->targets.x = goal->position_x;
    mouvements->targets.y = goal->position_y;
    mouvements->position_atteinte = false;
    states->waypoint_active = true;
  }

  // Déclenché si le client annule l'action
  void preempter() {
    if (!goal) return;
    
    states->waypoint_active = false;
    if (modes->mode() == MANUAL) {
      mouvements->stop();
    }
    server->setPreempted();
    goal.reset();
  }
};

#endif