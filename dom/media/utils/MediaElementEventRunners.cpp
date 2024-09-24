/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaElementEventRunners.h"
#include <stdint.h>

#include "MediaProfilerMarkers.h"
#include "mozilla/Casting.h"
#include "mozilla/ProfilerState.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/MediaError.h"
#include "mozilla/dom/TimeRanges.h"

extern mozilla::LazyLogModule gMediaElementEventsLog;
#define LOG_EVENT(type, msg) MOZ_LOG(gMediaElementEventsLog, type, msg)

namespace mozilla::dom {

nsMediaEventRunner::nsMediaEventRunner(const nsAString& aName,
                                       HTMLMediaElement* aElement,
                                       const nsAString& aEventName)
    : mElement(aElement),
      mName(aName),
      mEventName(aEventName),
      mLoadID(mElement->GetCurrentLoadID()) {}

bool nsMediaEventRunner::IsCancelled() const {
  return !mElement || mElement->GetCurrentLoadID() != mLoadID;
}

nsresult nsMediaEventRunner::DispatchEvent(const nsAString& aName) {
  nsresult rv = NS_OK;
  if (mElement) {
    ReportProfilerMarker();
    rv = mElement->DispatchEvent(aName);
  }
  return rv;
}

void nsMediaEventRunner::ReportProfilerMarker() {
  if (!profiler_is_collecting_markers()) {
    return;
  }
  // Report the buffered range.
  if (mEventName.EqualsLiteral("progress")) {
    RefPtr<TimeRanges> buffered = mElement->Buffered();
    if (buffered && buffered->Length() > 0) {
      for (size_t i = 0; i < buffered->Length(); ++i) {
        profiler_add_marker(nsPrintfCString("%p:progress", mElement.get()),
                            geckoprofiler::category::MEDIA_PLAYBACK, {},
                            BufferedUpdateMarker{},
                            AssertedCast<uint64_t>(buffered->Start(i) * 1000),
                            AssertedCast<uint64_t>(buffered->End(i) * 1000),
                            GetElementDurationMs());
      }
    }
  } else if (mEventName.EqualsLiteral("resize")) {
    MOZ_ASSERT(mElement->HasVideo());
    auto mediaInfo = mElement->GetMediaInfo();
    profiler_add_marker(nsPrintfCString("%p:resize", mElement.get()),
                        geckoprofiler::category::MEDIA_PLAYBACK, {},
                        VideoResizeMarker{}, mediaInfo.mVideo.mDisplay.width,
                        mediaInfo.mVideo.mDisplay.height);
  } else if (mEventName.EqualsLiteral("loadedmetadata")) {
    nsString src;
    mElement->GetCurrentSrc(src);
    auto mediaInfo = mElement->GetMediaInfo();
    profiler_add_marker(
        nsPrintfCString("%p:loadedmetadata", mElement.get()),
        geckoprofiler::category::MEDIA_PLAYBACK, {}, MetadataMarker{}, src,
        mediaInfo.HasAudio() ? mediaInfo.mAudio.mMimeType : "none"_ns,
        mediaInfo.HasVideo() ? mediaInfo.mVideo.mMimeType : "none"_ns);
  } else if (mEventName.EqualsLiteral("error")) {
    auto* error = mElement->GetError();
    nsString message;
    error->GetMessage(message);
    profiler_add_marker(nsPrintfCString("%p:error", mElement.get()),
                        geckoprofiler::category::MEDIA_PLAYBACK, {},
                        ErrorMarker{}, message);
  } else {
    nsPrintfCString markerName{"%p:", mElement.get()};
    markerName += NS_ConvertUTF16toUTF8(mEventName);
    PROFILER_MARKER_UNTYPED(markerName, MEDIA_PLAYBACK);
  }
}

uint64_t nsMediaEventRunner::GetElementDurationMs() const {
  MOZ_ASSERT(!IsCancelled());
  double duration = mElement->Duration();

  if (duration == std::numeric_limits<double>::infinity()) {
    return std::numeric_limits<uint64_t>::max();
  }

  if (std::isnan(duration) || duration <= 0) {
    // Duration is unknown or invalid
    return 0;
  }
  return AssertedCast<uint64_t>(duration * 1000);
}

NS_IMPL_CYCLE_COLLECTION(nsMediaEventRunner, mElement)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsMediaEventRunner)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsMediaEventRunner)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsMediaEventRunner)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRunnable)
NS_INTERFACE_MAP_END

NS_IMETHODIMP nsAsyncEventRunner::Run() {
  // Silently cancel if our load has been cancelled or element has been CCed.
  return IsCancelled() ? NS_OK : DispatchEvent(mEventName);
}

nsResolveOrRejectPendingPlayPromisesRunner::
    nsResolveOrRejectPendingPlayPromisesRunner(
        HTMLMediaElement* aElement, nsTArray<RefPtr<PlayPromise>>&& aPromises,
        nsresult aError)
    : nsMediaEventRunner(u"nsResolveOrRejectPendingPlayPromisesRunner"_ns,
                         aElement),
      mPromises(std::move(aPromises)),
      mError(aError) {
  mElement->mPendingPlayPromisesRunners.AppendElement(this);
}

void nsResolveOrRejectPendingPlayPromisesRunner::ResolveOrReject() {
  if (NS_SUCCEEDED(mError)) {
    PlayPromise::ResolvePromisesWithUndefined(mPromises);
  } else {
    PlayPromise::RejectPromises(mPromises, mError);
  }
}

NS_IMETHODIMP nsResolveOrRejectPendingPlayPromisesRunner::Run() {
  if (!IsCancelled()) {
    ResolveOrReject();
  }

  mElement->mPendingPlayPromisesRunners.RemoveElement(this);
  return NS_OK;
}

NS_IMETHODIMP nsNotifyAboutPlayingRunner::Run() {
  if (!IsCancelled()) {
    DispatchEvent(u"playing"_ns);
  }
  return nsResolveOrRejectPendingPlayPromisesRunner::Run();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsResolveOrRejectPendingPlayPromisesRunner,
                                   nsMediaEventRunner, mPromises)
NS_IMPL_ADDREF_INHERITED(nsResolveOrRejectPendingPlayPromisesRunner,
                         nsMediaEventRunner)
NS_IMPL_RELEASE_INHERITED(nsResolveOrRejectPendingPlayPromisesRunner,
                          nsMediaEventRunner)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
    nsResolveOrRejectPendingPlayPromisesRunner)
NS_INTERFACE_MAP_END_INHERITING(nsMediaEventRunner)

NS_IMETHODIMP nsSourceErrorEventRunner::Run() {
  // Silently cancel if our load has been cancelled.
  if (IsCancelled()) {
    return NS_OK;
  }
  LOG_EVENT(LogLevel::Debug,
            ("%p Dispatching simple event source error", mElement.get()));
  if (profiler_is_collecting_markers()) {
    profiler_add_marker(nsPrintfCString("%p:sourceerror", mElement.get()),
                        geckoprofiler::category::MEDIA_PLAYBACK, {},
                        ErrorMarker{}, mErrorDetails);
  }
  return nsContentUtils::DispatchTrustedEvent(mElement->OwnerDoc(), mSource,
                                              u"error"_ns, CanBubble::eNo,
                                              Cancelable::eNo);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsSourceErrorEventRunner, nsMediaEventRunner,
                                   mSource)
NS_IMPL_ADDREF_INHERITED(nsSourceErrorEventRunner, nsMediaEventRunner)
NS_IMPL_RELEASE_INHERITED(nsSourceErrorEventRunner, nsMediaEventRunner)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSourceErrorEventRunner)
NS_INTERFACE_MAP_END_INHERITING(nsMediaEventRunner)

NS_IMETHODIMP nsTimeupdateRunner::Run() {
  if (IsCancelled() || !ShouldDispatchTimeupdate()) {
    return NS_OK;
  }
  // After dispatching `timeupdate`, if the timeupdate event listener takes lots
  // of time then we end up spending all time handling just timeupdate events.
  // The spec is vague in this situation, so we choose to update time after we
  // dispatch the event in order to solve that issue.
  nsresult rv = DispatchEvent(mEventName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_EVENT(LogLevel::Debug,
              ("%p Failed to dispatch 'timeupdate'", mElement.get()));
  } else {
    mElement->UpdateLastTimeupdateDispatchTime();
  }
  return rv;
}

bool nsTimeupdateRunner::ShouldDispatchTimeupdate() const {
  if (mIsMandatory) {
    return true;
  }

  // If the main thread is busy, tasks may be delayed and dispatched at
  // unexpected times. Ensure we don't dispatch `timeupdate` more often
  // than once per `TIMEUPDATE_MS`.
  const TimeStamp& lastTime = mElement->LastTimeupdateDispatchTime();
  return lastTime.IsNull() || TimeStamp::Now() - lastTime >
                                  TimeDuration::FromMilliseconds(TIMEUPDATE_MS);
}

void nsTimeupdateRunner::ReportProfilerMarker() {
  if (!profiler_is_collecting_markers()) {
    return;
  }
  auto* videoElement = mElement->AsHTMLVideoElement();
  profiler_add_marker(nsPrintfCString("%p:timeupdate", mElement.get()),
                      geckoprofiler::category::MEDIA_PLAYBACK, {},
                      TimeUpdateMarker{},
                      AssertedCast<uint64_t>(mElement->CurrentTime() * 1000),
                      GetElementDurationMs(),
                      videoElement ? videoElement->MozPaintedFrames() : 0);
}

#undef LOG_EVENT
}  // namespace mozilla::dom
