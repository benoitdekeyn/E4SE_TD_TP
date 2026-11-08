#ifndef SERVEUR_ACTION_CPP
#define SERVEUR_ACTION_CPP

#include <actionlib/server/simple_action_server.h>
#include <TABDK/MoveAction.h>
#include <ros/ros.h>
#include "parametres.cpp"
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

  void init(NodeHandle &nh, Params &params, Mouvements &mouvements, Modes &modes, Odometry &odometry, States &states) {
    this->nh = &nh;
    this->params = &params;
    this->mouvements = &mouvements;
    this->modes = &modes;
    this->odometry = &odometry;
    this->states = &states;

    demarrer();
  }

  // Destructeur pcq on a utilisé 'new' dans init()
  ~ServeurAction() {
    if (server) delete server;
  }

  // Le reset global annule les goals actifs ou en attente.
  void reset() {
    states->waypoint_active = false;
    if (server->isActive()) {
      MoveResult res;
      res.temps_total = params->ticksToMs(ticks_ecoules);
      res.distance_parcourue = odometry->distanceParcourue() - distance_depart;
      res.position = odometry->position;
      server->setPreempted(res, "Robot reset");
    }
    goal.reset();
    ticks_ecoules = 0;
    if (server->isNewGoalAvailable()) {
      server->acceptNewGoal();
      server->setPreempted(MoveResult(), "Robot reset");
    }
  }

  // Actionlib conserve les dates des anciens goals même après leur préemption.
  void redemarrer() {
    delete server;
    server = nullptr;
    demarrer();
  }

  // Appelee achaque tour de la while(ok) de ros
  void step() {
    if (!goal) return;

    // Un tick représente une période de la boucle, sauf pendant la pause.
    if (!modes->paused()) {
      ++ticks_ecoules;
    }

    // Publication du feedback
    MoveFeedback fb;
    fb.position = odometry->position;
    fb.vitesse = odometry->vitesse;
    fb.distance_restante = odometry->distanceToPosition(goal->position_x, goal->position_y);
    fb.temps_ecoule = params->ticksToMs(ticks_ecoules);
    fb.mode = modes->mode();
    fb.pause = modes->paused();
    server->publishFeedback(fb);

    // Si on a atteint le goal, on publie le résultat
    if (mouvements->position_atteinte) {
      MoveResult res;
      res.atteint = true;
      res.temps_total = params->ticksToMs(ticks_ecoules);
      res.distance_parcourue = odometry->distanceParcourue() - distance_depart;
      res.position = odometry->position;
      // Ne pas reprendre un waypoint terminé après un changement de mode.
      states->waypoint_active = false;
      server->setSucceeded(res);
      goal.reset();
    }
  }

private:
  Server *server = nullptr;
  NodeHandle *nh = nullptr;
  Params *params = nullptr;
  Mouvements *mouvements = nullptr;
  Modes *modes = nullptr;
  Odometry *odometry = nullptr;
  States *states = nullptr;
  
  MoveGoalConstPtr goal;
  std::uint64_t ticks_ecoules = 0;
  double distance_depart = 0.0;

  void demarrer() {
    server = new Server(*nh, "moving", false);
    server->registerGoalCallback(boost::bind(&ServeurAction::execute, this));
    server->registerPreemptCallback(boost::bind(&ServeurAction::preempter, this));
    server->start();
  }

  // Déclenché automatiquement quand le client_action envoie un nouveau goal
  void execute() {
    goal = server->acceptNewGoal();
    ticks_ecoules = 0;
    // Le compteur d'odométrie est cumulé ; on mémorise son début pour ce goal.
    distance_depart = odometry->distanceParcourue();
    
    if (server->isPreemptRequested()) {
      preempter();
      return;
    }

    mouvements->targets.x = goal->position_x;
    mouvements->targets.y = goal->position_y;
    mouvements->position_atteinte = false;
    if (!params->move_smooth) {
      states->waypoint_state = REACH_POSITION;
    } else if (params->move_back) {
      states->waypoint_state = REACH_POSITION_SMOOTH_BACK;
    } else {
      states->waypoint_state = REACH_POSITION_SMOOTH;
    }
    states->waypoint_active = true;
  }

  // Déclenché si le client annule l'action
  void preempter() {
    if (!goal) return;
    
    states->waypoint_active = false;
    mouvements->stop();
    MoveResult res;
    res.temps_total = params->ticksToMs(ticks_ecoules);
    res.distance_parcourue = odometry->distanceParcourue() - distance_depart;
    res.position = odometry->position;
    server->setPreempted(res);
    goal.reset();
  }
};

#endif