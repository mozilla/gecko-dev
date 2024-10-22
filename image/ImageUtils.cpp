/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/image/ImageUtils.h"
#include "DecodePool.h"
#include "Decoder.h"
#include "DecoderFactory.h"
#include "IDecodingTask.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Logging.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"

namespace mozilla::image {

static LazyLogModule sLog("ImageUtils");

AnonymousDecoder::AnonymousDecoder() = default;

AnonymousDecoder::~AnonymousDecoder() = default;

class AnonymousDecoderTask : public IDecodingTask {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AnonymousDecoderTask, final)

  AnonymousDecoderTask(RefPtr<Decoder>&& aDecoder,
                       ThreadSafeWeakPtr<AnonymousDecoder>&& aOwner)
      : mDecoder(std::move(aDecoder)), mOwner(std::move(aOwner)) {}

  bool ShouldPreferSyncRun() const final { return false; }

  TaskPriority Priority() const final { return TaskPriority::eLow; }

  bool IsValid() const {
    return !AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownFinal) &&
           !mOwner.IsDead();
  }

  bool MaybeStart() {
    if (!IsValid()) {
      return false;
    }

    MOZ_LOG(sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderTask::Start -- queue", this));
    DecodePool::Singleton()->AsyncRun(this);
    return true;
  }

  void Resume() final {
    if (!IsValid()) {
      return;
    }

    MOZ_LOG(sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderTask::Resume -- queue", this));
    DecodePool::Singleton()->AsyncRun(this);
  }

  void Run() final {
    bool resume = true;
    while (!mOwner.IsDead() && resume) {
      LexerResult result = mDecoder->Decode(WrapNotNull(this));
      if (result == LexerResult(Yield::NEED_MORE_DATA)) {
        MOZ_LOG(sLog, LogLevel::Debug,
                ("[%p] AnonymousDecoderTask::Run -- need more data", this));
        MOZ_ASSERT(result == LexerResult(Yield::NEED_MORE_DATA));
        OnNeedMoreData();
        return;
      }

      // Check if we have a new frame to process.
      RefPtr<imgFrame> frame = mDecoder->GetCurrentFrame();
      if (frame) {
        RefPtr<gfx::SourceSurface> surface = frame->GetSourceSurface();
        if (surface) {
          MOZ_LOG(sLog, LogLevel::Debug,
                  ("[%p] AnonymousDecoderTask::Run -- new frame %p", this,
                   frame.get()));
          resume = OnFrameAvailable(std::move(frame), std::move(surface));
        } else {
          MOZ_ASSERT_UNREACHABLE("No surface from frame?");
        }
      }

      if (result.is<TerminalState>()) {
        MOZ_LOG(sLog, LogLevel::Debug,
                ("[%p] AnonymousDecoderTask::Run -- complete", this));
        OnComplete(result == LexerResult(TerminalState::SUCCESS));
        break;
      }

      MOZ_ASSERT(result == LexerResult(Yield::OUTPUT_AVAILABLE));
    }
  }

 protected:
  virtual ~AnonymousDecoderTask() = default;

  virtual void OnNeedMoreData() {}

  // Returns true if the caller should continue decoding more frames if
  // possible.
  virtual bool OnFrameAvailable(RefPtr<imgFrame>&& aFrame,
                                RefPtr<gfx::SourceSurface>&& aSurface) {
    MOZ_ASSERT_UNREACHABLE("Unhandled frame!");
    return true;
  }

  virtual void OnComplete(bool aSuccess) = 0;

  RefPtr<Decoder> mDecoder;
  ThreadSafeWeakPtr<AnonymousDecoder> mOwner;
};

class AnonymousMetadataDecoderTask final : public AnonymousDecoderTask {
 public:
  AnonymousMetadataDecoderTask(RefPtr<Decoder>&& aDecoder,
                               ThreadSafeWeakPtr<AnonymousDecoder>&& aOwner)
      : AnonymousDecoderTask(std::move(aDecoder), std::move(aOwner)) {}

 protected:
  void OnComplete(bool aSuccess) override {
    RefPtr<AnonymousDecoder> owner(mOwner);
    if (!owner) {
      return;
    }

    if (!aSuccess) {
      owner->OnMetadata(nullptr);
      return;
    }

    const auto& mdIn = mDecoder->GetImageMetadata();
    owner->OnMetadata(&mdIn);
  }
};

class AnonymousFrameCountDecoderTask final : public AnonymousDecoderTask {
 public:
  AnonymousFrameCountDecoderTask(RefPtr<Decoder>&& aDecoder,
                                 ThreadSafeWeakPtr<AnonymousDecoder>&& aOwner)
      : AnonymousDecoderTask(std::move(aDecoder), std::move(aOwner)) {}

 protected:
  void UpdateFrameCount(bool aComplete) {
    RefPtr<AnonymousDecoder> owner(mOwner);
    if (!owner) {
      return;
    }

    const auto& mdIn = mDecoder->GetImageMetadata();
    uint32_t frameCount = mdIn.HasFrameCount() ? mdIn.GetFrameCount() : 0;
    owner->OnFrameCount(frameCount, aComplete);
  }

  void OnNeedMoreData() override { UpdateFrameCount(/* aComplete */ false); }

  void OnComplete(bool aSuccess) override {
    UpdateFrameCount(/* aComplete */ true);
  }
};

class AnonymousFramesDecoderTask final : public AnonymousDecoderTask {
 public:
  AnonymousFramesDecoderTask(RefPtr<Decoder>&& aDecoder,
                             ThreadSafeWeakPtr<AnonymousDecoder>&& aOwner)
      : AnonymousDecoderTask(std::move(aDecoder), std::move(aOwner)) {}

  void SetOutputSize(const OrientedIntSize& aSize) {
    if (mDecoder) {
      mDecoder->SetOutputSize(aSize);
    }
  }

 protected:
  bool OnFrameAvailable(RefPtr<imgFrame>&& aFrame,
                        RefPtr<gfx::SourceSurface>&& aSurface) override {
    RefPtr<AnonymousDecoder> owner(mOwner);
    if (!owner) {
      return false;
    }

    return owner->OnFrameAvailable(std::move(aFrame), std::move(aSurface));
  }

  void OnComplete(bool aSuccess) override {
    RefPtr<AnonymousDecoder> owner(mOwner);
    if (!owner) {
      return;
    }

    owner->OnFramesComplete();
  }
};

class AnonymousDecoderImpl final : public AnonymousDecoder {
 public:
  explicit AnonymousDecoderImpl(const Maybe<gfx::IntSize>& aOutputSize)
      : mMutex("mozilla::image::AnonymousDecoderImpl::mMutex"),
        mOutputSize(aOutputSize) {}

  ~AnonymousDecoderImpl() override { Destroy(); }

#ifdef MOZ_REFCOUNTED_LEAK_CHECKING
  const char* typeName() const override {
    return "mozilla::image::AnonymousDecoderImpl";
  }

  size_t typeSize() const override { return sizeof(*this); }
#endif

  bool Initialize(RefPtr<Decoder>&& aDecoder) override {
    MutexAutoLock lock(mMutex);

    if (NS_WARN_IF(!aDecoder)) {
      MOZ_LOG(sLog, LogLevel::Error,
              ("[%p] AnonymousDecoderImpl::Initialize -- bad decoder", this));
      return false;
    }

    RefPtr<Decoder> metadataDecoder =
        DecoderFactory::CloneAnonymousMetadataDecoder(aDecoder);
    if (NS_WARN_IF(!metadataDecoder)) {
      MOZ_LOG(sLog, LogLevel::Error,
              ("[%p] AnonymousDecoderImpl::Initialize -- failed clone metadata "
               "decoder",
               this));
      return false;
    }

    DecoderFlags flags =
        aDecoder->GetDecoderFlags() | DecoderFlags::COUNT_FRAMES;
    RefPtr<Decoder> frameCountDecoder =
        DecoderFactory::CloneAnonymousMetadataDecoder(aDecoder, Some(flags));
    if (NS_WARN_IF(!frameCountDecoder)) {
      MOZ_LOG(sLog, LogLevel::Error,
              ("[%p] AnonymousDecoderImpl::Initialize -- failed clone frame "
               "count decoder",
               this));
      return false;
    }

    mMetadataTask = new AnonymousMetadataDecoderTask(
        std::move(metadataDecoder), ThreadSafeWeakPtr<AnonymousDecoder>(this));
    mFrameCountTask = new AnonymousFrameCountDecoderTask(
        std::move(frameCountDecoder),
        ThreadSafeWeakPtr<AnonymousDecoder>(this));
    mFramesTask = new AnonymousFramesDecoderTask(
        std::move(aDecoder), ThreadSafeWeakPtr<AnonymousDecoder>(this));

    MOZ_LOG(sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderImpl::Initialize -- success", this));
    return true;
  }

  void Destroy() override {
    MutexAutoLock lock(mMutex);
    DestroyLocked(NS_ERROR_ABORT);
  }

  void DestroyLocked(nsresult aResult) MOZ_REQUIRES(mMutex) {
    MOZ_LOG(sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderImpl::Destroy", this));

    mFramesToDecode = 0;
    mMetadataTask = nullptr;
    mFrameCountTask = nullptr;
    mFramesTask = nullptr;
    mPendingFramesResult.mFrames.Clear();
    mPendingFramesResult.mFinished = true;
    mMetadataPromise.RejectIfExists(aResult, __func__);
    mFrameCountPromise.RejectIfExists(aResult, __func__);
    mFramesPromise.RejectIfExists(aResult, __func__);
  }

  void OnMetadata(const ImageMetadata* aMetadata) override {
    MutexAutoLock lock(mMutex);

    // We must have already gotten destroyed before metadata decoding finished.
    if (!mMetadataTask) {
      return;
    }

    if (!aMetadata) {
      MOZ_LOG(sLog, LogLevel::Error,
              ("[%p] AnonymousDecoderImpl::OnMetadata -- failed", this));
      DestroyLocked(NS_ERROR_FAILURE);
      return;
    }

    const auto size = aMetadata->GetSize();
    mMetadataResult.mWidth = size.width;
    mMetadataResult.mHeight = size.height;
    mMetadataResult.mRepetitions = aMetadata->GetLoopCount();
    mMetadataResult.mAnimated = aMetadata->HasAnimation();

    MOZ_LOG(sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderImpl::OnMetadata -- %dx%d, repetitions %d, "
             "animated %d",
             this, size.width, size.height, mMetadataResult.mRepetitions,
             mMetadataResult.mAnimated));

    if (mOutputSize && !mMetadataResult.mAnimated && mFramesTask) {
      if (mOutputSize->width <= size.width &&
          mOutputSize->height <= size.height) {
        MOZ_LOG(
            sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderImpl::OnMetadata -- use output size %dx%d",
             this, mOutputSize->width, mOutputSize->height));
        mFramesTask->SetOutputSize(
            OrientedIntSize::FromUnknownSize(*mOutputSize));
      } else {
        MOZ_LOG(sLog, LogLevel::Debug,
                ("[%p] AnonymousDecoderImpl::OnMetadata -- cannot use output "
                 "size %dx%d, exceeds metadata size",
                 this, mOutputSize->width, mOutputSize->height));
      }
    }

    if (!mMetadataResult.mAnimated) {
      mMetadataResult.mFrameCount = 1;
      mMetadataResult.mFrameCountComplete = true;
      mMetadataTask = nullptr;
      mFrameCountTask = nullptr;
    } else if (mFrameCountTask && !mFrameCountTaskRunning) {
      MOZ_LOG(
          sLog, LogLevel::Debug,
          ("[%p] AnonymousDecoderImpl::OnMetadata -- start frame count task",
           this));
      mFrameCountTaskRunning = mFrameCountTask->MaybeStart();
      return;
    }

    mMetadataPromise.Resolve(mMetadataResult, __func__);

    if (mFramesTask && mFramesToDecode > 0 && !mFramesTaskRunning) {
      MOZ_LOG(sLog, LogLevel::Debug,
              ("[%p] AnonymousDecoderImpl::OnMetadata -- start frames task, "
               "want %zu",
               this, mFramesToDecode));
      mFramesTaskRunning = mFramesTask->MaybeStart();
    }
  }

  void OnFrameCount(uint32_t aFrameCount, bool aComplete) override {
    MutexAutoLock lock(mMutex);

    // We must have already gotten destroyed before frame count decoding
    // finished.
    if (!mFrameCountTask) {
      return;
    }

    MOZ_LOG(sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderImpl::OnFrameCount -- frameCount %u, "
             "complete %d",
             this, aFrameCount, aComplete));

    bool resolve = aComplete;
    if (mFrameCount < aFrameCount) {
      mFrameCount = aFrameCount;
      resolve = true;
    }

    // If metadata completing is waiting on an updated frame count, resolve it.
    mMetadataResult.mFrameCount = mFrameCount;
    mMetadataResult.mFrameCountComplete = aComplete;
    mMetadataPromise.ResolveIfExists(mMetadataResult, __func__);

    if (mMetadataTask) {
      mMetadataTask = nullptr;
      if (mFramesTask && mFramesToDecode > 0 && !mFramesTaskRunning) {
        MOZ_LOG(
            sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderImpl::OnFrameCount -- start frames task, "
             "want %zu",
             this, mFramesToDecode));
        mFramesTaskRunning = mFramesTask->MaybeStart();
      }
    }

    if (resolve) {
      mFrameCountPromise.ResolveIfExists(
          DecodeFrameCountResult{aFrameCount, aComplete}, __func__);
    }

    if (aComplete) {
      mFrameCountTask = nullptr;
    }
  }

  bool OnFrameAvailable(RefPtr<imgFrame>&& aFrame,
                        RefPtr<gfx::SourceSurface>&& aSurface) override {
    MutexAutoLock lock(mMutex);

    MOZ_DIAGNOSTIC_ASSERT(mFramesTaskRunning);

    // We must have already gotten destroyed before frame decoding finished.
    if (!mFramesTask) {
      mFramesTaskRunning = false;
      return false;
    }

    // Filter duplicate frames.
    if (mLastFrame == aFrame) {
      return true;
    }

    mPendingFramesResult.mFrames.AppendElement(
        DecodedFrame{std::move(aSurface), mMetadataResult.mAnimated
                                              ? aFrame->GetTimeout()
                                              : FrameTimeout::Forever()});
    mLastFrame = std::move(aFrame);

    MOZ_LOG(sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderImpl::OnFrameAvailable -- want %zu, got %zu",
             this, mFramesToDecode, mPendingFramesResult.mFrames.Length()));

    // Check if we have satisfied the number of requested frames.
    if (mFramesToDecode > mPendingFramesResult.mFrames.Length()) {
      return true;
    }

    mFramesToDecode = 0;
    if (!mFramesPromise.IsEmpty()) {
      mFramesPromise.Resolve(std::move(mPendingFramesResult), __func__);
    }
    mFramesTaskRunning = false;
    return false;
  }

  void OnFramesComplete() override {
    MutexAutoLock lock(mMutex);

    // We must have already gotten destroyed before frame decoding finished.
    if (!mFramesTask) {
      return;
    }

    MOZ_LOG(
        sLog, LogLevel::Debug,
        ("[%p] AnonymousDecoderImpl::OnFramesComplete -- wanted %zu, got %zu",
         this, mFramesToDecode, mPendingFramesResult.mFrames.Length()));

    mFramesToDecode = 0;
    mPendingFramesResult.mFinished = true;
    if (!mFramesPromise.IsEmpty()) {
      mFramesPromise.Resolve(std::move(mPendingFramesResult), __func__);
    }
    mLastFrame = nullptr;
    mFramesTask = nullptr;
  }

  RefPtr<DecodeMetadataPromise> DecodeMetadata() override {
    MutexAutoLock lock(mMutex);

    if (!mMetadataTask) {
      MOZ_LOG(sLog, LogLevel::Debug,
              ("[%p] AnonymousDecoderImpl::DecodeMetadata -- already complete",
               this));
      if (mMetadataResult.mWidth > 0 && mMetadataResult.mHeight > 0) {
        return DecodeMetadataPromise::CreateAndResolve(mMetadataResult,
                                                       __func__);
      }
      return DecodeMetadataPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }

    if (!mMetadataTaskRunning) {
      MOZ_LOG(sLog, LogLevel::Debug,
              ("[%p] AnonymousDecoderImpl::DecodeMetadata -- queue", this));
      mMetadataTaskRunning = mMetadataTask->MaybeStart();
    }

    return mMetadataPromise.Ensure(__func__);
  }

  RefPtr<DecodeFrameCountPromise> DecodeFrameCount(
      uint32_t aKnownFrameCount) override {
    MutexAutoLock lock(mMutex);

    MOZ_ASSERT(mFrameCountPromise.IsEmpty());

    // If we have finished, or we have an updated frame count, return right
    // away. This may drive the frame decoder for the application as the data
    // comes in from the network.
    if (!mFrameCountTask || aKnownFrameCount < mFrameCount) {
      MOZ_LOG(sLog, LogLevel::Debug,
              ("[%p] AnonymousDecoderImpl::DecodeFrameCount -- known %u, "
               "detected %u, complete %d",
               this, aKnownFrameCount, mFrameCount, !mFrameCountTask));
      return DecodeFrameCountPromise::CreateAndResolve(
          DecodeFrameCountResult{mFrameCount,
                                 /* mFinished */ !mFrameCountTask},
          __func__);
    }

    // mFrameCountTask is launching when metadata decoding is finished.
    MOZ_LOG(sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderImpl::DecodeFrameCount -- waiting, known "
             "%u, detected %u",
             this, aKnownFrameCount, mFrameCount));
    return mFrameCountPromise.Ensure(__func__);
  }

  RefPtr<DecodeFramesPromise> DecodeFrames(size_t aCount) override {
    MutexAutoLock lock(mMutex);

    // If we cleared our task reference, then we know we finished decoding.
    if (!mFramesTask) {
      mPendingFramesResult.mFinished = true;
      return DecodeFramesPromise::CreateAndResolve(
          std::move(mPendingFramesResult), __func__);
    }

    // If we are not waiting on any frames, then we know we paused decoding.
    // If we still are metadata decoding, we need to wait.
    if (mFramesToDecode == 0 && !mMetadataTask && !mFramesTaskRunning) {
      MOZ_LOG(sLog, LogLevel::Debug,
              ("[%p] AnonymousDecoderImpl::DecodeFrames -- queue", this));
      mFramesTaskRunning = mFramesTask->MaybeStart();
    }

    mFramesToDecode = std::max(mFramesToDecode, aCount);
    return mFramesPromise.Ensure(__func__);
  }

  void CancelDecodeFrames() override {
    MutexAutoLock lock(mMutex);
    MOZ_LOG(sLog, LogLevel::Debug,
            ("[%p] AnonymousDecoderImpl::CancelDecodeFrames", this));
    mFramesToDecode = 0;
    mFramesPromise.RejectIfExists(NS_ERROR_ABORT, __func__);
  }

 private:
  Mutex mMutex;
  MozPromiseHolder<DecodeMetadataPromise> mMetadataPromise
      MOZ_GUARDED_BY(mMutex);
  MozPromiseHolder<DecodeFrameCountPromise> mFrameCountPromise
      MOZ_GUARDED_BY(mMutex);
  MozPromiseHolder<DecodeFramesPromise> mFramesPromise MOZ_GUARDED_BY(mMutex);
  RefPtr<AnonymousFramesDecoderTask> mFramesTask MOZ_GUARDED_BY(mMutex);
  RefPtr<AnonymousMetadataDecoderTask> mMetadataTask MOZ_GUARDED_BY(mMutex);
  RefPtr<AnonymousFrameCountDecoderTask> mFrameCountTask MOZ_GUARDED_BY(mMutex);
  RefPtr<imgFrame> mLastFrame MOZ_GUARDED_BY(mMutex);
  DecodeMetadataResult mMetadataResult MOZ_GUARDED_BY(mMutex);
  DecodeFramesResult mPendingFramesResult MOZ_GUARDED_BY(mMutex);
  Maybe<gfx::IntSize> mOutputSize MOZ_GUARDED_BY(mMutex);
  size_t mFramesToDecode MOZ_GUARDED_BY(mMutex) = 1;
  uint32_t mFrameCount MOZ_GUARDED_BY(mMutex) = 0;
  bool mMetadataTaskRunning MOZ_GUARDED_BY(mMutex) = false;
  bool mFrameCountTaskRunning MOZ_GUARDED_BY(mMutex) = false;
  bool mFramesTaskRunning MOZ_GUARDED_BY(mMutex) = false;
};

/* static */ already_AddRefed<AnonymousDecoder> ImageUtils::CreateDecoder(
    SourceBuffer* aSourceBuffer, DecoderType aType,
    const Maybe<gfx::IntSize>& aOutputSize, SurfaceFlags aSurfaceFlags) {
  if (NS_WARN_IF(!aSourceBuffer)) {
    return nullptr;
  }

  if (NS_WARN_IF(aType == DecoderType::UNKNOWN)) {
    return nullptr;
  }

  RefPtr<Decoder> decoder = DecoderFactory::CreateAnonymousDecoder(
      aType, WrapNotNull(aSourceBuffer), Nothing(),
      DecoderFlags::IMAGE_IS_TRANSIENT, aSurfaceFlags);
  if (NS_WARN_IF(!decoder)) {
    return nullptr;
  }

  auto anonymousDecoder = MakeRefPtr<AnonymousDecoderImpl>(aOutputSize);
  if (NS_WARN_IF(!anonymousDecoder->Initialize(std::move(decoder)))) {
    return nullptr;
  }

  return anonymousDecoder.forget();
}

/* static */ DecoderType ImageUtils::GetDecoderType(
    const nsACString& aMimeType) {
  return DecoderFactory::GetDecoderType(aMimeType.Data());
}

}  // namespace mozilla::image
