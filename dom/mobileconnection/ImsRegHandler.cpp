/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImsRegHandler.h"

#include "ImsRegCallback.h"
#include "nsIImsRegService.h"

using mozilla::ErrorResult;
using mozilla::dom::mobileconnection::ImsRegCallback;

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(ImsRegHandler, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ImsRegHandler, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(ImsRegHandler)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ImsRegHandler, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDeviceConfig)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ImsRegHandler,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDeviceConfig)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ImsRegHandler)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

ImsRegHandler::ImsRegHandler(nsPIDOMWindow *aWindow, nsIImsRegHandler *aHandler)
  : DOMEventTargetHelper(aWindow)
  , mHandler(aHandler)
{
  MOZ_ASSERT(mHandler);

  mUnregisteredReason.SetIsVoid(true);

  int16_t capability = nsIImsRegHandler::IMS_CAPABILITY_UNKNOWN;
  mHandler->GetCapability(&capability);
  nsAutoString reason;
  mHandler->GetUnregisteredReason(reason);

  UpdateCapability(capability, reason);

  // GetSupportedBearers
  uint16_t* bearers = nullptr;
  uint32_t count = 0;
  nsTArray<ImsBearer> supportedBearers;
  nsresult rv = mHandler->GetSupportedBearers(&count, &bearers);
  NS_ENSURE_SUCCESS_VOID(rv);
  for (uint32_t i = 0; i < count; ++i) {
    uint16_t bearer = bearers[i];
    MOZ_ASSERT(bearer < static_cast<uint16_t>(ImsBearer::EndGuard_));
    supportedBearers.AppendElement(static_cast<ImsBearer>(bearer));
  }
  nsMemory::Free(bearers);
  mDeviceConfig = new ImsDeviceConfiguration(GetOwner(), supportedBearers);

  mHandler->RegisterListener(this);
}

ImsRegHandler::~ImsRegHandler()
{
  Shutdown();
}

void
ImsRegHandler::Shutdown()
{
  if (mHandler) {
    mHandler->UnregisterListener(this);
    mHandler = nullptr;
  }
}

void
ImsRegHandler::UpdateCapability(int16_t aCapability, const nsAString& aReason)
{
  // IMS is not registered
  if (aCapability == nsIImsRegHandler::IMS_CAPABILITY_UNKNOWN) {
    mCapability.SetNull();
    mUnregisteredReason = aReason;
    return;
  }

  // IMS is registered
  MOZ_ASSERT(aCapability >= 0 &&
    aCapability < static_cast<int16_t>(ImsCapability::EndGuard_));
  mCapability.SetValue(static_cast<ImsCapability>(aCapability));
  mUnregisteredReason.SetIsVoid(true);
}

JSObject*
ImsRegHandler::WrapObject(JSContext* aCx)
{
  return ImsRegHandlerBinding::Wrap(aCx, this);
}

already_AddRefed<ImsDeviceConfiguration>
ImsRegHandler::DeviceConfig() const
{
  nsRefPtr<ImsDeviceConfiguration> result = mDeviceConfig;
  return result.forget();
}

already_AddRefed<Promise>
ImsRegHandler::SetEnabled(bool aEnabled, ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    aRv.Throw(aRv.ErrorCode());
    return nullptr;
  }

  nsRefPtr<ImsRegCallback> requestCallback = new ImsRegCallback(promise);

  nsresult rv = mHandler->SetEnabled(aEnabled, requestCallback);

  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    // fall-through to return promise.
  }

  return promise.forget();
}

bool
ImsRegHandler::GetEnabled(ErrorResult& aRv) const
{
  bool result = false;

  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return result;
  }

  nsresult rv = mHandler->GetEnabled(&result);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return result;
  }

  return result;
}

already_AddRefed<Promise>
ImsRegHandler::SetPreferredProfile(ImsProfile aProfile, ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    aRv.Throw(aRv.ErrorCode());
    return nullptr;
  }

  nsRefPtr<ImsRegCallback> requestCallback = new ImsRegCallback(promise);

  nsresult rv =
    mHandler->SetPreferredProfile(static_cast<uint16_t>(aProfile),
                                  requestCallback);

  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    // fall-through to return promise.
  }

  return promise.forget();
}

ImsProfile
ImsRegHandler::GetPreferredProfile(ErrorResult& aRv) const
{
  ImsProfile result = ImsProfile::Cellular_preferred;

  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return result;
  }

  uint16_t profile = nsIImsRegHandler::IMS_PROFILE_CELLULAR_PREFERRED;
  nsresult rv = mHandler->GetPreferredProfile(&profile);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return result;
  }

  MOZ_ASSERT(profile < static_cast<uint16_t>(ImsProfile::EndGuard_));
  result = static_cast<ImsProfile>(profile);

  return result;
}

Nullable<ImsCapability>
ImsRegHandler::GetCapability() const
{
  return mCapability;
}

void
ImsRegHandler::GetUnregisteredReason(nsString& aReason) const
{
  aReason = mUnregisteredReason;
  return;
}

// nsIImsRegListener

NS_IMETHODIMP
ImsRegHandler::NotifyEnabledStateChanged(bool aEnabled)
{
  // Add |enabledstatechanged| when needed:
  // The enabled state is expected to be changed when set request is resolved,
  // so the caller knows when to get the updated enabled state.
  // If the change observed by multiple apps is expected,
  // then |enabledstatechanged| is required. Return NS_OK intentionally.
  return NS_OK;
}

NS_IMETHODIMP
ImsRegHandler::NotifyPreferredProfileChanged(uint16_t aProfile)
{
  // Add |profilechanged| when needed:
  // The preferred profile is expected to be changed when set request is resolved,
  // so the caller knows when to get the updated enabled state.
  // If the change observed by multiple apps is expected,
  // then |profilechanged| is required. Return NS_OK intentionally.
  return NS_OK;
}

NS_IMETHODIMP
ImsRegHandler::NotifyCapabilityChanged(int16_t aCapability,
                                       const nsAString& aUnregisteredReason)
{
  UpdateCapability(aCapability, aUnregisteredReason);

  return DispatchTrustedEvent(NS_LITERAL_STRING("capabilitychange"));
}

} // namespace dom
} // namespace mozilla
