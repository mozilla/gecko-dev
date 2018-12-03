/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerMarkerPayload_h
#define ProfilerMarkerPayload_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/net/TimingStruct.h"

#include "nsString.h"
#include "GeckoProfiler.h"

#include "js/Utility.h"
#include "gfxASurface.h"
#include "mozilla/ServoTraversalStatistics.h"

namespace mozilla {
namespace layers {
class Layer;
}  // namespace layers
}  // namespace mozilla

class SpliceableJSONWriter;
class UniqueStacks;

// This is an abstract class that can be implemented to supply data to be
// attached with a profiler marker.
//
// When subclassing this, note that the destructor can be called on any thread,
// i.e. not necessarily on the thread that created the object.
class ProfilerMarkerPayload {
 public:
  explicit ProfilerMarkerPayload(
      const mozilla::Maybe<nsID>& aDocShellId = mozilla::Nothing(),
      const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing(),
      UniqueProfilerBacktrace aStack = nullptr)
      : mStack(std::move(aStack)),
        mDocShellId(aDocShellId),
        mDocShellHistoryId(aDocShellHistoryId) {}

  ProfilerMarkerPayload(
      const mozilla::TimeStamp& aStartTime, const mozilla::TimeStamp& aEndTime,
      const mozilla::Maybe<nsID>& aDocShellId = mozilla::Nothing(),
      const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing(),
      UniqueProfilerBacktrace aStack = nullptr)
      : mStartTime(aStartTime),
        mEndTime(aEndTime),
        mStack(std::move(aStack)),
        mDocShellId(aDocShellId),
        mDocShellHistoryId(aDocShellHistoryId) {}

  virtual ~ProfilerMarkerPayload() {}

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) = 0;

  mozilla::TimeStamp GetStartTime() const { return mStartTime; }

 protected:
  void StreamType(const char* aMarkerType, SpliceableJSONWriter& aWriter);
  void StreamCommonProps(const char* aMarkerType, SpliceableJSONWriter& aWriter,
                         const mozilla::TimeStamp& aProcessStartTime,
                         UniqueStacks& aUniqueStacks);

  void SetStack(UniqueProfilerBacktrace aStack) { mStack = std::move(aStack); }

  void SetDocShellHistoryId(
      const mozilla::Maybe<uint32_t>& aDocShellHistoryId) {
    mDocShellHistoryId = aDocShellHistoryId;
  }

  void SetDocShellId(const mozilla::Maybe<nsID>& aDocShellId) {
    mDocShellId = aDocShellId;
  }

 private:
  mozilla::TimeStamp mStartTime;
  mozilla::TimeStamp mEndTime;
  UniqueProfilerBacktrace mStack;
  mozilla::Maybe<nsID> mDocShellId;
  mozilla::Maybe<uint32_t> mDocShellHistoryId;
};

#define DECL_STREAM_PAYLOAD                                               \
  virtual void StreamPayload(SpliceableJSONWriter& aWriter,               \
                             const mozilla::TimeStamp& aProcessStartTime, \
                             UniqueStacks& aUniqueStacks) override;

// TODO: Increase the coverage of tracing markers that include DocShell
// information
class TracingMarkerPayload : public ProfilerMarkerPayload {
 public:
  TracingMarkerPayload(
      const char* aCategory, TracingKind aKind,
      const mozilla::Maybe<nsID>& aDocShellId = mozilla::Nothing(),
      const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing(),
      UniqueProfilerBacktrace aCause = nullptr)
      : mCategory(aCategory), mKind(aKind) {
    if (aCause) {
      SetStack(std::move(aCause));
    }
    SetDocShellId(aDocShellId);
    SetDocShellHistoryId(aDocShellHistoryId);
  }

  DECL_STREAM_PAYLOAD

 private:
  const char* mCategory;
  TracingKind mKind;
};

class IOMarkerPayload : public ProfilerMarkerPayload {
 public:
  IOMarkerPayload(const char* aSource, const char* aFilename,
                  const mozilla::TimeStamp& aStartTime,
                  const mozilla::TimeStamp& aEndTime,
                  UniqueProfilerBacktrace aStack)
      : ProfilerMarkerPayload(aStartTime, aEndTime, mozilla::Nothing(),
                              mozilla::Nothing(), std::move(aStack)),
        mSource(aSource),
        mFilename(aFilename ? strdup(aFilename) : nullptr) {
    MOZ_ASSERT(aSource);
  }

  DECL_STREAM_PAYLOAD

 private:
  const char* mSource;
  mozilla::UniqueFreePtr<char> mFilename;
};

class DOMEventMarkerPayload : public TracingMarkerPayload {
 public:
  DOMEventMarkerPayload(const nsAString& aEventType,
                        const mozilla::TimeStamp& aTimeStamp,
                        const char* aCategory, TracingKind aKind,
                        const mozilla::Maybe<nsID>& aDocShellId,
                        const mozilla::Maybe<uint32_t>& aDocShellHistoryId)
      : TracingMarkerPayload(aCategory, aKind, aDocShellId, aDocShellHistoryId),
        mTimeStamp(aTimeStamp),
        mEventType(aEventType) {}

  DECL_STREAM_PAYLOAD

 private:
  mozilla::TimeStamp mTimeStamp;
  nsString mEventType;
};

class UserTimingMarkerPayload : public ProfilerMarkerPayload {
 public:
  UserTimingMarkerPayload(const nsAString& aName,
                          const mozilla::TimeStamp& aStartTime,
                          const mozilla::Maybe<nsID>& aDocShellId,
                          const mozilla::Maybe<uint32_t>& aDocShellHistoryId)
      : ProfilerMarkerPayload(aStartTime, aStartTime, aDocShellId,
                              aDocShellHistoryId),
        mEntryType("mark"),
        mName(aName) {}

  UserTimingMarkerPayload(const nsAString& aName,
                          const mozilla::Maybe<nsString>& aStartMark,
                          const mozilla::Maybe<nsString>& aEndMark,
                          const mozilla::TimeStamp& aStartTime,
                          const mozilla::TimeStamp& aEndTime,
                          const mozilla::Maybe<nsID>& aDocShellId,
                          const mozilla::Maybe<uint32_t>& aDocShellHistoryId)
      : ProfilerMarkerPayload(aStartTime, aEndTime, aDocShellId,
                              aDocShellHistoryId),
        mEntryType("measure"),
        mName(aName),
        mStartMark(aStartMark),
        mEndMark(aEndMark) {}

  DECL_STREAM_PAYLOAD

 private:
  // Either "mark" or "measure".
  const char* mEntryType;
  nsString mName;
  mozilla::Maybe<nsString> mStartMark;
  mozilla::Maybe<nsString> mEndMark;
};

// Contains the translation applied to a 2d layer so we can track the layer
// position at each frame.
class LayerTranslationMarkerPayload : public ProfilerMarkerPayload {
 public:
  LayerTranslationMarkerPayload(mozilla::layers::Layer* aLayer,
                                mozilla::gfx::Point aPoint,
                                mozilla::TimeStamp aStartTime)
      : ProfilerMarkerPayload(aStartTime, aStartTime),
        mLayer(aLayer),
        mPoint(aPoint) {}

  DECL_STREAM_PAYLOAD

 private:
  mozilla::layers::Layer* mLayer;
  mozilla::gfx::Point mPoint;
};

#include "Units.h"  // For ScreenIntPoint

// Tracks when a vsync occurs according to the HardwareComposer.
class VsyncMarkerPayload : public ProfilerMarkerPayload {
 public:
  explicit VsyncMarkerPayload(mozilla::TimeStamp aVsyncTimestamp)
      : ProfilerMarkerPayload(aVsyncTimestamp, aVsyncTimestamp) {}

  DECL_STREAM_PAYLOAD
};

class NetworkMarkerPayload : public ProfilerMarkerPayload {
 public:
  NetworkMarkerPayload(int64_t aID, const char* aURI, NetworkLoadType aType,
                       const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime, int32_t aPri,
                       int64_t aCount,
                       mozilla::net::CacheDisposition aCacheDisposition,
                       const mozilla::net::TimingStruct* aTimings = nullptr,
                       const char* aRedirectURI = nullptr)
      : ProfilerMarkerPayload(aStartTime, aEndTime, mozilla::Nothing()),
        mID(aID),
        mURI(aURI ? strdup(aURI) : nullptr),
        mRedirectURI(aRedirectURI && (strlen(aRedirectURI) > 0)
                         ? strdup(aRedirectURI)
                         : nullptr),
        mType(aType),
        mPri(aPri),
        mCount(aCount),
        mCacheDisposition(aCacheDisposition) {
    if (aTimings) {
      mTimings = *aTimings;
    }
  }

  DECL_STREAM_PAYLOAD

 private:
  int64_t mID;
  mozilla::UniqueFreePtr<char> mURI;
  mozilla::UniqueFreePtr<char> mRedirectURI;
  NetworkLoadType mType;
  int32_t mPri;
  int64_t mCount;
  mozilla::net::TimingStruct mTimings;
  mozilla::net::CacheDisposition mCacheDisposition;
};

class ScreenshotPayload : public ProfilerMarkerPayload {
 public:
  explicit ScreenshotPayload(mozilla::TimeStamp aTimeStamp,
                             nsCString&& aScreenshotDataURL,
                             const mozilla::gfx::IntSize& aWindowSize,
                             uintptr_t aWindowIdentifier)
      : ProfilerMarkerPayload(aTimeStamp, mozilla::TimeStamp()),
        mScreenshotDataURL(std::move(aScreenshotDataURL)),
        mWindowSize(aWindowSize),
        mWindowIdentifier(aWindowIdentifier) {}

  DECL_STREAM_PAYLOAD

 private:
  nsCString mScreenshotDataURL;
  mozilla::gfx::IntSize mWindowSize;
  uintptr_t mWindowIdentifier;
};

class GCSliceMarkerPayload : public ProfilerMarkerPayload {
 public:
  GCSliceMarkerPayload(const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime,
                       JS::UniqueChars&& aTimingJSON)
      : ProfilerMarkerPayload(aStartTime, aEndTime),
        mTimingJSON(std::move(aTimingJSON)) {}

  DECL_STREAM_PAYLOAD

 private:
  JS::UniqueChars mTimingJSON;
};

class GCMajorMarkerPayload : public ProfilerMarkerPayload {
 public:
  GCMajorMarkerPayload(const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime,
                       JS::UniqueChars&& aTimingJSON)
      : ProfilerMarkerPayload(aStartTime, aEndTime),
        mTimingJSON(std::move(aTimingJSON)) {}

  DECL_STREAM_PAYLOAD

 private:
  JS::UniqueChars mTimingJSON;
};

class GCMinorMarkerPayload : public ProfilerMarkerPayload {
 public:
  GCMinorMarkerPayload(const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime,
                       JS::UniqueChars&& aTimingData)
      : ProfilerMarkerPayload(aStartTime, aEndTime),
        mTimingData(std::move(aTimingData)) {}

  DECL_STREAM_PAYLOAD

 private:
  JS::UniqueChars mTimingData;
};

class HangMarkerPayload : public ProfilerMarkerPayload {
 public:
  HangMarkerPayload(const mozilla::TimeStamp& aStartTime,
                    const mozilla::TimeStamp& aEndTime)
      : ProfilerMarkerPayload(aStartTime, aEndTime) {}

  DECL_STREAM_PAYLOAD
 private:
};

class StyleMarkerPayload : public ProfilerMarkerPayload {
 public:
  StyleMarkerPayload(const mozilla::TimeStamp& aStartTime,
                     const mozilla::TimeStamp& aEndTime,
                     UniqueProfilerBacktrace aCause,
                     const mozilla::ServoTraversalStatistics& aStats,
                     const mozilla::Maybe<nsID>& aDocShellId,
                     const mozilla::Maybe<uint32_t>& aDocShellHistoryId)
      : ProfilerMarkerPayload(aStartTime, aEndTime, aDocShellId,
                              aDocShellHistoryId),
        mStats(aStats) {
    if (aCause) {
      SetStack(std::move(aCause));
    }
  }

  DECL_STREAM_PAYLOAD

 private:
  mozilla::ServoTraversalStatistics mStats;
};

class LongTaskMarkerPayload : public ProfilerMarkerPayload {
 public:
  LongTaskMarkerPayload(const mozilla::TimeStamp& aStartTime,
                        const mozilla::TimeStamp& aEndTime)
      : ProfilerMarkerPayload(aStartTime, aEndTime) {}

  DECL_STREAM_PAYLOAD
};

#endif  // ProfilerMarkerPayload_h
