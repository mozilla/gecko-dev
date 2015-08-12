/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_ImsRegCallback_h
#define mozilla_dom_mobileconnection_ImsRegCallback_h

#include "nsCOMPtr.h"
#include "nsIImsRegService.h"

namespace mozilla {
namespace dom {

class Promise;

namespace mobileconnection {

class ImsRegCallback MOZ_FINAL : public nsIImsRegCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMSREGCALLBACK

  ImsRegCallback(Promise* aPromise);

private:
  ~ImsRegCallback();

  nsRefPtr<Promise> mPromise;
};

} // namespace mobileconnection
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobileconnection_ImsRegCallback_h