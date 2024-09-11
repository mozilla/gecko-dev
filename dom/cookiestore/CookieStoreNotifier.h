/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CookieStoreNotifier_h
#define mozilla_dom_CookieStoreNotifier_h

#include "nsIObserver.h"
#include "mozilla/OriginAttributes.h"

class nsISerialEventTarget;

namespace mozilla::dom {

class CookieStore;
struct CookieListItem;
class Event;

class CookieStoreNotifier final : public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static already_AddRefed<CookieStoreNotifier> Create(
      CookieStore* aCookieStore);

  void Disentangle();

  void FireDelayedDOMEvents();

 private:
  CookieStoreNotifier(CookieStore* aCookieStore, const nsACString& aBaseDomain,
                      const OriginAttributes& aOriginAttributes);
  ~CookieStoreNotifier();

  void AddObserversOnMainThread(bool aPrivateBrowsing);
  void RemoveObserversOnMainThread(bool aPrivateBrowsing);

  // Raw pointer because this object is kept alive by this CookieStore object.
  CookieStore* mCookieStore;

  nsCString mBaseDomain;
  OriginAttributes mOriginAttributes;

  RefPtr<nsISerialEventTarget> mEventTarget;

  nsTArray<RefPtr<Event>> mDelayedDOMEvents;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_CookieStoreNotifier_h */
