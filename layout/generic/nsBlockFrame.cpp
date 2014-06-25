/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object for CSS display:block, inline-block, and list-item
 * boxes, also used for various anonymous boxes
 */

#include "nsBlockFrame.h"

#include "mozilla/DebugOnly.h"

#include "nsCOMPtr.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsBlockReflowContext.h"
#include "nsBlockReflowState.h"
#include "nsBulletFrame.h"
#include "nsLineBox.h"
#include "nsLineLayout.h"
#include "nsPlaceholderFrame.h"
#include "nsStyleConsts.h"
#include "nsFrameManager.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleContext.h"
#include "nsHTMLParts.h"
#include "nsGkAtoms.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "prprf.h"
#include "nsFloatManager.h"
#include "prenv.h"
#include "plstr.h"
#include "nsError.h"
#include "nsAutoPtr.h"
#include "nsIScrollableFrame.h"
#include <algorithm>
#ifdef ACCESSIBILITY
#include "nsIDOMHTMLDocument.h"
#endif
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSFrameConstructor.h"
#include "nsRenderingContext.h"
#include "TextOverflow.h"
#include "nsIFrameInlines.h"
#include "CounterStyleManager.h"

#include "nsBidiPresUtils.h"

static const int MIN_LINES_NEEDING_CURSOR = 20;

static const char16_t kDiscCharacter = 0x2022;

#define DISABLE_FLOAT_BREAKING_IN_COLUMNS

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::layout;

static void MarkAllDescendantLinesDirty(nsBlockFrame* aBlock)
{
  nsLineList::iterator line = aBlock->begin_lines();
  nsLineList::iterator endLine = aBlock->end_lines();
  while (line != endLine) {
    if (line->IsBlock()) {
      nsIFrame* f = line->mFirstChild;
      nsBlockFrame* bf = nsLayoutUtils::GetAsBlock(f);
      if (bf) {
        MarkAllDescendantLinesDirty(bf);
      }
    }
    line->MarkDirty();
    ++line;
  }
}

static void MarkSameFloatManagerLinesDirty(nsBlockFrame* aBlock)
{
  nsBlockFrame* blockWithFloatMgr = aBlock;
  while (!(blockWithFloatMgr->GetStateBits() & NS_BLOCK_FLOAT_MGR)) {
    nsBlockFrame* bf = nsLayoutUtils::GetAsBlock(blockWithFloatMgr->GetParent());
    if (!bf) {
      break;
    }
    blockWithFloatMgr = bf;
  }
    
  // Mark every line at and below the line where the float was
  // dirty, and mark their lines dirty too. We could probably do
  // something more efficient --- e.g., just dirty the lines that intersect
  // the float vertically.
  MarkAllDescendantLinesDirty(blockWithFloatMgr);
}

/**
 * Returns true if aFrame is a block that has one or more float children.
 */
static bool BlockHasAnyFloats(nsIFrame* aFrame)
{
  nsBlockFrame* block = nsLayoutUtils::GetAsBlock(aFrame);
  if (!block)
    return false;
  if (block->GetFirstChild(nsIFrame::kFloatList))
    return true;
    
  nsLineList::iterator line = block->begin_lines();
  nsLineList::iterator endLine = block->end_lines();
  while (line != endLine) {
    if (line->IsBlock() && BlockHasAnyFloats(line->mFirstChild))
      return true;
    ++line;
  }
  return false;
}

#ifdef DEBUG
#include "nsBlockDebugFlags.h"

bool nsBlockFrame::gLamePaintMetrics;
bool nsBlockFrame::gLameReflowMetrics;
bool nsBlockFrame::gNoisy;
bool nsBlockFrame::gNoisyDamageRepair;
bool nsBlockFrame::gNoisyIntrinsic;
bool nsBlockFrame::gNoisyReflow;
bool nsBlockFrame::gReallyNoisyReflow;
bool nsBlockFrame::gNoisyFloatManager;
bool nsBlockFrame::gVerifyLines;
bool nsBlockFrame::gDisableResizeOpt;

int32_t nsBlockFrame::gNoiseIndent;

struct BlockDebugFlags {
  const char* name;
  bool* on;
};

static const BlockDebugFlags gFlags[] = {
  { "reflow", &nsBlockFrame::gNoisyReflow },
  { "really-noisy-reflow", &nsBlockFrame::gReallyNoisyReflow },
  { "intrinsic", &nsBlockFrame::gNoisyIntrinsic },
  { "float-manager", &nsBlockFrame::gNoisyFloatManager },
  { "verify-lines", &nsBlockFrame::gVerifyLines },
  { "damage-repair", &nsBlockFrame::gNoisyDamageRepair },
  { "lame-paint-metrics", &nsBlockFrame::gLamePaintMetrics },
  { "lame-reflow-metrics", &nsBlockFrame::gLameReflowMetrics },
  { "disable-resize-opt", &nsBlockFrame::gDisableResizeOpt },
};
#define NUM_DEBUG_FLAGS (sizeof(gFlags) / sizeof(gFlags[0]))

static void
ShowDebugFlags()
{
  printf("Here are the available GECKO_BLOCK_DEBUG_FLAGS:\n");
  const BlockDebugFlags* bdf = gFlags;
  const BlockDebugFlags* end = gFlags + NUM_DEBUG_FLAGS;
  for (; bdf < end; bdf++) {
    printf("  %s\n", bdf->name);
  }
  printf("Note: GECKO_BLOCK_DEBUG_FLAGS is a comma separated list of flag\n");
  printf("names (no whitespace)\n");
}

void
nsBlockFrame::InitDebugFlags()
{
  static bool firstTime = true;
  if (firstTime) {
    firstTime = false;
    char* flags = PR_GetEnv("GECKO_BLOCK_DEBUG_FLAGS");
    if (flags) {
      bool error = false;
      for (;;) {
        char* cm = PL_strchr(flags, ',');
        if (cm) *cm = '\0';

        bool found = false;
        const BlockDebugFlags* bdf = gFlags;
        const BlockDebugFlags* end = gFlags + NUM_DEBUG_FLAGS;
        for (; bdf < end; bdf++) {
          if (PL_strcasecmp(bdf->name, flags) == 0) {
            *(bdf->on) = true;
            printf("nsBlockFrame: setting %s debug flag on\n", bdf->name);
            gNoisy = true;
            found = true;
            break;
          }
        }
        if (!found) {
          error = true;
        }

        if (!cm) break;
        *cm = ',';
        flags = cm + 1;
      }
      if (error) {
        ShowDebugFlags();
      }
    }
  }
}

#endif

// add in a sanity check for absurdly deep frame trees.  See bug 42138
// can't just use IsFrameTreeTooDeep() because that method has side effects we don't want
#define MAX_DEPTH_FOR_LIST_RENUMBERING 200  // 200 open displayable tags is pretty unrealistic

//----------------------------------------------------------------------

// Debugging support code

#ifdef DEBUG
const char* nsBlockFrame::kReflowCommandType[] = {
  "ContentChanged",
  "StyleChanged",
  "ReflowDirty",
  "Timeout",
  "UserDefined",
};
#endif

#ifdef REALLY_NOISY_FIRST_LINE
static void
DumpStyleGeneaology(nsIFrame* aFrame, const char* gap)
{
  fputs(gap, stdout);
  nsFrame::ListTag(stdout, aFrame);
  printf(": ");
  nsStyleContext* sc = aFrame->StyleContext();
  while (nullptr != sc) {
    nsStyleContext* psc;
    printf("%p ", sc);
    psc = sc->GetParent();
    sc = psc;
  }
  printf("\n");
}
#endif

#ifdef REFLOW_STATUS_COVERAGE
static void
RecordReflowStatus(bool aChildIsBlock, nsReflowStatus aFrameReflowStatus)
{
  static uint32_t record[2];

  // 0: child-is-block
  // 1: child-is-inline
  int index = 0;
  if (!aChildIsBlock) index |= 1;

  // Compute new status
  uint32_t newS = record[index];
  if (NS_INLINE_IS_BREAK(aFrameReflowStatus)) {
    if (NS_INLINE_IS_BREAK_BEFORE(aFrameReflowStatus)) {
      newS |= 1;
    }
    else if (NS_FRAME_IS_NOT_COMPLETE(aFrameReflowStatus)) {
      newS |= 2;
    }
    else {
      newS |= 4;
    }
  }
  else if (NS_FRAME_IS_NOT_COMPLETE(aFrameReflowStatus)) {
    newS |= 8;
  }
  else {
    newS |= 16;
  }

  // Log updates to the status that yield different values
  if (record[index] != newS) {
    record[index] = newS;
    printf("record(%d): %02x %02x\n", index, record[0], record[1]);
  }
}
#endif

// Destructor function for the overflowLines frame property
static void
DestroyOverflowLines(void* aPropertyValue)
{
  NS_ERROR("Overflow lines should never be destroyed by the FramePropertyTable");
}

NS_DECLARE_FRAME_PROPERTY(OverflowLinesProperty, DestroyOverflowLines)
NS_DECLARE_FRAME_PROPERTY_FRAMELIST(OverflowOutOfFlowsProperty)
NS_DECLARE_FRAME_PROPERTY_FRAMELIST(PushedFloatProperty)
NS_DECLARE_FRAME_PROPERTY_FRAMELIST(OutsideBulletProperty)
NS_DECLARE_FRAME_PROPERTY(InsideBulletProperty, nullptr)
NS_DECLARE_FRAME_PROPERTY(BottomEdgeOfChildrenProperty, nullptr)

//----------------------------------------------------------------------

nsBlockFrame*
NS_NewBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, nsFrameState aFlags)
{
  nsBlockFrame* it = new (aPresShell) nsBlockFrame(aContext);
  it->SetFlags(aFlags);
  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsBlockFrame)

nsBlockFrame::~nsBlockFrame()
{
}

void
nsBlockFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  ClearLineCursor();
  DestroyAbsoluteFrames(aDestructRoot);
  mFloats.DestroyFramesFrom(aDestructRoot);
  nsPresContext* presContext = PresContext();
  nsIPresShell* shell = presContext->PresShell();
  nsLineBox::DeleteLineList(presContext, mLines, aDestructRoot,
                            &mFrames);

  FramePropertyTable* props = presContext->PropertyTable();

  if (HasPushedFloats()) {
    SafelyDestroyFrameListProp(aDestructRoot, shell, props,
                               PushedFloatProperty());
    RemoveStateBits(NS_BLOCK_HAS_PUSHED_FLOATS);
  }

  // destroy overflow lines now
  FrameLines* overflowLines = RemoveOverflowLines();
  if (overflowLines) {
    nsLineBox::DeleteLineList(presContext, overflowLines->mLines,
                              aDestructRoot, &overflowLines->mFrames);
    delete overflowLines;
  }

  if (GetStateBits() & NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS) {
    SafelyDestroyFrameListProp(aDestructRoot, shell, props,
                               OverflowOutOfFlowsProperty());
    RemoveStateBits(NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS);
  }

  if (HasOutsideBullet()) {
    SafelyDestroyFrameListProp(aDestructRoot, shell, props,
                               OutsideBulletProperty());
    RemoveStateBits(NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET);
  }

  nsBlockFrameSuper::DestroyFrom(aDestructRoot);
}

/* virtual */ nsILineIterator*
nsBlockFrame::GetLineIterator()
{
  nsLineIterator* it = new nsLineIterator;
  if (!it)
    return nullptr;

  const nsStyleVisibility* visibility = StyleVisibility();
  nsresult rv = it->Init(mLines, visibility->mDirection == NS_STYLE_DIRECTION_RTL);
  if (NS_FAILED(rv)) {
    delete it;
    return nullptr;
  }
  return it;
}

NS_QUERYFRAME_HEAD(nsBlockFrame)
  NS_QUERYFRAME_ENTRY(nsBlockFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrameSuper)

nsSplittableType
nsBlockFrame::GetSplittableType() const
{
  return NS_FRAME_SPLITTABLE_NON_RECTANGULAR;
}

#ifdef DEBUG_FRAME_DUMP
void
nsBlockFrame::List(FILE* out, const char* aPrefix, uint32_t aFlags) const
{
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);

  fprintf_stderr(out, "%s<\n", str.get());

  nsCString pfx(aPrefix);
  pfx += "  ";

  // Output the lines
  if (!mLines.empty()) {
    const_line_iterator line = begin_lines(), line_end = end_lines();
    for ( ; line != line_end; ++line) {
      line->List(out, pfx.get(), aFlags);
    }
  }

  // Output the overflow lines.
  const FrameLines* overflowLines = GetOverflowLines();
  if (overflowLines && !overflowLines->mLines.empty()) {
    fprintf_stderr(out, "%sOverflow-lines %p/%p <\n", pfx.get(), overflowLines, &overflowLines->mFrames);
    nsCString nestedPfx(pfx);
    nestedPfx += "  ";
    const_line_iterator line = overflowLines->mLines.begin(),
                        line_end = overflowLines->mLines.end();
    for ( ; line != line_end; ++line) {
      line->List(out, nestedPfx.get(), aFlags);
    }
    fprintf_stderr(out, "%s>\n", pfx.get());
  }

  // skip the principal list - we printed the lines above
  // skip the overflow list - we printed the overflow lines above
  ChildListIterator lists(this);
  ChildListIDs skip(kPrincipalList | kOverflowList);
  for (; !lists.IsDone(); lists.Next()) {
    if (skip.Contains(lists.CurrentID())) {
      continue;
    }
    fprintf_stderr(out, "%s%s %p <\n", pfx.get(),
      mozilla::layout::ChildListName(lists.CurrentID()),
      &GetChildList(lists.CurrentID()));
    nsCString nestedPfx(pfx);
    nestedPfx += "  ";
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* kid = childFrames.get();
      kid->List(out, nestedPfx.get(), aFlags);
    }
    fprintf_stderr(out, "%s>\n", pfx.get());
  }

  fprintf_stderr(out, "%s>\n", aPrefix);
}

nsresult
nsBlockFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Block"), aResult);
}
#endif

#ifdef DEBUG
nsFrameState
nsBlockFrame::GetDebugStateBits() const
{
  // We don't want to include our cursor flag in the bits the
  // regression tester looks at
  return nsBlockFrameSuper::GetDebugStateBits() & ~NS_BLOCK_HAS_LINE_CURSOR;
}
#endif

nsIAtom*
nsBlockFrame::GetType() const
{
  return nsGkAtoms::blockFrame;
}

void
nsBlockFrame::InvalidateFrame(uint32_t aDisplayItemKey)
{
  if (IsSVGText()) {
    NS_ASSERTION(GetParent()->GetType() == nsGkAtoms::svgTextFrame,
                 "unexpected block frame in SVG text");
    GetParent()->InvalidateFrame();
    return;
  }
  nsBlockFrameSuper::InvalidateFrame(aDisplayItemKey);
}

void
nsBlockFrame::InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey)
{
  if (IsSVGText()) {
    NS_ASSERTION(GetParent()->GetType() == nsGkAtoms::svgTextFrame,
                 "unexpected block frame in SVG text");
    GetParent()->InvalidateFrame();
    return;
  }
  nsBlockFrameSuper::InvalidateFrameWithRect(aRect, aDisplayItemKey);
}

nscoord
nsBlockFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  nscoord result;
  if (nsLayoutUtils::GetLastLineBaseline(aWritingMode, this, &result))
    return result;
  return nsFrame::GetLogicalBaseline(aWritingMode);
}

nscoord
nsBlockFrame::GetCaretBaseline() const
{
  nsRect contentRect = GetContentRect();
  nsMargin bp = GetUsedBorderAndPadding();

  if (!mLines.empty()) {
    const_line_iterator line = begin_lines();
    const nsLineBox* firstLine = line;
    if (firstLine->GetChildCount()) {
      return bp.top + firstLine->mFirstChild->GetCaretBaseline();
    }
  }
  nsRefPtr<nsFontMetrics> fm;
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm), inflation);
  nscoord lineHeight =
    nsHTMLReflowState::CalcLineHeight(GetContent(), StyleContext(),
                                      contentRect.height, inflation);
  return nsLayoutUtils::GetCenteredFontBaseline(fm, lineHeight) + bp.top;
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

const nsFrameList&
nsBlockFrame::GetChildList(ChildListID aListID) const
{
  switch (aListID) {
    case kPrincipalList:
      return mFrames;
    case kOverflowList: {
      FrameLines* overflowLines = GetOverflowLines();
      return overflowLines ? overflowLines->mFrames : nsFrameList::EmptyList();
    }
    case kFloatList:
      return mFloats;
    case kOverflowOutOfFlowList: {
      const nsFrameList* list = GetOverflowOutOfFlows();
      return list ? *list : nsFrameList::EmptyList();
    }
    case kPushedFloatsList: {
      const nsFrameList* list = GetPushedFloats();
      return list ? *list : nsFrameList::EmptyList();
    }
    case kBulletList: {
      const nsFrameList* list = GetOutsideBulletList();
      return list ? *list : nsFrameList::EmptyList();
    }
    default:
      return nsContainerFrame::GetChildList(aListID);
  }
}

void
nsBlockFrame::GetChildLists(nsTArray<ChildList>* aLists) const
{
  nsContainerFrame::GetChildLists(aLists);
  FrameLines* overflowLines = GetOverflowLines();
  if (overflowLines) {
    overflowLines->mFrames.AppendIfNonempty(aLists, kOverflowList);
  }
  const nsFrameList* list = GetOverflowOutOfFlows();
  if (list) {
    list->AppendIfNonempty(aLists, kOverflowOutOfFlowList);
  }
  mFloats.AppendIfNonempty(aLists, kFloatList);
  list = GetOutsideBulletList();
  if (list) {
    list->AppendIfNonempty(aLists, kBulletList);
  }
  list = GetPushedFloats();
  if (list) {
    list->AppendIfNonempty(aLists, kPushedFloatsList);
  }
}

/* virtual */ bool
nsBlockFrame::IsFloatContainingBlock() const
{
  return true;
}

static void
ReparentFrame(nsIFrame* aFrame, nsContainerFrame* aOldParent,
              nsContainerFrame* aNewParent)
{
  NS_ASSERTION(aOldParent == aFrame->GetParent(),
               "Parent not consistent with expectations");

  aFrame->SetParent(aNewParent);

  // When pushing and pulling frames we need to check for whether any
  // views need to be reparented
  nsContainerFrame::ReparentFrameView(aFrame, aOldParent, aNewParent);
}
 
static void
ReparentFrames(nsFrameList& aFrameList, nsContainerFrame* aOldParent,
               nsContainerFrame* aNewParent)
{
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    ReparentFrame(e.get(), aOldParent, aNewParent);
  }
}
 
/**
 * Remove the first line from aFromLines and adjust the associated frame list
 * aFromFrames accordingly.  The removed line is assigned to *aOutLine and
 * a frame list with its frames is assigned to *aOutFrames, i.e. the frames
 * that were extracted from the head of aFromFrames.
 * aFromLines must contain at least one line, the line may be empty.
 * @return true if aFromLines becomes empty
 */
static bool
RemoveFirstLine(nsLineList& aFromLines, nsFrameList& aFromFrames,
                nsLineBox** aOutLine, nsFrameList* aOutFrames)
{
  nsLineList_iterator removedLine = aFromLines.begin();
  *aOutLine = removedLine;
  nsLineList_iterator next = aFromLines.erase(removedLine);
  bool isLastLine = next == aFromLines.end();
  nsIFrame* lastFrame = isLastLine ? aFromFrames.LastChild()
                                   : next->mFirstChild->GetPrevSibling();
  nsFrameList::FrameLinkEnumerator linkToBreak(aFromFrames, lastFrame);
  *aOutFrames = aFromFrames.ExtractHead(linkToBreak);
  return isLastLine;
}

//////////////////////////////////////////////////////////////////////
// Reflow methods

/* virtual */ void
nsBlockFrame::MarkIntrinsicWidthsDirty()
{
  nsBlockFrame* dirtyBlock = static_cast<nsBlockFrame*>(FirstContinuation());
  dirtyBlock->mMinWidth = NS_INTRINSIC_WIDTH_UNKNOWN;
  dirtyBlock->mPrefWidth = NS_INTRINSIC_WIDTH_UNKNOWN;
  if (!(GetStateBits() & NS_BLOCK_NEEDS_BIDI_RESOLUTION)) {
    for (nsIFrame* frame = dirtyBlock; frame; 
         frame = frame->GetNextContinuation()) {
      frame->AddStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);
    }
  }

  nsBlockFrameSuper::MarkIntrinsicWidthsDirty();
}

void
nsBlockFrame::CheckIntrinsicCacheAgainstShrinkWrapState()
{
  nsPresContext *presContext = PresContext();
  if (!nsLayoutUtils::FontSizeInflationEnabled(presContext)) {
    return;
  }
  bool inflationEnabled =
    !presContext->mInflationDisabledForShrinkWrap;
  if (inflationEnabled !=
      !!(GetStateBits() & NS_BLOCK_FRAME_INTRINSICS_INFLATED)) {
    mMinWidth = NS_INTRINSIC_WIDTH_UNKNOWN;
    mPrefWidth = NS_INTRINSIC_WIDTH_UNKNOWN;
    if (inflationEnabled) {
      AddStateBits(NS_BLOCK_FRAME_INTRINSICS_INFLATED);
    } else {
      RemoveStateBits(NS_BLOCK_FRAME_INTRINSICS_INFLATED);
    }
  }
}

/* virtual */ nscoord
nsBlockFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nsIFrame* firstInFlow = FirstContinuation();
  if (firstInFlow != this)
    return firstInFlow->GetMinWidth(aRenderingContext);

  DISPLAY_MIN_WIDTH(this, mMinWidth);

  CheckIntrinsicCacheAgainstShrinkWrapState();

  if (mMinWidth != NS_INTRINSIC_WIDTH_UNKNOWN)
    return mMinWidth;

#ifdef DEBUG
  if (gNoisyIntrinsic) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": GetMinWidth\n");
  }
  AutoNoisyIndenter indenter(gNoisyIntrinsic);
#endif

  for (nsBlockFrame* curFrame = this; curFrame;
       curFrame = static_cast<nsBlockFrame*>(curFrame->GetNextContinuation())) {
    curFrame->LazyMarkLinesDirty();
  }

  if (GetStateBits() & NS_BLOCK_NEEDS_BIDI_RESOLUTION)
    ResolveBidi();
  InlineMinWidthData data;
  for (nsBlockFrame* curFrame = this; curFrame;
       curFrame = static_cast<nsBlockFrame*>(curFrame->GetNextContinuation())) {
    for (line_iterator line = curFrame->begin_lines(), line_end = curFrame->end_lines();
      line != line_end; ++line)
    {
#ifdef DEBUG
      if (gNoisyIntrinsic) {
        IndentBy(stdout, gNoiseIndent);
        printf("line (%s%s)\n",
               line->IsBlock() ? "block" : "inline",
               line->IsEmpty() ? ", empty" : "");
      }
      AutoNoisyIndenter lineindent(gNoisyIntrinsic);
#endif
      if (line->IsBlock()) {
        data.ForceBreak(aRenderingContext);
        data.currentLine = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                        line->mFirstChild, nsLayoutUtils::MIN_WIDTH);
        data.ForceBreak(aRenderingContext);
      } else {
        if (!curFrame->GetPrevContinuation() &&
            line == curFrame->begin_lines()) {
          // Only add text-indent if it has no percentages; using a
          // percentage basis of 0 unconditionally would give strange
          // behavior for calc(10%-3px).
          const nsStyleCoord &indent = StyleText()->mTextIndent;
          if (indent.ConvertsToLength())
            data.currentLine += nsRuleNode::ComputeCoordPercentCalc(indent, 0);
        }
        // XXX Bug NNNNNN Should probably handle percentage text-indent.

        data.line = &line;
        data.lineContainer = curFrame;
        nsIFrame *kid = line->mFirstChild;
        for (int32_t i = 0, i_end = line->GetChildCount(); i != i_end;
             ++i, kid = kid->GetNextSibling()) {
          kid->AddInlineMinWidth(aRenderingContext, &data);
        }
      }
#ifdef DEBUG
      if (gNoisyIntrinsic) {
        IndentBy(stdout, gNoiseIndent);
        printf("min: [prevLines=%d currentLine=%d]\n",
               data.prevLines, data.currentLine);
      }
#endif
    }
  }
  data.ForceBreak(aRenderingContext);

  mMinWidth = data.prevLines;
  return mMinWidth;
}

/* virtual */ nscoord
nsBlockFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nsIFrame* firstInFlow = FirstContinuation();
  if (firstInFlow != this)
    return firstInFlow->GetPrefWidth(aRenderingContext);

  DISPLAY_PREF_WIDTH(this, mPrefWidth);

  CheckIntrinsicCacheAgainstShrinkWrapState();

  if (mPrefWidth != NS_INTRINSIC_WIDTH_UNKNOWN)
    return mPrefWidth;

#ifdef DEBUG
  if (gNoisyIntrinsic) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": GetPrefWidth\n");
  }
  AutoNoisyIndenter indenter(gNoisyIntrinsic);
#endif

  for (nsBlockFrame* curFrame = this; curFrame;
       curFrame = static_cast<nsBlockFrame*>(curFrame->GetNextContinuation())) {
    curFrame->LazyMarkLinesDirty();
  }

  if (GetStateBits() & NS_BLOCK_NEEDS_BIDI_RESOLUTION)
    ResolveBidi();
  InlinePrefWidthData data;
  for (nsBlockFrame* curFrame = this; curFrame;
       curFrame = static_cast<nsBlockFrame*>(curFrame->GetNextContinuation())) {
    for (line_iterator line = curFrame->begin_lines(), line_end = curFrame->end_lines();
         line != line_end; ++line)
    {
#ifdef DEBUG
      if (gNoisyIntrinsic) {
        IndentBy(stdout, gNoiseIndent);
        printf("line (%s%s)\n",
               line->IsBlock() ? "block" : "inline",
               line->IsEmpty() ? ", empty" : "");
      }
      AutoNoisyIndenter lineindent(gNoisyIntrinsic);
#endif
      if (line->IsBlock()) {
        data.ForceBreak(aRenderingContext);
        data.currentLine = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                        line->mFirstChild, nsLayoutUtils::PREF_WIDTH);
        data.ForceBreak(aRenderingContext);
      } else {
        if (!curFrame->GetPrevContinuation() &&
            line == curFrame->begin_lines()) {
          // Only add text-indent if it has no percentages; using a
          // percentage basis of 0 unconditionally would give strange
          // behavior for calc(10%-3px).
          const nsStyleCoord &indent = StyleText()->mTextIndent;
          if (indent.ConvertsToLength())
            data.currentLine += nsRuleNode::ComputeCoordPercentCalc(indent, 0);
        }
        // XXX Bug NNNNNN Should probably handle percentage text-indent.

        data.line = &line;
        data.lineContainer = curFrame;
        nsIFrame *kid = line->mFirstChild;
        for (int32_t i = 0, i_end = line->GetChildCount(); i != i_end;
             ++i, kid = kid->GetNextSibling()) {
          kid->AddInlinePrefWidth(aRenderingContext, &data);
        }
      }
#ifdef DEBUG
      if (gNoisyIntrinsic) {
        IndentBy(stdout, gNoiseIndent);
        printf("pref: [prevLines=%d currentLine=%d]\n",
               data.prevLines, data.currentLine);
      }
#endif
    }
  }
  data.ForceBreak(aRenderingContext);

  mPrefWidth = data.prevLines;
  return mPrefWidth;
}

nsRect
nsBlockFrame::ComputeTightBounds(gfxContext* aContext) const
{
  // be conservative
  if (StyleContext()->HasTextDecorationLines()) {
    return GetVisualOverflowRect();
  }
  return ComputeSimpleTightBounds(aContext);
}

/* virtual */ nsresult
nsBlockFrame::GetPrefWidthTightBounds(nsRenderingContext* aRenderingContext,
                                      nscoord* aX,
                                      nscoord* aXMost)
{
  nsIFrame* firstInFlow = FirstContinuation();
  if (firstInFlow != this) {
    return firstInFlow->GetPrefWidthTightBounds(aRenderingContext, aX, aXMost);
  }

  *aX = 0;
  *aXMost = 0;

  nsresult rv;
  InlinePrefWidthData data;
  for (nsBlockFrame* curFrame = this; curFrame;
       curFrame = static_cast<nsBlockFrame*>(curFrame->GetNextContinuation())) {
    for (line_iterator line = curFrame->begin_lines(), line_end = curFrame->end_lines();
         line != line_end; ++line)
    {
      nscoord childX, childXMost;
      if (line->IsBlock()) {
        data.ForceBreak(aRenderingContext);
        rv = line->mFirstChild->GetPrefWidthTightBounds(aRenderingContext,
                                                        &childX, &childXMost);
        NS_ENSURE_SUCCESS(rv, rv);
        *aX = std::min(*aX, childX);
        *aXMost = std::max(*aXMost, childXMost);
      } else {
        if (!curFrame->GetPrevContinuation() &&
            line == curFrame->begin_lines()) {
          // Only add text-indent if it has no percentages; using a
          // percentage basis of 0 unconditionally would give strange
          // behavior for calc(10%-3px).
          const nsStyleCoord &indent = StyleText()->mTextIndent;
          if (indent.ConvertsToLength()) {
            data.currentLine += nsRuleNode::ComputeCoordPercentCalc(indent, 0);
          }
        }
        // XXX Bug NNNNNN Should probably handle percentage text-indent.

        data.line = &line;
        data.lineContainer = curFrame;
        nsIFrame *kid = line->mFirstChild;
        for (int32_t i = 0, i_end = line->GetChildCount(); i != i_end;
             ++i, kid = kid->GetNextSibling()) {
          rv = kid->GetPrefWidthTightBounds(aRenderingContext, &childX,
                                            &childXMost);
          NS_ENSURE_SUCCESS(rv, rv);
          *aX = std::min(*aX, data.currentLine + childX);
          *aXMost = std::max(*aXMost, data.currentLine + childXMost);
          kid->AddInlinePrefWidth(aRenderingContext, &data);
        }
      }
    }
  }
  data.ForceBreak(aRenderingContext);

  return NS_OK;
}

static bool
AvailableSpaceShrunk(const nsRect& aOldAvailableSpace,
                     const nsRect& aNewAvailableSpace)
{
  if (aNewAvailableSpace.width == 0) {
    // Positions are not significant if the width is zero.
    return aOldAvailableSpace.width != 0;
  }
  NS_ASSERTION(aOldAvailableSpace.x <= aNewAvailableSpace.x &&
               aOldAvailableSpace.XMost() >= aNewAvailableSpace.XMost(),
               "available space should never grow");
  return aOldAvailableSpace.width != aNewAvailableSpace.width;
}

static LogicalSize
CalculateContainingBlockSizeForAbsolutes(WritingMode aWM,
                                         const nsHTMLReflowState& aReflowState,
                                         LogicalSize aFrameSize)
{
  // The issue here is that for a 'height' of 'auto' the reflow state
  // code won't know how to calculate the containing block height
  // because it's calculated bottom up. So we use our own computed
  // size as the dimensions.
  nsIFrame* frame = aReflowState.frame;

  LogicalSize cbSize(aFrameSize);
    // Containing block is relative to the padding edge
  const LogicalMargin& border =
    LogicalMargin(aWM, aReflowState.ComputedPhysicalBorderPadding() -
                       aReflowState.ComputedPhysicalPadding());
  cbSize.ISize(aWM) -= border.IStartEnd(aWM);
  cbSize.BSize(aWM) -= border.BStartEnd(aWM);

  if (frame->GetParent()->GetContent() == frame->GetContent() &&
      frame->GetParent()->GetType() != nsGkAtoms::canvasFrame) {
    // We are a wrapped frame for the content (and the wrapper is not the
    // canvas frame, whose size is not meaningful here).
    // Use the container's dimensions, if they have been precomputed.
    // XXX This is a hack! We really should be waiting until the outermost
    // frame is fully reflowed and using the resulting dimensions, even
    // if they're intrinsic.
    // In fact we should be attaching absolute children to the outermost
    // frame and not always sticking them in block frames.

    // First, find the reflow state for the outermost frame for this
    // content, except for fieldsets where the inner anonymous frame has
    // the correct padding area with the legend taken into account.
    const nsHTMLReflowState* aLastRS = &aReflowState;
    const nsHTMLReflowState* lastButOneRS = &aReflowState;
    while (aLastRS->parentReflowState &&
           aLastRS->parentReflowState->frame->GetContent() == frame->GetContent() &&
           aLastRS->parentReflowState->frame->GetType() != nsGkAtoms::fieldSetFrame) {
      lastButOneRS = aLastRS;
      aLastRS = aLastRS->parentReflowState;
    }
    if (aLastRS != &aReflowState) {
      // Scrollbars need to be specifically excluded, if present, because they are outside the
      // padding-edge. We need better APIs for getting the various boxes from a frame.
      nsIScrollableFrame* scrollFrame = do_QueryFrame(aLastRS->frame);
      nsMargin scrollbars(0,0,0,0);
      if (scrollFrame) {
        scrollbars =
          scrollFrame->GetDesiredScrollbarSizes(aLastRS->frame->PresContext(),
                                                aLastRS->rendContext);
        if (!lastButOneRS->mFlags.mAssumingHScrollbar) {
          scrollbars.top = scrollbars.bottom = 0;
        }
        if (!lastButOneRS->mFlags.mAssumingVScrollbar) {
          scrollbars.left = scrollbars.right = 0;
        }
      }
      // We found a reflow state for the outermost wrapping frame, so use
      // its computed metrics if available, converted to our writing mode
      WritingMode lastWM = aLastRS->GetWritingMode();
      LogicalSize lastRSSize =
        LogicalSize(lastWM,
                    aLastRS->ComputedISize(),
                    aLastRS->ComputedBSize()).ConvertTo(aWM, lastWM);
      LogicalMargin lastRSPadding =
        aLastRS->ComputedLogicalPadding().ConvertTo(aWM, lastWM);
      LogicalMargin logicalScrollbars(aWM, scrollbars);
      if (lastRSSize.ISize(aWM) != NS_UNCONSTRAINEDSIZE) {
        cbSize.ISize(aWM) = std::max(0, lastRSSize.ISize(aWM) +
                                        lastRSPadding.IStartEnd(aWM) -
                                        logicalScrollbars.IStartEnd(aWM));
      }
      if (lastRSSize.BSize(aWM) != NS_UNCONSTRAINEDSIZE) {
        cbSize.BSize(aWM) = std::max(0, lastRSSize.BSize(aWM) +
                                        lastRSPadding.BStartEnd(aWM) -
                                        logicalScrollbars.BStartEnd(aWM));
      }
    }
  }

  return cbSize;
}

void
nsBlockFrame::Reflow(nsPresContext*           aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsBlockFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
#ifdef DEBUG
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": begin reflow availSize=%d,%d computedSize=%d,%d\n",
           aReflowState.AvailableISize(), aReflowState.AvailableBSize(),
           aReflowState.ComputedISize(), aReflowState.ComputedBSize());
  }
  AutoNoisyIndenter indent(gNoisy);
  PRTime start = 0; // Initialize these variablies to silence the compiler.
  int32_t ctc = 0;        // We only use these if they are set (gLameReflowMetrics).
  if (gLameReflowMetrics) {
    start = PR_Now();
    ctc = nsLineBox::GetCtorCount();
  }
#endif

  const nsHTMLReflowState *reflowState = &aReflowState;
  WritingMode wm = aReflowState.GetWritingMode();
  nscoord consumedBSize = GetConsumedBSize();
  nscoord effectiveComputedBSize = GetEffectiveComputedBSize(aReflowState,
                                                             consumedBSize);
  Maybe<nsHTMLReflowState> mutableReflowState;
  // If we have non-auto block size, we're clipping our kids and we fit,
  // make sure our kids fit too.
  if (aReflowState.AvailableBSize() != NS_UNCONSTRAINEDSIZE &&
      aReflowState.ComputedBSize() != NS_AUTOHEIGHT &&
      ShouldApplyOverflowClipping(this, aReflowState.mStyleDisplay)) {
    LogicalMargin blockDirExtras = aReflowState.ComputedLogicalBorderPadding();
    if (GetLogicalSkipSides() & (LOGICAL_SIDE_B_START)) {
      blockDirExtras.BStart(wm) = 0;
    } else {
      // Bottom margin never causes us to create continuations, so we
      // don't need to worry about whether it fits in its entirety.
      blockDirExtras.BStart(wm) +=
        aReflowState.ComputedLogicalMargin().BStart(wm);
    }

    if (effectiveComputedBSize + blockDirExtras.BStartEnd(wm) <=
        aReflowState.AvailableBSize()) {
      mutableReflowState.construct(aReflowState);
      mutableReflowState.ref().AvailableBSize() = NS_UNCONSTRAINEDSIZE;
      reflowState = mutableReflowState.addr();
    }
  }

  // See comment below about oldSize. Use *only* for the
  // abs-pos-containing-block-size-change optimization!
  nsSize oldSize = GetSize();

  // Should we create a float manager?
  nsAutoFloatManager autoFloatManager(const_cast<nsHTMLReflowState&>(*reflowState));

  // XXXldb If we start storing the float manager in the frame rather
  // than keeping it around only during reflow then we should create it
  // only when there are actually floats to manage.  Otherwise things
  // like tables will gain significant bloat.
  bool needFloatManager = nsBlockFrame::BlockNeedsFloatManager(this);
  if (needFloatManager)
    autoFloatManager.CreateFloatManager(aPresContext);

  // OK, some lines may be reflowed. Blow away any saved line cursor
  // because we may invalidate the nondecreasing
  // overflowArea.VisualOverflow().y/yMost invariant, and we may even
  // delete the line with the line cursor.
  ClearLineCursor();

  if (IsFrameTreeTooDeep(*reflowState, aMetrics, aStatus)) {
    return;
  }

  bool blockStartMarginRoot, blockEndMarginRoot;
  IsMarginRoot(&blockStartMarginRoot, &blockEndMarginRoot);

  // Cache the consumed height in the block reflow state so that we don't have
  // to continually recompute it.
  nsBlockReflowState state(*reflowState, aPresContext, this,
                           blockStartMarginRoot, blockEndMarginRoot,
                           needFloatManager, consumedBSize);

  if (GetStateBits() & NS_BLOCK_NEEDS_BIDI_RESOLUTION)
    static_cast<nsBlockFrame*>(FirstContinuation())->ResolveBidi();

  if (RenumberLists(aPresContext)) {
    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
  }

  // ALWAYS drain overflow. We never want to leave the previnflow's
  // overflow lines hanging around; block reflow depends on the
  // overflow line lists being cleared out between reflow passes.
  DrainOverflowLines();

  // Handle paginated overflow (see nsContainerFrame.h)
  nsOverflowAreas ocBounds;
  nsReflowStatus ocStatus = NS_FRAME_COMPLETE;
  if (GetPrevInFlow()) {
    ReflowOverflowContainerChildren(aPresContext, *reflowState, ocBounds, 0,
                                    ocStatus);
  }

  // Now that we're done cleaning up our overflow container lists, we can
  // give |state| its nsOverflowContinuationTracker.
  nsOverflowContinuationTracker tracker(this, false);
  state.mOverflowTracker = &tracker;

  // Drain & handle pushed floats
  DrainPushedFloats(state);
  nsOverflowAreas fcBounds;
  nsReflowStatus fcStatus = NS_FRAME_COMPLETE;
  ReflowPushedFloats(state, fcBounds, fcStatus);

  // If we're not dirty (which means we'll mark everything dirty later)
  // and our width has changed, mark the lines dirty that we need to
  // mark dirty for a resize reflow.
  if (!(GetStateBits() & NS_FRAME_IS_DIRTY) && reflowState->mFlags.mHResize) {
    PrepareResizeReflow(state);
  }

  LazyMarkLinesDirty();

  mState &= ~NS_FRAME_FIRST_REFLOW;

  // Now reflow...
  ReflowDirtyLines(state);

  // If we have a next-in-flow, and that next-in-flow has pushed floats from
  // this frame from a previous iteration of reflow, then we should not return
  // a status of NS_FRAME_COMPLETE, since we actually have overflow, it's just
  // already been handled.

  // NOTE: This really shouldn't happen, since we _should_ pull back our floats
  // and reflow them, but just in case it does, this is a safety precaution so
  // we don't end up with a placeholder pointing to frames that have already
  // been deleted as part of removing our next-in-flow.
  if (NS_FRAME_IS_COMPLETE(state.mReflowStatus)) {
    nsBlockFrame* nif = static_cast<nsBlockFrame*>(GetNextInFlow());
    while (nif) {
      if (nif->HasPushedFloatsFromPrevContinuation()) {
        NS_MergeReflowStatusInto(&state.mReflowStatus, NS_FRAME_NOT_COMPLETE);
      }

      nif = static_cast<nsBlockFrame*>(nif->GetNextInFlow());
    }
  }

  NS_MergeReflowStatusInto(&state.mReflowStatus, ocStatus);
  NS_MergeReflowStatusInto(&state.mReflowStatus, fcStatus);

  // If we end in a BR with clear and affected floats continue,
  // we need to continue, too.
  if (NS_UNCONSTRAINEDSIZE != reflowState->AvailableBSize() &&
      NS_FRAME_IS_COMPLETE(state.mReflowStatus) &&
      state.mFloatManager->ClearContinues(FindTrailingClear())) {
    NS_FRAME_SET_INCOMPLETE(state.mReflowStatus);
  }

  if (!NS_FRAME_IS_FULLY_COMPLETE(state.mReflowStatus)) {
    if (HasOverflowLines() || HasPushedFloats()) {
      state.mReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
    }

#ifdef DEBUG_kipp
    ListTag(stdout); printf(": block is not fully complete\n");
#endif
  }

  // Place the "marker" (bullet) frame if it is placed next to a block
  // child.
  //
  // According to the CSS2 spec, section 12.6.1, the "marker" box
  // participates in the height calculation of the list-item box's
  // first line box.
  //
  // There are exactly two places a bullet can be placed: near the
  // first or second line. It's only placed on the second line in a
  // rare case: an empty first line followed by a second line that
  // contains a block (example: <LI>\n<P>... ). This is where
  // the second case can happen.
  if (HasOutsideBullet() && !mLines.empty() &&
      (mLines.front()->IsBlock() ||
       (0 == mLines.front()->BSize() &&
        mLines.front() != mLines.back() &&
        mLines.begin().next()->IsBlock()))) {
    // Reflow the bullet
    nsHTMLReflowMetrics metrics(aReflowState);
    // XXX Use the entire line when we fix bug 25888.
    nsLayoutUtils::LinePosition position;
    WritingMode wm = aReflowState.GetWritingMode();
    bool havePosition = nsLayoutUtils::GetFirstLinePosition(wm, this,
                                                            &position);
    nscoord lineTop = havePosition ?
      position.mBStart :
      reflowState->ComputedLogicalBorderPadding().BStart(wm);
    nsIFrame* bullet = GetOutsideBullet();
    ReflowBullet(bullet, state, metrics, lineTop);
    NS_ASSERTION(!BulletIsEmpty() || metrics.BSize(wm) == 0,
                 "empty bullet took up space");

    if (havePosition && !BulletIsEmpty()) {
      // We have some lines to align the bullet with.  

      // Doing the alignment using the baseline will also cater for
      // bullets that are placed next to a child block (bug 92896)
    
      // Tall bullets won't look particularly nice here...
      LogicalRect bbox = bullet->GetLogicalRect(wm, metrics.Width());
      bbox.BStart(wm) = position.mBaseline - metrics.BlockStartAscent();
      bullet->SetRect(wm, bbox, metrics.Width());
    }
    // Otherwise just leave the bullet where it is, up against our top padding.
  }

  CheckFloats(state);

  // Compute our final size
  nscoord blockEndEdgeOfChildren;
  ComputeFinalSize(*reflowState, state, aMetrics, &blockEndEdgeOfChildren);
  nsRect areaBounds = nsRect(0, 0, aMetrics.Width(), aMetrics.Height());
  ComputeOverflowAreas(areaBounds, reflowState->mStyleDisplay,
                       blockEndEdgeOfChildren, aMetrics.mOverflowAreas);
  // Factor overflow container child bounds into the overflow area
  aMetrics.mOverflowAreas.UnionWith(ocBounds);
  // Factor pushed float child bounds into the overflow area
  aMetrics.mOverflowAreas.UnionWith(fcBounds);

  // Let the absolutely positioned container reflow any absolutely positioned
  // child frames that need to be reflowed, e.g., elements with a percentage
  // based width/height
  // We want to do this under either of two conditions:
  //  1. If we didn't do the incremental reflow above.
  //  2. If our size changed.
  // Even though it's the padding edge that's the containing block, we
  // can use our rect (the border edge) since if the border style
  // changed, the reflow would have been targeted at us so we'd satisfy
  // condition 1.
  // XXX checking oldSize is bogus, there are various reasons we might have
  // reflowed but our size might not have been changed to what we
  // asked for (e.g., we ended up being pushed to a new page)
  // When WillReflowAgainForClearance is true, we will reflow again without
  // resetting the size. Because of this, we must not reflow our abs-pos children
  // in that situation --- what we think is our "new size"
  // will not be our real new size. This also happens to be more efficient.
  if (HasAbsolutelyPositionedChildren()) {
    nsAbsoluteContainingBlock* absoluteContainer = GetAbsoluteContainingBlock();
    bool haveInterrupt = aPresContext->HasPendingInterrupt();
    if (reflowState->WillReflowAgainForClearance() ||
        haveInterrupt) {
      // Make sure that when we reflow again we'll actually reflow all the abs
      // pos frames that might conceivably depend on our size (or all of them,
      // if we're dirty right now and interrupted; in that case we also need
      // to mark them all with NS_FRAME_IS_DIRTY).  Sadly, we can't do much
      // better than that, because we don't really know what our size will be,
      // and it might in fact not change on the followup reflow!
      if (haveInterrupt && (GetStateBits() & NS_FRAME_IS_DIRTY)) {
        absoluteContainer->MarkAllFramesDirty();
      } else {
        absoluteContainer->MarkSizeDependentFramesDirty();
      }
    } else {
      WritingMode parentWM = aMetrics.GetWritingMode();
      LogicalSize containingBlockSize =
        CalculateContainingBlockSizeForAbsolutes(parentWM, *reflowState,
                                                 aMetrics.Size(parentWM));

      // Mark frames that depend on changes we just made to this frame as dirty:
      // Now we can assume that the padding edge hasn't moved.
      // We need to reflow the absolutes if one of them depends on
      // its placeholder position, or the containing block size in a
      // direction in which the containing block size might have
      // changed.

      // XXX "width" and "height" in this block will become ISize and BSize
      // when nsAbsoluteContainingBlock is logicalized
      bool cbWidthChanged = aMetrics.Width() != oldSize.width;
      bool isRoot = !GetContent()->GetParent();
      // If isRoot and we have auto height, then we are the initial
      // containing block and the containing block height is the
      // viewport height, which can't change during incremental
      // reflow.
      bool cbHeightChanged =
        !(isRoot && NS_UNCONSTRAINEDSIZE == reflowState->ComputedHeight()) &&
        aMetrics.Height() != oldSize.height;

      nsRect containingBlock(nsPoint(0, 0),
                             containingBlockSize.GetPhysicalSize(parentWM));
      absoluteContainer->Reflow(this, aPresContext, *reflowState,
                                state.mReflowStatus,
                                containingBlock, true,
                                cbWidthChanged, cbHeightChanged,
                                &aMetrics.mOverflowAreas);
    }
  }

  FinishAndStoreOverflow(&aMetrics);

  // Clear the float manager pointer in the block reflow state so we
  // don't waste time translating the coordinate system back on a dead
  // float manager.
  if (needFloatManager)
    state.mFloatManager = nullptr;

  aStatus = state.mReflowStatus;

#ifdef DEBUG
  // Between when we drain pushed floats and when we complete reflow,
  // we're allowed to have multiple continuations of the same float on
  // our floats list, since a first-in-flow might get pushed to a later
  // continuation of its containing block.  But it's not permitted
  // outside that time.
  nsLayoutUtils::AssertNoDuplicateContinuations(this, mFloats);

  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": status=%x (%scomplete) metrics=%d,%d carriedMargin=%d",
           aStatus, NS_FRAME_IS_COMPLETE(aStatus) ? "" : "not ",
           aMetrics.Width(), aMetrics.Height(),
           aMetrics.mCarriedOutBottomMargin.get());
    if (HasOverflowAreas()) {
      printf(" overflow-vis={%d,%d,%d,%d}",
             aMetrics.VisualOverflow().x,
             aMetrics.VisualOverflow().y,
             aMetrics.VisualOverflow().width,
             aMetrics.VisualOverflow().height);
      printf(" overflow-scr={%d,%d,%d,%d}",
             aMetrics.ScrollableOverflow().x,
             aMetrics.ScrollableOverflow().y,
             aMetrics.ScrollableOverflow().width,
             aMetrics.ScrollableOverflow().height);
    }
    printf("\n");
  }

  if (gLameReflowMetrics) {
    PRTime end = PR_Now();

    int32_t ectc = nsLineBox::GetCtorCount();
    int32_t numLines = mLines.size();
    if (!numLines) numLines = 1;
    PRTime delta, perLineDelta, lines;
    lines = int64_t(numLines);
    delta = end - start;
    perLineDelta = delta / lines;

    ListTag(stdout);
    char buf[400];
    PR_snprintf(buf, sizeof(buf),
                ": %lld elapsed (%lld per line) (%d lines; %d new lines)",
                delta, perLineDelta, numLines, ectc - ctc);
    printf("%s\n", buf);
  }
#endif

  NS_FRAME_SET_TRUNCATION(aStatus, (*reflowState), aMetrics);
}

bool
nsBlockFrame::CheckForCollapsedBEndMarginFromClearanceLine()
{
  line_iterator begin = begin_lines();
  line_iterator line = end_lines();

  while (true) {
    if (begin == line) {
      return false;
    }
    --line;
    if (line->BSize() != 0 || !line->CachedIsEmpty()) {
      return false;
    }
    if (line->HasClearance()) {
      return true;
    }
  }
  // not reached
}

void
nsBlockFrame::ComputeFinalSize(const nsHTMLReflowState& aReflowState,
                               nsBlockReflowState&      aState,
                               nsHTMLReflowMetrics&     aMetrics,
                               nscoord*                 aBEndEdgeOfChildren)
{
  WritingMode wm = aState.mReflowState.GetWritingMode();
  const LogicalMargin& borderPadding = aState.BorderPadding();
#ifdef NOISY_FINAL_SIZE
  ListTag(stdout);
  printf(": mBCoord=%d mIsBEndMarginRoot=%s mPrevBEndMargin=%d bp=%d,%d\n",
         aState.mBCoord, aState.GetFlag(BRS_ISBENDMARGINROOT) ? "yes" : "no",
         aState.mPrevBEndMargin,
         borderPadding.BStart(wm), borderPadding.BEnd(wm));
#endif

  // Compute final inline size
  LogicalSize finalSize(wm);
  finalSize.ISize(wm) =
    NSCoordSaturatingAdd(NSCoordSaturatingAdd(borderPadding.IStart(wm),
                                              aReflowState.ComputedISize()),
                         borderPadding.IEnd(wm));

  // Return bottom margin information
  // rbs says he hit this assertion occasionally (see bug 86947), so
  // just set the margin to zero and we'll figure out why later
  //NS_ASSERTION(aMetrics.mCarriedOutBottomMargin.IsZero(),
  //             "someone else set the margin");
  nscoord nonCarriedOutBDirMargin = 0;
  if (!aState.GetFlag(BRS_ISBENDMARGINROOT)) {
    // Apply rule from CSS 2.1 section 8.3.1. If we have some empty
    // line with clearance and a non-zero top margin and all
    // subsequent lines are empty, then we do not allow our children's
    // carried out bottom margin to be carried out of us and collapse
    // with our own bottom margin.
    if (CheckForCollapsedBEndMarginFromClearanceLine()) {
      // Convert the children's carried out margin to something that
      // we will include in our height
      nonCarriedOutBDirMargin = aState.mPrevBEndMargin.get();
      aState.mPrevBEndMargin.Zero();
    }
    aMetrics.mCarriedOutBottomMargin = aState.mPrevBEndMargin;
  } else {
    aMetrics.mCarriedOutBottomMargin.Zero();
  }

  nscoord blockEndEdgeOfChildren = aState.mBCoord + nonCarriedOutBDirMargin;
  // Shrink wrap our height around our contents.
  if (aState.GetFlag(BRS_ISBENDMARGINROOT) ||
      NS_UNCONSTRAINEDSIZE != aReflowState.ComputedBSize()) {
    // When we are a bottom-margin root make sure that our last
    // childs bottom margin is fully applied. We also do this when
    // we have a computed height, since in that case the carried out
    // margin is not going to be applied anywhere, so we should note it
    // here to be included in the overflow area.
    // Apply the margin only if there's space for it.
    if (blockEndEdgeOfChildren < aState.mReflowState.AvailableBSize())
    {
      // Truncate bottom margin if it doesn't fit to our available height.
      blockEndEdgeOfChildren =
        std::min(blockEndEdgeOfChildren + aState.mPrevBEndMargin.get(),
               aState.mReflowState.AvailableBSize());
    }
  }
  if (aState.GetFlag(BRS_FLOAT_MGR)) {
    // Include the float manager's state to properly account for the
    // bottom margin of any floated elements; e.g., inside a table cell.
    nscoord floatHeight =
      aState.ClearFloats(blockEndEdgeOfChildren, NS_STYLE_CLEAR_BOTH,
                         nullptr, nsFloatManager::DONT_CLEAR_PUSHED_FLOATS);
    blockEndEdgeOfChildren = std::max(blockEndEdgeOfChildren, floatHeight);
  }

  if (NS_UNCONSTRAINEDSIZE != aReflowState.ComputedBSize()
      && (GetParent()->GetType() != nsGkAtoms::columnSetFrame ||
          aReflowState.parentReflowState->AvailableBSize() == NS_UNCONSTRAINEDSIZE)) {
    ComputeFinalBSize(aReflowState, &aState.mReflowStatus,
                      aState.mBCoord + nonCarriedOutBDirMargin,
                      borderPadding, finalSize, aState.mConsumedBSize);
    if (!NS_FRAME_IS_COMPLETE(aState.mReflowStatus)) {
      // Use the current height; continuations will take up the rest.
      // Do extend the height to at least consume the available
      // height, otherwise our left/right borders (for example) won't
      // extend all the way to the break.
      finalSize.BSize(wm) = std::max(aReflowState.AvailableBSize(),
                                     aState.mBCoord + nonCarriedOutBDirMargin);
      // ... but don't take up more block size than is available
      nscoord effectiveComputedBSize =
        GetEffectiveComputedBSize(aReflowState, aState.GetConsumedBSize());
      finalSize.BSize(wm) =
        std::min(finalSize.BSize(wm),
                 borderPadding.BStart(wm) + effectiveComputedBSize);
      // XXX It's pretty wrong that our bottom border still gets drawn on
      // on its own on the last-in-flow, even if we ran out of height
      // here. We need GetSkipSides to check whether we ran out of content
      // height in the current frame, not whether it's last-in-flow.
    }

    // Don't carry out a bottom margin when our height is fixed.
    aMetrics.mCarriedOutBottomMargin.Zero();
  }
  else if (NS_FRAME_IS_COMPLETE(aState.mReflowStatus)) {
    nscoord contentBSize = blockEndEdgeOfChildren - borderPadding.BStart(wm);
    nscoord autoBSize = aReflowState.ApplyMinMaxHeight(contentBSize);
    if (autoBSize != contentBSize) {
      // Our min-height or max-height made our height change.  Don't carry out
      // our kids' bottom margins.
      aMetrics.mCarriedOutBottomMargin.Zero();
    }
    autoBSize += borderPadding.BStart(wm) + borderPadding.BEnd(wm);
    finalSize.BSize(wm) = autoBSize;
  }
  else {
    NS_ASSERTION(aReflowState.AvailableBSize() != NS_UNCONSTRAINEDSIZE,
      "Shouldn't be incomplete if availableBSize is UNCONSTRAINED.");
    finalSize.BSize(wm) = std::max(aState.mBCoord,
                                   aReflowState.AvailableBSize());
    if (aReflowState.AvailableBSize() == NS_UNCONSTRAINEDSIZE)
      // This should never happen, but it does. See bug 414255
      finalSize.BSize(wm) = aState.mBCoord;
  }

  if (IS_TRUE_OVERFLOW_CONTAINER(this) &&
      NS_FRAME_IS_NOT_COMPLETE(aState.mReflowStatus)) {
    // Overflow containers can only be overflow complete.
    // Note that auto height overflow containers have no normal children
    NS_ASSERTION(finalSize.BSize(wm) == 0,
                 "overflow containers must be zero-block-size");
    NS_FRAME_SET_OVERFLOW_INCOMPLETE(aState.mReflowStatus);
  }

  // Screen out negative block sizes --- can happen due to integer overflows :-(
  finalSize.BSize(wm) = std::max(0, finalSize.BSize(wm));
  *aBEndEdgeOfChildren = blockEndEdgeOfChildren;

  FrameProperties properties = Properties();
  if (blockEndEdgeOfChildren != finalSize.BSize(wm) - borderPadding.BEnd(wm)) {
    properties.Set(BottomEdgeOfChildrenProperty(),
                   NS_INT32_TO_PTR(blockEndEdgeOfChildren));
  } else {
    properties.Delete(BottomEdgeOfChildrenProperty());
  }

  aMetrics.SetSize(wm, finalSize);

#ifdef DEBUG_blocks
  if (CRAZY_SIZE(aMetrics.Width()) || CRAZY_SIZE(aMetrics.Height())) {
    ListTag(stdout);
    printf(": WARNING: desired:%d,%d\n", aMetrics.Width(), aMetrics.Height());
  }
#endif
}

static void
ConsiderBottomEdgeOfChildren(nscoord aBottomEdgeOfChildren,
                             nsOverflowAreas& aOverflowAreas)
{
  // Factor in the bottom edge of the children.  Child frames will be added
  // to the overflow area as we iterate through the lines, but their margins
  // won't, so we need to account for bottom margins here.
  // REVIEW: For now, we do this for both visual and scrollable area,
  // although when we make scrollable overflow area not be a subset of
  // visual, we can change this.
  NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
    nsRect& o = aOverflowAreas.Overflow(otype);
    o.height = std::max(o.YMost(), aBottomEdgeOfChildren) - o.y;
  }
}

void
nsBlockFrame::ComputeOverflowAreas(const nsRect&         aBounds,
                                   const nsStyleDisplay* aDisplay,
                                   nscoord               aBottomEdgeOfChildren,
                                   nsOverflowAreas&      aOverflowAreas)
{
  // Compute the overflow areas of our children
  // XXX_perf: This can be done incrementally.  It is currently one of
  // the things that makes incremental reflow O(N^2).
  nsOverflowAreas areas(aBounds, aBounds);
  if (!ShouldApplyOverflowClipping(this, aDisplay)) {
    for (line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line) {
      areas.UnionWith(line->GetOverflowAreas());
    }

    // Factor an outside bullet in; normally the bullet will be factored into
    // the line-box's overflow areas. However, if the line is a block
    // line then it won't; if there are no lines, it won't. So just
    // factor it in anyway (it can't hurt if it was already done).
    // XXXldb Can we just fix GetOverflowArea instead?
    nsIFrame* outsideBullet = GetOutsideBullet();
    if (outsideBullet) {
      areas.UnionAllWith(outsideBullet->GetRect());
    }

    ConsiderBottomEdgeOfChildren(aBottomEdgeOfChildren, areas);
  }

#ifdef NOISY_COMBINED_AREA
  ListTag(stdout);
  printf(": ca=%d,%d,%d,%d\n", area.x, area.y, area.width, area.height);
#endif

  aOverflowAreas = areas;
}

bool
nsBlockFrame::UpdateOverflow()
{
  nsRect rect(nsPoint(0, 0), GetSize());
  nsOverflowAreas overflowAreas(rect, rect);

  // We need to update the overflow areas of lines manually, as they
  // get cached and re-used otherwise. Lines aren't exposed as normal
  // frame children, so calling UnionChildOverflow alone will end up
  // using the old cached values.
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line) {
    nsRect bounds = line->GetPhysicalBounds();
    nsOverflowAreas lineAreas(bounds, bounds);

    int32_t n = line->GetChildCount();
    for (nsIFrame* lineFrame = line->mFirstChild;
         n > 0; lineFrame = lineFrame->GetNextSibling(), --n) {
      ConsiderChildOverflow(lineAreas, lineFrame);
    }

    // Consider the overflow areas of the floats attached to the line as well
    if (line->HasFloats()) {
      for (nsFloatCache* fc = line->GetFirstFloat(); fc; fc = fc->Next()) {
        ConsiderChildOverflow(lineAreas, fc->mFloat);
      }
    }

    line->SetOverflowAreas(lineAreas);
    overflowAreas.UnionWith(lineAreas);
  }

  // Line cursor invariants depend on the overflow areas of the lines, so
  // we must clear the line cursor since those areas may have changed.
  ClearLineCursor();

  // Union with child frames, skipping the principal and float lists
  // since we already handled those using the line boxes.
  nsLayoutUtils::UnionChildOverflow(this, overflowAreas,
                                    kPrincipalList | kFloatList);

  bool found;
  nscoord bottomEdgeOfChildren = NS_PTR_TO_INT32(
    Properties().Get(BottomEdgeOfChildrenProperty(), &found));
  if (found) {
    ConsiderBottomEdgeOfChildren(bottomEdgeOfChildren, overflowAreas);
  }

  return FinishAndStoreOverflow(overflowAreas, GetSize());
}

void
nsBlockFrame::LazyMarkLinesDirty()
{
  if (GetStateBits() & NS_BLOCK_LOOK_FOR_DIRTY_FRAMES) {
    for (line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end; ++line) {
      int32_t n = line->GetChildCount();
      for (nsIFrame* lineFrame = line->mFirstChild;
           n > 0; lineFrame = lineFrame->GetNextSibling(), --n) {
        if (NS_SUBTREE_DIRTY(lineFrame)) {
          // NOTE:  MarkLineDirty does more than just marking the line dirty.
          MarkLineDirty(line, &mLines);
          break;
        }
      }
    }
    RemoveStateBits(NS_BLOCK_LOOK_FOR_DIRTY_FRAMES);
  }
}

void
nsBlockFrame::MarkLineDirty(line_iterator aLine, const nsLineList* aLineList)
{
  // Mark aLine dirty
  aLine->MarkDirty();
  aLine->SetInvalidateTextRuns(true);
#ifdef DEBUG
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": mark line %p dirty\n", static_cast<void*>(aLine.get()));
  }
#endif

  // Mark previous line dirty if it's an inline line so that it can
  // maybe pullup something from the line just affected.
  // XXX We don't need to do this if aPrevLine ends in a break-after...
  if (aLine != aLineList->front() && aLine->IsInline() &&
      aLine.prev()->IsInline()) {
    aLine.prev()->MarkDirty();
    aLine.prev()->SetInvalidateTextRuns(true);
#ifdef DEBUG
    if (gNoisyReflow) {
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": mark prev-line %p dirty\n",
             static_cast<void*>(aLine.prev().get()));
    }
#endif
  }
}

/**
 * Test whether lines are certain to be aligned left so that we can make
 * resizing optimizations
 */
static inline bool
IsAlignedLeft(uint8_t aAlignment,
              uint8_t aDirection,
              uint8_t aUnicodeBidi,
              nsIFrame* aFrame)
{
  return aFrame->IsSVGText() ||
         NS_STYLE_TEXT_ALIGN_LEFT == aAlignment ||
         (((NS_STYLE_TEXT_ALIGN_DEFAULT == aAlignment &&
           NS_STYLE_DIRECTION_LTR == aDirection) ||
          (NS_STYLE_TEXT_ALIGN_END == aAlignment &&
           NS_STYLE_DIRECTION_RTL == aDirection)) &&
         !(NS_STYLE_UNICODE_BIDI_PLAINTEXT & aUnicodeBidi));
}

void
nsBlockFrame::PrepareResizeReflow(nsBlockReflowState& aState)
{
  // See if we can try and avoid marking all the lines as dirty
  bool tryAndSkipLines =
    // The left content-edge must be a constant distance from the left
    // border-edge.
    !StylePadding()->mPadding.GetLeft().HasPercent();

#ifdef DEBUG
  if (gDisableResizeOpt) {
    tryAndSkipLines = false;
  }
  if (gNoisyReflow) {
    if (!tryAndSkipLines) {
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": marking all lines dirty: availISize=%d\n",
             aState.mReflowState.AvailableISize());
    }
  }
#endif

  if (tryAndSkipLines) {
    WritingMode wm = aState.mReflowState.GetWritingMode();
    nscoord newAvailISize =
      aState.mReflowState.ComputedLogicalBorderPadding().IStart(wm) +
      aState.mReflowState.ComputedISize();
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aState.mReflowState.ComputedLogicalBorderPadding().IStart(wm) &&
                 NS_UNCONSTRAINEDSIZE != aState.mReflowState.ComputedISize(),
                 "math on NS_UNCONSTRAINEDSIZE");

#ifdef DEBUG
    if (gNoisyReflow) {
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": trying to avoid marking all lines dirty\n");
    }
#endif

    for (line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line)
    {
      // We let child blocks make their own decisions the same
      // way we are here.
      bool isLastLine = line == mLines.back() && !GetNextInFlow();
      if (line->IsBlock() ||
          line->HasFloats() ||
          (!isLastLine && !line->HasBreakAfter()) ||
          ((isLastLine || !line->IsLineWrapped())) ||
          line->ResizeReflowOptimizationDisabled() ||
          line->IsImpactedByFloat() ||
          (line->IEnd() > newAvailISize)) {
        line->MarkDirty();
      }

#ifdef REALLY_NOISY_REFLOW
      if (!line->IsBlock()) {
        printf("PrepareResizeReflow thinks line %p is %simpacted by floats\n", 
               line.get(), line->IsImpactedByFloat() ? "" : "not ");
      }
#endif
#ifdef DEBUG
      if (gNoisyReflow && !line->IsDirty()) {
        IndentBy(stdout, gNoiseIndent + 1);
        printf("skipped: line=%p next=%p %s %s%s%s breakTypeBefore/After=%d/%d xmost=%d\n",
           static_cast<void*>(line.get()),
           static_cast<void*>((line.next() != end_lines() ? line.next().get() : nullptr)),
           line->IsBlock() ? "block" : "inline",
           line->HasBreakAfter() ? "has-break-after " : "",
           line->HasFloats() ? "has-floats " : "",
           line->IsImpactedByFloat() ? "impacted " : "",
           line->GetBreakTypeBefore(), line->GetBreakTypeAfter(),
           line->IEnd());
      }
#endif
    }
  }
  else {
    // Mark everything dirty
    for (line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line)
    {
      line->MarkDirty();
    }
  }
}

//----------------------------------------

/**
 * Propagate reflow "damage" from from earlier lines to the current
 * line.  The reflow damage comes from the following sources:
 *  1. The regions of float damage remembered during reflow.
 *  2. The combination of nonzero |aDeltaY| and any impact by a float,
 *     either the previous reflow or now.
 *
 * When entering this function, |aLine| is still at its old position and
 * |aDeltaY| indicates how much it will later be slid (assuming it
 * doesn't get marked dirty and reflowed entirely).
 */
void
nsBlockFrame::PropagateFloatDamage(nsBlockReflowState& aState,
                                   nsLineBox* aLine,
                                   nscoord aDeltaY)
{
  nsFloatManager *floatManager = aState.mReflowState.mFloatManager;
  NS_ASSERTION((aState.mReflowState.parentReflowState &&
                aState.mReflowState.parentReflowState->mFloatManager == floatManager) ||
                aState.mReflowState.mBlockDelta == 0, "Bad block delta passed in");

  // Check to see if there are any floats; if there aren't, there can't
  // be any float damage
  if (!floatManager->HasAnyFloats())
    return;

  // Check the damage region recorded in the float damage.
  if (floatManager->HasFloatDamage()) {
    // Need to check mBounds *and* mCombinedArea to find intersections 
    // with aLine's floats
    nscoord lineYA = aLine->BStart() + aDeltaY;
    nscoord lineYB = lineYA + aLine->BSize();
    // Scrollable overflow should be sufficient for things that affect
    // layout.
    nsRect overflow = aLine->GetOverflowArea(eScrollableOverflow);
    nscoord lineYCombinedA = overflow.y + aDeltaY;
    nscoord lineYCombinedB = lineYCombinedA + overflow.height;
    if (floatManager->IntersectsDamage(lineYA, lineYB) ||
        floatManager->IntersectsDamage(lineYCombinedA, lineYCombinedB)) {
      aLine->MarkDirty();
      return;
    }
  }

  // Check if the line is moving relative to the float manager
  if (aDeltaY + aState.mReflowState.mBlockDelta != 0) {
    if (aLine->IsBlock()) {
      // Unconditionally reflow sliding blocks; we only really need to reflow
      // if there's a float impacting this block, but the current float manager
      // makes it difficult to check that.  Therefore, we let the child block
      // decide what it needs to reflow.
      aLine->MarkDirty();
    } else {
      bool wasImpactedByFloat = aLine->IsImpactedByFloat();
      nsFlowAreaRect floatAvailableSpace =
        aState.GetFloatAvailableSpaceForBSize(aLine->BStart() + aDeltaY,
                                              aLine->BSize(),
                                              nullptr);

#ifdef REALLY_NOISY_REFLOW
    printf("nsBlockFrame::PropagateFloatDamage %p was = %d, is=%d\n", 
           this, wasImpactedByFloat, floatAvailableSpace.mHasFloats);
#endif

      // Mark the line dirty if it was or is affected by a float
      // We actually only really need to reflow if the amount of impact
      // changes, but that's not straightforward to check
      if (wasImpactedByFloat || floatAvailableSpace.mHasFloats) {
        aLine->MarkDirty();
      }
    }
  }
}

static bool LineHasClear(nsLineBox* aLine) {
  return aLine->IsBlock()
    ? (aLine->GetBreakTypeBefore() ||
       (aLine->mFirstChild->GetStateBits() & NS_BLOCK_HAS_CLEAR_CHILDREN) ||
       !nsBlockFrame::BlockCanIntersectFloats(aLine->mFirstChild))
    : aLine->HasFloatBreakAfter();
}


/**
 * Reparent a whole list of floats from aOldParent to this block.  The
 * floats might be taken from aOldParent's overflow list. They will be
 * removed from the list. They end up appended to our mFloats list.
 */
void
nsBlockFrame::ReparentFloats(nsIFrame* aFirstFrame, nsBlockFrame* aOldParent,
                             bool aReparentSiblings) {
  nsFrameList list;
  aOldParent->CollectFloats(aFirstFrame, list, aReparentSiblings);
  if (list.NotEmpty()) {
    for (nsIFrame* f = list.FirstChild(); f; f = f->GetNextSibling()) {
      ReparentFrame(f, aOldParent, this);
    }
    mFloats.AppendFrames(nullptr, list);
  }
}

static void DumpLine(const nsBlockReflowState& aState, nsLineBox* aLine,
                     nscoord aDeltaY, int32_t aDeltaIndent) {
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsRect ovis(aLine->GetVisualOverflowArea());
    nsRect oscr(aLine->GetScrollableOverflowArea());
    nsBlockFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent + aDeltaIndent);
    printf("line=%p mBCoord=%d dirty=%s oldBounds={%d,%d,%d,%d} oldoverflow-vis={%d,%d,%d,%d} oldoverflow-scr={%d,%d,%d,%d} deltaBCoord=%d mPrevBEndMargin=%d childCount=%d\n",
           static_cast<void*>(aLine), aState.mBCoord,
           aLine->IsDirty() ? "yes" : "no",
           aLine->IStart(), aLine->BStart(),
           aLine->ISize(), aLine->BSize(),
           ovis.x, ovis.y, ovis.width, ovis.height,
           oscr.x, oscr.y, oscr.width, oscr.height,
           aDeltaY, aState.mPrevBEndMargin.get(), aLine->GetChildCount());
  }
#endif
}

void
nsBlockFrame::ReflowDirtyLines(nsBlockReflowState& aState)
{
  bool keepGoing = true;
  bool repositionViews = false; // should we really need this?
  bool foundAnyClears = aState.mFloatBreakType != NS_STYLE_CLEAR_NONE;
  bool willReflowAgain = false;

#ifdef DEBUG
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": reflowing dirty lines");
    printf(" computedISize=%d\n", aState.mReflowState.ComputedISize());
  }
  AutoNoisyIndenter indent(gNoisyReflow);
#endif

  bool selfDirty = (GetStateBits() & NS_FRAME_IS_DIRTY) ||
                     (aState.mReflowState.mFlags.mVResize &&
                      (GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT));

  // Reflow our last line if our availableBSize has increased
  // so that we (and our last child) pull up content as necessary
  if (aState.mReflowState.AvailableBSize() != NS_UNCONSTRAINEDSIZE
      && GetNextInFlow() && aState.mReflowState.AvailableBSize() >
        GetLogicalSize().BSize(aState.mReflowState.GetWritingMode())) {
    line_iterator lastLine = end_lines();
    if (lastLine != begin_lines()) {
      --lastLine;
      lastLine->MarkDirty();
    }
  }
    // the amount by which we will slide the current line if it is not
    // dirty
  nscoord deltaBCoord = 0;

    // whether we did NOT reflow the previous line and thus we need to
    // recompute the carried out margin before the line if we want to
    // reflow it or if its previous margin is dirty
  bool needToRecoverState = false;
    // Float continuations were reflowed in ReflowPushedFloats
  bool reflowedFloat = mFloats.NotEmpty() &&
    (mFloats.FirstChild()->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT);
  bool lastLineMovedUp = false;
  // We save up information about BR-clearance here
  uint8_t inlineFloatBreakType = aState.mFloatBreakType;

  line_iterator line = begin_lines(), line_end = end_lines();

  // Reflow the lines that are already ours
  for ( ; line != line_end; ++line, aState.AdvanceToNextLine()) {
    DumpLine(aState, line, deltaBCoord, 0);
#ifdef DEBUG
    AutoNoisyIndenter indent2(gNoisyReflow);
#endif

    if (selfDirty)
      line->MarkDirty();

    // This really sucks, but we have to look inside any blocks that have clear
    // elements inside them.
    // XXX what can we do smarter here?
    if (!line->IsDirty() && line->IsBlock() &&
        (line->mFirstChild->GetStateBits() & NS_BLOCK_HAS_CLEAR_CHILDREN)) {
      line->MarkDirty();
    }

    nsIFrame *replacedBlock = nullptr;
    if (line->IsBlock() &&
        !nsBlockFrame::BlockCanIntersectFloats(line->mFirstChild)) {
      replacedBlock = line->mFirstChild;
    }

    // We have to reflow the line if it's a block whose clearance
    // might have changed, so detect that.
    if (!line->IsDirty() &&
        (line->GetBreakTypeBefore() != NS_STYLE_CLEAR_NONE ||
         replacedBlock)) {
      nscoord curBCoord = aState.mBCoord;
      // See where we would be after applying any clearance due to
      // BRs.
      if (inlineFloatBreakType != NS_STYLE_CLEAR_NONE) {
        curBCoord = aState.ClearFloats(curBCoord, inlineFloatBreakType);
      }

      nscoord newBCoord =
        aState.ClearFloats(curBCoord, line->GetBreakTypeBefore(), replacedBlock);

      if (line->HasClearance()) {
        // Reflow the line if it might not have clearance anymore.
        if (newBCoord == curBCoord
            // aState.mBCoord is the clearance point which should be the
            // top border-edge of the block frame. If sliding the
            // block by deltaBCoord isn't going to put it in the predicted
            // position, then we'd better reflow the line.
            || newBCoord != line->BStart() + deltaBCoord) {
          line->MarkDirty();
        }
      } else {
        // Reflow the line if the line might have clearance now.
        if (curBCoord != newBCoord) {
          line->MarkDirty();
        }
      }
    }

    // We might have to reflow a line that is after a clearing BR.
    if (inlineFloatBreakType != NS_STYLE_CLEAR_NONE) {
      aState.mBCoord = aState.ClearFloats(aState.mBCoord, inlineFloatBreakType);
      if (aState.mBCoord != line->BStart() + deltaBCoord) {
        // SlideLine is not going to put the line where the clearance
        // put it. Reflow the line to be sure.
        line->MarkDirty();
      }
      inlineFloatBreakType = NS_STYLE_CLEAR_NONE;
    }

    bool previousMarginWasDirty = line->IsPreviousMarginDirty();
    if (previousMarginWasDirty) {
      // If the previous margin is dirty, reflow the current line
      line->MarkDirty();
      line->ClearPreviousMarginDirty();
    } else if (line->BEnd() + deltaBCoord > aState.mBEndEdge) {
      // Lines that aren't dirty but get slid past our height constraint must
      // be reflowed.
      line->MarkDirty();
    }

    // If we have a constrained height (i.e., breaking columns/pages),
    // and the distance to the bottom might have changed, then we need
    // to reflow any line that might have floats in it, both because the
    // breakpoints within those floats may have changed and because we
    // might have to push/pull the floats in their entirety.
    // FIXME: What about a deltaBCoord or block-size change that forces us to
    // push lines?  Why does that work?
    if (!line->IsDirty() &&
        aState.mReflowState.AvailableBSize() != NS_UNCONSTRAINEDSIZE &&
        (deltaBCoord != 0 || aState.mReflowState.mFlags.mVResize ||
         aState.mReflowState.mFlags.mMustReflowPlaceholders) &&
        (line->IsBlock() || line->HasFloats() || line->HadFloatPushed())) {
      line->MarkDirty();
    }

    if (!line->IsDirty()) {
      // See if there's any reflow damage that requires that we mark the
      // line dirty.
      PropagateFloatDamage(aState, line, deltaBCoord);
    }

    // If the container width has changed reset the container width. If the
    // line's writing mode is not ltr, or if the line is not left-aligned, also
    // mark the line dirty.
    if (aState.mContainerWidth != line->mContainerWidth) {
      line->mContainerWidth = aState.mContainerWidth;

      bool isLastLine = line == mLines.back() &&
                        !GetNextInFlow() &&
                        NS_STYLE_TEXT_ALIGN_AUTO == StyleText()->mTextAlignLast;
      uint8_t align = isLastLine ?
        StyleText()->mTextAlign : StyleText()->mTextAlignLast;

      if (line->mWritingMode.IsVertical() ||
          !line->mWritingMode.IsBidiLTR() ||
          !IsAlignedLeft(align,
                         aState.mReflowState.mStyleVisibility->mDirection,
                         StyleTextReset()->mUnicodeBidi, this)) {
        line->MarkDirty();
      }
    }

    if (needToRecoverState && line->IsDirty()) {
      // We need to reconstruct the bottom margin only if we didn't
      // reflow the previous line and we do need to reflow (or repair
      // the top position of) the next line.
      aState.ReconstructMarginBefore(line);
    }

    bool reflowedPrevLine = !needToRecoverState;
    if (needToRecoverState) {
      needToRecoverState = false;

      // Update aState.mPrevChild as if we had reflowed all of the frames in
      // this line.
      if (line->IsDirty()) {
        NS_ASSERTION(line->mFirstChild->GetPrevSibling() ==
                     line.prev()->LastChild(), "unexpected line frames");
        aState.mPrevChild = line->mFirstChild->GetPrevSibling();
      }
    }

    // Now repair the line and update |aState.mBCoord| by calling
    // |ReflowLine| or |SlideLine|.
    // If we're going to reflow everything again, then no need to reflow
    // the dirty line ... unless the line has floats, in which case we'd
    // better reflow it now to refresh its float cache, which may contain
    // dangling frame pointers! Ugh! This reflow of the line may be
    // incorrect because we skipped reflowing previous lines (e.g., floats
    // may be placed incorrectly), but that's OK because we'll mark the
    // line dirty below under "if (aState.mReflowState.mDiscoveredClearance..."
    if (line->IsDirty() && (line->HasFloats() || !willReflowAgain)) {
      lastLineMovedUp = true;

      bool maybeReflowingForFirstTime =
        line->IStart() == 0 && line->BStart() == 0 &&
        line->ISize() == 0 && line->BSize() == 0;

      // Compute the dirty lines "before" BEnd, after factoring in
      // the running deltaBCoord value - the running value is implicit in
      // aState.mBCoord.
      nscoord oldB = line->BStart();
      nscoord oldBMost = line->BEnd();

      NS_ASSERTION(!willReflowAgain || !line->IsBlock(),
                   "Don't reflow blocks while willReflowAgain is true, reflow of block abs-pos children depends on this");

      // Reflow the dirty line. If it's an incremental reflow, then force
      // it to invalidate the dirty area if necessary
      ReflowLine(aState, line, &keepGoing);

      if (aState.mReflowState.WillReflowAgainForClearance()) {
        line->MarkDirty();
        willReflowAgain = true;
        // Note that once we've entered this state, every line that gets here
        // (e.g. because it has floats) gets marked dirty and reflowed again.
        // in the next pass. This is important, see above.
      }

      if (line->HasFloats()) {
        reflowedFloat = true;
      }

      if (!keepGoing) {
        DumpLine(aState, line, deltaBCoord, -1);
        if (0 == line->GetChildCount()) {
          DeleteLine(aState, line, line_end);
        }
        break;
      }

      // Test to see whether the margin that should be carried out
      // to the next line (NL) might have changed. In ReflowBlockFrame
      // we call nextLine->MarkPreviousMarginDirty if the block's
      // actual carried-out bottom margin changed. So here we only
      // need to worry about the following effects:
      // 1) the line was just created, and it might now be blocking
      // a carried-out bottom margin from previous lines that
      // used to reach NL from reaching NL
      // 2) the line used to be empty, and is now not empty,
      // thus blocking a carried-out bottom margin from previous lines
      // that used to reach NL from reaching NL
      // 3) the line wasn't empty, but now is, so a carried-out
      // bottom margin from previous lines that didn't used to reach NL
      // now does
      // 4) the line might have changed in a way that affects NL's
      // ShouldApplyBStartMargin decision. The three things that matter
      // are the line's emptiness, its adjacency to the top of the block,
      // and whether it has clearance (the latter only matters if the block
      // was and is adjacent to the top and empty).
      //
      // If the line is empty now, we can't reliably tell if the line was empty
      // before, so we just assume it was and do nextLine->MarkPreviousMarginDirty.
      // This means the checks in 4) are redundant; if the line is empty now
      // we don't need to check 4), but if the line is not empty now and we're sure
      // it wasn't empty before, any adjacency and clearance changes are irrelevant
      // to the result of nextLine->ShouldApplyBStartMargin.
      if (line.next() != end_lines()) {
        bool maybeWasEmpty = oldB == line.next()->BStart();
        bool isEmpty = line->CachedIsEmpty();
        if (maybeReflowingForFirstTime /*1*/ ||
            (isEmpty || maybeWasEmpty) /*2/3/4*/) {
          line.next()->MarkPreviousMarginDirty();
          // since it's marked dirty, nobody will care about |deltaBCoord|
        }
      }

      // If the line was just reflowed for the first time, then its
      // old mBounds cannot be trusted so this deltaBCoord computation is
      // bogus. But that's OK because we just did
      // MarkPreviousMarginDirty on the next line which will force it
      // to be reflowed, so this computation of deltaBCoord will not be
      // used.
      deltaBCoord = line->BEnd() - oldBMost;

      // Now do an interrupt check. We want to do this only in the case when we
      // actually reflow the line, so that if we get back in here we'll get
      // further on the reflow before interrupting.
      aState.mPresContext->CheckForInterrupt(this);
    } else {
      aState.mOverflowTracker->Skip(line->mFirstChild, aState.mReflowStatus);
        // Nop except for blocks (we don't create overflow container
        // continuations for any inlines atm), so only checking mFirstChild
        // is enough

      lastLineMovedUp = deltaBCoord < 0;

      if (deltaBCoord != 0)
        SlideLine(aState, line, deltaBCoord);
      else
        repositionViews = true;

      NS_ASSERTION(!line->IsDirty() || !line->HasFloats(),
                   "Possibly stale float cache here!");
      if (willReflowAgain && line->IsBlock()) {
        // If we're going to reflow everything again, and this line is a block,
        // then there is no need to recover float state. The line may contain
        // other lines with floats, but in that case RecoverStateFrom would only
        // add floats to the float manager. We don't need to do that because
        // everything's going to get reflowed again "for real". Calling
        // RecoverStateFrom in this situation could be lethal because the
        // block's descendant lines may have float caches containing dangling
        // frame pointers. Ugh!
        // If this line is inline, then we need to recover its state now
        // to make sure that we don't forget to move its floats by deltaBCoord.
      } else {
        // XXX EVIL O(N^2) EVIL
        aState.RecoverStateFrom(line, deltaBCoord);
      }

      // Keep mBCoord up to date in case we're propagating reflow damage
      // and also because our final height may depend on it. If the
      // line is inlines, then only update mBCoord if the line is not
      // empty, because that's what PlaceLine does. (Empty blocks may
      // want to update mBCoord, e.g. if they have clearance.)
      if (line->IsBlock() || !line->CachedIsEmpty()) {
        aState.mBCoord = line->BEnd();
      }

      needToRecoverState = true;

      if (reflowedPrevLine && !line->IsBlock() &&
          aState.mPresContext->HasPendingInterrupt()) {
        // Need to make sure to pull overflows from any prev-in-flows
        for (nsIFrame* inlineKid = line->mFirstChild; inlineKid;
             inlineKid = inlineKid->GetFirstPrincipalChild()) {
          inlineKid->PullOverflowsFromPrevInFlow();
        }
      }
    }

    // Record if we need to clear floats before reflowing the next
    // line. Note that inlineFloatBreakType will be handled and
    // cleared before the next line is processed, so there is no
    // need to combine break types here.
    if (line->HasFloatBreakAfter()) {
      inlineFloatBreakType = line->GetBreakTypeAfter();
    }

    if (LineHasClear(line.get())) {
      foundAnyClears = true;
    }

    DumpLine(aState, line, deltaBCoord, -1);

    if (aState.mPresContext->HasPendingInterrupt()) {
      willReflowAgain = true;
      // Another option here might be to leave |line| clean if
      // !HasPendingInterrupt() before the CheckForInterrupt() call, since in
      // that case the line really did reflow as it should have.  Not sure
      // whether that would be safe, so doing this for now instead.  Also not
      // sure whether we really want to mark all lines dirty after an
      // interrupt, but until we get better at propagating float damage we
      // really do need to do it this way; see comments inside MarkLineDirty.
      MarkLineDirtyForInterrupt(line);
    }
  }

  // Handle BR-clearance from the last line of the block
  if (inlineFloatBreakType != NS_STYLE_CLEAR_NONE) {
    aState.mBCoord = aState.ClearFloats(aState.mBCoord, inlineFloatBreakType);
  }

  if (needToRecoverState) {
    // Is this expensive?
    aState.ReconstructMarginBefore(line);

    // Update aState.mPrevChild as if we had reflowed all of the frames in
    // the last line.
    NS_ASSERTION(line == line_end || line->mFirstChild->GetPrevSibling() ==
                 line.prev()->LastChild(), "unexpected line frames");
    aState.mPrevChild =
      line == line_end ? mFrames.LastChild() : line->mFirstChild->GetPrevSibling();
  }

  // Should we really have to do this?
  if (repositionViews)
    nsContainerFrame::PlaceFrameView(this);

  // We can skip trying to pull up the next line if our height is constrained
  // (so we can report being incomplete) and there is no next in flow or we
  // were told not to or we know it will be futile, i.e.,
  // -- the next in flow is not changing
  // -- and we cannot have added more space for its first line to be
  // pulled up into,
  // -- it's an incremental reflow of a descendant
  // -- and we didn't reflow any floats (so the available space
  // didn't change)
  // -- my chain of next-in-flows either has no first line, or its first
  // line isn't dirty.
  bool heightConstrained =
    aState.mReflowState.AvailableBSize() != NS_UNCONSTRAINEDSIZE;
  bool skipPull = willReflowAgain && heightConstrained;
  if (!skipPull && heightConstrained && aState.mNextInFlow &&
      (aState.mReflowState.mFlags.mNextInFlowUntouched &&
       !lastLineMovedUp && 
       !(GetStateBits() & NS_FRAME_IS_DIRTY) &&
       !reflowedFloat)) {
    // We'll place lineIter at the last line of this block, so that 
    // nsBlockInFlowLineIterator::Next() will take us to the first
    // line of my next-in-flow-chain.  (But first, check that I 
    // have any lines -- if I don't, just bail out of this
    // optimization.) 
    line_iterator lineIter = this->end_lines();
    if (lineIter != this->begin_lines()) {
      lineIter--; // I have lines; step back from dummy iterator to last line.
      nsBlockInFlowLineIterator bifLineIter(this, lineIter);

      // Check for next-in-flow-chain's first line.
      // (First, see if there is such a line, and second, see if it's clean)
      if (!bifLineIter.Next() ||                
          !bifLineIter.GetLine()->IsDirty()) {
        skipPull=true;
      }
    }
  }

  if (skipPull && aState.mNextInFlow) {
    NS_ASSERTION(heightConstrained, "Height should be constrained here\n");
    if (IS_TRUE_OVERFLOW_CONTAINER(aState.mNextInFlow))
      NS_FRAME_SET_OVERFLOW_INCOMPLETE(aState.mReflowStatus);
    else
      NS_FRAME_SET_INCOMPLETE(aState.mReflowStatus);
  }
  
  if (!skipPull && aState.mNextInFlow) {
    // Pull data from a next-in-flow if there's still room for more
    // content here.
    while (keepGoing && aState.mNextInFlow) {
      // Grab first line from our next-in-flow
      nsBlockFrame* nextInFlow = aState.mNextInFlow;
      nsLineBox* pulledLine;
      nsFrameList pulledFrames;
      if (!nextInFlow->mLines.empty()) {
        RemoveFirstLine(nextInFlow->mLines, nextInFlow->mFrames,
                        &pulledLine, &pulledFrames);
      } else {
        // Grab an overflow line if there are any
        FrameLines* overflowLines = nextInFlow->GetOverflowLines();
        if (!overflowLines) {
          aState.mNextInFlow =
            static_cast<nsBlockFrame*>(nextInFlow->GetNextInFlow());
          continue;
        }
        bool last =
          RemoveFirstLine(overflowLines->mLines, overflowLines->mFrames,
                          &pulledLine, &pulledFrames);
        if (last) {
          nextInFlow->DestroyOverflowLines();
        }
      }

      if (pulledFrames.IsEmpty()) {
        // The line is empty. Try the next one.
        NS_ASSERTION(pulledLine->GetChildCount() == 0 &&
                     !pulledLine->mFirstChild, "bad empty line");
        nextInFlow->FreeLineBox(pulledLine);
        continue;
      }

      if (pulledLine == nextInFlow->GetLineCursor()) {
        nextInFlow->ClearLineCursor();
      }
      ReparentFrames(pulledFrames, nextInFlow, this);

      NS_ASSERTION(pulledFrames.LastChild() == pulledLine->LastChild(),
                   "Unexpected last frame");
      NS_ASSERTION(aState.mPrevChild || mLines.empty(), "should have a prevchild here");
      NS_ASSERTION(aState.mPrevChild == mFrames.LastChild(),
                   "Incorrect aState.mPrevChild before inserting line at end");

      // Shift pulledLine's frames into our mFrames list.
      mFrames.AppendFrames(nullptr, pulledFrames);

      // Add line to our line list, and set its last child as our new prev-child
      line = mLines.before_insert(end_lines(), pulledLine);
      aState.mPrevChild = mFrames.LastChild();

      // Reparent floats whose placeholders are in the line.
      ReparentFloats(pulledLine->mFirstChild, nextInFlow, true);

      DumpLine(aState, pulledLine, deltaBCoord, 0);
#ifdef DEBUG
      AutoNoisyIndenter indent2(gNoisyReflow);
#endif

      if (aState.mPresContext->HasPendingInterrupt()) {
        MarkLineDirtyForInterrupt(line);
      } else {
        // Now reflow it and any lines that it makes during it's reflow
        // (we have to loop here because reflowing the line may cause a new
        // line to be created; see SplitLine's callers for examples of
        // when this happens).
        while (line != end_lines()) {
          ReflowLine(aState, line, &keepGoing);

          if (aState.mReflowState.WillReflowAgainForClearance()) {
            line->MarkDirty();
            keepGoing = false;
            NS_FRAME_SET_INCOMPLETE(aState.mReflowStatus);
            break;
          }

          DumpLine(aState, line, deltaBCoord, -1);
          if (!keepGoing) {
            if (0 == line->GetChildCount()) {
              DeleteLine(aState, line, line_end);
            }
            break;
          }

          if (LineHasClear(line.get())) {
            foundAnyClears = true;
          }

          if (aState.mPresContext->CheckForInterrupt(this)) {
            MarkLineDirtyForInterrupt(line);
            break;
          }

          // If this is an inline frame then its time to stop
          ++line;
          aState.AdvanceToNextLine();
        }
      }
    }

    if (NS_FRAME_IS_NOT_COMPLETE(aState.mReflowStatus)) {
      aState.mReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
    } //XXXfr shouldn't set this flag when nextinflow has no lines
  }

  // Handle an odd-ball case: a list-item with no lines
  if (HasOutsideBullet() && mLines.empty()) {
    nsHTMLReflowMetrics metrics(aState.mReflowState);
    nsIFrame* bullet = GetOutsideBullet();
    WritingMode wm = aState.mReflowState.GetWritingMode();
    ReflowBullet(bullet, aState, metrics,
                 aState.mReflowState.ComputedPhysicalBorderPadding().top);
    NS_ASSERTION(!BulletIsEmpty() || metrics.BSize(wm) == 0,
                 "empty bullet took up space");

    if (!BulletIsEmpty()) {
      // There are no lines so we have to fake up some y motion so that
      // we end up with *some* height.

      if (metrics.BlockStartAscent() == nsHTMLReflowMetrics::ASK_FOR_BASELINE) {
        nscoord ascent;
        WritingMode wm = aState.mReflowState.GetWritingMode();
        if (nsLayoutUtils::GetFirstLineBaseline(wm, bullet, &ascent)) {
          metrics.SetBlockStartAscent(ascent);
        } else {
          metrics.SetBlockStartAscent(metrics.BSize(wm));
        }
      }

      nsRefPtr<nsFontMetrics> fm;
      nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm),
        nsLayoutUtils::FontSizeInflationFor(this));
      aState.mReflowState.rendContext->SetFont(fm); // FIXME: needed?

      nscoord minAscent =
        nsLayoutUtils::GetCenteredFontBaseline(fm, aState.mMinLineHeight);
      nscoord minDescent = aState.mMinLineHeight - minAscent;

      aState.mBCoord += std::max(minAscent, metrics.BlockStartAscent()) +
                        std::max(minDescent, metrics.BSize(wm) -
                                             metrics.BlockStartAscent());

      nscoord offset = minAscent - metrics.BlockStartAscent();
      if (offset > 0) {
        bullet->SetRect(bullet->GetRect() + nsPoint(0, offset));
      }
    }
  }

  if (foundAnyClears) {
    AddStateBits(NS_BLOCK_HAS_CLEAR_CHILDREN);
  } else {
    RemoveStateBits(NS_BLOCK_HAS_CLEAR_CHILDREN);
  }

#ifdef DEBUG
  VerifyLines(true);
  VerifyOverflowSituation();
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent - 1);
    ListTag(stdout);
    printf(": done reflowing dirty lines (status=%x)\n",
           aState.mReflowStatus);
  }
#endif
}

void
nsBlockFrame::MarkLineDirtyForInterrupt(nsLineBox* aLine)
{
  aLine->MarkDirty();

  // Just checking NS_FRAME_IS_DIRTY is ok, because we've already
  // marked the lines that need to be marked dirty based on our
  // vertical resize stuff.  So we'll definitely reflow all those kids;
  // the only question is how they should behave.
  if (GetStateBits() & NS_FRAME_IS_DIRTY) {
    // Mark all our child frames dirty so we make sure to reflow them
    // later.
    int32_t n = aLine->GetChildCount();
    for (nsIFrame* f = aLine->mFirstChild; n > 0;
         f = f->GetNextSibling(), --n) {
      f->AddStateBits(NS_FRAME_IS_DIRTY);
    }
    // And mark all the floats whose reflows we might be skipping dirty too.
    if (aLine->HasFloats()) {
      for (nsFloatCache* fc = aLine->GetFirstFloat(); fc; fc = fc->Next()) {
        fc->mFloat->AddStateBits(NS_FRAME_IS_DIRTY);
      }
    }
  } else {
    // Dirty all the descendant lines of block kids to handle float damage,
    // since our nsFloatManager will go away by the next time we're reflowing.
    // XXXbz Can we do something more like what PropagateFloatDamage does?
    // Would need to sort out the exact business with mBlockDelta for that....
    // This marks way too much dirty.  If we ever make this better, revisit
    // which lines we mark dirty in the interrupt case in ReflowDirtyLines.
    nsBlockFrame* bf = nsLayoutUtils::GetAsBlock(aLine->mFirstChild);
    if (bf) {
      MarkAllDescendantLinesDirty(bf);
    }
  }
}

void
nsBlockFrame::DeleteLine(nsBlockReflowState& aState,
                         nsLineList::iterator aLine,
                         nsLineList::iterator aLineEnd)
{
  NS_PRECONDITION(0 == aLine->GetChildCount(), "can't delete !empty line");
  if (0 == aLine->GetChildCount()) {
    NS_ASSERTION(aState.mCurrentLine == aLine,
                 "using function more generally than designed, "
                 "but perhaps OK now");
    nsLineBox* line = aLine;
    aLine = mLines.erase(aLine);
    FreeLineBox(line);
    // Mark the previous margin of the next line dirty since we need to
    // recompute its top position.
    if (aLine != aLineEnd)
      aLine->MarkPreviousMarginDirty();
  }
}

/**
 * Reflow a line. The line will either contain a single block frame
 * or contain 1 or more inline frames. aKeepReflowGoing indicates
 * whether or not the caller should continue to reflow more lines.
 */
void
nsBlockFrame::ReflowLine(nsBlockReflowState& aState,
                         line_iterator aLine,
                         bool* aKeepReflowGoing)
{
  NS_ABORT_IF_FALSE(aLine->GetChildCount(), "reflowing empty line");

  // Setup the line-layout for the new line
  aState.mCurrentLine = aLine;
  aLine->ClearDirty();
  aLine->InvalidateCachedIsEmpty();
  aLine->ClearHadFloatPushed();

  // Now that we know what kind of line we have, reflow it
  if (aLine->IsBlock()) {
    ReflowBlockFrame(aState, aLine, aKeepReflowGoing);
  } else {
    aLine->SetLineWrapped(false);
    ReflowInlineFrames(aState, aLine, aKeepReflowGoing);
  }
}

nsIFrame*
nsBlockFrame::PullFrame(nsBlockReflowState& aState,
                        line_iterator       aLine)
{
  // First check our remaining lines.
  if (end_lines() != aLine.next()) {
    return PullFrameFrom(aLine, this, aLine.next());
  }

  NS_ASSERTION(!GetOverflowLines(),
    "Our overflow lines should have been removed at the start of reflow");

  // Try each next-in-flow.
  nsBlockFrame* nextInFlow = aState.mNextInFlow;
  while (nextInFlow) {
    if (nextInFlow->mLines.empty()) {
      nextInFlow->DrainSelfOverflowList();
    }
    if (!nextInFlow->mLines.empty()) {
      return PullFrameFrom(aLine, nextInFlow, nextInFlow->mLines.begin());
    }
    nextInFlow = static_cast<nsBlockFrame*>(nextInFlow->GetNextInFlow());
    aState.mNextInFlow = nextInFlow;
  }

  return nullptr;
}

nsIFrame*
nsBlockFrame::PullFrameFrom(nsLineBox*           aLine,
                            nsBlockFrame*        aFromContainer,
                            nsLineList::iterator aFromLine)
{
  nsLineBox* fromLine = aFromLine;
  NS_ABORT_IF_FALSE(fromLine, "bad line to pull from");
  NS_ABORT_IF_FALSE(fromLine->GetChildCount(), "empty line");
  NS_ABORT_IF_FALSE(aLine->GetChildCount(), "empty line");

  NS_ASSERTION(fromLine->IsBlock() == fromLine->mFirstChild->IsBlockOutside(),
               "Disagreement about whether it's a block or not");

  if (fromLine->IsBlock()) {
    // If our line is not empty and the child in aFromLine is a block
    // then we cannot pull up the frame into this line. In this case
    // we stop pulling.
    return nullptr;
  }
  // Take frame from fromLine
  nsIFrame* frame = fromLine->mFirstChild;
  nsIFrame* newFirstChild = frame->GetNextSibling();

  if (aFromContainer != this) {
    // The frame is being pulled from a next-in-flow; therefore we
    // need to add it to our sibling list.
    MOZ_ASSERT(aLine == mLines.back());
    MOZ_ASSERT(aFromLine == aFromContainer->mLines.begin(),
               "should only pull from first line");
    aFromContainer->mFrames.RemoveFrame(frame);

    // When pushing and pulling frames we need to check for whether any
    // views need to be reparented.
    ReparentFrame(frame, aFromContainer, this);
    mFrames.AppendFrame(nullptr, frame);

    // The frame might have (or contain) floats that need to be brought
    // over too. (pass 'false' since there are no siblings to check)
    ReparentFloats(frame, aFromContainer, false);
  } else {
    MOZ_ASSERT(aLine == aFromLine.prev());
  }

  aLine->NoteFrameAdded(frame);
  fromLine->NoteFrameRemoved(frame);

  if (fromLine->GetChildCount() > 0) {
    // Mark line dirty now that we pulled a child
    fromLine->MarkDirty();
    fromLine->mFirstChild = newFirstChild;
  } else {
    // Free up the fromLine now that it's empty.
    // Its bounds might need to be redrawn, though.
    if (aFromLine.next() != aFromContainer->mLines.end()) {
      aFromLine.next()->MarkPreviousMarginDirty();
    }
    aFromContainer->mLines.erase(aFromLine);
    // aFromLine is now invalid
    aFromContainer->FreeLineBox(fromLine);
  }

#ifdef DEBUG
  VerifyLines(true);
  VerifyOverflowSituation();
#endif

  return frame;
}

void
nsBlockFrame::SlideLine(nsBlockReflowState& aState,
                        nsLineBox* aLine, nscoord aDY)
{
  NS_PRECONDITION(aDY != 0, "why slide a line nowhere?");

  // Adjust line state
  aLine->SlideBy(aDY, aState.mContainerWidth);

  // Adjust the frames in the line
  nsIFrame* kid = aLine->mFirstChild;
  if (!kid) {
    return;
  }

  if (aLine->IsBlock()) {
    if (aDY) {
      kid->MovePositionBy(nsPoint(0, aDY));
    }

    // Make sure the frame's view and any child views are updated
    nsContainerFrame::PlaceFrameView(kid);
  }
  else {
    // Adjust the Y coordinate of the frames in the line.
    // Note: we need to re-position views even if aDY is 0, because
    // one of our parent frames may have moved and so the view's position
    // relative to its parent may have changed
    int32_t n = aLine->GetChildCount();
    while (--n >= 0) {
      if (aDY) {
        kid->MovePositionBy(nsPoint(0, aDY));
      }
      // Make sure the frame's view and any child views are updated
      nsContainerFrame::PlaceFrameView(kid);
      kid = kid->GetNextSibling();
    }
  }
}

nsresult 
nsBlockFrame::AttributeChanged(int32_t         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               int32_t         aModType)
{
  nsresult rv = nsBlockFrameSuper::AttributeChanged(aNameSpaceID,
                                                    aAttribute, aModType);

  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsGkAtoms::start == aAttribute ||
      (nsGkAtoms::reversed == aAttribute && mContent->IsHTML(nsGkAtoms::ol))) {
    nsPresContext* presContext = PresContext();

    // XXX Not sure if this is necessary anymore
    if (RenumberLists(presContext)) {
      presContext->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eStyleChange,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }
  else if (nsGkAtoms::value == aAttribute) {
    const nsStyleDisplay* styleDisplay = StyleDisplay();
    if (NS_STYLE_DISPLAY_LIST_ITEM == styleDisplay->mDisplay) {
      // Search for the closest ancestor that's a block frame. We
      // make the assumption that all related list items share a
      // common block parent.
      // XXXldb I think that's a bad assumption.
      nsBlockFrame* blockParent = nsLayoutUtils::FindNearestBlockAncestor(this);

      // Tell the enclosing block frame to renumber list items within
      // itself
      if (nullptr != blockParent) {
        nsPresContext* presContext = PresContext();
        // XXX Not sure if this is necessary anymore
        if (blockParent->RenumberLists(presContext)) {
          presContext->PresShell()->
            FrameNeedsReflow(blockParent, nsIPresShell::eStyleChange,
                             NS_FRAME_HAS_DIRTY_CHILDREN);
        }
      }
    }
  }

  return rv;
}

static inline bool
IsNonAutoNonZeroHeight(const nsStyleCoord& aCoord)
{
  if (aCoord.GetUnit() == eStyleUnit_Auto)
    return false;
  if (aCoord.IsCoordPercentCalcUnit()) {
    // If we evaluate the length/percent/calc at a percentage basis of
    // both nscoord_MAX and 0, and it's zero both ways, then it's a zero
    // length, percent, or combination thereof.  Test > 0 so we clamp
    // negative calc() results to 0.
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, nscoord_MAX) > 0 ||
           nsRuleNode::ComputeCoordPercentCalc(aCoord, 0) > 0;
  }
  NS_ABORT_IF_FALSE(false, "unexpected unit for height or min-height");
  return true;
}

/* virtual */ bool
nsBlockFrame::IsSelfEmpty()
{
  // Blocks which are margin-roots (including inline-blocks) cannot be treated
  // as empty for margin-collapsing and other purposes. They're more like
  // replaced elements.
  if (GetStateBits() & NS_BLOCK_MARGIN_ROOT)
    return false;

  const nsStylePosition* position = StylePosition();

  if (IsNonAutoNonZeroHeight(position->mMinHeight) ||
      IsNonAutoNonZeroHeight(position->mHeight))
    return false;

  const nsStyleBorder* border = StyleBorder();
  const nsStylePadding* padding = StylePadding();
  if (border->GetComputedBorderWidth(NS_SIDE_TOP) != 0 ||
      border->GetComputedBorderWidth(NS_SIDE_BOTTOM) != 0 ||
      !nsLayoutUtils::IsPaddingZero(padding->mPadding.GetTop()) ||
      !nsLayoutUtils::IsPaddingZero(padding->mPadding.GetBottom())) {
    return false;
  }

  if (HasOutsideBullet() && !BulletIsEmpty()) {
    return false;
  }

  return true;
}

bool
nsBlockFrame::CachedIsEmpty()
{
  if (!IsSelfEmpty()) {
    return false;
  }

  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line)
  {
    if (!line->CachedIsEmpty())
      return false;
  }

  return true;
}

bool
nsBlockFrame::IsEmpty()
{
  if (!IsSelfEmpty()) {
    return false;
  }

  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line)
  {
    if (!line->IsEmpty())
      return false;
  }

  return true;
}

bool
nsBlockFrame::ShouldApplyBStartMargin(nsBlockReflowState& aState,
                                      nsLineBox* aLine,
                                      nsIFrame* aChildFrame)
{
  if (aState.GetFlag(BRS_APPLYBSTARTMARGIN)) {
    // Apply short-circuit check to avoid searching the line list
    return true;
  }

  if (!aState.IsAdjacentWithTop() ||
      aChildFrame->StyleBorder()->mBoxDecorationBreak ==
        NS_STYLE_BOX_DECORATION_BREAK_CLONE) {
    // If we aren't at the top Y coordinate then something of non-zero
    // height must have been placed. Therefore the childs top-margin
    // applies.
    aState.SetFlag(BRS_APPLYBSTARTMARGIN, true);
    return true;
  }

  // Determine if this line is "essentially" the first line
  line_iterator line = begin_lines();
  if (aState.GetFlag(BRS_HAVELINEADJACENTTOTOP)) {
    line = aState.mLineAdjacentToTop;
  }
  while (line != aLine) {
    if (!line->CachedIsEmpty() || line->HasClearance()) {
      // A line which precedes aLine is non-empty, or has clearance,
      // so therefore the top margin applies.
      aState.SetFlag(BRS_APPLYBSTARTMARGIN, true);
      return true;
    }
    // No need to apply the top margin if the line has floats.  We
    // should collapse anyway (bug 44419)
    ++line;
    aState.SetFlag(BRS_HAVELINEADJACENTTOTOP, true);
    aState.mLineAdjacentToTop = line;
  }

  // The line being reflowed is "essentially" the first line in the
  // block. Therefore its top-margin will be collapsed by the
  // generational collapsing logic with its parent (us).
  return false;
}

void
nsBlockFrame::ReflowBlockFrame(nsBlockReflowState& aState,
                               line_iterator aLine,
                               bool* aKeepReflowGoing)
{
  NS_PRECONDITION(*aKeepReflowGoing, "bad caller");

  nsIFrame* frame = aLine->mFirstChild;
  if (!frame) {
    NS_ASSERTION(false, "program error - unexpected empty line"); 
    return; 
  }

  // Prepare the block reflow engine
  const nsStyleDisplay* display = frame->StyleDisplay();
  nsBlockReflowContext brc(aState.mPresContext, aState.mReflowState);

  uint8_t breakType = display->mBreakType;
  if (NS_STYLE_CLEAR_NONE != aState.mFloatBreakType) {
    breakType = nsLayoutUtils::CombineBreakType(breakType,
                                                aState.mFloatBreakType);
    aState.mFloatBreakType = NS_STYLE_CLEAR_NONE;
  }

  // Clear past floats before the block if the clear style is not none
  aLine->SetBreakTypeBefore(breakType);

  // See if we should apply the top margin. If the block frame being
  // reflowed is a continuation (non-null prev-in-flow) then we don't
  // apply its top margin because it's not significant unless it has
  // 'box-decoration-break:clone'.  Otherwise, dig deeper.
  bool applyBStartMargin = (frame->StyleBorder()->mBoxDecorationBreak ==
                              NS_STYLE_BOX_DECORATION_BREAK_CLONE ||
                            !frame->GetPrevInFlow()) &&
                           ShouldApplyBStartMargin(aState, aLine, frame);
  if (applyBStartMargin) {
    // The HasClearance setting is only valid if ShouldApplyBStartMargin
    // returned false (in which case the top-margin-root set our
    // clearance flag). Otherwise clear it now. We'll set it later on
    // ourselves if necessary.
    aLine->ClearHasClearance();
  }
  bool treatWithClearance = aLine->HasClearance();

  bool mightClearFloats = breakType != NS_STYLE_CLEAR_NONE;
  nsIFrame *replacedBlock = nullptr;
  if (!nsBlockFrame::BlockCanIntersectFloats(frame)) {
    mightClearFloats = true;
    replacedBlock = frame;
  }

  // If our top margin was counted as part of some parents top-margin
  // collapse and we are being speculatively reflowed assuming this
  // frame DID NOT need clearance, then we need to check that
  // assumption.
  if (!treatWithClearance && !applyBStartMargin && mightClearFloats &&
      aState.mReflowState.mDiscoveredClearance) {
    nscoord curBCoord = aState.mBCoord + aState.mPrevBEndMargin.get();
    nscoord clearBCoord = aState.ClearFloats(curBCoord, breakType, replacedBlock);
    if (clearBCoord != curBCoord) {
      // Looks like that assumption was invalid, we do need
      // clearance. Tell our ancestor so it can reflow again. It is
      // responsible for actually setting our clearance flag before
      // the next reflow.
      treatWithClearance = true;
      // Only record the first frame that requires clearance
      if (!*aState.mReflowState.mDiscoveredClearance) {
        *aState.mReflowState.mDiscoveredClearance = frame;
      }
      aState.mPrevChild = frame;
      // Exactly what we do now is flexible since we'll definitely be
      // reflowed.
      return;
    }
  }
  if (treatWithClearance) {
    applyBStartMargin = true;
  }

  nsIFrame* clearanceFrame = nullptr;
  nscoord startingBCoord = aState.mBCoord;
  nsCollapsingMargin incomingMargin = aState.mPrevBEndMargin;
  nscoord clearance;
  // Save the original position of the frame so that we can reposition
  // its view as needed.
  nsPoint originalPosition = frame->GetPosition();
  while (true) {
    clearance = 0;
    nscoord bStartMargin = 0;
    bool mayNeedRetry = false;
    bool clearedFloats = false;
    if (applyBStartMargin) {
      // Precompute the blocks top margin value so that we can get the
      // correct available space (there might be a float that's
      // already been placed below the aState.mPrevBEndMargin

      // Setup a reflowState to get the style computed margin-top
      // value. We'll use a reason of `resize' so that we don't fudge
      // any incremental reflow state.

      // The availSpace here is irrelevant to our needs - all we want
      // out if this setup is the margin-top value which doesn't depend
      // on the childs available space.
      // XXX building a complete nsHTMLReflowState just to get the margin-top
      // seems like a waste. And we do this for almost every block!
      nsSize availSpace(aState.ContentISize(), NS_UNCONSTRAINEDSIZE);
      nsHTMLReflowState reflowState(aState.mPresContext, aState.mReflowState,
                                    frame, availSpace);

      if (treatWithClearance) {
        aState.mBCoord += aState.mPrevBEndMargin.get();
        aState.mPrevBEndMargin.Zero();
      }

      // Now compute the collapsed margin-block-start value into
      // aState.mPrevBEndMargin, assuming that all child margins
      // collapse down to clearanceFrame.
      nsBlockReflowContext::ComputeCollapsedBStartMargin(reflowState,
                                                         &aState.mPrevBEndMargin,
                                                         clearanceFrame,
                                                         &mayNeedRetry);

      // XXX optimization; we could check the collapsing children to see if they are sure
      // to require clearance, and so avoid retrying them

      if (clearanceFrame) {
        // Don't allow retries on the second pass. The clearance decisions for the
        // blocks whose top-margins collapse with ours are now fixed.
        mayNeedRetry = false;
      }

      if (!treatWithClearance && !clearanceFrame && mightClearFloats) {
        // We don't know if we need clearance and this is the first,
        // optimistic pass.  So determine whether *this block* needs
        // clearance. Note that we do not allow the decision for whether
        // this block has clearance to change on the second pass; that
        // decision is only allowed to be made under the optimistic
        // first pass.
        nscoord curBCoord = aState.mBCoord + aState.mPrevBEndMargin.get();
        nscoord clearBCoord = aState.ClearFloats(curBCoord, breakType, replacedBlock);
        if (clearBCoord != curBCoord) {
          // Looks like we need clearance and we didn't know about it already. So
          // recompute collapsed margin
          treatWithClearance = true;
          // Remember this decision, needed for incremental reflow
          aLine->SetHasClearance();

          // Apply incoming margins
          aState.mBCoord += aState.mPrevBEndMargin.get();
          aState.mPrevBEndMargin.Zero();

          // Compute the collapsed margin again, ignoring the incoming margin this time
          mayNeedRetry = false;
          nsBlockReflowContext::ComputeCollapsedBStartMargin(reflowState,
                                                             &aState.mPrevBEndMargin,
                                                             clearanceFrame,
                                                             &mayNeedRetry);
        }
      }

      // Temporarily advance the running Y value so that the
      // GetAvailableSpace method will return the right available
      // space. This undone as soon as the horizontal margins are
      // computed.
      bStartMargin = aState.mPrevBEndMargin.get();

      if (treatWithClearance) {
        nscoord currentBCoord = aState.mBCoord;
        // advance mBCoord to the clear position.
        aState.mBCoord = aState.ClearFloats(aState.mBCoord, breakType,
                                            replacedBlock);

        clearedFloats = aState.mBCoord != currentBCoord;

        // Compute clearance. It's the amount we need to add to the block-start
        // border-edge of the frame, after applying collapsed margins
        // from the frame and its children, to get it to line up with
        // the block-end of the floats. The former is
        // currentBCoord + bStartMargin, the latter is the current
        // aState.mBCoord.
        // Note that negative clearance is possible
        clearance = aState.mBCoord - (currentBCoord + bStartMargin);

        // Add clearance to our top margin while we compute available
        // space for the frame
        bStartMargin += clearance;

        // Note that aState.mBCoord should stay where it is: at the block-start
        // border-edge of the frame
      } else {
        // Advance aState.mBCoord to the block-start border-edge of the frame.
        aState.mBCoord += bStartMargin;
      }
    }

    // Here aState.mBCoord is the block-start border-edge of the block.
    // Compute the available space for the block
    nsFlowAreaRect floatAvailableSpace = aState.GetFloatAvailableSpace();
#ifdef REALLY_NOISY_REFLOW
    printf("setting line %p isImpacted to %s\n",
           aLine.get(), floatAvailableSpace.mHasFloats?"true":"false");
#endif
    aLine->SetLineIsImpactedByFloat(floatAvailableSpace.mHasFloats);
    nsRect availSpace;
    aState.ComputeBlockAvailSpace(frame, display, floatAvailableSpace,
                                  replacedBlock != nullptr, availSpace);

    // The check for
    //   (!aState.mReflowState.mFlags.mIsTopOfPage || clearedFloats)
    // is to some degree out of paranoia:  if we reliably eat up top
    // margins at the top of the page as we ought to, it wouldn't be
    // needed.
    if ((!aState.mReflowState.mFlags.mIsTopOfPage || clearedFloats) &&
        availSpace.height < 0) {
      // We know already that this child block won't fit on this
      // page/column due to the top margin or the clearance.  So we need
      // to get out of here now.  (If we don't, most blocks will handle
      // things fine, and report break-before, but zero-height blocks
      // won't, and will thus make their parent overly-large and force
      // *it* to be pushed in its entirety.)
      // Doing this means that we also don't need to worry about the
      // |availSpace.height += bStartMargin| below interacting with pushed
      // floats (which force nscoord_MAX clearance) to cause a
      // constrained height to turn into an unconstrained one.
      aState.mBCoord = startingBCoord;
      aState.mPrevBEndMargin = incomingMargin;
      *aKeepReflowGoing = false;
      if (ShouldAvoidBreakInside(aState.mReflowState)) {
        aState.mReflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
      } else {
        PushLines(aState, aLine.prev());
        NS_FRAME_SET_INCOMPLETE(aState.mReflowStatus);
      }
      return;
    }

    // Now put the Y coordinate back to the top of the top-margin +
    // clearance, and flow the block.
    aState.mBCoord -= bStartMargin;
    availSpace.y -= bStartMargin;
    if (NS_UNCONSTRAINEDSIZE != availSpace.height) {
      availSpace.height += bStartMargin;
    }

    // Reflow the block into the available space
    // construct the html reflow state for the block. ReflowBlock
    // will initialize it
    nsHTMLReflowState blockHtmlRS(aState.mPresContext, aState.mReflowState, frame,
                                  availSpace.Size());
    blockHtmlRS.mFlags.mHasClearance = aLine->HasClearance();

    nsFloatManager::SavedState floatManagerState;
    if (mayNeedRetry) {
      blockHtmlRS.mDiscoveredClearance = &clearanceFrame;
      aState.mFloatManager->PushState(&floatManagerState);
    } else if (!applyBStartMargin) {
      blockHtmlRS.mDiscoveredClearance = aState.mReflowState.mDiscoveredClearance;
    }

    nsReflowStatus frameReflowStatus = NS_FRAME_COMPLETE;
    brc.ReflowBlock(availSpace, applyBStartMargin, aState.mPrevBEndMargin,
                    clearance, aState.IsAdjacentWithTop(),
                    aLine.get(), blockHtmlRS, frameReflowStatus, aState);

    if (mayNeedRetry && clearanceFrame) {
      aState.mFloatManager->PopState(&floatManagerState);
      aState.mBCoord = startingBCoord;
      aState.mPrevBEndMargin = incomingMargin;
      continue;
    }

    aState.mPrevChild = frame;

    if (blockHtmlRS.WillReflowAgainForClearance()) {
      // If an ancestor of ours is going to reflow for clearance, we
      // need to avoid calling PlaceBlock, because it unsets dirty bits
      // on the child block (both itself, and through its call to
      // nsFrame::DidReflow), and those dirty bits imply dirtiness for
      // all of the child block, including the lines it didn't reflow.
      NS_ASSERTION(originalPosition == frame->GetPosition(),
                   "we need to call PositionChildViews");
      return;
    }

#if defined(REFLOW_STATUS_COVERAGE)
    RecordReflowStatus(true, frameReflowStatus);
#endif
    
    if (NS_INLINE_IS_BREAK_BEFORE(frameReflowStatus)) {
      // None of the child block fits.
      *aKeepReflowGoing = false;
      if (ShouldAvoidBreakInside(aState.mReflowState)) {
        aState.mReflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
      } else {
        PushLines(aState, aLine.prev());
        NS_FRAME_SET_INCOMPLETE(aState.mReflowStatus);
      }
    }
    else {
      // Note: line-break-after a block is a nop
      
      // Try to place the child block.
      // Don't force the block to fit if we have positive clearance, because
      // pushing it to the next page would give it more room.
      // Don't force the block to fit if it's impacted by a float. If it is,
      // then pushing it to the next page would give it more room. Note that
      // isImpacted doesn't include impact from the block's own floats.
      bool forceFit = aState.IsAdjacentWithTop() && clearance <= 0 &&
        !floatAvailableSpace.mHasFloats;
      nsCollapsingMargin collapsedBEndMargin;
      nsOverflowAreas overflowAreas;
      *aKeepReflowGoing = brc.PlaceBlock(blockHtmlRS, forceFit, aLine.get(),
                                         collapsedBEndMargin,
                                         overflowAreas,
                                         frameReflowStatus);
      if (!NS_FRAME_IS_FULLY_COMPLETE(frameReflowStatus) &&
          ShouldAvoidBreakInside(aState.mReflowState)) {
        *aKeepReflowGoing = false;
      }

      if (aLine->SetCarriedOutBEndMargin(collapsedBEndMargin)) {
        line_iterator nextLine = aLine;
        ++nextLine;
        if (nextLine != end_lines()) {
          nextLine->MarkPreviousMarginDirty();
        }
      }

      aLine->SetOverflowAreas(overflowAreas);
      if (*aKeepReflowGoing) {
        // Some of the child block fit
        
        // Advance to new Y position
        nscoord newBCoord = aLine->BEnd();
        aState.mBCoord = newBCoord;

        
        // Continue the block frame now if it didn't completely fit in
        // the available space.
        if (!NS_FRAME_IS_FULLY_COMPLETE(frameReflowStatus)) {
          bool madeContinuation =
            CreateContinuationFor(aState, nullptr, frame);
          
          nsIFrame* nextFrame = frame->GetNextInFlow();
          NS_ASSERTION(nextFrame, "We're supposed to have a next-in-flow by now");
          
          if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus)) {
            // If nextFrame used to be an overflow container, make it a normal block
            if (!madeContinuation &&
                (NS_FRAME_IS_OVERFLOW_CONTAINER & nextFrame->GetStateBits())) {
              nsOverflowContinuationTracker::AutoFinish fini(aState.mOverflowTracker, frame);
              nsContainerFrame* parent = nextFrame->GetParent();
              nsresult rv = parent->StealFrame(nextFrame);
              if (NS_FAILED(rv)) {
                return;
              }
              if (parent != this)
                ReparentFrame(nextFrame, parent, this);
              mFrames.InsertFrame(nullptr, frame, nextFrame);
              madeContinuation = true; // needs to be added to mLines
              nextFrame->RemoveStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
              frameReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
            }

            // Push continuation to a new line, but only if we actually made one.
            if (madeContinuation) {
              nsLineBox* line = NewLineBox(nextFrame, true);
              mLines.after_insert(aLine, line);
            }

            PushLines(aState, aLine);
            NS_FRAME_SET_INCOMPLETE(aState.mReflowStatus);

            // If we need to reflow the continuation of the block child,
            // then we'd better reflow our continuation
            if (frameReflowStatus & NS_FRAME_REFLOW_NEXTINFLOW) {
              aState.mReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
              // We also need to make that continuation's line dirty so
              // it gets reflowed when we reflow our next in flow. The
              // nif's line must always be either a line of the nif's
              // parent block (only if we didn't make a continuation) or
              // else one of our own overflow lines. In the latter case
              // the line is already marked dirty, so just handle the
              // first case.
              if (!madeContinuation) {
                nsBlockFrame* nifBlock =
                  nsLayoutUtils::GetAsBlock(nextFrame->GetParent());
                NS_ASSERTION(nifBlock,
                             "A block's child's next in flow's parent must be a block!");
                for (line_iterator line = nifBlock->begin_lines(),
                     line_end = nifBlock->end_lines(); line != line_end; ++line) {
                  if (line->Contains(nextFrame)) {
                    line->MarkDirty();
                    break;
                  }
                }
              }
            }
            *aKeepReflowGoing = false;
            
            // The bottom margin for a block is only applied on the last
            // flow block. Since we just continued the child block frame,
            // we know that line->mFirstChild is not the last flow block
            // therefore zero out the running margin value.
#ifdef NOISY_BLOCK_DIR_MARGINS
            ListTag(stdout);
            printf(": reflow incomplete, frame=");
            nsFrame::ListTag(stdout, frame);
            printf(" prevBEndMargin=%d, setting to zero\n",
                   aState.mPrevBEndMargin);
#endif
            aState.mPrevBEndMargin.Zero();
          }
          else { // frame is complete but its overflow is not complete
            // Disconnect the next-in-flow and put it in our overflow tracker
            if (!madeContinuation &&
                !(NS_FRAME_IS_OVERFLOW_CONTAINER & nextFrame->GetStateBits())) {
              // It already exists, but as a normal next-in-flow, so we need
              // to dig it out of the child lists.
              nsresult rv = nextFrame->GetParent()->StealFrame(nextFrame);
              if (NS_FAILED(rv)) {
                return;
              }
            }
            else if (madeContinuation) {
              mFrames.RemoveFrame(nextFrame);
            }

            // Put it in our overflow list
            aState.mOverflowTracker->Insert(nextFrame, frameReflowStatus);
            NS_MergeReflowStatusInto(&aState.mReflowStatus, frameReflowStatus);

#ifdef NOISY_BLOCK_DIR_MARGINS
            ListTag(stdout);
            printf(": reflow complete but overflow incomplete for ");
            nsFrame::ListTag(stdout, frame);
            printf(" prevBEndMargin=%d collapsedBEndMargin=%d\n",
                   aState.mPrevBEndMargin, collapsedBEndMargin.get());
#endif
            aState.mPrevBEndMargin = collapsedBEndMargin;
          }
        }
        else { // frame is fully complete
#ifdef NOISY_BLOCK_DIR_MARGINS
          ListTag(stdout);
          printf(": reflow complete for ");
          nsFrame::ListTag(stdout, frame);
          printf(" prevBEndMargin=%d collapsedBEndMargin=%d\n",
                 aState.mPrevBEndMargin, collapsedBEndMargin.get());
#endif
          aState.mPrevBEndMargin = collapsedBEndMargin;
        }
#ifdef NOISY_BLOCK_DIR_MARGINS
        ListTag(stdout);
        printf(": frame=");
        nsFrame::ListTag(stdout, frame);
        printf(" carriedOutBEndMargin=%d collapsedBEndMargin=%d => %d\n",
               brc.GetCarriedOutBEndMargin(), collapsedBEndMargin.get(),
               aState.mPrevBEndMargin);
#endif
      } else {
        if ((aLine == mLines.front() && !GetPrevInFlow()) ||
            ShouldAvoidBreakInside(aState.mReflowState)) {
          // If it's our very first line *or* we're not at the top of the page
          // and we have page-break-inside:avoid, then we need to be pushed to
          // our parent's next-in-flow.
          aState.mReflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
        } else {
          // Push the line that didn't fit and any lines that follow it
          // to our next-in-flow.
          PushLines(aState, aLine.prev());
          NS_FRAME_SET_INCOMPLETE(aState.mReflowStatus);
        }
      }
    }
    break; // out of the reflow retry loop
  }

  // Now that we've got its final position all figured out, position any child
  // views it may have.  Note that the case when frame has a view got handled
  // by FinishReflowChild, but that function didn't have the coordinates needed
  // to correctly decide whether to reposition child views.
  if (originalPosition != frame->GetPosition() && !frame->HasView()) {
    nsContainerFrame::PositionChildViews(frame);
  }
  
#ifdef DEBUG
  VerifyLines(true);
#endif
}

void
nsBlockFrame::ReflowInlineFrames(nsBlockReflowState& aState,
                                 line_iterator aLine,
                                 bool* aKeepReflowGoing)
{
  *aKeepReflowGoing = true;

  aLine->SetLineIsImpactedByFloat(false);

  // Setup initial coordinate system for reflowing the inline frames
  // into. Apply a previous block frame's bottom margin first.
  if (ShouldApplyBStartMargin(aState, aLine, aLine->mFirstChild)) {
    aState.mBCoord += aState.mPrevBEndMargin.get();
  }
  nsFlowAreaRect floatAvailableSpace = aState.GetFloatAvailableSpace();

  LineReflowStatus lineReflowStatus;
  do {
    nscoord availableSpaceHeight = 0;
    do {
      bool allowPullUp = true;
      nsIContent* forceBreakInContent = nullptr;
      int32_t forceBreakOffset = -1;
      gfxBreakPriority forceBreakPriority = gfxBreakPriority::eNoBreak;
      do {
        nsFloatManager::SavedState floatManagerState;
        aState.mReflowState.mFloatManager->PushState(&floatManagerState);

        // Once upon a time we allocated the first 30 nsLineLayout objects
        // on the stack, and then we switched to the heap.  At that time
        // these objects were large (1100 bytes on a 32 bit system).
        // Then the nsLineLayout object was shrunk to 156 bytes by
        // removing some internal buffers.  Given that it is so much
        // smaller, the complexity of 2 different ways of allocating
        // no longer makes sense.  Now we always allocate on the stack.
        nsLineLayout lineLayout(aState.mPresContext,
                                aState.mReflowState.mFloatManager,
                                &aState.mReflowState, &aLine);
        lineLayout.Init(&aState, aState.mMinLineHeight, aState.mLineNumber);
        if (forceBreakInContent) {
          lineLayout.ForceBreakAtPosition(forceBreakInContent, forceBreakOffset);
        }
        DoReflowInlineFrames(aState, lineLayout, aLine,
                             floatAvailableSpace, availableSpaceHeight,
                             &floatManagerState, aKeepReflowGoing,
                             &lineReflowStatus, allowPullUp);
        lineLayout.EndLineReflow();

        if (LINE_REFLOW_REDO_NO_PULL == lineReflowStatus ||
            LINE_REFLOW_REDO_MORE_FLOATS == lineReflowStatus ||
            LINE_REFLOW_REDO_NEXT_BAND == lineReflowStatus) {
          if (lineLayout.NeedsBackup()) {
            NS_ASSERTION(!forceBreakInContent, "Backing up twice; this should never be necessary");
            // If there is no saved break position, then this will set
            // set forceBreakInContent to null and we won't back up, which is
            // correct.
            forceBreakInContent = lineLayout.GetLastOptionalBreakPosition(&forceBreakOffset, &forceBreakPriority);
          } else {
            forceBreakInContent = nullptr;
          }
          // restore the float manager state
          aState.mReflowState.mFloatManager->PopState(&floatManagerState);
          // Clear out float lists
          aState.mCurrentLineFloats.DeleteAll();
          aState.mBelowCurrentLineFloats.DeleteAll();
        }

        // Don't allow pullup on a subsequent LINE_REFLOW_REDO_NO_PULL pass
        allowPullUp = false;
      } while (LINE_REFLOW_REDO_NO_PULL == lineReflowStatus);
    } while (LINE_REFLOW_REDO_MORE_FLOATS == lineReflowStatus);
  } while (LINE_REFLOW_REDO_NEXT_BAND == lineReflowStatus);
}

void
nsBlockFrame::PushTruncatedLine(nsBlockReflowState& aState,
                                line_iterator       aLine,
                                bool*               aKeepReflowGoing)
{
  PushLines(aState, aLine.prev());
  *aKeepReflowGoing = false;
  NS_FRAME_SET_INCOMPLETE(aState.mReflowStatus);
}

#ifdef DEBUG
static const char* LineReflowStatusNames[] = {
  "LINE_REFLOW_OK", "LINE_REFLOW_STOP", "LINE_REFLOW_REDO_NO_PULL",
  "LINE_REFLOW_REDO_MORE_FLOATS",
  "LINE_REFLOW_REDO_NEXT_BAND", "LINE_REFLOW_TRUNCATED"
};
#endif

void
nsBlockFrame::DoReflowInlineFrames(nsBlockReflowState& aState,
                                   nsLineLayout& aLineLayout,
                                   line_iterator aLine,
                                   nsFlowAreaRect& aFloatAvailableSpace,
                                   nscoord& aAvailableSpaceHeight,
                                   nsFloatManager::SavedState*
                                     aFloatStateBeforeLine,
                                   bool* aKeepReflowGoing,
                                   LineReflowStatus* aLineReflowStatus,
                                   bool aAllowPullUp)
{
  // Forget all of the floats on the line
  aLine->FreeFloats(aState.mFloatCacheFreeList);
  aState.mFloatOverflowAreas.Clear();

  // We need to set this flag on the line if any of our reflow passes
  // are impacted by floats.
  if (aFloatAvailableSpace.mHasFloats)
    aLine->SetLineIsImpactedByFloat(true);
#ifdef REALLY_NOISY_REFLOW
  printf("nsBlockFrame::DoReflowInlineFrames %p impacted = %d\n",
         this, aFloatAvailableSpace.mHasFloats);
#endif

  WritingMode lineWM = GetWritingMode(aLine->mFirstChild);
  LogicalRect lineRect(lineWM, aFloatAvailableSpace.mRect,
                       aState.mContainerWidth);

  nscoord iStart = lineRect.IStart(lineWM);
  nscoord availISize = lineRect.ISize(lineWM);
  nscoord availBSize;
  if (aState.GetFlag(BRS_UNCONSTRAINEDBSIZE)) {
    availBSize = NS_UNCONSTRAINEDSIZE;
  }
  else {
    /* XXX get the height right! */
    availBSize = lineRect.BSize(lineWM);
  }

  // Make sure to enable resize optimization before we call BeginLineReflow
  // because it might get disabled there
  aLine->EnableResizeReflowOptimization();

  aLineLayout.BeginLineReflow(iStart, aState.mBCoord,
                              availISize, availBSize,
                              aFloatAvailableSpace.mHasFloats,
                              false, /*XXX isTopOfPage*/
                              lineWM, aState.mContainerWidth);

  aState.SetFlag(BRS_LINE_LAYOUT_EMPTY, false);

  // XXX Unfortunately we need to know this before reflowing the first
  // inline frame in the line. FIX ME.
  if ((0 == aLineLayout.GetLineNumber()) &&
      (NS_BLOCK_HAS_FIRST_LETTER_CHILD & mState) &&
      (NS_BLOCK_HAS_FIRST_LETTER_STYLE & mState)) {
    aLineLayout.SetFirstLetterStyleOK(true);
  }
  NS_ASSERTION(!((NS_BLOCK_HAS_FIRST_LETTER_CHILD & mState) &&
                 GetPrevContinuation()),
               "first letter child bit should only be on first continuation");

  // Reflow the frames that are already on the line first
  LineReflowStatus lineReflowStatus = LINE_REFLOW_OK;
  int32_t i;
  nsIFrame* frame = aLine->mFirstChild;

  if (aFloatAvailableSpace.mHasFloats) {
    // There is a soft break opportunity at the start of the line, because
    // we can always move this line down below float(s).
    if (aLineLayout.NotifyOptionalBreakPosition(frame->GetContent(), 0, true, gfxBreakPriority::eNormalBreak)) {
      lineReflowStatus = LINE_REFLOW_REDO_NEXT_BAND;
    }
  }

  // need to repeatedly call GetChildCount here, because the child
  // count can change during the loop!
  for (i = 0; LINE_REFLOW_OK == lineReflowStatus && i < aLine->GetChildCount();
       i++, frame = frame->GetNextSibling()) {
    ReflowInlineFrame(aState, aLineLayout, aLine, frame, &lineReflowStatus);
    if (LINE_REFLOW_OK != lineReflowStatus) {
      // It is possible that one or more of next lines are empty
      // (because of DeleteNextInFlowChild). If so, delete them now
      // in case we are finished.
      ++aLine;
      while ((aLine != end_lines()) && (0 == aLine->GetChildCount())) {
        // XXX Is this still necessary now that DeleteNextInFlowChild
        // uses DoRemoveFrame?
        nsLineBox *toremove = aLine;
        aLine = mLines.erase(aLine);
        NS_ASSERTION(nullptr == toremove->mFirstChild, "bad empty line");
        FreeLineBox(toremove);
      }
      --aLine;

      NS_ASSERTION(lineReflowStatus != LINE_REFLOW_TRUNCATED,
                   "ReflowInlineFrame should never determine that a line "
                   "needs to go to the next page/column");
    }
  }

  // Don't pull up new frames into lines with continuation placeholders
  if (aAllowPullUp) {
    // Pull frames and reflow them until we can't
    while (LINE_REFLOW_OK == lineReflowStatus) {
      frame = PullFrame(aState, aLine);
      if (!frame) {
        break;
      }

      while (LINE_REFLOW_OK == lineReflowStatus) {
        int32_t oldCount = aLine->GetChildCount();
        ReflowInlineFrame(aState, aLineLayout, aLine, frame, &lineReflowStatus);
        if (aLine->GetChildCount() != oldCount) {
          // We just created a continuation for aFrame AND its going
          // to end up on this line (e.g. :first-letter
          // situation). Therefore we have to loop here before trying
          // to pull another frame.
          frame = frame->GetNextSibling();
        }
        else {
          break;
        }
      }
    }
  }

  aState.SetFlag(BRS_LINE_LAYOUT_EMPTY, aLineLayout.LineIsEmpty());

  // We only need to backup if the line isn't going to be reflowed again anyway
  bool needsBackup = aLineLayout.NeedsBackup() &&
    (lineReflowStatus == LINE_REFLOW_STOP || lineReflowStatus == LINE_REFLOW_OK);
  if (needsBackup && aLineLayout.HaveForcedBreakPosition()) {
  	NS_WARNING("We shouldn't be backing up more than once! "
               "Someone must have set a break opportunity beyond the available width, "
               "even though there were better break opportunities before it");
    needsBackup = false;
  }
  if (needsBackup) {
    // We need to try backing up to before a text run
    int32_t offset;
    gfxBreakPriority breakPriority;
    nsIContent* breakContent = aLineLayout.GetLastOptionalBreakPosition(&offset, &breakPriority);
    // XXX It's possible, in fact not unusual, for the break opportunity to already
    // be the end of the line. We should detect that and optimize to not
    // re-do the line.
    if (breakContent) {
      // We can back up!
      lineReflowStatus = LINE_REFLOW_REDO_NO_PULL;
    }
  } else {
    // In case we reflow this line again, remember that we don't
    // need to force any breaking
    aLineLayout.ClearOptionalBreakPosition();
  }

  if (LINE_REFLOW_REDO_NEXT_BAND == lineReflowStatus) {
    // This happens only when we have a line that is impacted by
    // floats and the first element in the line doesn't fit with
    // the floats.
    //
    // What we do is to advance past the first float we find and
    // then reflow the line all over again.
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aFloatAvailableSpace.mRect.height,
                 "unconstrained height on totally empty line");

    // See the analogous code for blocks in nsBlockReflowState::ClearFloats.
    if (aFloatAvailableSpace.mRect.height > 0) {
      NS_ASSERTION(aFloatAvailableSpace.mHasFloats,
                   "redo line on totally empty line with non-empty band...");
      // We should never hit this case if we've placed floats on the
      // line; if we have, then the GetFloatAvailableSpace call is wrong
      // and needs to happen after the caller pops the space manager
      // state.
      aState.mFloatManager->AssertStateMatches(aFloatStateBeforeLine);
      aState.mBCoord += aFloatAvailableSpace.mRect.height;
      aFloatAvailableSpace = aState.GetFloatAvailableSpace();
    } else {
      NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aState.mReflowState.AvailableBSize(),
                   "We shouldn't be running out of height here");
      if (NS_UNCONSTRAINEDSIZE == aState.mReflowState.AvailableBSize()) {
        // just move it down a bit to try to get out of this mess
        aState.mBCoord += 1;
        // We should never hit this case if we've placed floats on the
        // line; if we have, then the GetFloatAvailableSpace call is wrong
        // and needs to happen after the caller pops the space manager
        // state.
        aState.mFloatManager->AssertStateMatches(aFloatStateBeforeLine);
        aFloatAvailableSpace = aState.GetFloatAvailableSpace();
      } else {
        // There's nowhere to retry placing the line, so we want to push
        // it to the next page/column where its contents can fit not
        // next to a float.
        lineReflowStatus = LINE_REFLOW_TRUNCATED;
        PushTruncatedLine(aState, aLine, aKeepReflowGoing);
      }
    }

    // XXX: a small optimization can be done here when paginating:
    // if the new Y coordinate is past the end of the block then
    // push the line and return now instead of later on after we are
    // past the float.
  }
  else if (LINE_REFLOW_TRUNCATED != lineReflowStatus &&
           LINE_REFLOW_REDO_NO_PULL != lineReflowStatus) {
    // If we are propagating out a break-before status then there is
    // no point in placing the line.
    if (!NS_INLINE_IS_BREAK_BEFORE(aState.mReflowStatus)) {
      if (!PlaceLine(aState, aLineLayout, aLine, aFloatStateBeforeLine,
                     aFloatAvailableSpace.mRect, aAvailableSpaceHeight,
                     aKeepReflowGoing)) {
        lineReflowStatus = LINE_REFLOW_REDO_MORE_FLOATS;
        // PlaceLine already called GetAvailableSpaceForBSize for us.
      }
    }
  }
#ifdef DEBUG
  if (gNoisyReflow) {
    printf("Line reflow status = %s\n", LineReflowStatusNames[lineReflowStatus]);
  }
#endif

  if (aLineLayout.GetDirtyNextLine()) {
    // aLine may have been pushed to the overflow lines.
    FrameLines* overflowLines = GetOverflowLines();
    // We can't just compare iterators front() to aLine here, since they may be in
    // different lists.
    bool pushedToOverflowLines = overflowLines &&
      overflowLines->mLines.front() == aLine.get();
    if (pushedToOverflowLines) {
      // aLine is stale, it's associated with the main line list but it should
      // be associated with the overflow line list now
      aLine = overflowLines->mLines.begin();
    }
    nsBlockInFlowLineIterator iter(this, aLine, pushedToOverflowLines);
    if (iter.Next() && iter.GetLine()->IsInline()) {
      iter.GetLine()->MarkDirty();
      if (iter.GetContainer() != this) {
        aState.mReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
      }
    }
  }

  *aLineReflowStatus = lineReflowStatus;
}

/**
 * Reflow an inline frame. The reflow status is mapped from the frames
 * reflow status to the lines reflow status (not to our reflow status).
 * The line reflow status is simple: true means keep placing frames
 * on the line; false means don't (the line is done). If the line
 * has some sort of breaking affect then aLine's break-type will be set
 * to something other than NS_STYLE_CLEAR_NONE.
 */
void
nsBlockFrame::ReflowInlineFrame(nsBlockReflowState& aState,
                                nsLineLayout& aLineLayout,
                                line_iterator aLine,
                                nsIFrame* aFrame,
                                LineReflowStatus* aLineReflowStatus)
{
  if (!aFrame) { // XXX change to MOZ_ASSERT(aFrame)
    NS_ERROR("why call me?");
    return;
  }
  
  *aLineReflowStatus = LINE_REFLOW_OK;

#ifdef NOISY_FIRST_LETTER
  ListTag(stdout);
  printf(": reflowing ");
  nsFrame::ListTag(stdout, aFrame);
  printf(" reflowingFirstLetter=%s\n",
         aLineLayout.GetFirstLetterStyleOK() ? "on" : "off");
#endif

  // Reflow the inline frame
  nsReflowStatus frameReflowStatus;
  bool           pushedFrame;
  aLineLayout.ReflowFrame(aFrame, frameReflowStatus, nullptr, pushedFrame);

  if (frameReflowStatus & NS_FRAME_REFLOW_NEXTINFLOW) {
    aLineLayout.SetDirtyNextLine();
  }

#ifdef REALLY_NOISY_REFLOW_CHILD
  nsFrame::ListTag(stdout, aFrame);
  printf(": status=%x\n", frameReflowStatus);
#endif

#if defined(REFLOW_STATUS_COVERAGE)
  RecordReflowStatus(false, frameReflowStatus);
#endif

  // Send post-reflow notification
  aState.mPrevChild = aFrame;

   /* XXX
      This is where we need to add logic to handle some odd behavior.
      For one thing, we should usually place at least one thing next
      to a left float, even when that float takes up all the width on a line.
      see bug 22496
   */

  // Process the child frames reflow status. There are 5 cases:
  // complete, not-complete, break-before, break-after-complete,
  // break-after-not-complete. There are two situations: we are a
  // block or we are an inline. This makes a total of 10 cases
  // (fortunately, there is some overlap).
  aLine->SetBreakTypeAfter(NS_STYLE_CLEAR_NONE);
  if (NS_INLINE_IS_BREAK(frameReflowStatus) || 
      (NS_STYLE_CLEAR_NONE != aState.mFloatBreakType)) {
    // Always abort the line reflow (because a line break is the
    // minimal amount of break we do).
    *aLineReflowStatus = LINE_REFLOW_STOP;

    // XXX what should aLine's break-type be set to in all these cases?
    uint8_t breakType = NS_INLINE_GET_BREAK_TYPE(frameReflowStatus);
    NS_ASSERTION((NS_STYLE_CLEAR_NONE != breakType) || 
                 (NS_STYLE_CLEAR_NONE != aState.mFloatBreakType), "bad break type");
    NS_ASSERTION(NS_STYLE_CLEAR_MAX >= breakType, "invalid break type");

    if (NS_INLINE_IS_BREAK_BEFORE(frameReflowStatus)) {
      // Break-before cases.
      if (aFrame == aLine->mFirstChild) {
        // If we break before the first frame on the line then we must
        // be trying to place content where there's no room (e.g. on a
        // line with wide floats). Inform the caller to reflow the
        // line after skipping past a float.
        *aLineReflowStatus = LINE_REFLOW_REDO_NEXT_BAND;
      }
      else {
        // It's not the first child on this line so go ahead and split
        // the line. We will see the frame again on the next-line.
        SplitLine(aState, aLineLayout, aLine, aFrame, aLineReflowStatus);

        // If we're splitting the line because the frame didn't fit and it
        // was pushed, then mark the line as having word wrapped. We need to
        // know that if we're shrink wrapping our width
        if (pushedFrame) {
          aLine->SetLineWrapped(true);
        }
      }
    }
    else {
      // If a float split and its prev-in-flow was followed by a <BR>, then combine 
      // the <BR>'s break type with the inline's break type (the inline will be the very 
      // next frame after the split float).
      if (NS_STYLE_CLEAR_NONE != aState.mFloatBreakType) {
        breakType = nsLayoutUtils::CombineBreakType(breakType,
                                                    aState.mFloatBreakType);
        aState.mFloatBreakType = NS_STYLE_CLEAR_NONE;
      }
      // Break-after cases
      if (breakType == NS_STYLE_CLEAR_LINE) {
        if (!aLineLayout.GetLineEndsInBR()) {
          breakType = NS_STYLE_CLEAR_NONE;
        }
      }
      aLine->SetBreakTypeAfter(breakType);
      if (NS_FRAME_IS_COMPLETE(frameReflowStatus)) {
        // Split line, but after the frame just reflowed
        SplitLine(aState, aLineLayout, aLine, aFrame->GetNextSibling(), aLineReflowStatus);

        if (NS_INLINE_IS_BREAK_AFTER(frameReflowStatus) &&
            !aLineLayout.GetLineEndsInBR()) {
          aLineLayout.SetDirtyNextLine();
        }
      }
    }
  }

  if (!NS_FRAME_IS_FULLY_COMPLETE(frameReflowStatus)) {
    // Create a continuation for the incomplete frame. Note that the
    // frame may already have a continuation.
    CreateContinuationFor(aState, aLine, aFrame);

    // Remember that the line has wrapped
    if (!aLineLayout.GetLineEndsInBR()) {
      aLine->SetLineWrapped(true);
    }
    
    // If we just ended a first-letter frame or reflowed a placeholder then 
    // don't split the line and don't stop the line reflow...
    // But if we are going to stop anyways we'd better split the line.
    if ((!(frameReflowStatus & NS_INLINE_BREAK_FIRST_LETTER_COMPLETE) && 
         nsGkAtoms::placeholderFrame != aFrame->GetType()) ||
        *aLineReflowStatus == LINE_REFLOW_STOP) {
      // Split line after the current frame
      *aLineReflowStatus = LINE_REFLOW_STOP;
      SplitLine(aState, aLineLayout, aLine, aFrame->GetNextSibling(), aLineReflowStatus);
    }
  }
}

bool
nsBlockFrame::CreateContinuationFor(nsBlockReflowState& aState,
                                    nsLineBox*          aLine,
                                    nsIFrame*           aFrame)
{
  nsIFrame* newFrame = nullptr;

  if (!aFrame->GetNextInFlow()) {
    newFrame = aState.mPresContext->PresShell()->FrameConstructor()->
      CreateContinuingFrame(aState.mPresContext, aFrame, this);

    mFrames.InsertFrame(nullptr, aFrame, newFrame);

    if (aLine) {
      aLine->NoteFrameAdded(newFrame);
    }
  }
#ifdef DEBUG
  VerifyLines(false);
#endif
  return !!newFrame;
}

nsresult
nsBlockFrame::SplitFloat(nsBlockReflowState& aState,
                         nsIFrame*           aFloat,
                         nsReflowStatus      aFloatStatus)
{
  nsIFrame* nextInFlow = aFloat->GetNextInFlow();
  if (nextInFlow) {
    nsContainerFrame *oldParent = nextInFlow->GetParent();
    DebugOnly<nsresult> rv = oldParent->StealFrame(nextInFlow);
    NS_ASSERTION(NS_SUCCEEDED(rv), "StealFrame failed");
    if (oldParent != this) {
      ReparentFrame(nextInFlow, oldParent, this);
    }
  } else {
    nextInFlow = aState.mPresContext->PresShell()->FrameConstructor()->
      CreateContinuingFrame(aState.mPresContext, aFloat, this);
  }
  if (NS_FRAME_OVERFLOW_IS_INCOMPLETE(aFloatStatus))
    aFloat->GetNextInFlow()->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);

  // The containing block is now overflow-incomplete.
  NS_FRAME_SET_OVERFLOW_INCOMPLETE(aState.mReflowStatus);

  if (aFloat->StyleDisplay()->mFloats == NS_STYLE_FLOAT_LEFT) {
    aState.mFloatManager->SetSplitLeftFloatAcrossBreak();
  } else {
    NS_ABORT_IF_FALSE(aFloat->StyleDisplay()->mFloats ==
                        NS_STYLE_FLOAT_RIGHT, "unexpected float side");
    aState.mFloatManager->SetSplitRightFloatAcrossBreak();
  }

  aState.AppendPushedFloat(nextInFlow);
  return NS_OK;
}

static nsFloatCache*
GetLastFloat(nsLineBox* aLine)
{
  nsFloatCache* fc = aLine->GetFirstFloat();
  while (fc && fc->Next()) {
    fc = fc->Next();
  }
  return fc;
}

static bool
CheckPlaceholderInLine(nsIFrame* aBlock, nsLineBox* aLine, nsFloatCache* aFC)
{
  if (!aFC)
    return true;
  NS_ASSERTION(!aFC->mFloat->GetPrevContinuation(),
               "float in a line should never be a continuation");
  NS_ASSERTION(!(aFC->mFloat->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT),
               "float in a line should never be a pushed float");
  nsIFrame* ph = aBlock->PresContext()->FrameManager()->
                   GetPlaceholderFrameFor(aFC->mFloat->FirstInFlow());
  for (nsIFrame* f = ph; f; f = f->GetParent()) {
    if (f->GetParent() == aBlock)
      return aLine->Contains(f);
  }
  NS_ASSERTION(false, "aBlock is not an ancestor of aFrame!");
  return true;
}

void
nsBlockFrame::SplitLine(nsBlockReflowState& aState,
                        nsLineLayout& aLineLayout,
                        line_iterator aLine,
                        nsIFrame* aFrame,
                        LineReflowStatus* aLineReflowStatus)
{
  NS_ABORT_IF_FALSE(aLine->IsInline(), "illegal SplitLine on block line");

  int32_t pushCount = aLine->GetChildCount() - aLineLayout.GetCurrentSpanCount();
  NS_ABORT_IF_FALSE(pushCount >= 0, "bad push count"); 

#ifdef DEBUG
  if (gNoisyReflow) {
    nsFrame::IndentBy(stdout, gNoiseIndent);
    printf("split line: from line=%p pushCount=%d aFrame=",
           static_cast<void*>(aLine.get()), pushCount);
    if (aFrame) {
      nsFrame::ListTag(stdout, aFrame);
    }
    else {
      printf("(null)");
    }
    printf("\n");
    if (gReallyNoisyReflow) {
      aLine->List(stdout, gNoiseIndent+1);
    }
  }
#endif

  if (0 != pushCount) {
    NS_ABORT_IF_FALSE(aLine->GetChildCount() > pushCount, "bad push");
    NS_ABORT_IF_FALSE(nullptr != aFrame, "whoops");
#ifdef DEBUG
    {
      nsIFrame *f = aFrame;
      int32_t count = pushCount;
      while (f && count > 0) {
        f = f->GetNextSibling();
        --count;
      }
      NS_ASSERTION(count == 0, "Not enough frames to push");
    }
#endif

    // Put frames being split out into their own line
    nsLineBox* newLine = NewLineBox(aLine, aFrame, pushCount);
    mLines.after_insert(aLine, newLine);
#ifdef DEBUG
    if (gReallyNoisyReflow) {
      newLine->List(stdout, gNoiseIndent+1);
    }
#endif

    // Let line layout know that some frames are no longer part of its
    // state.
    aLineLayout.SplitLineTo(aLine->GetChildCount());

    // If floats have been placed whose placeholders have been pushed to the new
    // line, we need to reflow the old line again. We don't want to look at the
    // frames in the new line, because as a large paragraph is laid out the 
    // we'd get O(N^2) performance. So instead we just check that the last
    // float and the last below-current-line float are still in aLine.
    if (!CheckPlaceholderInLine(this, aLine, GetLastFloat(aLine)) ||
        !CheckPlaceholderInLine(this, aLine, aState.mBelowCurrentLineFloats.Tail())) {
      *aLineReflowStatus = LINE_REFLOW_REDO_NO_PULL;
    }

#ifdef DEBUG
    VerifyLines(true);
#endif
  }
}

bool
nsBlockFrame::IsLastLine(nsBlockReflowState& aState,
                         line_iterator aLine)
{
  while (++aLine != end_lines()) {
    // There is another line
    if (0 != aLine->GetChildCount()) {
      // If the next line is a block line then this line is the last in a
      // group of inline lines.
      return aLine->IsBlock();
    }
    // The next line is empty, try the next one
  }

  // XXX Not sure about this part
  // Try our next-in-flows lines to answer the question
  nsBlockFrame* nextInFlow = (nsBlockFrame*) GetNextInFlow();
  while (nullptr != nextInFlow) {
    for (line_iterator line = nextInFlow->begin_lines(),
                   line_end = nextInFlow->end_lines();
         line != line_end;
         ++line)
    {
      if (0 != line->GetChildCount())
        return line->IsBlock();
    }
    nextInFlow = (nsBlockFrame*) nextInFlow->GetNextInFlow();
  }

  // This is the last line - so don't allow justification
  return true;
}

bool
nsBlockFrame::PlaceLine(nsBlockReflowState& aState,
                        nsLineLayout&       aLineLayout,
                        line_iterator       aLine,
                        nsFloatManager::SavedState *aFloatStateBeforeLine,
                        nsRect&             aFloatAvailableSpace,
                        nscoord&            aAvailableSpaceHeight,
                        bool*             aKeepReflowGoing)
{
  // Trim extra white-space from the line before placing the frames
  aLineLayout.TrimTrailingWhiteSpace();

  // Vertically align the frames on this line.
  //
  // According to the CSS2 spec, section 12.6.1, the "marker" box
  // participates in the height calculation of the list-item box's
  // first line box.
  //
  // There are exactly two places a bullet can be placed: near the
  // first or second line. It's only placed on the second line in a
  // rare case: when the first line is empty.
  bool addedBullet = false;
  if (HasOutsideBullet() &&
      ((aLine == mLines.front() &&
        (!aLineLayout.IsZeroBSize() || (aLine == mLines.back()))) ||
       (mLines.front() != mLines.back() &&
        0 == mLines.front()->BSize() &&
        aLine == mLines.begin().next()))) {
    nsHTMLReflowMetrics metrics(aState.mReflowState);
    nsIFrame* bullet = GetOutsideBullet();
    ReflowBullet(bullet, aState, metrics, aState.mBCoord);
    NS_ASSERTION(!BulletIsEmpty() ||
                 metrics.BSize(aState.mReflowState.GetWritingMode()) == 0,
                 "empty bullet took up space");
    aLineLayout.AddBulletFrame(bullet, metrics);
    addedBullet = true;
  }
  aLineLayout.VerticalAlignLine();

  // We want to compare to the available space that we would have had in
  // the line's height *before* we placed any floats in the line itself.
  // Floats that are in the line are handled during line reflow (and may
  // result in floats being pushed to below the line or (I HOPE???) in a
  // reflow with a forced break position).
  nsRect oldFloatAvailableSpace(aFloatAvailableSpace);
  // As we redo for floats, we can't reduce the amount of height we're
  // checking.
  aAvailableSpaceHeight = std::max(aAvailableSpaceHeight, aLine->BSize());
  aFloatAvailableSpace =
    aState.GetFloatAvailableSpaceForBSize(aLine->BStart(),
                                          aAvailableSpaceHeight,
                                          aFloatStateBeforeLine).mRect;
  NS_ASSERTION(aFloatAvailableSpace.y == oldFloatAvailableSpace.y, "yikes");
  // Restore the height to the position of the next band.
  aFloatAvailableSpace.height = oldFloatAvailableSpace.height;
  // If the available space between the floats is smaller now that we
  // know the height, return false (and cause another pass with
  // LINE_REFLOW_REDO_MORE_FLOATS).
  if (AvailableSpaceShrunk(oldFloatAvailableSpace, aFloatAvailableSpace)) {
    return false;
  }

#ifdef DEBUG
  {
    static nscoord lastHeight = 0;
    if (CRAZY_SIZE(aLine->BStart())) {
      lastHeight = aLine->BStart();
      if (abs(aLine->BStart() - lastHeight) > CRAZY_COORD/10) {
        nsFrame::ListTag(stdout);
        printf(": line=%p y=%d line.bounds.height=%d\n",
               static_cast<void*>(aLine.get()),
               aLine->BStart(), aLine->BSize());
      }
    }
    else {
      lastHeight = 0;
    }
  }
#endif

  // Only block frames horizontally align their children because
  // inline frames "shrink-wrap" around their children (therefore
  // there is no extra horizontal space).
  const nsStyleText* styleText = StyleText();

  /**
   * text-align-last defaults to the same value as text-align when
   * text-align-last is set to auto (except when text-align is set to justify),
   * so in that case we don't need to set isLastLine.
   *
   * In other words, isLastLine really means isLastLineAndWeCare.
   */
  bool isLastLine =
    !IsSVGText() &&
    ((NS_STYLE_TEXT_ALIGN_AUTO != styleText->mTextAlignLast ||
      NS_STYLE_TEXT_ALIGN_JUSTIFY == styleText->mTextAlign) &&
     (aLineLayout.GetLineEndsInBR() ||
      IsLastLine(aState, aLine)));

  aLineLayout.TextAlignLine(aLine, isLastLine);

  // From here on, pfd->mBounds rectangles are incorrect because bidi
  // might have moved frames around!
  nsOverflowAreas overflowAreas;
  aLineLayout.RelativePositionFrames(overflowAreas);
  aLine->SetOverflowAreas(overflowAreas);
  if (addedBullet) {
    aLineLayout.RemoveBulletFrame(GetOutsideBullet());
  }

  // Inline lines do not have margins themselves; however they are
  // impacted by prior block margins. If this line ends up having some
  // height then we zero out the previous bottom margin value that was
  // already applied to the line's starting Y coordinate. Otherwise we
  // leave it be so that the previous blocks bottom margin can be
  // collapsed with a block that follows.
  nscoord newBCoord;

  if (!aLine->CachedIsEmpty()) {
    // This line has some height. Therefore the application of the
    // previous-bottom-margin should stick.
    aState.mPrevBEndMargin.Zero();
    newBCoord = aLine->BEnd();
  }
  else {
    // Don't let the previous-bottom-margin value affect the newBCoord
    // coordinate (it was applied in ReflowInlineFrames speculatively)
    // since the line is empty.
    // We already called |ShouldApplyBStartMargin|, and if we applied it
    // then BRS_APPLYBSTARTMARGIN is set.
    nscoord dy = aState.GetFlag(BRS_APPLYBSTARTMARGIN)
                   ? -aState.mPrevBEndMargin.get() : 0;
    newBCoord = aState.mBCoord + dy;
  }

  if (!NS_FRAME_IS_FULLY_COMPLETE(aState.mReflowStatus) &&
      ShouldAvoidBreakInside(aState.mReflowState)) {
    aLine->AppendFloats(aState.mCurrentLineFloats);
    aState.mReflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
    return true;
  }

  // See if the line fit (our first line always does).
  if (mLines.front() != aLine &&
      newBCoord > aState.mBEndEdge &&
      aState.mBEndEdge != NS_UNCONSTRAINEDSIZE) {
    NS_ASSERTION(aState.mCurrentLine == aLine, "oops");
    if (ShouldAvoidBreakInside(aState.mReflowState)) {
      // All our content doesn't fit, start on the next page.
      aState.mReflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
    } else {
      // Push aLine and all of its children and anything else that
      // follows to our next-in-flow.
      PushTruncatedLine(aState, aLine, aKeepReflowGoing);
    }
    return true;
  }

  aState.mBCoord = newBCoord;

  // Add the already placed current-line floats to the line
  aLine->AppendFloats(aState.mCurrentLineFloats);

  // Any below current line floats to place?
  if (aState.mBelowCurrentLineFloats.NotEmpty()) {
    // Reflow the below-current-line floats, which places on the line's
    // float list.
    aState.PlaceBelowCurrentLineFloats(aState.mBelowCurrentLineFloats, aLine);
    aLine->AppendFloats(aState.mBelowCurrentLineFloats);
  }

  // When a line has floats, factor them into the combined-area
  // computations.
  if (aLine->HasFloats()) {
    // Combine the float combined area (stored in aState) and the
    // value computed by the line layout code.
    nsOverflowAreas lineOverflowAreas;
    NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
      nsRect &o = lineOverflowAreas.Overflow(otype);
      o = aLine->GetOverflowArea(otype);
#ifdef NOISY_COMBINED_AREA
      ListTag(stdout);
      printf(": overflow %d lineCA=%d,%d,%d,%d floatCA=%d,%d,%d,%d\n",
             otype,
             o.x, o.y, o.width, o.height,
             aState.mFloatOverflowAreas.Overflow(otype).x,
             aState.mFloatOverflowAreas.Overflow(otype).y,
             aState.mFloatOverflowAreas.Overflow(otype).width,
             aState.mFloatOverflowAreas.Overflow(otype).height);
#endif
      o.UnionRect(aState.mFloatOverflowAreas.Overflow(otype), o);

#ifdef NOISY_COMBINED_AREA
      printf("  ==> final lineCA=%d,%d,%d,%d\n",
             o.x, o.y, o.width, o.height);
#endif
    }
    aLine->SetOverflowAreas(lineOverflowAreas);
  }

  // Apply break-after clearing if necessary
  // This must stay in sync with |ReflowDirtyLines|.
  if (aLine->HasFloatBreakAfter()) {
    aState.mBCoord = aState.ClearFloats(aState.mBCoord, aLine->GetBreakTypeAfter());
  }
  return true;
}

void
nsBlockFrame::PushLines(nsBlockReflowState&  aState,
                        nsLineList::iterator aLineBefore)
{
  // NOTE: aLineBefore is always a normal line, not an overflow line.
  // The following expression will assert otherwise.
  DebugOnly<bool> check = aLineBefore == mLines.begin();

  nsLineList::iterator overBegin(aLineBefore.next());

  // PushTruncatedPlaceholderLine sometimes pushes the first line.  Ugh.
  bool firstLine = overBegin == begin_lines();

  if (overBegin != end_lines()) {
    // Remove floats in the lines from mFloats
    nsFrameList floats;
    CollectFloats(overBegin->mFirstChild, floats, true);

    if (floats.NotEmpty()) {
      // Push the floats onto the front of the overflow out-of-flows list
      nsAutoOOFFrameList oofs(this);
      oofs.mList.InsertFrames(nullptr, nullptr, floats);
    }

    // overflow lines can already exist in some cases, in particular,
    // when shrinkwrapping and we discover that the shrinkwap causes
    // the height of some child block to grow which creates additional
    // overflowing content. In such cases we must prepend the new
    // overflow to the existing overflow.
    FrameLines* overflowLines = RemoveOverflowLines();
    if (!overflowLines) {
      // XXXldb use presshell arena!
      overflowLines = new FrameLines();
    }
    if (overflowLines) {
      nsIFrame* lineBeforeLastFrame;
      if (firstLine) {
        lineBeforeLastFrame = nullptr; // removes all frames
      } else {
        nsIFrame* f = overBegin->mFirstChild;
        lineBeforeLastFrame = f ? f->GetPrevSibling() : mFrames.LastChild();
        NS_ASSERTION(!f || lineBeforeLastFrame == aLineBefore->LastChild(),
                     "unexpected line frames");
      }
      nsFrameList pushedFrames = mFrames.RemoveFramesAfter(lineBeforeLastFrame);
      overflowLines->mFrames.InsertFrames(nullptr, nullptr, pushedFrames);

      overflowLines->mLines.splice(overflowLines->mLines.begin(), mLines,
                                    overBegin, end_lines());
      NS_ASSERTION(!overflowLines->mLines.empty(), "should not be empty");
      // this takes ownership but it won't delete it immediately so we
      // can keep using it.
      SetOverflowLines(overflowLines);
  
      // Mark all the overflow lines dirty so that they get reflowed when
      // they are pulled up by our next-in-flow.

      // XXXldb Can this get called O(N) times making the whole thing O(N^2)?
      for (line_iterator line = overflowLines->mLines.begin(),
             line_end = overflowLines->mLines.end();
           line != line_end;
           ++line)
      {
        line->MarkDirty();
        line->MarkPreviousMarginDirty();
        line->SetBoundsEmpty();
        if (line->HasFloats()) {
          line->FreeFloats(aState.mFloatCacheFreeList);
        }
      }
    }
  }

#ifdef DEBUG
  VerifyOverflowSituation();
#endif
}

// The overflowLines property is stored as a pointer to a line list,
// which must be deleted.  However, the following functions all maintain
// the invariant that the property is never set if the list is empty.

bool
nsBlockFrame::DrainOverflowLines()
{
#ifdef DEBUG
  VerifyOverflowSituation();
#endif

  // Steal the prev-in-flow's overflow lines and prepend them.
  bool didFindOverflow = false;
  nsBlockFrame* prevBlock = static_cast<nsBlockFrame*>(GetPrevInFlow());
  if (prevBlock) {
    prevBlock->ClearLineCursor();
    FrameLines* overflowLines = prevBlock->RemoveOverflowLines();
    if (overflowLines) {
      // Make all the frames on the overflow line list mine.
      ReparentFrames(overflowLines->mFrames, prevBlock, this);

      // Make the overflow out-of-flow frames mine too.
      nsAutoOOFFrameList oofs(prevBlock);
      if (oofs.mList.NotEmpty()) {
        ReparentFrames(oofs.mList, prevBlock, this);
        mFloats.InsertFrames(nullptr, nullptr, oofs.mList);
      }

      if (!mLines.empty()) {
        // Remember to recompute the margins on the first line. This will
        // also recompute the correct deltaBCoord if necessary.
        mLines.front()->MarkPreviousMarginDirty();
      }
      // The overflow lines have already been marked dirty and their previous
      // margins marked dirty also.
      
      // Prepend the overflow frames/lines to our principal list.
      mFrames.InsertFrames(nullptr, nullptr, overflowLines->mFrames);
      mLines.splice(mLines.begin(), overflowLines->mLines);
      NS_ASSERTION(overflowLines->mLines.empty(), "splice should empty list");
      delete overflowLines;
      didFindOverflow = true;
    }
  }

  // Now append our own overflow lines.
  return DrainSelfOverflowList() || didFindOverflow;
}

bool
nsBlockFrame::DrainSelfOverflowList()
{
  nsAutoPtr<FrameLines> ourOverflowLines(RemoveOverflowLines());
  if (!ourOverflowLines) {
    return false;
  }

  // No need to reparent frames in our own overflow lines/oofs, because they're
  // already ours. But we should put overflow floats back in mFloats.
  nsAutoOOFFrameList oofs(this);
  if (oofs.mList.NotEmpty()) {
    // The overflow floats go after our regular floats.
    mFloats.AppendFrames(nullptr, oofs.mList);
  }

  if (!ourOverflowLines->mLines.empty()) {
    mFrames.AppendFrames(nullptr, ourOverflowLines->mFrames);
    mLines.splice(mLines.end(), ourOverflowLines->mLines);
  }
  return true;
}

/**
 * Pushed floats are floats whose placeholders are in a previous
 * continuation.  They might themselves be next-continuations of a float
 * that partially fit in an earlier continuation, or they might be the
 * first continuation of a float that couldn't be placed at all.
 *
 * Pushed floats live permanently at the beginning of a block's float
 * list, where they must live *before* any floats whose placeholders are
 * in that block.
 *
 * Temporarily, during reflow, they also live on the pushed floats list,
 * which only holds them between (a) when one continuation pushes them to
 * its pushed floats list because they don't fit and (b) when the next
 * continuation pulls them onto the beginning of its float list.
 *
 * DrainPushedFloats sets up pushed floats the way we need them at the
 * start of reflow; they are then reflowed by ReflowPushedFloats (which
 * might push some of them on).  Floats with placeholders in this block
 * are reflowed by (nsBlockReflowState/nsLineLayout)::AddFloat, which
 * also maintains these invariants.
 */
void
nsBlockFrame::DrainPushedFloats(nsBlockReflowState& aState)
{
#ifdef DEBUG
  // Between when we drain pushed floats and when we complete reflow,
  // we're allowed to have multiple continuations of the same float on
  // our floats list, since a first-in-flow might get pushed to a later
  // continuation of its containing block.  But it's not permitted
  // outside that time.
  nsLayoutUtils::AssertNoDuplicateContinuations(this, mFloats);
#endif

  // If we're getting reflowed multiple times without our
  // next-continuation being reflowed, we might need to pull back floats
  // that we just put in the list to be pushed to our next-in-flow.
  // We don't want to pull back any next-in-flows of floats on our own
  // float list, and we only need to pull back first-in-flows whose
  // placeholders were in earlier blocks (since first-in-flows whose
  // placeholders are in this block will get pulled appropriately by
  // AddFloat, and will then be more likely to be in the correct order).
  // FIXME: What if there's a continuation in our pushed floats list
  // whose prev-in-flow is in a previous continuation of this block
  // rather than this block?  Might we need to pull it back so we don't
  // report ourselves complete?
  // FIXME: Maybe we should just pull all of them back?
  nsPresContext* presContext = PresContext();
  nsFrameList* ourPushedFloats = GetPushedFloats();
  if (ourPushedFloats) {
    // When we pull back floats, we want to put them with the pushed
    // floats, which must live at the start of our float list, but we
    // want them at the end of those pushed floats.
    // FIXME: This isn't quite right!  What if they're all pushed floats?
    nsIFrame *insertionPrevSibling = nullptr; /* beginning of list */
    for (nsIFrame* f = mFloats.FirstChild();
         f && (f->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT);
         f = f->GetNextSibling()) {
      insertionPrevSibling = f;
    }

    for (nsIFrame *f = ourPushedFloats->LastChild(), *next; f; f = next) {
      next = f->GetPrevSibling();

      if (f->GetPrevContinuation()) {
        // FIXME
      } else {
        nsPlaceholderFrame *placeholder =
          presContext->FrameManager()->GetPlaceholderFrameFor(f);
        nsIFrame *floatOriginalParent = presContext->PresShell()->
          FrameConstructor()->GetFloatContainingBlock(placeholder);
        if (floatOriginalParent != this) {
          // This is a first continuation that was pushed from one of our
          // previous continuations.  Take it out of the pushed floats
          // list and put it in our floats list, before any of our
          // floats, but after other pushed floats.
          ourPushedFloats->RemoveFrame(f);
          mFloats.InsertFrame(nullptr, insertionPrevSibling, f);
        }
      }
    }

    if (ourPushedFloats->IsEmpty()) {
      RemovePushedFloats()->Delete(presContext->PresShell());
    }
  }

  // After our prev-in-flow has completed reflow, it may have a pushed
  // floats list, containing floats that we need to own.  Take these.
  nsBlockFrame* prevBlock = static_cast<nsBlockFrame*>(GetPrevInFlow());
  if (prevBlock) {
    AutoFrameListPtr list(presContext, prevBlock->RemovePushedFloats());
    if (list && list->NotEmpty()) {
      mFloats.InsertFrames(this, nullptr, *list);
    }
  }
}

nsBlockFrame::FrameLines*
nsBlockFrame::GetOverflowLines() const
{
  if (!HasOverflowLines()) {
    return nullptr;
  }
  FrameLines* prop =
    static_cast<FrameLines*>(Properties().Get(OverflowLinesProperty()));
  NS_ASSERTION(prop && !prop->mLines.empty() &&
               prop->mLines.front()->GetChildCount() == 0 ? prop->mFrames.IsEmpty() :
                 prop->mLines.front()->mFirstChild == prop->mFrames.FirstChild(),
               "value should always be stored and non-empty when state set");
  return prop;
}

nsBlockFrame::FrameLines*
nsBlockFrame::RemoveOverflowLines()
{
  if (!HasOverflowLines()) {
    return nullptr;
  }
  FrameLines* prop =
    static_cast<FrameLines*>(Properties().Remove(OverflowLinesProperty()));
  NS_ASSERTION(prop && !prop->mLines.empty() &&
               prop->mLines.front()->GetChildCount() == 0 ? prop->mFrames.IsEmpty() :
                 prop->mLines.front()->mFirstChild == prop->mFrames.FirstChild(),
               "value should always be stored and non-empty when state set");
  RemoveStateBits(NS_BLOCK_HAS_OVERFLOW_LINES);
  return prop;
}

void
nsBlockFrame::DestroyOverflowLines()
{
  NS_ASSERTION(HasOverflowLines(), "huh?");
  FrameLines* prop =
    static_cast<FrameLines*>(Properties().Remove(OverflowLinesProperty()));
  NS_ASSERTION(prop && prop->mLines.empty(),
               "value should always be stored but empty when destroying");
  RemoveStateBits(NS_BLOCK_HAS_OVERFLOW_LINES);
  delete prop;
}

// This takes ownership of aOverflowLines.
// XXX We should allocate overflowLines from presShell arena!
void
nsBlockFrame::SetOverflowLines(FrameLines* aOverflowLines)
{
  NS_ASSERTION(aOverflowLines, "null lines");
  NS_ASSERTION(!aOverflowLines->mLines.empty(), "empty lines");
  NS_ASSERTION(aOverflowLines->mLines.front()->mFirstChild ==
               aOverflowLines->mFrames.FirstChild(),
               "invalid overflow lines / frames");
  NS_ASSERTION(!(GetStateBits() & NS_BLOCK_HAS_OVERFLOW_LINES),
               "Overwriting existing overflow lines");

  FrameProperties props = Properties();
  // Verify that we won't overwrite an existing overflow list
  NS_ASSERTION(!props.Get(OverflowLinesProperty()), "existing overflow list");
  props.Set(OverflowLinesProperty(), aOverflowLines);
  AddStateBits(NS_BLOCK_HAS_OVERFLOW_LINES);
}

nsFrameList*
nsBlockFrame::GetOverflowOutOfFlows() const
{
  if (!(GetStateBits() & NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS)) {
    return nullptr;
  }
  nsFrameList* result =
    GetPropTableFrames(OverflowOutOfFlowsProperty());
  NS_ASSERTION(result, "value should always be non-empty when state set");
  return result;
}

// This takes ownership of the frames
void
nsBlockFrame::SetOverflowOutOfFlows(const nsFrameList& aList,
                                    nsFrameList* aPropValue)
{
  NS_PRECONDITION(!!(GetStateBits() & NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS) ==
                  !!aPropValue, "state does not match value");

  if (aList.IsEmpty()) {
    if (!(GetStateBits() & NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS)) {
      return;
    }
    nsFrameList* list = RemovePropTableFrames(OverflowOutOfFlowsProperty());
    NS_ASSERTION(aPropValue == list, "prop value mismatch");
    list->Clear();
    list->Delete(PresContext()->PresShell());
    RemoveStateBits(NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS);
  }
  else if (GetStateBits() & NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS) {
    NS_ASSERTION(aPropValue == GetPropTableFrames(OverflowOutOfFlowsProperty()),
                 "prop value mismatch");
    *aPropValue = aList;
  }
  else {
    SetPropTableFrames(new (PresContext()->PresShell()) nsFrameList(aList),
                       OverflowOutOfFlowsProperty());
    AddStateBits(NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS);
  }
}

nsBulletFrame*
nsBlockFrame::GetInsideBullet() const
{
  if (!HasInsideBullet()) {
    return nullptr;
  }
  NS_ASSERTION(!HasOutsideBullet(), "invalid bullet state");
  nsBulletFrame* frame =
    static_cast<nsBulletFrame*>(Properties().Get(InsideBulletProperty()));
  NS_ASSERTION(frame && frame->GetType() == nsGkAtoms::bulletFrame,
               "bogus inside bullet frame");
  return frame;
}

nsBulletFrame*
nsBlockFrame::GetOutsideBullet() const
{
  nsFrameList* list = GetOutsideBulletList();
  return list ? static_cast<nsBulletFrame*>(list->FirstChild())
              : nullptr;
}

nsFrameList*
nsBlockFrame::GetOutsideBulletList() const
{
  if (!HasOutsideBullet()) {
    return nullptr;
  }
  NS_ASSERTION(!HasInsideBullet(), "invalid bullet state");
  nsFrameList* list =
    static_cast<nsFrameList*>(Properties().Get(OutsideBulletProperty()));
  NS_ASSERTION(list && list->GetLength() == 1 &&
               list->FirstChild()->GetType() == nsGkAtoms::bulletFrame,
               "bogus outside bullet list");
  return list;
}

nsFrameList*
nsBlockFrame::GetPushedFloats() const
{
  if (!HasPushedFloats()) {
    return nullptr;
  }
  nsFrameList* result =
    static_cast<nsFrameList*>(Properties().Get(PushedFloatProperty()));
  NS_ASSERTION(result, "value should always be non-empty when state set");
  return result;
}

nsFrameList*
nsBlockFrame::EnsurePushedFloats()
{
  nsFrameList *result = GetPushedFloats();
  if (result)
    return result;

  result = new (PresContext()->PresShell()) nsFrameList;
  Properties().Set(PushedFloatProperty(), result);
  AddStateBits(NS_BLOCK_HAS_PUSHED_FLOATS);

  return result;
}

nsFrameList*
nsBlockFrame::RemovePushedFloats()
{
  if (!HasPushedFloats()) {
    return nullptr;
  }
  nsFrameList *result =
    static_cast<nsFrameList*>(Properties().Remove(PushedFloatProperty()));
  RemoveStateBits(NS_BLOCK_HAS_PUSHED_FLOATS);
  NS_ASSERTION(result, "value should always be non-empty when state set");
  return result;
}

//////////////////////////////////////////////////////////////////////
// Frame list manipulation routines

void
nsBlockFrame::AppendFrames(ChildListID  aListID,
                           nsFrameList& aFrameList)
{
  if (aFrameList.IsEmpty()) {
    return;
  }
  if (aListID != kPrincipalList) {
    if (kFloatList == aListID) {
      mFloats.AppendFrames(nullptr, aFrameList);
      return;
    }
    MOZ_ASSERT(kNoReflowPrincipalList == aListID, "unexpected child list");
  }

  // Find the proper last-child for where the append should go
  nsIFrame* lastKid = mFrames.LastChild();
  NS_ASSERTION((mLines.empty() ? nullptr : mLines.back()->LastChild()) ==
               lastKid, "out-of-sync mLines / mFrames");

#ifdef NOISY_REFLOW_REASON
  ListTag(stdout);
  printf(": append ");
  nsFrame::ListTag(stdout, aFrameList);
  if (lastKid) {
    printf(" after ");
    nsFrame::ListTag(stdout, lastKid);
  }
  printf("\n");
#endif

  AddFrames(aFrameList, lastKid);
  if (aListID != kNoReflowPrincipalList) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN); // XXX sufficient?
  }
}

void
nsBlockFrame::InsertFrames(ChildListID aListID,
                           nsIFrame* aPrevFrame,
                           nsFrameList& aFrameList)
{
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  if (aListID != kPrincipalList) {
    if (kFloatList == aListID) {
      mFloats.InsertFrames(this, aPrevFrame, aFrameList);
      return;
    }
    MOZ_ASSERT(kNoReflowPrincipalList == aListID, "unexpected child list");
  }

#ifdef NOISY_REFLOW_REASON
  ListTag(stdout);
  printf(": insert ");
  nsFrame::ListTag(stdout, aFrameList);
  if (aPrevFrame) {
    printf(" after ");
    nsFrame::ListTag(stdout, aPrevFrame);
  }
  printf("\n");
#endif

  AddFrames(aFrameList, aPrevFrame);
  if (aListID != kNoReflowPrincipalList) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN); // XXX sufficient?
  }
}

void
nsBlockFrame::RemoveFrame(ChildListID aListID,
                          nsIFrame* aOldFrame)
{
#ifdef NOISY_REFLOW_REASON
  ListTag(stdout);
  printf(": remove ");
  nsFrame::ListTag(stdout, aOldFrame);
  printf("\n");
#endif

  if (aListID == kPrincipalList) {
    bool hasFloats = BlockHasAnyFloats(aOldFrame);
    DoRemoveFrame(aOldFrame, REMOVE_FIXED_CONTINUATIONS);
    if (hasFloats) {
      MarkSameFloatManagerLinesDirty(this);
    }
  }
  else if (kFloatList == aListID) {
    // Make sure to mark affected lines dirty for the float frame
    // we are removing; this way is a bit messy, but so is the rest of the code.
    // See bug 390762.
    NS_ASSERTION(!aOldFrame->GetPrevContinuation(),
                 "RemoveFrame should not be called on pushed floats.");
    for (nsIFrame* f = aOldFrame;
         f && !(f->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER);
         f = f->GetNextContinuation()) {
      MarkSameFloatManagerLinesDirty(static_cast<nsBlockFrame*>(f->GetParent()));
    }
    DoRemoveOutOfFlowFrame(aOldFrame);
  }
  else if (kNoReflowPrincipalList == aListID) {
    // Skip the call to |FrameNeedsReflow| below by returning now.
    DoRemoveFrame(aOldFrame, REMOVE_FIXED_CONTINUATIONS);
    return;
  }
  else {
    MOZ_CRASH("unexpected child list");
  }

  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN); // XXX sufficient?
}

static bool
ShouldPutNextSiblingOnNewLine(nsIFrame* aLastFrame)
{
  nsIAtom* type = aLastFrame->GetType();
  if (type == nsGkAtoms::brFrame) {
    return true;
  }
  // XXX the TEXT_OFFSETS_NEED_FIXING check is a wallpaper for bug 822910.
  if (type == nsGkAtoms::textFrame &&
      !(aLastFrame->GetStateBits() & TEXT_OFFSETS_NEED_FIXING)) {
    return aLastFrame->HasSignificantTerminalNewline();
  }
  return false;
}

void
nsBlockFrame::AddFrames(nsFrameList& aFrameList, nsIFrame* aPrevSibling)
{
  // Clear our line cursor, since our lines may change.
  ClearLineCursor();

  if (aFrameList.IsEmpty()) {
    return;
  }

  // If we're inserting at the beginning of our list and we have an
  // inside bullet, insert after that bullet.
  if (!aPrevSibling && HasInsideBullet()) {
    aPrevSibling = GetInsideBullet();
  }
  
  // Attempt to find the line that contains the previous sibling
  FrameLines* overflowLines;
  nsLineList* lineList = &mLines;
  nsLineList::iterator prevSibLine = lineList->end();
  int32_t prevSiblingIndex = -1;
  if (aPrevSibling) {
    // XXX_perf This is technically O(N^2) in some cases, but by using
    // RFind instead of Find, we make it O(N) in the most common case,
    // which is appending content.

    // Find the line that contains the previous sibling
    if (!nsLineBox::RFindLineContaining(aPrevSibling, lineList->begin(),
                                        prevSibLine, mFrames.LastChild(),
                                        &prevSiblingIndex)) {
      // Not in mLines - try overflow lines.
      overflowLines = GetOverflowLines();
      lineList = overflowLines ? &overflowLines->mLines : nullptr;
      if (overflowLines) {
        prevSibLine = overflowLines->mLines.end();
        prevSiblingIndex = -1;
        if (!nsLineBox::RFindLineContaining(aPrevSibling, lineList->begin(),
                                            prevSibLine,
                                            overflowLines->mFrames.LastChild(),
                                            &prevSiblingIndex)) {
          lineList = nullptr;
        }
      }
      if (!lineList) {
        // Note: defensive code! RFindLineContaining must not return
        // false in this case, so if it does...
        NS_NOTREACHED("prev sibling not in line list");
        lineList = &mLines;
        aPrevSibling = nullptr;
        prevSibLine = lineList->end();
      }
    }
  }

  // Find the frame following aPrevSibling so that we can join up the
  // two lists of frames.
  if (aPrevSibling) {
    // Split line containing aPrevSibling in two if the insertion
    // point is somewhere in the middle of the line.
    int32_t rem = prevSibLine->GetChildCount() - prevSiblingIndex - 1;
    if (rem) {
      // Split the line in two where the frame(s) are being inserted.
      nsLineBox* line = NewLineBox(prevSibLine, aPrevSibling->GetNextSibling(), rem);
      lineList->after_insert(prevSibLine, line);
      // Mark prevSibLine dirty and as needing textrun invalidation, since
      // we may be breaking up text in the line. Its previous line may also
      // need to be invalidated because it may be able to pull some text up.
      MarkLineDirty(prevSibLine, lineList);
      // The new line will also need its textruns recomputed because of the
      // frame changes.
      line->MarkDirty();
      line->SetInvalidateTextRuns(true);
    }
  }
  else if (! lineList->empty()) {
    lineList->front()->MarkDirty();
    lineList->front()->SetInvalidateTextRuns(true);
  }
  nsFrameList& frames = lineList == &mLines ? mFrames : overflowLines->mFrames;
  const nsFrameList::Slice& newFrames =
    frames.InsertFrames(nullptr, aPrevSibling, aFrameList);

  // Walk through the new frames being added and update the line data
  // structures to fit.
  for (nsFrameList::Enumerator e(newFrames); !e.AtEnd(); e.Next()) {
    nsIFrame* newFrame = e.get();
    NS_ASSERTION(!aPrevSibling || aPrevSibling->GetNextSibling() == newFrame,
                 "Unexpected aPrevSibling");
    NS_ASSERTION(newFrame->GetType() != nsGkAtoms::placeholderFrame ||
                 (!newFrame->IsAbsolutelyPositioned() &&
                  !newFrame->IsFloating()),
                 "Placeholders should not float or be positioned");

    bool isBlock = newFrame->IsBlockOutside();

    // If the frame is a block frame, or if there is no previous line or if the
    // previous line is a block line we need to make a new line.  We also make
    // a new line, as an optimization, in the two cases we know we'll need it:
    // if the previous line ended with a <br>, or if it has significant whitespace
    // and ended in a newline.
    if (isBlock || prevSibLine == lineList->end() || prevSibLine->IsBlock() ||
        (aPrevSibling && ShouldPutNextSiblingOnNewLine(aPrevSibling))) {
      // Create a new line for the frame and add its line to the line
      // list.
      nsLineBox* line = NewLineBox(newFrame, isBlock);
      if (prevSibLine != lineList->end()) {
        // Append new line after prevSibLine
        lineList->after_insert(prevSibLine, line);
        ++prevSibLine;
      }
      else {
        // New line is going before the other lines
        lineList->push_front(line);
        prevSibLine = lineList->begin();
      }
    }
    else {
      prevSibLine->NoteFrameAdded(newFrame);
      // We're adding inline content to prevSibLine, so we need to mark it
      // dirty, ensure its textruns are recomputed, and possibly do the same
      // to its previous line since that line may be able to pull content up.
      MarkLineDirty(prevSibLine, lineList);
    }

    aPrevSibling = newFrame;
  }

#ifdef DEBUG
  MOZ_ASSERT(aFrameList.IsEmpty());
  VerifyLines(true);
#endif
}

void
nsBlockFrame::RemoveFloatFromFloatCache(nsIFrame* aFloat)
{
  // Find which line contains the float, so we can update
  // the float cache.
  line_iterator line = begin_lines(), line_end = end_lines();
  for ( ; line != line_end; ++line) {
    if (line->IsInline() && line->RemoveFloat(aFloat)) {
      break;
    }
  }
}

void
nsBlockFrame::RemoveFloat(nsIFrame* aFloat)
{
#ifdef DEBUG
  // Floats live in mFloats, or in the PushedFloat or OverflowOutOfFlows
  // frame list properties.
  if (!mFloats.ContainsFrame(aFloat)) {
    MOZ_ASSERT((GetOverflowOutOfFlows() &&
                GetOverflowOutOfFlows()->ContainsFrame(aFloat)) ||
               (GetPushedFloats() &&
                GetPushedFloats()->ContainsFrame(aFloat)),
               "aFloat is not our child or on an unexpected frame list");
  }
#endif

  if (mFloats.StartRemoveFrame(aFloat)) {
    return;
  }

  nsFrameList* list = GetPushedFloats();
  if (list && list->ContinueRemoveFrame(aFloat)) {
#if 0
    // XXXmats not yet - need to investigate nsBlockReflowState::mPushedFloats
    // first so we don't leave it pointing to a deleted list.
    if (list->IsEmpty()) {
      delete RemovePushedFloats();
    }
#endif
    return;
  }

  {
    nsAutoOOFFrameList oofs(this);
    if (oofs.mList.ContinueRemoveFrame(aFloat)) {
      return;
    }
  }
}

void
nsBlockFrame::DoRemoveOutOfFlowFrame(nsIFrame* aFrame)
{
  // The containing block is always the parent of aFrame.
  nsBlockFrame* block = (nsBlockFrame*)aFrame->GetParent();

  // Remove aFrame from the appropriate list.
  if (aFrame->IsAbsolutelyPositioned()) {
    // This also deletes the next-in-flows
    block->GetAbsoluteContainingBlock()->RemoveFrame(block,
                                                     kAbsoluteList,
                                                     aFrame);
  }
  else {
    // First remove aFrame's next-in-flows.
    nsIFrame* nif = aFrame->GetNextInFlow();
    if (nif) {
      nif->GetParent()->DeleteNextInFlowChild(nif, false);
    }
    // Now remove aFrame from its child list and Destroy it.
    block->RemoveFloatFromFloatCache(aFrame);
    block->RemoveFloat(aFrame);
    aFrame->Destroy();
  }
}

/**
 * This helps us iterate over the list of all normal + overflow lines
 */
void
nsBlockFrame::TryAllLines(nsLineList::iterator* aIterator,
                          nsLineList::iterator* aStartIterator,
                          nsLineList::iterator* aEndIterator,
                          bool* aInOverflowLines,
                          FrameLines** aOverflowLines)
{
  if (*aIterator == *aEndIterator) {
    if (!*aInOverflowLines) {
      // Try the overflow lines
      *aInOverflowLines = true;
      FrameLines* lines = GetOverflowLines();
      if (lines) {
        *aStartIterator = lines->mLines.begin();
        *aIterator = *aStartIterator;
        *aEndIterator = lines->mLines.end();
        *aOverflowLines = lines;
      }
    }
  }
}

nsBlockInFlowLineIterator::nsBlockInFlowLineIterator(nsBlockFrame* aFrame,
    line_iterator aLine)
  : mFrame(aFrame), mLine(aLine), mLineList(&aFrame->mLines)
{
  // This will assert if aLine isn't in mLines of aFrame:
  DebugOnly<bool> check = aLine == mFrame->begin_lines();
}

nsBlockInFlowLineIterator::nsBlockInFlowLineIterator(nsBlockFrame* aFrame,
    line_iterator aLine, bool aInOverflow)
  : mFrame(aFrame), mLine(aLine),
    mLineList(aInOverflow ? &aFrame->GetOverflowLines()->mLines
                          : &aFrame->mLines)
{
}

nsBlockInFlowLineIterator::nsBlockInFlowLineIterator(nsBlockFrame* aFrame,
    bool* aFoundValidLine)
  : mFrame(aFrame), mLineList(&aFrame->mLines)
{
  mLine = aFrame->begin_lines();
  *aFoundValidLine = FindValidLine();
}

static nsIFrame*
FindChildContaining(nsBlockFrame* aFrame, nsIFrame* aFindFrame)
{
  NS_ASSERTION(aFrame, "must have frame");
  nsIFrame* child;
  while (true) {
    nsIFrame* block = aFrame;
    do {
      child = nsLayoutUtils::FindChildContainingDescendant(block, aFindFrame);
      if (child)
        break;
      block = block->GetNextContinuation();
    } while (block);
    if (!child)
      return nullptr;
    if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW))
      break;
    aFindFrame = aFrame->PresContext()->FrameManager()->GetPlaceholderFrameFor(child);
  }

  return child;
}

nsBlockInFlowLineIterator::nsBlockInFlowLineIterator(nsBlockFrame* aFrame,
    nsIFrame* aFindFrame, bool* aFoundValidLine)
  : mFrame(aFrame), mLineList(&aFrame->mLines)
{
  *aFoundValidLine = false;

  nsIFrame* child = FindChildContaining(aFrame, aFindFrame);
  if (!child)
    return;

  // Try to use the cursor if it exists, otherwise fall back to the first line
  nsLineBox* cursor = aFrame->GetLineCursor();
  if (!cursor) {
    line_iterator iter = aFrame->begin_lines();
    if (iter != aFrame->end_lines()) {
      cursor = iter;
    }
  }

  if (cursor) {
    // Perform a simultaneous forward and reverse search starting from the
    // line cursor.
    nsBlockFrame::line_iterator line = aFrame->line(cursor);
    nsBlockFrame::reverse_line_iterator rline = aFrame->rline(cursor);
    nsBlockFrame::line_iterator line_end = aFrame->end_lines();
    nsBlockFrame::reverse_line_iterator rline_end = aFrame->rend_lines();
    // rline is positioned on the line containing 'cursor', so it's not
    // rline_end. So we can safely increment it (i.e. move it to one line
    // earlier) to start searching there.
    ++rline;
    while (line != line_end || rline != rline_end) {
      if (line != line_end) {
        if (line->Contains(child)) {
          *aFoundValidLine = true;
          mLine = line;
          return;
        }
        ++line;
      }
      if (rline != rline_end) {
        if (rline->Contains(child)) {
          *aFoundValidLine = true;
          mLine = rline;
          return;
        }
        ++rline;
      }
    }
    // Didn't find the line
  }

  // If we reach here, it means that we have not been able to find the
  // desired frame in our in-flow lines.  So we should start looking at
  // our overflow lines. In order to do that, we set mLine to the end
  // iterator so that FindValidLine starts to look at overflow lines,
  // if any.

  mLine = aFrame->end_lines();

  if (!FindValidLine())
    return;

  do {
    if (mLine->Contains(child)) {
      *aFoundValidLine = true;
      return;
    }
  } while (Next());
}

nsBlockFrame::line_iterator
nsBlockInFlowLineIterator::End()
{
  return mLineList->end();
}

bool
nsBlockInFlowLineIterator::IsLastLineInList()
{
  line_iterator end = End();
  return mLine != end && mLine.next() == end;
}

bool
nsBlockInFlowLineIterator::Next()
{
  ++mLine;
  return FindValidLine();
}

bool
nsBlockInFlowLineIterator::Prev()
{
  line_iterator begin = mLineList->begin();
  if (mLine != begin) {
    --mLine;
    return true;
  }
  bool currentlyInOverflowLines = GetInOverflow();
  while (true) {
    if (currentlyInOverflowLines) {
      mLineList = &mFrame->mLines;
      mLine = mLineList->end();
      if (mLine != mLineList->begin()) {
        --mLine;
        return true;
      }
    } else {
      mFrame = static_cast<nsBlockFrame*>(mFrame->GetPrevInFlow());
      if (!mFrame)
        return false;
      nsBlockFrame::FrameLines* overflowLines = mFrame->GetOverflowLines();
      if (overflowLines) {
        mLineList = &overflowLines->mLines;
        mLine = mLineList->end();
        NS_ASSERTION(mLine != mLineList->begin(), "empty overflow line list?");
        --mLine;
        return true;
      }
    }
    currentlyInOverflowLines = !currentlyInOverflowLines;
  }
}

bool
nsBlockInFlowLineIterator::FindValidLine()
{
  line_iterator end = mLineList->end();
  if (mLine != end)
    return true;
  bool currentlyInOverflowLines = GetInOverflow();
  while (true) {
    if (currentlyInOverflowLines) {
      mFrame = static_cast<nsBlockFrame*>(mFrame->GetNextInFlow());
      if (!mFrame)
        return false;
      mLineList = &mFrame->mLines;
      mLine = mLineList->begin();
      if (mLine != mLineList->end())
        return true;
    } else {
      nsBlockFrame::FrameLines* overflowLines = mFrame->GetOverflowLines();
      if (overflowLines) {
        mLineList = &overflowLines->mLines;
        mLine = mLineList->begin();
        NS_ASSERTION(mLine != mLineList->end(), "empty overflow line list?");
        return true;
      }
    }
    currentlyInOverflowLines = !currentlyInOverflowLines;
  }
}

static void RemoveBlockChild(nsIFrame* aFrame,
                             bool      aRemoveOnlyFluidContinuations)
{
  if (!aFrame) {
    return;
  }
  nsBlockFrame* nextBlock = nsLayoutUtils::GetAsBlock(aFrame->GetParent());
  NS_ASSERTION(nextBlock,
               "Our child's continuation's parent is not a block?");
  nextBlock->DoRemoveFrame(aFrame,
      (aRemoveOnlyFluidContinuations ? 0 : nsBlockFrame::REMOVE_FIXED_CONTINUATIONS));
}

// This function removes aDeletedFrame and all its continuations.  It
// is optimized for deleting a whole series of frames. The easy
// implementation would invoke itself recursively on
// aDeletedFrame->GetNextContinuation, then locate the line containing
// aDeletedFrame and remove aDeletedFrame from that line. But here we
// start by locating aDeletedFrame and then scanning from that point
// on looking for continuations.
void
nsBlockFrame::DoRemoveFrame(nsIFrame* aDeletedFrame, uint32_t aFlags)
{
  // Clear our line cursor, since our lines may change.
  ClearLineCursor();

  if (aDeletedFrame->GetStateBits() &
      (NS_FRAME_OUT_OF_FLOW | NS_FRAME_IS_OVERFLOW_CONTAINER)) {
    if (!aDeletedFrame->GetPrevInFlow()) {
      NS_ASSERTION(aDeletedFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW,
                   "Expected out-of-flow frame");
      DoRemoveOutOfFlowFrame(aDeletedFrame);
    }
    else {
      nsContainerFrame::DeleteNextInFlowChild(aDeletedFrame,
                                              (aFlags & FRAMES_ARE_EMPTY) != 0);
    }
    return;
  }

  // Find the line that contains deletedFrame
  nsLineList::iterator line_start = mLines.begin(),
                       line_end = mLines.end();
  nsLineList::iterator line = line_start;
  FrameLines* overflowLines = nullptr;
  bool searchingOverflowList = false;
  // Make sure we look in the overflow lines even if the normal line
  // list is empty
  TryAllLines(&line, &line_start, &line_end, &searchingOverflowList,
              &overflowLines);
  while (line != line_end) {
    if (line->Contains(aDeletedFrame)) {
      break;
    }
    ++line;
    TryAllLines(&line, &line_start, &line_end, &searchingOverflowList,
                &overflowLines);
  }

  if (line == line_end) {
    NS_ERROR("can't find deleted frame in lines");
    return;
  }
  
  if (!(aFlags & FRAMES_ARE_EMPTY)) {
    if (line != line_start) {
      line.prev()->MarkDirty();
      line.prev()->SetInvalidateTextRuns(true);
    }
    else if (searchingOverflowList && !mLines.empty()) {
      mLines.back()->MarkDirty();
      mLines.back()->SetInvalidateTextRuns(true);
    }
  }

  while (line != line_end && aDeletedFrame) {
    NS_ASSERTION(this == aDeletedFrame->GetParent(), "messed up delete code");
    NS_ASSERTION(line->Contains(aDeletedFrame), "frame not in line");

    if (!(aFlags & FRAMES_ARE_EMPTY)) {
      line->MarkDirty();
      line->SetInvalidateTextRuns(true);
    }

    // If the frame being deleted is the last one on the line then
    // optimize away the line->Contains(next-in-flow) call below.
    bool isLastFrameOnLine = 1 == line->GetChildCount();
    if (!isLastFrameOnLine) {
      line_iterator next = line.next();
      nsIFrame* lastFrame = next != line_end ?
        next->mFirstChild->GetPrevSibling() :
        (searchingOverflowList ? overflowLines->mFrames.LastChild() : 
                                 mFrames.LastChild());
      NS_ASSERTION(next == line_end || lastFrame == line->LastChild(),
                   "unexpected line frames");
      isLastFrameOnLine = lastFrame == aDeletedFrame;
    }

    // Remove aDeletedFrame from the line
    if (line->mFirstChild == aDeletedFrame) {
      // We should be setting this to null if aDeletedFrame
      // is the only frame on the line. HOWEVER in that case
      // we will be removing the line anyway, see below.
      line->mFirstChild = aDeletedFrame->GetNextSibling();
    }

    // Hmm, this won't do anything if we're removing a frame in the first
    // overflow line... Hopefully doesn't matter
    --line;
    if (line != line_end && !line->IsBlock()) {
      // Since we just removed a frame that follows some inline
      // frames, we need to reflow the previous line.
      line->MarkDirty();
    }
    ++line;

    // Take aDeletedFrame out of the sibling list. Note that
    // prevSibling will only be nullptr when we are deleting the very
    // first frame in the main or overflow list.
    if (searchingOverflowList) {
      overflowLines->mFrames.RemoveFrame(aDeletedFrame);
    } else {
      mFrames.RemoveFrame(aDeletedFrame);
    }

    // Update the child count of the line to be accurate
    line->NoteFrameRemoved(aDeletedFrame);

    // Destroy frame; capture its next continuation first in case we need
    // to destroy that too.
    nsIFrame* deletedNextContinuation = (aFlags & REMOVE_FIXED_CONTINUATIONS) ?
        aDeletedFrame->GetNextContinuation() : aDeletedFrame->GetNextInFlow();
#ifdef NOISY_REMOVE_FRAME
    printf("DoRemoveFrame: %s line=%p frame=",
           searchingOverflowList?"overflow":"normal", line.get());
    nsFrame::ListTag(stdout, aDeletedFrame);
    printf(" prevSibling=%p deletedNextContinuation=%p\n",
           aDeletedFrame->GetPrevSibling(), deletedNextContinuation);
#endif

    // If next-in-flow is an overflow container, must remove it first.
    if (deletedNextContinuation &&
        deletedNextContinuation->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
      deletedNextContinuation->GetParent()->
        DeleteNextInFlowChild(deletedNextContinuation, false);
      deletedNextContinuation = nullptr;
    }

    aDeletedFrame->Destroy();
    aDeletedFrame = deletedNextContinuation;

    bool haveAdvancedToNextLine = false;
    // If line is empty, remove it now.
    if (0 == line->GetChildCount()) {
#ifdef NOISY_REMOVE_FRAME
        printf("DoRemoveFrame: %s line=%p became empty so it will be removed\n",
               searchingOverflowList?"overflow":"normal", line.get());
#endif
      nsLineBox *cur = line;
      if (!searchingOverflowList) {
        line = mLines.erase(line);
        // Invalidate the space taken up by the line.
        // XXX We need to do this if we're removing a frame as a result of
        // a call to RemoveFrame(), but we may not need to do this in all
        // cases...
#ifdef NOISY_BLOCK_INVALIDATE
        nsRect visOverflow(cur->GetVisualOverflowArea());
        printf("%p invalidate 10 (%d, %d, %d, %d)\n",
               this, visOverflow.x, visOverflow.y,
               visOverflow.width, visOverflow.height);
#endif
      } else {
        line = overflowLines->mLines.erase(line);
        if (overflowLines->mLines.empty()) {
          DestroyOverflowLines();
          overflowLines = nullptr;
          // We just invalidated our iterators.  Since we were in
          // the overflow lines list, which is now empty, set them
          // so we're at the end of the regular line list.
          line_start = mLines.begin();
          line_end = mLines.end();
          line = line_end;
        }
      }
      FreeLineBox(cur);

      // If we're removing a line, ReflowDirtyLines isn't going to
      // know that it needs to slide lines unless something is marked
      // dirty.  So mark the previous margin of the next line dirty if
      // there is one.
      if (line != line_end) {
        line->MarkPreviousMarginDirty();
      }
      haveAdvancedToNextLine = true;
    } else {
      // Make the line that just lost a frame dirty, and advance to
      // the next line.
      if (!deletedNextContinuation || isLastFrameOnLine ||
          !line->Contains(deletedNextContinuation)) {
        line->MarkDirty();
        ++line;
        haveAdvancedToNextLine = true;
      }
    }

    if (deletedNextContinuation) {
      // See if we should keep looking in the current flow's line list.
      if (deletedNextContinuation->GetParent() != this) {
        // The deceased frames continuation is not a child of the
        // current block. So break out of the loop so that we advance
        // to the next parent.
        //
        // If we have a continuation in a different block then all bets are
        // off regarding whether we are deleting frames without actual content,
        // so don't propagate FRAMES_ARE_EMPTY any further.
        aFlags &= ~FRAMES_ARE_EMPTY;
        break;
      }

      // If we advanced to the next line then check if we should switch to the
      // overflow line list.
      if (haveAdvancedToNextLine) {
        if (line != line_end && !searchingOverflowList &&
            !line->Contains(deletedNextContinuation)) {
          // We have advanced to the next *normal* line but the next-in-flow
          // is not there - force a switch to the overflow line list.
          line = line_end;
        }

        TryAllLines(&line, &line_start, &line_end, &searchingOverflowList,
                    &overflowLines);
#ifdef NOISY_REMOVE_FRAME
        printf("DoRemoveFrame: now on %s line=%p\n",
               searchingOverflowList?"overflow":"normal", line.get());
#endif
      }
    }
  }

  if (!(aFlags & FRAMES_ARE_EMPTY) && line.next() != line_end) {
    line.next()->MarkDirty();
    line.next()->SetInvalidateTextRuns(true);
  }

#ifdef DEBUG
  VerifyLines(true);
  VerifyOverflowSituation();
#endif

  // Advance to next flow block if the frame has more continuations
  RemoveBlockChild(aDeletedFrame, !(aFlags & REMOVE_FIXED_CONTINUATIONS));
}

static bool
FindBlockLineFor(nsIFrame*             aChild,
                 nsLineList::iterator  aBegin,
                 nsLineList::iterator  aEnd,
                 nsLineList::iterator* aResult)
{
  MOZ_ASSERT(aChild->IsBlockOutside());
  for (nsLineList::iterator line = aBegin; line != aEnd; ++line) {
    MOZ_ASSERT(line->GetChildCount() > 0);
    if (line->IsBlock() && line->mFirstChild == aChild) {
      MOZ_ASSERT(line->GetChildCount() == 1);
      *aResult = line;
      return true;
    }
  }
  return false;
}

static bool
FindInlineLineFor(nsIFrame*             aChild,
                  const nsFrameList&    aFrameList,
                  nsLineList::iterator  aBegin,
                  nsLineList::iterator  aEnd,
                  nsLineList::iterator* aResult)
{
  MOZ_ASSERT(!aChild->IsBlockOutside());
  for (nsLineList::iterator line = aBegin; line != aEnd; ++line) {
    MOZ_ASSERT(line->GetChildCount() > 0);
    if (!line->IsBlock()) {
      // Optimize by comparing the line's last child first.
      nsLineList::iterator next = line.next();
      if (aChild == (next == aEnd ? aFrameList.LastChild()
                                  : next->mFirstChild->GetPrevSibling()) ||
          line->Contains(aChild)) {
        *aResult = line;
        return true;
      }
    }
  }
  return false;
}

static bool
FindLineFor(nsIFrame*             aChild,
            const nsFrameList&    aFrameList,
            nsLineList::iterator  aBegin,
            nsLineList::iterator  aEnd,
            nsLineList::iterator* aResult)
{
  return aChild->IsBlockOutside() ?
    FindBlockLineFor(aChild, aBegin, aEnd, aResult) :
    FindInlineLineFor(aChild, aFrameList, aBegin, aEnd, aResult);
}

nsresult
nsBlockFrame::StealFrame(nsIFrame* aChild,
                         bool      aForceNormal)
{
  MOZ_ASSERT(aChild->GetParent() == this);

  if ((aChild->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
      aChild->IsFloating()) {
    RemoveFloat(aChild);
    return NS_OK;
  }

  if ((aChild->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)
      && !aForceNormal) {
    return nsContainerFrame::StealFrame(aChild);
  }

  MOZ_ASSERT(!(aChild->GetStateBits() & NS_FRAME_OUT_OF_FLOW));

  nsLineList::iterator line;
  if (FindLineFor(aChild, mFrames, mLines.begin(), mLines.end(), &line)) {
    RemoveFrameFromLine(aChild, line, mFrames, mLines);
  } else {
    FrameLines* overflowLines = GetOverflowLines();
    DebugOnly<bool> found;
    found = FindLineFor(aChild, overflowLines->mFrames,
                        overflowLines->mLines.begin(),
                        overflowLines->mLines.end(), &line);
    MOZ_ASSERT(found);
    RemoveFrameFromLine(aChild, line, overflowLines->mFrames,
                        overflowLines->mLines);
    if (overflowLines->mLines.empty()) {
      DestroyOverflowLines();
    }
  }

  return NS_OK;
}

void
nsBlockFrame::RemoveFrameFromLine(nsIFrame* aChild, nsLineList::iterator aLine,
                                  nsFrameList& aFrameList, nsLineList& aLineList)
{
  aFrameList.RemoveFrame(aChild);
  if (aChild == aLine->mFirstChild) {
    aLine->mFirstChild = aChild->GetNextSibling();
  }
  aLine->NoteFrameRemoved(aChild);
  if (aLine->GetChildCount() > 0) {
    aLine->MarkDirty();
  } else {
    // The line became empty - destroy it.
    nsLineBox* lineBox = aLine;
    aLine = aLineList.erase(aLine);
    if (aLine != aLineList.end()) {
      aLine->MarkPreviousMarginDirty();
    }
    FreeLineBox(lineBox);
  }
}

void
nsBlockFrame::DeleteNextInFlowChild(nsIFrame* aNextInFlow,
                                    bool      aDeletingEmptyFrames)
{
  NS_PRECONDITION(aNextInFlow->GetPrevInFlow(), "bad next-in-flow");

  if (aNextInFlow->GetStateBits() &
      (NS_FRAME_OUT_OF_FLOW | NS_FRAME_IS_OVERFLOW_CONTAINER)) {
    nsContainerFrame::DeleteNextInFlowChild(aNextInFlow, aDeletingEmptyFrames);
  }
  else {
#ifdef DEBUG
    if (aDeletingEmptyFrames) {
      nsLayoutUtils::AssertTreeOnlyEmptyNextInFlows(aNextInFlow);
    }
#endif
    DoRemoveFrame(aNextInFlow,
        aDeletingEmptyFrames ? FRAMES_ARE_EMPTY : 0);
  }
}

const nsStyleText*
nsBlockFrame::StyleTextForLineLayout()
{
  // Return the pointer to an unmodified style text
  return StyleText();
}

////////////////////////////////////////////////////////////////////////
// Float support

nsRect
nsBlockFrame::AdjustFloatAvailableSpace(nsBlockReflowState& aState,
                                        const nsRect& aFloatAvailableSpace,
                                        nsIFrame* aFloatFrame)
{
  // Compute the available width. By default, assume the width of the
  // containing block.
  nscoord availISize;
  const nsStyleDisplay* floatDisplay = aFloatFrame->StyleDisplay();

  if (NS_STYLE_DISPLAY_TABLE != floatDisplay->mDisplay ||
      eCompatibility_NavQuirks != aState.mPresContext->CompatibilityMode() ) {
    availISize = aState.ContentISize();
  }
  else {
    // This quirk matches the one in nsBlockReflowState::FlowAndPlaceFloat
    // give tables only the available space
    // if they can shrink we may not be constrained to place
    // them in the next line
    availISize = aFloatAvailableSpace.width;
  }

  nscoord availBSize = NS_UNCONSTRAINEDSIZE == aState.ContentBSize()
                       ? NS_UNCONSTRAINEDSIZE
                       : std::max(0, aState.ContentBEnd() - aState.mBCoord);

#ifdef DISABLE_FLOAT_BREAKING_IN_COLUMNS
  if (availBSize != NS_UNCONSTRAINEDSIZE &&
      nsLayoutUtils::GetClosestFrameOfType(this, nsGkAtoms::columnSetFrame)) {
    // Tell the float it has unrestricted block-size, so it won't break.
    // If the float doesn't actually fit in the column it will fail to be
    // placed, and either move to the block-start of the next column or just
    // overflow.
    availBSize = NS_UNCONSTRAINEDSIZE;
  }
#endif

  WritingMode wm = aState.mReflowState.GetWritingMode();
  LogicalRect availSpace(wm, aState.ContentIStart(), aState.ContentBStart(),
                         availISize, availBSize);

  // for now return a physical rect
  return availSpace.GetPhysicalRect(wm, aState.ContentISize());
}

nscoord
nsBlockFrame::ComputeFloatWidth(nsBlockReflowState& aState,
                                const nsRect&       aFloatAvailableSpace,
                                nsIFrame*           aFloat)
{
  NS_PRECONDITION(aFloat->GetStateBits() & NS_FRAME_OUT_OF_FLOW,
                  "aFloat must be an out-of-flow frame");
  // Reflow the float.
  nsRect availSpace = AdjustFloatAvailableSpace(aState, aFloatAvailableSpace,
                                                aFloat);

  nsHTMLReflowState floatRS(aState.mPresContext, aState.mReflowState, aFloat, 
                            availSpace.Size());
  return floatRS.ComputedWidth() + floatRS.ComputedPhysicalBorderPadding().LeftRight() +
    floatRS.ComputedPhysicalMargin().LeftRight();
}

void
nsBlockFrame::ReflowFloat(nsBlockReflowState& aState,
                          const nsRect&       aAdjustedAvailableSpace,
                          nsIFrame*           aFloat,
                          nsMargin&           aFloatMargin,
                          nsMargin&           aFloatOffsets,
                          bool                aFloatPushedDown,
                          nsReflowStatus&     aReflowStatus)
{
  NS_PRECONDITION(aFloat->GetStateBits() & NS_FRAME_OUT_OF_FLOW,
                  "aFloat must be an out-of-flow frame");
  // Reflow the float.
  aReflowStatus = NS_FRAME_COMPLETE;

#ifdef NOISY_FLOAT
  printf("Reflow Float %p in parent %p, availSpace(%d,%d,%d,%d)\n",
          aFloat, this, 
          aFloatAvailableSpace.x, aFloatAvailableSpace.y, 
          aFloatAvailableSpace.width, aFloatAvailableSpace.height
  );
#endif

  nsHTMLReflowState floatRS(aState.mPresContext, aState.mReflowState, aFloat,
                            nsSize(aAdjustedAvailableSpace.width,
                                   aAdjustedAvailableSpace.height));

  // Normally the mIsTopOfPage state is copied from the parent reflow
  // state.  However, when reflowing a float, if we've placed other
  // floats that force this float *down* or *narrower*, we should unset
  // the mIsTopOfPage state.
  // FIXME: This is somewhat redundant with the |isAdjacentWithTop|
  // variable below, which has the exact same effect.  Perhaps it should
  // be merged into that, except that the test for narrowing here is not
  // about adjacency with the top, so it seems misleading.
  if (floatRS.mFlags.mIsTopOfPage &&
      (aFloatPushedDown ||
       aAdjustedAvailableSpace.width != aState.ContentISize())) {
    floatRS.mFlags.mIsTopOfPage = false;
  }

  // Setup a block reflow context to reflow the float.
  nsBlockReflowContext brc(aState.mPresContext, aState.mReflowState);

  // Reflow the float
  bool isAdjacentWithTop = aState.IsAdjacentWithTop();

  nsIFrame* clearanceFrame = nullptr;
  do {
    nsCollapsingMargin margin;
    bool mayNeedRetry = false;
    floatRS.mDiscoveredClearance = nullptr;
    // Only first in flow gets a top margin.
    if (!aFloat->GetPrevInFlow()) {
      nsBlockReflowContext::ComputeCollapsedBStartMargin(floatRS, &margin,
                                                         clearanceFrame,
                                                         &mayNeedRetry);

      if (mayNeedRetry && !clearanceFrame) {
        floatRS.mDiscoveredClearance = &clearanceFrame;
        // We don't need to push the float manager state because the the block has its own
        // float manager that will be destroyed and recreated
      }
    }

    brc.ReflowBlock(aAdjustedAvailableSpace, true, margin,
                    0, isAdjacentWithTop,
                    nullptr, floatRS,
                    aReflowStatus, aState);
  } while (clearanceFrame);

  if (!NS_FRAME_IS_FULLY_COMPLETE(aReflowStatus) &&
      ShouldAvoidBreakInside(floatRS)) {
    aReflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
  } else if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus) &&
             (NS_UNCONSTRAINEDSIZE == aAdjustedAvailableSpace.height)) {
    // An incomplete reflow status means we should split the float 
    // if the height is constrained (bug 145305). 
    aReflowStatus = NS_FRAME_COMPLETE;
  }

  if (aReflowStatus & NS_FRAME_REFLOW_NEXTINFLOW) {
    aState.mReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
  }

  if (aFloat->GetType() == nsGkAtoms::letterFrame) {
    // We never split floating first letters; an incomplete state for
    // such frames simply means that there is more content to be
    // reflowed on the line.
    if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus)) 
      aReflowStatus = NS_FRAME_COMPLETE;
  }

  // Capture the margin and offsets information for the caller
  aFloatMargin = floatRS.ComputedPhysicalMargin(); // float margins don't collapse
  aFloatOffsets = floatRS.ComputedPhysicalOffsets();

  const nsHTMLReflowMetrics& metrics = brc.GetMetrics();

  // Set the rect, make sure the view is properly sized and positioned,
  // and tell the frame we're done reflowing it
  // XXXldb This seems like the wrong place to be doing this -- shouldn't
  // we be doing this in nsBlockReflowState::FlowAndPlaceFloat after
  // we've positioned the float, and shouldn't we be doing the equivalent
  // of |PlaceFrameView| here?
  aFloat->SetSize(nsSize(metrics.Width(), metrics.Height()));
  if (aFloat->HasView()) {
    nsContainerFrame::SyncFrameViewAfterReflow(aState.mPresContext, aFloat,
                                               aFloat->GetView(),
                                               metrics.VisualOverflow(),
                                               NS_FRAME_NO_MOVE_VIEW);
  }
  // Pass floatRS so the frame hierarchy can be used (redoFloatRS has the same hierarchy)  
  aFloat->DidReflow(aState.mPresContext, &floatRS,
                    nsDidReflowStatus::FINISHED);

#ifdef NOISY_FLOAT
  printf("end ReflowFloat %p, sized to %d,%d\n",
         aFloat, metrics.Width(), metrics.Height());
#endif
}

uint8_t
nsBlockFrame::FindTrailingClear()
{
  // find the break type of the last line
  for (nsIFrame* b = this; b; b = b->GetPrevInFlow()) {
    nsBlockFrame* block = static_cast<nsBlockFrame*>(b);
    line_iterator endLine = block->end_lines();
    if (endLine != block->begin_lines()) {
      --endLine;
      return endLine->GetBreakTypeAfter();
    }
  }
  return NS_STYLE_CLEAR_NONE;
}

void
nsBlockFrame::ReflowPushedFloats(nsBlockReflowState& aState,
                                 nsOverflowAreas&    aOverflowAreas,
                                 nsReflowStatus&     aStatus)
{
  // Pushed floats live at the start of our float list; see comment
  // above nsBlockFrame::DrainPushedFloats.
  for (nsIFrame* f = mFloats.FirstChild(), *next;
       f && (f->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT);
       f = next) {
    // save next sibling now, since reflowing could push the entire
    // float, changing its siblings
    next = f->GetNextSibling();

    // When we push a first-continuation float in a non-initial reflow,
    // it's possible that we end up with two continuations with the same
    // parent.  This happens if, on the previous reflow of the block or
    // a previous reflow of the line containing the block, the float was
    // split between continuations A and B of the parent, but on the
    // current reflow, none of the float can fit in A.
    //
    // When this happens, we might even have the two continuations
    // out-of-order due to the management of the pushed floats.  In
    // particular, if the float's placeholder was in a pushed line that
    // we reflowed before it was pushed, and we split the float during
    // that reflow, we might have the continuation of the float before
    // the float itself.  (In the general case, however, it's correct
    // for floats in the pushed floats list to come before floats
    // anchored in pushed lines; however, in this case it's wrong.  We
    // should probably find a way to fix it somehow, since it leads to
    // incorrect layout in some cases.)
    //
    // When we have these out-of-order continuations, we might hit the
    // next-continuation before the previous-continuation.  When that
    // happens, just push it.  When we reflow the next continuation,
    // we'll either pull all of its content back and destroy it (by
    // calling DeleteNextInFlowChild), or nsBlockFrame::SplitFloat will
    // pull it out of its current position and push it again (and
    // potentially repeat this cycle for the next continuation, although
    // hopefully then they'll be in the right order).
    //
    // We should also need this code for the in-order case if the first
    // continuation of a float gets moved across more than one
    // continuation of the containing block.  In this case we'd manage
    // to push the second continuation without this check, but not the
    // third and later.
    nsIFrame *prevContinuation = f->GetPrevContinuation();
    if (prevContinuation && prevContinuation->GetParent() == f->GetParent()) {
      mFloats.RemoveFrame(f);
      aState.AppendPushedFloat(f);
      continue;
    }

    // Always call FlowAndPlaceFloat; we might need to place this float
    // if didn't belong to this block the last time it was reflowed.
    aState.FlowAndPlaceFloat(f);

    ConsiderChildOverflow(aOverflowAreas, f);
  }

  // If there are continued floats, then we may need to continue BR clearance
  if (0 != aState.ClearFloats(0, NS_STYLE_CLEAR_BOTH)) {
    aState.mFloatBreakType = static_cast<nsBlockFrame*>(GetPrevInFlow())
                               ->FindTrailingClear();
  }
}

void
nsBlockFrame::RecoverFloats(nsFloatManager& aFloatManager)
{
  // Recover our own floats
  nsIFrame* stop = nullptr; // Stop before we reach pushed floats that
                           // belong to our next-in-flow
  for (nsIFrame* f = mFloats.FirstChild(); f && f != stop; f = f->GetNextSibling()) {
    nsRect region = nsFloatManager::GetRegionFor(f);
    aFloatManager.AddFloat(f, region);
    if (!stop && f->GetNextInFlow())
      stop = f->GetNextInFlow();
  }

  // Recurse into our overflow container children
  for (nsIFrame* oc = GetFirstChild(kOverflowContainersList);
       oc; oc = oc->GetNextSibling()) {
    RecoverFloatsFor(oc, aFloatManager);
  }

  // Recurse into our normal children
  for (nsBlockFrame::line_iterator line = begin_lines(); line != end_lines(); ++line) {
    if (line->IsBlock()) {
      RecoverFloatsFor(line->mFirstChild, aFloatManager);
    }
  }
}

void
nsBlockFrame::RecoverFloatsFor(nsIFrame*       aFrame,
                               nsFloatManager& aFloatManager)
{
  NS_PRECONDITION(aFrame, "null frame");
  // Only blocks have floats
  nsBlockFrame* block = nsLayoutUtils::GetAsBlock(aFrame);
  // Don't recover any state inside a block that has its own space manager
  // (we don't currently have any blocks like this, though, thanks to our
  // use of extra frames for 'overflow')
  if (block && !nsBlockFrame::BlockNeedsFloatManager(block)) {
    // If the element is relatively positioned, then adjust x and y
    // accordingly so that we consider relatively positioned frames
    // at their original position.
    nsPoint pos = block->GetNormalPosition();
    aFloatManager.Translate(pos.x, pos.y);
    block->RecoverFloats(aFloatManager);
    aFloatManager.Translate(-pos.x, -pos.y);
  }
}

//////////////////////////////////////////////////////////////////////
// Painting, event handling

#ifdef DEBUG
static void ComputeVisualOverflowArea(nsLineList& aLines,
                                      nscoord aWidth, nscoord aHeight,
                                      nsRect& aResult)
{
  nscoord xa = 0, ya = 0, xb = aWidth, yb = aHeight;
  for (nsLineList::iterator line = aLines.begin(), line_end = aLines.end();
       line != line_end;
       ++line) {
    // Compute min and max x/y values for the reflowed frame's
    // combined areas
    nsRect visOverflow(line->GetVisualOverflowArea());
    nscoord x = visOverflow.x;
    nscoord y = visOverflow.y;
    nscoord xmost = x + visOverflow.width;
    nscoord ymost = y + visOverflow.height;
    if (x < xa) {
      xa = x;
    }
    if (xmost > xb) {
      xb = xmost;
    }
    if (y < ya) {
      ya = y;
    }
    if (ymost > yb) {
      yb = ymost;
    }
  }

  aResult.x = xa;
  aResult.y = ya;
  aResult.width = xb - xa;
  aResult.height = yb - ya;
}
#endif

bool
nsBlockFrame::IsVisibleInSelection(nsISelection* aSelection)
{
  if (mContent->IsHTML() && (mContent->Tag() == nsGkAtoms::html ||
                             mContent->Tag() == nsGkAtoms::body))
    return true;

  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
  bool visible;
  nsresult rv = aSelection->ContainsNode(node, true, &visible);
  return NS_SUCCEEDED(rv) && visible;
}

#ifdef DEBUG
static void DebugOutputDrawLine(int32_t aDepth, nsLineBox* aLine, bool aDrawn) {
  if (nsBlockFrame::gNoisyDamageRepair) {
    nsFrame::IndentBy(stdout, aDepth+1);
    nsRect lineArea = aLine->GetVisualOverflowArea();
    printf("%s line=%p bounds=%d,%d,%d,%d ca=%d,%d,%d,%d\n",
           aDrawn ? "draw" : "skip",
           static_cast<void*>(aLine),
           aLine->IStart(), aLine->BStart(),
           aLine->ISize(), aLine->BSize(),
           lineArea.x, lineArea.y,
           lineArea.width, lineArea.height);
  }
}
#endif

static void
DisplayLine(nsDisplayListBuilder* aBuilder, const nsRect& aLineArea,
            const nsRect& aDirtyRect, nsBlockFrame::line_iterator& aLine,
            int32_t aDepth, int32_t& aDrawnLines, const nsDisplayListSet& aLists,
            nsBlockFrame* aFrame, TextOverflow* aTextOverflow) {
  // If the line's combined area (which includes child frames that
  // stick outside of the line's bounding box or our bounding box)
  // intersects the dirty rect then paint the line.
  bool intersect = aLineArea.Intersects(aDirtyRect);
#ifdef DEBUG
  if (nsBlockFrame::gLamePaintMetrics) {
    aDrawnLines++;
  }
  DebugOutputDrawLine(aDepth, aLine.get(), intersect);
#endif
  // The line might contain a placeholder for a visible out-of-flow, in which
  // case we need to descend into it. If there is such a placeholder, we will
  // have NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO set.
  // In particular, we really want to check ShouldDescendIntoFrame()
  // on all the frames on the line, but that might be expensive.  So
  // we approximate it by checking it on aFrame; if it's true for any
  // frame in the line, it's also true for aFrame.
  bool lineInline = aLine->IsInline();
  bool lineMayHaveTextOverflow = aTextOverflow && lineInline;
  if (!intersect && !aBuilder->ShouldDescendIntoFrame(aFrame) &&
      !lineMayHaveTextOverflow)
    return;

  // Collect our line's display items in a temporary nsDisplayListCollection,
  // so that we can apply any "text-overflow" clipping to the entire collection
  // without affecting previous lines.
  nsDisplayListCollection collection;

  // Block-level child backgrounds go on the blockBorderBackgrounds list ...
  // Inline-level child backgrounds go on the regular child content list.
  nsDisplayListSet childLists(collection,
    lineInline ? collection.Content() : collection.BlockBorderBackgrounds());

  uint32_t flags = lineInline ? nsIFrame::DISPLAY_CHILD_INLINE : 0;

  nsIFrame* kid = aLine->mFirstChild;
  int32_t n = aLine->GetChildCount();
  while (--n >= 0) {
    aFrame->BuildDisplayListForChild(aBuilder, kid, aDirtyRect,
                                     childLists, flags);
    kid = kid->GetNextSibling();
  }
  
  if (lineMayHaveTextOverflow) {
    aTextOverflow->ProcessLine(collection, aLine.get());
  }

  collection.MoveTo(aLists);
}

void
nsBlockFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  int32_t drawnLines; // Will only be used if set (gLamePaintMetrics).
  int32_t depth = 0;
#ifdef DEBUG
  if (gNoisyDamageRepair) {
      depth = GetDepth();
      nsRect ca;
      ::ComputeVisualOverflowArea(mLines, mRect.width, mRect.height, ca);
      nsFrame::IndentBy(stdout, depth);
      ListTag(stdout);
      printf(": bounds=%d,%d,%d,%d dirty(absolute)=%d,%d,%d,%d ca=%d,%d,%d,%d\n",
             mRect.x, mRect.y, mRect.width, mRect.height,
             aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height,
             ca.x, ca.y, ca.width, ca.height);
  }
  PRTime start = 0; // Initialize these variables to silence the compiler.
  if (gLamePaintMetrics) {
    start = PR_Now();
    drawnLines = 0;
  }
#endif

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  if (GetPrevInFlow()) {
    DisplayOverflowContainers(aBuilder, aDirtyRect, aLists);
    for (nsIFrame* f = mFloats.FirstChild(); f; f = f->GetNextSibling()) {
      if (f->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT)
         BuildDisplayListForChild(aBuilder, f, aDirtyRect, aLists);
    }
  }

  aBuilder->MarkFramesForDisplayList(this, mFloats, aDirtyRect);

  // Prepare for text-overflow processing.
  nsAutoPtr<TextOverflow> textOverflow(
    TextOverflow::WillProcessLines(aBuilder, this));

  // We'll collect our lines' display items here, & then append this to aLists.
  nsDisplayListCollection linesDisplayListCollection;

  // Don't use the line cursor if we might have a descendant placeholder ...
  // it might skip lines that contain placeholders but don't themselves
  // intersect with the dirty area.
  // In particular, we really want to check ShouldDescendIntoFrame()
  // on all our child frames, but that might be expensive.  So we
  // approximate it by checking it on |this|; if it's true for any
  // frame in our child list, it's also true for |this|.
  nsLineBox* cursor = aBuilder->ShouldDescendIntoFrame(this) ?
    nullptr : GetFirstLineContaining(aDirtyRect.y);
  line_iterator line_end = end_lines();
  
  if (cursor) {
    for (line_iterator line = mLines.begin(cursor);
         line != line_end;
         ++line) {
      nsRect lineArea = line->GetVisualOverflowArea();
      if (!lineArea.IsEmpty()) {
        // Because we have a cursor, the combinedArea.ys are non-decreasing.
        // Once we've passed aDirtyRect.YMost(), we can never see it again.
        if (lineArea.y >= aDirtyRect.YMost()) {
          break;
        }
        DisplayLine(aBuilder, lineArea, aDirtyRect, line, depth, drawnLines,
                    linesDisplayListCollection, this, textOverflow);
      }
    }
  } else {
    bool nonDecreasingYs = true;
    int32_t lineCount = 0;
    nscoord lastY = INT32_MIN;
    nscoord lastYMost = INT32_MIN;
    for (line_iterator line = begin_lines();
         line != line_end;
         ++line) {
      nsRect lineArea = line->GetVisualOverflowArea();
      DisplayLine(aBuilder, lineArea, aDirtyRect, line, depth, drawnLines,
                  linesDisplayListCollection, this, textOverflow);
      if (!lineArea.IsEmpty()) {
        if (lineArea.y < lastY
            || lineArea.YMost() < lastYMost) {
          nonDecreasingYs = false;
        }
        lastY = lineArea.y;
        lastYMost = lineArea.YMost();
      }
      lineCount++;
    }

    if (nonDecreasingYs && lineCount >= MIN_LINES_NEEDING_CURSOR) {
      SetupLineCursor();
    }
  }

  // Pick up the resulting text-overflow markers.  We append them to
  // PositionedDescendants just before we append the lines' display items,
  // so that our text-overflow markers will appear on top of this block's
  // normal content but below any of its its' positioned children.
  if (textOverflow) {
    aLists.PositionedDescendants()->AppendToTop(&textOverflow->GetMarkers());
  }
  linesDisplayListCollection.MoveTo(aLists);

  if (HasOutsideBullet()) {
    // Display outside bullets manually
    nsIFrame* bullet = GetOutsideBullet();
    BuildDisplayListForChild(aBuilder, bullet, aDirtyRect, aLists);
  }

#ifdef DEBUG
  if (gLamePaintMetrics) {
    PRTime end = PR_Now();

    int32_t numLines = mLines.size();
    if (!numLines) numLines = 1;
    PRTime lines, deltaPerLine, delta;
    lines = int64_t(numLines);
    delta = end - start;
    deltaPerLine = delta / lines;

    ListTag(stdout);
    char buf[400];
    PR_snprintf(buf, sizeof(buf),
                ": %lld elapsed (%lld per line) lines=%d drawn=%d skip=%d",
                delta, deltaPerLine,
                numLines, drawnLines, numLines - drawnLines);
    printf("%s\n", buf);
  }
#endif
}

#ifdef ACCESSIBILITY
a11y::AccType
nsBlockFrame::AccessibleType()
{
  // block frame may be for <hr>
  if (mContent->Tag() == nsGkAtoms::hr) {
    return a11y::eHTMLHRType;
  }

  if (!HasBullet() || !PresContext()) {
    if (!mContent->GetParent()) {
      // Don't create accessible objects for the root content node, they are redundant with
      // the nsDocAccessible object created with the document node
      return a11y::eNoType;
    }
    
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc =
      do_QueryInterface(mContent->GetDocument());
    if (htmlDoc) {
      nsCOMPtr<nsIDOMHTMLElement> body;
      htmlDoc->GetBody(getter_AddRefs(body));
      if (SameCOMIdentity(body, mContent)) {
        // Don't create accessible objects for the body, they are redundant with
        // the nsDocAccessible object created with the document node
        return a11y::eNoType;
      }
    }

    // Not a bullet, treat as normal HTML container
    return a11y::eHyperTextType;
  }

  // Create special list bullet accessible
  return a11y::eHTMLLiType;
}
#endif

void nsBlockFrame::ClearLineCursor()
{
  if (!(GetStateBits() & NS_BLOCK_HAS_LINE_CURSOR)) {
    return;
  }

  Properties().Delete(LineCursorProperty());
  RemoveStateBits(NS_BLOCK_HAS_LINE_CURSOR);
}

void nsBlockFrame::SetupLineCursor()
{
  if (GetStateBits() & NS_BLOCK_HAS_LINE_CURSOR
      || mLines.empty()) {
    return;
  }
   
  Properties().Set(LineCursorProperty(), mLines.front());
  AddStateBits(NS_BLOCK_HAS_LINE_CURSOR);
}

nsLineBox* nsBlockFrame::GetFirstLineContaining(nscoord y)
{
  if (!(GetStateBits() & NS_BLOCK_HAS_LINE_CURSOR)) {
    return nullptr;
  }

  FrameProperties props = Properties();
  
  nsLineBox* property = static_cast<nsLineBox*>
    (props.Get(LineCursorProperty()));
  line_iterator cursor = mLines.begin(property);
  nsRect cursorArea = cursor->GetVisualOverflowArea();

  while ((cursorArea.IsEmpty() || cursorArea.YMost() > y)
         && cursor != mLines.front()) {
    cursor = cursor.prev();
    cursorArea = cursor->GetVisualOverflowArea();
  }
  while ((cursorArea.IsEmpty() || cursorArea.YMost() <= y)
         && cursor != mLines.back()) {
    cursor = cursor.next();
    cursorArea = cursor->GetVisualOverflowArea();
  }

  if (cursor.get() != property) {
    props.Set(LineCursorProperty(), cursor.get());
  }

  return cursor.get();
}

/* virtual */ void
nsBlockFrame::ChildIsDirty(nsIFrame* aChild)
{
  // See if the child is absolutely positioned
  if (aChild->GetStateBits() & NS_FRAME_OUT_OF_FLOW &&
      aChild->IsAbsolutelyPositioned()) {
    // do nothing
  } else if (aChild == GetOutsideBullet()) {
    // The bullet lives in the first line, unless the first line has
    // height 0 and there is a second line, in which case it lives
    // in the second line.
    line_iterator bulletLine = begin_lines();
    if (bulletLine != end_lines() && bulletLine->BSize() == 0 &&
        bulletLine != mLines.back()) {
      bulletLine = bulletLine.next();
    }
    
    if (bulletLine != end_lines()) {
      MarkLineDirty(bulletLine, &mLines);
    }
    // otherwise we have an empty line list, and ReflowDirtyLines
    // will handle reflowing the bullet.
  } else {
    // Note that we should go through our children to mark lines dirty
    // before the next reflow.  Doing it now could make things O(N^2)
    // since finding the right line is O(N).
    // We don't need to worry about marking lines on the overflow list
    // as dirty; we're guaranteed to reflow them if we take them off the
    // overflow list.
    // However, we might have gotten a float, in which case we need to
    // reflow the line containing its placeholder.  So find the
    // ancestor-or-self of the placeholder that's a child of the block,
    // and mark it as NS_FRAME_HAS_DIRTY_CHILDREN too, so that we mark
    // its line dirty when we handle NS_BLOCK_LOOK_FOR_DIRTY_FRAMES.
    // We need to take some care to handle the case where a float is in
    // a different continuation than its placeholder, including marking
    // an extra block with NS_BLOCK_LOOK_FOR_DIRTY_FRAMES.
    if (!(aChild->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
      AddStateBits(NS_BLOCK_LOOK_FOR_DIRTY_FRAMES);
    } else {
      NS_ASSERTION(aChild->IsFloating(), "should be a float");
      nsIFrame *thisFC = FirstContinuation();
      nsIFrame *placeholderPath =
        PresContext()->FrameManager()->GetPlaceholderFrameFor(aChild);
      // SVG code sometimes sends FrameNeedsReflow notifications during
      // frame destruction, leading to null placeholders, but we're safe
      // ignoring those.
      if (placeholderPath) {
        for (;;) {
          nsIFrame *parent = placeholderPath->GetParent();
          if (parent->GetContent() == mContent &&
              parent->FirstContinuation() == thisFC) {
            parent->AddStateBits(NS_BLOCK_LOOK_FOR_DIRTY_FRAMES);
            break;
          }
          placeholderPath = parent;
        }
        placeholderPath->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
      }
    }
  }

  nsBlockFrameSuper::ChildIsDirty(aChild);
}

void
nsBlockFrame::Init(nsIContent*       aContent,
                   nsContainerFrame* aParent,
                   nsIFrame*         aPrevInFlow)
{
  if (aPrevInFlow) {
    // Copy over the inherited block frame bits from the prev-in-flow.
    SetFlags(aPrevInFlow->GetStateBits() &
             (NS_BLOCK_FLAGS_MASK & ~NS_BLOCK_FLAGS_NON_INHERITED_MASK));
  }

  nsBlockFrameSuper::Init(aContent, aParent, aPrevInFlow);

  if (!aPrevInFlow ||
      aPrevInFlow->GetStateBits() & NS_BLOCK_NEEDS_BIDI_RESOLUTION)
    AddStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);

  if ((GetStateBits() &
       (NS_FRAME_FONT_INFLATION_CONTAINER | NS_BLOCK_FLOAT_MGR)) ==
      (NS_FRAME_FONT_INFLATION_CONTAINER | NS_BLOCK_FLOAT_MGR)) {
    AddStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT);
  }
}

void
nsBlockFrame::SetInitialChildList(ChildListID     aListID,
                                  nsFrameList&    aChildList)
{
  NS_ASSERTION(aListID != kPrincipalList ||
               (GetStateBits() & (NS_BLOCK_FRAME_HAS_INSIDE_BULLET |
                                  NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET)) == 0,
               "how can we have a bullet already?");

  if (kAbsoluteList == aListID) {
    nsContainerFrame::SetInitialChildList(aListID, aChildList);
  }
  else if (kFloatList == aListID) {
    mFloats.SetFrames(aChildList);
  }
  else {
    nsPresContext* presContext = PresContext();

#ifdef DEBUG
    // The only times a block that is an anonymous box is allowed to have a
    // first-letter frame are when it's the block inside a non-anonymous cell,
    // the block inside a fieldset, a scrolled content block, or a column
    // content block.  Note that this means that blocks which are the anonymous
    // block in {ib} splits do NOT get first-letter frames.  Note that
    // NS_BLOCK_HAS_FIRST_LETTER_STYLE gets set on all continuations of the
    // block.
    nsIAtom *pseudo = StyleContext()->GetPseudo();
    bool haveFirstLetterStyle =
      (!pseudo ||
       (pseudo == nsCSSAnonBoxes::cellContent &&
        GetParent()->StyleContext()->GetPseudo() == nullptr) ||
       pseudo == nsCSSAnonBoxes::fieldsetContent ||
       pseudo == nsCSSAnonBoxes::scrolledContent ||
       pseudo == nsCSSAnonBoxes::columnContent ||
       pseudo == nsCSSAnonBoxes::mozSVGText) &&
      !IsFrameOfType(eMathML) &&
      nsRefPtr<nsStyleContext>(GetFirstLetterStyle(presContext)) != nullptr;
    NS_ASSERTION(haveFirstLetterStyle ==
                 ((mState & NS_BLOCK_HAS_FIRST_LETTER_STYLE) != 0),
                 "NS_BLOCK_HAS_FIRST_LETTER_STYLE state out of sync");
#endif
    
    AddFrames(aChildList, nullptr);

    // Create a list bullet if this is a list-item. Note that this is
    // done here so that RenumberLists will work (it needs the bullets
    // to store the bullet numbers).  Also note that due to various
    // wrapper frames (scrollframes, columns) we want to use the
    // outermost (primary, ideally, but it's not set yet when we get
    // here) frame of our content for the display check.  On the other
    // hand, we look at ourselves for the GetPrevInFlow() check, since
    // for a columnset we don't want a bullet per column.  Note that
    // the outermost frame for the content is the primary frame in
    // most cases; the ones when it's not (like tables) can't be
    // NS_STYLE_DISPLAY_LIST_ITEM).
    nsIFrame* possibleListItem = this;
    while (1) {
      nsIFrame* parent = possibleListItem->GetParent();
      if (parent->GetContent() != GetContent()) {
        break;
      }
      possibleListItem = parent;
    }
    if (NS_STYLE_DISPLAY_LIST_ITEM ==
          possibleListItem->StyleDisplay()->mDisplay &&
        !GetPrevInFlow()) {
      // Resolve style for the bullet frame
      const nsStyleList* styleList = StyleList();
      CounterStyle* style = styleList->GetCounterStyle();
      nsCSSPseudoElements::Type pseudoType = style->IsBullet() ?
        nsCSSPseudoElements::ePseudo_mozListBullet :
        nsCSSPseudoElements::ePseudo_mozListNumber;

      nsIPresShell *shell = presContext->PresShell();

      nsStyleContext* parentStyle =
        CorrectStyleParentFrame(this,
          nsCSSPseudoElements::GetPseudoAtom(pseudoType))->StyleContext();
      nsRefPtr<nsStyleContext> kidSC = shell->StyleSet()->
        ResolvePseudoElementStyle(mContent->AsElement(), pseudoType,
                                  parentStyle, nullptr);

      // Create bullet frame
      nsBulletFrame* bullet = new (shell) nsBulletFrame(kidSC);
      bullet->Init(mContent, this, nullptr);

      // If the list bullet frame should be positioned inside then add
      // it to the flow now.
      if (NS_STYLE_LIST_STYLE_POSITION_INSIDE ==
            styleList->mListStylePosition) {
        nsFrameList bulletList(bullet, bullet);
        AddFrames(bulletList, nullptr);
        Properties().Set(InsideBulletProperty(), bullet);
        AddStateBits(NS_BLOCK_FRAME_HAS_INSIDE_BULLET);
      } else {
        nsFrameList* bulletList = new (shell) nsFrameList(bullet, bullet);
        Properties().Set(OutsideBulletProperty(), bulletList);
        AddStateBits(NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET);
      }
    }
  }
}

bool
nsBlockFrame::BulletIsEmpty() const
{
  NS_ASSERTION(mContent->GetPrimaryFrame()->StyleDisplay()->mDisplay ==
                 NS_STYLE_DISPLAY_LIST_ITEM && HasOutsideBullet(),
               "should only care when we have an outside bullet");
  const nsStyleList* list = StyleList();
  return list->GetCounterStyle()->IsNone() &&
         !list->GetListStyleImage();
}

void
nsBlockFrame::GetSpokenBulletText(nsAString& aText) const
{
  const nsStyleList* myList = StyleList();
  if (myList->GetListStyleImage()) {
    aText.Assign(kDiscCharacter);
    aText.Append(' ');
  } else {
    nsBulletFrame* bullet = GetBullet();
    if (bullet) {
      bullet->GetSpokenText(aText);
    } else {
      aText.Truncate();
    }
  }
}

// static
bool
nsBlockFrame::FrameStartsCounterScope(nsIFrame* aFrame)
{
  nsIContent* content = aFrame->GetContent();
  if (!content || !content->IsHTML())
    return false;

  nsIAtom *localName = content->NodeInfo()->NameAtom();
  return localName == nsGkAtoms::ol ||
         localName == nsGkAtoms::ul ||
         localName == nsGkAtoms::dir ||
         localName == nsGkAtoms::menu;
}

bool
nsBlockFrame::RenumberLists(nsPresContext* aPresContext)
{
  if (!FrameStartsCounterScope(this)) {
    // If this frame doesn't start a counter scope then we don't need
    // to renumber child list items.
    return false;
  }

  MOZ_ASSERT(mContent->IsHTML(),
             "FrameStartsCounterScope should only return true for HTML elements");

  // Setup initial list ordinal value
  // XXX Map html's start property to counter-reset style
  int32_t ordinal = 1;
  int32_t increment;
  if (mContent->Tag() == nsGkAtoms::ol &&
      mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::reversed)) {
    increment = -1;
  } else {
    increment = 1;
  }

  nsGenericHTMLElement *hc = nsGenericHTMLElement::FromContent(mContent);
  // Must be non-null, since FrameStartsCounterScope only returns true
  // for HTML elements.
  MOZ_ASSERT(hc, "How is mContent not HTML?");
  const nsAttrValue* attr = hc->GetParsedAttr(nsGkAtoms::start);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    ordinal = attr->GetIntegerValue();
  } else if (increment < 0) {
    // <ol reversed> case, or some other case with a negative increment: count
    // up the child list
    ordinal = 0;
    for (nsIContent* kid = mContent->GetFirstChild(); kid;
         kid = kid->GetNextSibling()) {
      if (kid->IsHTML(nsGkAtoms::li)) {
        // FIXME: This isn't right in terms of what CSS says to do for
        // overflow of counters (but it only matters when this node has
        // more than numeric_limits<int32_t>::max() children).
        ordinal -= increment;
      }
    }
  }

  // Get to first-in-flow
  nsBlockFrame* block = static_cast<nsBlockFrame*>(FirstInFlow());
  return RenumberListsInBlock(aPresContext, block, &ordinal, 0, increment);
}

bool
nsBlockFrame::RenumberListsInBlock(nsPresContext* aPresContext,
                                   nsBlockFrame* aBlockFrame,
                                   int32_t* aOrdinal,
                                   int32_t aDepth,
                                   int32_t aIncrement)
{
  // Examine each line in the block
  bool foundValidLine;
  nsBlockInFlowLineIterator bifLineIter(aBlockFrame, &foundValidLine);
  
  if (!foundValidLine)
    return false;

  bool renumberedABullet = false;

  do {
    nsLineList::iterator line = bifLineIter.GetLine();
    nsIFrame* kid = line->mFirstChild;
    int32_t n = line->GetChildCount();
    while (--n >= 0) {
      bool kidRenumberedABullet = RenumberListsFor(aPresContext, kid, aOrdinal,
                                                   aDepth, aIncrement);
      if (kidRenumberedABullet) {
        line->MarkDirty();
        renumberedABullet = true;
      }
      kid = kid->GetNextSibling();
    }
  } while (bifLineIter.Next());

  // We need to set NS_FRAME_HAS_DIRTY_CHILDREN bits up the tree between
  // the bullet and the caller of RenumberLists.  But the caller itself
  // has to be responsible for setting the bit itself, since that caller
  // might be making a FrameNeedsReflow call, which requires that the
  // bit not be set yet.
  if (renumberedABullet && aDepth != 0) {
    aBlockFrame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
  }

  return renumberedABullet;
}

bool
nsBlockFrame::RenumberListsFor(nsPresContext* aPresContext,
                               nsIFrame* aKid,
                               int32_t* aOrdinal,
                               int32_t aDepth,
                               int32_t aIncrement)
{
  NS_PRECONDITION(aPresContext && aKid && aOrdinal, "null params are immoral!");

  // add in a sanity check for absurdly deep frame trees.  See bug 42138
  if (MAX_DEPTH_FOR_LIST_RENUMBERING < aDepth)
    return false;

  // if the frame is a placeholder, then get the out of flow frame
  nsIFrame* kid = nsPlaceholderFrame::GetRealFrameFor(aKid);
  const nsStyleDisplay* display = kid->StyleDisplay();

  // drill down through any wrappers to the real frame
  kid = kid->GetContentInsertionFrame();

  // possible there is no content insertion frame
  if (!kid)
    return false;

  bool kidRenumberedABullet = false;

  // If the frame is a list-item and the frame implements our
  // block frame API then get its bullet and set the list item
  // ordinal.
  if (NS_STYLE_DISPLAY_LIST_ITEM == display->mDisplay) {
    // Make certain that the frame is a block frame in case
    // something foreign has crept in.
    nsBlockFrame* listItem = nsLayoutUtils::GetAsBlock(kid);
    if (listItem) {
      nsBulletFrame* bullet = listItem->GetBullet();
      if (bullet) {
        bool changed;
        *aOrdinal = bullet->SetListItemOrdinal(*aOrdinal, &changed, aIncrement);
        if (changed) {
          kidRenumberedABullet = true;

          // The ordinal changed - mark the bullet frame, and any
          // intermediate frames between it and the block (are there
          // ever any?), dirty.
          // The calling code will make the necessary FrameNeedsReflow
          // call for the list ancestor.
          bullet->AddStateBits(NS_FRAME_IS_DIRTY);
          nsIFrame *f = bullet;
          do {
            nsIFrame *parent = f->GetParent();
            parent->ChildIsDirty(f);
            f = parent;
          } while (f != listItem);
        }
      }

      // XXX temporary? if the list-item has child list-items they
      // should be numbered too; especially since the list-item is
      // itself (ASSUMED!) not to be a counter-resetter.
      bool meToo = RenumberListsInBlock(aPresContext, listItem, aOrdinal,
                                        aDepth + 1, aIncrement);
      if (meToo) {
        kidRenumberedABullet = true;
      }
    }
  }
  else if (NS_STYLE_DISPLAY_BLOCK == display->mDisplay) {
    if (FrameStartsCounterScope(kid)) {
      // Don't bother recursing into a block frame that is a new
      // counter scope. Any list-items in there will be handled by
      // it.
    }
    else {
      // If the display=block element is a block frame then go ahead
      // and recurse into it, as it might have child list-items.
      nsBlockFrame* kidBlock = nsLayoutUtils::GetAsBlock(kid);
      if (kidBlock) {
        kidRenumberedABullet = RenumberListsInBlock(aPresContext, kidBlock,
                                                    aOrdinal, aDepth + 1,
                                                    aIncrement);
      }
    }
  }
  return kidRenumberedABullet;
}

void
nsBlockFrame::ReflowBullet(nsIFrame* aBulletFrame,
                           nsBlockReflowState& aState,
                           nsHTMLReflowMetrics& aMetrics,
                           nscoord aLineTop)
{
  const nsHTMLReflowState &rs = aState.mReflowState;

  // Reflow the bullet now
  nsSize availSize;
  // Make up a width since it doesn't really matter (XXX).
  availSize.width = aState.ContentISize();
  availSize.height = NS_UNCONSTRAINEDSIZE;

  // Get the reason right.
  // XXXwaterson Should this look just like the logic in
  // nsBlockReflowContext::ReflowBlock and nsLineLayout::ReflowFrame?
  nsHTMLReflowState reflowState(aState.mPresContext, rs,
                                aBulletFrame, availSize);
  nsReflowStatus  status;
  aBulletFrame->WillReflow(aState.mPresContext);
  aBulletFrame->Reflow(aState.mPresContext, aMetrics, reflowState, status);

  // Get the float available space using our saved state from before we
  // started reflowing the block, so that we ignore any floats inside
  // the block.
  // FIXME: aLineTop isn't actually set correctly by some callers, since
  // they reposition the line.
  nsRect floatAvailSpace =
    aState.GetFloatAvailableSpaceWithState(aLineTop,
                                           &aState.mFloatManagerStateBefore)
          .mRect;
  // FIXME (bug 25888): need to check the entire region that the first
  // line overlaps, not just the top pixel.

  // Place the bullet now.  We want to place the bullet relative to the
  // border-box of the associated block (using the right/left margin of
  // the bullet frame as separation).  However, if a line box would be
  // displaced by floats that are *outside* the associated block, we
  // want to displace it by the same amount.  That is, we act as though
  // the edge of the floats is the content-edge of the block, and place
  // the bullet at a position offset from there by the block's padding,
  // the block's border, and the bullet frame's margin.

  // IStart from floatAvailSpace gives us the content/float start edge
  // in the current writing mode. Then we subtract out the start
  // border/padding and the bullet's width and margin to offset the position.
  WritingMode wm = rs.GetWritingMode();
  nscoord containerWidth = floatAvailSpace.XMost();
  LogicalRect logicalFAS(wm, floatAvailSpace, containerWidth);
  // Get the bullet's margin, converted to our writing mode so that we can
  // combine it with other logical values here.
  WritingMode bulletWM = reflowState.GetWritingMode();
  LogicalMargin bulletMargin =
    reflowState.ComputedLogicalMargin().ConvertTo(wm, bulletWM);
  nscoord iStart = logicalFAS.IStart(wm) -
                   rs.ComputedLogicalBorderPadding().IStart(wm) -
                   bulletMargin.IEnd(wm) -
                   aMetrics.ISize(wm);

  // Approximate the bullets position; vertical alignment will provide
  // the final vertical location. We pass our writing-mode here, because
  // it may be different from the bullet frame's mode.
  nscoord bStart = logicalFAS.BStart(wm);
  aBulletFrame->SetRect(wm, LogicalRect(wm, LogicalPoint(wm, iStart, bStart),
                                        LogicalSize(wm, aMetrics.ISize(wm),
                                                    aMetrics.BSize(wm))),
                        containerWidth);
  aBulletFrame->DidReflow(aState.mPresContext, &aState.mReflowState,
                          nsDidReflowStatus::FINISHED);
}

// This is used to scan frames for any float placeholders, add their
// floats to the list represented by aList, and remove the
// floats from whatever list they might be in. We don't search descendants
// that are float containing blocks.  Floats that or not children of 'this'
// are ignored (they are not added to aList).
void
nsBlockFrame::DoCollectFloats(nsIFrame* aFrame, nsFrameList& aList,
                              bool aCollectSiblings)
{
  while (aFrame) {
    // Don't descend into float containing blocks.
    if (!aFrame->IsFloatContainingBlock()) {
      nsIFrame *outOfFlowFrame =
        aFrame->GetType() == nsGkAtoms::placeholderFrame ?
          nsLayoutUtils::GetFloatFromPlaceholder(aFrame) : nullptr;
      if (outOfFlowFrame && outOfFlowFrame->GetParent() == this) {
        RemoveFloat(outOfFlowFrame);
        aList.AppendFrame(nullptr, outOfFlowFrame);
        // FIXME: By not pulling floats whose parent is one of our
        // later siblings, are we risking the pushed floats getting
        // out-of-order?
        // XXXmats nsInlineFrame's lazy reparenting depends on NOT doing that.
      }

      DoCollectFloats(aFrame->GetFirstPrincipalChild(), aList, true);
      DoCollectFloats(aFrame->GetFirstChild(kOverflowList), aList, true);
    }
    if (!aCollectSiblings)
      break;
    aFrame = aFrame->GetNextSibling();
  }
}

void
nsBlockFrame::CheckFloats(nsBlockReflowState& aState)
{
#ifdef DEBUG
  // If any line is still dirty, that must mean we're going to reflow this
  // block again soon (e.g. because we bailed out after noticing that
  // clearance was imposed), so don't worry if the floats are out of sync.
  bool anyLineDirty = false;

  // Check that the float list is what we would have built
  nsAutoTArray<nsIFrame*, 8> lineFloats;
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end; ++line) {
    if (line->HasFloats()) {
      nsFloatCache* fc = line->GetFirstFloat();
      while (fc) {
        lineFloats.AppendElement(fc->mFloat);
        fc = fc->Next();
      }
    }
    if (line->IsDirty()) {
      anyLineDirty = true;
    }
  }
  
  nsAutoTArray<nsIFrame*, 8> storedFloats;
  bool equal = true;
  uint32_t i = 0;
  for (nsIFrame* f = mFloats.FirstChild(); f; f = f->GetNextSibling()) {
    if (f->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT)
      continue;
    storedFloats.AppendElement(f);
    if (i < lineFloats.Length() && lineFloats.ElementAt(i) != f) {
      equal = false;
    }
    ++i;
  }

  if ((!equal || lineFloats.Length() != storedFloats.Length()) && !anyLineDirty) {
    NS_WARNING("nsBlockFrame::CheckFloats: Explicit float list is out of sync with float cache");
#if defined(DEBUG_roc)
    nsFrame::RootFrameList(PresContext(), stdout, 0);
    for (i = 0; i < lineFloats.Length(); ++i) {
      printf("Line float: %p\n", lineFloats.ElementAt(i));
    }
    for (i = 0; i < storedFloats.Length(); ++i) {
      printf("Stored float: %p\n", storedFloats.ElementAt(i));
    }
#endif
  }
#endif

  const nsFrameList* oofs = GetOverflowOutOfFlows();
  if (oofs && oofs->NotEmpty()) {
    // Floats that were pushed should be removed from our float
    // manager.  Otherwise the float manager's YMost or XMost might
    // be larger than necessary, causing this block to get an
    // incorrect desired height (or width).  Some of these floats
    // may not actually have been added to the float manager because
    // they weren't reflowed before being pushed; that's OK,
    // RemoveRegions will ignore them. It is safe to do this here
    // because we know from here on the float manager will only be
    // used for its XMost and YMost, not to place new floats and
    // lines.
    aState.mFloatManager->RemoveTrailingRegions(oofs->FirstChild());
  }
}

void
nsBlockFrame::IsMarginRoot(bool* aBStartMarginRoot, bool* aBEndMarginRoot)
{
  if (!(GetStateBits() & NS_BLOCK_MARGIN_ROOT)) {
    nsIFrame* parent = GetParent();
    if (!parent || parent->IsFloatContainingBlock()) {
      *aBStartMarginRoot = false;
      *aBEndMarginRoot = false;
      return;
    }
    if (parent->GetType() == nsGkAtoms::columnSetFrame) {
      *aBStartMarginRoot = GetPrevInFlow() == nullptr;
      *aBEndMarginRoot = GetNextInFlow() == nullptr;
      return;
    }
  }

  *aBStartMarginRoot = true;
  *aBEndMarginRoot = true;
}

/* static */
bool
nsBlockFrame::BlockNeedsFloatManager(nsIFrame* aBlock)
{
  NS_PRECONDITION(aBlock, "Must have a frame");
  NS_ASSERTION(nsLayoutUtils::GetAsBlock(aBlock), "aBlock must be a block");

  nsIFrame* parent = aBlock->GetParent();
  return (aBlock->GetStateBits() & NS_BLOCK_FLOAT_MGR) ||
    (parent && !parent->IsFloatContainingBlock());
}

/* static */
bool
nsBlockFrame::BlockCanIntersectFloats(nsIFrame* aFrame)
{
  return aFrame->IsFrameOfType(nsIFrame::eBlockFrame) &&
         !aFrame->IsFrameOfType(nsIFrame::eReplaced) &&
         !(aFrame->GetStateBits() & NS_BLOCK_FLOAT_MGR);
}

// Note that this width can vary based on the vertical position.
// However, the cases where it varies are the cases where the width fits
// in the available space given, which means that variation shouldn't
// matter.
/* static */
nsBlockFrame::ReplacedElementWidthToClear
nsBlockFrame::WidthToClearPastFloats(nsBlockReflowState& aState,
                                     const nsRect& aFloatAvailableSpace,
                                     nsIFrame* aFrame)
{
  nscoord leftOffset, rightOffset;
  WritingMode wm = aState.mReflowState.GetWritingMode();
  nsCSSOffsetState offsetState(aFrame, aState.mReflowState.rendContext,
                               aState.mContentArea.Width(wm));

  ReplacedElementWidthToClear result;
  aState.ComputeReplacedBlockOffsetsForFloats(aFrame, aFloatAvailableSpace,
                                              leftOffset, rightOffset);
  nscoord availWidth = aState.mContentArea.Width(wm) - leftOffset - rightOffset;

  // We actually don't want the min width here; see bug 427782; we only
  // want to displace if the width won't compute to a value small enough
  // to fit.
  // All we really need here is the result of ComputeSize, and we
  // could *almost* get that from an nsCSSOffsetState, except for the
  // last argument.
  nsSize availSpace(availWidth, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState reflowState(aState.mPresContext, aState.mReflowState,
                                aFrame, availSpace);
  result.borderBoxWidth = reflowState.ComputedWidth() +
                          reflowState.ComputedPhysicalBorderPadding().LeftRight();
  // Use the margins from offsetState rather than reflowState so that
  // they aren't reduced by ignoring margins in overconstrained cases.
  result.marginLeft  = offsetState.ComputedPhysicalMargin().left;
  result.marginRight = offsetState.ComputedPhysicalMargin().right;
  return result;
}
 
/* static */
nsBlockFrame*
nsBlockFrame::GetNearestAncestorBlock(nsIFrame* aCandidate)
{
  nsBlockFrame* block = nullptr;
  while(aCandidate) {
    block = nsLayoutUtils::GetAsBlock(aCandidate);
    if (block) { 
      // yay, candidate is a block!
      return block;
    }
    // Not a block. Check its parent next.
    aCandidate = aCandidate->GetParent();
  }
  NS_NOTREACHED("Fell off frame tree looking for ancestor block!");
  return nullptr;
}

void
nsBlockFrame::ComputeFinalBSize(const nsHTMLReflowState& aReflowState,
                                nsReflowStatus*          aStatus,
                                nscoord                  aContentBSize,
                                const LogicalMargin&     aBorderPadding,
                                LogicalSize&             aFinalSize,
                                nscoord                  aConsumed)
{
  WritingMode wm = aReflowState.GetWritingMode();
  // Figure out how much of the computed height should be
  // applied to this frame.
  nscoord computedBSizeLeftOver = GetEffectiveComputedBSize(aReflowState,
                                                            aConsumed);
  NS_ASSERTION(!( IS_TRUE_OVERFLOW_CONTAINER(this)
                  && computedBSizeLeftOver ),
               "overflow container must not have computedBSizeLeftOver");

  aFinalSize.BSize(wm) =
    NSCoordSaturatingAdd(NSCoordSaturatingAdd(aBorderPadding.BStart(wm),
                                              computedBSizeLeftOver),
                         aBorderPadding.BEnd(wm));

  if (NS_FRAME_IS_NOT_COMPLETE(*aStatus)
      && aFinalSize.BSize(wm) < aReflowState.AvailableBSize()) {
    // We ran out of height on this page but we're incomplete
    // Set status to complete except for overflow
    NS_FRAME_SET_OVERFLOW_INCOMPLETE(*aStatus);
  }

  if (NS_FRAME_IS_COMPLETE(*aStatus)) {
    if (computedBSizeLeftOver > 0 &&
        NS_UNCONSTRAINEDSIZE != aReflowState.AvailableBSize() &&
        aFinalSize.BSize(wm) > aReflowState.AvailableBSize()) {
      if (ShouldAvoidBreakInside(aReflowState)) {
        *aStatus = NS_INLINE_LINE_BREAK_BEFORE();
        return;
      }
      // We don't fit and we consumed some of the computed height,
      // so we should consume all the available height and then
      // break.  If our bottom border/padding straddles the break
      // point, then this will increase our height and push the
      // border/padding to the next page/column.
      aFinalSize.BSize(wm) = std::max(aReflowState.AvailableBSize(),
                                      aContentBSize);
      NS_FRAME_SET_INCOMPLETE(*aStatus);
      if (!GetNextInFlow())
        *aStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
    }
  }
}

nsresult
nsBlockFrame::ResolveBidi()
{
  NS_ASSERTION(!GetPrevInFlow(),
               "ResolveBidi called on non-first continuation");

  nsPresContext* presContext = PresContext();
  if (!presContext->BidiEnabled()) {
    return NS_OK;
  }

  return nsBidiPresUtils::Resolve(this);
}

#ifdef DEBUG
void
nsBlockFrame::VerifyLines(bool aFinalCheckOK)
{
  if (!gVerifyLines) {
    return;
  }
  if (mLines.empty()) {
    return;
  }

  nsLineBox* cursor = GetLineCursor();

  // Add up the counts on each line. Also validate that IsFirstLine is
  // set properly.
  int32_t count = 0;
  line_iterator line, line_end;
  for (line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line) {
    if (line == cursor) {
      cursor = nullptr;
    }
    if (aFinalCheckOK) {
      NS_ABORT_IF_FALSE(line->GetChildCount(), "empty line");
      if (line->IsBlock()) {
        NS_ASSERTION(1 == line->GetChildCount(), "bad first line");
      }
    }
    count += line->GetChildCount();
  }

  // Then count the frames
  int32_t frameCount = 0;
  nsIFrame* frame = mLines.front()->mFirstChild;
  while (frame) {
    frameCount++;
    frame = frame->GetNextSibling();
  }
  NS_ASSERTION(count == frameCount, "bad line list");

  // Next: test that each line has right number of frames on it
  for (line = begin_lines(), line_end = end_lines();
       line != line_end;
        ) {
    count = line->GetChildCount();
    frame = line->mFirstChild;
    while (--count >= 0) {
      frame = frame->GetNextSibling();
    }
    ++line;
    if ((line != line_end) && (0 != line->GetChildCount())) {
      NS_ASSERTION(frame == line->mFirstChild, "bad line list");
    }
  }

  if (cursor) {
    FrameLines* overflowLines = GetOverflowLines();
    if (overflowLines) {
      line_iterator line = overflowLines->mLines.begin();
      line_iterator line_end = overflowLines->mLines.end();
      for (; line != line_end; ++line) {
        if (line == cursor) {
          cursor = nullptr;
          break;
        }
      }
    }
  }
  NS_ASSERTION(!cursor, "stale LineCursorProperty");
}

void
nsBlockFrame::VerifyOverflowSituation()
{
  nsBlockFrame* flow = static_cast<nsBlockFrame*>(FirstInFlow());
  while (flow) {
    FrameLines* overflowLines = flow->GetOverflowLines();
    if (overflowLines) {
      NS_ASSERTION(!overflowLines->mLines.empty(),
                   "should not be empty if present");
      NS_ASSERTION(overflowLines->mLines.front()->mFirstChild,
                   "bad overflow lines");
      NS_ASSERTION(overflowLines->mLines.front()->mFirstChild ==
                   overflowLines->mFrames.FirstChild(),
                   "bad overflow frames / lines");
    }
    nsLineBox* cursor = flow->GetLineCursor();
    if (cursor) {
      line_iterator line = flow->begin_lines();
      line_iterator line_end = flow->end_lines();
      for (; line != line_end && line != cursor; ++line)
        ;
      if (line == line_end && overflowLines) {
        line = overflowLines->mLines.begin();
        line_end = overflowLines->mLines.end();
        for (; line != line_end && line != cursor; ++line)
          ;
        }
      MOZ_ASSERT(line != line_end, "stale LineCursorProperty");
    }
    flow = static_cast<nsBlockFrame*>(flow->GetNextInFlow());
  }
}

int32_t
nsBlockFrame::GetDepth() const
{
  int32_t depth = 0;
  nsIFrame* parent = GetParent();
  while (parent) {
    parent = parent->GetParent();
    depth++;
  }
  return depth;
}
#endif
