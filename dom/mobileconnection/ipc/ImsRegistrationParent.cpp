/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/mobileconnection/ImsRegistrationParent.h"

#include "mozilla/AppProcessChecker.h"
#include "nsIMobileConnectionService.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

/**
 * ImsRegServiceFinderParent
 */
bool
ImsRegServiceFinderParent::RecvCheckDeviceCapability(bool* aIsServiceInstalled,
                                                     nsTArray<uint32_t>* aEnabledServiceIds)
{
  MOZ_ASSERT(aIsServiceInstalled);
  MOZ_ASSERT(aEnabledServiceIds);

  nsCOMPtr<nsIImsRegService> imsService = do_GetService(IMS_REG_SERVICE_CONTRACTID);
  if (imsService) {
    *aIsServiceInstalled = true;

    nsCOMPtr<nsIMobileConnectionService> service =
      do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
    NS_ASSERTION(service, "This shouldn't fail!");

    uint32_t numItems = 0;
    if (NS_SUCCEEDED(service->GetNumItems(&numItems))) {
      for (uint32_t i = 0; i < numItems; i++) {
        nsCOMPtr<nsIImsRegHandler> handler;
        imsService->GetHandlerByServiceId(i, getter_AddRefs(handler));
        if (handler) {
          aEnabledServiceIds->AppendElement(i);
        }
      }
    }
  }

  return true;
}

void
ImsRegServiceFinderParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

/**
 * ImsRegistrationParent
 */

ImsRegistrationParent::ImsRegistrationParent(uint32_t aServiceId)
  : mLive(true)
{
  MOZ_COUNT_CTOR(ImsRegistrationParent);

  nsCOMPtr<nsIImsRegService> service =
    do_GetService(IMS_REG_SERVICE_CONTRACTID);
  NS_ASSERTION(service, "This shouldn't fail!");

  nsresult rv = service->GetHandlerByServiceId(aServiceId,
                                               getter_AddRefs(mHandler));
  if (NS_SUCCEEDED(rv) && mHandler) {
    mHandler->RegisterListener(this);
  }
}

void
ImsRegistrationParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mLive = false;
  if (mHandler) {
    mHandler->UnregisterListener(this);
    mHandler = nullptr;
  }
}

bool
ImsRegistrationParent::RecvPImsRegistrationRequestConstructor(PImsRegistrationRequestParent* aActor,
                                                               const ImsRegistrationRequest& aRequest)
{
  ImsRegistrationRequestParent* actor = static_cast<ImsRegistrationRequestParent*>(aActor);

  switch (aRequest.type()) {
    case ImsRegistrationRequest::TSetImsEnabledRequest:
      return actor->DoRequest(aRequest.get_SetImsEnabledRequest());
    case ImsRegistrationRequest::TSetImsPreferredProfileRequest:
      return actor->DoRequest(aRequest.get_SetImsPreferredProfileRequest());
    default:
      MOZ_CRASH("Received invalid request type!");
  }

  return false;
}

PImsRegistrationRequestParent*
ImsRegistrationParent::AllocPImsRegistrationRequestParent(const ImsRegistrationRequest& request)
{
  if (!AssertAppProcessPermission(Manager(), "mobileconnection")) {
    return nullptr;
  }

  ImsRegistrationRequestParent* actor =
    new ImsRegistrationRequestParent(mHandler);
  // Add an extra ref for IPDL. Will be released in
  // ImsRegistrationParent::DeallocPImsRegistrationRequestParent().
  actor->AddRef();
  return actor;
}

bool
ImsRegistrationParent::DeallocPImsRegistrationRequestParent(PImsRegistrationRequestParent* aActor)
{
  // ImsRegistrationRequestParent is refcounted, must not be freed manually.
  static_cast<ImsRegistrationRequestParent*>(aActor)->Release();
  return true;
}

bool
ImsRegistrationParent::RecvInit(bool* aEnabled, uint16_t* aProfile,
                                int16_t* aCapability, nsString* aUnregisteredReason,
                                nsTArray<uint16_t>* aSupportedBearers)
{
  NS_ENSURE_TRUE(mHandler, false);

  NS_ENSURE_SUCCESS(mHandler->GetEnabled(aEnabled), false);
  NS_ENSURE_SUCCESS(mHandler->GetPreferredProfile(aProfile), false);
  NS_ENSURE_SUCCESS(mHandler->GetCapability(aCapability), false);
  NS_ENSURE_SUCCESS(mHandler->GetUnregisteredReason(*aUnregisteredReason), false);

  // GetSupportedBearers
  uint16_t* bearers = nullptr;
  uint32_t count = 0;
  nsresult rv = mHandler->GetSupportedBearers(&count, &bearers);
  NS_ENSURE_SUCCESS(rv, false);
  for (uint32_t i = 0; i < count; ++i) {
    aSupportedBearers->AppendElement(bearers[i]);
  }
  nsMemory::Free(bearers);

  return true;
}

// nsIImsRegListener

NS_IMPL_ISUPPORTS(ImsRegistrationParent, nsIImsRegListener)

NS_IMETHODIMP
ImsRegistrationParent::NotifyEnabledStateChanged(bool aEnabled)
{
  return SendNotifyEnabledStateChanged(aEnabled) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ImsRegistrationParent::NotifyPreferredProfileChanged(uint16_t aProfile)
{
  return SendNotifyPreferredProfileChanged(aProfile) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ImsRegistrationParent::NotifyCapabilityChanged(int16_t aCapability,
                                               const nsAString& aUnregisteredReason)
{
  return SendNotifyImsCapabilityChanged(aCapability,
                                        nsAutoString(aUnregisteredReason))
    ? NS_OK : NS_ERROR_FAILURE;
}

/******************************************************************************
 * PImsRegistrationRequestParent
 ******************************************************************************/

void
ImsRegistrationRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mLive = false;
  mHandler = nullptr;
}

bool
ImsRegistrationRequestParent::DoRequest(const SetImsEnabledRequest& aRequest)
{
  NS_ENSURE_TRUE(mHandler, false);

  return NS_SUCCEEDED(mHandler->SetEnabled(aRequest.enabled(), this));
}

bool
ImsRegistrationRequestParent::DoRequest(const SetImsPreferredProfileRequest& aRequest)
{
  NS_ENSURE_TRUE(mHandler, false);

  return NS_SUCCEEDED(mHandler->SetPreferredProfile(aRequest.profile(), this));
}

nsresult
ImsRegistrationRequestParent::SendReply(const ImsRegistrationReply& aReply)
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return Send__delete__(this, aReply) ? NS_OK : NS_ERROR_FAILURE;
}

// nsIImsRegCallback

NS_IMPL_ISUPPORTS(ImsRegistrationRequestParent, nsIImsRegCallback);

NS_IMETHODIMP
ImsRegistrationRequestParent::NotifySuccess()
{
  return SendReply(ImsRegistrationReplySuccess());
}

NS_IMETHODIMP
ImsRegistrationRequestParent::NotifyError(const nsAString& aError)
{
  return SendReply(ImsRegistrationReplyError(nsAutoString(aError)));
}
