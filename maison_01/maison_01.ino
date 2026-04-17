#include <LCD_I2C.h>
#include <DHT.h>
#include <OneButton.h>
#include <HCSR04.h>
#include <AccelStepper.h>

#define PIN_PHOTO A0
#define PIN_BOUTON 4
#define PIN_DHT 7
#define PIN_LED 9
#define PIN_TRIG 12
#define PIN_ECHO 11
#define DHT_TYPE DHT11

#define MOTOR_INTERFACE_TYPE 4
#define IN1 8
#define IN2 6
#define IN3 5
#define IN4 3

#define MOTEUR_VITESSE_MAX 500
#define MOTEUR_ACCELERATION 100
#define MOTEUR_VITESSE 200
#define MOTEUR_TOUR_COMPLET 2038

#define SEUIL_LED 30

#define DELAI_BOOT 3000
#define INTERVAL_PHOTO 1000
#define INTERVAL_DHT 5000
#define INTERVAL_LCD 250
#define INTERVAL_SERIAL 3000

#define CLICK_TICKS 300
#define PRESS_TICKS 1000

#define LUM_INIT_MIN 1023
#define LUM_INIT_MAX 0

#define PCT_MIN 0
#define PCT_MAX 100

#define SERIAL_BAUD 9600

LCD_I2C lcd(0x27, 16, 2);
DHT dht(PIN_DHT, DHT_TYPE);
OneButton bouton(PIN_BOUTON, true);
HCSR04 hc(PIN_TRIG, PIN_ECHO);
AccelStepper myStepper(MOTOR_INTERFACE_TYPE, IN1, IN3, IN2, IN4);

enum Etat {
  BOOT,
  LUM_DIST,
  DHT_STATE,
  CALIBRATION
};

Etat etatCourant = BOOT;
Etat etatPrecedent = BOOT;

int lumRaw = 0;
int lumMin = LUM_INIT_MIN;
int lumMax = LUM_INIT_MAX;
int lumPct = 0;
float temperature = 0.0;
float humidite = 0.0;
float distanceCm = 0.0;

unsigned long tDernierePhoto = 0;
unsigned long tDernierDHT = 0;
unsigned long tDerniereSerial = 0;
unsigned long tDerniereLCD = 0;
unsigned long tBoot = 0;

void lirePhoto();
void lireDHT();
void lireDistance();
void majLED();
void envoyerSerial();
void afficherLCD();
int mapperLuminosite(int val, int minVal, int maxVal);
int setMinPhotoR(int val);
int setMaxPhotoR(int val);
void onSimpleClic();
void onDoubleClic();

void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(PIN_LED, OUTPUT);

  lcd.begin();
  lcd.backlight();
  dht.begin();

  myStepper.setMaxSpeed(MOTEUR_VITESSE_MAX);
  myStepper.setAcceleration(MOTEUR_ACCELERATION);
  myStepper.setSpeed(MOTEUR_VITESSE);
  myStepper.moveTo(MOTEUR_TOUR_COMPLET);

  bouton.attachClick(onSimpleClic);
  bouton.attachDoubleClick(onDoubleClic);
  bouton.setClickTicks(CLICK_TICKS);
  bouton.setPressTicks(PRESS_TICKS);

  lcd.setCursor(PCT_MIN, PCT_MIN);
  lcd.print("etd:2349185");

  tBoot = millis();
  etatCourant = BOOT;
}

void loop() {
  unsigned long maintenant = millis();

  bouton.tick();

  if (myStepper.distanceToGo() == 0) {
    myStepper.moveTo(-myStepper.currentPosition());
  }

  myStepper.run();

  switch (etatCourant) {
    case BOOT:
      if (maintenant - tBoot >= DELAI_BOOT) {
        etatCourant = DHT_STATE;
        lcd.clear();
      }
      break;
    case LUM_DIST:
    case DHT_STATE:
    case CALIBRATION:
      break;
  }

  if (maintenant - tDernierePhoto >= INTERVAL_PHOTO) {
    lirePhoto();
    majLED();
    tDernierePhoto = maintenant;
  }

  if (maintenant - tDernierDHT >= INTERVAL_DHT) {
    lireDHT();
    tDernierDHT = maintenant;
  }

  if (maintenant - tDerniereLCD >= INTERVAL_LCD) {
    lireDistance();
    afficherLCD();
    tDerniereLCD = maintenant;
  }

  if (maintenant - tDerniereSerial >= INTERVAL_SERIAL) {
    envoyerSerial();
    tDerniereSerial = maintenant;
  }
}

void onSimpleClic() {
  switch (etatCourant) {
    case LUM_DIST:
      etatCourant = DHT_STATE;
      break;
    case DHT_STATE:
      etatCourant = LUM_DIST;
      break;
    case CALIBRATION:
      etatCourant = DHT_STATE;
      break;
    default:
      break;
  }
}

void onDoubleClic() {
  if (etatCourant == LUM_DIST || etatCourant == DHT_STATE) {
    lumMin = LUM_INIT_MIN;
    lumMax = LUM_INIT_MAX;
    etatCourant = CALIBRATION;
  }
}

void lirePhoto() {
  lumRaw = analogRead(PIN_PHOTO);
  lumPct = mapperLuminosite(lumRaw, lumMin, lumMax);
}

int mapperLuminosite(int val, int minVal, int maxVal) {
  if (maxVal == minVal) return PCT_MIN;
  int pct = map(val, minVal, maxVal, PCT_MIN, PCT_MAX);
  if (pct < PCT_MIN) pct = PCT_MIN;
  if (pct > PCT_MAX) pct = PCT_MAX;
  return pct;
}

int setMinPhotoR(int val) {
  if (val < lumMin) {
    lumMin = val;
  }
  return lumMin;
}

int setMaxPhotoR(int val) {
  if (val > lumMax) {
    lumMax = val;
  }
  return lumMax;
}

void lireDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidite = h;
}

void lireDistance() {
  distanceCm = hc.dist();
}

void majLED() {
  digitalWrite(PIN_LED, (lumPct < SEUIL_LED) ? HIGH : LOW);
}

void envoyerSerial() {
  Serial.print("Lum:");
  Serial.print(lumPct);
  Serial.print(",Min:");
  Serial.print(lumMin);
  Serial.print(",Max:");
  Serial.print(lumMax);
  Serial.print(",Dist:");
  Serial.print(distanceCm, 1);
  Serial.print(",T:");
  Serial.print(temperature, 1);
  Serial.print(",H:");
  Serial.println(humidite, 1);
}

void afficherLCD() {
  if (etatCourant == BOOT) return;

  bool refresh = (etatCourant != etatPrecedent);
  etatPrecedent = etatCourant;

  switch (etatCourant) {
    case LUM_DIST:
      if (refresh) lcd.clear();
      lcd.setCursor(PCT_MIN, PCT_MIN);
      lcd.print("Lum : ");
      lcd.print(lumPct);
      lcd.print("%   ");
      lcd.setCursor(PCT_MIN, 1);
      lcd.print("Dist : ");
      lcd.print(distanceCm, 1);
      lcd.print(" cm   ");
      break;

    case DHT_STATE:
      if (refresh) lcd.clear();
      lcd.setCursor(PCT_MIN, PCT_MIN);
      lcd.print("Temp : ");
      lcd.print(temperature, 1);
      lcd.print(" C   ");
      lcd.setCursor(PCT_MIN, 1);
      lcd.print("Hum : ");
      lcd.print(humidite, 1);
      lcd.print(" %   ");
      break;

    case CALIBRATION:
      lumRaw = analogRead(PIN_PHOTO);

      lumMin = setMinPhotoR(lumRaw);
      lumMax = setMaxPhotoR(lumRaw);

      if (refresh) lcd.clear();
      lcd.setCursor(PCT_MIN, PCT_MIN);
      lcd.print("Lum min : ");
      lcd.print(lumMin);
      lcd.print("   ");
      lcd.setCursor(PCT_MIN, 1);
      lcd.print("Lum max : ");
      lcd.print(lumMax);
      lcd.print("   ");
      break;

    default:
      break;
  }
}
