/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/PGamepadEventChannelParent.h"

#ifndef mozilla_dom_GamepadEventChannelParent_h_
#define mozilla_dom_GamepadEventChannelParent_h_

namespace mozilla{
namespace dom{

class GamepadEventChannelParent final : public PGamepadEventChannelParent
{
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GamepadEventChannelParent)
  GamepadEventChannelParent();
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual bool RecvGamepadListenerAdded() override;
  virtual bool RecvGamepadListenerRemoved() override;
  void DispatchUpdateEvent(const GamepadChangeEvent& aEvent);
  bool HasGamepadListener() const { return mHasGamepadListener; }
 private:
  ~GamepadEventChannelParent() {}
  bool mHasGamepadListener;
  nsCOMPtr<nsIThread> mBackgroundThread;
};

}// namespace dom
}// namespace mozilla

#endif
