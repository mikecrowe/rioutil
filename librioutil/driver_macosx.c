/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.4 driver_macosx.c 
 *
 *   Allows rioutil to communicate with the rio through Mac OS X.
 *
 *   02/11/2002:
 *     - Fixed several glaring bugs.
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

#if defined(__MacOSX__)

#include <unistd.h>
#include <IOKit/IOCFPlugIn.h>

/* this variable is needed by both usb_open and usb_claiminterface */
CFRunLoopSourceRef runLoopSource;
IONotificationPortRef gNotifyPort;

static int osx_debug = 0;

struct rioutil_usbdevice *usb_open(UInt32 idVendor, UInt32 idProduct, UInt32 timeout){
  io_iterator_t deviceIterator;
  io_service_t usbDevice;
  mach_port_t masterPort;

  IOCFPlugInInterface **plugInInterface = NULL;

  CFMutableDictionaryRef matchingDict;

  IOReturn result;
  SInt32 score;
  UInt16 vendor, product;

  struct rioutil_usbdevice *dev;

  /* Create a master port for communication with IOKit */
  result = IOMasterPort(MACH_PORT_NULL, &masterPort);
  if (result || !masterPort)
    return NULL;

  /* set up the matching dictionary for class IOUSBDevice and it's
     subclasses */
  if((matchingDict = IOServiceMatching(kIOUSBDeviceClassName)) == NULL){
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = NULL;

    return NULL;
  }

  /* add the device to the matching dictionary */
  CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorName),
		       CFNumberCreate(kCFAllocatorDefault,
				      kCFNumberSInt32Type, &idVendor));
  CFDictionarySetValue(matchingDict, CFSTR(kUSBProductName),
		       CFNumberCreate(kCFAllocatorDefault,
				      kCFNumberSInt32Type, &idProduct));
  
  gNotifyPort = IONotificationPortCreate(masterPort);
  runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource,
		     kCFRunLoopDefaultMode);

  matchingDict = (CFMutableDictionaryRef) CFRetain(matchingDict);
  matchingDict = (CFMutableDictionaryRef) CFRetain(matchingDict);
  matchingDict = (CFMutableDictionaryRef) CFRetain(matchingDict);

  result = IOServiceAddMatchingNotification(gNotifyPort,
					    kIOFirstMatchNotification,
					    matchingDict, NULL, NULL,
					    &deviceIterator);

  dev = (struct rioutil_usbdevice *)malloc(sizeof(struct rioutil_usbdevice));

  /* find device */
  while (usbDevice = IOIteratorNext(deviceIterator)){
    /* Create an intermediate plug-in */
    result = IOCreatePlugInInterfaceForService(usbDevice,
					       kIOUSBDeviceUserClientTypeID,
					       kIOCFPlugInInterfaceID,
					       &plugInInterface,
					       &score);

    result = IOObjectRelease(usbDevice);
    if (result || !plugInInterface)
      continue;

    (*plugInInterface)->QueryInterface(plugInInterface,
				       CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
				       (LPVOID)&dev->device);

    if (!dev->device)
      continue;

    result = (*(dev->device))->GetDeviceVendor(dev->device, &vendor);
    result = (*(dev->device))->GetDeviceProduct(dev->device, &product);
    if ((vendor != idVendor) || (product != idProduct))
      continue;

    result = (*(dev->device))->USBDeviceOpenSeize(dev->device);
    if (result)
      continue;

    /* right vendor and product and it opened, device is ready to go */
    IOObjectRelease(deviceIterator);
    return dev;
  }
  
  free(dev);
  return NULL;
}

void usb_close(struct rioutil_usbdevice *dev){
  if (dev == NULL)
    return;

  usb_releaseinterface (dev, 0);

  if (dev->device != NULL) {
    (*(dev->device))->USBDeviceClose(dev->device);
    (*(dev->device))->Release(dev->device);
  }

  dev->device = NULL;

  IONotificationPortDestroy(gNotifyPort);
}

int usb_claiminterface(struct rioutil_usbdevice *dev, int interface){
  io_iterator_t iterator;
  io_service_t  usbInterface;  

  IOReturn kernResult, result;
  IOUSBFindInterfaceRequest request;
  IOCFPlugInInterface **plugInInterface = NULL;

  SInt32 score;

  request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
  request.bAlternateSetting = kIOUSBFindInterfaceDontCare;

  (*(dev->device))->CreateInterfaceIterator(dev->device, &request, &iterator);
  
  usbInterface = IOIteratorNext(iterator);

  kernResult = IOCreatePlugInInterfaceForService(usbInterface,
						 kIOUSBInterfaceUserClientTypeID,
						 kIOCFPlugInInterfaceID,
						 &plugInInterface, &score);

  //No longer need the usbInterface object after getting the plug-in
  kernResult = IOObjectRelease(usbInterface);
  if ((kernResult != kIOReturnSuccess) || !plugInInterface)
    return -1;
  
  //Now create the device interface for the interface
  result = (*plugInInterface)->QueryInterface(plugInInterface,
					      CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
					      (LPVOID) &dev->interface);
  //No longer need the intermediate plug-in
  (*plugInInterface)->Release(plugInInterface);

  if (result || !dev->interface)
    return -1;
  
  /* claim the interface */
  kernResult = (*(dev->interface))->USBInterfaceOpenSeize(dev->interface);
  if (kernResult)
    return -1;

  IOObjectRelease(iterator);

  /* interface is claimed and async IO is set up return 0 */
  return 0;
}

int usb_releaseinterface(struct rioutil_usbdevice *dev, int interface){
  if (dev->interface){
    (*(dev->interface))->USBInterfaceClose(dev->interface);
    (*(dev->interface))->Release(dev->interface);
  }

  dev->interface = NULL;

  return 0;
}

int usb_control_msg(struct rioutil_usbdevice *dev, UInt16 request, UInt16 value, UInt16 index,
		    UInt16 length, unsigned char *data){
  IOUSBDevRequestTO urequest;

  urequest.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBVendor, kUSBDevice);
  urequest.bRequest = request;
  urequest.wValue = value;
  urequest.wIndex = index;
  urequest.wLength = length;
  urequest.pData = data;

  urequest.noDataTimeout = 0;
  urequest.completionTimeout = 5000;

  if((*(dev->device))->DeviceRequestTO(dev->device, &urequest) == kIOReturnSuccess)
    return 0;
  else
    return -1;
}

void compl(){
}

static int usb_bulk_msg(struct rioutil_usbdevice *dev, UInt8 direction, UInt16 pipe,
			unsigned char *data, UInt32 size) {
  IOReturn ret;
  UInt32 timeout = 10000;

  switch(direction){
  case kUSBIn:
    ret = (*(dev->interface))->WritePipeTO(dev->interface, pipe, data, size,
					   0, timeout);
    break;
  case kUSBOut:
    ret = (*(dev->interface))->ReadPipeTO(dev->interface, pipe, data, &size,
					  0, timeout);
    break;
  default:
    return -1;
  }

  if (ret == kIOReturnSuccess)
    return 0;
  else
    return -1;
}

int write_bulk(rios_t *rio, unsigned char *data, u_int32_t size){
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  return usb_bulk_msg(dev->dev, kUSBIn, 0x2, data, size);
}

int read_bulk(rios_t *rio, unsigned char *data, u_int32_t size){
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  int ret = usb_bulk_msg(dev->dev, kUSBOut, 0x1, data, size);

  if (ret < 0) {
    rio_log (rio, ret, "error reading from device. resetting...\n");

    (*(dev->device))->ResetDevice(dev->device);
  }

  return ret;
}

int control_msg(rios_t *rio, u_int8_t direction, u_int8_t request,
		u_int16_t value, u_int16_t index, u_int16_t length,
		unsigned char *buffer){
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  return usb_control_msg(dev->dev, request, value, index, length, buffer);
}

int usb_open_rio (rios_t *rio, int number) {
  int i;
  struct rioutil_usbdevice *plyr;
  struct player_device_info *p;
  void *dev;
  
  for (p = &player_devices[0] ; p->vendor_id ; p++)
    if ((dev = usb_open (p->vendor_id, p->product_id, 5000)) != NULL)
      break;
    
  if (p->vendor_id == 0)
    return -1;

  plyr = (struct rioutil_usbdevice *) calloc (1, sizeof (struct rioutil_usbdevice));
  plyr->dev   = dev;
  plyr->entry = p;

  usb_claiminterface (dev, 0);

  rio->dev = plyr;

  return 0;
}

void usb_close_rio (rios_t *rio) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  usbclose (dev->dev);
  free (dev);
}

void usb_setdebug(int i) {
  osx_debug = i;
}
#endif /* __MacOSX__ */
