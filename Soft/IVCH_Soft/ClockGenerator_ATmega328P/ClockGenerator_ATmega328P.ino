/*
 * Clock generator for a custom board, ATmega328P, TQFP-32.
 * External crystal: 16.384 MHz (16384 kHz), CPU prescaler: 1.
 * PD2 / Arduino D2 / package pin 32: 32768 Hz, 50% duty.
 * PD4 / Arduino D4 / package pin  2: 1 s HIGH, 1 s LOW (0.5 Hz).
 *
 * Timer1 interrupts at 65536 Hz. Both outputs start LOW.
 * Arduino timekeeping and all other interrupt sources are disabled.
 * See README_RU.md for Arduino IDE and programmer settings.
 */

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#if !defined(__AVR_ATmega328P__)
#error "Select ATmega328P (Arduino AVR Boards / Arduino Uno)."
#endif

// Use the actual crystal frequency, independent of the IDE's F_CPU default.
constexpr uint32_t CRYSTAL_HZ = 16384000UL;
constexpr uint32_t OUTPUT_HZ = 32768UL;
constexpr uint32_t TIMER_EVENTS_HZ = OUTPUT_HZ * 2UL;
constexpr uint32_t TIMER_COUNTS = CRYSTAL_HZ / TIMER_EVENTS_HZ;

static_assert(CRYSTAL_HZ % TIMER_EVENTS_HZ == 0,
              "The requested frequency must divide the crystal exactly.");
static_assert(TIMER_COUNTS >= 1 && TIMER_COUNTS <= 65536UL,
              "Timer1 compare value does not fit.");
static_assert(TIMER_EVENTS_HZ == 65536UL,
              "The 16-bit second divider requires 65536 events per second.");

static volatile uint16_t secondDivider = 0;

ISR(TIMER1_COMPA_vect)
{
  // Writing ONE to a PIN register toggles only that output bit.
  // Keep the fast edge before the divider's variable-length branch.
  PIND = _BV(PD2);

  // Unsigned 16-bit overflow occurs once per 65536 events = one second.
  if (++secondDivider == 0) {
    PIND = _BV(PD4);
  }
}

void setup()
{
  cli();

  MCUSR &= ~_BV(WDRF);
  wdt_disable();
  clock_prescale_set(clock_div_1);

  // Dedicated generator: avoid jitter caused by other interrupt handlers.
  TIMSK0 = 0;
  TIMSK1 = 0;
  TIMSK2 = 0;
  EIMSK = 0;
  PCICR = 0;
  UCSR0B = 0;
  SPCR &= ~_BV(SPIE);
  TWCR = 0;
  ADCSRA &= ~_BV(ADIE);
  ACSR &= ~_BV(ACIE);
  EECR &= ~_BV(EERIE);

  // Set the output latches LOW before enabling the drivers.
  PORTD &= ~(_BV(PD2) | _BV(PD4));
  DDRD |= _BV(DDD2) | _BV(DDD4);
  secondDivider = 0;

  TCCR1A = 0;                         // Normal GPIO, no OC1A/OC1B output.
  TCCR1B = 0;                         // Stop Timer1 while configuring.
  TCCR1C = 0;
  TCNT1 = 0;
  OCR1A = static_cast<uint16_t>(TIMER_COUNTS - 1UL); // 249.
  TIFR1 = _BV(ICF1) | _BV(OCF1B) | _BV(OCF1A) | _BV(TOV1);
  TIMSK1 = _BV(OCIE1A);

  // Idle sleep keeps Timer1 running and gives repeatable interrupt latency.
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  TCCR1B = _BV(WGM12) | _BV(CS10);    // CTC, CPU clock / 1.
  sei();
}

void loop()
{
  sleep_cpu();
}
