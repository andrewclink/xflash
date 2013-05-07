#include "util.h"
#include <stdio.h>


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
