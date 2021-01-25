// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */

#include "gui/utils.hpp"
#ifdef __APPLE__

#include <AvailabilityMacros.h>
#import <Cocoa/Cocoa.h>

// original idea come from growl framework
// http://growl.info/about
static bool get_yass_auto_start() {
  NSURL *itemURL = [[NSBundle mainBundle] bundleURL];
  CFURLRef URLToToggle = (__bridge CFURLRef)itemURL;

  bool found = false;

  LSSharedFileListRef loginItems =
      LSSharedFileListCreate(NULL, kLSSharedFileListSessionLoginItems, NULL);
  if (loginItems) {
    UInt32 seed = 0U;
    CFArrayRef currentLoginItems =
        LSSharedFileListCopySnapshot(loginItems, &seed);
    const CFIndex count = CFArrayGetCount(currentLoginItems);
    for (CFIndex idx = 0; idx < count; ++idx) {
      LSSharedFileListItemRef item =
          (LSSharedFileListItemRef)CFArrayGetValueAtIndex(currentLoginItems,
                                                          idx);
      CFURLRef outURL = NULL;

      const UInt32 resolutionFlags = kLSSharedFileListNoUserInteraction |
                                     kLSSharedFileListDoNotMountVolumes;
      outURL = LSSharedFileListItemCopyResolvedURL(item, resolutionFlags,
                                                   /*outError*/ NULL);
      if (outURL == NULL) {
        if (outURL)
          CFRelease(outURL);
        continue;
      }
      found = CFEqual(outURL, URLToToggle);
      CFRelease(outURL);

      if (found)
        break;
    }
    CFRelease(currentLoginItems);
    CFRelease(loginItems);
  }
  return found;
}

static void set_yass_auto_start(bool enabled) {
  NSURL *itemURL = [[NSBundle mainBundle] bundleURL];
  CFURLRef URLToToggle = (__bridge CFURLRef)itemURL;

  LSSharedFileListRef loginItems = LSSharedFileListCreate(
      kCFAllocatorDefault, kLSSharedFileListSessionLoginItems,
      /*options*/ NULL);
  if (loginItems) {
    UInt32 seed = 0U;
    Boolean found;
    LSSharedFileListItemRef existingItem = NULL;

    CFArrayRef currentLoginItems =
        LSSharedFileListCopySnapshot(loginItems, &seed);
    const CFIndex count = CFArrayGetCount(currentLoginItems);
    for (CFIndex idx = 0; idx < count; ++idx) {
      LSSharedFileListItemRef item =
          (LSSharedFileListItemRef)CFArrayGetValueAtIndex(currentLoginItems,
                                                          idx);
      CFURLRef outURL = NULL;

      const UInt32 resolutionFlags = kLSSharedFileListNoUserInteraction |
                                     kLSSharedFileListDoNotMountVolumes;
      outURL = LSSharedFileListItemCopyResolvedURL(item, resolutionFlags,
                                                   /*outError*/ NULL);
      if (outURL == NULL) {
        if (outURL)
          CFRelease(outURL);
        continue;
      }
      found = CFEqual(outURL, URLToToggle);
      CFRelease(outURL);

      if (found) {
        existingItem = item;
        break;
      }
    }

    if (enabled && !found) {
      NSString *displayName = @"" DEFAULT_AUTOSTART_NAME;

      LSSharedFileListItemRef newItem = LSSharedFileListInsertItemURL(
          loginItems, kLSSharedFileListItemBeforeFirst,
          (__bridge CFStringRef)displayName,
          /*icon*/ NULL, URLToToggle,
          /*propertiesToSet*/ NULL, /*propertiesToClear*/ NULL);
      if (newItem)
        CFRelease(newItem);
    } else if (!enabled && found) {
      LSSharedFileListItemRemove(loginItems, existingItem);
    }

    CFRelease(currentLoginItems);
    CFRelease(loginItems);
  }
}

bool Utils::GetAutoStart() { return get_yass_auto_start(); }

void Utils::EnableAutoStart(bool on) { set_yass_auto_start(on); }

#endif // __APPLE__
