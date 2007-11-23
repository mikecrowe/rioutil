/*****************************************************************************/

/*
 *	usbdrvlinux.c  --  Linux USB driver interface.
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
 *  $Id: usbdrvlinux.c,v 1.4 2003/08/06 15:56:25 hjelmn Exp $	 
 *
 *  History:
 *   0.1  23.06.1999  Created
 *   0.2  07.01.2000  Expanded to usbdevfs capabilities
 *
 */

/*****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef WITH_USBDEVFS

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>

#if BYTE_ORDER == BIG_ENDIAN
# include <byteswap.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "usbdevfs.h"

/* --------------------------------------------------------------------- */

const char *usbbus = "/proc/bus/usb/";

/* --------------------------------------------------------------------- */

/*
 * Parse and show the different USB descriptors.
 */
void riousb_show_device_descriptor(FILE *f, struct usb_device_descriptor *desc)
{
        fprintf(f, "  Length              = %2d%s\n", desc->bLength,
		desc->bLength == USB_DT_DEVICE_SIZE ? "" : " (!!!)");
        fprintf(f, "  DescriptorType      = %02x\n", desc->bDescriptorType);
        fprintf(f, "  USB version         = %x.%02x\n",
	       desc->bcdUSB[1], desc->bcdUSB[0]);
        fprintf(f, "  Vendor:Product      = %02x%02x:%02x%02x\n",
	       desc->idVendor[1], desc->idVendor[0], desc->idProduct[1], desc->idProduct[0]);
        fprintf(f, "  MaxPacketSize0      = %d\n", desc->bMaxPacketSize0);
        fprintf(f, "  NumConfigurations   = %d\n", desc->bNumConfigurations);
        fprintf(f, "  Device version      = %x.%02x\n",
		desc->bcdDevice[1], desc->bcdDevice[0]);
        fprintf(f, "  Device Class:SubClass:Protocol = %02x:%02x:%02x\n",
		desc->bDeviceClass, desc->bDeviceSubClass, desc->bDeviceProtocol);
        switch (desc->bDeviceClass) {
        case 0:
                fprintf(f, "    Per-interface classes\n");
                break;
        case 9:
                fprintf(f, "    Hub device class\n");
                break;
        case 0xff:
                fprintf(f, "    Vendor class\n");
                break;
        default:
                fprintf(f, "    Unknown class\n");
        }
}

/* ---------------------------------------------------------------------- */

void riousb_close(struct usbdevice *dev)
{
	if (!dev)
		return;
	if (close(dev->fd) != 0)
            fprintf(stderr, "riousb_close error: %s (%d)\n", strerror(errno), errno);
	free(dev);
}

static int parsedev(int fd, unsigned int *bus, unsigned int *dev, int vendorid, int productid)
{
	char buf[16384];
	char *start, *end, *lineend, *cp;
	int devnum = -1, busnum = -1, vendor = -1, product = -1;
	int ret;

	if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
		return -1;
	ret = read(fd, buf, sizeof(buf)-1);
        if (ret == -1)
                return -1;
        end = buf + ret;
        *end = 0;
        start = buf;
	ret = 0;
        while (start < end) {
                lineend = strchr(start, '\n');
                if (!lineend)
                        break;
                *lineend = 0;
                switch (start[0]) {
		case 'T':  /* topology line */
                        if ((cp = strstr(start, "Dev#="))) {
                                devnum = strtoul(cp + 5, NULL, 0);
                        } else
                                devnum = -1;
                        if ((cp = strstr(start, "Bus="))) {
                                busnum = strtoul(cp + 4, NULL, 0);
                        } else
                                busnum = -1;
			break;

                case 'P':
                        if ((cp = strstr(start, "Vendor="))) {
                                vendor = strtoul(cp + 7, NULL, 16);
                        } else
                                vendor = -1;
                        if ((cp = strstr(start, "ProdID="))) {
                                product = strtoul(cp + 7, NULL, 16);
                        } else
                                product = -1;
			if (vendor != -1 && product != -1 && devnum >= 1 && devnum <= 127 &&
			    busnum >= 0 && busnum <= 999 &&
			    (vendorid == vendor || vendorid == -1) &&
			    (productid == product || productid == -1)) {
				if (bus)
					*bus = busnum;
				if (dev)
					*dev = devnum;
				ret++;
			}
			break;
		}
                start = lineend + 1;
	}
	return ret;
}

struct usbdevice *riousb_open_bynumber(unsigned int busnum, unsigned int devnum, int vendorid, int productid)
{
	struct usbdevice *dev;
        struct usb_device_descriptor_x desc;
	unsigned int vid, pid;
	char devsfile[256];
	int ret, fd;

	snprintf(devsfile, sizeof(devsfile), "%s/%03u/%03u", usbbus, busnum, devnum);
	if ((fd =  open(devsfile, O_RDWR)) == -1){
	  printf("Could not open %s\n", devsfile);
		return NULL;
	}
	if ((ret = read(fd, &desc, sizeof(desc))) != sizeof(desc)) {
		if (ret > 0)
			errno = EIO;
		close(fd);
		return NULL;
	}
	vid = desc.idVendor[0] | (desc.idVendor[1] << 8);
	pid = desc.idProduct[0] | (desc.idProduct[1] << 8);

#if BYTE_ORDER == BIG_ENDIAN
	vid = bswap_16(vid);
	pid = bswap_16(pid);
#endif

	if ((vid != vendorid && vendorid == -1) ||
	    (pid != productid && productid == -1)) {
		errno = -ENOENT;
		close(fd);
		return NULL;
	}
	if (!(dev = malloc(sizeof(struct usbdevice)))) {
		close(fd);
		return NULL;
	}
	dev->fd = fd;
	dev->desc = desc;

	return dev;
}

struct usbdevice *riousb_open(int vendorid, int productid, unsigned int timeout)
{
        struct usb_device_descriptor_x desc;
	struct dirent *de, *de2;
	DIR *d, *d2;
	int fd;
	unsigned char buf[256];
        unsigned int vid, pid;
	struct stat statbuf;

        if (stat(usbbus, &statbuf)) {
                fprintf(stderr, "cannot open %s, %s (%d)\n", usbbus, strerror(errno), errno);
                return NULL;
        }
        if (!S_ISDIR(statbuf.st_mode)) {
                if ((fd = open(usbbus, O_RDWR)) == -1) {
                        fprintf(stderr, "cannot open %s, %s (%d)\n", usbbus, strerror(errno), errno);
                        return NULL;
                }
		printf("Here's where its failing");
                return NULL;
        }

        d = opendir(usbbus);
        if (!d) {   
                fprintf(stderr, "cannot open %s, %s (%d)\n", usbbus, strerror(errno), errno);
                return NULL;
        }

        while ((de = readdir(d))) {
                if (de->d_name[0] < '0' || de->d_name[0] > '9')
                        continue;
                snprintf(buf, sizeof(buf), "%s%s/", usbbus, de->d_name);
                if (!(d2 = opendir(buf)))
                        continue;
                while ((de2 = readdir(d2))) {
                        if (de2->d_name[0] == '.')
                                continue;
                        snprintf(buf, sizeof(buf), "%s%s/%s", usbbus, de->d_name, de2->d_name);
                        if ((fd = open(buf, O_RDWR)) == -1) {
                                fprintf(stderr, "cannot open %s, %s (%d)\n", buf, strerror(errno), errno);
                                continue;
                        }
                        if (read(fd, &desc, sizeof(desc)) != sizeof(desc)) {
                                fprintf(stderr, "cannot read device descriptor %s (%d)\n", strerror(errno), errno);
                                close(fd);
                                continue;
                        }

		        vid = desc.idVendor[0] | (desc.idVendor[1] << 8);
		        pid = desc.idProduct[0] | (desc.idProduct[1] << 8);

#if BYTE_ORDER == BIG_ENDIAN
			vid = bswap_16(vid);
			pid = bswap_16(pid);
#endif
			
                        if ((vid == vendorid || vendorid == 0xffff) &&
                            (pid == productid || productid == 0xffff)) {
			  return riousb_open_bynumber(strtoul(de->d_name, NULL, 0), atoi(de2->d_name), vendorid, productid);
                        }
                        close(fd);
                }
                closedir(d2);
        }
        closedir(d);

	return NULL;
}

int riousb_control_msg(struct usbdevice *dev, unsigned char requesttype, unsigned char request,
		    unsigned short value, unsigned short index, unsigned short length, void *data, unsigned int timeout)
{
        struct usbdevfs_ctrltransfer ctrl;
	int i;

	ctrl = (struct usbdevfs_ctrltransfer){ requesttype, request, value, index, length, 5000, data };
	i = ioctl(dev->fd, USBDEVFS_CONTROL, &ctrl);

	if (i < 0) {
          fprintf(stderr, "riousb_control_msg error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return i;
}

int riousb_bulk_msg(struct usbdevice *dev, unsigned int ep, unsigned int dlen, void *data, unsigned int timeout)
{
	struct usbdevfs_bulktransfer bulk;
	int i;

	bulk = (struct usbdevfs_bulktransfer){ ep, dlen, timeout, data };
	i = ioctl(dev->fd, USBDEVFS_BULK, &bulk);

	if (i < 0) {
          fprintf(stderr, "riousb_bulk_msg error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return i;
}

int riousb_resetep(struct usbdevice *dev, unsigned int ep)
{
	int i;

	i = ioctl(dev->fd, USBDEVFS_RESETEP, &ep);
	if (i < 0) {
          fprintf(stderr, "riousb_resetep error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return 0;
}

int riousb_reset(struct usbdevice *dev)
{
	int i;

	i = ioctl(dev->fd, USBDEVFS_RESET);
	if (i < 0) {
          fprintf(stderr, "riousb_reset error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return 0;
}

int riousb_setconfiguration(struct usbdevice *dev, unsigned int configuration)
{
	int i;

	i = ioctl(dev->fd, USBDEVFS_SETCONFIGURATION, &configuration);
	if (i < 0) {
          fprintf(stderr, "riousb_setconfiguration error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return 0;
}

int riousb_setinterface(struct usbdevice *dev, unsigned int intf, unsigned int altsetting)
{
	struct usbdevfs_setinterface setif;
	int i;

	setif = (struct usbdevfs_setinterface) { intf, altsetting };
	i = ioctl(dev->fd, USBDEVFS_SETINTERFACE, &setif);
	if (i < 0) {
          fprintf(stderr, "riousb_setinterface error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return 0;
}

int riousb_getdevicedescriptor(struct usbdevice *dev, struct usb_device_descriptor *desc)
{
	if (desc)
		memcpy(desc, &dev->desc, sizeof(*desc));
	return 0;
}

int riousb_claiminterface(struct usbdevice *dev, unsigned int intf)
{
	int i;

	i = ioctl(dev->fd, USBDEVFS_CLAIMINTERFACE, &intf);
	if (i < 0) {
          fprintf(stderr, "riousb_claiminterface error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return 0;
}

int riousb_releaseinterface(struct usbdevice *dev, unsigned int intf)
{
	int i;

	i = ioctl(dev->fd, USBDEVFS_RELEASEINTERFACE, &intf);
	if (i < 0) {
          fprintf(stderr, "riousb_releaseinterface error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return 0;
}

int riousb_discsignal(struct usbdevice *dev, unsigned int signr, void *context)
{
	struct usbdevfs_disconnectsignal s = { signr, context };
	int i;

	i = ioctl(dev->fd, USBDEVFS_DISCSIGNAL, &s);
	if (i < 0) {
          fprintf(stderr, "riousb_discsignal error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return 0;
}

int riousb_submiturb(struct usbdevice *dev, struct usbdevfs_urb *urb)
{
	int i;

	i = ioctl(dev->fd, USBDEVFS_SUBMITURB, urb);
	if (i < 0) {
          fprintf(stderr, "riousb_submiturb error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return 0;
}

int riousb_discardurb(struct usbdevice *dev, struct usbdevfs_urb *urb)
{
	int i;

	i = ioctl(dev->fd, USBDEVFS_DISCARDURB, urb);
	if (i < 0) {
          fprintf(stderr, "riousb_discardurb error: %s (%d)\n", strerror(errno), errno);
	  return -1;
        }

	return 0;
}

struct usbdevfs_urb *riousb_reapurb(struct usbdevice *dev, unsigned int nonblock)
{
	int ret;
	struct usbdevfs_urb *urb;

	ret = ioctl(dev->fd, nonblock ? USBDEVFS_REAPURBNDELAY : USBDEVFS_REAPURB, &urb);
	if (ret < 0) {
          fprintf(stderr, "riousb_reapurb error: %s (%d)\n", strerror(errno), errno);
	  return NULL;
        }
	return urb;
}

/* ---------------------------------------------------------------------- */
#endif /*usbdevfs*/
