/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotifyUtils.h"

#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/quota/NotifyUtilsCommon.h"

namespace mozilla::dom::localstorage {

void NotifyDatabaseWorkStarted() {
  if (!StaticPrefs::dom_storage_testing()) {
    return;
  }

  quota::NotifyObserversOnMainThread("LocalStorage::DatabaseWorkStarted");
}

void NotifyRequestFinalizationStarted() {
  if (!StaticPrefs::dom_storage_testing()) {
    return;
  }

  quota::NotifyObserversOnMainThread(
      "LocalStorage::RequestFinalizationStarted");
}

}  // namespace mozilla::dom::localstorage
