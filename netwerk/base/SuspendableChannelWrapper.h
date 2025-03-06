/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SuspendableChannelWrapper_h__
#define SuspendableChannelWrapper_h__

#include "nsISuspendableChannelWrapper.h"

class nsIStreamListener;

namespace mozilla {
namespace net {

class BaseSuspendableChannelWrapper : public nsISuspendableChannelWrapper {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUSPENDABLECHANNELWRAPPER
  NS_FORWARD_SAFE_NSIREQUEST(mInnerChannel)
  NS_FORWARD_SAFE_NSICHANNEL(mInnerChannel)

  explicit BaseSuspendableChannelWrapper(nsIChannel* aInnerChannel)
      : mInnerChannel(aInnerChannel) {}

 protected:
  virtual ~BaseSuspendableChannelWrapper() = default;
  nsCOMPtr<nsIChannel> mInnerChannel;
};

class SuspendableChannelWrapper final : public BaseSuspendableChannelWrapper {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD Suspend() override;
  NS_IMETHOD Resume() override;
  NS_IMETHOD IsPending(bool*) override;

  NS_IMETHOD AsyncOpen(nsIStreamListener* aListener) override;
  NS_IMETHOD Open(nsIInputStream** _retval) override;

  explicit SuspendableChannelWrapper(nsIChannel* aInnerChannel)
      : BaseSuspendableChannelWrapper(aInnerChannel) {}

 private:
  ~SuspendableChannelWrapper() override = default;

  nsCOMPtr<nsIStreamListener> mListener;
  uint32_t mSuspendCount = 0;
  bool mOuterOpened = false;
  bool mInnerOpened = false;
};

}  // namespace net
}  // namespace mozilla

#endif  // SuspendableChannelWrapper_h__
