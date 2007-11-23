/**
 *   (c) 2003 Nathan Hjelm <hjelmn@unm.edu>
 *   v1.4 driver.h
 *
 *   usb drivers header file
 *   
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *   
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *   
 *   You should have received a copy of the GNU Library Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

#if !defined (_DRIVER_H)
#define _DRIVER_H

#include "config.h"
#include "rio.h"

/* supported rio devices */
#define VENDOR_DIAMOND01 0x045a

/* flash players */
#define PRODUCT_RIO600   0x5001
#define PRODUCT_RIO800   0x5002
#define PRODUCT_PSAPLAY  0x5003

#define PRODUCT_RIOS10   0x5005
#define PRODUCT_RIOS50   0x5006
#define PRODUCT_RIOS35   0x5007
#define PRODUCT_RIO900   0x5008
#define PRODUCT_RIOS30   0x5009
#define PRODUCT_FUSE     0x500d
#define PRODUCT_CHIBA    0x500e
#define PRODUCT_CALI     0x500f
#define PRODUCT_RIOS11   0x5010

/* hard drive players */
#define PRODUCT_RIORIOT  0x5202

/* I and read  are device->computer.
   O and write are computer->device. */
struct player_device_info {
  int vendor_id;
  int product_id;
  int iep;
  int oep;
  int type;
};

static struct player_device_info player_devices[] = {
  /* Rio 600/800/900 and Nike psa[play Use bulk endpoint 2 for read/write */
  {VENDOR_DIAMOND01, PRODUCT_RIO600 , 0x2, 0x2, RIO600   },
  {VENDOR_DIAMOND01, PRODUCT_RIO800 , 0x2, 0x2, RIO800   },
  {VENDOR_DIAMOND01, PRODUCT_PSAPLAY, 0x2, 0x2, PSAPLAY  },
  {VENDOR_DIAMOND01, PRODUCT_RIO900 , 0x2, 0x2, RIO900   },
  /* Rio S-Series Uses bulk endpoint 2 for read and 1 for write */
  {VENDOR_DIAMOND01, PRODUCT_RIOS30 , 0x1, 0x2, RIOS30   },
  {VENDOR_DIAMOND01, PRODUCT_RIOS35 , 0x1, 0x2, RIOS35   },
  {VENDOR_DIAMOND01, PRODUCT_RIOS10 , 0x1, 0x2, RIOS10   },
  {VENDOR_DIAMOND01, PRODUCT_RIOS50 , 0x1, 0x2, RIOS50   },
  {VENDOR_DIAMOND01, PRODUCT_FUSE   , 0x1, 0x2, RIOFUSE  },
  {VENDOR_DIAMOND01, PRODUCT_CHIBA  , 0x1, 0x2, RIOCHIBA },
  {VENDOR_DIAMOND01, PRODUCT_CALI   , 0x1, 0x2, RIOCALI  },
  {VENDOR_DIAMOND01, PRODUCT_RIOS11 , 0x1, 0x2, RIOS11   },
  /* Rio Riot Uses bulk endpoint 1 for read and 2 for write */
  {VENDOR_DIAMOND01, PRODUCT_RIORIOT, 0x2, 0x1, RIORIOT  },
  {0}
};

struct rioutil_usbdevice {
  void *dev;
  struct player_device_info *entry;
};

/* defined for WITH_USBDEVFS below */
#if defined(WITH_USBDEVFS)
#include <asm/types.h>
struct usb_device_descriptor_x {
  __u8  bLength;
  __u8  bDescriptorType;
  __u8  bcdUSB[2];
  __u8  bDeviceClass;
  __u8  bDeviceSubClass;
  __u8  bDeviceProtocol;
  __u8  bMaxPacketSize0;
  __u8  idVendor[2];
  __u8  idProduct[2];
  __u8  bcdDevice[2];
  __u8  iManufacturer;
  __u8  iProduct;
  __u8  iSerialNumber;
  __u8  bNumConfigurations;
};

#include "usbdevfs.h"

static char driver_method[] = "usbdevfs";

/* END -- defined WITH_USBDEVFS */

/* defined for WITH_LIBUSB below */
#elif defined(WITH_LIBUSB)

#include <usb.h>

static char driver_method[] = "libusb";

/* END -- defined WITH_LIBUSB */

/* defined for MacOSX below */
#elif defined(__MacOSX__)

#include <IOKit/IOCFBundle.h>
#include <IOKit/usb/IOUSBLib.h>

struct usbdevice {
  IOUSBDeviceInterface182 **device;
  IOUSBInterfaceInterface183 **interface;
};

#include <machine/types.h>

static char driver_method[] = "OS X";

/* END -- defined MacOSX */

/* default driver definitions */
#else
static char driver_method[] = "built-in";
#endif

#include "rio_usb.h"

int  usb_open_rio  (rios_t *rio, int number);
void usb_close_rio (rios_t *rio);

int  read_bulk  (rios_t *rio, unsigned char *buffer, u_int32_t size);
int  write_bulk (rios_t *rio, unsigned char *buffer, u_int32_t size);
int  control_msg(rios_t *rio, u_int8_t direction, u_int8_t request,
		 u_int16_t value, u_int16_t index, u_int16_t length,
		 unsigned char *buffer);

void usb_setdebug(int);
#endif
