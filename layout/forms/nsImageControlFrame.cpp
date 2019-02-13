/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsImageFrame.h"
#include "nsIFormControlFrame.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsFormControlFrame.h"
#include "nsLayoutUtils.h"
#include "mozilla/MouseEvents.h"
#include "nsIContent.h"

using namespace mozilla;

typedef nsImageFrame nsImageControlFrameSuper;
class nsImageControlFrame : public nsImageControlFrameSuper,
                            public nsIFormControlFrame
{
public:
  explicit nsImageControlFrame(nsStyleContext* aContext);
  ~nsImageControlFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  virtual void Reflow(nsPresContext*           aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus) override;

  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual nsIAtom* GetType() const override;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("ImageControl"), aResult);
  }
#endif

  virtual nsresult GetCursor(const nsPoint&    aPoint,
                             nsIFrame::Cursor& aCursor) override;
  // nsIFormContromFrame
  virtual void SetFocus(bool aOn, bool aRepaint) override;
  virtual nsresult SetFormProperty(nsIAtom* aName, 
                                   const nsAString& aValue) override;
};


nsImageControlFrame::nsImageControlFrame(nsStyleContext* aContext):
  nsImageControlFrameSuper(aContext)
{
}

nsImageControlFrame::~nsImageControlFrame()
{
}

void
nsImageControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  if (!GetPrevInFlow()) {
    nsFormControlFrame::RegUnRegAccessKey(this, false);
  }
  nsImageControlFrameSuper::DestroyFrom(aDestructRoot);
}

nsIFrame*
NS_NewImageControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsImageControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsImageControlFrame)

void
nsImageControlFrame::Init(nsIContent*       aContent,
                          nsContainerFrame* aParent,
                          nsIFrame*         aPrevInFlow)
{
  nsImageControlFrameSuper::Init(aContent, aParent, aPrevInFlow);

  if (aPrevInFlow) {
    return;
  }
  
  mContent->SetProperty(nsGkAtoms::imageClickedPoint,
                        new nsIntPoint(0, 0),
                        nsINode::DeleteProperty<nsIntPoint>);
}

NS_QUERYFRAME_HEAD(nsImageControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsImageControlFrameSuper)

#ifdef ACCESSIBILITY
a11y::AccType
nsImageControlFrame::AccessibleType()
{
  if (mContent->IsAnyOfHTMLElements(nsGkAtoms::button, nsGkAtoms::input)) {
    return a11y::eHTMLButtonType;
  }

  return a11y::eNoType;
}
#endif

nsIAtom*
nsImageControlFrame::GetType() const
{
  return nsGkAtoms::imageControlFrame; 
}

void
nsImageControlFrame::Reflow(nsPresContext*           aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsImageControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  if (!GetPrevInFlow() && (mState & NS_FRAME_FIRST_REFLOW)) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }
  return nsImageControlFrameSuper::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

nsresult 
nsImageControlFrame::HandleEvent(nsPresContext* aPresContext,
                                 WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // Don't do anything if the event has already been handled by someone
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  // do we have user-input style?
  const nsStyleUserInterface* uiStyle = StyleUserInterface();
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) { // XXX cache disabled
    return NS_OK;
  }

  *aEventStatus = nsEventStatus_eIgnore;

  if (aEvent->message == NS_MOUSE_BUTTON_UP &&
      aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton) {
    // Store click point for HTMLInputElement::SubmitNamesValues
    // Do this on MouseUp because the specs don't say and that's what IE does
    nsIntPoint* lastClickPoint =
      static_cast<nsIntPoint*>
                 (mContent->GetProperty(nsGkAtoms::imageClickedPoint));
    if (lastClickPoint) {
      // normally lastClickedPoint is not null, as it's allocated in Init()
      nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
      TranslateEventCoords(pt, *lastClickPoint);
    }
  }
  return nsImageControlFrameSuper::HandleEvent(aPresContext, aEvent,
                                               aEventStatus);
}

void 
nsImageControlFrame::SetFocus(bool aOn, bool aRepaint)
{
}

nsresult
nsImageControlFrame::GetCursor(const nsPoint&    aPoint,
                               nsIFrame::Cursor& aCursor)
{
  // Use style defined cursor if one is provided, otherwise when
  // the cursor style is "auto" we use the pointer cursor.
  FillCursorInformationFromStyle(StyleUserInterface(), aCursor);

  if (NS_STYLE_CURSOR_AUTO == aCursor.mCursor) {
    aCursor.mCursor = NS_STYLE_CURSOR_POINTER;
  }

  return NS_OK;
}

nsresult
nsImageControlFrame::SetFormProperty(nsIAtom* aName,
                                     const nsAString& aValue)
{
  return NS_OK;
}
