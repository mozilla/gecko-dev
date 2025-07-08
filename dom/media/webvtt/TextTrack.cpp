/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrack.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/TextTrackBinding.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/TextTrackCueList.h"
#include "mozilla/dom/TextTrackList.h"
#include "mozilla/dom/TextTrackRegion.h"
#include "nsGlobalWindowInner.h"

extern mozilla::LazyLogModule gTextTrackLog;

#define WEBVTT_LOG(msg, ...)              \
  MOZ_LOG(gTextTrackLog, LogLevel::Debug, \
          ("TextTrack=%p, " msg, this, ##__VA_ARGS__))
#define WEBVTT_LOGV(msg, ...)               \
  MOZ_LOG(gTextTrackLog, LogLevel::Verbose, \
          ("TextTrack=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(TextTrack, DOMEventTargetHelper, mCueList,
                                   mActiveCueList, mTextTrackList,
                                   mTrackElement)

NS_IMPL_ADDREF_INHERITED(TextTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TextTrack::TextTrack(nsPIDOMWindowInner* aOwnerWindow, TextTrackKind aKind,
                     const nsAString& aLabel, const nsAString& aLanguage,
                     TextTrackMode aMode, TextTrackReadyState aReadyState,
                     TextTrackSource aTextTrackSource)
    : DOMEventTargetHelper(aOwnerWindow),
      mKind(aKind),
      mLabel(aLabel),
      mLanguage(aLanguage),
      mMode(aMode),
      mReadyState(aReadyState),
      mTextTrackSource(aTextTrackSource) {
  SetDefaultSettings();
}

TextTrack::TextTrack(nsPIDOMWindowInner* aOwnerWindow,
                     TextTrackList* aTextTrackList, TextTrackKind aKind,
                     const nsAString& aLabel, const nsAString& aLanguage,
                     TextTrackMode aMode, TextTrackReadyState aReadyState,
                     TextTrackSource aTextTrackSource)
    : DOMEventTargetHelper(aOwnerWindow),
      mTextTrackList(aTextTrackList),
      mKind(aKind),
      mLabel(aLabel),
      mLanguage(aLanguage),
      mMode(aMode),
      mReadyState(aReadyState),
      mTextTrackSource(aTextTrackSource) {
  SetDefaultSettings();
}

TextTrack::~TextTrack() = default;

void TextTrack::SetDefaultSettings() {
  nsPIDOMWindowInner* ownerWindow = GetOwnerWindow();
  mCueList = new TextTrackCueList(ownerWindow);
  mActiveCueList = new TextTrackCueList(ownerWindow);
  mCuePos = 0;
  mDirty = false;
}

JSObject* TextTrack::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return TextTrack_Binding::Wrap(aCx, this, aGivenProto);
}

void TextTrack::SetMode(TextTrackMode aValue) {
  if (mMode == aValue) {
    return;
  }
  WEBVTT_LOG("Set mode=%s for track kind %s", GetEnumString(aValue).get(),
             GetEnumString(mKind).get());
  mMode = aValue;

  HTMLMediaElement* mediaElement = GetMediaElement();
  if (aValue == TextTrackMode::Disabled) {
    for (size_t i = 0; i < mCueList->Length() && mediaElement; ++i) {
      mediaElement->NotifyCueRemoved(*(*mCueList)[i]);
    }
    SetCuesInactive();
  } else {
    for (size_t i = 0; i < mCueList->Length() && mediaElement; ++i) {
      mediaElement->NotifyCueAdded(*(*mCueList)[i]);
    }
  }
  if (mediaElement) {
    mediaElement->NotifyTextTrackModeChanged();
  }
  // https://html.spec.whatwg.org/multipage/media.html#sourcing-out-of-band-text-tracks:start-the-track-processing-model
  // Run the `start-the-track-processing-model` to track's corresponding track
  // element whenever track's mode changes.
  if (mTrackElement) {
    mTrackElement->MaybeDispatchLoadResource();
  }
  // Ensure the TimeMarchesOn is called in case that the mCueList
  // is empty.
  NotifyCueUpdated(nullptr);
}

void TextTrack::GetId(nsAString& aId) const {
  // If the track has a track element then its id should be the same as the
  // track element's id.
  if (mTrackElement) {
    mTrackElement->GetAttr(nsGkAtoms::id, aId);
  }
}

void TextTrack::AddCue(TextTrackCue& aCue) {
  WEBVTT_LOG("AddCue %p [%f:%f]", &aCue, aCue.StartTime(), aCue.EndTime());
  TextTrack* oldTextTrack = aCue.GetTrack();
  if (oldTextTrack) {
    ErrorResult dummy;
    oldTextTrack->RemoveCue(aCue, dummy);
  }
  mCueList->AddCue(aCue);
  aCue.SetTrack(this);
  HTMLMediaElement* mediaElement = GetMediaElement();
  if (mediaElement && (mMode != TextTrackMode::Disabled)) {
    mediaElement->NotifyCueAdded(aCue);
  }
}

void TextTrack::RemoveCue(TextTrackCue& aCue, ErrorResult& aRv) {
  WEBVTT_LOG("RemoveCue %p", &aCue);
  // Bug1304948, check the aCue belongs to the TextTrack.
  mCueList->RemoveCue(aCue, aRv);
  if (aRv.Failed()) {
    return;
  }
  aCue.SetActive(false);
  aCue.SetTrack(nullptr);
  HTMLMediaElement* mediaElement = GetMediaElement();
  if (mediaElement) {
    mediaElement->NotifyCueRemoved(aCue);
  }
}

void TextTrack::ClearAllCues() {
  WEBVTT_LOG("ClearAllCues");
  ErrorResult dummy;
  while (!mCueList->IsEmpty()) {
    RemoveCue(*(*mCueList)[0], dummy);
  }
}

void TextTrack::SetCuesDirty() {
  for (uint32_t i = 0; i < mCueList->Length(); i++) {
    ((*mCueList)[i])->Reset();
  }
}

TextTrackCueList* TextTrack::GetActiveCues() {
  if (mMode != TextTrackMode::Disabled) {
    return mActiveCueList;
  }
  return nullptr;
}

void TextTrack::GetActiveCueArray(nsTArray<RefPtr<TextTrackCue>>& aCues) {
  if (mMode != TextTrackMode::Disabled) {
    mActiveCueList->GetArray(aCues);
  }
}

TextTrackReadyState TextTrack::ReadyState() const { return mReadyState; }

void TextTrack::SetReadyState(TextTrackReadyState aState) {
  WEBVTT_LOG("SetReadyState=%s", EnumValueToString(aState));
  mReadyState = aState;
  HTMLMediaElement* mediaElement = GetMediaElement();
  if (mediaElement && (mReadyState == TextTrackReadyState::Loaded ||
                       mReadyState == TextTrackReadyState::FailedToLoad)) {
    mediaElement->RemoveTextTrack(this, true);
    mediaElement->UpdateReadyState();
  }
}

TextTrackList* TextTrack::GetTextTrackList() { return mTextTrackList; }

void TextTrack::SetTextTrackList(TextTrackList* aTextTrackList) {
  mTextTrackList = aTextTrackList;
}

HTMLTrackElement* TextTrack::GetTrackElement() { return mTrackElement; }

void TextTrack::SetTrackElement(HTMLTrackElement* aTrackElement) {
  mTrackElement = aTrackElement;
}

void TextTrack::SetCuesInactive() {
  WEBVTT_LOG("SetCuesInactive");
  mCueList->SetCuesInactive();
}

void TextTrack::NotifyCueUpdated(TextTrackCue* aCue) {
  WEBVTT_LOG("NotifyCueUpdated, cue=%p", aCue);
  mCueList->NotifyCueUpdated(aCue);
  HTMLMediaElement* mediaElement = GetMediaElement();
  if (mediaElement) {
    mediaElement->NotifyCueUpdated(aCue);
  }
}

void TextTrack::GetLabel(nsAString& aLabel) const {
  if (mTrackElement) {
    mTrackElement->GetLabel(aLabel);
  } else {
    aLabel = mLabel;
  }
}
void TextTrack::GetLanguage(nsAString& aLanguage) const {
  if (mTrackElement) {
    mTrackElement->GetSrclang(aLanguage);
  } else {
    aLanguage = mLanguage;
  }
}

void TextTrack::DispatchAsyncTrustedEvent(const nsString& aEventName) {
  nsGlobalWindowInner* win = GetOwnerWindow();
  if (!win) {
    return;
  }
  win->Dispatch(
      NS_NewRunnableFunction("dom::TextTrack::DispatchAsyncTrustedEvent",
                             [self = RefPtr{this}, aEventName]() {
                               self->DispatchTrustedEvent(aEventName);
                             }));
}

bool TextTrack::IsLoaded() {
  if (mMode == TextTrackMode::Disabled) {
    return true;
  }
  // If the TrackElement's src is null, we can not block the
  // MediaElement.
  if (mTrackElement) {
    nsAutoString src;
    if (!(mTrackElement->GetAttr(nsGkAtoms::src, src))) {
      return true;
    }
  }
  return mReadyState >= TextTrackReadyState::Loaded;
}

void TextTrack::NotifyCueActiveStateChanged(TextTrackCue* aCue) {
  MOZ_ASSERT(aCue);
  if (aCue->GetActive()) {
    MOZ_ASSERT(!mActiveCueList->IsCueExist(aCue));
    WEBVTT_LOG("NotifyCueActiveStateChanged, add cue %p to the active list",
               aCue);
    mActiveCueList->AddCue(*aCue);
  } else {
    MOZ_ASSERT(mActiveCueList->IsCueExist(aCue));
    WEBVTT_LOG(
        "NotifyCueActiveStateChanged, remove cue %p from the active list",
        aCue);
    mActiveCueList->RemoveCue(*aCue);
  }
}

void TextTrack::GetOverlappingCurrentOtherAndMissCues(
    CueBuckets* aCurrentCues, CueBuckets* aOtherCues, CueBuckets* aMissCues,
    const media::TimeInterval& aInterval,
    const Maybe<double>& aLastTime) const {
  const HTMLMediaElement* mediaElement = GetMediaElement();
  if (!mediaElement || Mode() == TextTrackMode::Disabled ||
      mCueList->IsEmpty()) {
    return;
  }

  // According to `time marches on` step1, current cue list contains the cues
  // whose start times are less than or equal to the current playback position
  // and whose end times are greater than the current playback position.
  // https://html.spec.whatwg.org/multipage/media.html#time-marches-on
  MOZ_ASSERT(aCurrentCues && aOtherCues);
  const double playbackTime = mediaElement->CurrentTime();
  const double intervalStart = aInterval.mStart.ToSeconds();
  const double intervalEnd = aInterval.mEnd.ToSeconds();

  if (intervalEnd < (*mCueList)[0]->StartTime()) {
    WEBVTT_LOGV("Abort : interval ends before the first cue starts");
    return;
  }
  // Optimize the loop range by identifying the first cue that starts after the
  // interval.
  size_t lastIdx = 0;
  struct LastIdxComparator {
    const double mIntervalEnd;
    explicit LastIdxComparator(double aIntervalEnd)
        : mIntervalEnd(aIntervalEnd) {}
    int operator()(const TextTrackCue* aCue) const {
      return aCue->StartTime() > mIntervalEnd ? 0 : -1;
    }
  } compLast(intervalEnd);
  if (!BinarySearchIf(mCueList->GetCuesArray(), 0, mCueList->Length(), compLast,
                      &lastIdx)) {
    // Failed to find the match, set it to the last idx.
    lastIdx = mCueList->Length() - 1;
  }

  // Search cues in the partial range.
  for (size_t idx = 0; idx <= lastIdx; ++idx) {
    TextTrackCue* cue = (*mCueList)[idx];
    double cueStart = cue->StartTime();
    double cueEnd = cue->EndTime();
    if (cueStart <= playbackTime && cueEnd > playbackTime) {
      WEBVTT_LOG("Add cue %p [%f:%f] to current cue list", cue, cueStart,
                 cueEnd);
      aCurrentCues->AddCue(cue);
    } else {
      // As the spec doesn't have a restriction for the negative duration, it
      // does happen sometime if user sets it explicitly. It will be treated as
      // a `missing cue` (a subset of the `other cues`) and it won't be
      // displayed.
      if (cueEnd < cueStart) {
        // Add cue into `otherCue` only when its start time is contained by the
        // current time interval.
        if (intervalStart <= cueStart && cueStart < intervalEnd) {
          WEBVTT_LOG(
              "[Negative duration] Add cue %p [%f:%f] to other cues and "
              "missing cues list",
              cue, cueStart, cueEnd);
          aOtherCues->AddCue(cue);
          aMissCues->AddCue(cue);
        }
        continue;
      }
      // Cues are completely outside the time interval.
      if (cueEnd < intervalStart || cueStart > intervalEnd) {
        continue;
      }
      WEBVTT_LOG("Add cue %p [%f:%f] to other cue list", cue, cueStart, cueEnd);
      aOtherCues->AddCue(cue);
      if (aLastTime && cueStart >= *aLastTime && cueEnd <= playbackTime) {
        WEBVTT_LOG("Add cue %p [%f:%f] to missing cues list", cue, cueStart,
                   cueEnd);
        aMissCues->AddCue(cue);
      }
    }
  }
}

HTMLMediaElement* TextTrack::GetMediaElement() const {
  return mTextTrackList ? mTextTrackList->GetMediaElement() : nullptr;
}

void TextTrack::CueBuckets::AddCue(TextTrackCue* aCue) {
  if (aCue->GetActive()) {
    ActiveCues().AppendElement(aCue);
    if (aCue->PauseOnExit()) {
      mHasPauseOnExist[static_cast<uint8_t>(CueActivityState::Active)] = true;
    }
  } else {
    InactiveCues().AppendElement(aCue);
    if (aCue->PauseOnExit()) {
      mHasPauseOnExist[static_cast<uint8_t>(CueActivityState::Inactive)] = true;
    }
  }
  AllCues().AppendElement(aCue);
  if (aCue->PauseOnExit()) {
    mHasPauseOnExist[static_cast<uint8_t>(CueActivityState::All)] = true;
  }
}

}  // namespace mozilla::dom
