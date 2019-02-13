/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_provider_MulticastDNSDeviceProvider_h
#define mozilla_dom_presentation_provider_MulticastDNSDeviceProvider_h

#include "nsCOMPtr.h"
#include "nsICancelable.h"
#include "nsIDNSServiceDiscovery.h"
#include "nsIPresentationDeviceProvider.h"
#include "nsITCPPresentationServer.h"
#include "nsRefPtr.h"
#include "nsString.h"
#include "nsWeakPtr.h"

namespace mozilla {
namespace dom {
namespace presentation {

class DNSServiceWrappedListener;
class MulticastDNSService;

class MulticastDNSDeviceProvider final
  : public nsIPresentationDeviceProvider
  , public nsIDNSServiceDiscoveryListener
  , public nsIDNSRegistrationListener
  , public nsIDNSServiceResolveListener
  , public nsITCPPresentationServerListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONDEVICEPROVIDER
  NS_DECL_NSIDNSSERVICEDISCOVERYLISTENER
  NS_DECL_NSIDNSREGISTRATIONLISTENER
  NS_DECL_NSIDNSSERVICERESOLVELISTENER
  NS_DECL_NSITCPPRESENTATIONSERVERLISTENER

  explicit MulticastDNSDeviceProvider() = default;
  nsresult Init();
  nsresult Uninit();

private:
  virtual ~MulticastDNSDeviceProvider();
  nsresult RegisterService(uint32_t aPort);

  bool mInitialized = false;
  nsWeakPtr mDeviceListener;
  nsCOMPtr<nsITCPPresentationServer> mPresentationServer;
  nsCOMPtr<nsIDNSServiceDiscovery> mMulticastDNS;
  nsRefPtr<DNSServiceWrappedListener> mWrappedListener;

  nsCOMPtr<nsICancelable> mDiscoveryRequest;
  nsCOMPtr<nsICancelable> mRegisterRequest;

  nsCString mRegisteredName;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_provider_MulticastDNSDeviceProvider_h
