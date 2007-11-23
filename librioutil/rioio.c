/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.4 rioio.c 
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

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "rio_internal.h"
#include "driver.h"

int read_block_rio (rios_t *rio, unsigned char *ptr, u_int32_t size) {
  int ret;
  char *buffer;

  buffer = (ptr) ? ptr : rio->buffer;

  ret = read_bulk (rio, buffer, size);
  if (ret < 0)
    return ret;

  if (rio->debug > 1 || (rio->debug > 0 && size <= 64)) {
    rio_log (rio, 0, "Dir: In\n");
    pretty_print_block(buffer, size);
  }
  
  return URIO_SUCCESS;
}

int write_cksum_rio (rios_t *rio, unsigned char *ptr, u_int32_t size, char *cksum_hdr) {
  unsigned int *intp;
  int ret;

  memset(rio->buffer, 0, 12);
  intp = (unsigned int *)rio->buffer;

  if (ptr != NULL)
    intp[2] = crc32_rio(ptr, size);
  
  bcopy (cksum_hdr, rio->buffer, 8);

  ret = write_bulk (rio, rio->buffer, 64);
  if (ret < 0) {
    return EWRITE;
  }
  
  if (rio->debug > 0) {
    rio_log (rio, 0, "Dir: Out\n");
    pretty_print_block(rio->buffer, 64);
  }

  return URIO_SUCCESS;
}

int write_block_rio (rios_t *rio, unsigned char *ptr, u_int32_t size, char *cksum_hdr) {
  int ret;

  if (!rio || !rio->dev)
    return -1;
  
  if (cksum_hdr != NULL) {
    if (rio->abort) {
      rio->abort = 0;
      rio_log (rio, 0, "aborting transfer\n");
      return -1;
    }

    if ((ret = write_cksum_rio (rio, ptr, size, cksum_hdr)) != URIO_SUCCESS)
      return ret;
  }
  
  ret = write_bulk (rio, ptr, size);
  if (ret < 0) {
    return EWRITE;
  }
  
  if ( ((rio->debug > 0) && (size <= 64)) || (rio->debug > 2) ) {
    rio_log (rio, 0, "Dir: Out\n");
    pretty_print_block(ptr, size);
  }
  
  if (cksum_hdr != NULL) {
    usleep(1000);
  }
  
  ret = read_block_rio (rio, NULL, 64);
  if (ret < 0) {
    return EWRITE;
  }
  
  if ( (cksum_hdr) && strstr(cksum_hdr, "CRIODATA") && (strstr(rio->buffer, "SRIODATA") == NULL) ) {
    rio_log (rio, EWRITE, "second SRIODATA not found\n");
    return EWRITE;
  }
  
  return URIO_SUCCESS;
}

/* all this command does is call control_msg but it allows to print debug without editing mutiple files */
int send_command_rio (rios_t *rio, int request, int value, int index) {
  static int cretry = 0;
  int ret = URIO_SUCCESS;

  if (cretry > 3)
    return ECOMMAND;
  else if (!rio || !rio->dev)
    return ENOINST;
  
  if (rio->debug > 1) {
    rio_log (rio, 0, "\nCommand:\n");
    rio_log (rio, 0, "len: 0x%04x rt: 0x%02x rq: 0x%02x va: 0x%04x id: 0x%04x\n", 
	     0x0c,
	     0x00,
	     request,
	     value,
	     index
	     );
  }

  if (control_msg(rio, RIO_DIR_IN, request, value, index, 0x0c, rio->cmd_buffer) < 0)
    return ECOMMAND;
  
  if (rio->debug > 1) {
    pretty_print_block(rio->cmd_buffer, 0xc);
  }

  if (rio->cmd_buffer[0] != 0x1 && request != 0x66) {
    cretry++;
    rio_log (rio, -1, "Device did not respond to command. Retrying..");

    ret = send_command_rio (rio, request, value, index);

    cretry = 0;
  }

  return ret;
}

int abort_transfer_rio(rios_t *rio) {
  int ret;
  
  /* what would brian boitano do? */
  memset(rio->buffer, 0, 12);
  sprintf(rio->buffer, "CRIOABRT");
  
  /* write an abort to the rio */
  ret = write_bulk (rio, rio->buffer, 64);
  if (ret < 0)
    return ret;
  
  ret = send_command_rio (rio, 0x66, 0, 0);
  if (ret < 0)
    return ret;
  
  return URIO_SUCCESS;
}
