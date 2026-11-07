#ifndef MOUVEMENTS_CPP
#define MOUVEMENTS_CPP

#include "parametres.cpp"
#include "calculs.cpp"
#include "odometry.cpp"

#include <cmath>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Pose2D.h>
#include <ros/ros.h>

using namespace ros;
using geometry_msgs::Twist;
using geometry_msgs::Pose2D;

// Cibles courantes des différents modes de déplacement
struct Targets {
  double position_x = 0.0; // pour reachPositionX
  double angle = 0.0;      // pour reachAngle
  double x = 0.0;          // pour reachPosition
  double y = 0.0;
};

// Valeurs intermédiaires de reachPosition, utiles pour le debug
struct PositionDebug {
  double angle_target = 0.0;
  double erreur_angle = 0.0;
  double erreur_distance = 0.0;
  double delta_x = 0.0;
  double delta_y = 0.0;
  bool pivot_en_cours = false;
};

// Commande des moteurs via /cmd_vel
class Mouvements {
public:
  Targets targets;
  PositionDebug debug;
  // Mis à true par reachPositionSmooth() quand la cible est dans la tolérance.
  bool position_atteinte = false;

  // params et odom appartiennent au contrôleur, on garde juste leur adresse
  void init(NodeHandle &nh, Params *params, Odometry *odom) {
    this->odom = odom;
    pub_vel = nh.advertise<Twist>("/cmd_vel", 1000);

    this->Kp_linear          = params->Kp_linear;
    this->precision_linear   = params->precision_linear;
    this->Kp_pivot           = params->Kp_pivot;
    this->Kp_angular         = params->Kp_angular;
    this->trigger_pivot    = params->trigger_pivot;
    this->precision_angular  = params->precision_angular;
    this->linear_speed       = params->linear_speed;
    this->angular_speed      = params->angular_speed;
  }

  void reset() {
    stop();
    targets = Targets();
    debug = PositionDebug();
    position_atteinte = false;
  }

  /**
   * Arrête les moteurs du robot.
   */
  void stop() {
    pub_vel.publish(Twist());
  }

  /**
   * Avance le robot à une vitesse linéaire donnée sur l'axe x.
   * @param v La vitesse linéaire poisitive.
   */
  void forward(double v) {
    Twist velocity;
    velocity.linear.x = fabs(v);
    pub_vel.publish(velocity);
  }

  /**
   * Recule le robot à une vitesse linéaire donnée sur l'axe x.
   * @param v La vitesse linéaire positive.
   */
  void backward(double v) {
    Twist velocity;
    velocity.linear.x = -fabs(v);
    pub_vel.publish(velocity);
  }

  /**
   * Déplace le robot avec une vitesse linéaire et angulaire donnée.
   * Les vitesses sont combinées pour faire des arc de cercle.
   * Vitesse angulaire positive -> sens trigonométrique
   * Vitesse angulaire négative -> sens horaire
   * @param linear La vitesse linéaire signée sur l'axe x.
   * @param angular La vitesse angulaire signée sur l'axe z.
   */
  void move(double linear, double angular) {
    Twist velocity;
    velocity.linear.x = linear;
    velocity.angular.z = angular;
    pub_vel.publish(velocity);
  }

  /**
   * Fait pivoter le robot à une vitesse angulaire donnée sur l'axe z.
   * Valeur positive -> sens trigonométrique
   * Valeur négative -> sens horaire
   * @param w La vitesse angulaire.
   */
  void pivoter(double w) {
    Twist velocity;
    velocity.angular.z = w;
    pub_vel.publish(velocity);
  }

  /**
   * Atteint une position donnée sur l'axe x,
   * en avançant ou en reculant tout droit uniquement.
   * Correcteur PROPORTIONNEL -> Kp_POS
   * @param x La position cible sur l'axe x.
   */
  void reachPositionX(double x) {
    double error = x - odom->position.x;
    double soft_linear_speed = fabs(Kp_linear * error);
    soft_linear_speed = soft_linear_speed > linear_speed ? linear_speed : soft_linear_speed;

    if (error > precision_linear) {
      forward(soft_linear_speed);
    } else if (error < -precision_linear) {
      backward(soft_linear_speed);
    } else {
      stop();
    }
  }

  /**
   * Atteint un angle donné en pivotant autour de l'axe z.
   * Correcteur PROPORTIONNEL -> Kp_ANG
   * @param angle L'angle cible.
   */
  void reachAngle(double angle) {
    double error = cal_delta_Theta(angle, odom->position.theta);
    double soft_linear_speed = fabs(Kp_angular * error);
    soft_linear_speed = soft_linear_speed > angular_speed ? angular_speed : soft_linear_speed;

    if (error > precision_angular) {
      pivoter(soft_linear_speed);
    } else if (error < -precision_angular) {
      pivoter(-soft_linear_speed);
    } else {
      stop();
    }
  }

  /**
   * Atteint une position donnée sur l'axe x et y en suivant ces étapes :
   * 1. Pivoter pour s'aligner avec la direction de la cible.
   * 2. Avancer ou reculer jusqu'à atteindre la position cible,
   *    en ajustant l'angle en MODE BLOQUANT si nécessaire en cours de route.
   * Correcteur PROPORTIONNEL -> Kp_POS pour la distance,
   * Correcteur PROPORTIONNEL -> Kp_ANG pour l'angle.
   * @param x La position cible sur l'axe x.
   * @param y La position cible sur l'axe y.
   * @return void
   */
  void reachPosition(double x, double y) {
    debug.delta_x = x - odom->position.x;
    debug.delta_y = y - odom->position.y;
    debug.angle_target = atan2(debug.delta_y, debug.delta_x);
    debug.erreur_distance = sqrt(pow(debug.delta_x, 2) + pow(debug.delta_y, 2));
    debug.erreur_angle = cal_delta_Theta(debug.angle_target, odom->position.theta);

    // Si l'erreur angulaire est supérieure au seuil, pivoter pour s'aligner avec la cible.
    if (fabs(debug.erreur_angle) > precision_angular) {

      // Calcul de la commande de contrôle angulaire proportionnelle à l'erreur angulaire.
      double soft_angular_speed = fabs(Kp_angular * debug.erreur_angle);
      soft_angular_speed = soft_angular_speed > angular_speed ? angular_speed : soft_angular_speed;

      // Pivoter dans la direction appropriée en fonction du signe de l'erreur angulaire.
      if (debug.erreur_angle > precision_angular) {
        pivoter(soft_angular_speed);
      } else if (debug.erreur_angle < -precision_angular) {
        pivoter(-soft_angular_speed);
      }
    }
    // Si l'erreur angulaire est dans le seuil, contrôler la distance vers la cible.
    else
    {
      // Calcul de la commande de contrôle proportionnelle à l'erreur de distance.
      double soft_linear_speed = fabs(Kp_linear * debug.erreur_distance);
      soft_linear_speed = soft_linear_speed > linear_speed ? linear_speed : soft_linear_speed;

      // Déterminer la direction du mouvement en fonction de l'erreur de distance.
      if (debug.erreur_distance > precision_linear) {
        forward(soft_linear_speed);
      } else {
        stop();
      }
    }
  }

  /**
   * Atteint une position donnée sur l'axe x et y en suivant ces étapes :
   * 1. Pivoter pour s'aligner avec la direction de la cible.
   * 2. Avancer ou reculer jusqu'à atteindre la position cible,
   *    en ajustant l'angle EN PARRALLÈLE si nécessaire en cours de route.
   * Correcteur PROPORTIONNEL -> Kp_POS pour la distance,
   * Correcteur PROPORTIONNEL -> Kp_ANG pour l'angle.
   * @param x La position cible sur l'axe x.
   * @param y La position cible sur l'axe y.
   * @return void
   */
  void reachPositionSmooth(double x, double y) {
    position_atteinte = false;
    debug.delta_x = x - odom->position.x;
    debug.delta_y = y - odom->position.y;
    debug.angle_target = atan2(debug.delta_y, debug.delta_x);
    debug.erreur_distance = sqrt(pow(debug.delta_x, 2) + pow(debug.delta_y, 2));
    debug.erreur_angle = cal_delta_Theta(debug.angle_target, odom->position.theta);

    if (debug.erreur_distance < precision_linear && debug.erreur_distance > -precision_linear) {
      stop();
      position_atteinte = true;
      return;
    }

    // Si l'erreur angulaire est supérieure au seuil de pivot bloquant, on pivote pour s'aligner avec la cible.
    if (fabs(debug.erreur_angle) > trigger_pivot) {
      debug.pivot_en_cours = true;
    }

    if (debug.pivot_en_cours == true) 
    {
      // Calcul de la commande de contrôle angulaire proportionnelle à l'erreur angulaire.
      double soft_angular_speed = fabs(Kp_pivot * debug.erreur_angle);
      // Limitation de la vitesse angulaire pour ne pas dépasser la vitesse maximale autorisée.
      soft_angular_speed = soft_angular_speed > angular_speed ? angular_speed : soft_angular_speed;

      // Pivoter dans la direction appropriée en fonction du signe de l'erreur angulaire.
      if (debug.erreur_angle > precision_angular) {
        pivoter(soft_angular_speed);
      } else if (debug.erreur_angle < -precision_angular) {
        pivoter(-soft_angular_speed);
      } else {
        debug.pivot_en_cours = false;
      }
    }
    // Si l'erreur angulaire est dans le seuil de correction en parallèle de l'avancement,
    // on avance ou recule en fonction de l'erreur de distance, tout en ajustant l'angle en parallèle si nécessaire.
    else
    {
      // Calcul de la commande de contrôle proportionnelle à l'erreur de distance.
      double soft_linear_speed = fabs(Kp_linear * debug.erreur_distance);
      // Bridage de la vitesse linéaire pour ne pas dépasser la vitesse maximale autorisée.
      soft_linear_speed = soft_linear_speed > linear_speed ? linear_speed : soft_linear_speed;

      // Calcul de la commande de contrôle angulaire proportionnelle à l'erreur angulaire
      double soft_angular_speed = fabs(Kp_angular * debug.erreur_angle);
      soft_angular_speed = soft_angular_speed > angular_speed ? angular_speed : soft_angular_speed;

      // Déterminer le mouvement linéaire en fonction de l'erreur de distance.
      if (debug.erreur_distance > precision_linear) {
        soft_linear_speed = soft_linear_speed;
      } else if (debug.erreur_distance < -precision_linear) {
        soft_linear_speed = -soft_linear_speed;
      } else {
        soft_linear_speed = 0;
      }

      // Déterminer la rotation angulaire en fonction de l'erreur angulaire.
      if (soft_linear_speed != 0) {
        if (debug.erreur_angle > precision_angular) {
          soft_angular_speed = soft_angular_speed;
        } else if (debug.erreur_angle < -precision_angular) {
          soft_angular_speed = -soft_angular_speed;
        } else {
          soft_angular_speed = 0;
        }
      } else {
        soft_angular_speed = 0;
      }

      // Appliquer les vitesses linéaire et angulaire calculées.
      move(soft_linear_speed, soft_angular_speed);
    }
  }

private:
  double Kp_linear;
  double precision_linear;
  double Kp_pivot;
  double Kp_angular;
  double trigger_pivot;
  double precision_angular;
  double linear_speed;
  double angular_speed;
  Odometry *odom;
  Publisher pub_vel;
};

#endif
