/*
 * Interrupt-Driven Button + PWM LED Fader
 * -----------------------------------------
 * Board   : Arduino Uno (ATmega328P) in Wokwi
 * Button  : D2  (INT0 - hardware external interrupt pin)
 * LED     : D9  (OC1A - Timer1 hardware PWM output)
 *
 * On each debounced press, the LED steps through
 * 0% -> 25% -> 50% -> 75% -> 100% -> 0% ... brightness
 * using PWM duty cycle (not on/off toggling).
 *
 * Design rules followed:
 *  - Button is handled by a hardware interrupt (attachInterrupt),
 *    never polled in loop().
 *  - The ISR is tiny: it only checks a timestamp and sets a
 *    volatile flag. All real work happens in loop().
 *  - Debouncing is done with a time-window check (40 ms) using
 *    millis(), which is safe to read inside an AVR ISR since it's
 *    just an integer compare, not a blocking call.
 *  - Brightness changes go through analogWrite(), which drives
 *    Timer1's hardware PWM comparator on pin 9 - true PWM, not
 *    manual bit-banged on/off.
 */

const uint8_t BUTTON_PIN = 2;   // INT0 on Uno
const uint8_t LED_PIN    = 9;   // Timer1 / OC1A PWM-capable pin

// Duty cycle steps as 8-bit PWM values (0-255 maps to 0-100%)
const uint8_t DUTY_STEPS[] = { 0, 64, 128, 191, 255 }; // 0,25,50,75,100%
const uint8_t NUM_STEPS    = sizeof(DUTY_STEPS) / sizeof(DUTY_STEPS[0]);

const unsigned long DEBOUNCE_MS = 40; // debounce window

// Shared state between ISR and loop() - MUST be volatile
volatile bool pressFlag = false;
volatile unsigned long lastInterruptTime = 0;

uint8_t stepIndex = 0;

// ---- Tiny ISR ----
void buttonISR() {
  unsigned long now = millis();
  if (now - lastInterruptTime >= DEBOUNCE_MS) {
    pressFlag = true;
    lastInterruptTime = now;
  }
  // else: bounce within debounce window, ignore silently
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // button ties pin to GND when pressed

  // Falling edge = button pressed (pin goes HIGH -> LOW because of pullup)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  analogWrite(LED_PIN, DUTY_STEPS[stepIndex]);
  Serial.println(F("Interrupt-driven PWM LED fader ready."));
  printBrightness();
}

void loop() {
  // Atomically read & clear the flag set by the ISR
  bool doStep = false;
  noInterrupts();
  if (pressFlag) {
    pressFlag = false;
    doStep = true;
  }
  interrupts();

  if (doStep) {
    stepIndex = (stepIndex + 1) % NUM_STEPS;
    analogWrite(LED_PIN, DUTY_STEPS[stepIndex]);
    printBrightness();
  }

  // loop() is free to do other work here - it is never blocked
  // waiting on the button, because input is interrupt-driven.
}

void printBrightness() {
  unsigned int pct = (DUTY_STEPS[stepIndex] * 100UL) / 255UL;
  Serial.print(F("Brightness: "));
  Serial.print(pct);
  Serial.println(F("%"));
}
