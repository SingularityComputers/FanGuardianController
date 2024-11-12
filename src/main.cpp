#include <Arduino.h>

#define NUMBER_OF_FANS 4
#define LED_PIN 13
#define LED_FLASH_DURATION 2000
#define LED_FLASH_INTERVAL 100
#define MAIN_CLOCK_FREQUENCY_HZ 48000000
#define PWM_CLOCK_FREQUENCY_HZ 30000

unsigned long previousMillis = 0;
bool ledState = LOW;  // initial LED state is off
bool flashInProgress = false;

const byte bufferSize = 32;
char serialBuffer[bufferSize];
boolean newData = false;

const int NUM_SAMPLES = 10; // number of samples to consider for moving average
const int hallSensorPins[] = {4, 5, 2, 3}; 
volatile unsigned long lastTime[NUMBER_OF_FANS] = {0};
volatile unsigned long fanRPM[NUMBER_OF_FANS] = {0};
volatile boolean fanInterruptFlag[NUMBER_OF_FANS] = {false};
unsigned long fanInterruptCurrentTime = 0;
unsigned long fanInterruptPeriod = 0;
unsigned long rpmBuffer[NUMBER_OF_FANS][NUM_SAMPLES] = {0}; // buffer to store recent RPM values
int rpmBufferIndex[NUMBER_OF_FANS] = {0}; // index to track the current position in the buffer

// pwmCounterPeriod is the maximum value the timer counts to before it resets to zero
// in our case this causes the timer to reset every 1600 clock cycles (counting from 0 to 1599)
// The frequency can be calculated by:
//    freq = GCLK4_freq / (TCC0_prescaler * (1 + TOP_value))
// where,
//    freq = 30000Hz (30kHz) - the brushless PWM FANs operates silent on this frequency
//    GCLK4_freq = 48000000 (48MHz) - the main clock source
//    TCC0_prescaler = 1 - no need to further divide the main clock
uint32_t pwmCounterPeriod = (MAIN_CLOCK_FREQUENCY_HZ / PWM_CLOCK_FREQUENCY_HZ) - 1;

// function headers
void fanInterrupt(int fanIndex);
void flashLED();
void checkSerialData();
void processSerialCommand();
void checkFanInterrupts();
void configurePWMChannels();
void configureRPMDetectPins();
void setDutyCycle(uint8_t channel, float dutyCyclePercent);

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);   // Initialize hardware UART on pins 6 (TX) and 7 (RX)
  delay(200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("DEBUG: pb-controller started.");
  configurePWMChannels();
  configureRPMDetectPins();
}

void loop() {
  delay(100);
  checkSerialData();
  processSerialCommand();
  checkFanInterrupts();
  flashLED();
}

// fanInterrupt is the callback function to run when an interrupt happens
void fanInterrupt(int fanIndex) {
  unsigned long rpm = 0;
  fanInterruptFlag[fanIndex] = true;
  fanInterruptCurrentTime = micros();
  fanInterruptPeriod = fanInterruptCurrentTime - lastTime[fanIndex];
  rpm = 60e6 / (fanInterruptPeriod * 2);
  
  // update the RPM buffer with the new value
  rpmBuffer[fanIndex][rpmBufferIndex[fanIndex]] = rpm;
  rpmBufferIndex[fanIndex] = (rpmBufferIndex[fanIndex] + 1) % NUM_SAMPLES;

  // calculate the moving average of RPM values
  unsigned long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += rpmBuffer[fanIndex][i];
  }
  unsigned long movingAverageRPM = sum / NUM_SAMPLES;

  // update the RPM value only if it's within a certain percentage of the moving average
  unsigned long threshold = movingAverageRPM * 1.5; // 20% higher than the moving average
  if (rpm <= threshold) {
    fanRPM[fanIndex] = rpm;
  }
    
  lastTime[fanIndex] = fanInterruptCurrentTime;
}

// configureRPMDetectPins configures interrupt pins for Hall sensors
void configureRPMDetectPins() {
  for (int i = 0; i < 4; i++) {
    pinMode(hallSensorPins[i], INPUT_PULLUP);
  }

  attachInterrupt(digitalPinToInterrupt(hallSensorPins[0]),[] {  fanInterrupt(0); }, FALLING);
  attachInterrupt(digitalPinToInterrupt(hallSensorPins[1]),[] {  fanInterrupt(1); }, FALLING);
  attachInterrupt(digitalPinToInterrupt(hallSensorPins[2]),[] {  fanInterrupt(2); }, FALLING);
  attachInterrupt(digitalPinToInterrupt(hallSensorPins[3]),[] {  fanInterrupt(3); }, FALLING);
}

// flashLED just flashes the heartbeat LED
void flashLED() {
  unsigned long currentMillis = millis(); // Get the current time

  if (!flashInProgress) {
    // if a flash is not in progress, check if it's time to start a new flash
    if (currentMillis - previousMillis >= LED_FLASH_INTERVAL) {
      previousMillis = currentMillis; // update the previous time
      flashInProgress = true; // start a new flash
      ledState = true; // turn on the LED
      digitalWrite(LED_PIN, ledState); // update the LED
    }
  } else {
    // if a flash is in progress, check if it's time to end the flash
    if (currentMillis - previousMillis >= LED_FLASH_DURATION) {
      previousMillis = currentMillis; // update the previous time
      flashInProgress = false; // end the flash
      ledState = false; // turn off the LED
      digitalWrite(LED_PIN, ledState); // update the LED
    }
  }
}

// checkSerialData looks for incoming serial data in the buffer
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

// processSerialCommand processes the incoming serial instructions
void processSerialCommand() {
  if (newData == true) {
    // parse #RF (read fan) commands
    if (strcmp(serialBuffer, "#RF\0") == 0) {
      // Send back the response: #RF,0,0,0,0
      Serial1.printf("#RF,%d,%d,%d,%d\n", fanRPM[0], fanRPM[1], fanRPM[2], fanRPM[3]);
      Serial.printf("DEBUG: #RF,%d,%d,%d,%d\n", fanRPM[0], fanRPM[1], fanRPM[2], fanRPM[3]);
    } else if (strncmp(serialBuffer, "#WPWM,", 5) == 0) {
      // Parse #WPWM (write PWM) commands, format: #WPWM,<channel>,<dutyCycle>
      char *token = strtok(serialBuffer + 5, ",");
      if (token != NULL) {
        int channel = atoi(token);
        token = strtok(NULL, ",");
        if (token != NULL) {
          int dutyCyclePercent = atoi(token);
          if ((channel == 0 || channel == 1 || channel == 2 || channel == 3) &&
              (dutyCyclePercent >= 0 && dutyCyclePercent <= 100)) {
            float dutyCycle = dutyCyclePercent / 100.0;
            setDutyCycle(channel, dutyCycle);
            Serial.printf("DEBUG: Duty cycle set: Channel %d to %d%%\n", channel, dutyCyclePercent);
          }
        }
      }
    }
    newData = false;
    serialBuffer[0] = '\0';
  }
}

// checkFanInterrupts checks if fan interrupt has triggered
void checkFanInterrupts() {
  unsigned long currentTime = millis();

  for (int i = 0; i < NUMBER_OF_FANS; i++) {
    if (!fanInterruptFlag[i] && (currentTime - lastTime[i] > 500)) {
      fanRPM[i] = 0;
    }
    fanInterruptFlag[i] = false;  // reset the interrupt flag
  }
}

// configurePWMChannels configures the hardware PWM generator
void configurePWMChannels() {
  // we are using TCC0, limit pwmCounterPeriod to 24 bits
  // guide:
  //    AT07058: SAM D10/D11/D21/DA1/R/L/C Timer
  //    Counter for Control Applications (TCC) Driver, section 3.2.1
  pwmCounterPeriod = ( pwmCounterPeriod < 0x00ffffff ) ? pwmCounterPeriod : 0x00ffffff;

  // enable and configure generic clock generator 4
  GCLK->GENCTRL.reg = GCLK_GENCTRL_IDC |          // improve duty cycle
                      GCLK_GENCTRL_GENEN |        // enable generic clock gen
                      GCLK_GENCTRL_SRC_DFLL48M |  // select 48MHz as source
                      GCLK_GENCTRL_ID(4);         // select GCLK4
  while (GCLK->STATUS.bit.SYNCBUSY);              // wait for synchronization

  // set clock divider of 1 to generic clock generator 4
  GCLK->GENDIV.reg = GCLK_GENDIV_DIV(1) |         // divide 48 MHz by 1
                     GCLK_GENDIV_ID(4);           // apply to GCLK4
  while (GCLK->STATUS.bit.SYNCBUSY);              // wait for synchronization
  
  // enable GCLK4 and connect it to TCC0 and TCC1
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN |        // enable generic clock generator
                      GCLK_CLKCTRL_GEN_GCLK4 |    // select GCLK4
                      GCLK_CLKCTRL_ID_TCC0_TCC1;  // feed GCLK4 to TCC0/1
  while (GCLK->STATUS.bit.SYNCBUSY);              // wait for synchronization

  // divide counter by 1 giving 48 MHz (20.83 ns) on each TCC0 tick
  TCC0->CTRLA.reg |= TCC_CTRLA_PRESCALER(TCC_CTRLA_PRESCALER_DIV1_Val);

  // use "Normal PWM" (single-slope PWM): count up to PER, match on CC[n]
  TCC0->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;         // select NPWM as waveform
  while (TCC0->SYNCBUSY.bit.WAVE);                // wait for synchronization

  // Set the pwmCounterPeriod
  TCC0->PER.reg = pwmCounterPeriod;
  while (TCC0->SYNCBUSY.bit.PER);

  // set PWM signal to output 50% duty cycle for initial value
  // n for CC[n] is determined by n = x % 4 where x is from WO[x]
  // PA12 is TCC0/ WO[6]
  // PA13 is TCC0/ WO[7]
  // guide:
  //    SMART ARM-Based Microcontroller, DATASHEET, Table 6-1
  TCC0->CC[2].reg = pwmCounterPeriod * 0.5;
  while (TCC0->SYNCBUSY.bit.CC2);

  TCC0->CC[3].reg = pwmCounterPeriod * 0.5;
  while (TCC0->SYNCBUSY.bit.CC3);

  // configure PA12/PA13 pins to be output
  PORT->Group[PORTA].DIRSET.reg = PORT_PA12;      // set pin as output
  PORT->Group[PORTA].OUTCLR.reg = PORT_PA12;      // set pin to low

  PORT->Group[PORTA].DIRSET.reg = PORT_PA13;      // set pin as output
  PORT->Group[PORTA].OUTCLR.reg = PORT_PA13;      // set pin to low

  // enable the port multiplexer for PA12/PA13
  PORT->Group[PORTA].PINCFG[12].reg |= PORT_PINCFG_PMUXEN;
  PORT->Group[PORTA].PINCFG[13].reg |= PORT_PINCFG_PMUXEN;

  // connect TCC0 timer to PA12/PA13. Function F is TCC.
  //    odd pin num (2*n + 1): use PMUXO
  //    even pin num (2*n): use PMUXE
  PORT->Group[PORTA].PMUX[6].reg |= PORT_PMUX_PMUXE_F;
  PORT->Group[PORTA].PMUX[6].reg |= PORT_PMUX_PMUXO_F;

  // enable output (start the PWM)
  TCC0->CTRLA.reg |= (TCC_CTRLA_ENABLE);
  while (TCC0->SYNCBUSY.bit.ENABLE);      // wait for synchronization
}

// setDutyCycle sets PWM channels duty cycle in percent (ie: 0.5)
void setDutyCycle(uint8_t channel, float dutyCyclePercent) {
  uint32_t dutyCycleValue = (uint32_t)(pwmCounterPeriod * dutyCyclePercent);

  if (channel == 2) {
    TCC0->CC[2].reg = dutyCycleValue;
    while (TCC0->SYNCBUSY.bit.CC2);
  } else if (channel == 3) {
    TCC0->CC[3].reg = dutyCycleValue;
    while (TCC0->SYNCBUSY.bit.CC3);
  }
}
