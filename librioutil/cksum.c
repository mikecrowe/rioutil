/*
 * $Id: cksum.c,v 1.3 2004/01/10 09:20:19 hjelmn Exp $
 */

/*
 * Mike Touloumtzis <miket@bluemug.com> dissassembled the spRio600.dll
 * and noticed the magic value 0x04c11db7, which is the polynomial for
 * CRC-32.
 *
 * So it does appear to be using CRC-32, but in some mangled way.
 *
 * info on the crc32 algo. was obtained from:
 *
 * http://chesworth.com/pv/technical/crc_error_detection_guide.htm
 * and
 * http://www.embedded.com/internet/0001/0001connect.htm
 *
 */

#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif

#include "rio_internal.h"

#include <sys/types.h>
#include <stdlib.h>

#ifdef linux
#include <byteswap.h>
#include <endian.h>
#elif defined(__FreeBSD__) || defined(__MacOSX__)
#include <machine/endian.h>
#else
#include <sys/endian.h>
#include <sys/bswap.h>
#endif

#define CRC32POLY 	0x04C11DB7

/*
 * 1024 byte look up table.
 */
void crc32_init_table(void);
static u_int32_t *crc32_table = NULL;

void crc32_init_table(void)
{
	u_int32_t i, j, r;

	crc32_table = (u_int32_t *)malloc(sizeof(u_int32_t) * 256);
	
	for (i = 0 ; i < 256 ; i++)
	{
		r = i << 24;
		for (j = 0; j < 8; j++)
		{
			if (r & 0x80000000)
				r = (r << 1) ^ CRC32POLY;
			else
				r <<= 1;
		}
		crc32_table[i] = r;
	}
	return;
}

unsigned int crc32_rio(unsigned char *buf, unsigned int length)
{
    unsigned long crc = 0;
    int i;
	
    if (crc32_table == NULL)
	crc32_init_table();
	
    for (i = 0 ; i < length ; i++)
	crc = (crc<<8) ^ crc32_table[((crc >> 24) ^ buf[i]) & 0xff];
    
#if BYTE_ORDER == BIG_ENDIAN
    crc = bswap_32(crc);
#endif /* BIG_ENDIAN */

    return crc;
}
