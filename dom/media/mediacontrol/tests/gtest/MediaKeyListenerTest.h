/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIAKEYLISTENERTEST_H_
#define DOM_MEDIA_MEDIAKEYLISTENERTEST_H_

#include "MediaControlKeySource.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace dom {

class MediaKeyListenerTest : public MediaControlKeyListener {
 public:
  NS_INLINE_DECL_REFCOUNTING(MediaKeyListenerTest, override)

  void Clear() { mReceivedAction = mozilla::Nothing(); }

  void OnActionPerformed(const MediaControlAction& aAction) override {
    mReceivedAction = Some(aAction);
  }
  bool IsKeyEqualTo(MediaControlKey aResult) const {
    if (mReceivedAction.isSome() && mReceivedAction->mKey.isSome()) {
      return mReceivedAction->mKey.value() == aResult;
    }
    return false;
  }

  mozilla::Maybe<SeekDetails> GetSeekDetails() const {
    if (mReceivedAction.isSome()) {
      return mReceivedAction->mDetails;
    }
    return Nothing();
  }

  bool IsReceivedResult() const { return mReceivedAction.isSome(); }

 private:
  ~MediaKeyListenerTest() = default;
  mozilla::Maybe<MediaControlAction> mReceivedAction;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_MEDIA_MEDIAKEYLISTENERTEST_H_
