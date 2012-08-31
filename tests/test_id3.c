#include "rioi.h"
#include "id3.h"
#include <string.h>

static int errors;

void check(const char *name, const char *got, const char *expected)
{
    if (strcmp(got, expected))
    {
	fprintf(stderr, "%s: got '%s'\n", name, got);
	fprintf(stderr, "%s: expected '%s'\n", name, expected);
	++errors;
    }
}

struct File
{
    const char *filename;
    const char *utf8_title;
    const char *utf8_artist;
    const char *latin1_title;
    const char *latin1_artist;
};

struct File files[] =
{
    {
	"unicode.mp3",
	"µSD £500 ©2001ばんごはん。", "Café être",
	"\xb5""SD \xa3""500 \xa9""2001??????", "Caf\xe9 \xeatre"
    },
    {
	"latin1.mp3",
	"Café", "£500",
	"Caf\xe9", "\xa3""500"
    },
};
const file_count = sizeof(files)/sizeof(files[0]);

int main()
{
    struct File *f;
    for(f = files; f != (files + file_count); ++f)
    {
	rio_file_t info;
	memset(&info, 0, sizeof(info));

	get_id3_info((char *)f->filename, &info, "UTF-8");
	check("UTF8-title", info.title, f->utf8_title);
	check("UTF8-artist", info.artist, f->utf8_artist);

	get_id3_info((char *)f->filename, &info, "ISO-8859-1//TRANSLIT");
	check("UTF8-title", info.title, f->latin1_title);
	check("UTF8-artist", info.artist, f->latin1_artist);
    }

    return errors;
}

int main2(int argc, char *argv[])
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


