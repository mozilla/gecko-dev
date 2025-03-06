/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SuspendableChannelWrapper.h"
#include "nsIStreamListener.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(BaseSuspendableChannelWrapper, nsISuspendableChannelWrapper)

NS_IMPL_ADDREF_INHERITED(SuspendableChannelWrapper,
                         BaseSuspendableChannelWrapper)
NS_IMPL_RELEASE_INHERITED(SuspendableChannelWrapper,
                          BaseSuspendableChannelWrapper)
NS_INTERFACE_MAP_BEGIN(SuspendableChannelWrapper)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsISuspendableChannelWrapper)
NS_INTERFACE_MAP_END_AGGREGATED(mInnerChannel)

NS_IMETHODIMP
SuspendableChannelWrapper::Suspend() {
  if (mInnerOpened) {
    mInnerChannel->Suspend();
  } else {
    mSuspendCount++;
  }
  return NS_OK;
}

NS_IMETHODIMP
SuspendableChannelWrapper::Resume() {
  if (mInnerOpened) {
    mInnerChannel->Resume();
  } else if (mSuspendCount > 0) {
    mSuspendCount--;
  }

  if (!mSuspendCount && mOuterOpened && !mInnerOpened) {
    mInnerOpened = true;
    MOZ_ASSERT(mListener);
    return mInnerChannel->AsyncOpen(mListener);
  }
  return NS_OK;
}

NS_IMETHODIMP
SuspendableChannelWrapper::IsPending(bool* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  if (mInnerOpened) {
    return mInnerChannel->IsPending(_retval);
  }
  *_retval = mSuspendCount > 0;
  return NS_OK;
}

NS_IMETHODIMP
SuspendableChannelWrapper::AsyncOpen(nsIStreamListener* aListener) {
  if (mOuterOpened) {
    return NS_ERROR_ALREADY_OPENED;
  }

  mListener = aListener;
  mOuterOpened = true;

  if (mInnerOpened || !mSuspendCount) {
    return mInnerChannel->AsyncOpen(mListener);
  }

  return NS_OK;
}

NS_IMETHODIMP
SuspendableChannelWrapper::Open(nsIInputStream** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace net
}  // namespace mozilla
