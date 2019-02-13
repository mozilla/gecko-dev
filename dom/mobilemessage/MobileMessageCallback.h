/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MobileMessageCallback_h
#define mozilla_dom_mobilemessage_MobileMessageCallback_h

#include "nsIMobileMessageCallback.h"
#include "nsCOMPtr.h"
#include "DOMRequest.h"

class Promise;

namespace mozilla {
namespace dom {
namespace mobilemessage {

class MobileMessageCallback final : public nsIMobileMessageCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILEMESSAGECALLBACK

  explicit MobileMessageCallback(DOMRequest* aDOMRequest);
  explicit MobileMessageCallback(Promise* aPromise);

private:
  ~MobileMessageCallback();

  nsRefPtr<DOMRequest> mDOMRequest;
  nsRefPtr<Promise> mPromise;

  nsresult NotifySuccess(JS::Handle<JS::Value> aResult, bool aAsync = false);
  nsresult NotifySuccess(nsISupports *aMessage, bool aAsync = false);
  nsresult NotifyError(int32_t aError, DOMError *aDetailedError = nullptr, bool aAsync = false);
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_MobileMessageCallback_h
