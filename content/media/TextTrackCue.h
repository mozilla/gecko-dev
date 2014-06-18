/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackCue_h
#define mozilla_dom_TextTrackCue_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/VTTCueBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIWebVTTParserWrapper.h"
#include "mozilla/StaticPtr.h"
#include "nsIDocument.h"
#include "mozilla/dom/HTMLDivElement.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/TextTrack.h"

namespace mozilla {
namespace dom {

class HTMLTrackElement;
class TextTrackRegion;

class TextTrackCue MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextTrackCue, DOMEventTargetHelper)

  // TextTrackCue WebIDL
  // See bug 868509 about splitting out the WebVTT-specific interfaces.
  static already_AddRefed<TextTrackCue>
  Constructor(GlobalObject& aGlobal,
              double aStartTime,
              double aEndTime,
              const nsAString& aText,
              ErrorResult& aRv)
  {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
    nsRefPtr<TextTrackCue> ttcue = new TextTrackCue(window, aStartTime,
                                                    aEndTime, aText, aRv);
    return ttcue.forget();
  }
  TextTrackCue(nsPIDOMWindow* aGlobal, double aStartTime, double aEndTime,
               const nsAString& aText, ErrorResult& aRv);

  TextTrackCue(nsPIDOMWindow* aGlobal, double aStartTime, double aEndTime,
               const nsAString& aText, HTMLTrackElement* aTrackElement,
               ErrorResult& aRv);

  ~TextTrackCue();

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  TextTrack* GetTrack() const
  {
    return mTrack;
  }

  void GetId(nsAString& aId) const
  {
    aId = mId;
  }

  void SetId(const nsAString& aId)
  {
    if (mId == aId) {
      return;
    }

    mId = aId;
  }

  double StartTime() const
  {
    return mStartTime;
  }

  void SetStartTime(double aStartTime)
  {
    if (mStartTime == aStartTime) {
      return;
    }

    mStartTime = aStartTime;
    mReset = true;
  }

  double EndTime() const
  {
    return mEndTime;
  }

  void SetEndTime(double aEndTime)
  {
    if (mEndTime == aEndTime) {
      return;
    }

    mEndTime = aEndTime;
    mReset = true;
  }

  bool PauseOnExit()
  {
    return mPauseOnExit;
  }

  void SetPauseOnExit(bool aPauseOnExit)
  {
    if (mPauseOnExit == aPauseOnExit) {
      return;
    }

    mPauseOnExit = aPauseOnExit;
  }

  TextTrackRegion* GetRegion();
  void SetRegion(TextTrackRegion* aRegion);

  DirectionSetting Vertical() const
  {
    return mVertical;
  }

  void SetVertical(const DirectionSetting& aVertical)
  {
    if (mVertical == aVertical) {
      return;
    }

    mReset = true;
    mVertical = aVertical;
  }

  bool SnapToLines()
  {
    return mSnapToLines;
  }

  void SetSnapToLines(bool aSnapToLines)
  {
    if (mSnapToLines == aSnapToLines) {
      return;
    }

    mReset = true;
    mSnapToLines = aSnapToLines;
  }

  void GetLine(OwningLongOrAutoKeyword& aLine) const
  {
    if (mLineIsAutoKeyword) {
      aLine.SetAsAutoKeyword() = AutoKeyword::Auto;
      return;
    }
    aLine.SetAsLong() = mLineLong;
  }

  void SetLine(const LongOrAutoKeyword& aLine)
  {
    if (aLine.IsLong() &&
        (mLineIsAutoKeyword || (aLine.GetAsLong() != mLineLong))) {
      mLineIsAutoKeyword = false;
      mLineLong = aLine.GetAsLong();
      mReset = true;
      return;
    }
    if (aLine.IsAutoKeyword() && !mLineIsAutoKeyword) {
      mLineIsAutoKeyword = true;
      mReset = true;
    }
  }

  AlignSetting LineAlign() const
  {
    return mLineAlign;
  }

  void SetLineAlign(AlignSetting& aLineAlign, ErrorResult& aRv)
  {
    if (mLineAlign == aLineAlign)
      return;

    if (aLineAlign == AlignSetting::Left ||
        aLineAlign == AlignSetting::Right) {
      return aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    }

    mReset = true;
    mLineAlign = aLineAlign;
  }

  int32_t Position() const
  {
    return mPosition;
  }

  void SetPosition(int32_t aPosition, ErrorResult& aRv)
  {
    if (mPosition == aPosition) {
      return;
    }

    if (aPosition > 100 || aPosition < 0){
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    mReset = true;
    mPosition = aPosition;
  }

  AlignSetting PositionAlign() const
  {
    return mPositionAlign;
  }

  void SetPositionAlign(AlignSetting aPositionAlign, ErrorResult& aRv)
  {
    if (mPositionAlign == aPositionAlign)
      return;

    if (aPositionAlign == AlignSetting::Left ||
        aPositionAlign == AlignSetting::Right) {
      return aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    }

    mReset = true;
    mPositionAlign = aPositionAlign;
  }

  int32_t Size() const
  {
    return mSize;
  }

  void SetSize(int32_t aSize, ErrorResult& aRv)
  {
    if (mSize == aSize) {
      return;
    }

    if (aSize < 0 || aSize > 100) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    mReset = true;
    mSize = aSize;
  }

  AlignSetting Align() const
  {
    return mAlign;
  }

  void SetAlign(AlignSetting& aAlign)
  {
    if (mAlign == aAlign) {
      return;
    }

    mReset = true;
    mAlign = aAlign;
  }

  void GetText(nsAString& aText) const
  {
    aText = mText;
  }

  void SetText(const nsAString& aText)
  {
    if (mText == aText) {
      return;
    }

    mReset = true;
    mText = aText;
  }

  IMPL_EVENT_HANDLER(enter)
  IMPL_EVENT_HANDLER(exit)

  HTMLDivElement* GetDisplayState()
  {
    return static_cast<HTMLDivElement*>(mDisplayState.get());
  }

  void SetDisplayState(HTMLDivElement* aDisplayState)
  {
    mDisplayState = aDisplayState;
    mReset = false;
  }

  void Reset()
  {
    mReset = true;
  }

  bool HasBeenReset()
  {
    return mReset;
  }

  // Helper functions for implementation.
  bool
  operator==(const TextTrackCue& rhs) const
  {
    return mId.Equals(rhs.mId);
  }

  const nsAString& Id() const
  {
    return mId;
  }

  void SetTrack(TextTrack* aTextTrack)
  {
    mTrack = aTextTrack;
  }

  /**
   * Produces a tree of anonymous content based on the tree of the processed
   * cue text.
   *
   * Returns a DocumentFragment that is the head of the tree of anonymous
   * content.
   */
  already_AddRefed<DocumentFragment> GetCueAsHTML();

  void SetTrackElement(HTMLTrackElement* aTrackElement);

private:
  void SetDefaultCueSettings();
  nsresult StashDocument();

  nsRefPtr<nsIDocument> mDocument;
  nsString mText;
  double mStartTime;
  double mEndTime;

  nsRefPtr<TextTrack> mTrack;
  nsRefPtr<HTMLTrackElement> mTrackElement;
  nsString mId;
  int32_t mPosition;
  AlignSetting mPositionAlign;
  int32_t mSize;
  bool mPauseOnExit;
  bool mSnapToLines;
  nsRefPtr<TextTrackRegion> mRegion;
  DirectionSetting mVertical;
  bool mLineIsAutoKeyword;
  long mLineLong;
  AlignSetting mAlign;
  AlignSetting mLineAlign;

  // Holds the computed DOM elements that represent the parsed cue text.
  // http://www.whatwg.org/specs/web-apps/current-work/#text-track-cue-display-state
  nsRefPtr<nsGenericHTMLElement> mDisplayState;
  // Tells whether or not we need to recompute mDisplayState. This is set
  // anytime a property that relates to the display of the TextTrackCue is
  // changed.
  bool mReset;

  static StaticRefPtr<nsIWebVTTParserWrapper> sParserWrapper;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackCue_h
