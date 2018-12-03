/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This frame type is used for input type=date, time, month, week, and
 * datetime-local.
 *
 * NOTE: some of the above-mentioned input types are still to-be-implemented.
 * See nsCSSFrameConstructor::FindInputData, as well as bug 1286182 (date),
 * bug 1306215 (month), bug 1306216 (week) and bug 1306217 (datetime-local).
 */

#ifndef nsDateTimeControlFrame_h__
#define nsDateTimeControlFrame_h__

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
struct DateTimeValue;
}  // namespace dom
}  // namespace mozilla

class nsDateTimeControlFrame final : public nsContainerFrame,
                                     public nsIAnonymousContentCreator {
  typedef mozilla::dom::DateTimeValue DateTimeValue;

  explicit nsDateTimeControlFrame(ComputedStyle* aStyle);

 public:
  friend nsIFrame* NS_NewDateTimeControlFrame(nsIPresShell* aPresShell,
                                              ComputedStyle* aStyle);

  void ContentStatesChanged(mozilla::EventStates aStates) override;
  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) override;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsDateTimeControlFrame)

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("DateTimeControl"), aResult);
  }
#endif

  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  // Reflow
  nscoord GetMinISize(gfxContext* aRenderingContext) override;

  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  bool IsLeafDynamic() const override;

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilter) override;

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  nsIContent* GetInputAreaContent();

  void OnValueChanged();
  void OnMinMaxStepAttrChanged();
  void HandleFocusEvent();
  void HandleBlurEvent();
  bool HasBadInput();

 private:
  class SyncDisabledStateEvent;
  friend class SyncDisabledStateEvent;
  class SyncDisabledStateEvent : public mozilla::Runnable {
   public:
    explicit SyncDisabledStateEvent(nsDateTimeControlFrame* aFrame)
        : mozilla::Runnable("nsDateTimeControlFrame::SyncDisabledStateEvent"),
          mFrame(aFrame) {}

    NS_IMETHOD Run() override {
      nsDateTimeControlFrame* frame =
          static_cast<nsDateTimeControlFrame*>(mFrame.GetFrame());
      NS_ENSURE_STATE(frame);

      frame->SyncDisabledState();
      return NS_OK;
    }

   private:
    WeakFrame mFrame;
  };

  /**
   * Sync the disabled state of the anonymous children up with our content's.
   */
  void SyncDisabledState();

  mozilla::dom::Element* GetInputAreaContentAsElement();

  // Anonymous child which is bound via XBL to an element that wraps the input
  // area and reset button.
  RefPtr<mozilla::dom::Element> mInputAreaContent;
};

#endif  // nsDateTimeControlFrame_h__
