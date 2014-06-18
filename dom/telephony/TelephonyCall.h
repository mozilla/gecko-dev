/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephonycall_h__
#define mozilla_dom_telephony_telephonycall_h__

#include "mozilla/dom/telephony/TelephonyCommon.h"

#include "mozilla/dom/DOMError.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class TelephonyCall MOZ_FINAL : public DOMEventTargetHelper
{
  nsRefPtr<Telephony> mTelephony;
  nsRefPtr<TelephonyCallGroup> mGroup;

  uint32_t mServiceId;
  nsString mNumber;
  nsString mSecondNumber;
  nsString mState;
  bool mEmergency;
  nsRefPtr<DOMError> mError;
  bool mSwitchable;
  bool mMergeable;

  uint32_t mCallIndex;
  uint16_t mCallState;
  bool mLive;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TelephonyCall,
                                           DOMEventTargetHelper)

  friend class Telephony;

  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  void
  GetNumber(nsString& aNumber) const
  {
    aNumber.Assign(mNumber);
  }

  void
  GetSecondNumber(nsString& aSecondNumber) const
  {
    aSecondNumber.Assign(mSecondNumber);
  }

  void
  GetState(nsString& aState) const
  {
    aState.Assign(mState);
  }

  bool
  Emergency() const
  {
    return mEmergency;
  }

  bool
  Switchable() const
  {
    return mSwitchable;
  }

  bool
  Mergeable() const
  {
    return mMergeable;
  }

  already_AddRefed<DOMError>
  GetError() const;

  already_AddRefed<TelephonyCallGroup>
  GetGroup() const;

  void
  Answer(ErrorResult& aRv);

  void
  HangUp(ErrorResult& aRv);

  void
  Hold(ErrorResult& aRv);

  void
  Resume(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(dialing)
  IMPL_EVENT_HANDLER(alerting)
  IMPL_EVENT_HANDLER(connecting)
  IMPL_EVENT_HANDLER(connected)
  IMPL_EVENT_HANDLER(disconnecting)
  IMPL_EVENT_HANDLER(disconnected)
  IMPL_EVENT_HANDLER(holding)
  IMPL_EVENT_HANDLER(held)
  IMPL_EVENT_HANDLER(resuming)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(groupchange)

  static already_AddRefed<TelephonyCall>
  Create(Telephony* aTelephony, uint32_t aServiceId,
         const nsAString& aNumber, uint16_t aCallState, uint32_t aCallIndex,
         bool aEmergency = false, bool aIsConference = false,
         bool aSwitchable = true, bool aMergeable = true);

  void
  ChangeState(uint16_t aCallState)
  {
    ChangeStateInternal(aCallState, true);
  }

  uint32_t
  ServiceId() const
  {
    return mServiceId;
  }

  uint32_t
  CallIndex() const
  {
    return mCallIndex;
  }

  uint16_t
  CallState() const
  {
    return mCallState;
  }

  void
  UpdateEmergency(bool aEmergency)
  {
    mEmergency = aEmergency;
  }

  void
  UpdateSecondNumber(const nsAString& aNumber)
  {
    mSecondNumber = aNumber;
  }

  void
  UpdateSwitchable(bool aSwitchable) {
    mSwitchable = aSwitchable;
  }

  void
  UpdateMergeable(bool aMergeable) {
    mMergeable = aMergeable;
  }

  void
  NotifyError(const nsAString& aError);

  void
  ChangeGroup(TelephonyCallGroup* aGroup);

private:
  TelephonyCall(nsPIDOMWindow* aOwner);

  ~TelephonyCall();

  void
  ChangeStateInternal(uint16_t aCallState, bool aFireEvents);

  nsresult
  DispatchCallEvent(const nsAString& aType,
                    TelephonyCall* aCall);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_telephony_telephonycall_h__
