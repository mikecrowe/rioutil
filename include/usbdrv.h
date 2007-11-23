/*****************************************************************************/

/*
 *	usbdrv.h  --  Linux USB driver interface.
 *
 *	Copyright (C) 1999-2000
 *          Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 *  $Id: usbdrv.h,v 1.5 2004/06/07 16:31:48 hjelmn Exp $	 
 *
 *  History:
 *   0.1  07.01.2000  Created
 *
 */

/*****************************************************************************/

#ifndef _USBDRV_H
#define _USBDRV_H

/* --------------------------------------------------------------------- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>

#include "driver.h"

struct usbdevice {
  int fd;
  struct usb_device_descriptor_x desc;
};

#ifdef HAVE_LINUX_USBDEVICE_FS_H
#include <linux/types.h>
#include <linux/usbdevice_fs.h>
#else
#include <linux/types.h>
#include "usbdevice_fs.h"
#endif

/* --------------------------------------------------------------------- */

struct usb_device_descriptor {
        u_int8_t  bLength;
        u_int8_t  bDescriptorType;
        u_int8_t  bcdUSB[2];
        u_int8_t  bDeviceClass;
        u_int8_t  bDeviceSubClass;
        u_int8_t  bDeviceProtocol;
        u_int8_t  bMaxPacketSize0;
        u_int8_t  idVendor[2];
        u_int8_t  idProduct[2];
        u_int8_t  bcdDevice[2];
        u_int8_t  iManufacturer;
        u_int8_t  iProduct;
        u_int8_t  iSerialNumber;
        u_int8_t  bNumConfigurations;
};

//#define USB_DT_DEVICE_SIZE sizeof(struct usb_device_descriptor)

/* --------------------------------------------------------------------- */

extern char const *usb_devicefs_mountpoint;

extern void riousb_show_device_descriptor(FILE *f, struct usb_device_descriptor *desc);
extern void riousb_close(struct usbdevice *dev);
extern struct usbdevice *riousb_open_bynumber(unsigned int busnum, unsigned int devnum, int vendorid, int productid);
extern struct usbdevice *riousb_open(int vendorid, int productid, unsigned int timeout);
extern int riousb_control_msg(struct usbdevice *dev, unsigned char requesttype, unsigned char request,
			  unsigned short value, unsigned short index, unsigned short length, void *data, unsigned int timeout);
extern int riousb_bulk_msg(struct usbdevice *dev, unsigned int ep, unsigned int dlen, void *data, unsigned int timeout);
extern int riousb_resetep(struct usbdevice *dev, unsigned int ep);
extern int riousb_reset(struct usbdevice *dev);
extern int riousb_setconfiguration(struct usbdevice *dev, unsigned int configuration);
extern int riousb_setinterface(struct usbdevice *dev, unsigned int intf, unsigned int altsetting);
extern int riousb_getdevicedescriptor(struct usbdevice *dev, struct usb_device_descriptor *desc);
extern int riousb_claiminterface(struct usbdevice *dev, unsigned int intf);
extern int riousb_releaseinterface(struct usbdevice *dev, unsigned int intf);


/* --------------------------------------------------------------------- */
#endif /* _USBDRV_H */
