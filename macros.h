#ifndef _MACROS_H__
#define _MACROS_H__

#define FALSE 0
#define TRUE  1

#define setInput(ddr, pin)   ((ddr) &= ~(1 << (pin)))
#define readInput(port, pin) ((port) & (1 << (pin)))

#endif //_MACROS_H__