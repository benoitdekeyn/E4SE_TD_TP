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

// Commande des moteurs via /cmd_vel
class Mouvements {
public:
  Targets targets;
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
    this->trigger_pivot      = params->trigger_pivot;
    this->precision_angular  = params->precision_angular;
    this->linear_speed       = params->linear_speed;
    this->angular_speed      = params->angular_speed;
  }

  void reset() {
    stop();
    targets = Targets();
    pivot_en_cours = false;
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
   * @param v La vitesse linéaire positive.
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
    position_atteinte = false;
    double erreur_distance = odom->distanceToPosition(x, y);
    double erreur_angle = calculerErreurAngle(x, y, false);

    // Si l'erreur angulaire est supérieure au seuil, pivoter pour s'aligner avec la cible.
    if (fabs(erreur_angle) > precision_angular) {
      pivoterVersCible(Kp_angular, erreur_angle);
    }
    // Si l'erreur angulaire est dans le seuil, contrôler la distance vers la cible.
    else
    {
      if (erreur_distance > precision_linear) {
        forward(commande_proportionnelle(Kp_linear, erreur_distance, linear_speed));
      } else {
        stop();
        position_atteinte = true;
      }
    }
  }

  /**
   * Atteint une position donnée sur l'axe x et y en suivant ces étapes :
   * 1. Pivoter pour s'aligner avec la direction de la cible.
   * 2. Avancer ou reculer jusqu'à atteindre la position cible,
   *    en ajustant l'angle EN PARALLÈLE si nécessaire en cours de route (rapport § 4.5).
   * Correcteur PROPORTIONNEL -> Kp_POS pour la distance,
   * Correcteur PROPORTIONNEL -> Kp_ANG pour l'angle.
   * @param x La position cible sur l'axe x.
   * @param y La position cible sur l'axe y.
   * @param arriere Si vrai, vise l'angle opposé (cible + π) et recule.
   * @return void
   */
  void reachPositionSmooth(double x, double y, bool arriere = false) {
    position_atteinte = false;
    double erreur_distance = odom->distanceToPosition(x, y);
    double erreur_angle = calculerErreurAngle(x, y, arriere);

    // Vérifié avant l'angle pour éviter des pivots inutiles si le robot dépasse légèrement la cible (rapport § 4.5).
    if (erreur_distance < precision_linear) {
      stop();
      position_atteinte = true;
      return;
    }

    // Hystérésis de pivot : au-delà de trigger_pivot, on finit l'alignement sur place jusqu'à precision_angular.
    if (fabs(erreur_angle) > trigger_pivot) {
      pivot_en_cours = true;
    }

    if (pivot_en_cours)
    {
      if (!pivoterVersCible(Kp_pivot, erreur_angle)) {
        pivot_en_cours = false;
      }
    }

    // Si l'erreur angulaire est dans le seuil de correction en parallèle de l'avancement,
    // on avance ou recule en fonction de l'erreur de distance, tout en ajustant l'angle en parallèle si nécessaire.
    else
    {
      double soft_linear_speed = commande_proportionnelle(Kp_linear, erreur_distance, linear_speed);
      double soft_angular_speed = commande_proportionnelle(Kp_angular, erreur_angle, angular_speed);

      // Si on doit aller en arrière, on inverse la vitesse linéaire 
      if (arriere) {
        soft_linear_speed = -soft_linear_speed;
      }

      // Si l'erreur angulaire est dans la précision, on ne tourne pas.
      if (fabs(erreur_angle) <= precision_angular) {
        soft_angular_speed = 0;
      }
      // Si l'erreur angulaire est négative, on inverse la vitesse angulaire.
      else if (erreur_angle < 0) {
        soft_angular_speed = -soft_angular_speed;
      }

      // Appliquer les vitesses linéaire et angulaire calculées.
      move(soft_linear_speed, soft_angular_speed);
    }
  }

  /**
   * Comme reachPositionSmooth, mais choisit la marche arrière quand la cible
   * est à plus de 90° de l'axe avant du robot (derrière lui) pour éviter un demi-tour (rapport § 4.5).
   */
  void reachPositionSmoothBack(double x, double y) {
    double erreur_angle_abs = fabs(calculerErreurAngle(x, y, false));
    bool arriere = erreur_angle_abs > M_PI / 2;

    reachPositionSmooth(x, y, arriere);
  }

private:

  // Erreur angulaire vers la cible ; en marche arrière, l'angle visé est décalé de π.
  double calculerErreurAngle(double x, double y, bool arriere) {
    double angle_cible = atan2(y - odom->position.y, x - odom->position.x);

    // si on est en marche arriere, on retourne à 180° la cible
    if (arriere) {
      angle_cible = cal_angle_offset(angle_cible, M_PI);
    }
    return cal_delta_Theta(angle_cible, odom->position.theta);
  }

  // Pivote vers la cible avec le gain Kp. Retourne false si l'erreur est dans la précision angulaire.
  bool pivoterVersCible(double Kp, double erreur_angle) {
    double vitesse = commande_proportionnelle(Kp, erreur_angle, angular_speed);
    if (erreur_angle > precision_angular) {
      pivoter(vitesse);
      return true;
    }
    if (erreur_angle < -precision_angular) {
      pivoter(-vitesse);
      return true;
    }
    return false;
  }

  // Mémorise qu'un pivot bloquant est en cours entre deux appels.
  bool pivot_en_cours = false;
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
