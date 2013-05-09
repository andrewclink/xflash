//
//  untitled
//
//  Created by Andrew Clink on 2013-05-05.
//  Copyright (c) 2013 Design Elements. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <libusb.h>

#include "util.h"
#include "bootloader.h"
#include "ihex.h"
#include "colors.h"


  
static libusb_context *ctx = NULL;
static int forceProductID = 0xffffFFFF;
static int forceVendorID  = 0xFFFFffff;


int verbose=0;

libusb_device * find_device(void)
{
  ssize_t i = 0;
  printf("Searching for devices...\n");

  libusb_device **list;
  ssize_t cnt = libusb_get_device_list(ctx, &list);

  if (cnt < 0)
  {
    return NULL;
  }

  uint16_t searchProductID = (0xFFFFffff != forceProductID) ? forceProductID : BOOTLOADER_VID;
  uint16_t searchVendorID  = (0xFFFFffff != forceVendorID)  ? forceVendorID  : BOOTLOADER_PID;
  
  printf("Searching for Vendor ID:  0x%04x\n", searchProductID);
  printf("Searching for Product ID: 0x%04x\n", searchVendorID);
  

  int found = 0;
  libusb_device *device = NULL;
  for (i = 0; i < cnt; i++) 
  {
       device = list[i];
      
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
      
      if (verbose > 2)
        printf("-> Checking 0x%04x:0x%04x: ", desc.idVendor, desc.idProduct);
      
      
      // Is this a bootloader?
      if (desc.idVendor == searchVendorID && desc.idProduct == searchProductID)
      {
        if (verbose > 2)
          printf( CL_GREEN " <=\n" CL_RESET);
        found = 1;
        break;
      }

      // Is this a resettable application?
      if (0xFFFFffff == forceVendorID && desc.idVendor == MY_VID && desc.idProduct == searchProductID)
      {
        if (verbose > 2)
          printf(CL_RED " <=\n" CL_RESET);
        found = 1;
        break;
      }

      if (verbose > 2)
        printf("\n");
  }

  if (found)
  {
    libusb_ref_device(device); 
  }
  else
  {
    device = NULL;
  }

  libusb_free_device_list(list, 1);
  return device;
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

int main(int argc, char *argv[])
{
  int s;//tatus

  libusb_init(&ctx);
  
  // Read options
  int opt;
  while ((opt = getopt(argc, argv, "V:p:v:")) != -1)
  {
    switch(opt)
    {
      case 'V': // Verbosity
        verbose = atoi(optarg);
        printf("Setting verbose: %d\n", verbose);
        if (verbose > 2)
          libusb_set_debug(ctx, 3);
        
        break;

      case 'v': // Vendor ID
        sscanf((optarg[1] == 'x' || optarg[1] == 'X') ? optarg + 2 : optarg, "%04x", &forceVendorID);
        break;

      case 'p': // Product ID
        sscanf((optarg[1] == 'x' || optarg[1] == 'X') ? optarg + 2 : optarg, "%04x", &forceProductID);
        break;
    }
  }
  
  // Find an interesting device
  //
  libusb_device *dev = find_device();
  libusb_device_handle *devHandle = NULL;
  
  if (NULL == dev)
  {
    printf("Could not locate device\n");
    exit(1);
  }

  
  // Open Device
  //
  if (dev) 
  {
    s = libusb_open(dev, &devHandle);
    if (0 != s)
    {
      printf(CL_RED "Could not open device: error %d\n" CL_RESET, s);
      exit(2);
    }
    
    // Referenced device is returned from find_device; open also adds a reference.
    // Let libusb own the device now.
    libusb_unref_device(dev);
  }


  // Dump device info for debugging
  //
  if (verbose > 2)
  {
    printf("Using Device:\n");
    printdev(dev);
  }
  

  // Determine what to do. If the device has a prototype vendor ID, 
  // assume it should be reset to bootloader. Otherwise, flash.
  //
  struct libusb_device_descriptor desc;
  int r = libusb_get_device_descriptor(dev, &desc);
  if (r < 0) {
    printf("-> Failed to get device descriptor\n");
    exit(1);
  }
  
  if (MY_VID == desc.idVendor)
  {
    forceVendorID = 0xFFFFffff; // Look for bootloader now.
    
    printf(CL_YELLOW "Resetting application\n" CL_RESET);
    // Set Configuration
    s = libusb_set_configuration(devHandle, 1);
    if (s !=0) { printf("libusb_set_configuration %d\n", s); exit(2); }

    // Reset device into bootloader
    s = libusb_control_transfer(devHandle, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN, REQ_APP_RESET, 0, 0, NULL, 0, 1000);
    libusb_close(devHandle);
    devHandle = NULL;
    
    int i;
    for (i=0; i<10; i++)
    {
      usleep(100000); // Wait 100 milliseconds for the device to reattach
      dev = find_device();
      if (NULL != dev) 
        break;
    }
    
    if (NULL == dev)
    {
      // Waited a whole second for the device to reattach and came up emtpy-handed.
      printf(CL_RED "Unable to locate device after reset\n");
      exit(2);
    }

    // Go ahead and open this device now.
    s = libusb_open(dev, &devHandle);
    if (0 != s)
    {
      printf(CL_RED "Could not open device: error %d\n" CL_RESET, s);
      exit(2);
    }

    // Referenced device is returned from find_device; open also adds a reference.
    // Let libusb own the device now.
    libusb_unref_device(dev);
  }
  

  // Parse args
  // Assuming flash for now


  // Create a bootloader object to manage the flash
  bootloader_t bootloader;
  bootloader_init(&bootloader, devHandle);

  
  ihex_t * hex = ihex_fromPath(argv[1]);
  ihex_crc(hex, bootloader.info.memsize, 0xff);
  
  // Erase device
  s = bootloader_erase(&bootloader);
  
  // Write flash
  printf(CL_GREEN "-> Writing %d bytes\n" CL_RESET, hex->size);
  bootloader_writeFlash(&bootloader, hex);
  printf(CL_GREEN "\nDone\n" CL_RESET);
    
  // Check App CRC
  uint32_t crc=0;
  s = bootloader_appCRC(&bootloader, &crc); 
  printf("File CRC:0x%04x\n", hex->crc);
  printf("App CRC: 0x%04x\n", crc);

  if (crc == hex->crc)
  {
    printf(CL_GREEN "CRC Matches\n" CL_RESET);
    s = bootloader_reset(&bootloader);
    if (s != 0)
    {
      printf(CL_RED "Could not reset target: %d\n" CL_RESET, s);
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