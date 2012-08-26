#include "rioi.h"
#include "id3.h"

int main(int argc, char *argv[])
{
    while (*++argv)
    {
	rio_file_t info;
	memset(&info, 0, sizeof(info));

	get_id3_info(*argv, &info);

	printf("Title: '%s'\n", info.title);
    }

    return 0;
}


