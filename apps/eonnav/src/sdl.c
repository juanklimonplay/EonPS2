/* sdl.c
 * SDL2 graphics driver for EonNav (Links on the PlayStation 2).
 * Draws into a linear memory framebuffer (the same primitives as framebuf.c)
 * and presents it through an SDL2 streaming texture. Input comes from the
 * gamepad (pointer on the left stick), a USB keyboard and a USB mouse.
 * The same file builds on a Linux host, where EONNAV_SHOT=<file.ppm> and
 * EONNAV_SHOT_MS=<ms> save the screen and exit, for headless testing.
 * This file is a part of EonNav, released under GPL (like Links).
 */

#include "cfg.h"

#ifdef GRDRV_SDL

#include "links.h"
#include "bits.h"

#include <SDL.h>

#include "arrow.inc"

#define fb_pixelsize	4
#include "fbcommon.inc"

#define POLL_MS		20
#define NUMBER_OF_DEVICES	10

/* Pointer speed, in pixels per poll at full stick deflection. */
#define STICK_SPEED	14
#define STICK_DEAD	6000

static SDL_Window *sdl_window;
static SDL_Renderer *sdl_renderer;
static SDL_Texture *sdl_texture;
static SDL_GameController *sdl_pad;

static unsigned char *fb_vmem;
static int fb_linesize;
static int fb_xsize, fb_ysize;
static int fb_dirty;

static struct timer *sdl_timer;
static unsigned char *sdl_driver_param;

static int mouse_x, mouse_y;
static int mouse_buttons;		/* bitmask of held B_LEFT/B_RIGHT/B_MIDDLE */
static int pointer_visible = 1;

static int stick_x, stick_y, stick_rx, stick_ry;
static int scroll_accum;

static char *shot_file;
static uttime shot_at;

extern struct graphics_driver sdl_driver;

#define TEST_INACTIVITY if (dev != current_virtual_device) return;
#define TEST_INACTIVITY_0 if (dev != current_virtual_device) return 0;

static void sdl_draw_bitmap(struct graphics_device *dev, struct bitmap *bmp, int x, int y)
{
	unsigned char *scr_start;
	size_t copy_len;
	int xs, ys;
	unsigned char *data = bmp->data;

	if (!data)
		return;
	TEST_INACTIVITY
	CLIP_DRAW_BITMAP
	xs = bmp->x;
	ys = bmp->y;
	if (x + xs > dev->clip.x2) xs = dev->clip.x2 - x;
	if (y + ys > dev->clip.y2) ys = dev->clip.y2 - y;
	if (dev->clip.x1 - x > 0) {
		xs -= (dev->clip.x1 - x);
		data += fb_pixelsize * (dev->clip.x1 - x);
		x = dev->clip.x1;
	}
	if (dev->clip.y1 - y > 0) {
		ys -= (dev->clip.y1 - y);
		data += bmp->skip * (dev->clip.y1 - y);
		y = dev->clip.y1;
	}
	scr_start = fb_vmem + y * fb_linesize + x * fb_pixelsize;
	copy_len = xs * fb_pixelsize;
	for (; ys; ys--) {
		memcpy_to_fb_inline(scr_start, data, copy_len, 0);
		data += bmp->skip;
		scr_start += fb_linesize;
	}
	fb_dirty = 1;
}

static void sdl_fill_area(struct graphics_device *dev, int x1, int y1, int x2, int y2, long color)
{
	unsigned char *dest;
	int y;

	TEST_INACTIVITY
	CLIP_FILL_AREA
	dest = fb_vmem + y1 * fb_linesize + x1 * fb_pixelsize;
	for (y = y2 - y1; y; y--) {
		pixel_set(dest, (x2 - x1) * fb_pixelsize, &color);
		dest += fb_linesize;
	}
	fb_dirty = 1;
}

static void sdl_draw_hline(struct graphics_device *dev, int x1, int y, int x2, long color)
{
	TEST_INACTIVITY
	CLIP_DRAW_HLINE
	pixel_set(fb_vmem + y * fb_linesize + x1 * fb_pixelsize, (x2 - x1) * fb_pixelsize, &color);
	fb_dirty = 1;
}

static void sdl_draw_vline(struct graphics_device *dev, int x, int y1, int y2, long color)
{
	unsigned char *dest;
	int y;
	unsigned val = (unsigned)color;

	TEST_INACTIVITY
	CLIP_DRAW_VLINE
	dest = fb_vmem + y1 * fb_linesize + x * fb_pixelsize;
	for (y = y2 - y1; y; y--) {
		*(unsigned *)dest = val;
		dest += fb_linesize;
	}
	fb_dirty = 1;
}

static int sdl_scroll(struct graphics_device *dev, struct rect_set **ignore, int scx, int scy)
{
	unsigned char *dest, *src;
	int y, len, dest_off, src_off;

	TEST_INACTIVITY_0
	if (scx >= 0) {
		len = (dev->clip.x2 - dev->clip.x1 - scx) * fb_pixelsize;
		dest_off = scx * fb_pixelsize;
		src_off = 0;
	} else {
		len = (dev->clip.x2 - dev->clip.x1 + scx) * fb_pixelsize;
		dest_off = 0;
		src_off = -scx * fb_pixelsize;
	}
	if (scy > 0) {
		dest = fb_vmem + (dev->clip.y2 - 1) * fb_linesize + dev->clip.x1 * fb_pixelsize;
		src = dest - fb_linesize * scy;
		dest += dest_off;
		src += src_off;
		for (y = dev->clip.y2 - dev->clip.y1 - scy; y; y--) {
			memcpy_to_fb(dest, src, len, 1);
			dest -= fb_linesize;
			src -= fb_linesize;
		}
	} else {
		dest = fb_vmem + dev->clip.y1 * fb_linesize + dev->clip.x1 * fb_pixelsize;
		src = dest - fb_linesize * scy;
		dest += dest_off;
		src += src_off;
		for (y = dev->clip.y2 - dev->clip.y1 + scy; y; y--) {
			if (scy)
				memcpy_to_fb(dest, src, len, 1);
			else
				memmove_in_fb(dest, src, len);
			dest += fb_linesize;
			src += fb_linesize;
		}
	}
	fb_dirty = 1;
	return 1;
}

static int sdl_get_empty_bitmap(struct bitmap *dest)
{
	dest->data = NULL;
	if (dest->x && (size_t)dest->x * (size_t)dest->y / (size_t)dest->x != (size_t)dest->y)
		return -1;
	if ((size_t)dest->x * (size_t)dest->y > MAX_SIZE_T / 2 / fb_pixelsize)
		return -1;
	dest->data = mem_alloc_mayfail((size_t)dest->x * (size_t)dest->y * fb_pixelsize);
	if (!dest->data)
		return -1;
	dest->skip = (ssize_t)dest->x * fb_pixelsize;
	dest->flags = 0;
	return 0;
}

static void sdl_register_bitmap(struct bitmap *bmp)
{
}

static void sdl_unregister_bitmap(struct bitmap *bmp)
{
	if (bmp->data)
		mem_free(bmp->data);
}

static void *sdl_prepare_strip(struct bitmap *bmp, int top, int lines)
{
	if (!bmp->data)
		return NULL;
	return ((unsigned char *)bmp->data) + bmp->skip * top;
}

static void sdl_commit_strip(struct bitmap *bmp, int top, int lines)
{
}

/* Copy the framebuffer to the texture and draw the arrow over it there, so
 * the arrow never has to be erased from the framebuffer itself. */
static void sdl_present(void)
{
	void *pixels;
	int pitch, y, x;

	if (SDL_LockTexture(sdl_texture, NULL, &pixels, &pitch))
		return;
	for (y = 0; y < fb_ysize; y++)
		memcpy((unsigned char *)pixels + y * pitch, fb_vmem + y * fb_linesize, fb_xsize * fb_pixelsize);
	if (pointer_visible) {
		for (y = 0; y < arrow_height && mouse_y + y < fb_ysize; y++) {
			unsigned black = arrow[y * 2], white = arrow[y * 2 + 1];
			unsigned *row = (unsigned *)((unsigned char *)pixels + (mouse_y + y) * pitch);
			for (x = 0; x < arrow_width && mouse_x + x < fb_xsize; x++) {
				unsigned bit = 0x80000000U >> x;
				if (black & bit) row[mouse_x + x] = 0x000000;
				else if (white & bit) row[mouse_x + x] = 0xffffff;
			}
		}
	}
	SDL_UnlockTexture(sdl_texture);
	SDL_RenderClear(sdl_renderer);
	SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
	SDL_RenderPresent(sdl_renderer);
	fb_dirty = 0;
}

static void save_shot(void)
{
	FILE *f = fopen(shot_file, "wb");
	int x, y;
	if (!f) return;
	fprintf(f, "P6\n%d %d\n255\n", fb_xsize, fb_ysize);
	for (y = 0; y < fb_ysize; y++) {
		unsigned char *p = fb_vmem + y * fb_linesize;
		for (x = 0; x < fb_xsize; x++, p += 4) {
			fputc(p[2], f);
			fputc(p[1], f);
			fputc(p[0], f);
		}
	}
	fclose(f);
}

static void send_key(int key, int flags)
{
	if (current_virtual_device && current_virtual_device->keyboard_handler)
		current_virtual_device->keyboard_handler(current_virtual_device, key, flags);
}

static void send_mouse(int buttons)
{
	if (current_virtual_device && current_virtual_device->mouse_handler)
		current_virtual_device->mouse_handler(current_virtual_device, mouse_x, mouse_y, buttons);
}

static void move_pointer(int x, int y)
{
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x >= fb_xsize) x = fb_xsize - 1;
	if (y >= fb_ysize) y = fb_ysize - 1;
	if (x == mouse_x && y == mouse_y)
		return;
	mouse_x = x;
	mouse_y = y;
	fb_dirty = 1;
	if (mouse_buttons & (1 << B_LEFT)) send_mouse(B_DRAG | B_LEFT);
	else send_mouse(B_MOVE);
}

static void press_button(int b, int down)
{
	if (down) mouse_buttons |= 1 << b;
	else mouse_buttons &= ~(1 << b);
	send_mouse((down ? B_DOWN : B_UP) | b);
}

static int map_sdl_key(SDL_Keycode k)
{
	switch (k) {
		case SDLK_RETURN: case SDLK_KP_ENTER: return KBD_ENTER;
		case SDLK_BACKSPACE: return KBD_BS;
		case SDLK_TAB: return KBD_TAB;
		case SDLK_ESCAPE: return KBD_ESC;
		case SDLK_LEFT: return KBD_LEFT;
		case SDLK_RIGHT: return KBD_RIGHT;
		case SDLK_UP: return KBD_UP;
		case SDLK_DOWN: return KBD_DOWN;
		case SDLK_INSERT: return KBD_INS;
		case SDLK_DELETE: return KBD_DEL;
		case SDLK_HOME: return KBD_HOME;
		case SDLK_END: return KBD_END;
		case SDLK_PAGEUP: return KBD_PAGE_UP;
		case SDLK_PAGEDOWN: return KBD_PAGE_DOWN;
		case SDLK_F1: return KBD_F1;
		case SDLK_F2: return KBD_F2;
		case SDLK_F3: return KBD_F3;
		case SDLK_F4: return KBD_F4;
		case SDLK_F5: return KBD_F5;
		case SDLK_F6: return KBD_F6;
		case SDLK_F7: return KBD_F7;
		case SDLK_F8: return KBD_F8;
		case SDLK_F9: return KBD_F9;
		case SDLK_F10: return KBD_F10;
		case SDLK_F11: return KBD_F11;
		case SDLK_F12: return KBD_F12;
	}
	return 0;
}

/* Gamepad layout (also printed on the start page):
 *   left stick: pointer   right stick: scroll   X: click
 *   O: back   triangle: go to address   square: menu
 *   d-pad: move between links   L1/R1: page up/down   START: enter */
static void pad_button(int b, int down)
{
	switch (b) {
		case SDL_CONTROLLER_BUTTON_A: press_button(B_LEFT, down); return;
	}
	if (!down)
		return;
	switch (b) {
		case SDL_CONTROLLER_BUTTON_B: send_key(KBD_BS, 0); break;
		case SDL_CONTROLLER_BUTTON_Y: send_key('g', 0); break;
		case SDL_CONTROLLER_BUTTON_X: send_key(KBD_ESC, 0); break;
		case SDL_CONTROLLER_BUTTON_START: send_key(KBD_ENTER, 0); break;
		case SDL_CONTROLLER_BUTTON_DPAD_UP: send_key(KBD_UP, 0); break;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN: send_key(KBD_DOWN, 0); break;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT: send_key(KBD_LEFT, 0); break;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: send_key(KBD_RIGHT, 0); break;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: send_key(KBD_PAGE_UP, 0); break;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: send_key(KBD_PAGE_DOWN, 0); break;
	}
}

static int stick(int v)
{
	if (v > -STICK_DEAD && v < STICK_DEAD)
		return 0;
	return v;
}

static void sdl_poll(void *p)
{
	SDL_Event ev;

	sdl_timer = install_timer(POLL_MS, sdl_poll, NULL);
	while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
			case SDL_QUIT:
				send_key(KBD_CTRL_C, 0);
				break;
			case SDL_CONTROLLERDEVICEADDED:
				if (!sdl_pad)
					sdl_pad = SDL_GameControllerOpen(ev.cdevice.which);
				break;
			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
				pad_button(ev.cbutton.button, ev.type == SDL_CONTROLLERBUTTONDOWN);
				break;
			case SDL_CONTROLLERAXISMOTION:
				switch (ev.caxis.axis) {
					case SDL_CONTROLLER_AXIS_LEFTX: stick_x = stick(ev.caxis.value); break;
					case SDL_CONTROLLER_AXIS_LEFTY: stick_y = stick(ev.caxis.value); break;
					case SDL_CONTROLLER_AXIS_RIGHTX: stick_rx = stick(ev.caxis.value); break;
					case SDL_CONTROLLER_AXIS_RIGHTY: stick_ry = stick(ev.caxis.value); break;
				}
				break;
			case SDL_MOUSEMOTION:
				move_pointer(ev.motion.x, ev.motion.y);
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP: {
				int b = ev.button.button == SDL_BUTTON_RIGHT ? B_RIGHT : ev.button.button == SDL_BUTTON_MIDDLE ? B_MIDDLE : B_LEFT;
				move_pointer(ev.button.x, ev.button.y);
				press_button(b, ev.type == SDL_MOUSEBUTTONDOWN);
				break;
			}
			case SDL_MOUSEWHEEL:
				if (ev.wheel.y > 0) send_mouse(B_MOVE | B_WHEELUP);
				if (ev.wheel.y < 0) send_mouse(B_MOVE | B_WHEELDOWN);
				break;
			case SDL_TEXTINPUT: {
				unsigned char *s = (unsigned char *)ev.text.text;
				while (*s) {
					int c = get_utf_8(&s);
					if (!c) break;
					send_key(c, 0);
				}
				break;
			}
			case SDL_KEYDOWN: {
				int flags = 0;
				int key = map_sdl_key(ev.key.keysym.sym);
				SDL_Keymod m = ev.key.keysym.mod;
				if (m & KMOD_SHIFT) flags |= KBD_SHIFT;
				if (m & KMOD_CTRL) flags |= KBD_CTRL;
				if (m & KMOD_ALT) flags |= KBD_ALT;
				if (!key && (flags & (KBD_CTRL | KBD_ALT)) && ev.key.keysym.sym < 128)
					key = ev.key.keysym.sym;
				if ((flags & KBD_CTRL) && key == 'c') key = KBD_CTRL_C, flags = 0;
				if (key) send_key(key, flags);
				break;
			}
		}
	}
	if (stick_x || stick_y)
		move_pointer(mouse_x + stick_x * STICK_SPEED / 32768, mouse_y + stick_y * STICK_SPEED / 32768);
	if (stick_ry) {
		scroll_accum += stick_ry;
		while (scroll_accum >= 65536) scroll_accum -= 65536, send_mouse(B_MOVE | B_WHEELDOWN);
		while (scroll_accum <= -65536) scroll_accum += 65536, send_mouse(B_MOVE | B_WHEELUP);
	} else scroll_accum = 0;
	if (fb_dirty)
		sdl_present();
	if (shot_file && get_time() >= shot_at) {
		save_shot();
		shot_file = NULL;
		send_key(KBD_CTRL_C, 0);
	}
}

static unsigned char *sdl_init_driver(unsigned char *param, unsigned char *ignore)
{
	unsigned char *e;
	int w = 640, h = 448;
	char *s;

	if ((s = getenv("EONNAV_SIZE")) && sscanf(s, "%dx%d", &w, &h) != 2)
		w = 640, h = 448;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)) {
		e = stracpy(cast_uchar "SDL_Init failed: ");
		add_to_strn(&e, cast_uchar SDL_GetError());
		add_to_strn(&e, cast_uchar "\n");
		return e;
	}
	sdl_window = SDL_CreateWindow("EonNav", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, 0);
	if (!sdl_window) goto sdl_fail;
	sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 0);
	if (!sdl_renderer) goto sdl_fail;
	sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, w, h);
	if (!sdl_texture) goto sdl_fail;
	SDL_ShowCursor(SDL_DISABLE);
	SDL_StartTextInput();

	fb_xsize = w;
	fb_ysize = h;
	fb_linesize = w * fb_pixelsize;
	fb_vmem = mem_calloc(fb_linesize * fb_ysize);
	sdl_driver.x = fb_xsize;
	sdl_driver.y = fb_ysize;
	/* 4 bytes per pixel, 24 bits: B, G, R, pad in memory == SDL RGB888 */
	sdl_driver.depth = 196;
	sdl_driver.get_color = get_color_fn(sdl_driver.depth);
	sdl_driver_param = param ? stracpy(param) : NULL;
	init_virtual_devices(&sdl_driver, NUMBER_OF_DEVICES);
	mouse_x = fb_xsize / 2;
	mouse_y = fb_ysize / 2;
	if ((s = getenv("EONNAV_SHOT"))) {
		shot_file = s;
		shot_at = get_time() + ((s = getenv("EONNAV_SHOT_MS")) ? atoi(s) : 3000);
	}
	sdl_timer = install_timer(POLL_MS, sdl_poll, NULL);
	return NULL;

sdl_fail:
	e = stracpy(cast_uchar "SDL video setup failed: ");
	add_to_strn(&e, cast_uchar SDL_GetError());
	add_to_strn(&e, cast_uchar "\n");
	SDL_Quit();
	return e;
}

static void sdl_shutdown_driver(void)
{
	if (sdl_timer) kill_timer(sdl_timer), sdl_timer = NULL;
	shutdown_virtual_devices();
	mem_free(fb_vmem);
	if (sdl_driver_param) mem_free(sdl_driver_param), sdl_driver_param = NULL;
	if (sdl_pad) SDL_GameControllerClose(sdl_pad), sdl_pad = NULL;
	SDL_DestroyTexture(sdl_texture);
	SDL_DestroyRenderer(sdl_renderer);
	SDL_DestroyWindow(sdl_window);
	SDL_Quit();
}

static void sdl_emergency_shutdown(void)
{
	SDL_Quit();
}

static unsigned char *sdl_get_driver_param(void)
{
	return sdl_driver_param;
}

static int sdl_block(struct graphics_device *dev)
{
	return 0;
}

static int sdl_unblock(struct graphics_device *dev)
{
	return 0;
}

struct graphics_driver sdl_driver = {
	cast_uchar "sdl",
	sdl_init_driver,
	init_virtual_device,
	shutdown_virtual_device,
	sdl_shutdown_driver,
	sdl_emergency_shutdown,
	(void (*)(void))NULL,			/* after_fork */
	sdl_get_driver_param,
	(unsigned char *(*)(void))NULL,		/* get_af_unix_name */
	(void (*)(int *, int *, int *, int *))NULL,	/* get_margin */
	(int (*)(int, int, int, int))NULL,	/* set_margin */
	sdl_get_empty_bitmap,
	sdl_register_bitmap,
	sdl_prepare_strip,
	sdl_commit_strip,
	sdl_unregister_bitmap,
	sdl_draw_bitmap,
	(long (*)(int))NULL,			/* get_color, set in init */
	sdl_fill_area,
	sdl_draw_hline,
	sdl_draw_vline,
	sdl_scroll,
	(void (*)(struct graphics_device *dev))NULL,	/* set_clip_area */
	(void (*)(struct graphics_device *dev))NULL,	/* flush */
	sdl_block,
	sdl_unblock,
	(void (*)(void))NULL,			/* set_palette */
	NULL,					/* get_real_colors */
	(void (*)(struct graphics_device *, unsigned char *))NULL,	/* set_title */
	(int (*)(unsigned char *, int))NULL,	/* exec */
	(void (*)(unsigned char *))NULL,	/* set_clipboard_text */
	(unsigned char *(*)(void))NULL,		/* get_clipboard_text */
	0,					/* depth */
	0, 0,					/* size */
	GD_DONT_USE_SCROLL,			/* flags */
	NULL,					/* param */
};

#endif /* GRDRV_SDL */
