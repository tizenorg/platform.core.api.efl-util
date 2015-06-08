#ifndef TIZEN_WINDOW_SCREEN_CLIENT_PROTOCOL_H
#define TIZEN_WINDOW_SCREEN_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct tizen_window_screen;

extern const struct wl_interface tizen_window_screen_interface;

#ifndef TIZEN_WINDOW_SCREEN_MODE_ENUM
#define TIZEN_WINDOW_SCREEN_MODE_ENUM
enum tizen_window_screen_mode {
	TIZEN_WINDOW_SCREEN_MODE_DEFAULT = 0,
	TIZEN_WINDOW_SCREEN_MODE_ALWAYS_ON = 1,
};
#endif /* TIZEN_WINDOW_SCREEN_MODE_ENUM */

#ifndef TIZEN_WINDOW_SCREEN_ERROR_STATE_ENUM
#define TIZEN_WINDOW_SCREEN_ERROR_STATE_ENUM
enum tizen_window_screen_error_state {
	TIZEN_WINDOW_SCREEN_ERROR_STATE_NONE = 0,
	TIZEN_WINDOW_SCREEN_ERROR_STATE_PERMISSION_DENIED = 1,
};
#endif /* TIZEN_WINDOW_SCREEN_ERROR_STATE_ENUM */

struct tizen_window_screen_listener {
	/**
	 * done - (none)
	 * @surface: (none)
	 * @mode: (none)
	 * @error_state: (none)
	 */
	void (*done)(void *data,
		     struct tizen_window_screen *tizen_window_screen,
		     struct wl_surface *surface,
		     uint32_t mode,
		     uint32_t error_state);
};

static inline int
tizen_window_screen_add_listener(struct tizen_window_screen *tizen_window_screen,
				 const struct tizen_window_screen_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) tizen_window_screen,
				     (void (**)(void)) listener, data);
}

#define TIZEN_WINDOW_SCREEN_SET_MODE	0

static inline void
tizen_window_screen_set_user_data(struct tizen_window_screen *tizen_window_screen, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) tizen_window_screen, user_data);
}

static inline void *
tizen_window_screen_get_user_data(struct tizen_window_screen *tizen_window_screen)
{
	return wl_proxy_get_user_data((struct wl_proxy *) tizen_window_screen);
}

static inline void
tizen_window_screen_destroy(struct tizen_window_screen *tizen_window_screen)
{
	wl_proxy_destroy((struct wl_proxy *) tizen_window_screen);
}

static inline void
tizen_window_screen_set_mode(struct tizen_window_screen *tizen_window_screen, struct wl_surface *surface, uint32_t mode)
{
	wl_proxy_marshal((struct wl_proxy *) tizen_window_screen,
			 TIZEN_WINDOW_SCREEN_SET_MODE, surface, mode);
}

#ifdef  __cplusplus
}
#endif

#endif
