//
//  bootloader
//
//  Created by Andrew Clink on 2013-05-06.
//  Copyright (c) 2013 Design Elements. All rights reserved.
//
#include <libusb.h>
#include "ihex.h"

#define ACTUALLY_FLASH 1

#ifndef bootloader_h
#define bootloader_h

#define VID 0x59E3
#define PID 0xBBBB

#define REQ_INFO        0xB0
#define REQ_ERASE       0xB1
#define REQ_START_WRITE 0xB2
#define REQ_CRC_APP     0xB3
#define REQ_CRC_BOOT    0xB4
#define REQ_RESET       0xBF


// buffer must be read little endian
typedef  struct {
  uint8_t magic[4];      // 4s  4
  uint8_t version;       // B   1
  uint8_t part[4];       // 4s  4
  uint16_t pagesize;     // H   2
  uint32_t memsize;      // I   4
  uint32_t jumpaddr;     // I   4
  uint8_t hw_prod[16];   // 16s 16
  uint8_t hw_ver[16];    // 16s 16

  uint8_t padding[32];
} __attribute__((packed)) bootloader_info_t;

typedef struct {
	libusb_device_handle *devHandle;
	bootloader_info_t info;
} bootloader_t;



void bootloader_init(bootloader_t * bootloader, libusb_device_handle *devHandle);
void bootloader_free(bootloader_t * bootloader);

void bootloader_readInfo(bootloader_t* buffer);
const char * bootloader_strForDevice(uint8_t * deviceID);

int bootloader_reset(bootloader_t *bootloader);
int bootloader_erase(bootloader_t* bootloader);
int bootloader_appCRC(bootloader_t * bootloader, uint32_t* buffer);
void bootloader_writeFlash(bootloader_t *bootloader, ihex_t *hex);





#endif
