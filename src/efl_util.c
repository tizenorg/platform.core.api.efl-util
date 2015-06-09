/*
 * Copyright (c) 2011-2015 Samsung Electronics Co., Ltd All Rights Reserved
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
#include <Ecore_Evas.h>

#if X11
#include <Ecore_X.h>
#include <utilX.h>
#endif /* end of X11 */

#if WAYLAND
#include <Ecore_Wayland.h>
#include <wayland-client.h>
#include "tizen_notification-client-protocol.h"
#include "tizen_window_screen-client-protocol.h"
#endif /* end of WAYLAND */

/* callback handler index */
#define CBH_NOTI_LEV 0
#define CBH_SCR_MODE 1
#define CBH_MAX      2

typedef void (*Efl_Util_Cb)(Evas_Object *, int, void *);

typedef struct _Efl_Util_Callback_Info
{
   Evas_Object *win;
   Efl_Util_Cb cb;
   void *data;
} Efl_Util_Callback_Info;

typedef struct _Efl_Util_Wl_Surface_Lv_Info
{
   void *surface; /* wl_surface */
   int level;
   Eina_Bool wait_for_done;
} Efl_Util_Wl_Surface_Lv_Info;

typedef struct _Efl_Util_Wl_Surface_Scr_Mode_Info
{
   void *surface; /* wl_surface */
   unsigned int mode;
   Eina_Bool wait_for_done;
} Efl_Util_Wl_Surface_Scr_Mode_Info;

typedef struct _Efl_Util_Data
{
   /* x11 related stuffs */
   struct
   {
      Eina_Bool init;
      Ecore_Event_Handler *handler; /* x11 client message handler */
      #if X11
      Ecore_X_Display *dpy;
      #endif /* end of X11 */
   } x11;

   struct
   {
      Eina_List *info_list; /* list of callback info */
      unsigned int atom; /* x11 atom */
   } cb_handler[CBH_MAX];

   /* wayland related stuffs */
   struct
   {
      Eina_Bool init;
      #if WAYLAND
      struct wl_display *dpy;
      struct
      {
         struct tizen_notification *proto;
         Eina_Hash *hash;
      } noti_lv;
      struct
      {
         struct tizen_window_screen *proto;
         Eina_Hash *hash;
      } scr_mode;
      #endif /* end of WAYLAND */
   } wl;
} Efl_Util_Data;

static Efl_Util_Data _eflutil =
{
   {
      EINA_FALSE,
      NULL,
      #if X11
      NULL
      #endif /* end of X11 */
   },
   {
      { NULL, 0 }, /* handler for notification level */
      { NULL, 0 }  /* handler for screen mode */
   },
   {
      EINA_FALSE,
      #if WAYLAND
      NULL,
      { NULL, NULL }, /* tizen_notification protocol */
      { NULL, NULL }  /* tizen_window_screen protocol */
      #endif /* end of WAYLAND */
   }
};

static Eina_Bool               _cb_info_add(Evas_Object *win, Efl_Util_Cb cb, void *data, int idx);
static Eina_Bool               _cb_info_del_by_win(Evas_Object *win, int idx);
static Eina_List              *_cb_info_list_get(int idx);
static Efl_Util_Callback_Info *_cb_info_find_by_win(Evas_Object *win, int idx);
#if X11
static Efl_Util_Callback_Info *_cb_info_find_by_xwin(unsigned int xwin);
static Eina_Bool               _cb_x11_client_msg(void *data, int type, void *event);
static Eina_Bool               _x11_init(void);
#endif /* end of X11 */
#if WAYLAND
static Eina_Bool               _wl_init(void);
static void                    _cb_wl_reg_global(void *data, struct wl_registry *reg, unsigned int name, const char *interface, unsigned int version);
static void                    _cb_wl_reg_global_remove(void *data, struct wl_registry *reg, unsigned int name);
static Efl_Util_Callback_Info *_cb_info_find_by_wlsurf(void *wlsurf, int idx);
static void                    _cb_wl_tz_noti_lv_done(void *data, struct tizen_notification *proto, struct wl_surface *surface, int32_t level, uint32_t state);
static void                    _cb_wl_tz_scr_mode_done(void *data, struct tizen_window_screen *proto, struct wl_surface *surface, uint32_t mode, uint32_t state);

static const struct wl_registry_listener _wl_reg_listener =
{
   _cb_wl_reg_global,
   _cb_wl_reg_global_remove
};

struct tizen_notification_listener _wl_tz_noti_lv_listener =
{
   _cb_wl_tz_noti_lv_done
};

struct tizen_window_screen_listener _wl_tz_scr_mode_listener =
{
   _cb_wl_tz_scr_mode_done
};
#endif /* end of WAYLAND */

static Eina_Bool
_cb_info_add(Evas_Object *win,
             Efl_Util_Cb cb,
             void *data,
             int idx)
{
   Efl_Util_Callback_Info *info;

   info = _cb_info_find_by_win(win, idx);
   if (info)
     {
        _eflutil.cb_handler[idx].info_list
           = eina_list_remove(_eflutil.cb_handler[idx].info_list,
                              info);
        free(info);
     }

   info = (Efl_Util_Callback_Info *)calloc(1, sizeof(Efl_Util_Callback_Info));
   if (!info) return EINA_FALSE;

   info->win = win;
   info->cb = cb;
   info->data = data;

   _eflutil.cb_handler[idx].info_list
      = eina_list_append(_eflutil.cb_handler[idx].info_list,
                         info);

#if X11
   if (!_eflutil.x11.handler)
     _eflutil.x11.handler = ecore_event_handler_add(ECORE_X_EVENT_CLIENT_MESSAGE,
                                                    _cb_x11_client_msg,
                                                    NULL);
#endif /* end of X11 */

   return EINA_TRUE;
}

static Eina_Bool
_cb_info_del_by_win(Evas_Object *win,
                    int idx)
{
   Efl_Util_Callback_Info *info;
   unsigned int count;

   info = _cb_info_find_by_win(win, idx);
   if (!info) return EINA_FALSE;

   _eflutil.cb_handler[idx].info_list
      = eina_list_remove(_eflutil.cb_handler[idx].info_list,
                         info);
   free(info);

   count = eina_list_count(_eflutil.cb_handler[idx].info_list);
   if ((count == 0) && (_eflutil.x11.handler))
     {
        ecore_event_handler_del(_eflutil.x11.handler);
        _eflutil.x11.handler = NULL;
     }

   return EINA_TRUE;
}

static Eina_List *
_cb_info_list_get(int idx)
{
   return _eflutil.cb_handler[idx].info_list;
}

static Efl_Util_Callback_Info *
_cb_info_find_by_win(Evas_Object *win,
                     int idx)
{
   Eina_List *l, *ll;
   Efl_Util_Callback_Info *info;

   l = _cb_info_list_get(idx);
   EINA_LIST_FOREACH(l, ll, info)
     {
        if (info->win == win) return info;
     }

   return NULL;
}

#if X11
static Efl_Util_Callback_Info *
_cb_info_find_by_xwin(unsigned int xwin,
                      int idx)
{
   Eina_List *l, *ll;
   Efl_Util_Callback_Info *info;
   unsigned int xwin2;

   l = _cb_info_list_get(idx);
   EINA_LIST_FOREACH(l, ll, info)
     {
        xwin2 = elm_win_xwindow_get(info->win);
        if (xwin == xwin2) return info;
     }

   return NULL;
}

static Eina_Bool
_cb_x11_client_msg(void *data,
                   int type,
                   void *event)
{
   Ecore_X_Event_Client_Message *ev;
   Ecore_X_Window xwin;
   Efl_Util_Callback_Info *info;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   xwin = ev->win;
   if (xwin == 0) return ECORE_CALLBACK_PASS_ON;

   if (ev->message_type == _eflutil.atom.noti_lv)
     {
        info = _cb_info_find_by_xwin(xwin, CBH_NOTI_LEV);

        /* permission denied */
        if ((ev->data.l[1] == 0) &&
            (info) &&
            (info->cb))
          {
             info->cb(info->win,
                      EFL_UTIL_ERROR_PERMISSION_DENIED,
                      info->data);
          }
     }
   else if (ev->message_type == _eflutil.atom.scr_mode)
     {
        info = _cb_info_find_by_xwin(xwin, CBH_SCR_MODE);

        /* permission denied */
        if ((ev->data.l[1] == 0) &&
            (info) &&
            (info->cb))
          {
             info->cb(info->win,
                      EFL_UTIL_ERROR_PERMISSION_DENIED,
                      info->data);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_x11_init(void)
{
   if (_eflutil.x11.init) return EINA_TRUE;

   _eflutil.x11.dpy = ecore_x_display_get();
   EINA_SAFETY_ON_NULL_RETURN_VAL(_eflutil.x11.dpy, EINA_FALSE);

   _eflutil.x11.init = EINA_TRUE;

   return EINA_TRUE;
}
#endif /* end of X11 */

#if WAYLAND
static Eina_Bool
_wl_init(void)
{
   struct wl_registry *reg;

   if (_eflutil.wl.init) return EINA_TRUE;

   _eflutil.wl.dpy = ecore_wl_display_get();
   EINA_SAFETY_ON_NULL_RETURN_VAL(_eflutil.wl.dpy, EINA_FALSE);

   reg = wl_display_get_registry(_eflutil.wl.dpy);
   EINA_SAFETY_ON_NULL_RETURN_VAL(reg, EINA_FALSE);

   wl_registry_add_listener(reg, &_wl_reg_listener, NULL);

   _eflutil.wl.init = EINA_TRUE;

   return EINA_TRUE;
}

static void
_cb_wl_reg_global(void *data,
                  struct wl_registry *reg,
                  unsigned int name,
                  const char *interface,
                  unsigned int version)
{
   if (!strcmp(interface, "tizen_notification"))
     {
        struct tizen_notification *proto;
        proto = wl_registry_bind(reg,
                                 name,
                                 &tizen_notification_interface,
                                 1);
        if (!proto) return;

        tizen_notification_add_listener(proto,
                                        &_wl_tz_noti_lv_listener,
                                        NULL);

        _eflutil.wl.noti_lv.hash = eina_hash_pointer_new(free);
        _eflutil.wl.noti_lv.proto = proto;
     }
   else if (!strcmp(interface, "tizen_window_screen"))
     {
        struct tizen_window_screen *proto;
        proto = wl_registry_bind(reg,
                                 name,
                                 &tizen_window_screen_interface,
                                 1);
        if (!proto) return;

        tizen_window_screen_add_listener(proto,
                                         &_wl_tz_scr_mode_listener,
                                         NULL);

        _eflutil.wl.scr_mode.hash = eina_hash_pointer_new(free);
        _eflutil.wl.scr_mode.proto = proto;
     }
}

static void
_cb_wl_reg_global_remove(void *data,
                         struct wl_registry *reg,
                         unsigned int name)
{
   _eflutil.wl.noti_lv.proto = NULL;
   eina_hash_free(_eflutil.wl.noti_lv.hash);
   _eflutil.wl.scr_mode.proto = NULL;
   eina_hash_free(_eflutil.wl.scr_mode.hash);
}

static Efl_Util_Callback_Info *
_cb_info_find_by_wlsurf(void *wlsurf,
                        int idx)
{
   Eina_List *l, *ll;
   Efl_Util_Callback_Info *info;
   Ecore_Wl_Window *wlwin2 = NULL;
   void *wlsurf2 = NULL;

   l = _cb_info_list_get(idx);
   EINA_LIST_FOREACH(l, ll, info)
     {
        wlwin2 = elm_win_wl_window_get(info->win);
        wlsurf2 = ecore_wl_window_surface_get(wlwin2);
        if (wlsurf== wlsurf2) return info;
     }

   return NULL;
}

static void
_cb_wl_tz_noti_lv_done(void *data,
                       struct tizen_notification *proto,
                       struct wl_surface *surface,
                       int32_t level,
                       uint32_t state)
{
   Efl_Util_Wl_Surface_Lv_Info *lv_info;
   Efl_Util_Callback_Info *cb_info;

   lv_info = eina_hash_find(_eflutil.wl.noti_lv.hash, &surface);
   if (lv_info)
     {
        lv_info->level = level;
        lv_info->wait_for_done = EINA_FALSE;
     }

   if (state != TIZEN_NOTIFICATION_ERROR_STATE_PERMISSION_DENIED) return;

   cb_info = _cb_info_find_by_wlsurf((void *)surface, CBH_NOTI_LEV);
   if (!cb_info) return;
   if (!cb_info->cb) return;

   cb_info->cb(cb_info->win,
               EFL_UTIL_ERROR_PERMISSION_DENIED,
               cb_info->data);
}

static void
_cb_wl_tz_scr_mode_done(void *data,
                        struct tizen_window_screen *proto,
                        struct wl_surface *surface,
                        uint32_t mode,
                        uint32_t state)
{

   Efl_Util_Wl_Surface_Scr_Mode_Info *scr_mode_info;
   Efl_Util_Callback_Info *cb_info;

   scr_mode_info = eina_hash_find(_eflutil.wl.scr_mode.hash, &surface);
   if (scr_mode_info)
     {
        scr_mode_info->mode = mode;
        scr_mode_info->wait_for_done = EINA_FALSE;
     }

   if (state != TIZEN_WINDOW_SCREEN_ERROR_STATE_PERMISSION_DENIED) return;

   cb_info = _cb_info_find_by_wlsurf((void *)surface, CBH_SCR_MODE);
   if (!cb_info) return;
   if (!cb_info->cb) return;

   cb_info->cb(cb_info->win,
               EFL_UTIL_ERROR_PERMISSION_DENIED,
               cb_info->data);
}
#endif /* end of WAYLAND */

API int
efl_util_set_notification_window_level(Evas_Object *window,
                                       efl_util_notification_level_e level)
{
   Eina_Bool res;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL((level >= EFL_UTIL_NOTIFICATION_LEVEL_1) &&
                                   (level <= EFL_UTIL_NOTIFICATION_LEVEL_TOP),
                                   EFL_UTIL_ERROR_INVALID_PARAMETER);

#if X11
   Ecore_X_Window_Type window_type;
   Ecore_X_Window xwin;

   res = _x11_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   xwin = elm_win_xwindow_get(window);
   if (xwin)
     {
        if (ecore_x_netwm_window_type_get(xwin, &window_type) == EINA_TRUE)
          {
             // success to get window type
             if (window_type != ECORE_X_WINDOW_TYPE_NOTIFICATION)
               {
                  // given EFL window's type is not notification type.
                  return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
               }
          }
        else
          return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;

        utilx_set_system_notification_level(_eflutil.x11.dpy,
                                            xwin,
                                            level);
        return EFL_UTIL_ERROR_NONE;
     }
#endif /* end of X11 */

#if WAYLAND
   Elm_Win_Type type;
   Ecore_Wl_Window *wlwin;
   struct wl_surface *surface;
   Efl_Util_Wl_Surface_Lv_Info *lv_info;

   res = _wl_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   type = elm_win_type_get(window);
   EINA_SAFETY_ON_FALSE_RETURN_VAL((type == ELM_WIN_NOTIFICATION),
                                   EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);

   wlwin = elm_win_wl_window_get(window);
   if (wlwin)
     {
        while (!_eflutil.wl.noti_lv.proto)
          wl_display_dispatch(_eflutil.wl.dpy);

        surface = ecore_wl_window_surface_get(wlwin);
        EINA_SAFETY_ON_NULL_RETURN_VAL(surface,
                                       EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);

        lv_info = eina_hash_find(_eflutil.wl.noti_lv.hash, &surface);
        if (!lv_info)
          {
             lv_info = calloc(1, sizeof(Efl_Util_Wl_Surface_Lv_Info));
             EINA_SAFETY_ON_NULL_RETURN_VAL(lv_info, EFL_UTIL_ERROR_OUT_OF_MEMORY);

             lv_info->surface = surface;
             lv_info->level = (int)level;
             lv_info->wait_for_done = EINA_TRUE;

             eina_hash_add(_eflutil.wl.noti_lv.hash,
                           &surface,
                           lv_info);
          }
        else
          {
             lv_info->level = (int)level;
             lv_info->wait_for_done = EINA_TRUE;
          }

        tizen_notification_set_level(_eflutil.wl.noti_lv.proto,
                                     surface,
                                     (int)level);

        return EFL_UTIL_ERROR_NONE;
     }
#endif /* end of WAYLAND */

   return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
}

API int
efl_util_get_notification_window_level(Evas_Object *window,
                                       efl_util_notification_level_e *level)
{
   Eina_Bool res;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_NULL_RETURN_VAL(level, EFL_UTIL_ERROR_INVALID_PARAMETER);

#if X11
   Ecore_X_Window_Type window_type;
   Utilx_Notification_Level utilx_level;
   Ecore_X_Window xwin;

   res = _x11_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   xwin = elm_win_xwindow_get(window);
   if (xwin)
     {
        if (ecore_x_netwm_window_type_get(xwin, &window_type) == EINA_TRUE)
          {
             // success to get window type
             if (window_type != ECORE_X_WINDOW_TYPE_NOTIFICATION)
               {
                  // given EFL window's type is not notification type.
                  return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
               }

             utilx_level = utilx_get_system_notification_level(_eflutil.x11.dpy, xwin);
             if (utilx_level == UTILX_NOTIFICATION_LEVEL_LOW)
               *level = EFL_UTIL_NOTIFICATION_LEVEL_1;
             else if(utilx_level == UTILX_NOTIFICATION_LEVEL_NORMAL)
               *level = EFL_UTIL_NOTIFICATION_LEVEL_2;
             else if(utilx_level == UTILX_NOTIFICATION_LEVEL_HIGH)
               *level = EFL_UTIL_NOTIFICATION_LEVEL_3;
             else
               return EFL_UTIL_ERROR_INVALID_PARAMETER;
          }
        else
          return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;

        return EFL_UTIL_ERROR_NONE;
     }
#endif /* end of X11 */

#if WAYLAND
   Elm_Win_Type type;
   Ecore_Wl_Window *wlwin;
   struct wl_surface *surface;
   Efl_Util_Wl_Surface_Lv_Info *lv_info;

   res = _wl_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   type = elm_win_type_get(window);
   EINA_SAFETY_ON_FALSE_RETURN_VAL((type == ELM_WIN_NOTIFICATION),
                                   EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);

   wlwin = elm_win_wl_window_get(window);
   if (wlwin)
     {
        while (!_eflutil.wl.noti_lv.proto)
          wl_display_dispatch(_eflutil.wl.dpy);

        surface = ecore_wl_window_surface_get(wlwin);
        EINA_SAFETY_ON_NULL_RETURN_VAL(surface,
                                       EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);

        lv_info = eina_hash_find(_eflutil.wl.noti_lv.hash, &surface);
        if (lv_info)
          {
             if (lv_info->wait_for_done)
               {
                  if (ecore_wl_window_shell_surface_get(wlwin) ||
                      ecore_wl_window_xdg_surface_get(wlwin))
                    {
                       while (lv_info->wait_for_done)
                         {
                            ecore_wl_flush();
                            wl_display_dispatch(_eflutil.wl.dpy);
                         }
                    }
                  else
                    {
                       *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;
                       return EFL_UTIL_ERROR_INVALID_PARAMETER;
                    }
               }

             switch (lv_info->level)
               {
                case TIZEN_NOTIFICATION_LEVEL_1:       *level = EFL_UTIL_NOTIFICATION_LEVEL_1;       break;
                case TIZEN_NOTIFICATION_LEVEL_2:       *level = EFL_UTIL_NOTIFICATION_LEVEL_2;       break;
                case TIZEN_NOTIFICATION_LEVEL_3:       *level = EFL_UTIL_NOTIFICATION_LEVEL_3;       break;
                case TIZEN_NOTIFICATION_LEVEL_NONE:    *level = EFL_UTIL_NOTIFICATION_LEVEL_NONE;    break;
                case TIZEN_NOTIFICATION_LEVEL_DEFAULT: *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT; break;
                case TIZEN_NOTIFICATION_LEVEL_MEDIUM:  *level = EFL_UTIL_NOTIFICATION_LEVEL_MEDIUM;  break;
                case TIZEN_NOTIFICATION_LEVEL_HIGH:    *level = EFL_UTIL_NOTIFICATION_LEVEL_HIGH;    break;
                case TIZEN_NOTIFICATION_LEVEL_TOP:     *level = EFL_UTIL_NOTIFICATION_LEVEL_TOP;     break;
                default:                               *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;
                                                       return EFL_UTIL_ERROR_INVALID_PARAMETER;
               }
             return EFL_UTIL_ERROR_NONE;
          }
        else
          *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;

        return EFL_UTIL_ERROR_NONE;
     }
#endif /* end of WAYLAND */
   return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
}

API int
efl_util_set_notification_window_level_error_cb(Evas_Object *window,
                                                efl_util_notification_window_level_error_cb callback,
                                                void *user_data)
{
   Eina_Bool ret = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_NULL_RETURN_VAL(callback, EFL_UTIL_ERROR_INVALID_PARAMETER);

   ret = _cb_info_add(window,
                      (Efl_Util_Cb)callback,
                      user_data,
                      CBH_NOTI_LEV);
   if (!ret) return EFL_UTIL_ERROR_OUT_OF_MEMORY;

#if X11
   if (!_eflutil.atom.noti_lv)
     _eflutil.atom.noti_lv = ecore_x_atom_get("_E_NOTIFICATION_LEVEL_ACCESS_RESULT");
#endif /* end of X11 */

   return EFL_UTIL_ERROR_NONE;
}

API int
efl_util_unset_notification_window_level_error_cb(Evas_Object *window)
{
   Eina_Bool ret = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);

   ret = _cb_info_del_by_win(window, CBH_NOTI_LEV);
   if (!ret) return EFL_UTIL_ERROR_INVALID_PARAMETER;

   return EFL_UTIL_ERROR_NONE;
}

API int
efl_util_set_window_opaque_state(Evas_Object *window,
                                 int opaque)
{
   Eina_Bool res;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(((opaque >= 0) && (opaque <= 1)),
                                   EFL_UTIL_ERROR_INVALID_PARAMETER);

#if X11
   Ecore_X_Window xwin;
   Utilx_Opaque_State state;
   int ret;

   res = _x11_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   xwin = elm_win_xwindow_get(window);
   EINA_SAFETY_ON_NULL_RETURN_VAL(xwin, EFL_UTIL_ERROR_INVALID_PARAMETER);

   if (opaque)
     state = UTILX_OPAQUE_STATE_ON;
   else
     state = UTILX_OPAQUE_STATE_OFF;

   ret = utilx_set_window_opaque_state(_eflutil.x11.dpy,
                                       xwin,
                                       state);

   if (!ret)
     return EFL_UTIL_ERROR_INVALID_PARAMETER;
   else
     return EFL_UTIL_ERROR_NONE;
#endif /* end of X11 */

#if WAYLAND
   Ecore_Wl_Window *wlwin;
   int x, y, w, h;

   res = _wl_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   wlwin = elm_win_wl_window_get(window);
   if (wlwin)
     {
        evas_object_geometry_get(window, &x, &y, &w, &h);

        if (opaque)
          ecore_wl_window_opaque_region_set(wlwin, x, y, w, h);
        else
          ecore_wl_window_opaque_region_set(wlwin, 0, 0, 0, 0);

        return EFL_UTIL_ERROR_NONE;
     }

   return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
#endif /* end of WAYLAND */
}

API int
efl_util_set_window_screen_mode(Evas_Object *window,
                                efl_util_screen_mode_e mode)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(((mode >= EFL_UTIL_SCREEN_MODE_DEFAULT) &&
                                    (mode <= EFL_UTIL_SCREEN_MODE_ALWAYS_ON)),
                                   EFL_UTIL_ERROR_INVALID_PARAMETER);

#if X11
   Evas *e;
   Ecore_Evas *ee;
   int id;

   e = evas_object_evas_get(window);
   EINA_SAFETY_ON_NULL_RETURN_VAL(e, EFL_UTIL_ERROR_INVALID_PARAMETER);

   ee = ecore_evas_ecore_evas_get(e);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ee, EFL_UTIL_ERROR_INVALID_PARAMETER);

   id = ecore_evas_aux_hint_id_get(ee, "wm.policy.win.lcd.lock");
   if (mode == EFL_UTIL_SCREEN_MODE_ALWAYS_ON)
     {
        if (id == -1)
          ecore_evas_aux_hint_add(ee, "wm.policy.win.lcd.lock", "1");
        else
          ecore_evas_aux_hint_val_set(ee, id, "1");
     }
   else if (mode == EFL_UTIL_SCREEN_MODE_DEFAULT)
     {
        if (id == -1)
          ecore_evas_aux_hint_add(ee, "wm.policy.win.lcd.lock", "0");
        else
          ecore_evas_aux_hint_val_set(ee, id, "0");
     }
   else
     return EFL_UTIL_ERROR_INVALID_PARAMETER;

   return EFL_UTIL_ERROR_NONE;
#endif /* end of X11 */

#if WAYLAND
   Ecore_Wl_Window *wlwin;
   struct wl_surface *surface;
   Efl_Util_Wl_Surface_Scr_Mode_Info *scr_mode_info;
   Eina_Bool res;

   res = _wl_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   wlwin = elm_win_wl_window_get(window);
   if (wlwin)
     {
        while (!_eflutil.wl.scr_mode.proto)
          wl_display_dispatch(_eflutil.wl.dpy);

        surface = ecore_wl_window_surface_get(wlwin);
        EINA_SAFETY_ON_NULL_RETURN_VAL(surface,
                                       EFL_UTIL_ERROR_INVALID_PARAMETER);

        scr_mode_info = eina_hash_find(_eflutil.wl.scr_mode.hash, &surface);
        if (!scr_mode_info)
          {
             scr_mode_info = calloc(1, sizeof(Efl_Util_Wl_Surface_Scr_Mode_Info));
             EINA_SAFETY_ON_NULL_RETURN_VAL(scr_mode_info, EFL_UTIL_ERROR_OUT_OF_MEMORY);

             scr_mode_info->surface = surface;
             scr_mode_info->mode = (unsigned int)mode;
             scr_mode_info->wait_for_done = EINA_TRUE;

             eina_hash_add(_eflutil.wl.scr_mode.hash,
                           &surface,
                           scr_mode_info);
          }
        else
          {
             scr_mode_info->mode = (unsigned int)mode;
             scr_mode_info->wait_for_done = EINA_TRUE;
          }

        tizen_window_screen_set_mode(_eflutil.wl.scr_mode.proto,
                                     surface,
                                     (unsigned int)mode);

        return EFL_UTIL_ERROR_NONE;
     }
   else
     return EFL_UTIL_ERROR_INVALID_PARAMETER;
#endif /* end of WAYLAND */
}

API int
efl_util_get_window_screen_mode(Evas_Object *window,
                                efl_util_screen_mode_e *mode)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_NULL_RETURN_VAL(mode, EFL_UTIL_ERROR_INVALID_PARAMETER);

#if X11
   Evas *e;
   Ecore_Evas *ee;
   const char *str;
   int id;

   e = evas_object_evas_get(window);
   EINA_SAFETY_ON_NULL_RETURN_VAL(e, EFL_UTIL_ERROR_INVALID_PARAMETER);

   ee = ecore_evas_ecore_evas_get(e);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ee, EFL_UTIL_ERROR_INVALID_PARAMETER);

   id = ecore_evas_aux_hint_id_get(ee, "wm.policy.win.lcd.lock");
   EINA_SAFETY_ON_TRUE_RETURN_VAL((id == -1), EFL_UTIL_ERROR_INVALID_PARAMETER);

   str = ecore_evas_aux_hint_val_get(ee, id);
   EINA_SAFETY_ON_NULL_RETURN_VAL(str, EFL_UTIL_ERROR_INVALID_PARAMETER);

   if (strncmp(str, "1", strlen("1")) == 0)
     *mode = EFL_UTIL_SCREEN_MODE_ALWAYS_ON;
   else
     *mode = EFL_UTIL_SCREEN_MODE_DEFAULT;

   return EFL_UTIL_ERROR_NONE;
#endif /* end of X11 */

#if WAYLAND
   Ecore_Wl_Window *wlwin;
   struct wl_surface *surface;
   Efl_Util_Wl_Surface_Scr_Mode_Info *scr_mode_info;
   Eina_Bool res;

   res = _wl_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   wlwin = elm_win_wl_window_get(window);
   if (wlwin)
     {
        while (!_eflutil.wl.scr_mode.proto)
          wl_display_dispatch(_eflutil.wl.dpy);

        surface = ecore_wl_window_surface_get(wlwin);
        EINA_SAFETY_ON_NULL_RETURN_VAL(surface,
                                       EFL_UTIL_ERROR_INVALID_PARAMETER);

        scr_mode_info = eina_hash_find(_eflutil.wl.scr_mode.hash, &surface);
        if (scr_mode_info)
          {
             if (scr_mode_info->wait_for_done)
               {
                  while (scr_mode_info->wait_for_done)
                    {
                       ecore_wl_flush();
                       wl_display_dispatch(_eflutil.wl.dpy);
                    }
               }

             switch (scr_mode_info->mode)
               {
                case TIZEN_WINDOW_SCREEN_MODE_DEFAULT:   *mode = EFL_UTIL_SCREEN_MODE_DEFAULT;   break;
                case TIZEN_WINDOW_SCREEN_MODE_ALWAYS_ON: *mode = EFL_UTIL_SCREEN_MODE_ALWAYS_ON; break;
                default:                                 *mode = EFL_UTIL_SCREEN_MODE_DEFAULT;
                  return EFL_UTIL_ERROR_INVALID_PARAMETER;
               }
             return EFL_UTIL_ERROR_NONE;
          }
        else
          {
             *mode = EFL_UTIL_SCREEN_MODE_DEFAULT;
             return EFL_UTIL_ERROR_INVALID_PARAMETER;
          }
     }
   else
     return EFL_UTIL_ERROR_INVALID_PARAMETER;
#endif /* end of WAYLAND */
}

API int
efl_util_set_window_screen_mode_error_cb(Evas_Object *window,
                                         efl_util_window_screen_mode_error_cb callback,
                                         void *user_data)
{
   Eina_Bool ret = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_NULL_RETURN_VAL(callback, EFL_UTIL_ERROR_INVALID_PARAMETER);

   ret = _cb_info_add(window,
                      (Efl_Util_Cb)callback,
                      user_data,
                      CBH_SCR_MODE);
   if (!ret) return EFL_UTIL_ERROR_OUT_OF_MEMORY;

#if X11
   if (!_eflutil.atom.scr_mode)
     _eflutil.atom.scr_mode = ecore_x_atom_get("_E_SCREEN_MODE_ACCESS_RESULT");
#endif /* end of X11 */

   return EFL_UTIL_ERROR_NONE;
}

API int
efl_util_unset_window_screen_mode_error_cb(Evas_Object *window)
{
   Eina_Bool ret = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);

   ret = _cb_info_del_by_win(window, CBH_SCR_MODE);
   if (!ret) return EFL_UTIL_ERROR_INVALID_PARAMETER;

   return EFL_UTIL_ERROR_NONE;
}

API int
efl_util_input_initialize_generator(efl_util_input_device_type_e dev_type)
{
   return EFL_UTIL_ERROR_NONE;
}

API void
efl_util_input_deinitialize_generator(void)
{
   return;
}

API int
efl_util_input_generate_key(const char *key_name,
                            int pressed)
{
   return EFL_UTIL_ERROR_NONE;
}

API int
efl_util_input_generate_touch(int idx,
                              efl_util_input_touch_type_e touch_type,
                              int x,
                              int y)
{
   return EFL_UTIL_ERROR_NONE;
}

API efl_util_screenshot_h
efl_util_screenshot_initialize(int width,
                               int height)
{
   return 0;
}

API tbm_surface_h
efl_util_screenshot_take_tbm_surface(efl_util_screenshot_h screenshot)
{
   return 0;
}

API int
efl_util_screenshot_deinitialize(efl_util_screenshot_h screenshot)
{
   return EFL_UTIL_ERROR_NONE;
}
