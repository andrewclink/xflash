//
//  bootloader
//
//  Created by Andrew Clink on 2013-05-06.
//  Copyright (c) 2013 Design Elements. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "bootloader.h"
#include "util.h"
#include "colors.h"

extern int verbose;

#define chkStatus(_s_, _msg_) _chkStatus(_s_, _msg_, __LINE__, 1)
#define chkStatusSoft(_s_, _msg_) _chkStatus(_s_, _msg_, __LINE__, 0)
static void _chkStatus(int status, const char * msg, int line, int kill);
static void _chkStatus(int status, const char * msg, int line, int kill)
{
  if (status != 0)
  { 
    printf(CL_RED "Line %d: %s failed: %d\n" CL_RESET, 
      line, msg, status); 
    
    if (kill)
      exit(3); 
  }
}


void bootloader_init(bootloader_t * bootloader, libusb_device_handle *devHandle)
{
  sleep(1);
  
  int status;
  memset(bootloader, '\0', sizeof(*bootloader));
  bootloader->devHandle = devHandle;
    
  // Set Configuration
  status = libusb_set_configuration(bootloader->devHandle, 1);
  chkStatusSoft(status, "libusb_set_configuration");
  
  // Claim the bulk interface
  status = libusb_claim_interface(bootloader->devHandle, 0);
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

  memset(buffer, '\0', sizeof(*buffer));
  
  int i;
  for(i=0; i<4; i++)
  {
    status = libusb_control_transfer(bootloader->devHandle, 
                                     LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN, 
                                     REQ_INFO,  /* Request */
                                     0,         /* bValue */
                                     0,         /* wIndex */
                                     (uint8_t*)buffer, /* Receive Buffer */ 
                                     64,    /* Size */
                                     1000); /* Timeout */
    
    if (status > -1) 
      break;

    printf(CL_RED "Info request failed: %d\n" CL_RESET, status);
    printf("Retrying\n");
    usleep(250000);
  }

  if (status < 0)
    exit(3);
  
  
  // Correct endian
  buffer->pagesize = buffer->pagesize;
  buffer->memsize  = buffer->memsize;
  buffer->jumpaddr = buffer->jumpaddr;
  
  
  printf("-----------------------\n");
  if (verbose > 1)
  {
    printf("  Magic: "); printHexStr((uint8_t *)&buffer->magic, 4); printf("\n");
    printf("  Version: %d\n", buffer->version);
    printf("  Part: "); printHexStr((uint8_t *)&buffer->part, 4); printf("\n");
    printf("  Part: %s\n", bootloader_strForDevice((uint8_t *)&buffer->part));
    printf("  Pagesize: %d; ", buffer->pagesize);
    printf("  Memsize: %d; ",  buffer->memsize + 1); 
    printf("  Jump Addr: 0x%x\n", buffer->jumpaddr);
    printf("  Prod: %s; HWVer: %s\n", buffer->hw_prod, buffer->hw_ver);
  }
  else
  {
    printf("  Bootloader Version %d\n", buffer->version);
    printf("  Part: %s\n", bootloader_strForDevice((uint8_t *)&buffer->part));
    printf("  Memsize: %d; ",  buffer->memsize + 1); 
    printf("  Prod: %s; HWVer: %s\n", buffer->hw_prod, buffer->hw_ver);
  }
  printf("-----------------------\n");
}


int bootloader_reset(bootloader_t *bootloader)
{
  return libusb_control_transfer(bootloader->devHandle, 0x40 | 0x80, REQ_RESET, 0, 0, NULL, 0, 1000);
}

int bootloader_erase(bootloader_t *bootloader)
{
#if ACTUALLY_FLASH
  return libusb_control_transfer(bootloader->devHandle, 0x40 | 0x80, REQ_ERASE, 0, 0, NULL, 0, 1000);
#else
  return 0;
#endif
}

int bootloader_appCRC(bootloader_t * bootloader, uint32_t* crc)
{
  int status = libusb_control_transfer(bootloader->devHandle, 0x40 | 0x80, REQ_CRC_APP, 0, 0, (uint8_t *)crc, 4, 1000);
  return status;
}

#pragma mark - Writing Flash
#define TSIZE 256

struct _flash_context {
  int cnt;
  int bytesWritten;
  uint8_t * buf;
  bootloader_t * bootloader;
};

static void _bootloader_didReadHexRecord(ihex_t * hex, ihex_record_t* rec, struct _flash_context *ctx)
{
  struct _flash_context * c = ctx; //(struct _flash_context *)ctx;

  // Ignore non-data records
  if (rec->recordType != ihex_recordtype_data)
  {
    if (verbose > 1)
      printf("(Ignoring non-data record)\n");

    // Write the final buffer
    if (ihex_recordtype_EOF == rec->recordType)
    {
      // Pad to TSIZE
      int delta = TSIZE-c->cnt;
      if (verbose > 1)
        printf("Last buffer has %d bytes; delta: %d\n", c->cnt, delta);
      memset((c->buf + c->cnt), 0xff, delta);
      c->cnt += delta;
      
      // printf(CL_GREEN "=> Writing packet: %d bytes " CL_RESET, c->cnt);
      int transfered=0;


#if ACTUALLY_FLASH
      int status = libusb_bulk_transfer(c->bootloader->devHandle, LIBUSB_ENDPOINT_OUT | 0x01, c->buf, c->cnt, &transfered, 1000);
#else
      int status = 1;
      if (verbose > 1)
      {
        printHexStr(c->buf, c->cnt);
        printf("\n");
      }
#endif
      if (status >= 0)
      {
        printf("\b\b\b\b100%%");
        if (verbose > 2)
          printf("(wrote %d OK)\n", status);
      }
      else
        printf(CL_RED "Error writing final buffer: %d\n" CL_RESET, status);

      if (verbose > 1)
        printf ("\nDone\n");
      return;
    }
  }
  

  

  // Write the record's data into the buffer
  int len = TSIZE - c->cnt;
  len = MIN(rec->len, len);            // May not copy the entire record!
  // printf("->Copying %d bytes\n", len);
  memcpy(c->buf + c->cnt, rec->data, rec->len);
  c->cnt += len;
  
  if (c->cnt >= TSIZE)
  {
    // Send the USB packet
    //
    // printf(CL_GREEN "=> Writing packet: %d bytes " CL_RESET, c->cnt);
    int transfered=0;
#if ACTUALLY_FLASH
    int status = libusb_bulk_transfer(c->bootloader->devHandle, LIBUSB_ENDPOINT_OUT | 0x01, c->buf, c->cnt, &transfered, 1000);
#else
    int status = 1;
    if (verbose > 1)
    {
      printHexStr(c->buf, c->cnt);
      printf("\n");
    }
#endif
    
    if (status >= 0)
    {
      c->bytesWritten += c->cnt;
      printf("\b\b\b\b"); // Back up
      printf("% 3d%%", c->bytesWritten * 100 / hex->size);
    }
    else
      printf(CL_RED "Flash Error: %d\n" CL_RESET, status);
    
    
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
  
  // Check that the file will fit
  if (hex->maxAddr > bootloader->info.memsize)
  {
    printf(CL_RED "Input file size exceeds max device memory\n" CL_RESET);
    exit(4);
  }
  
  // Signal Write Start
  //
#if ACTUALLY_FLASH
  status = libusb_control_transfer(bootloader->devHandle, 0x40 | 0x80, REQ_START_WRITE, 0, 0, NULL, 0, 1000);
#else
  status = 0;
#endif  
  if (status < 0)
    printf(CL_RED "Could not start write\n" CL_RESET);
  
  
  // Read the hex, writing to the bootloader in the callback
  uint8_t * buf = malloc(TSIZE+1); // byte 0 is the current buffer count! byte 1 is the start of the buffer
  struct _flash_context context = { 0, 0, buf, bootloader };
  ihex_read(hex, (ihex_readCallback*)_bootloader_didReadHexRecord, &context);
  
  free(buf);
}