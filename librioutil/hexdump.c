/*
 * $Id: hexdump.c,v 1.1.1.1 2001/12/19 20:03:15 hjelmn Exp $
 *
 * Pretty print a buffer with ASCII.
 *
 */

#include <stdio.h>

#include "rio_internal.h"

void pretty_print_block(unsigned char *b, int len){
    int x, y, indent, count = 0;
    
    indent = 16; /* whatever */
    
    fputc('\n', stderr);
    
    while (count < len){
	fprintf(stderr, "%04x : ", count);
	for (x = 0 ; x < indent ; x++){
	    fprintf(stderr, "%02x ", b[x + count]);
	    if ((x + count + 1) >= len){
		x++;
		for (y = 0 ; y < (indent - x) ; y++)
		    fprintf(stderr, "   ");
		break;
	    }
	}
	fprintf(stderr, ": ");
	
	for (x = 0 ; x < indent ; x++){
	    if (isprint(b[x + count]))
		fputc(b[x + count], stderr);
	    else
		fputc('.', stderr);
	    
	    if ((x + count + 1) >= len){
		x++;
		for (y = 0 ; y < (indent - x) ; y++)
		    fputc(' ', stderr);
		break;
	    }
	}

	fputc('\n', stderr);
	count += indent;
    }

    fputc('\n', stderr);
}

