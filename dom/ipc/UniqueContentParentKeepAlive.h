/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UniqueContentParentKeepAlive_h
#define mozilla_dom_UniqueContentParentKeepAlive_h

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsIDOMProcessParent.h"

namespace mozilla::dom {

class ContentParent;
class ThreadsafeContentParentHandle;

struct ContentParentKeepAliveDeleter {
  void operator()(ContentParent* aProcess);
  void operator()(ThreadsafeContentParentHandle* aHandle);
  uint64_t mBrowserId = 0;
};

// Helper for managing a ContentParent combined with the KeepAlive which is
// keeping it alive for use by a specific BrowserId.
//
// This generally should not be created directly, but rather should be created
// using `ContentParent::AddKeepAlive`.
using UniqueContentParentKeepAlive =
    UniquePtr<ContentParent, ContentParentKeepAliveDeleter>;

using UniqueThreadsafeContentParentKeepAlive =
    UniquePtr<ThreadsafeContentParentHandle, ContentParentKeepAliveDeleter>;

UniqueContentParentKeepAlive UniqueContentParentKeepAliveFromThreadsafe(
    UniqueThreadsafeContentParentKeepAlive aKeepAlive);
UniqueThreadsafeContentParentKeepAlive UniqueContentParentKeepAliveToThreadsafe(
    UniqueContentParentKeepAlive aKeepAlive);

// Wrap a UniqueContentParentKeepAlive to make it usable from JS.
//
// Should not be called on a KeepAlive for a still-launching ContentParent.
already_AddRefed<nsIContentParentKeepAlive> WrapContentParentKeepAliveForJS(
    UniqueContentParentKeepAlive aKeepAlive);

}  // namespace mozilla::dom

#endif  // mozilla_dom_UniqueContentParentKeepAlive_h
