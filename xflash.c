//
//  untitled
//
//  Created by Andrew Clink on 2013-05-05.
//  Copyright (c) 2013 Design Elements. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <libusb.h>
#include <string.h>

#include "util.h"
#include "ihex.h"

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
  
  
static libusb_context *ctx = NULL;
static libusb_device  *dev = NULL;
static libusb_device_handle *devHandle = NULL;

void bootloader_readInfo(bootloader_info_t* buffer);
const char * bootloader_strForDevice(uint8_t * deviceID);




void find_device()
{
  ssize_t i = 0;

  libusb_init(&ctx);
  // libusb_set_debug(ctx, 3);

  printf("Searching for devices\n");

  libusb_device **list;
  ssize_t cnt = libusb_get_device_list(ctx, &list);

  int err = 0;
  if (cnt < 0)
  {
    printf("-> No devices found\n\n");
    exit(1);
  }

  for (i = 0; i < cnt; i++) 
  {
      libusb_device *device = list[i];
      
      // printf("-> Found device %p\n", device);

      // Determine if device is interesting
      //
      struct libusb_device_descriptor desc;
      int status = libusb_get_device_descriptor(device, &desc);
      if (status < 0) 
      {
        printf("---> Failed to get device descriptor\n");
        continue;
      }
      
      if (desc.idVendor == VID && desc.idProduct == PID)
      {
        // Device is interesting
        dev = device;
        break;
      }
  }

  // Open Device
  //
  if (dev) 
  {
    err = libusb_open(dev, &devHandle);
  }

  libusb_free_device_list(list, 1);
}

void printdev(libusb_device *dev) 
{
  struct libusb_device_descriptor desc;
  // int desc;
  int r = libusb_get_device_descriptor(dev, &desc);
  if (r < 0) {
    printf("-> Failed to get device descriptor\n");
    return;
  }
  printf("  Number of configurations: %d\n", desc.bNumConfigurations);
  printf("  Device Class: %d\n", desc.bDeviceClass);
  printf("  VendorID: 0x%02x\n", desc.idVendor);
  printf("  ProductID: 0x%02x\n", desc.idProduct);
}

#pragma mark - Bootloader

const char * bootloader_strForDevice(uint8_t * deviceID)
{
  // '1e9441': 'ATxmega16A4U',
  // '1e9541': 'ATxmega32A4U',
  // '1e9646': 'ATxmega64A4U',
  // '1e9746': 'ATxmega128A4U',

  if (NULL == deviceID) return "(Null Device)";
  if (*deviceID++ == 0x1e)
  {
    switch (*deviceID++)
    {
      case 0x94: if (*deviceID == 0x41) { return "ATxmega16A4U" ;} break;
      case 0x95: if (*deviceID == 0x41) { return "ATxmega32A4U" ;} break;
      case 0x96: if (*deviceID == 0x46) { return "ATxmega64A4U" ;} break;
      case 0x97: if (*deviceID == 0x46) { return "ATxmega128A4U";} break;
      default: break;
    }
  }
  
  return "Unknown Device";
}

  // int libusb_control_transfer  ( libusb_device_handle *  dev_handle,
  //   uint8_t  bmRequestType,
  //   uint8_t  bRequest,
  //   uint16_t   wValue,
  //   uint16_t   wIndex,
  //   unsigned char *  data,
  //   uint16_t   wLength,
  //   unsigned int   timeout 
  //   )

void bootloader_readInfo(bootloader_info_t* buffer)
{
  printf("-----------------------\n");
  printf("Reading Info into %p\n", buffer);

  memset(buffer, '\0', sizeof(*buffer));
  
  libusb_control_transfer(devHandle, 0x40 | 0x80, REQ_INFO, 0, 0, (unsigned char*)buffer, 64, 1);
  
  // Correct endian
  buffer->pagesize = htole16(buffer->pagesize);
  buffer->memsize  = htole32(buffer->memsize);
  buffer->jumpaddr = htole32(buffer->jumpaddr);
  
  
  printf("\n\n");
  
  printf("  Magic: "); printHexStr((uint8_t *)&buffer->magic, 4);
  printf("  Version: %d\n", buffer->version);
  printf("  Part: "); printHexStr((uint8_t *)&buffer->part, 4);
  printf("  Part: %s\n", bootloader_strForDevice((uint8_t *)&buffer->part));
  printf("  Pagesize: %d; ", buffer->pagesize);
  printf("  Memsize: %d; ",  buffer->memsize + 1); 
  printf("  Jump Addr: 0x%x\n", buffer->jumpaddr);
  printf("  Prod: %s", buffer->hw_prod);
  printf("  HWVer: %s", buffer->hw_ver);
  printf("-----------------------\n");
}


void bootloader_writeFlash(const char * path)
{
  
}

int main(int argc, const char ** argv)
{
  find_device();
  if (NULL == dev)
  {
    printf("Could not locate device\n");
    exit(1);
  }
  
  printf("Using Device:\n");
  printdev(dev);
  
  libusb_set_configuration(devHandle, 0);

  bootloader_info_t info;
  bootloader_readInfo(&info);
  
  // Parse args
  
  // Assuming flash
  ihex_t * hex = ihex_fromPath(argv[1]);
  ihex_crc(hex, info.memsize, 0xff);
  
  ihex_free(hex);
  return 0;
}