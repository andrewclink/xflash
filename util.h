
#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdlib.h>
#include <stdint.h>

#define MAX(a,b) \
  ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

#define MIN(a,b) \
  ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

uint16_t htole16(uint16_t i);
uint32_t htole32(uint32_t i);


void printHexStr(uint8_t * buffer, int len);


#endif;
