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


#define LOG_TAG "TIZEN_N_EFL_UTIL"

#include <efl_util.h>
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Elementary.h>

#if ECORE_X_FOUND
#include <Ecore_X.h>
#include <utilX.h>
#endif

int efl_util_set_notification_window_level (Evas_Object* window, efl_util_notification_level_e level)
{
	EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);
	EINA_SAFETY_ON_FALSE_RETURN_VAL(level >= EFL_UTIL_NOTIFICATION_LEVEL_1 &&
					level <= EFL_UTIL_NOTIFICATION_LEVEL_2,
					EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);

#if ECORE_X_FOUND
	Ecore_X_Window xwin = elm_win_xwindow_get(window);
	if (xwin)
	{
		Ecore_X_Window_Type window_type;
		if(ecore_x_netwm_window_type_get(xwin, &window_type) == EINA_TRUE)
		{
			// success to get window type
			if(window_type != ECORE_X_WINDOW_TYPE_NOTIFICATION)
			{
				// given EFL window's type is not notification type.
				return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
			}
		}
		else
			return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;

		utilx_set_system_notification_level(ecore_x_display_get(), xwin,
						    level);
		return EFL_UTIL_ERROR_NONE;
        }
#endif

#if ECORE_WAYLAND_FOUND
	Ecore_Wl_Window wl_win = elm_win_wl_window_get(window);
	if (wl_win)
	{
		printf("not implemented for wayland yet\n");
		return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
        }
#endif

       return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
}



int efl_util_get_notification_window_level (Evas_Object* window, efl_util_notification_level_e* level)
{
	
	EINA_SAFETY_ON_NULL_RETURN_VAL(window,
				       EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);

#if ECORE_X_FOUND
	Ecore_X_Window_Type window_type;
	Utilx_Notification_Level utilx_level;
	Ecore_X_Window xwin = elm_win_xwindow_get(window);
	if (xwin)
	{
		if(ecore_x_netwm_window_type_get(xwin, &window_type) == EINA_TRUE)
		{
			// success to get window type
			if(window_type != ECORE_X_WINDOW_TYPE_NOTIFICATION)
			{
				// given EFL window's type is not notification type.
				return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
			}
		
			utilx_level = utilx_get_system_notification_level (ecore_x_display_get(), xwin);

			if(utilx_level == UTILX_NOTIFICATION_LEVEL_LOW)
			{
				*level = EFL_UTIL_NOTIFICATION_LEVEL_1;
			}
			else if(utilx_level == UTILX_NOTIFICATION_LEVEL_NORMAL)
			{
				*level = EFL_UTIL_NOTIFICATION_LEVEL_2;
			}
			else if(utilx_level == UTILX_NOTIFICATION_LEVEL_HIGH)
			{
				*level = EFL_UTIL_NOTIFICATION_LEVEL_2;
			}
			else
			{
				return EFL_UTIL_ERROR_INVALID_PARAMETER;
			}
		
		}
		else
		{
			// fail to get window type
			return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
		}
	
		return EFL_UTIL_ERROR_NONE;
	}
#endif

#if ECORE_WAYLAND_FOUND
	Ecore_Wl_Window wl_win = elm_win_wl_window_get(window);
	if (wl_win)
	{
		printf("not implemented for wayland yet\n");
		return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
        }
#endif
	return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
}

int efl_util_grab_key (Evas_Object *obj, const char* key, int grab_mode)
{
	EINA_SAFETY_ON_NULL_RETURN_VAL(obj, 0);

#if ECORE_X_FOUND
	Ecore_X_Window x_win;
	Ecore_X_Display *x_disp;

	x_win = elm_win_xwindow_get(obj);
	if (x_win)
	{
		x_disp = ecore_x_display_get();
		return utilx_grab_key(x_disp, x_win, key, grab_mode);
	}

#endif

#if ECORE_WAYLAND_FOUND
	Ecore_Wl_Window wl_win = elm_win_wl_window_get(obj);
	if (wl_win)
	{
		printf("not implemented for wayland yet\n");
		return 0;
	}
#endif

	return 0;
}

int efl_util_ungrab_key (Evas_Object *obj, const char* key)
{
	EINA_SAFETY_ON_NULL_RETURN_VAL(obj, 0);

#if ECORE_X_FOUND
	Ecore_X_Window x_win;
	Ecore_X_Display *x_disp;

	x_win = elm_win_xwindow_get(obj);
	if (x_win)
	{
		x_disp = ecore_x_display_get();
		return utilx_ungrab_key (x_disp, x_win, key);
	}
#endif

#if ECORE_WAYLAND_FOUND
	Ecore_Wl_Window wl_win = elm_win_wl_window_get(obj);
	if (wl_win)
	{
		printf("not implemented for wayland yet\n");
		return 0;
	}
#endif

	return 0;
}

void efl_util_set_system_notification_level (Evas_Object *obj, Efl_Util_Notification_Level level)
{
	EINA_SAFETY_ON_NULL_RETURN(obj);
	EINA_SAFETY_ON_FALSE_RETURN(level >= EFL_UTIL_NOTIFICATION_LEVEL_LOW &&
				    level <= EFL_UTIL_NOTIFICATION_LEVEL_UNKNOWN);

#if ECORE_X_FOUND
	Ecore_X_Window x_win;
	Ecore_X_Display *x_disp;

	x_win = elm_win_xwindow_get(obj);
	if (x_win)
	{
		x_disp = ecore_x_display_get();
		utilx_set_system_notification_level(x_disp, x_win, level);
		return;
	}
#endif

#if ECORE_WAYLAND_FOUND
	Ecore_Wl_Window wl_win = elm_win_wl_window_get(obj);
	if (wl_win)
	{
		printf("not implemented for wayland yet\n");
	}
#endif
}

Efl_Util_Notification_Level efl_util_get_system_notification_level (Evas_Object *obj)
{
	EINA_SAFETY_ON_NULL_RETURN_VAL(obj, EFL_UTIL_NOTIFICATION_LEVEL_UNKNOWN);

#if ECORE_X_FOUND
	Ecore_X_Window x_win;
	Ecore_X_Display *x_disp;

	x_win = elm_win_xwindow_get(obj);
	if (x_win)
	{
		x_disp = ecore_x_display_get();
		return utilx_get_system_notification_level(x_disp, x_win);
	}

#endif

#if ECORE_WAYLAND_FOUND
	Ecore_Wl_Window wl_win = elm_win_wl_window_get(obj);
	if (wl_win)
	{
		printf("not implemented for wayland yet\n");
		return EFL_UTIL_NOTIFICATION_LEVEL_UNKNOWN;
	}
#endif

	return EFL_UTIL_NOTIFICATION_LEVEL_UNKNOWN;
}

void efl_util_netwm_window_type_set(Evas_Object *obj, Efl_Util_Window_Type type)
{
	EINA_SAFETY_ON_NULL_RETURN(obj);
	EINA_SAFETY_ON_FALSE_RETURN(type == EFL_UTIL_WINDOW_TYPE_NORMAL ||
				    type == EFL_UTIL_WINDOW_TYPE_NOTIFICATION);

#if ECORE_X_FOUND
	Ecore_X_Window x_win;

	x_win = elm_win_xwindow_get(obj);
	if (x_win)
	{
		ecore_x_netwm_window_type_set(x_win, type);
		return;
	}

#endif

#if ECORE_WAYLAND_FOUND
	Ecore_Wl_Window wl_win = elm_win_wl_window_get(obj);
	if (wl_win)
		printf("not implemented for wayland yet\n");
#endif
}

void efl_util_set_window_effect_style(Evas_Object *win, Efl_Util_Effect_Type type, Efl_Util_Effect_Style style)
{

	EINA_SAFETY_ON_NULL_RETURN(win);

#if ECORE_X_FOUND
	Ecore_X_Window x_win;
	Ecore_X_Display *x_disp;

	x_win = elm_win_xwindow_get(win);
	if (x_win)
	{
		x_disp = ecore_x_display_get();
		utilx_set_window_effect_style(x_disp, x_win, type, style);
		return;
	}
#endif

#if ECORE_WAYLAND_FOUND
	Ecore_Wl_Window wl_win = elm_win_wl_window_get(win);
	if (wl_win)
		printf("not implemented for wayland yet\n");
#endif
}

int efl_util_set_window_opaque_state (Evas_Object *win, Efl_Util_Opaque_State state)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(win, 0);

#if ECORE_X_FOUND
	Ecore_X_Window x_win;
	Ecore_X_Display *x_disp;

	x_win = elm_win_xwindow_get(win);
	if (x_win)
	{
		x_disp = ecore_x_display_get();
		return utilx_set_window_opaque_state(x_disp, x_win, state);
	}
#endif

#if ECORE_WAYLAND_FOUND
	Ecore_Wl_Window wl_win = elm_win_wl_window_get(win);
	if (wl_win)
		printf("not implemented for wayland yet\n");
#endif

	return 0;
}
