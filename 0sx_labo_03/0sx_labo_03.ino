#include "OneButton.h"
#include <LCD_I2C.h>

#define PHOTORESISTANCE A0
#define JOYSTICK_X      A1
#define JOYSTICK_Y      A2
#define BUTTON_PIN      4
#define LED_PIN         11

const int LUM_MIN = 513;
const int LUM_MAX = 893;
const int LUM_SEUIL = 50;

const unsigned long DELAI_PHARES = 5000;
const unsigned long INTERVAL_SERIE = 100;
const unsigned long INTERVAL_LCD = 100;

const int JOYSTICK_MIN = 0;
const int JOYSTICK_MAX = 1023;
const int VITESSE_MAP_MIN = -100;   
const int VITESSE_MAP_MAX = 100;    

const float POSITION_MIN = -100.0;
const float POSITION_MAX = 100.0;


typedef enum {
  TASK_CAPTEUR,
  TASK_JOYSTICK
} Tasks;

OneButton button(BUTTON_PIN, true, true);
LCD_I2C lcd(0x27, 16, 2);

// ---------- VARIABLES GLOBALES ----------
Tasks currentTask = TASK_CAPTEUR;

// État système
bool etatPhares = false;

// Timers
unsigned long tempsPrecedentSerie = 0;
unsigned long tempsPrecedentLCD = 0;
unsigned long tempsPrecedentJoystick = 0;

unsigned long debutSousSeuil = 0;
unsigned long debutAuDessusSeuil = 0;

// Position joystick
float posX = 0.0;
float posY = 0.0;

int valXBrut = 0;
int valYBrut = 0;

// Caractère personnalisé LCD
byte custom85[8] = {
  0b11100,
  0b10100,
  0b11111,
  0b10100,
  0b11111,
  0b00001,
  0b00001,
  0b00111,
};


//void myClickFunction();
//void afficherPageCapteur();
//void afficherPageJoystick();
//void mettreAJourJoystick();
//void mettreAJourEclairage();
//void affichageSerie();
//int lirePourcentageLuminosite();
//int lireValX();
//int lireValY();


void allumage() {
  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, custom85);

  lcd.setCursor(0, 0);
  lcd.print("ACHAMOU");

  lcd.setCursor(0, 1);
  lcd.write(byte(0));

  lcd.setCursor(14, 1);
  lcd.print("85");

  delay(3000);
  lcd.clear();
}

int lireValX() {
  return analogRead(JOYSTICK_X);
}

int lireValY() {
  return analogRead(JOYSTICK_Y);
}

int lirePourcentageLuminosite() {
  int valeurBrute = analogRead(PHOTORESISTANCE);

  int pourcentage = map(valeurBrute, LUM_MIN, LUM_MAX, 0, 100);

  if (pourcentage < 0) {
    pourcentage = 0;
  }

  if (pourcentage > 100) {
    pourcentage = 100;
  }

  return pourcentage;
}

void mettreAJourEclairage() {
  int pourcentageLuminosite = lirePourcentageLuminosite();
  unsigned long maintenant = millis();

  if (pourcentageLuminosite < LUM_SEUIL) {
    if (debutSousSeuil == 0) {
      debutSousSeuil = maintenant;
    }

    debutAuDessusSeuil = 0;

    if (!etatPhares && (maintenant - debutSousSeuil >= DELAI_PHARES)) {
      etatPhares = true;
      digitalWrite(LED_PIN, HIGH);
    }
  }
  else if (pourcentageLuminosite > LUM_SEUIL) {
    if (debutAuDessusSeuil == 0) {
      debutAuDessusSeuil = maintenant;
    }

    debutSousSeuil = 0;

    if (etatPhares && (maintenant - debutAuDessusSeuil >= DELAI_PHARES)) {
      etatPhares = false;
      digitalWrite(LED_PIN, LOW);
    }
  }
  else {
    debutSousSeuil = 0;
    debutAuDessusSeuil = 0;
  }
}

void mettreAJourJoystick() {
  unsigned long maintenant = millis();
  float deltaT = (maintenant - tempsPrecedentJoystick) / 1000.0;

  tempsPrecedentJoystick = maintenant;

  valXBrut = lireValX();
  valYBrut = lireValY();

  float vitesseX = map(valXBrut, JOYSTICK_MIN, JOYSTICK_MAX, VITESSE_MAP_MIN, VITESSE_MAP_MAX) / 10.0;
  float vitesseY = map(valYBrut, JOYSTICK_MIN, JOYSTICK_MAX, VITESSE_MAP_MIN, VITESSE_MAP_MAX) / 10.0;

  posX += vitesseX * deltaT;
  posY += vitesseY * deltaT;

  if (posX > POSITION_MAX) posX = POSITION_MAX;
  if (posX < POSITION_MIN) posX = POSITION_MIN;

  if (posY > POSITION_MAX) posY = POSITION_MAX;
  if (posY < POSITION_MIN) posY = POSITION_MIN;
}

void afficherPageCapteur() {
  int pourcentageLuminosite = lirePourcentageLuminosite();

  lcd.setCursor(0, 0);
  lcd.print("Lum : ");
  lcd.print(pourcentageLuminosite);
  lcd.print("%   ");

  lcd.setCursor(0, 1);
  lcd.print("LUM : ");
  if (etatPhares) {
    lcd.print("ON ");
  } else {
    lcd.print("OFF");
  }
  lcd.print("        ");
}

void afficherPageJoystick() {
  lcd.setCursor(0, 0);
  lcd.print("X:");
  lcd.print((int)posX);
  lcd.print(" cm     ");

  lcd.setCursor(0, 1);
  lcd.print("Y:");
  lcd.print((int)posY);
  lcd.print(" cm     ");
}

void myClickFunction() {
  lcd.clear();

  if (currentTask == TASK_CAPTEUR) {
    currentTask = TASK_JOYSTICK;
  } else {
    currentTask = TASK_CAPTEUR;
  }

  Serial.println(currentTask);
}

void affichageSerie() {
  Serial.print("etd:2349185,x:");
  Serial.print(valXBrut);
  Serial.print(",y:");
  Serial.print(valYBrut);
  Serial.print(",sys:");
  Serial.println(etatPhares ? 1 : 0);
}


void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  button.attachClick(myClickFunction);

  allumage();

  tempsPrecedentJoystick = millis();
}

void loop() {
  unsigned long currentTime = millis();

  button.tick();

  mettreAJourEclairage();
  mettreAJourJoystick();

 
  if (currentTime - tempsPrecedentSerie >= INTERVAL_SERIE) {
    tempsPrecedentSerie = currentTime;
    affichageSerie();
  }

 
  if (currentTime - tempsPrecedentLCD >= INTERVAL_LCD) {
    tempsPrecedentLCD = currentTime;

    switch (currentTask) {
      case TASK_CAPTEUR:
        afficherPageCapteur();
        break;

      case TASK_JOYSTICK:
        afficherPageJoystick();
        break;
    }
  }
}