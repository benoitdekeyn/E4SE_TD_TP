#ifndef MOUVEMENTS_CPP
#define MOUVEMENTS_CPP

#include "parametres.cpp"
#include "calculs.cpp"
#include "odometry.cpp"

#include <cmath>
#include <geometry_msgs/Twist.h>
#include <ros/ros.h>

using geometry_msgs::Twist;

#define Kp_POS 1.0
#define SEUIL_PRECISION_POS 0.02 // m
#define Kp_ANG 2.0
#define SEUIL_PRECISION_ANG deg_to_rad(2) // rad
#define SEUIL_PIVOT_ANG deg_to_rad(5) // rad
#define COEF_V_ANGULAIRE_CORRECTION 0.5

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

  // params et odom appartiennent au contrôleur, on garde juste leur adresse
  void init(ros::NodeHandle &nh, Params *params, Odometry *odom) {
    params_ = params;
    odom_ = odom;
    pub_vel_ = nh.advertise<Twist>("/cmd_vel", 1000);
  }

  void reset() {
    stop();
    targets = Targets();
    debug = PositionDebug();
  }

  /**
   * Arrête les moteurs du robot.
   */
  void stop() {
    pub_vel_.publish(Twist());
  }

  /**
   * Avance le robot à une vitesse linéaire donnée sur l'axe x.
   * @param v La vitesse linéaire poisitive.
   */
  void forward(double v) {
    Twist velocity;
    velocity.linear.x = fabs(v);
    pub_vel_.publish(velocity);
  }

  /**
   * Recule le robot à une vitesse linéaire donnée sur l'axe x.
   * @param v La vitesse linéaire positive.
   */
  void backward(double v) {
    Twist velocity;
    velocity.linear.x = -fabs(v);
    pub_vel_.publish(velocity);
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
    pub_vel_.publish(velocity);
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
    pub_vel_.publish(velocity);
  }

  /**
   * Atteint une position donnée sur l'axe x,
   * en avançant ou en reculant tout droit uniquement.
   * Correcteur PROPORTIONNEL -> Kp_POS
   * @param x La position cible sur l'axe x.
   */
  void reachPositionX(double x) {
    double error = x - odom_->pose.x;
    double linear_speed = fabs(Kp_POS * error);
    linear_speed = linear_speed > params_->linear_speed ? params_->linear_speed : linear_speed;

    if (error > SEUIL_PRECISION_POS) {
      forward(linear_speed);
    } else if (error < -SEUIL_PRECISION_POS) {
      backward(linear_speed);
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
    double error = cal_delta_Theta(angle, odom_->pose.rotation);
    double linear_speed = fabs(Kp_ANG * error);
    linear_speed = linear_speed > params_->angular_speed ? params_->angular_speed : linear_speed;

    if (error > SEUIL_PRECISION_ANG) {
      pivoter(linear_speed);
    } else if (error < -SEUIL_PRECISION_ANG) {
      pivoter(-linear_speed);
    } else {
      stop();
    }
  }

  /**
   * Atteint une position donnée sur l'axe x et y en suivant ces étapes :
   * 1. Pivoter pour s'aligner avec la direction de la cible.
   * 2. Avancer ou reculer jusqu'à atteindre la position cible,
   *    en ajustant l'angle en mode bloquant si nécessaire en cours de route.
   * Correcteur PROPORTIONNEL -> Kp_POS pour la distance,
   * Correcteur PROPORTIONNEL -> Kp_ANG pour l'angle.
   * @param x La position cible sur l'axe x.
   * @param y La position cible sur l'axe y.
   * @return void
   */
  void reachPosition(double x, double y) {
    debug.delta_x = x - odom_->pose.x;
    debug.delta_y = y - odom_->pose.y;
    debug.angle_target = atan2(debug.delta_y, debug.delta_x);
    debug.erreur_distance = sqrt(pow(debug.delta_x, 2) + pow(debug.delta_y, 2));
    debug.erreur_angle = cal_delta_Theta(debug.angle_target, odom_->pose.rotation);

    // Si l'erreur angulaire est supérieure au seuil, pivoter pour s'aligner avec la cible.
    if (fabs(debug.erreur_angle) > SEUIL_PRECISION_ANG) {

      // Calcul de la commande de contrôle angulaire proportionnelle à l'erreur angulaire.
      double angular_speed = fabs(Kp_ANG * debug.erreur_angle);
      angular_speed = angular_speed > params_->angular_speed ? params_->angular_speed : angular_speed;

      // Pivoter dans la direction appropriée en fonction du signe de l'erreur angulaire.
      if (debug.erreur_angle > SEUIL_PRECISION_ANG) {
        pivoter(angular_speed);
      } else if (debug.erreur_angle < -SEUIL_PRECISION_ANG) {
        pivoter(-angular_speed);
      }
    }
    // Si l'erreur angulaire est dans le seuil, contrôler la distance vers la cible.
    else
    {
      // Calcul de la commande de contrôle proportionnelle à l'erreur de distance.
      double linear_speed = fabs(Kp_POS * debug.erreur_distance);
      linear_speed = linear_speed > params_->linear_speed ? params_->linear_speed : linear_speed;

      // Déterminer la direction du mouvement en fonction de l'erreur de distance.
      if (debug.erreur_distance > SEUIL_PRECISION_POS) {
        forward(linear_speed);
      } else if (debug.erreur_distance < -SEUIL_PRECISION_POS) {
        backward(linear_speed);
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
    debug.delta_x = x - odom_->pose.x;
    debug.delta_y = y - odom_->pose.y;
    debug.angle_target = atan2(debug.delta_y, debug.delta_x);
    debug.erreur_distance = sqrt(pow(debug.delta_x, 2) + pow(debug.delta_y, 2));
    debug.erreur_angle = cal_delta_Theta(debug.angle_target, odom_->pose.rotation);

    // Si l'erreur angulaire est supérieure au seuil de pivot bloquant, on pivote pour s'aligner avec la cible.
    if (fabs(debug.erreur_angle) > SEUIL_PIVOT_ANG) {
      debug.pivot_en_cours = true;
    }

    if (debug.pivot_en_cours == true) {

      // Calcul de la commande de contrôle angulaire proportionnelle à l'erreur angulaire.
      double angular_speed = fabs(Kp_ANG * debug.erreur_angle);
      // Limitation de la vitesse angulaire pour ne pas dépasser la vitesse maximale autorisée.
      angular_speed = angular_speed > params_->angular_speed ? params_->angular_speed : angular_speed;

      // Pivoter dans la direction appropriée en fonction du signe de l'erreur angulaire.
      if (debug.erreur_angle > SEUIL_PRECISION_ANG) {
        pivoter(angular_speed);
      } else if (debug.erreur_angle < -SEUIL_PRECISION_ANG) {
        pivoter(-angular_speed);
      } else {
        debug.pivot_en_cours = false;
      }
    }
    // Si l'erreur angulaire est dans le seuil de corrctionen parralele de l'avancement,
    // on avance ou recule en fonction de l'erreur de distance, tout en ajustant l'angle en parallèle si nécessaire.
    else
    {
      // Calcul de la commande de contrôle proportionnelle à l'erreur de distance.
      double linear_speed = fabs(Kp_POS * debug.erreur_distance);
      // Bridage de la vitesse linéaire pour ne pas dépasser la vitesse maximale autorisée.
      linear_speed = linear_speed > params_->linear_speed ? params_->linear_speed : linear_speed;

      // Calcul de la commande de contrôle angulaire proportionnelle à l'erreur angulaire
      double angular_speed = fabs(Kp_ANG * debug.erreur_angle)*COEF_V_ANGULAIRE_CORRECTION;
      angular_speed = angular_speed > params_->angular_speed ? params_->angular_speed : angular_speed;

      // Déterminer le mouvement linéaire en fonction de l'erreur de distance.
      if (debug.erreur_distance > SEUIL_PRECISION_POS) {
        linear_speed = linear_speed;
      } else if (debug.erreur_distance < -SEUIL_PRECISION_POS) {
        linear_speed = -linear_speed;
      } else {
        linear_speed = 0;
      }

      // Déterminer la rotation angulaire en fonction de l'erreur angulaire.
      if (linear_speed != 0) {
        if (debug.erreur_angle > SEUIL_PRECISION_ANG) {
          angular_speed = angular_speed;
        } else if (debug.erreur_angle < -SEUIL_PRECISION_ANG) {
          angular_speed = -angular_speed;
        } else {
          angular_speed = 0;
        }
      } else {
        angular_speed = 0;
      }

      // Appliquer les vitesses linéaire et angulaire calculées.
      move(linear_speed, angular_speed);
    }
  }

private:
  Params *params_;
  Odometry *odom_;
  ros::Publisher pub_vel_;
};

#endif
