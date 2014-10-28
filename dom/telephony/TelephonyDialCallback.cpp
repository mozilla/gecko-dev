/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyDialCallback.h"

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;

NS_IMPL_ISUPPORTS_INHERITED(TelephonyDialCallback, TelephonyCallback,
                            nsITelephonyDialCallback)

TelephonyDialCallback::TelephonyDialCallback(nsPIDOMWindow* aWindow,
                                             Telephony* aTelephony,
                                             Promise* aPromise,
                                             uint32_t aServiceId)
  : TelephonyCallback(aPromise), mWindow(aWindow), mTelephony(aTelephony),
    mServiceId(aServiceId)
{
  MOZ_ASSERT(mTelephony);
}

// nsITelephonyDialCallback

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialCallSuccess(uint32_t aCallIndex,
                                             const nsAString& aNumber)
{
  nsRefPtr<TelephonyCallId> id = mTelephony->CreateCallId(aNumber);
  nsRefPtr<TelephonyCall> call =
    mTelephony->CreateCall(id, mServiceId, aCallIndex,
                           nsITelephonyService::CALL_STATE_DIALING);

  mPromise->MaybeResolve(call);
  return NS_OK;
}
