/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrack_h
#define mozilla_dom_TextTrack_h

#include "TimeUnits.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/DefineEnum.h"
#include "mozilla/dom/TextTrackBinding.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"

namespace mozilla::dom {

class TextTrackList;
class TextTrackCue;
class TextTrackCueList;
class HTMLTrackElement;
class HTMLMediaElement;

enum class TextTrackSource : uint8_t {
  Track,
  AddTextTrack,
  MediaResourceSpecific,
};

// Constants for numeric readyState property values.
MOZ_DEFINE_ENUM_CLASS_WITH_BASE_AND_TOSTRING(TextTrackReadyState, uint8_t,
                                             (NotLoaded, Loading, Loaded,
                                              FailedToLoad));

class TextTrack final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextTrack, DOMEventTargetHelper)

  TextTrack(nsPIDOMWindowInner* aOwnerWindow, TextTrackKind aKind,
            const nsAString& aLabel, const nsAString& aLanguage,
            TextTrackMode aMode, TextTrackReadyState aReadyState,
            TextTrackSource aTextTrackSource);
  TextTrack(nsPIDOMWindowInner* aOwnerWindow, TextTrackList* aTextTrackList,
            TextTrackKind aKind, const nsAString& aLabel,
            const nsAString& aLanguage, TextTrackMode aMode,
            TextTrackReadyState aReadyState, TextTrackSource aTextTrackSource);

  void SetDefaultSettings();

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  TextTrackKind Kind() const { return mKind; }
  void GetLabel(nsAString& aLabel) const;
  void GetLanguage(nsAString& aLanguage) const;
  void GetInBandMetadataTrackDispatchType(nsAString& aType) const {
    aType = mType;
  }
  void GetId(nsAString& aId) const;

  TextTrackMode Mode() const { return mMode; }
  void SetMode(TextTrackMode aValue);

  TextTrackCueList* GetCues() const {
    if (mMode == TextTrackMode::Disabled) {
      return nullptr;
    }
    return mCueList;
  }

  TextTrackCueList* GetActiveCues();
  void GetActiveCueArray(nsTArray<RefPtr<TextTrackCue>>& aCues);

  TextTrackReadyState ReadyState() const;
  void SetReadyState(TextTrackReadyState aState);

  void AddCue(TextTrackCue& aCue);
  void RemoveCue(TextTrackCue& aCue, ErrorResult& aRv);
  void SetDirty() { mDirty = true; }
  void SetCuesDirty();

  TextTrackList* GetTextTrackList();
  void SetTextTrackList(TextTrackList* aTextTrackList);

  IMPL_EVENT_HANDLER(cuechange)

  HTMLTrackElement* GetTrackElement();
  void SetTrackElement(HTMLTrackElement* aTrackElement);

  TextTrackSource GetTextTrackSource() { return mTextTrackSource; }

  void SetCuesInactive();

  void NotifyCueUpdated(TextTrackCue* aCue);

  void DispatchAsyncTrustedEvent(const nsString& aEventName);

  bool IsLoaded();

  // Called when associated cue's active flag has been changed, and then we
  // would add or remove the cue to the active cue list.
  void NotifyCueActiveStateChanged(TextTrackCue* aCue);

  enum class CueActivityState : uint8_t { Inactive = 0, Active, All, Count };
  class CueBuckets final {
   public:
    void AddCue(TextTrackCue* aCue);

    nsTArray<RefPtr<TextTrackCue>>& ActiveCues() {
      return mCues[static_cast<size_t>(CueActivityState::Active)];
    }

    nsTArray<RefPtr<TextTrackCue>>& InactiveCues() {
      return mCues[static_cast<size_t>(CueActivityState::Inactive)];
    }

    nsTArray<RefPtr<TextTrackCue>>& AllCues() {
      return mCues[static_cast<size_t>(CueActivityState::All)];
    }

    bool HasPauseOnExit(CueActivityState aState) const {
      MOZ_DIAGNOSTIC_ASSERT(aState != CueActivityState::Count);
      return mHasPauseOnExist[static_cast<uint8_t>(aState)];
    }

   private:
    nsTArray<RefPtr<TextTrackCue>>
        mCues[static_cast<size_t>(CueActivityState::Count)];

    // Returns true if any cue has in the given stage has the PauseOnExit flag
    // set to true.
    bool mHasPauseOnExist[static_cast<size_t>(CueActivityState::Count)] = {
        false};
  };

  // Use this function to get `current cues`, `other cues` and `miss cues`
  // which are overlapping with the given interval.
  // The `current cues` have start time are less than or equal to the current
  // playback position and whose end times are greater than the current playback
  // position. the `other cues` are not in the current cues.
  // `aLastTime` is the last time defined in the time marches on step3, it will
  // only exists when miss cues calculation is needed.
  void GetOverlappingCurrentOtherAndMissCues(
      CueBuckets* aCurrentCues, CueBuckets* aOtherCues, CueBuckets* aMissCues,
      const media::TimeInterval& aInterval,
      const Maybe<double>& aLastTime) const;

  void ClearAllCues();

 private:
  ~TextTrack();

  HTMLMediaElement* GetMediaElement() const;

  RefPtr<TextTrackList> mTextTrackList;

  TextTrackKind mKind;
  nsString mLabel;
  nsString mLanguage;
  nsString mType;
  TextTrackMode mMode;

  RefPtr<TextTrackCueList> mCueList;
  RefPtr<TextTrackCueList> mActiveCueList;
  RefPtr<HTMLTrackElement> mTrackElement;

  uint32_t mCuePos;
  TextTrackReadyState mReadyState;
  bool mDirty;

  // An enum that represents where the track was sourced from.
  TextTrackSource mTextTrackSource;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TextTrack_h
