//
//  bootloader
//
//  Created by Andrew Clink on 2013-05-06.
//  Copyright (c) 2013 Design Elements. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bootloader.h"
#include "util.h"
#include "colors.h"

#define chkStatus(_s_, _msg_) _chkStatus(_s_, _msg_, __LINE__)
static void _chkStatus(int status, const char * msg, int line);
static void _chkStatus(int status, const char * msg, int line)
{
  if (status != 0)
  { 
    printf(CL_RED "Line %d: %s failed: %d\n" CL_RESET, 
      line, msg, status); 
    exit(3); 
  }
}


void bootloader_init(bootloader_t * bootloader, libusb_device_handle *devHandle)
{
  int status;
  memset(bootloader, '\0', sizeof(*bootloader));
  bootloader->devHandle = devHandle;
    
  // Set Configuration
  status = libusb_set_configuration(bootloader->devHandle, 0);
  chkStatus(status, "libusb_set_configuration");
  
  // Claim the bulk interface
  status = libusb_claim_interface(bootloader->devHandle, 1);
  chkStatus(status, "libusb_claim_interface");

  // Read Device Info
  bootloader_readInfo(bootloader);
}

void bootloader_free(bootloader_t * bootloader)
{
  // Clean up
  libusb_release_interface(bootloader->devHandle, 1);
  libusb_close(bootloader->devHandle);
}

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

void bootloader_readInfo(bootloader_t* bootloader)
{
  bootloader_info_t * buffer = &bootloader->info;
  int status;

  printf("-----------------------\n");
  printf("Reading Info into %p\n", buffer);


  memset(buffer, '\0', sizeof(*buffer));
  
  status = libusb_control_transfer(bootloader->devHandle, 0x40 | 0x80, REQ_INFO, 0, 0, (uint8_t*)buffer, 64, 1000);
  chkStatus(status, "libusb_control_transfer");
  
  // Correct endian
  buffer->pagesize = htole16(buffer->pagesize);
  buffer->memsize  = htole32(buffer->memsize);
  buffer->jumpaddr = htole32(buffer->jumpaddr);
  
  
  printf("\n\n");
  
  printf("  Magic: "); printHexStr((uint8_t *)&buffer->magic, 4); printf("\n");
  printf("  Version: %d\n", buffer->version);
  printf("  Part: "); printHexStr((uint8_t *)&buffer->part, 4); printf("\n");
  printf("  Part: %s\n", bootloader_strForDevice((uint8_t *)&buffer->part));
  printf("  Pagesize: %d; ", buffer->pagesize);
  printf("  Memsize: %d; ",  buffer->memsize + 1); 
  printf("  Jump Addr: 0x%x\n", buffer->jumpaddr);
  printf("  Prod: %s\n", buffer->hw_prod);
  printf("  HWVer: %s\n", buffer->hw_ver);
  printf("-----------------------\n");
}


int bootloader_reset(bootloader_t *bootloader)
{
  return libusb_control_transfer(bootloader->devHandle, 0x40 | 0x80, REQ_RESET, 0, 0, NULL, 0, 1000);
}

int bootloader_erase(bootloader_t *bootloader)
{
  return libusb_control_transfer(bootloader->devHandle, 0x40 | 0x80, REQ_ERASE, 0, 0, NULL, 0, 1000);
}

int bootloader_appCRC(bootloader_t * bootloader, uint32_t* buffer)
{
  int status = libusb_control_transfer(bootloader->devHandle, 0x40 | 0x80, REQ_CRC_APP, 0, 0, (uint8_t*)buffer, 4, 1000);
  *buffer = htole32(*buffer);

  return status;
}

#pragma mark - Writing Flash
#define TSIZE 64

struct _flash_context {
  int cnt;
  uint8_t * buf;
  bootloader_t * bootloader;
};

static void _bootloader_didReadHexRecord(ihex_t * hex, ihex_record_t* rec, struct _flash_context *ctx)
{
  struct _flash_context * c = ctx; //(struct _flash_context *)ctx;

  // Ignore non-data records
  if (rec->recordType != ihex_recordtype_data)
  {
    printf("(Ignoring non-data record)\n");

    // Write the final buffer
    if (ihex_recordtype_EOF == rec->recordType)
    {
      // Pad to TSIZE
      int delta = TSIZE-c->cnt;
      printf("Last buffer has %d bytes; delta: %d\n", c->cnt, delta);
      memset((c->buf + c->cnt), 0xcf, delta);
      c->cnt += delta;
      
      printf(CL_GREEN "=> Writing packet: %d bytes " CL_RESET, c->cnt);
      int transfered=0;
      printf("Handle: %p\n", c->bootloader->devHandle);
      int status = libusb_bulk_transfer(c->bootloader->devHandle, 1, c->buf, c->cnt, &transfered, 1000);
      if (status == 0)
        printf("(wrote %d OK)\n", transfered);
      else
        printf(CL_RED "Error: %d\n" CL_RESET, status);

      printf ("Done\n");
      return;
    }
  }
  

  // Write the record's data into the buffer
  int len = TSIZE - c->cnt;
  len = MIN(rec->len, len);            // May not copy the entire record!
  // printf("->Copying %d bytes\n", len);  
  c->cnt += len;
  
  if (c->cnt >= TSIZE)
  {
    // Send the USB packet
    //
    printf(CL_GREEN "=> Writing packet: %d bytes " CL_RESET, c->cnt);
    int transfered=0;
    int status = libusb_bulk_transfer(c->bootloader->devHandle, 1, c->buf, TSIZE, &transfered, 1000);
    if (status >= 0)
      printf("(wrote %d OK)\n", status);
    else
      printf(CL_RED "Error: %d\n" CL_RESET, status);
    
    
    // If we didn't write the entire record, copy it to the beginning of the buffer
    if (len < rec->len)
    {
      c->cnt = rec->len - len;
      // Copy the unhandled bytes
      memcpy(c->buf, rec->data + len, c->cnt);
    }
    else
    {
      // Record data was aligned; new buffer is empty
      c->cnt = 0;
    }
    
  }
  else
  {
    if (len < rec->len)
    {
      printf(CL_RED "This should never happen: " CL_RESET 
             " Had enough room to write %d byte record, but only wrote %d bytes\n",
             rec->len, len);
    }
  }

  
  
}

void bootloader_writeFlash(bootloader_t *bootloader, ihex_t *hex)
{
  int status;
  printf("-> Writing Flash\n");
  
  // Check that the file will fit
  if (hex->maxAddr > bootloader->info.memsize)
  {
    printf(CL_RED "Input file size exceeds max device memory\n" CL_RESET);
    exit(4);
  }
  
  // Signal Write Start
  //
  status = libusb_control_transfer(bootloader->devHandle, 0x40 | 0x80, REQ_START_WRITE, 0, 0, NULL, 0, 1000);
  if (status < 0)
    printf(CL_RED "Input file size exceeds max device memory\n" CL_RESET);
  
  
  // Read the hex, writing to the bootloader in the callback
  uint8_t * buf = malloc(TSIZE+1); // byte 0 is the current buffer count! byte 1 is the start of the buffer
  struct _flash_context context = { 0, buf, bootloader };
  ihex_read(hex, (ihex_readCallback*)_bootloader_didReadHexRecord, &context);
  
  free(buf);
  
  printf("-> Wrote\n");
}