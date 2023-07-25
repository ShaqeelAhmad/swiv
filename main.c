/*
 * Copyright 2023 Shaqeel Ahmad
 * Copyright 2011-2013 Bert Muennich
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
#define _MAPPINGS_CONFIG
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

typedef struct {
	struct timeval when;
	bool active;
	timeout_f handler;
} timeout_t;

/* timeout handler functions: */
void animate(void);
void slideshow(void);

appmode_t mode;
arl_t arl;
img_t img;
tns_t tns;
win_t win;

fileinfo_t *files;
int filecnt, fileidx;
int alternate;
int markcnt;
int markidx;

struct {
	int fd;
	xkb_keysym_t keysym;
	unsigned int sh;
} repeat_key;

int prefix;
bool extprefix;

typedef struct {
	int err;
	char *cmd;
} extcmd_t;

struct {
	extcmd_t f;
	int fd;
	unsigned int i, lastsep;
	pid_t pid;
} info;

struct {
	extcmd_t f;
	bool warned;
} keyhandler;

timeout_t timeouts[] = {
	{ { 0, 0 }, false, animate      },
	{ { 0, 0 }, false, slideshow    },
};

cursor_t imgcursor[3] = {
	CURSOR_ARROW, CURSOR_ARROW, CURSOR_ARROW
};

void cleanup(void)
{
	img_close(&img, false);
	arl_cleanup(&arl);
	tns_free(&tns);
	win_close(&win);
}

void check_add_file(char *filename, bool given)
{
	char *path;

	if (*filename == '\0')
		return;

	if (access(filename, R_OK) < 0 ||
	    (path = realpath(filename, NULL)) == NULL)
	{
		if (given)
			error(0, errno, "%s", filename);
		return;
	}

	if (fileidx == filecnt) {
		filecnt *= 2;
		files = erealloc(files, filecnt * sizeof(*files));
		memset(&files[filecnt/2], 0, filecnt/2 * sizeof(*files));
	}

	files[fileidx].name = estrdup(filename);
	files[fileidx].path = path;
	if (given)
		files[fileidx].flags |= FF_WARN;
	fileidx++;
}

void remove_file(int n, bool manual)
{
	if (n < 0 || n >= filecnt)
		return;

	if (filecnt == 1) {
		if (!manual)
			fprintf(stderr, "swiv: no more files to display, aborting\n");
		exit(manual ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	if (files[n].flags & FF_MARK)
		markcnt--;

	if (files[n].path != files[n].name)
		free((void*) files[n].path);
	free((void*) files[n].name);

	if (n + 1 < filecnt) {
		if (tns.thumbs != NULL) {
			memmove(tns.thumbs + n, tns.thumbs + n + 1, (filecnt - n - 1) *
			        sizeof(*tns.thumbs));
			memset(tns.thumbs + filecnt - 1, 0, sizeof(*tns.thumbs));
		}
		memmove(files + n, files + n + 1, (filecnt - n - 1) * sizeof(*files));
	}
	filecnt--;
	if (fileidx > n || fileidx == filecnt)
		fileidx--;
	if (alternate > n || alternate == filecnt)
		alternate--;
	if (markidx > n || markidx == filecnt)
		markidx--;
}

void set_timeout(timeout_f handler, int time, bool overwrite)
{
	int i;

	for (i = 0; i < ARRLEN(timeouts); i++) {
		if (timeouts[i].handler == handler) {
			if (!timeouts[i].active || overwrite) {
				gettimeofday(&timeouts[i].when, 0);
				TV_ADD_MSEC(&timeouts[i].when, time);
				timeouts[i].active = true;
			}
			return;
		}
	}
}

void reset_timeout(timeout_f handler)
{
	int i;

	for (i = 0; i < ARRLEN(timeouts); i++) {
		if (timeouts[i].handler == handler) {
			timeouts[i].active = false;
			return;
		}
	}
}

bool check_timeouts(struct timeval *t)
{
	int i = 0, tdiff, tmin = -1;
	struct timeval now;

	while (i < ARRLEN(timeouts)) {
		if (timeouts[i].active) {
			gettimeofday(&now, 0);
			tdiff = TV_DIFF(&timeouts[i].when, &now);
			if (tdiff <= 0) {
				timeouts[i].active = false;
				if (timeouts[i].handler != NULL)
					timeouts[i].handler();
				i = tmin = -1;
			} else if (tmin < 0 || tdiff < tmin) {
				tmin = tdiff;
			}
		}
		i++;
	}
	if (tmin > 0 && t != NULL)
		TV_SET_MSEC(t, tmin);
	return tmin > 0;
}

void close_info(void)
{
	if (info.fd != -1) {
		kill(info.pid, SIGTERM);
		close(info.fd);
		info.fd = -1;
	}
}

void open_info(void)
{
	int pfd[2];
	char w[12], h[12];

	if (info.f.err != 0 || info.fd >= 0 || win.bar.h == 0)
		return;
	win.bar.l.buf[0] = '\0';
	if (pipe(pfd) < 0)
		return;
	if ((info.pid = fork()) == 0) {
		close(pfd[0]);
		dup2(pfd[1], 1);
		snprintf(w, sizeof(w), "%d", img.w);
		snprintf(h, sizeof(h), "%d", img.h);
		execl(info.f.cmd, info.f.cmd, files[fileidx].name, w, h, NULL);
		error(EXIT_FAILURE, errno, "exec: %s", info.f.cmd);
	}
	close(pfd[1]);
	if (info.pid < 0) {
		close(pfd[0]);
	} else {
		fcntl(pfd[0], F_SETFL, O_NONBLOCK);
		info.fd = pfd[0];
		info.i = info.lastsep = 0;
	}
}

void read_info(void)
{
	ssize_t i, n;
	char buf[BAR_L_LEN];

	while (true) {
		n = read(info.fd, buf, sizeof(buf));
		if (n < 0 && errno == EAGAIN)
			return;
		else if (n == 0)
			goto end;
		for (i = 0; i < n; i++) {
			if (buf[i] == '\n') {
				if (info.lastsep == 0) {
					win.bar.l.buf[info.i++] = ' ';
					info.lastsep = 1;
				}
			} else {
				win.bar.l.buf[info.i++] = buf[i];
				info.lastsep = 0;
			}
			if (info.i + 1 == win.bar.l.size)
				goto end;
		}
	}
end:
	info.i -= info.lastsep;
	win.bar.l.buf[info.i] = '\0';
	win.redraw = true;
	close_info();
}

void load_image(int new)
{
	bool prev = new < fileidx;
	static int current;

	if (new < 0 || new >= filecnt)
		return;

	reset_timeout(slideshow);

	if (new != current)
		alternate = current;

	img_close(&img, false);
	while (!img_load(&img, &files[new])) {
		remove_file(new, false);
		if (new >= filecnt)
			new = filecnt - 1;
		else if (new > 0 && prev)
			new--;
	}
	files[new].flags &= ~FF_WARN;
	fileidx = current = new;

	close_info();
	open_info();
	arl_setup(&arl, files[fileidx].path);

	if (img.multi.cnt > 0 && img.multi.animate)
		set_timeout(animate, img.multi.frames[img.multi.sel].delay, true);
	else
		reset_timeout(animate);
}

bool mark_image(int n, bool on)
{
	markidx = n;
	if (!!(files[n].flags & FF_MARK) != on) {
		files[n].flags ^= FF_MARK;
		markcnt += on ? 1 : -1;
		if (mode == MODE_THUMB)
			tns_mark(&tns, n, on);
		return true;
	}
	return false;
}

void bar_put(win_bar_t *bar, const char *fmt, ...)
{
	size_t len = bar->size - (bar->p - bar->buf), n;
	va_list ap;

	va_start(ap, fmt);
	n = vsnprintf(bar->p, len, fmt, ap);
	bar->p += MIN(len, n);
	va_end(ap);
}

#define BAR_SEP "  "

void update_info(void)
{
	unsigned int i, fn, fw;
	const char * mark;
	win_bar_t *l = &win.bar.l, *r = &win.bar.r;

	/* update bar contents */
	if (win.bar.h == 0)
		return;
	for (fw = 0, i = filecnt; i > 0; fw++, i /= 10);
	mark = files[fileidx].flags & FF_MARK ? "* " : "";
	l->p = l->buf;
	r->p = r->buf;
	if (mode == MODE_THUMB) {
		if (tns.loadnext < tns.end)
			bar_put(l, "Loading... %0*d", fw, tns.loadnext + 1);
		else if (tns.initnext < filecnt)
			bar_put(l, "Caching... %0*d", fw, tns.initnext + 1);
		else
			strncpy(l->buf, files[fileidx].name, l->size);
		bar_put(r, "%s%0*d/%d", mark, fw, fileidx + 1, filecnt);
	} else {
		bar_put(r, "%s", mark);
		if (img.ss.on) {
			if (img.ss.delay % 10 != 0)
				bar_put(r, "%2.1fs" BAR_SEP, (float)img.ss.delay / 10);
			else
				bar_put(r, "%ds" BAR_SEP, img.ss.delay / 10);
		}
		if (img.gamma != 0)
			bar_put(r, "G%+d" BAR_SEP, img.gamma);
		bar_put(r, "%3d%%" BAR_SEP, (int) (img.zoom * 100.0));
		if (img.multi.cnt > 0) {
			for (fn = 0, i = img.multi.cnt; i > 0; fn++, i /= 10);
			bar_put(r, "%0*d/%d" BAR_SEP, fn, img.multi.sel + 1, img.multi.cnt);
		}
		bar_put(r, "%0*d/%d", fw, fileidx + 1, filecnt);
		if (info.f.err)
			strncpy(l->buf, files[fileidx].name, l->size);
	}
}

int ptr_third_x(void)
{
	int x, y;

	win_cursor_pos(&win, &x, &y);
	return MAX(0, MIN(2, (x / (win.width * 0.33))));
}

void reset_cursor(void)
{
	int c;
	cursor_t cursor = CURSOR_NONE;

	if (mode == MODE_IMAGE) {
		c = ptr_third_x();
		c = MAX(fileidx > 0 ? 0 : 1, c);
		c = MIN(fileidx + 1 < filecnt ? 2 : 1, c);
		cursor = imgcursor[c];
	} else {
		if (tns.loadnext < tns.end || tns.initnext < filecnt)
			cursor = CURSOR_WATCH;
		else
			cursor = CURSOR_ARROW;
	}
	win_set_cursor(&win, cursor);
}

void redraw(void)
{
	int t;

	if (mode == MODE_IMAGE) {
		img_render(&img);
		if (img.ss.on) {
			t = img.ss.delay * 100;
			if (img.multi.cnt > 0 && img.multi.animate)
				t = MAX(t, img.multi.length);
			set_timeout(slideshow, t, false);
		}
	} else {
		tns_render(&tns);
	}
	reset_cursor();
	update_info();
	win_draw(&win);
}

void animate(void)
{
	if (img_frame_animate(&img)) {
		win.redraw = true;
		set_timeout(animate, img.multi.frames[img.multi.sel].delay, true);
	}
}

void slideshow(void)
{
	load_image(fileidx + 1 < filecnt ? fileidx + 1 : 0);
	win.redraw = true;
}

void run_key_handler(const char *key, uint32_t mask)
{
	pid_t pid;
	FILE *pfs;
	bool marked = mode == MODE_THUMB && markcnt > 0;
	bool changed = false;
	int f, i, pfd[2];
	int fcnt = marked ? markcnt : 1;
	char kstr[32];
	struct stat *oldst, st;

	if (keyhandler.f.err != 0) {
		if (!keyhandler.warned) {
			error(0, keyhandler.f.err, "%s", keyhandler.f.cmd);
			keyhandler.warned = true;
		}
		return;
	}
	if (key == NULL)
		return;

	if (pipe(pfd) < 0) {
		error(0, errno, "pipe");
		return;
	}
	if ((pfs = fdopen(pfd[1], "w")) == NULL) {
		error(0, errno, "open pipe");
		close(pfd[0]), close(pfd[1]);
		return;
	}
	oldst = emalloc(fcnt * sizeof(*oldst));

	close_info();
	strncpy(win.bar.l.buf, "Running key handler...", win.bar.l.size);

	win_draw(&win);
	win_set_cursor(&win, CURSOR_WATCH);

	// We're redrawing the bar manually here
	wl_surface_attach(win.surface, win.buffer.wl_buf, 0, 0);
	wl_surface_damage_buffer(win.surface, 0, win.height, win.width, win.bar.h);
	wl_surface_commit(win.surface);
	wl_display_dispatch(win.display);
	wl_display_flush(win.display);

	snprintf(kstr, sizeof(kstr), "%s%s%s%s",
	         mask & ControlMask ? "C-" : "",
	         mask & Mod1Mask    ? "M-" : "",
	         mask & ShiftMask   ? "S-" : "", key);

	if ((pid = fork()) == 0) {
		close(pfd[1]);
		dup2(pfd[0], 0);
		execl(keyhandler.f.cmd, keyhandler.f.cmd, kstr, NULL);
		error(EXIT_FAILURE, errno, "exec: %s", keyhandler.f.cmd);
	}
	close(pfd[0]);
	if (pid < 0) {
		error(0, errno, "fork");
		fclose(pfs);
		goto end;
	}

	for (f = i = 0; f < fcnt; i++) {
		if ((marked && (files[i].flags & FF_MARK)) || (!marked && i == fileidx)) {
			stat(files[i].path, &oldst[f]);
			fprintf(pfs, "%s\n", files[i].name);
			f++;
		}
	}
	fclose(pfs);
	while (waitpid(pid, NULL, 0) == -1 && errno == EINTR);

	for (f = i = 0; f < fcnt; i++) {
		if ((marked && (files[i].flags & FF_MARK)) || (!marked && i == fileidx)) {
			if (stat(files[i].path, &st) != 0 ||
				  memcmp(&oldst[f].st_mtime, &st.st_mtime, sizeof(st.st_mtime)) != 0)
			{
				if (tns.thumbs != NULL) {
					tns_unload(&tns, i);
					tns.loadnext = MIN(tns.loadnext, i);
				}
				changed = true;
			}
			f++;
		}
	}

end:
	if (mode == MODE_IMAGE) {
		if (changed) {
			img_close(&img, true);
			load_image(fileidx);
		} else {
			open_info();
		}
	}
	free(oldst);
	reset_cursor();
	win.redraw = true;
}

#define MODMASK(mask) ((mask) & (ShiftMask|ControlMask|Mod1Mask))

void keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	win_t *win = data;
	int i;
	bool keysym_has_func = false;
	unsigned int sh = 0;

	const struct itimerspec zero_value = {0};
	// Stop keyboard repeat
	if (repeat_key.fd != -1 &&
			timerfd_settime(repeat_key.fd, 0, &zero_value, NULL) < 0)
		error(EXIT_FAILURE, errno, "timerfd_settime: stopping key repeat");

	if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
		return;

	// change from linux evdev scancode to xkb scancode
	key += 8;

	xkb_keysym_t keysym = xkb_state_key_get_one_sym(win->xkb_state, key);

	if (win->mods_depressed & ShiftMask) {
		xkb_state_update_mask(win->xkb_state, win->mods_depressed & (~ShiftMask),
				win->mods_latched, win->mods_locked, 0, 0, win->group);
		xkb_keysym_t shkeysym = xkb_state_key_get_one_sym(win->xkb_state, key);
		xkb_state_update_mask(win->xkb_state, win->mods_depressed, win->mods_latched,
				win->mods_locked, 0, 0, win->group);
		if (keysym != shkeysym)
			sh = ShiftMask;
	}

#define IsModifierKey(keysym) \
  ((((xkb_keysym_t)(keysym) >= XKB_KEY_Shift_L) && \
	((xkb_keysym_t)(keysym) <= XKB_KEY_Hyper_R)) \
   || (((xkb_keysym_t)(keysym) >= XKB_KEY_ISO_Lock) && \
       ((xkb_keysym_t)(keysym) <= XKB_KEY_ISO_Level5_Lock)) \
   || ((xkb_keysym_t)(keysym) == XKB_KEY_Mode_switch) \
   || ((xkb_keysym_t)(keysym) == XKB_KEY_Num_Lock))

	if (IsModifierKey(keysym))
		return;

	if (keysym == XKB_KEY_Escape && MODMASK(win->mods_depressed) == 0) {
		extprefix = false;
		return;
	} else if (extprefix) {
		char buf[64] = {0};
		int n = xkb_keysym_get_name(keysym, buf, sizeof(buf));
		run_key_handler(n > 0 ? buf : NULL, win->mods_depressed & ~sh);

		extprefix = false;
		return;
	} else if (keysym >= XKB_KEY_0 && keysym <= XKB_KEY_9) {
		/* number prefix for commands */
		prefix = prefix * 10 + (int) (keysym - XKB_KEY_0);
		return;
	} else for (i = 0; i < ARRLEN(keys); i++) {
		if (keys[i].keysym == keysym &&
		    MODMASK(keys[i].mask | sh) == MODMASK(win->mods_depressed) &&
		    keys[i].cmd >= 0 && keys[i].cmd < CMD_COUNT &&
		    (cmds[keys[i].cmd].mode < 0 || cmds[keys[i].cmd].mode == mode))
		{
			keysym_has_func = true;

			if (cmds[keys[i].cmd].func(keys[i].arg))
				win->redraw = true;
		}
	}

	if (keysym_has_func && repeat_key.fd != -1 &&
			xkb_keymap_key_repeats(win->xkb_keymap, key) &&
			win->repeat_rate != 0) {
		repeat_key.keysym = keysym;
		repeat_key.sh = sh;
		struct itimerspec t = {
			.it_value = {
				.tv_sec = 0,
				.tv_nsec = win->repeat_delay * 1000000,
			},
			.it_interval = {
				.tv_sec = 0,
				.tv_nsec = 1000000000 / win->repeat_rate,
			},
		};

		if (t.it_value.tv_nsec >= 1000000000) {
			t.it_value.tv_sec += t.it_value.tv_nsec / 1000000000;
			t.it_value.tv_nsec %= 1000000000;
		}
		if (t.it_interval.tv_nsec >= 1000000000) {
			t.it_interval.tv_sec += t.it_interval.tv_nsec / 1000000000;
			t.it_interval.tv_nsec %= 1000000000;
		}

		if (timerfd_settime(repeat_key.fd, 0, &t, NULL) < 0) {
			error(EXIT_FAILURE, errno, "timerfd_settime: starting key repeat");
		}
	}

	prefix = 0;
}

void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	win_t *win = data;
	int i, sel;
	static uint32_t firstclick;
	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		win->pointer.prevsel = -1;
		return;
	}
	if (mode == MODE_IMAGE) {
		reset_cursor();

		for (i = 0; i < ARRLEN(buttons); i++) {
			if (buttons[i].button == button &&
			    MODMASK(buttons[i].mask) == MODMASK(win->mods_depressed) &&
			    buttons[i].cmd >= 0 && buttons[i].cmd < CMD_COUNT &&
			    (cmds[buttons[i].cmd].mode < 0 || cmds[buttons[i].cmd].mode == mode))
			{
				if (cmds[buttons[i].cmd].func(buttons[i].arg))
					win->redraw = true;
			}
		}
	} else {
		switch (button) {
		case BTN_LEFT:
			if ((sel = tns_translate(&tns, win->pointer.x, win->pointer.y)) >= 0) {
				if (sel != fileidx) {
					tns_highlight(&tns, fileidx, false);
					tns_highlight(&tns, sel, true);
					fileidx = sel;
					firstclick = time;
					win->redraw = true;
				} else if (time - firstclick <= TO_DOUBLE_CLICK) {
					mode = MODE_IMAGE;
					load_image(fileidx);
					win->redraw = true;
				} else {
					firstclick = time;
				}
			}
			break;
		case BTN_RIGHT:
			if ((sel = tns_translate(&tns, win->pointer.x, win->pointer.y)) >= 0) {
				bool on = !(files[sel].flags & FF_MARK);

				if (sel >= 0 && mark_image(sel, on))
					win->redraw = true;

				win->pointer.prevsel = sel;
			}
			break;
		}
	}
}

void pointer_handle_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	win_t *win = data;
	win->pointer.x = wl_fixed_to_int(surface_x);
	win->pointer.y = wl_fixed_to_int(surface_y);
	reset_cursor();

	int sel;
	if (win->pointer.prevsel > 0 &&
			(sel = tns_translate(&tns, win->pointer.x, win->pointer.y)) >= 0 &&
			sel != win->pointer.prevsel) {
		win->pointer.prevsel = sel;
		bool on = !(files[sel].flags & FF_MARK);
		if (mark_image(sel, on))
			win->redraw = true;
	}
}

void pointer_handle_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
		uint32_t axis, wl_fixed_t value)
{
	win_t *win = data;
	static int accum_value = 0;
	int i, dir;
	accum_value += wl_fixed_to_int(value);

	// arbitrary value for avoiding calling functions on every small scroll
	if (-4 < accum_value && accum_value < 4)
		return;

	if (mode == MODE_THUMB && axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
		if (tns_scroll(&tns, accum_value < 0 ? DIR_UP : DIR_DOWN,
					(win->mods_depressed & ControlMask) != 0))
			win->redraw = true;
	} else if (mode == MODE_IMAGE) {
		dir = accum_value < 0 ? -1 : 1;

		for (i = 0; i < ARRLEN(scrolls); i++) {
			if (scrolls[i].axis == axis &&
				scrolls[i].dir == dir &&
			    MODMASK(scrolls[i].mask) == MODMASK(win->mods_depressed) &&
			    scrolls[i].cmd >= 0 && scrolls[i].cmd < CMD_COUNT &&
			    (cmds[scrolls[i].cmd].mode < 0 || cmds[scrolls[i].cmd].mode == mode))
			{
				if (cmds[scrolls[i].cmd].func(scrolls[i].arg))
					win->redraw = true;
			}
		}
	}
	accum_value = 0;
}

static void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time);

static struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done,
};

static void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
	win_t *win = data;
	wl_callback_destroy(cb);

	cb = wl_surface_frame(win->surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, data);

	if (win->resized) {
		if (mode == MODE_IMAGE) {
			img.dirty = true;
			img.checkpan = true;
		} else {
			tns.dirty = true;
		}

		win_recreate_buffer(win);

		win->resized = false;
		win->redraw = true;
	} else if (win->redraw) {
		redraw();
		win->redraw = false;
	} else {
		wl_surface_commit(win->surface);
		return;
	}
	wl_surface_attach(win->surface, win->buffer.wl_buf, 0, 0);
	wl_surface_damage_buffer(win->surface, 0, 0, win->width,
			win->height + win->bar.h);
	wl_surface_commit(win->surface);
}

const struct timespec ten_ms = {0, 10000000};

void run(void)
{
	bool init_thumb, load_thumb, to_set;
	struct timeval timeout;
	fd_set fds;
	int nfds, wl_fd;

	struct wl_callback *cb = wl_surface_frame(win.surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, &win);

	wl_fd = wl_display_get_fd(win.display);
	int ret = 1;
	while (ret > 0) {
		ret = wl_display_dispatch_pending(win.display);
		wl_display_flush(win.display);
	}
	if (ret < 0)
		error(EXIT_FAILURE, errno, " wl_display_dispatch_pending");

	while (!win.quit) {
		init_thumb = mode == MODE_THUMB && tns.initnext < filecnt;
		load_thumb = mode == MODE_THUMB && tns.loadnext < tns.end;
		if (load_thumb) {
			if (!tns_load(&tns, tns.loadnext, false, false)) {
				remove_file(tns.loadnext, false);
				tns.dirty = true;
			}
			win.redraw = true;
		} else if (init_thumb) {
			if (!tns_load(&tns, tns.initnext, false, true))
				remove_file(tns.initnext, false);
		}

		to_set = check_timeouts(&timeout);
		FD_ZERO(&fds);
		FD_SET(wl_fd, &fds);
		nfds = wl_fd;

		if (repeat_key.fd != -1) {
			FD_SET(repeat_key.fd, &fds);
			nfds = MAX(nfds, repeat_key.fd);
		}
		if (info.fd != -1) {
			FD_SET(info.fd, &fds);
			nfds = MAX(nfds, info.fd);
		}
		if (arl.fd != -1) {
			FD_SET(arl.fd, &fds);
			nfds = MAX(nfds, arl.fd);
		}

		select(nfds + 1, &fds, 0, 0, to_set ? &timeout : NULL);

		if (repeat_key.fd != -1 && FD_ISSET(repeat_key.fd, &fds)) {
			uint64_t expiration_count;
			ssize_t ret = read(repeat_key.fd, &expiration_count, sizeof(expiration_count));
			if (ret < 0) {
				if (errno != EAGAIN)
					error(0, errno, "key repeat error");
			} else {
				int i;
				unsigned int sh = repeat_key.sh;
				xkb_keysym_t keysym = repeat_key.keysym;
				for (i = 0; i < ARRLEN(keys); i++) {
					if (keys[i].keysym == keysym &&
						MODMASK(keys[i].mask | sh) == MODMASK(win.mods_depressed) &&
						keys[i].cmd >= 0 && keys[i].cmd < CMD_COUNT &&
						(cmds[keys[i].cmd].mode < 0 || cmds[keys[i].cmd].mode == mode))
					{
						if (cmds[keys[i].cmd].func(keys[i].arg))
							win.redraw = true;
					}
				}
			}
		}

		if (info.fd != -1 && FD_ISSET(info.fd, &fds))
			read_info();

		if (arl.fd != -1 && FD_ISSET(arl.fd, &fds)) {
			if (arl_handle(&arl)) {
				/* when too fast, imlib2 can't load the image */
				nanosleep(&ten_ms, NULL);
				img_close(&img, true);
				load_image(fileidx);
				win.redraw = true;
			}
		}
		if (FD_ISSET(wl_fd, &fds) && wl_display_dispatch(win.display) == -1)
			error(EXIT_FAILURE, errno, "wl_display_dispatch");

		if (wl_display_flush(win.display) == -1 )
			error(EXIT_FAILURE, errno, "wl_display_flush");
	}
}

int fncmp(const void *a, const void *b)
{
	return strcoll(((fileinfo_t*) a)->name, ((fileinfo_t*) b)->name);
}

void sigchld(int sig)
{
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

void setup_signal(int sig, void (*handler)(int sig))
{
	struct sigaction sa;

	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	if (sigaction(sig, &sa, 0) == -1)
		error(EXIT_FAILURE, errno, "signal %d", sig);
}

int main(int argc, char **argv)
{
	int i, start;
	size_t n;
	ssize_t len;
	char *filename;
	const char *homedir, *dsuffix = "";
	struct stat fstats;
	r_dir_t dir;

	setup_signal(SIGCHLD, sigchld);
	setup_signal(SIGPIPE, SIG_IGN);

	setlocale(LC_COLLATE, "");

	parse_options(argc, argv);

	if (options->clean_cache) {
		tns_init(&tns, NULL, NULL, NULL, NULL);
		tns_clean_cache(&tns);
		exit(EXIT_SUCCESS);
	}

	if (options->filecnt == 0 && !options->from_stdin) {
		print_usage();
		exit(EXIT_FAILURE);
	}

	if (options->recursive || options->from_stdin)
		filecnt = 1024;
	else
		filecnt = options->filecnt;

	files = emalloc(filecnt * sizeof(*files));
	memset(files, 0, filecnt * sizeof(*files));
	fileidx = 0;

	if (options->from_stdin) {
		n = 0;
		filename = NULL;
		while ((len = getline(&filename, &n, stdin)) > 0) {
			if (filename[len-1] == '\n')
				filename[len-1] = '\0';
			check_add_file(filename, true);
		}
		free(filename);
	}

	for (i = 0; i < options->filecnt; i++) {
		filename = options->filenames[i];

		if (stat(filename, &fstats) < 0) {
			error(0, errno, "%s", filename);
			continue;
		}
		if (!S_ISDIR(fstats.st_mode)) {
			check_add_file(filename, true);
		} else {
			if (r_opendir(&dir, filename, options->recursive) < 0) {
				error(0, errno, "%s", filename);
				continue;
			}
			start = fileidx;
			while ((filename = r_readdir(&dir, true)) != NULL) {
				check_add_file(filename, false);
				free((void*) filename);
			}
			r_closedir(&dir);
			if (fileidx - start > 1)
				qsort(files + start, fileidx - start, sizeof(fileinfo_t), fncmp);
		}
	}

	if (fileidx == 0)
		error(EXIT_FAILURE, 0, "No valid image file given, aborting");

	filecnt = fileidx;
	fileidx = options->startnum < filecnt ? options->startnum : 0;

	for (i = 0; i < ARRLEN(buttons); i++) {
		if (buttons[i].cmd == i_cursor_navigate) {
			imgcursor[0] = CURSOR_LEFT;
			imgcursor[2] = CURSOR_RIGHT;
			break;
		}
	}

	win_init(&win);
	img_init(&img, &win);
	arl_init(&arl);

	if ((homedir = getenv("XDG_CONFIG_HOME")) == NULL || homedir[0] == '\0') {
		homedir = getenv("HOME");
		dsuffix = "/.config";
	}
	if (homedir != NULL) {
		extcmd_t *cmd[] = { &info.f, &keyhandler.f };
		const char *name[] = { "image-info", "key-handler" };

		for (i = 0; i < ARRLEN(cmd); i++) {
			n = strlen(homedir) + strlen(dsuffix) + strlen(name[i]) + 12;
			cmd[i]->cmd = (char*) emalloc(n);
			snprintf(cmd[i]->cmd, n, "%s%s/swiv/exec/%s", homedir, dsuffix, name[i]);
			if (access(cmd[i]->cmd, X_OK) != 0) {
				snprintf(cmd[i]->cmd, n, "%s%s/sxiv/exec/%s", homedir, dsuffix, name[i]);
				if (access(cmd[i]->cmd, X_OK) != 0)
					cmd[i]->err = errno;
			}
		}
	} else {
		error(0, 0, "Exec directory not found");
	}
	info.fd = -1;

	if (options->thumb_mode) {
		mode = MODE_THUMB;
		tns_init(&tns, files, &filecnt, &fileidx, &win);
		while (!tns_load(&tns, fileidx, false, false))
			remove_file(fileidx, false);
	} else {
		mode = MODE_IMAGE;
		tns.thumbs = NULL;
		load_image(fileidx);
	}
	win_open(&win);
	win_set_cursor(&win, CURSOR_WATCH);

	repeat_key.fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	if (repeat_key.fd < 0)
		error(0, errno, "Failed to create timerfd, can't handle key repeats");

	atexit(cleanup);

	run();

	return 0;
}
