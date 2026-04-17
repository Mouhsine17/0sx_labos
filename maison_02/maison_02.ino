#include <LCD_I2C.h>
#include <DHT.h>
#include <OneButton.h>
#include <HCSR04.h>
#include <AccelStepper.h>

#define PIN_PHOTO   A0
#define PIN_BOUTON   4
#define PIN_DHT      7
#define PIN_LED      9
#define PIN_TRIG    12
#define PIN_ECHO    11

#define MOTOR_INTERFACE_TYPE 4
#define IN_1 30
#define IN_2 32
#define IN_3 34
#define IN_4 36

#define DHT_TYPE        DHT11
#define SEUIL_LED       30
#define DELAI_BOOT      3000
#define INTERVAL_PHOTO  1000
#define INTERVAL_DHT    5000
#define INTERVAL_LCD     250
#define INTERVAL_SERIAL 3000
#define INTERVAL_LED_BLINK 100
#define CLICK_TICKS      300
#define PRESS_TICKS     1000
#define LUM_INIT_MIN    1023
#define LUM_INIT_MAX       0
#define PCT_MIN            0
#define PCT_MAX          100
#define SERIAL_BAUD     9600

#define VANNE_FERMEE      0
#define VANNE_OUVERTE  2038
#define DIST_MIN          20
#define DIST_MAX          25
#define STEPPER_MAX_SPEED  500.0
#define STEPPER_ACCEL      100.0

LCD_I2C      lcd(0x27, 16, 2);
DHT          dht(PIN_DHT, DHT_TYPE);
OneButton    bouton(PIN_BOUTON, true);
HCSR04       hc(PIN_TRIG, PIN_ECHO);
AccelStepper myStepper(MOTOR_INTERFACE_TYPE, IN_1, IN_3, IN_2, IN_4);

enum EtatPrincipal {
  BOOT,
  LUM_DIST,
  DHT_STATE,
  CALIBRATION,
  ETAT_VANNE
};

enum EtatVanne {
  VANNE_FERME,
  VANNE_OUVERTURE,
  VANNE_OUVERT,
  VANNE_FERMETURE,
  VANNE_ARRET
};

EtatPrincipal etatCourant   = BOOT;
EtatPrincipal etatPrecedent = BOOT;
EtatVanne     etatVanne     = VANNE_FERMETURE;

int   lumRaw    = 0;
int   lumMin    = LUM_INIT_MIN;
int   lumMax    = LUM_INIT_MAX;
int   lumPct    = 0;
float temperature = 0.0;
float humidite    = 0.0;
float distanceCm  = 0.0;

int vannePct = 100;

unsigned long tDernierePhoto  = 0;
unsigned long tDernierDHT     = 0;
unsigned long tDerniereSerial = 0;
unsigned long tDerniereLCD    = 0;
unsigned long tBoot           = 0;
unsigned long tDernierBlink   = 0;
bool          ledBlinkState   = false;

void lirePhoto();
void lireDHT();
void lireDistance();
void majLED();
void envoyerSerial();
void afficherLCD();
int  mapperLuminosite(int val, int minVal, int maxVal);
int  setMinPhotoR(int val);
int  setMaxPhotoR(int val);
void onSimpleClic();
void onDoubleClic();
void tacheMoteur();
void tacheVanne();
int  calcVannePct();
void demarrerOuverture();
void demarrerFermeture();

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(PIN_LED, OUTPUT);

  lcd.begin();
  lcd.backlight();
  dht.begin();

  bouton.attachClick(onSimpleClic);
  bouton.attachDoubleClick(onDoubleClic);
  bouton.setClickTicks(CLICK_TICKS);
  bouton.setPressTicks(PRESS_TICKS);

  lcd.setCursor(0, 0);
  lcd.print("etd:2349185");

  myStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  myStepper.setAcceleration(STEPPER_ACCEL);
  myStepper.setCurrentPosition(VANNE_OUVERTE);
  myStepper.moveTo(VANNE_FERMEE);

  tBoot = millis();
  etatCourant = BOOT;
}

void loop() {
  unsigned long maintenant = millis();

  bouton.tick();
  tacheMoteur();

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
    case ETAT_VANNE:
      tacheVanne();
      break;
  }

  if (maintenant - tDernierePhoto >= INTERVAL_PHOTO) {
    lirePhoto();
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

  if (etatVanne == VANNE_OUVERTURE || etatVanne == VANNE_FERMETURE) {
    if (maintenant - tDernierBlink >= INTERVAL_LED_BLINK) {
      ledBlinkState = !ledBlinkState;
      digitalWrite(PIN_LED, ledBlinkState ? HIGH : LOW);
      tDernierBlink = maintenant;
    }
  } else {
    majLED();
  }
}

void tacheMoteur() {
  myStepper.run();
  vannePct = calcVannePct();
}

int calcVannePct() {
  long pos = myStepper.currentPosition();
  int pct = map(pos, VANNE_FERMEE, VANNE_OUVERTE, PCT_MIN, PCT_MAX);
  if (pct < PCT_MIN) pct = PCT_MIN;
  if (pct > PCT_MAX) pct = PCT_MAX;
  return pct;
}

void tacheVanne() {
  switch (etatVanne) {
    case VANNE_FERME:
      if (distanceCm > 0 && distanceCm < DIST_MIN) {
        demarrerOuverture();
      }
      break;

    case VANNE_OUVERTURE:
      if (etatCourant != ETAT_VANNE) {
        etatCourant = ETAT_VANNE;
      }
      if (myStepper.distanceToGo() == 0) {
        etatVanne = VANNE_OUVERT;
      }
      break;

    case VANNE_OUVERT:
      if (distanceCm > 0 && distanceCm > DIST_MAX) {
        demarrerFermeture();
      }
      break;

    case VANNE_FERMETURE:
      if (etatCourant != ETAT_VANNE) {
        etatCourant = ETAT_VANNE;
      }
      if (myStepper.distanceToGo() == 0) {
        etatVanne = VANNE_FERME;
      }
      break;

    case VANNE_ARRET:
      break;
  }
}

void demarrerOuverture() {
  myStepper.moveTo(VANNE_OUVERTE);
  etatVanne = VANNE_OUVERTURE;
}

void demarrerFermeture() {
  myStepper.moveTo(VANNE_FERMEE);
  etatVanne = VANNE_FERMETURE;
}

void onSimpleClic() {
  if (etatVanne == VANNE_OUVERTURE || etatVanne == VANNE_FERMETURE) {
    myStepper.stop();
    etatVanne = VANNE_ARRET;
    return;
  }

  if (etatVanne == VANNE_ARRET) {
    demarrerOuverture();
    return;
  }

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
    case ETAT_VANNE:
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
  if (val < lumMin) lumMin = val;
  return lumMin;
}

int setMaxPhotoR(int val) {
  if (val > lumMax) lumMax = val;
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
  Serial.print(humidite, 1);
  Serial.print(",Van:");
  Serial.println(vannePct);
}

void afficherLCD() {
  if (etatCourant == BOOT) return;

  bool refresh = (etatCourant != etatPrecedent);
  etatPrecedent = etatCourant;

  switch (etatCourant) {
    case LUM_DIST:
      if (refresh) lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Lum : ");
      lcd.print(lumPct);
      lcd.print("%   ");
      lcd.setCursor(0, 1);
      lcd.print("Dist : ");
      lcd.print(distanceCm, 1);
      lcd.print(" cm   ");
      break;

    case DHT_STATE:
      if (refresh) lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Temp : ");
      lcd.print(temperature, 1);
      lcd.print(" C   ");
      lcd.setCursor(0, 1);
      lcd.print("Hum : ");
      lcd.print(humidite, 1);
      lcd.print(" %   ");
      break;

    case CALIBRATION:
      lumRaw = analogRead(PIN_PHOTO);
      lumMin = setMinPhotoR(lumRaw);
      lumMax = setMaxPhotoR(lumRaw);

      if (refresh) lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Lum min : ");
      lcd.print(lumMin);
      lcd.print("   ");
      lcd.setCursor(0, 1);
      lcd.print("Lum max : ");
      lcd.print(lumMax);
      lcd.print("   ");
      break;

    case ETAT_VANNE: {
      if (refresh) lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Vanne : ");
      lcd.print(vannePct);
      lcd.print("%   ");
      lcd.setCursor(0, 1);
      lcd.print("Etat : ");
      switch (etatVanne) {
        case VANNE_FERME:     lcd.print("Ferme    "); break;
        case VANNE_OUVERTURE: lcd.print("Ouverture"); break;
        case VANNE_OUVERT:    lcd.print("Ouvert   "); break;
        case VANNE_FERMETURE: lcd.print("Fermeture"); break;
        case VANNE_ARRET:     lcd.print("Arret    "); break;
      }
      break;
    }

    default:
      break;
  }
}
