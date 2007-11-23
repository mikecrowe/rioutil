/*
  (c) 2001 Nathan Hjelm <hjelmn@unm.edu>

  Based off of usbdevs support in rio500-0.7 <rio500.sourceforge.net>
*/

#ifndef __USBDEVFS_H
#define __USBDEVFS_H

#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(WITH_USBDEVFS)

#if defined(HAVE_LINUX_USBDEVICE_FS_H)

#if defined(__cplusplus)
extern "C" {
#endif

#include <linux/types.h>
#include <linux/usbdevice_fs.h>

#if defined(__cplusplus)
}
#endif

#endif /* defined(HAVE_LINUX_USBDEVICE_FS_H) */

#include <linux/usb.h>

#define USBRIO_BULK_OUT 0x1
#define USBRIO_BULK_IN  0x2

#define USBRIO600_BULK_OUT 0x2

#if defined(__cplusplus)
extern "C" {
#endif

#include "usbdrv.h"

#if defined(__cplusplus)
}
#endif

#endif /* defined(WITH_USBDEVFS) */

#endif /* __USBDEVFS_H */
