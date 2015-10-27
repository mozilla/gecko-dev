/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/mobileconnection/ImsRegistrationChild.h"

#include "mozilla/dom/ContentChild.h"
#include "ImsRegIPCService.h"
#include "ImsRegCallback.h"

using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

/**
 * ImsRegistrationChild
 */

NS_IMPL_ISUPPORTS(ImsRegistrationChild, nsIImsRegHandler)

ImsRegistrationChild::ImsRegistrationChild(uint32_t aServiceId)
  : mLive(true)
{
  MOZ_COUNT_CTOR(ImsRegistrationChild);
}

void
ImsRegistrationChild::Init()
{
  SendInit(&mEnabled, &mPreferredProfile, &mCapability, &mUnregisteredReason,
           &mSupportedBearers);
}

void
ImsRegistrationChild::Shutdown()
{
  if (mLive) {
    mLive = false;
    Send__delete__(this);
  }

  mListeners.Clear();
}

bool
ImsRegistrationChild::SendRequest(const ImsRegistrationRequest& aRequest,
                                  nsIImsRegCallback* aCallback)
{
  NS_ENSURE_TRUE(mLive, false);

  // Deallocated in ImsRegistrationChild::DeallocPImsRegistrationRequestChild().
  ImsRegistrationRequestChild* actor =
    new ImsRegistrationRequestChild(aCallback);
  SendPImsRegistrationRequestConstructor(actor, aRequest);

  return true;
}

void
ImsRegistrationChild::ActorDestroy(ActorDestroyReason why)
{
  mLive = false;
}

PImsRegistrationRequestChild*
ImsRegistrationChild::AllocPImsRegistrationRequestChild(const ImsRegistrationRequest& request)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool
ImsRegistrationChild::DeallocPImsRegistrationRequestChild(PImsRegistrationRequestChild* aActor)
{
  delete aActor;
  return true;
}

bool
ImsRegistrationChild::RecvNotifyEnabledStateChanged(const bool& aEnabled)
{
  mEnabled = aEnabled;

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyEnabledStateChanged(aEnabled);
  }

  return true;
}

bool
ImsRegistrationChild::RecvNotifyPreferredProfileChanged(const uint16_t& aProfile)
{
  mPreferredProfile = aProfile;

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyPreferredProfileChanged(aProfile);
  }

  return true;
}

bool
ImsRegistrationChild::RecvNotifyImsCapabilityChanged(const int16_t& aCapability,
                                                     const nsString& aUnregisteredReason)
{
  mCapability = aCapability;
  mUnregisteredReason = aUnregisteredReason;

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyCapabilityChanged(aCapability, aUnregisteredReason);
  }

  return true;
}

// nsIImsRegHandler

NS_IMETHODIMP
ImsRegistrationChild::RegisterListener(nsIImsRegListener* aListener)
{
  NS_ENSURE_TRUE(!mListeners.Contains(aListener), NS_ERROR_UNEXPECTED);

  mListeners.AppendObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP
ImsRegistrationChild::UnregisterListener(nsIImsRegListener* aListener)
{
  NS_ENSURE_TRUE(mListeners.Contains(aListener), NS_ERROR_UNEXPECTED);

  mListeners.RemoveObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP
ImsRegistrationChild::GetSupportedBearers(uint32_t *aCount, uint16_t **aBearers)
{
  NS_ENSURE_ARG(aCount);
  NS_ENSURE_ARG(aBearers);

  *aCount = mSupportedBearers.Length();
  *aBearers =
    static_cast<uint16_t*>(nsMemory::Alloc((*aCount) * sizeof(uint16_t)));
  NS_ENSURE_TRUE(*aBearers, NS_ERROR_OUT_OF_MEMORY);

  for (uint32_t i = 0; i < *aCount; i++) {
    (*aBearers)[i] = mSupportedBearers[i];
  }

  return NS_OK;
}

NS_IMETHODIMP ImsRegistrationChild::SetEnabled(bool aEnabled, nsIImsRegCallback *aCallback)
{
  return SendRequest(SetImsEnabledRequest(aEnabled), aCallback)
    ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP ImsRegistrationChild::GetEnabled(bool* aEnabled)
{
  MOZ_ASSERT(aEnabled);

  *aEnabled = mEnabled;
  return NS_OK;
}

NS_IMETHODIMP ImsRegistrationChild::SetPreferredProfile(uint16_t aProfile, nsIImsRegCallback *aCallback)
{
  return SendRequest(SetImsPreferredProfileRequest(aProfile), aCallback)
    ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP ImsRegistrationChild::GetPreferredProfile(uint16_t* aProfile)
{
  MOZ_ASSERT(aProfile);

  *aProfile = mPreferredProfile;
  return NS_OK;
}

NS_IMETHODIMP ImsRegistrationChild::GetCapability(int16_t *aCapability)
{
  MOZ_ASSERT(aCapability);

  *aCapability = mCapability;
  return NS_OK;
}

NS_IMETHODIMP ImsRegistrationChild::GetUnregisteredReason(nsAString & aUnregisteredReason)
{
  aUnregisteredReason = mUnregisteredReason;
  return NS_OK;
}

/******************************************************************************
 * ImsRegistrationRequestChild
 ******************************************************************************/

void
ImsRegistrationRequestChild::ActorDestroy(ActorDestroyReason why)
{
  mRequestCallback = nullptr;
}

bool
ImsRegistrationRequestChild::DoReply(const ImsRegistrationReplySuccess& aReply)
{
  return NS_SUCCEEDED(mRequestCallback->NotifySuccess());
}

bool
ImsRegistrationRequestChild::DoReply(const ImsRegistrationReplyError& aReply)
{
  return NS_SUCCEEDED(mRequestCallback->NotifyError(aReply.error()));
}

bool
ImsRegistrationRequestChild::Recv__delete__(const ImsRegistrationReply& aReply)
{
  MOZ_ASSERT(mRequestCallback);

  switch (aReply.type()) {
    case ImsRegistrationReply::TImsRegistrationReplySuccess:
      return DoReply(aReply.get_ImsRegistrationReplySuccess());
    case ImsRegistrationReply::TImsRegistrationReplyError:
      return DoReply(aReply.get_ImsRegistrationReplyError());
    default:
      MOZ_CRASH("Received invalid response type!");
  }

  return false;
}
