/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_network_Connection_h
#define mozilla_dom_network_Connection_h

#include "Types.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Observer.h"
#include "mozilla/dom/NetworkInformationBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsINetworkProperties.h"

namespace mozilla {

namespace hal {
class NetworkInformation;
} // namespace hal

namespace dom {
namespace network {

class Connection final : public DOMEventTargetHelper
                       , public NetworkObserver
                       , public nsINetworkProperties
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSINETWORKPROPERTIES

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  explicit Connection(nsPIDOMWindow *aWindow);

  void Shutdown();

  // For IObserver
  void Notify(const hal::NetworkInformation& aNetworkInfo) override;

  // WebIDL

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  ConnectionType Type() const { return mType; }

  IMPL_EVENT_HANDLER(typechange)

private:
  ~Connection() {}

  /**
   * Update the connection information stored in the object using a
   * NetworkInformation object.
   */
  void UpdateFromNetworkInfo(const hal::NetworkInformation& aNetworkInfo);

  /**
   * The type of current connection.
   */
  ConnectionType mType;

  /**
   * If the connection is WIFI
   */
  bool mIsWifi;

  /**
   * DHCP Gateway information for IPV4, in network byte order. 0 if unassigned.
   */
  uint32_t mDHCPGateway;
};

} // namespace network
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_network_Connection_h
