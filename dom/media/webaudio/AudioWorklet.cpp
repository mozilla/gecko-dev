/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioWorklet.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/WorkletImpl.h"

namespace mozilla::dom {

NS_IMPL_ADDREF_INHERITED(AudioWorklet, Worklet)
NS_IMPL_RELEASE_INHERITED(AudioWorklet, Worklet)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioWorklet)
NS_INTERFACE_MAP_END_INHERITING(Worklet)

NS_IMPL_CYCLE_COLLECTION_CLASS(AudioWorklet)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AudioWorklet, Worklet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPort)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AudioWorklet, Worklet)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPort)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

AudioWorklet::AudioWorklet(nsPIDOMWindowInner* aWindow,
                           RefPtr<WorkletImpl> aImpl, nsISupports* aOwnedObject,
                           MessagePort* aPort)
    : Worklet(aWindow, aImpl, aOwnedObject), mPort(aPort) {
  MOZ_ASSERT(aPort);
}

}  // namespace mozilla::dom
