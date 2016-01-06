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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <tbm_bufmgr.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <Elementary.h>
#include <Ecore_Evas.h>

#if X11
#include <X11/Xlib.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/Xvproto.h>
#include <X11/extensions/Xdamage.h>
#include <dri2.h>
#include <Ecore_X.h>
#include <utilX.h>
#endif /* end of X11 */

#if WAYLAND
#include <Ecore_Wayland.h>
#include <wayland-client.h>
#include <wayland-tbm-client.h>
#include <tizen-extension-client-protocol.h>
#include <screenshooter-client-protocol.h>
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

#if WAYLAND
typedef struct _Efl_Util_Wl_Surface_Lv_Info
{
   void *surface; /* wl_surface */
   int level;
   Eina_Bool wait_for_done;
   uint32_t state;
} Efl_Util_Wl_Surface_Lv_Info;

typedef struct _Efl_Util_Wl_Surface_Scr_Mode_Info
{
   void *surface; /* wl_surface */
   unsigned int mode;
   Eina_Bool wait_for_done;
   uint32_t state;
} Efl_Util_Wl_Surface_Scr_Mode_Info;

typedef struct _Efl_Util_Wl_Output_Info
{
    struct wl_output *output;
    int offset_x, offset_y, width, height;
} Efl_Util_Wl_Output_Info;
#endif

typedef struct _Efl_Util_Data
{
#if X11
   /* x11 related stuffs */
   struct
   {
      Eina_Bool init;
      Ecore_Event_Handler *handler; /* x11 client message handler */
      Ecore_X_Display *dpy;
   } x11;
#endif /* end of X11 */

#if WAYLAND
   /* wayland related stuffs */
   struct
   {
      Eina_Bool init;
      struct wl_display *dpy;
      struct wl_event_queue *queue;

      struct
      {
         struct tizen_policy *proto;
         Eina_Hash *hash_noti_lv;
         Eina_Hash *hash_scr_mode;
      } policy;
      struct
      {
         struct screenshooter *screenshooter;
         struct wayland_tbm_client *tbm_client;
         Eina_List *output_list;
      } shot;
   } wl;
#endif /* end of WAYLAND */

   struct
   {
      Eina_List *info_list; /* list of callback info */
      unsigned int atom; /* x11 atom */
   } cb_handler[CBH_MAX];
} Efl_Util_Data;

static Efl_Util_Data _eflutil =
{
#if X11
   {
      EINA_FALSE,
      NULL,
      NULL
   },
#endif /* end of X11 */
#if WAYLAND
   {
      EINA_FALSE,
      NULL, NULL,
      { NULL, NULL, NULL }, /* tizen_policy protocol */
      { NULL, NULL, NULL }  /* screenshooter protocol */
   },
#endif /* end of WAYLAND */
   {
      { NULL, 0 }, /* handler for notification level */
      { NULL, 0 }  /* handler for screen mode */
   },
};

static Eina_Bool               _cb_info_add(Evas_Object *win, Efl_Util_Cb cb, void *data, int idx);
static Eina_Bool               _cb_info_del_by_win(Evas_Object *win, int idx);
static Eina_List              *_cb_info_list_get(int idx);
static Efl_Util_Callback_Info *_cb_info_find_by_win(Evas_Object *win, int idx);
#if X11
static Efl_Util_Callback_Info *_cb_info_find_by_xwin(unsigned int xwin, int idx);
static Eina_Bool               _cb_x11_client_msg(void *data, int type, void *event);
static Eina_Bool               _x11_init(void);
#endif /* end of X11 */
#if WAYLAND
static Eina_Bool               _wl_init(void);
static void                    _cb_wl_reg_global(void *data, struct wl_registry *reg, unsigned int name, const char *interface, unsigned int version);
static void                    _cb_wl_reg_global_remove(void *data, struct wl_registry *reg, unsigned int name);
static Efl_Util_Callback_Info *_cb_info_find_by_wlsurf(void *wlsurf, int idx);
static void                    _cb_wl_tz_policy_conformant(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface, uint32_t is_conformant);
static void                    _cb_wl_tz_policy_conformant_area(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface, uint32_t conformant_part, uint32_t state, int32_t x, int32_t y, int32_t w, int32_t h);
static void                    _cb_wl_tz_policy_notification_done(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface, int32_t level, uint32_t state);
static void                    _cb_wl_tz_policy_transient_for_done(void *data, struct tizen_policy *tizen_policy, uint32_t child_id);
static void                    _cb_wl_tz_policy_scr_mode_done(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface, uint32_t mode, uint32_t state);

static const struct wl_registry_listener _wl_reg_listener =
{
   _cb_wl_reg_global,
   _cb_wl_reg_global_remove
};

struct tizen_policy_listener _wl_tz_policy_listener =
{
   _cb_wl_tz_policy_conformant,
   _cb_wl_tz_policy_conformant_area,
   _cb_wl_tz_policy_notification_done,
   _cb_wl_tz_policy_transient_for_done,
   _cb_wl_tz_policy_scr_mode_done
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

   info = _cb_info_find_by_win(win, idx);
   if (!info) return EINA_FALSE;

   _eflutil.cb_handler[idx].info_list
      = eina_list_remove(_eflutil.cb_handler[idx].info_list,
                         info);
   free(info);

#if X11
   unsigned int count = eina_list_count(_eflutil.cb_handler[idx].info_list);
   if ((count == 0) && (_eflutil.x11.handler))
     {
        ecore_event_handler_del(_eflutil.x11.handler);
        _eflutil.x11.handler = NULL;
     }
#endif

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

   if (ev->message_type == _eflutil.cb_handler[CBH_NOTI_LEV].atom)
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
   else if (ev->message_type == _eflutil.cb_handler[CBH_SCR_MODE].atom)
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
   struct wl_registry *reg = NULL;

   if (_eflutil.wl.init) return EINA_TRUE;

   ecore_wl_init(NULL);

   _eflutil.wl.dpy = ecore_wl_display_get();
   EINA_SAFETY_ON_NULL_GOTO(_eflutil.wl.dpy, fail);

   _eflutil.wl.queue = wl_display_create_queue(_eflutil.wl.dpy);
   EINA_SAFETY_ON_NULL_GOTO(_eflutil.wl.queue, fail);

   reg = wl_display_get_registry(_eflutil.wl.dpy);
   EINA_SAFETY_ON_NULL_GOTO(reg, fail);

   wl_proxy_set_queue((struct wl_proxy*)reg, _eflutil.wl.queue);
   wl_registry_add_listener(reg, &_wl_reg_listener, NULL);

   _eflutil.wl.init = EINA_TRUE;

   return EINA_TRUE;
fail:
   if (_eflutil.wl.queue)
     {
        wl_event_queue_destroy(_eflutil.wl.queue);
        _eflutil.wl.queue = NULL;
     }

   if (reg)
     wl_registry_destroy(reg);
   ecore_wl_shutdown();
   return EINA_FALSE;
}

static void
_cb_wl_output_geometry(void *data, struct wl_output *wl_output, int x, int y,
                       int physical_width, int physical_height, int subpixel,
                       const char *make, const char *model, int transform)
{
   Efl_Util_Wl_Output_Info *output = wl_output_get_user_data(wl_output);
   if (wl_output == output->output)
     {
        output->offset_x = x;
        output->offset_y = y;
     }
}

static void
_cb_wl_output_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                   int width, int height, int refresh)
{
   Efl_Util_Wl_Output_Info *output = wl_output_get_user_data(wl_output);
   if (wl_output == output->output && (flags & WL_OUTPUT_MODE_CURRENT))
     {
        output->width = width;
        output->height = height;
     }
}

static void
_cb_wl_output_done(void *data, struct wl_output *wl_output)
{
}

static void
_cb_wl_output_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
}

static const struct wl_output_listener output_listener =
{
    _cb_wl_output_geometry,
    _cb_wl_output_mode,
    _cb_wl_output_done,
    _cb_wl_output_scale
};

static void
_cb_wl_screenshot_done(void *data, struct screenshooter *screenshooter)
{
   Eina_Bool *shot_done = (Eina_Bool*)data;
   if (shot_done)
     *shot_done = EINA_TRUE;
}

static const struct screenshooter_listener screenshooter_listener =
{
    _cb_wl_screenshot_done
};

static void
_cb_wl_reg_global(void *data,
                  struct wl_registry *reg,
                  unsigned int name,
                  const char *interface,
                  unsigned int version)
{
   if (!strcmp(interface, "tizen_policy"))
     {
        struct tizen_policy *proto;
        proto = wl_registry_bind(reg,
                                  name,
                                  &tizen_policy_interface,
                                  1);
        if (!proto) return;

        tizen_policy_add_listener(proto,
                                  &_wl_tz_policy_listener,
                                  NULL);

        _eflutil.wl.policy.hash_noti_lv = eina_hash_pointer_new(free);
        _eflutil.wl.policy.hash_scr_mode = eina_hash_pointer_new(free);
        _eflutil.wl.policy.proto = proto;
     }
   else if (strcmp(interface, "wl_output") == 0)
     {
        Efl_Util_Wl_Output_Info *output = calloc(1, sizeof(Efl_Util_Wl_Output_Info));
        EINA_SAFETY_ON_NULL_RETURN(output);

        _eflutil.wl.shot.output_list = eina_list_append(_eflutil.wl.shot.output_list, output);

        output->output = wl_registry_bind(reg, name, &wl_output_interface, version);
        wl_output_add_listener(output->output, &output_listener, output);
     }
   else if (strcmp(interface, "screenshooter") == 0)
     {
        _eflutil.wl.shot.screenshooter = wl_registry_bind(reg, name, &screenshooter_interface, version);
        screenshooter_add_listener(_eflutil.wl.shot.screenshooter, &screenshooter_listener, NULL);
     }
}

static void
_cb_wl_reg_global_remove(void *data,
                         struct wl_registry *reg,
                         unsigned int name)
{
   _eflutil.wl.policy.proto = NULL;
   eina_hash_free(_eflutil.wl.policy.hash_noti_lv);
   eina_hash_free(_eflutil.wl.policy.hash_scr_mode);
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
_cb_wl_tz_policy_conformant(void *data, struct tizen_policy *tizen_policy,
                            struct wl_surface *surface, uint32_t is_conformant)
{
}

static void
_cb_wl_tz_policy_conformant_area(void *data, struct tizen_policy *tizen_policy,
                                 struct wl_surface *surface, uint32_t conformant_part,
                                 uint32_t state, int32_t x, int32_t y, int32_t w, int32_t h)
{
}

static void
_cb_wl_tz_policy_notification_done(void *data,
                                   struct tizen_policy *tizen_policy,
                                   struct wl_surface *surface,
                                   int32_t level,
                                   uint32_t state)
{
   Efl_Util_Wl_Surface_Lv_Info *lv_info;
   Efl_Util_Callback_Info *cb_info;

   lv_info = eina_hash_find(_eflutil.wl.policy.hash_noti_lv, &surface);
   if (lv_info)
     {
        lv_info->level = level;
        lv_info->wait_for_done = EINA_FALSE;
        lv_info->state = state;
     }

   if (state != TIZEN_POLICY_ERROR_STATE_PERMISSION_DENIED) return;

   cb_info = _cb_info_find_by_wlsurf((void *)surface, CBH_NOTI_LEV);
   if (!cb_info) return;
   if (!cb_info->cb) return;

   cb_info->cb(cb_info->win,
               EFL_UTIL_ERROR_PERMISSION_DENIED,
               cb_info->data);
}

static void
_cb_wl_tz_policy_transient_for_done(void *data, struct tizen_policy *tizen_policy, uint32_t child_id)
{
}

static void
_cb_wl_tz_policy_scr_mode_done(void *data,
                               struct tizen_policy *tizen_policy,
                               struct wl_surface *surface,
                               uint32_t mode,
                               uint32_t state)
{

   Efl_Util_Wl_Surface_Scr_Mode_Info *scr_mode_info;
   Efl_Util_Callback_Info *cb_info;

   scr_mode_info = eina_hash_find(_eflutil.wl.policy.hash_scr_mode, &surface);
   if (scr_mode_info)
     {
        scr_mode_info->mode = mode;
        scr_mode_info->wait_for_done = EINA_FALSE;
        scr_mode_info->state = state;
     }

   if (state != TIZEN_POLICY_ERROR_STATE_PERMISSION_DENIED) return;

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
   EINA_SAFETY_ON_FALSE_RETURN_VAL((level >= EFL_UTIL_NOTIFICATION_LEVEL_NONE) &&
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

   return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
#endif /* end of X11 */

#if WAYLAND
   Elm_Win_Type type;
   Ecore_Wl_Window *wlwin;
   struct wl_surface *surface;
   Efl_Util_Wl_Surface_Lv_Info *lv_info;
   Ecore_Wl_Window_Type wl_type;

   res = _wl_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   wlwin = elm_win_wl_window_get(window);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wlwin, EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);

   type = elm_win_type_get(window);
   if (type != ELM_WIN_NOTIFICATION)
     {
        wl_type = ecore_wl_window_type_get(wlwin);
        EINA_SAFETY_ON_FALSE_RETURN_VAL((wl_type == ECORE_WL_WINDOW_TYPE_NOTIFICATION),
                                        EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);
     }

   while (!_eflutil.wl.policy.proto)
     wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

   surface = ecore_wl_window_surface_get(wlwin);
   EINA_SAFETY_ON_NULL_RETURN_VAL(surface,
                                  EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);

   lv_info = eina_hash_find(_eflutil.wl.policy.hash_noti_lv, &surface);
   if (!lv_info)
     {
        lv_info = calloc(1, sizeof(Efl_Util_Wl_Surface_Lv_Info));
        EINA_SAFETY_ON_NULL_RETURN_VAL(lv_info, EFL_UTIL_ERROR_OUT_OF_MEMORY);

        lv_info->surface = surface;
        lv_info->level = (int)level;
        lv_info->wait_for_done = EINA_TRUE;
        lv_info->state = TIZEN_POLICY_ERROR_STATE_NONE;
        eina_hash_add(_eflutil.wl.policy.hash_noti_lv,
                      &surface,
                      lv_info);
     }
   else
     {
        lv_info->level = (int)level;
        lv_info->wait_for_done = EINA_TRUE;
     }

   tizen_policy_set_notification_level(_eflutil.wl.policy.proto,
                                       surface, (int)level);

   if (lv_info->wait_for_done)
     {
        int count = 0;
        while (lv_info->wait_for_done && (count < 3))
          {
             ecore_wl_flush();
             wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);
             count++;
          }

        if (lv_info->wait_for_done)
          {
             return EFL_UTIL_ERROR_INVALID_PARAMETER;
          }
        else
          {
             if (lv_info->state == TIZEN_POLICY_ERROR_STATE_PERMISSION_DENIED)
               {
                  return EFL_UTIL_ERROR_PERMISSION_DENIED;
               }
          }
     }

   return EFL_UTIL_ERROR_NONE;
#endif /* end of WAYLAND */
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

   return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;
#endif /* end of X11 */

#if WAYLAND
   Elm_Win_Type type;
   Ecore_Wl_Window *wlwin;
   struct wl_surface *surface;
   Efl_Util_Wl_Surface_Lv_Info *lv_info;
   Ecore_Wl_Window_Type wl_type;

   res = _wl_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   wlwin = elm_win_wl_window_get(window);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wlwin, EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);

   type = elm_win_type_get(window);
   if (type != ELM_WIN_NOTIFICATION)
     {
        wl_type = ecore_wl_window_type_get(wlwin);
        EINA_SAFETY_ON_FALSE_RETURN_VAL((wl_type == ECORE_WL_WINDOW_TYPE_NOTIFICATION),
                                        EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);
     }

   while (!_eflutil.wl.policy.proto)
     wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

   surface = ecore_wl_window_surface_get(wlwin);
   EINA_SAFETY_ON_NULL_RETURN_VAL(surface,
                                  EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE);

   lv_info = eina_hash_find(_eflutil.wl.policy.hash_noti_lv, &surface);
   if (lv_info)
     {
        if (lv_info->wait_for_done)
          {
             int count = 0;
             while ((lv_info->wait_for_done) && (count < 3))
               {
                  ecore_wl_flush();
                  wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);
                  count++;
               }

             if (lv_info->wait_for_done)
               {
                  *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;
                  return EFL_UTIL_ERROR_INVALID_PARAMETER;
               }
          }

        switch (lv_info->level)
          {
           case TIZEN_POLICY_LEVEL_1:       *level = EFL_UTIL_NOTIFICATION_LEVEL_1;       break;
           case TIZEN_POLICY_LEVEL_2:       *level = EFL_UTIL_NOTIFICATION_LEVEL_2;       break;
           case TIZEN_POLICY_LEVEL_3:       *level = EFL_UTIL_NOTIFICATION_LEVEL_3;       break;
           case TIZEN_POLICY_LEVEL_NONE:    *level = EFL_UTIL_NOTIFICATION_LEVEL_NONE;    break;
           case TIZEN_POLICY_LEVEL_DEFAULT: *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT; break;
           case TIZEN_POLICY_LEVEL_MEDIUM:  *level = EFL_UTIL_NOTIFICATION_LEVEL_MEDIUM;  break;
           case TIZEN_POLICY_LEVEL_HIGH:    *level = EFL_UTIL_NOTIFICATION_LEVEL_HIGH;    break;
           case TIZEN_POLICY_LEVEL_TOP:     *level = EFL_UTIL_NOTIFICATION_LEVEL_TOP;     break;
           default:                         *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;
            return EFL_UTIL_ERROR_INVALID_PARAMETER;
          }
        return EFL_UTIL_ERROR_NONE;
     }
   else
     *level = EFL_UTIL_NOTIFICATION_LEVEL_DEFAULT;

   return EFL_UTIL_ERROR_NONE;
#endif /* end of WAYLAND */
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
   if (!_eflutil.cb_handler[CBH_NOTI_LEV].atom)
     _eflutil.cb_handler[CBH_NOTI_LEV].atom = ecore_x_atom_get("_E_NOTIFICATION_LEVEL_ACCESS_RESULT");
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
   EINA_SAFETY_ON_FALSE_RETURN_VAL(xwin > 0, EFL_UTIL_ERROR_INVALID_PARAMETER);

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
   struct wl_surface *surface;

   if (!_eflutil.wl.policy.proto)
     {
        int ret = 0;

        res = _wl_init();
        EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

        while (!_eflutil.wl.policy.proto && ret != -1)
          ret = wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

        EINA_SAFETY_ON_NULL_RETURN_VAL(_eflutil.wl.policy.proto, EFL_UTIL_ERROR_INVALID_PARAMETER);
     }

   wlwin = elm_win_wl_window_get(window);
   if (!wlwin)
      return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;

   surface  = ecore_wl_window_surface_get(wlwin);
   if (!surface)
      return EFL_UTIL_ERROR_NOT_SUPPORTED_WINDOW_TYPE;

   tizen_policy_set_opaque_state(_eflutil.wl.policy.proto, surface, opaque);

   return EFL_UTIL_ERROR_NONE;
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
        while (!_eflutil.wl.policy.proto)
          wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

        surface = ecore_wl_window_surface_get(wlwin);
        EINA_SAFETY_ON_NULL_RETURN_VAL(surface,
                                       EFL_UTIL_ERROR_INVALID_PARAMETER);

        scr_mode_info = eina_hash_find(_eflutil.wl.policy.hash_scr_mode, &surface);
        if (!scr_mode_info)
          {
             scr_mode_info = calloc(1, sizeof(Efl_Util_Wl_Surface_Scr_Mode_Info));
             EINA_SAFETY_ON_NULL_RETURN_VAL(scr_mode_info, EFL_UTIL_ERROR_OUT_OF_MEMORY);

             scr_mode_info->surface = surface;
             scr_mode_info->mode = (unsigned int)mode;
             scr_mode_info->wait_for_done = EINA_TRUE;

             eina_hash_add(_eflutil.wl.policy.hash_scr_mode,
                           &surface,
                           scr_mode_info);
          }
        else
          {
             scr_mode_info->mode = (unsigned int)mode;
             scr_mode_info->wait_for_done = EINA_TRUE;
          }

        tizen_policy_set_window_screen_mode(_eflutil.wl.policy.proto,
                                            surface, (unsigned int)mode);
        if (scr_mode_info->wait_for_done)
          {
             int count = 0;
             while (scr_mode_info->wait_for_done && (count < 3))
               {
                  ecore_wl_flush();
                  wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);
                  count++;
               }

             if (scr_mode_info->wait_for_done)
               {
                  return EFL_UTIL_ERROR_INVALID_PARAMETER;
               }
             else
               {
                  if (scr_mode_info->state == TIZEN_POLICY_ERROR_STATE_PERMISSION_DENIED)
                    {
                       return EFL_UTIL_ERROR_PERMISSION_DENIED;
                    }
               }
          }

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
        while (!_eflutil.wl.policy.proto)
          wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

        surface = ecore_wl_window_surface_get(wlwin);
        EINA_SAFETY_ON_NULL_RETURN_VAL(surface,
                                       EFL_UTIL_ERROR_INVALID_PARAMETER);

        scr_mode_info = eina_hash_find(_eflutil.wl.policy.hash_scr_mode, &surface);
        if (scr_mode_info)
          {
             if (scr_mode_info->wait_for_done)
               {
                  while (scr_mode_info->wait_for_done)
                    {
                       ecore_wl_flush();
                       wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);
                    }
               }

             switch (scr_mode_info->mode)
               {
                case TIZEN_POLICY_MODE_DEFAULT:   *mode = EFL_UTIL_SCREEN_MODE_DEFAULT;   break;
                case TIZEN_POLICY_MODE_ALWAYS_ON: *mode = EFL_UTIL_SCREEN_MODE_ALWAYS_ON; break;
                default:                          *mode = EFL_UTIL_SCREEN_MODE_DEFAULT;
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
   if (!_eflutil.cb_handler[CBH_SCR_MODE].atom)
     _eflutil.cb_handler[CBH_SCR_MODE].atom = ecore_x_atom_get("_E_SCREEN_MODE_ACCESS_RESULT");
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

struct _efl_util_screenshot_h
{
   int width;
   int height;

#if X11
   Ecore_X_Display *dpy;
   int internal_display;
   int screen;
   Window root;
   Pixmap pixmap;
   GC gc;
   Atom atom_capture;

   /* port */
   int port;

   /* damage */
   Damage   damage;
   int      damage_base;

   /* dri2 */
   int eventBase, errorBase;
   int dri2Major, dri2Minor;
   char *driver_name, *device_name;
   drm_magic_t magic;

   /* drm */
   int drm_fd;
#endif

   Eina_Bool shot_done;

   /* tbm bufmgr */
   tbm_bufmgr bufmgr;
};

/* scrrenshot handle */
static efl_util_screenshot_h g_screenshot;

#if X11
#define FOURCC(a,b,c,d) (((unsigned)d&0xff)<<24 | ((unsigned)c&0xff)<<16 | ((unsigned)b&0xff)<<8 | ((unsigned)a&0xff))
#define FOURCC_RGB32    FOURCC('R','G','B','4')
#define TIMEOUT_CAPTURE 3

/* x error handling */
static Bool g_efl_util_x_error_caught;

static int
_efl_util_screenshot_x_error_handle(Display *dpy, XErrorEvent *ev)
{
   if (!g_screenshot || (dpy != g_screenshot->dpy))
     return 0;

   g_efl_util_x_error_caught = True;

   return 0;
}

static int
_efl_util_screenshot_get_port(Display *dpy, unsigned int id, Window win)
{
   unsigned int ver, rev, req_base, evt_base, err_base;
   unsigned int adaptors;
   XvAdaptorInfo *ai = NULL;
   XvImageFormatValues *fo = NULL;
   int formats;
   int i, j, p;

   if (XvQueryExtension(dpy, &ver, &rev, &req_base, &evt_base, &err_base) != Success)
     {
        fprintf(stderr, "[screenshot] fail: no XV extension. \n");
        return -1;
     }

   if (XvQueryAdaptors(dpy, win, &adaptors, &ai) != Success)
     {
        fprintf(stderr, "[screenshot] fail: query adaptors. \n");
        return -1;
     }

   EINA_SAFETY_ON_NULL_RETURN_VAL(ai, -1);

   for (i = 0; i < adaptors; i++)
     {
        int support_format = False;

        if (!(ai[i].type & XvInputMask) ||
            !(ai[i].type & XvStillMask))
          continue;

        p = ai[i].base_id;

        fo = XvListImageFormats(dpy, p, &formats);
        for (j = 0; j < formats; j++)
          if (fo[j].id == (int)id)
            support_format = True;

        if (fo)
          XFree(fo);

        if (!support_format)
          continue;

        for (; p < ai[i].base_id + ai[i].num_ports; p++)
          {
             if (XvGrabPort(dpy, p, 0) == Success)
               {
                  XvFreeAdaptorInfo(ai);
                  return p;
               }
          }
     }

   XvFreeAdaptorInfo(ai);

   XSync(dpy, False);

   return -1;
}

static int _efl_util_screenshot_get_best_size(Display *dpy, int port, int width, int height, unsigned int *best_width, unsigned int *best_height)
{
   XErrorHandler old_handler = NULL;

   Atom atom_capture = XInternAtom(dpy, "_USER_WM_PORT_ATTRIBUTE_CAPTURE", False);

   g_efl_util_x_error_caught = False;
   old_handler = XSetErrorHandler(_efl_util_screenshot_x_error_handle);

   XvSetPortAttribute(dpy, port, atom_capture, 1);
   XSync(dpy, False);

   g_efl_util_x_error_caught = False;
   XSetErrorHandler(old_handler);

   XvQueryBestSize(dpy, port, 0, 0, 0, width, height, best_width, best_height);
   if (best_width <= 0 || best_height <= 0)
     return 0;

   return 1;
}
#endif

API efl_util_screenshot_h efl_util_screenshot_initialize(int width, int height)
{
#if X11
   efl_util_screenshot_h screenshot = NULL;
   int depth = 0;
   int damage_err_base = 0;
   unsigned int best_width = 0;
   unsigned int best_height = 0;

   EINA_SAFETY_ON_FALSE_GOTO(width > 0, fail_param);
   EINA_SAFETY_ON_FALSE_GOTO(height > 0, fail_param);

   if (g_screenshot != NULL)
     {
        if (g_screenshot->width != width || g_screenshot->height != height)
          {
             // TODO: recreate pixmap and update information
             if (!_efl_util_screenshot_get_best_size(screenshot->dpy, screenshot->port, width, height, &best_width, &best_height))
               {
                  set_last_result(EFL_UTIL_ERROR_SCREENSHOT_INIT_FAIL);
                  return NULL;
               }

             g_screenshot->width = width;
             g_screenshot->height = height;
          }

        return g_screenshot;
     }

   screenshot = calloc(1, sizeof(struct _efl_util_screenshot_h));
   EINA_SAFETY_ON_NULL_GOTO(screenshot, fail_memory);

   /* set dpy */
   screenshot->dpy = ecore_x_display_get();
   if (!screenshot->dpy)
     {
        screenshot->dpy = XOpenDisplay(0);
        EINA_SAFETY_ON_NULL_GOTO(screenshot, fail_init);

        /* for XCloseDisplay at denitialization */
        screenshot->internal_display = 1;
     }

   /* set screen */
   screenshot->screen = DefaultScreen(screenshot->dpy);

   /* set root window */
   screenshot->root = DefaultRootWindow(screenshot->dpy);

   /* initialize capture adaptor */
   screenshot->port = _efl_util_screenshot_get_port(screenshot->dpy, FOURCC_RGB32, screenshot->root);
   EINA_SAFETY_ON_FALSE_GOTO(screenshot->port > 0, fail_init);

   /* get the best size */
   _efl_util_screenshot_get_best_size(screenshot->dpy, screenshot->port, width, height, &best_width, &best_height);
   EINA_SAFETY_ON_FALSE_GOTO(best_width > 0, fail_init);
   EINA_SAFETY_ON_FALSE_GOTO(best_height > 0, fail_init);

   /* set the width and the height */
   screenshot->width = best_width;
   screenshot->height = best_height;

   /* create a pixmap */
   depth = DefaultDepth(screenshot->dpy, screenshot->screen);
   screenshot->pixmap = XCreatePixmap(screenshot->dpy, screenshot->root, screenshot->width, screenshot->height, depth);
   EINA_SAFETY_ON_FALSE_GOTO(screenshot->pixmap > 0, fail_init);

   screenshot->gc = XCreateGC(screenshot->dpy, screenshot->pixmap, 0, 0);
   EINA_SAFETY_ON_NULL_GOTO(screenshot->gc, fail_init);

   XSetForeground(screenshot->dpy, screenshot->gc, 0xFF000000);
   XFillRectangle(screenshot->dpy, screenshot->pixmap, screenshot->gc, 0, 0, width, height);

   /* initialize damage */
   if (!XDamageQueryExtension(screenshot->dpy, &screenshot->damage_base, &damage_err_base))
     goto fail_init;

   screenshot->damage = XDamageCreate(screenshot->dpy, screenshot->pixmap, XDamageReportNonEmpty);
   EINA_SAFETY_ON_FALSE_GOTO(screenshot->damage > 0, fail_init);

   /* initialize dri3 and dri2 */
   if (!DRI2QueryExtension(screenshot->dpy, &screenshot->eventBase, &screenshot->errorBase))
     {
        fprintf(stderr, "[screenshot] fail: DRI2QueryExtention\n");
        goto fail_init;
     }

   if (!DRI2QueryVersion(screenshot->dpy, &screenshot->dri2Major, &screenshot->dri2Minor))
     {
        fprintf(stderr, "[screenshot] fail: DRI2QueryVersion\n");
        goto fail_init;
     }

   if (!DRI2Connect(screenshot->dpy, screenshot->root, &screenshot->driver_name, &screenshot->device_name))
     {
        fprintf(stderr, "[screenshot] fail: DRI2Connect\n");
        goto fail_init;
     }

   screenshot->drm_fd = open(screenshot->device_name, O_RDWR);
   EINA_SAFETY_ON_FALSE_GOTO(screenshot->drm_fd >= 0, fail_init);

   if (drmGetMagic(screenshot->drm_fd, &screenshot->magic))
     {
        fprintf(stderr, "[screenshot] fail: drmGetMagic\n");
        goto fail_init;
     }

   if (!DRI2Authenticate(screenshot->dpy, screenshot->root, screenshot->magic))
     {
        fprintf(stderr, "[screenshot] fail: DRI2Authenticate\n");
        goto fail_init;
     }

   if (!drmAuthMagic(screenshot->drm_fd, screenshot->magic))
     {
        fprintf(stderr, "[screenshot] fail: drmAuthMagic\n");
        goto fail_init;
     }

   DRI2CreateDrawable(screenshot->dpy, screenshot->pixmap);

   /* tbm bufmgr */
   screenshot->bufmgr = tbm_bufmgr_init(screenshot->drm_fd);
   EINA_SAFETY_ON_NULL_GOTO(screenshot->bufmgr, fail_init);

   XFlush(screenshot->dpy);

   g_screenshot = screenshot;
   set_last_result(EFL_UTIL_ERROR_NONE);

   return g_screenshot;
#endif

#if WAYLAND
   efl_util_screenshot_h screenshot = NULL;

   if (!_eflutil.wl.shot.screenshooter)
     {
        int ret = 0;
        _wl_init();
        while (!_eflutil.wl.shot.screenshooter && ret != -1)
          ret = wl_display_roundtrip_queue(_eflutil.wl.dpy, _eflutil.wl.queue);
        EINA_SAFETY_ON_NULL_GOTO(_eflutil.wl.shot.screenshooter, fail_init);

       _eflutil.wl.shot.tbm_client = wayland_tbm_client_init(_eflutil.wl.dpy);
       EINA_SAFETY_ON_NULL_GOTO(_eflutil.wl.shot.tbm_client, fail_init);
     }

   EINA_SAFETY_ON_FALSE_GOTO(width > 0, fail_param);
   EINA_SAFETY_ON_FALSE_GOTO(height > 0, fail_param);

   if (g_screenshot)
     {
        if (g_screenshot->width != width || g_screenshot->height != height)
          {
             g_screenshot->width = width;
             g_screenshot->height = height;
          }

        return g_screenshot;
     }

   screenshot = calloc(1, sizeof(struct _efl_util_screenshot_h));
   EINA_SAFETY_ON_NULL_GOTO(screenshot, fail_memory);

   screenshot->width = width;
   screenshot->height = height;

   screenshot->bufmgr = wayland_tbm_client_get_bufmgr(_eflutil.wl.shot.tbm_client);
   EINA_SAFETY_ON_NULL_GOTO(screenshot->bufmgr, fail_init);

   g_screenshot = screenshot;
   set_last_result(EFL_UTIL_ERROR_NONE);

   screenshooter_set_user_data(_eflutil.wl.shot.screenshooter, &screenshot->shot_done);

   return g_screenshot;
#endif
fail_param:
   if (screenshot)
     efl_util_screenshot_deinitialize(screenshot);
   set_last_result(EFL_UTIL_ERROR_INVALID_PARAMETER);
   return NULL;
fail_memory:
   if (screenshot)
     efl_util_screenshot_deinitialize(screenshot);
   set_last_result(EFL_UTIL_ERROR_OUT_OF_MEMORY);
   return NULL;
fail_init:
   if (screenshot)
     efl_util_screenshot_deinitialize(screenshot);
   set_last_result(EFL_UTIL_ERROR_SCREENSHOT_INIT_FAIL);
   return NULL;
}

API int efl_util_screenshot_deinitialize(efl_util_screenshot_h screenshot)
{
#if X11
   if (!screenshot)
     return EFL_UTIL_ERROR_INVALID_PARAMETER;

   /* tbm bufmgr */
   if (screenshot->bufmgr)
     tbm_bufmgr_deinit(screenshot->bufmgr);

   DRI2DestroyDrawable(screenshot->dpy, screenshot->pixmap);

   /* dri2 */
   if (screenshot->drm_fd)
     close(screenshot->drm_fd);
   if (screenshot->driver_name)
     free(screenshot->driver_name);
   if (screenshot->device_name)
     free(screenshot->device_name);

   /* xv */
   if (screenshot->port > 0 && screenshot->pixmap > 0)
     XvStopVideo(screenshot->dpy, screenshot->port, screenshot->pixmap);

   /* damage */
   if (screenshot->damage)
     XDamageDestroy(screenshot->dpy, screenshot->damage);

   /* gc */
   if (screenshot->gc)
     XFreeGC(screenshot->dpy, screenshot->gc);

   /* pixmap */
   if (screenshot->pixmap > 0)
     XFreePixmap(screenshot->dpy, screenshot->pixmap);

   /* port */
   if (screenshot->port > 0)
     XvUngrabPort(screenshot->dpy, screenshot->port, 0);

   XSync(screenshot->dpy, False);

   /* dpy */
   if (screenshot->internal_display ==1 && screenshot->dpy)
     XCloseDisplay(screenshot->dpy);

   free(screenshot);
   g_screenshot = NULL;

   return EFL_UTIL_ERROR_NONE;
#endif
#if WAYLAND
   if (!screenshot)
     return EFL_UTIL_ERROR_NONE;

   free(screenshot);
   g_screenshot = NULL;

   if (_eflutil.wl.shot.screenshooter)
     screenshooter_set_user_data(_eflutil.wl.shot.screenshooter, NULL);

   return EFL_UTIL_ERROR_NONE;
#endif
}


API tbm_surface_h efl_util_screenshot_take_tbm_surface(efl_util_screenshot_h screenshot)
{
#if X11
   XEvent ev = {0,};
   XErrorHandler old_handler = NULL;
   unsigned int attachment = DRI2BufferFrontLeft;
   int nbufs = 0;
   DRI2Buffer *bufs = NULL;
   tbm_bo t_bo = NULL;
   tbm_surface_h t_surface = NULL;
   int buf_width = 0;
   int buf_height = 0;
   tbm_surface_info_s surf_info;
   int i;

   if (screenshot != g_screenshot)
     {
        set_last_result(EFL_UTIL_ERROR_INVALID_PARAMETER);
        return NULL;
     }

   /* for flush other pending requests and pending events */
   XSync(screenshot->dpy, 0);

   g_efl_util_x_error_caught = False;
   old_handler = XSetErrorHandler(_efl_util_screenshot_x_error_handle);

   /* dump here */
   XvPutStill(screenshot->dpy, screenshot->port, screenshot->pixmap, screenshot->gc,
              0, 0, screenshot->width, screenshot->height,
              0, 0, screenshot->width, screenshot->height);

   XSync(screenshot->dpy, 0);

   if (g_efl_util_x_error_caught)
     {
        g_efl_util_x_error_caught = False;
        XSetErrorHandler(old_handler);
        goto fail;
     }

   g_efl_util_x_error_caught = False;
   XSetErrorHandler(old_handler);

   if (XPending(screenshot->dpy))
     XNextEvent(screenshot->dpy, &ev);
   else
     {
        int fd = ConnectionNumber(screenshot->dpy);
        fd_set mask;
        struct timeval tv;
        int ret;

        FD_ZERO(&mask);
        FD_SET(fd, &mask);

        tv.tv_usec = 0;
        tv.tv_sec = TIMEOUT_CAPTURE;

        ret = select(fd + 1, &mask, 0, 0, &tv);
        if (ret < 0)
          fprintf(stderr, "[screenshot] fail: select.\n");
        else if (ret == 0)
          fprintf(stderr, "[screenshot] fail: timeout(%d sec)!\n", TIMEOUT_CAPTURE);
        else if (XPending(screenshot->dpy))
          XNextEvent(screenshot->dpy, &ev);
        else
          fprintf(stderr, "[screenshot] fail: not passed a event!\n");
     }

   /* check if the capture is done by xserver and pixmap has got the captured image */
   if (ev.type == (screenshot->damage_base + XDamageNotify))
     {
        XDamageNotifyEvent *damage_ev = (XDamageNotifyEvent *)&ev;
        if (damage_ev->drawable == screenshot->pixmap)
          {
             /* Get DRI2 FrontLeft buffer of the pixmap */
             bufs = DRI2GetBuffers(screenshot->dpy, screenshot->pixmap, &buf_width, &buf_height, &attachment, 1, &nbufs);
             if (!bufs)
               {
                  fprintf(stderr, "[screenshot] fail: DRI2GetBuffers\n");
                  goto fail;
               }

             t_bo = tbm_bo_import(screenshot->bufmgr, bufs[0].name);
             if (!t_bo)
               {
                  fprintf(stderr, "[screenshot] fail: import tbm_bo!\n");
                  goto fail;
               }

             surf_info.width = buf_width;
             surf_info.height = buf_height;
             surf_info.format = TBM_FORMAT_XRGB8888;
             surf_info.bpp = 32;
             surf_info.size = bufs->pitch * surf_info.height;
             surf_info.num_planes = 1;
             for (i = 0; i < surf_info.num_planes; i++)
               {
                  surf_info.planes[i].size = bufs->pitch * surf_info.height;
                  surf_info.planes[i].stride = bufs->pitch;
                  surf_info.planes[i].offset = 0;
               }
             t_surface = tbm_surface_internal_create_with_bos(&surf_info, &t_bo, 1);
             if (!t_surface)
               {
                  fprintf(stderr, "[screenshot] fail: get tbm_surface!\n");
                  goto fail;
               }

             tbm_bo_unref(t_bo);
             free(bufs);

             XDamageSubtract(screenshot->dpy, screenshot->damage, None, None );

             set_last_result(EFL_UTIL_ERROR_NONE);

             return t_surface;
          }

        XDamageSubtract(screenshot->dpy, screenshot->damage, None, None );
     }

fail:

   if (t_bo)
     tbm_bo_unref(t_bo);
   if (bufs)
     free(bufs);

   set_last_result(EFL_UTIL_ERROR_SCREENSHOT_EXECUTION_FAIL);

   return NULL;
#endif

#if WAYLAND
   tbm_surface_h t_surface = NULL;
   struct wl_buffer *buffer = NULL;
   Efl_Util_Wl_Output_Info *output;
   int ret = 0;

   if (screenshot != g_screenshot)
     {
        set_last_result(EFL_UTIL_ERROR_INVALID_PARAMETER);
        return NULL;
     }

   output = eina_list_nth(_eflutil.wl.shot.output_list, 0);
   if (!output)
     {
        fprintf(stderr, "[screenshot] fail: no output for screenshot\n");
        goto fail;
     }

   t_surface = tbm_surface_create(screenshot->width, screenshot->height, TBM_FORMAT_XRGB8888);
   if (!t_surface)
     {
        fprintf(stderr, "[screenshot] fail: tbm_surface_create\n");
        goto fail;
     }

   buffer = wayland_tbm_client_create_buffer(_eflutil.wl.shot.tbm_client, t_surface);
   if (!buffer)
     {
        fprintf(stderr, "[screenshot] fail: create wl_buffer for screenshot\n");
        goto fail;
     }

   screenshooter_shoot(_eflutil.wl.shot.screenshooter, output->output, buffer);

   screenshot->shot_done = EINA_FALSE;
   while (!screenshot->shot_done && ret != -1)
     ret = wl_display_roundtrip_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

   if (ret == -1)
     {
        fprintf(stderr, "[screenshot] fail: screenshooter_shoot\n");
        goto fail;
     }

   wl_buffer_destroy(buffer);

   /* reset shot_done for next screenshot */
   screenshot->shot_done = EINA_FALSE;

   return t_surface;

fail:
   if (t_surface)
     tbm_surface_destroy(t_surface);
   if (buffer)
     wl_buffer_destroy(buffer);

   set_last_result(EFL_UTIL_ERROR_SCREENSHOT_EXECUTION_FAIL);

   return NULL;
#endif
}
