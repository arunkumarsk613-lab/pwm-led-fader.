# pwm-led-fader.
# Interrupt-Driven Button + PWM LED Fader

Arduino Uno firmware, built and simulated entirely in Wokwi (wokwi.com).
A hardware external interrupt reacts instantly to a button press; a hardware
Timer1 PWM channel fades an LED through five brightness steps
(0% -> 25% -> 50% -> 75% -> 100% -> back to 0%) on every debounced press.

## Circuit

| Component              | Arduino Uno pin | Notes                                              |
|-------------------------|------------------|------------------------------------------------------|
| Push button              | D2 (INT0)       | Other leg to GND, INPUT_PULLUP used, no external resistor needed |
| LED (+220 ohm resistor)  | D9 (OC1A)       | Timer1 hardware PWM output                          |

D2 is used because it's one of the Uno's two external-interrupt-capable pins
(D2 = INT0, D3 = INT1) -- required so attachInterrupt() works.
D9 is used because it's one of Timer1's PWM output pins (OC1A), so
analogWrite() produces real hardware PWM, not a software approximation.

## How the interrupt + debounce works

* attachInterrupt(digitalPinToInterrupt(2), buttonISR, FALLING) wires the
  button directly to the AVR's external interrupt hardware (INT0). The CPU
  is notified the instant the pin goes low -- no polling loop delay.
* The ISR (buttonISR) is kept intentionally tiny: it reads millis(),
  compares it against the last accepted press time, and if at least
  DEBOUNCE_MS (40 ms) has passed, sets a volatile bool pressFlag = true.
  That's it -- no delay(), no serial prints, no long logic inside the ISR.
* loop() reads and clears pressFlag inside a short noInterrupts() /
  interrupts() critical section (so the ISR can't modify it mid-read),
  then advances the brightness step and calls analogWrite().
* Because the flag and timestamp are shared between the ISR and loop(),
  both are declared volatile -- without this, the compiler could cache
  their values in a register and loop() would never see the ISR's update.

### Why a 40 ms debounce window?
Mechanical button bounce typically settles within 5-30 ms. A 40 ms
rejection window comfortably covers that without making the button feel
sluggish to a human, whose fastest deliberate button taps are still tens of
milliseconds apart.

## PWM timer maths

analogWrite() on pin 9 drives Timer1 in the ATmega328P using
Phase-Correct PWM mode with the default clock prescaler of 64 and an
8-bit counting range (0-255):

    PWM frequency = F_CPU / (Prescaler x 2 x TOP)
                  = 16,000,000 Hz / (64 x 2 x 256)
                  = 16,000,000 / 32,768
                  ~= 488.28 Hz

The factor of 2 appears because phase-correct mode counts the timer
up to TOP and back down every cycle (triangle wave), rather than
resetting straight back to 0 like fast PWM would. This is why Timer1/Timer0
default PWM on the Uno comes out to ~490 Hz instead of the ~980 Hz you'd get
from fast PWM with the same prescaler.

analogWrite(LED_PIN, value) sets Timer1's OCR1A compare register to
value (0-255). The duty cycle is simply:

    Duty % = (OCR1A / 255) x 100

So the five brightness steps used in firmware map to:

| Step | OCR1A value | Duty cycle |
|------|-------------|------------|
| 0    | 0           | 0%         |
| 1    | 64          | ~25%       |
| 2    | 128         | ~50%       |
| 3    | 191         | ~75%       |
| 4    | 255         | 100%       |

### Why ~490 Hz is a fine choice here
~490 Hz is far above the ~50-100 Hz flicker-fusion threshold of the human
eye, so the LED reads as a smooth, flicker-free brightness rather than a
visibly blinking light -- while still being low enough that
analogWrite()'s default configuration (no manual register setup) is
sufficient. No manual timer/prescaler configuration was needed for this
project; if a different frequency were required (e.g. to avoid audible
whine when driving a motor, or to dodge camera rolling-shutter beat
patterns), it would be changed by writing directly to TCCR1B's CS1x
prescaler bits.

## Interrupts, ISR priority, and the AVR equivalent of NVIC

On ARM Cortex-M parts (STM32, RP2040, etc.) interrupt routing and priority
are managed by the NVIC (Nested Vectored Interrupt Controller), which
lets you assign priority levels and enable/disable nesting per interrupt.
The ATmega328P (classic Arduino Uno) doesn't have an NVIC -- instead it has a
simpler, fixed-priority interrupt vector table: each interrupt source
(INT0, INT1, Timer1 overflow/compare, etc.) has one fixed vector slot, and
priority is determined purely by vector table position (INT0 outranks INT1,
which outranks timer interrupts, etc.), with no runtime-configurable
priority levels. attachInterrupt() registers our handler into that
existing INT0 vector. The behavior this project relies on -- instant,
hardware-triggered reaction to the button edge -- is exactly the same
concept NVIC provides on ARM parts, just without configurable priority
levels.

If you re-target this project to an NVIC-based board in Wokwi (e.g. a
Raspberry Pi Pico or STM32 "Blue Pill"), the same attachInterrupt() /
analogWrite() Arduino API calls remain valid -- the framework configures
the NVIC and PWM peripheral for you under the hood.

## Files

* sketch.ino -- firmware
* diagram.json -- Wokwi circuit definition (button on D2, LED+resistor on D9)

## Running it
 https://wokwi.com/projects/469773456500557825
See the main project instructions for step-by-step simulator setup and
how to generate a shareable Wokwi link.
