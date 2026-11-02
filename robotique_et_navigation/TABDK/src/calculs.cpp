#ifndef CALCULS_CPP
#define CALCULS_CPP

#include <cmath>

// Convertit un temps en millisecondes en nombre de tics de la boucle
int msToTicks(int ms) {
  return frequence * ms / 1000;
}

// Convertit un angle en radians en degrés.
double rad_to_deg(double radians) {
  return radians * (180.0 / M_PI);
}

// Convertit un angle en degrés en radians.
double deg_to_rad(double degrees) {
  return degrees * (M_PI / 180.0);
}

// Ajoute un offset à un angle, résultat normalisé entre -π et π (radians).
double cal_angle_offset(double current_angle, double offset) {
  double new_angle = current_angle + offset;
  while (new_angle > M_PI) new_angle -= 2 * M_PI;
  while (new_angle < -M_PI) new_angle += 2 * M_PI;
  return new_angle;
}

// Différence angulaire signée entre deux angles, normalisée entre -π et π (radians).
float cal_delta_Theta(float _theta, float _theta0) {
  float theta, theta0, delta_theta;

  if (_theta < 0) theta = _theta + 2 * M_PI;
  else theta = _theta;

  if (_theta0 < 0) theta0 = _theta0 + 2 * M_PI;
  else theta0 = _theta0;

  delta_theta = theta - theta0;

  if (delta_theta < -M_PI) {
    delta_theta = delta_theta + 2 * M_PI;
  } else if (delta_theta >= M_PI) {
    delta_theta = delta_theta - 2 * M_PI;
  }
  return delta_theta;
}

#endif
