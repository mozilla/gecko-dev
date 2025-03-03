/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioWorklet_h
#define mozilla_dom_AudioWorklet_h

#include "mozilla/dom/Worklet.h"

namespace mozilla::dom {

class MessagePort;

class AudioWorklet final : public Worklet {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioWorklet, Worklet)

  AudioWorklet(nsPIDOMWindowInner* aWindow, RefPtr<WorkletImpl> aImpl,
               nsISupports* aOwnedObject, MessagePort* aPort);

  MessagePort* Port() const { return mPort; };

 private:
  ~AudioWorklet() = default;

  RefPtr<MessagePort> mPort;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AudioWorklet_h
