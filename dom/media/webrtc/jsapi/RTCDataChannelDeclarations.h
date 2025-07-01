/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RTCDataChannelDeclarations_h
#define RTCDataChannelDeclarations_h

// This defines only what's necessary to create RTCDataChannels, since this
// gets used with MOZ_INTERNAL_API not set for media/webrtc/signaling/testing

#include "nsCOMPtr.h"
#include "nsStringFwd.h"
#include "mozilla/dom/Nullable.h"

class nsPIDOMWindowInner;

namespace mozilla {
class DataChannel;

namespace dom {
class RTCDataChannel;

nsresult NS_NewDOMDataChannel(already_AddRefed<DataChannel>&& aDataChannel,
                              const nsACString& aLabel, bool aOrdered,
                              Nullable<uint16_t> aMaxLifeTime,
                              Nullable<uint16_t> aMaxRetransmits,
                              const nsACString& aProtocol, bool aNegotiated,
                              nsPIDOMWindowInner* aWindow,
                              RTCDataChannel** aDomDataChannel);

}  // namespace dom
}  // namespace mozilla
#endif  // RTCDataChannelDeclarations_h
