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
#include "bootloader.h"
#include "ihex.h"
#include "colors.h"


  
static libusb_context *ctx = NULL;
static libusb_device  *dev = NULL;
static libusb_device_handle *devHandle = NULL;





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



  // int libusb_control_transfer  ( libusb_device_handle *  dev_handle,
  //   uint8_t  bmRequestType,
  //   uint8_t  bRequest,
  //   uint16_t   wValue,
  //   uint16_t   wIndex,
  //   unsigned char *  data,
  //   uint16_t   wLength,
  //   unsigned int   timeout 
  //   )






int main(int argc, const char ** argv)
{
  int status;
  
  find_device();
  if (NULL == dev)
  {
    printf("Could not locate device\n");
    exit(1);
  }
  
  printf("Using Device:\n");
  printdev(dev);
  

  bootloader_t bootloader;
  bootloader_init(&bootloader, devHandle);

  // Parse args
  
  // Assuming flash
  ihex_t * hex = ihex_fromPath(argv[1]);
  ihex_crc(hex, bootloader.info.memsize, 0xff);
  
  // Erase device
  status = bootloader_erase(&bootloader);
  
  // Write flash
  bootloader_writeFlash(&bootloader, hex);
  printf("Flash finished\n");
    
  // Check App CRC
  uint32_t crc=0;
  status = bootloader_appCRC(&bootloader, &crc); printf("Status: %d\n",status);

  printf("File CRC:0x%04x\n", hex->crc);
  printf(" App CRC: 0x%04x\n", crc);

  if (crc == hex->crc)
  {
    printf(CL_GREEN "CRC Matches\n" CL_RESET);
    status = bootloader_reset(&bootloader);
    if (status != 0)
    {
      printf(CL_RED "Could not reset target: %d\n" CL_RESET, status);
    }
  }
  else
  {
    printf(CL_RED "CRC Mismatch\n" CL_RESET);
  }
  
  bootloader_free(&bootloader);
  ihex_free(hex);
  return 0;
}