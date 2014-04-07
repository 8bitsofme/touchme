#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "tlc5940.h"
#include "usart.h"

void all_on(void) {
  while(gsUpdateFlag);
  TLC5940_SetAllGS(4000);
  TLC5940_SetGSUpdateFlag();
  printString("1");
}

void all_off(void) {
  while(gsUpdateFlag);
  TLC5940_SetAllGS(0);
  TLC5940_SetGSUpdateFlag();
  printString("0");
}

void delay_s(uint16_t delay) {
  uint8_t i;
  for (i = 0; i < delay * 100; i++) {
    _delay_ms(10);
  }
}

ISR(__vector_default){}

int main(void) {
  initUSART();
  printString("OK\r\n");
  printString("TLC5940_Init\r\n");
  TLC5940_Init();
  printString("TLC5940_SetAllGS(0)\r\n");
  TLC5940_SetAllGS(0);

  sei();

  for (;;) {
    all_on();
    delay_s(1);
    all_off();
    delay_s(1);
  }
  return 0;
}