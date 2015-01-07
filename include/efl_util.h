/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. 
 */


#ifndef __TIZEN_UI_EFL_UTIL_H__
#define __TIZEN_UI_EFL_UTIL_H__

#include <tizen.h>
#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file efl_util.h
 */

/**
 * @addtogroup CAPI_EFL_UTIL_MODULE
 * @{
 */

// Duplicated from utilX.h, should be moved to somewhere common in the future

#ifndef KEY_VOLUMEUP
#define KEY_VOLUMEUP		"XF86AudioRaiseVolume"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Volume Up' key */
#endif

#ifndef KEY_VOLUMEDOWN
#define KEY_VOLUMEDOWN		"XF86AudioLowerVolume"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Volume Down' key */
#endif

#ifndef KEY_CAMERA
#define KEY_CAMERA		"XF86WebCam"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Half-Press of Camera' key */
#endif

#ifndef KEY_CONFIG
#define KEY_CONFIG		"XF86Pictures"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Full-Press of Camera' key */
#endif

#ifndef KEY_POWER
#define KEY_POWER		"XF86PowerOff"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Power' key */
#endif

#ifndef KEY_PAUSE
#define KEY_PAUSE		"XF86Standby"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Pause' key */
#endif

#ifndef KEY_CANCEL
#define KEY_CANCEL              "Cancel"        /**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Cancel' key */
#endif

// Earjack/BT Headset/Multimedia keys
#ifndef KEY_PLAYCD
#define KEY_PLAYCD		"XF86AudioPlay"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Play Audio' key */
#endif

#ifndef KEY_STOPCD
#define KEY_STOPCD		"XF86AudioStop"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Stop Audio' key */
#endif

#ifndef KEY_PAUSECD
#define KEY_PAUSECD		"XF86AudioPause"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Pause Audio' key */
#endif

#ifndef KEY_NEXTSONG
#define KEY_NEXTSONG		"XF86AudioNext"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Next Song' key */
#endif

#ifndef KEY_PREVIOUSSONG
#define KEY_PREVIOUSSONG	"XF86AudioPrev"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Previous Song' key */
#endif

#ifndef KEY_REWIND
#define KEY_REWIND		"XF86AudioRewind"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Rewind Song' key */
#endif

#ifndef KEY_FASTFORWARD
#define KEY_FASTFORWARD		"XF86AudioForward"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Forward Song' key */
#endif

#ifndef KEY_MEDIA
#define KEY_MEDIA		"XF86AudioMedia"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Media' key */
#endif

// 3-Touch key
#ifndef KEY_SEND
#define KEY_SEND		"XF86Send"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Send' key */
#endif

#ifndef KEY_SELECT
#define KEY_SELECT		"XF86Phone"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Home' key */
#endif

#ifndef KEY_END
#define KEY_END			"XF86Stop"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'End' key */
#endif

// Renamed 3-Touch key
#ifndef KEY_MENU
#define KEY_MENU		"XF86Send"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Send' key */
#endif

#ifndef KEY_HOME
#define KEY_HOME		"XF86Phone"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'Home' key */
#endif

#ifndef KEY_BACK
#define KEY_BACK		"XF86Stop"	/**< this macro means the XKeySym (XServer Key Symbol) corresponds to 'End' key */
#endif

#ifndef OR_EXCLUSIVE_GRAB
#define OR_EXCLUSIVE_GRAB	0xf00000	/**< this means that the client window will always get the grabbed-key exclusively regardless of the position on the window stack but the grab is overridable by the other client window */
#endif

#ifndef EXCLUSIVE_GRAB
#define EXCLUSIVE_GRAB		0x0f0000	/**< this means that the client window will always get the grabbed-key exclusively regardless of the position on the window stack */
#endif

#ifndef TOP_POSITION_GRAB
#define TOP_POSITION_GRAB	0x00f000	/**< this means that the client window will get the grabbed-key only when on the top of the grabbing-window stack */
#endif

#ifndef SHARED_GRAB
#define SHARED_GRAB		0x000f00	/**< this means that the client window will get the grabbed-key together with the other client window(s) */
#endif

#ifndef GRAB_MODE_MASK
#define GRAB_MODE_MASK	0xffff00	/**< this mask will be used for getting the key-grab mode of a client window */
#endif

typedef enum _Efl_Util_Window_Type
{
	EFL_UTIL_WINDOW_TYPE_NORMAL = 8, /**< ecore_x compatible, ECORE_X_WINDOW_TYPE_NORMAL */
	EFL_UTIL_WINDOW_TYPE_NOTIFICATION = 12, /**< ecore_x compatible, ECORE_X_WINDOW_TYPE_NOTIFICATION */
} Efl_Util_Window_Type;

typedef enum _Efl_Util_Notification_Level
{
	EFL_UTIL_NOTIFICATION_LEVEL_LOW, /**< low level notification */
	EFL_UTIL_NOTIFICATION_LEVEL_NORMAL, /**< normal level notification*/
	EFL_UTIL_NOTIFICATION_LEVEL_HIGH, /**< high level notification */
	EFL_UTIL_NOTIFICATION_LEVEL_UNKNOWN
} Efl_Util_Notification_Level;

/**
 * @brief Enumeration for EFL UTIL ERROR.
 * @since_tizen 2.3
 */
typedef enum
{
	EFL_UTIL_ERROR_NONE = TIZEN_ERROR_NONE,	/**< Successful */
	EFL_UTIL_ERROR_INVALID_PARAMETER = TIZEN_ERROR_INVALID_PARAMETER, /**< Invalid parameter */
	EFL_UTIL_ERROR_OUT_OF_MEMORY = TIZEN_ERROR_OUT_OF_MEMORY, /**< Out of memory */
	EFL_UTIL_ERROR_PERMISSION_DENIED = TIZEN_ERROR_PERMISSION_DENIED, /**< Permisson denied */
	EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE = -0x02800000 | 0x01  /**< Window type not supported */
	//EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE = TIZEN_ERROR_EFL_UTIL | 0x01  /**< Window type not supported */
} efl_util_error_e;

// TODO: are we going to have more states than on/off shouldn't we move it to a bool in the API's
typedef enum _Efl_Util_Opaque_State
{
	EFL_UTIL_OPAQUE_STATE_OFF = 0, /**< Transparent state */
	EFL_UTIL_OPAQUE_STATE_ON  = 1, /**< Opaque state */
} Efl_Util_Opaque_State;

/**
 * @brief Enumeration of notification window's priority level.
 * @since_tizen 2.3
 */
typedef enum
{
	EFL_UTIL_NOTIFICATION_LEVEL_1, /**< Default notification level */
	EFL_UTIL_NOTIFICATION_LEVEL_2, /**< Higher notification level than default */
	EFL_UTIL_NOTIFICATION_LEVEL_3, /**< The highest notification level */
} efl_util_notification_level_e; 

typedef enum _Efl_Util_Effect_Type
{
	EFL_UTIL_EFFECT_TYPE_MAP, /**< Effect for Window's Map Notify Event */
	EFL_UTIL_EFFECT_TYPE_UNMAP, /**< Effect for Window's UnMap Notify Event */
	EFL_UTIL_EFFECT_TYPE_RAISEABOVE, /**< Effect for Window's Configure Notify ( RaiseAbove case ) Event */
	EFL_UTIL_EFFECT_TYPE_ROTATION, /**< Effect for Window's Rotation Property Change Notify Event ( X Property: ECORE_X_ATOM_E_ILLUME_ROTATE_WINDOW_ANGLE ) */
	EFL_UTIL_EFFECT_TYPE_FOCUSIN, /**< Effect for Window's FocusIn Event ( E17's Event: E_EVENT_BORDER_FOCUS_IN ) */
	EFL_UTIL_EFFECT_TYPE_FOCUSOUT /**< Effect for Window's FocusOut Event ( E17's Event : E_EVENT_BORDER_FOCUS_OUT ) */
} Efl_Util_Effect_Type;

typedef enum _Efl_Util_Effect_Style
{
	EFL_UTIL_EFFECT_STYLE_DEFAULT, /**< Default Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_NONE, /**< None of Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_CUSTOM0, /**< Custom0 Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_CUSTOM1, /**< Custom1 Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_CUSTOM2, /**< Custom2 Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_CUSTOM3, /**< Custom3 Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_CUSTOM4, /**< Custom4 Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_CUSTOM5, /**< Custom5 Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_CUSTOM6, /**< Custom6 Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_CUSTOM7, /**< Custom7 Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_CUSTOM8, /**< Custom8 Effect Style for Effect Type */
	EFL_UTIL_EFFECT_STYLE_CUSTOM9 /**< Custom9 Effect Style for Effect Type */
} Efl_Util_Effect_Style;

/**
 * @brief Sets the priority level for the specified notification window, asynchronously.
 * @since_tizen 2.3
 * @privlevel public
 * @privilege %http://tizen.org/privilege/window.priority.set
 * @remarks This API can be used for a notification type window only.
 * @param[in] window The EFL window
 * @param[in] level The notification window level
 * @return @c 0 on success,
 *         otherwise a negative error value
 * @retval #EFL_UTIL_ERROR_NONE Successful
 * @retval #EFL_UTIL_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE Window type not supported
 */
int efl_util_set_notification_window_level (Evas_Object *window, efl_util_notification_level_e level);


/**
 * @brief Gets the priority level for the specified notification window, asynchronously.
 * @since_tizen 2.3
 *
 * @remarks This API can be used for a notification type window only.
 * @param[in] window The EFL window
 * @param[out] level The notification window level
 * @return @c 0 on success,
 *         otherwise a negative error value
 * @retval #EFL_UTIL_ERROR_NONE Successful
 * @retval #EFL_UTIL_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE Window type not supported
 */
int efl_util_get_notification_window_level (Evas_Object *window, efl_util_notification_level_e* level);


/**
 * @brief Called when an error occurs for setting notification window level
 * @since_tizen 2.3
 * @param[in]	window	The EFL window
 * @param[in]	error_code	The error code (#EFL_UTIL_ERROR_PERMISSION_DENIED)
 * @param[in]	user_data	The user data passed from the callback registration function
 * @see efl_util_set_notification_window_level_error_cb()
 * @see efl_util_unset_notification_window_level_error_cb()
 */
typedef void (*efl_util_notification_window_level_error_cb)(Evas_Object *window, int error_code, void *user_data);


/**
 * @brief Registers a callback function to be invoked when an error which set the notification level occurs.
 * @since_tizen 2.3
 * @param[in] window	The EFL window
 * @param[in] callback	The callback function to register
 * @param[in] user_data The user data to be passed to the callback function
 * @return @c 0 on success,
 *		   otherwise a negative error value
 * @retval #EFL_UTIL_ERROR_NONE Successful
 * @retval #EFL_UTIL_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #EFL_UTIL_ERROR_OUT_OF_MEMORY Out of memory
 * @post  efl_util_notification_window_level_error_cb() will be invoked.
 * @see efl_util_unset_notification_window_level_error_cb()
 * @see efl_util_notification_window_level_error_cb()
 */
int efl_util_set_notification_window_level_error_cb(Evas_Object *window, efl_util_notification_window_level_error_cb callback, void *user_data);


/**
 * @brief Unregisters the callback function.
 * @since_tizen 2.3
 * @param[in] window The EFL window
 * @return @c 0 on success,
 *		   otherwise a negative error value
 * @retval #EFL_UTIL_ERROR_NONE Successful
 * @retval #EFL_UTIL_ERROR_INVALID_PARAMETER Invalid parameter
 * @see efl_util_set_notification_window_level_error_cb()
 */
int efl_util_unset_notification_window_level_error_cb(Evas_Object *window);


/** 
 * @brief Grabs a key specfied by key_name for obj in grab_mode.
 *
 * @param obj The Evas_Object representing the window to set the key grab to.
 * @param key The key of interest.
 * @param grab_mode EXCLUSIVE_GRAB, TOP_POSITION_GRAB, SHARED_GRAB, EXCLUSIVE_GRAB
 *
 * @return 0 on Success, fail othrewise.
 * @see efl_util_ungrab_key()
 */
int efl_util_grab_key (Evas_Object *obj, const char* key, int grab_mode);

/** 
 * @brief Ungrabs a key specfied by key_name for win
 *
 * @param obj The Evas_Object representing the window to ungrab the key.
 * @param key The key of interest.
 *
 * @return 0 on Success, fail otherwise.
 * @see efl_util_grab_key()
 */
int efl_util_ungrab_key (Evas_Object *obj, const char* key);

/** 
 * @brief Sets the priority level for the specified notification window
 *
 * @param obj The Evas_Object representing the window to set the notification level.
 * @param level The notification level.
 *
 * @see Efl_Util_Notification_Level
 */
void efl_util_set_system_notification_level (Evas_Object *obj, Efl_Util_Notification_Level level);

/** 
 * @brief Gets the priority level for the specified notification window.
 *
 * @param obj The Evas_Object representing the window to get the system notification level
 * set to.
 *
 * @return current notication level (EFL_UTIL_NOTIFICATION_LEVEL_LOW,
 *         EFL_UTIL_NOTIFICATION_LEVEL_NORMAL, EFL_UTIL_NOTIFICATION_LEVEL_HIGH)
 */
Efl_Util_Notification_Level efl_util_get_system_notification_level (Evas_Object *obj);

/** 
 * @brief Set the functional type of window by sending _NET_WM_WINDOW_TYPE property to window.
 *
 * @param obj The Evas_Object representing the window to set the net_wm to.
 * @param type The type to be set.
 */
void efl_util_netwm_window_type_set(Evas_Object *obj, Efl_Util_Window_Type type);

/** 
 * @brief Sets a window's effect style with effect type.
 *
 * @param win The window to set the style to.
 * @param type type Specifies the window's effect type ( ex. EFL_UTIL_EFFECT_TYPE_MAP, EFL_UTIL_EFFECT_TYPE_UNMAP, etc )
 * @param style Specifies  the window's effect style ( ex. EFL_UTIL_EFFECT_STYLE_DEFAULT, EFL_UTIL_EFFECT_STYLE_NONE, EFL_UTIL_EFFECT_STYLE_CUSTOM0, etc )
 */
void efl_util_set_window_effect_style(Evas_Object *win, Efl_Util_Effect_Type type, Efl_Util_Effect_Style style);

/**
 * @brief Sets the window's opaque state
 *
 * @param win The window to set the opaque state to.
 * @param state The state (EFL_UTIL_OPAQUE_STATE_ON, EFL_UTIL_OPAQUE_STATE_OFF).
 *
 * @return 0 on failure
 */
int efl_util_set_window_opaque_state (Evas_Object *win, Efl_Util_Opaque_State state);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif				/* __TIZEN_UI_EFL_UTIL_H__ */
