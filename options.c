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

static int hexchar(char c)
{
	if ('A' <= c && c <= 'F')
		return c - 'A' + 10;
	if ('a' <= c && c <= 'f')
		return c - 'a' + 10;
	if ('0' <= c && c <= '9')
		return c - '0';

	return -1;
}

static double parse_hex(char **s)
{
	if ((*s)[0] == '\0' || (*s)[1] == '\0')
		return -1;
	int l = hexchar((*s)[0]);
	int r = hexchar((*s)[1]);
	if (l < 0 || r < 0)
		return -1;

	*s += 2;
	return (double)(l * 16 + r) / 255.0;
}

static bool parse_color(color_t *color, char *s)
{
	if (*s == '#')
		s++;

	color->r = parse_hex(&s);
	color->g = parse_hex(&s);
	color->b = parse_hex(&s);

	if (*s)
		color->a = parse_hex(&s);
	else
		color->a = 1;

	return color->r >= 0 && color->g >= 0 && color->b >= 0 && color->a >= 0;
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
	_options.bg = (color_t){
		.a = 1,
		.r = 1,
		.g = 1,
		.b = 1,
	};
	_options.fg = (color_t){
		.a = 1,
		.r = 0,
		.g = 0,
		.b = 0,
	};

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
				if (!parse_color(&_options.bg, optarg))
					error(EXIT_FAILURE, 0, "Invalid argument for option -B: %s", optarg);
				break;
			case 'b':
				_options.hide_bar = true;
				break;
			case 'C':
				if (!parse_color(&_options.fg, optarg))
					error(EXIT_FAILURE, 0, "Invalid argument for option -C: %s", optarg);
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
				// Ignored
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
