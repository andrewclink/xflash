//
//  ihex
//
//  Created by Andrew Clink on 2013-05-06.
//  Copyright (c) 2013 Design Elements. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "ihex.h"
#include "util.h"
#include "colors.h"

extern int verbose;

ihex_t * ihex_fromPath(const char * path)
{
  // Create ihex object
  ihex_t * hex = malloc(sizeof(ihex_t));
  ihex_init(hex);
  
  // Load file
  ihex_loadFile(hex, path);
  
  return hex;
}

void ihex_init(ihex_t * ihex)
{
  memset(ihex, '\0', sizeof(*ihex));
  ihex->fd = -1;
}

void ihex_free(ihex_t *ihex)
{
  if (ihex->fd != -1)
  {
    close(ihex->fd);
    ihex->fd = -1;
  }

  free(ihex);
}


int ihex_loadFile(ihex_t * ihex, const char * path)
{
  printf("-> Loading %s\n", path);

  if (ihex->fd != -1)
  {
    close(ihex->fd);
    ihex->fd = -1;
  }
  
  ihex->fd = open(path, O_RDONLY);
  if (-1 == ihex->fd)
  {
    perror("Could not open file");
    exit(2);
  }
  
  return 1;
}

#pragma mark - Reading

static inline uint8_t charToNibble(unsigned char byte)
{
  uint8_t nibble;
  if (byte >= '0' && byte <= '9')
    nibble = byte - '0';
  else
  if (byte >= 'A' && byte <= 'F')
    nibble = (byte - 'A') + 0xA;
  else
  if (byte >= 'A' && byte <= 'F')
    nibble = (byte - 'A') + 0xA;
  else  
  {
    printf(CL_RED "Warning: Unknown nibble %c (0x%02x)\n" CL_RESET, byte, byte);
    return 0x00;
  }
    
  return nibble;
}


void ihex_read(ihex_t * hex, ihex_readCallback callback, void *context)
{
  // Rewind the file
  lseek(hex->fd, 0, SEEK_SET);
  
  // Read chunks of the file
  int binCount=-1; // Number of bytes in the current binBuf
  int recordLen=0; // Length of the current record to go into binBuf
  
  unsigned char lastA=0x1b;      // Used in case a is the last character of +buf+, meaning we haven't read b
  char    * buf = malloc(MAX_LINE );
  char    * charPtr;
  uint8_t * binBuf = malloc(MAX_BIN_LINE);

  int len;
  for(;;)
  {
    memset(buf, '\0', MAX_LINE);

    // Wrap read to catch EAGAIN
    for(;;)
    {
      len = read(hex->fd, buf, MAX_LINE);
      if ((len < 0) && (errno == EAGAIN || errno == EINTR))
      {
        if (verbose > 1)
          printf(CL_YELLOW "Repeating Read: %s\n" CL_RESET, strerror(errno));
        continue;
      }
      break;
    }
    
    if (len == 0)
    {
      // Finished reading
      if (verbose > 2)
        printf("Finalizing Read\n");
      goto finalize_read;
    }

    if (len < 0)
    {
      printf(CL_RED "Read Error %d: \n" CL_RESET, len);
      perror(CL_RED "Could not read" CL_RESET);
      goto finalize_read;
    }

    if (verbose > 2)
      printf("Read %d bytes; Current Bin count: %d\n", len, binCount);

    charPtr = buf;
    
    // printHexStr((uint8_t*)buf, MAX_LINE);
    
      
    // Scan through the current characters, then call the callback when 
    // an entire line is parsed
    //
    
    
    // Convert char bytes into actual bytes and place them in the binBuf
    //
    int eor_found = 0;
    uint8_t byte;
    #define breakIfLen() if (charPtr>=charPtrEnd) break
    // #define breakIfEnd(_b_) if () { printf("(EOR)\n"); eor_found = 1; break;}
    #define breakUnlessValid(_b_) \
      if (!((_b_ >= '0' && _b_<='9') || (_b_ >= 'A' && _b_ <= 'F'))) \
        { printf(" Invalid byte at (buf %d/%d) (rec %d/%d): 0x%x (%c)\n", \
          i, len, \
          binCount, (4 /*Header*/ + recordLen + 1 /*Checksum*/), \
          _b_, _b_); \
        exit(3); }
        
    char *charPtrEnd = charPtr + len;
    for(; charPtr < charPtrEnd;)
    {
      for(;;)
      {
        breakIfLen();
        if (binCount >= (4 /*Header*/ + recordLen  + 1/*+ 1 Checksum*/)) 
        {
          if (verbose > 2)
            printf("-> Finished %d/%d bytes in record\n", 
              binCount, (4 /*Header*/ + recordLen  + 1 /*Checksum*/));
          
          // Eat bytes until start of record
          // while (charPtr < charPtrEnd && *charPtr != ':')
          // {
          //   printf("(EOR) Skipping 0x%02x\n", *charPtr);
          //   charPtr++;
          // }
          
          eor_found = 1;
          break;
        }

        
        // Expect start code as the start of each record
        // 
        if (-1 == binCount)
        {
          if (verbose > 1)
            printf("\n");

          // Eat bytes until start of record
          while (charPtr < charPtrEnd && *charPtr != ':')
          {
           // printf("(SOR) Skipping 0x%02x\n", *charPtr);
           charPtr++;
          }          
          
          // If we stopped looking for SOF because charPtr is at end, break.
          if (charPtr >= charPtrEnd)
            break;
          
          if (*charPtr != ':') 
          {
            printf(CL_RED "Invalid Start of Frame Character\n" CL_RESET);
            exit(3);
          }
          
          // printf("SOR '%c'  ", *charPtr);
          
          charPtr++;
          binCount++;
        }
        
        if (charPtr >= charPtrEnd)
        {
          if (verbose > 3)
            printf(CL_YELLOW "Buffer empty after SOF\n" CL_RESET);

          break;
        }
        
        
        // Read two chars (one byte)
        unsigned char a, b='0';

        if (lastA != 0x1b)
        { 
          if (verbose > 3)
            printf(CL_YELLOW "Resuming with nibble a=0x%02x\n" CL_RESET, lastA);
          
          a = lastA;
          lastA = 0x1b;
        }
        else
        {
          a = *charPtr++;
        }
        
        if (charPtr >= charPtrEnd)
        {
          // We have to read more characters before continuing
          if (verbose > 3)
            printf(CL_YELLOW "Caching nibble a (0x%02x) for buffer refresh\n" CL_RESET, a);
          lastA = a;
          break;
        }

        b = *charPtr++;

        
        // Convert char nibbles into a byte
        byte = charToNibble(a) << 4;
        byte |= charToNibble(b);

        // Track record Length if appropriate
        if (0 == binCount)
        {
          recordLen = byte;
          // printf("Got Record Length %d\n", recordLen);
        }

        // Copy into binBuf
        binBuf[binCount++] = byte;
        
        // Loop and check record length
      }
    
      if (eor_found)
      {
        // BinBuf Complete
        // Create Record
        //
        ihex_record_t record;
        _ihex_createRecord(&record, binBuf, binCount);
        
        if (0 == hex->wasRead)
        {
          if (record.addr > hex->maxAddr)
            hex->maxAddr = record.addr;
          
          // Update hex byte size
          hex->size += record.len;
        }
        
        // Call callback
        // 
        callback(hex, &record, context);

        // Check for EOF
        if (ihex_recordtype_EOF == record.recordType)
          break;
      
        // Reset
        eor_found = 0;
        memset(binBuf, '\0', sizeof(*binBuf));
        binCount = -1;
        recordLen = 0;
        continue;
      } // End of record

      if (verbose > 0) printf("\n");
      break;
    } // End of char buffer
  } // End of file

finalize_read:

  if (verbose > 2)
    printf("Freeing buf: %p ; binBuf: %p;\n", buf, binBuf);

  free(buf);
  free(binBuf);
  
  hex->wasRead = 1;
}

static inline uint16_t _readUInt16(uint8_t* ptr)
{
  uint16_t i=0;
  i |= *ptr++ << 8; 
  i |= *ptr++;
  
  return i;
}

void _ihex_createRecord(ihex_record_t * record, uint8_t * buf, int len)
{
  if (len < 4) { printf("Bad record\n"); return; }
  
  uint8_t * ptr = buf;
  record->len  = *ptr++;
  record->addr = _readUInt16(ptr); ptr+= 2;
  record->recordType = *ptr++;
  record->data = ptr;
  record->checksum = buf[len-1];
    
  // Check record checksum here
}


#pragma mark - CRC Calculation

struct crc_context {
  int count; // Number of bytes processed so far
  int ha;
  int hb; 
  int crc;
};

#define CRC32_POLY 0x0080001BL

static void _ihexCRC_updateContext(struct crc_context *c, uint16_t d)
{
  c->ha = c->crc << 1;
  c->ha &= 0x00FFfffe;
  c->hb = c->crc & (1 << 23);
  if (c->hb > 0)
    c->hb = 0x00FFffff;
  
  c->crc = (c->ha ^ d) ^ (c->hb & CRC32_POLY);
  c->crc &= 0x00ffFFFF;
  c->count++;
}

static void ihex_didReadCRC(ihex_t * hex, ihex_record_t* rec, struct crc_context *c)
{
  // Dump
  //
  if (verbose > 1)
  {
    printf("\e[;33m %08x \e[m", rec->addr);
    printf("\e[;33m %d  \e[m", rec->recordType);
    printf("\e[;33m % 4d     \e[m", rec->len);

    printHexStr(rec->data, rec->len);
    
    printf("\e[;33m%02x\e[m", rec->checksum);
    printf("\n");
  }
  
  if (ihex_recordtype_EOF == rec->recordType)
  {
    return;
  }
  
  // Calculate CRC
  //
  uint8_t * ptr = rec->data;
  uint8_t * dataEnd = ptr + rec->len;
  
  while (ptr < dataEnd)
  {
    uint16_t d;
    d  = *ptr++; 
    d |= (ptr >= dataEnd ? 0xff : *ptr) << 8; ptr++; // Ensure ptr increments so the loop will terminate

    _ihexCRC_updateContext(c, d);
  }
}

void ihex_crc(ihex_t * ihex, int maxAddr, uint8_t pad)
{
  struct crc_context context;
  memset(&context, '\0', sizeof(context));
  
  // Process all bytes in the file
  ihex_read(ihex, (ihex_readCallback*)ihex_didReadCRC, &context);
  
  // Pad to memory length
  while(context.count <= maxAddr/2)
    _ihexCRC_updateContext(&context, (pad | pad << 8));
  
  // Save CRC
  ihex->crc = context.crc;
}
