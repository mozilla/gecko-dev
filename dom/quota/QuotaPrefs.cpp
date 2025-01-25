/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaPrefs.h"

#include "mozilla/StaticPrefs_dom.h"
#include "prenv.h"

// The STATIC_PREF macro helps avoid lines exceeding 80 characters due to
// long method names generated from StaticPrefList.yaml. It constructs
// method names by concatenating components of the preference path.
#define STATIC_PREF(b1, b2, b3, b4) \
  StaticPrefs::b1##_##b2##_##b3##_##b4##_DoNotUseDirectly

namespace mozilla::dom::quota {

// static
bool QuotaPrefs::LazyOriginInitializationEnabled() {
  return IncrementalOriginInitializationEnabled() ||
         STATIC_PREF(dom, quotaManager, temporaryStorage,
                     lazyOriginInitialization)();
}

// static
bool QuotaPrefs::TriggerOriginInitializationInBackgroundEnabled() {
  return IncrementalOriginInitializationEnabled() ||
         STATIC_PREF(dom, quotaManager, temporaryStorage,
                     triggerOriginInitializationInBackground)();
}

// static
bool QuotaPrefs::IncrementalOriginInitializationEnabled() {
  if (STATIC_PREF(dom, quotaManager, temporaryStorage,
                  incrementalOriginInitialization)()) {
    return true;
  }

  const char* env = PR_GetEnv("MOZ_ENABLE_INC_ORIGIN_INIT");
  return (env && *env == '1');
}

}  // namespace mozilla::dom::quota
