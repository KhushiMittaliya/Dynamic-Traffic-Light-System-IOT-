#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD Setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

// IR Sensor Pins (1 per lane)
const int irSensors[4] = {
  D0, // Lane 1
  D8, // Lane 2
  A0, // Lane 3
  D1  // Lane 4
};

// Shift Register Pins
const int dataPin = D5;   // GPIO14
const int latchPin = D6;  // GPIO12
const int clockPin = D7;  // GPIO13

// LED Bit Mapping per Lane: RED, YELLOW, GREEN
const int RED[]    = {0, 3, 8, 11};   // Q0, Q3, Q8, Q11
const int YELLOW[] = {1, 4, 9, 12};
const int GREEN[]  = {2, 5, 10, 13};

// Shift register state
byte ledState[2] = {0, 0};

// Emergency control
bool emergency = false;
int emergencyLane = 0;
int lastLane = -1; // Track last normal lane before emergency

void setup() {
  Serial.begin(9600);
  delay(500);

  // IR Sensor Pins
  for (int i = 0; i < 4; i++) {
    pinMode(irSensors[i], INPUT);
  }

  // Shift Register Pins
  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);

  // LCD Init
  Wire.begin(D3, D4); 
  lcd.begin(16, 2); // Required for I2C on ESP8266
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(" Dynamic Traffic ");
  lcd.setCursor(0, 1);
  lcd.print("   Light System  ");
  delay(2000);
  lcd.clear();

  setAllRed();
}

void loop() {
  checkEmergency();

  static int lane = 0;

  if (emergency) {
    handleEmergency(emergencyLane);
    lane = (lastLane + 1) % 4; // Resume from next lane
    emergency = false;
  } else {
    int duration = getGreenTime(lane);

    setLaneGreen(lane);
    lastLane = lane;

    if (showCountdownWithInterrupt(lane + 1, duration)) {
      handleEmergency(emergencyLane);
      lane = (lastLane + 1) % 4; // Resume from next lane
      emergency = false;
      return;
    }

    setLaneYellow(lane);
    delay(2000);

    setAllRed();
    delay(1000);

    lane = (lane + 1) % 4;
  }
}

// ================= Shift Register Control =================

void shiftOutData() {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, ledState[1]); // High byte
  shiftOut(dataPin, clockPin, MSBFIRST, ledState[0]); // Low byte
  digitalWrite(latchPin, HIGH);
}

void clearLEDs() {
  ledState[0] = 0;
  ledState[1] = 0;
}

void setLED(int bit, bool state) {
  if (bit < 8) {
    bitWrite(ledState[0], bit, state);
  } else {
    bitWrite(ledState[1], bit - 8, state);
  }
}

void setAllRed() {
  clearLEDs();
  for (int i = 0; i < 4; i++) {
    setLED(RED[i], true);
  }
  shiftOutData();
}

void setLaneGreen(int lane) {
  clearLEDs();
  setLED(GREEN[lane], true);
  for (int i = 0; i < 4; i++) {
    if (i != lane) {
      setLED(RED[i], true);
    }
  }
  shiftOutData();
}

void setLaneYellow(int lane) {
  clearLEDs();
  setLED(YELLOW[lane], true);
  for (int i = 0; i < 4; i++) {
    if (i != lane) {
      setLED(RED[i], true);
    }
  }
  shiftOutData();
}

// ================= IR-Based Smart Timer =================

int getGreenTime(int lane) {
  int sensor = digitalRead(irSensors[lane]);
  if (sensor == LOW) {
    return 15;  // Traffic detected
  }
  return 10;     // No traffic
}

// ================= Emergency Detection =================

void checkEmergency() {
  if (Serial.available()) {
    char input = Serial.read();
    if (input >= '1' && input <= '4') {
      emergency = true;
      emergencyLane = input - '1';
      Serial.print("Emergency at Lane ");
      Serial.println(emergencyLane + 1);
    }
  }
}

void handleEmergency(int lane) {
  // Set only the emergency lane to green, all others red
  setLaneGreen(lane);

  // Show emergency message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EMERGENCY LANE ");
  lcd.setCursor(0, 1);
  lcd.print("L");
  lcd.print(lane + 1);
  lcd.print(" GREEN 5 sec");
  delay(1500);

  for (int i = 5; i > 0; i--) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EMERGENCY LANE ");
    lcd.setCursor(0, 1);
    lcd.print("L");
    lcd.print(lane + 1);
    lcd.print(" Time: ");
    lcd.print(i);
    lcd.print("s ");
    delay(1000);
  }

  setLaneYellow(lane);
  delay(2000);

  setAllRed();
  delay(1000);
}

// ================= Countdown with Interrupt =================

bool showCountdownWithInterrupt(int lane, int seconds) {
  lcd.clear();
  for (int i = seconds; i > 0; i--) {
    lcd.setCursor(0, 0);
    lcd.print("Lane ");
    lcd.print(lane);
    lcd.print(" GREEN     ");

    lcd.setCursor(0, 1);
    lcd.print("Time Left: ");
    lcd.print(i);
    lcd.print("s   ");

    delay(1000);
    checkEmergency();
    if (emergency) return true;
  }
  return false;
}
