//
//  ihex
//
//  Created by Andrew Clink on 2013-05-06.
//  Copyright (c) 2013 Design Elements. All rights reserved.
//
#include <stdint.h>

#ifndef ihex_h
#define ihex_h

#define MAX_LINE 128
#define MAX_BIN_LINE (MAX_LINE/2)

typedef struct {
  int fd;
  int maxaddr;
  uint32_t crc;
} ihex_t;

typedef enum {
  ihex_recordtype_data = 0,
  ihex_recordtype_EOF,
  ihex_recordtype_ext_seg,
  ihex_recordtype_start_seg,
  ihex_recordtype_ext_lin,
  ihex_recordtype_start_lin,
} ihex_recordtype_t;

typedef struct {
  int len;
  int addr;
  ihex_recordtype_t recordType;
  uint8_t * data;
  uint16_t checksum;
} ihex_record_t;


typedef void ihex_readCallback(ihex_t *, ihex_record_t*, void *);

ihex_t * ihex_fromPath(const char * path);
void ihex_init(ihex_t * ihex);
void ihex_free(ihex_t * ihex);
int ihex_loadFile(ihex_t * ihex, const char * path);

// Reading hex
void _ihex_createRecord(ihex_record_t * record, uint8_t * buf, int len);
void ihex_read(ihex_t * hex, ihex_readCallback callback, void * context);

// Atmel CRC
void ihex_crc(ihex_t * ihex, int maxAddr, uint8_t pad);


#endif
