/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.4 driver_usbdevfs.c 
 *
 *   Allows rioutil to communicate with the rio through linux usbdevfs.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *   
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *   
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

#include "driver.h"

#if defined(WITH_USBDEVFS)

#include <errno.h>

void usb_close_rio (rios_t *rio) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  riousb_close (dev->dev);
  
  free (dev);
}

int usb_open_rio (rios_t *rio, int number) {
  struct usbdevice *tmp;
  struct rioutil_usbdevice *plyr = NULL;
  struct player_device_info *p;
  
  int ret;

  if (number != 0) {
    int vendor_id, product_id;

    vendor_id = (number & 0xffff0000) >> 16;
    product_id = number & 0x0000ffff;
    
    tmp = riousb_open(vendor_id, product_id, 5000);
  } else {
    for (p = &player_devices[0] ; p->vendor_id ; p++)
      if ((tmp = riousb_open (p->vendor_id, p->product_id, 5000)) != NULL)
	break;
  }

  if (tmp) {
    plyr = (struct rioutil_usbdevice *) calloc (1, sizeof (struct rioutil_usbdevice));
    if (plyr == NULL) {
      riousb_close (tmp);

      return errno;
    }

    ret = riousb_claiminterface (tmp, 0);
    if (ret < 0) {
      riousb_close (tmp);

      return ret;
    }
    
    plyr->dev = tmp;
    plyr->entry = p;
  }

  rio->dev = (void *)plyr;
  
  return 0;
}

void usb_setdebug(int i) {
}

/* from librio500-usbdevfs.c */
int bulk (struct usbdevice *dev, int ep, unsigned char *buffer, u_int32_t size){
  int transmitted = 0;
  void *data;
  int len;
  int ret;

  do {
    len = size - transmitted;
    data = (unsigned char *)buffer + transmitted;

    /* The 15000 is a timeout value */
    ret = riousb_bulk_msg (dev, ep, len, data, 15000);
    if (ret < 0) {
      return ret;
    } else {
      transmitted += ret;
    }
  } while (transmitted < size);

  return transmitted;
}

int read_bulk (rios_t *rio, unsigned char *buffer, u_int32_t buffer_size){
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  int ret;
  struct usbdevice *ud;
  int read_ep;

  read_ep = dev->entry->iep | 0x80;
  ud = (struct usbdevice *)dev->dev;

  ret = bulk (ud, read_ep, buffer, buffer_size);
  if (ret < 0) {
    fprintf(stderr,"read_bulk error\n");
    if (errno == ETIMEDOUT) {
      rio_log (rio, 0, "Resetting device.\n");

      riousb_reset (ud);
    }
  }
  
  return ret;
}

int write_bulk (rios_t *rio, unsigned char *buffer, u_int32_t size){
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  int i;
  int j = 0;
  int ret = 0;
  int block_size = 4096;
  int transmitted = 0;
  struct usbdevice *ud;
  int write_ep;

  write_ep = dev->entry->oep;
  ud = (struct usbdevice *)dev->dev;

  if (size < block_size)
    return bulk(ud, write_ep, buffer, size);

  /* need to break up the data, but i am not sure why we are limited to
     only 4096 bytes at a time */
  for (i = size ; i > 0 ; i -= block_size, j++) {
    ret = bulk (ud, write_ep, &buffer[j * block_size], (i < block_size) ? i : block_size);
    if (ret < 0) {
      rio_log (rio, ret, "write_bulk error\n");
      return ret;
    } else
      transmitted += ret;
  }

  return transmitted;
}

/* from rio500-usbdevfs.c */
int control_msg (rios_t *rio, u_int8_t direction, u_int8_t request,
		 u_int16_t value, u_int16_t index, u_int16_t length,
		 unsigned char *buffer) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  int ret;

  unsigned char requesttype = 0;
  struct usbdevice *ud;

  ud = (struct usbdevice *)dev->dev;

  if (direction == RIO_DIR_IN)
    requesttype = USB_DIR_IN |
      USB_TYPE_VENDOR | USB_RECIP_DEVICE;
  else
    requesttype = USB_DIR_OUT |
      USB_TYPE_VENDOR | USB_RECIP_DEVICE;

  /* The 5000 is a timeout value */
  ret = riousb_control_msg(ud, requesttype, request, value, index, length, buffer, 15000);
  if (ret < 0) {
    rio_log (rio, ret, "control_msg error. request=0x%x\n",request);
  }

  return ret;
}

#endif /* WITH_USBDEVFS */
