/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImsRegHandler_h
#define mozilla_dom_ImsRegHandler_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ImsRegHandlerBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIImsRegService.h"

class nsIImsRegHandler;

namespace mozilla {
namespace dom {

class Promise;

class ImsRegHandler MOZ_FINAL : public DOMEventTargetHelper
                              , private nsIImsRegListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIIMSREGLISTENER
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ImsRegHandler, DOMEventTargetHelper)
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  ImsRegHandler(nsPIDOMWindow *aWindow, nsIImsRegHandler *aHandler);

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL APIs
  already_AddRefed<ImsDeviceConfiguration>
  DeviceConfig() const;

  already_AddRefed<Promise>
  SetEnabled(bool aEnabled, ErrorResult& aRv);

  bool
  GetEnabled(ErrorResult& aRv) const;

  already_AddRefed<Promise>
  SetPreferredProfile(ImsProfile aProfile, ErrorResult& aRv);

  ImsProfile
  GetPreferredProfile(ErrorResult& aRv) const;

  Nullable<ImsCapability>
  GetCapability() const;

  void
  GetUnregisteredReason(nsString& aReason) const;

  IMPL_EVENT_HANDLER(capabilitychange)

private:
  ~ImsRegHandler();

  void
  Shutdown();

  void
  UpdateCapability(int16_t aCapability, const nsAString& aReason);

  nsCOMPtr<nsIImsRegHandler> mHandler;

  nsRefPtr<ImsDeviceConfiguration> mDeviceConfig;
  Nullable<ImsCapability> mCapability;
  nsString mUnregisteredReason;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ImsRegHandler_h
