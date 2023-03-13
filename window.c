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
#define _WINDOW_CONFIG
#include "config.h"

#include <cairo.h>
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-cursor.h>

#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-client-protocol.h"
#include "shm.h"

enum {
	H_TEXT_PAD = 5,
	V_TEXT_PAD = 1
};

static struct {
	char *name;
	struct wl_cursor *cursor;
} cursors[CURSOR_COUNT] = {
	{ "left_ptr" },
	{ "dotbox" },
	{ "watch" },
	{ "sb_left_arrow" },
	{ "sb_right_arrow" },
	{ "left_ptr" },
};

static int barheight;

// functions defined in main.c for satisfying listener interfaces
void keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
void pointer_handle_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y);
void pointer_handle_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
		uint32_t axis, wl_fixed_t value);

void win_render_imlib_image(win_t *win, int x, int y)
{
	uint32_t *img_data = imlib_image_get_data_for_reading_only();
	int img_w = imlib_image_get_width();
	int img_h = imlib_image_get_height();

	cairo_surface_t *img_surf = cairo_image_surface_create_for_data(
			(unsigned char*)img_data, CAIRO_FORMAT_ARGB32, img_w, img_h,
			img_w * 4);
	cairo_status_t status = cairo_surface_status(img_surf);
	if (status != CAIRO_STATUS_SUCCESS)
		error(EXIT_FAILURE, 0, "error: cairo surface: %s",
				cairo_status_to_string(status));

	cairo_set_source_surface(win->buffer.cr, img_surf, x, y);
	cairo_paint(win->buffer.cr);

	cairo_surface_destroy(img_surf);
}

void win_render_imlib_image_at_size(win_t *win, int x, int y, int w, int h)
{
	Imlib_Image saved_img = imlib_context_get_image();
	Imlib_Image img = imlib_create_cropped_scaled_image(0, 0, imlib_image_get_width(), imlib_image_get_height(), w, h);
	if (img == NULL)
		error(EXIT_FAILURE, 0, "error: failed to scale image");

	imlib_context_set_image(img);

	win_render_imlib_image(win, x, y);

	imlib_free_image();
	imlib_context_set_image(saved_img);
}

static void pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	win_t *win = data;
	win->pointer.serial = serial;

	wl_pointer_set_cursor(wl_pointer, serial, win->pointer.surface,
			win->pointer.image->hotspot_x, win->pointer.image->hotspot_y);
}

static void pointer_handle_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface)
{
}
static void pointer_handle_frame(void *data, struct wl_pointer *wl_pointer)
{
}
static void pointer_handle_axis_source(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis_source)
{
}
static void pointer_handle_axis_stop(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis)
{
}
static void pointer_handle_axis_discrete(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis, int32_t discrete)
{
}

struct wl_pointer_listener pointer_listener = {
	.enter = pointer_handle_enter,
	.leave = pointer_handle_leave,
	.motion = pointer_handle_motion,
	.button = pointer_handle_button,
	.axis = pointer_handle_axis,

	// Unused, only for satisfying interface
	.frame = pointer_handle_frame,
	.axis_source = pointer_handle_axis_source,
	.axis_stop = pointer_handle_axis_stop,
	.axis_discrete = pointer_handle_axis_discrete,
};

static void keyboard_handle_keymap(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t format, int32_t fd, uint32_t size)
{
	win_t *win = data;

	if (win->xkb_context == NULL)
		win->xkb_context = xkb_context_new(0);

	char *keymap_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (keymap_str == MAP_FAILED) {
		error(EXIT_FAILURE, errno, "failed to get keymap");
		close(fd);
		return;
	}
	if (win->xkb_keymap != NULL) {
		xkb_keymap_unref(win->xkb_keymap);
		win->xkb_keymap = NULL;
	}
	if (win->xkb_state != NULL) {
		xkb_state_unref(win->xkb_state);
		win->xkb_state = NULL;
	}

	win->xkb_keymap = xkb_keymap_new_from_string(win->xkb_context, keymap_str,
			XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

	win->xkb_state = xkb_state_new(win->xkb_keymap);

	munmap(keymap_str, size);

	close(fd);
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
		uint32_t mods_locked, uint32_t group)
{
	win_t *win = data;
	win->mods_depressed = mods_depressed;

	win->mods_latched   = mods_latched;
	win->mods_locked    = mods_locked;
	win->group          = group;
	xkb_state_update_mask(win->xkb_state, mods_depressed, mods_latched,
			mods_locked, 0, 0, group);
}

static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
		int32_t rate, int32_t delay)
{
	win_t *win = data;
	win->repeat_rate = rate;
	win->repeat_delay = delay;
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
}
static void keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface)
{
}

struct wl_keyboard_listener keyboard_listener = {
	.enter = keyboard_handle_enter,
	.leave = keyboard_handle_leave,
	.keymap = keyboard_handle_keymap,
	.key = keyboard_handle_key,
	.modifiers = keyboard_handle_modifiers,
	.repeat_info = keyboard_handle_repeat_info,
};

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
		uint32_t capabilities)
{
	win_t *win = data;
	if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
		struct wl_pointer *pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(pointer, &pointer_listener, win);
		win->pointer.pointer = pointer;
	}

	if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
		struct wl_keyboard *keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(keyboard, &keyboard_listener, win);
	}
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = seat_handle_name,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version)
{
	win_t *win = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		int want_version = 4;
		if (want_version > version)
			error(EXIT_FAILURE, 0,
					"error: wl_compositor: want version %d got %d",
					want_version, version);
		win->compositor = wl_registry_bind(registry, name,
				&wl_compositor_interface, want_version);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		win->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		win->xdg_wm_base = wl_registry_bind(registry, name,
				&xdg_wm_base_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		int want_version = 5;
		if (want_version > version)
			error(EXIT_FAILURE, 0,
					"error: wl_seat: want version %d got %d",
					want_version, version);
		struct wl_seat *seat = wl_registry_bind(registry, name,
				&wl_seat_interface, want_version);
		win->seat = seat;
		wl_seat_add_listener(seat, &seat_listener, win);
	} else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
		win->decor_manager = wl_registry_bind(registry, name,
				&zxdg_decoration_manager_v1_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *wl_registry,
		uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static void xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	win_t *win = data;
	win->quit = true;
}

static void free_buffer(win_buf_t *buf)
{
	if (buf->data == NULL)
		return;

	wl_buffer_destroy(buf->wl_buf);

	close(buf->fd);
	buf->data = NULL;
	buf->data_size = 0;
	buf->wl_buf = NULL;

	g_object_unref(buf->layout);
	buf->layout = NULL;
	cairo_destroy(buf->cr);
	buf->cr = NULL;
}

static win_buf_t new_buffer(int width, int height, struct wl_shm *shm,
		PangoFontDescription *font_desc)
{
	win_buf_t buf = {0};
	int stride = width * 4;
	int shm_pool_size = height * stride;

	int fd = allocate_shm_file(shm_pool_size);
	if (fd < 0)
		error(EXIT_FAILURE, 0, "error: failed to allocate shm file");

	uint8_t *pool_data = mmap(NULL, shm_pool_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (pool_data == MAP_FAILED) {
		close(fd);
		error(EXIT_FAILURE, errno, "error: failed to allocate framebuffer");
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, shm_pool_size);
	if (pool == NULL) {
		close(fd);
		munmap(pool_data, shm_pool_size);
		error(EXIT_FAILURE, errno, "error: failed to create shm pool");
	}

	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width,
			height, stride, WL_SHM_FORMAT_ARGB8888);
	if (buffer == NULL) {
		error(EXIT_FAILURE, errno, "error: failed to create wl_buffer");
	}
	wl_shm_pool_destroy(pool);

	buf.fd = fd;
	buf.wl_buf = buffer;
	buf.data = pool_data;
	buf.data_size = shm_pool_size;

	buf.cr_surf = cairo_image_surface_create_for_data(
			(unsigned char *)buf.data, CAIRO_FORMAT_ARGB32, width,
			height, 4 * width);
	cairo_status_t status = cairo_surface_status(buf.cr_surf);
	if (status != CAIRO_STATUS_SUCCESS) {
		error(EXIT_FAILURE, 0, "error: cairo surface: %s",
				cairo_status_to_string(status));
	}
	buf.cr = cairo_create(buf.cr_surf);
	status = cairo_status(buf.cr);
	cairo_surface_destroy(buf.cr_surf);
	if (status != CAIRO_STATUS_SUCCESS) {
		error(EXIT_FAILURE, 0, "error: cairo: %s",
				cairo_status_to_string(status));
	}

	buf.layout = pango_cairo_create_layout(buf.cr);
	pango_layout_set_font_description(buf.layout, font_desc);
	return buf;
}

void win_recreate_buffer(win_t *win)
{
	// Copy the contents of the previous buffer to the newer buffer for
	// smoother resizing since redrawing the whole image is kind of slow.
	win_buf_t prev = win->buffer;
	win->buffer = new_buffer(win->width, win->height + win->bar.h, win->shm, win->font_desc);

	cairo_set_source_surface(win->buffer.cr, prev.cr_surf, 0, 0);
	cairo_paint(win->buffer.cr);

	free_buffer(&prev);
}

static void xdg_toplevel_handle_configure(void *data,
		struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height,
		struct wl_array *states)
{
	enum xdg_toplevel_state *state;
	win_t *win = data;

	win->fullscreen = false;
	wl_array_for_each(state, states) {
		if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN)
			win->fullscreen = true;
	}


	if (width <= 0)
		win->width = WIN_WIDTH;
	else
		win->width = width;

	if (height <= 0)
		win->height = WIN_HEIGHT;
	else
		win->height = height;

	if (win->bar.h > 0)
		win->height -= win->bar.h;

	win->resized = true;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
	.close = xdg_toplevel_handle_close,
};

void win_init(win_t *win)
{
	memset(win, 0, sizeof(win_t));

	win->height = WIN_HEIGHT;
	win->width = WIN_WIDTH;
	if ((win->display = wl_display_connect(NULL)) == NULL)
		error(EXIT_FAILURE, 0, "error opening wayland display");

	struct wl_registry *registry = wl_display_get_registry(win->display);
	wl_registry_add_listener(registry, &registry_listener, win);
	wl_display_roundtrip(win->display);

	if (win->shm == NULL || win->compositor == NULL || win->xdg_wm_base == NULL)
		error(EXIT_FAILURE, 0, "error: no wl_shm, xdg_wm_base or wl_compositor");

	win->bg = options->bg;
	win->fg = options->fg;

	if (options->font != NULL)
		win->font_desc = pango_font_description_from_string(options->font);

	win->buffer = new_buffer(win->width, win->height, win->shm, win->font_desc);

	int fontheight;
	pango_layout_get_pixel_size(win->buffer.layout, NULL, &fontheight);
	barheight = fontheight + 2 * V_TEXT_PAD;


	win->bar.l.size = BAR_L_LEN;
	win->bar.r.size = BAR_R_LEN;
	/* 3 padding bytes needed by utf8_decode */
	win->bar.l.buf = emalloc(win->bar.l.size + 3);
	win->bar.l.buf[0] = '\0';
	win->bar.r.buf = emalloc(win->bar.r.size + 3);
	win->bar.r.buf[0] = '\0';
	win->bar.h = options->hide_bar ? 0 : barheight;
}

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial)
{
	win_t *win = data;
	xdg_surface_ack_configure(xdg_surface, serial);
	wl_surface_commit(win->surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};
static void xdg_wm_base_handle_ping(void *data, struct xdg_wm_base *xdg_wm_base,
		uint32_t serial)
{
	xdg_wm_base_pong(xdg_wm_base, serial);
}

struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_handle_ping,
};

static void cursor_callback(void *data, struct wl_callback *cb, uint32_t time);

static struct wl_callback_listener cursor_callback_listener = {
	.done = cursor_callback,
};

static void win_render_cursor(win_t *win)
{
	win->pointer.image = win->pointer.cursor->images[win->pointer.curimg];
	struct wl_buffer *cursor_buffer = wl_cursor_image_get_buffer(win->pointer.image);

	wl_surface_attach(win->pointer.surface, cursor_buffer, 0, 0);
	wl_surface_damage_buffer(win->pointer.surface, 0, 0,
			win->pointer.image->width, win->pointer.image->height);
	wl_surface_commit(win->pointer.surface);

	wl_pointer_set_cursor(win->pointer.pointer, win->pointer.serial, win->pointer.surface,
			win->pointer.image->hotspot_x, win->pointer.image->hotspot_y);
}

static void cursor_callback(void *data, struct wl_callback *cb, uint32_t time)
{
	static uint32_t prevtime = 0;
	win_t *win = data;
	wl_callback_destroy(cb);

	cb = wl_surface_frame(win->surface);
	wl_callback_add_listener(cb, &cursor_callback_listener , win);

	win_render_cursor(win);

	if (win->pointer.cursor->image_count > 1) {
		if (time - prevtime >= win->pointer.image->delay) {
			win->pointer.curimg = (win->pointer.curimg+1) % win->pointer.cursor->image_count;
			prevtime = time;
		}
	}
}

void win_set_cursor(win_t *win, cursor_t cursor)
{
	if (cursor >= 0 && cursor < ARRLEN(cursors) &&
			win->pointer.cursor != cursors[cursor].cursor &&
			cursors[cursor].cursor != NULL) {
		win->pointer.curimg = 0;
		win->pointer.cursor = cursors[cursor].cursor;
	}
	win_render_cursor(win);
}

void win_cursor_pos(win_t *win, int *x, int *y)
{
	if (x) *x = win->pointer.x;
	if (y) *y = win->pointer.y;
}

static void init_cursor(win_t *win)
{
	int i;
	int cursor_size = 24;
	char *env_cursor_size = getenv("XCURSOR_SIZE");
	if (env_cursor_size && *env_cursor_size) {
		char *end;
		int n = strtol(env_cursor_size, &end, 0);
		if (*end == '\0' && n > 0) {
			cursor_size = n;
		}
	}

	char *cursor_theme = getenv("XCURSOR_THEME");
	win->pointer.theme = wl_cursor_theme_load(cursor_theme, cursor_size, win->shm);
	for (i = 0; i < ARRLEN(cursors); i++) {
		cursors[i].cursor = wl_cursor_theme_get_cursor(win->pointer.theme,
				cursors[i].name);
	}
	win->pointer.cursor = cursors[0].cursor;
	if (cursors[0].cursor == NULL) {
		error(EXIT_FAILURE, 0,
				"error: cursor theme %s doesn't have cursor left_ptr",
				cursor_theme);
	}
	win->pointer.image = win->pointer.cursor->images[0];
	win->pointer.curimg = 0;
	struct wl_buffer *cursor_buffer = wl_cursor_image_get_buffer(win->pointer.image);

	win->pointer.surface = wl_compositor_create_surface(win->compositor);
	wl_surface_attach(win->pointer.surface, cursor_buffer, 0, 0);
	wl_surface_damage_buffer(win->pointer.surface, 0, 0,
			win->pointer.image->width, win->pointer.image->height);
	wl_surface_commit(win->pointer.surface);
}

void win_open(win_t *win)
{
	win->surface = wl_compositor_create_surface(win->compositor);
	win->xdg_surface = xdg_wm_base_get_xdg_surface(win->xdg_wm_base, win->surface);
	win->xdg_toplevel = xdg_surface_get_toplevel(win->xdg_surface);

	xdg_surface_add_listener(win->xdg_surface, &xdg_surface_listener, win);
	xdg_toplevel_add_listener(win->xdg_toplevel, &xdg_toplevel_listener, win);

	xdg_toplevel_set_title(win->xdg_toplevel, "swiv");
	xdg_toplevel_set_app_id(win->xdg_toplevel,
			options->res_name != NULL ? options->res_name : "swiv");

	if (options->fullscreen)
		win_toggle_fullscreen(win);

	if (win->decor_manager != NULL) {
		win->top_decor =
			zxdg_decoration_manager_v1_get_toplevel_decoration(
					win->decor_manager, win->xdg_toplevel);
		zxdg_toplevel_decoration_v1_set_mode(
				win->top_decor,
				ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	xdg_wm_base_add_listener(win->xdg_wm_base, &xdg_wm_base_listener, win);

	init_cursor(win);

	wl_surface_commit(win->surface);
	wl_display_roundtrip(win->display);

	wl_surface_attach(win->surface, win->buffer.wl_buf, 0, 0);
	wl_surface_damage_buffer(win->surface, 0, 0, UINT32_MAX, UINT32_MAX);
	wl_surface_commit(win->surface);

	struct wl_callback *cb = wl_surface_frame(win->pointer.surface);
	wl_callback_add_listener(cb, &cursor_callback_listener, win);
}

CLEANUP void win_close(win_t *win)
{
	if (win->top_decor != NULL)
		zxdg_toplevel_decoration_v1_destroy(win->top_decor);
	if (win->decor_manager != NULL)
		zxdg_decoration_manager_v1_destroy(win->decor_manager);

	wl_cursor_theme_destroy(win->pointer.theme);
	wl_surface_destroy(win->pointer.surface);
	free_buffer(&win->buffer);
	xdg_toplevel_destroy(win->xdg_toplevel);
	xdg_surface_destroy(win->xdg_surface);
	xdg_wm_base_destroy(win->xdg_wm_base);
	wl_surface_destroy(win->surface);
	wl_display_disconnect(win->display);
}

void win_toggle_fullscreen(win_t *win)
{
	// win->fullscreen value will be set at xdg_toplevel_handle_configure
	if (win->fullscreen)
		xdg_toplevel_unset_fullscreen(win->xdg_toplevel);
	else
		xdg_toplevel_set_fullscreen(win->xdg_toplevel, NULL);
}

void win_toggle_bar(win_t *win)
{
	if (win->bar.h != 0) {
		win->height += win->bar.h;
		win->bar.h = 0;
	} else {
		win->bar.h = barheight;
		win->height -= win->bar.h;
	}
}

void win_clear(win_t *win)
{
	cairo_set_source_rgba(win->buffer.cr, win->bg.r, win->bg.g, win->bg.b, win->bg.a);
	cairo_paint(win->buffer.cr);
}

#define TEXTWIDTH(win, text, len) \
	win_draw_text(win, NULL, 0, 0, text, len, 0)

static int win_draw_text(win_buf_t *buf, color_t *color, int x, int y, char *text,
		int len, int w)
{
	int width;
	pango_layout_set_text(buf->layout, text, len);
	pango_layout_get_pixel_size(buf->layout, &width, NULL);

	if (color == NULL)
		return width;

	cairo_move_to(buf->cr, x, y);
	cairo_set_source_rgba(buf->cr, color->r, color->g, color->b, color->a);
	pango_cairo_show_layout(buf->cr, buf->layout);

	return width;
}

static void win_draw_bar(win_t *win)
{
	int len, x, y, w, tw;
	win_bar_t *l, *r;
	win_buf_t *buf = &win->buffer;

	if ((l = &win->bar.l)->buf == NULL || (r = &win->bar.r)->buf == NULL)
		return;

	y = win->height + V_TEXT_PAD;
	w = win->width - 2*H_TEXT_PAD;

	cairo_set_source_rgba(buf->cr, win->fg.r, win->fg.g, win->fg.b, win->fg.a);
	cairo_rectangle(buf->cr, 0, win->height, win->width, win->bar.h);
	cairo_fill(buf->cr);

	if ((len = strlen(r->buf)) > 0) {
		if ((tw = TEXTWIDTH(buf, r->buf, len)) > w)
			return;

		x = win->width - tw - 2 * H_TEXT_PAD;
		w -= tw;

		win_draw_text(buf, &win->bg, x, y, r->buf, len, tw);
	}
	if ((len = strlen(l->buf)) > 0) {
		x = H_TEXT_PAD;
		w -= H_TEXT_PAD; /* gap between left and right parts */

		win_draw_text(buf, &win->bg, x, y, l->buf, len, w);
	}
}

void win_draw(win_t *win)
{
	if (win->bar.h > 0)
		win_draw_bar(win);

	win->redraw = true;
}

void win_draw_rect(win_t *win, int x, int y, int w, int h, bool fill, int lw,
		color_t col)
{
	cairo_t *cr = win->buffer.cr;
	cairo_set_source_rgba(cr, col.r, col.g, col.b, col.a);
	cairo_set_line_width(cr, lw);
	cairo_rectangle(cr, x, y, w, h);

	if (fill)
		cairo_fill(cr);
	else
		cairo_stroke(cr);
}
