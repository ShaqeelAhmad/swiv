/*
 * Copyright 2023 Shaqeel Ahmad
 * Copyright 2011 Bert Muennich
 *
 * This file is part of swiv.
 *
 * swiv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * swiv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with swiv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "swiv.h"
#define _IMAGE_CONFIG
#include "config.h"
#include "version.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

opt_t _options;
const opt_t *options = (const opt_t*) &_options;

void print_usage(void)
{
	printf("usage: swiv [-abcfhiopqrtvZ] [-A FRAMERATE] [-B COLOR] [-C COLOR] "
	       "[-e WID] [-F FONT] [-G GAMMA] [-g GEOMETRY] [-N NAME] [-n NUM] "
	       "[-S DELAY] [-s MODE] [-z ZOOM] "
	       "FILES...\n");
}

void print_version(void)
{
	puts("swiv " VERSION);
}

// This function only parses the size, offsets are ignored.
static void parse_geometry(char *s, int *w, int *h)
{
	long n;
	char *end = s;

	if (s == NULL || *s == '\0')
		return;

	if (*s != 'x') {
		n = strtol(s, &end, 10);
		if (n < 0 || end == s)
			return;
		*w = n;
	}
	s = end;
	if (*s == 'x') {
		s++;
		n = strtol(s, &end, 10);
		if (n < 0 || end == s)
			return;
		*h = n;
	}
}

void parse_options(int argc, char **argv)
{
	int n, opt;
	char *end, *s;
	const char *scalemodes = "dfwh";

	progname = strrchr(argv[0], '/');
	progname = progname ? progname + 1 : argv[0];

	_options.from_stdin = false;
	_options.to_stdout = false;
	_options.recursive = false;
	_options.startnum = 0;

	_options.scalemode = SCALE_DOWN;
	_options.zoom = 1.0;
	_options.animate = false;
	_options.gamma = 0;
	_options.slideshow = 0;
	_options.framerate = 0;

	_options.fullscreen = false;
	_options.hide_bar = false;
	_options.res_name = NULL;
	_options.font = NULL;
	_options.bg = NULL;
	_options.fg = NULL;

	_options.geometry.w = 0;
	_options.geometry.h = 0;

	_options.quiet = false;
	_options.thumb_mode = false;
	_options.clean_cache = false;
	_options.private_mode = false;

	while ((opt = getopt(argc, argv, "A:aB:bC:ce:F:fG:g:hin:N:opqrS:s:tvZz:")) != -1) {
		switch (opt) {
			case '?':
				print_usage();
				exit(EXIT_FAILURE);
			case 'A':
				n = strtol(optarg, &end, 0);
				if (*end != '\0' || n <= 0)
					error(EXIT_FAILURE, 0, "Invalid argument for option -A: %s", optarg);
				_options.framerate = n;
				/* fall through */
			case 'a':
				_options.animate = true;
				break;
			case 'B':
				_options.bg = optarg;
				break;
			case 'b':
				_options.hide_bar = true;
				break;
			case 'C':
				_options.fg = optarg;
				break;
			case 'c':
				_options.clean_cache = true;
				break;
			case 'e':
				// Ignored
				break;
			case 'F':
				_options.font = optarg;
				if (optarg == NULL || *optarg == '\0')
					error(EXIT_FAILURE, 0, "Invalid argument for option -F: %s", optarg);
				break;
			case 'f':
				_options.fullscreen = true;
				break;
			case 'G':
				n = strtol(optarg, &end, 0);
				if (*end != '\0')
					error(EXIT_FAILURE, 0, "Invalid argument for option -G: %s", optarg);
				_options.gamma = n;
				break;
			case 'g':
				parse_geometry(optarg, &_options.geometry.w,
						&_options.geometry.h);
				break;
			case 'h':
				print_usage();
				exit(EXIT_SUCCESS);
			case 'i':
				_options.from_stdin = true;
				break;
			case 'n':
				n = strtol(optarg, &end, 0);
				if (*end != '\0' || n <= 0)
					error(EXIT_FAILURE, 0, "Invalid argument for option -n: %s", optarg);
				_options.startnum = n - 1;
				break;
			case 'N':
				_options.res_name = optarg;
				break;
			case 'o':
				_options.to_stdout = true;
				break;
			case 'p':
				_options.private_mode = true;
				break;
			case 'q':
				_options.quiet = true;
				break;
			case 'r':
				_options.recursive = true;
				break;
			case 'S':
				n = strtof(optarg, &end) * 10;
				if (*end != '\0' || n <= 0)
					error(EXIT_FAILURE, 0, "Invalid argument for option -S: %s", optarg);
				_options.slideshow = n;
				break;
			case 's':
				s = strchr(scalemodes, optarg[0]);
				if (s == NULL || *s == '\0' || strlen(optarg) != 1)
					error(EXIT_FAILURE, 0, "Invalid argument for option -s: %s", optarg);
				_options.scalemode = s - scalemodes;
				break;
			case 't':
				_options.thumb_mode = true;
				break;
			case 'v':
				print_version();
				exit(EXIT_SUCCESS);
			case 'Z':
				_options.scalemode = SCALE_ZOOM;
				_options.zoom = 1.0;
				break;
			case 'z':
				n = strtol(optarg, &end, 0);
				if (*end != '\0' || n <= 0)
					error(EXIT_FAILURE, 0, "Invalid argument for option -z: %s", optarg);
				_options.scalemode = SCALE_ZOOM;
				_options.zoom = (float) n / 100.0;
				break;
		}
	}

	_options.filenames = argv + optind;
	_options.filecnt = argc - optind;

	if (_options.filecnt == 1 && STREQ(_options.filenames[0], "-")) {
		_options.filenames++;
		_options.filecnt--;
		_options.from_stdin = true;
	}
}
