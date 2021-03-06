/* -*- mode: c++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*-
 * vim: ts=4 sw=4 noet ai cindent syntax=cpp
 *
 * Conky, a system monitor, based on torsmo
 *
 * Any original torsmo code is licensed under the BSD license
 *
 * All code written since the fork of torsmo is licensed under the GPL
 *
 * Please see COPYING for details
 *
 * Copyright (c) 2004, Hannu Saransaari and Lauri Hakkarainen
 * Copyright (c) 2005-2010 Brenden Matthews, Philip Kovacs, et. al.
 *	(see AUTHORS)
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "text_object.h"
#include "conky.h"
#include "common.h"
#include "timed-thread.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <string>
#include <stdarg.h>
#include <cmath>
#include <ctime>
#include <locale.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif /* HAVE_DIRENT_H */
#include <sys/time.h>
#include <sys/param.h>
#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif /* HAVE_SYS_INOTIFY_H */
#ifdef BUILD_X11
#include "x11.h"
#include <X11/Xutil.h>
#ifdef BUILD_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif
#ifdef BUILD_IMLIB2
#include "imlib2.h"
#endif /* BUILD_IMLIB2 */
#endif /* BUILD_X11 */
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <getopt.h>
#ifdef BUILD_NCURSES
#include <ncurses.h>
#ifdef LEAKFREE_NCURSES
#include "nc.h"
#endif
#endif
#if defined BUILD_WEATHER_XOAP || defined BUILD_RSS
#include <libxml/parser.h>
#endif

/* local headers */
#include "core.h"
#include "build.h"
#include "colours.h"
#include "diskio.h"
#include "exec.h"
#ifdef BUILD_X11
#include "fonts.h"
#endif
#ifdef BUILD_ICONV
#include "iconv_tools.h"
#endif
#ifdef BUILD_LUA
#include "llua.h"
#endif /* BUILD_LUA */
#include "logging.h"
#include "mail.h"
#include "net_stat.h"
#include "temphelper.h"
#include "template.h"
#include "timeinfo.h"
#include "top.h"
#ifdef BUILD_MYSQL
#include "mysql.h"
#endif /* BUILD_MYSQL */

/* check for OS and include appropriate headers */
#if defined(__linux__)
#include "linux.h"
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include "freebsd.h"
#elif defined(__OpenBSD__)
#include "openbsd.h"
#endif
#ifdef BUILD_HTTP
#include <microhttpd.h>
#endif

#if defined(__FreeBSD_kernel__)
#include <bsd/bsd.h>
#endif

#ifdef BUILD_BUILTIN_CONFIG
#include "defconfig.h"
#include "conf_cookie.h"
#endif

#ifndef S_ISSOCK
#define S_ISSOCK(x)   ((x & S_IFMT) == S_IFSOCK)
#endif

#define MAIL_FILE "$MAIL"
#define MAX_IF_BLOCK_DEPTH 5

//#define SIGNAL_BLOCKING
#undef SIGNAL_BLOCKING

/* debugging level, used by logging.h */
int global_debug_level = 0;

/* disable inotify auto reload feature if desired */
int disable_auto_reload = 0;

/* two strings for internal use */
static char *tmpstring1, *tmpstring2;

/* variables holding various config settings */
int short_units;
int format_human_readable;
int cpu_separate;
enum spacer_state {
	NO_SPACER = 0,
	LEFT_SPACER,
	RIGHT_SPACER
} use_spacer;
int top_cpu, top_mem, top_time;
#ifdef BUILD_IOSTATS
int top_io;
#endif
int top_running;
int output_methods;
static int extra_newline;
enum x_initialiser_state x_initialised = NO;
static volatile int g_signal_pending;
/* Update interval */
double update_interval;
double update_interval_old;
double update_interval_bat;
void *global_cpu = NULL;
unsigned int max_text_width = 0;
int ifup_strictness = IFUP_UP;

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
extern kvm_t *kd;
#endif

int argc_copy;
char** argv_copy;

/* prototypes for internally used functions */
static void signal_handler(int);
static void print_version(void) __attribute__((noreturn));
static void reload_config(void);

static void print_version(void)
{
	std::cout << PACKAGE_NAME" "VERSION" compiled "BUILD_DATE" for "BUILD_ARCH"\n"
		"\nCompiled in features:\n\n"
		"System config file: "SYSTEM_CONFIG_FILE"\n"
		"Package library path: "PACKAGE_LIBDIR"\n\n"
#ifdef BUILD_X11
		" X11:\n"
# ifdef BUILD_XDAMAGE
		"  * Xdamage extension\n"
# endif /* BUILD_XDAMAGE */
# ifdef BUILD_XDBE
		"  * XDBE (double buffer extension)\n"
# endif /* BUILD_XDBE */
# ifdef BUILD_XFT
		"  * Xft\n"
# endif /* BUILD_XFT */
# ifdef BUILD_ARGB
		"  * ARGB visual\n"
# endif /* BUILD_ARGB */
#endif /* BUILD_X11 */
		"\n Music detection:\n"
#ifdef BUILD_AUDACIOUS
		"  * Audacious\n"
#endif /* BUILD_AUDACIOUS */
#ifdef BUILD_BMPX
		"  * BMPx\n"
#endif /* BUILD_BMPX */
#ifdef BUILD_MPD
		"  * MPD\n"
#endif /* BUILD_MPD */
#ifdef BUILD_MOC
		"  * MOC\n"
#endif /* BUILD_MOC */
#ifdef BUILD_XMMS2
		"  * XMMS2\n"
#endif /* BUILD_XMMS2 */
		"\n General:\n"
#ifdef HAVE_OPENMP
		"  * OpenMP\n"
#endif /* HAVE_OPENMP */
#ifdef BUILD_MATH
		"  * math\n"
#endif /* BUILD_MATH */
#ifdef BUILD_HDDTEMP
		"  * hddtemp\n"
#endif /* BUILD_HDDTEMP */
#ifdef BUILD_PORT_MONITORS
		"  * portmon\n"
#endif /* BUILD_PORT_MONITORS */
#ifdef BUILD_HTTP
		"  * HTTP\n"
#endif
#ifdef BUILD_IRC
		"  * IRC\n"
#endif
#ifdef BUILD_CURL
		"  * Curl\n"
#endif /* BUILD_CURL */
#ifdef BUILD_RSS
		"  * RSS\n"
#endif /* BUILD_RSS */
#ifdef BUILD_WEATHER_METAR
		"  * Weather (METAR)\n"
#ifdef BUILD_WEATHER_XOAP
		"  * Weather (XOAP)\n"
#endif /* BUILD_WEATHER_XOAP */
#endif /* BUILD_WEATHER_METAR */
#ifdef BUILD_WLAN
		"  * wireless\n"
#endif /* BUILD_WLAN */
#ifdef BUILD_IBM
		"  * support for IBM/Lenovo notebooks\n"
#endif /* BUILD_IBM */
#ifdef BUILD_NVIDIA
		"  * nvidia\n"
#endif /* BUILD_NVIDIA */
#ifdef BUILD_EVE
		"  * eve-online\n"
#endif /* BUILD_EVE */
#ifdef BUILD_BUILTIN_CONFIG
		"  * builtin default configuration\n"
#endif /* BUILD_BUILTIN_CONFIG */
#ifdef BUILD_IMLIB2
		"  * Imlib2\n"
#endif /* BUILD_IMLIB2 */
#ifdef BUILD_MIXER_ALSA
		"  * ALSA mixer support\n"
#endif /* BUILD_MIXER_ALSA */
#ifdef BUILD_APCUPSD
		"  * apcupsd\n"
#endif /* BUILD_APCUPSD */
#ifdef BUILD_IOSTATS
		"  * iostats\n"
#endif /* BUILD_IOSTATS */
#ifdef BUILD_NCURSES
		"  * ncurses\n"
#endif /* BUILD_NCURSES */
#ifdef BUILD_LUA
		"  * Lua\n"
		"\n  Lua bindings:\n"
#ifdef BUILD_LUA_CAIRO
		"   * Cairo\n"
#endif /* BUILD_LUA_CAIRO */
#ifdef BUILD_LUA_IMLIB2
		"   * Imlib2\n"
#endif /* BUILD_LUA_IMLIB2 */
#endif /* BUILD_LUA */
	;

	exit(EXIT_SUCCESS);
}

static const char *suffixes[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "" };


#ifdef BUILD_X11

static void X11_create_window(void);
static void X11_initialisation(void);

struct _x11_stuff_s {
	Region region;
#ifdef BUILD_XDAMAGE
	Damage damage;
	XserverRegion region2, part;
	int event_base, error_base;
#endif
} x11_stuff;

/* text size */

static int text_start_x, text_start_y;	/* text start position in window */
static int text_width = 1, text_height = 1; /* initially 1 so no zero-sized window is created */

/* alignments */
enum alignment {
	ALIGNMENT_ERROR,
	TOP_LEFT,
	TOP_RIGHT,
	TOP_MIDDLE,
	BOTTOM_LEFT,
	BOTTOM_RIGHT,
	BOTTOM_MIDDLE,
	MIDDLE_LEFT,
	MIDDLE_MIDDLE,
	MIDDLE_RIGHT,
	NONE
};

/* display to connect to */
static char *disp = NULL;

#endif /* BUILD_X11 */

/* struct that has all info to be shared between
 * instances of the same text object */
struct information info;

/* path to config file */
std::string current_config;

bool stdinconfig = false;

/* set to 1 if you want all text to be in uppercase */
static unsigned int stuff_in_uppercase;

/* Run how many times? */
static unsigned long total_run_times;

/* fork? */
static int fork_to_background;

/* set to 0 after the first time conky is run, so we don't fork again after the
 * first forking */
static int first_pass = 1;

static int cpu_avg_samples, net_avg_samples, diskio_avg_samples;

/* filenames for output */
char *overwrite_file = NULL; FILE *overwrite_fpointer = NULL;
char *append_file = NULL; FILE *append_fpointer = NULL;

#ifdef BUILD_HTTP
std::string webpage;
struct MHD_Daemon *httpd;
bool http_refresh;

int sendanswer(void *cls, struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls) {
	struct MHD_Response *response = MHD_create_response_from_data(webpage.length(), (void*) webpage.c_str(), MHD_NO, MHD_NO);
	int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	if(cls || url || method || version || upload_data || upload_data_size || con_cls) {}	//make compiler happy
	return ret;
}
#endif

#ifdef BUILD_X11

static int show_graph_scale;
static int show_graph_range;

/* Position on the screen */
static int text_alignment;
static int gap_x, gap_y;

/* border */
static int draw_borders;
static int draw_graph_borders;
static int stippled_borders;

int get_stippled_borders(void)
{
	return stippled_borders;
}

static int draw_shades, draw_outline;

long default_fg_color, default_bg_color, default_out_color;

/* create own window or draw stuff to root? */
static int set_transparent = 0;

#ifdef OWN_WINDOW
static int own_window = 0;
static int background_colour = 0;

/* fixed size/pos is set if wm/user changes them */
static int fixed_size = 0, fixed_pos = 0;
#endif

static int minimum_width, minimum_height;
static int maximum_width;

#endif /* BUILD_X11 */

#ifdef __OpenBSD__
static int sensor_device;
#endif

long color0, color1, color2, color3, color4, color5, color6, color7, color8,
	 color9;

/* maximum size of config TEXT buffer, i.e. below TEXT line. */
unsigned int max_user_text;

/* maximum size of individual text buffers, ie $exec buffer size */
unsigned int text_buffer_size = DEFAULT_TEXT_BUFFER_SIZE;

/* UTF-8 */
bool utf8_mode = false;

/* no buffers in used memory? */
int no_buffers;

/* pad percentages to decimals? */
static int pad_percents = 0;

static char *global_text = 0;

char *get_global_text(void)
{
	return global_text;
}

long global_text_lines;

static int total_updates;
static int updatereset;

void set_updatereset(int i)
{
	updatereset = i;
}

int get_updatereset(void)
{
	return updatereset;
}

int get_total_updates(void)
{
	return total_updates;
}

#define SECRIT_MULTILINE_CHAR '\x02'

int calc_text_width(const char *s)
{
	size_t slen = strlen(s);

#ifdef BUILD_X11
	if ((output_methods & TO_X) == 0) {
#endif /* BUILD_X11 */
		return slen;
#ifdef BUILD_X11
	}
#ifdef BUILD_XFT
	if (use_xft) {
		XGlyphInfo gi;

		if (utf8_mode) {
			XftTextExtentsUtf8(display, fonts[selected_font].xftfont,
					(const FcChar8 *) s, slen, &gi);
		} else {
			XftTextExtents8(display, fonts[selected_font].xftfont,
					(const FcChar8 *) s, slen, &gi);
		}
		return gi.xOff;
	} else
#endif /* BUILD_XFT */
	{
		return XTextWidth(fonts[selected_font].font, s, slen);
	}
#endif /* BUILD_X11 */
}

/* formatted text to render on screen, generated in generate_text(),
 * drawn in draw_stuff() */

static char *text_buffer;

/* quite boring functions */

static inline void for_each_line(char *b, int f(char *, int))
{
	char *ps, *pe;
	int special_index = 0; /* specials index */

	if(! b) return;
	for (ps = b, pe = b; *pe; pe++) {
		if (*pe == '\n') {
			*pe = '\0';
			special_index = f(ps, special_index);
			*pe = '\n';
			ps = pe + 1;
		}
	}

	if (ps < pe) {
		f(ps, special_index);
	}
}

static void convert_escapes(char *buf)
{
	char *p = buf, *s = buf;

	while (*s) {
		if (*s == '\\') {
			s++;
			if (*s == 'n') {
				*p++ = '\n';
			} else if (*s == '\\') {
				*p++ = '\\';
			}
			s++;
		} else {
			*p++ = *s++;
		}
	}
	*p = '\0';
}

/* Prints anything normally printed with snprintf according to the current value
 * of use_spacer.  Actually slightly more flexible than snprintf, as you can
 * safely specify the destination buffer as one of your inputs.  */
int spaced_print(char *buf, int size, const char *format, int width, ...)
{
	int len = 0;
	va_list argp;
	char *tempbuf;

	if (size < 1) {
		return 0;
	}
	tempbuf = (char*)malloc(size * sizeof(char));

	// Passes the varargs along to vsnprintf
	va_start(argp, width);
	vsnprintf(tempbuf, size, format, argp);
	va_end(argp);

	switch (use_spacer) {
		case NO_SPACER:
			len = snprintf(buf, size, "%s", tempbuf);
			break;
		case LEFT_SPACER:
			len = snprintf(buf, size, "%*s", width, tempbuf);
			break;
		case RIGHT_SPACER:
			len = snprintf(buf, size, "%-*s", width, tempbuf);
			break;
	}
	free(tempbuf);
	return len;
}

/* print percentage values
 *
 * - i.e., unsigned values between 0 and 100
 * - respect the value of pad_percents */
int percent_print(char *buf, int size, unsigned value)
{
	return spaced_print(buf, size, "%u", pad_percents, value);
}

#if defined(__FreeBSD__)
unsigned long long llabs(long long num) {
       if(num < 0) return -num;
       else return num;
}
#endif

/* converts from bytes to human readable format (K, M, G, T)
 *
 * The algorithm always divides by 1024, as unit-conversion of byte
 * counts suggests. But for output length determination we need to
 * compare with 1000 here, as we print in decimal form. */
void human_readable(long long num, char *buf, int size)
{
	const char **suffix = suffixes;
	float fnum;
	int precision;
	int width;
	const char *format;

	/* Possibly just output as usual, for example for stdout usage */
	if (!format_human_readable) {
		spaced_print(buf, size, "%d", 6, round_to_int(num));
		return;
	}
	if (short_units) {
		width = 5;
		format = "%.*f%.1s";
	} else {
		width = 7;
		format = "%.*f%-3s";
	}

	if (llabs(num) < 1000LL) {
		spaced_print(buf, size, format, width, 0, (float)num, *suffix);
		return;
	}

	while (llabs(num / 1024) >= 1000LL && **(suffix + 2)) {
		num /= 1024;
		suffix++;
	}

	suffix++;
	fnum = num / 1024.0;

	/* fnum should now be < 1000, so looks like 'AAA.BBBBB'
	 *
	 * The goal is to always have a significance of 3, by
	 * adjusting the decimal part of the number. Sample output:
	 *  123MiB
	 * 23.4GiB
	 * 5.12B
	 * so the point of alignment resides between number and unit. The
	 * upside of this is that there is minimal padding necessary, though
	 * there should be a way to make alignment take place at the decimal
	 * dot (then with fixed width decimal part).
	 *
	 * Note the repdigits below: when given a precision value, printf()
	 * rounds the float to it, not just cuts off the remaining digits. So
	 * e.g. 99.95 with a precision of 1 gets 100.0, which again should be
	 * printed with a precision of 0. Yay. */

	precision = 0;		/* print 100-999 without decimal part */
	if (fnum < 99.95)
		precision = 1;	/* print 10-99 with one decimal place */
	if (fnum < 9.995)
		precision = 2;	/* print 0-9 with two decimal places */

	spaced_print(buf, size, format, width, precision, fnum, *suffix);
}

/* global object list root element */
static struct text_object global_root_object;

static long current_text_color;

void set_current_text_color(long colour)
{
	current_text_color = colour;
}

long get_current_text_color(void)
{
	return current_text_color;
}

//adds newstring to to the tree unless you can already see it when travelling back.
//if it's possible to attach it then it returns a pointer to the leaf, else it returns NULL
struct conftree* conftree_add(struct conftree* previous, const char* newstring) {
	struct conftree* node;
	struct conftree* node2;

	for(node = previous; node != NULL; node = node->back) {
		if(strcmp(node->string, newstring) == 0) {
			return NULL;
		}
	}
	node = (struct conftree*)malloc(sizeof(struct conftree));
	if (previous != NULL) {
		if(previous->vert_next == NULL) {
			previous->vert_next = node;
		} else {
			for(node2 = previous->vert_next; node2->horz_next != NULL; node2 = node2->horz_next ) { }
			node2->horz_next = node;
		}
	}
	node->string = strdup(newstring);
	node->horz_next = NULL;
	node->vert_next = NULL;
	node->back = previous;
	return node;
}

void conftree_empty(struct conftree* tree) {
	if(tree) {
		conftree_empty(tree->horz_next);
		conftree_empty(tree->vert_next);
		free(tree->string);
		free(tree);
	}
}

struct conftree *currentconffile;

static void extract_variable_text(const char *p)
{
	free_text_objects(&global_root_object);
	free_and_zero(tmpstring1);
	free_and_zero(tmpstring2);
	free_and_zero(text_buffer);

	extract_variable_text_internal(&global_root_object, p);
}

void parse_conky_vars(struct text_object *root, const char *txt,
		char *p, int p_max_size)
{
	extract_variable_text_internal(root, txt);
	generate_text_internal(p, p_max_size, *root);
}

/* substitutes all occurrences of '\n' with SECRIT_MULTILINE_CHAR, which allows
 * multiline objects like $exec work with $align[rc] and friends
 */
void substitute_newlines(char *p, long l)
{
	char *s = p;
	if (l < 0) return;
	while (p && *p && p < s + l) {
		if (*p == '\n') {
			/* only substitute if it's not the last newline */
			*p = SECRIT_MULTILINE_CHAR;
		}
		p++;
	}
}


/* IFBLOCK jumping algorithm
 *
 * This is easier as it looks like:
 * - each IF checks it's condition
 *   - on FALSE: jump
 *   - on TRUE: don't care
 * - each ELSE jumps unconditionally
 * - each ENDIF is silently being ignored
 *
 * Why this works (or: how jumping works):
 * Jumping means to overwrite the "obj" variable of the loop and set it to the
 * target (i.e. the corresponding ELSE or ENDIF). After that, the for-loop does
 * the rest: as regularly, "obj" is being updated to point to obj->next, so
 * object parsing continues right after the corresponding ELSE or ENDIF. This
 * means that if we find an ELSE, it's corresponding IF must not have jumped,
 * so we need to jump always. If we encounter an ENDIF, it's corresponding IF
 * or ELSE has not jumped, and there is nothing to do.
 */
void generate_text_internal(char *p, int p_max_size, struct text_object root)
{
	struct text_object *obj;
	size_t a;

	if(! p) return;

#ifdef BUILD_ICONV
	char *buff_in;

	buff_in = (char *)malloc(p_max_size);
	memset(buff_in, 0, p_max_size);
#endif /* BUILD_ICONV */

	p[0] = 0;
	obj = root.next;
	while (obj && p_max_size > 0) {

		/* check callbacks for existence and act accordingly */
		if (obj->callbacks.print) {
			(*obj->callbacks.print)(obj, p, p_max_size);
		} else if (obj->callbacks.iftest) {
			if (!(*obj->callbacks.iftest)(obj)) {
				DBGP2("jumping");
				if (obj->ifblock_next)
					obj = obj->ifblock_next;
			}
		} else if (obj->callbacks.barval) {
			new_bar(obj, p, p_max_size, (*obj->callbacks.barval)(obj));
		} else if (obj->callbacks.gaugeval) {
			new_gauge(obj, p, p_max_size, (*obj->callbacks.gaugeval)(obj));
#ifdef BUILD_X11
		} else if (obj->callbacks.graphval) {
			new_graph(obj, p, p_max_size, (*obj->callbacks.graphval)(obj));
#endif /* BUILD_X11 */
		} else if (obj->callbacks.percentage) {
			percent_print(p, p_max_size, (*obj->callbacks.percentage)(obj));
		}

		a = strlen(p);
#ifdef BUILD_ICONV
		iconv_convert(&a, buff_in, p, p_max_size);
#endif /* BUILD_ICONV */
		if (!obj->verbatim_output)
			substitute_newlines(p, a - 2);
		p += a;
		p_max_size -= a;
		(*p) = 0;

		obj = obj->next;
	}
#ifdef BUILD_X11
	/* load any new fonts we may have had */
	load_fonts();
#endif /* BUILD_X11 */
#ifdef BUILD_ICONV
	free(buff_in);
#endif /* BUILD_ICONV */
}

void evaluate(const char *text, char *p, int p_max_size)
{
	struct text_object subroot;

	parse_conky_vars(&subroot, text, p, p_max_size);
	DBGP2("evaluated '%s' to '%s'", text, p);

	free_text_objects(&subroot);
}

double current_update_time, next_update_time, last_update_time;

static void generate_text(void)
{
	char *p;
	unsigned int i, j, k;

	special_count = 0;

	/* update info */

	current_update_time = get_time();

	update_stuff();

	/* add things to the buffer */

	/* generate text */

	p = text_buffer;

	generate_text_internal(p, max_user_text, global_root_object);
	if(max_text_width > 0) {
		for(i = 0, j = 0; p[i] != 0; i++) {
			if(p[i] == '\n') j = 0;
			else if(j == max_text_width) {
				k = i + strlen(p + i) + 1;
				if(k < text_buffer_size) {
					while(k != i) {
						p[k] = p[k-1];
						k--;
					}
					p[k] = '\n';
					j = 0;
				} else NORM_ERR("The end of the text_buffer is reached, increase \"text_buffer_size\"");
			} else j++;
		}
	}

	if (stuff_in_uppercase) {
		char *tmp_p;

		tmp_p = text_buffer;
		while (*tmp_p) {
			*tmp_p = toupper(*tmp_p);
			tmp_p++;
		}
	}

	next_update_time += update_interval;
	if (next_update_time < get_time()) {
		next_update_time = get_time() + update_interval;
	} else if (next_update_time > get_time() + update_interval) {
		next_update_time = get_time() + update_interval;
	}
	last_update_time = current_update_time;
	total_updates++;
}

void set_update_interval(double interval)
{
	update_interval = interval;
	update_interval_old = interval;
}

int get_string_width(const char *s)
{
	return *s ? calc_text_width(s) : 0;
}

#ifdef BUILD_X11
static int get_string_width_special(char *s, int special_index)
{
	char *p, *final;
	special_t *current = specials;
	int idx = 1;
	int width = 0;
	long i;

	if (!s)
		return 0;

	if ((output_methods & TO_X) == 0)
		return strlen(s);

	p = strndup(s, text_buffer_size);
	final = p;

	for(i = 0; i < special_index; i++)
		current = current->next;
	for(i = 0; i < idx; i++)
		current = current->next;

	while (*p) {
		if (*p == SPECIAL_CHAR) {
			/* shift everything over by 1 so that the special char
			 * doesn't mess up the size calculation */
			for (i = 0; i < (long)strlen(p); i++) {
				*(p + i) = *(p + i + 1);
			}
			if (current->type == GRAPH
					|| current->type == GAUGE
					|| current->type == BAR) {
				width += current->width;
			}
			idx++;
			current = current->next;
		} else if (*p == SECRIT_MULTILINE_CHAR) {
			*p = 0;
			break;
		} else {
			p++;
		}
	}
	if (strlen(final) > 1) {
		width += calc_text_width(final);
	}
	free(final);
	return width;
}

static int text_size_updater(char *s, int special_index);

int last_font_height;
static void update_text_area(void)
{
	int x = 0, y = 0;

	if ((output_methods & TO_X) == 0)
		return;
	/* update text size if it isn't fixed */
#ifdef OWN_WINDOW
	if (!fixed_size)
#endif
	{
		text_width = minimum_width;
		text_height = 0;
		last_font_height = font_height();
		for_each_line(text_buffer, text_size_updater);
		text_width += 1;
		if (text_height < minimum_height) {
			text_height = minimum_height;
		}
		if (text_width > maximum_width && maximum_width > 0) {
			text_width = maximum_width;
		}
	}

	/* get text position on workarea */
	switch (text_alignment) {
		case TOP_LEFT: case TOP_RIGHT: case TOP_MIDDLE:
			y = gap_y;
			break;

		case BOTTOM_LEFT: case BOTTOM_RIGHT: case BOTTOM_MIDDLE: default:
			y = workarea[3] - text_height - gap_y;
			break;

		case MIDDLE_LEFT: case MIDDLE_RIGHT: case MIDDLE_MIDDLE:
			y = workarea[3] / 2 - text_height / 2 - gap_y;
			break;
	}
	switch (text_alignment) {
		case TOP_LEFT: case BOTTOM_LEFT: case MIDDLE_LEFT: default:
			x = gap_x;
			break;

		case TOP_RIGHT: case BOTTOM_RIGHT: case MIDDLE_RIGHT:
			x = workarea[2] - text_width - gap_x;
			break;

		case TOP_MIDDLE: case BOTTOM_MIDDLE: case MIDDLE_MIDDLE:
			x = workarea[2] / 2 - text_width / 2 - gap_x;
			break;
	}
#ifdef OWN_WINDOW
	if (text_alignment == NONE) {	// Let the WM manage the window
			x = window.x;
			y = window.y;

			fixed_pos = 1;
			fixed_size = 1;
	}
#endif /* OWN_WINDOW */
#ifdef OWN_WINDOW

	if (own_window && !fixed_pos) {
		x += workarea[0];
		y += workarea[1];

		long border_total = window.border_inner_margin
						+ window.border_outer_margin + window.border_width;
		text_start_x = text_start_y = border_total;
		window.x = x - border_total;
		window.y = y - border_total;
	} else
#endif
	{
		/* If window size doesn't match to workarea's size,
		 * then window probably includes panels (gnome).
		 * Blah, doesn't work on KDE. */
		if (workarea[2] != window.width || workarea[3] != window.height) {
			y += workarea[1];
			x += workarea[0];
		}

		text_start_x = x;
		text_start_y = y;
	}
#ifdef BUILD_LUA
	/* update lua window globals */
	llua_update_window_table(text_start_x, text_start_y, text_width, text_height);
#endif /* BUILD_LUA */
}

/* drawing stuff */

static int cur_x, cur_y;	/* current x and y for drawing */
#endif
//draw_mode also without BUILD_X11 because we only need to print to stdout with FG
static int draw_mode;		/* FG, BG or OUTLINE */
#ifdef BUILD_X11
static long current_color;

static int text_size_updater(char *s, int special_index)
{
	int w = 0;
	int lw;
	int contain_SECRIT_MULTILINE_CHAR = 0;
	char *p;
	special_t *current = specials;

	for(int i = 0; i < special_index; i++)
		current = current->next;

	if ((output_methods & TO_X) == 0)
		return 0;
	/* get string widths and skip specials */
	p = s;
	while (*p) {
		if (*p == SPECIAL_CHAR) {
			*p = '\0';
			w += get_string_width(s);
			*p = SPECIAL_CHAR;

			if (current->type == BAR
					|| current->type == GAUGE
					|| current->type == GRAPH) {
				w += current->width;
				if (current->height > last_font_height) {
					last_font_height = current->height;
					last_font_height += font_height();
				}
			} else if (current->type == OFFSET) {
				if (current->arg > 0) {
					w += current->arg;
				}
			} else if (current->type == VOFFSET) {
				last_font_height += current->arg;
			} else if (current->type == GOTO) {
				if (current->arg > cur_x) {
					w = (int) current->arg;
				}
			} else if (current->type == TAB) {
				int start = current->arg;
				int step = current->width;

				if (!step || step < 0) {
					step = 10;
				}
				w += step - (cur_x - text_start_x - start) % step;
			} else if (current->type == FONT) {
				selected_font = current->font_added;
				if (font_height() > last_font_height) {
					last_font_height = font_height();
				}
			}

			special_index++;
			current = current->next;
			s = p + 1;
		} else if (*p == SECRIT_MULTILINE_CHAR) {
			contain_SECRIT_MULTILINE_CHAR = 1;
			*p = '\0';
			lw = get_string_width(s);
			*p = SECRIT_MULTILINE_CHAR;
			s = p + 1;
			w = lw > w ? lw : w;
			text_height += last_font_height;
		}
		p++;
	}
	/* Check also last substring if string contains SECRIT_MULTILINE_CHAR */
	if (contain_SECRIT_MULTILINE_CHAR) {
		lw = get_string_width(s);
		w = lw > w ? lw : w;
	} else {
		w += get_string_width(s);
	}
	if (w > text_width) {
		text_width = w;
	}
	if (text_width > maximum_width && maximum_width) {
		text_width = maximum_width;
	}

	text_height += last_font_height;
	last_font_height = font_height();
	return special_index;
}
#endif /* BUILD_X11 */

static inline void set_foreground_color(long c)
{
#ifdef BUILD_X11
	if (output_methods & TO_X) {
#ifdef BUILD_ARGB
		if (have_argb_visual) {
			current_color = c | (own_window_argb_value << 24);
		} else {
#endif /* BUILD_ARGB */
			current_color = c;
#ifdef BUILD_ARGB
		}
#endif /* BUILD_ARGB */
		XSetForeground(display, window.gc, current_color);
	}
#endif /* BUILD_X11 */
#ifdef BUILD_NCURSES
	if (output_methods & TO_NCURSES) {
		attron(COLOR_PAIR(c));
	}
#endif /* BUILD_NCURSES */
	UNUSED(c);
	return;
}

std::string string_replace_all(std::string original, std::string oldpart, std::string newpart, std::string::size_type start) {
	std::string::size_type i = start;
	int oldpartlen = oldpart.length();
	while(1) {
		i = original.find(oldpart, i);
		if(i == std::string::npos) break;
		original.replace(i, oldpartlen, newpart);
	}
	return original;
}

static void draw_string(const char *s)
{
	int i, i2, pos, width_of_s;
	int max = 0;
	int added;
	char *s_with_newlines;

	if (s[0] == '\0') {
		return;
	}

	width_of_s = get_string_width(s);
	s_with_newlines = strdup(s);
	for(i = 0; i < (int) strlen(s_with_newlines); i++) {
		if(s_with_newlines[i] == SECRIT_MULTILINE_CHAR) {
			s_with_newlines[i] = '\n';
		}
	}
	if ((output_methods & TO_STDOUT) && draw_mode == FG) {
		printf("%s\n", s_with_newlines);
		if (extra_newline) fputc('\n', stdout);
		fflush(stdout);	/* output immediately, don't buffer */
	}
	if ((output_methods & TO_STDERR) && draw_mode == FG) {
		fprintf(stderr, "%s\n", s_with_newlines);
		fflush(stderr);	/* output immediately, don't buffer */
	}
	if ((output_methods & OVERWRITE_FILE) && draw_mode == FG && overwrite_fpointer) {
		fprintf(overwrite_fpointer, "%s\n", s_with_newlines);
	}
	if ((output_methods & APPEND_FILE) && draw_mode == FG && append_fpointer) {
		fprintf(append_fpointer, "%s\n", s_with_newlines);
	}
#ifdef BUILD_NCURSES
	if ((output_methods & TO_NCURSES) && draw_mode == FG) {
		printw("%s", s_with_newlines);
	}
#endif
#ifdef BUILD_HTTP
	if ((output_methods & TO_HTTP) && draw_mode == FG) {
		std::string::size_type origlen = webpage.length();
		webpage.append(s_with_newlines);
		webpage = string_replace_all(webpage, "\n", "<br />", origlen);
		webpage = string_replace_all(webpage, "  ", "&nbsp;&nbsp;", origlen);
		webpage = string_replace_all(webpage, "&nbsp; ", "&nbsp;&nbsp;", origlen);
		webpage.append("<br />");
	}
#endif
	free(s_with_newlines);
	memset(tmpstring1, 0, text_buffer_size);
	memset(tmpstring2, 0, text_buffer_size);
	strncpy(tmpstring1, s, text_buffer_size - 1);
	pos = 0;
	added = 0;

#ifdef BUILD_X11
	if (output_methods & TO_X) {
		max = ((text_width - width_of_s) / get_string_width(" "));
	}
#endif /* BUILD_X11 */
	/* This code looks for tabs in the text and coverts them to spaces.
	 * The trick is getting the correct number of spaces, and not going
	 * over the window's size without forcing the window larger. */
	for (i = 0; i < (int) text_buffer_size; i++) {
		if (tmpstring1[i] == '\t') {
			i2 = 0;
			for (i2 = 0; i2 < (8 - (1 + pos) % 8) && added <= max; i2++) {
				/* guard against overrun */
				tmpstring2[MIN(pos + i2, (int)text_buffer_size - 1)] = ' ';
				added++;
			}
			pos += i2;
		} else {
			/* guard against overrun */
			tmpstring2[MIN(pos, (int) text_buffer_size - 1)] = tmpstring1[i];
			pos++;
		}
	}
#ifdef BUILD_X11
	if (output_methods & TO_X) {
		if (text_width == maximum_width) {
			/* this means the text is probably pushing the limit,
			 * so we'll chop it */
			while (cur_x + get_string_width(tmpstring2) - text_start_x
					> maximum_width && strlen(tmpstring2) > 0) {
				tmpstring2[strlen(tmpstring2) - 1] = '\0';
			}
		}
	}
#endif /* BUILD_X11 */
	s = tmpstring2;
#ifdef BUILD_X11
	if (output_methods & TO_X) {
#ifdef BUILD_XFT
		if (use_xft) {
			XColor c;
			XftColor c2;

			c.pixel = current_color;
			// query color on custom colormap
			XQueryColor(display, window.colourmap, &c);

			c2.pixel = c.pixel;
			c2.color.red = c.red;
			c2.color.green = c.green;
			c2.color.blue = c.blue;
			c2.color.alpha = fonts[selected_font].font_alpha;
			if (utf8_mode) {
				XftDrawStringUtf8(window.xftdraw, &c2, fonts[selected_font].xftfont,
					cur_x, cur_y, (const XftChar8 *) s, strlen(s));
			} else {
				XftDrawString8(window.xftdraw, &c2, fonts[selected_font].xftfont,
					cur_x, cur_y, (const XftChar8 *) s, strlen(s));
			}
		} else
#endif
		{
			XDrawString(display, window.drawable, window.gc, cur_x, cur_y, s,
				strlen(s));
		}
		cur_x += width_of_s;
	}
#endif /* BUILD_X11 */
	memcpy(tmpstring1, s, text_buffer_size);
}

int draw_each_line_inner(char *s, int special_index, int last_special_applied)
{
#ifdef BUILD_X11
	int font_h = 0;
	int cur_y_add = 0;
#endif /* BUILD_X11 */
	char *recurse = 0;
	char *p = s;
	int last_special_needed = -1;
	int orig_special_index = special_index;

#ifdef BUILD_X11
	if (output_methods & TO_X) {
		font_h = font_height();
		cur_y += font_ascent();
	}
	cur_x = text_start_x;
#endif /* BUILD_X11 */

	while (*p) {
		if (*p == SECRIT_MULTILINE_CHAR) {
			/* special newline marker for multiline objects */
			recurse = p + 1;
			*p = '\0';
			break;
		}
		if (*p == SPECIAL_CHAR || last_special_applied > -1) {
#ifdef BUILD_X11
			int w = 0;
#endif /* BUILD_X11 */

			/* draw string before special, unless we're dealing multiline
			 * specials */
			if (last_special_applied > -1) {
				special_index = last_special_applied;
			} else {
				*p = '\0';
				draw_string(s);
				*p = SPECIAL_CHAR;
				s = p + 1;
			}
			/* draw special */
			special_t *current = specials;
			for(int i = 0; i < special_index; i++)
				current = current->next;
			switch (current->type) {
#ifdef BUILD_X11
				case HORIZONTAL_LINE:
				{
					int h = current->height;
					int mid = font_ascent() / 2;

					w = text_start_x + text_width - cur_x;

					XSetLineAttributes(display, window.gc, h, LineSolid,
						CapButt, JoinMiter);
					XDrawLine(display, window.drawable, window.gc, cur_x,
						cur_y - mid / 2, cur_x + w, cur_y - mid / 2);
					break;
				}

				case STIPPLED_HR:
				{
					int h = current->height;
					char tmp_s = current->arg;
					int mid = font_ascent() / 2;
					char ss[2] = { tmp_s, tmp_s };

					w = text_start_x + text_width - cur_x - 1;
					XSetLineAttributes(display, window.gc, h, LineOnOffDash,
						CapButt, JoinMiter);
					XSetDashes(display, window.gc, 0, ss, 2);
					XDrawLine(display, window.drawable, window.gc, cur_x,
						cur_y - mid / 2, cur_x + w, cur_y - mid / 2);
					break;
				}

				case BAR:
				{
					int h, by;
					double bar_usage, scale;
					if (cur_x - text_start_x > maximum_width
							&& maximum_width > 0) {
						break;
					}
					h = current->height;
					bar_usage = current->arg;
					scale = current->scale;
					by = cur_y - (font_ascent() / 2) - 1;

					if (h < font_h) {
						by -= h / 2 - 1;
					}
					w = current->width;
					if (w == 0) {
						w = text_start_x + text_width - cur_x - 1;
					}
					if (w < 0) {
						w = 0;
					}

					XSetLineAttributes(display, window.gc, 1, LineSolid,
						CapButt, JoinMiter);

					XDrawRectangle(display, window.drawable, window.gc, cur_x,
						by, w, h);
					XFillRectangle(display, window.drawable, window.gc, cur_x,
						by, w * bar_usage / scale, h);
					if (h > cur_y_add
							&& h > font_h) {
						cur_y_add = h;
					}
					break;
				}

				case GAUGE: /* new GAUGE  */
				{
					int h, by = 0;
					unsigned long last_colour = current_color;
#ifdef MATH
					float angle, px, py;
					double usage, scale;
#endif /* MATH */

					if (cur_x - text_start_x > maximum_width
							&& maximum_width > 0) {
						break;
					}

					h = current->height;
					by = cur_y - (font_ascent() / 2) - 1;

					if (h < font_h) {
						by -= h / 2 - 1;
					}
					w = current->width;
					if (w == 0) {
						w = text_start_x + text_width - cur_x - 1;
					}
					if (w < 0) {
						w = 0;
					}

					XSetLineAttributes(display, window.gc, 1, LineSolid,
							CapButt, JoinMiter);

					XDrawArc(display, window.drawable, window.gc,
							cur_x, by, w, h * 2, 0, 180*64);

#ifdef MATH
					usage = current->arg;
					scale = current->scale;
					angle = M_PI * usage / scale;
					px = (float)(cur_x+(w/2.))-(float)(w/2.)*cos(angle);
					py = (float)(by+(h))-(float)(h)*sin(angle);

					XDrawLine(display, window.drawable, window.gc,
							cur_x + (w/2.), by+(h), (int)(px), (int)(py));
#endif /* MATH */

					if (h > cur_y_add
							&& h > font_h) {
						cur_y_add = h;
					}

					set_foreground_color(last_colour);

					break;

				}

				case GRAPH:
				{
					int h, by, i = 0, j = 0;
					int colour_idx = 0;
					unsigned long last_colour = current_color;
					unsigned long *tmpcolour = 0;
					if (cur_x - text_start_x > maximum_width
							&& maximum_width > 0) {
						break;
					}
					h = current->height;
					by = cur_y - (font_ascent() / 2) - 1;

					if (h < font_h) {
						by -= h / 2 - 1;
					}
					w = current->width;
					if (w == 0) {
						w = text_start_x + text_width - cur_x - 1;
					}
					if (w < 0) {
						w = 0;
					}
					if (draw_graph_borders) {
						XSetLineAttributes(display, window.gc, 1, LineSolid,
							CapButt, JoinMiter);
						XDrawRectangle(display, window.drawable, window.gc,
							cur_x, by, w, h);
					}
					XSetLineAttributes(display, window.gc, 1, LineSolid,
						CapButt, JoinMiter);

					if (current->last_colour != 0
							|| current->first_colour != 0) {
						tmpcolour = do_gradient(w - 1, current->last_colour, current->first_colour);
					}
					colour_idx = 0;
					for (i = w - 2; i > -1; i--) {
						if (current->last_colour != 0
								|| current->first_colour != 0) {
							if (current->tempgrad) {
#ifdef DEBUG_lol
								assert(
										(int)((float)(w - 2) - current->graph[j] *
											(w - 2) / (float)current->scale)
										< w - 1
									  );
								assert(
										(int)((float)(w - 2) - current->graph[j] *
											(w - 2) / (float)current->scale)
										> -1
									  );
								if (current->graph[j] == current->scale) {
									assert(
											(int)((float)(w - 2) - current->graph[j] *
												(w - 2) / (float)current->scale)
											== 0
										  );
								}
#endif /* DEBUG_lol */
								XSetForeground(display, window.gc, tmpcolour[
										(int)((float)(w - 2) - current->graph[j] *
											(w - 2) / (float)current->scale)
										]);
							} else {
								XSetForeground(display, window.gc, tmpcolour[colour_idx++]);
							}
						}
						/* this is mugfugly, but it works */
						XDrawLine(display, window.drawable, window.gc,
								cur_x + i + 1, by + h, cur_x + i + 1,
								round_to_int((double)by + h - current->graph[j] *
									(h - 1) / current->scale));
						if ((w - i) / ((float) (w - 2) /
									(current->graph_width)) > j
								&& j < MAX_GRAPH_DEPTH - 3) {
							j++;
						}
					}
					free_and_zero(tmpcolour);
					if (h > cur_y_add
							&& h > font_h) {
						cur_y_add = h;
					}
					/* if (draw_mode == BG) {
						set_foreground_color(default_bg_color);
					} else if (draw_mode == OUTLINE) {
						set_foreground_color(default_out_color);
					} else {
						set_foreground_color(default_fg_color);
					} */
					if (show_graph_range) {
						int tmp_x = cur_x;
						int tmp_y = cur_y;
						unsigned short int seconds = update_interval * w;
						char *tmp_day_str;
						char *tmp_hour_str;
						char *tmp_min_str;
						char *tmp_sec_str;
						char *tmp_str;
						unsigned short int timeunits;
						if (seconds != 0) {
							timeunits = seconds / 86400; seconds %= 86400;
							if (timeunits <= 0 ||
									asprintf(&tmp_day_str, "%dd", timeunits) == -1) {
								tmp_day_str = strdup("");
							}
							timeunits = seconds / 3600; seconds %= 3600;
							if (timeunits <= 0 ||
									asprintf(&tmp_hour_str, "%dh", timeunits) == -1) {
								tmp_hour_str = strdup("");
							}
							timeunits = seconds / 60; seconds %= 60;
							if (timeunits <= 0 ||
									asprintf(&tmp_min_str, "%dm", timeunits) == -1) {
								tmp_min_str = strdup("");
							}
							if (seconds <= 0 ||
									asprintf(&tmp_sec_str, "%ds", seconds) == -1) {
								tmp_sec_str = strdup("");
							}
							if (asprintf(&tmp_str, "%s%s%s%s", tmp_day_str, tmp_hour_str, tmp_min_str, tmp_sec_str) == -1)
								tmp_str = strdup("");
							free(tmp_day_str); free(tmp_hour_str); free(tmp_min_str); free(tmp_sec_str);
						} else {
							tmp_str = strdup("Range not possible"); // should never happen, but better safe then sorry
						}
						cur_x += (w / 2) - (font_ascent() * (strlen(tmp_str) / 2));
						cur_y += font_h / 2;
						draw_string(tmp_str);
						free(tmp_str);
						cur_x = tmp_x;
						cur_y = tmp_y;
					}
#ifdef MATH
					if (show_graph_scale && (current->show_scale == 1)) {
						int tmp_x = cur_x;
						int tmp_y = cur_y;
						char *tmp_str;
						cur_x += font_ascent() / 2;
						cur_y += font_h / 2;
						tmp_str = (char *)
							calloc(log10(floor(current->scale)) + 4,
									sizeof(char));
						sprintf(tmp_str, "%.1f", current->scale);
						draw_string(tmp_str);
						free(tmp_str);
						cur_x = tmp_x;
						cur_y = tmp_y;
					}
#endif
					set_foreground_color(last_colour);
					break;
				}

				case FONT:
				{
					int old = font_ascent();

					cur_y -= font_ascent();
					selected_font = current->font_added;
					set_font();
					if (cur_y + font_ascent() < cur_y + old) {
						cur_y += old;
					} else {
						cur_y += font_ascent();
					}
					font_h = font_height();
					break;
				}
#endif /* BUILD_X11 */
				case FG:
					if (draw_mode == FG) {
						set_foreground_color(current->arg);
					}
					break;

#ifdef BUILD_X11
				case BG:
					if (draw_mode == BG) {
						set_foreground_color(current->arg);
					}
					break;

				case OUTLINE:
					if (draw_mode == OUTLINE) {
						set_foreground_color(current->arg);
					}
					break;

				case OFFSET:
					w += current->arg;
					last_special_needed = special_index;
					break;

				case VOFFSET:
					cur_y += current->arg;
					break;

				case GOTO:
					if (current->arg >= 0) {
						cur_x = (int) current->arg;
					}
					last_special_needed = special_index;
					break;

				case TAB:
				{
					int start = current->arg;
					int step = current->width;

					if (!step || step < 0) {
						step = 10;
					}
					w = step - (cur_x - text_start_x - start) % step;
					last_special_needed = special_index;
					break;
				}

				case ALIGNR:
				{
					/* TODO: add back in "+ window.border_inner_margin" to the end of
					 * this line? */
					int pos_x = text_start_x + text_width -
						get_string_width_special(s, special_index);

					/* printf("pos_x %i text_start_x %i text_width %i cur_x %i "
						"get_string_width(p) %i gap_x %i "
						"current->arg %i window.border_inner_margin %i "
						"window.border_width %i\n", pos_x, text_start_x, text_width,
						cur_x, get_string_width_special(s), gap_x,
						current->arg, window.border_inner_margin,
						window.border_width); */
					if (pos_x > current->arg && pos_x > cur_x) {
						cur_x = pos_x - current->arg;
					}
					last_special_needed = special_index;
					break;
				}

				case ALIGNC:
				{
					int pos_x = (text_width) / 2 - get_string_width_special(s,
							special_index) / 2 - (cur_x -
								text_start_x);
					/* int pos_x = text_start_x + text_width / 2 -
						get_string_width_special(s) / 2; */

					/* printf("pos_x %i text_start_x %i text_width %i cur_x %i "
						"get_string_width(p) %i gap_x %i "
						"current->arg %i\n", pos_x, text_start_x,
						text_width, cur_x, get_string_width(s), gap_x,
						current->arg); */
					if (pos_x > current->arg) {
						w = pos_x - current->arg;
					}
					last_special_needed = special_index;
					break;
				}
#endif /* BUILD_X11 */
			}

#ifdef BUILD_X11
			cur_x += w;
#endif /* BUILD_X11 */

			if (special_index != last_special_applied) {
				special_index++;
			} else {
				special_index = orig_special_index;
				last_special_applied = -1;
			}
		}
		p++;
	}

#ifdef BUILD_X11
	cur_y += cur_y_add;
#endif /* BUILD_X11 */
	draw_string(s);
#ifdef BUILD_NCURSES
	if (output_methods & TO_NCURSES) {
		printw("\n");
	}
#endif /* BUILD_NCURSES */
#ifdef BUILD_X11
	if (output_methods & TO_X)
		cur_y += font_descent();
#endif /* BUILD_X11 */
	if (recurse && *recurse) {
		special_index = draw_each_line_inner(recurse, special_index, last_special_needed);
		*(recurse - 1) = SECRIT_MULTILINE_CHAR;
	}
	return special_index;
}

static int draw_line(char *s, int special_index)
{
#ifdef BUILD_X11
	if (output_methods & TO_X) {
		return draw_each_line_inner(s, special_index, -1);
	}
#endif /* BUILD_X11 */
#ifdef BUILD_NCURSES
	if (output_methods & TO_NCURSES) {
		return draw_each_line_inner(s, special_index, -1);
	}
#endif /* BUILD_NCURSES */
	draw_string(s);
	UNUSED(special_index);
	return 0;
}

static void draw_text(void)
{
#ifdef BUILD_HTTP
#define WEBPAGE_START1 "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\" />"
#define WEBPAGE_START2 "<title>Conky</title></head><body style=\"font-family: monospace\"><p>"
#define WEBPAGE_END "</p></body></html>"
	if (output_methods & TO_HTTP) {
		webpage = WEBPAGE_START1;
		if(http_refresh) {
			webpage.append("<meta http-equiv=\"refresh\" content=\"");
			std::stringstream update_interval_str;
			update_interval_str << update_interval;
			webpage.append(update_interval_str.str());
			webpage.append("\" />");
		}
		webpage.append(WEBPAGE_START2);
	}
#endif
#ifdef BUILD_X11
#ifdef BUILD_LUA
	llua_draw_pre_hook();
#endif /* BUILD_LUA */
	if (output_methods & TO_X) {
		cur_y = text_start_y;

		/* draw borders */
		if (draw_borders && window.border_width > 0) {
			if (stippled_borders) {
				char ss[2] = { (char)stippled_borders, (char)stippled_borders };
				XSetLineAttributes(display, window.gc, window.border_width, LineOnOffDash,
					CapButt, JoinMiter);
				XSetDashes(display, window.gc, 0, ss, 2);
			} else {
				XSetLineAttributes(display, window.gc, window.border_width, LineSolid,
					CapButt, JoinMiter);
			}

			XDrawRectangle(display, window.drawable, window.gc,
				text_start_x - window.border_inner_margin - window.border_width,
				text_start_y - window.border_inner_margin - window.border_width,
				text_width + window.border_inner_margin * 2 + window.border_width * 2,
				text_height + window.border_inner_margin * 2 + window.border_width * 2);
		}

		/* draw text */
	}
	setup_fonts();
#endif /* BUILD_X11 */
#ifdef BUILD_NCURSES
	init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
	attron(COLOR_PAIR(COLOR_WHITE));
#endif /* BUILD_NCURSES */
	for_each_line(text_buffer, draw_line);
#if defined(BUILD_LUA) && defined(BUILD_X11)
	llua_draw_post_hook();
#endif /* BUILD_LUA */
#ifdef BUILD_HTTP
	if (output_methods & TO_HTTP) {
		webpage.append(WEBPAGE_END);
	}
#endif
}

static void draw_stuff(void)
{
#ifdef BUILD_IMLIB2
	cimlib_render(text_start_x, text_start_y, window.width, window.height);
#endif /* BUILD_IMLIB2 */
	if (overwrite_file) {
		overwrite_fpointer = fopen(overwrite_file, "w");
		if(!overwrite_fpointer)
			NORM_ERR("Can't overwrite '%s' anymore", overwrite_file);
	}
	if (append_file) {
		append_fpointer = fopen(append_file, "a");
		if(!append_fpointer)
			NORM_ERR("Can't append '%s' anymore", append_file);
	}
#ifdef BUILD_X11
	if (output_methods & TO_X) {
		selected_font = 0;
		if (draw_shades && !draw_outline) {
			text_start_x++;
			text_start_y++;
			set_foreground_color(default_bg_color);
			draw_mode = BG;
			draw_text();
			text_start_x--;
			text_start_y--;
		}

		if (draw_outline) {
			int i, j;
			selected_font = 0;

			for (i = -1; i < 2; i++) {
				for (j = -1; j < 2; j++) {
					if (i == 0 && j == 0) {
						continue;
					}
					text_start_x += i;
					text_start_y += j;
					set_foreground_color(default_out_color);
					draw_mode = OUTLINE;
					draw_text();
					text_start_x -= i;
					text_start_y -= j;
				}
			}
		}

		set_foreground_color(default_fg_color);
	}
#endif /* BUILD_X11 */
	draw_mode = FG;
	draw_text();
#if defined(BUILD_X11) && defined(BUILD_XDBE)
	if (output_methods & TO_X) {
		xdbe_swap_buffers();
	}
#endif /* BUILD_X11 && BUILD_XDBE */
	if(overwrite_fpointer) {
		fclose(overwrite_fpointer);
		overwrite_fpointer = 0;
	}
	if (append_fpointer) {
		fclose(append_fpointer);
		append_fpointer = 0;
	}
}

#ifdef BUILD_X11
static void clear_text(int exposures)
{
#ifdef BUILD_XDBE
	if (use_xdbe) {
		/* The swap action is XdbeBackground, which clears */
		return;
	} else
#endif
	if (display && window.window) { // make sure these are !null
		/* there is some extra space for borders and outlines */
		long border_total = window.border_inner_margin
						+ window.border_outer_margin + window.border_width;

		XClearArea(display, window.window, text_start_x - border_total, 
			text_start_y - border_total, text_width + 2*border_total,
			text_height + 2*border_total, exposures ? True : 0);
	}
}
#endif /* BUILD_X11 */

static int need_to_update;

/* update_text() generates new text and clears old text area */
static void update_text(void)
{
#ifdef BUILD_IMLIB2
	cimlib_cleanup();
#endif /* BUILD_IMLIB2 */
	generate_text();
#ifdef BUILD_X11
	if (output_methods & TO_X)
		clear_text(1);
#endif /* BUILD_X11 */
	need_to_update = 1;
#ifdef BUILD_LUA
	llua_update_info(&info, update_interval);
#endif /* BUILD_LUA */
}

#ifdef HAVE_SYS_INOTIFY_H
int inotify_fd;
#endif

static void main_loop(void)
{
	int terminate = 0;
#ifdef SIGNAL_BLOCKING
	sigset_t newmask, oldmask;
#endif
	double t;
#ifdef HAVE_SYS_INOTIFY_H
	int inotify_config_wd = -1;
#define INOTIFY_EVENT_SIZE  (sizeof(struct inotify_event))
#define INOTIFY_BUF_LEN     (20 * (INOTIFY_EVENT_SIZE + 16))
	char inotify_buff[INOTIFY_BUF_LEN];
#endif /* HAVE_SYS_INOTIFY_H */


#ifdef SIGNAL_BLOCKING
	sigemptyset(&newmask);
	sigaddset(&newmask, SIGINT);
	sigaddset(&newmask, SIGTERM);
	sigaddset(&newmask, SIGUSR1);
#endif

	last_update_time = 0.0;
	next_update_time = get_time();
	info.looped = 0;
	while (terminate == 0 && (total_run_times == 0 || info.looped < total_run_times)) {
		if(update_interval_bat != NOBATTERY && update_interval_bat != update_interval_old) {
			char buf[64];

			get_battery_short_status(buf, 64, "BAT0");
			if(buf[0] == 'D') {
				update_interval = update_interval_bat;
			} else {
				update_interval = update_interval_old;
			}
		}
		info.looped++;

#ifdef SIGNAL_BLOCKING
		/* block signals.  we will inspect for pending signals later */
		if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0) {
			CRIT_ERR(NULL, NULL, "unable to sigprocmask()");
		}
#endif

#ifdef BUILD_X11
		if (output_methods & TO_X) {
			XFlush(display);

			/* wait for X event or timeout */

			if (!XPending(display)) {
				fd_set fdsr;
				struct timeval tv;
				int s;
				t = next_update_time - get_time();

				if (t < 0) {
					t = 0;
				} else if (t > update_interval) {
					t = update_interval;
				}

				tv.tv_sec = (long) t;
				tv.tv_usec = (long) (t * 1000000) % 1000000;
				FD_ZERO(&fdsr);
				FD_SET(ConnectionNumber(display), &fdsr);

				s = select(ConnectionNumber(display) + 1, &fdsr, 0, 0, &tv);
				if (s == -1) {
					if (errno != EINTR) {
						NORM_ERR("can't select(): %s", strerror(errno));
					}
				} else {
					/* timeout */
					if (s == 0) {
						update_text();
					}
				}
			}

			if (need_to_update) {
#ifdef OWN_WINDOW
				int wx = window.x, wy = window.y;
#endif

				need_to_update = 0;
				selected_font = 0;
				update_text_area();

#if defined(OWN_WINDOW) || defined(BUILD_XDBE)
				long border_total = window.border_inner_margin
							+ window.border_outer_margin + window.border_width;
#endif
#ifdef OWN_WINDOW
				if (own_window) {
					int changed = 0;

					/* resize window if it isn't right size */
					if (!fixed_size
							&& (text_width + 2*border_total != window.width
								|| text_height + 2*border_total != window.height)) {
						window.width = text_width + 2*border_total;
						window.height = text_height + 2*border_total;
						draw_stuff(); /* redraw everything in our newly sized window */
						XResizeWindow(display, window.window, window.width,
								window.height); /* resize window */
						set_transparent_background(window.window, own_window_argb_value);
#ifdef BUILD_XDBE
						/* swap buffers */
						xdbe_swap_buffers();
#endif

						changed++;
#ifdef BUILD_LUA
						/* update lua window globals */
						llua_update_window_table(text_start_x, text_start_y, text_width, text_height);
#endif /* BUILD_LUA */
					}

					/* move window if it isn't in right position */
					if (!fixed_pos && (window.x != wx || window.y != wy)) {
						XMoveWindow(display, window.window, window.x, window.y);
						changed++;
					}

					/* update struts */
					if (changed && window.type == TYPE_PANEL) {
						int sidenum = -1;

						fprintf(stderr, PACKAGE_NAME": defining struts\n");
						fflush(stderr);

						switch (text_alignment) {
							case TOP_LEFT:
							case TOP_RIGHT:
							case TOP_MIDDLE:
								{
									sidenum = 2;
									break;
								}
							case BOTTOM_LEFT:
							case BOTTOM_RIGHT:
							case BOTTOM_MIDDLE:
								{
									sidenum = 3;
									break;
								}
							case MIDDLE_LEFT:
								{
									sidenum = 0;
									break;
								}
							case MIDDLE_RIGHT:
								{
									sidenum = 1;
									break;
								}
						}

						set_struts(sidenum);
					}
				}
#endif

				clear_text(1);

#ifdef BUILD_XDBE
				if (use_xdbe) {
					XRectangle r;

					r.x = text_start_x - border_total;
					r.y = text_start_y - border_total;
					r.width = text_width + 2*border_total;
					r.height = text_height + 2*border_total;
					XUnionRectWithRegion(&r, x11_stuff.region, x11_stuff.region);
				}
#endif
			}

			/* handle X events */
			while (XPending(display)) {
				XEvent ev;

				XNextEvent(display, &ev);
				switch (ev.type) {
					case Expose:
					{
						XRectangle r;
						r.x = ev.xexpose.x;
						r.y = ev.xexpose.y;
						r.width = ev.xexpose.width;
						r.height = ev.xexpose.height;
						XUnionRectWithRegion(&r, x11_stuff.region, x11_stuff.region);
						break;
					}

					case PropertyNotify:
					{
						if ( ev.xproperty.state == PropertyNewValue ) {
							get_x11_desktop_info( ev.xproperty.display, ev.xproperty.atom );
						}
						break;
					}

#ifdef OWN_WINDOW
					case ReparentNotify:
						/* make background transparent */
						if (own_window) {
							set_transparent_background(window.window, own_window_argb_value);
						}
						break;

					case ConfigureNotify:
						if (own_window) {
							/* if window size isn't what expected, set fixed size */
							if (ev.xconfigure.width != window.width
									|| ev.xconfigure.height != window.height) {
								if (window.width != 0 && window.height != 0) {
									fixed_size = 1;
								}

								/* clear old stuff before screwing up
								 * size and pos */
								clear_text(1);

								{
									XWindowAttributes attrs;
									if (XGetWindowAttributes(display,
											window.window, &attrs)) {
										window.width = attrs.width;
										window.height = attrs.height;
									}
								}

								text_width = window.width - window.border_inner_margin * 2 - window.border_outer_margin * 2 - window.border_width * 2;
								text_height = window.height - window.border_inner_margin * 2 - window.border_outer_margin * 2 - window.border_width * 2;
								if (text_width > maximum_width
										&& maximum_width > 0) {
									text_width = maximum_width;
								}
							}

							/* if position isn't what expected, set fixed pos
							 * total_updates avoids setting fixed_pos when window
							 * is set to weird locations when started */
							/* // this is broken
							if (total_updates >= 2 && !fixed_pos
									&& (window.x != ev.xconfigure.x
									|| window.y != ev.xconfigure.y)
									&& (ev.xconfigure.x != 0
									|| ev.xconfigure.y != 0)) {
								fixed_pos = 1;
							} */
						}
						break;

					case ButtonPress:
						if (own_window) {
							/* if an ordinary window with decorations */
							if ((window.type == TYPE_NORMAL &&
										(!TEST_HINT(window.hints,
													HINT_UNDECORATED))) ||
									window.type == TYPE_DESKTOP) {
								/* allow conky to hold input focus. */
								break;
							} else {
								/* forward the click to the desktop window */
								XUngrabPointer(display, ev.xbutton.time);
								ev.xbutton.window = window.desktop;
								ev.xbutton.x = ev.xbutton.x_root;
								ev.xbutton.y = ev.xbutton.y_root;
								XSendEvent(display, ev.xbutton.window, False,
									ButtonPressMask, &ev);
								XSetInputFocus(display, ev.xbutton.window,
									RevertToParent, ev.xbutton.time);
							}
						}
						break;

					case ButtonRelease:
						if (own_window) {
							/* if an ordinary window with decorations */
							if ((window.type == TYPE_NORMAL)
									&& (!TEST_HINT(window.hints,
									HINT_UNDECORATED))) {
								/* allow conky to hold input focus. */
								break;
							} else {
								/* forward the release to the desktop window */
								ev.xbutton.window = window.desktop;
								ev.xbutton.x = ev.xbutton.x_root;
								ev.xbutton.y = ev.xbutton.y_root;
								XSendEvent(display, ev.xbutton.window, False,
									ButtonReleaseMask, &ev);
							}
						}
						break;

#endif

					default:
#ifdef BUILD_XDAMAGE
						if (ev.type == x11_stuff.event_base + XDamageNotify) {
							XDamageNotifyEvent *dev = (XDamageNotifyEvent *) &ev;

							XFixesSetRegion(display, x11_stuff.part, &dev->area, 1);
							XFixesUnionRegion(display, x11_stuff.region2, x11_stuff.region2, x11_stuff.part);
						}
#endif /* BUILD_XDAMAGE */
						break;
				}
			}

#ifdef BUILD_XDAMAGE
			XDamageSubtract(display, x11_stuff.damage, x11_stuff.region2, None);
			XFixesSetRegion(display, x11_stuff.region2, 0, 0);
#endif /* BUILD_XDAMAGE */

			/* XDBE doesn't seem to provide a way to clear the back buffer
			 * without interfering with the front buffer, other than passing
			 * XdbeBackground to XdbeSwapBuffers. That means that if we're
			 * using XDBE, we need to redraw the text even if it wasn't part of
			 * the exposed area. OTOH, if we're not going to call draw_stuff at
			 * all, then no swap happens and we can safely do nothing. */

			if (!XEmptyRegion(x11_stuff.region)) {
#ifdef BUILD_XDBE
				if (use_xdbe) {
					XRectangle r;

					long border_total = window.border_inner_margin
									+ window.border_outer_margin + window.border_width;
					r.x = text_start_x - border_total;
					r.y = text_start_y - border_total;
					r.width = text_width + 2*border_total;
					r.height = text_height + 2*border_total;
					XUnionRectWithRegion(&r, x11_stuff.region, x11_stuff.region);
				}
#endif
				XSetRegion(display, window.gc, x11_stuff.region);
#ifdef BUILD_XFT
				if (use_xft) {
					XftDrawSetClip(window.xftdraw, x11_stuff.region);
				}
#endif
				draw_stuff();
				XDestroyRegion(x11_stuff.region);
				x11_stuff.region = XCreateRegion();
			}
		} else {
#endif /* BUILD_X11 */
			t = (next_update_time - get_time()) * 1000000;
			if(t > 0) usleep((useconds_t)t);
			update_text();
			draw_stuff();
#ifdef BUILD_NCURSES
			if(output_methods & TO_NCURSES) {
				refresh();
				clear();
			}
#endif
#ifdef BUILD_X11
		}
#endif /* BUILD_X11 */

#ifdef SIGNAL_BLOCKING
		/* unblock signals of interest and let handler fly */
		if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
			CRIT_ERR(NULL, NULL, "unable to sigprocmask()");
		}
#endif

		switch (g_signal_pending) {
			case SIGHUP:
			case SIGUSR1:
				NORM_ERR("received SIGHUP or SIGUSR1. reloading the config file.");
				reload_config();
				break;
			case SIGINT:
			case SIGTERM:
				NORM_ERR("received SIGINT or SIGTERM to terminate. bye!");
				terminate = 1;
#ifdef BUILD_X11
				if (output_methods & TO_X) {
					XDestroyRegion(x11_stuff.region);
					x11_stuff.region = NULL;
#ifdef BUILD_XDAMAGE
					XDamageDestroy(display, x11_stuff.damage);
					XFixesDestroyRegion(display, x11_stuff.region2);
					XFixesDestroyRegion(display, x11_stuff.part);
#endif /* BUILD_XDAMAGE */
					free_and_zero(disp);
				}
#endif /* BUILD_X11 */
				free_and_zero(overwrite_file);
				free_and_zero(append_file);
				break;
			default:
				/* Reaching here means someone set a signal
				 * (SIGXXXX, signal_handler), but didn't write any code
				 * to deal with it.
				 * If you don't want to handle a signal, don't set a handler on
				 * it in the first place. */
				if (g_signal_pending) {
					NORM_ERR("ignoring signal (%d)", g_signal_pending);
				}
				break;
		}
#ifdef HAVE_SYS_INOTIFY_H
		if (!disable_auto_reload && inotify_fd != -1
						&& inotify_config_wd == -1 && !current_config.empty()) {
			inotify_config_wd = inotify_add_watch(inotify_fd,
					current_config.c_str(),
					IN_MODIFY);
		}
		if (!disable_auto_reload && inotify_fd != -1
						&& inotify_config_wd != -1 && !current_config.empty()) {
			int len = 0, idx = 0;
			fd_set descriptors;
			struct timeval time_to_wait;

			FD_ZERO(&descriptors);
			FD_SET(inotify_fd, &descriptors);

			time_to_wait.tv_sec = time_to_wait.tv_usec = 0;

			select(inotify_fd + 1, &descriptors, NULL, NULL, &time_to_wait);
			if (FD_ISSET(inotify_fd, &descriptors)) {
				/* process inotify events */
				len = read(inotify_fd, inotify_buff, INOTIFY_BUF_LEN);
				while (len > 0 && idx < len) {
					struct inotify_event *ev = (struct inotify_event *) &inotify_buff[idx];
					if (ev->wd == inotify_config_wd && (ev->mask & IN_MODIFY || ev->mask & IN_IGNORED)) {
						/* current_config should be reloaded */
						NORM_ERR("'%s' modified, reloading...", current_config.c_str());
						reload_config();
						if (ev->mask & IN_IGNORED) {
							/* for some reason we get IN_IGNORED here
							 * sometimes, so we need to re-add the watch */
							inotify_config_wd = inotify_add_watch(inotify_fd,
									current_config.c_str(),
									IN_MODIFY);
						}
						break;
					}
#ifdef BUILD_LUA
					else {
						llua_inotify_query(ev->wd, ev->mask);
					}
#endif /* BUILD_LUA */
					idx += INOTIFY_EVENT_SIZE + ev->len;
				}
			}
		} else if (disable_auto_reload && inotify_fd != -1) {
			inotify_rm_watch(inotify_fd, inotify_config_wd);
			close(inotify_fd);
			inotify_fd = inotify_config_wd = 0;
		}
#endif /* HAVE_SYS_INOTIFY_H */

#ifdef BUILD_LUA
		llua_update_info(&info, update_interval);
#endif /* BUILD_LUA */
		g_signal_pending = 0;
	}
	clean_up(current_mail_spool, NULL);

#ifdef HAVE_SYS_INOTIFY_H
	if (inotify_fd != -1) {
		inotify_rm_watch(inotify_fd, inotify_config_wd);
		close(inotify_fd);
		inotify_fd = inotify_config_wd = 0;
	}
#endif /* HAVE_SYS_INOTIFY_H */
}

#ifdef BUILD_X11
static void load_config_file_x11(const char *);
#endif /* BUILD_X11 */
void initialisation(int argc, char** argv);

	/* reload the config file */
static void reload_config(void)
{
	clean_up(NULL, NULL);
	sleep(1); /* slight pause */
	initialisation(argc_copy, argv_copy);
}

#ifdef BUILD_X11
void clean_up_x11() {
	if(window_created == 1) {
		long border_total = window.border_inner_margin
							+ window.border_outer_margin + window.border_width;
		XClearArea(display, window.window, text_start_x - border_total,
			text_start_y - border_total, text_width + 2*border_total,
			text_height + 2*border_total, 0);
	}
	destroy_window();
	free_fonts();
	if(x11_stuff.region) {
		XDestroyRegion(x11_stuff.region);
		x11_stuff.region = NULL;
	}
	if(display) {
		XCloseDisplay(display);
		display = NULL;
	}
	free_and_zero(info.x11.desktop.all_names);
	free_and_zero(info.x11.desktop.name);
	x_initialised = NO;
}
#endif

void free_specials(special_t *current) {
	if (current) {
		free_specials(current->next);
		if(current->type == GRAPH)
			free(current->graph);
		delete current;
		current = NULL;
	}
}

void clean_up_without_threads(void *memtofree1, void* memtofree2) {

#ifdef BUILD_NCURSES
	if(output_methods & TO_NCURSES) {
		endwin();
	}
#endif
#ifdef BUILD_HTTP
	if(output_methods & TO_HTTP) {
		MHD_stop_daemon(httpd);
	}
#endif
	conftree_empty(currentconffile);
	currentconffile = NULL;
	free_and_zero(memtofree1);
	free_and_zero(memtofree2);
	timed_thread::destroy_registered_threads();

	free_and_zero(info.cpu_usage);
#ifdef BUILD_X11
	if (x_initialised == YES) {
		clean_up_x11();
	}else{
		free(fonts);	//in set_default_configurations a font is set but not loaded
		font_count = -1;
	}

#endif /* BUILD_X11 */

	free_templates();

	if (info.first_process) {
		free_all_processes();
		info.first_process = NULL;
	}

#ifdef BUILD_X11
	free_desktop_info();
#endif /* BUILD_X11 */

	free_text_objects(&global_root_object);
	free_and_zero(tmpstring1);
	free_and_zero(tmpstring2);
	free_and_zero(text_buffer);
	free_and_zero(global_text);

#ifdef BUILD_PORT_MONITORS
	tcp_portmon_clear();
#endif
#ifdef BUILD_CURL
	ccurl_free_info();
#endif
#ifdef BUILD_RSS
	rss_free_info();
#endif
#if defined BUILD_WEATHER_METAR || defined BUILD_WEATHER_XOAP
	weather_free_info();
#endif
#ifdef BUILD_LUA
	llua_shutdown_hook();
	llua_close();
#endif /* BUILD_LUA */
#ifdef BUILD_IMLIB2
	if (output_methods & TO_X)
		cimlib_deinit();
#endif /* BUILD_IMLIB2 */
#if defined BUILD_WEATHER_XOAP || defined BUILD_RSS
	xmlCleanupParser();
#endif
#ifdef BUILD_MYSQL
	free_mysql_global();
#endif

	free_specials(specials);

	clear_net_stats();
	clear_diskio_stats();
	free_and_zero(global_cpu);
}

void clean_up(void *memtofree1, void* memtofree2)
{
	free_update_callbacks();
	clean_up_without_threads(memtofree1, memtofree2);
}

static bool string_to_bool(const char *s)
{
	if (!s) {
		// Assumes an option without a true/false means true
		return true;
	} else if (strcasecmp(s, "yes") == EQUAL) {
		return true;
	} else if (strcasecmp(s, "true") == EQUAL) {
		return true;
	} else if (strcasecmp(s, "1") == EQUAL) {
		return true;
	}
	return false;
}

#ifdef BUILD_X11
static enum alignment string_to_alignment(const char *s)
{
	if (strcasecmp(s, "top_left") == EQUAL) {
		return TOP_LEFT;
	} else if (strcasecmp(s, "top_right") == EQUAL) {
		return TOP_RIGHT;
	} else if (strcasecmp(s, "top_middle") == EQUAL) {
		return TOP_MIDDLE;
	} else if (strcasecmp(s, "bottom_left") == EQUAL) {
		return BOTTOM_LEFT;
	} else if (strcasecmp(s, "bottom_right") == EQUAL) {
		return BOTTOM_RIGHT;
	} else if (strcasecmp(s, "bottom_middle") == EQUAL) {
		return BOTTOM_MIDDLE;
	} else if (strcasecmp(s, "middle_left") == EQUAL) {
		return MIDDLE_LEFT;
	} else if (strcasecmp(s, "middle_right") == EQUAL) {
		return MIDDLE_RIGHT;
	} else if (strcasecmp(s, "middle_middle") == EQUAL) {
		return MIDDLE_MIDDLE;
	} else if (strcasecmp(s, "tl") == EQUAL) {
		return TOP_LEFT;
	} else if (strcasecmp(s, "tr") == EQUAL) {
		return TOP_RIGHT;
	} else if (strcasecmp(s, "tm") == EQUAL) {
		return TOP_MIDDLE;
	} else if (strcasecmp(s, "bl") == EQUAL) {
		return BOTTOM_LEFT;
	} else if (strcasecmp(s, "br") == EQUAL) {
		return BOTTOM_RIGHT;
	} else if (strcasecmp(s, "bm") == EQUAL) {
		return BOTTOM_MIDDLE;
	} else if (strcasecmp(s, "ml") == EQUAL) {
		return MIDDLE_LEFT;
	} else if (strcasecmp(s, "mr") == EQUAL) {
		return MIDDLE_RIGHT;
	} else if (strcasecmp(s, "mm") == EQUAL) {
		return MIDDLE_MIDDLE;
	} else if (strcasecmp(s, "none") == EQUAL) {
		return NONE;
	}
	return ALIGNMENT_ERROR;
}
#endif /* BUILD_X11 */

#ifdef BUILD_X11
static void set_default_configurations_for_x(void)
{
	default_fg_color = WhitePixel(display, screen);
	default_bg_color = BlackPixel(display, screen);
	default_out_color = BlackPixel(display, screen);
	color0 = default_fg_color;
	color1 = default_fg_color;
	color2 = default_fg_color;
	color3 = default_fg_color;
	color4 = default_fg_color;
	color5 = default_fg_color;
	color6 = default_fg_color;
	color7 = default_fg_color;
	color8 = default_fg_color;
	color9 = default_fg_color;
	current_text_color = default_fg_color;
}
#endif /* BUILD_X11 */

static void set_default_configurations(void)
{
#ifdef BUILD_MPD
	char *mpd_env_host;
	char *mpd_env_port;
#endif /* BUILD_MPD */
	update_uname();
	fork_to_background = 0;
	total_run_times = 0;
	info.cpu_avg_samples = 2;
	info.net_avg_samples = 2;
	info.diskio_avg_samples = 2;
	info.memmax = 0;
	top_cpu = 0;
	cpu_separate = 0;
	short_units = 0;
	format_human_readable = 1;
	top_mem = 0;
	top_time = 0;
#ifdef BUILD_HTTP
	http_refresh = false;
#endif
#ifdef BUILD_IOSTATS
	top_io = 0;
#endif
	top_running = 0;
#ifdef BUILD_MPD
	mpd_env_host = getenv("MPD_HOST");
	mpd_env_port = getenv("MPD_PORT");

	if (!mpd_env_host || !strlen(mpd_env_host)) {
		mpd_set_host("localhost");
	} else {
		/* MPD_HOST environment variable is set */
		char *mpd_hostpart = strchr(mpd_env_host, '@');
		if (!mpd_hostpart) {
			mpd_set_host(mpd_env_host);
		} else {
			/* MPD_HOST contains a password */
			char *mpd_password = (char *)malloc(mpd_hostpart - mpd_env_host + 1);
			snprintf(mpd_password, mpd_hostpart - mpd_env_host + 1, "%s", mpd_env_host);

			if (!strlen(mpd_hostpart + 1)) {
				mpd_set_host("localhost");
			} else {
				mpd_set_host(mpd_hostpart + 1);
			}

			mpd_set_password(mpd_password, 1);
			free(mpd_password);
		}
	}


	if (!mpd_env_port || mpd_set_port(mpd_env_port)) {
		/* failed to set port from environment variable */
		mpd_set_port("6600");
	}
#endif /* BUILD_MPD */
#ifdef BUILD_MYSQL
	mysql_settings.host = NULL;
	mysql_settings.port = 0;
	mysql_settings.user = NULL;
	mysql_settings.password = NULL;
	mysql_settings.db = NULL;
#endif /* BUILD_MYSQL */
#ifdef BUILD_XMMS2
	info.xmms2.artist = NULL;
	info.xmms2.album = NULL;
	info.xmms2.title = NULL;
	info.xmms2.genre = NULL;
	info.xmms2.comment = NULL;
	info.xmms2.url = NULL;
	info.xmms2.status = NULL;
	info.xmms2.playlist = NULL;
#endif /* BUILD_XMMS2 */
	use_spacer = NO_SPACER;
#ifdef BUILD_X11
	output_methods = TO_X;
#else
	output_methods = TO_STDOUT;
#endif
#ifdef BUILD_X11
#ifdef BUILD_XFT
	use_xft = 0;
#endif
	show_graph_scale = 0;
	show_graph_range = 0;
	draw_shades = 1;
	draw_borders = 0;
	draw_graph_borders = 1;
	draw_outline = 0;
	set_first_font("6x10");
	gap_x = 5;
	gap_y = 60;
	minimum_width = 5;
	minimum_height = 5;
	maximum_width = 0;
#ifdef OWN_WINDOW
	own_window = 0;
	window.type = TYPE_NORMAL;
	window.hints = 0;
	strcpy(window.class_name, PACKAGE_NAME);
	sprintf(window.title, PACKAGE_NAME" (%s)", info.uname_s.nodename);
#ifdef BUILD_ARGB
	use_argb_visual = 0;
	own_window_argb_value = 255;
#endif
#endif
	stippled_borders = 0;
	window.border_inner_margin = 3;
	window.border_outer_margin = 1;
	window.border_width = 1;
	text_alignment = BOTTOM_LEFT;
	info.x11.monitor.number = 1;
	info.x11.monitor.current = 0;
	info.x11.desktop.current = 1;
	info.x11.desktop.number = 1;
	info.x11.desktop.nitems = 0;
	info.x11.desktop.all_names = NULL;
	info.x11.desktop.name = NULL;
#endif /* BUILD_X11 */

	free_templates();

	free(current_mail_spool);
	{
		char buf[256];

		variable_substitute(MAIL_FILE, buf, 256);
		if (buf[0] != '\0') {
			current_mail_spool = strndup(buf, text_buffer_size);
		}
	}

	no_buffers = 1;
	set_update_interval(3);
	update_interval_bat = NOBATTERY;
	info.music_player_interval = 1.0;
	stuff_in_uppercase = 0;
	info.users.number = 1;

	set_times_in_seconds(0);

#ifdef BUILD_PORT_MONITORS
	/* set default connection limit */
	tcp_portmon_set_max_connections(0);
#endif
}

/* returns 1 if you can overwrite or create the file at 'path' */
static bool overwrite_works(const char *path)
{
	FILE *filepointer;

	if (!(filepointer = fopen(path, "w")))
		return false;
	fclose(filepointer);
	return true;
}

/* returns 1 if you can append or create the file at 'path' */
static bool append_works(const char *path)
{
	FILE *filepointer;

	if (!(filepointer = fopen(path, "a")))
		return false;
	fclose(filepointer);
	return true;
}

#ifdef BUILD_X11
#ifdef DEBUG
/* WARNING, this type not in Xlib spec */
int x11_error_handler(Display *d, XErrorEvent *err)
	__attribute__((noreturn));
int x11_error_handler(Display *d, XErrorEvent *err)
{
	NORM_ERR("X Error: type %i Display %lx XID %li serial %lu error_code %i request_code %i minor_code %i other Display: %lx\n",
			err->type,
			(long unsigned)err->display,
			(long)err->resourceid,
			err->serial,
			err->error_code,
			err->request_code,
			err->minor_code,
			(long unsigned)d
			);
	abort();
}

int x11_ioerror_handler(Display *d)
	__attribute__((noreturn));
int x11_ioerror_handler(Display *d)
{
	NORM_ERR("X Error: Display %lx\n",
			(long unsigned)d
			);
	exit(1);
}
#endif /* DEBUG */

static void X11_initialisation(void)
{
	if (x_initialised == YES) return;
	output_methods |= TO_X;
	init_X11(disp);
	set_default_configurations_for_x();
	x_initialised = YES;
#ifdef DEBUG
	_Xdebug = 1;
	/* WARNING, this type not in Xlib spec */
	XSetErrorHandler(&x11_error_handler);
	XSetIOErrorHandler(&x11_ioerror_handler);
#endif /* DEBUG */
}

static void X11_create_window(void)
{
	if (output_methods & TO_X) {
#ifdef OWN_WINDOW
		init_window(own_window, text_width + window.border_inner_margin * 2 + window.border_outer_margin * 2 + window.border_width * 2,
				text_height + window.border_inner_margin * 2 + window.border_outer_margin * 2 + window.border_width * 2, set_transparent, background_colour,
				argv_copy, argc_copy);
#else /* OWN_WINDOW */
		init_window(0, text_width + window.border_inner_margin * 2 + window.border_outer_margin * 2 + window.border_width * 2,
				text_height + window.border_inner_margin * 2 + window.border_outer_margin * 2 + window.border_width * 2, set_transparent, 0,
				argv_copy, argc_copy);
#endif /* OWN_WINDOW */

		setup_fonts();
		load_fonts();
		update_text_area();	/* to position text/window on screen */

#ifdef OWN_WINDOW
		if (own_window && !fixed_pos) {
			XMoveWindow(display, window.window, window.x, window.y);
		}
		if (own_window) {
			set_transparent_background(window.window, own_window_argb_value);
		}
#endif

		create_gc();

		draw_stuff();

		x11_stuff.region = XCreateRegion();
#ifdef BUILD_XDAMAGE
		if (!XDamageQueryExtension(display, &x11_stuff.event_base, &x11_stuff.error_base)) {
			NORM_ERR("Xdamage extension unavailable");
		}
		x11_stuff.damage = XDamageCreate(display, window.window, XDamageReportNonEmpty);
		x11_stuff.region2 = XFixesCreateRegionFromWindow(display, window.window, 0);
		x11_stuff.part = XFixesCreateRegionFromWindow(display, window.window, 0);
#endif /* BUILD_XDAMAGE */

		selected_font = 0;
		update_text_area();	/* to get initial size of the window */
	}
#ifdef BUILD_LUA
	/* setup lua window globals */
	llua_setup_window_table(text_start_x, text_start_y, text_width, text_height);
#endif /* BUILD_LUA */
}
#endif /* BUILD_X11 */

#define CONF_ERR NORM_ERR("%s: %d: config file error", f, line)
#define CONF_ERR2(a) NORM_ERR("%s: %d: config file error: %s", f, line, a)
#define CONF2(a) if (strcasecmp(name, a) == 0)
#define CONF(a) else CONF2(a)
#define CONF3(a, b) else if (strcasecmp(name, a) == 0 \
		|| strcasecmp(name, b) == 0)
#define CONF_CONTINUE 1
#define CONF_BREAK 2
#define CONF_BUFF_SIZE 512

static FILE *open_config_file(const char *f)
{
#ifdef BUILD_BUILTIN_CONFIG
	if (!strcmp(f, "==builtin==")) {
		return conf_cookie_open();
	} else
#endif /* BUILD_BUILTIN_CONFIG */
		return fopen(f, "r");
}

static int do_config_step(int *line, FILE *fp, char *buf, char **name, char **value)
{
	char *p, *p2;
	(*line)++;
	if (fgets(buf, CONF_BUFF_SIZE, fp) == NULL) {
		return CONF_BREAK;
	}
	remove_comments(buf);

	p = buf;

	/* skip spaces */
	while (*p && isspace((int) *p)) {
		p++;
	}
	if (*p == '\0') {
		return CONF_CONTINUE;	/* empty line */
	}

	*name = p;

	/* skip name */
	p2 = p;
	while (*p2 && !isspace((int) *p2)) {
		p2++;
	}
	if (*p2 != '\0') {
		*p2 = '\0';	/* break at name's end */
		p2++;
	}

	/* get value */
	if (*p2) {
		p = p2;
		while (*p && isspace((int) *p)) {
			p++;
		}

		*value = p;

		p2 = *value + strlen(*value);
		while (isspace((int) *(p2 - 1))) {
			*--p2 = '\0';
		}
	} else {
		*value = 0;
	}
	return 0;
}

#ifdef BUILD_X11
void setalignment(int* text_alignment, const char* value, const char *f, int line, bool conffile) {
#ifdef OWN_WINDOW
	if (window.type == TYPE_DOCK) {
		NORM_ERR("alignment is disabled when own_window_type is dock");
	} else
#endif /*OWN_WINDOW */
	if (value) {
		int a = string_to_alignment(value);

		if (a <= 0) {
			if(conffile == true) {
				CONF_ERR;
			} else NORM_ERR("'%s' is not a alignment setting", value);
		} else {
			*text_alignment = a;
		}
	} else if(conffile == true) {
		CONF_ERR;
	}
}
#endif /* BUILD_X11 */

char load_config_file(const char *f)
{
	int line = 0;
	FILE *fp;

	fp = open_config_file(f);
	if (!fp) {
		return FALSE;
	}
	DBGP("reading contents from config file '%s'", f);

	while (!feof(fp)) {
		char buff[CONF_BUFF_SIZE], *name, *value;
		int ret = do_config_step(&line, fp, buff, &name, &value);
		if (ret == CONF_BREAK) {
			break;
		} else if (ret == CONF_CONTINUE) {
			continue;
		}

#ifdef BUILD_X11
		CONF2("out_to_x") {
			if (string_to_bool(value)) {
				output_methods &= TO_X;
			} else {
				clean_up_x11();
				output_methods &= ~TO_X;
				x_initialised = NEVER;
			}
		}
		CONF("display") {
			if (!value || x_initialised == YES) {
				CONF_ERR;
			} else {
				free_and_zero(disp);
				disp = strdup(value);
			}
		}
		CONF("alignment") {
			setalignment(&text_alignment, value, f, line, true);
		}
		CONF("background") {
			fork_to_background = string_to_bool(value);
		}
#else
		CONF2("background") {
			fork_to_background = string_to_bool(value);
		}
#endif /* BUILD_X11 */
#ifdef BUILD_X11
		CONF("show_graph_scale") {
			show_graph_scale = string_to_bool(value);
		}
		CONF("show_graph_range") {
			show_graph_range = string_to_bool(value);
		}
		CONF("border_inner_margin") {
			if (value) {
				window.border_inner_margin = strtol(value, 0, 0);
				if (window.border_inner_margin < 0) window.border_inner_margin = 0;
			} else {
				CONF_ERR;
			}
		}
		CONF("border_outer_margin") {
			if (value) {
				window.border_outer_margin = strtol(value, 0, 0);
				if (window.border_outer_margin < 0) window.border_outer_margin = 0;
			} else {
				CONF_ERR;
			}
		}
		CONF("border_width") {
			if (value) {
				window.border_width = strtol(value, 0, 0);
				if (window.border_width < 0) window.border_width = 0;
			} else {
				CONF_ERR;
			}
		}
#endif /* BUILD_X11 */
#define TEMPLATE_CONF(n) \
		CONF("template"#n) { \
			if (set_template(n, value)) \
				CONF_ERR; \
		}
		TEMPLATE_CONF(0)
		TEMPLATE_CONF(1)
		TEMPLATE_CONF(2)
		TEMPLATE_CONF(3)
		TEMPLATE_CONF(4)
		TEMPLATE_CONF(5)
		TEMPLATE_CONF(6)
		TEMPLATE_CONF(7)
		TEMPLATE_CONF(8)
		TEMPLATE_CONF(9)
		CONF("imap") {
			if (value) {
				parse_global_imap_mail_args(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("pop3") {
			if (value) {
				parse_global_pop3_mail_args(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("default_bar_size") {
			char err = 0;
			if (value) {
				if (sscanf(value, "%d %d", &default_bar_width, &default_bar_height) != 2) {
					err = 1;
				}
			} else {
				err = 1;
			}
			if (err) {
				CONF_ERR2("default_bar_size takes 2 integer arguments (ie. 'default_bar_size 0 6')")
			}
		}
#ifdef BUILD_X11
		CONF("default_graph_size") {
			char err = 0;
			if (value) {
				if (sscanf(value, "%d %d", &default_graph_width, &default_graph_height) != 2) {
					err = 1;
				}
			} else {
				err = 1;
			}
			if (err) {
				CONF_ERR2("default_graph_size takes 2 integer arguments (ie. 'default_graph_size 0 6')")
			}
		}
		CONF("default_gauge_size") {
			char err = 0;
			if (value) {
				if (sscanf(value, "%d %d", &default_gauge_width, &default_gauge_height) != 2) {
					err = 1;
				}
			} else {
				err = 1;
			}
			if (err) {
				CONF_ERR2("default_gauge_size takes 2 integer arguments (ie. 'default_gauge_size 0 6')")
			}
		}
#endif
#ifdef BUILD_MPD
		CONF("mpd_host") {
			if (value) {
				mpd_set_host(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("mpd_port") {
			if (value && mpd_set_port(value)) {
				CONF_ERR;
			}
		}
		CONF("mpd_password") {
			if (value) {
				mpd_set_password(value, 0);
			} else {
				CONF_ERR;
			}
		}
#endif /* BUILD_MPD */
#ifdef BUILD_MYSQL
		CONF("mysql_host") {
			if (value) {
				mysql_set_host(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("mysql_port") {
			if (value) {
				mysql_set_port(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("mysql_user") {
			if (value) {
				mysql_set_user(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("mysql_password") {
			if (value) {
				mysql_set_password(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("mysql_db") {
			if (value) {
				mysql_set_db(value);
			} else {
				CONF_ERR;
			}
		}
#endif /* BUILD_MYSQL */
		CONF("music_player_interval") {
			if (value) {
				info.music_player_interval = strtod(value, 0);
			} else {
				CONF_ERR;
			}
		}
#ifdef __OpenBSD__
		CONF("sensor_device") {
			if (value) {
				sensor_device = strtol(value, 0, 0);
			} else {
				CONF_ERR;
			}
		}
#endif
		CONF("cpu_avg_samples") {
			if (value) {
				cpu_avg_samples = strtol(value, 0, 0);
				if (cpu_avg_samples < 1 || cpu_avg_samples > 14) {
					CONF_ERR;
				} else {
					info.cpu_avg_samples = cpu_avg_samples;
				}
			} else {
				CONF_ERR;
			}
		}
		CONF("net_avg_samples") {
			if (value) {
				net_avg_samples = strtol(value, 0, 0);
				if (net_avg_samples < 1 || net_avg_samples > 14) {
					CONF_ERR;
				} else {
					info.net_avg_samples = net_avg_samples;
				}
			} else {
				CONF_ERR;
			}
		}
		CONF("diskio_avg_samples") {
			if (value) {
				diskio_avg_samples = strtol(value, 0, 0);
				if (diskio_avg_samples < 1 || diskio_avg_samples > 14) {
					CONF_ERR;
				} else {
					info.diskio_avg_samples = diskio_avg_samples;
				}
			} else {
				CONF_ERR;
			}
		}

#ifdef BUILD_XDBE
		CONF("double_buffer") {
			use_xdbe = string_to_bool(value);
		}
#endif
#ifdef BUILD_X11
		CONF("override_utf8_locale") {
			utf8_mode = string_to_bool(value);
		}
		CONF("draw_borders") {
			draw_borders = string_to_bool(value);
		}
		CONF("draw_graph_borders") {
			draw_graph_borders = string_to_bool(value);
		}
		CONF("draw_shades") {
			draw_shades = string_to_bool(value);
		}
		CONF("draw_outline") {
			draw_outline = string_to_bool(value);
		}
#endif /* BUILD_X11 */
		CONF("times_in_seconds") {
			set_times_in_seconds(string_to_bool(value));
		}
		CONF("max_text_width") {
			max_text_width = atoi(value);
		}
		CONF("out_to_console") {
			if(string_to_bool(value)) {
				output_methods |= TO_STDOUT;
			} else {
				output_methods &= ~TO_STDOUT;
			}
		}
		CONF("extra_newline") {
			extra_newline = string_to_bool(value);
		}
		CONF("disable_auto_reload") {
			disable_auto_reload = string_to_bool(value);
		}
		CONF("out_to_stderr") {
			if(string_to_bool(value))
				output_methods |= TO_STDERR;
		}
#ifdef BUILD_HTTP
		CONF("out_to_http") {
			if(string_to_bool(value)) {
				output_methods |= TO_HTTP;
				httpd = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, HTTPPORT, NULL, NULL, &sendanswer, NULL, MHD_OPTION_END);
			}
		}
		CONF("http_refresh") {
			if(string_to_bool(value)) {
				http_refresh = true;
			}
		}
#endif
#ifdef BUILD_NCURSES
		CONF("out_to_ncurses") {
			if(string_to_bool(value)) {
				initscr();
				start_color();
				output_methods |= TO_NCURSES;
			}
		}
#endif
		CONF("overwrite_file") {
			free_and_zero(overwrite_file);
			if (overwrite_works(value)) {
				overwrite_file = strdup(value);
				output_methods |= OVERWRITE_FILE;
			} else
				NORM_ERR("overwrite_file won't be able to create/overwrite '%s'", value);
		}
		CONF("append_file") {
			free_and_zero(append_file);
			if(append_works(value)) {
				append_file = strdup(value);
				output_methods |= APPEND_FILE;
			} else
				NORM_ERR("append_file won't be able to create/append '%s'", value);
		}
		CONF("use_spacer") {
			if (value) {
				if (strcasecmp(value, "left") == EQUAL) {
					use_spacer = LEFT_SPACER;
				} else if (strcasecmp(value, "right") == EQUAL) {
					use_spacer = RIGHT_SPACER;
				} else if (strcasecmp(value, "none") == EQUAL) {
					use_spacer = NO_SPACER;
				} else {
					use_spacer = string_to_bool(value) ? RIGHT_SPACER : NO_SPACER;
					NORM_ERR("use_spacer should have an argument of left, right, or"
						" none.  '%s' seems to be some form of '%s', so"
						" defaulting to %s.", value,
						use_spacer ? "true" : "false",
						use_spacer ? "right" : "none");
					if (use_spacer) {
						use_spacer = RIGHT_SPACER;
					} else {
						use_spacer = NO_SPACER;
					}
				}
			} else {
				NORM_ERR("use_spacer should have an argument. Defaulting to right.");
				use_spacer = RIGHT_SPACER;
			}
		}
#ifdef BUILD_X11
#ifdef BUILD_XFT
		CONF("use_xft") {
			use_xft = string_to_bool(value);
		}
		CONF("font") {
			if (value) {
				set_first_font(value);
			}
		}
		CONF("xftalpha") {
			if (value && font_count >= 0) {
				fonts[0].font_alpha = atof(value) * 65535.0;
			}
		}
		CONF("xftfont") {
			if (use_xft) {
#else
		CONF("use_xft") {
			if (string_to_bool(value)) {
				NORM_ERR("Xft not enabled at compile time");
			}
		}
		CONF("xftfont") {
			/* xftfont silently ignored when no Xft */
		}
		CONF("xftalpha") {
			/* xftalpha is silently ignored when no Xft */
		}
		CONF("font") {
#endif
			if (value) {
				set_first_font(value);
			}
#ifdef BUILD_XFT
			}
#endif
		}
		CONF("gap_x") {
			if (value) {
				gap_x = atoi(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("gap_y") {
			if (value) {
				gap_y = atoi(value);
			} else {
				CONF_ERR;
			}
		}
#endif /* BUILD_X11 */
		CONF("mail_spool") {
			if (value) {
				char buffer[256];

				variable_substitute(value, buffer, 256);

				if (buffer[0] != '\0') {
					free_and_zero(current_mail_spool);
					current_mail_spool = strndup(buffer, text_buffer_size);
				}
			} else {
				CONF_ERR;
			}
		}
#ifdef BUILD_X11
		CONF("minimum_size") {
			if (value) {
				if (sscanf(value, "%d %d", &minimum_width, &minimum_height)
						!= 2) {
					if (sscanf(value, "%d", &minimum_width) != 1) {
						CONF_ERR;
					}
				}
			} else {
				CONF_ERR;
			}
		}
		CONF("maximum_width") {
			if (value) {
				if (sscanf(value, "%d", &maximum_width) != 1) {
					CONF_ERR;
				}
			} else {
				CONF_ERR;
			}
		}
#endif /* BUILD_X11 */
		CONF("no_buffers") {
			no_buffers = string_to_bool(value);
		}
		CONF("top_name_width") {
			if (set_top_name_width(value))
				CONF_ERR;
		}
		CONF("top_cpu_separate") {
			cpu_separate = string_to_bool(value);
		}
		CONF("short_units") {
			short_units = string_to_bool(value);
		}
		CONF("format_human_readable") {
			format_human_readable = string_to_bool(value);
		}
#ifdef HDDTEMP
		CONF("hddtemp_host") {
			set_hddtemp_host(value);
		}
		CONF("hddtemp_port") {
			set_hddtemp_port(value);
		}
#endif /* HDDTEMP */
		CONF("pad_percents") {
			pad_percents = atoi(value);
		}
#ifdef BUILD_X11
#ifdef OWN_WINDOW
		CONF("own_window") {
			if (value) {
				own_window = string_to_bool(value);
			}
		}
		CONF("own_window_class") {
			if (value) {
				memset(window.class_name, 0, sizeof(window.class_name));
				strncpy(window.class_name, value,
						sizeof(window.class_name) - 1);
			}
		}
		CONF("own_window_title") {
			if (value) {
				memset(window.title, 0, sizeof(window.title));
				strncpy(window.title, value, sizeof(window.title) - 1);
			}
		}
		CONF("own_window_transparent") {
			if (value) {
				set_transparent = string_to_bool(value);
			}
		}
		CONF("own_window_hints") {
			if (value) {
				char *p_hint, *p_save;
				char delim[] = ", ";

				/* tokenize the value into individual hints */
				if ((p_hint = strtok_r(value, delim, &p_save)) != NULL) {
					do {
						/* fprintf(stderr, "hint [%s] parsed\n", p_hint); */
						if (strncmp(p_hint, "undecorate", 10) == EQUAL) {
							SET_HINT(window.hints, HINT_UNDECORATED);
						} else if (strncmp(p_hint, "below", 5) == EQUAL) {
							SET_HINT(window.hints, HINT_BELOW);
						} else if (strncmp(p_hint, "above", 5) == EQUAL) {
							SET_HINT(window.hints, HINT_ABOVE);
						} else if (strncmp(p_hint, "sticky", 6) == EQUAL) {
							SET_HINT(window.hints, HINT_STICKY);
						} else if (strncmp(p_hint, "skip_taskbar", 12) == EQUAL) {
							SET_HINT(window.hints, HINT_SKIP_TASKBAR);
						} else if (strncmp(p_hint, "skip_pager", 10) == EQUAL) {
							SET_HINT(window.hints, HINT_SKIP_PAGER);
						} else {
							CONF_ERR;
						}

						p_hint = strtok_r(NULL, delim, &p_save);
					} while (p_hint != NULL);
				}
			} else {
				CONF_ERR;
			}
		}
		CONF("own_window_type") {
			if (value) {
				if (strncmp(value, "normal", 6) == EQUAL) {
					window.type = TYPE_NORMAL;
				} else if (strncmp(value, "desktop", 7) == EQUAL) {
					window.type = TYPE_DESKTOP;
				} else if (strncmp(value, "dock", 4) == EQUAL) {
					window.type = TYPE_DOCK;
					text_alignment = TOP_LEFT;
				} else if (strncmp(value, "panel", 5) == EQUAL) {
					window.type = TYPE_PANEL;
				} else if (strncmp(value, "override", 8) == EQUAL) {
					window.type = TYPE_OVERRIDE;
				} else {
					CONF_ERR;
				}
			} else {
				CONF_ERR;
			}
		}
#ifdef BUILD_ARGB
		CONF("own_window_argb_visual") {
			use_argb_visual = string_to_bool(value);
		}
		CONF("own_window_argb_value") {
			own_window_argb_value = strtol(value, 0, 0);
			if (own_window_argb_value > 255 || own_window_argb_value < 0) {
				CONF_ERR2("own_window_argb_value must be <= 255 and >= 0");
			}
		}
#endif /* BUILD_ARGB */
#endif
		CONF("stippled_borders") {
			if (value) {
				stippled_borders = strtol(value, 0, 0);
			} else {
				stippled_borders = 4;
			}
		}
#ifdef BUILD_IMLIB2
		CONF("imlib_cache_size") {
			if (value) {
				cimlib_set_cache_size(atoi(value));
			}
		}
		CONF("imlib_cache_flush_interval") {
			if (value) {
				cimlib_set_cache_flush_interval(atoi(value));
			}
		}
#endif /* BUILD_IMLIB2 */
#endif /* BUILD_X11 */
		CONF("update_interval_on_battery") {
			if (value) {
				update_interval_bat = strtod(value, 0);
			} else {
				CONF_ERR;
			}
		}
		CONF("update_interval") {
			if (value) {
				set_update_interval(strtod(value, 0));
			} else {
				CONF_ERR;
			}
			if (info.music_player_interval == 0) {
				// default to update_interval
				info.music_player_interval = update_interval;
			}
		}
		CONF("total_run_times") {
			if (value) {
				total_run_times = strtod(value, 0);
			} else {
				CONF_ERR;
			}
		}
		CONF("uppercase") {
			stuff_in_uppercase = string_to_bool(value);
		}
		CONF("max_user_text") {
			if (value) {
				max_user_text = atoi(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("text_buffer_size") {
			if (value) {
				text_buffer_size = atoi(value);
				if (text_buffer_size < DEFAULT_TEXT_BUFFER_SIZE) {
					NORM_ERR("text_buffer_size must be >=%i bytes", DEFAULT_TEXT_BUFFER_SIZE);
					text_buffer_size = DEFAULT_TEXT_BUFFER_SIZE;
				}
			} else {
				CONF_ERR;
			}
		}
		CONF("text") {
#ifdef BUILD_X11
			if (output_methods & TO_X) {
				X11_initialisation();
			}
#endif

			free_and_zero(global_text);

			global_text = (char *) malloc(1);
			global_text[0] = '\0';

			while (!feof(fp)) {
				unsigned int l = strlen(global_text);
				unsigned int bl;
				char buf[CONF_BUFF_SIZE];

				if (fgets(buf, CONF_BUFF_SIZE, fp) == NULL) {
					break;
				}

				/* Remove \\-\n. */
				bl = strlen(buf);
				if (bl >= 2 && buf[bl-2] == '\\' && buf[bl-1] == '\n') {
					buf[bl-2] = '\0';
					bl -= 2;
					if (bl == 0) {
						continue;
					}
				}

				/* Check for continuation of \\-\n. */
				if (l > 0 && buf[0] == '\n' && global_text[l-1] == '\\') {
					global_text[l-1] = '\0';
					continue;
				}

				global_text = (char *) realloc(global_text, l + bl + 1);
				strcat(global_text, buf);

				if (strlen(global_text) > max_user_text) {
					break;
				}
			}
			global_text_lines = line + 1;
			break;
		}
#ifdef BUILD_PORT_MONITORS
		CONF("max_port_monitor_connections") {
			int max;
			if (!value || (sscanf(value, "%d", &max) != 1)) {
				/* an error. use default, warn and continue. */
				tcp_portmon_set_max_connections(0);
				CONF_ERR;
			} else if (tcp_portmon_set_max_connections(max)) {
				/* max is < 0, default has been set*/
				CONF_ERR;
			}
		}
#endif
		CONF("if_up_strictness") {
			if (!value) {
				NORM_ERR("incorrect if_up_strictness value, defaulting to 'up'");
				ifup_strictness = IFUP_UP;
			} else if (strcasecmp(value, "up") == EQUAL) {
				ifup_strictness = IFUP_UP;
			} else if (strcasecmp(value, "link") == EQUAL) {
				ifup_strictness = IFUP_LINK;
			} else if (strcasecmp(value, "address") == EQUAL) {
				ifup_strictness = IFUP_ADDR;
			} else {
				NORM_ERR("incorrect if_up_strictness value, defaulting to 'up'");
				ifup_strictness = IFUP_UP;
			}
		}

		CONF("temperature_unit") {
			if (!value) {
				NORM_ERR("config option 'temperature_unit' needs an argument, either 'celsius' or 'fahrenheit'");
			} else if (set_temp_output_unit(value)) {
				NORM_ERR("temperature_unit: incorrect argument");
			}
		}

#ifdef BUILD_LUA
		CONF("lua_load") {
			if (value) {
				char *ptr = strtok(value, " ");
				while (ptr) {
					llua_load(ptr);
					ptr = strtok(NULL, " ");
				}
			} else {
				CONF_ERR;
			}
		}
#ifdef BUILD_X11
		CONF("lua_draw_hook_pre") {
			if (value) {
				llua_set_draw_pre_hook(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("lua_draw_hook_post") {
			if (value) {
				llua_set_draw_post_hook(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("lua_startup_hook") {
			if (value) {
				llua_set_startup_hook(value);
			} else {
				CONF_ERR;
			}
		}
		CONF("lua_shutdown_hook") {
			if (value) {
				llua_set_shutdown_hook(value);
			} else {
				CONF_ERR;
			}
		}
#endif /* BUILD_X11 */
#endif /* BUILD_LUA */

		CONF("color0"){}
		CONF("color1"){}
		CONF("color2"){}
		CONF("color3"){}
		CONF("color4"){}
		CONF("color5"){}
		CONF("color6"){}
		CONF("color7"){}
		CONF("color8"){}
		CONF("color9"){}
		CONF("default_color"){}
		CONF3("default_shade_color", "default_shadecolor"){}
		CONF3("default_outline_color", "default_outlinecolor") {}
		CONF("own_window_colour") {}

		else {
			NORM_ERR("%s: %d: no such configuration: '%s'", f, line, name);
		}
	}

	fclose(fp);

	if (info.music_player_interval == 0) {
		// default to update_interval
		info.music_player_interval = update_interval;
	}
	if (!global_text) { // didn't supply any text
		CRIT_ERR(NULL, NULL, "missing text block in configuration; exiting");
	}
	if (!output_methods) {
		CRIT_ERR(0, 0, "no output_methods have been selected; exiting");
	}
#if defined(BUILD_NCURSES)
#if defined(BUILD_X11)
	if ((output_methods & TO_X) && (output_methods & TO_NCURSES)) {
		NORM_ERR("out_to_x and out_to_ncurses are incompatible, turning out_to_ncurses off");
		output_methods &= ~TO_NCURSES;
		endwin();
	}
#endif /* BUILD_X11 */
	if ((output_methods & (TO_STDOUT | TO_STDERR)) && (output_methods & TO_NCURSES)) {
		NORM_ERR("out_to_ncurses conflicts with out_to_console and out_to_stderr, disabling the later ones");
		output_methods &= ~(TO_STDOUT | TO_STDERR);
	}
#endif /* BUILD_NCURSES */
	return TRUE;
}

#ifdef BUILD_X11
static void load_config_file_x11(const char *f)
{
	int line = 0;
	FILE *fp;

	fp = open_config_file(f);
	if (!fp) {
		return;
	}
	DBGP("reading contents from config file '%s'", f);

	while (!feof(fp)) {
		char buff[CONF_BUFF_SIZE], *name, *value;
		int ret = do_config_step(&line, fp, buff, &name, &value);
		if (ret == CONF_BREAK) {
			break;
		} else if (ret == CONF_CONTINUE) {
			continue;
		}

		CONF2("color0") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					color0 = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF("color1") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					color1 = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF("color2") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					color2 = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF("color3") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					color3 = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF("color4") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					color4 = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF("color5") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					color5 = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF("color6") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					color6 = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF("color7") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					color7 = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF("color8") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					color8 = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF("color9") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					color9 = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF("default_color") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					default_fg_color = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF3("default_shade_color", "default_shadecolor") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					default_bg_color = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
		CONF3("default_outline_color", "default_outlinecolor") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					default_out_color = get_x11_color(value);
				} else {
					CONF_ERR;
				}
			}
		}
#ifdef OWN_WINDOW
		CONF("own_window_colour") {
			X11_initialisation();
			if (x_initialised == YES) {
				if (value) {
					background_colour = get_x11_color(value);
				} else {
					NORM_ERR("Invalid colour for own_window_colour (try omitting the "
						"'#' for hex colours");
				}
			}
		}
#endif
		CONF("text") {
			/* initialize BUILD_X11 if nothing BUILD_X11-related is mentioned before TEXT (and if BUILD_X11 is the default outputmethod) */
			if(output_methods & TO_X) {
				X11_initialisation();
			}
		}
#undef CONF
#undef CONF2
#undef CONF3
#undef CONF_ERR
#undef CONF_ERR2
#undef CONF_BREAK
#undef CONF_CONTINUE
#undef CONF_BUFF_SIZE
	}

	fclose(fp);

}
#endif /* BUILD_X11 */

static void print_help(const char *prog_name) {
	printf("Usage: %s [OPTION]...\n"
			PACKAGE_NAME" is a system monitor that renders text on desktop or to own transparent\n"
			"window. Command line options will override configurations defined in config\n"
			"file.\n"
			"   -v, --version             version\n"
			"   -q, --quiet               quiet mode\n"
			"   -D, --debug               increase debugging output, ie. -DD for more debugging\n"
			"   -c, --config=FILE         config file to load\n"
#ifdef BUILD_BUILTIN_CONFIG
			"   -C, --print-config        print the builtin default config to stdout\n"
			"                             e.g. 'conky -C > ~/.conkyrc' will create a new default config\n"
#endif
			"   -d, --daemonize           daemonize, fork to background\n"
			"   -h, --help                help\n"
#ifdef BUILD_X11
			"   -a, --alignment=ALIGNMENT text alignment on screen, {top,bottom,middle}_{left,right,middle}\n"
			"   -f, --font=FONT           font to use\n"
			"   -X, --display=DISPLAY     X11 display to use\n"
#ifdef OWN_WINDOW
			"   -o, --own-window          create own window to draw\n"
#endif
#ifdef BUILD_XDBE
			"   -b, --double-buffer       double buffer (prevents flickering)\n"
#endif
			"   -w, --window-id=WIN_ID    window id to draw\n"
			"   -x X                      x position\n"
			"   -y Y                      y position\n"
#endif /* BUILD_X11 */
			"   -s, --for-scripts=TEXT    render TEXT on stdout and exit, enclose TEXT by single quotes\n"
			"   -S, --stdin-config        read configuration from stdin\n"
			"   -t, --text=TEXT           text to render, remember single quotes, like -t '$uptime'\n"
			"   -u, --interval=SECS       update interval\n"
			"   -i COUNT                  number of times to update "PACKAGE_NAME" (and quit)\n"
			"   -p, --pause=SECS          pause for SECS seconds at startup before doing anything\n",
			prog_name
	);
}

inline void reset_optind() {
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__) \
		|| defined(__NetBSD__)
	optind = optreset = 1;
#else
	optind = 0;
#endif
}

/* : means that character before that takes an argument */
static const char *getopt_string = "vVqdDSs:t:u:i:hc:p:"
#ifdef BUILD_X11
	"x:y:w:a:f:X:"
#ifdef OWN_WINDOW
	"o"
#endif
#ifdef BUILD_XDBE
	"b"
#endif
#endif /* BUILD_X11 */
#ifdef BUILD_BUILTIN_CONFIG
	"C"
#endif
	;

static const struct option longopts[] = {
	{ "help", 0, NULL, 'h' },
	{ "version", 0, NULL, 'V' },
	{ "quiet", 0, NULL, 'q' },
	{ "debug", 0, NULL, 'D' },
	{ "config", 1, NULL, 'c' },
#ifdef BUILD_BUILTIN_CONFIG
	{ "print-config", 0, NULL, 'C' },
#endif
	{ "daemonize", 0, NULL, 'd' },
#ifdef BUILD_X11
	{ "alignment", 1, NULL, 'a' },
	{ "font", 1, NULL, 'f' },
	{ "display", 1, NULL, 'X' },
#ifdef OWN_WINDOW
	{ "own-window", 0, NULL, 'o' },
#endif
#ifdef BUILD_XDBE
	{ "double-buffer", 0, NULL, 'b' },
#endif
	{ "window-id", 1, NULL, 'w' },
#endif /* BUILD_X11 */
	{ "for-scripts", 1, NULL, 's' },
	{ "stdin-config", 0, NULL, 'S' },
	{ "text", 1, NULL, 't' },
	{ "interval", 1, NULL, 'u' },
	{ "pause", 1, NULL, 'p' },
	{ 0, 0, 0, 0 }
};

void set_current_config() {
	/* set configfile to stdin if that's requested or check if specified config file is valid */
	if(stdinconfig) {
		char mystdin[32];
#define CONKYSTDIN "/proc/%u/fd/0"
		sprintf(mystdin, CONKYSTDIN, getpid());
		current_config = mystdin;
	} else if (not current_config.empty()) {
		struct stat sb;
		if (stat(current_config.c_str(), &sb) ||
				(!S_ISREG(sb.st_mode) && !S_ISLNK(sb.st_mode))) {
			NORM_ERR("invalid configuration file '%s'\n", current_config.c_str());
			current_config.clear();
		}
	}

	/* load current_config, CONFIG_FILE or SYSTEM_CONFIG_FILE */

	if (current_config.empty()) {
		/* load default config file */
		FILE *fp;

		/* Try to use personal config file first */
		std::string buf = to_real_path(CONFIG_FILE);
		if (!buf.empty() && (fp = fopen(buf.c_str(), "r"))) {
			current_config = buf;
			fclose(fp);
		}

		/* Try to use system config file if personal config not readable */
		if (current_config.empty() && (fp = fopen(SYSTEM_CONFIG_FILE, "r"))) {
			current_config = SYSTEM_CONFIG_FILE;
			fclose(fp);
		}

		/* No readable config found */
		if (current_config.empty()) {
#define NOCFGFILEFOUND "no readable personal or system-wide config file found"
#ifdef BUILD_BUILTIN_CONFIG
			current_config = "==builtin==";
			NORM_ERR(NOCFGFILEFOUND
					", using builtin default");
#else
			CRIT_ERR(NULL, NULL, NOCFGFILEFOUND);
#endif /* ! CONF_OUTPUT */
		}
	}
}

void initialisation(int argc, char **argv) {
	struct sigaction act, oact;
	bool for_scripts = false;

	set_default_configurations();

	reset_optind();
	while (1) {
		int c = getopt_long(argc, argv, getopt_string, longopts, NULL);

		if (c == -1) {
			break;
		}else if (c == 's') {
			free_and_zero(global_text);
			global_text = strndup(optarg, max_user_text);
			convert_escapes(global_text);
			total_run_times = 1;
			output_methods = TO_STDOUT;
			for_scripts = true;
		}
	}
	if(for_scripts == false) {
		set_current_config();
		load_config_file(current_config.c_str());
		currentconffile = conftree_add(currentconffile, current_config.c_str());
	}

#ifdef MAIL_FILE
	if (current_mail_spool == NULL) {
		char buf[256];

		variable_substitute(MAIL_FILE, buf, 256);

		if (buf[0] != '\0') {
			current_mail_spool = strndup(buf, text_buffer_size);
		}
	}
#endif

	/* handle other command line arguments */

	reset_optind();

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	if ((kd = kvm_open("/dev/null", "/dev/null", "/dev/null", O_RDONLY,
			"kvm_open")) == NULL) {
		CRIT_ERR(NULL, NULL, "cannot read kvm");
	}
#endif

	while (1) {
		int c = getopt_long(argc, argv, getopt_string, longopts, NULL);
		int startup_pause;

		if (c == -1) {
			break;
		}

		switch (c) {
			case 'd':
				fork_to_background = 1;
				break;
#ifdef BUILD_X11
			case 'f':
				set_first_font(optarg);
				break;
			case 'a':
				setalignment(&text_alignment, optarg, NULL, 0, false);
				break;

#ifdef OWN_WINDOW
			case 'o':
				own_window = 1;
				break;
#endif
#ifdef BUILD_XDBE
			case 'b':
				use_xdbe = 1;
				break;
#endif
#endif /* BUILD_X11 */
			case 't':
				free_and_zero(global_text);
				global_text = strndup(optarg, max_user_text);
				convert_escapes(global_text);
				break;

			case 'u':
				update_interval = strtod(optarg, 0);
				update_interval_old = update_interval;
				if (info.music_player_interval == 0) {
					// default to update_interval
					info.music_player_interval = update_interval;
				}
				break;

			case 'i':
				total_run_times = strtod(optarg, 0);
				break;
#ifdef BUILD_X11
			case 'x':
				gap_x = atoi(optarg);
				break;

			case 'y':
				gap_y = atoi(optarg);
				break;
#endif /* BUILD_X11 */
			case 'p':
				if (first_pass) {
					startup_pause = atoi(optarg);
					sleep(startup_pause);
				}
				break;

			case '?':
				exit(EXIT_FAILURE);
		}
	}

#ifdef BUILD_X11
	/* load font */
	if (output_methods & TO_X) {
		load_config_file_x11(current_config.c_str());
	}
#endif /* BUILD_X11 */

	/* generate text and get initial size */
	extract_variable_text(global_text);
	free_and_zero(global_text);
	/* fork */
	if (fork_to_background && first_pass) {
		int pid = fork();

		switch (pid) {
			case -1:
				NORM_ERR(PACKAGE_NAME": couldn't fork() to background: %s",
					strerror(errno));
				break;

			case 0:
				/* child process */
				usleep(25000);
				fprintf(stderr, "\n");
				fflush(stderr);
				break;

			default:
				/* parent process */
				fprintf(stderr, PACKAGE_NAME": forked to background, pid is %d\n",
					pid);
				fflush(stderr);
				exit(EXIT_SUCCESS);
		}
	}

	start_update_threading();

	text_buffer = (char*)malloc(max_user_text);
	memset(text_buffer, 0, max_user_text);
	tmpstring1 = (char*)malloc(text_buffer_size);
	memset(tmpstring1, 0, text_buffer_size);
	tmpstring2 = (char*)malloc(text_buffer_size);
	memset(tmpstring2, 0, text_buffer_size);

#ifdef BUILD_X11
	X11_create_window();
#endif /* BUILD_X11 */
#ifdef BUILD_LUA
	llua_setup_info(&info, update_interval);
#endif /* BUILD_LUA */
#ifdef BUILD_WEATHER_XOAP
	xmlInitParser();
#endif /* BUILD_WEATHER_XOAP */

	/* Set signal handlers */
	act.sa_handler = signal_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;
#endif

	if (		sigaction(SIGINT,  &act, &oact) < 0
			||	sigaction(SIGALRM, &act, &oact) < 0
			||	sigaction(SIGUSR1, &act, &oact) < 0
			||	sigaction(SIGHUP,  &act, &oact) < 0
			||	sigaction(SIGTERM, &act, &oact) < 0) {
		NORM_ERR("error setting signal handler: %s", strerror(errno));
	}

#ifdef BUILD_LUA
	llua_startup_hook();
#endif /* BUILD_LUA */
}

#ifdef BUILD_X11
bool isutf8(const char* envvar) {
	char *s = getenv(envvar);
	if(s) {
		std::string temp = s;
		std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
		if( (temp.find("utf-8") != std::string::npos) || (temp.find("utf8") != std::string::npos) ) {
			return true;
		}
	}
	return false;
}
#endif

int main(int argc, char **argv)
{
	argc_copy = argc;
	argv_copy = argv;
	g_signal_pending = 0;
	max_user_text = MAX_USER_TEXT_DEFAULT;
	memset(&info, 0, sizeof(info));
	free_templates();
	clear_net_stats();

#ifdef BUILD_PORT_MONITORS
	/* set default connection limit */
	tcp_portmon_set_max_connections(0);
#endif

	/* handle command line parameters that don't change configs */
#ifdef BUILD_X11
	if(isutf8("LC_ALL") || isutf8("LC_CTYPE") || isutf8("LANG")) {
		utf8_mode = true;
	}
	if (!setlocale(LC_CTYPE, "")) {
		NORM_ERR("Can't set the specified locale!\nCheck LANG, LC_CTYPE, LC_ALL.");
	}
#endif /* BUILD_X11 */
	while (1) {
		int c = getopt_long(argc, argv, getopt_string, longopts, NULL);

		if (c == -1) {
			break;
		}

		switch (c) {
			case 'D':
				global_debug_level++;
				break;
			case 'v':
			case 'V':
				print_version(); /* doesn't return */
			case 'c':
				current_config = optarg;
				break;
			case 'S':
				stdinconfig = true;
				break;
			case 'q':
				if (!freopen("/dev/null", "w", stderr))
					CRIT_ERR(0, 0, "could not open /dev/null as stderr!");
				break;
			case 'h':
				print_help(argv[0]);
				return 0;
#ifdef BUILD_BUILTIN_CONFIG
			case 'C':
				print_defconfig();
				return 0;
#endif
#ifdef BUILD_X11
			case 'w':
				window.window = strtol(optarg, 0, 0);
				break;
			case 'X':
				free_and_zero(disp);
				disp = strdup(optarg);
				break;
#endif /* BUILD_X11 */

			case '?':
				exit(EXIT_FAILURE);
		}
	}

	set_current_config();

#ifdef BUILD_WEATHER_XOAP
	/* Load xoap keys, if existing */
	load_xoap_keys();
#endif /* BUILD_WEATHER_XOAP */

#ifdef HAVE_SYS_INOTIFY_H
	inotify_fd = inotify_init();
	if(inotify_fd != -1) {
		int fl;

		fl = fcntl(inotify_fd, F_GETFL);
		fcntl(inotify_fd, F_SETFL, fl | O_NONBLOCK);
	}
#endif /* HAVE_SYS_INOTIFY_H */

	initialisation(argc, argv);

	first_pass = 0; /* don't ever call fork() again */

	main_loop();

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	kvm_close(kd);
#endif

#ifdef LEAKFREE_NCURSES
	_nc_free_and_exit(0);	//hide false memleaks
#endif
	return 0;

}

void alarm_handler(void) {
	if(childpid > 0) {
		kill(childpid, SIGTERM);
	}
}

static void signal_handler(int sig)
{
	/* signal handler is light as a feather, as it should be.
	 * we will poll g_signal_pending with each loop of conky
	 * and do any signal processing there, NOT here (except
	 * SIGALRM because this is caused when conky is hanging) */
	if(sig == SIGALRM) {
		alarm_handler();
	} else {
		g_signal_pending = sig;
	}
}
