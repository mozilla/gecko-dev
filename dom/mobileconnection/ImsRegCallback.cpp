/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImsRegCallback.h"

#include "mozilla/dom/ImsRegHandlerBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsIImsRegService.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

NS_IMPL_ISUPPORTS(ImsRegCallback, nsIImsRegCallback)

ImsRegCallback::ImsRegCallback(Promise* aPromise)
  : mPromise(aPromise)
{
}

ImsRegCallback::~ImsRegCallback()
{
}

// nsIImsRegCallback

NS_IMETHODIMP ImsRegCallback::NotifySuccess()
{
  mPromise->MaybeResolve(JS::UndefinedHandleValue);
  return NS_OK;
}

NS_IMETHODIMP ImsRegCallback::NotifyError(const nsAString & aRrror)
{
  mPromise->MaybeRejectBrokenly(aRrror);
  return NS_OK;
}

} // namespace mobileconnection
} // namespace dom
} // namespace mozilla
