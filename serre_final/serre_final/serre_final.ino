#include <LCD_I2C.h>
#include <DHT.h>
#include <OneButton.h>
#include <HCSR04.h>
#include <WiFiEspAT.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>

#include "Irrigation.h"
#include "Convoyeur.h"

#define PIN_PHOTO      A0
#define PIN_BOUTON      4
#define PIN_DHT         7
#define PIN_LED         9
#define PIN_TRIG       12
#define PIN_ECHO       11

#define IN_1  30
#define IN_2  32
#define IN_3  34
#define IN_4  36

#define PIN_CONV_MOT1  45
#define PIN_CONV_MOT2  44
#define PIN_JOY_X      A1
#define PIN_JOY_BTN     3
#define PIN_MAT_CLK    31
#define PIN_MAT_DIN    35
#define PIN_MAT_CS     33

#define DHT_TYPE          DHT11
#define SEUIL_LED         30
#define DELAI_BOOT      3000
#define INTERVAL_PHOTO  1000
#define INTERVAL_DHT    5000
#define INTERVAL_LCD     250
#define INTERVAL_SERIAL 3000
#define INTERVAL_CONV     50
#define INTERVAL_MQTT   5000
#define CLICK_TICKS      300
#define PRESS_TICKS     1000
#define LUM_INIT_MIN    1023
#define LUM_INIT_MAX       0
#define PCT_MIN            0
#define PCT_MAX          100
#define VANNE_FERMEE       0
#define VANNE_OUVERTE   2038
#define SERIAL_BAUD     9600
#define AT_BAUD_RATE  115200

#define ETD_NUM         "1"
#define MQTT_HOST       "arduino.nb.shawinigan.info"
#define MQTT_PORT       1883
#define MQTT_USER       "etdshawi"
#define MQTT_PASS       "shawi123"
#define MQTT_TOPIC_PUB  "etd/" ETD_NUM
#define MQTT_TOPIC_SUB  "etu_" ETD_NUM "/#"
#define MQTT_TOPIC_CONV "etu_" ETD_NUM "/convVit"
#define WIFI_SSID       "TechniquesInformatique-Etudiant"
#define WIFI_PASS       "shawi123"

LCD_I2C   lcd(0x27, 16, 2);
DHT       dht(PIN_DHT, DHT_TYPE);
OneButton bouton(PIN_BOUTON, true);
HCSR04    hc(PIN_TRIG, PIN_ECHO);

Irrigation irrigation(PIN_LED, IN_1, IN_2, IN_3, IN_4);
Convoyeur  convoyeur(PIN_CONV_MOT1, PIN_CONV_MOT2,
                     PIN_JOY_X, PIN_JOY_BTN,
                     PIN_MAT_CLK, PIN_MAT_DIN, PIN_MAT_CS);

WiFiClient  wifiClient;
MqttClient  mqttClient(wifiClient);

enum LCDState { BOOT, LUM_DIST, DHT_STATE, CALIBRATION, DATA_RECEIVED };
LCDState lcdState     = BOOT;
LCDState lcdPrecedent = BOOT;

int   lumRaw       = 0;
int   lumMin       = LUM_INIT_MIN;
int   lumMax       = LUM_INIT_MAX;
int   lumPct       = 0;
float temperature  = 0.0;
float humidite     = 0.0;
float distanceCm   = 0.0;
bool  btnClickFlag = false;

unsigned long tDernierePhoto  = 0;
unsigned long tDernierDHT     = 0;
unsigned long tDerniereSerial = 0;
unsigned long tDerniereLCD    = 0;
unsigned long tDernierConv    = 0;
unsigned long tDernierMqtt    = 0;
unsigned long tBoot           = 0;

void lirePhoto();
void lireDHT();
void lireDistance();
void majLED();
void envoyerSerial();
void afficherLCD();
void lcdLumDist(bool refresh);
void lcdDHT(bool refresh);
void lcdCalibration(bool refresh);
void lcdDataReceived(bool refresh);
int  mapperLuminosite(int val, int minVal, int maxVal);
int  setMinPhotoR(int val);
int  setMaxPhotoR(int val);
void onSimpleClic();
void onDoubleClic();
void connecterWifi();
void connecterMqtt();
void tacheMqtt();
void envoyerMqtt();

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial);

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Serre v1.0");
  lcd.setCursor(0, 1);
  lcd.print("etd:2349185");

  dht.begin();

  bouton.attachClick(onSimpleClic);
  bouton.attachDoubleClick(onDoubleClic);
  bouton.setClickTicks(CLICK_TICKS);
  bouton.setPressTicks(PRESS_TICKS);

  irrigation.setClosedOpenedPos(VANNE_FERMEE, VANNE_OUVERTE);
  irrigation.setDistance(distanceCm);
  irrigation.setBtnClickFlag(btnClickFlag);
  irrigation.begin();

  convoyeur.begin();

  Serial1.begin(AT_BAUD_RATE);
  WiFi.init(&Serial1);
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Module WiFi absent !");
  } else {
    connecterWifi();
    if (WiFi.status() == WL_CONNECTED) {
      connecterMqtt();
    }
  }

  tBoot = millis();
}

void loop() {
  unsigned long maintenant = millis();

  bouton.tick();

  if (lcdState == BOOT && maintenant - tBoot >= DELAI_BOOT) {
    lcdState = DHT_STATE;
    lcd.clear();
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

  irrigation.update();

  if (maintenant - tDernierConv >= INTERVAL_CONV) {
    convoyeur.update();
    tDernierConv = maintenant;
  }

  if (lcdState == DATA_RECEIVED) {
    static unsigned long tData = 0;
    if (tData == 0) tData = maintenant;
    if (maintenant - tData >= 2000) {
      tData    = 0;
      lcdState = (lcdPrecedent == DATA_RECEIVED) ? DHT_STATE : lcdPrecedent;
    }
  }

  if (maintenant - tDerniereSerial >= INTERVAL_SERIAL) {
    envoyerSerial();
    tDerniereSerial = maintenant;
  }

  tacheMqtt();
}

void lirePhoto() {
  lumRaw = analogRead(PIN_PHOTO);
  lumPct = mapperLuminosite(lumRaw, lumMin, lumMax);
  majLED();
}

void majLED() {
  digitalWrite(PIN_LED, (lumPct < SEUIL_LED) ? HIGH : LOW);
}

int mapperLuminosite(int val, int minVal, int maxVal) {
  if (maxVal == minVal) return PCT_MIN;
  int pct = (int)map(val, minVal, maxVal, PCT_MIN, PCT_MAX);
  if (pct < PCT_MIN) pct = PCT_MIN;
  if (pct > PCT_MAX) pct = PCT_MAX;
  return pct;
}

void lireDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidite    = h;
}

void lireDistance() {
  distanceCm = hc.dist();
}

void onSimpleClic() {
  switch (lcdState) {
    case LUM_DIST:      lcdState = DHT_STATE; break;
    case DHT_STATE:     lcdState = LUM_DIST;  break;
    case CALIBRATION:   lcdState = DHT_STATE; break;
    case DATA_RECEIVED: lcdState = DHT_STATE; break;
    default: break;
  }
}

void onDoubleClic() {
  if (lcdState == LUM_DIST || lcdState == DHT_STATE) {
    lumMin   = LUM_INIT_MIN;
    lumMax   = LUM_INIT_MAX;
    lcdState = CALIBRATION;
  }
}

void afficherLCD() {
  if (lcdState == BOOT) return;

  bool refresh = (lcdState != lcdPrecedent);
  lcdPrecedent = lcdState;

  switch (lcdState) {
    case LUM_DIST:      lcdLumDist(refresh);      break;
    case DHT_STATE:     lcdDHT(refresh);          break;
    case CALIBRATION:   lcdCalibration(refresh);  break;
    case DATA_RECEIVED: lcdDataReceived(refresh); break;
    default: break;
  }
}

void lcdLumDist(bool refresh) {
  if (refresh) lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Lum : ");
  lcd.print(lumPct);
  lcd.print("%   ");
  lcd.setCursor(0, 1);
  lcd.print("Dist: ");
  lcd.print(distanceCm, 1);
  lcd.print(" cm   ");
}

void lcdDHT(bool refresh) {
  if (refresh) lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperature, 1);
  lcd.print(" C   ");
  lcd.setCursor(0, 1);
  lcd.print("Hum : ");
  lcd.print(humidite, 1);
  lcd.print(" %   ");
}

void lcdCalibration(bool refresh) {
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
}

void lcdDataReceived(bool refresh) {
  if (refresh) lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MQTT recu!      ");
  lcd.setCursor(0, 1);
  lcd.print("Conv: ");
  lcd.print(convoyeur.getVitesse());
  lcd.print("      ");
}

int setMinPhotoR(int val) {
  if (val < lumMin) lumMin = val;
  return lumMin;
}

int setMaxPhotoR(int val) {
  if (val > lumMax) lumMax = val;
  return lumMax;
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
  Serial.print(irrigation.getPositionPct());
  Serial.print(",Conv:");
  Serial.println(convoyeur.getVitesse());
}

void connecterWifi() {
  WiFi.disconnect();
  WiFi.setPersistent();
  WiFi.endAP();
  Serial.print("Connexion WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK");
  } else {
    Serial.println(" ECHEC");
  }
}

void connecterMqtt() {
  mqttClient.setUsernamePassword(MQTT_USER, MQTT_PASS);
  Serial.print("Connexion MQTT...");
  if (mqttClient.connect(MQTT_HOST, MQTT_PORT)) {
    Serial.println(" OK");
    mqttClient.subscribe(MQTT_TOPIC_SUB);
    Serial.print("Abonne a : ");
    Serial.println(MQTT_TOPIC_SUB);
  } else {
    Serial.print(" ECHEC code=");
    Serial.println(mqttClient.connectError());
  }
}

void tacheMqtt() {
  if (!mqttClient.connected()) {
    if (WiFi.status() != WL_CONNECTED) connecterWifi();
    if (WiFi.status() == WL_CONNECTED)  connecterMqtt();
    return;
  }

  int messageSize = mqttClient.parseMessage();
  if (messageSize > 0) {
    String topic = mqttClient.messageTopic();
    String payload = "";
    while (mqttClient.available()) {
      payload += (char)mqttClient.read();
    }

    Serial.print("MQTT recu [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(payload);

    if (topic == MQTT_TOPIC_CONV) {
      if (convoyeur.estEnFonction()) {
        int vitesse = payload.toInt();
        if (vitesse < -100) vitesse = -100;
        if (vitesse >  100) vitesse = 100;
        convoyeur.setVitesseMqtt(vitesse);
      } else {
        Serial.println("Convoyeur inactif - commande MQTT ignoree");
      }
    }

    if (lcdState != DATA_RECEIVED) {
      lcdPrecedent = lcdState;
    }
    lcdState = DATA_RECEIVED;
  }

  unsigned long now = millis();
  if (now - tDernierMqtt >= INTERVAL_MQTT) {
    envoyerMqtt();
    tDernierMqtt = now;
  }
}

void envoyerMqtt() {
  StaticJsonDocument<256> doc;
  doc["name"]     = "2349185";
  doc["temp"]     = String(temperature, 2);
  doc["hum"]      = String(humidite,    2);
  doc["millis"]   = (unsigned long)millis();
  doc["lum"]      = (int)lumPct;
  doc["irrState"] = (int)irrigation.getCurrentState();
  doc["irrPos"]   = (int)irrigation.getPositionPct();
  doc["convVit"]  = (int)convoyeur.getVitesse();

  char buf[256];
  serializeJson(doc, buf);

  mqttClient.beginMessage(MQTT_TOPIC_PUB);
  mqttClient.print(buf);
  mqttClient.endMessage();

  Serial.print("MQTT pub: ");
  Serial.println(buf);
}
