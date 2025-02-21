/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/css/StreamLoader.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Encoding.h"
#include "mozilla/glean/NetwerkMetrics.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/dom/CacheExpirationTime.h"
#include "nsContentUtils.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIStreamTransportService.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"

namespace mozilla::css {

StreamLoader::StreamLoader(SheetLoadData& aSheetLoadData)
    : mSheetLoadData(&aSheetLoadData),
      mStatus(NS_OK),
      mMainThreadSheetLoadData(new nsMainThreadPtrHolder<SheetLoadData>(
          "StreamLoader::SheetLoadData", mSheetLoadData, false)) {}

StreamLoader::~StreamLoader() {
#ifdef NIGHTLY_BUILD
  MOZ_RELEASE_ASSERT(mOnStopProcessingDone || mChannelOpenFailed);
#endif
}

NS_IMPL_ISUPPORTS(StreamLoader, nsIStreamListener,
                  nsIThreadRetargetableStreamListener, nsIChannelEventSink,
                  nsIInterfaceRequestor)

/* nsIRequestObserver implementation */
NS_IMETHODIMP
StreamLoader::OnStartRequest(nsIRequest* aRequest) {
  MOZ_ASSERT(aRequest);
  mRequest = aRequest;
  mSheetLoadData->OnStartRequest(aRequest);

  // It's kinda bad to let Web content send a number that results
  // in a potentially large allocation directly, but efficiency of
  // compression bombs is so great that it doesn't make much sense
  // to require a site to send one before going ahead and allocating.
  if (nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest)) {
    int64_t length;
    nsresult rv = channel->GetContentLength(&length);
    if (NS_SUCCEEDED(rv) && length > 0) {
      CheckedInt<nsACString::size_type> checkedLength(length);
      if (!checkedLength.isValid()) {
        return (mStatus = NS_ERROR_OUT_OF_MEMORY);
      }
      if (!mBytes.SetCapacity(checkedLength.value(), fallible)) {
        return (mStatus = NS_ERROR_OUT_OF_MEMORY);
      }
    }
  }
  if (nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(aRequest)) {
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    RefPtr queue =
        TaskQueue::Create(sts.forget(), "css::StreamLoader Delivery Queue");
    rr->RetargetDeliveryTo(queue);
  }

  return NS_OK;
}

NS_IMETHODIMP
StreamLoader::CheckListenerChain() { return NS_OK; }

NS_IMETHODIMP
StreamLoader::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  MOZ_ASSERT_IF(!StaticPrefs::network_send_OnDataFinished_cssLoader(),
                !mOnStopProcessingDone);
  mRequest = nullptr;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

  // StreamLoader::OnStopRequest can get triggered twice for a request.
  // Once from the path
  // nsIThreadRetargetableStreamListener::OnDataFinished->StreamLoader::OnDataFinished
  // (non-main thread)  and
  // once from nsIRequestObserver::OnStopRequest path (main thread). It is
  // guaranteed that we will always get
  // nsIThreadRetargetableStreamListener::OnDataFinished trigger first and this
  // is always followed by nsIRequestObserver::OnStopRequest

  // If we are executing OnStopRequest OMT, we need to block resolution of parse
  // promise and unblock again if we are executing this in main thread.
  // Resolution of parse promise fires onLoadEvent and this should not happen
  // before main thread OnStopRequest is dispatched.
  if (NS_IsMainThread()) {
    channel->SetNotificationCallbacks(nullptr);

    mSheetLoadData->mNetworkMetadata =
        new SubResourceNetworkMetadataHolder(aRequest);

    if (mOnDataFinishedTime) {
      // collect telemetry for the delta between OnDataFinished and
      // OnStopRequest
      TimeDuration delta = (TimeStamp::Now() - mOnDataFinishedTime);
      glean::networking::http_content_cssloader_ondatafinished_to_onstop_delay
          .AccumulateRawDuration(delta);
    }
    mSheetLoadData->mSheet->UnblockParsePromise();
  } else {
    if (mSheetLoadData->mRecordErrors) {
      // We can't report errors off main thread right now.
      return NS_OK;
    }
  }

  auto HandleErrorInMainThread = [&] {
    MOZ_ASSERT(mStatus != NS_OK_PARSE_SHEET);
    MOZ_ASSERT(NS_IsMainThread());
    if (net::UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(
            mStatus)) {
      // Handle sheet not loading error because source was a tracking URL (or
      // fingerprinting, cryptomining, etc). We make a note of this sheet node
      // by including it in a dedicated array of blocked tracking nodes under
      // its parent document.
      //
      // Multiple sheet load instances might be tied to this request, we
      // annotate each one linked to a valid owning element (node).
      //
      // TODO(emilio): Maybe this should be done in Loader::NotifyObservers?
      // Feels pretty random here.
      for (SheetLoadData* data = mSheetLoadData; data; data = data->mNext) {
        if (nsINode* node = data->mSheet->GetOwnerNode()) {
          node->OwnerDoc()->AddBlockedNodeByClassifier(node);
        }
      }
    }
    mSheetLoadData->mLoader->SheetComplete(*mSheetLoadData, mStatus);
  };

  if (mOnStopProcessingDone) {
    MOZ_ASSERT(NS_IsMainThread());
    if (mStatus != NS_OK_PARSE_SHEET) {
      HandleErrorInMainThread();
    }
    return NS_OK;
  }

  mOnStopProcessingDone = true;

  // Decoded data
  nsCString utf8String;
  {
    nsresult status = NS_FAILED(mStatus) ? mStatus : aStatus;
    mStatus = mSheetLoadData->VerifySheetReadyToParse(status, mBOMBytes, mBytes,
                                                      channel);
    if (mStatus != NS_OK_PARSE_SHEET) {
      if (NS_IsMainThread()) {
        HandleErrorInMainThread();
      }
      return mStatus;
    }

    // At this point all the conditions that requires us to run on main
    // are checked in VerifySheetReadyToParse

    // BOM detection generally happens during the write callback, but that
    // won't have happened if fewer than three bytes were received.
    if (mEncodingFromBOM.isNothing()) {
      HandleBOM();
      MOZ_ASSERT(mEncodingFromBOM.isSome());
    }
    // Hold the nsStringBuffer for the bytes from the stack to ensure release
    // after its scope ends
    nsCString bytes = std::move(mBytes);
    // The BOM handling has happened, but we still may not have an encoding if
    // there was no BOM. Ensure we have one.
    const Encoding* encoding = mEncodingFromBOM.value();
    if (!encoding) {
      // No BOM
      encoding = mSheetLoadData->DetermineNonBOMEncoding(bytes, channel);
    }
    mSheetLoadData->mEncoding = encoding;

    size_t validated = 0;
    if (encoding == UTF_8_ENCODING) {
      validated = Encoding::UTF8ValidUpTo(bytes);
    }

    if (validated == bytes.Length()) {
      // Either this is UTF-8 and all valid, or it's not UTF-8 but is an empty
      // string. This assumes that an empty string in any encoding decodes to
      // empty string, which seems like a plausible assumption.
      utf8String = std::move(bytes);
    } else {
      // FIXME: Seems early returning here is wrong, what completes the sheet?
      MOZ_TRY(encoding->DecodeWithoutBOMHandling(bytes, utf8String, validated));
    }
  }  // run destructor for `bytes`

  // For reasons I don't understand, factoring the below lines into
  // a method on SheetLoadData resulted in a linker error. Hence,
  // accessing fields of mSheetLoadData from here.
  mSheetLoadData->mLoader->ParseSheet(utf8String, mMainThreadSheetLoadData,
                                      Loader::AllowAsyncParse::Yes);

  return NS_OK;
}

/* nsIStreamListener implementation */
NS_IMETHODIMP
StreamLoader::OnDataAvailable(nsIRequest*, nsIInputStream* aInputStream,
                              uint64_t, uint32_t aCount) {
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }
  uint32_t dummy;
  return aInputStream->ReadSegments(WriteSegmentFun, this, aCount, &dummy);
}

void StreamLoader::HandleBOM() {
  MOZ_ASSERT(mEncodingFromBOM.isNothing());
  MOZ_ASSERT(mBytes.IsEmpty());

  auto [encoding, bomLength] = Encoding::ForBOM(mBOMBytes);
  mEncodingFromBOM.emplace(encoding);  // Null means no BOM.

  // BOMs are three bytes at most, but may be fewer. Copy over anything
  // that wasn't part of the BOM to mBytes. Note that we need to track
  // any BOM bytes as well for SRI handling.
  mBytes.Append(Substring(mBOMBytes, bomLength));
  mBOMBytes.Truncate(bomLength);
}

NS_IMETHODIMP
StreamLoader::OnDataFinished(nsresult aResult) {
  nsCOMPtr<nsIRequest> request = mRequest.forget();
  if (StaticPrefs::network_send_OnDataFinished_cssLoader()) {
    MOZ_ASSERT(mOnDataFinishedTime.IsNull(),
               "OnDataFinished should only be called once");
    mOnDataFinishedTime = TimeStamp::Now();
    return OnStopRequest(request, aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
StreamLoader::GetInterface(const nsIID& aIID, void** aResult) {
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    return QueryInterface(aIID, aResult);
  }

  return NS_NOINTERFACE;
}

nsresult StreamLoader::AsyncOnChannelRedirect(
    nsIChannel* aOld, nsIChannel* aNew, uint32_t aFlags,
    nsIAsyncVerifyRedirectCallback* aCallback) {
  mSheetLoadData->SetMinimumExpirationTime(
      nsContentUtils::GetSubresourceCacheExpirationTime(aOld,
                                                        mSheetLoadData->mURI));

  aCallback->OnRedirectVerifyCallback(NS_OK);

  return NS_OK;
}

nsresult StreamLoader::WriteSegmentFun(nsIInputStream*, void* aClosure,
                                       const char* aSegment, uint32_t,
                                       uint32_t aCount, uint32_t* aWriteCount) {
  *aWriteCount = 0;
  StreamLoader* self = static_cast<StreamLoader*>(aClosure);
  if (NS_FAILED(self->mStatus)) {
    return self->mStatus;
  }

  // If we haven't done BOM detection yet, divert bytes into the special buffer.
  if (self->mEncodingFromBOM.isNothing()) {
    size_t bytesToCopy = std::min<size_t>(3 - self->mBOMBytes.Length(), aCount);
    self->mBOMBytes.Append(aSegment, bytesToCopy);
    aSegment += bytesToCopy;
    *aWriteCount += bytesToCopy;
    aCount -= bytesToCopy;

    if (self->mBOMBytes.Length() == 3) {
      self->HandleBOM();
    } else {
      return NS_OK;
    }
  }

  if (!self->mBytes.Append(aSegment, aCount, fallible)) {
    self->mBytes.Truncate();
    return (self->mStatus = NS_ERROR_OUT_OF_MEMORY);
  }

  *aWriteCount += aCount;
  return NS_OK;
}

}  // namespace mozilla::css
