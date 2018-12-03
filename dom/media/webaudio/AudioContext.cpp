/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioContext.h"

#include "blink/PeriodicWave.h"

#include "mozilla/AutoplayPermissionManager.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/NotNull.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"

#include "mozilla/dom/AnalyserNode.h"
#include "mozilla/dom/AnalyserNodeBinding.h"
#include "mozilla/dom/AudioBufferSourceNodeBinding.h"
#include "mozilla/dom/AudioContextBinding.h"
#include "mozilla/dom/BaseAudioContextBinding.h"
#include "mozilla/dom/BiquadFilterNodeBinding.h"
#include "mozilla/dom/ChannelMergerNodeBinding.h"
#include "mozilla/dom/ChannelSplitterNodeBinding.h"
#include "mozilla/dom/ConvolverNodeBinding.h"
#include "mozilla/dom/DelayNodeBinding.h"
#include "mozilla/dom/DynamicsCompressorNodeBinding.h"
#include "mozilla/dom/GainNodeBinding.h"
#include "mozilla/dom/IIRFilterNodeBinding.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/MediaElementAudioSourceNodeBinding.h"
#include "mozilla/dom/MediaStreamAudioSourceNodeBinding.h"
#include "mozilla/dom/OfflineAudioContextBinding.h"
#include "mozilla/dom/OscillatorNodeBinding.h"
#include "mozilla/dom/PannerNodeBinding.h"
#include "mozilla/dom/PeriodicWaveBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/StereoPannerNodeBinding.h"
#include "mozilla/dom/WaveShaperNodeBinding.h"
#include "mozilla/dom/Worklet.h"

#include "AudioBuffer.h"
#include "AudioBufferSourceNode.h"
#include "AudioChannelService.h"
#include "AudioDestinationNode.h"
#include "AudioListener.h"
#include "AudioNodeStream.h"
#include "AudioStream.h"
#include "AudioWorkletImpl.h"
#include "AutoplayPolicy.h"
#include "BiquadFilterNode.h"
#include "ChannelMergerNode.h"
#include "ChannelSplitterNode.h"
#include "ConstantSourceNode.h"
#include "ConvolverNode.h"
#include "DelayNode.h"
#include "DynamicsCompressorNode.h"
#include "GainNode.h"
#include "IIRFilterNode.h"
#include "MediaElementAudioSourceNode.h"
#include "MediaStreamAudioDestinationNode.h"
#include "MediaStreamAudioSourceNode.h"
#include "MediaStreamGraph.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsRFPService.h"
#include "OscillatorNode.h"
#include "PannerNode.h"
#include "PeriodicWave.h"
#include "ScriptProcessorNode.h"
#include "StereoPannerNode.h"
#include "WaveShaperNode.h"

extern mozilla::LazyLogModule gAutoplayPermissionLog;

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

// 0 is a special value that MediaStreams use to denote they are not part of a
// AudioContext.
static dom::AudioContext::AudioContextId gAudioContextId = 1;

NS_IMPL_CYCLE_COLLECTION_CLASS(AudioContext)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDestination)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromiseGripArray)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingResumePromises)
  if (!tmp->mIsStarted) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mActiveNodes)
  }
  // mDecodeJobs owns the WebAudioDecodeJob objects whose lifetime is managed
  // explicitly. mAllNodes is an array of weak pointers, ignore it here.
  // mBasicWaveFormCache cannot participate in cycles, ignore it here.

  // Remove weak reference on the global window as the context is not usable
  // without mDestination.
  tmp->DisconnectFromWindow();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AudioContext,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDestination)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromiseGripArray)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingResumePromises)
  if (!tmp->mIsStarted) {
    MOZ_ASSERT(tmp->mIsOffline,
               "Online AudioContexts should always be started");
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mActiveNodes)
  }
  // mDecodeJobs owns the WebAudioDecodeJob objects whose lifetime is managed
  // explicitly. mAllNodes is an array of weak pointers, ignore it here.
  // mBasicWaveFormCache cannot participate in cycles, ignore it here.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(AudioContext, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioContext, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioContext)
  NS_INTERFACE_MAP_ENTRY(nsIMemoryReporter)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

static float GetSampleRateForAudioContext(bool aIsOffline, float aSampleRate) {
  if (aIsOffline || aSampleRate != 0.0) {
    return aSampleRate;
  } else {
    return static_cast<float>(CubebUtils::PreferredSampleRate());
  }
}

AudioContext::AudioContext(nsPIDOMWindowInner* aWindow, bool aIsOffline,
                           uint32_t aNumberOfChannels, uint32_t aLength,
                           float aSampleRate)
    : DOMEventTargetHelper(aWindow),
      mId(gAudioContextId++),
      mSampleRate(GetSampleRateForAudioContext(aIsOffline, aSampleRate)),
      mAudioContextState(AudioContextState::Suspended),
      mNumberOfChannels(aNumberOfChannels),
      mIsOffline(aIsOffline),
      mIsStarted(!aIsOffline),
      mIsShutDown(false),
      mCloseCalled(false),
      mSuspendCalled(false),
      mIsDisconnecting(false),
      mWasAllowedToStart(true) {
  bool mute = aWindow->AddAudioContext(this);

  // Note: AudioDestinationNode needs an AudioContext that must already be
  // bound to the window.
  const bool allowedToStart = AutoplayPolicy::IsAllowedToPlay(*this);
  mDestination = new AudioDestinationNode(this, aIsOffline, allowedToStart,
                                          aNumberOfChannels, aLength);

  // The context can't be muted until it has a destination.
  if (mute) {
    Mute();
  }

  // If an AudioContext is not allowed to start, we would postpone its state
  // transition from `suspended` to `running` until sites explicitly call
  // AudioContext.resume() or AudioScheduledSourceNode.start().
  if (!allowedToStart) {
    AUTOPLAY_LOG("AudioContext %p is not allowed to start", this);
    mWasAllowedToStart = false;
    SuspendInternal(nullptr);
    DispatchBlockedEvent();
  }

  FFTBlock::MainThreadInit();
}

void AudioContext::NotifyScheduledSourceNodeStarted() {
  MOZ_ASSERT(NS_IsMainThread());
  // Only try to start AudioContext when AudioContext was not allowed to start.
  if (mWasAllowedToStart) {
    return;
  }

  const bool isAllowedToPlay = AutoplayPolicy::IsAllowedToPlay(*this);
  AUTOPLAY_LOG("Trying to start AudioContext %p, IsAllowedToPlay=%d", this,
               isAllowedToPlay);
  if (isAllowedToPlay) {
    ResumeInternal();
  } else {
    EnsureAutoplayRequested();
  }
}

void AudioContext::EnsureAutoplayRequested() {
  nsPIDOMWindowInner* parent = GetParentObject();
  if (!parent || !parent->AsGlobal()) {
    return;
  }

  RefPtr<AutoplayPermissionManager> request =
      AutoplayPolicy::RequestFor(*(parent->GetExtantDoc()));
  if (!request) {
    return;
  }

  AUTOPLAY_LOG("AudioContext %p EnsureAutoplayRequested %p", this,
               request.get());
  RefPtr<AudioContext> self = this;
  request->RequestWithPrompt()->Then(
      parent->AsGlobal()->AbstractMainThreadFor(TaskCategory::Other), __func__,
      [self, request](bool aApproved) {
        AUTOPLAY_LOG("%p Autoplay request approved request=%p", self.get(),
                     request.get());
        self->mWasAllowedToStart = true;
        self->ResumeInternal();
      },
      [self, request](nsresult aError) {
        AUTOPLAY_LOG("%p Autoplay request denied request=%p", self.get(),
                     request.get());
        self->mWasAllowedToStart = false;
        self->DispatchBlockedEvent();
        nsIDocument* doc = self->GetParentObject()
                               ? self->GetParentObject()->GetExtantDoc()
                               : nullptr;
        nsContentUtils::ReportToConsole(
            nsIScriptError::warningFlag, NS_LITERAL_CSTRING("Media"), doc,
            nsContentUtils::eDOM_PROPERTIES, "BlockAutoplayError");
      });
}

nsresult AudioContext::Init() {
  if (!mIsOffline) {
    nsresult rv = mDestination->CreateAudioChannelAgent();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void AudioContext::DisconnectFromWindow() {
  nsPIDOMWindowInner* window = GetOwner();
  if (window) {
    window->RemoveAudioContext(this);
  }
}

AudioContext::~AudioContext() {
  DisconnectFromWindow();
  UnregisterWeakMemoryReporter(this);
}

JSObject* AudioContext::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  if (mIsOffline) {
    return OfflineAudioContext_Binding::Wrap(aCx, this, aGivenProto);
  } else {
    return AudioContext_Binding::Wrap(aCx, this, aGivenProto);
  }
}

/* static */ already_AddRefed<AudioContext> AudioContext::Constructor(
    const GlobalObject& aGlobal, const AudioContextOptions& aOptions,
    ErrorResult& aRv) {
  // Audio playback is not yet supported when recording or replaying. See bug
  // 1304147.
  if (recordreplay::IsRecordingOrReplaying()) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  float sampleRate = MediaStreamGraph::REQUEST_DEFAULT_SAMPLE_RATE;
  if (Preferences::GetBool(
          "media.webaudio.audiocontextoptions-samplerate.enabled")) {
    if (aOptions.mSampleRate > 0 &&
        (aOptions.mSampleRate - WebAudioUtils::MinSampleRate < 0.0 ||
         WebAudioUtils::MaxSampleRate - aOptions.mSampleRate < 0.0)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
    sampleRate = aOptions.mSampleRate;
  }

  uint32_t maxChannelCount = std::min<uint32_t>(
      WebAudioUtils::MaxChannelCount, CubebUtils::MaxNumberOfChannels());
  RefPtr<AudioContext> object =
      new AudioContext(window, false, maxChannelCount, 0, sampleRate);
  aRv = object->Init();
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RegisterWeakMemoryReporter(object);

  return object.forget();
}

/* static */ already_AddRefed<AudioContext> AudioContext::Constructor(
    const GlobalObject& aGlobal, const OfflineAudioContextOptions& aOptions,
    ErrorResult& aRv) {
  return Constructor(aGlobal, aOptions.mNumberOfChannels, aOptions.mLength,
                     aOptions.mSampleRate, aRv);
}

/* static */ already_AddRefed<AudioContext> AudioContext::Constructor(
    const GlobalObject& aGlobal, uint32_t aNumberOfChannels, uint32_t aLength,
    float aSampleRate, ErrorResult& aRv) {
  // Audio playback is not yet supported when recording or replaying. See bug
  // 1304147.
  if (recordreplay::IsRecordingOrReplaying()) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (aNumberOfChannels == 0 ||
      aNumberOfChannels > WebAudioUtils::MaxChannelCount || aLength == 0 ||
      aSampleRate < WebAudioUtils::MinSampleRate ||
      aSampleRate > WebAudioUtils::MaxSampleRate) {
    // The DOM binding protects us against infinity and NaN
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  RefPtr<AudioContext> object =
      new AudioContext(window, true, aNumberOfChannels, aLength, aSampleRate);

  RegisterWeakMemoryReporter(object);

  return object.forget();
}

bool AudioContext::CheckClosed(ErrorResult& aRv) {
  if (mAudioContextState == AudioContextState::Closed || mIsShutDown ||
      mIsDisconnecting) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return true;
  }
  return false;
}

already_AddRefed<AudioBufferSourceNode> AudioContext::CreateBufferSource(
    ErrorResult& aRv) {
  return AudioBufferSourceNode::Create(nullptr, *this,
                                       AudioBufferSourceOptions(), aRv);
}

already_AddRefed<ConstantSourceNode> AudioContext::CreateConstantSource(
    ErrorResult& aRv) {
  if (CheckClosed(aRv)) {
    return nullptr;
  }

  RefPtr<ConstantSourceNode> constantSourceNode = new ConstantSourceNode(this);
  return constantSourceNode.forget();
}

already_AddRefed<AudioBuffer> AudioContext::CreateBuffer(
    uint32_t aNumberOfChannels, uint32_t aLength, float aSampleRate,
    ErrorResult& aRv) {
  if (!aNumberOfChannels) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  return AudioBuffer::Create(GetOwner(), aNumberOfChannels, aLength,
                             aSampleRate, aRv);
}

namespace {

bool IsValidBufferSize(uint32_t aBufferSize) {
  switch (aBufferSize) {
    case 0:  // let the implementation choose the buffer size
    case 256:
    case 512:
    case 1024:
    case 2048:
    case 4096:
    case 8192:
    case 16384:
      return true;
    default:
      return false;
  }
}

}  // namespace

already_AddRefed<MediaStreamAudioDestinationNode>
AudioContext::CreateMediaStreamDestination(ErrorResult& aRv) {
  return MediaStreamAudioDestinationNode::Create(*this, AudioNodeOptions(),
                                                 aRv);
}

already_AddRefed<ScriptProcessorNode> AudioContext::CreateScriptProcessor(
    uint32_t aBufferSize, uint32_t aNumberOfInputChannels,
    uint32_t aNumberOfOutputChannels, ErrorResult& aRv) {
  if ((aNumberOfInputChannels == 0 && aNumberOfOutputChannels == 0) ||
      aNumberOfInputChannels > WebAudioUtils::MaxChannelCount ||
      aNumberOfOutputChannels > WebAudioUtils::MaxChannelCount ||
      !IsValidBufferSize(aBufferSize)) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  if (CheckClosed(aRv)) {
    return nullptr;
  }

  RefPtr<ScriptProcessorNode> scriptProcessor = new ScriptProcessorNode(
      this, aBufferSize, aNumberOfInputChannels, aNumberOfOutputChannels);
  return scriptProcessor.forget();
}

already_AddRefed<AnalyserNode> AudioContext::CreateAnalyser(ErrorResult& aRv) {
  return AnalyserNode::Create(*this, AnalyserOptions(), aRv);
}

already_AddRefed<StereoPannerNode> AudioContext::CreateStereoPanner(
    ErrorResult& aRv) {
  return StereoPannerNode::Create(*this, StereoPannerOptions(), aRv);
}

already_AddRefed<MediaElementAudioSourceNode>
AudioContext::CreateMediaElementSource(HTMLMediaElement& aMediaElement,
                                       ErrorResult& aRv) {
  MediaElementAudioSourceOptions options;
  options.mMediaElement = aMediaElement;

  return MediaElementAudioSourceNode::Create(*this, options, aRv);
}

already_AddRefed<MediaStreamAudioSourceNode>
AudioContext::CreateMediaStreamSource(DOMMediaStream& aMediaStream,
                                      ErrorResult& aRv) {
  MediaStreamAudioSourceOptions options;
  options.mMediaStream = aMediaStream;

  return MediaStreamAudioSourceNode::Create(*this, options, aRv);
}

already_AddRefed<GainNode> AudioContext::CreateGain(ErrorResult& aRv) {
  return GainNode::Create(*this, GainOptions(), aRv);
}

already_AddRefed<WaveShaperNode> AudioContext::CreateWaveShaper(
    ErrorResult& aRv) {
  return WaveShaperNode::Create(*this, WaveShaperOptions(), aRv);
}

already_AddRefed<DelayNode> AudioContext::CreateDelay(double aMaxDelayTime,
                                                      ErrorResult& aRv) {
  DelayOptions options;
  options.mMaxDelayTime = aMaxDelayTime;
  return DelayNode::Create(*this, options, aRv);
}

already_AddRefed<PannerNode> AudioContext::CreatePanner(ErrorResult& aRv) {
  return PannerNode::Create(*this, PannerOptions(), aRv);
}

already_AddRefed<ConvolverNode> AudioContext::CreateConvolver(
    ErrorResult& aRv) {
  return ConvolverNode::Create(nullptr, *this, ConvolverOptions(), aRv);
}

already_AddRefed<ChannelSplitterNode> AudioContext::CreateChannelSplitter(
    uint32_t aNumberOfOutputs, ErrorResult& aRv) {
  ChannelSplitterOptions options;
  options.mNumberOfOutputs = aNumberOfOutputs;
  return ChannelSplitterNode::Create(*this, options, aRv);
}

already_AddRefed<ChannelMergerNode> AudioContext::CreateChannelMerger(
    uint32_t aNumberOfInputs, ErrorResult& aRv) {
  ChannelMergerOptions options;
  options.mNumberOfInputs = aNumberOfInputs;
  return ChannelMergerNode::Create(*this, options, aRv);
}

already_AddRefed<DynamicsCompressorNode> AudioContext::CreateDynamicsCompressor(
    ErrorResult& aRv) {
  return DynamicsCompressorNode::Create(*this, DynamicsCompressorOptions(),
                                        aRv);
}

already_AddRefed<BiquadFilterNode> AudioContext::CreateBiquadFilter(
    ErrorResult& aRv) {
  return BiquadFilterNode::Create(*this, BiquadFilterOptions(), aRv);
}

already_AddRefed<IIRFilterNode> AudioContext::CreateIIRFilter(
    const Sequence<double>& aFeedforward, const Sequence<double>& aFeedback,
    mozilla::ErrorResult& aRv) {
  IIRFilterOptions options;
  options.mFeedforward = aFeedforward;
  options.mFeedback = aFeedback;
  return IIRFilterNode::Create(*this, options, aRv);
}

already_AddRefed<OscillatorNode> AudioContext::CreateOscillator(
    ErrorResult& aRv) {
  return OscillatorNode::Create(*this, OscillatorOptions(), aRv);
}

already_AddRefed<PeriodicWave> AudioContext::CreatePeriodicWave(
    const Float32Array& aRealData, const Float32Array& aImagData,
    const PeriodicWaveConstraints& aConstraints, ErrorResult& aRv) {
  aRealData.ComputeLengthAndData();
  aImagData.ComputeLengthAndData();

  if (aRealData.Length() != aImagData.Length() || aRealData.Length() == 0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  RefPtr<PeriodicWave> periodicWave = new PeriodicWave(
      this, aRealData.Data(), aImagData.Data(), aImagData.Length(),
      aConstraints.mDisableNormalization, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return periodicWave.forget();
}

AudioListener* AudioContext::Listener() {
  if (!mListener) {
    mListener = new AudioListener(this);
  }
  return mListener;
}

Worklet* AudioContext::GetAudioWorklet(ErrorResult& aRv) {
  if (!mWorklet) {
    mWorklet = AudioWorkletImpl::CreateWorklet(this, aRv);
  }

  return mWorklet;
}

bool AudioContext::IsRunning() const {
  return mAudioContextState == AudioContextState::Running;
}

already_AddRefed<Promise> AudioContext::DecodeAudioData(
    const ArrayBuffer& aBuffer,
    const Optional<OwningNonNull<DecodeSuccessCallback> >& aSuccessCallback,
    const Optional<OwningNonNull<DecodeErrorCallback> >& aFailureCallback,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise;
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> obj(cx, js::CheckedUnwrap(aBuffer.Obj()));
  if (!obj) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  JSAutoRealm ar(cx, obj);

  promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  aBuffer.ComputeLengthAndData();

  if (aBuffer.IsShared()) {
    // Throw if the object is mapping shared memory (must opt in).
    aRv.ThrowTypeError<MSG_TYPEDARRAY_IS_SHARED>(
        NS_LITERAL_STRING("Argument of AudioContext.decodeAudioData"));
    return nullptr;
  }

  if (!aBuffer.Data()) {
    // Throw if the buffer is detached
    aRv.ThrowTypeError<MSG_TYPEDARRAY_IS_DETACHED>(
        NS_LITERAL_STRING("Argument of AudioContext.decodeAudioData"));
    return nullptr;
  }

  // Detach the array buffer
  size_t length = aBuffer.Length();

  uint8_t* data = static_cast<uint8_t*>(JS_StealArrayBufferContents(cx, obj));

  // Sniff the content of the media.
  // Failed type sniffing will be handled by AsyncDecodeWebAudio.
  nsAutoCString contentType;
  NS_SniffContent(NS_DATA_SNIFFER_CATEGORY, nullptr, data, length, contentType);

  RefPtr<DecodeErrorCallback> failureCallback;
  RefPtr<DecodeSuccessCallback> successCallback;
  if (aFailureCallback.WasPassed()) {
    failureCallback = &aFailureCallback.Value();
  }
  if (aSuccessCallback.WasPassed()) {
    successCallback = &aSuccessCallback.Value();
  }
  UniquePtr<WebAudioDecodeJob> job(
      new WebAudioDecodeJob(this, promise, successCallback, failureCallback));
  AsyncDecodeWebAudio(contentType.get(), data, length, *job);
  // Transfer the ownership to mDecodeJobs
  mDecodeJobs.AppendElement(std::move(job));

  return promise.forget();
}

void AudioContext::RemoveFromDecodeQueue(WebAudioDecodeJob* aDecodeJob) {
  // Since UniquePtr doesn't provide an operator== which allows you to compare
  // against raw pointers, we need to iterate manually.
  for (uint32_t i = 0; i < mDecodeJobs.Length(); ++i) {
    if (mDecodeJobs[i].get() == aDecodeJob) {
      mDecodeJobs.RemoveElementAt(i);
      break;
    }
  }
}

void AudioContext::RegisterActiveNode(AudioNode* aNode) {
  if (!mIsShutDown) {
    mActiveNodes.PutEntry(aNode);
  }
}

void AudioContext::UnregisterActiveNode(AudioNode* aNode) {
  mActiveNodes.RemoveEntry(aNode);
}

uint32_t AudioContext::MaxChannelCount() const {
  return std::min<uint32_t>(
      WebAudioUtils::MaxChannelCount,
      mIsOffline ? mNumberOfChannels : CubebUtils::MaxNumberOfChannels());
}

uint32_t AudioContext::ActiveNodeCount() const { return mActiveNodes.Count(); }

MediaStreamGraph* AudioContext::Graph() const {
  return Destination()->Stream()->Graph();
}

AudioNodeStream* AudioContext::DestinationStream() const {
  if (Destination()) {
    return Destination()->Stream();
  }
  return nullptr;
}

double AudioContext::CurrentTime() {
  MediaStream* stream = Destination()->Stream();

  double rawTime = stream->StreamTimeToSeconds(stream->GetCurrentTime());

  // CurrentTime increments in intervals of 128/sampleRate. If the Timer
  // Precision Reduction is smaller than this interval, the jittered time
  // can always be reversed to the raw step of the interval. In that case
  // we can simply return the un-reduced time; and avoid breaking tests.
  // We have to convert each variable into a common magnitude, we choose ms.
  if ((128 / mSampleRate) * 1000.0 > nsRFPService::TimerResolution() / 1000.0) {
    return rawTime;
  }

  // The value of a MediaStream's CurrentTime will always advance forward; it
  // will never reset (even if one rewinds a video.) Therefore we can use a
  // single Random Seed initialized at the same time as the object.
  return nsRFPService::ReduceTimePrecisionAsSecs(rawTime,
                                                 GetRandomTimelineSeed());
}

void AudioContext::DisconnectFromOwner() {
  mIsDisconnecting = true;
  Shutdown();
  DOMEventTargetHelper::DisconnectFromOwner();
}

void AudioContext::BindToOwner(nsIGlobalObject* aNew) {
  auto scopeExit =
      MakeScopeExit([&] { DOMEventTargetHelper::BindToOwner(aNew); });

  if (GetOwner()) {
    GetOwner()->RemoveAudioContext(this);
  }

  nsCOMPtr<nsPIDOMWindowInner> newWindow = do_QueryInterface(aNew);
  if (newWindow) {
    newWindow->AddAudioContext(this);
  }
}

void AudioContext::Shutdown() {
  mIsShutDown = true;

  // We don't want to touch promises if the global is going away soon.
  if (!mIsDisconnecting) {
    if (!mIsOffline) {
      RefPtr<Promise> ignored = Close(IgnoreErrors());
    }

    for (auto p : mPromiseGripArray) {
      p->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    mPromiseGripArray.Clear();

    for (const auto& p : mPendingResumePromises) {
      p->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    }
    mPendingResumePromises.Clear();
  }

  // Release references to active nodes.
  // Active AudioNodes don't unregister in destructors, at which point the
  // Node is already unregistered.
  mActiveNodes.Clear();

  // For offline contexts, we can destroy the MediaStreamGraph at this point.
  if (mIsOffline && mDestination) {
    mDestination->OfflineShutdown();
  }
}

StateChangeTask::StateChangeTask(AudioContext* aAudioContext, void* aPromise,
                                 AudioContextState aNewState)
    : Runnable("dom::StateChangeTask"),
      mAudioContext(aAudioContext),
      mPromise(aPromise),
      mAudioNodeStream(nullptr),
      mNewState(aNewState) {
  MOZ_ASSERT(NS_IsMainThread(),
             "This constructor should be used from the main thread.");
}

StateChangeTask::StateChangeTask(AudioNodeStream* aStream, void* aPromise,
                                 AudioContextState aNewState)
    : Runnable("dom::StateChangeTask"),
      mAudioContext(nullptr),
      mPromise(aPromise),
      mAudioNodeStream(aStream),
      mNewState(aNewState) {
  MOZ_ASSERT(!NS_IsMainThread(),
             "This constructor should be used from the graph thread.");
}

NS_IMETHODIMP
StateChangeTask::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mAudioContext && !mAudioNodeStream) {
    return NS_OK;
  }
  if (mAudioNodeStream) {
    AudioNode* node = mAudioNodeStream->Engine()->NodeMainThread();
    if (!node) {
      return NS_OK;
    }
    mAudioContext = node->Context();
    if (!mAudioContext) {
      return NS_OK;
    }
  }

  mAudioContext->OnStateChanged(mPromise, mNewState);
  // We have can't call Release() on the AudioContext on the MSG thread, so we
  // unref it here, on the main thread.
  mAudioContext = nullptr;

  return NS_OK;
}

/* This runnable allows to fire the "statechange" event */
class OnStateChangeTask final : public Runnable {
 public:
  explicit OnStateChangeTask(AudioContext* aAudioContext)
      : Runnable("dom::OnStateChangeTask"), mAudioContext(aAudioContext) {}

  NS_IMETHODIMP
  Run() override {
    nsPIDOMWindowInner* parent = mAudioContext->GetParentObject();
    if (!parent) {
      return NS_ERROR_FAILURE;
    }

    nsIDocument* doc = parent->GetExtantDoc();
    if (!doc) {
      return NS_ERROR_FAILURE;
    }

    return nsContentUtils::DispatchTrustedEvent(
        doc, static_cast<DOMEventTargetHelper*>(mAudioContext),
        NS_LITERAL_STRING("statechange"), CanBubble::eNo, Cancelable::eNo);
  }

 private:
  RefPtr<AudioContext> mAudioContext;
};

void AudioContext::Dispatch(already_AddRefed<nsIRunnable>&& aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  // It can happen that this runnable took a long time to reach the main thread,
  // and the global is not valid anymore.
  if (parentObject) {
    parentObject->AbstractMainThreadFor(TaskCategory::Other)
        ->Dispatch(std::move(aRunnable));
  } else {
    RefPtr<nsIRunnable> runnable(aRunnable);
    runnable = nullptr;
  }
}

void AudioContext::OnStateChanged(void* aPromise, AudioContextState aNewState) {
  MOZ_ASSERT(NS_IsMainThread());

  // This can happen if close() was called right after creating the
  // AudioContext, before the context has switched to "running".
  if (mAudioContextState == AudioContextState::Closed &&
      aNewState == AudioContextState::Running && !aPromise) {
    return;
  }

  // This can happen if this is called in reaction to a
  // MediaStreamGraph shutdown, and a AudioContext was being
  // suspended at the same time, for example if a page was being
  // closed.
  if (mAudioContextState == AudioContextState::Closed &&
      aNewState == AudioContextState::Suspended) {
    return;
  }

#ifndef WIN32  // Bug 1170547
#ifndef XP_MACOSX
#ifdef DEBUG

  if (!((mAudioContextState == AudioContextState::Suspended &&
         aNewState == AudioContextState::Running) ||
        (mAudioContextState == AudioContextState::Running &&
         aNewState == AudioContextState::Suspended) ||
        (mAudioContextState == AudioContextState::Running &&
         aNewState == AudioContextState::Closed) ||
        (mAudioContextState == AudioContextState::Suspended &&
         aNewState == AudioContextState::Closed) ||
        (mAudioContextState == aNewState))) {
    fprintf(stderr,
            "Invalid transition: mAudioContextState: %d -> aNewState %d\n",
            static_cast<int>(mAudioContextState), static_cast<int>(aNewState));
    MOZ_ASSERT(false);
  }

#endif  // DEBUG
#endif  // XP_MACOSX
#endif  // WIN32

  if (aPromise) {
    Promise* promise = reinterpret_cast<Promise*>(aPromise);
    // It is possible for the promise to have been removed from
    // mPromiseGripArray if the cycle collector has severed our connections. DO
    // NOT dereference the promise pointer in that case since it may point to
    // already freed memory.
    if (mPromiseGripArray.Contains(promise)) {
      promise->MaybeResolveWithUndefined();
      DebugOnly<bool> rv = mPromiseGripArray.RemoveElement(promise);
      MOZ_ASSERT(rv, "Promise wasn't in the grip array?");
    }
  }

  // Resolve all pending promises once the audio context has been allowed to
  // start.
  if (aNewState == AudioContextState::Running) {
    for (const auto& p : mPendingResumePromises) {
      p->MaybeResolveWithUndefined();
    }
    mPendingResumePromises.Clear();
  }

  if (mAudioContextState != aNewState) {
    RefPtr<OnStateChangeTask> task = new OnStateChangeTask(this);
    Dispatch(task.forget());
  }

  mAudioContextState = aNewState;
}

nsTArray<MediaStream*> AudioContext::GetAllStreams() const {
  nsTArray<MediaStream*> streams;
  for (auto iter = mAllNodes.ConstIter(); !iter.Done(); iter.Next()) {
    MediaStream* s = iter.Get()->GetKey()->GetStream();
    if (s) {
      streams.AppendElement(s);
    }
  }
  return streams;
}

already_AddRefed<Promise> AudioContext::Suspend(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise;
  promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  if (mIsOffline) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return promise.forget();
  }

  if (mAudioContextState == AudioContextState::Closed || mCloseCalled) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  mPromiseGripArray.AppendElement(promise);
  SuspendInternal(promise);
  return promise.forget();
}

void AudioContext::SuspendInternal(void* aPromise) {
  Destination()->Suspend();

  nsTArray<MediaStream*> streams;
  // If mSuspendCalled is true then we already suspended all our streams,
  // so don't suspend them again (since suspend(); suspend(); resume(); should
  // cancel both suspends). But we still need to do ApplyAudioContextOperation
  // to ensure our new promise is resolved.
  if (!mSuspendCalled) {
    streams = GetAllStreams();
  }
  Graph()->ApplyAudioContextOperation(DestinationStream(), streams,
                                      AudioContextOperation::Suspend, aPromise);

  mSuspendCalled = true;
}

already_AddRefed<Promise> AudioContext::Resume(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise;
  promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mIsOffline) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return promise.forget();
  }

  if (mAudioContextState == AudioContextState::Closed || mCloseCalled) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  mPendingResumePromises.AppendElement(promise);

  const bool isAllowedToPlay = AutoplayPolicy::IsAllowedToPlay(*this);
  AUTOPLAY_LOG("Trying to resume AudioContext %p, IsAllowedToPlay=%d", this,
               isAllowedToPlay);
  if (isAllowedToPlay) {
    mWasAllowedToStart = true;
    ResumeInternal();
  } else if (!isAllowedToPlay && !mWasAllowedToStart) {
    EnsureAutoplayRequested();
  }

  return promise.forget();
}

void AudioContext::ResumeInternal() {
  AUTOPLAY_LOG("Allow to resume AudioContext %p", this);
  Destination()->Resume();

  nsTArray<MediaStream*> streams;
  // If mSuspendCalled is false then we already resumed all our streams,
  // so don't resume them again (since suspend(); resume(); resume(); should
  // be OK). But we still need to do ApplyAudioContextOperation
  // to ensure our new promise is resolved.
  if (mSuspendCalled) {
    streams = GetAllStreams();
  }
  Graph()->ApplyAudioContextOperation(DestinationStream(), streams,
                                      AudioContextOperation::Resume, nullptr);
  mSuspendCalled = false;
}

void AudioContext::DispatchBlockedEvent() {
  if (!StaticPrefs::MediaBlockEventEnabled()) {
    return;
  }

  RefPtr<AudioContext> self = this;
  RefPtr<nsIRunnable> r =
      NS_NewRunnableFunction("AudioContext::AutoplayBlocked", [self]() {
        nsPIDOMWindowInner* parent = self->GetParentObject();
        if (!parent) {
          return;
        }

        nsIDocument* doc = parent->GetExtantDoc();
        if (!doc) {
          return;
        }

        AUTOPLAY_LOG("Dispatch `blocked` event for AudioContext %p",
                     self.get());
        nsContentUtils::DispatchTrustedEvent(
            doc, static_cast<DOMEventTargetHelper*>(self),
            NS_LITERAL_STRING("blocked"), CanBubble::eNo, Cancelable::eNo);
      });
  Dispatch(r.forget());
}

already_AddRefed<Promise> AudioContext::Close(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise;
  promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mIsOffline) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return promise.forget();
  }

  if (mAudioContextState == AudioContextState::Closed) {
    promise->MaybeResolve(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  if (Destination()) {
    Destination()->DestroyAudioChannelAgent();
  }

  mPromiseGripArray.AppendElement(promise);

  // This can be called when freeing a document, and the streams are dead at
  // this point, so we need extra null-checks.
  AudioNodeStream* ds = DestinationStream();
  if (ds) {
    nsTArray<MediaStream*> streams;
    // If mSuspendCalled or mCloseCalled are true then we already suspended
    // all our streams, so don't suspend them again. But we still need to do
    // ApplyAudioContextOperation to ensure our new promise is resolved.
    if (!mSuspendCalled && !mCloseCalled) {
      streams = GetAllStreams();
    }
    Graph()->ApplyAudioContextOperation(ds, streams,
                                        AudioContextOperation::Close, promise);
  }
  mCloseCalled = true;

  return promise.forget();
}

void AudioContext::RegisterNode(AudioNode* aNode) {
  MOZ_ASSERT(!mAllNodes.Contains(aNode));
  mAllNodes.PutEntry(aNode);
}

void AudioContext::UnregisterNode(AudioNode* aNode) {
  MOZ_ASSERT(mAllNodes.Contains(aNode));
  mAllNodes.RemoveEntry(aNode);
}

JSObject* AudioContext::GetGlobalJSObject() const {
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  if (!parentObject) {
    return nullptr;
  }

  // This can also return null.
  return parentObject->GetGlobalJSObject();
}

already_AddRefed<Promise> AudioContext::StartRendering(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());

  MOZ_ASSERT(mIsOffline, "This should only be called on OfflineAudioContext");
  if (mIsStarted) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  mIsStarted = true;
  RefPtr<Promise> promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  mDestination->StartRendering(promise);

  OnStateChanged(nullptr, AudioContextState::Running);

  return promise.forget();
}

unsigned long AudioContext::Length() {
  MOZ_ASSERT(mIsOffline);
  return mDestination->Length();
}

void AudioContext::Mute() const {
  MOZ_ASSERT(!mIsOffline);
  if (mDestination) {
    mDestination->Mute();
  }
}

void AudioContext::Unmute() const {
  MOZ_ASSERT(!mIsOffline);
  if (mDestination) {
    mDestination->Unmute();
  }
}

size_t AudioContext::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  // AudioNodes are tracked separately because we do not want the AudioContext
  // to track all of the AudioNodes it creates, so we wouldn't be able to
  // traverse them from here.

  size_t amount = aMallocSizeOf(this);
  if (mListener) {
    amount += mListener->SizeOfIncludingThis(aMallocSizeOf);
  }
  amount += mDecodeJobs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < mDecodeJobs.Length(); ++i) {
    amount += mDecodeJobs[i]->SizeOfIncludingThis(aMallocSizeOf);
  }
  amount += mActiveNodes.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

NS_IMETHODIMP
AudioContext::CollectReports(nsIHandleReportCallback* aHandleReport,
                             nsISupports* aData, bool aAnonymize) {
  const nsLiteralCString nodeDescription(
      "Memory used by AudioNode DOM objects (Web Audio).");
  for (auto iter = mAllNodes.ConstIter(); !iter.Done(); iter.Next()) {
    AudioNode* node = iter.Get()->GetKey();
    int64_t amount = node->SizeOfIncludingThis(MallocSizeOf);
    nsPrintfCString domNodePath("explicit/webaudio/audio-node/%s/dom-nodes",
                                node->NodeType());
    aHandleReport->Callback(EmptyCString(), domNodePath, KIND_HEAP, UNITS_BYTES,
                            amount, nodeDescription, aData);
  }

  int64_t amount = SizeOfIncludingThis(MallocSizeOf);
  MOZ_COLLECT_REPORT("explicit/webaudio/audiocontext", KIND_HEAP, UNITS_BYTES,
                     amount,
                     "Memory used by AudioContext objects (Web Audio).");

  return NS_OK;
}

BasicWaveFormCache* AudioContext::GetBasicWaveFormCache() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mBasicWaveFormCache) {
    mBasicWaveFormCache = new BasicWaveFormCache(SampleRate());
  }
  return mBasicWaveFormCache;
}

BasicWaveFormCache::BasicWaveFormCache(uint32_t aSampleRate)
    : mSampleRate(aSampleRate) {
  MOZ_ASSERT(NS_IsMainThread());
}
BasicWaveFormCache::~BasicWaveFormCache() {}

WebCore::PeriodicWave* BasicWaveFormCache::GetBasicWaveForm(
    OscillatorType aType) {
  MOZ_ASSERT(!NS_IsMainThread());
  if (aType == OscillatorType::Sawtooth) {
    if (!mSawtooth) {
      mSawtooth = WebCore::PeriodicWave::createSawtooth(mSampleRate);
    }
    return mSawtooth;
  } else if (aType == OscillatorType::Square) {
    if (!mSquare) {
      mSquare = WebCore::PeriodicWave::createSquare(mSampleRate);
    }
    return mSquare;
  } else if (aType == OscillatorType::Triangle) {
    if (!mTriangle) {
      mTriangle = WebCore::PeriodicWave::createTriangle(mSampleRate);
    }
    return mTriangle;
  } else {
    MOZ_ASSERT(false, "Not reached");
    return nullptr;
  }
}

}  // namespace dom
}  // namespace mozilla
