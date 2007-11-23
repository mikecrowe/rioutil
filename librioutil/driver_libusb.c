/*
  (c) 2002-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>

  v1.4

  Rio driver for use with libusb 0.1.6
*/

#include <stdio.h>
#include <errno.h>

#include "driver.h"

#if defined (WITH_LIBUSB) && defined (HAVE_LIBUSB)

int usb_open_rio (rios_t *rio, int number) {
  struct rioutil_usbdevice *plyr;

  struct usb_bus *bus = NULL;
  struct usb_device *dev = NULL;

  int current = 0, ret;
  struct player_device_info *p;

  struct usb_device *plyr_device = NULL;

  usb_init();

  usb_find_busses();
  usb_find_devices();

  /* find a suitable device based on device table and player number */
  for (bus = usb_busses ; bus ; bus = bus->next)
    for (dev = bus->devices ; dev ; dev = dev->next) {
      rio_log (rio, 0, "USB Device: idVendor = %08x, idProduct = %08x\n", dev->descriptor.idVendor,
	       dev->descriptor.idProduct);

      for (p = &player_devices[0] ; p->vendor_id ; p++) {
	if (dev->descriptor.idVendor == p->vendor_id &&
	    dev->descriptor.idProduct == p->product_id &&
	    current++ == number) {	  
	  plyr_device = dev;
	  goto done_looking;
	}
      }
    }

 done_looking:
  if (plyr_device == NULL || p->product_id == 0)
    return -1;

  plyr = (struct rioutil_usbdevice *) calloc (1, sizeof (struct rioutil_usbdevice));
  if (plyr == NULL) {
    perror ("rio_open");
    
    return errno;
  }

  plyr->entry = p;
  /* actually open the device */
  plyr->dev   = (void *) usb_open (plyr_device);
  if (plyr->dev == NULL)
    return -1;

  ret = usb_claim_interface (plyr->dev, 0);
  if (ret < 0) {
    usb_close (plyr->dev);
    free (plyr);

    return -1;
  }

  rio->dev    = (void *)plyr;

  rio_log (rio, 0, "Player found and opened.\n");

  return 0;
}

void usb_close_rio (rios_t *rio) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  usb_release_interface (dev->dev, 0);
  usb_close (dev->dev);

  free (dev);
}

/* direction is unused  here */
int control_msg(rios_t *rio, u_int8_t direction, u_int8_t request,
		u_int16_t value, u_int16_t index, u_int16_t length,
		unsigned char *buffer) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  unsigned char requesttype = 0;
  int ret;
  struct usb_dev_handle *ud;

  ud = dev->dev;

  requesttype = 0x80 | USB_TYPE_VENDOR | USB_RECIP_DEVICE;

  ret = usb_control_msg(ud, requesttype, request, value, index, (char *)buffer, length, 15000);

  if (ret == length)
    return URIO_SUCCESS;
  else
    return ret;
}

int write_bulk(rios_t *rio, unsigned char *buffer, u_int32_t buffer_size) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  int write_ep;
  struct usb_dev_handle *ud;

  write_ep = dev->entry->oep;
  ud = dev->dev;

  return usb_bulk_write(ud, write_ep, (char *)buffer, buffer_size, 10000);
}

int read_bulk(rios_t *rio, unsigned char *buffer, u_int32_t buffer_size){
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  int ret;
  int read_ep;
  struct usb_dev_handle *ud;

  read_ep = dev->entry->iep | 0x80;
  ud = dev->dev;

  ret = usb_bulk_read(ud, read_ep, (char *)buffer, buffer_size, 10000);
  if (ret < 0) {
    rio_log (rio, ret, "error reading from device (%i). resetting..\n", ret);
    rio_log (rio, ret, "size = %i\n", buffer_size);
    usb_reset (ud);
  }
  
  return ret;
}

void usb_setdebug (int i) {
  usb_set_debug (i);
}

#endif /* WITH_LIBUSB */
