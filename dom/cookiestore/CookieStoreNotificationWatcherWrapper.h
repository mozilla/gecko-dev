/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CookieStoreNotificationWatcherWrapper_h
#define mozilla_dom_CookieStoreNotificationWatcherWrapper_h

#include "mozilla/OriginAttributes.h"

namespace mozilla::dom {

class Promise;
class CookieStore;
class CookieStoreNotificationWatcher;

class CookieStoreNotificationWatcherWrapper final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CookieStoreNotificationWatcherWrapper)

  static already_AddRefed<CookieStoreNotificationWatcherWrapper> Create(
      CookieStore* aCookieStore);

  void ResolvePromiseWhenNotified(const nsID& aOperationID, Promise* aPromise);
  void ForgetOperationID(const nsID& aOperationID);

 private:
  CookieStoreNotificationWatcherWrapper() = default;
  ~CookieStoreNotificationWatcherWrapper();

  void CreateWatcherOnMainThread(bool aPrivateBrowsing);

  RefPtr<CookieStoreNotificationWatcher> mWatcherOnMainThread;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_CookieStoreNotificationWatcherWrapper_h */
