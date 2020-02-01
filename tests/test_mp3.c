#include "rioi.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main()
{
    int errors = 0;
    long frame_len;
    void *frame_buffer;
    const char temp_filename[] = "temp.mp3";
    const long file_size = INT_MAX/29;
    const int expected_time = 3358;

    {
	FILE *frame_file = fopen("frame.mp3", "r");
	if (!frame_file) {
	    perror("Unable to open frame file\n");
	    return 1;
	}

	if (fseek(frame_file, 0, SEEK_END) < 0) {
	    perror("Unable to seek in frame file\n");
	    fclose(frame_file);
	    return 1;
	}

	frame_len = ftell(frame_file);
	frame_buffer = malloc(frame_len);

	rewind(frame_file);

	if (fread(frame_buffer, frame_len, 1, frame_file) < 1) {
	    perror("Unable to read frame file\n");
	    fclose(frame_file);
	    return 1;
	}

	fclose(frame_file);
    }

    {
	FILE *test_file = fopen(temp_filename, "w");
	if (!test_file) {
	    perror("Unable to open test file\n");
	    return 1;
	}

	while (ftell(test_file) < file_size) {
	    if (fwrite(frame_buffer, frame_len, 1, test_file) < 0) {
		perror("Failed to write test file\n");
		fclose(test_file);
		return 1;
	    }
	}

	fclose(test_file);
    }

    {
	rios_t rio_latin1;
	memset(&rio_latin1, 0, sizeof(rio_latin1));

	info_page_t info;
	memset(&info, 0, sizeof(info));
	info.data = (rio_file_t *)calloc(1, sizeof(rio_file_t));

	if (mp3_info(&info, (char *)temp_filename, &rio_latin1) == URIO_SUCCESS) {
	    if (info.data->time != expected_time) {
		fprintf(stderr, "Expected time: %d\n", expected_time);
		fprintf(stderr, "Actual time:   %d\n", info.data->time);
		++errors;
	    }
	} else {
	    fprintf(stderr, "Failed to get MP3 info\n");
	    ++errors;
	}
    }

    remove(temp_filename);

    return errors;
}
