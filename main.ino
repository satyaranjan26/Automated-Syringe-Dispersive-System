#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Stepper motor pins
const int stepPin = 5;
const int dirPin = 4;

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad setup
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {13, 12, 10, 9};
byte colPins[COLS] = {8, 7, 6};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// State machine
enum State { WAIT_OFFSET, WAIT_MODE, WAIT_STEPS };
State currentState = WAIT_OFFSET;
bool directionFlag = LOW;  // LOW = PUSH, HIGH = PULL
String inputBuffer = "";
float offsetML = 0.0;

// Global invalid input flag
bool invalidInput = false;

// Forward declaration
int handleVolumeInput(char key, State context);

void setup() {
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);

  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Set Offset (ml):");
  lcd.setCursor(0, 1);
  lcd.print("Volume: __ ml");
  Serial.println("Set Offset:");
}

void loop() {
  char key = keypad.getKey();
  if (!key) return;

  if (key == '*') {
    inputBuffer = "";
    invalidInput = false;

    if (currentState == WAIT_OFFSET) {
      lcd.setCursor(0, 0);
      lcd.print("Set Offset (ml):");
      lcd.setCursor(0, 1);
      lcd.print("Volume: __ ml   ");
      return;
    }
    else if (currentState == WAIT_MODE || currentState == WAIT_STEPS) {
      currentState = WAIT_MODE;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("1: PUSH  2: PULL");
      lcd.setCursor(0, 1);
      lcd.print("Select mode");
      Serial.println("Back to mode selection");
      return;
    }
  }

  switch (currentState) {
    case WAIT_OFFSET: {
      int result = handleVolumeInput(key, WAIT_OFFSET);
      if (result == 1) {
        offsetML = (inputBuffer.length() == 1)
                    ? inputBuffer.toInt() / 10.0
                    : (String(inputBuffer[2]) + inputBuffer[1] + "." + inputBuffer[0]).toFloat();

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("1: PUSH  2: PULL");
        lcd.setCursor(0, 1);
        lcd.print("Select mode");

        Serial.print("Offset set to: ");
        Serial.print(offsetML);
        Serial.println(" ml");

        currentState = WAIT_MODE;
        inputBuffer = "";
      }
      break;
    }

    case WAIT_MODE:
      if (key == '1' || key == '2') {
        directionFlag = (key == '1') ? LOW : HIGH;

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Mode: ");
        lcd.print(key == '1' ? "PUSH" : "PULL");
        lcd.setCursor(0, 1);
        lcd.print("Volume: __ ml");

        Serial.print("Mode set to: ");
        Serial.println(key == '1' ? "PUSH" : "PULL");

        currentState = WAIT_STEPS;
        inputBuffer = "";
      }
      break;

    case WAIT_STEPS: {
      int result = handleVolumeInput(key, WAIT_STEPS);
      if (result == 1) {
        float enteredVolume = (inputBuffer.length() == 1)
                              ? inputBuffer.toInt() / 10.0
                              : (String(inputBuffer[2]) + inputBuffer[1] + "." + inputBuffer[0]).toFloat();

        if (directionFlag == LOW) { // PUSH
          if (enteredVolume > offsetML) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Invalid: Exceeds");
            lcd.setCursor(0, 1);
            lcd.print("offset * to retry");
            delay(1500);
            currentState = WAIT_MODE;
            inputBuffer = "";
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("1: PUSH  2: PULL");
            lcd.setCursor(0, 1);
            lcd.print("Select mode");
          } else {
            offsetML -= enteredVolume;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Pushing ");
            lcd.print(enteredVolume, 1);
            lcd.print(" ml");
            lcd.setCursor(0, 1);
            lcd.print("Please wait...");
            Serial.print("Pushing ");
            Serial.print(enteredVolume);
            Serial.println(" ml");
            moveSteps(enteredVolume, directionFlag);
            currentState = WAIT_MODE;
            inputBuffer = "";
            delay(1000);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("1: PUSH  2: PULL");
            lcd.setCursor(0, 1);
            lcd.print("Select mode");
          }
        } else { // PULL
          if (enteredVolume + offsetML > 20.0) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Invalid: Max 20ml");
            lcd.setCursor(0, 1);
            lcd.print("* to retry");
            delay(1500);
            currentState = WAIT_MODE;
            inputBuffer = "";
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("1: PUSH  2: PULL");
            lcd.setCursor(0, 1);
            lcd.print("Select mode");
          } else {
            offsetML += enteredVolume;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Pulling ");
            lcd.print(enteredVolume, 1);
            lcd.print(" ml");
            lcd.setCursor(0, 1);
            lcd.print("Please wait...");
            Serial.print("Pulling ");
            Serial.print(enteredVolume);
            Serial.println(" ml");
            moveSteps(enteredVolume, directionFlag);
            currentState = WAIT_MODE;
            inputBuffer = "";
            delay(1000);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("1: PUSH  2: PULL");
            lcd.setCursor(0, 1);
            lcd.print("Select mode");
          }
        }
      }
      break;
    }
  }
}

int handleVolumeInput(char key, State context) {
  if (invalidInput && key != '*') {
    return -1;  // ignore all keys except '*'
  }

  if (key >= '0' && key <= '9') {
    inputBuffer = key + inputBuffer;
    int len = inputBuffer.length();
    String displayVal;
    bool slate = true;

    if(len == 1) {
      displayVal = "0." + inputBuffer;
    } else if(len == 2) {
      displayVal = inputBuffer.substring(1) + "." + inputBuffer[0];
    } else if(len == 3) {
      displayVal = String(inputBuffer[2]) + String(inputBuffer[1]) + "." + inputBuffer[0];
    } else {
      slate = false;
    }

    lcd.setCursor(0, 1);
    if (slate) {
      lcd.print("Volume: ");
      lcd.print(displayVal);
      lcd.print(" ml   ");
    } else {
      lcd.print("Invalid Value   ");
      lcd.setCursor(0, 0);
      lcd.print("Press * to retry ");
      invalidInput = true;
    }
    return 0;
  }
  else if (key == '#') {
    if (inputBuffer.length() > 0 && !invalidInput) {
      return 1;
    }
  }

  return 0;
}

void moveSteps(float volume, bool direction) {
  int steps = volume * 325;
  digitalWrite(dirPin, direction);
  for (int i = 0; i < steps; i++) {
    char keyDuringMove = keypad.getKey();
    if (keyDuringMove == '0') {
      Serial.println("Aborted by user.");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ABORTED!");
      lcd.setCursor(0, 1);
      lcd.print("Returning...");
      delay(1000);

      currentState = WAIT_MODE;
      inputBuffer = "";

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("1:PUSH  2:PULL");
      lcd.setCursor(0, 1);
      lcd.print("Select mode");
      return;
    }
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(2000);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(2000);
  }
  delay(500);
}