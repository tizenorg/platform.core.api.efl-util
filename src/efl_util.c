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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Elementary.h>

#if X11
#include <Ecore_X.h>
#include <utilX.h>
#endif

#if WAYLAND
#include <Ecore_Wayland.h>
#include <wayland-client.h>
#include "tizen_notification-client-protocol.h"
#endif

typedef struct _notification_error_cb_info
{
   Evas_Object *window;
   efl_util_notification_window_level_error_cb err_cb;
   void *user_data;
} notification_error_cb_info;

Eina_List *_g_notification_error_cb_info_list;
static Ecore_Event_Handler* _noti_level_access_result_handler = NULL;
static int _noti_handler_count = 0;

static notification_error_cb_info *_notification_error_cb_info_find(Evas_Object *window);
static Eina_Bool _efl_util_notification_info_add(Evas_Object *window, efl_util_notification_window_level_error_cb callback, void *user_data);
static Eina_Bool _efl_util_notification_info_del(Evas_Object *window);

#if X11
static unsigned int _noti_level_access_result_atom = 0;

static Eina_Bool _efl_util_client_message(void *data, int type, void *event);
static notification_error_cb_info *_notification_error_cb_info_find_by_xwin(unsigned int xwin);
#endif

#if WAYLAND
typedef struct _Surface_Level
{
   struct wl_surface *surface;
   int32_t level;
   Eina_Bool wait_set_level_done;
} Surface_Level;

static void _cb_handle_registry_global(void *data, struct wl_registry *registry, unsigned int name, const char *interface, unsigned int version);
static void _cb_handle_registry_global_remove(void *data, struct wl_registry *registry, unsigned int name);
static void _notification_set_level_done(void *data, struct tizen_notification *tizen_notification, struct wl_surface *surface, int32_t level, uint32_t error_state);
static notification_error_cb_info *_notification_error_cb_info_find_by_wl_surface(struct wl_surface *surface);

static const struct wl_registry_listener _registry_listener =
{
   _cb_handle_registry_global,
   _cb_handle_registry_global_remove
};

struct tizen_notification_listener _tizen_notification_listener =
{
   _notification_set_level_done,
};

static struct tizen_notification *_tizen_notification = NULL;
static Eina_Bool _efl_util_init_done = EINA_FALSE;
static Eina_Hash *hash_surface_levels = NULL;

static void
_cb_handle_registry_global(void *data, struct wl_registry *registry, unsigned int name, const char *interface, unsigned int version)
{
   if (!strcmp(interface, "tizen_notification"))
     {
        _tizen_notification = wl_registry_bind(registry, name, &tizen_notification_interface, 1);
        if (!_tizen_notification) return;
        tizen_notification_add_listener(_tizen_notification, &_tizen_notification_listener, NULL);
        _efl_util_init_done = EINA_TRUE;
        hash_surface_levels = eina_hash_pointer_new(free);
     }
}

# define _FREE_FUNC(_h, _fn) do { if (_h) { _fn((void*)_h); _h = NULL; } } while (0)
static void
_cb_handle_registry_global_remove(void *data, struct wl_registry *registry, unsigned int name)
{
   _tizen_notification = NULL;
   _efl_util_init_done = EINA_FALSE;
   _FREE_FUNC(hash_surface_levels, eina_hash_free);
   /* no-op */
}

static void
_notification_set_level_done(void *data,
                             struct tizen_notification *tizen_notification,
                             struct wl_surface *surface,
                             int32_t level,
                             uint32_t error_state)
{
   Surface_Level *sl;
   notification_error_cb_info *cb_info = NULL;
   efl_util_error_e error_cb_state = EFL_UTIL_ERROR_NONE;

   if (hash_surface_levels)
     {
        sl = eina_hash_find(hash_surface_levels, &surface);
        if (sl)
          {
             sl->level = level;
             sl->wait_set_level_done = EINA_FALSE;
          }
     }

   cb_info = _notification_error_cb_info_find_by_wl_surface(surface);
   if (cb_info)
     {
        switch (error_state)
          {
             case TIZEN_NOTIFICATION_ERROR_STATE_NONE:
                error_cb_state = EFL_UTIL_ERROR_NONE;
                break;
             case TIZEN_NOTIFICATION_ERROR_STATE_PERMISSION_DENIED:
             default:
                error_cb_state = EFL_UTIL_ERROR_PERMISSION_DENIED;
                break;
          }
        if (cb_info->err_cb)
          cb_info->err_cb(cb_info->window, error_cb_state , cb_info->user_data);
     }
}

static void
_efl_util_wl_init(void)
{
   static Eina_Bool init = EINA_FALSE;
   if (!init)
     {
        wl_registry_add_listener(wl_display_get_registry(ecore_wl_display_get()),
                                 &_registry_listener, NULL);
        init = EINA_TRUE;
     }
   while (!_efl_util_init_done)
     wl_display_dispatch(ecore_wl_display_get());
}
#endif

int
efl_util_set_notification_window_level(Evas_Object *window, efl_util_notification_level_e level)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL((level >= EFL_UTIL_NOTIFICATION_LEVEL_1) &&
                                   (level <= EFL_UTIL_NOTIFICATION_LEVEL_3),
                                   EFL_UTIL_ERROR_INVALID_PARAMETER);

#if X11
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

#if WAYLAND
   Ecore_Wl_Window *wl_win = elm_win_wl_window_get(window);
   if (wl_win)
     {
        _efl_util_wl_init();

        if (hash_surface_levels)
          {
             Surface_Level *sl;
             struct wl_surface *surface = ecore_wl_window_surface_get(wl_win);
             sl = eina_hash_find(hash_surface_levels, &surface);
             if (!sl)
               {
                  sl = calloc(1, sizeof(Surface_Level));
                  if (sl)
                    {
                       sl->surface = surface;
                       sl->level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;
                       sl->wait_set_level_done = EINA_TRUE;
                       eina_hash_add(hash_surface_levels, &surface, sl);
                    }
               }
             else
               {
                  sl->wait_set_level_done = EINA_TRUE;
               }
          }

        //Add notification window type check
        tizen_notification_set_level(_tizen_notification,
                                     ecore_wl_window_surface_get(wl_win),
                                     level);
        return EFL_UTIL_ERROR_NONE;
     }
#endif

   return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
}

int
efl_util_get_notification_window_level(Evas_Object *window, efl_util_notification_level_e *level)
{

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_NULL_RETURN_VAL(level, EFL_UTIL_ERROR_INVALID_PARAMETER);

#if X11
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
                  *level = EFL_UTIL_NOTIFICATION_LEVEL_3;
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

#if WAYLAND
   Ecore_Wl_Window *wl_win = elm_win_wl_window_get(window);
   if (wl_win)
     {
        Surface_Level *sl;
        struct wl_surface *surface = ecore_wl_window_surface_get(wl_win);

        sl = eina_hash_find(hash_surface_levels, &surface);
        if (sl)
          {
            if (sl->wait_set_level_done)
              {
                  if (ecore_wl_window_shell_surface_get(wl_win) ||
                      ecore_wl_window_xdg_surface_get(wl_win))
                    {
                       while (sl->wait_set_level_done)
                         {
                            ecore_wl_flush();
                            wl_display_dispatch(ecore_wl_display_get());
                         }
                    }
                  else
                    {
                       *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;
                       return EFL_UTIL_ERROR_INVALID_PARAMETER;
                    }
              }

            switch (sl->level)
              {
                 case TIZEN_NOTIFICATION_LEVEL_1:
                   *level = EFL_UTIL_NOTIFICATION_LEVEL_1;
                   break;
                 case TIZEN_NOTIFICATION_LEVEL_2:
                   *level = EFL_UTIL_NOTIFICATION_LEVEL_2;
                   break;
                 case TIZEN_NOTIFICATION_LEVEL_3:
                   *level = EFL_UTIL_NOTIFICATION_LEVEL_3;
                   break;
                 case TIZEN_NOTIFICATION_LEVEL_NONE:
                   *level = EFL_UTIL_NOTIFICATION_LEVEL_NONE;
                   break;
                 case TIZEN_NOTIFICATION_LEVEL_DEFAULT:
                   *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;
                   break;
                 case TIZEN_NOTIFICATION_LEVEL_MEDIUM:
                   *level = EFL_UTIL_NOTIFICATION_LEVEL_MEDIUM;
                   break;
                 case TIZEN_NOTIFICATION_LEVEL_HIGH:
                   *level = EFL_UTIL_NOTIFICATION_LEVEL_HIGH;
                   break;
                 case TIZEN_NOTIFICATION_LEVEL_TOP:
                   *level = EFL_UTIL_NOTIFICATION_LEVEL_TOP;
                 default:
                   *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;
                   return EFL_UTIL_ERROR_INVALID_PARAMETER;
              }
            return EFL_UTIL_ERROR_NONE;
          }
        else
          {
             *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;
          }
     }
#endif
   return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
}

int
efl_util_set_notification_window_level_error_cb(Evas_Object *window, efl_util_notification_window_level_error_cb callback, void *user_data)
{
   Eina_Bool ret = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_NULL_RETURN_VAL(callback, EFL_UTIL_ERROR_INVALID_PARAMETER);

   ret = _efl_util_notification_info_add(window, callback, user_data);
   if (ret)
     {
#if X11
        if (!_noti_level_access_result_atom)
          _noti_level_access_result_atom = ecore_x_atom_get("_E_NOTIFICATION_LEVEL_ACCESS_RESULT");

        if (!_noti_level_access_result_handler)
          _noti_level_access_result_handler = ecore_event_handler_add(ECORE_X_EVENT_CLIENT_MESSAGE, _efl_util_client_message, NULL);
        _noti_handler_count++;

        return EFL_UTIL_ERROR_NONE;
#endif

#if WAYLAND
        return EFL_UTIL_ERROR_NONE;
#endif
     }
   return EFL_UTIL_ERROR_OUT_OF_MEMORY;
}

int
efl_util_unset_notification_window_level_error_cb(Evas_Object *window)
{
   Eina_Bool ret = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);

   ret = _efl_util_notification_info_del(window);
   if (ret)
     {
        _noti_handler_count--;
        if (_noti_handler_count == 0)
          {
             if (_noti_level_access_result_handler)
               {
                  ecore_event_handler_del(_noti_level_access_result_handler);
                  _noti_level_access_result_handler = NULL;
               }
          }
        return EFL_UTIL_ERROR_NONE;
     }

   return EFL_UTIL_ERROR_INVALID_PARAMETER;
}

#if X11
static Eina_Bool
_efl_util_client_message(void *data, int type, void *event)
{
   Ecore_X_Event_Client_Message *ev;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   if (ev->message_type == _noti_level_access_result_atom)
     {
        Ecore_X_Window xwin;
        xwin = ev->win;

        notification_error_cb_info *cb_info = NULL;
        cb_info = _notification_error_cb_info_find_by_xwin(xwin);
        if (cb_info)
          {
             int access = ev->data.l[1];
             if (access == 0) // permission denied
               {
                  if (cb_info->err_cb)
                    {
                       cb_info->err_cb(cb_info->window, EFL_UTIL_ERROR_PERMISSION_DENIED, cb_info->user_data);
                    }
               }
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static notification_error_cb_info *
_notification_error_cb_info_find_by_xwin(unsigned int xwin)
{
   Eina_List *l;
   notification_error_cb_info* temp;
   unsigned int temp_xwin;

   EINA_LIST_FOREACH(_g_notification_error_cb_info_list, l, temp)
     {
        if (temp->window)
          {
             temp_xwin = elm_win_xwindow_get(temp->window);
             if (xwin == temp_xwin)
               {
                  return temp;
               }
          }
     }

   return NULL;
}
#endif

#if WAYLAND
static notification_error_cb_info *
_notification_error_cb_info_find_by_wl_surface(struct wl_surface *surface)
{
   Eina_List *l;
   notification_error_cb_info* temp;
   struct wl_surface *temp_surface;

   EINA_LIST_FOREACH(_g_notification_error_cb_info_list, l, temp)
     {
        if (temp->window)
          {
             temp_surface = ecore_wl_window_surface_get(elm_win_wl_window_get(temp->window));
             if (surface == temp_surface)
               {
                  return temp;
               }
          }
     }

   return NULL;
}
#endif

static notification_error_cb_info *
_notification_error_cb_info_find(Evas_Object *window)
{
   Eina_List *l;
   notification_error_cb_info* temp;

   EINA_LIST_FOREACH(_g_notification_error_cb_info_list, l, temp)
     {
        if (temp->window == window)
          {
             return temp;
          }
     }

   return NULL;
}

static Eina_Bool
_efl_util_notification_info_add(Evas_Object *window, efl_util_notification_window_level_error_cb callback, void *user_data)
{
   notification_error_cb_info* _err_info = _notification_error_cb_info_find(window);

   if (_err_info)
     {
        _g_notification_error_cb_info_list = eina_list_remove(_g_notification_error_cb_info_list, _err_info);
        free(_err_info);
        _err_info = NULL;
     }

   _err_info = (notification_error_cb_info*)calloc(1, sizeof(notification_error_cb_info));
   if (!_err_info)
     {
        return EINA_FALSE;
     }
   _err_info->window = window;
   _err_info->err_cb = callback;
   _err_info->user_data = user_data;

   _g_notification_error_cb_info_list = eina_list_append(_g_notification_error_cb_info_list, _err_info);

   return EINA_TRUE;
}

static Eina_Bool
_efl_util_notification_info_del(Evas_Object *window)
{
   notification_error_cb_info* _err_info = _notification_error_cb_info_find(window);
   if (!_err_info)
     {
        return EINA_FALSE;
     }

   _g_notification_error_cb_info_list = eina_list_remove(_g_notification_error_cb_info_list, _err_info);
   free(_err_info);

   return EINA_TRUE;
}
