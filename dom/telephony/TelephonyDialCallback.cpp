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
                                             Promise* aPromise)
  : TelephonyCallback(aPromise), mWindow(aWindow), mTelephony(aTelephony)
{
  MOZ_ASSERT(mTelephony);
}

// nsITelephonyDialCallback

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialCallSuccess(uint32_t aClientId,
                                             uint32_t aCallIndex,
                                             const nsAString& aNumber)
{
  nsRefPtr<TelephonyCallId> id = mTelephony->CreateCallId(aNumber);
  nsRefPtr<TelephonyCall> call =
    mTelephony->CreateCall(id, aClientId, aCallIndex,
                           nsITelephonyService::CALL_STATE_DIALING);

  mPromise->MaybeResolve(call);
  return NS_OK;
}
