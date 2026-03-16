int valMax = 255;
int valMin = 0;
int nbFois = 4;        
int tempsA = 250;     
int tempsB = 10;    
int compteur = 0;
int brightness = 0;

enum State {
  Clignotement,
  Variation,
  Etat
};

State state = Clignotement;

void etatcligno() {
  if (compteur < nbFois) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(tempsA);
    digitalWrite(LED_BUILTIN, LOW);
    delay(tempsA);
    compteur++;
  } else {
    compteur = 0;
    brightness = valMax;         
    state = Variation;
    Serial.println("Variation - 2349185");
  }
}

void etatvari() {
  analogWrite(LED_BUILTIN, brightness);
  delay(tempsB);

  brightness--;

  if (brightness <= valMin) {
    brightness = valMin;
    state = Etat;
    Serial.println("Allume - 2349185");
  }
}


void etatLumi() {
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);

  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);

  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);

  state = Clignotement;
  Serial.println("Clignotement - 2349185");
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  Serial.println("Clignotement - 2349185");
}


void loop() {
  switch (state) {
    case Clignotement:
      etatcligno();
      break;

    case Variation:
      etatvari();
      break;

    case Etat:
      etatLumi();
      break;
  }
}
