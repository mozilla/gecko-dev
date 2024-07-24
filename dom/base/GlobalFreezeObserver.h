/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_BASE_GLOBALFREEZEOBSERVER_H_
#define DOM_BASE_GLOBALFREEZEOBSERVER_H_

#include "mozilla/Attributes.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"

namespace mozilla {

class GlobalFreezeObserver : public nsISupports,
                             public LinkedListElement<GlobalFreezeObserver> {
 public:
  virtual void FrozenCallback(nsIGlobalObject* aOwner) = 0;
  virtual void ThawedCallback(nsIGlobalObject* aOwner) {};

  bool Observing() { return !!mOwner; }

  /**
   * This method is non-virtual because it's expected that any object
   * subclassing GlobalFreezeObserver that wants to know when it is disconnected
   * from the global will also subclass GlobalTeardownObserver and take any
   * relevant action by overriding GlobalTeardownObserver::DisconnectFromOwner.
   */
  void DisconnectFreezeObserver() {
    if (mOwner) {
      mOwner->RemoveGlobalFreezeObserver(this);
      mOwner = nullptr;
    }
  }

 protected:
  virtual ~GlobalFreezeObserver() { DisconnectFreezeObserver(); }

  void BindToOwner(nsIGlobalObject* aOwner) {
    MOZ_ASSERT(!mOwner);

    if (aOwner) {
      MOZ_ASSERT(
          NS_IsMainThread(),
          "GlobalFreezeObserver is currently only supported in window object");
      mOwner = aOwner;
      aOwner->AddGlobalFreezeObserver(this);
    }
  }

 private:
  // The parent global object. The global will clear this when
  // it is destroyed by calling DisconnectFreezeObserver().
  nsIGlobalObject* MOZ_NON_OWNING_REF mOwner = nullptr;
};

}  // namespace mozilla

#endif
