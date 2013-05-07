#include "util.h"
#include <stdio.h>
uint16_t htole16(uint16_t i)
{
#if 0
  uint16_t swapped = (i>>8) | (i<<8);
  printf("Swapping %04x => %04x\n", i, swapped);
  return swapped;
#else
  return i;
#endif
}

uint32_t htole32(uint32_t i)
{
#if 0
  uint32_t swapped;
  swapped = (( i >>24)&0xff)      | // move byte 3 to byte 0
            (( i <<8)&0xff0000)   | // move byte 1 to byte 2
            (( i >>8)&0xff00)     | // move byte 2 to byte 1
            (( i <<24)&0xff000000); // byte 0 to byte 3
  return swapped;

#else

  return i;

#endif
}


void printHexStr(uint8_t * buffer, int len)
{
  uint8_t * ptr = buffer;
  int i;
  for(i=1; i<=len; i++)
  {
    printf("%02x ", *ptr++);
    if ((i % 16) == 0)
      printf("\n");
  }
}
