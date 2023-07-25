#ifdef _WINDOW_CONFIG

/* default window dimensions (overwritten via -g option): */
enum {
	WIN_WIDTH  = 800,
	WIN_HEIGHT = 600
};

/* colors and font are configured with command line options:
 * -B 'background' -C 'foreground' and -F 'font'
 */

#endif
#ifdef _IMAGE_CONFIG

/* levels (in percent) to use when zooming via '-' and '+':
 * (first/last value is used as min/max zoom level)
 */
static const float zoom_levels[] = {
	 12.5,  25.0,  50.0,  75.0,
	100.0, 150.0, 200.0, 400.0, 800.0
};

/* default slideshow delay (in sec, overwritten via -S option): */
enum { SLIDESHOW_DELAY = 5 };

/* gamma correction: the user-visible ranges [-GAMMA_RANGE, 0] and
 * (0, GAMMA_RANGE] are mapped to the ranges [0, 1], and (1, GAMMA_MAX].
 * */
static const double GAMMA_MAX   = 10.0;
static const int    GAMMA_RANGE = 32;

/* command i_scroll pans image 1/PAN_FRACTION of screen width/height */
static const int PAN_FRACTION = 5;

/* if false, pixelate images at zoom level != 100%,
 * toggled with 'a' key binding
 */
static const bool ANTI_ALIAS = true;

/* if true, use a checkerboard background for alpha layer,
 * toggled with 'A' key binding
 */
static const bool ALPHA_LAYER = false;

#endif
#ifdef _THUMBS_CONFIG

/* thumbnail sizes in pixels (width == height): */
static const int thumb_sizes[] = { 32, 64, 96, 128, 160 };

/* thumbnail size at startup, index into thumb_sizes[]: */
static const int THUMB_SIZE = 3;

#endif
#ifdef _MAPPINGS_CONFIG

#define ControlMask (1 << 2)
#define Mod1Mask    (1 << 3)
#define ShiftMask   (1 << 0)
#define None 0

/* keyboard mappings for image and thumbnail mode: */
static const keymap_t keys[] = {
	/* modifiers    key               function              argument */
	{ 0,            XKB_KEY_q,             g_quit,               None },
	{ 0,            XKB_KEY_Return,        g_switch_mode,        None },
	{ 0,            XKB_KEY_f,             g_toggle_fullscreen,  None },
	{ 0,            XKB_KEY_b,             g_toggle_bar,         None },
	{ ControlMask,  XKB_KEY_x,             g_prefix_external,    None },
	{ 0,            XKB_KEY_g,             g_first,              None },
	{ 0,            XKB_KEY_G,             g_n_or_last,          None },
	{ 0,            XKB_KEY_r,             g_reload_image,       None },
	{ 0,            XKB_KEY_D,             g_remove_image,       None },
	{ ControlMask,  XKB_KEY_h,             g_scroll_screen,      DIR_LEFT },
	{ ControlMask,  XKB_KEY_Left,          g_scroll_screen,      DIR_LEFT },
	{ ControlMask,  XKB_KEY_j,             g_scroll_screen,      DIR_DOWN },
	{ ControlMask,  XKB_KEY_Down,          g_scroll_screen,      DIR_DOWN },
	{ ControlMask,  XKB_KEY_k,             g_scroll_screen,      DIR_UP },
	{ ControlMask,  XKB_KEY_Up,            g_scroll_screen,      DIR_UP },
	{ ControlMask,  XKB_KEY_l,             g_scroll_screen,      DIR_RIGHT },
	{ ControlMask,  XKB_KEY_Right,         g_scroll_screen,      DIR_RIGHT },
	{ 0,            XKB_KEY_plus,          g_zoom,               +1 },
	{ 0,            XKB_KEY_KP_Add,        g_zoom,               +1 },
	{ 0,            XKB_KEY_minus,         g_zoom,               -1 },
	{ 0,            XKB_KEY_KP_Subtract,   g_zoom,               -1 },
	{ 0,            XKB_KEY_m,             g_toggle_image_mark,  None },
	{ 0,            XKB_KEY_M,             g_mark_range,         None },
	{ ControlMask,  XKB_KEY_m,             g_reverse_marks,      None },
	{ ControlMask,  XKB_KEY_u,             g_unmark_all,         None },
	{ 0,            XKB_KEY_N,             g_navigate_marked,    +1 },
	{ 0,            XKB_KEY_P,             g_navigate_marked,    -1 },
	{ 0,            XKB_KEY_braceleft,     g_change_gamma,       -1 },
	{ 0,            XKB_KEY_braceright,    g_change_gamma,       +1 },
	{ ControlMask,  XKB_KEY_g,             g_change_gamma,        0 },

	{ 0,            XKB_KEY_h,             t_move_sel,           DIR_LEFT },
	{ 0,            XKB_KEY_Left,          t_move_sel,           DIR_LEFT },
	{ 0,            XKB_KEY_j,             t_move_sel,           DIR_DOWN },
	{ 0,            XKB_KEY_Down,          t_move_sel,           DIR_DOWN },
	{ 0,            XKB_KEY_k,             t_move_sel,           DIR_UP },
	{ 0,            XKB_KEY_Up,            t_move_sel,           DIR_UP },
	{ 0,            XKB_KEY_l,             t_move_sel,           DIR_RIGHT },
	{ 0,            XKB_KEY_Right,         t_move_sel,           DIR_RIGHT },
	{ 0,            XKB_KEY_R,             t_reload_all,         None },

	{ 0,            XKB_KEY_n,             i_navigate,           +1 },
	{ 0,            XKB_KEY_n,             i_scroll_to_edge,     DIR_LEFT | DIR_UP },
	{ 0,            XKB_KEY_space,         i_navigate,           +1 },
	{ 0,            XKB_KEY_p,             i_navigate,           -1 },
	{ 0,            XKB_KEY_p,             i_scroll_to_edge,     DIR_LEFT | DIR_UP },
	{ 0,            XKB_KEY_BackSpace,     i_navigate,           -1 },
	{ 0,            XKB_KEY_bracketright,  i_navigate,           +10 },
	{ 0,            XKB_KEY_bracketleft,   i_navigate,           -10 },
	{ ControlMask,  XKB_KEY_6,             i_alternate,          None },
	{ ControlMask,  XKB_KEY_n,             i_navigate_frame,     +1 },
	{ ControlMask,  XKB_KEY_p,             i_navigate_frame,     -1 },
	{ ControlMask,  XKB_KEY_space,         i_toggle_animation,   None },
	{ 0,            XKB_KEY_h,             i_scroll,             DIR_LEFT },
	{ 0,            XKB_KEY_Left,          i_scroll,             DIR_LEFT },
	{ 0,            XKB_KEY_j,             i_scroll,             DIR_DOWN },
	{ 0,            XKB_KEY_Down,          i_scroll,             DIR_DOWN },
	{ 0,            XKB_KEY_k,             i_scroll,             DIR_UP },
	{ 0,            XKB_KEY_Up,            i_scroll,             DIR_UP },
	{ 0,            XKB_KEY_l,             i_scroll,             DIR_RIGHT },
	{ 0,            XKB_KEY_Right,         i_scroll,             DIR_RIGHT },
	{ 0,            XKB_KEY_H,             i_scroll_to_edge,     DIR_LEFT },
	{ 0,            XKB_KEY_J,             i_scroll_to_edge,     DIR_DOWN },
	{ 0,            XKB_KEY_K,             i_scroll_to_edge,     DIR_UP },
	{ 0,            XKB_KEY_L,             i_scroll_to_edge,     DIR_RIGHT },
	{ 0,            XKB_KEY_equal,         i_set_zoom,           100 },
	{ 0,            XKB_KEY_w,             i_fit_to_win,         SCALE_DOWN },
	{ 0,            XKB_KEY_W,             i_fit_to_win,         SCALE_FIT },
	{ 0,            XKB_KEY_e,             i_fit_to_win,         SCALE_WIDTH },
	{ 0,            XKB_KEY_E,             i_fit_to_win,         SCALE_HEIGHT },
	{ 0,            XKB_KEY_less,          i_rotate,             DEGREE_270 },
	{ 0,            XKB_KEY_greater,       i_rotate,             DEGREE_90 },
	{ 0,            XKB_KEY_question,      i_rotate,             DEGREE_180 },
	{ 0,            XKB_KEY_bar,           i_flip,               FLIP_HORIZONTAL },
	{ 0,            XKB_KEY_underscore,    i_flip,               FLIP_VERTICAL },
	{ 0,            XKB_KEY_a,             i_toggle_antialias,   None },
	{ 0,            XKB_KEY_A,             i_toggle_alpha,       None },
	{ 0,            XKB_KEY_s,             i_slideshow,          None },
};

#include <linux/input-event-codes.h>
/* mouse button mappings for image mode: */
static const button_t buttons[] = {
	/* modifiers    button              function              argument */
	{ 0,            BTN_LEFT,           i_cursor_navigate,    None },
	{ 0,            BTN_MIDDLE,         i_drag,               DRAG_ABSOLUTE },
	{ 0,            BTN_RIGHT,          g_switch_mode,        None },
};

/* mouse scrolling mappings for image mode */
static const scroll_t scrolls[] = {
	/* direction of -1 means up / left and +1 means down / right. It changes
	 * depending on the axis.
	 */

	/* modifiers          axis                    direction    function  argument */
	{ 0,          WL_POINTER_AXIS_VERTICAL_SCROLL,    +1,       g_zoom,     -1 },
	{ 0,          WL_POINTER_AXIS_VERTICAL_SCROLL,    -1,       g_zoom,     +1 },
};

#endif
