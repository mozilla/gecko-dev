/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImsDeviceConfiguration.h"
#include "mozilla/dom/ImsRegHandlerBinding.h"

namespace mozilla {
namespace dom {


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ImsDeviceConfiguration, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ImsDeviceConfiguration)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImsDeviceConfiguration)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImsDeviceConfiguration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ImsDeviceConfiguration::ImsDeviceConfiguration(nsPIDOMWindow* aWindow,
                                               const nsTArray<ImsBearer>& aBearers)
  : mWindow(aWindow)
  , mBearers(aBearers)
{
}

ImsDeviceConfiguration::~ImsDeviceConfiguration()
{
}

JSObject*
ImsDeviceConfiguration::WrapObject(JSContext* aCx)
{
  return ImsDeviceConfigurationBinding::Wrap(aCx, this);
}

void
ImsDeviceConfiguration::GetSupportedBearers(nsTArray<ImsBearer>& aBearers) const
{
  aBearers = mBearers;
}

} // namespace dom
} // namespace mozilla