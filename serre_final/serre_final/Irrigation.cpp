#include "Irrigation.h"

Irrigation::Irrigation(int ledPin, int pin1, int pin2, int pin3, int pin4)
  : _stepper(4, pin1, pin3, pin2, pin4),
    _ledPin(ledPin)
{}

void Irrigation::begin() {
  pinMode(_ledPin, OUTPUT);
  _stepper.setMaxSpeed(STEPPER_MAX_SPD);
  _stepper.setAcceleration(STEPPER_ACCEL);
  _stepper.setCurrentPosition(_posOuverte);
  _stepper.moveTo(_posFermee);
  _state = FERMETURE;
}

int Irrigation::getPositionPct() {
  long pos = _stepper.currentPosition();
  int pct = (int)map(pos, _posFermee, _posOuverte, 0, 100);
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

void Irrigation::demarrerOuverture() {
  _stepper.moveTo(_posOuverte);
  _state = OUVERTURE;
}

void Irrigation::demarrerFermeture() {
  _stepper.moveTo(_posFermee);
  _state = FERMETURE;
}

void Irrigation::majLED() {
  // LED fixe selon état ; blink géré dans update() pour OUVERTURE/FERMETURE
  if (_state != OUVERTURE && _state != FERMETURE) {
    digitalWrite(_ledPin, LOW);
  }
}

void Irrigation::update() {
  _stepper.run();

  // Vérifier click bouton externe
  bool clicked = false;
  if (_pClickFlag != nullptr && *_pClickFlag) {
    clicked = true;
    *_pClickFlag = false; // consommer le flag
  }

  // Distance courante
  float dist = (_pDist != nullptr) ? *_pDist : 0.0f;

  switch (_state) {
    case FERME:
      if (dist > 0 && dist < _distMin) {
        demarrerOuverture();
      }
      break;

    case OUVERTURE:
      if (clicked) {
        _stepper.stop();
        _state = ARRET;
        break;
      }
      if (_stepper.distanceToGo() == 0) {
        _state = OUVERT;
      }
      break;

    case OUVERT:
      if (dist > 0 && dist > _distMax) {
        demarrerFermeture();
      }
      break;

    case FERMETURE:
      if (clicked) {
        _stepper.stop();
        _state = ARRET;
        break;
      }
      if (_stepper.distanceToGo() == 0) {
        _state = FERME;
      }
      break;

    case ARRET:
      if (clicked) {
        demarrerOuverture();
      }
      break;
  }

  // Blink LED pendant mouvement
  if (_state == OUVERTURE || _state == FERMETURE) {
    unsigned long now = millis();
    if (now - _tDernierBlink >= BLINK_RATE) {
      _blinkState = !_blinkState;
      digitalWrite(_ledPin, _blinkState ? HIGH : LOW);
      _tDernierBlink = now;
    }
  } else {
    digitalWrite(_ledPin, LOW);
  }
}
