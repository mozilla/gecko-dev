/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_ImsRegIPCService_h
#define mozilla_dom_mobileconnection_ImsRegIPCService_h

#include "nsCOMPtr.h"
#include "mozilla/dom/mobileconnection/ImsRegistrationChild.h"
#include "nsIImsRegService.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

class MobileConnectionChild;

class ImsRegIPCService MOZ_FINAL : public nsIImsRegService
{
  friend class MobileConnectionChild;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMSREGSERVICE

  static already_AddRefed<ImsRegIPCService>
  GetSingleton();

private:
  ImsRegIPCService();
  ~ImsRegIPCService();

  nsTArray<nsRefPtr<ImsRegistrationChild>> mHandlers;
};

} // namespace mobileconnection
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobileconnection_ImsRegIPCService_h