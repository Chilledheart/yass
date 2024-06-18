// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>

extern int app_indicator_init(void);
extern void app_indicator_uninit(void);

extern int app_indicator_get_type(void);
extern void* app_indicator_new(const char*, const char*, int);
extern void* app_indicator_new_with_path(const char*, const char*, int, const char*);
extern void app_indicator_set_status(void*, int);
extern void app_indicator_set_menu(void*, void*);
extern void app_indicator_set_secondary_activate_target(void*, void*);

static int (*o_app_indicator_get_type)(void);
static void* (*o_app_indicator_new)(const char*, const char*, int);
static void* (*o_app_indicator_new_with_path)(const char*, const char*, int, const char*);
static void (*o_app_indicator_set_status)(void*, int);
static void (*o_app_indicator_set_menu)(void*, void*);
static void (*o_app_indicator_set_secondary_activate_target)(void*, void*);

static void* app_indicator_lib;

int app_indicator_init(void) {
  if (app_indicator_lib != NULL) {
    return 0;
  }
  app_indicator_lib = dlopen("libayatana-appindicator3.so.1", RTLD_LAZY);
  if (app_indicator_lib == NULL) {
    app_indicator_lib = dlopen("libappindicator3.so.1", RTLD_LAZY);
  }
  if (app_indicator_lib != NULL) {
    o_app_indicator_get_type = dlsym(app_indicator_lib, "app_indicator_get_type");
    o_app_indicator_new = dlsym(app_indicator_lib, "app_indicator_new");
    o_app_indicator_new_with_path = dlsym(app_indicator_lib, "app_indicator_new_with_path");
    o_app_indicator_set_status = dlsym(app_indicator_lib, "app_indicator_set_status");
    o_app_indicator_set_menu = dlsym(app_indicator_lib, "app_indicator_set_menu");
    o_app_indicator_set_secondary_activate_target =
        dlsym(app_indicator_lib, "app_indicator_set_secondary_activate_target");
    if (o_app_indicator_get_type == NULL || o_app_indicator_new == NULL || o_app_indicator_new_with_path == NULL ||
        o_app_indicator_set_status == NULL || o_app_indicator_set_menu == NULL ||
        o_app_indicator_set_secondary_activate_target == NULL) {
      app_indicator_uninit();
    }
  }
  return app_indicator_lib == NULL ? -1 : 0;
}

void app_indicator_uninit(void) {
  if (app_indicator_lib != NULL) {
    o_app_indicator_get_type = NULL;
    o_app_indicator_new = NULL;
    o_app_indicator_new_with_path = NULL;
    o_app_indicator_set_status = NULL;
    o_app_indicator_set_menu = NULL;
    o_app_indicator_set_secondary_activate_target = NULL;
    dlclose(app_indicator_lib);
    app_indicator_lib = NULL;
  }
}

int app_indicator_get_type(void) {
  assert(o_app_indicator_get_type && "app_indicator_init is required to call first");

  return o_app_indicator_get_type();
}

void* app_indicator_new(const char* id, const char* icon_name, int category) {
  assert(o_app_indicator_new && "app_indicator_init is required to call first");

  return o_app_indicator_new(id, icon_name, category);
}

void* app_indicator_new_with_path(const char* id, const char* icon_name, int category, const char* icon_theme_path) {
  assert(o_app_indicator_new_with_path && "app_indicator_init is required to call first");

  return o_app_indicator_new_with_path(id, icon_name, category, NULL);
}

void app_indicator_set_status(void* indicator, int status) {
  assert(o_app_indicator_set_status && "app_indicator_init is required to call first");

  o_app_indicator_set_status(indicator, status);
}

void app_indicator_set_menu(void* indicator, void* menu) {
  assert(o_app_indicator_set_menu && "app_indicator_init is required to call first");

  o_app_indicator_set_menu(indicator, menu);
}

void app_indicator_set_secondary_activate_target(void* indicator, void* menuitem) {
  assert(o_app_indicator_set_secondary_activate_target && "app_indicator_init is required to call first");

  o_app_indicator_set_secondary_activate_target(indicator, menuitem);
}
