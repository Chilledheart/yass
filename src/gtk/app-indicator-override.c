// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>

extern int app_indicator_init(void);

extern int app_indicator_get_type(void);
extern void* app_indicator_new(const char*, const char*, int);
extern void* app_indicator_new_with_path(const char*, const char*, int, const char*);
extern void app_indicator_set_status(void*, int);
extern void app_indicator_set_menu(void*, void*);
extern void app_indicator_set_secondary_activate_target(void*, void*);

static int (*original_indicator_get_type)(void);
static void* (*original_indicator_new)(const char*, const char*, int);
static void* (*original_indicator_new_with_path)(const char*, const char*, int, const char*);
static void (*original_set_status)(void*, int);
static void (*original_set_menu)(void*, void*);
static void (*original_set_secondary_activate_target)(void*, void*);

static void* app_indicator_lib;

int app_indicator_init(void) {
  if (app_indicator_lib != NULL) {
    dlclose(app_indicator_lib);
  }
  app_indicator_lib = dlopen("libappindicator3.so.1", RTLD_LAZY);
  return app_indicator_lib == NULL ? -1 : 0;
}

int app_indicator_get_type(void) {
  assert(app_indicator_lib && "app_indicator_init is required before any app indicator call");
  if (!original_indicator_get_type)
    original_indicator_get_type = dlsym(app_indicator_lib, "app_indicator_get_type");
  assert(original_indicator_get_type && "symbol app_indicator_new not found");

  return original_indicator_get_type();
}

void* app_indicator_new(const char* id, const char* icon_name, int category) {
  assert(app_indicator_lib && "app_indicator_init is required before any app indicator call");
  if (!original_indicator_new)
    original_indicator_new = dlsym(app_indicator_lib, "app_indicator_new");
  assert(original_indicator_new && "symbol app_indicator_new not found");

  return original_indicator_new(id, icon_name, category);
}

void* app_indicator_new_with_path(const char* id, const char* icon_name, int category, const char* icon_theme_path) {
  assert(app_indicator_lib && "app_indicator_init is required before any app indicator call");
  if (!original_indicator_new_with_path)
    original_indicator_new_with_path = dlsym(app_indicator_lib, "app_indicator_new_with_path");
  assert(original_indicator_new_with_path && "symbol app_indicator_new_with_path not found");

  return original_indicator_new_with_path(id, icon_name, category, NULL);
}

void app_indicator_set_status(void* indicator, int status) {
  assert(app_indicator_lib && "app_indicator_init is required before any app indicator call");
  if (!original_set_status)
    original_set_status = dlsym(app_indicator_lib, "app_indicator_set_status");
  assert(original_set_status && "symbol app_indicator_set_status not found");

  original_set_status(indicator, status);
}

void app_indicator_set_menu(void* indicator, void* menu) {
  assert(app_indicator_lib && "app_indicator_init is required before any app indicator call");
  if (!original_set_menu)
    original_set_menu = dlsym(app_indicator_lib, "app_indicator_set_menu");
  assert(original_set_menu && "symbol app_indicator_set_menu not found");

  original_set_menu(indicator, menu);
}

void app_indicator_set_secondary_activate_target(void* indicator, void* menuitem) {
  assert(app_indicator_lib && "app_indicator_init is required before any app indicator call");
  if (!original_set_secondary_activate_target)
    original_set_secondary_activate_target = dlsym(app_indicator_lib, "app_indicator_set_secondary_activate_target");
  assert(original_set_secondary_activate_target && "symbol app_indicator_set_secondary_activate_target not found");

  original_set_secondary_activate_target(indicator, menuitem);
}
