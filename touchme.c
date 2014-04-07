#include <stdint.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "macros.h"
#include "tlc5940.h"

#define DEBOUNCE_TIME 1000

#define INPUT_DDR   DDRC
#define INPUT_PORT  PORTC
#define INPUT_PIN   PINC

#define THUMB     PC5
#define POINTER   PC4
#define MIDDLE    PC3
#define RING      PC2
#define PINKY     PC1
#define PALM      PC0

#define THUMB_BIT     5
#define POINTER_BIT   4
#define MIDDLE_BIT    3
#define RING_BIT      2
#define PINKY_BIT     1
#define PALM_BIT      0

// Makefile params
#ifndef MIN_RATE
#define MIN_RATE 0
#endif
#ifndef MAX_RATE
#define MAX_RATE 100
#endif
#ifndef MAX_LIT
#define MAX_LIT 60
#endif
#ifndef MAX_LIT_DEMO
#define MAX_LIT_DEMO 45
#endif
#ifndef BASE_INTENSITY
#define BASE_INTENSITY 500
#endif
#ifndef MAX_INTENSITY
#define MAX_INTENSITY 4000
#endif

// MODES
typedef enum {
  DEMO,
  PRODUCTION,
  COMMAND
} mode_t;

volatile mode_t current_mode = PRODUCTION;
volatile mode_t previous_mode = PRODUCTION;
volatile uint8_t glove_status = 0;
volatile uint8_t glove_count = 0;
volatile uint8_t need_to_blink = 0;
uint8_t blink_status = 0;

ISR(TIMER2_OVF_vect){
  // if the index finger is grounded
  if(!(INPUT_PIN & _BV(POINTER)) && !(INPUT_PIN & _BV(RING))){
    // if the index finger wasn't already grounded
    if(!(glove_status & _BV(POINTER_BIT)) && !(glove_status & _BV(RING_BIT))){
      glove_status |= _BV(POINTER_BIT);
      glove_status |= _BV(RING_BIT);

      glove_count++;

      if (glove_count > 100) {
        switch(current_mode){
          case  DEMO:
            previous_mode = DEMO;
            current_mode = PRODUCTION;
            break;
          case  PRODUCTION:
            previous_mode = PRODUCTION;
            current_mode = DEMO; 
            break;
        }
        need_to_blink = 1;
        TLC5940_SetAllGS(4095);
        glove_count = 0;
      }
    }
  }
  // if the index finger isn't grounded
  else{
    glove_status &= ~_BV(POINTER_BIT);
    glove_status &= ~_BV(RING_BIT);
    glove_count = 0;
  }
}

// INPUT GLOBALS
uint8_t inputPins[6] = { THUMB, POINTER, MIDDLE, RING, PINKY, PALM };
uint8_t previousInputStatus[6] = { 0, 0, 0, 0, 0, 0 };
uint8_t currentInputStatus[6] = { 0, 0, 0, 0, 0, 0 };
uint16_t glove_held[6] = {0};

// DEMO MODE GLOBALS
uint16_t base_intensity_d = BASE_INTENSITY;
uint16_t max_intensity_d = 4000;
uint8_t number_d = 96;

// COMMAND MODE GLOBALS
uint8_t command_mode_pattern[2] = { MIDDLE, RING };
uint8_t mode_switch_pattern[4] = { POINTER, POINTER, MIDDLE, PINKY };

// Display behaviors
uint8_t active_leds[100] = {FALSE};
int16_t fade_value[100] = {0};
channel_t currently_lit = 0;
channel_t current_max_lit = 45;

inline uint8_t randomBetween(uint8_t min, uint8_t max) {
  return (min + (rand() % (max - min)));
}

void bubblePop(channel_t led) {
  if (active_leds[led]) {
    if (fade_value[led] == -2) {
      // hit ceiling time to flash
      fade_value[led] = -1;
      TLC5940_SetGS(led, 4095);
      return;
    } else if (fade_value[led] == -1) {
      // finished flashing turn off and deactivate.
      active_leds[led] = FALSE;
      fade_value[led] = 0;
      currently_lit--;
      TLC5940_SetGS(led, 0);
      return;
    }

    fade_value[led] += randomBetween(MIN_RATE, MAX_RATE);
    if (fade_value[led] >= max_intensity_d) {
      fade_value[led] = -2;
      TLC5940_SetGS(led, 0);
      return;
    }
    TLC5940_SetGS(led, fade_value[led]);
    return;
  }

  if ((currently_lit < current_max_lit && current_mode == PRODUCTION) || (currently_lit < MAX_LIT_DEMO && current_mode == DEMO)) {
    active_leds[led] = TRUE;
    currently_lit++;
  }
}

void initialize(void) {

  // wait for power to stabilize
  _delay_ms(200);

  // Initialize the input pins
  for(uint8_t i = 0; i < 6; i++) {
    setInput(INPUT_DDR, inputPins[i]); // Set the pins to input
    setHigh(INPUT_PORT, inputPins[i]); // Turn on the pullup resistor
  }

  // Initialize the TLC drivers.
  TLC5940_Init();
  TLC5940_SetAllGS(0);

  // set up timer 2
  // prescale clock by 256 //1024
  TCCR2B |= _BV(CS22) | _BV(CS21) | _BV(CS20);
  // enable interrupt at timer overflow
  TIMSK2 |= _BV(TOIE2);

  sei();
}

void commandIteration(void) {
  // Check to see if a code has been entered. 
  while(gsUpdateFlag);
  TLC5940_SetAllGS(4095);
  TLC5940_SetGSUpdateFlag();
}

void demoIteration(void) {
  uint8_t i;
  while(gsUpdateFlag);
  for (i = 0; i < numChannels; i++) {
    bubblePop(rand() % 96);
  }
  TLC5940_SetGSUpdateFlag();
}

void updateFinger(uint8_t input_bit, uint8_t status_bit) {
  if (!(INPUT_PIN & _BV(input_bit))) {
    if (glove_status & _BV(status_bit)) {
      glove_held[status_bit]++;
      if (glove_held[status_bit] > MAX_INTENSITY) {
        glove_held[status_bit] = MAX_INTENSITY;
      }
    } else {
      glove_status |= _BV(status_bit);
    }
  }
}

void productionIteration(void) {
  uint16_t longest_held = 0;
  glove_status = 0;
  for (uint8_t i; i < 6; i++) {
    if (!(INPUT_PIN & _BV(inputPins[i]))) {
      _delay_us(DEBOUNCE_TIME);
      if (!(INPUT_PIN & _BV(inputPins[i]))) {
        if (glove_status & _BV(i)) {
          glove_held[i]++;
          if (glove_held[i] > MAX_INTENSITY) {
            glove_held[i] = MAX_INTENSITY;
          }
        } else {
          glove_status |= _BV(i);
        }
      }
    } else {
      glove_status &= ~_BV(i);
    }
  }

  while(gsUpdateFlag);
  TLC5940_SetAllGS(base_intensity_d);
  for (uint8_t i; i < 6; i++) {
    if (glove_held[i] > longest_held) {
      longest_held = glove_held[i];
      TLC5940_SetAllGS(base_intensity_d + longest_held);
    }
  }
  for (uint8_t i = 0; i < 96; i++) {
    if (active_leds[i]) {
      TLC5940_SetGS(i, fade_value[i]);
      if (!glove_status) {
        fade_value[i]--;
      }
    }
  }
  if (glove_status) {
    for (uint8_t i; i < numChannels; i++) {
      bubblePop(rand() % 96);
    }
  }
  TLC5940_SetGSUpdateFlag();
}

// blinky mode for debugging
void blinkIteration(void){
  uint8_t i;
  if (blink_status == 0){
    blink_status = 1;
    while(gsUpdateFlag);
    for (i = 0; i < numChannels; i++) {
      TLC5940_SetGS(i, 4095);
    }
    TLC5940_SetGSUpdateFlag();
  }
  else{
    blink_status = 0;
    while(gsUpdateFlag);
    for (i = 0; i < numChannels; i++) {
      TLC5940_SetGS(i, 0);
    }
    TLC5940_SetGSUpdateFlag();
  }
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
}

void blinkIt(void) {
  while(gsUpdateFlag);
  TLC5940_SetAllGS(4095);
  TLC5940_SetGSUpdateFlag();
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  while(gsUpdateFlag);
  TLC5940_SetAllGS(0);
  TLC5940_SetGSUpdateFlag();
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  while(gsUpdateFlag);
  TLC5940_SetAllGS(4095);
  TLC5940_SetGSUpdateFlag();
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  while(gsUpdateFlag);
  TLC5940_SetAllGS(0);
  TLC5940_SetGSUpdateFlag();
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  _delay_ms(200);
  need_to_blink = 0;
}

int main(void) {
  initialize();

  while (1) {

    if (need_to_blink) {
      blinkIt();
    }

    switch (current_mode) {
      case DEMO: 
        demoIteration();
        break;
      case PRODUCTION:
        productionIteration();
        break;
      case COMMAND:
        //commandIteration();
        blinkIteration();
        break;
      default:
        break;
    }
  }

  return 0;
}