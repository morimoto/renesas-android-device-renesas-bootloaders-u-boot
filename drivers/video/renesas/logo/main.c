// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2020 GlobalLogic
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if ((argc < 2) || (argc > 3)) {
		printf("Invalid input parameters\n");
		printf("Usage: <input file> <output file>\n");
		goto exit;
	}
	char *input_filename, *output_filename;
	const char *extension = ".argb";

	if (argc == 2) {
		input_filename = argv[1];
		output_filename = malloc(strlen(argv[1]) + strlen(extension) + 1);
		strcpy(output_filename, argv[1]);
		strcat(output_filename, extension);
	} else {
		input_filename = argv[1];
		output_filename = argv[2];
	}

	int input_fd = open(input_filename, O_RDONLY, 0);
	if (input_fd == -1) {
		printf("Failed to open input file\n");
		goto exit;
	}

	int output_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (output_fd == -1) {
		printf("Failed to create or open output file\n");
		goto exit;
	}
	uint32_t buffer = 0;
	unsigned long pixel_counter = 0;

	while ((read(input_fd, &buffer, sizeof(buffer)) == sizeof(buffer))) {
		uint32_t out_value = (uint32_t) (
		  ((buffer & 0x000000FF) << 16)
		| ((buffer & 0x0000FF00)) //B
		| ((buffer & 0x00FF0000) >> 16)
		| ((buffer & 0xFF000000)) //A
		);

		if(sizeof(out_value) != write(output_fd, &out_value, sizeof(out_value))) {
			printf("Failed to write pixel to file\n");
			goto exit;
		}
		pixel_counter++;
	}

	printf("Successfully converted\n Number of converted pixels = \t\t %lu\n", pixel_counter);
	close(input_fd);
	close(output_fd);
	if(argc == 2)
		free(output_filename);

exit:
	exit(0);
}
