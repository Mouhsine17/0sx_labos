#pragma once

#include <AccelStepper.h>

typedef enum { FERME, OUVERTURE, OUVERT, FERMETURE, ARRET } IrrigationState;

class Irrigation {
public:
  // Constructeur : broche LED indicatrice + 4 broches stepper
  Irrigation(int ledPin, int pin1, int pin2, int pin3, int pin4);

  // Initialisation (à appeler dans setup)
  void begin();

  // Retourne la position brute (steps)
  int getPosition() { return (int)_stepper.currentPosition(); }

  // Retourne la position en pourcentage (0-100)
  int getPositionPct();

  // Configure les positions fermée et ouverte (en steps)
  void setClosedOpenedPos(int closed, int opened) {
    _posFermee = closed;
    _posOuverte = opened;
  }

  // Fournit une référence à la variable distance (cm)
  // La classe lira cette valeur dans update()
  int setDistance(float &dist) {
    _pDist = &dist;
    return (int)dist;
  }

  // Fournit une référence au flag de click bouton
  void setBtnClickFlag(bool &clickFlag) {
    _pClickFlag = &clickFlag;
  }

  // Vrai si le stepper est en mouvement (OUVERTURE ou FERMETURE)
  int isMoving() {
    return (_state == OUVERTURE || _state == FERMETURE) ? 1 : 0;
  }

  // Retourne l'état courant
  int getCurrentState() { return (int)_state; }

  // Appelée dans loop() — met à jour la machine à états
  void update();

private:
  AccelStepper _stepper;
  int          _ledPin;
  IrrigationState _state = FERMETURE;

  int  _posFermee  = 0;
  int  _posOuverte = 2038;

  // Distances seuils (cm)
  int _distMin = 20;
  int _distMax = 25;

  // Références externes
  float *_pDist      = nullptr;
  bool  *_pClickFlag = nullptr;

  // LED blink
  unsigned long _tDernierBlink = 0;
  bool          _blinkState    = false;

  static const int BLINK_RATE       = 100;
  static const int STEPPER_MAX_SPD  = 500;
  static const int STEPPER_ACCEL    = 100;

  void demarrerOuverture();
  void demarrerFermeture();
  void majLED();
};
