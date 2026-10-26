#include <SoftwareSerial.h>

SoftwareSerial BLE(2, 3);

const int trigPin = 9;
const int echoPin = 10;

const int ledPin = 13;
const int speakerPin = 8;
const int buttonPin = 4;

const int modeLeftPin = 5;
const int modeRightPin = 6;

enum State {
  UNCALIBRATED,
  CALIBRATED,
  ARMED,
  ALARM
};

State state = UNCALIBRATED;

int mode = 1;

float referenceDistance = -1;
float lastDistance = 999;

bool lastButtonState = HIGH;
unsigned long pressStart = 0;
bool holdTriggered = false;

int tapCount = 0;
unsigned long lastTapTime = 0;

int freq = 1500;
int dir = 1;

float getDistance() {

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) return 999;

  return duration * 0.0343 / 2.0;
}

float getAverageDistance() {

  float sum = 0;

  for (int i = 0; i < 10; i++) {
    sum += getDistance();
    delay(15);
  }

  return sum / 10.0;
}

void setup() {

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(ledPin, OUTPUT);
  pinMode(speakerPin, OUTPUT);

  pinMode(buttonPin, INPUT_PULLUP);

  pinMode(modeLeftPin, INPUT_PULLUP);
  pinMode(modeRightPin, INPUT_PULLUP);

  Serial.begin(9600);
  BLE.begin(9600);

  Serial.println("SYSTEM READY");
}

void updateMode() {

  bool left = digitalRead(modeLeftPin);
  bool right = digitalRead(modeRightPin);

  if (left == LOW) mode = 0;
  else if (right == LOW) mode = 2;
  else mode = 1;
}

void handleButton() {

  bool current = digitalRead(buttonPin);

  if (lastButtonState == HIGH && current == LOW) {
    pressStart = millis();
    holdTriggered = false;
  }

  if (current == LOW && !holdTriggered) {

    if (millis() - pressStart >= 2000) {

      holdTriggered = true;

      if (state == ALARM) {
        state = CALIBRATED;
        noTone(speakerPin);

        Serial.println("STOP HOLD");
        BLE.println("STOP");
      }
    }
  }

  if (lastButtonState == LOW && current == HIGH) {

    if (holdTriggered) {
      lastButtonState = current;
      return;
    }

    unsigned long held = millis() - pressStart;

    if (held < 400) {

      referenceDistance = getAverageDistance();

      if (referenceDistance > 5 && referenceDistance < 400) {
        state = CALIBRATED;

        Serial.print("CALIBRATED: ");
        Serial.println(referenceDistance);

        BLE.println("CAL");
      }
    }

    unsigned long now = millis();

    if (now - lastTapTime < 400) tapCount++;
    else tapCount = 1;

    lastTapTime = now;

    if (tapCount == 2) {

      if (state == CALIBRATED) {
        state = ARMED;
        Serial.println("ARMED");
        BLE.println("ARM");
      }
      else if (state == ARMED) {
        state = CALIBRATED;
        Serial.println("DISARMED");
        BLE.println("DISARM");
      }

      tapCount = 0;
    }
  }

  lastButtonState = current;
}

void updateState() {

  if (state == ARMED && referenceDistance > 5) {

    if (lastDistance < referenceDistance - 2.5) {

      state = ALARM;

      Serial.println("ALARM!");
      BLE.println("ALARM");
    }
  }
}

void updateSiren() {

  if (mode != 0) return;
  if (state != ALARM) return;

  tone(speakerPin, freq);

  freq += dir * 40;

  if (freq > 3000) dir = -1;
  if (freq < 1500) dir = 1;
}

void updateOutputs() {

  if (mode == 2) {

    digitalWrite(ledPin, millis() % 1000 < 50);
    noTone(speakerPin);

    BLE.println(lastDistance);
    return;
  }

  if (mode == 1) {

    noTone(speakerPin);

    if (state == UNCALIBRATED) {
      digitalWrite(ledPin, millis() % 800 < 400);
    }

    else if (state == CALIBRATED) {
      digitalWrite(ledPin, millis() % 500 < 120);
    }

    else if (state == ARMED) {
      digitalWrite(ledPin, LOW);
    }

    else if (state == ALARM) {
      digitalWrite(ledPin, millis() % 120 < 60);
    }

    return;
  }

  if (mode == 0) {

    if (state == UNCALIBRATED) {
      digitalWrite(ledPin, millis() % 800 < 400);
      noTone(speakerPin);
    }

    else if (state == CALIBRATED) {
      digitalWrite(ledPin, millis() % 600 < 120);
      noTone(speakerPin);
    }

    else if (state == ARMED) {
      digitalWrite(ledPin, LOW);
      noTone(speakerPin);
    }

    else if (state == ALARM) {
      digitalWrite(ledPin, millis() % 150 < 75);
    }

    return;
  }
}

void loop() {

  updateMode();

  lastDistance = getDistance();

  handleButton();
  updateState();
  updateOutputs();
  updateSiren();
}
