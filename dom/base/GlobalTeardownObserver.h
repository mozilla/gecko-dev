/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_BASE_GLOBALTEARDOWNOBSERVER_H_
#define DOM_BASE_GLOBALTEARDOWNOBSERVER_H_

#include "mozilla/Attributes.h"
#include "nsIGlobalObject.h"
#include "nsIScriptGlobalObject.h"

namespace mozilla {

class GlobalTeardownObserver

    : public nsISupports,
      public LinkedListElement<GlobalTeardownObserver> {
 public:
  GlobalTeardownObserver();
  explicit GlobalTeardownObserver(nsIGlobalObject* aGlobalObject,
                                  bool aHasOrHasHadOwnerWindow = false);

  nsGlobalWindowInner* GetOwnerWindow() const;
  nsIGlobalObject* GetOwnerGlobal() const { return mParentObject; }
  bool HasOrHasHadOwnerWindow() const { return mHasOrHasHadOwnerWindow; }

  void GetParentObject(nsIScriptGlobalObject** aParentObject) {
    if (mParentObject) {
      CallQueryInterface(mParentObject, aParentObject);
    } else {
      *aParentObject = nullptr;
    }
  }

  virtual void DisconnectFromOwner();

  // A global permanently becomes invalid when DisconnectEventTargetObjects() is
  // called.  Normally this means:
  // - For the main thread, when nsGlobalWindowInner::FreeInnerObjects is
  //   called.
  // - For a worker thread, when clearing the main event queue.  (Which we do
  //   slightly later than when the spec notionally calls for it to be done.)
  //
  // A global may also become temporarily invalid when:
  // - For the main thread, if the window is no longer the WindowProxy's current
  //   inner window due to being placed in the bfcache.
  nsresult CheckCurrentGlobalCorrectness() const;

 protected:
  virtual ~GlobalTeardownObserver();

  void BindToOwner(nsIGlobalObject* aOwner);

 private:
  // The parent global object.  The global will clear this when
  // it is destroyed by calling DisconnectFromOwner().
  nsIGlobalObject* MOZ_NON_OWNING_REF mParentObject = nullptr;
  // If mParentObject is or has been an inner window, then this is true. It is
  // obtained in BindToOwner.
  bool mHasOrHasHadOwnerWindow = false;
};

}  // namespace mozilla

#endif
