const int ledPins[] = {8, 9, 10, 11};
const int potentiometerPin = A1;
const int bouton = 2;

int potentiometerValue = 0;
int niveau = 0;

unsigned long lastTime = 0;
const unsigned long intervalle = 20;

const unsigned long nombre = 20;

const unsigned long potMax = 1023;
const unsigned long potMin = 20;
const unsigned long nbLed = 4;
const unsigned long nbIdx = 3;

unsigned long dernierAffichage = 0;
const unsigned long delaiAffichage = 200;

void setup() {
  Serial.begin(9600);

  for (int i = 0; i < nbLed; i++) {
    pinMode(ledPins[i], OUTPUT);
  }

  pinMode(bouton, INPUT_PULLUP);
}

int estClic(unsigned long ct) {
  static unsigned long lastTime = 0;
  static int lastState = HIGH;
  const int rate = 50;
  int clic = 0;

  if (ct - lastTime < rate) {
    return clic;
  }

  lastTime = ct;

  int state = digitalRead(bouton);

  if (state == LOW && lastState == HIGH) {
    clic = 1;
  }

  lastState = state;

  return clic;
}


void afficherLeds(int valeur) {

  int idx = map(valeur, 0, potMax, 0, nbIdx);

  for (int i = 0; i <= nbIdx; i++) {
    if (i <= idx) {
      digitalWrite(ledPins[i], HIGH);
    } else {
      digitalWrite(ledPins[i], LOW);
    }
  } 
}

void afficherBarre(int niveau) {
  int pourcentage = niveau * 5;

  Serial.print(pourcentage);
  Serial.print("%");
  Serial.print("[");

  for (int i = 0; i < nombre; i++) {
    if (i < niveau) {
      Serial.print(">");
    } else {
      Serial.print(".");
    }
  }

  Serial.println("]");
}

void loop() {
  if (millis() - lastTime >= intervalle) {
    lastTime = millis();

    potentiometerValue = analogRead(potentiometerPin);
    niveau = map(potentiometerValue, 0, potMax, 0, potMin);

    afficherLeds(potentiometerValue);
  }

  if (estClic(millis())) {
    if (millis() - dernierAffichage >= delaiAffichage) {
      afficherBarre(niveau);
      dernierAffichage = millis();
    }
  }
}
