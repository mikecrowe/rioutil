#include "rioi.h"
#include "id3.h"
#include <string.h>

int main(int argc, char *argv[])
{
    while (*++argv)
    {
	rio_file_t info;
	memset(&info, 0, sizeof(info));

	get_id3_info(*argv, &info, "UTF-8");

	printf("Title(UTF-8): '%s'\n", info.title);
	printf("Artist(UTF-8): '%s'\n", info.artist);

	get_id3_info(*argv, &info, "US-ASCII//TRANSLIT");
	printf("Title(ASCII): '%s'\n", info.title);
	printf("Artist(ASCII): '%s'\n", info.artist);

	get_id3_info(*argv, &info, "ISO-8859-1//TRANSLIT");
	printf("Title(ISO-8859-1): '%s'\n", info.title);
	printf("Artist(ISO-8859-1): '%s'\n", info.artist);

    }

    return 0;
}


