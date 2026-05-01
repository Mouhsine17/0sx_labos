#include "Convoyeur.h"

// Identifiants des symboles (utilisés par setAffichage)
#define SYMBOLE_INACTIF 0
#define SYMBOLE_ACTIF   1
#define SYMBOLE_AVANCE  2
#define SYMBOLE_RECULE  3

// Constantes
#define VITESSE_MIN  -100
#define VITESSE_MAX   100
#define PWM_MAX       255
#define JOY_MIN         0
#define JOY_MAX      1023


// "-" : tiret horizontal
static const uint8_t SYM_INACTIF[8] = {
  0b00000000,
  0b00000000,
  0b00000000,
  0b01111110,
  0b01111110,
  0b00000000,
  0b00000000,
  0b00000000
};

// "!" 
static const uint8_t SYM_ACTIF[8] = {
  0b00011000,
  0b00011000,
  0b00011000,
  0b00011000,
  0b00011000,
  0b00000000,
  0b00011000,
  0b00011000
};

// Flèche vers le haut
static const uint8_t SYM_AVANCE[8] = {
  0b00011000,
  0b00111100,
  0b01111110,
  0b11011011,
  0b00011000,
  0b00011000,
  0b00011000,
  0b00011000
};

// Flèche vers le bas
static const uint8_t SYM_RECULE[8] = {
  0b00011000,
  0b00011000,
  0b00011000,
  0b00011000,
  0b11011011,
  0b01111110,
  0b00111100,
  0b00011000
};


Convoyeur *Convoyeur::instance = nullptr;


Convoyeur::Convoyeur(byte motPin1, byte motPin2, byte manette, byte btnPin,
                     byte aff_CLK, byte aff_DIN, byte aff_CS)
  : _moteur_pin_1(motPin1),
    _moteur_pin_2(motPin2),
    _manette(manette),
    _btn_pin(btnPin),
    _aff_CLK(aff_CLK),
    _aff_DIN(aff_DIN),
    _aff_CS(aff_CS),
    _button(btnPin),
    _u8g2(U8G2_R0, aff_CLK, aff_DIN, aff_CS, U8X8_PIN_NONE)
{
  instance = this;

  _button.attachClick(buttonClick, this);
  _button.attachLongPressStart(longPress, this);
  _button.setClickTicks(300);
  _button.setPressTicks(1000);
}


void Convoyeur::begin() {
  pinMode(_moteur_pin_1, OUTPUT);
  pinMode(_moteur_pin_2, OUTPUT);

  analogWrite(_moteur_pin_1, 0);
  analogWrite(_moteur_pin_2, 0);

  _u8g2.begin();
  _u8g2.setContrast(5);

  setAffichage(SYMBOLE_INACTIF);
}



void Convoyeur::buttonClick(void* context) {
  Convoyeur *self = static_cast<Convoyeur*>(context);
  self->_buttonPressed = true;
}

void Convoyeur::longPress(void* context) {
  Convoyeur *self = static_cast<Convoyeur*>(context);
  self->_longPress = true;
}


bool Convoyeur::estEnFonction() {
  return _state != ConvState::INACTIF;
}


void Convoyeur::update() {
  _currentTime = millis();
  _button.tick();

  switch (_state) {
    case ConvState::INACTIF:  inactifState();  break;
    case ConvState::ACTIF:    actifState();    break;
    case ConvState::MANUEL:   manuelState();   break;
    case ConvState::CONSTANT: constantState(); break;
  }

  // Consommer les flags après traitement
  _buttonPressed = false;
  _longPress     = false;

  setMoteurVitesse(_vitesse);
}


void Convoyeur::inactifState() {
  _vitesse = 0;
  setAffichage(SYMBOLE_INACTIF);

  if (_longPress) {
    _state = ConvState::ACTIF;
  }
}


void Convoyeur::actifState() {
  _vitesse = 0;
  setAffichage(SYMBOLE_ACTIF);

  if (_longPress) {
    _state = ConvState::INACTIF;
    return;
  }

  if (joystickHorsZone()) {
    _state = ConvState::MANUEL;
  }
}

void Convoyeur::manuelState() {
  _vitesse = lireJoystickPct();

  
  if (_vitesse > 0)      setAffichage(SYMBOLE_AVANCE);
  else if (_vitesse < 0) setAffichage(SYMBOLE_RECULE);
  else                   setAffichage(SYMBOLE_ACTIF);

  if (_longPress) {
    _state = ConvState::INACTIF;
    return;
  }

  if (_buttonPressed) {
    _vitesseConst = _vitesse;
    _state = ConvState::CONSTANT;
    return;
  }

  if (!joystickHorsZone()) {
    _state = ConvState::ACTIF;
  }
}


void Convoyeur::constantState() {
  _vitesse = _vitesseConst;

  if (_vitesse > 0)      setAffichage(SYMBOLE_AVANCE);
  else if (_vitesse < 0) setAffichage(SYMBOLE_RECULE);
  else                   setAffichage(SYMBOLE_ACTIF);

  if (_longPress) {
    _state = ConvState::INACTIF;
    return;
  }

  if (_buttonPressed) {
    _state = ConvState::MANUEL;
  }
}


void Convoyeur::setMoteurVitesse(int vitesse) {
  if (vitesse > VITESSE_MAX) vitesse = VITESSE_MAX;
  if (vitesse < VITESSE_MIN) vitesse = VITESSE_MIN;

  if (vitesse > 0) {
    int pwm = map(vitesse, 0, VITESSE_MAX, 0, PWM_MAX);
    analogWrite(_moteur_pin_1, pwm);
    analogWrite(_moteur_pin_2, 0);
  } else if (vitesse < 0) {
    int pwm = map(-vitesse, 0, VITESSE_MAX, 0, PWM_MAX);
    analogWrite(_moteur_pin_1, 0);
    analogWrite(_moteur_pin_2, pwm);
  } else {
    analogWrite(_moteur_pin_1, 0);
    analogWrite(_moteur_pin_2, 0);
  }
}


void Convoyeur::setAffichage(int id_symbole) {
  if (id_symbole == _id_symbole_courant) {
    return; // pas de changement, on ne fait rien
  }
  _id_symbole_courant = id_symbole;

  const uint8_t* sym;
  switch (id_symbole) {
    case SYMBOLE_INACTIF: sym = SYM_INACTIF; break;
    case SYMBOLE_ACTIF:   sym = SYM_ACTIF;   break;
    case SYMBOLE_AVANCE:  sym = SYM_AVANCE;  break;
    case SYMBOLE_RECULE:  sym = SYM_RECULE;  break;
    default:              sym = SYM_INACTIF; break;
  }

  _u8g2.clearBuffer();
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      if (sym[y] & (1 << (7 - x))) {
        _u8g2.drawPixel(x, y);
      }
    }
  }
  _u8g2.sendBuffer();
}

int Convoyeur::lireJoystickPct() {
  int raw = analogRead(_manette);
  int pct = map(raw, JOY_MIN, JOY_MAX, VITESSE_MIN, VITESSE_MAX);
  if (pct < VITESSE_MIN) pct = VITESSE_MIN;
  if (pct > VITESSE_MAX) pct = VITESSE_MAX;
  return pct;
}

bool Convoyeur::joystickHorsZone() {
  int raw = analogRead(_manette);
  return abs(raw - _joystickCentre) > _joystickDeadZone;
}
