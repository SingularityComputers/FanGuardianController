#include <Arduino.h>

#define NUMBER_OF_FANS 4
#define LED_PIN 13
#define FLASH_DURATION 2000
#define FLASH_INTERVAL 100

unsigned long previousMillis = 0;
bool ledState = LOW;  // initial LED state is off
bool flashInProgress = false;

const byte bufferSize = 32;
char serialBuffer[bufferSize];
boolean newData = false;

const int hallSensorPins[] = {4, 5, 2, 3}; 
volatile unsigned long lastTime[NUMBER_OF_FANS] = {0};
volatile unsigned long fanRPM[NUMBER_OF_FANS] = {0};
volatile boolean fanInterruptFlag[NUMBER_OF_FANS] = {false};

const int NUM_SAMPLES = 10; // Number of samples to consider for moving average
unsigned long rpmBuffer[NUMBER_OF_FANS][NUM_SAMPLES] = {0}; // Buffer to store recent RPM values
int rpmBufferIndex[NUMBER_OF_FANS] = {0}; // Index to track the current position in the buffer

void fanInterrupt(int fanIndex);
void flashLED();
void checkSerialData();
void processSerialCommand();
void checkFanInterrupts();

unsigned long currentTime = 0;
unsigned long period = 0;
unsigned long rpm = 0;
void fanInterrupt(int fanIndex) {
  fanInterruptFlag[fanIndex] = true;
  currentTime = micros();
  period = currentTime - lastTime[fanIndex];
  rpm = 60e6 / (period * 2);
  
  // Update the RPM buffer with the new value
  rpmBuffer[fanIndex][rpmBufferIndex[fanIndex]] = rpm;
  rpmBufferIndex[fanIndex] = (rpmBufferIndex[fanIndex] + 1) % NUM_SAMPLES;

  // Calculate the moving average of RPM values
  unsigned long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += rpmBuffer[fanIndex][i];
  }
  unsigned long movingAverageRPM = sum / NUM_SAMPLES;

  // Update the RPM value only if it's within a certain percentage of the moving average
  unsigned long threshold = movingAverageRPM * 1.5; // 20% higher than the moving average
  if (rpm <= threshold) {
    fanRPM[fanIndex] = rpm;
  }
    
  lastTime[fanIndex] = currentTime;
}

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);   // Initialize hardware UART on pins 6 (TX) and 7 (RX)
  delay(200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("DEBUG: pb-controller started.");

  // Initialize interrupt pins for Hall sensors
  for (int i = 0; i < 4; i++) {
    pinMode(hallSensorPins[i], INPUT_PULLUP);
  }

  attachInterrupt(digitalPinToInterrupt(hallSensorPins[0]),[] {  fanInterrupt(0); }, FALLING);
  attachInterrupt(digitalPinToInterrupt(hallSensorPins[1]),[] {  fanInterrupt(1); }, FALLING);
  attachInterrupt(digitalPinToInterrupt(hallSensorPins[2]),[] {  fanInterrupt(2); }, FALLING);
  attachInterrupt(digitalPinToInterrupt(hallSensorPins[3]),[] {  fanInterrupt(3); }, FALLING);
}

void loop() {
  delay(100);
  checkSerialData();
  processSerialCommand();
  checkFanInterrupts();
  flashLED();
}

void flashLED() {
  unsigned long currentMillis = millis(); // Get the current time

  if (!flashInProgress) {
    // If a flash is not in progress, check if it's time to start a new flash
    if (currentMillis - previousMillis >= FLASH_INTERVAL) {
      previousMillis = currentMillis; // Update the previous time
      flashInProgress = true; // Start a new flash
      ledState = true; // Turn on the LED
      digitalWrite(LED_PIN, ledState); // Update the LED
    }
  } else {
    // If a flash is in progress, check if it's time to end the flash
    if (currentMillis - previousMillis >= FLASH_DURATION) {
      previousMillis = currentMillis; // Update the previous time
      flashInProgress = false; // End the flash
      ledState = false; // Turn off the LED
      digitalWrite(LED_PIN, ledState); // Update the LED
    }
  }
}

void checkSerialData() {
  static byte ndx = 0;
  char rc;
  while (Serial1.available() > 0 && newData == false) {
      rc = Serial1.read();
      if (rc == '\n' || rc == '\r') {
          serialBuffer[ndx] = '\0';
          ndx = 0;
          newData = true;
      }
      else {
          serialBuffer[ndx] = rc;
          ndx++;
          if (ndx >= bufferSize) {
              ndx = bufferSize - 1;
          }
      }
  }
}

void processSerialCommand() {
  if (newData == true) {
    if (strcmp(serialBuffer, "#RF\0") == 0) {
      // Send back the response: #RF,0,0,0,0
      Serial1.printf("#RF,%d,%d,%d,%d\n", fanRPM[0], fanRPM[1], fanRPM[2], fanRPM[3]);
      Serial.printf("DEBUG: #RF,%d,%d,%d,%d\n", fanRPM[0], fanRPM[1], fanRPM[2], fanRPM[3]);
    }
    newData = false;
    serialBuffer[0] = '\0';
  }
}

void checkFanInterrupts() {
  unsigned long currentTime = millis();

  for (int i = 0; i < NUMBER_OF_FANS; i++) {
    if (!fanInterruptFlag[i] && (currentTime - lastTime[i] > 500)) {
      fanRPM[i] = 0;
    }
    fanInterruptFlag[i] = false;  // Reset the interrupt flag
  }
}
