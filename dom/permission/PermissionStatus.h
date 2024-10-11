/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PermissionStatus_h_
#define mozilla_dom_PermissionStatus_h_

#include "mozilla/dom/PermissionsBinding.h"
#include "mozilla/dom/PermissionStatusBinding.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/MozPromise.h"

namespace mozilla::dom {

class PermissionStatusSink;

class PermissionStatus : public DOMEventTargetHelper {
  friend class PermissionStatusSink;

 public:
  using SimplePromise = MozPromise<nsresult, nsresult, true>;

  PermissionStatus(nsIGlobalObject* aGlobal, PermissionName aName);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  PermissionState State() const { return mState; }
  void SetState(PermissionState aState) { mState = aState; }

  IMPL_EVENT_HANDLER(change)

  void DisconnectFromOwner() override;

  PermissionName Name() const { return mName; }

  void GetType(nsACString& aName) const;

  RefPtr<SimplePromise> Init();

 protected:
  ~PermissionStatus();

  /**
   * This method returns the internal permission type, which should be equal to
   * the permission name for all but the MIDI permission because of the SysEx
   * support: internally, we have both "midi" and "midi-sysex" permission types
   * but we only have a "midi" (public) permission name.
   *
   * Note: the `MidiPermissionDescriptor` descriptor has an optional `sysex`
   * boolean, which is used to determine whether to return "midi" or
   * "midi-sysex" for the MIDI permission.
   */
  virtual nsLiteralCString GetPermissionType() const;

 private:
  virtual already_AddRefed<PermissionStatusSink> CreateSink();

  void PermissionChanged(uint32_t aAction);

  PermissionState ComputeStateFromAction(uint32_t aAction);

  PermissionName mName;
  RefPtr<PermissionStatusSink> mSink;

 protected:
  PermissionState mState;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_permissionstatus_h_
