#pragma once


enum class ConvState { MANUEL, CONSTANT, ACTIF, INACTIF };

#include <OneButton.h>
#include <U8g2lib.h>

class Convoyeur {
public:

  Convoyeur(byte motPin1, byte motPin2, byte manette, byte btnPin,
            byte aff_CLK, byte aff_DIN, byte aff_CS);

  void begin();

 
  bool estEnFonction();

 
  void update();

  
  int       getVitesse() const { return _vitesse; }
  ConvState getEtat()    const { return _state; }

private:
  unsigned long _currentTime = 0;

  byte _moteur_pin_1 = 0;
  byte _moteur_pin_2 = 0;
  byte _manette      = 0;
  byte _btn_pin      = 0;
  byte _aff_CLK      = 0;
  byte _aff_DIN      = 0;
  byte _aff_CS       = 0;

  
  int _joystickDeadZone = 50;     
  int _joystickCentre   = 512;

  OneButton _button;
  bool _buttonPressed = false;
  bool _longPress     = false;

  static Convoyeur *instance;
  static void buttonClick(void *context);
  static void longPress(void *context);

  ConvState _state         = ConvState::INACTIF;
  int       _vitesse       = 0;     
  int       _vitesseConst  = 0;     

  int _id_symbole_courant = -1;

  U8G2_MAX7219_8X8_F_4W_SW_SPI _u8g2;

  // Méthodes par état
  void manuelState();
  void constantState();
  void actifState();
  void inactifState();

  // Configure les broches du moteur
  // vitesse : -100 .. 100
  void setMoteurVitesse(int vitesse);

  // Configure l'affichage du convoyeur
  // id_symbole : 0=INACTIF, 1=ACTIF, 2=AVANCE, 3=RECULE
  void setAffichage(int id_symbole);

  // Helpers
  int  lireJoystickPct();
  bool joystickHorsZone();
};
