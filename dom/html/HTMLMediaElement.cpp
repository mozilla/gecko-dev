/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLMediaElementBinding.h"
#include "mozilla/dom/HTMLSourceElement.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/AsyncEventDispatcher.h"
#ifdef MOZ_EME
#include "mozilla/dom/MediaEncryptedEvent.h"
#endif

#include "base/basictypes.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsIDOMHTMLSourceElement.h"
#include "TimeRanges.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDocShell.h"
#include "nsError.h"
#include "nsNodeInfoManager.h"
#include "nsNetUtil.h"
#include "nsXPCOMStrings.h"
#include "xpcpublic.h"
#include "nsThreadUtils.h"
#include "nsIThreadInternal.h"
#include "nsContentUtils.h"
#include "nsIRequest.h"
#include "nsQueryObject.h"

#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"

#include "nsITimer.h"

#include "MediaError.h"
#include "MediaDecoder.h"
#include "nsICategoryManager.h"
#include "MediaResource.h"

#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsCORSListenerProxy.h"
#include "nsCycleCollectionParticipant.h"
#include "nsICachingChannel.h"
#include "nsLayoutUtils.h"
#include "nsVideoFrame.h"
#include "Layers.h"
#include <limits>
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsMediaFragmentURIParser.h"
#include "nsURIHashKey.h"
#include "nsJSUtils.h"
#include "MediaStreamGraph.h"
#include "nsIScriptError.h"
#include "nsHostObjectProtocolHandler.h"
#include "mozilla/dom/MediaSource.h"
#include "MediaMetadataManager.h"
#include "MediaSourceDecoder.h"
#include "AudioStreamTrack.h"
#include "VideoStreamTrack.h"

#include "AudioChannelService.h"

#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/WakeLock.h"

#include "mozilla/dom/AudioTrack.h"
#include "mozilla/dom/AudioTrackList.h"
#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/VideoTrackList.h"
#include "mozilla/dom/TextTrack.h"
#include "nsIContentPolicy.h"
#include "mozilla/Telemetry.h"

#include "ImageContainer.h"
#include "nsRange.h"
#include <algorithm>

static PRLogModuleInfo* gMediaElementLog;
static PRLogModuleInfo* gMediaElementEventsLog;
#define LOG(type, msg) MOZ_LOG(gMediaElementLog, type, msg)
#define LOG_EVENT(type, msg) MOZ_LOG(gMediaElementEventsLog, type, msg)

#include "nsIContentSecurityPolicy.h"

#include "mozilla/Preferences.h"
#include "mozilla/FloatingPoint.h"

#include "nsIPermissionManager.h"
#include "nsContentTypeParser.h"

#include "mozilla/EventStateManager.h"

using namespace mozilla::layers;
using mozilla::net::nsMediaFragmentURIParser;

namespace mozilla {
namespace dom {

// Number of milliseconds between progress events as defined by spec
static const uint32_t PROGRESS_MS = 350;

// Number of milliseconds of no data before a stall event is fired as defined by spec
static const uint32_t STALL_MS = 3000;

// Used by AudioChannel for suppresssing the volume to this ratio.
#define FADED_VOLUME_RATIO 0.25

// These constants are arbitrary
// Minimum playbackRate for a media
static const double MIN_PLAYBACKRATE = 0.25;
// Maximum playbackRate for a media
static const double MAX_PLAYBACKRATE = 5.0;
// These are the limits beyonds which SoundTouch does not perform too well and when
// speech is hard to understand anyway.
// Threshold above which audio is muted
static const double THRESHOLD_HIGH_PLAYBACKRATE_AUDIO = 4.0;
// Threshold under which audio is muted
static const double THRESHOLD_LOW_PLAYBACKRATE_AUDIO = 0.5;

// Under certain conditions there may be no-one holding references to
// a media element from script, DOM parent, etc, but the element may still
// fire meaningful events in the future so we can't destroy it yet:
// 1) If the element is delaying the load event (or would be, if it were
// in a document), then events up to loadeddata or error could be fired,
// so we need to stay alive.
// 2) If the element is not paused and playback has not ended, then
// we will (or might) play, sending timeupdate and ended events and possibly
// audio output, so we need to stay alive.
// 3) if the element is seeking then we will fire seeking events and possibly
// start playing afterward, so we need to stay alive.
// 4) If autoplay could start playback in this element (if we got enough data),
// then we need to stay alive.
// 5) if the element is currently loading, not suspended, and its source is
// not a MediaSource, then script might be waiting for progress events or a
// 'stalled' or 'suspend' event, so we need to stay alive.
// If we're already suspended then (all other conditions being met),
// it's OK to just disappear without firing any more events,
// since we have the freedom to remain suspended indefinitely. Note
// that we could use this 'suspended' loophole to garbage-collect a suspended
// element in case 4 even if it had 'autoplay' set, but we choose not to.
// If someone throws away all references to a loading 'autoplay' element
// sound should still eventually play.
// 6) If the source is a MediaSource, most loading events will not fire unless
// appendBuffer() is called on a SourceBuffer, in which case something is
// already referencing the SourceBuffer, which keeps the associated media
// element alive. Further, a MediaSource will never time out the resource
// fetch, and so should not keep the media element alive if it is
// unreferenced. A pending 'stalled' event keeps the media element alive.
//
// Media elements owned by inactive documents (i.e. documents not contained in any
// document viewer) should never hold a self-reference because none of the
// above conditions are allowed: the element will stop loading and playing
// and never resume loading or playing unless its owner document changes to
// an active document (which can only happen if there is an external reference
// to the element).
// Media elements with no owner doc should be able to hold a self-reference.
// Something native must have created the element and may expect it to
// stay alive to play.

// It's very important that any change in state which could change the value of
// needSelfReference in AddRemoveSelfReference be followed by a call to
// AddRemoveSelfReference before this element could die!
// It's especially important if needSelfReference would change to 'true',
// since if we neglect to add a self-reference, this element might be
// garbage collected while there are still event listeners that should
// receive events. If we neglect to remove the self-reference then the element
// just lives longer than it needs to.

class nsMediaEvent : public nsRunnable
{
public:

  explicit nsMediaEvent(HTMLMediaElement* aElement) :
    mElement(aElement),
    mLoadID(mElement->GetCurrentLoadID()) {}
  ~nsMediaEvent() {}

  NS_IMETHOD Run() = 0;

protected:
  bool IsCancelled() {
    return mElement->GetCurrentLoadID() != mLoadID;
  }

  nsRefPtr<HTMLMediaElement> mElement;
  uint32_t mLoadID;
};

class HTMLMediaElement::nsAsyncEventRunner : public nsMediaEvent
{
private:
  nsString mName;

public:
  nsAsyncEventRunner(const nsAString& aName, HTMLMediaElement* aElement) :
    nsMediaEvent(aElement), mName(aName)
  {
  }

  NS_IMETHOD Run()
  {
    // Silently cancel if our load has been cancelled.
    if (IsCancelled())
      return NS_OK;

    return mElement->DispatchEvent(mName);
  }
};

class nsSourceErrorEventRunner : public nsMediaEvent
{
private:
  nsCOMPtr<nsIContent> mSource;
public:
  nsSourceErrorEventRunner(HTMLMediaElement* aElement,
                           nsIContent* aSource)
    : nsMediaEvent(aElement),
      mSource(aSource)
  {
  }

  NS_IMETHOD Run() {
    // Silently cancel if our load has been cancelled.
    if (IsCancelled())
      return NS_OK;
    LOG_EVENT(LogLevel::Debug, ("%p Dispatching simple event source error", mElement.get()));
    return nsContentUtils::DispatchTrustedEvent(mElement->OwnerDoc(),
                                                mSource,
                                                NS_LITERAL_STRING("error"),
                                                false,
                                                false);
  }
};

/**
 * There is a reference cycle involving this class: MediaLoadListener
 * holds a reference to the HTMLMediaElement, which holds a reference
 * to an nsIChannel, which holds a reference to this listener.
 * We break the reference cycle in OnStartRequest by clearing mElement.
 */
class HTMLMediaElement::MediaLoadListener final : public nsIStreamListener,
                                                  public nsIChannelEventSink,
                                                  public nsIInterfaceRequestor,
                                                  public nsIObserver
{
  ~MediaLoadListener() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR

public:
  explicit MediaLoadListener(HTMLMediaElement* aElement)
    : mElement(aElement),
      mLoadID(aElement->GetCurrentLoadID())
  {
    MOZ_ASSERT(mElement, "Must pass an element to call back");
  }

private:
  nsRefPtr<HTMLMediaElement> mElement;
  nsCOMPtr<nsIStreamListener> mNextListener;
  uint32_t mLoadID;
};

NS_IMPL_ISUPPORTS(HTMLMediaElement::MediaLoadListener, nsIRequestObserver,
                  nsIStreamListener, nsIChannelEventSink,
                  nsIInterfaceRequestor, nsIObserver)

NS_IMETHODIMP
HTMLMediaElement::MediaLoadListener::Observe(nsISupports* aSubject,
                                             const char* aTopic, const char16_t* aData)
{
  nsContentUtils::UnregisterShutdownObserver(this);

  // Clear mElement to break cycle so we don't leak on shutdown
  mElement = nullptr;
  return NS_OK;
}

void HTMLMediaElement::ReportLoadError(const char* aMsg,
                                       const char16_t** aParams,
                                       uint32_t aParamCount)
{
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Media"),
                                  OwnerDoc(),
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aMsg,
                                  aParams,
                                  aParamCount);
}


NS_IMETHODIMP HTMLMediaElement::MediaLoadListener::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  nsContentUtils::UnregisterShutdownObserver(this);

  if (!mElement) {
    // We've been notified by the shutdown observer, and are shutting down.
    return NS_BINDING_ABORTED;
  }

  // The element is only needed until we've had a chance to call
  // InitializeDecoderForChannel. So make sure mElement is cleared here.
  nsRefPtr<HTMLMediaElement> element;
  element.swap(mElement);

  if (mLoadID != element->GetCurrentLoadID()) {
    // The channel has been cancelled before we had a chance to create
    // a decoder. Abort, don't dispatch an "error" event, as the new load
    // may not be in an error state.
    return NS_BINDING_ABORTED;
  }

  // Don't continue to load if the request failed or has been canceled.
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_FAILED(status)) {
    if (element)
      element->NotifyLoadError();
    return status;
  }

  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(aRequest);
  bool succeeded;
  if (hc && NS_SUCCEEDED(hc->GetRequestSucceeded(&succeeded)) && !succeeded) {
    element->NotifyLoadError();
    uint32_t responseStatus = 0;
    hc->GetResponseStatus(&responseStatus);
    nsAutoString code;
    code.AppendInt(responseStatus);
    nsAutoString src;
    element->GetCurrentSrc(src);
    const char16_t* params[] = { code.get(), src.get() };
    element->ReportLoadError("MediaLoadHttpError", params, ArrayLength(params));
    return NS_BINDING_ABORTED;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (channel &&
      element &&
      NS_SUCCEEDED(rv = element->InitializeDecoderForChannel(channel, getter_AddRefs(mNextListener))) &&
      mNextListener) {
    rv = mNextListener->OnStartRequest(aRequest, aContext);
  } else {
    // If InitializeDecoderForChannel() returned an error, fire a network
    // error.
    if (NS_FAILED(rv) && !mNextListener && element) {
      // Load failed, attempt to load the next candidate resource. If there
      // are none, this will trigger a MEDIA_ERR_SRC_NOT_SUPPORTED error.
      element->NotifyLoadError();
    }
    // If InitializeDecoderForChannel did not return a listener (but may
    // have otherwise succeeded), we abort the connection since we aren't
    // interested in keeping the channel alive ourselves.
    rv = NS_BINDING_ABORTED;
  }

  return rv;
}

NS_IMETHODIMP HTMLMediaElement::MediaLoadListener::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                                                                 nsresult aStatus)
{
  if (mNextListener) {
    return mNextListener->OnStopRequest(aRequest, aContext, aStatus);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLMediaElement::MediaLoadListener::OnDataAvailable(nsIRequest* aRequest,
                                                     nsISupports* aContext,
                                                     nsIInputStream* aStream,
                                                     uint64_t aOffset,
                                                     uint32_t aCount)
{
  if (!mNextListener) {
    NS_ERROR("Must have a chained listener; OnStartRequest should have canceled this request");
    return NS_BINDING_ABORTED;
  }
  return mNextListener->OnDataAvailable(aRequest, aContext, aStream, aOffset, aCount);
}

NS_IMETHODIMP HTMLMediaElement::MediaLoadListener::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                                                          nsIChannel* aNewChannel,
                                                                          uint32_t aFlags,
                                                                          nsIAsyncVerifyRedirectCallback* cb)
{
  // TODO is this really correct?? See bug #579329.
  if (mElement)
    mElement->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);
  nsCOMPtr<nsIChannelEventSink> sink = do_QueryInterface(mNextListener);
  if (sink)
    return sink->AsyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags, cb);

  cb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP HTMLMediaElement::MediaLoadListener::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

NS_IMPL_ADDREF_INHERITED(HTMLMediaElement, nsGenericHTMLElement)
NS_IMPL_RELEASE_INHERITED(HTMLMediaElement, nsGenericHTMLElement)

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLMediaElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLMediaElement, nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSrcMediaSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSrcStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlaybackStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSrcAttrStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourcePointer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoadBlockedDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceLoadCandidate)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAudioChannelAgent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
  for (uint32_t i = 0; i < tmp->mOutputStreams.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOutputStreams[i].mStream);
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlayed);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTextTrackManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAudioTrackList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVideoTrackList)
#ifdef MOZ_EME
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaKeys)
#endif
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLMediaElement, nsGenericHTMLElement)
  if (tmp->mSrcStream) {
    // Need to EndMediaStreamPlayback to clear mSrcStream and make sure everything
    // gets unhooked correctly.
    tmp->EndSrcMediaStreamPlayback();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSrcAttrStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMediaSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSrcMediaSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourcePointer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoadBlockedDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourceLoadCandidate)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAudioChannelAgent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
  for (uint32_t i = 0; i < tmp->mOutputStreams.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mOutputStreams[i].mStream)
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPlayed)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTextTrackManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAudioTrackList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVideoTrackList)
#ifdef MOZ_EME
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMediaKeys)
#endif
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLMediaElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLMediaElement)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

// nsIDOMHTMLMediaElement
NS_IMPL_URI_ATTR(HTMLMediaElement, Src, src)
NS_IMPL_BOOL_ATTR(HTMLMediaElement, Controls, controls)
NS_IMPL_BOOL_ATTR(HTMLMediaElement, Autoplay, autoplay)
NS_IMPL_BOOL_ATTR(HTMLMediaElement, Loop, loop)
NS_IMPL_BOOL_ATTR(HTMLMediaElement, DefaultMuted, muted)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(HTMLMediaElement, Preload, preload, nullptr)

NS_IMETHODIMP
HTMLMediaElement::GetMozAudioChannelType(nsAString& aValue)
{
  nsString defaultValue;
  AudioChannelService::GetDefaultAudioChannelString(defaultValue);

  NS_ConvertUTF16toUTF8 str(defaultValue);
  GetEnumAttr(nsGkAtoms::mozaudiochannel, str.get(), aValue);
  return NS_OK;
}

NS_IMETHODIMP
HTMLMediaElement::SetMozAudioChannelType(const nsAString& aValue)
{
  return SetAttrHelper(nsGkAtoms::mozaudiochannel, aValue);
}

NS_IMETHODIMP_(bool)
HTMLMediaElement::IsVideo()
{
  return false;
}

already_AddRefed<MediaSource>
HTMLMediaElement::GetMozMediaSourceObject() const
{
  nsRefPtr<MediaSource> source = mMediaSource;
  return source.forget();
}

already_AddRefed<DOMMediaStream>
HTMLMediaElement::GetMozSrcObject() const
{
  NS_ASSERTION(!mSrcAttrStream || mSrcAttrStream->GetStream(),
               "MediaStream should have been set up properly");
  nsRefPtr<DOMMediaStream> stream = mSrcAttrStream;
  return stream.forget();
}

void
HTMLMediaElement::SetMozSrcObject(DOMMediaStream& aValue)
{
  SetMozSrcObject(&aValue);
}

void
HTMLMediaElement::SetMozSrcObject(DOMMediaStream* aValue)
{
  mSrcAttrStream = aValue;
  DoLoad();
}

/* readonly attribute nsIDOMHTMLMediaElement mozAutoplayEnabled; */
NS_IMETHODIMP HTMLMediaElement::GetMozAutoplayEnabled(bool *aAutoplayEnabled)
{
  *aAutoplayEnabled = mAutoplayEnabled;

  return NS_OK;
}

/* readonly attribute nsIDOMMediaError error; */
NS_IMETHODIMP HTMLMediaElement::GetError(nsIDOMMediaError * *aError)
{
  NS_IF_ADDREF(*aError = mError);

  return NS_OK;
}

/* readonly attribute boolean ended; */
bool
HTMLMediaElement::Ended()
{
  if (mSrcStream) {
    return GetSrcMediaStream()->IsFinished();
  }

  if (mDecoder) {
    return mDecoder->IsEndedOrShutdown();
  }

  return false;
}

NS_IMETHODIMP HTMLMediaElement::GetEnded(bool* aEnded)
{
  *aEnded = Ended();
  return NS_OK;
}

/* readonly attribute DOMString currentSrc; */
NS_IMETHODIMP HTMLMediaElement::GetCurrentSrc(nsAString & aCurrentSrc)
{
  nsAutoCString src;
  GetCurrentSpec(src);
  aCurrentSrc = NS_ConvertUTF8toUTF16(src);
  return NS_OK;
}

/* readonly attribute unsigned short networkState; */
NS_IMETHODIMP HTMLMediaElement::GetNetworkState(uint16_t* aNetworkState)
{
  *aNetworkState = NetworkState();
  return NS_OK;
}

nsresult
HTMLMediaElement::OnChannelRedirect(nsIChannel* aChannel,
                                    nsIChannel* aNewChannel,
                                    uint32_t aFlags)
{
  NS_ASSERTION(aChannel == mChannel, "Channels should match!");
  mChannel = aNewChannel;

  // Handle forwarding of Range header so that the intial detection
  // of seeking support (via result code 206) works across redirects.
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aChannel);
  NS_ENSURE_STATE(http);

  NS_NAMED_LITERAL_CSTRING(rangeHdr, "Range");

  nsAutoCString rangeVal;
  if (NS_SUCCEEDED(http->GetRequestHeader(rangeHdr, rangeVal))) {
    NS_ENSURE_STATE(!rangeVal.IsEmpty());

    http = do_QueryInterface(aNewChannel);
    NS_ENSURE_STATE(http);

    nsresult rv = http->SetRequestHeader(rangeHdr, rangeVal, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void HTMLMediaElement::ShutdownDecoder()
{
  RemoveMediaElementFromURITable();
  NS_ASSERTION(mDecoder, "Must have decoder to shut down");
  mDecoder->Shutdown();
  mDecoder = nullptr;
}

void HTMLMediaElement::AbortExistingLoads()
{
#ifdef MOZ_EME
  // If there is no existing decoder then we don't have anything to
  // report. This prevents reporting the initial load from an
  // empty video element as a failed EME load.
  if (mDecoder) {
    ReportEMETelemetry();
  }
#endif
  // Abort any already-running instance of the resource selection algorithm.
  mLoadWaitStatus = NOT_WAITING;

  // Set a new load ID. This will cause events which were enqueued
  // with a different load ID to silently be cancelled.
  mCurrentLoadID++;

  bool fireTimeUpdate = false;

  // When aborting the existing loads, empty the objects in audio track list and
  // video track list, no events (in particular, no removetrack events) are
  // fired as part of this. Ending MediaStream sends track ended notifications,
  // so we empty the track lists prior.
  AudioTracks()->EmptyTracks();
  VideoTracks()->EmptyTracks();

  if (mDecoder) {
    fireTimeUpdate = mDecoder->GetCurrentTime() != 0.0;
    ShutdownDecoder();
  }
  if (mSrcStream) {
    EndSrcMediaStreamPlayback();
  }

  RemoveMediaElementFromURITable();
  mLoadingSrc = nullptr;
  mMediaSource = nullptr;

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING ||
      mNetworkState == nsIDOMHTMLMediaElement::NETWORK_IDLE)
  {
    DispatchAsyncEvent(NS_LITERAL_STRING("abort"));
  }

  mError = nullptr;
  mLoadedDataFired = false;
  mAutoplaying = true;
  mIsLoadingFromSourceChildren = false;
  mSuspendedAfterFirstFrame = false;
  mAllowSuspendAfterFirstFrame = true;
  mHaveQueuedSelectResource = false;
  mSuspendedForPreloadNone = false;
  mDownloadSuspendedByCache = false;
  mMediaInfo = MediaInfo();
  mIsEncrypted = false;
#ifdef MOZ_EME
  mPendingEncryptedInitData.mInitDatas.Clear();
#endif // MOZ_EME
  mSourcePointer = nullptr;

  mTags = nullptr;

  if (mNetworkState != nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    NS_ASSERTION(!mDecoder && !mSrcStream, "How did someone setup a new stream/decoder already?");
    // ChangeNetworkState() will call UpdateAudioChannelPlayingState()
    // indirectly which depends on mPaused. So we need to update mPaused first.
    mPaused = true;
    ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_EMPTY);
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_NOTHING);

    if (fireTimeUpdate) {
      // Since we destroyed the decoder above, the current playback position
      // will now be reported as 0. The playback position was non-zero when
      // we destroyed the decoder, so fire a timeupdate event so that the
      // change will be reflected in the controls.
      FireTimeUpdate(false);
    }
    DispatchAsyncEvent(NS_LITERAL_STRING("emptied"));
  }

  // We may have changed mPaused, mAutoplaying, and other
  // things which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  mIsRunningSelectResource = false;
}

void HTMLMediaElement::NoSupportedMediaSourceError()
{
  NS_ASSERTION(mNetworkState == NETWORK_LOADING,
               "Not loading during source selection?");

  mError = new MediaError(this, nsIDOMMediaError::MEDIA_ERR_SRC_NOT_SUPPORTED);
  ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE);
  DispatchAsyncEvent(NS_LITERAL_STRING("error"));
  ChangeDelayLoadStatus(false);
}

typedef void (HTMLMediaElement::*SyncSectionFn)();

// Runs a "synchronous section", a function that must run once the event loop
// has reached a "stable state". See:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/webappapis.html#synchronous-section
class nsSyncSection : public nsMediaEvent
{
private:
  nsCOMPtr<nsIRunnable> mRunnable;
public:
  nsSyncSection(HTMLMediaElement* aElement,
                nsIRunnable* aRunnable) :
    nsMediaEvent(aElement),
    mRunnable(aRunnable)
  {
  }

  NS_IMETHOD Run() {
    // Silently cancel if our load has been cancelled.
    if (IsCancelled())
      return NS_OK;
    mRunnable->Run();
    return NS_OK;
  }
};

void HTMLMediaElement::RunInStableState(nsIRunnable* aRunnable)
{
  nsCOMPtr<nsIRunnable> event = new nsSyncSection(this, aRunnable);
  nsContentUtils::RunInStableState(event.forget());
}

void HTMLMediaElement::QueueLoadFromSourceTask()
{
  ChangeDelayLoadStatus(true);
  ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_LOADING);
  RunInStableState(
    NS_NewRunnableMethod(this, &HTMLMediaElement::LoadFromSourceChildren));
}

void HTMLMediaElement::QueueSelectResourceTask()
{
  // Don't allow multiple async select resource calls to be queued.
  if (mHaveQueuedSelectResource)
    return;
  mHaveQueuedSelectResource = true;
  ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE);
  RunInStableState(
    NS_NewRunnableMethod(this, &HTMLMediaElement::SelectResourceWrapper));
}

/* void load (); */
NS_IMETHODIMP HTMLMediaElement::Load()
{
  if (mIsRunningLoadMethod) {
    return NS_OK;
  }

  mIsDoingExplicitLoad = true;
  DoLoad();

  return NS_OK;
}

void HTMLMediaElement::DoLoad()
{
  if (mIsRunningLoadMethod) {
    return;
  }

  SetPlayedOrSeeked(false);
  mIsRunningLoadMethod = true;
  AbortExistingLoads();
  SetPlaybackRate(mDefaultPlaybackRate);
  QueueSelectResourceTask();
  ResetState();
  mIsRunningLoadMethod = false;
}

void HTMLMediaElement::ResetState()
{
  // There might be a pending MediaDecoder::PlaybackPositionChanged() which
  // will overwrite |mMediaInfo.mVideo.mDisplay| in UpdateMediaSize() to give
  // staled videoWidth and videoHeight. We have to call ForgetElement() here
  // such that the staled callbacks won't reach us.
  if (mVideoFrameContainer) {
    mVideoFrameContainer->ForgetElement();
    mVideoFrameContainer = nullptr;
  }
}

static bool HasSourceChildren(nsIContent* aElement)
{
  for (nsIContent* child = aElement->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(nsGkAtoms::source))
    {
      return true;
    }
  }
  return false;
}

void HTMLMediaElement::SelectResourceWrapper()
{
  SelectResource();
  mIsRunningSelectResource = false;
  mHaveQueuedSelectResource = false;
  mIsDoingExplicitLoad = false;
}

void HTMLMediaElement::SelectResource()
{
  if (!mSrcAttrStream && !HasAttr(kNameSpaceID_None, nsGkAtoms::src) &&
      !HasSourceChildren(this)) {
    // The media element has neither a src attribute nor any source
    // element children, abort the load.
    ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_EMPTY);
    ChangeDelayLoadStatus(false);
    return;
  }

  ChangeDelayLoadStatus(true);

  ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_LOADING);
  DispatchAsyncEvent(NS_LITERAL_STRING("loadstart"));

  // Delay setting mIsRunningSeletResource until after UpdatePreloadAction
  // so that we don't lose our state change by bailing out of the preload
  // state update
  UpdatePreloadAction();
  mIsRunningSelectResource = true;

  // If we have a 'src' attribute, use that exclusively.
  nsAutoString src;
  if (mSrcAttrStream) {
    SetupSrcMediaStreamPlayback(mSrcAttrStream);
  } else if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NewURIFromString(src, getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv)) {
      LOG(LogLevel::Debug, ("%p Trying load from src=%s", this, NS_ConvertUTF16toUTF8(src).get()));
      NS_ASSERTION(!mIsLoadingFromSourceChildren,
        "Should think we're not loading from source children by default");

      RemoveMediaElementFromURITable();
      mLoadingSrc = uri;
      mMediaSource = mSrcMediaSource;
      UpdatePreloadAction();
      if (mPreloadAction == HTMLMediaElement::PRELOAD_NONE &&
          !IsMediaStreamURI(mLoadingSrc)) {
        // preload:none media, suspend the load here before we make any
        // network requests.
        SuspendLoad();
        return;
      }

      rv = LoadResource();
      if (NS_SUCCEEDED(rv)) {
        return;
      }
    } else {
      const char16_t* params[] = { src.get() };
      ReportLoadError("MediaLoadInvalidURI", params, ArrayLength(params));
    }
    NoSupportedMediaSourceError();
  } else {
    // Otherwise, the source elements will be used.
    mIsLoadingFromSourceChildren = true;
    LoadFromSourceChildren();
  }
}

void HTMLMediaElement::NotifyLoadError()
{
  if (!mIsLoadingFromSourceChildren) {
    LOG(LogLevel::Debug, ("NotifyLoadError(), no supported media error"));
    NoSupportedMediaSourceError();
  } else if (mSourceLoadCandidate) {
    DispatchAsyncSourceError(mSourceLoadCandidate);
    QueueLoadFromSourceTask();
  } else {
    NS_WARNING("Should know the source we were loading from!");
  }
}

void HTMLMediaElement::NotifyMediaTrackEnabled(MediaTrack* aTrack)
{
  if (!aTrack) {
    return;
  }

  // TODO: We are dealing with single audio track and video track for now.
  if (AudioTrack* track = aTrack->AsAudioTrack()) {
    if (!track->Enabled()) {
      SetMutedInternal(mMuted | MUTED_BY_AUDIO_TRACK);
    } else {
      SetMutedInternal(mMuted & ~MUTED_BY_AUDIO_TRACK);
    }
  } else if (VideoTrack* track = aTrack->AsVideoTrack()) {
    mDisableVideo = !track->Selected();
  }
}

void HTMLMediaElement::NotifyMediaStreamTracksAvailable(DOMMediaStream* aStream)
{
  if (!mSrcStream || mSrcStream != aStream) {
    return;
  }

  bool videoHasChanged = IsVideo() && HasVideo() != !VideoTracks()->IsEmpty();

  if (videoHasChanged) {
    // We are a video element and HasVideo() changed so update the screen
    // wakelock
    NotifyOwnerDocumentActivityChanged();
  }

  mWatchManager.ManualNotify(&HTMLMediaElement::UpdateReadyStateInternal);
}

void HTMLMediaElement::LoadFromSourceChildren()
{
  NS_ASSERTION(mDelayingLoadEvent,
               "Should delay load event (if in document) during load");
  NS_ASSERTION(mIsLoadingFromSourceChildren,
               "Must remember we're loading from source children");

  nsIDocument* parentDoc = OwnerDoc()->GetParentDocument();
  if (parentDoc) {
    parentDoc->FlushPendingNotifications(Flush_Layout);
  }

  while (true) {
    nsIContent* child = GetNextSource();
    if (!child) {
      // Exhausted candidates, wait for more candidates to be appended to
      // the media element.
      mLoadWaitStatus = WAITING_FOR_SOURCE;
      ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_NO_SOURCE);
      ChangeDelayLoadStatus(false);
      ReportLoadError("MediaLoadExhaustedCandidates");
      return;
    }

    // Must have src attribute.
    nsAutoString src;
    if (!child->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
      ReportLoadError("MediaLoadSourceMissingSrc");
      DispatchAsyncSourceError(child);
      continue;
    }

    // If we have a type attribute, it must be a supported type.
    nsAutoString type;
    if (child->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type) &&
        GetCanPlay(type) == CANPLAY_NO) {
      DispatchAsyncSourceError(child);
      const char16_t* params[] = { type.get(), src.get() };
      ReportLoadError("MediaLoadUnsupportedTypeAttribute", params, ArrayLength(params));
      continue;
    }
    nsAutoString media;
    HTMLSourceElement *childSrc = HTMLSourceElement::FromContent(child);
    MOZ_ASSERT(childSrc, "Expect child to be HTMLSourceElement");
    if (childSrc && !childSrc->MatchesCurrentMedia()) {
      DispatchAsyncSourceError(child);
      const char16_t* params[] = { media.get(), src.get() };
      ReportLoadError("MediaLoadSourceMediaNotMatched", params, ArrayLength(params));
      continue;
    }
    LOG(LogLevel::Debug, ("%p Trying load from <source>=%s type=%s media=%s", this,
      NS_ConvertUTF16toUTF8(src).get(), NS_ConvertUTF16toUTF8(type).get(),
      NS_ConvertUTF16toUTF8(media).get()));

    nsCOMPtr<nsIURI> uri;
    NewURIFromString(src, getter_AddRefs(uri));
    if (!uri) {
      DispatchAsyncSourceError(child);
      const char16_t* params[] = { src.get() };
      ReportLoadError("MediaLoadInvalidURI", params, ArrayLength(params));
      continue;
    }

    RemoveMediaElementFromURITable();
    mLoadingSrc = uri;
    mMediaSource = childSrc->GetSrcMediaSource();
    NS_ASSERTION(mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING,
                 "Network state should be loading");

    if (mPreloadAction == HTMLMediaElement::PRELOAD_NONE &&
        !IsMediaStreamURI(mLoadingSrc)) {
      // preload:none media, suspend the load here before we make any
      // network requests.
      SuspendLoad();
      return;
    }

    if (NS_SUCCEEDED(LoadResource())) {
      return;
    }

    // If we fail to load, loop back and try loading the next resource.
    DispatchAsyncSourceError(child);
  }
  NS_NOTREACHED("Execution should not reach here!");
}

void HTMLMediaElement::SuspendLoad()
{
  mSuspendedForPreloadNone = true;
  ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_IDLE);
  ChangeDelayLoadStatus(false);
}

void HTMLMediaElement::ResumeLoad(PreloadAction aAction)
{
  NS_ASSERTION(mSuspendedForPreloadNone,
    "Must be halted for preload:none to resume from preload:none suspended load.");
  mSuspendedForPreloadNone = false;
  mPreloadAction = aAction;
  ChangeDelayLoadStatus(true);
  ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_LOADING);
  if (!mIsLoadingFromSourceChildren) {
    // We were loading from the element's src attribute.
    if (NS_FAILED(LoadResource())) {
      NoSupportedMediaSourceError();
    }
  } else {
    // We were loading from a child <source> element. Try to resume the
    // load of that child, and if that fails, try the next child.
    if (NS_FAILED(LoadResource())) {
      LoadFromSourceChildren();
    }
  }
}

static bool IsAutoplayEnabled()
{
  return Preferences::GetBool("media.autoplay.enabled");
}

static bool UseAudioChannelService()
{
  return Preferences::GetBool("media.useAudioChannelService");
}

void HTMLMediaElement::UpdatePreloadAction()
{
  PreloadAction nextAction = PRELOAD_UNDEFINED;
  // If autoplay is set, or we're playing, we should always preload data,
  // as we'll need it to play.
  if ((IsAutoplayEnabled() && HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay)) ||
      !mPaused)
  {
    nextAction = HTMLMediaElement::PRELOAD_ENOUGH;
  } else {
    // Find the appropriate preload action by looking at the attribute.
    const nsAttrValue* val = mAttrsAndChildren.GetAttr(nsGkAtoms::preload,
                                                       kNameSpaceID_None);
    // MSE doesn't work if preload is none, so it ignores the pref when src is
    // from MSE.
    uint32_t preloadDefault = mMediaSource ?
                              HTMLMediaElement::PRELOAD_ATTR_METADATA :
                              Preferences::GetInt("media.preload.default",
                                                  HTMLMediaElement::PRELOAD_ATTR_METADATA);
    uint32_t preloadAuto =
      Preferences::GetInt("media.preload.auto",
                          HTMLMediaElement::PRELOAD_ENOUGH);
    if (!val) {
      // Attribute is not set. Use the preload action specified by the
      // media.preload.default pref, or just preload metadata if not present.
      nextAction = static_cast<PreloadAction>(preloadDefault);
    } else if (val->Type() == nsAttrValue::eEnum) {
      PreloadAttrValue attr = static_cast<PreloadAttrValue>(val->GetEnumValue());
      if (attr == HTMLMediaElement::PRELOAD_ATTR_EMPTY ||
          attr == HTMLMediaElement::PRELOAD_ATTR_AUTO)
      {
        nextAction = static_cast<PreloadAction>(preloadAuto);
      } else if (attr == HTMLMediaElement::PRELOAD_ATTR_METADATA) {
        nextAction = HTMLMediaElement::PRELOAD_METADATA;
      } else if (attr == HTMLMediaElement::PRELOAD_ATTR_NONE) {
        nextAction = HTMLMediaElement::PRELOAD_NONE;
      }
    } else {
      // Use the suggested "missing value default" of "metadata", or the value
      // specified by the media.preload.default, if present.
      nextAction = static_cast<PreloadAction>(preloadDefault);
    }
  }

  if (nextAction == HTMLMediaElement::PRELOAD_NONE && mIsDoingExplicitLoad) {
    nextAction = HTMLMediaElement::PRELOAD_METADATA;
  }

  mPreloadAction = nextAction;

  if (nextAction == HTMLMediaElement::PRELOAD_ENOUGH) {
    if (mSuspendedForPreloadNone) {
      // Our load was previouly suspended due to the media having preload
      // value "none". The preload value has changed to preload:auto, so
      // resume the load.
      ResumeLoad(PRELOAD_ENOUGH);
    } else {
      // Preload as much of the video as we can, i.e. don't suspend after
      // the first frame.
      StopSuspendingAfterFirstFrame();
    }

  } else if (nextAction == HTMLMediaElement::PRELOAD_METADATA) {
    // Ensure that the video can be suspended after first frame.
    mAllowSuspendAfterFirstFrame = true;
    if (mSuspendedForPreloadNone) {
      // Our load was previouly suspended due to the media having preload
      // value "none". The preload value has changed to preload:metadata, so
      // resume the load. We'll pause the load again after we've read the
      // metadata.
      ResumeLoad(PRELOAD_METADATA);
    }
  }
}

nsresult HTMLMediaElement::LoadResource()
{
  NS_ASSERTION(mDelayingLoadEvent,
               "Should delay load event (if in document) during load");

  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nullptr;
  }

  // Check if media is allowed for the docshell.
  nsCOMPtr<nsIDocShell> docShell = OwnerDoc()->GetDocShell();
  if (docShell && !docShell->GetAllowMedia()) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(IsAnyOfHTMLElements(nsGkAtoms::audio, nsGkAtoms::video));
  nsContentPolicyType contentPolicyType = IsHTMLElement(nsGkAtoms::audio) ?
    nsIContentPolicy::TYPE_INTERNAL_AUDIO : nsIContentPolicy::TYPE_INTERNAL_VIDEO;

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(contentPolicyType,
                                          mLoadingSrc,
                                          NodePrincipal(),
                                          static_cast<Element*>(this),
                                          EmptyCString(), // mime type
                                          nullptr, // extra
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy(),
                                          nsContentUtils::GetSecurityManager());
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_CP_REJECTED(shouldLoad)) {
    return NS_ERROR_FAILURE;
  }

  // Set the media element's CORS mode only when loading a resource
  mCORSMode = AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));

#ifdef MOZ_EME
  if (mMediaKeys &&
      !IsMediaStreamURI(mLoadingSrc) &&
      Preferences::GetBool("media.eme.mse-only", true)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }
#endif

  HTMLMediaElement* other = LookupMediaElementURITable(mLoadingSrc);
  if (other && other->mDecoder) {
    // Clone it.
    nsresult rv = InitializeDecoderAsClone(other->mDecoder);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  if (IsMediaStreamURI(mLoadingSrc)) {
    nsRefPtr<DOMMediaStream> stream;
    rv = NS_GetStreamForMediaStreamURI(mLoadingSrc, getter_AddRefs(stream));
    if (NS_FAILED(rv)) {
      nsAutoString spec;
      GetCurrentSrc(spec);
      const char16_t* params[] = { spec.get() };
      ReportLoadError("MediaLoadInvalidURI", params, ArrayLength(params));
      return rv;
    }
    SetupSrcMediaStreamPlayback(stream);
    return NS_OK;
  }

  if (mMediaSource) {
    nsRefPtr<MediaSourceDecoder> decoder = new MediaSourceDecoder(this);
    if (!mMediaSource->Attach(decoder)) {
      // TODO: Handle failure: run "If the media data cannot be fetched at
      // all, due to network errors, causing the user agent to give up
      // trying to fetch the resource" section of resource fetch algorithm.
      return NS_ERROR_FAILURE;
    }
    nsRefPtr<MediaResource> resource =
      MediaSourceDecoder::CreateResource(mMediaSource->GetPrincipal());
    if (IsAutoplayEnabled()) {
      mJoinLatency.Start();
    }
    return FinishDecoderSetup(decoder, resource, nullptr, nullptr);
  }

  nsSecurityFlags securityFlags = nsILoadInfo::SEC_NORMAL;
  if (nsContentUtils::ChannelShouldInheritPrincipal(NodePrincipal(),
                                                    mLoadingSrc,
                                                    false, // aInheritForAboutBlank
                                                    false // aForceInherit
                                                    )) {
    securityFlags = nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     mLoadingSrc,
                     static_cast<Element*>(this),
                     securityFlags,
                     contentPolicyType,
                     loadGroup,
                     nullptr,   // aCallbacks
                     nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY |
                     nsIChannel::LOAD_MEDIA_SNIFFER_OVERRIDES_CONTENT_TYPE |
                     nsIChannel::LOAD_CALL_CONTENT_SNIFFERS);

  NS_ENSURE_SUCCESS(rv,rv);

  // The listener holds a strong reference to us.  This creates a
  // reference cycle, once we've set mChannel, which is manually broken
  // in the listener's OnStartRequest method after it is finished with
  // the element. The cycle will also be broken if we get a shutdown
  // notification before OnStartRequest fires.  Necko guarantees that
  // OnStartRequest will eventually fire if we don't shut down first.
  nsRefPtr<MediaLoadListener> loadListener = new MediaLoadListener(this);

  channel->SetNotificationCallbacks(loadListener);

  nsCOMPtr<nsIStreamListener> listener;
  if (ShouldCheckAllowOrigin()) {
    nsRefPtr<nsCORSListenerProxy> corsListener =
      new nsCORSListenerProxy(loadListener,
                              NodePrincipal(),
                              GetCORSMode() == CORS_USE_CREDENTIALS);
    rv = corsListener->Init(channel, DataURIHandling::Allow);
    NS_ENSURE_SUCCESS(rv, rv);
    listener = corsListener;
  } else {
    rv = nsContentUtils::GetSecurityManager()->
           CheckLoadURIWithPrincipal(NodePrincipal(),
                                     mLoadingSrc,
                                     nsIScriptSecurityManager::STANDARD);
    listener = loadListener;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(channel);
  if (hc) {
    // Use a byte range request from the start of the resource.
    // This enables us to detect if the stream supports byte range
    // requests, and therefore seeking, early.
    hc->SetRequestHeader(NS_LITERAL_CSTRING("Range"),
                         NS_LITERAL_CSTRING("bytes=0-"),
                         false);

    SetRequestHeaders(hc);
  }

  rv = channel->AsyncOpen(listener, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  // Else the channel must be open and starting to download. If it encounters
  // a non-catastrophic failure, it will set a new task to continue loading
  // another candidate.  It's safe to set it as mChannel now.
  mChannel = channel;

  // loadListener will be unregistered either on shutdown or when
  // OnStartRequest for the channel we just opened fires.
  nsContentUtils::RegisterShutdownObserver(loadListener);
  return NS_OK;
}

nsresult HTMLMediaElement::LoadWithChannel(nsIChannel* aChannel,
                                           nsIStreamListener** aListener)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aListener);

  *aListener = nullptr;

  // Make sure we don't reenter during synchronous abort events.
  if (mIsRunningLoadMethod)
    return NS_OK;
  mIsRunningLoadMethod = true;
  AbortExistingLoads();
  mIsRunningLoadMethod = false;

  nsresult rv = aChannel->GetOriginalURI(getter_AddRefs(mLoadingSrc));
  NS_ENSURE_SUCCESS(rv, rv);

  ChangeDelayLoadStatus(true);
  rv = InitializeDecoderForChannel(aChannel, aListener);
  if (NS_FAILED(rv)) {
    ChangeDelayLoadStatus(false);
    return rv;
  }

  SetPlaybackRate(mDefaultPlaybackRate);
  DispatchAsyncEvent(NS_LITERAL_STRING("loadstart"));

  return NS_OK;
}

/* readonly attribute unsigned short readyState; */
NS_IMETHODIMP HTMLMediaElement::GetReadyState(uint16_t* aReadyState)
{
  *aReadyState = ReadyState();

  return NS_OK;
}

/* readonly attribute boolean seeking; */
bool
HTMLMediaElement::Seeking() const
{
  return mDecoder && mDecoder->IsSeeking();
}

NS_IMETHODIMP HTMLMediaElement::GetSeeking(bool* aSeeking)
{
  *aSeeking = Seeking();
  return NS_OK;
}

/* attribute double currentTime; */
double
HTMLMediaElement::CurrentTime() const
{
  if (mSrcStream) {
    MediaStream* stream = GetSrcMediaStream();
    if (stream) {
      return stream->StreamTimeToSeconds(stream->GetCurrentTime());
    }
  }

  if (mDecoder) {
    return mDecoder->GetCurrentTime();
  }

  return 0.0;
}

NS_IMETHODIMP HTMLMediaElement::GetCurrentTime(double* aCurrentTime)
{
  *aCurrentTime = CurrentTime();
  return NS_OK;
}

void
HTMLMediaElement::FastSeek(double aTime, ErrorResult& aRv)
{
  Seek(aTime, SeekTarget::PrevSyncPoint, aRv);
}

void
HTMLMediaElement::SetCurrentTime(double aCurrentTime, ErrorResult& aRv)
{
  Seek(aCurrentTime, SeekTarget::Accurate, aRv);
}

/**
 * Check if aValue is inside a range of aRanges, and if so sets aIsInRanges
 * to true and put the range index in aIntervalIndex. If aValue is not
 * inside a range, aIsInRanges is set to false, and aIntervalIndex
 * is set to the index of the range which ends immediately before aValue
 * (and can be -1 if aValue is before aRanges.Start(0)). Returns NS_OK
 * on success, and NS_ERROR_FAILURE on failure.
 */
static nsresult
IsInRanges(dom::TimeRanges& aRanges,
           double aValue,
           bool& aIsInRanges,
           int32_t& aIntervalIndex)
{
  aIsInRanges = false;
  uint32_t length;
  nsresult rv = aRanges.GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  for (uint32_t i = 0; i < length; i++) {
    double start, end;
    rv = aRanges.Start(i, &start);
    NS_ENSURE_SUCCESS(rv, rv);
    if (start > aValue) {
      aIntervalIndex = i - 1;
      return NS_OK;
    }
    rv = aRanges.End(i, &end);
    NS_ENSURE_SUCCESS(rv, rv);
    if (aValue <= end) {
      aIntervalIndex = i;
      aIsInRanges = true;
      return NS_OK;
    }
  }
  aIntervalIndex = length - 1;
  return NS_OK;
}

void
HTMLMediaElement::Seek(double aTime,
                       SeekTarget::Type aSeekType,
                       ErrorResult& aRv)
{
  // aTime should be non-NaN.
  MOZ_ASSERT(!mozilla::IsNaN(aTime));

  // Detect if user has interacted with element by seeking so that
  // play will not be blocked when initiated by a script.
  if (EventStateManager::IsHandlingUserInput() || nsContentUtils::IsCallerChrome()) {
    mHasUserInteraction = true;
  }

  StopSuspendingAfterFirstFrame();

  if (mSrcStream) {
    // do nothing since streams aren't seekable; we effectively clamp to
    // the current time.
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (!mPlayed) {
    LOG(LogLevel::Debug, ("HTMLMediaElement::mPlayed not available."));
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mCurrentPlayRangeStart != -1.0) {
    double rangeEndTime = CurrentTime();
    LOG(LogLevel::Debug, ("%p Adding \'played\' a range : [%f, %f]", this, mCurrentPlayRangeStart, rangeEndTime));
    // Multiple seek without playing, or seek while playing.
    if (mCurrentPlayRangeStart != rangeEndTime) {
      mPlayed->Add(mCurrentPlayRangeStart, rangeEndTime);
    }
    // Reset the current played range start time. We'll re-set it once
    // the seek completes.
    mCurrentPlayRangeStart = -1.0;
  }

  if (!mDecoder) {
    LOG(LogLevel::Debug, ("%p SetCurrentTime(%f) failed: no decoder", this, aTime));
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    LOG(LogLevel::Debug, ("%p SetCurrentTime(%f) failed: no source", this, aTime));
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Clamp the seek target to inside the seekable ranges.
  nsRefPtr<dom::TimeRanges> seekable = new dom::TimeRanges();
  media::TimeIntervals seekableIntervals = mDecoder->GetSeekable();
  if (seekableIntervals.IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  seekableIntervals.ToTimeRanges(seekable);
  uint32_t length = 0;
  seekable->GetLength(&length);
  if (!length) {
    return;
  }

  // If the position we want to seek to is not in a seekable range, we seek
  // to the closest position in the seekable ranges instead. If two positions
  // are equally close, we seek to the closest position from the currentTime.
  // See seeking spec, point 7 :
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#seeking
  int32_t range = 0;
  bool isInRange = false;
  if (NS_FAILED(IsInRanges(*seekable, aTime, isInRange, range))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (!isInRange) {
    if (range != -1) {
      // |range + 1| can't be negative, because the only possible negative value
      // for |range| is -1.
      if (uint32_t(range + 1) < length) {
        double leftBound, rightBound;
        if (NS_FAILED(seekable->End(range, &leftBound))) {
          aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
          return;
        }
        if (NS_FAILED(seekable->Start(range + 1, &rightBound))) {
          aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
          return;
        }
        double distanceLeft = Abs(leftBound - aTime);
        double distanceRight = Abs(rightBound - aTime);
        if (distanceLeft == distanceRight) {
          double currentTime = CurrentTime();
          distanceLeft = Abs(leftBound - currentTime);
          distanceRight = Abs(rightBound - currentTime);
        }
        aTime = (distanceLeft < distanceRight) ? leftBound : rightBound;
      } else {
        // Seek target is after the end last range in seekable data.
        // Clamp the seek target to the end of the last seekable range.
        if (NS_FAILED(seekable->End(length - 1, &aTime))) {
          aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
          return;
        }
      }
    } else {
      // aTime is before the first range in |seekable|, the closest point we can
      // seek to is the start of the first range.
      seekable->Start(0, &aTime);
    }
  }

  // TODO: The spec requires us to update the current time to reflect the
  //       actual seek target before beginning the synchronous section, but
  //       that requires changing all MediaDecoderReaders to support telling
  //       us the fastSeek target, and it's currently not possible to get
  //       this information as we don't yet control the demuxer for all
  //       MediaDecoderReaders.

  mPlayingBeforeSeek = IsPotentiallyPlaying();
  // The media backend is responsible for dispatching the timeupdate
  // event if it changes the playback position as a result of the seek.
  LOG(LogLevel::Debug, ("%p SetCurrentTime(%f) starting seek", this, aTime));
  nsresult rv = mDecoder->Seek(aTime, aSeekType);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  // We changed whether we're seeking so we need to AddRemoveSelfReference.
  AddRemoveSelfReference();
}

NS_IMETHODIMP HTMLMediaElement::SetCurrentTime(double aCurrentTime)
{
  // Detect for a NaN and invalid values.
  if (mozilla::IsNaN(aCurrentTime)) {
    LOG(LogLevel::Debug, ("%p SetCurrentTime(%f) failed: bad time", this, aCurrentTime));
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  SetCurrentTime(aCurrentTime, rv);
  return rv.StealNSResult();
}

/* readonly attribute double duration; */
double
HTMLMediaElement::Duration() const
{
  if (mSrcStream) {
    return std::numeric_limits<double>::infinity();
  }

  if (mDecoder) {
    return mDecoder->GetDuration();
  }

  return std::numeric_limits<double>::quiet_NaN();
}

NS_IMETHODIMP HTMLMediaElement::GetDuration(double* aDuration)
{
  *aDuration = Duration();
  return NS_OK;
}

already_AddRefed<TimeRanges>
HTMLMediaElement::Seekable() const
{
  nsRefPtr<TimeRanges> ranges = new TimeRanges();
  if (mDecoder && mReadyState > nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    mDecoder->GetSeekable().ToTimeRanges(ranges);
  }
  return ranges.forget();
}

/* readonly attribute nsIDOMHTMLTimeRanges seekable; */
NS_IMETHODIMP HTMLMediaElement::GetSeekable(nsIDOMTimeRanges** aSeekable)
{
  nsRefPtr<TimeRanges> ranges = Seekable();
  ranges.forget(aSeekable);
  return NS_OK;
}

/* readonly attribute boolean paused; */
NS_IMETHODIMP HTMLMediaElement::GetPaused(bool* aPaused)
{
  *aPaused = Paused();

  return NS_OK;
}

already_AddRefed<TimeRanges>
HTMLMediaElement::Played()
{
  nsRefPtr<TimeRanges> ranges = new TimeRanges();

  uint32_t timeRangeCount = 0;
  if (mPlayed) {
    mPlayed->GetLength(&timeRangeCount);
  }
  for (uint32_t i = 0; i < timeRangeCount; i++) {
    double begin;
    double end;
    mPlayed->Start(i, &begin);
    mPlayed->End(i, &end);
    ranges->Add(begin, end);
  }

  if (mCurrentPlayRangeStart != -1.0) {
    double now = CurrentTime();
    if (mCurrentPlayRangeStart != now) {
      ranges->Add(mCurrentPlayRangeStart, now);
    }
  }

  ranges->Normalize();
  return ranges.forget();
}

/* readonly attribute nsIDOMHTMLTimeRanges played; */
NS_IMETHODIMP HTMLMediaElement::GetPlayed(nsIDOMTimeRanges** aPlayed)
{
  nsRefPtr<TimeRanges> ranges = Played();
  ranges.forget(aPlayed);
  return NS_OK;
}

/* void pause (); */
void
HTMLMediaElement::Pause(ErrorResult& aRv)
{
  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    LOG(LogLevel::Debug, ("Loading due to Pause()"));
    DoLoad();
  } else if (mDecoder) {
    mDecoder->Pause();
  }

  bool oldPaused = mPaused;
  mPaused = true;
  mAutoplaying = false;
  // We changed mPaused and mAutoplaying which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  if (!oldPaused) {
    if (mSrcStream) {
      MediaStream* stream = GetSrcMediaStream();
      if (stream) {
        stream->ChangeExplicitBlockerCount(1);
      }
    }
    FireTimeUpdate(false);
    DispatchAsyncEvent(NS_LITERAL_STRING("pause"));
  }
}

NS_IMETHODIMP HTMLMediaElement::Pause()
{
  ErrorResult rv;
  Pause(rv);
  return rv.StealNSResult();
}

/* attribute double volume; */
NS_IMETHODIMP HTMLMediaElement::GetVolume(double* aVolume)
{
  *aVolume = Volume();
  return NS_OK;
}

void
HTMLMediaElement::SetVolume(double aVolume, ErrorResult& aRv)
{
  if (aVolume < 0.0 || aVolume > 1.0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (aVolume == mVolume)
    return;

  mVolume = aVolume;

  // Here we want just to update the volume.
  SetVolumeInternal();

  DispatchAsyncEvent(NS_LITERAL_STRING("volumechange"));
}

NS_IMETHODIMP HTMLMediaElement::SetVolume(double aVolume)
{
  ErrorResult rv;
  SetVolume(aVolume, rv);
  return rv.StealNSResult();
}

// Helper struct with arguments for our hash iterator.
typedef struct MOZ_STACK_CLASS {
  JSContext* cx;
  JS::Handle<JSObject*> tags;
  bool error;
} MetadataIterCx;

PLDHashOperator
HTMLMediaElement::BuildObjectFromTags(nsCStringHashKey::KeyType aKey,
                                      nsCString aValue,
                                      void* aUserArg)
{
  MetadataIterCx* args = static_cast<MetadataIterCx*>(aUserArg);

  nsString wideValue = NS_ConvertUTF8toUTF16(aValue);
  JS::Rooted<JSString*> string(args->cx, JS_NewUCStringCopyZ(args->cx, wideValue.Data()));
  if (!string) {
    NS_WARNING("Failed to perform string copy");
    args->error = true;
    return PL_DHASH_STOP;
  }
  if (!JS_DefineProperty(args->cx, args->tags, aKey.Data(), string, JSPROP_ENUMERATE)) {
    NS_WARNING("Failed to set metadata property");
    args->error = true;
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

void
HTMLMediaElement::MozGetMetadata(JSContext* cx,
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aRv)
{
  if (mReadyState < nsIDOMHTMLMediaElement::HAVE_METADATA) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  JS::Rooted<JSObject*> tags(cx, JS_NewPlainObject(cx));
  if (!tags) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  if (mTags) {
    MetadataIterCx iter = {cx, tags, false};
    mTags->EnumerateRead(BuildObjectFromTags, static_cast<void*>(&iter));
    if (iter.error) {
      NS_WARNING("couldn't create metadata object!");
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }

  aRetval.set(tags);
}

NS_IMETHODIMP
HTMLMediaElement::MozGetMetadata(JSContext* cx, JS::MutableHandle<JS::Value> aValue)
{
  ErrorResult rv;
  JS::Rooted<JSObject*> obj(cx);
  MozGetMetadata(cx, &obj, rv);
  if (!rv.Failed()) {
    MOZ_ASSERT(obj);
    aValue.setObject(*obj);
  }

  return rv.StealNSResult();
}

/* attribute boolean muted; */
NS_IMETHODIMP HTMLMediaElement::GetMuted(bool* aMuted)
{
  *aMuted = Muted();
  return NS_OK;
}

void HTMLMediaElement::SetMutedInternal(uint32_t aMuted)
{
  uint32_t oldMuted = mMuted;
  mMuted = aMuted;

  if (!!aMuted == !!oldMuted) {
    return;
  }

  SetVolumeInternal();
}

void HTMLMediaElement::SetVolumeInternal()
{
  float effectiveVolume = mMuted ? 0.0f :
    mAudioChannelFaded ? float(mVolume) * FADED_VOLUME_RATIO : float(mVolume);

  if (mAudioChannelAgent) {
    float volume;
    nsresult rv = mAudioChannelAgent->GetWindowVolume(&volume);
    if (NS_SUCCEEDED(rv)) {
      effectiveVolume *= volume;
    }
  }

  if (mDecoder) {
    mDecoder->SetVolume(effectiveVolume);
  } else if (mSrcStream) {
    GetSrcMediaStream()->SetAudioOutputVolume(this, effectiveVolume);
  }
}

NS_IMETHODIMP HTMLMediaElement::SetMuted(bool aMuted)
{
  if (aMuted == Muted()) {
    return NS_OK;
  }

  if (aMuted) {
    SetMutedInternal(mMuted | MUTED_BY_CONTENT);
  } else {
    SetMutedInternal(mMuted & ~MUTED_BY_CONTENT);
  }

  DispatchAsyncEvent(NS_LITERAL_STRING("volumechange"));
  return NS_OK;
}

already_AddRefed<DOMMediaStream>
HTMLMediaElement::CaptureStreamInternal(bool aFinishWhenEnded,
                                        MediaStreamGraph* aGraph)
{
  nsIDOMWindow* window = OwnerDoc()->GetInnerWindow();
  if (!window) {
    return nullptr;
  }
#ifdef MOZ_EME
  if (ContainsRestrictedContent()) {
    return nullptr;
  }
#endif
  OutputMediaStream* out = mOutputStreams.AppendElement();
  out->mStream = DOMMediaStream::CreateTrackUnionStream(window, aGraph);
  nsRefPtr<nsIPrincipal> principal = GetCurrentPrincipal();
  out->mStream->CombineWithPrincipal(principal);
  out->mStream->SetCORSMode(mCORSMode);
  out->mFinishWhenEnded = aFinishWhenEnded;

  mAudioCaptured = true;
  // Block the output stream initially.
  // Decoders are responsible for removing the block while they are playing
  // back into the output stream.
  out->mStream->GetStream()->ChangeExplicitBlockerCount(1);
  if (mDecoder) {
    mDecoder->AddOutputStream(out->mStream->GetStream()->AsProcessedStream(),
                              aFinishWhenEnded);
    if (mReadyState >= HAVE_METADATA) {
      // Expose the tracks to JS directly.
      if (HasAudio()) {
        TrackID audioTrackId = mMediaInfo.mAudio.mTrackId;
        out->mStream->CreateDOMTrack(audioTrackId, MediaSegment::AUDIO);
      }
      if (HasVideo()) {
        TrackID videoTrackId = mMediaInfo.mVideo.mTrackId;
        out->mStream->CreateDOMTrack(videoTrackId, MediaSegment::VIDEO);
      }
    }
  }
  nsRefPtr<DOMMediaStream> result = out->mStream;
  return result.forget();
}

already_AddRefed<DOMMediaStream>
HTMLMediaElement::MozCaptureStream(ErrorResult& aRv,
                                   MediaStreamGraph* aGraph)
{
  nsRefPtr<DOMMediaStream> stream = CaptureStreamInternal(false, aGraph);
  if (!stream) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return stream.forget();
}

already_AddRefed<DOMMediaStream>
HTMLMediaElement::MozCaptureStreamUntilEnded(ErrorResult& aRv,
                                             MediaStreamGraph* aGraph)
{
  nsRefPtr<DOMMediaStream> stream = CaptureStreamInternal(true, aGraph);
  if (!stream) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return stream.forget();
}

NS_IMETHODIMP HTMLMediaElement::GetMozAudioCaptured(bool* aCaptured)
{
  *aCaptured = MozAudioCaptured();
  return NS_OK;
}

class MediaElementSetForURI : public nsURIHashKey {
public:
  explicit MediaElementSetForURI(const nsIURI* aKey) : nsURIHashKey(aKey) {}
  MediaElementSetForURI(const MediaElementSetForURI& toCopy)
    : nsURIHashKey(toCopy), mElements(toCopy.mElements) {}
  nsTArray<HTMLMediaElement*> mElements;
};

typedef nsTHashtable<MediaElementSetForURI> MediaElementURITable;
// Elements in this table must have non-null mDecoder and mLoadingSrc, and those
// can't change while the element is in the table. The table is keyed by
// the element's mLoadingSrc. Each entry has a list of all elements with the
// same mLoadingSrc.
static MediaElementURITable* gElementTable;

#ifdef DEBUG
// Returns the number of times aElement appears in the media element table
// for aURI. If this returns other than 0 or 1, there's a bug somewhere!
static unsigned
MediaElementTableCount(HTMLMediaElement* aElement, nsIURI* aURI)
{
  if (!gElementTable || !aElement || !aURI) {
    return 0;
  }
  MediaElementSetForURI* entry = gElementTable->GetEntry(aURI);
  if (!entry) {
    return 0;
  }
  uint32_t count = 0;
  for (uint32_t i = 0; i < entry->mElements.Length(); ++i) {
    HTMLMediaElement* elem = entry->mElements[i];
    if (elem == aElement) {
      count++;
    }
  }
  return count;
}
#endif

void
HTMLMediaElement::AddMediaElementToURITable()
{
  NS_ASSERTION(mDecoder && mDecoder->GetResource(), "Call this only with decoder Load called");
  NS_ASSERTION(MediaElementTableCount(this, mLoadingSrc) == 0,
    "Should not have entry for element in element table before addition");
  if (!gElementTable) {
    gElementTable = new MediaElementURITable();
  }
  MediaElementSetForURI* entry = gElementTable->PutEntry(mLoadingSrc);
  entry->mElements.AppendElement(this);
  NS_ASSERTION(MediaElementTableCount(this, mLoadingSrc) == 1,
    "Should have a single entry for element in element table after addition");
}

void
HTMLMediaElement::RemoveMediaElementFromURITable()
{
  if (!mDecoder || !mLoadingSrc || !gElementTable) {
    return;
  }
  MediaElementSetForURI* entry = gElementTable->GetEntry(mLoadingSrc);
  if (!entry) {
    return;
  }
  entry->mElements.RemoveElement(this);
  if (entry->mElements.IsEmpty()) {
    gElementTable->RemoveEntry(mLoadingSrc);
    if (gElementTable->Count() == 0) {
      delete gElementTable;
      gElementTable = nullptr;
    }
  }
  NS_ASSERTION(MediaElementTableCount(this, mLoadingSrc) == 0,
    "After remove, should no longer have an entry in element table");
}

HTMLMediaElement*
HTMLMediaElement::LookupMediaElementURITable(nsIURI* aURI)
{
  if (!gElementTable) {
    return nullptr;
  }
  MediaElementSetForURI* entry = gElementTable->GetEntry(aURI);
  if (!entry) {
    return nullptr;
  }
  for (uint32_t i = 0; i < entry->mElements.Length(); ++i) {
    HTMLMediaElement* elem = entry->mElements[i];
    bool equal;
    // Look for elements that have the same principal and CORS mode.
    // Ditto for anything else that could cause us to send different headers.
    if (NS_SUCCEEDED(elem->NodePrincipal()->Equals(NodePrincipal(), &equal)) && equal &&
        elem->mCORSMode == mCORSMode) {
      NS_ASSERTION(elem->mDecoder && elem->mDecoder->GetResource(), "Decoder gone");
      MediaResource* resource = elem->mDecoder->GetResource();
      if (resource->CanClone()) {
        return elem;
      }
    }
  }
  return nullptr;
}

HTMLMediaElement::HTMLMediaElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mWatchManager(this, AbstractThread::MainThread()),
    mCurrentLoadID(0),
    mNetworkState(nsIDOMHTMLMediaElement::NETWORK_EMPTY),
    mReadyState(nsIDOMHTMLMediaElement::HAVE_NOTHING, "HTMLMediaElement::mReadyState"),
    mLoadWaitStatus(NOT_WAITING),
    mVolume(1.0),
    mPreloadAction(PRELOAD_UNDEFINED),
    mLastCurrentTime(0.0),
    mFragmentStart(-1.0),
    mFragmentEnd(-1.0),
    mDefaultPlaybackRate(1.0),
    mPlaybackRate(1.0),
    mPreservesPitch(true),
    mPlayed(new TimeRanges),
    mCurrentPlayRangeStart(-1.0),
    mBegun(false),
    mLoadedDataFired(false),
    mAutoplaying(true),
    mAutoplayEnabled(true),
    mPaused(true),
    mMuted(0),
    mStatsShowing(false),
    mAllowCasting(false),
    mIsCasting(false),
    mAudioCaptured(false),
    mPlayingBeforeSeek(false),
    mPlayingThroughTheAudioChannelBeforeSeek(false),
    mPausedForInactiveDocumentOrChannel(false),
    mEventDeliveryPaused(false),
    mWaitingFired(false),
    mIsRunningLoadMethod(false),
    mIsDoingExplicitLoad(false),
    mIsLoadingFromSourceChildren(false),
    mDelayingLoadEvent(false),
    mIsRunningSelectResource(false),
    mHaveQueuedSelectResource(false),
    mSuspendedAfterFirstFrame(false),
    mAllowSuspendAfterFirstFrame(true),
    mHasPlayedOrSeeked(false),
    mHasSelfReference(false),
    mShuttingDown(false),
    mSuspendedForPreloadNone(false),
    mMediaSecurityVerified(false),
    mCORSMode(CORS_NONE),
    mIsEncrypted(false),
    mDownloadSuspendedByCache(false, "HTMLMediaElement::mDownloadSuspendedByCache"),
    mAudioChannelFaded(false),
    mPlayingThroughTheAudioChannel(false),
    mDisableVideo(false),
    mElementInTreeState(ELEMENT_NOT_INTREE),
    mHasUserInteraction(false)
{
  if (!gMediaElementLog) {
    gMediaElementLog = PR_NewLogModule("nsMediaElement");
  }
  if (!gMediaElementEventsLog) {
    gMediaElementEventsLog = PR_NewLogModule("nsMediaElementEvents");
  }

  mAudioChannel = AudioChannelService::GetDefaultAudioChannel();

  mPaused.SetOuter(this);

  RegisterActivityObserver();
  NotifyOwnerDocumentActivityChanged();

  MOZ_ASSERT(NS_IsMainThread());
  mWatchManager.Watch(mDownloadSuspendedByCache, &HTMLMediaElement::UpdateReadyStateInternal);
  // Paradoxically, there is a self-edge whereby UpdateReadyStateInternal refuses
  // to run until mReadyState reaches at least HAVE_METADATA by some other means.
  mWatchManager.Watch(mReadyState, &HTMLMediaElement::UpdateReadyStateInternal);
}

HTMLMediaElement::~HTMLMediaElement()
{
  NS_ASSERTION(!mHasSelfReference,
               "How can we be destroyed if we're still holding a self reference?");

  if (mVideoFrameContainer) {
    mVideoFrameContainer->ForgetElement();
  }
  UnregisterActivityObserver();
  if (mDecoder) {
    ShutdownDecoder();
  }
  if (mProgressTimer) {
    StopProgress();
  }
  if (mSrcStream) {
    EndSrcMediaStreamPlayback();
  }

  NS_ASSERTION(MediaElementTableCount(this, mLoadingSrc) == 0,
    "Destroyed media element should no longer be in element table");

  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }

  WakeLockRelease();
}

void
HTMLMediaElement::GetItemValueText(DOMString& aValue)
{
  // Can't call GetSrc because we don't have a JSContext
  GetURIAttr(nsGkAtoms::src, nullptr, aValue);
}

void
HTMLMediaElement::SetItemValueText(const nsAString& aValue)
{
  // Can't call SetSrc because we don't have a JSContext
  SetAttr(kNameSpaceID_None, nsGkAtoms::src, aValue, true);
}

void HTMLMediaElement::StopSuspendingAfterFirstFrame()
{
  mAllowSuspendAfterFirstFrame = false;
  if (!mSuspendedAfterFirstFrame)
    return;
  mSuspendedAfterFirstFrame = false;
  if (mDecoder) {
    mDecoder->Resume(true);
  }
}

void HTMLMediaElement::SetPlayedOrSeeked(bool aValue)
{
  if (aValue == mHasPlayedOrSeeked) {
    return;
  }

  mHasPlayedOrSeeked = aValue;

  // Force a reflow so that the poster frame hides or shows immediately.
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) {
    return;
  }
  frame->PresContext()->PresShell()->FrameNeedsReflow(frame,
                                                      nsIPresShell::eTreeChange,
                                                      NS_FRAME_IS_DIRTY);
}

void
HTMLMediaElement::ResetConnectionState()
{
  SetCurrentTime(0);
  FireTimeUpdate(false);
  DispatchAsyncEvent(NS_LITERAL_STRING("ended"));
  ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_EMPTY);
  ChangeDelayLoadStatus(false);
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_NOTHING);
}

void
HTMLMediaElement::Play(ErrorResult& aRv)
{
  // Prevent media element from being auto-started by a script when
  // media.autoplay.enabled=false
  if (!mHasUserInteraction
      && !IsAutoplayEnabled()
      && !EventStateManager::IsHandlingUserInput()
      && !nsContentUtils::IsCallerChrome()) {
    LOG(LogLevel::Debug, ("%p Blocked attempt to autoplay media.", this));
    return;
  }

  // Play was not blocked so assume user interacted with the element.
  mHasUserInteraction = true;

  StopSuspendingAfterFirstFrame();
  SetPlayedOrSeeked(true);

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    DoLoad();
  }
  if (mSuspendedForPreloadNone) {
    ResumeLoad(PRELOAD_ENOUGH);
  }
  // Even if we just did Load() or ResumeLoad(), we could already have a decoder
  // here if we managed to clone an existing decoder.
  if (mDecoder) {
    if (mDecoder->IsEndedOrShutdown()) {
      SetCurrentTime(0);
    }
    if (!mPausedForInactiveDocumentOrChannel) {
      aRv = mDecoder->Play();
      if (aRv.Failed()) {
        return;
      }
    }
  }

  if (mCurrentPlayRangeStart == -1.0) {
    mCurrentPlayRangeStart = CurrentTime();
  }

  // TODO: If the playback has ended, then the user agent must set
  // seek to the effective start.
  if (mPaused) {
    if (mSrcStream) {
      GetSrcMediaStream()->ChangeExplicitBlockerCount(-1);
    }
    DispatchAsyncEvent(NS_LITERAL_STRING("play"));
    switch (mReadyState) {
    case nsIDOMHTMLMediaElement::HAVE_NOTHING:
      DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
      break;
    case nsIDOMHTMLMediaElement::HAVE_METADATA:
    case nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA:
      FireTimeUpdate(false);
      DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
      break;
    case nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA:
    case nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA:
      DispatchAsyncEvent(NS_LITERAL_STRING("playing"));
      break;
    }
  }

  mPaused = false;
  mAutoplaying = false;
  // We changed mPaused and mAutoplaying which can affect AddRemoveSelfReference
  // and our preload status.
  AddRemoveSelfReference();
  UpdatePreloadAction();
}

NS_IMETHODIMP HTMLMediaElement::Play()
{
  ErrorResult rv;
  Play(rv);
  return rv.StealNSResult();
}

HTMLMediaElement::WakeLockBoolWrapper&
HTMLMediaElement::WakeLockBoolWrapper::operator=(bool val)
{
  if (mValue == val) {
    return *this;
  }

  mValue = val;
  UpdateWakeLock();
  return *this;
}

HTMLMediaElement::WakeLockBoolWrapper::~WakeLockBoolWrapper()
{
  if (mTimer) {
    mTimer->Cancel();
  }
}

void
HTMLMediaElement::WakeLockBoolWrapper::SetCanPlay(bool aCanPlay)
{
  mCanPlay = aCanPlay;
  UpdateWakeLock();
}

void
HTMLMediaElement::WakeLockBoolWrapper::UpdateWakeLock()
{
  if (!mOuter) {
    return;
  }

  bool playing = (!mValue && mCanPlay);

  if (playing) {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
    mOuter->WakeLockCreate();
  } else if (!mTimer) {
    // Don't release the wake lock immediately; instead, release it after a
    // grace period.
    int timeout = Preferences::GetInt("media.wakelock_timeout", 2000);
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mTimer) {
      mTimer->InitWithFuncCallback(TimerCallback, this, timeout,
                                   nsITimer::TYPE_ONE_SHOT);
    }
  }
}

void
HTMLMediaElement::WakeLockBoolWrapper::TimerCallback(nsITimer* aTimer,
                                                     void* aClosure)
{
  WakeLockBoolWrapper* wakeLock = static_cast<WakeLockBoolWrapper*>(aClosure);
  wakeLock->mOuter->WakeLockRelease();
  wakeLock->mTimer = nullptr;
}

void
HTMLMediaElement::WakeLockCreate()
{
  if (!mWakeLock) {
    nsRefPtr<power::PowerManagerService> pmService =
      power::PowerManagerService::GetInstance();
    NS_ENSURE_TRUE_VOID(pmService);

    ErrorResult rv;
    mWakeLock = pmService->NewWakeLock(NS_LITERAL_STRING("cpu"),
                                       OwnerDoc()->GetInnerWindow(),
                                       rv);
  }
}

void
HTMLMediaElement::WakeLockRelease()
{
  if (mWakeLock) {
    ErrorResult rv;
    mWakeLock->Unlock(rv);
    NS_WARN_IF_FALSE(!rv.Failed(), "Failed to unlock the wakelock.");
    mWakeLock = nullptr;
  }
}

bool HTMLMediaElement::ParseAttribute(int32_t aNamespaceID,
                                      nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  // Mappings from 'preload' attribute strings to an enumeration.
  static const nsAttrValue::EnumTable kPreloadTable[] = {
    { "",         HTMLMediaElement::PRELOAD_ATTR_EMPTY },
    { "none",     HTMLMediaElement::PRELOAD_ATTR_NONE },
    { "metadata", HTMLMediaElement::PRELOAD_ATTR_METADATA },
    { "auto",     HTMLMediaElement::PRELOAD_ATTR_AUTO },
    { 0 }
  };

  if (aNamespaceID == kNameSpaceID_None) {
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return true;
    }
    if (aAttribute == nsGkAtoms::crossorigin) {
      ParseCORSValue(aValue, aResult);
      return true;
    }
    if (aAttribute == nsGkAtoms::preload) {
      return aResult.ParseEnumValue(aValue, kPreloadTable, false);
    }

    if (aAttribute == nsGkAtoms::mozaudiochannel) {
      const nsAttrValue::EnumTable* table =
        AudioChannelService::GetAudioChannelTable();
      MOZ_ASSERT(table);

      bool parsed = aResult.ParseEnumValue(aValue, table, false, &table[0]);
      if (!parsed) {
        return false;
      }

      AudioChannel audioChannel = static_cast<AudioChannel>(aResult.GetEnumValue());

      if (audioChannel == mAudioChannel ||
          !CheckAudioChannelPermissions(aValue)) {
        return true;
      }

      // We cannot change the AudioChannel of a decoder.
      if (mDecoder) {
        return true;
      }

      mAudioChannel = audioChannel;

      if (mSrcStream) {
        nsRefPtr<MediaStream> stream = mSrcStream->GetStream();
        if (stream) {
          stream->SetAudioChannelType(mAudioChannel);
        }
      }

      return true;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

bool HTMLMediaElement::CheckAudioChannelPermissions(const nsAString& aString)
{
  if (!UseAudioChannelService()) {
    return true;
  }

  // Only normal channel doesn't need permission.
  if (aString.EqualsASCII("normal")) {
    return true;
  }

  // Maybe this audio channel is equal to the default value from the pref.
  nsString audioChannel;
  AudioChannelService::GetDefaultAudioChannelString(audioChannel);
  if (audioChannel.Equals(aString)) {
    return true;
  }

  nsCOMPtr<nsIPermissionManager> permissionManager =
    services::GetPermissionManager();
  if (!permissionManager) {
    return false;
  }

  uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;
  permissionManager->TestExactPermissionFromPrincipal(NodePrincipal(),
    nsCString(NS_LITERAL_CSTRING("audio-channel-") + NS_ConvertUTF16toUTF8(aString)).get(), &perm);
  if (perm != nsIPermissionManager::ALLOW_ACTION) {
    return false;
  }

  return true;
}

void HTMLMediaElement::DoneCreatingElement()
{
   if (HasAttr(kNameSpaceID_None, nsGkAtoms::muted)) {
     mMuted |= MUTED_BY_CONTENT;
   }
}

bool HTMLMediaElement::IsHTMLFocusable(bool aWithMouse,
                                       bool* aIsFocusable,
                                       int32_t* aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  *aIsFocusable = true;
  return false;
}

int32_t HTMLMediaElement::TabIndexDefault()
{
  return 0;
}

nsresult HTMLMediaElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                   nsIAtom* aPrefix, const nsAString& aValue,
                                   bool aNotify)
{
  nsresult rv =
    nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                  aNotify);
  if (NS_FAILED(rv))
    return rv;
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::src) {
    DoLoad();
  }
  if (aNotify && aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::autoplay) {
      StopSuspendingAfterFirstFrame();
      CheckAutoplayDataReady();
      // This attribute can affect AddRemoveSelfReference
      AddRemoveSelfReference();
      UpdatePreloadAction();
    } else if (aName == nsGkAtoms::preload) {
      UpdatePreloadAction();
    }
  }

  return rv;
}

nsresult HTMLMediaElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttr,
                                     bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttr, aNotify);
  if (NS_FAILED(rv))
    return rv;
  if (aNotify && aNameSpaceID == kNameSpaceID_None) {
    if (aAttr == nsGkAtoms::autoplay) {
      // This attribute can affect AddRemoveSelfReference
      AddRemoveSelfReference();
      UpdatePreloadAction();
    } else if (aAttr == nsGkAtoms::preload) {
      UpdatePreloadAction();
    }
  }

  return rv;
}

nsresult
HTMLMediaElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::src) {
    mSrcMediaSource = nullptr;
    if (aValue) {
      nsString srcStr = aValue->GetStringValue();
      nsCOMPtr<nsIURI> uri;
      NewURIFromString(srcStr, getter_AddRefs(uri));
      if (uri && IsMediaSourceURI(uri)) {
        nsresult rv =
          NS_GetSourceForMediaSourceURI(uri, getter_AddRefs(mSrcMediaSource));
        if (NS_FAILED(rv)) {
          nsAutoString spec;
          GetCurrentSrc(spec);
          const char16_t* params[] = { spec.get() };
          ReportLoadError("MediaLoadInvalidURI", params, ArrayLength(params));
        }
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName,
                                            aValue, aNotify);
}

nsresult HTMLMediaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                      nsIContent* aBindingParent,
                                      bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument,
                                                 aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  if (aDocument) {
    mAutoplayEnabled =
      IsAutoplayEnabled() && (!aDocument || !aDocument->IsStaticDocument()) &&
      !IsEditable();
    // The preload action depends on the value of the autoplay attribute.
    // It's value may have changed, so update it.
    UpdatePreloadAction();
  }
  mElementInTreeState = ELEMENT_INTREE;

  if (mDecoder) {
    // When the MediaElement is binding to tree, the dormant status is
    // aligned to document's hidden status.
    mDecoder->NotifyOwnerActivityChanged();
  }

  return rv;
}

#ifdef MOZ_EME
void
HTMLMediaElement::ReportEMETelemetry()
{
  // Report telemetry for EME videos when a page is unloaded.
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mIsEncrypted && Preferences::GetBool("media.eme.enabled")) {
    Telemetry::Accumulate(Telemetry::VIDEO_EME_PLAY_SUCCESS, mLoadedDataFired);
    LOG(LogLevel::Debug, ("%p VIDEO_EME_PLAY_SUCCESS = %s",
                       this, mLoadedDataFired ? "true" : "false"));
  }
}
#endif

void
HTMLMediaElement::ReportMSETelemetry()
{
  // Report telemetry for videos when a page is unloaded. We
  // want to know data on what state the video is at when
  // the user has exited.
  enum UnloadedState {
    ENDED = 0,
    PAUSED = 1,
    STALLED = 2,
    SEEKING = 3,
    OTHER = 4
  };

  UnloadedState state = OTHER;
  if (Seeking()) {
    state = SEEKING;
  }
  else if (Ended()) {
    state = ENDED;
  }
  else if (Paused()) {
    state = PAUSED;
  }
  else {
    // For buffering we check if the current playback position is at the end
    // of a buffered range, within a margin of error. We also consider to be
    // buffering if the last frame status was buffering and the ready state is
    // HAVE_CURRENT_DATA to account for times where we are in a buffering state
    // regardless of what actual data we have buffered.
    bool stalled = false;
    nsRefPtr<TimeRanges> ranges = Buffered();
    const double errorMargin = 0.05;
    double t = CurrentTime();
    TimeRanges::index_type index = ranges->Find(t, errorMargin);
    ErrorResult ignore;
    stalled = index != TimeRanges::NoIndex &&
              (ranges->End(index, ignore) - t) < errorMargin;
    stalled |= mDecoder && NextFrameStatus() == MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING &&
               mReadyState == HTMLMediaElement::HAVE_CURRENT_DATA;
    if (stalled) {
      state = STALLED;
    }
  }

  Telemetry::Accumulate(Telemetry::VIDEO_MSE_UNLOAD_STATE, state);
  LOG(LogLevel::Debug, ("%p VIDEO_MSE_UNLOAD_STATE = %d", this, state));

  Telemetry::Accumulate(Telemetry::VIDEO_MSE_PLAY_TIME_MS, SECONDS_TO_MS(mPlayTime.Total()));
  LOG(LogLevel::Debug, ("%p VIDEO_MSE_PLAY_TIME_MS = %f", this, mPlayTime.Total()));

  double latency = mJoinLatency.Count() ? mJoinLatency.Total() / mJoinLatency.Count() : 0.0;
  Telemetry::Accumulate(Telemetry::VIDEO_MSE_JOIN_LATENCY_MS, SECONDS_TO_MS(latency));
  LOG(LogLevel::Debug, ("%p VIDEO_MSE_JOIN_LATENCY = %f (%d ms) count=%d\n",
                     this, latency, SECONDS_TO_MS(latency), mJoinLatency.Count()));
}

void HTMLMediaElement::UnbindFromTree(bool aDeep,
                                      bool aNullParent)
{
  if (!mPaused && mNetworkState != nsIDOMHTMLMediaElement::NETWORK_EMPTY) {
    Pause();
  }

  mElementInTreeState = ELEMENT_NOT_INTREE_HAD_INTREE;

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  if (mDecoder) {
    MOZ_ASSERT(IsHidden());
    mDecoder->NotifyOwnerActivityChanged();
  }
}

/* static */
CanPlayStatus
HTMLMediaElement::GetCanPlay(const nsAString& aType)
{
  nsContentTypeParser parser(aType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv))
    return CANPLAY_NO;

  nsAutoString codecs;
  rv = parser.GetParameter("codecs", codecs);

  NS_ConvertUTF16toUTF8 mimeTypeUTF8(mimeType);
  return DecoderTraits::CanHandleMediaType(mimeTypeUTF8.get(),
                                           NS_SUCCEEDED(rv),
                                           codecs);
}

NS_IMETHODIMP
HTMLMediaElement::CanPlayType(const nsAString& aType, nsAString& aResult)
{
  switch (GetCanPlay(aType)) {
  case CANPLAY_NO:
    aResult.Truncate();
    break;
  case CANPLAY_YES:
    aResult.AssignLiteral("probably");
    break;
  default:
  case CANPLAY_MAYBE:
    aResult.AssignLiteral("maybe");
    break;
  }

  LOG(LogLevel::Debug, ("%p CanPlayType(%s) = \"%s\"", this,
                     NS_ConvertUTF16toUTF8(aType).get(),
                     NS_ConvertUTF16toUTF8(aResult).get()));

  return NS_OK;
}

nsresult HTMLMediaElement::InitializeDecoderAsClone(MediaDecoder* aOriginal)
{
  NS_ASSERTION(mLoadingSrc, "mLoadingSrc must already be set");
  NS_ASSERTION(mDecoder == nullptr, "Shouldn't have a decoder");

  MediaResource* originalResource = aOriginal->GetResource();
  if (!originalResource)
    return NS_ERROR_FAILURE;
  nsRefPtr<MediaDecoder> decoder = aOriginal->Clone();
  if (!decoder)
    return NS_ERROR_FAILURE;

  LOG(LogLevel::Debug, ("%p Cloned decoder %p from %p", this, decoder.get(), aOriginal));

  if (!decoder->Init(this)) {
    LOG(LogLevel::Debug, ("%p Failed to init cloned decoder %p", this, decoder.get()));
    return NS_ERROR_FAILURE;
  }

  decoder->SetMediaSeekable(aOriginal->IsMediaSeekable());

  nsRefPtr<MediaResource> resource = originalResource->CloneData(decoder);
  if (!resource) {
    LOG(LogLevel::Debug, ("%p Failed to cloned stream for decoder %p", this, decoder.get()));
    return NS_ERROR_FAILURE;
  }

  return FinishDecoderSetup(decoder, resource, nullptr, aOriginal);
}

nsresult HTMLMediaElement::InitializeDecoderForChannel(nsIChannel* aChannel,
                                                       nsIStreamListener** aListener)
{
  NS_ASSERTION(mLoadingSrc, "mLoadingSrc must already be set");
  NS_ASSERTION(mDecoder == nullptr, "Shouldn't have a decoder");

  nsAutoCString mimeType;

  aChannel->GetContentType(mimeType);
  NS_ASSERTION(!mimeType.IsEmpty(), "We should have the Content-Type.");

  nsRefPtr<MediaDecoder> decoder = DecoderTraits::CreateDecoder(mimeType, this);
  if (!decoder) {
    nsAutoString src;
    GetCurrentSrc(src);
    NS_ConvertUTF8toUTF16 mimeUTF16(mimeType);
    const char16_t* params[] = { mimeUTF16.get(), src.get() };
    ReportLoadError("MediaLoadUnsupportedMimeType", params, ArrayLength(params));
    return NS_ERROR_FAILURE;
  }

  LOG(LogLevel::Debug, ("%p Created decoder %p for type %s", this, decoder.get(), mimeType.get()));

  nsRefPtr<MediaResource> resource = MediaResource::Create(decoder, aChannel);
  if (!resource)
    return NS_ERROR_OUT_OF_MEMORY;

  // stream successfully created, the stream now owns the channel.
  mChannel = nullptr;

  // We postpone the |FinishDecoderSetup| function call until we get
  // |OnConnected| signal from MediaStreamController which is held by
  // RtspMediaResource.
  if (DecoderTraits::DecoderWaitsForOnConnected(mimeType)) {
    decoder->SetResource(resource);
    SetDecoder(decoder);
    if (aListener) {
      *aListener = nullptr;
    }
    return NS_OK;
  } else {
    return FinishDecoderSetup(decoder, resource, aListener, nullptr);
  }
}

nsresult HTMLMediaElement::FinishDecoderSetup(MediaDecoder* aDecoder,
                                              MediaResource* aStream,
                                              nsIStreamListener** aListener,
                                              MediaDecoder* aCloneDonor)
{
  ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_LOADING);

  // Force a same-origin check before allowing events for this media resource.
  mMediaSecurityVerified = false;

  // The new stream has not been suspended by us.
  mPausedForInactiveDocumentOrChannel = false;
  mEventDeliveryPaused = false;
  mPendingEvents.Clear();
  // Set mDecoder now so if methods like GetCurrentSrc get called between
  // here and Load(), they work.
  SetDecoder(aDecoder);

  // Tell the decoder about its MediaResource now so things like principals are
  // available immediately.
  mDecoder->SetResource(aStream);
  mDecoder->SetAudioChannel(mAudioChannel);
  mDecoder->SetVolume(mMuted ? 0.0 : mVolume);
  mDecoder->SetPreservesPitch(mPreservesPitch);
  mDecoder->SetPlaybackRate(mPlaybackRate);
  if (mPreloadAction == HTMLMediaElement::PRELOAD_METADATA) {
    mDecoder->SetMinimizePrerollUntilPlaybackStarts();
  }

  // Update decoder principal before we start decoding, since it
  // can affect how we feed data to MediaStreams
  NotifyDecoderPrincipalChanged();

  nsresult rv = aDecoder->Load(aListener, aCloneDonor);
  if (NS_FAILED(rv)) {
    ShutdownDecoder();
    LOG(LogLevel::Debug, ("%p Failed to load for decoder %p", this, aDecoder));
    return rv;
  }

  for (uint32_t i = 0; i < mOutputStreams.Length(); ++i) {
    OutputMediaStream* ms = &mOutputStreams[i];
    aDecoder->AddOutputStream(ms->mStream->GetStream()->AsProcessedStream(),
                              ms->mFinishWhenEnded);
  }

#ifdef MOZ_EME
  if (mMediaKeys) {
    mDecoder->SetCDMProxy(mMediaKeys->GetCDMProxy());
  }
#endif

  // Decoder successfully created, the decoder now owns the MediaResource
  // which owns the channel.
  mChannel = nullptr;

  AddMediaElementToURITable();

  // We may want to suspend the new stream now.
  // This will also do an AddRemoveSelfReference.
  NotifyOwnerDocumentActivityChanged();

  if (!mPaused) {
    SetPlayedOrSeeked(true);
    if (!mPausedForInactiveDocumentOrChannel) {
      rv = mDecoder->Play();
    }
  }

  if (NS_FAILED(rv)) {
    ShutdownDecoder();
  }

  NS_ASSERTION(NS_SUCCEEDED(rv) == (MediaElementTableCount(this, mLoadingSrc) == 1),
    "Media element should have single table entry if decode initialized");

  return rv;
}

class HTMLMediaElement::StreamListener : public MediaStreamListener,
                                         public WatchTarget
{
public:
  explicit StreamListener(HTMLMediaElement* aElement, const char* aName) :
    WatchTarget(aName),
    mElement(aElement),
    mHaveCurrentData(false),
    mBlocked(false),
    mMutex(aName),
    mPendingNotifyOutput(false)
  {}
  void Forget() { mElement = nullptr; }

  // Main thread
  void DoNotifyFinished()
  {
    if (mElement) {
      nsRefPtr<HTMLMediaElement> deathGrip = mElement;
      mElement->PlaybackEnded();
    }
  }

  MediaDecoderOwner::NextFrameStatus NextFrameStatus()
  {
    if (!mElement || !mHaveCurrentData) {
      return MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
    }
    return mBlocked ? MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING
                    : MediaDecoderOwner::NEXT_FRAME_AVAILABLE;
  }

  void DoNotifyBlocked()
  {
    mBlocked = true;
    NotifyWatchers();
  }
  void DoNotifyUnblocked()
  {
    mBlocked = false;
    NotifyWatchers();
  }
  void DoNotifyOutput()
  {
    {
      MutexAutoLock lock(mMutex);
      mPendingNotifyOutput = false;
    }
    if (mElement && mHaveCurrentData) {
      nsRefPtr<HTMLMediaElement> deathGrip = mElement;
      mElement->FireTimeUpdate(true);
    }
  }
  void DoNotifyHaveCurrentData()
  {
    mHaveCurrentData = true;
    if (mElement) {
      nsRefPtr<HTMLMediaElement> deathGrip = mElement;
      mElement->FirstFrameLoaded();
    }
    NotifyWatchers();
    DoNotifyOutput();
  }

  // These notifications run on the media graph thread so we need to
  // dispatch events to the main thread.
  virtual void NotifyBlockingChanged(MediaStreamGraph* aGraph, Blocking aBlocked) override
  {
    nsCOMPtr<nsIRunnable> event;
    if (aBlocked == BLOCKED) {
      event = NS_NewRunnableMethod(this, &StreamListener::DoNotifyBlocked);
    } else {
      event = NS_NewRunnableMethod(this, &StreamListener::DoNotifyUnblocked);
    }
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
  }
  virtual void NotifyEvent(MediaStreamGraph* aGraph,
                           MediaStreamListener::MediaStreamGraphEvent event) override
  {
    if (event == EVENT_FINISHED) {
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(this, &StreamListener::DoNotifyFinished);
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
    }
  }
  virtual void NotifyHasCurrentData(MediaStreamGraph* aGraph) override
  {
    MutexAutoLock lock(mMutex);
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &StreamListener::DoNotifyHaveCurrentData);
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
  }
  virtual void NotifyOutput(MediaStreamGraph* aGraph,
                            GraphTime aCurrentTime) override
  {
    MutexAutoLock lock(mMutex);
    if (mPendingNotifyOutput)
      return;
    mPendingNotifyOutput = true;
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &StreamListener::DoNotifyOutput);
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
  }

private:
  // These fields may only be accessed on the main thread
  HTMLMediaElement* mElement;
  bool mHaveCurrentData;
  bool mBlocked;

  // mMutex protects the fields below; they can be accessed on any thread
  Mutex mMutex;
  bool mPendingNotifyOutput;
};

/**
 * This listener observes the first video frame to arrive with a non-empty size,
 * and calls HTMLMediaElement::ReceivedMediaStreamInitialSize() with that size.
 */
class HTMLMediaElement::StreamSizeListener : public MediaStreamListener {
public:
  explicit StreamSizeListener(HTMLMediaElement* aElement) :
    mElement(aElement),
    mMutex("HTMLMediaElement::StreamSizeListener")
  {}
  void Forget() { mElement = nullptr; }

  void ReceivedSize()
  {
    if (!mElement) {
      return;
    }
    gfxIntSize size;
    {
      MutexAutoLock lock(mMutex);
      size = mInitialSize;
    }
    nsRefPtr<HTMLMediaElement> deathGrip = mElement;
    mElement->UpdateInitialMediaSize(size);
  }
  virtual void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                        StreamTime aTrackOffset,
                                        uint32_t aTrackEvents,
                                        const MediaSegment& aQueuedMedia) override
  {
    MutexAutoLock lock(mMutex);
    if (mInitialSize != gfxIntSize(0,0) ||
        aQueuedMedia.GetType() != MediaSegment::VIDEO) {
      return;
    }
    const VideoSegment& video = static_cast<const VideoSegment&>(aQueuedMedia);
    for (VideoSegment::ConstChunkIterator c(video); !c.IsEnded(); c.Next()) {
      if (c->mFrame.GetIntrinsicSize() != gfxIntSize(0,0)) {
        mInitialSize = c->mFrame.GetIntrinsicSize();
        nsCOMPtr<nsIRunnable> event =
          NS_NewRunnableMethod(this, &StreamSizeListener::ReceivedSize);
        aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
      }
    }
  }

private:
  // These fields may only be accessed on the main thread
  HTMLMediaElement* mElement;

  // mMutex protects the fields below; they can be accessed on any thread
  Mutex mMutex;
  gfxIntSize mInitialSize;
};

class HTMLMediaElement::MediaStreamTracksAvailableCallback:
    public DOMMediaStream::OnTracksAvailableCallback
{
public:
  explicit MediaStreamTracksAvailableCallback(HTMLMediaElement* aElement):
      DOMMediaStream::OnTracksAvailableCallback(),
      mElement(aElement)
    {}
  virtual void NotifyTracksAvailable(DOMMediaStream* aStream)
  {
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

    mElement->NotifyMediaStreamTracksAvailable(aStream);
  }
private:
  HTMLMediaElement* mElement;
};

void HTMLMediaElement::SetupSrcMediaStreamPlayback(DOMMediaStream* aStream)
{
  NS_ASSERTION(!mSrcStream && !mMediaStreamListener && !mMediaStreamSizeListener,
               "Should have been ended already");

  mSrcStream = aStream;

  nsIDOMWindow* window = OwnerDoc()->GetInnerWindow();
  if (!window) {
    return;
  }

  // XXX Remove this if with CameraPreviewMediaStream per bug 1124630.
  if (!mSrcStream->GetStream()->AsCameraPreviewStream()) {
    // Now that we have access to |mSrcStream| we can pipe it to our shadow
    // version |mPlaybackStream|. If two media elements are playing the
    // same realtime DOMMediaStream, this allows them to pause playback
    // independently of each other.
    mPlaybackStream = DOMMediaStream::CreateTrackUnionStream(window);
    mPlaybackStreamInputPort = mPlaybackStream->GetStream()->AsProcessedStream()->
      AllocateInputPort(mSrcStream->GetStream(), MediaInputPort::FLAG_BLOCK_OUTPUT);

    nsRefPtr<nsIPrincipal> principal = GetCurrentPrincipal();
    mPlaybackStream->CombineWithPrincipal(principal);

    // Let |mSrcStream| decide when the stream has finished.
    GetSrcMediaStream()->AsProcessedStream()->SetAutofinish(true);
  }

  nsRefPtr<MediaStream> stream = mSrcStream->GetStream();
  if (stream) {
    stream->SetAudioChannelType(mAudioChannel);
  }

  // XXX if we ever support capturing the output of a media element which is
  // playing a stream, we'll need to add a CombineWithPrincipal call here.
  mMediaStreamListener = new StreamListener(this, "HTMLMediaElement::mMediaStreamListener");
  mMediaStreamSizeListener = new StreamSizeListener(this);
  mWatchManager.Watch(*mMediaStreamListener, &HTMLMediaElement::UpdateReadyStateInternal);

  GetSrcMediaStream()->AddListener(mMediaStreamListener);
  // Listen for an initial image size on mSrcStream so we can get results even
  // if we block the mPlaybackStream.
  stream->AddListener(mMediaStreamSizeListener);
  if (mPaused) {
    GetSrcMediaStream()->ChangeExplicitBlockerCount(1);
  }
  if (mPausedForInactiveDocumentOrChannel) {
    GetSrcMediaStream()->ChangeExplicitBlockerCount(1);
  }

  ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_IDLE);

  ChangeDelayLoadStatus(false);
  GetSrcMediaStream()->AddAudioOutput(this);
  SetVolumeInternal();

  bool bUseOverlayImage = mSrcStream->AsDOMHwMediaStream() != nullptr;
  VideoFrameContainer* container;

  if (bUseOverlayImage) {
    container = GetOverlayImageVideoFrameContainer();
  }
  else {
    container = GetVideoFrameContainer();
  }

  if (container) {
    GetSrcMediaStream()->AddVideoOutput(container);
  }

  CheckAutoplayDataReady();

  // Note: we must call DisconnectTrackListListeners(...)  before dropping
  // mSrcStream
  mSrcStream->ConstructMediaTracks(AudioTracks(), VideoTracks());

  mSrcStream->OnTracksAvailable(new MediaStreamTracksAvailableCallback(this));

  // FirstFrameLoaded() will be called when the stream has current data.
}

void HTMLMediaElement::EndSrcMediaStreamPlayback()
{
  MediaStream* stream = GetSrcMediaStream();
  if (stream) {
    stream->RemoveListener(mMediaStreamListener);
  }
  if (mSrcStream->GetStream()) {
    mSrcStream->GetStream()->RemoveListener(mMediaStreamSizeListener);
  }
  mSrcStream->DisconnectTrackListListeners(AudioTracks(), VideoTracks());

  if (mPlaybackStreamInputPort) {
    mPlaybackStreamInputPort->Destroy();
  }

  // Kill its reference to this element
  mWatchManager.Unwatch(*mMediaStreamListener, &HTMLMediaElement::UpdateReadyStateInternal);
  mMediaStreamListener->Forget();
  mMediaStreamListener = nullptr;
  mMediaStreamSizeListener->Forget();
  mMediaStreamSizeListener = nullptr;
  if (stream) {
    stream->RemoveAudioOutput(this);
  }
  VideoFrameContainer* container = GetVideoFrameContainer();
  if (container) {
    if (stream) {
      stream->RemoveVideoOutput(container);
    }
    container->ClearCurrentFrame();
  }
  if (mPaused && stream) {
    stream->ChangeExplicitBlockerCount(-1);
  }
  if (mPausedForInactiveDocumentOrChannel && stream) {
    stream->ChangeExplicitBlockerCount(-1);
  }
  mSrcStream = nullptr;
  mPlaybackStreamInputPort = nullptr;
  mPlaybackStream = nullptr;
}

void HTMLMediaElement::ProcessMediaFragmentURI()
{
  nsMediaFragmentURIParser parser(mLoadingSrc);

  if (mDecoder && parser.HasEndTime()) {
    mFragmentEnd = parser.GetEndTime();
  }

  if (parser.HasStartTime()) {
    SetCurrentTime(parser.GetStartTime());
    mFragmentStart = parser.GetStartTime();
  }
}

void HTMLMediaElement::MetadataLoaded(const MediaInfo* aInfo,
                                      nsAutoPtr<const MetadataTags> aTags)
{
  MOZ_ASSERT(NS_IsMainThread());

  mMediaInfo = *aInfo;
  mIsEncrypted = aInfo->IsEncrypted()
#ifdef MOZ_EME
                 || mPendingEncryptedInitData.IsEncrypted()
#endif // MOZ_EME
                 ;
  mTags = aTags.forget();
  mLoadedDataFired = false;
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);

  if (mIsEncrypted) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->NotifyObservers(static_cast<nsIContent*>(this), "media-eme-metadataloaded", nullptr);
  }

  DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  if (IsVideo() && HasVideo()) {
    DispatchAsyncEvent(NS_LITERAL_STRING("resize"));
  }
  DispatchAsyncEvent(NS_LITERAL_STRING("loadedmetadata"));
  if (mDecoder && mDecoder->IsTransportSeekable() && mDecoder->IsMediaSeekable()) {
    ProcessMediaFragmentURI();
    mDecoder->SetFragmentEndTime(mFragmentEnd);
  }
  if (mIsEncrypted) {
    if (!mMediaSource && Preferences::GetBool("media.eme.mse-only", true)) {
      DecodeError();
      return;
    }

#ifdef MOZ_EME
    // Dispatch a distinct 'encrypted' event for each initData we have.
    for (const auto& initData : mPendingEncryptedInitData.mInitDatas) {
      DispatchEncrypted(initData.mInitData, initData.mType);
    }
    mPendingEncryptedInitData.mInitDatas.Clear();
#endif // MOZ_EME
  }

  // Expose the tracks to JS directly.
  for (OutputMediaStream& out : mOutputStreams) {
    if (aInfo->HasAudio()) {
      TrackID audioTrackId = aInfo->mAudio.mTrackId;
      out.mStream->CreateDOMTrack(audioTrackId, MediaSegment::AUDIO);
    }
    if (aInfo->HasVideo()) {
      TrackID videoTrackId = aInfo->mVideo.mTrackId;
      out.mStream->CreateDOMTrack(videoTrackId, MediaSegment::VIDEO);
    }
  }

  // If this element had a video track, but consists only of an audio track now,
  // delete the VideoFrameContainer. This happens when the src is changed to an
  // audio only file.
  // Else update its dimensions.
  if (!aInfo->HasVideo()) {
    ResetState();
  } else {
    mWatchManager.ManualNotify(&HTMLMediaElement::UpdateReadyStateInternal);
  }

  if (IsVideo() && aInfo->HasVideo()) {
    // We are a video element playing video so update the screen wakelock
    NotifyOwnerDocumentActivityChanged();
  }
}

void HTMLMediaElement::FirstFrameLoaded()
{
  NS_ASSERTION(!mSuspendedAfterFirstFrame, "Should not have already suspended");

  ChangeDelayLoadStatus(false);

  if (mDecoder && mAllowSuspendAfterFirstFrame && mPaused &&
      !HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay) &&
      mPreloadAction == HTMLMediaElement::PRELOAD_METADATA) {
    mSuspendedAfterFirstFrame = true;
    mDecoder->Suspend();
  }
}

void HTMLMediaElement::NetworkError()
{
  Error(nsIDOMMediaError::MEDIA_ERR_NETWORK);
}

void HTMLMediaElement::DecodeError()
{
  nsAutoString src;
  GetCurrentSrc(src);
  const char16_t* params[] = { src.get() };
  ReportLoadError("MediaLoadDecodeError", params, ArrayLength(params));

  if (mDecoder) {
    ShutdownDecoder();
  }
  RemoveMediaElementFromURITable();
  mLoadingSrc = nullptr;
  mMediaSource = nullptr;
  if (mIsLoadingFromSourceChildren) {
    mError = nullptr;
    if (mSourceLoadCandidate) {
      DispatchAsyncSourceError(mSourceLoadCandidate);
      QueueLoadFromSourceTask();
    } else {
      NS_WARNING("Should know the source we were loading from!");
    }
  } else {
    Error(nsIDOMMediaError::MEDIA_ERR_DECODE);
  }
}

void HTMLMediaElement::LoadAborted()
{
  Error(nsIDOMMediaError::MEDIA_ERR_ABORTED);
}

void HTMLMediaElement::Error(uint16_t aErrorCode)
{
  NS_ASSERTION(aErrorCode == nsIDOMMediaError::MEDIA_ERR_DECODE ||
               aErrorCode == nsIDOMMediaError::MEDIA_ERR_NETWORK ||
               aErrorCode == nsIDOMMediaError::MEDIA_ERR_ABORTED,
               "Only use nsIDOMMediaError codes!");

  // Since we have multiple paths calling into DecodeError, e.g.
  // MediaKeys::Terminated and EMEH264Decoder::Error. We should take the 1st
  // one only in order not to fire multiple 'error' events.
  if (mError) {
    return;
  }

  mError = new MediaError(this, aErrorCode);
  DispatchAsyncEvent(NS_LITERAL_STRING("error"));
  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_EMPTY);
    DispatchAsyncEvent(NS_LITERAL_STRING("emptied"));
  } else {
    ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_IDLE);
  }
  ChangeDelayLoadStatus(false);
}

void HTMLMediaElement::PlaybackEnded()
{
  // We changed state which can affect AddRemoveSelfReference
  AddRemoveSelfReference();

  NS_ASSERTION(!mDecoder || mDecoder->IsEndedOrShutdown(),
               "Decoder fired ended, but not in ended state");

  // Discard all output streams that have finished now.
  for (int32_t i = mOutputStreams.Length() - 1; i >= 0; --i) {
    if (mOutputStreams[i].mFinishWhenEnded) {
      mOutputStreams.RemoveElementAt(i);
    }
  }

  if (mSrcStream || (mDecoder && mDecoder->IsInfinite())) {
    LOG(LogLevel::Debug, ("%p, got duration by reaching the end of the resource", this));
    DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::loop)) {
    SetCurrentTime(0);
    return;
  }

  Pause();

  FireTimeUpdate(false);
  DispatchAsyncEvent(NS_LITERAL_STRING("ended"));
}

void HTMLMediaElement::SeekStarted()
{
  DispatchAsyncEvent(NS_LITERAL_STRING("seeking"));
  // Set the Variable if the Seekstarted while active playing
  if(mPlayingThroughTheAudioChannel) {
    mPlayingThroughTheAudioChannelBeforeSeek = true;
  }
}

void HTMLMediaElement::SeekCompleted()
{
  mPlayingBeforeSeek = false;
  SetPlayedOrSeeked(true);
  FireTimeUpdate(false);
  DispatchAsyncEvent(NS_LITERAL_STRING("seeked"));
  // We changed whether we're seeking so we need to AddRemoveSelfReference
  AddRemoveSelfReference();
  if (mTextTrackManager) {
    mTextTrackManager->DidSeek();
  }
  if (mCurrentPlayRangeStart == -1.0) {
    mCurrentPlayRangeStart = CurrentTime();
  }
  // Unset the variable on seekend
  mPlayingThroughTheAudioChannelBeforeSeek = false;
}

void HTMLMediaElement::NotifySuspendedByCache(bool aIsSuspended)
{
  mDownloadSuspendedByCache = aIsSuspended;
}

void HTMLMediaElement::DownloadSuspended()
{
  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING) {
    DispatchAsyncEvent(NS_LITERAL_STRING("progress"));
  }
  if (mBegun) {
    ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_IDLE);
  }
}

void HTMLMediaElement::DownloadResumed(bool aForceNetworkLoading)
{
  if (mBegun || aForceNetworkLoading) {
    ChangeNetworkState(nsIDOMHTMLMediaElement::NETWORK_LOADING);
  }
}

void HTMLMediaElement::CheckProgress(bool aHaveNewProgress)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING);

  TimeStamp now = TimeStamp::NowLoRes();

  if (aHaveNewProgress) {
    mDataTime = now;
  }

  // If this is the first progress, or PROGRESS_MS has passed since the last
  // progress event fired and more data has arrived since then, fire a
  // progress event.
  NS_ASSERTION((mProgressTime.IsNull() && !aHaveNewProgress) ||
               !mDataTime.IsNull(),
               "null TimeStamp mDataTime should not be used in comparison");
  if (mProgressTime.IsNull() ? aHaveNewProgress
      : (now - mProgressTime >= TimeDuration::FromMilliseconds(PROGRESS_MS) &&
         mDataTime > mProgressTime)) {
    DispatchAsyncEvent(NS_LITERAL_STRING("progress"));
    // Resolution() ensures that future data will have now > mProgressTime,
    // and so will trigger another event.  mDataTime is not reset because it
    // is still required to detect stalled; it is similarly offset by
    // resolution to indicate the new data has not yet arrived.
    mProgressTime = now - TimeDuration::Resolution();
    if (mDataTime > mProgressTime) {
      mDataTime = mProgressTime;
    }
    if (!mProgressTimer) {
      NS_ASSERTION(aHaveNewProgress,
                   "timer dispatched when there was no timer");
      // Were stalled.  Restart timer.
      StartProgressTimer();
      if (!mLoadedDataFired) {
        ChangeDelayLoadStatus(true);
      }
    }
  }

  if (now - mDataTime >= TimeDuration::FromMilliseconds(STALL_MS)) {
    DispatchAsyncEvent(NS_LITERAL_STRING("stalled"));

    if (mMediaSource) {
      ChangeDelayLoadStatus(false);
    }

    NS_ASSERTION(mProgressTimer, "detected stalled without timer");
    // Stop timer events, which prevents repeated stalled events until there
    // is more progress.
    StopProgress();
  }

  AddRemoveSelfReference();
}

/* static */
void HTMLMediaElement::ProgressTimerCallback(nsITimer* aTimer, void* aClosure)
{
  auto decoder = static_cast<HTMLMediaElement*>(aClosure);
  decoder->CheckProgress(false);
}

void HTMLMediaElement::StartProgressTimer()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING);
  NS_ASSERTION(!mProgressTimer, "Already started progress timer.");

  mProgressTimer = do_CreateInstance("@mozilla.org/timer;1");
  mProgressTimer->InitWithFuncCallback(ProgressTimerCallback,
                                       this,
                                       PROGRESS_MS,
                                       nsITimer::TYPE_REPEATING_SLACK);
}

void HTMLMediaElement::StartProgress()
{
  // Record the time now for detecting stalled.
  mDataTime = TimeStamp::NowLoRes();
  // Reset mProgressTime so that mDataTime is not indicating bytes received
  // after the last progress event.
  mProgressTime = TimeStamp();
  StartProgressTimer();
}

void HTMLMediaElement::StopProgress()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mProgressTimer) {
    return;
  }

  mProgressTimer->Cancel();
  mProgressTimer = nullptr;
}

void HTMLMediaElement::DownloadProgressed()
{
  if (mNetworkState != nsIDOMHTMLMediaElement::NETWORK_LOADING) {
    return;
  }
  CheckProgress(true);
}

bool HTMLMediaElement::ShouldCheckAllowOrigin()
{
  return mCORSMode != CORS_NONE;
}

bool HTMLMediaElement::IsCORSSameOrigin()
{
  bool subsumes;
  nsRefPtr<nsIPrincipal> principal = GetCurrentPrincipal();
  return
    (NS_SUCCEEDED(NodePrincipal()->Subsumes(principal, &subsumes)) && subsumes) ||
    ShouldCheckAllowOrigin();
}

void
HTMLMediaElement::UpdateReadyStateInternal()
{
  if (!mDecoder && !mSrcStream) {
    // Not initialized - bail out.
    return;
  }

  if (mDecoder && mReadyState < nsIDOMHTMLMediaElement::HAVE_METADATA) {
    // aNextFrame might have a next frame because the decoder can advance
    // on its own thread before MetadataLoaded gets a chance to run.
    // The arrival of more data can't change us out of this readyState.
    return;
  }

  if (mSrcStream && mReadyState < nsIDOMHTMLMediaElement::HAVE_METADATA) {
    bool hasAudio = !AudioTracks()->IsEmpty();
    bool hasVideo = !VideoTracks()->IsEmpty();

    if ((!hasAudio && !hasVideo) ||
        (IsVideo() && hasVideo && !HasVideo())) {
      return;
    }

    // We are playing a stream that has video and a video frame is now set.
    // This means we have all metadata needed to change ready state.
    MediaInfo mediaInfo = mMediaInfo;
    if (hasAudio) {
      mediaInfo.EnableAudio();
    }
    if (hasVideo) {
      mediaInfo.EnableVideo();
    }
    MetadataLoaded(&mediaInfo, nsAutoPtr<const MetadataTags>(nullptr));
  }

  if (NextFrameStatus() == MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING) {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);
    return;
  }

  if (IsVideo() && HasVideo() && !IsPlaybackEnded() &&
        GetImageContainer() && !GetImageContainer()->HasCurrentImage()) {
    // Don't advance if we are playing video, but don't have a video frame.
    // Also, if video became available after advancing to HAVE_CURRENT_DATA
    // while we are still playing, we need to revert to HAVE_METADATA until
    // a video frame is available.
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_METADATA);
    return;
  }

  if (mDownloadSuspendedByCache && mDecoder && !mDecoder->IsEndedOrShutdown()) {
    // The decoder has signaled that the download has been suspended by the
    // media cache. So move readyState into HAVE_ENOUGH_DATA, in case there's
    // script waiting for a "canplaythrough" event; without this forced
    // transition, we will never fire the "canplaythrough" event if the
    // media cache is too small, and scripts are bound to fail. Don't force
    // this transition if the decoder is in ended state; the readyState
    // should remain at HAVE_CURRENT_DATA in this case.
    // Note that this state transition includes the case where we finished
    // downloaded the whole data stream.
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
    return;
  }

  if (NextFrameStatus() != MediaDecoderOwner::NEXT_FRAME_AVAILABLE) {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA);
    if (!mWaitingFired && NextFrameStatus() == MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING) {
      FireTimeUpdate(false);
      DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
      mWaitingFired = true;
    }
    return;
  }

  if (mSrcStream) {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
    return;
  }

  // Now see if we should set HAVE_ENOUGH_DATA.
  // If it's something we don't know the size of, then we can't
  // make a real estimate, so we go straight to HAVE_ENOUGH_DATA once
  // we've downloaded enough data that our download rate is considered
  // reliable. We have to move to HAVE_ENOUGH_DATA at some point or
  // autoplay elements for live streams will never play. Otherwise we
  // move to HAVE_ENOUGH_DATA if we can play through the entire media
  // without stopping to buffer.
  if (mDecoder->CanPlayThrough())
  {
    ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA);
    return;
  }
  ChangeReadyState(nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA);
}

static const char* const gReadyStateToString[] = {
  "HAVE_NOTHING",
  "HAVE_METADATA",
  "HAVE_CURRENT_DATA",
  "HAVE_FUTURE_DATA",
  "HAVE_ENOUGH_DATA"
};

void HTMLMediaElement::ChangeReadyState(nsMediaReadyState aState)
{
  nsMediaReadyState oldState = mReadyState;
  mReadyState = aState;

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY ||
      oldState == mReadyState) {
    return;
  }

  LOG(LogLevel::Debug, ("%p Ready state changed to %s", this, gReadyStateToString[aState]));

  UpdateAudioChannelPlayingState();

  // Handle raising of "waiting" event during seek (see 4.8.10.9)
  if (mPlayingBeforeSeek &&
      mReadyState < nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA) {
    DispatchAsyncEvent(NS_LITERAL_STRING("waiting"));
  }

  if (oldState < nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA &&
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA &&
      !mLoadedDataFired) {
    DispatchAsyncEvent(NS_LITERAL_STRING("loadeddata"));
    mLoadedDataFired = true;
  }

  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA) {
    mWaitingFired = false;
  }

  if (oldState < nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA &&
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA) {
    DispatchAsyncEvent(NS_LITERAL_STRING("canplay"));
  }

  CheckAutoplayDataReady();

  if (oldState < nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA &&
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA &&
      IsPotentiallyPlaying()) {
    DispatchAsyncEvent(NS_LITERAL_STRING("playing"));
  }

  if (oldState < nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA &&
      mReadyState >= nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA) {
    DispatchAsyncEvent(NS_LITERAL_STRING("canplaythrough"));
  }
}

static const char* const gNetworkStateToString[] = {
  "EMPTY",
  "IDLE",
  "LOADING",
  "NO_SOURCE"
 };

void HTMLMediaElement::ChangeNetworkState(nsMediaNetworkState aState)
{
  if (mNetworkState == aState) {
    return;
  }

  nsMediaNetworkState oldState = mNetworkState;
  mNetworkState = aState;
  LOG(LogLevel::Debug, ("%p Network state changed to %s", this, gNetworkStateToString[aState]));

  // TODO: |mBegun| reflects the download status. We should be able to remove
  // it and check |mNetworkState| only.

  if (oldState == nsIDOMHTMLMediaElement::NETWORK_LOADING) {
    // Reset |mBegun| since we're not downloading anymore.
    mBegun = false;
    // Stop progress notification when exiting NETWORK_LOADING.
    StopProgress();
  }

  if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING) {
    // Download is begun.
    mBegun = true;
    // Start progress notification when entering NETWORK_LOADING.
    StartProgress();
  } else if (mNetworkState == nsIDOMHTMLMediaElement::NETWORK_IDLE && !mError) {
    // Fire 'suspend' event when entering NETWORK_IDLE and no error presented.
    DispatchAsyncEvent(NS_LITERAL_STRING("suspend"));
  }

  // Changing mNetworkState affects AddRemoveSelfReference().
  AddRemoveSelfReference();
}

bool HTMLMediaElement::CanActivateAutoplay()
{
  // For stream inputs, we activate autoplay on HAVE_NOTHING because
  // this element itself might be blocking the stream from making progress by
  // being paused.
  return !mPausedForInactiveDocumentOrChannel &&
         mAutoplaying &&
         mPaused &&
         ((mDecoder && mReadyState >= nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA) ||
          mSrcStream) &&
         HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay) &&
         mAutoplayEnabled &&
         !IsEditable();
}

void HTMLMediaElement::CheckAutoplayDataReady()
{
  if (CanActivateAutoplay()) {
    mPaused = false;
    // We changed mPaused which can affect AddRemoveSelfReference
    AddRemoveSelfReference();

    if (mDecoder) {
      SetPlayedOrSeeked(true);
      if (mCurrentPlayRangeStart == -1.0) {
        mCurrentPlayRangeStart = CurrentTime();
      }
      mDecoder->Play();
    } else if (mSrcStream) {
      SetPlayedOrSeeked(true);
      GetSrcMediaStream()->ChangeExplicitBlockerCount(-1);
    }
    DispatchAsyncEvent(NS_LITERAL_STRING("play"));
  }
}

bool HTMLMediaElement::IsActive()
{
  nsIDocument* ownerDoc = OwnerDoc();
  return ownerDoc && ownerDoc->IsActive() && ownerDoc->IsVisible();
}

bool HTMLMediaElement::IsHidden()
{
  if (mElementInTreeState == ELEMENT_NOT_INTREE_HAD_INTREE) {
    return true;
  }
  nsIDocument* ownerDoc = OwnerDoc();
  return !ownerDoc || ownerDoc->Hidden();
}

VideoFrameContainer* HTMLMediaElement::GetVideoFrameContainer()
{
  if (mVideoFrameContainer)
    return mVideoFrameContainer;

  // Only video frames need an image container.
  if (!IsVideo()) {
    return nullptr;
  }

  mVideoFrameContainer =
    new VideoFrameContainer(this, LayerManager::CreateImageContainer(ImageContainer::ASYNCHRONOUS));

  return mVideoFrameContainer;
}

VideoFrameContainer* HTMLMediaElement::GetOverlayImageVideoFrameContainer()
{
  if (mVideoFrameContainer)
    return mVideoFrameContainer;

  // Only video frames need an image container.
  if (!IsVideo()) {
    return nullptr;
  }

  mVideoFrameContainer =
    new VideoFrameContainer(this, LayerManager::CreateImageContainer(ImageContainer::ASYNCHRONOUS_OVERLAY));

  return mVideoFrameContainer;
}

nsresult HTMLMediaElement::DispatchEvent(const nsAString& aName)
{
  LOG_EVENT(LogLevel::Debug, ("%p Dispatching event %s", this,
                          NS_ConvertUTF16toUTF8(aName).get()));

  // Save events that occur while in the bfcache. These will be dispatched
  // if the page comes out of the bfcache.
  if (mEventDeliveryPaused) {
    mPendingEvents.AppendElement(aName);
    return NS_OK;
  }

  return nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                              static_cast<nsIContent*>(this),
                                              aName,
                                              false,
                                              false);
}

nsresult HTMLMediaElement::DispatchAsyncEvent(const nsAString& aName)
{
  LOG_EVENT(LogLevel::Debug, ("%p Queuing event %s", this,
            NS_ConvertUTF16toUTF8(aName).get()));

  // Save events that occur while in the bfcache. These will be dispatched
  // if the page comes out of the bfcache.
  if (mEventDeliveryPaused) {
    mPendingEvents.AppendElement(aName);
    return NS_OK;
  }

  nsCOMPtr<nsIRunnable> event = new nsAsyncEventRunner(aName, this);
  NS_DispatchToMainThread(event);

  // Only collect rebuffer and stall rate stats for MSE video.
  if (!mMediaSource) {
    return NS_OK;
  }

  if ((aName.EqualsLiteral("play") || aName.EqualsLiteral("playing"))) {
    mPlayTime.Start();
    mJoinLatency.Pause();
  } else if (aName.EqualsLiteral("waiting")) {
    mPlayTime.Pause();
    Telemetry::Accumulate(Telemetry::VIDEO_MSE_BUFFERING_COUNT, 1);
  } else if (aName.EqualsLiteral("pause")) {
    mPlayTime.Pause();
  }

  return NS_OK;
}

nsresult HTMLMediaElement::DispatchPendingMediaEvents()
{
  NS_ASSERTION(!mEventDeliveryPaused,
               "Must not be in bfcache when dispatching pending media events");

  uint32_t count = mPendingEvents.Length();
  for (uint32_t i = 0; i < count; ++i) {
    DispatchAsyncEvent(mPendingEvents[i]);
  }
  mPendingEvents.Clear();

  return NS_OK;
}

bool HTMLMediaElement::IsPotentiallyPlaying() const
{
  // TODO:
  //   playback has not stopped due to errors,
  //   and the element has not paused for user interaction
  return
    !mPaused &&
    (mReadyState == nsIDOMHTMLMediaElement::HAVE_ENOUGH_DATA ||
    mReadyState == nsIDOMHTMLMediaElement::HAVE_FUTURE_DATA) &&
    !IsPlaybackEnded();
}

bool HTMLMediaElement::IsPlaybackEnded() const
{
  // TODO:
  //   the current playback position is equal to the effective end of the media resource.
  //   See bug 449157.
  return mReadyState >= nsIDOMHTMLMediaElement::HAVE_METADATA &&
    mDecoder ? mDecoder->IsEndedOrShutdown() : false;
}

already_AddRefed<nsIPrincipal> HTMLMediaElement::GetCurrentPrincipal()
{
  if (mDecoder) {
    return mDecoder->GetCurrentPrincipal();
  }
  if (mSrcStream) {
    nsRefPtr<nsIPrincipal> principal = mSrcStream->GetPrincipal();
    return principal.forget();
  }
  return nullptr;
}

void HTMLMediaElement::NotifyDecoderPrincipalChanged()
{
  nsRefPtr<nsIPrincipal> principal = GetCurrentPrincipal();

  mDecoder->UpdateSameOriginStatus(!principal || IsCORSSameOrigin());

  for (uint32_t i = 0; i < mOutputStreams.Length(); ++i) {
    OutputMediaStream* ms = &mOutputStreams[i];
    ms->mStream->SetCORSMode(mCORSMode);
    ms->mStream->CombineWithPrincipal(principal);
  }
}

void HTMLMediaElement::UpdateMediaSize(const nsIntSize& aSize)
{
  if (IsVideo() && mReadyState != HAVE_NOTHING &&
      mMediaInfo.mVideo.mDisplay != aSize) {
    DispatchAsyncEvent(NS_LITERAL_STRING("resize"));
  }

  mMediaInfo.mVideo.mDisplay = aSize;
  mWatchManager.ManualNotify(&HTMLMediaElement::UpdateReadyStateInternal);
}

void HTMLMediaElement::UpdateInitialMediaSize(const nsIntSize& aSize)
{
  if (!mMediaInfo.HasVideo()) {
    UpdateMediaSize(aSize);
  }
}

void HTMLMediaElement::SuspendOrResumeElement(bool aPauseElement, bool aSuspendEvents)
{
  if (aPauseElement != mPausedForInactiveDocumentOrChannel) {
    mPausedForInactiveDocumentOrChannel = aPauseElement;
    if (aPauseElement) {
      if (mMediaSource) {
        ReportMSETelemetry();
#ifdef MOZ_EME
        ReportEMETelemetry();
#endif
      }

#ifdef MOZ_EME
      // For EME content, force destruction of the CDM client (and CDM
      // instance if this is the last client for that CDM instance) and
      // the CDM's decoder. This ensures the CDM gets reliable and prompt
      // shutdown notifications, as it may have book-keeping it needs
      // to do on shutdown.
      if (mMediaKeys) {
        mMediaKeys->Shutdown();
        mMediaKeys = nullptr;
        if (mDecoder) {
          ShutdownDecoder();
        }
      }
#endif
      if (mDecoder) {
        mDecoder->Pause();
        mDecoder->Suspend();
      } else if (mSrcStream) {
        GetSrcMediaStream()->ChangeExplicitBlockerCount(1);
      }
      mEventDeliveryPaused = aSuspendEvents;
    } else {
#ifdef MOZ_EME
      MOZ_ASSERT(!mMediaKeys);
#endif
      if (mDecoder) {
        mDecoder->Resume(false);
        if (!mPaused && !mDecoder->IsEndedOrShutdown()) {
          mDecoder->Play();
        }
      } else if (mSrcStream) {
        GetSrcMediaStream()->ChangeExplicitBlockerCount(-1);
      }
      if (mEventDeliveryPaused) {
        mEventDeliveryPaused = false;
        DispatchPendingMediaEvents();
      }
    }
  }
}

bool HTMLMediaElement::IsBeingDestroyed()
{
  nsIDocument* ownerDoc = OwnerDoc();
  nsIDocShell* docShell = ownerDoc ? ownerDoc->GetDocShell() : nullptr;
  bool isBeingDestroyed = false;
  if (docShell) {
    docShell->IsBeingDestroyed(&isBeingDestroyed);
  }
  return isBeingDestroyed;
}

void HTMLMediaElement::NotifyOwnerDocumentActivityChanged()
{
  nsIDocument* ownerDoc = OwnerDoc();
  if (mDecoder && !IsBeingDestroyed()) {
    mDecoder->SetElementVisibility(!ownerDoc->Hidden());
    mDecoder->NotifyOwnerActivityChanged();
  }

  // SetVisibilityState will update mMuted with MUTED_BY_AUDIO_CHANNEL via the
  // CanPlayChanged callback.
  if (UseAudioChannelService() && mPlayingThroughTheAudioChannel &&
      mAudioChannelAgent) {
    AutoNoJSAPI nojsapi;
    mAudioChannelAgent->SetVisibilityState(!ownerDoc->Hidden());
  }
  bool pauseElement = !IsActive() || (mMuted & MUTED_BY_AUDIO_CHANNEL);

  SuspendOrResumeElement(pauseElement, !IsActive());

  AddRemoveSelfReference();
}

void HTMLMediaElement::AddRemoveSelfReference()
{
  // XXX we could release earlier here in many situations if we examined
  // which event listeners are attached. Right now we assume there is a
  // potential listener for every event. We would also have to keep the
  // element alive if it was playing and producing audio output --- right now
  // that's covered by the !mPaused check.
  nsIDocument* ownerDoc = OwnerDoc();

  // See the comment at the top of this file for the explanation of this
  // boolean expression.
  bool needSelfReference = !mShuttingDown &&
    ownerDoc->IsActive() &&
    (mDelayingLoadEvent ||
     (!mPaused && mDecoder && !mDecoder->IsEndedOrShutdown()) ||
     (!mPaused && mSrcStream && !mSrcStream->IsFinished()) ||
     (mDecoder && mDecoder->IsSeeking()) ||
     CanActivateAutoplay() ||
     (mMediaSource ? mProgressTimer :
      mNetworkState == nsIDOMHTMLMediaElement::NETWORK_LOADING));

  if (needSelfReference != mHasSelfReference) {
    mHasSelfReference = needSelfReference;
    if (needSelfReference) {
      // The observer service will hold a strong reference to us. This
      // will do to keep us alive. We need to know about shutdown so that
      // we can release our self-reference.
      nsContentUtils::RegisterShutdownObserver(this);
    } else {
      // Dispatch Release asynchronously so that we don't destroy this object
      // inside a call stack of method calls on this object
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(this, &HTMLMediaElement::DoRemoveSelfReference);
      NS_DispatchToMainThread(event);
    }
  }

  UpdateAudioChannelPlayingState();
}

void HTMLMediaElement::DoRemoveSelfReference()
{
  // We don't need the shutdown observer anymore. Unregistering releases
  // its reference to us, which we were using as our self-reference.
  nsContentUtils::UnregisterShutdownObserver(this);
}

nsresult HTMLMediaElement::Observe(nsISupports* aSubject,
                                   const char* aTopic, const char16_t* aData)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);

  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    mShuttingDown = true;
    AddRemoveSelfReference();
  }
  return NS_OK;
}

bool
HTMLMediaElement::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~(eCONTENT | eMEDIA));
}

void HTMLMediaElement::DispatchAsyncSourceError(nsIContent* aSourceElement)
{
  LOG_EVENT(LogLevel::Debug, ("%p Queuing simple source error event", this));

  nsCOMPtr<nsIRunnable> event = new nsSourceErrorEventRunner(this, aSourceElement);
  NS_DispatchToMainThread(event);
}

void HTMLMediaElement::NotifyAddedSource()
{
  // If a source element is inserted as a child of a media element
  // that has no src attribute and whose networkState has the value
  // NETWORK_EMPTY, the user agent must invoke the media element's
  // resource selection algorithm.
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::src) &&
      mNetworkState == nsIDOMHTMLMediaElement::NETWORK_EMPTY)
  {
    QueueSelectResourceTask();
  }

  // A load was paused in the resource selection algorithm, waiting for
  // a new source child to be added, resume the resource selection algorithm.
  if (mLoadWaitStatus == WAITING_FOR_SOURCE) {
    QueueLoadFromSourceTask();
  }
}

nsIContent* HTMLMediaElement::GetNextSource()
{
  nsCOMPtr<nsIDOMNode> thisDomNode = do_QueryObject(this);

  mSourceLoadCandidate = nullptr;

  nsresult rv = NS_OK;
  if (!mSourcePointer) {
    // First time this has been run, create a selection to cover children.
    mSourcePointer = new nsRange(this);
    // If this media element is removed from the DOM, don't gravitate the
    // range up to its ancestor, leave it attached to the media element.
    mSourcePointer->SetEnableGravitationOnElementRemoval(false);

    rv = mSourcePointer->SelectNodeContents(thisDomNode);
    if (NS_FAILED(rv)) return nullptr;

    rv = mSourcePointer->Collapse(true);
    if (NS_FAILED(rv)) return nullptr;
  }

  while (true) {
#ifdef DEBUG
    nsCOMPtr<nsIDOMNode> startContainer;
    rv = mSourcePointer->GetStartContainer(getter_AddRefs(startContainer));
    if (NS_FAILED(rv)) return nullptr;
    NS_ASSERTION(startContainer == thisDomNode,
                "Should only iterate over direct children");
#endif

    int32_t startOffset = 0;
    rv = mSourcePointer->GetStartOffset(&startOffset);
    NS_ENSURE_SUCCESS(rv, nullptr);

    if (uint32_t(startOffset) == GetChildCount())
      return nullptr; // No more children.

    // Advance the range to the next child.
    rv = mSourcePointer->SetStart(thisDomNode, startOffset + 1);
    NS_ENSURE_SUCCESS(rv, nullptr);

    nsIContent* child = GetChildAt(startOffset);

    // If child is a <source> element, it is the next candidate.
    if (child && child->IsHTMLElement(nsGkAtoms::source)) {
      mSourceLoadCandidate = child;
      return child;
    }
  }
  NS_NOTREACHED("Execution should not reach here!");
  return nullptr;
}

void HTMLMediaElement::ChangeDelayLoadStatus(bool aDelay)
{
  if (mDelayingLoadEvent == aDelay)
    return;

  mDelayingLoadEvent = aDelay;

  LOG(LogLevel::Debug, ("%p ChangeDelayLoadStatus(%d) doc=0x%p", this, aDelay, mLoadBlockedDoc.get()));
  if (mDecoder) {
    mDecoder->SetLoadInBackground(!aDelay);
  }
  if (aDelay) {
    mLoadBlockedDoc = OwnerDoc();
    mLoadBlockedDoc->BlockOnload();
  } else {
    // mLoadBlockedDoc might be null due to GC unlinking
    if (mLoadBlockedDoc) {
      mLoadBlockedDoc->UnblockOnload(false);
      mLoadBlockedDoc = nullptr;
    }
  }

  // We changed mDelayingLoadEvent which can affect AddRemoveSelfReference
  AddRemoveSelfReference();
}

already_AddRefed<nsILoadGroup> HTMLMediaElement::GetDocumentLoadGroup()
{
  if (!OwnerDoc()->IsActive()) {
    NS_WARNING("Load group requested for media element in inactive document.");
  }
  return OwnerDoc()->GetDocumentLoadGroup();
}

nsresult
HTMLMediaElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDest->OwnerDoc()->IsStaticDocument()) {
    HTMLMediaElement* dest = static_cast<HTMLMediaElement*>(aDest);
    dest->mMediaInfo = mMediaInfo;
  }
  return rv;
}

already_AddRefed<TimeRanges>
HTMLMediaElement::Buffered() const
{
  nsRefPtr<TimeRanges> ranges = new TimeRanges();
  if (mReadyState > nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    if (mDecoder) {
      media::TimeIntervals buffered = mDecoder->GetBuffered();
      if (!buffered.IsInvalid()) {
        buffered.ToTimeRanges(ranges);
      }
    }
  }
  return ranges.forget();
}

nsresult HTMLMediaElement::GetBuffered(nsIDOMTimeRanges** aBuffered)
{
  nsRefPtr<TimeRanges> ranges = Buffered();
  ranges.forget(aBuffered);
  return NS_OK;
}

void HTMLMediaElement::SetRequestHeaders(nsIHttpChannel* aChannel)
{
  // Send Accept header for video and audio types only (Bug 489071)
  SetAcceptHeader(aChannel);

  // Media elements are likely candidates for HTTP Pipeline head of line
  // blocking problems, so disable pipelines.
  nsLoadFlags loadflags;
  aChannel->GetLoadFlags(&loadflags);
  loadflags |= nsIRequest::INHIBIT_PIPELINE;
  aChannel->SetLoadFlags(loadflags);

  // Apache doesn't send Content-Length when gzip transfer encoding is used,
  // which prevents us from estimating the video length (if explicit Content-Duration
  // and a length spec in the container are not present either) and from seeking.
  // So, disable the standard "Accept-Encoding: gzip,deflate" that we usually send.
  // See bug 614760.
  aChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept-Encoding"),
                             EmptyCString(), false);

  // Set the Referer header
  aChannel->SetReferrerWithPolicy(OwnerDoc()->GetDocumentURI(),
                                  OwnerDoc()->GetReferrerPolicy());
}

void HTMLMediaElement::FireTimeUpdate(bool aPeriodic)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  TimeStamp now = TimeStamp::Now();
  double time = CurrentTime();

  // Fire a timeupdate event if this is not a periodic update (i.e. it's a
  // timeupdate event mandated by the spec), or if it's a periodic update
  // and TIMEUPDATE_MS has passed since the last timeupdate event fired and
  // the time has changed.
  if (!aPeriodic ||
      (mLastCurrentTime != time &&
       (mTimeUpdateTime.IsNull() ||
        now - mTimeUpdateTime >= TimeDuration::FromMilliseconds(TIMEUPDATE_MS)))) {
    DispatchAsyncEvent(NS_LITERAL_STRING("timeupdate"));
    mTimeUpdateTime = now;
    mLastCurrentTime = time;
  }
  if (mFragmentEnd >= 0.0 && time >= mFragmentEnd) {
    Pause();
    mFragmentEnd = -1.0;
    mFragmentStart = -1.0;
    mDecoder->SetFragmentEndTime(mFragmentEnd);
  }

  // Update the cues displaying on the video.
  // Here mTextTrackManager can be null if the cycle collector has unlinked
  // us before our parent. In that case UnbindFromTree will call us
  // when our parent is unlinked.
  if (mTextTrackManager) {
    mTextTrackManager->UpdateCueDisplay();
  }
}

void HTMLMediaElement::GetCurrentSpec(nsCString& aString)
{
  if (mLoadingSrc) {
    mLoadingSrc->GetSpec(aString);
  } else {
    aString.Truncate();
  }
}

/* attribute double mozFragmentEnd; */
double
HTMLMediaElement::MozFragmentEnd()
{
  double duration = Duration();

  // If there is no end fragment, or the fragment end is greater than the
  // duration, return the duration.
  return (mFragmentEnd < 0.0 || mFragmentEnd > duration) ? duration : mFragmentEnd;
}

NS_IMETHODIMP HTMLMediaElement::GetMozFragmentEnd(double* aTime)
{
  *aTime = MozFragmentEnd();
  return NS_OK;
}

static double ClampPlaybackRate(double aPlaybackRate)
{
  if (aPlaybackRate == 0.0) {
    return aPlaybackRate;
  }
  if (Abs(aPlaybackRate) < MIN_PLAYBACKRATE) {
    return aPlaybackRate < 0 ? -MIN_PLAYBACKRATE : MIN_PLAYBACKRATE;
  }
  if (Abs(aPlaybackRate) > MAX_PLAYBACKRATE) {
    return aPlaybackRate < 0 ? -MAX_PLAYBACKRATE : MAX_PLAYBACKRATE;
  }
  return aPlaybackRate;
}

/* attribute double defaultPlaybackRate; */
NS_IMETHODIMP HTMLMediaElement::GetDefaultPlaybackRate(double* aDefaultPlaybackRate)
{
  *aDefaultPlaybackRate = DefaultPlaybackRate();
  return NS_OK;
}

void
HTMLMediaElement::SetDefaultPlaybackRate(double aDefaultPlaybackRate, ErrorResult& aRv)
{
  if (aDefaultPlaybackRate < 0) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  mDefaultPlaybackRate = ClampPlaybackRate(aDefaultPlaybackRate);
  DispatchAsyncEvent(NS_LITERAL_STRING("ratechange"));
}

NS_IMETHODIMP HTMLMediaElement::SetDefaultPlaybackRate(double aDefaultPlaybackRate)
{
  ErrorResult rv;
  SetDefaultPlaybackRate(aDefaultPlaybackRate, rv);
  return rv.StealNSResult();
}

/* attribute double playbackRate; */
NS_IMETHODIMP HTMLMediaElement::GetPlaybackRate(double* aPlaybackRate)
{
  *aPlaybackRate = PlaybackRate();
  return NS_OK;
}

void
HTMLMediaElement::SetPlaybackRate(double aPlaybackRate, ErrorResult& aRv)
{
  // Changing the playback rate of a media that has more than two channels is
  // not supported.
  if (aPlaybackRate < 0) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  mPlaybackRate = ClampPlaybackRate(aPlaybackRate);

  if (mPlaybackRate != 0.0 &&
      (mPlaybackRate < 0 || mPlaybackRate > THRESHOLD_HIGH_PLAYBACKRATE_AUDIO ||
       mPlaybackRate < THRESHOLD_LOW_PLAYBACKRATE_AUDIO)) {
    SetMutedInternal(mMuted | MUTED_BY_INVALID_PLAYBACK_RATE);
  } else {
    SetMutedInternal(mMuted & ~MUTED_BY_INVALID_PLAYBACK_RATE);
  }

  if (mDecoder) {
    mDecoder->SetPlaybackRate(mPlaybackRate);
  }
  DispatchAsyncEvent(NS_LITERAL_STRING("ratechange"));
}

NS_IMETHODIMP HTMLMediaElement::SetPlaybackRate(double aPlaybackRate)
{
  ErrorResult rv;
  SetPlaybackRate(aPlaybackRate, rv);
  return rv.StealNSResult();
}

/* attribute bool mozPreservesPitch; */
NS_IMETHODIMP HTMLMediaElement::GetMozPreservesPitch(bool* aPreservesPitch)
{
  *aPreservesPitch = MozPreservesPitch();
  return NS_OK;
}

NS_IMETHODIMP HTMLMediaElement::SetMozPreservesPitch(bool aPreservesPitch)
{
  mPreservesPitch = aPreservesPitch;
  if (mDecoder) {
    mDecoder->SetPreservesPitch(mPreservesPitch);
  }
  return NS_OK;
}

ImageContainer* HTMLMediaElement::GetImageContainer()
{
  VideoFrameContainer* container = GetVideoFrameContainer();
  return container ? container->GetImageContainer() : nullptr;
}

nsresult HTMLMediaElement::UpdateChannelMuteState(AudioChannelState aCanPlay)
{
  if (!UseAudioChannelService()) {
    return NS_OK;
  }

  if ((aCanPlay == AUDIO_CHANNEL_STATE_FADED && !mAudioChannelFaded) ||
      (aCanPlay != AUDIO_CHANNEL_STATE_FADED && mAudioChannelFaded)) {
    mAudioChannelFaded = !mAudioChannelFaded;
    SetVolumeInternal();
  }

  // We have to mute this channel.
  if (aCanPlay == AUDIO_CHANNEL_STATE_MUTED && !(mMuted & MUTED_BY_AUDIO_CHANNEL)) {
    SetMutedInternal(mMuted | MUTED_BY_AUDIO_CHANNEL);
    DispatchAsyncEvent(NS_LITERAL_STRING("mozinterruptbegin"));
  } else if (aCanPlay != AUDIO_CHANNEL_STATE_MUTED &&
             (mMuted & MUTED_BY_AUDIO_CHANNEL)) {
    SetMutedInternal(mMuted & ~MUTED_BY_AUDIO_CHANNEL);
    DispatchAsyncEvent(NS_LITERAL_STRING("mozinterruptend"));
  }

  SuspendOrResumeElement(mMuted & MUTED_BY_AUDIO_CHANNEL, false);
  return NS_OK;
}

void HTMLMediaElement::UpdateAudioChannelPlayingState()
{
  if (!UseAudioChannelService()) {
    return;
  }

  bool playingThroughTheAudioChannel =
     (!mPaused &&
      (HasAttr(kNameSpaceID_None, nsGkAtoms::loop) ||
       (mReadyState >= nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA &&
        !IsPlaybackEnded() &&
        (!mSrcStream || HasAudio())) ||
       mPlayingThroughTheAudioChannelBeforeSeek));
  if (playingThroughTheAudioChannel != mPlayingThroughTheAudioChannel) {
    mPlayingThroughTheAudioChannel = playingThroughTheAudioChannel;

    // If we are not playing, we don't need to create a new audioChannelAgent.
    if (!mAudioChannelAgent && !mPlayingThroughTheAudioChannel) {
       return;
    }

    if (!mAudioChannelAgent) {
      nsresult rv;
      mAudioChannelAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1", &rv);
      if (!mAudioChannelAgent) {
        return;
      }
      // Use a weak ref so the audio channel agent can't leak |this|.
      if (AudioChannel::Normal == mAudioChannel && IsVideo()) {
        mAudioChannelAgent->InitWithVideo(OwnerDoc()->GetWindow(),
                                          static_cast<int32_t>(mAudioChannel),
                                          this, true);
      } else {
        mAudioChannelAgent->InitWithWeakCallback(OwnerDoc()->GetWindow(),
                                                 static_cast<int32_t>(mAudioChannel),
                                                 this);
      }
      mAudioChannelAgent->SetVisibilityState(!OwnerDoc()->Hidden());
    }

    // This is needed to pass nsContentUtils::IsCallerChrome().
    // AudioChannel API should not called from content but it can happen that
    // this method has some content JS in its stack.
    AutoNoJSAPI nojsapi;

    if (mPlayingThroughTheAudioChannel) {
      int32_t canPlay;
      mAudioChannelAgent->StartPlaying(&canPlay);
      CanPlayChanged(canPlay);
    } else {
      mAudioChannelAgent->StopPlaying();
      mAudioChannelAgent = nullptr;
    }
  }
}

/* void canPlayChanged (in boolean canPlay); */
NS_IMETHODIMP HTMLMediaElement::CanPlayChanged(int32_t canPlay)
{
  static_assert(static_cast<AudioChannelState>(
                nsIAudioChannelAgent::AUDIO_AGENT_STATE_NORMAL) ==
                AUDIO_CHANNEL_STATE_NORMAL &&
                static_cast<AudioChannelState>(
                nsIAudioChannelAgent::AUDIO_AGENT_STATE_MUTED) ==
                AUDIO_CHANNEL_STATE_MUTED &&
                static_cast<AudioChannelState>(
                nsIAudioChannelAgent::AUDIO_AGENT_STATE_FADED) ==
                AUDIO_CHANNEL_STATE_FADED,
                "Enum of channel state on nsIAudioChannelAgent.idl should be "
                "the same with AudioChannelCommon.h");

  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);

  UpdateChannelMuteState(static_cast<AudioChannelState>(canPlay));
  mPaused.SetCanPlay(canPlay != AUDIO_CHANNEL_STATE_MUTED);
  return NS_OK;
}

#ifdef MOZ_EME
MediaKeys*
HTMLMediaElement::GetMediaKeys() const
{
  return mMediaKeys;
}

bool
HTMLMediaElement::ContainsRestrictedContent()
{
  return GetMediaKeys() != nullptr;
}

already_AddRefed<Promise>
HTMLMediaElement::SetMediaKeys(mozilla::dom::MediaKeys* aMediaKeys,
                               ErrorResult& aRv)
{
  if (MozAudioCaptured()) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(OwnerDoc()->GetInnerWindow());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  nsRefPtr<DetailedPromise> promise = DetailedPromise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  if (mMediaKeys == aMediaKeys) {
    promise->MaybeResolve(JS::UndefinedHandleValue);
    return promise.forget();
  }
  if (aMediaKeys && aMediaKeys->IsBoundToMediaElement()) {
    promise->MaybeReject(NS_ERROR_DOM_QUOTA_EXCEEDED_ERR,
                         NS_LITERAL_CSTRING("MediaKeys object is already bound to another HTMLMediaElement"));
    return promise.forget();
  }
  if (mMediaKeys) {
    // Existing MediaKeys object. Shut it down.
    mMediaKeys->Shutdown();
    mMediaKeys = nullptr;
  }
  if (mDecoder &&
      !mMediaSource &&
      Preferences::GetBool("media.eme.mse-only", true)) {
    ShutdownDecoder();
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                         NS_LITERAL_CSTRING("EME not supported on non-MSE streams"));
    return promise.forget();
  }

  mMediaKeys = aMediaKeys;
  if (mMediaKeys) {
    if (NS_FAILED(mMediaKeys->Bind(this))) {
      promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                           NS_LITERAL_CSTRING("Failed to bind MediaKeys object to HTMLMediaElement"));
      mMediaKeys = nullptr;
      return promise.forget();
    }
    if (mDecoder) {
      mDecoder->SetCDMProxy(mMediaKeys->GetCDMProxy());
    }
  }
  promise->MaybeResolve(JS::UndefinedHandleValue);
  return promise.forget();
}

EventHandlerNonNull*
HTMLMediaElement::GetOnencrypted()
{
  EventListenerManager *elm = GetExistingListenerManager();
  return elm ? elm->GetEventHandler(nsGkAtoms::onencrypted, EmptyString())
              : nullptr;
}

void
HTMLMediaElement::SetOnencrypted(EventHandlerNonNull* handler)
{
  EventListenerManager *elm = GetOrCreateListenerManager();
  if (elm) {
    elm->SetEventHandler(nsGkAtoms::onencrypted, EmptyString(), handler);
  }
}

void
HTMLMediaElement::DispatchEncrypted(const nsTArray<uint8_t>& aInitData,
                                    const nsAString& aInitDataType)
{
  if (mReadyState == nsIDOMHTMLMediaElement::HAVE_NOTHING) {
    // Ready state not HAVE_METADATA (yet), don't dispatch encrypted now.
    // Queueing for later dispatch in MetadataLoaded.
    mPendingEncryptedInitData.AddInitData(aInitDataType, aInitData);
    return;
  }

  nsRefPtr<MediaEncryptedEvent> event;
  if (IsCORSSameOrigin()) {
    event = MediaEncryptedEvent::Constructor(this, aInitDataType, aInitData);
  } else {
    event = MediaEncryptedEvent::Constructor(this);
  }

  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  asyncDispatcher->PostDOMEvent();
}

bool
HTMLMediaElement::IsEventAttributeName(nsIAtom* aName)
{
  return aName == nsGkAtoms::onencrypted ||
         nsGenericHTMLElement::IsEventAttributeName(aName);
}

already_AddRefed<nsIPrincipal>
HTMLMediaElement::GetTopLevelPrincipal()
{
  nsRefPtr<nsIPrincipal> principal;
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(OwnerDoc()->GetParentObject());
  nsCOMPtr<nsIDOMWindow> topWindow;
  if (!window) {
    return nullptr;
  }
  window->GetTop(getter_AddRefs(topWindow));
  nsCOMPtr<nsPIDOMWindow> top = do_QueryInterface(topWindow);
  if (!top) {
    return nullptr;
  }
  nsIDocument* doc = top->GetExtantDoc();
  if (!doc) {
    return nullptr;
  }
  principal = doc->NodePrincipal();
  return principal.forget();
}
#endif // MOZ_EME

NS_IMETHODIMP HTMLMediaElement::WindowVolumeChanged()
{
  SetVolumeInternal();
  return NS_OK;
}

AudioTrackList*
HTMLMediaElement::AudioTracks()
{
  if (!mAudioTrackList) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(OwnerDoc()->GetParentObject());
    mAudioTrackList = new AudioTrackList(window, this);
  }
  return mAudioTrackList;
}

VideoTrackList*
HTMLMediaElement::VideoTracks()
{
  if (!mVideoTrackList) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(OwnerDoc()->GetParentObject());
    mVideoTrackList = new VideoTrackList(window, this);
  }
  return mVideoTrackList;
}

/* readonly attribute TextTrackList? textTracks; */
TextTrackList*
HTMLMediaElement::GetTextTracks()
{
  return GetOrCreateTextTrackManager()->GetTextTracks();
}

already_AddRefed<TextTrack>
HTMLMediaElement::AddTextTrack(TextTrackKind aKind,
                               const nsAString& aLabel,
                               const nsAString& aLanguage)
{
  return
    GetOrCreateTextTrackManager()->AddTextTrack(aKind, aLabel, aLanguage,
                                                TextTrackMode::Hidden,
                                                TextTrackReadyState::Loaded,
                                                TextTrackSource::AddTextTrack);
}

void
HTMLMediaElement::PopulatePendingTextTrackList()
{
  if (mTextTrackManager) {
    mTextTrackManager->PopulatePendingList();
  }
}

TextTrackManager*
HTMLMediaElement::GetOrCreateTextTrackManager()
{
  if (!mTextTrackManager) {
    mTextTrackManager = new TextTrackManager(this);
    mTextTrackManager->AddListeners();
  }
  return mTextTrackManager;
}

void
HTMLMediaElement::SetMozAudioChannelType(AudioChannel aValue, ErrorResult& aRv)
{
  nsString channel;
  channel.AssignASCII(AudioChannelValues::strings[uint32_t(aValue)].value,
                      AudioChannelValues::strings[uint32_t(aValue)].length);
  SetHTMLAttr(nsGkAtoms::mozaudiochannel, channel, aRv);
}

MediaDecoderOwner::NextFrameStatus
HTMLMediaElement::NextFrameStatus()
{
  if (mDecoder) {
    return mDecoder->NextFrameStatus();
  } else if (mMediaStreamListener) {
    return mMediaStreamListener->NextFrameStatus();
  }
  return NEXT_FRAME_UNINITIALIZED;
}

} // namespace dom
} // namespace mozilla
