/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImsRegIPCService.h"

#include "mozilla/dom/ContentChild.h"
#include "nsIMobileConnectionService.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

namespace {
  static bool gImsRegServiceFinderChecked = false;
  static bool gImsRegServiceInstalled = false;
  static nsTArray<uint32_t> gImsRegEnabledServiceIds;
  // This instance is acutally owned by nsLayoutModule after
  // do_GetService(IMS_REG_SERVICE_CONTRACTID) is invoked.
  static ImsRegIPCService* gImsRegServiceSingleton = nullptr;

  void QueryImsRegServiceFinder() {
    if (!gImsRegServiceFinderChecked) {
      PImsRegServiceFinderChild* finder =
        ContentChild::GetSingleton()->SendPImsRegServiceFinderConstructor();
      MOZ_ASSERT(finder);

      NS_ENSURE_TRUE_VOID(
        finder->SendCheckDeviceCapability(&gImsRegServiceInstalled,
                                          &gImsRegEnabledServiceIds));
      NS_ENSURE_TRUE_VOID(finder->Send__delete__(finder));

      gImsRegServiceFinderChecked = true;
    }
  }
} // anonymous namespace

NS_IMPL_ISUPPORTS(ImsRegIPCService, nsIImsRegService)

ImsRegIPCService::ImsRegIPCService()
{
  nsCOMPtr<nsIMobileConnectionService> service =
    do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(service);

  uint32_t numItems = 0;
  NS_ENSURE_SUCCESS_VOID(service->GetNumItems(&numItems));
  mHandlers.SetLength(numItems);
}

ImsRegIPCService::~ImsRegIPCService()
{
  gImsRegServiceSingleton = nullptr;

  uint32_t count = mHandlers.Length();
  for (uint32_t i = 0; i < count; i++) {
    if (mHandlers[i]) {
      mHandlers[i]->Shutdown();
    }
  }
}

/* static */ already_AddRefed<ImsRegIPCService>
ImsRegIPCService::GetSingleton()
{
  QueryImsRegServiceFinder();

  if (gImsRegServiceInstalled && !gImsRegServiceSingleton) {
    gImsRegServiceSingleton = new ImsRegIPCService();
  }

  nsRefPtr<ImsRegIPCService> service = gImsRegServiceSingleton;
  return service.forget();
}

NS_IMETHODIMP
ImsRegIPCService::GetHandlerByServiceId(uint32_t aServiceId,
                                        nsIImsRegHandler** aHandler)
{
  MOZ_ASSERT(aHandler);
  NS_ENSURE_TRUE(aServiceId < mHandlers.Length(), NS_ERROR_INVALID_ARG);

  *aHandler = nullptr;

  if (gImsRegEnabledServiceIds.IndexOf(aServiceId) !=
      gImsRegEnabledServiceIds.NoIndex) {
    if (!mHandlers[aServiceId]) {
      nsRefPtr<ImsRegistrationChild> child = new ImsRegistrationChild(aServiceId);
      // |SendPImsRegistrationConstructor| adds another reference to the child
      // actor and removes in |DeallocPImsRegistrationChild|.
      ContentChild::GetSingleton()->SendPImsRegistrationConstructor(child,
                                                                    aServiceId);
      child->Init();
      mHandlers[aServiceId] = child;
    }

    nsRefPtr<nsIImsRegHandler> handler(mHandlers[aServiceId]);
    handler.forget(aHandler);
  }

  return NS_OK;
}