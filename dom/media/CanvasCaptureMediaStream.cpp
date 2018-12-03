/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasCaptureMediaStream.h"

#include "DOMMediaStream.h"
#include "ImageContainer.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "gfxPlatform.h"
#include "mozilla/Atomics.h"
#include "mozilla/dom/CanvasCaptureMediaStreamBinding.h"
#include "mozilla/gfx/2D.h"
#include "nsContentUtils.h"
#include "Tracing.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

class OutputStreamDriver::TrackListener : public MediaStreamTrackListener {
 public:
  TrackListener(TrackID aTrackId, const PrincipalHandle& aPrincipalHandle,
                SourceMediaStream* aSourceStream)
      : mEnded(false),
        mSourceStream(aSourceStream),
        mTrackId(aTrackId),
        mPrincipalHandle(aPrincipalHandle),
        mMutex("CanvasCaptureMediaStream OutputStreamDriver::StreamListener") {
    MOZ_ASSERT(mSourceStream);
  }

  void Forget() {
    EndTrack();
    mSourceStream->EndTrack(mTrackId);

    MutexAutoLock lock(mMutex);
    mImage = nullptr;
  }

  void EndTrack() { mEnded = true; }

  void SetImage(const RefPtr<layers::Image>& aImage, const TimeStamp& aTime) {
    MutexAutoLock lock(mMutex);
    mImage = aImage;
    mImageTime = aTime;
  }

  void NotifyPull(MediaStreamGraph* aGraph, StreamTime aEndOfAppendedData,
                  StreamTime aDesiredTime) override {
    // Called on the MediaStreamGraph thread.
    TRACE_AUDIO_CALLBACK_COMMENT("SourceMediaStream %p track %i",
                                 mSourceStream.get(), mTrackId);
    MOZ_ASSERT(mSourceStream);
    StreamTime delta = aDesiredTime - aEndOfAppendedData;
    MOZ_ASSERT(delta > 0);

    MutexAutoLock lock(mMutex);

    RefPtr<Image> image = mImage;
    IntSize size = image ? image->GetSize() : IntSize(0, 0);
    VideoSegment segment;
    segment.AppendFrame(image.forget(), delta, size, mPrincipalHandle, false,
                        mImageTime);

    mSourceStream->AppendToTrack(mTrackId, &segment);

    if (mEnded) {
      mSourceStream->EndTrack(mTrackId);
    }
  }

  void NotifyEnded() override {
    Forget();

    mSourceStream->Graph()->DispatchToMainThreadAfterStreamStateUpdate(
        NS_NewRunnableFunction(
            "OutputStreamDriver::TrackListener::RemoveTrackListener",
            [self = RefPtr<TrackListener>(this), this]() {
              if (!mSourceStream->IsDestroyed()) {
                mSourceStream->RemoveTrackListener(this, mTrackId);
              }
            }));
  }

  void NotifyRemoved() override { Forget(); }

 protected:
  ~TrackListener() = default;

 private:
  Atomic<bool> mEnded;
  const RefPtr<SourceMediaStream> mSourceStream;
  const TrackID mTrackId;
  const PrincipalHandle mPrincipalHandle;

  Mutex mMutex;
  // The below members are protected by mMutex.
  RefPtr<layers::Image> mImage;
  TimeStamp mImageTime;
};

OutputStreamDriver::OutputStreamDriver(SourceMediaStream* aSourceStream,
                                       const TrackID& aTrackId,
                                       const PrincipalHandle& aPrincipalHandle)
    : FrameCaptureListener(),
      mSourceStream(aSourceStream),
      mTrackListener(
          new TrackListener(aTrackId, aPrincipalHandle, aSourceStream)) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSourceStream);
  mSourceStream->AddTrack(aTrackId, new VideoSegment());
  mSourceStream->AddTrackListener(mTrackListener, aTrackId);
  mSourceStream->SetPullingEnabled(aTrackId, true);

  // All CanvasCaptureMediaStreams shall at least get one frame.
  mFrameCaptureRequested = true;
}

OutputStreamDriver::~OutputStreamDriver() {
  MOZ_ASSERT(NS_IsMainThread());
  // MediaStreamGraph will keep the listener alive until it can end the track in
  // the graph on the next NotifyPull().
  mTrackListener->EndTrack();
}

void OutputStreamDriver::EndTrack() { mTrackListener->EndTrack(); }

void OutputStreamDriver::SetImage(const RefPtr<layers::Image>& aImage,
                                  const TimeStamp& aTime) {
  mTrackListener->SetImage(aImage, aTime);
}

// ----------------------------------------------------------------------

class TimerDriver : public OutputStreamDriver {
 public:
  explicit TimerDriver(SourceMediaStream* aSourceStream, const double& aFPS,
                       const TrackID& aTrackId,
                       const PrincipalHandle& aPrincipalHandle)
      : OutputStreamDriver(aSourceStream, aTrackId, aPrincipalHandle),
        mFPS(aFPS),
        mTimer(nullptr) {
    if (mFPS == 0.0) {
      return;
    }

    NS_NewTimerWithFuncCallback(
        getter_AddRefs(mTimer), &TimerTick, this, int(1000 / mFPS),
        nsITimer::TYPE_REPEATING_SLACK, "dom::TimerDriver::TimerDriver");
  }

  static void TimerTick(nsITimer* aTimer, void* aClosure) {
    MOZ_ASSERT(aClosure);
    TimerDriver* driver = static_cast<TimerDriver*>(aClosure);

    driver->RequestFrameCapture();
  }

  void NewFrame(already_AddRefed<Image> aImage,
                const TimeStamp& aTime) override {
    RefPtr<Image> image = aImage;

    if (!mFrameCaptureRequested) {
      return;
    }

    mFrameCaptureRequested = false;
    SetImage(image.forget(), aTime);
  }

  void Forget() override {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
  }

 protected:
  virtual ~TimerDriver() {}

 private:
  const double mFPS;
  nsCOMPtr<nsITimer> mTimer;
};

// ----------------------------------------------------------------------

class AutoDriver : public OutputStreamDriver {
 public:
  explicit AutoDriver(SourceMediaStream* aSourceStream, const TrackID& aTrackId,
                      const PrincipalHandle& aPrincipalHandle)
      : OutputStreamDriver(aSourceStream, aTrackId, aPrincipalHandle) {}

  void NewFrame(already_AddRefed<Image> aImage,
                const TimeStamp& aTime) override {
    // Don't reset `mFrameCaptureRequested` since AutoDriver shall always have
    // `mFrameCaptureRequested` set to true.
    // This also means we should accept every frame as NewFrame is called only
    // after something changed.

    RefPtr<Image> image = aImage;
    SetImage(image.forget(), aTime);
  }

 protected:
  virtual ~AutoDriver() {}
};

// ----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_INHERITED(CanvasCaptureMediaStream, DOMMediaStream,
                                   mCanvas)

NS_IMPL_ADDREF_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanvasCaptureMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

CanvasCaptureMediaStream::CanvasCaptureMediaStream(nsPIDOMWindowInner* aWindow,
                                                   HTMLCanvasElement* aCanvas)
    : DOMMediaStream(aWindow), mCanvas(aCanvas), mOutputStreamDriver(nullptr) {}

CanvasCaptureMediaStream::~CanvasCaptureMediaStream() {
  if (mOutputStreamDriver) {
    mOutputStreamDriver->Forget();
  }
}

JSObject* CanvasCaptureMediaStream::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return dom::CanvasCaptureMediaStream_Binding::Wrap(aCx, this, aGivenProto);
}

void CanvasCaptureMediaStream::RequestFrame() {
  if (mOutputStreamDriver) {
    mOutputStreamDriver->RequestFrameCapture();
  }
}

nsresult CanvasCaptureMediaStream::Init(const dom::Optional<double>& aFPS,
                                        const TrackID& aTrackId,
                                        nsIPrincipal* aPrincipal) {
  PrincipalHandle principalHandle = MakePrincipalHandle(aPrincipal);

  if (!aFPS.WasPassed()) {
    mOutputStreamDriver = new AutoDriver(GetInputStream()->AsSourceStream(),
                                         aTrackId, principalHandle);
  } else if (aFPS.Value() < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  } else {
    // Cap frame rate to 60 FPS for sanity
    double fps = std::min(60.0, aFPS.Value());
    mOutputStreamDriver = new TimerDriver(GetInputStream()->AsSourceStream(),
                                          fps, aTrackId, principalHandle);
  }
  return NS_OK;
}

already_AddRefed<CanvasCaptureMediaStream>
CanvasCaptureMediaStream::CreateSourceStream(nsPIDOMWindowInner* aWindow,
                                             HTMLCanvasElement* aCanvas) {
  RefPtr<CanvasCaptureMediaStream> stream =
      new CanvasCaptureMediaStream(aWindow, aCanvas);
  MediaStreamGraph* graph = MediaStreamGraph::GetInstance(
      MediaStreamGraph::SYSTEM_THREAD_DRIVER, aWindow,
      MediaStreamGraph::REQUEST_DEFAULT_SAMPLE_RATE);
  stream->InitSourceStream(graph);
  return stream.forget();
}

FrameCaptureListener* CanvasCaptureMediaStream::FrameCaptureListener() {
  return mOutputStreamDriver;
}

void CanvasCaptureMediaStream::StopCapture() {
  if (!mOutputStreamDriver) {
    return;
  }

  mOutputStreamDriver->EndTrack();
  mOutputStreamDriver->Forget();
  mOutputStreamDriver = nullptr;
}

}  // namespace dom
}  // namespace mozilla
