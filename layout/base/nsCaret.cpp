/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the caret is the text cursor used, e.g., when editing */

#include "nsCOMPtr.h"

#include "nsITimer.h"

#include "nsFrameSelection.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMNode.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsRenderingContext.h"
#include "nsPresContext.h"
#include "nsBlockFrame.h"
#include "nsISelectionController.h"
#include "nsCaret.h"
#include "nsTextFrame.h"
#include "nsXULPopupManager.h"
#include "nsMenuPopupFrame.h"
#include "nsTextFragment.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Selection.h"
#include <algorithm>

// The bidi indicator hangs off the caret to one side, to show which
// direction the typing is in. It needs to be at least 2x2 to avoid looking like 
// an insignificant dot
static const int32_t kMinBidiIndicatorPixels = 2;

#include "nsIBidiKeyboard.h"
#include "nsContentUtils.h"

using namespace mozilla;

/**
 * Find the first frame in an in-order traversal of the frame subtree rooted
 * at aFrame which is either a text frame logically at the end of a line,
 * or which is aStopAtFrame. Return null if no such frame is found. We don't
 * descend into the children of non-eLineParticipant frames.
 */
static nsIFrame*
CheckForTrailingTextFrameRecursive(nsIFrame* aFrame, nsIFrame* aStopAtFrame)
{
  if (aFrame == aStopAtFrame ||
      ((aFrame->GetType() == nsGkAtoms::textFrame &&
       (static_cast<nsTextFrame*>(aFrame))->IsAtEndOfLine())))
    return aFrame;
  if (!aFrame->IsFrameOfType(nsIFrame::eLineParticipant))
    return nullptr;

  for (nsIFrame* f = aFrame->GetFirstPrincipalChild(); f; f = f->GetNextSibling())
  {
    nsIFrame* r = CheckForTrailingTextFrameRecursive(f, aStopAtFrame);
    if (r)
      return r;
  }
  return nullptr;
}

static nsLineBox*
FindContainingLine(nsIFrame* aFrame)
{
  while (aFrame && aFrame->IsFrameOfType(nsIFrame::eLineParticipant))
  {
    nsIFrame* parent = aFrame->GetParent();
    nsBlockFrame* blockParent = nsLayoutUtils::GetAsBlock(parent);
    if (blockParent)
    {
      bool isValid;
      nsBlockInFlowLineIterator iter(blockParent, aFrame, &isValid);
      return isValid ? iter.GetLine().get() : nullptr;
    }
    aFrame = parent;
  }
  return nullptr;
}

static void
AdjustCaretFrameForLineEnd(nsIFrame** aFrame, int32_t* aOffset)
{
  nsLineBox* line = FindContainingLine(*aFrame);
  if (!line)
    return;
  int32_t count = line->GetChildCount();
  for (nsIFrame* f = line->mFirstChild; count > 0; --count, f = f->GetNextSibling())
  {
    nsIFrame* r = CheckForTrailingTextFrameRecursive(f, *aFrame);
    if (r == *aFrame)
      return;
    if (r)
    {
      *aFrame = r;
      NS_ASSERTION(r->GetType() == nsGkAtoms::textFrame, "Expected text frame");
      *aOffset = (static_cast<nsTextFrame*>(r))->GetContentEnd();
      return;
    }
  }
}

//-----------------------------------------------------------------------------

nsCaret::nsCaret()
: mPresShell(nullptr)
, mBlinkRate(500)
, mVisible(false)
, mDrawn(false)
, mPendingDraw(false)
, mReadOnly(false)
, mShowDuringSelection(false)
, mIgnoreUserModify(true)
, mKeyboardRTL(false)
, mLastBidiLevel(0)
, mLastContentOffset(0)
, mLastHint(nsFrameSelection::HINTLEFT)
{
}

//-----------------------------------------------------------------------------
nsCaret::~nsCaret()
{
  KillTimer();
}

//-----------------------------------------------------------------------------
nsresult nsCaret::Init(nsIPresShell *inPresShell)
{
  NS_ENSURE_ARG(inPresShell);

  mPresShell = do_GetWeakReference(inPresShell);    // the presshell owns us, so no addref
  NS_ASSERTION(mPresShell, "Hey, pres shell should support weak refs");

  // XXX we should just do this LookAndFeel consultation every time
  // we need these values.
  mCaretWidthCSSPx = LookAndFeel::GetInt(LookAndFeel::eIntID_CaretWidth, 1);
  mCaretAspectRatio =
    LookAndFeel::GetFloat(LookAndFeel::eFloatID_CaretAspectRatio, 0.0f);

  mBlinkRate = static_cast<uint32_t>(
    LookAndFeel::GetInt(LookAndFeel::eIntID_CaretBlinkTime, mBlinkRate));
  mShowDuringSelection =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ShowCaretDuringSelection,
                        mShowDuringSelection ? 1 : 0) != 0;

  // get the selection from the pres shell, and set ourselves up as a selection
  // listener

  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mPresShell);
  if (!selCon)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISelection> domSelection;
  nsresult rv = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                     getter_AddRefs(domSelection));
  if (NS_FAILED(rv))
    return rv;
  if (!domSelection)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISelectionPrivate> privateSelection = do_QueryInterface(domSelection);
  if (privateSelection)
    privateSelection->AddSelectionListener(this);
  mDomSelectionWeak = do_GetWeakReference(domSelection);
  
  // set up the blink timer
  if (mVisible)
  {
    StartBlinking();
  }
  mBidiUI = Preferences::GetBool("bidi.browser.ui");

  return NS_OK;
}

static bool
DrawCJKCaret(nsIFrame* aFrame, int32_t aOffset)
{
  nsIContent* content = aFrame->GetContent();
  const nsTextFragment* frag = content->GetText();
  if (!frag)
    return false;
  if (aOffset < 0 || uint32_t(aOffset) >= frag->GetLength())
    return false;
  char16_t ch = frag->CharAt(aOffset);
  return 0x2e80 <= ch && ch <= 0xd7ff;
}

nsCaret::Metrics nsCaret::ComputeMetrics(nsIFrame* aFrame, int32_t aOffset, nscoord aCaretHeight)
{
  // Compute nominal sizes in appunits
  nscoord caretWidth = (aCaretHeight * mCaretAspectRatio) +
                       nsPresContext::CSSPixelsToAppUnits(mCaretWidthCSSPx);

  if (DrawCJKCaret(aFrame, aOffset)) {
    caretWidth += nsPresContext::CSSPixelsToAppUnits(1);
  }
  nscoord bidiIndicatorSize = nsPresContext::CSSPixelsToAppUnits(kMinBidiIndicatorPixels);
  bidiIndicatorSize = std::max(caretWidth, bidiIndicatorSize);

  // Round them to device pixels. Always round down, except that anything
  // between 0 and 1 goes up to 1 so we don't let the caret disappear.
  int32_t tpp = aFrame->PresContext()->AppUnitsPerDevPixel();
  Metrics result;
  result.mCaretWidth = NS_ROUND_BORDER_TO_PIXELS(caretWidth, tpp);
  result.mBidiIndicatorSize = NS_ROUND_BORDER_TO_PIXELS(bidiIndicatorSize, tpp);
  return result;
}

//-----------------------------------------------------------------------------
void nsCaret::Terminate()
{
  // this doesn't erase the caret if it's drawn. Should it? We might not have
  // a good drawing environment during teardown.
  
  KillTimer();
  mBlinkTimer = nullptr;

  // unregiser ourselves as a selection listener
  nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
  nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryInterface(domSelection));
  if (privateSelection)
    privateSelection->RemoveSelectionListener(this);
  mDomSelectionWeak = nullptr;
  mPresShell = nullptr;

  mLastContent = nullptr;
}

//-----------------------------------------------------------------------------
NS_IMPL_ISUPPORTS(nsCaret, nsISelectionListener)

//-----------------------------------------------------------------------------
nsISelection* nsCaret::GetCaretDOMSelection()
{
  nsCOMPtr<nsISelection> sel(do_QueryReferent(mDomSelectionWeak));
  return sel;  
}

//-----------------------------------------------------------------------------
nsresult nsCaret::SetCaretDOMSelection(nsISelection *aDOMSel)
{
  NS_ENSURE_ARG_POINTER(aDOMSel);
  mDomSelectionWeak = do_GetWeakReference(aDOMSel);   // weak reference to pres shell
  if (mVisible)
  {
    // Stop the caret from blinking in its previous location.
    StopBlinking();
    // Start the caret blinking in the new location.
    StartBlinking();
  }
  return NS_OK;
}


//-----------------------------------------------------------------------------
void nsCaret::SetCaretVisible(bool inMakeVisible)
{
  mVisible = inMakeVisible;
  if (mVisible) {
    SetIgnoreUserModify(true);
    StartBlinking();
  } else {
    StopBlinking();
    SetIgnoreUserModify(false);
  }
}


//-----------------------------------------------------------------------------
nsresult nsCaret::GetCaretVisible(bool *outMakeVisible)
{
  NS_ENSURE_ARG_POINTER(outMakeVisible);
  *outMakeVisible = (mVisible && MustDrawCaret(true));
  return NS_OK;
}


//-----------------------------------------------------------------------------
void nsCaret::SetCaretReadOnly(bool inMakeReadonly)
{
  mReadOnly = inMakeReadonly;
}

nsresult
nsCaret::GetGeometryForFrame(nsIFrame* aFrame,
                             int32_t   aFrameOffset,
                             nsRect*   aRect,
                             nscoord*  aBidiIndicatorSize)
{
  nsPoint framePos(0, 0);
  nsresult rv = aFrame->GetPointFromOffset(aFrameOffset, &framePos);
  if (NS_FAILED(rv))
    return rv;

  nsIFrame* frame = aFrame->GetContentInsertionFrame();
  if (!frame) {
    frame = aFrame;
  }
  NS_ASSERTION(!(frame->GetStateBits() & NS_FRAME_IN_REFLOW),
               "We should not be in the middle of reflow");
  nscoord baseline = frame->GetCaretBaseline();
  nscoord ascent = 0, descent = 0;
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(fm),
    nsLayoutUtils::FontSizeInflationFor(aFrame));
  NS_ASSERTION(fm, "We should be able to get the font metrics");
  if (fm) {
    ascent = fm->MaxAscent();
    descent = fm->MaxDescent();
  }
  nscoord height = ascent + descent;
  framePos.y = baseline - ascent;
  Metrics caretMetrics = ComputeMetrics(aFrame, aFrameOffset, height);
  *aRect = nsRect(framePos, nsSize(caretMetrics.mCaretWidth, height));

  // Clamp the x-position to be within our scroll frame. If we don't, then it
  // clips us, and we don't appear at all. See bug 335560.
  nsIFrame *scrollFrame =
    nsLayoutUtils::GetClosestFrameOfType(aFrame, nsGkAtoms::scrollFrame);
  if (scrollFrame) {
    // First, use the scrollFrame to get at the scrollable view that we're in.
    nsIScrollableFrame *sf = do_QueryFrame(scrollFrame);
    nsIFrame *scrolled = sf->GetScrolledFrame();
    nsRect caretInScroll = *aRect + aFrame->GetOffsetTo(scrolled);

    // Now see if thet caret extends beyond the view's bounds. If it does,
    // then snap it back, put it as close to the edge as it can.
    nscoord overflow = caretInScroll.XMost() -
      scrolled->GetVisualOverflowRectRelativeToSelf().width;
    if (overflow > 0)
      aRect->x -= overflow;
  }

  if (aBidiIndicatorSize)
    *aBidiIndicatorSize = caretMetrics.mBidiIndicatorSize;

  return NS_OK;
}

nsIFrame* nsCaret::GetGeometry(nsISelection* aSelection, nsRect* aRect,
                               nscoord* aBidiIndicatorSize)
{
  nsCOMPtr<nsIDOMNode> focusNode;
  nsresult rv = aSelection->GetFocusNode(getter_AddRefs(focusNode));
  if (NS_FAILED(rv) || !focusNode)
    return nullptr;

  int32_t focusOffset;
  rv = aSelection->GetFocusOffset(&focusOffset);
  if (NS_FAILED(rv))
    return nullptr;
    
  nsCOMPtr<nsIContent> contentNode = do_QueryInterface(focusNode);
  if (!contentNode)
    return nullptr;

  nsRefPtr<nsFrameSelection> frameSelection = GetFrameSelection();
  if (!frameSelection)
    return nullptr;
  uint8_t bidiLevel = frameSelection->GetCaretBidiLevel();
  nsIFrame* frame;
  int32_t frameOffset;
  rv = GetCaretFrameForNodeOffset(contentNode, focusOffset,
                                  frameSelection->GetHint(), bidiLevel,
                                  &frame, &frameOffset);
  if (NS_FAILED(rv) || !frame)
    return nullptr;

  GetGeometryForFrame(frame, frameOffset, aRect, aBidiIndicatorSize);
  return frame;
}

void nsCaret::DrawCaretAfterBriefDelay()
{
  // Make sure readonly caret gets drawn again if it needs to be
  if (!mBlinkTimer) {
    nsresult  err;
    mBlinkTimer = do_CreateInstance("@mozilla.org/timer;1", &err);    
    if (NS_FAILED(err))
      return;
  }    

  mBlinkTimer->InitWithFuncCallback(CaretBlinkCallback, this, 0,
                                    nsITimer::TYPE_ONE_SHOT);
}

void nsCaret::EraseCaret()
{
  if (mDrawn) {
    DrawCaret(true);
    if (mReadOnly && mBlinkRate) {
      // If readonly we don't have a blink timer set, so caret won't
      // be redrawn automatically. We need to force the caret to get
      // redrawn right after the paint
      DrawCaretAfterBriefDelay();
    }
  }
}

void nsCaret::SetVisibilityDuringSelection(bool aVisibility) 
{
  mShowDuringSelection = aVisibility;
}

nsresult nsCaret::DrawAtPosition(nsIDOMNode* aNode, int32_t aOffset)
{
  NS_ENSURE_ARG(aNode);

  uint8_t bidiLevel;
  nsRefPtr<nsFrameSelection> frameSelection = GetFrameSelection();
  if (!frameSelection)
    return NS_ERROR_FAILURE;
  bidiLevel = frameSelection->GetCaretBidiLevel();

  // DrawAtPosition is used by consumers who want us to stay drawn where they
  // tell us. Setting mBlinkRate to 0 tells us to not set a timer to erase
  // ourselves, our consumer will take care of that.
  mBlinkRate = 0;

  nsCOMPtr<nsIContent> node = do_QueryInterface(aNode);
  nsresult rv = DrawAtPositionWithHint(aNode, aOffset,
                                       nsFrameSelection::GetHintForPosition(node, aOffset),
                                       bidiLevel, true)
    ?  NS_OK : NS_ERROR_FAILURE;
  ToggleDrawnStatus();
  return rv;
}

nsIFrame * nsCaret::GetCaretFrame(int32_t *aOffset)
{
  // Return null if we're not drawn to prevent anybody from trying to draw us.
  if (!mDrawn)
    return nullptr;

  // Recompute the frame that we're supposed to draw in to guarantee that
  // we're not going to try to draw into a stale (dead) frame.
  int32_t offset;
  nsIFrame *frame = nullptr;
  nsresult rv = GetCaretFrameForNodeOffset(mLastContent, mLastContentOffset,
                                           mLastHint, mLastBidiLevel, &frame,
                                           &offset);
  if (NS_FAILED(rv))
    return nullptr;

  if (aOffset) {
    *aOffset = offset;
  }
  return frame;
}

void nsCaret::InvalidateOutsideCaret()
{
  nsIFrame *frame = GetCaretFrame();

  // Only invalidate if we are not fully contained by our frame's rect.
  if (frame && !frame->GetVisualOverflowRect().Contains(GetCaretRect())) {
    frame->SchedulePaint();
  }
}

void nsCaret::UpdateCaretPosition()
{
  // We'll recalculate anyway if we're not drawn right now.
  if (!mDrawn)
    return;

  // A trick! Make the DrawCaret code recalculate the caret's current
  // position.
  mDrawn = false;
  DrawCaret(false);
}

void nsCaret::PaintCaret(nsDisplayListBuilder *aBuilder,
                         nsRenderingContext *aCtx,
                         nsIFrame* aForFrame,
                         const nsPoint &aOffset)
{
  NS_ASSERTION(mDrawn, "The caret shouldn't be drawing");

  const nsRect drawCaretRect = mCaretRect + aOffset;
  int32_t contentOffset;

#ifdef DEBUG
  nsIFrame* frame =
#endif
    GetCaretFrame(&contentOffset);
  NS_ASSERTION(frame == aForFrame, "We're referring different frame");
  // If the offset falls outside of the frame, then don't paint the caret.
  int32_t startOffset, endOffset;
  if (aForFrame->GetType() == nsGkAtoms::textFrame &&
      (NS_FAILED(aForFrame->GetOffsets(startOffset, endOffset)) ||
      startOffset > contentOffset ||
      endOffset < contentOffset)) {
    return;
  }
  nscolor foregroundColor = aForFrame->GetCaretColorAt(contentOffset);

  aCtx->SetColor(foregroundColor);
  aCtx->FillRect(drawCaretRect);
  if (!GetHookRect().IsEmpty())
    aCtx->FillRect(GetHookRect() + aOffset);
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::NotifySelectionChanged(nsIDOMDocument *, nsISelection *aDomSel, int16_t aReason)
{
  if (aReason & nsISelectionListener::MOUSEUP_REASON)//this wont do
    return NS_OK;

  nsCOMPtr<nsISelection> domSel(do_QueryReferent(mDomSelectionWeak));

  // The same caret is shared amongst the document and any text widgets it
  // may contain. This means that the caret could get notifications from
  // multiple selections.
  //
  // If this notification is for a selection that is not the one the
  // the caret is currently interested in (mDomSelectionWeak), then there
  // is nothing to do!

  if (domSel != aDomSel)
    return NS_OK;

  if (mVisible)
  {
    // Stop the caret from blinking in its previous location.
    StopBlinking();

    // Start the caret blinking in the new location.
    StartBlinking();
  }

  return NS_OK;
}


//-----------------------------------------------------------------------------
void nsCaret::KillTimer()
{
  if (mBlinkTimer)
  {
    mBlinkTimer->Cancel();
  }
}


//-----------------------------------------------------------------------------
nsresult nsCaret::PrimeTimer()
{
  // set up the blink timer
  if (!mReadOnly && mBlinkRate > 0)
  {
    if (!mBlinkTimer) {
      nsresult  err;
      mBlinkTimer = do_CreateInstance("@mozilla.org/timer;1", &err);    
      if (NS_FAILED(err))
        return err;
    }    

    mBlinkTimer->InitWithFuncCallback(CaretBlinkCallback, this, mBlinkRate,
                                      nsITimer::TYPE_REPEATING_SLACK);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
void nsCaret::StartBlinking()
{
  if (mReadOnly) {
    // Make sure the one draw command we use for a readonly caret isn't
    // done until the selection is set
    DrawCaretAfterBriefDelay();
    return;
  }
  PrimeTimer();

  // If we are currently drawn, then the second call to DrawCaret below will
  // actually erase the caret. That would cause the caret to spend an "off"
  // cycle before it appears, which is not really what we want. This first
  // call to DrawCaret makes sure that the first cycle after a call to
  // StartBlinking is an "on" cycle.
  if (mDrawn)
    DrawCaret(true);

  DrawCaret(true);    // draw it right away
}


//-----------------------------------------------------------------------------
void nsCaret::StopBlinking()
{
  if (mDrawn)     // erase the caret if necessary
    DrawCaret(true);

  NS_ASSERTION(!mDrawn, "Caret still drawn after StopBlinking().");
  KillTimer();
}

bool
nsCaret::DrawAtPositionWithHint(nsIDOMNode*             aNode,
                                int32_t                 aOffset,
                                nsFrameSelection::HINT  aFrameHint,
                                uint8_t                 aBidiLevel,
                                bool                    aInvalidate)
{
  nsCOMPtr<nsIContent> contentNode = do_QueryInterface(aNode);
  if (!contentNode)
    return false;

  nsIFrame* theFrame = nullptr;
  int32_t   theFrameOffset = 0;

  nsresult rv = GetCaretFrameForNodeOffset(contentNode, aOffset, aFrameHint, aBidiLevel,
                                           &theFrame, &theFrameOffset);
  if (NS_FAILED(rv) || !theFrame)
    return false;

  // now we have a frame, check whether it's appropriate to show the caret here
  const nsStyleUserInterface* userinterface = theFrame->StyleUserInterface();
  if ((!mIgnoreUserModify &&
       userinterface->mUserModify == NS_STYLE_USER_MODIFY_READ_ONLY) ||
      (userinterface->mUserInput == NS_STYLE_USER_INPUT_NONE) ||
      (userinterface->mUserInput == NS_STYLE_USER_INPUT_DISABLED))
  {
    return false;
  }  

  if (!mDrawn)
  {
    // save stuff so we can figure out what frame we're in later.
    mLastContent = contentNode;
    mLastContentOffset = aOffset;
    mLastHint = aFrameHint;
    mLastBidiLevel = aBidiLevel;

    // If there has been a reflow, set the caret Bidi level to the level of the current frame
    if (aBidiLevel & BIDI_LEVEL_UNDEFINED) {
      nsRefPtr<nsFrameSelection> frameSelection = GetFrameSelection();
      if (!frameSelection)
        return false;
      frameSelection->SetCaretBidiLevel(NS_GET_EMBEDDING_LEVEL(theFrame));
    }

    // Only update the caret's rect when we're not currently drawn.
    if (!UpdateCaretRects(theFrame, theFrameOffset))
      return false;
  }

  if (aInvalidate)
    theFrame->SchedulePaint();

  return true;
}

nsresult 
nsCaret::GetCaretFrameForNodeOffset(nsIContent*             aContentNode,
                                    int32_t                 aOffset,
                                    nsFrameSelection::HINT aFrameHint,
                                    uint8_t                 aBidiLevel,
                                    nsIFrame**              aReturnFrame,
                                    int32_t*                aReturnOffset)
{

  //get frame selection and find out what frame to use...
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell)
    return NS_ERROR_FAILURE;

  if (!aContentNode || !aContentNode->IsInDoc() ||
      presShell->GetDocument() != aContentNode->GetCurrentDoc())
    return NS_ERROR_FAILURE;

  nsRefPtr<nsFrameSelection> frameSelection = GetFrameSelection();
  if (!frameSelection)
    return NS_ERROR_FAILURE;

  nsIFrame* theFrame = nullptr;
  int32_t   theFrameOffset = 0;

  theFrame = frameSelection->GetFrameForNodeOffset(aContentNode, aOffset,
                                                   aFrameHint, &theFrameOffset);
  if (!theFrame)
    return NS_ERROR_FAILURE;

  // if theFrame is after a text frame that's logically at the end of the line
  // (e.g. if theFrame is a <br> frame), then put the caret at the end of
  // that text frame instead. This way, the caret will be positioned as if
  // trailing whitespace was not trimmed.
  AdjustCaretFrameForLineEnd(&theFrame, &theFrameOffset);
  
  // Mamdouh : modification of the caret to work at rtl and ltr with Bidi
  //
  // Direction Style from visibility->mDirection
  // ------------------
  // NS_STYLE_DIRECTION_LTR : LTR or Default
  // NS_STYLE_DIRECTION_RTL
  // NS_STYLE_DIRECTION_INHERIT
  if (mBidiUI)
  {
    // If there has been a reflow, take the caret Bidi level to be the level of the current frame
    if (aBidiLevel & BIDI_LEVEL_UNDEFINED)
      aBidiLevel = NS_GET_EMBEDDING_LEVEL(theFrame);

    int32_t start;
    int32_t end;
    nsIFrame* frameBefore;
    nsIFrame* frameAfter;
    uint8_t levelBefore;     // Bidi level of the character before the caret
    uint8_t levelAfter;      // Bidi level of the character after the caret

    theFrame->GetOffsets(start, end);
    if (start == 0 || end == 0 || start == theFrameOffset || end == theFrameOffset)
    {
      nsPrevNextBidiLevels levels = frameSelection->
        GetPrevNextBidiLevels(aContentNode, aOffset, false);
    
      /* Boundary condition, we need to know the Bidi levels of the characters before and after the caret */
      if (levels.mFrameBefore || levels.mFrameAfter)
      {
        frameBefore = levels.mFrameBefore;
        frameAfter = levels.mFrameAfter;
        levelBefore = levels.mLevelBefore;
        levelAfter = levels.mLevelAfter;

        if ((levelBefore != levelAfter) || (aBidiLevel != levelBefore))
        {
          aBidiLevel = std::max(aBidiLevel, std::min(levelBefore, levelAfter));                                  // rule c3
          aBidiLevel = std::min(aBidiLevel, std::max(levelBefore, levelAfter));                                  // rule c4
          if (aBidiLevel == levelBefore                                                                      // rule c1
              || (aBidiLevel > levelBefore && aBidiLevel < levelAfter && !((aBidiLevel ^ levelBefore) & 1))    // rule c5
              || (aBidiLevel < levelBefore && aBidiLevel > levelAfter && !((aBidiLevel ^ levelBefore) & 1)))  // rule c9
          {
            if (theFrame != frameBefore)
            {
              if (frameBefore) // if there is a frameBefore, move into it
              {
                theFrame = frameBefore;
                theFrame->GetOffsets(start, end);
                theFrameOffset = end;
              }
              else 
              {
                // if there is no frameBefore, we must be at the beginning of the line
                // so we stay with the current frame.
                // Exception: when the first frame on the line has a different Bidi level from the paragraph level, there is no
                // real frame for the caret to be in. We have to find the visually first frame on the line.
                uint8_t baseLevel = NS_GET_BASE_LEVEL(frameAfter);
                if (baseLevel != levelAfter)
                {
                  nsPeekOffsetStruct pos(eSelectBeginLine, eDirPrevious, 0, 0, false, true, false, true);
                  if (NS_SUCCEEDED(frameAfter->PeekOffset(&pos))) {
                    theFrame = pos.mResultFrame;
                    theFrameOffset = pos.mContentOffset;
                  }
                }
              }
            }
          }
          else if (aBidiLevel == levelAfter                                                                     // rule c2
                   || (aBidiLevel > levelBefore && aBidiLevel < levelAfter && !((aBidiLevel ^ levelAfter) & 1))   // rule c6
                   || (aBidiLevel < levelBefore && aBidiLevel > levelAfter && !((aBidiLevel ^ levelAfter) & 1)))  // rule c10
          {
            if (theFrame != frameAfter)
            {
              if (frameAfter)
              {
                // if there is a frameAfter, move into it
                theFrame = frameAfter;
                theFrame->GetOffsets(start, end);
                theFrameOffset = start;
              }
              else 
              {
                // if there is no frameAfter, we must be at the end of the line
                // so we stay with the current frame.
                // Exception: when the last frame on the line has a different Bidi level from the paragraph level, there is no
                // real frame for the caret to be in. We have to find the visually last frame on the line.
                uint8_t baseLevel = NS_GET_BASE_LEVEL(frameBefore);
                if (baseLevel != levelBefore)
                {
                  nsPeekOffsetStruct pos(eSelectEndLine, eDirNext, 0, 0, false, true, false, true);
                  if (NS_SUCCEEDED(frameBefore->PeekOffset(&pos))) {
                    theFrame = pos.mResultFrame;
                    theFrameOffset = pos.mContentOffset;
                  }
                }
              }
            }
          }
          else if (aBidiLevel > levelBefore && aBidiLevel < levelAfter  // rule c7/8
                   && !((levelBefore ^ levelAfter) & 1)                 // before and after have the same parity
                   && ((aBidiLevel ^ levelAfter) & 1))                  // caret has different parity
          {
            if (NS_SUCCEEDED(frameSelection->GetFrameFromLevel(frameAfter, eDirNext, aBidiLevel, &theFrame)))
            {
              theFrame->GetOffsets(start, end);
              levelAfter = NS_GET_EMBEDDING_LEVEL(theFrame);
              if (aBidiLevel & 1) // c8: caret to the right of the rightmost character
                theFrameOffset = (levelAfter & 1) ? start : end;
              else               // c7: caret to the left of the leftmost character
                theFrameOffset = (levelAfter & 1) ? end : start;
            }
          }
          else if (aBidiLevel < levelBefore && aBidiLevel > levelAfter  // rule c11/12
                   && !((levelBefore ^ levelAfter) & 1)                 // before and after have the same parity
                   && ((aBidiLevel ^ levelAfter) & 1))                  // caret has different parity
          {
            if (NS_SUCCEEDED(frameSelection->GetFrameFromLevel(frameBefore, eDirPrevious, aBidiLevel, &theFrame)))
            {
              theFrame->GetOffsets(start, end);
              levelBefore = NS_GET_EMBEDDING_LEVEL(theFrame);
              if (aBidiLevel & 1) // c12: caret to the left of the leftmost character
                theFrameOffset = (levelBefore & 1) ? end : start;
              else               // c11: caret to the right of the rightmost character
                theFrameOffset = (levelBefore & 1) ? start : end;
            }
          }   
        }
      }
    }
  }

  NS_ASSERTION(!theFrame || theFrame->PresContext()->PresShell() == presShell,
               "caret frame is in wrong document");
  *aReturnFrame = theFrame;
  *aReturnOffset = theFrameOffset;
  return NS_OK;
}

void
nsCaret::CheckCaretDrawingState()
{
  if (mDrawn) {
    // The caret is drawn; if it shouldn't be, erase it.
    if (!mVisible || !MustDrawCaret(true))
      EraseCaret();
  }
  else
  {
    // The caret is not drawn; if it should be, draw it.
    if (mPendingDraw && (mVisible && MustDrawCaret(true)))
      DrawCaret(true);
  }
}

size_t nsCaret::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t total = aMallocSizeOf(this);
  if (mPresShell) {
    // We only want the size of the nsWeakReference object, not the PresShell
    // (since we don't own the PresShell).
    total += mPresShell->SizeOfOnlyThis(aMallocSizeOf);
  }
  if (mDomSelectionWeak) {
    // We only want size of the nsWeakReference object, not the selection
    // (again, we don't own the selection).
    total += mDomSelectionWeak->SizeOfOnlyThis(aMallocSizeOf);
  }
  if (mBlinkTimer) {
    total += mBlinkTimer->SizeOfIncludingThis(aMallocSizeOf);
  }
  return total;
}

/*-----------------------------------------------------------------------------

  MustDrawCaret
  
  Find out if we need to do any caret drawing. This returns true if
  either:
  a) The caret has been drawn, and we need to erase it.
  b) The caret is not drawn, and the selection is collapsed.
  c) The caret is not hidden due to open XUL popups
     (see IsMenuPopupHidingCaret()).
  
----------------------------------------------------------------------------- */
bool nsCaret::MustDrawCaret(bool aIgnoreDrawnState)
{
  if (!aIgnoreDrawnState && mDrawn)
    return true;

  nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
  if (!domSelection)
    return false;

  bool isCollapsed;
  if (NS_FAILED(domSelection->GetIsCollapsed(&isCollapsed)))
    return false;

  if (mShowDuringSelection)
    return true;      // show the caret even in selections

  if (IsMenuPopupHidingCaret())
    return false;

  return isCollapsed;
}

bool nsCaret::IsMenuPopupHidingCaret()
{
#ifdef MOZ_XUL
  // Check if there are open popups.
  nsXULPopupManager *popMgr = nsXULPopupManager::GetInstance();
  nsTArray<nsIFrame*> popups;
  popMgr->GetVisiblePopups(popups);

  if (popups.Length() == 0)
    return false; // No popups, so caret can't be hidden by them.

  // Get the selection focus content, that's where the caret would 
  // go if it was drawn.
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
  if (!domSelection)
    return true; // No selection/caret to draw.
  domSelection->GetFocusNode(getter_AddRefs(node));
  if (!node)
    return true; // No selection/caret to draw.
  nsCOMPtr<nsIContent> caretContent = do_QueryInterface(node);
  if (!caretContent)
    return true; // No selection/caret to draw.

  // If there's a menu popup open before the popup with
  // the caret, don't show the caret.
  for (uint32_t i=0; i<popups.Length(); i++) {
    nsMenuPopupFrame* popupFrame = static_cast<nsMenuPopupFrame*>(popups[i]);
    nsIContent* popupContent = popupFrame->GetContent();

    if (nsContentUtils::ContentIsDescendantOf(caretContent, popupContent)) {
      // The caret is in this popup. There were no menu popups before this
      // popup, so don't hide the caret.
      return false;
    }

    if (popupFrame->PopupType() == ePopupTypeMenu && !popupFrame->IsContextMenu()) {
      // This is an open menu popup. It does not contain the caret (else we'd
      // have returned above). Even if the caret is in a subsequent popup,
      // or another document/frame, it should be hidden.
      return true;
    }
  }
#endif

  // There are no open menu popups, no need to hide the caret.
  return false;
}

void nsCaret::DrawCaret(bool aInvalidate)
{
  // Do we need to draw the caret at all?
  if (!MustDrawCaret(false))
    return;
  
  // Can we draw the caret now?
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  NS_ENSURE_TRUE_VOID(presShell);
  {
    if (presShell->IsPaintingSuppressed())
    {
      if (!mDrawn)
        mPendingDraw = true;

      // PresShell::UnsuppressAndInvalidate() will call CheckCaretDrawingState()
      // to get us drawn.
      return;
    }
  }

  nsCOMPtr<nsIDOMNode> node;
  int32_t offset;
  nsFrameSelection::HINT hint;
  uint8_t bidiLevel;

  if (!mDrawn)
  {
    nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
    nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryInterface(domSelection));
    if (!privateSelection) return;
    
    bool isCollapsed = false;
    domSelection->GetIsCollapsed(&isCollapsed);
    if (!mShowDuringSelection && !isCollapsed)
      return;

    bool hintRight;
    privateSelection->GetInterlinePosition(&hintRight);//translate hint.
    hint = hintRight ? nsFrameSelection::HINTRIGHT : nsFrameSelection::HINTLEFT;

    // get the node and offset, which is where we want the caret to draw
    domSelection->GetFocusNode(getter_AddRefs(node));
    if (!node)
      return;
    
    if (NS_FAILED(domSelection->GetFocusOffset(&offset)))
      return;

    nsRefPtr<nsFrameSelection> frameSelection = GetFrameSelection();
    if (!frameSelection)
      return;

    bidiLevel = frameSelection->GetCaretBidiLevel();
    mPendingDraw = false;
  }
  else
  {
    if (!mLastContent)
    {
      mDrawn = false;
      return;
    }
    if (!mLastContent->IsInDoc() ||
        presShell->GetDocument() != mLastContent->GetCurrentDoc())
    {
      mLastContent = nullptr;
      mDrawn = false;
      return;
    }
    node = do_QueryInterface(mLastContent);
    offset = mLastContentOffset;
    hint = mLastHint;
    bidiLevel = mLastBidiLevel;
  }

  DrawAtPositionWithHint(node, offset, hint, bidiLevel, aInvalidate);
  ToggleDrawnStatus();
}

bool
nsCaret::UpdateCaretRects(nsIFrame* aFrame, int32_t aFrameOffset)
{
  NS_ASSERTION(aFrame, "Should have a frame here");

  nscoord bidiIndicatorSize;
  nsresult rv =
    GetGeometryForFrame(aFrame, aFrameOffset, &mCaretRect, &bidiIndicatorSize);
  if (NS_FAILED(rv)) {
    return false;
  }

  // on RTL frames the right edge of mCaretRect must be equal to framePos
  const nsStyleVisibility* vis = aFrame->StyleVisibility();
  if (NS_STYLE_DIRECTION_RTL == vis->mDirection)
    mCaretRect.x -= mCaretRect.width;

  mHookRect.SetEmpty();

  // Simon -- make a hook to draw to the left or right of the caret to show keyboard language direction
  bool isCaretRTL = false;
  nsIBidiKeyboard* bidiKeyboard = nsContentUtils::GetBidiKeyboard();
  // if bidiKeyboard->IsLangRTL() fails, there is no way to tell the
  // keyboard direction, or the user has no right-to-left keyboard
  // installed, so we never draw the hook.
  if (bidiKeyboard && NS_SUCCEEDED(bidiKeyboard->IsLangRTL(&isCaretRTL)) &&
      mBidiUI) {
    if (isCaretRTL != mKeyboardRTL) {
      /* if the caret bidi level and the keyboard language direction are not in
       * synch, the keyboard language must have been changed by the
       * user, and if the caret is in a boundary condition (between left-to-right and
       * right-to-left characters) it may have to change position to
       * reflect the location in which the next character typed will
       * appear. We will call |SelectionLanguageChange| and exit
       * without drawing the caret in the old position.
       */ 
      mKeyboardRTL = isCaretRTL;
      nsCOMPtr<nsISelectionPrivate> domSelection = do_QueryReferent(mDomSelectionWeak);
      if (!domSelection ||
          NS_SUCCEEDED(domSelection->SelectionLanguageChange(mKeyboardRTL)))
        return false;
    }
    // If keyboard language is RTL, draw the hook on the left; if LTR, to the right
    // The height of the hook rectangle is the same as the width of the caret
    // rectangle.
    mHookRect.SetRect(mCaretRect.x + ((isCaretRTL) ?
                      bidiIndicatorSize * -1 :
                      mCaretRect.width),
                      mCaretRect.y + bidiIndicatorSize,
                      bidiIndicatorSize,
                      mCaretRect.width);
  }
  return true;
}

//-----------------------------------------------------------------------------
/* static */
void nsCaret::CaretBlinkCallback(nsITimer *aTimer, void *aClosure)
{
  nsCaret   *theCaret = reinterpret_cast<nsCaret*>(aClosure);
  if (!theCaret) return;
  
  theCaret->DrawCaret(true);
}


//-----------------------------------------------------------------------------
nsFrameSelection*
nsCaret::GetFrameSelection()
{
  nsCOMPtr<nsISelection> sel = do_QueryReferent(mDomSelectionWeak);
  if (!sel)
    return nullptr;

  return static_cast<dom::Selection*>(sel.get())->GetFrameSelection();
}

void
nsCaret::SetIgnoreUserModify(bool aIgnoreUserModify)
{
  if (!aIgnoreUserModify && mIgnoreUserModify && mDrawn) {
    // We're turning off mIgnoreUserModify. If the caret's drawn
    // in a read-only node we must erase it, else the next call
    // to DrawCaret() won't erase the old caret, due to the new
    // mIgnoreUserModify value.
    nsIFrame *frame = GetCaretFrame();
    if (frame) {
      const nsStyleUserInterface* userinterface = frame->StyleUserInterface();
      if (userinterface->mUserModify == NS_STYLE_USER_MODIFY_READ_ONLY) {
        StopBlinking();
      }
    }
  }
  mIgnoreUserModify = aIgnoreUserModify;
}
