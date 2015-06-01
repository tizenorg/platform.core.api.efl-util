#ifndef TIZEN_NOTIFICATION_CLIENT_PROTOCOL_H
#define TIZEN_NOTIFICATION_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct tizen_notification;

extern const struct wl_interface tizen_notification_interface;

#ifndef TIZEN_NOTIFICATION_LEVEL_ENUM
#define TIZEN_NOTIFICATION_LEVEL_ENUM
enum tizen_notification_level {
	TIZEN_NOTIFICATION_LEVEL_1 = 0,
	TIZEN_NOTIFICATION_LEVEL_2 = 1,
	TIZEN_NOTIFICATION_LEVEL_3 = 2,
};
#endif /* TIZEN_NOTIFICATION_LEVEL_ENUM */

#ifndef TIZEN_NOTIFICATION_ERROR_STATE_ENUM
#define TIZEN_NOTIFICATION_ERROR_STATE_ENUM
enum tizen_notification_error_state {
	TIZEN_NOTIFICATION_ERROR_STATE_NONE = 0,
	TIZEN_NOTIFICATION_ERROR_STATE_INVALID_PARAMETER = 1,
	TIZEN_NOTIFICATION_ERROR_STATE_OUT_OF_MEMORY = 2,
	TIZEN_NOTIFICATION_ERROR_STATE_PERMISSION_DENIED = 3,
	TIZEN_NOTIFICATION_ERROR_STATE_NOT_SUPPORTED_WINDOW_TYPE = 4,
};
#endif /* TIZEN_NOTIFICATION_ERROR_STATE_ENUM */

struct tizen_notification_listener {
	/**
	 * done - (none)
	 * @surface: (none)
	 * @level: (none)
	 * @error_state: (none)
	 */
	void (*done)(void *data,
		     struct tizen_notification *tizen_notification,
		     struct wl_surface *surface,
		     uint32_t level,
		     uint32_t error_state);
};

static inline int
tizen_notification_add_listener(struct tizen_notification *tizen_notification,
				const struct tizen_notification_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) tizen_notification,
				     (void (**)(void)) listener, data);
}

#define TIZEN_NOTIFICATION_SET_LEVEL	0

static inline void
tizen_notification_set_user_data(struct tizen_notification *tizen_notification, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) tizen_notification, user_data);
}

static inline void *
tizen_notification_get_user_data(struct tizen_notification *tizen_notification)
{
	return wl_proxy_get_user_data((struct wl_proxy *) tizen_notification);
}

static inline void
tizen_notification_destroy(struct tizen_notification *tizen_notification)
{
	wl_proxy_destroy((struct wl_proxy *) tizen_notification);
}

static inline void
tizen_notification_set_level(struct tizen_notification *tizen_notification, struct wl_surface *surface, uint32_t level)
{
	wl_proxy_marshal((struct wl_proxy *) tizen_notification,
			 TIZEN_NOTIFICATION_SET_LEVEL, surface, level);
}

#ifdef  __cplusplus
}
#endif

#endif
