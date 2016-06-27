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

#include <Ecore_Wayland.h>
#include <wayland-client.h>
#include <wayland-tbm-client.h>
#include <tizen-extension-client-protocol.h>
#include <screenshooter-client-protocol.h>

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
   uint32_t state;
} Efl_Util_Wl_Surface_Lv_Info;

typedef struct _Efl_Util_Wl_Surface_Scr_Mode_Info
{
   void *surface; /* wl_surface */
   unsigned int mode;
   Eina_Bool wait_for_done;
   uint32_t state;
} Efl_Util_Wl_Surface_Scr_Mode_Info;

typedef struct _Efl_Util_Wl_Surface_Brightness_Info
{
   void *surface; /* wl_surface */
   int brightness;
   Eina_Bool wait_for_done;
   uint32_t state;
} Efl_Util_Wl_Surface_Brightness_Info;

typedef struct _Efl_Util_Wl_Output_Info
{
    struct wl_output *output;
    int offset_x, offset_y, width, height;
} Efl_Util_Wl_Output_Info;

typedef struct _Efl_Util_Data
{
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
      struct
      {
         struct tizen_input_device_manager *devicemgr;
         int request_notified;
      } devmgr;
      struct
      {
         struct tizen_display_policy *proto;
         Eina_Hash *hash_brightness;
      } display_policy;
   } wl;

   struct
   {
      Eina_List *info_list; /* list of callback info */
      unsigned int atom; /* x11 atom */
   } cb_handler[CBH_MAX];
} Efl_Util_Data;

static Efl_Util_Data _eflutil =
{
   {
      EINA_FALSE,
      NULL, NULL,
      { NULL, NULL, NULL }, /* tizen_policy protocol */
      { NULL, NULL, NULL }, /* screenshooter protocol */
      { NULL, -1 } /* tizen_input_device_manager protocol */
   },
   {
      { NULL, 0 }, /* handler for notification level */
      { NULL, 0 }  /* handler for screen mode */
   },
};

static Eina_Bool               _cb_info_add(Evas_Object *win, Efl_Util_Cb cb, void *data, int idx);
static Eina_Bool               _cb_info_del_by_win(Evas_Object *win, int idx);
static Eina_List              *_cb_info_list_get(int idx);
static Efl_Util_Callback_Info *_cb_info_find_by_win(Evas_Object *win, int idx);
static Eina_Bool               _wl_init(void);
static void                    _cb_wl_reg_global(void *data, struct wl_registry *reg, unsigned int name, const char *interface, unsigned int version);
static void                    _cb_wl_reg_global_remove(void *data, struct wl_registry *reg, unsigned int name);
static Efl_Util_Callback_Info *_cb_info_find_by_wlsurf(void *wlsurf, int idx);
static void                    _cb_wl_tz_policy_conformant(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface, uint32_t is_conformant);
static void                    _cb_wl_tz_policy_conformant_area(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface, uint32_t conformant_part, uint32_t state, int32_t x, int32_t y, int32_t w, int32_t h);
static void                    _cb_wl_tz_policy_notification_done(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface, int32_t level, uint32_t state);
static void                    _cb_wl_tz_policy_transient_for_done(void *data, struct tizen_policy *tizen_policy, uint32_t child_id);
static void                    _cb_wl_tz_policy_scr_mode_done(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface, uint32_t mode, uint32_t state);
static void                    _cb_wl_tz_policy_iconify_state_changed(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface_resource, uint32_t iconified, uint32_t force);
static void                    _cb_wl_tz_policy_supported_aux_hints(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface_resource, struct wl_array *hints, uint32_t num_hints);
static void                    _cb_wl_tz_policy_allowed_aux_hint(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface_resource, int id);

static void                    _cb_wl_tz_display_policy_brightness_done(void *data, struct tizen_display_policy *tizen_display_policy, struct wl_surface *surface_resource, int32_t brightness, uint32_t state);

static void                    _cb_device_add(void *data EINA_UNUSED, struct tizen_input_device_manager *tizen_input_device_manager EINA_UNUSED, uint32_t serial  EINA_UNUSED, const char *identifier  EINA_UNUSED, struct tizen_input_device *device  EINA_UNUSED, struct wl_seat *seat EINA_UNUSED);
static void                    _cb_device_remove(void *data EINA_UNUSED, struct tizen_input_device_manager *tizen_input_device_manager EINA_UNUSED, uint32_t serial EINA_UNUSED, const char *identifier  EINA_UNUSED, struct tizen_input_device *device EINA_UNUSED, struct wl_seat *seat EINA_UNUSED);
static void                    _cb_error(void *data EINA_UNUSED, struct tizen_input_device_manager *tizen_input_device_manager EINA_UNUSED, uint32_t errorcode);
static void                    _cb_block_expired(void *data EINA_UNUSED, struct tizen_input_device_manager *tizen_input_device_manager EINA_UNUSED);

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
   _cb_wl_tz_policy_scr_mode_done,
   _cb_wl_tz_policy_iconify_state_changed,
   _cb_wl_tz_policy_supported_aux_hints,
   _cb_wl_tz_policy_allowed_aux_hint,
};

struct tizen_input_device_manager_listener _wl_tz_devmgr_listener =
{
   _cb_device_add,
   _cb_device_remove,
   _cb_error,
   _cb_block_expired
};

struct tizen_display_policy_listener _wl_tz_display_policy_listener =
{
   _cb_wl_tz_display_policy_brightness_done,
};

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
   else if (strcmp(interface, "tizen_input_device_manager") == 0)
     {
        _eflutil.wl.devmgr.devicemgr = wl_registry_bind(reg, name, &tizen_input_device_manager_interface, version);
        tizen_input_device_manager_add_listener(_eflutil.wl.devmgr.devicemgr, &_wl_tz_devmgr_listener, NULL);
     }
   else if (!strcmp(interface, "tizen_display_policy"))
     {
        _eflutil.wl.display_policy.proto = wl_registry_bind(reg, name, &tizen_display_policy_interface, version);
        if (!_eflutil.wl.display_policy.proto) return;

        tizen_display_policy_add_listener(_eflutil.wl.display_policy.proto,
                                          &_wl_tz_display_policy_listener,
                                          NULL);

        _eflutil.wl.display_policy.hash_brightness = eina_hash_pointer_new(free);
     }

}
/* LCOV_EXCL_START */
static void
_cb_wl_reg_global_remove(void *data,
                         struct wl_registry *reg,
                         unsigned int name)
{
   _eflutil.wl.policy.proto = NULL;
   eina_hash_free(_eflutil.wl.policy.hash_noti_lv);
   eina_hash_free(_eflutil.wl.policy.hash_scr_mode);

   _eflutil.wl.display_policy.proto = NULL;
   eina_hash_free(_eflutil.wl.display_policy.hash_brightness);
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
/* LCOV_EXCL_STOP */

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

/* LCOV_EXCL_START */
static void
_cb_wl_tz_policy_transient_for_done(void *data, struct tizen_policy *tizen_policy, uint32_t child_id)
{
}
/* LCOV_EXCL_STOP */

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

/* LCOV_EXCL_START */
static void                    _cb_wl_tz_policy_iconify_state_changed(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface_resource, uint32_t iconified, uint32_t force)
{
}

static void                    _cb_wl_tz_policy_supported_aux_hints(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface_resource, struct wl_array *hints, uint32_t num_hints)
{
}

static void                    _cb_wl_tz_policy_allowed_aux_hint(void *data, struct tizen_policy *tizen_policy, struct wl_surface *surface_resource, int id)
{
}
/* LCOV_EXCL_STOP */

static void
_cb_wl_tz_display_policy_brightness_done(void *data,
                                 struct tizen_display_policy *tizen_display_policy,
                                 struct wl_surface *surface,
                                 int32_t brightness,
                                 uint32_t state)
{
   Efl_Util_Wl_Surface_Brightness_Info *brightness_info;

   brightness_info = eina_hash_find(_eflutil.wl.display_policy.hash_brightness, &surface);
   if (brightness_info)
     {
        brightness_info->brightness = brightness;
        brightness_info->wait_for_done = EINA_FALSE;
        brightness_info->state = state;
     }
}

static void
_cb_window_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Efl_Util_Wl_Surface_Lv_Info *lv_info;

   lv_info = data;
   if (EINA_UNLIKELY(!lv_info))
     return;

   eina_hash_del(_eflutil.wl.policy.hash_noti_lv, &lv_info->surface, lv_info);
}

API int
efl_util_set_notification_window_level(Evas_Object *window,
                                       efl_util_notification_level_e level)
{
   Eina_Bool res;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL((level >= EFL_UTIL_NOTIFICATION_LEVEL_NONE) &&
                                   (level <= EFL_UTIL_NOTIFICATION_LEVEL_TOP),
                                   EFL_UTIL_ERROR_INVALID_PARAMETER);

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

        evas_object_event_callback_add(window, EVAS_CALLBACK_DEL,
                                       _cb_window_del, lv_info);
     }
   else
     {
        lv_info->level = (int)level;
        lv_info->wait_for_done = EINA_TRUE;
        lv_info->state = TIZEN_POLICY_ERROR_STATE_NONE;
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
}

API int
efl_util_get_notification_window_level(Evas_Object *window,
                                       efl_util_notification_level_e *level)
{
   Eina_Bool res;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_NULL_RETURN_VAL(level, EFL_UTIL_ERROR_INVALID_PARAMETER);

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
}

API int
efl_util_set_window_screen_mode(Evas_Object *window,
                                efl_util_screen_mode_e mode)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(((mode >= EFL_UTIL_SCREEN_MODE_DEFAULT) &&
                                    (mode <= EFL_UTIL_SCREEN_MODE_ALWAYS_ON)),
                                   EFL_UTIL_ERROR_INVALID_PARAMETER);

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
             scr_mode_info->state = TIZEN_POLICY_ERROR_STATE_NONE;

             eina_hash_add(_eflutil.wl.policy.hash_scr_mode,
                           &surface,
                           scr_mode_info);
          }
        else
          {
             scr_mode_info->mode = (unsigned int)mode;
             scr_mode_info->wait_for_done = EINA_TRUE;
             scr_mode_info->state = TIZEN_POLICY_ERROR_STATE_NONE;
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
}

API int
efl_util_get_window_screen_mode(Evas_Object *window,
                                efl_util_screen_mode_e *mode)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_NULL_RETURN_VAL(mode, EFL_UTIL_ERROR_INVALID_PARAMETER);

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
}

#ifndef TIZEN_WEARABLE
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
#endif

API int
efl_util_set_window_brightness(Evas_Object *window, int brightness)
{
   Ecore_Wl_Window *wlwin;
   struct wl_surface *surface;
   Efl_Util_Wl_Surface_Brightness_Info *brightness_info;
   Eina_Bool res;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(brightness <= 100, EFL_UTIL_ERROR_INVALID_PARAMETER);

   res = _wl_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   wlwin = elm_win_wl_window_get(window);
   if (wlwin)
     {
        while (!_eflutil.wl.display_policy.proto)
          wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

        surface = ecore_wl_window_surface_get(wlwin);
        EINA_SAFETY_ON_NULL_RETURN_VAL(surface,
                                       EFL_UTIL_ERROR_INVALID_PARAMETER);

        brightness_info = eina_hash_find(_eflutil.wl.display_policy.hash_brightness, &surface);
        if (!brightness_info)
          {
             brightness_info = calloc(1, sizeof(Efl_Util_Wl_Surface_Brightness_Info));
             EINA_SAFETY_ON_NULL_RETURN_VAL(brightness_info, EFL_UTIL_ERROR_OUT_OF_MEMORY);

             brightness_info->surface = surface;
             brightness_info->brightness = brightness;
             brightness_info->wait_for_done = EINA_TRUE;
             brightness_info->state = TIZEN_DISPLAY_POLICY_ERROR_STATE_NONE;

             eina_hash_add(_eflutil.wl.display_policy.hash_brightness,
                           &surface,
                           brightness_info);
           }
         else
           {
              brightness_info->brightness = brightness;
              brightness_info->wait_for_done = EINA_TRUE;
              brightness_info->state = TIZEN_DISPLAY_POLICY_ERROR_STATE_NONE;
           }

         tizen_display_policy_set_window_brightness(_eflutil.wl.display_policy.proto,
                                                    surface, brightness);
         if (brightness_info->wait_for_done)
           {
              int count = 0;
              while (brightness_info->wait_for_done && (count < 3))
                {
                   ecore_wl_flush();
                   wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);
                   count++;
                }

              if (brightness_info->wait_for_done)
                {
                   return EFL_UTIL_ERROR_INVALID_PARAMETER;
                }
              else
                {
                   if (brightness_info->state == TIZEN_DISPLAY_POLICY_ERROR_STATE_PERMISSION_DENIED)
                     {
                        return EFL_UTIL_ERROR_PERMISSION_DENIED;
                     }
                }
           }
        return EFL_UTIL_ERROR_NONE;
     }
   else
     return EFL_UTIL_ERROR_INVALID_PARAMETER;
}

API int
efl_util_get_window_brightness(Evas_Object *window, int *brightness)
{
   Ecore_Wl_Window *wlwin;
   struct wl_surface *surface;
   Efl_Util_Wl_Surface_Brightness_Info *brightness_info;
   Eina_Bool res;

   EINA_SAFETY_ON_NULL_RETURN_VAL(window, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_NULL_RETURN_VAL(brightness, EFL_UTIL_ERROR_INVALID_PARAMETER);

   res = _wl_init();
   EINA_SAFETY_ON_FALSE_RETURN_VAL(res, EFL_UTIL_ERROR_INVALID_PARAMETER);

   wlwin = elm_win_wl_window_get(window);
   if (!wlwin) return EFL_UTIL_ERROR_INVALID_PARAMETER;

   while (!_eflutil.wl.display_policy.proto)
     wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

   surface = ecore_wl_window_surface_get(wlwin);
   EINA_SAFETY_ON_NULL_RETURN_VAL(surface,
                                  EFL_UTIL_ERROR_INVALID_PARAMETER);

   brightness_info = eina_hash_find(_eflutil.wl.display_policy.hash_brightness, &surface);
   if (brightness_info)
     {
        if (brightness_info->wait_for_done)
          {
             while (brightness_info->wait_for_done)
               {
                  ecore_wl_flush();
                  wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);
               }
          }
         *brightness = brightness_info->brightness;
     }
   else
     *brightness = -1;

   return EFL_UTIL_ERROR_NONE;
}


struct _efl_util_inputgen_h
{
   unsigned int init_type;
};

static void
_cb_device_add(void *data EINA_UNUSED,
               struct tizen_input_device_manager *tizen_input_device_manager EINA_UNUSED,
               uint32_t serial EINA_UNUSED,
               const char *identifier EINA_UNUSED,
               struct tizen_input_device *device EINA_UNUSED,
               struct wl_seat *seat EINA_UNUSED)
{
   ;
}

/* LCOV_EXCL_START */
static void
_cb_device_remove(void *data EINA_UNUSED,
               struct tizen_input_device_manager *tizen_input_device_manager EINA_UNUSED,
               uint32_t serial  EINA_UNUSED,
               const char *identifier  EINA_UNUSED,
               struct tizen_input_device *device  EINA_UNUSED,
               struct wl_seat *seat  EINA_UNUSED)
{
   ;
}
/* LCOV_EXCL_STOP */

static void
_cb_error(void *data EINA_UNUSED,
          struct tizen_input_device_manager *tizen_input_device_manager EINA_UNUSED,
          uint32_t errorcode)
{
   _eflutil.wl.devmgr.request_notified = errorcode;
}

/* LCOV_EXCL_START */
static void
_cb_block_expired(void *data EINA_UNUSED,
                  struct tizen_input_device_manager *tizen_input_device_manager EINA_UNUSED)
{
   ;
}
/* LCOV_EXCL_STOP */

static efl_util_error_e
_efl_util_input_convert_input_generator_error(int ret)
{
   switch (ret)
     {
        case TIZEN_INPUT_DEVICE_MANAGER_ERROR_NONE:
           return EFL_UTIL_ERROR_NONE;
        case TIZEN_INPUT_DEVICE_MANAGER_ERROR_NO_PERMISSION:
           return EFL_UTIL_ERROR_PERMISSION_DENIED;
        case TIZEN_INPUT_DEVICE_MANAGER_ERROR_NO_SYSTEM_RESOURCES:
           return EFL_UTIL_ERROR_OUT_OF_MEMORY;
        case TIZEN_INPUT_DEVICE_MANAGER_ERROR_INVALID_PARAMETER:
           return EFL_UTIL_ERROR_INVALID_PARAMETER;
        default :
           return EFL_UTIL_ERROR_NONE;
     }
}

API efl_util_inputgen_h
efl_util_input_initialize_generator(efl_util_input_device_type_e dev_type)
{
   int ret = EFL_UTIL_ERROR_NONE;
   efl_util_inputgen_h inputgen_h = NULL;

   if ((dev_type <= EFL_UTIL_INPUT_DEVTYPE_NONE) ||
       (dev_type >= EFL_UTIL_INPUT_DEVTYPE_MAX))
     {
        set_last_result(EFL_UTIL_ERROR_NO_SUCH_DEVICE);
        goto out;
     }

   inputgen_h = (efl_util_inputgen_h)calloc(1, sizeof(struct _efl_util_inputgen_h));
   if (!inputgen_h)
     {
        set_last_result(EFL_UTIL_ERROR_OUT_OF_MEMORY);
        goto out;
     }

   inputgen_h->init_type |= dev_type;

   ret = _wl_init();
   if (ret == (int)EINA_FALSE)
     {
        set_last_result(EFL_UTIL_ERROR_INVALID_PARAMETER);
        goto out;
     }

   while (!_eflutil.wl.devmgr.devicemgr)
     wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

   tizen_input_device_manager_init_generator(_eflutil.wl.devmgr.devicemgr);

   while (_eflutil.wl.devmgr.request_notified == -1)
     wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

   ret = _efl_util_input_convert_input_generator_error(_eflutil.wl.devmgr.request_notified);
   _eflutil.wl.devmgr.request_notified = -1;

   set_last_result(ret);
   if (ret != TIZEN_INPUT_DEVICE_MANAGER_ERROR_NONE)
     goto out;

   return inputgen_h;

out:
   if (inputgen_h)
     {
        free(inputgen_h);
        inputgen_h = NULL;
     }
   return NULL;
}

API int
efl_util_input_deinitialize_generator(efl_util_inputgen_h inputgen_h)
{
   int ret = EFL_UTIL_ERROR_NONE;
   EINA_SAFETY_ON_NULL_RETURN_VAL(inputgen_h, EFL_UTIL_ERROR_INVALID_PARAMETER);

   free(inputgen_h);
   inputgen_h = NULL;

   EINA_SAFETY_ON_NULL_RETURN_VAL(_eflutil.wl.devmgr.devicemgr, EFL_UTIL_ERROR_INVALID_PARAMETER);

   tizen_input_device_manager_deinit_generator(_eflutil.wl.devmgr.devicemgr);

   while (_eflutil.wl.devmgr.request_notified == -1)
     wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

   ret = _efl_util_input_convert_input_generator_error(_eflutil.wl.devmgr.request_notified);
   _eflutil.wl.devmgr.request_notified = -1;

   return ret;
}

API int
efl_util_input_generate_key(efl_util_inputgen_h inputgen_h, const char *key_name, int pressed)
{
   int ret = EFL_UTIL_ERROR_NONE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(inputgen_h, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_NULL_RETURN_VAL(key_name, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(pressed == 0 || pressed == 1, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(inputgen_h->init_type & EFL_UTIL_INPUT_DEVTYPE_KEYBOARD, EFL_UTIL_ERROR_NO_SUCH_DEVICE);

   EINA_SAFETY_ON_NULL_RETURN_VAL(_eflutil.wl.devmgr.devicemgr, EFL_UTIL_ERROR_INVALID_PARAMETER);

   tizen_input_device_manager_generate_key(_eflutil.wl.devmgr.devicemgr, key_name, pressed);

   while (_eflutil.wl.devmgr.request_notified == -1)
     wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

   ret = _efl_util_input_convert_input_generator_error(_eflutil.wl.devmgr.request_notified);
   _eflutil.wl.devmgr.request_notified = -1;

   return ret;
}

API int
efl_util_input_generate_touch(efl_util_inputgen_h inputgen_h, int idx,
                              efl_util_input_touch_type_e touch_type, int x, int y)
{
   int ret;
   enum tizen_input_device_manager_pointer_event_type type;

   EINA_SAFETY_ON_NULL_RETURN_VAL(inputgen_h, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(idx >= 0, EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL((x > 0 && y > 0), EFL_UTIL_ERROR_INVALID_PARAMETER);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(inputgen_h->init_type & EFL_UTIL_INPUT_DEVTYPE_TOUCHSCREEN, EFL_UTIL_ERROR_NO_SUCH_DEVICE);

   EINA_SAFETY_ON_NULL_RETURN_VAL(_eflutil.wl.devmgr.devicemgr, EFL_UTIL_ERROR_INVALID_PARAMETER);

   switch(touch_type)
     {
        case EFL_UTIL_INPUT_TOUCH_BEGIN:
           type = TIZEN_INPUT_DEVICE_MANAGER_POINTER_EVENT_TYPE_BEGIN;
           break;
        case EFL_UTIL_INPUT_TOUCH_UPDATE:
           type = TIZEN_INPUT_DEVICE_MANAGER_POINTER_EVENT_TYPE_UPDATE;
           break;
        case EFL_UTIL_INPUT_TOUCH_END:
           type = TIZEN_INPUT_DEVICE_MANAGER_POINTER_EVENT_TYPE_END;
           break;
        default:
           return EFL_UTIL_ERROR_INVALID_PARAMETER;
     }

   tizen_input_device_manager_generate_touch(_eflutil.wl.devmgr.devicemgr, type, x, y, idx);

   while (_eflutil.wl.devmgr.request_notified == -1)
     wl_display_dispatch_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

   ret = _efl_util_input_convert_input_generator_error(_eflutil.wl.devmgr.request_notified);
   _eflutil.wl.devmgr.request_notified = -1;

   return ret;
}

struct _efl_util_screenshot_h
{
   int width;
   int height;

   Eina_Bool shot_done;

   /* tbm bufmgr */
   tbm_bufmgr bufmgr;
};

/* scrrenshot handle */
static efl_util_screenshot_h g_screenshot;

API efl_util_screenshot_h
efl_util_screenshot_initialize(int width, int height)
{
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

fail_param:
   if (screenshot)
     efl_util_screenshot_deinitialize(screenshot);
   set_last_result(EFL_UTIL_ERROR_INVALID_PARAMETER);
   return NULL;
fail_memory:
/* LCOV_EXCL_START */
   set_last_result(EFL_UTIL_ERROR_OUT_OF_MEMORY);
   return NULL;
/* LCOV_EXCL_STOP */
fail_init:
   if (screenshot)
     efl_util_screenshot_deinitialize(screenshot);
   set_last_result(EFL_UTIL_ERROR_SCREENSHOT_INIT_FAIL);
   return NULL;
}

API int
efl_util_screenshot_deinitialize(efl_util_screenshot_h screenshot)
{
   if (!screenshot)
     return EFL_UTIL_ERROR_INVALID_PARAMETER;

   free(screenshot);
   g_screenshot = NULL;

   if (_eflutil.wl.shot.screenshooter)
     screenshooter_set_user_data(_eflutil.wl.shot.screenshooter, NULL);

   return EFL_UTIL_ERROR_NONE;
}


API tbm_surface_h
efl_util_screenshot_take_tbm_surface(efl_util_screenshot_h screenshot)
{
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
        fprintf(stderr, "[screenshot] fail: no output for screenshot\n"); /* LCOV_EXCL_LINE */
        goto fail;
     }

   t_surface = tbm_surface_create(screenshot->width, screenshot->height, TBM_FORMAT_XRGB8888);
   if (!t_surface)
     {
        fprintf(stderr, "[screenshot] fail: tbm_surface_create\n"); /* LCOV_EXCL_LINE */
        goto fail;
     }

   buffer = wayland_tbm_client_create_buffer(_eflutil.wl.shot.tbm_client, t_surface);
   if (!buffer)
     {
        fprintf(stderr, "[screenshot] fail: create wl_buffer for screenshot\n"); /* LCOV_EXCL_LINE */
        goto fail;
     }

   screenshooter_shoot(_eflutil.wl.shot.screenshooter, output->output, buffer);

   screenshot->shot_done = EINA_FALSE;
   while (!screenshot->shot_done && ret != -1)
     ret = wl_display_roundtrip_queue(_eflutil.wl.dpy, _eflutil.wl.queue);

   if (ret == -1)
     {
        fprintf(stderr, "[screenshot] fail: screenshooter_shoot\n"); /* LCOV_EXCL_LINE */
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
}
