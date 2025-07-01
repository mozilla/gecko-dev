/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDataChannelDeclarations_h
#define nsDOMDataChannelDeclarations_h

// This defines only what's necessary to create nsDOMDataChannels, since this
// gets used with MOZ_INTERNAL_API not set for media/webrtc/signaling/testing

#include "nsCOMPtr.h"
#include "nsStringFwd.h"
#include "mozilla/dom/Nullable.h"

namespace mozilla {
class DataChannel;
}  // namespace mozilla

class nsDOMDataChannel;
class nsPIDOMWindowInner;

nsresult NS_NewDOMDataChannel(
    already_AddRefed<mozilla::DataChannel>&& aDataChannel,
    const nsAString& aLabel, bool aOrdered,
    mozilla::dom::Nullable<uint16_t> aMaxLifeTime,
    mozilla::dom::Nullable<uint16_t> aMaxRetransmits,
    const nsAString& aProtocol, bool aNegotiated, nsPIDOMWindowInner* aWindow,
    nsDOMDataChannel** aDomDataChannel);

#endif  // nsDOMDataChannelDeclarations_h
