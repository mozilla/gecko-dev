/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGTextFrame.h"

// Keep others in (case-insensitive) order:
#include "DOMSVGPoint.h"
#include "gfx2DGlue.h"
#include "gfxFont.h"
#include "gfxSkipChars.h"
#include "gfxTypes.h"
#include "LookAndFeel.h"
#include "mozilla/gfx/2D.h"
#include "nsAlgorithm.h"
#include "nsBlockFrame.h"
#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGLength.h"
#include "nsISelection.h"
#include "nsQuickSort.h"
#include "nsRenderingContext.h"
#include "nsSVGEffects.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsSVGPaintServerFrame.h"
#include "mozilla/dom/SVGRect.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"
#include "nsTArray.h"
#include "nsTextFrame.h"
#include "nsTextNode.h"
#include "SVGAnimatedNumberList.h"
#include "SVGContentUtils.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGPathElement.h"
#include "SVGTextPathElement.h"
#include "nsLayoutUtils.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

// ============================================================================
// Utility functions

/**
 * Using the specified gfxSkipCharsIterator, converts an offset and length
 * in original char indexes to skipped char indexes.
 *
 * @param aIterator The gfxSkipCharsIterator to use for the conversion.
 * @param aOriginalOffset The original offset (input).
 * @param aOriginalLength The original length (input).
 * @param aSkippedOffset The skipped offset (output).
 * @param aSkippedLength The skipped length (output).
 */
static void
ConvertOriginalToSkipped(gfxSkipCharsIterator& aIterator,
                         uint32_t aOriginalOffset, uint32_t aOriginalLength,
                         uint32_t& aSkippedOffset, uint32_t& aSkippedLength)
{
  aSkippedOffset = aIterator.ConvertOriginalToSkipped(aOriginalOffset);
  aIterator.AdvanceOriginal(aOriginalLength);
  aSkippedLength = aIterator.GetSkippedOffset() - aSkippedOffset;
}

/**
 * Using the specified gfxSkipCharsIterator, converts an offset and length
 * in original char indexes to skipped char indexes in place.
 *
 * @param aIterator The gfxSkipCharsIterator to use for the conversion.
 * @param aOriginalOffset The offset to convert from original to skipped.
 * @param aOriginalLength The length to convert from original to skipped.
 */
static void
ConvertOriginalToSkipped(gfxSkipCharsIterator& aIterator,
                         uint32_t& aOffset, uint32_t& aLength)
{
  ConvertOriginalToSkipped(aIterator, aOffset, aLength, aOffset, aLength);
}

/**
 * Converts an nsPoint from app units to user space units using the specified
 * nsPresContext and returns it as a gfxPoint.
 */
static gfxPoint
AppUnitsToGfxUnits(const nsPoint& aPoint, const nsPresContext* aContext)
{
  return gfxPoint(aContext->AppUnitsToGfxUnits(aPoint.x),
                  aContext->AppUnitsToGfxUnits(aPoint.y));
}

/**
 * Converts a gfxRect that is in app units to CSS pixels using the specified
 * nsPresContext and returns it as a gfxRect.
 */
static gfxRect
AppUnitsToFloatCSSPixels(const gfxRect& aRect, const nsPresContext* aContext)
{
  return gfxRect(aContext->AppUnitsToFloatCSSPixels(aRect.x),
                 aContext->AppUnitsToFloatCSSPixels(aRect.y),
                 aContext->AppUnitsToFloatCSSPixels(aRect.width),
                 aContext->AppUnitsToFloatCSSPixels(aRect.height));
}

/**
 * Scales a gfxRect around a given point.
 *
 * @param aRect The rectangle to scale.
 * @param aPoint The point around which to scale.
 * @param aScale The scale amount.
 */
static void
ScaleAround(gfxRect& aRect, const gfxPoint& aPoint, double aScale)
{
  aRect.x = aPoint.x - aScale * (aPoint.x - aRect.x);
  aRect.y = aPoint.y - aScale * (aPoint.y - aRect.y);
  aRect.width *= aScale;
  aRect.height *= aScale;
}

/**
 * Returns whether a gfxPoint lies within a gfxRect.
 */
static bool
Inside(const gfxRect& aRect, const gfxPoint& aPoint)
{
  return aPoint.x >= aRect.x &&
         aPoint.x < aRect.XMost() &&
         aPoint.y >= aRect.y &&
         aPoint.y < aRect.YMost();
}

/**
 * Gets the measured ascent and descent of the text in the given nsTextFrame
 * in app units.
 *
 * @param aFrame The text frame.
 * @param aAscent The ascent in app units (output).
 * @param aDescent The descent in app units (output).
 */
static void
GetAscentAndDescentInAppUnits(nsTextFrame* aFrame,
                              gfxFloat& aAscent, gfxFloat& aDescent)
{
  gfxSkipCharsIterator it = aFrame->EnsureTextRun(nsTextFrame::eInflated);
  gfxTextRun* textRun = aFrame->GetTextRun(nsTextFrame::eInflated);

  uint32_t offset, length;
  ConvertOriginalToSkipped(it,
                           aFrame->GetContentOffset(),
                           aFrame->GetContentLength(),
                           offset, length);

  gfxTextRun::Metrics metrics =
    textRun->MeasureText(offset, length, gfxFont::LOOSE_INK_EXTENTS, nullptr,
                         nullptr);

  aAscent = metrics.mAscent;
  aDescent = metrics.mDescent;
}

/**
 * Updates an interval by intersecting it with another interval.
 * The intervals are specified using a start index and a length.
 */
static void
IntersectInterval(uint32_t& aStart, uint32_t& aLength,
                  uint32_t aStartOther, uint32_t aLengthOther)
{
  uint32_t aEnd = aStart + aLength;
  uint32_t aEndOther = aStartOther + aLengthOther;

  if (aStartOther >= aEnd || aStart >= aEndOther) {
    aLength = 0;
  } else {
    if (aStartOther >= aStart)
      aStart = aStartOther;
    aLength = std::min(aEnd, aEndOther) - aStart;
  }
}

/**
 * Intersects an interval as IntersectInterval does but by taking
 * the offset and length of the other interval from a
 * nsTextFrame::TrimmedOffsets object.
 */
static void
TrimOffsets(uint32_t& aStart, uint32_t& aLength,
            const nsTextFrame::TrimmedOffsets& aTrimmedOffsets)
{
  IntersectInterval(aStart, aLength,
                    aTrimmedOffsets.mStart, aTrimmedOffsets.mLength);
}

/**
 * Returns the closest ancestor-or-self node that is not an SVG <a>
 * element.
 */
static nsIContent*
GetFirstNonAAncestor(nsIContent* aContent)
{
  while (aContent && aContent->IsSVG(nsGkAtoms::a)) {
    aContent = aContent->GetParent();
  }
  return aContent;
}

/**
 * Returns whether the given node is a text content element[1], taking into
 * account whether it has a valid parent.
 *
 * For example, in:
 *
 *   <svg xmlns="http://www.w3.org/2000/svg">
 *     <text><a/><text/></text>
 *     <tspan/>
 *   </svg>
 *
 * true would be returned for the outer <text> element and the <a> element,
 * and false for the inner <text> element (since a <text> is not allowed
 * to be a child of another <text>) and the <tspan> element (because it
 * must be inside a <text> subtree).
 *
 * Note that we don't support the <tref> element yet and this function
 * returns false for it.
 *
 * [1] https://svgwg.org/svg2-draft/intro.html#TermTextContentElement
 */
static bool
IsTextContentElement(nsIContent* aContent)
{
  if (!aContent->IsSVG()) {
    return false;
  }

  if (aContent->Tag() == nsGkAtoms::text) {
    nsIContent* parent = GetFirstNonAAncestor(aContent->GetParent());
    return !parent || !IsTextContentElement(parent);
  }

  if (aContent->Tag() == nsGkAtoms::textPath) {
    nsIContent* parent = GetFirstNonAAncestor(aContent->GetParent());
    return parent && parent->IsSVG(nsGkAtoms::text);
  }

  if (aContent->Tag() == nsGkAtoms::a ||
      aContent->Tag() == nsGkAtoms::tspan ||
      aContent->Tag() == nsGkAtoms::altGlyph) {
    return true;
  }

  return false;
}

/**
 * Returns whether the specified frame is an nsTextFrame that has some text
 * content.
 */
static bool
IsNonEmptyTextFrame(nsIFrame* aFrame)
{
  nsTextFrame* textFrame = do_QueryFrame(aFrame);
  if (!textFrame) {
    return false;
  }

  return textFrame->GetContentLength() != 0;
}

/**
 * Takes an nsIFrame and if it is a text frame that has some text content,
 * returns it as an nsTextFrame and its corresponding nsTextNode.
 *
 * @param aFrame The frame to look at.
 * @param aTextFrame aFrame as an nsTextFrame (output).
 * @param aTextNode The nsTextNode content of aFrame (output).
 * @return true if aFrame is a non-empty text frame, false otherwise.
 */
static bool
GetNonEmptyTextFrameAndNode(nsIFrame* aFrame,
                            nsTextFrame*& aTextFrame,
                            nsTextNode*& aTextNode)
{
  nsTextFrame* text = do_QueryFrame(aFrame);
  if (!text) {
    return false;
  }

  nsIContent* content = text->GetContent();
  NS_ASSERTION(content && content->IsNodeOfType(nsINode::eTEXT),
               "unexpected content type for nsTextFrame");

  nsTextNode* node = static_cast<nsTextNode*>(content);
  if (node->TextLength() == 0) {
    return false;
  }

  aTextFrame = text;
  aTextNode = node;
  return true;
}

/**
 * Returns whether the specified atom is for one of the five
 * glyph positioning attributes that can appear on SVG text
 * elements -- x, y, dx, dy or rotate.
 */
static bool
IsGlyphPositioningAttribute(nsIAtom* aAttribute)
{
  return aAttribute == nsGkAtoms::x ||
         aAttribute == nsGkAtoms::y ||
         aAttribute == nsGkAtoms::dx ||
         aAttribute == nsGkAtoms::dy ||
         aAttribute == nsGkAtoms::rotate;
}

/**
 * Returns the position in app units of a given baseline (using an
 * SVG dominant-baseline property value) for a given nsTextFrame.
 *
 * @param aFrame The text frame to inspect.
 * @param aTextRun The text run of aFrame.
 * @param aDominantBaseline The dominant-baseline value to use.
 */
static nscoord
GetBaselinePosition(nsTextFrame* aFrame,
                    gfxTextRun* aTextRun,
                    uint8_t aDominantBaseline)
{
  // use a dummy WritingMode, because nsTextFrame::GetLogicalBaseLine
  // doesn't use it anyway
  WritingMode writingMode;
  switch (aDominantBaseline) {
    case NS_STYLE_DOMINANT_BASELINE_HANGING:
    case NS_STYLE_DOMINANT_BASELINE_TEXT_BEFORE_EDGE:
      return 0;
    case NS_STYLE_DOMINANT_BASELINE_USE_SCRIPT:
    case NS_STYLE_DOMINANT_BASELINE_NO_CHANGE:
    case NS_STYLE_DOMINANT_BASELINE_RESET_SIZE:
      // These three should not simply map to 'baseline', but we don't
      // support the complex baseline model that SVG 1.1 has and which
      // css3-linebox now defines.
      // (fall through)
    case NS_STYLE_DOMINANT_BASELINE_AUTO:
    case NS_STYLE_DOMINANT_BASELINE_ALPHABETIC:
      return aFrame->GetLogicalBaseline(writingMode);
  }

  gfxTextRun::Metrics metrics =
    aTextRun->MeasureText(0, aTextRun->GetLength(), gfxFont::LOOSE_INK_EXTENTS,
                          nullptr, nullptr);

  switch (aDominantBaseline) {
    case NS_STYLE_DOMINANT_BASELINE_TEXT_AFTER_EDGE:
    case NS_STYLE_DOMINANT_BASELINE_IDEOGRAPHIC:
      return metrics.mAscent + metrics.mDescent;
    case NS_STYLE_DOMINANT_BASELINE_CENTRAL:
    case NS_STYLE_DOMINANT_BASELINE_MIDDLE:
    case NS_STYLE_DOMINANT_BASELINE_MATHEMATICAL:
      return (metrics.mAscent + metrics.mDescent) / 2.0;
  }

  NS_NOTREACHED("unexpected dominant-baseline value");
  return aFrame->GetLogicalBaseline(writingMode);
}

/**
 * For a given text run, returns the number of skipped characters that comprise
 * the ligature group and/or cluster that includes the character represented
 * by the specified gfxSkipCharsIterator.
 *
 * @param aTextRun The text run to use for determining whether a given character
 *   is part of a ligature or cluster.
 * @param aIterator The gfxSkipCharsIterator to use for the current position
 *   in the text run.
 */
static uint32_t
ClusterLength(gfxTextRun* aTextRun, const gfxSkipCharsIterator& aIterator)
{
  uint32_t start = aIterator.GetSkippedOffset();
  uint32_t end = start + 1;
  while (end < aTextRun->GetLength() &&
         (!aTextRun->IsLigatureGroupStart(end) ||
          !aTextRun->IsClusterStart(end))) {
    end++;
  }
  return end - start;
}

/**
 * Truncates an array to be at most the length of another array.
 *
 * @param aArrayToTruncate The array to truncate.
 * @param aReferenceArray The array whose length will be used to truncate
 *   aArrayToTruncate to.
 */
template<typename T, typename U>
static void
TruncateTo(nsTArray<T>& aArrayToTruncate, const nsTArray<U>& aReferenceArray)
{
  uint32_t length = aReferenceArray.Length();
  if (aArrayToTruncate.Length() > length) {
    aArrayToTruncate.TruncateLength(length);
  }
}

/**
 * Asserts that the anonymous block child of the SVGTextFrame has been
 * reflowed (or does not exist).  Returns null if the child has not been
 * reflowed, and the frame otherwise.
 *
 * We check whether the kid has been reflowed and not the frame itself
 * since we sometimes need to call this function during reflow, after the
 * kid has been reflowed but before we have cleared the dirty bits on the
 * frame itself.
 */
static SVGTextFrame*
FrameIfAnonymousChildReflowed(SVGTextFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "aFrame must not be null");
  nsIFrame* kid = aFrame->GetFirstPrincipalChild();
  if (NS_SUBTREE_DIRTY(kid)) {
    MOZ_ASSERT(false, "should have already reflowed the anonymous block child");
    return nullptr;
  }
  return aFrame;
}

static double
GetContextScale(const gfxMatrix& aMatrix)
{
  // The context scale is the ratio of the length of the transformed
  // diagonal vector (1,1) to the length of the untransformed diagonal
  // (which is sqrt(2)).
  gfxPoint p = aMatrix.Transform(gfxPoint(1, 1)) -
               aMatrix.Transform(gfxPoint(0, 0));
  return SVGContentUtils::ComputeNormalizedHypotenuse(p.x, p.y);
}

// ============================================================================
// Utility classes

namespace mozilla {

// ----------------------------------------------------------------------------
// TextRenderedRun

/**
 * A run of text within a single nsTextFrame whose glyphs can all be painted
 * with a single call to nsTextFrame::PaintText.  A text rendered run can
 * be created for a sequence of two or more consecutive glyphs as long as:
 *
 *   - Only the first glyph has (or none of the glyphs have) been positioned
 *     with SVG text positioning attributes
 *   - All of the glyphs have zero rotation
 *   - The glyphs are not on a text path
 *   - The glyphs correspond to content within the one nsTextFrame
 *
 * A TextRenderedRunIterator produces TextRenderedRuns required for painting a
 * whole SVGTextFrame.
 */
struct TextRenderedRun
{
  /**
   * Constructs a TextRenderedRun that is uninitialized except for mFrame
   * being null.
   */
  TextRenderedRun()
    : mFrame(nullptr)
  {
  }

  /**
   * Constructs a TextRenderedRun with all of the information required to
   * paint it.  See the comments documenting the member variables below
   * for descriptions of the arguments.
   */
  TextRenderedRun(nsTextFrame* aFrame, const gfxPoint& aPosition,
                  float aLengthAdjustScaleFactor, double aRotate,
                  float aFontSizeScaleFactor, nscoord aBaseline,
                  uint32_t aTextFrameContentOffset,
                  uint32_t aTextFrameContentLength,
                  uint32_t aTextElementCharIndex)
    : mFrame(aFrame),
      mPosition(aPosition),
      mLengthAdjustScaleFactor(aLengthAdjustScaleFactor),
      mRotate(static_cast<float>(aRotate)),
      mFontSizeScaleFactor(aFontSizeScaleFactor),
      mBaseline(aBaseline),
      mTextFrameContentOffset(aTextFrameContentOffset),
      mTextFrameContentLength(aTextFrameContentLength),
      mTextElementCharIndex(aTextElementCharIndex)
  {
  }

  /**
   * Returns the text run for the text frame that this rendered run is part of.
   */
  gfxTextRun* GetTextRun() const
  {
    mFrame->EnsureTextRun(nsTextFrame::eInflated);
    return mFrame->GetTextRun(nsTextFrame::eInflated);
  }

  /**
   * Returns whether this rendered run is RTL.
   */
  bool IsRightToLeft() const
  {
    return GetTextRun()->IsRightToLeft();
  }

  /**
   * Returns the transform that converts from a <text> element's user space into
   * the coordinate space that rendered runs can be painted directly in.
   *
   * The difference between this method and GetTransformFromRunUserSpaceToUserSpace
   * is that when calling in to nsTextFrame::PaintText, it will already take
   * into account any left clip edge (that is, it doesn't just apply a visual
   * clip to the rendered text, it shifts the glyphs over so that they are
   * painted with their left edge at the x coordinate passed in to it).
   * Thus we need to account for this in our transform.
   *
   *
   * Assume that we have <text x="100" y="100" rotate="0 0 1 0 0 1">abcdef</text>.
   * This would result in four text rendered runs:
   *
   *   - one for "ab"
   *   - one for "c"
   *   - one for "de"
   *   - one for "f"
   *
   * Assume now that we are painting the third TextRenderedRun.  It will have
   * a left clip edge that is the sum of the advances of "abc", and it will
   * have a right clip edge that is the advance of "f".  In
   * SVGTextFrame::PaintSVG(), we pass in nsPoint() (i.e., the origin)
   * as the point at which to paint the text frame, and we pass in the
   * clip edge values.  The nsTextFrame will paint the substring of its
   * text such that the top-left corner of the "d"'s glyph cell will be at
   * (0, 0) in the current coordinate system.
   *
   * Thus, GetTransformFromUserSpaceForPainting must return a transform from
   * whatever user space the <text> element is in to a coordinate space in
   * device pixels (as that's what nsTextFrame works in) where the origin is at
   * the same position as our user space mPositions[i].mPosition value for
   * the "d" glyph, which will be (100 + userSpaceAdvance("abc"), 100).
   * The translation required to do this (ignoring the scale to get from
   * user space to device pixels, and ignoring the
   * (100 + userSpaceAdvance("abc"), 100) translation) is:
   *
   *   (-leftEdge, -baseline)
   *
   * where baseline is the distance between the baseline of the text and the top
   * edge of the nsTextFrame.  We translate by -leftEdge horizontally because
   * the nsTextFrame will already shift the glyphs over by that amount and start
   * painting glyphs at x = 0.  We translate by -baseline vertically so that
   * painting the top edges of the glyphs at y = 0 will result in their
   * baselines being at our desired y position.
   *
   *
   * Now for an example with RTL text.  Assume our content is now
   * <text x="100" y="100" rotate="0 0 1 0 0 1">WERBEH</text>.  We'd have
   * the following text rendered runs:
   *
   *   - one for "EH"
   *   - one for "B"
   *   - one for "ER"
   *   - one for "W"
   *
   * Again, we are painting the third TextRenderedRun.  The left clip edge
   * is the advance of the "W" and the right clip edge is the sum of the
   * advances of "BEH".  Our translation to get the rendered "ER" glyphs
   * in the right place this time is:
   *
   *   (-frameWidth + rightEdge, -baseline)
   *
   * which is equivalent to:
   *
   *   (-(leftEdge + advance("ER")), -baseline)
   *
   * The reason we have to shift left additionally by the width of the run
   * of glyphs we are painting is that although the nsTextFrame is RTL,
   * we still supply the top-left corner to paint the frame at when calling
   * nsTextFrame::PaintText, even though our user space positions for each
   * glyph in mPositions specifies the origin of each glyph, which for RTL
   * glyphs is at the right edge of the glyph cell.
   *
   *
   * For any other use of an nsTextFrame in the context of a particular run
   * (such as hit testing, or getting its rectangle),
   * GetTransformFromRunUserSpaceToUserSpace should be used.
   *
   * @param aContext The context to use for unit conversions.
   * @param aItem The nsCharClipDisplayItem that holds the amount of clipping
   *   from the left and right edges of the text frame for this rendered run.
   *   An appropriate nsCharClipDisplayItem can be obtained by constructing an
   *   SVGCharClipDisplayItem for the TextRenderedRun.
   */
  gfxMatrix GetTransformFromUserSpaceForPainting(
                                      nsPresContext* aContext,
                                      const nsCharClipDisplayItem& aItem) const;

  /**
   * Returns the transform that converts from "run user space" to a <text>
   * element's user space.  Run user space is a coordinate system that has the
   * same size as the <text>'s user space but rotated and translated such that
   * (0,0) is the top-left of the rectangle that bounds the text.
   *
   * @param aContext The context to use for unit conversions.
   */
  gfxMatrix GetTransformFromRunUserSpaceToUserSpace(nsPresContext* aContext) const;

  /**
   * Returns the transform that converts from "run user space" to float pixels
   * relative to the nsTextFrame that this rendered run is a part of.
   *
   * @param aContext The context to use for unit conversions.
   */
  gfxMatrix GetTransformFromRunUserSpaceToFrameUserSpace(nsPresContext* aContext) const;

  /**
   * Flag values used for the aFlags arguments of GetRunUserSpaceRect,
   * GetFrameUserSpaceRect and GetUserSpaceRect.
   */
  enum {
    // Includes the fill geometry of the text in the returned rectangle.
    eIncludeFill = 1,
    // Includes the stroke geometry of the text in the returned rectangle.
    eIncludeStroke = 2,
    // Includes any text shadow in the returned rectangle.
    eIncludeTextShadow = 4,
    // Don't include any horizontal glyph overflow in the returned rectangle.
    eNoHorizontalOverflow = 8
  };

  /**
   * Returns a rectangle that bounds the fill and/or stroke of the rendered run
   * in run user space.
   *
   * @param aContext The context to use for unit conversions.
   * @param aFlags A combination of the flags above (eIncludeFill and
   *   eIncludeStroke) indicating what parts of the text to include in
   *   the rectangle.
   */
  SVGBBox GetRunUserSpaceRect(nsPresContext* aContext, uint32_t aFlags) const;

  /**
   * Returns a rectangle that covers the fill and/or stroke of the rendered run
   * in "frame user space".
   *
   * Frame user space is a coordinate space of the same scale as the <text>
   * element's user space, but with its rotation set to the rotation of
   * the glyphs within this rendered run and its origin set to the position
   * such that placing the nsTextFrame there would result in the glyphs in
   * this rendered run being at their correct positions.
   *
   * For example, say we have <text x="100 150" y="100">ab</text>.  Assume
   * the advance of both the "a" and the "b" is 12 user units, and the
   * ascent of the text is 8 user units and its descent is 6 user units,
   * and that we are not measuing the stroke of the text, so that we stay
   * entirely within the glyph cells.
   *
   * There will be two text rendered runs, one for "a" and one for "b".
   *
   * The frame user space for the "a" run will have its origin at
   * (100, 100 - 8) in the <text> element's user space and will have its
   * axes aligned with the user space (since there is no rotate="" or
   * text path involve) and with its scale the same as the user space.
   * The rect returned by this method will be (0, 0, 12, 14), since the "a"
   * glyph is right at the left of the nsTextFrame.
   *
   * The frame user space for the "b" run will have its origin at
   * (150 - 12, 100 - 8), and scale/rotation the same as above.  The rect
   * returned by this method will be (12, 0, 12, 14), since we are
   * advance("a") horizontally in to the text frame.
   *
   * @param aContext The context to use for unit conversions.
   * @param aFlags A combination of the flags above (eIncludeFill and
   *   eIncludeStroke) indicating what parts of the text to include in
   *   the rectangle.
   */
  SVGBBox GetFrameUserSpaceRect(nsPresContext* aContext, uint32_t aFlags) const;

  /**
   * Returns a rectangle that covers the fill and/or stroke of the rendered run
   * in the <text> element's user space.
   *
   * @param aContext The context to use for unit conversions.
   * @param aFlags A combination of the flags above indicating what parts of the
   *   text to include in the rectangle.
   * @param aAdditionalTransform An additional transform to apply to the
   *   frame user space rectangle before its bounds are transformed into
   *   user space.
   */
  SVGBBox GetUserSpaceRect(nsPresContext* aContext, uint32_t aFlags,
                           const gfxMatrix* aAdditionalTransform = nullptr) const;

  /**
   * Gets the app unit amounts to clip from the left and right edges of
   * the nsTextFrame in order to paint just this rendered run.
   *
   * Note that if clip edge amounts land in the middle of a glyph, the
   * glyph won't be painted at all.  The clip edges are thus more of
   * a selection mechanism for which glyphs will be painted, rather
   * than a geometric clip.
   */
  void GetClipEdges(nscoord& aLeftEdge, nscoord& aRightEdge) const;

  /**
   * Returns the advance width of the whole rendered run.
   */
  nscoord GetAdvanceWidth() const;

  /**
   * Returns the index of the character into this rendered run whose
   * glyph cell contains the given point, or -1 if there is no such
   * character.  This does not hit test against any overflow.
   *
   * @param aContext The context to use for unit conversions.
   * @param aPoint The point in the user space of the <text> element.
   */
  int32_t GetCharNumAtPosition(nsPresContext* aContext,
                               const gfxPoint& aPoint) const;

  /**
   * The text frame that this rendered run lies within.
   */
  nsTextFrame* mFrame;

  /**
   * The point in user space that the text is positioned at.
   *
   * The x coordinate is the left edge of a LTR run of text or the right edge of
   * an RTL run.  The y coordinate is the baseline of the text.
   */
  gfxPoint mPosition;

  /**
   * The horizontal scale factor to apply when painting glyphs to take
   * into account textLength="".
   */
  float mLengthAdjustScaleFactor;

  /**
   * The rotation in radians in the user coordinate system that the text has.
   */
  float mRotate;

  /**
   * The scale factor that was used to transform the text run's original font
   * size into a sane range for painting and measurement.
   */
  double mFontSizeScaleFactor;

  /**
   * The baseline in app units of this text run.  The measurement is from the
   * top of the text frame.
   */
  nscoord mBaseline;

  /**
   * The offset and length in mFrame's content nsTextNode that corresponds to
   * this text rendered run.  These are original char indexes.
   */
  uint32_t mTextFrameContentOffset;
  uint32_t mTextFrameContentLength;

  /**
   * The character index in the whole SVG <text> element that this text rendered
   * run begins at.
   */
  uint32_t mTextElementCharIndex;
};

gfxMatrix
TextRenderedRun::GetTransformFromUserSpaceForPainting(
                                       nsPresContext* aContext,
                                       const nsCharClipDisplayItem& aItem) const
{
  // We transform to device pixels positioned such that painting the text frame
  // at (0,0) with aItem will result in the text being in the right place.

  gfxMatrix m;
  if (!mFrame) {
    return m;
  }

  float cssPxPerDevPx = aContext->
    AppUnitsToFloatCSSPixels(aContext->AppUnitsPerDevPixel());

  // Glyph position in user space.
  m.Translate(mPosition / cssPxPerDevPx);

  // Take into account any font size scaling and scaling due to textLength="".
  m.Scale(1.0 / mFontSizeScaleFactor, 1.0 / mFontSizeScaleFactor);

  // Rotation due to rotate="" or a <textPath>.
  m.Rotate(mRotate);

  m.Scale(mLengthAdjustScaleFactor, 1.0);

  // Translation to get the text frame in the right place.
  nsPoint t(IsRightToLeft() ?
              -mFrame->GetRect().width + aItem.mRightEdge :
              -aItem.mLeftEdge,
            -mBaseline);
  m.Translate(AppUnitsToGfxUnits(t, aContext));

  return m;
}

gfxMatrix
TextRenderedRun::GetTransformFromRunUserSpaceToUserSpace(
                                                  nsPresContext* aContext) const
{
  gfxMatrix m;
  if (!mFrame) {
    return m;
  }

  float cssPxPerDevPx = aContext->
    AppUnitsToFloatCSSPixels(aContext->AppUnitsPerDevPixel());

  nscoord left, right;
  GetClipEdges(left, right);

  // Glyph position in user space.
  m.Translate(mPosition);

  // Rotation due to rotate="" or a <textPath>.
  m.Rotate(mRotate);

  // Scale due to textLength="".
  m.Scale(mLengthAdjustScaleFactor, 1.0);

  // Translation to get the text frame in the right place.
  nsPoint t(IsRightToLeft() ?
              -mFrame->GetRect().width + left + right :
              0,
            -mBaseline);
  m.Translate(AppUnitsToGfxUnits(t, aContext) *
                cssPxPerDevPx / mFontSizeScaleFactor);

  return m;
}

gfxMatrix
TextRenderedRun::GetTransformFromRunUserSpaceToFrameUserSpace(
                                                  nsPresContext* aContext) const
{
  gfxMatrix m;
  if (!mFrame) {
    return m;
  }

  nscoord left, right;
  GetClipEdges(left, right);

  // Translate by the horizontal distance into the text frame this
  // rendered run is.
  return m.Translate(gfxPoint(gfxFloat(left) / aContext->AppUnitsPerCSSPixel(),
                              0));
}

SVGBBox
TextRenderedRun::GetRunUserSpaceRect(nsPresContext* aContext,
                                     uint32_t aFlags) const
{
  SVGBBox r;
  if (!mFrame) {
    return r;
  }

  // Determine the amount of overflow above and below the frame's mRect.
  //
  // We need to call GetVisualOverflowRectRelativeToSelf because this includes
  // overflowing decorations, which the MeasureText call below does not.  We
  // assume here the decorations only overflow above and below the frame, never
  // horizontally.
  nsRect self = mFrame->GetVisualOverflowRectRelativeToSelf();
  nsRect rect = mFrame->GetRect();
  nscoord above = -self.y;
  nscoord below = self.YMost() - rect.height;

  gfxSkipCharsIterator it = mFrame->EnsureTextRun(nsTextFrame::eInflated);
  gfxTextRun* textRun = mFrame->GetTextRun(nsTextFrame::eInflated);

  // Get the content range for this rendered run.
  uint32_t offset, length;
  ConvertOriginalToSkipped(it, mTextFrameContentOffset, mTextFrameContentLength,
                           offset, length);

  // Measure that range.
  gfxTextRun::Metrics metrics =
    textRun->MeasureText(offset, length, gfxFont::LOOSE_INK_EXTENTS,
                         nullptr, nullptr);

  // Determine the rectangle that covers the rendered run's fill,
  // taking into account the measured vertical overflow due to
  // decorations.
  nscoord baseline = metrics.mBoundingBox.y + metrics.mAscent;
  gfxFloat x, width;
  if (aFlags & eNoHorizontalOverflow) {
    x = 0.0;
    width = textRun->GetAdvanceWidth(offset, length, nullptr);
  } else {
    x = metrics.mBoundingBox.x;
    width = metrics.mBoundingBox.width;
  }
  nsRect fillInAppUnits(x, baseline - above,
                        width, metrics.mBoundingBox.height + above + below);

  // Account for text-shadow.
  if (aFlags & eIncludeTextShadow) {
    fillInAppUnits =
      nsLayoutUtils::GetTextShadowRectsUnion(fillInAppUnits, mFrame);
  }

  // Convert the app units rectangle to user units.
  gfxRect fill = AppUnitsToFloatCSSPixels(gfxRect(fillInAppUnits.x,
                                                  fillInAppUnits.y,
                                                  fillInAppUnits.width,
                                                  fillInAppUnits.height),
                                          aContext);

  // Scale the rectangle up due to any mFontSizeScaleFactor.  We scale
  // it around the text's origin.
  ScaleAround(fill,
              gfxPoint(0.0, aContext->AppUnitsToFloatCSSPixels(baseline)),
              1.0 / mFontSizeScaleFactor);

  // Include the fill if requested.
  if (aFlags & eIncludeFill) {
    r = fill;
  }

  // Include the stroke if requested.
  if ((aFlags & eIncludeStroke) &&
      nsSVGUtils::GetStrokeWidth(mFrame) > 0) {
    r.UnionEdges(nsSVGUtils::PathExtentsToMaxStrokeExtents(fill, mFrame,
                                                           gfxMatrix()));
  }

  return r;
}

SVGBBox
TextRenderedRun::GetFrameUserSpaceRect(nsPresContext* aContext,
                                       uint32_t aFlags) const
{
  SVGBBox r = GetRunUserSpaceRect(aContext, aFlags);
  if (r.IsEmpty()) {
    return r;
  }
  gfxMatrix m = GetTransformFromRunUserSpaceToFrameUserSpace(aContext);
  return m.TransformBounds(r.ToThebesRect());
}

SVGBBox
TextRenderedRun::GetUserSpaceRect(nsPresContext* aContext,
                                  uint32_t aFlags,
                                  const gfxMatrix* aAdditionalTransform) const
{
  SVGBBox r = GetRunUserSpaceRect(aContext, aFlags);
  if (r.IsEmpty()) {
    return r;
  }
  gfxMatrix m = GetTransformFromRunUserSpaceToUserSpace(aContext);
  if (aAdditionalTransform) {
    m.Multiply(*aAdditionalTransform);
  }
  return m.TransformBounds(r.ToThebesRect());
}

void
TextRenderedRun::GetClipEdges(nscoord& aLeftEdge, nscoord& aRightEdge) const
{
  uint32_t contentLength = mFrame->GetContentLength();
  if (mTextFrameContentOffset == 0 &&
      mTextFrameContentLength == contentLength) {
    // If the rendered run covers the entire content, we know we don't need
    // to clip without having to measure anything.
    aLeftEdge = 0;
    aRightEdge = 0;
    return;
  }

  gfxSkipCharsIterator it = mFrame->EnsureTextRun(nsTextFrame::eInflated);
  gfxTextRun* textRun = mFrame->GetTextRun(nsTextFrame::eInflated);

  // Get the covered content offset/length for this rendered run in skipped
  // characters, since that is what GetAdvanceWidth expects.
  uint32_t runOffset, runLength, frameOffset, frameLength;
  ConvertOriginalToSkipped(it, mTextFrameContentOffset, mTextFrameContentLength,
                               runOffset, runLength);

  // Get the offset/length of the whole nsTextFrame.
  frameOffset = mFrame->GetContentOffset();
  frameLength = mFrame->GetContentLength();

  // Trim the whole-nsTextFrame offset/length to remove any leading/trailing
  // white space, as the nsTextFrame when painting does not include them when
  // interpreting clip edges.
  nsTextFrame::TrimmedOffsets trimmedOffsets =
    mFrame->GetTrimmedOffsets(mFrame->GetContent()->GetText(), true);
  TrimOffsets(frameOffset, frameLength, trimmedOffsets);

  // Convert the trimmed whole-nsTextFrame offset/length into skipped
  // characters.
  ConvertOriginalToSkipped(it, frameOffset, frameLength);

  // Measure the advance width in the text run between the start of
  // frame's content and the start of the rendered run's content,
  nscoord leftEdge =
    textRun->GetAdvanceWidth(frameOffset, runOffset - frameOffset, nullptr);

  // and between the end of the rendered run's content and the end
  // of the frame's content.
  nscoord rightEdge =
    textRun->GetAdvanceWidth(runOffset + runLength,
                             frameOffset + frameLength - (runOffset + runLength),
                             nullptr);

  if (textRun->IsRightToLeft()) {
    aLeftEdge  = rightEdge;
    aRightEdge = leftEdge;
  } else {
    aLeftEdge  = leftEdge;
    aRightEdge = rightEdge;
  }
}

nscoord
TextRenderedRun::GetAdvanceWidth() const
{
  gfxSkipCharsIterator it = mFrame->EnsureTextRun(nsTextFrame::eInflated);
  gfxTextRun* textRun = mFrame->GetTextRun(nsTextFrame::eInflated);

  uint32_t offset, length;
  ConvertOriginalToSkipped(it, mTextFrameContentOffset, mTextFrameContentLength,
                           offset, length);

  return textRun->GetAdvanceWidth(offset, length, nullptr);
}

int32_t
TextRenderedRun::GetCharNumAtPosition(nsPresContext* aContext,
                                      const gfxPoint& aPoint) const
{
  if (mTextFrameContentLength == 0) {
    return -1;
  }

  float cssPxPerDevPx = aContext->
    AppUnitsToFloatCSSPixels(aContext->AppUnitsPerDevPixel());

  // Convert the point from user space into run user space, and take
  // into account any mFontSizeScaleFactor.
  gfxMatrix m = GetTransformFromRunUserSpaceToUserSpace(aContext).Invert();
  gfxPoint p = m.Transform(aPoint) / cssPxPerDevPx * mFontSizeScaleFactor;

  // First check that the point lies vertically between the top and bottom
  // edges of the text.
  gfxFloat ascent, descent;
  GetAscentAndDescentInAppUnits(mFrame, ascent, descent);

  gfxFloat topEdge = mFrame->GetLogicalBaseline(mFrame->GetWritingMode()) - ascent;
  gfxFloat bottomEdge = topEdge + ascent + descent;

  if (p.y < aContext->AppUnitsToGfxUnits(topEdge) ||
      p.y >= aContext->AppUnitsToGfxUnits(bottomEdge)) {
    return -1;
  }

  gfxSkipCharsIterator it = mFrame->EnsureTextRun(nsTextFrame::eInflated);
  gfxTextRun* textRun = mFrame->GetTextRun(nsTextFrame::eInflated);

  // Next check that the point lies horizontally within the left and right
  // edges of the text.
  uint32_t offset, length;
  ConvertOriginalToSkipped(it, mTextFrameContentOffset, mTextFrameContentLength,
                           offset, length);
  gfxFloat runAdvance =
    aContext->AppUnitsToGfxUnits(textRun->GetAdvanceWidth(offset, length,
                                                          nullptr));

  if (p.x < 0 || p.x >= runAdvance) {
    return -1;
  }

  // Finally, measure progressively smaller portions of the rendered run to
  // find which glyph it lies within.  This will need to change once we
  // support letter-spacing and word-spacing.
  bool rtl = textRun->IsRightToLeft();
  for (int32_t i = mTextFrameContentLength - 1; i >= 0; i--) {
    ConvertOriginalToSkipped(it, mTextFrameContentOffset, i, offset, length);
    gfxFloat advance =
      aContext->AppUnitsToGfxUnits(textRun->GetAdvanceWidth(offset, length,
                                                            nullptr));
    if ((rtl && p.x < runAdvance - advance) ||
        (!rtl && p.x >= advance)) {
      return i;
    }
  }
  return -1;
}

// ----------------------------------------------------------------------------
// TextNodeIterator

enum SubtreePosition
{
  eBeforeSubtree,
  eWithinSubtree,
  eAfterSubtree
};

/**
 * An iterator class for nsTextNodes that are descendants of a given node, the
 * root.  Nodes are iterated in document order.  An optional subtree can be
 * specified, in which case the iterator will track whether the current state of
 * the traversal over the tree is within that subtree or is past that subtree.
 */
class TextNodeIterator
{
public:
  /**
   * Constructs a TextNodeIterator with the specified root node and optional
   * subtree.
   */
  TextNodeIterator(nsIContent* aRoot, nsIContent* aSubtree = nullptr)
    : mRoot(aRoot),
      mSubtree(aSubtree == aRoot ? nullptr : aSubtree),
      mCurrent(aRoot),
      mSubtreePosition(mSubtree ? eBeforeSubtree : eWithinSubtree)
  {
    NS_ASSERTION(aRoot, "expected non-null root");
    if (!aRoot->IsNodeOfType(nsINode::eTEXT)) {
      Next();
    }
  }

  /**
   * Returns the current nsTextNode, or null if the iterator has finished.
   */
  nsTextNode* Current() const
  {
    return static_cast<nsTextNode*>(mCurrent);
  }

  /**
   * Advances to the next nsTextNode and returns it, or null if the end of
   * iteration has been reached.
   */
  nsTextNode* Next();

  /**
   * Returns whether the iterator is currently within the subtree rooted
   * at mSubtree.  Returns true if we are not tracking a subtree (we consider
   * that we're always within the subtree).
   */
  bool IsWithinSubtree() const
  {
    return mSubtreePosition == eWithinSubtree;
  }

  /**
   * Returns whether the iterator is past the subtree rooted at mSubtree.
   * Returns false if we are not tracking a subtree.
   */
  bool IsAfterSubtree() const
  {
    return mSubtreePosition == eAfterSubtree;
  }

private:
  /**
   * The root under which all nsTextNodes will be iterated over.
   */
  nsIContent* mRoot;

  /**
   * The node rooting the subtree to track.
   */
  nsIContent* mSubtree;

  /**
   * The current node during iteration.
   */
  nsIContent* mCurrent;

  /**
   * The current iterator position relative to mSubtree.
   */
  SubtreePosition mSubtreePosition;
};

nsTextNode*
TextNodeIterator::Next()
{
  // Starting from mCurrent, we do a non-recursive traversal to the next
  // nsTextNode beneath mRoot, updating mSubtreePosition appropriately if we
  // encounter mSubtree.
  if (mCurrent) {
    do {
      nsIContent* next = IsTextContentElement(mCurrent) ?
                           mCurrent->GetFirstChild() :
                           nullptr;
      if (next) {
        mCurrent = next;
        if (mCurrent == mSubtree) {
          mSubtreePosition = eWithinSubtree;
        }
      } else {
        for (;;) {
          if (mCurrent == mRoot) {
            mCurrent = nullptr;
            break;
          }
          if (mCurrent == mSubtree) {
            mSubtreePosition = eAfterSubtree;
          }
          next = mCurrent->GetNextSibling();
          if (next) {
            mCurrent = next;
            if (mCurrent == mSubtree) {
              mSubtreePosition = eWithinSubtree;
            }
            break;
          }
          if (mCurrent == mSubtree) {
            mSubtreePosition = eAfterSubtree;
          }
          mCurrent = mCurrent->GetParent();
        }
      }
    } while (mCurrent && !mCurrent->IsNodeOfType(nsINode::eTEXT));
  }

  return static_cast<nsTextNode*>(mCurrent);
}

// ----------------------------------------------------------------------------
// TextNodeCorrespondenceRecorder

/**
 * TextNodeCorrespondence is used as the value of a frame property that
 * is stored on all its descendant nsTextFrames.  It stores the number of DOM
 * characters between it and the previous nsTextFrame that did not have an
 * nsTextFrame created for them, due to either not being in a correctly
 * parented text content element, or because they were display:none.
 * These are called "undisplayed characters".
 *
 * See also TextNodeCorrespondenceRecorder below, which is what sets the
 * frame property.
 */
struct TextNodeCorrespondence
{
  TextNodeCorrespondence(uint32_t aUndisplayedCharacters)
    : mUndisplayedCharacters(aUndisplayedCharacters)
  {
  }

  uint32_t mUndisplayedCharacters;
};

static void DestroyTextNodeCorrespondence(void* aPropertyValue)
{
  delete static_cast<TextNodeCorrespondence*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(TextNodeCorrespondenceProperty, DestroyTextNodeCorrespondence)

/**
 * Returns the number of undisplayed characters before the specified
 * nsTextFrame.
 */
static uint32_t
GetUndisplayedCharactersBeforeFrame(nsTextFrame* aFrame)
{
  void* value = aFrame->Properties().Get(TextNodeCorrespondenceProperty());
  TextNodeCorrespondence* correspondence =
    static_cast<TextNodeCorrespondence*>(value);
  if (!correspondence) {
    NS_NOTREACHED("expected a TextNodeCorrespondenceProperty on nsTextFrame "
                  "used for SVG text");
    return 0;
  }
  return correspondence->mUndisplayedCharacters;
}

/**
 * Traverses the nsTextFrames for an SVGTextFrame and records a
 * TextNodeCorrespondenceProperty on each for the number of undisplayed DOM
 * characters between each frame.  This is done by iterating simultaenously
 * over the nsTextNodes and nsTextFrames and noting when nsTextNodes (or
 * parts of them) are skipped when finding the next nsTextFrame.
 */
class TextNodeCorrespondenceRecorder
{
public:
  /**
   * Entry point for the TextNodeCorrespondenceProperty recording.
   */
  static void RecordCorrespondence(SVGTextFrame* aRoot);

private:
  TextNodeCorrespondenceRecorder(SVGTextFrame* aRoot)
    : mNodeIterator(aRoot->GetContent()),
      mPreviousNode(nullptr),
      mNodeCharIndex(0)
  {
  }

  void Record(SVGTextFrame* aRoot);
  void TraverseAndRecord(nsIFrame* aFrame);

  /**
   * Returns the next non-empty nsTextNode.
   */
  nsTextNode* NextNode();

  /**
   * The iterator over the nsTextNodes that we use as we simultaneously
   * iterate over the nsTextFrames.
   */
  TextNodeIterator mNodeIterator;

  /**
   * The previous nsTextNode we iterated over.
   */
  nsTextNode* mPreviousNode;

  /**
   * The index into the current nsTextNode's character content.
   */
  uint32_t mNodeCharIndex;
};

/* static */ void
TextNodeCorrespondenceRecorder::RecordCorrespondence(SVGTextFrame* aRoot)
{
  TextNodeCorrespondenceRecorder recorder(aRoot);
  recorder.Record(aRoot);
}

void
TextNodeCorrespondenceRecorder::Record(SVGTextFrame* aRoot)
{
  if (!mNodeIterator.Current()) {
    // If there are no nsTextNodes then there is nothing to do.
    return;
  }

  // Traverse over all the nsTextFrames and record the number of undisplayed
  // characters.
  TraverseAndRecord(aRoot);

  // Find how many undisplayed characters there are after the final nsTextFrame.
  uint32_t undisplayed = 0;
  if (mNodeIterator.Current()) {
    if (mPreviousNode && mPreviousNode->TextLength() != mNodeCharIndex) {
      // The last nsTextFrame ended part way through an nsTextNode.  The
      // remaining characters count as undisplayed.
      NS_ASSERTION(mNodeCharIndex < mPreviousNode->TextLength(),
                   "incorrect tracking of undisplayed characters in "
                   "text nodes");
      undisplayed += mPreviousNode->TextLength() - mNodeCharIndex;
    }
    // All the remaining nsTextNodes that we iterate must also be undisplayed.
    for (nsTextNode* textNode = mNodeIterator.Current();
         textNode;
         textNode = NextNode()) {
      undisplayed += textNode->TextLength();
    }
  }

  // Record the trailing number of undisplayed characters on the
  // SVGTextFrame.
  aRoot->mTrailingUndisplayedCharacters = undisplayed;
}

nsTextNode*
TextNodeCorrespondenceRecorder::NextNode()
{
  mPreviousNode = mNodeIterator.Current();
  nsTextNode* next;
  do {
    next = mNodeIterator.Next();
  } while (next && next->TextLength() == 0);
  return next;
}

void
TextNodeCorrespondenceRecorder::TraverseAndRecord(nsIFrame* aFrame)
{
  // Recursively iterate over the frame tree, for frames that correspond
  // to text content elements.
  if (IsTextContentElement(aFrame->GetContent())) {
    for (nsIFrame* f = aFrame->GetFirstPrincipalChild();
         f;
         f = f->GetNextSibling()) {
      TraverseAndRecord(f);
    }
    return;
  }

  nsTextFrame* frame;  // The current text frame.
  nsTextNode* node;    // The text node for the current text frame.
  if (!GetNonEmptyTextFrameAndNode(aFrame, frame, node)) {
    // If this isn't an nsTextFrame, or is empty, nothing to do.
    return;
  }

  NS_ASSERTION(frame->GetContentOffset() >= 0,
               "don't know how to handle negative content indexes");

  uint32_t undisplayed = 0;
  if (!mPreviousNode) {
    // Must be the very first text frame.
    NS_ASSERTION(mNodeCharIndex == 0, "incorrect tracking of undisplayed "
                                      "characters in text nodes");
    if (!mNodeIterator.Current()) {
      NS_NOTREACHED("incorrect tracking of correspondence between text frames "
                    "and text nodes");
    } else {
      // Each whole nsTextNode we find before we get to the text node for the
      // first text frame must be undisplayed.
      while (mNodeIterator.Current() != node) {
        undisplayed += mNodeIterator.Current()->TextLength();
        NextNode();
      }
      // If the first text frame starts at a non-zero content offset, then those
      // earlier characters are also undisplayed.
      undisplayed += frame->GetContentOffset();
      NextNode();
    }
  } else if (mPreviousNode == node) {
    // Same text node as last time.
    if (static_cast<uint32_t>(frame->GetContentOffset()) != mNodeCharIndex) {
      // We have some characters in the middle of the text node
      // that are undisplayed.
      NS_ASSERTION(mNodeCharIndex <
                     static_cast<uint32_t>(frame->GetContentOffset()),
                   "incorrect tracking of undisplayed characters in "
                   "text nodes");
      undisplayed = frame->GetContentOffset() - mNodeCharIndex;
    }
  } else {
    // Different text node from last time.
    if (mPreviousNode->TextLength() != mNodeCharIndex) {
      NS_ASSERTION(mNodeCharIndex < mPreviousNode->TextLength(),
                   "incorrect tracking of undisplayed characters in "
                   "text nodes");
      // Any trailing characters at the end of the previous nsTextNode are
      // undisplayed.
      undisplayed = mPreviousNode->TextLength() - mNodeCharIndex;
    }
    // Each whole nsTextNode we find before we get to the text node for
    // the current text frame must be undisplayed.
    while (mNodeIterator.Current() != node) {
      undisplayed += mNodeIterator.Current()->TextLength();
      NextNode();
    }
    // If the current text frame starts at a non-zero content offset, then those
    // earlier characters are also undisplayed.
    undisplayed += frame->GetContentOffset();
    NextNode();
  }

  // Set the frame property.
  frame->Properties().Set(TextNodeCorrespondenceProperty(),
                          new TextNodeCorrespondence(undisplayed));

  // Remember how far into the current nsTextNode we are.
  mNodeCharIndex = frame->GetContentEnd();
}

// ----------------------------------------------------------------------------
// TextFrameIterator

/**
 * An iterator class for nsTextFrames that are descendants of an
 * SVGTextFrame.  The iterator can optionally track whether the
 * current nsTextFrame is for a descendant of, or past, a given subtree
 * content node or frame.  (This functionality is used for example by the SVG
 * DOM text methods to get only the nsTextFrames for a particular <tspan>.)
 *
 * TextFrameIterator also tracks and exposes other information about the
 * current nsTextFrame:
 *
 *   * how many undisplayed characters came just before it
 *   * its position (in app units) relative to the SVGTextFrame's anonymous
 *     block frame
 *   * what nsInlineFrame corresponding to a <textPath> element it is a
 *     descendant of
 *   * what computed dominant-baseline value applies to it
 *
 * Note that any text frames that are empty -- whose ContentLength() is 0 --
 * will be skipped over.
 */
class TextFrameIterator
{
public:
  /**
   * Constructs a TextFrameIterator for the specified SVGTextFrame
   * with an optional frame subtree to restrict iterated text frames to.
   */
  TextFrameIterator(SVGTextFrame* aRoot, nsIFrame* aSubtree = nullptr)
    : mRootFrame(aRoot),
      mSubtree(aSubtree),
      mCurrentFrame(aRoot),
      mCurrentPosition(),
      mSubtreePosition(mSubtree ? eBeforeSubtree : eWithinSubtree)
  {
    Init();
  }

  /**
   * Constructs a TextFrameIterator for the specified SVGTextFrame
   * with an optional frame content subtree to restrict iterated text frames to.
   */
  TextFrameIterator(SVGTextFrame* aRoot, nsIContent* aSubtree)
    : mRootFrame(aRoot),
      mSubtree(aRoot && aSubtree && aSubtree != aRoot->GetContent() ?
                 aSubtree->GetPrimaryFrame() :
                 nullptr),
      mCurrentFrame(aRoot),
      mCurrentPosition(),
      mSubtreePosition(mSubtree ? eBeforeSubtree : eWithinSubtree)
  {
    Init();
  }

  /**
   * Returns the root SVGTextFrame this TextFrameIterator is iterating over.
   */
  SVGTextFrame* Root() const
  {
    return mRootFrame;
  }

  /**
   * Returns the current nsTextFrame.
   */
  nsTextFrame* Current() const
  {
    return do_QueryFrame(mCurrentFrame);
  }

  /**
   * Returns the number of undisplayed characters in the DOM just before the
   * current frame.
   */
  uint32_t UndisplayedCharacters() const;

  /**
   * Returns the current frame's position, in app units, relative to the
   * root SVGTextFrame's anonymous block frame.
   */
  nsPoint Position() const
  {
    return mCurrentPosition;
  }

  /**
   * Advances to the next nsTextFrame and returns it.
   */
  nsTextFrame* Next();

  /**
   * Returns whether the iterator is within the subtree.
   */
  bool IsWithinSubtree() const
  {
    return mSubtreePosition == eWithinSubtree;
  }

  /**
   * Returns whether the iterator is past the subtree.
   */
  bool IsAfterSubtree() const
  {
    return mSubtreePosition == eAfterSubtree;
  }

  /**
   * Returns the frame corresponding to the <textPath> element, if we
   * are inside one.
   */
  nsIFrame* TextPathFrame() const
  {
    return mTextPathFrames.IsEmpty() ?
             nullptr :
             mTextPathFrames.ElementAt(mTextPathFrames.Length() - 1);
  }

  /**
   * Returns the current frame's computed dominant-baseline value.
   */
  uint8_t DominantBaseline() const
  {
    return mBaselines.ElementAt(mBaselines.Length() - 1);
  }

  /**
   * Finishes the iterator.
   */
  void Close()
  {
    mCurrentFrame = nullptr;
  }

private:
  /**
   * Initializes the iterator and advances to the first item.
   */
  void Init()
  {
    if (!mRootFrame) {
      return;
    }

    mBaselines.AppendElement(mRootFrame->StyleSVGReset()->mDominantBaseline);
    Next();
  }

  /**
   * Pushes the specified frame's computed dominant-baseline value.
   * If the value of the property is "auto", then the parent frame's
   * computed value is used.
   */
  void PushBaseline(nsIFrame* aNextFrame);

  /**
   * Pops the current dominant-baseline off the stack.
   */
  void PopBaseline();

  /**
   * The root frame we are iterating through.
   */
  SVGTextFrame* mRootFrame;

  /**
   * The frame for the subtree we are also interested in tracking.
   */
  nsIFrame* mSubtree;

  /**
   * The current value of the iterator.
   */
  nsIFrame* mCurrentFrame;

  /**
   * The position, in app units, of the current frame relative to mRootFrame.
   */
  nsPoint mCurrentPosition;

  /**
   * Stack of frames corresponding to <textPath> elements that are in scope
   * for the current frame.
   */
  nsAutoTArray<nsIFrame*, 1> mTextPathFrames;

  /**
   * Stack of dominant-baseline values to record as we traverse through the
   * frame tree.
   */
  nsAutoTArray<uint8_t, 8> mBaselines;

  /**
   * The iterator's current position relative to mSubtree.
   */
  SubtreePosition mSubtreePosition;
};

uint32_t
TextFrameIterator::UndisplayedCharacters() const
{
  MOZ_ASSERT(!(mRootFrame->GetFirstPrincipalChild() &&
               NS_SUBTREE_DIRTY(mRootFrame->GetFirstPrincipalChild())),
             "should have already reflowed the anonymous block child");

  if (!mCurrentFrame) {
    return mRootFrame->mTrailingUndisplayedCharacters;
  }

  nsTextFrame* frame = do_QueryFrame(mCurrentFrame);
  return GetUndisplayedCharactersBeforeFrame(frame);
}

nsTextFrame*
TextFrameIterator::Next()
{
  // Starting from mCurrentFrame, we do a non-recursive traversal to the next
  // nsTextFrame beneath mRoot, updating mSubtreePosition appropriately if we
  // encounter mSubtree.
  if (mCurrentFrame) {
    do {
      nsIFrame* next = IsTextContentElement(mCurrentFrame->GetContent()) ?
                         mCurrentFrame->GetFirstPrincipalChild() :
                         nullptr;
      if (next) {
        // Descend into this frame, and accumulate its position.
        mCurrentPosition += next->GetPosition();
        if (next->GetContent()->Tag() == nsGkAtoms::textPath) {
          // Record this <textPath> frame.
          mTextPathFrames.AppendElement(next);
        }
        // Record the frame's baseline.
        PushBaseline(next);
        mCurrentFrame = next;
        if (mCurrentFrame == mSubtree) {
          // If the current frame is mSubtree, we have now moved into it.
          mSubtreePosition = eWithinSubtree;
        }
      } else {
        for (;;) {
          // We want to move past the current frame.
          if (mCurrentFrame == mRootFrame) {
            // If we've reached the root frame, we're finished.
            mCurrentFrame = nullptr;
            break;
          }
          // Remove the current frame's position.
          mCurrentPosition -= mCurrentFrame->GetPosition();
          if (mCurrentFrame->GetContent()->Tag() == nsGkAtoms::textPath) {
            // Pop off the <textPath> frame if this is a <textPath>.
            mTextPathFrames.TruncateLength(mTextPathFrames.Length() - 1);
          }
          // Pop off the current baseline.
          PopBaseline();
          if (mCurrentFrame == mSubtree) {
            // If this was mSubtree, we have now moved past it.
            mSubtreePosition = eAfterSubtree;
          }
          next = mCurrentFrame->GetNextSibling();
          if (next) {
            // Moving to the next sibling.
            mCurrentPosition += next->GetPosition();
            if (next->GetContent()->Tag() == nsGkAtoms::textPath) {
              // Record this <textPath> frame.
              mTextPathFrames.AppendElement(next);
            }
            // Record the frame's baseline.
            PushBaseline(next);
            mCurrentFrame = next;
            if (mCurrentFrame == mSubtree) {
              // If the current frame is mSubtree, we have now moved into it.
              mSubtreePosition = eWithinSubtree;
            }
            break;
          }
          if (mCurrentFrame == mSubtree) {
            // If there is no next sibling frame, and the current frame is
            // mSubtree, we have now moved past it.
            mSubtreePosition = eAfterSubtree;
          }
          // Ascend out of this frame.
          mCurrentFrame = mCurrentFrame->GetParent();
        }
      }
    } while (mCurrentFrame &&
             !IsNonEmptyTextFrame(mCurrentFrame));
  }

  return Current();
}

void
TextFrameIterator::PushBaseline(nsIFrame* aNextFrame)
{
  uint8_t baseline = aNextFrame->StyleSVGReset()->mDominantBaseline;
  if (baseline == NS_STYLE_DOMINANT_BASELINE_AUTO) {
    baseline = mBaselines.LastElement();
  }
  mBaselines.AppendElement(baseline);
}

void
TextFrameIterator::PopBaseline()
{
  NS_ASSERTION(!mBaselines.IsEmpty(), "popped too many baselines");
  mBaselines.TruncateLength(mBaselines.Length() - 1);
}

// -----------------------------------------------------------------------------
// TextRenderedRunIterator

/**
 * Iterator for TextRenderedRun objects for the SVGTextFrame.
 */
class TextRenderedRunIterator
{
public:
  /**
   * Values for the aFilter argument of the constructor, to indicate which frames
   * we should be limited to iterating TextRenderedRun objects for.
   */
  enum RenderedRunFilter {
    // Iterate TextRenderedRuns for all nsTextFrames.
    eAllFrames,
    // Iterate only TextRenderedRuns for nsTextFrames that are
    // visibility:visible.
    eVisibleFrames
  };

  /**
   * Constructs a TextRenderedRunIterator with an optional frame subtree to
   * restrict iterated rendered runs to.
   *
   * @param aSVGTextFrame The SVGTextFrame whose rendered runs to iterate
   *   through.
   * @param aFilter Indicates whether to iterate rendered runs for non-visible
   *   nsTextFrames.
   * @param aSubtree An optional frame subtree to restrict iterated rendered
   *   runs to.
   */
  TextRenderedRunIterator(SVGTextFrame* aSVGTextFrame,
                          RenderedRunFilter aFilter = eAllFrames,
                          nsIFrame* aSubtree = nullptr)
    : mFrameIterator(FrameIfAnonymousChildReflowed(aSVGTextFrame), aSubtree),
      mFilter(aFilter),
      mTextElementCharIndex(0),
      mFrameStartTextElementCharIndex(0),
      mFontSizeScaleFactor(aSVGTextFrame->mFontSizeScaleFactor),
      mCurrent(First())
  {
  }

  /**
   * Constructs a TextRenderedRunIterator with a content subtree to restrict
   * iterated rendered runs to.
   *
   * @param aSVGTextFrame The SVGTextFrame whose rendered runs to iterate
   *   through.
   * @param aFilter Indicates whether to iterate rendered runs for non-visible
   *   nsTextFrames.
   * @param aSubtree A content subtree to restrict iterated rendered runs to.
   */
  TextRenderedRunIterator(SVGTextFrame* aSVGTextFrame,
                          RenderedRunFilter aFilter,
                          nsIContent* aSubtree)
    : mFrameIterator(FrameIfAnonymousChildReflowed(aSVGTextFrame), aSubtree),
      mFilter(aFilter),
      mTextElementCharIndex(0),
      mFrameStartTextElementCharIndex(0),
      mFontSizeScaleFactor(aSVGTextFrame->mFontSizeScaleFactor),
      mCurrent(First())
  {
  }

  /**
   * Returns the current TextRenderedRun.
   */
  TextRenderedRun Current() const
  {
    return mCurrent;
  }

  /**
   * Advances to the next TextRenderedRun and returns it.
   */
  TextRenderedRun Next();

private:
  /**
   * Returns the root SVGTextFrame this iterator is for.
   */
  SVGTextFrame* Root() const
  {
    return mFrameIterator.Root();
  }

  /**
   * Advances to the first TextRenderedRun and returns it.
   */
  TextRenderedRun First();

  /**
   * The frame iterator to use.
   */
  TextFrameIterator mFrameIterator;

  /**
   * The filter indicating which TextRenderedRuns to return.
   */
  RenderedRunFilter mFilter;

  /**
   * The character index across the entire <text> element we are currently
   * up to.
   */
  uint32_t mTextElementCharIndex;

  /**
   * The character index across the entire <text> for the start of the current
   * frame.
   */
  uint32_t mFrameStartTextElementCharIndex;

  /**
   * The font-size scale factor we used when constructing the nsTextFrames.
   */
  double mFontSizeScaleFactor;

  /**
   * The current TextRenderedRun.
   */
  TextRenderedRun mCurrent;
};

TextRenderedRun
TextRenderedRunIterator::Next()
{
  if (!mFrameIterator.Current()) {
    // If there are no more frames, then there are no more rendered runs to
    // return.
    mCurrent = TextRenderedRun();
    return mCurrent;
  }

  // The values we will use to initialize the TextRenderedRun with.
  nsTextFrame* frame;
  gfxPoint pt;
  double rotate;
  nscoord baseline;
  uint32_t offset, length;
  uint32_t charIndex;

  // We loop, because we want to skip over rendered runs that either aren't
  // within our subtree of interest, because they don't match the filter,
  // or because they are hidden due to having fallen off the end of a
  // <textPath>.
  for (;;) {
    if (mFrameIterator.IsAfterSubtree()) {
      mCurrent = TextRenderedRun();
      return mCurrent;
    }

    frame = mFrameIterator.Current();

    charIndex = mTextElementCharIndex;

    // Find the end of the rendered run, by looking through the
    // SVGTextFrame's positions array until we find one that is recorded
    // as a run boundary.
    uint32_t runStart, runEnd;  // XXX Replace runStart with mTextElementCharIndex.
    runStart = mTextElementCharIndex;
    runEnd = runStart + 1;
    while (runEnd < Root()->mPositions.Length() &&
           !Root()->mPositions[runEnd].mRunBoundary) {
      runEnd++;
    }

    // Convert the global run start/end indexes into an offset/length into the
    // current frame's nsTextNode.
    offset = frame->GetContentOffset() + runStart -
             mFrameStartTextElementCharIndex;
    length = runEnd - runStart;

    // If the end of the frame's content comes before the run boundary we found
    // in SVGTextFrame's position array, we need to shorten the rendered run.
    uint32_t contentEnd = frame->GetContentEnd();
    if (offset + length > contentEnd) {
      length = contentEnd - offset;
    }

    NS_ASSERTION(offset >= uint32_t(frame->GetContentOffset()), "invalid offset");
    NS_ASSERTION(offset + length <= contentEnd, "invalid offset or length");

    // Get the frame's baseline position.
    frame->EnsureTextRun(nsTextFrame::eInflated);
    baseline = GetBaselinePosition(frame,
                                   frame->GetTextRun(nsTextFrame::eInflated),
                                   mFrameIterator.DominantBaseline());

    // Trim the offset/length to remove any leading/trailing white space.
    uint32_t untrimmedOffset = offset;
    uint32_t untrimmedLength = length;
    nsTextFrame::TrimmedOffsets trimmedOffsets =
      frame->GetTrimmedOffsets(frame->GetContent()->GetText(), true);
    TrimOffsets(offset, length, trimmedOffsets);
    charIndex += offset - untrimmedOffset;

    // Get the position and rotation of the character that begins this
    // rendered run.
    pt = Root()->mPositions[charIndex].mPosition;
    rotate = Root()->mPositions[charIndex].mAngle;

    // Determine if we should skip this rendered run.
    bool skip = !mFrameIterator.IsWithinSubtree() ||
                Root()->mPositions[mTextElementCharIndex].mHidden;
    if (mFilter == eVisibleFrames) {
      skip = skip || !frame->StyleVisibility()->IsVisible();
    }

    // Update our global character index to move past the characters
    // corresponding to this rendered run.
    mTextElementCharIndex += untrimmedLength;

    // If we have moved past the end of the current frame's content, we need to
    // advance to the next frame.
    if (offset + untrimmedLength >= contentEnd) {
      mFrameIterator.Next();
      mTextElementCharIndex += mFrameIterator.UndisplayedCharacters();
      mFrameStartTextElementCharIndex = mTextElementCharIndex;
    }

    if (!mFrameIterator.Current()) {
      if (skip) {
        // That was the last frame, and we skipped this rendered run.  So we
        // have no rendered run to return.
        mCurrent = TextRenderedRun();
        return mCurrent;
      }
      break;
    }

    if (length && !skip) {
      // Only return a rendered run if it didn't get collapsed away entirely
      // (due to it being all white space) and if we don't want to skip it.
      break;
    }
  }

  mCurrent = TextRenderedRun(frame, pt, Root()->mLengthAdjustScaleFactor,
                             rotate, mFontSizeScaleFactor, baseline,
                             offset, length, charIndex);
  return mCurrent;
}

TextRenderedRun
TextRenderedRunIterator::First()
{
  if (!mFrameIterator.Current()) {
    return TextRenderedRun();
  }

  if (Root()->mPositions.IsEmpty()) {
    mFrameIterator.Close();
    return TextRenderedRun();
  }

  // Get the character index for the start of this rendered run, by skipping
  // any undisplayed characters.
  mTextElementCharIndex = mFrameIterator.UndisplayedCharacters();
  mFrameStartTextElementCharIndex = mTextElementCharIndex;

  return Next();
}

// -----------------------------------------------------------------------------
// CharIterator

/**
 * Iterator for characters within an SVGTextFrame.
 */
class CharIterator
{
public:
  /**
   * Values for the aFilter argument of the constructor, to indicate which
   * characters we should be iterating over.
   */
  enum CharacterFilter {
    // Iterate over all original characters from the DOM that are within valid
    // text content elements.
    eOriginal,
    // Iterate only over characters that are addressable by the positioning
    // attributes x="", y="", etc.  This includes all characters after
    // collapsing white space as required by the value of 'white-space'.
    eAddressable,
    // Iterate only over characters that are the first of clusters or ligature
    // groups.
    eClusterAndLigatureGroupStart,
    // Iterate only over characters that are part of a cluster or ligature
    // group but not the first character.
    eClusterOrLigatureGroupMiddle
  };

  /**
   * Constructs a CharIterator.
   *
   * @param aSVGTextFrame The SVGTextFrame whose characters to iterate
   *   through.
   * @param aFilter Indicates which characters to iterate over.
   * @param aSubtree A content subtree to track whether the current character
   *   is within.
   */
  CharIterator(SVGTextFrame* aSVGTextFrame,
               CharacterFilter aFilter,
               nsIContent* aSubtree = nullptr);

  /**
   * Returns whether the iterator is finished.
   */
  bool AtEnd() const
  {
    return !mFrameIterator.Current();
  }

  /**
   * Advances to the next matching character.  Returns true if there was a
   * character to advance to, and false otherwise.
   */
  bool Next();

  /**
   * Advances ahead aCount matching characters.  Returns true if there were
   * enough characters to advance past, and false otherwise.
   */
  bool Next(uint32_t aCount);

  /**
   * Advances ahead up to aCount matching characters.
   */
  void NextWithinSubtree(uint32_t aCount);

  /**
   * Advances to the character with the specified index.  The index is in the
   * space of original characters (i.e., all DOM characters under the <text>
   * that are within valid text content elements).
   */
  bool AdvanceToCharacter(uint32_t aTextElementCharIndex);

  /**
   * Advances to the first matching character after the current nsTextFrame.
   */
  bool AdvancePastCurrentFrame();

  /**
   * Advances to the first matching character after the frames within
   * the current <textPath>.
   */
  bool AdvancePastCurrentTextPathFrame();

  /**
   * Advances to the first matching character of the subtree.  Returns true
   * if we successfully advance to the subtree, or if we are already within
   * the subtree.  Returns false if we are past the subtree.
   */
  bool AdvanceToSubtree();

  /**
   * Returns the nsTextFrame for the current character.
   */
  nsTextFrame* TextFrame() const
  {
    return mFrameIterator.Current();
  }

  /**
   * Returns whether the iterator is within the subtree.
   */
  bool IsWithinSubtree() const
  {
    return mFrameIterator.IsWithinSubtree();
  }

  /**
   * Returns whether the iterator is past the subtree.
   */
  bool IsAfterSubtree() const
  {
    return mFrameIterator.IsAfterSubtree();
  }

  /**
   * Returns whether the current character is a skipped character.
   */
  bool IsOriginalCharSkipped() const
  {
    return mSkipCharsIterator.IsOriginalCharSkipped();
  }

  /**
   * Returns whether the current character is the start of a cluster and
   * ligature group.
   */
  bool IsClusterAndLigatureGroupStart() const;

  /**
   * Returns whether the current character is trimmed away when painting,
   * due to it being leading/trailing white space.
   */
  bool IsOriginalCharTrimmed() const;

  /**
   * Returns whether the current character is unaddressable from the SVG glyph
   * positioning attributes.
   */
  bool IsOriginalCharUnaddressable() const
  {
    return IsOriginalCharSkipped() || IsOriginalCharTrimmed();
  }

  /**
   * Returns the text run for the current character.
   */
  gfxTextRun* TextRun() const
  {
    return mTextRun;
  }

  /**
   * Returns the current character index.
   */
  uint32_t TextElementCharIndex() const
  {
    return mTextElementCharIndex;
  }

  /**
   * Returns the character index for the start of the cluster/ligature group it
   * is part of.
   */
  uint32_t GlyphStartTextElementCharIndex() const
  {
    return mGlyphStartTextElementCharIndex;
  }

  /**
   * Returns the number of undisplayed characters between the beginning of
   * the glyph and the current character.
   */
  uint32_t GlyphUndisplayedCharacters() const
  {
    return mGlyphUndisplayedCharacters;
  }

  /**
   * Gets the original character offsets within the nsTextNode for the
   * cluster/ligature group the current character is a part of.
   *
   * @param aOriginalOffset The offset of the start of the cluster/ligature
   *   group (output).
   * @param aOriginalLength The length of cluster/ligature group (output).
   */
  void GetOriginalGlyphOffsets(uint32_t& aOriginalOffset,
                               uint32_t& aOriginalLength) const;

  /**
   * Gets the advance, in user units, of the glyph the current character is
   * part of.
   *
   * @param aContext The context to use for unit conversions.
   */
  gfxFloat GetGlyphAdvance(nsPresContext* aContext) const;

  /**
   * Gets the advance, in user units, of the current character.  If the
   * character is a part of ligature, then the advance returned will be
   * a fraction of the ligature glyph's advance.
   *
   * @param aContext The context to use for unit conversions.
   */
  gfxFloat GetAdvance(nsPresContext* aContext) const;

  /**
   * Gets the specified partial advance of the glyph the current character is
   * part of.  The partial advance is measured from the first character
   * corresponding to the glyph until the specified part length.
   *
   * The part length value does not include any undisplayed characters in the
   * middle of the cluster/ligature group.  For example, if you have:
   *
   *   <text>f<tspan display="none">x</tspan>i</text>
   *
   * and the "f" and "i" are ligaturized, then calling GetGlyphPartialAdvance
   * with aPartLength values will have the following results:
   *
   *   0 => 0
   *   1 => adv("fi") / 2
   *   2 => adv("fi")
   *
   * @param aPartLength The number of characters in the cluster/ligature group
   *   to measure.
   * @param aContext The context to use for unit conversions.
   */
  gfxFloat GetGlyphPartialAdvance(uint32_t aPartLength,
                                  nsPresContext* aContext) const;

  /**
   * Returns the frame corresponding to the <textPath> that the current
   * character is within.
   */
  nsIFrame* TextPathFrame() const
  {
    return mFrameIterator.TextPathFrame();
  }

private:
  /**
   * Advances to the next character without checking it against the filter.
   * Returns true if there was a next character to advance to, or false
   * otherwise.
   */
  bool NextCharacter();

  /**
   * Returns whether the current character matches the filter.
   */
  bool MatchesFilter() const;

  /**
   * If this is the start of a glyph, record it.
   */
  void UpdateGlyphStartTextElementCharIndex() {
    if (!IsOriginalCharSkipped() && IsClusterAndLigatureGroupStart()) {
      mGlyphStartTextElementCharIndex = mTextElementCharIndex;
      mGlyphUndisplayedCharacters = 0;
    }
  }

  /**
   * The filter to use.
   */
  CharacterFilter mFilter;

  /**
   * The iterator for text frames.
   */
  TextFrameIterator mFrameIterator;

  /**
   * A gfxSkipCharsIterator for the text frame the current character is
   * a part of.
   */
  gfxSkipCharsIterator mSkipCharsIterator;

  // Cache for information computed by IsOriginalCharTrimmed.
  mutable nsTextFrame* mFrameForTrimCheck;
  mutable uint32_t mTrimmedOffset;
  mutable uint32_t mTrimmedLength;

  /**
   * The text run the current character is a part of.
   */
  gfxTextRun* mTextRun;

  /**
   * The current character's index.
   */
  uint32_t mTextElementCharIndex;

  /**
   * The index of the character that starts the cluster/ligature group the
   * current character is a part of.
   */
  uint32_t mGlyphStartTextElementCharIndex;

  /**
   * If we are iterating in mode eClusterOrLigatureGroupMiddle, then
   * this tracks how many undisplayed characters were encountered
   * between the start of this glyph (at mGlyphStartTextElementCharIndex)
   * and the current character (at mTextElementCharIndex).
   */
  uint32_t mGlyphUndisplayedCharacters;

  /**
   * The scale factor to apply to glyph advances returned by
   * GetGlyphAdvance etc. to take into account textLength="".
   */
  float mLengthAdjustScaleFactor;
};

CharIterator::CharIterator(SVGTextFrame* aSVGTextFrame,
                           CharIterator::CharacterFilter aFilter,
                           nsIContent* aSubtree)
  : mFilter(aFilter),
    mFrameIterator(FrameIfAnonymousChildReflowed(aSVGTextFrame), aSubtree),
    mFrameForTrimCheck(nullptr),
    mTrimmedOffset(0),
    mTrimmedLength(0),
    mTextElementCharIndex(0),
    mGlyphStartTextElementCharIndex(0),
    mLengthAdjustScaleFactor(aSVGTextFrame->mLengthAdjustScaleFactor)
{
  if (!AtEnd()) {
    mSkipCharsIterator = TextFrame()->EnsureTextRun(nsTextFrame::eInflated);
    mTextRun = TextFrame()->GetTextRun(nsTextFrame::eInflated);
    mTextElementCharIndex = mFrameIterator.UndisplayedCharacters();
    UpdateGlyphStartTextElementCharIndex();
    if (!MatchesFilter()) {
      Next();
    }
  }
}

bool
CharIterator::Next()
{
  while (NextCharacter()) {
    if (MatchesFilter()) {
      return true;
    }
  }
  return false;
}

bool
CharIterator::Next(uint32_t aCount)
{
  if (aCount == 0 && AtEnd()) {
    return false;
  }
  while (aCount) {
    if (!Next()) {
      return false;
    }
    aCount--;
  }
  return true;
}

void
CharIterator::NextWithinSubtree(uint32_t aCount)
{
  while (IsWithinSubtree() && aCount) {
    --aCount;
    if (!Next()) {
      return;
    }
  }
}

bool
CharIterator::AdvanceToCharacter(uint32_t aTextElementCharIndex)
{
  while (mTextElementCharIndex < aTextElementCharIndex) {
    if (!Next()) {
      return false;
    }
  }
  return true;
}

bool
CharIterator::AdvancePastCurrentFrame()
{
  // XXX Can do this better than one character at a time if it matters.
  nsTextFrame* currentFrame = TextFrame();
  do {
    if (!Next()) {
      return false;
    }
  } while (TextFrame() == currentFrame);
  return true;
}

bool
CharIterator::AdvancePastCurrentTextPathFrame()
{
  nsIFrame* currentTextPathFrame = TextPathFrame();
  NS_ASSERTION(currentTextPathFrame,
               "expected AdvancePastCurrentTextPathFrame to be called only "
               "within a text path frame");
  do {
    if (!AdvancePastCurrentFrame()) {
      return false;
    }
  } while (TextPathFrame() == currentTextPathFrame);
  return true;
}

bool
CharIterator::AdvanceToSubtree()
{
  while (!IsWithinSubtree()) {
    if (IsAfterSubtree()) {
      return false;
    }
    if (!AdvancePastCurrentFrame()) {
      return false;
    }
  }
  return true;
}

bool
CharIterator::IsClusterAndLigatureGroupStart() const
{
  return mTextRun->IsLigatureGroupStart(mSkipCharsIterator.GetSkippedOffset()) &&
         mTextRun->IsClusterStart(mSkipCharsIterator.GetSkippedOffset());
}

bool
CharIterator::IsOriginalCharTrimmed() const
{
  if (mFrameForTrimCheck != TextFrame()) {
    // Since we do a lot of trim checking, we cache the trimmed offsets and
    // lengths while we are in the same frame.
    mFrameForTrimCheck = TextFrame();
    uint32_t offset = mFrameForTrimCheck->GetContentOffset();
    uint32_t length = mFrameForTrimCheck->GetContentLength();
    nsIContent* content = mFrameForTrimCheck->GetContent();
    nsTextFrame::TrimmedOffsets trim =
      mFrameForTrimCheck->GetTrimmedOffsets(content->GetText(), true);
    TrimOffsets(offset, length, trim);
    mTrimmedOffset = offset;
    mTrimmedLength = length;
  }

  // A character is trimmed if it is outside the mTrimmedOffset/mTrimmedLength
  // range and it is not a significant newline character.
  uint32_t index = mSkipCharsIterator.GetOriginalOffset();
  return !((index >= mTrimmedOffset &&
            index < mTrimmedOffset + mTrimmedLength) ||
           (index >= mTrimmedOffset + mTrimmedLength &&
            mFrameForTrimCheck->StyleText()->NewlineIsSignificant() &&
            mFrameForTrimCheck->GetContent()->GetText()->CharAt(index) == '\n'));
}

void
CharIterator::GetOriginalGlyphOffsets(uint32_t& aOriginalOffset,
                                      uint32_t& aOriginalLength) const
{
  gfxSkipCharsIterator it = TextFrame()->EnsureTextRun(nsTextFrame::eInflated);
  it.SetOriginalOffset(mSkipCharsIterator.GetOriginalOffset() -
                         (mTextElementCharIndex -
                          mGlyphStartTextElementCharIndex -
                          mGlyphUndisplayedCharacters));

  while (it.GetSkippedOffset() > 0 &&
         (!mTextRun->IsClusterStart(it.GetSkippedOffset()) ||
          !mTextRun->IsLigatureGroupStart(it.GetSkippedOffset()))) {
    it.AdvanceSkipped(-1);
  }

  aOriginalOffset = it.GetOriginalOffset();

  // Find the end of the cluster/ligature group.
  it.SetOriginalOffset(mSkipCharsIterator.GetOriginalOffset());
  do {
    it.AdvanceSkipped(1);
  } while (it.GetSkippedOffset() < mTextRun->GetLength() &&
           (!mTextRun->IsClusterStart(it.GetSkippedOffset()) ||
            !mTextRun->IsLigatureGroupStart(it.GetSkippedOffset())));

  aOriginalLength = it.GetOriginalOffset() - aOriginalOffset;
}

gfxFloat
CharIterator::GetGlyphAdvance(nsPresContext* aContext) const
{
  uint32_t offset, length;
  GetOriginalGlyphOffsets(offset, length);

  gfxSkipCharsIterator it = TextFrame()->EnsureTextRun(nsTextFrame::eInflated);
  ConvertOriginalToSkipped(it, offset, length);

  float cssPxPerDevPx = aContext->
    AppUnitsToFloatCSSPixels(aContext->AppUnitsPerDevPixel());

  gfxFloat advance = mTextRun->GetAdvanceWidth(offset, length, nullptr);
  return aContext->AppUnitsToGfxUnits(advance) *
         mLengthAdjustScaleFactor * cssPxPerDevPx;
}

gfxFloat
CharIterator::GetAdvance(nsPresContext* aContext) const
{
  float cssPxPerDevPx = aContext->
    AppUnitsToFloatCSSPixels(aContext->AppUnitsPerDevPixel());

  gfxFloat advance =
    mTextRun->GetAdvanceWidth(mSkipCharsIterator.GetSkippedOffset(), 1, nullptr);
  return aContext->AppUnitsToGfxUnits(advance) *
         mLengthAdjustScaleFactor * cssPxPerDevPx;
}

gfxFloat
CharIterator::GetGlyphPartialAdvance(uint32_t aPartLength,
                                     nsPresContext* aContext) const
{
  uint32_t offset, length;
  GetOriginalGlyphOffsets(offset, length);

  NS_ASSERTION(aPartLength <= length, "invalid aPartLength value");
  length = aPartLength;

  gfxSkipCharsIterator it = TextFrame()->EnsureTextRun(nsTextFrame::eInflated);
  ConvertOriginalToSkipped(it, offset, length);

  float cssPxPerDevPx = aContext->
    AppUnitsToFloatCSSPixels(aContext->AppUnitsPerDevPixel());

  gfxFloat advance = mTextRun->GetAdvanceWidth(offset, length, nullptr);
  return aContext->AppUnitsToGfxUnits(advance) *
         mLengthAdjustScaleFactor * cssPxPerDevPx;
}

bool
CharIterator::NextCharacter()
{
  if (AtEnd()) {
    return false;
  }

  mTextElementCharIndex++;

  // Advance within the current text run.
  mSkipCharsIterator.AdvanceOriginal(1);
  if (mSkipCharsIterator.GetOriginalOffset() < TextFrame()->GetContentEnd()) {
    // We're still within the part of the text run for the current text frame.
    UpdateGlyphStartTextElementCharIndex();
    return true;
  }

  // Advance to the next frame.
  mFrameIterator.Next();

  // Skip any undisplayed characters.
  uint32_t undisplayed = mFrameIterator.UndisplayedCharacters();
  mGlyphUndisplayedCharacters += undisplayed;
  mTextElementCharIndex += undisplayed;
  if (!TextFrame()) {
    // We're at the end.
    mSkipCharsIterator = gfxSkipCharsIterator();
    return false;
  }

  mSkipCharsIterator = TextFrame()->EnsureTextRun(nsTextFrame::eInflated);
  mTextRun = TextFrame()->GetTextRun(nsTextFrame::eInflated);
  UpdateGlyphStartTextElementCharIndex();
  return true;
}

bool
CharIterator::MatchesFilter() const
{
  if (mFilter == eOriginal) {
    return true;
  }

  if (IsOriginalCharSkipped()) {
    return false;
  }

  if (mFilter == eAddressable) {
    return !IsOriginalCharUnaddressable();
  }

  return (mFilter == eClusterAndLigatureGroupStart) ==
         IsClusterAndLigatureGroupStart();
}

// -----------------------------------------------------------------------------
// nsCharClipDisplayItem

/**
 * An nsCharClipDisplayItem that obtains its left and right clip edges from a
 * TextRenderedRun object.
 */
class SVGCharClipDisplayItem : public nsCharClipDisplayItem {
public:
  SVGCharClipDisplayItem(const TextRenderedRun& aRun)
    : nsCharClipDisplayItem(aRun.mFrame)
  {
    aRun.GetClipEdges(mLeftEdge, mRightEdge);
  }

  NS_DISPLAY_DECL_NAME("SVGText", TYPE_TEXT)
};

// -----------------------------------------------------------------------------
// SVGTextDrawPathCallbacks

/**
 * Text frame draw callback class that paints the text and text decoration parts
 * of an nsTextFrame using SVG painting properties, and selection backgrounds
 * and decorations as they would normally.
 *
 * An instance of this class is passed to nsTextFrame::PaintText if painting
 * cannot be done directly (e.g. if we are using an SVG pattern fill, stroking
 * the text, etc.).
 */
class SVGTextDrawPathCallbacks : public nsTextFrame::DrawPathCallbacks
{
public:
  /**
   * Constructs an SVGTextDrawPathCallbacks.
   *
   * @param aContext The context to use for painting.
   * @param aFrame The nsTextFrame to paint.
   * @param aCanvasTM The transformation matrix to set when painting; this
   *   should be the FOR_OUTERSVG_TM canvas TM of the text, so that
   *   paint servers are painted correctly.
   * @param aShouldPaintSVGGlyphs Whether SVG glyphs should be painted.
   */
  SVGTextDrawPathCallbacks(nsRenderingContext* aContext,
                           nsTextFrame* aFrame,
                           const gfxMatrix& aCanvasTM,
                           bool aShouldPaintSVGGlyphs)
    : DrawPathCallbacks(aShouldPaintSVGGlyphs),
      gfx(aContext->ThebesContext()),
      mRenderMode(SVGAutoRenderState::GetRenderMode(aContext)),
      mFrame(aFrame),
      mCanvasTM(aCanvasTM)
  {
  }

  void NotifyBeforeText(nscolor aColor) MOZ_OVERRIDE;
  void NotifyGlyphPathEmitted() MOZ_OVERRIDE;
  void NotifyBeforeSVGGlyphPainted() MOZ_OVERRIDE;
  void NotifyAfterSVGGlyphPainted() MOZ_OVERRIDE;
  void NotifyAfterText() MOZ_OVERRIDE;
  void NotifyBeforeSelectionBackground(nscolor aColor) MOZ_OVERRIDE;
  void NotifySelectionBackgroundPathEmitted() MOZ_OVERRIDE;
  void NotifyBeforeDecorationLine(nscolor aColor) MOZ_OVERRIDE;
  void NotifyDecorationLinePathEmitted() MOZ_OVERRIDE;
  void NotifyBeforeSelectionDecorationLine(nscolor aColor) MOZ_OVERRIDE;
  void NotifySelectionDecorationLinePathEmitted() MOZ_OVERRIDE;

private:
  void FillWithOpacity();

  void SetupContext();

  /**
   * Paints a piece of text geometry.  This is called when glyphs
   * or text decorations have been emitted to the gfxContext.
   */
  void HandleTextGeometry();

  /**
   * Sets the gfxContext paint to the appropriate color or pattern
   * for filling text geometry.
   */
  bool SetFillColor();

  /**
   * Fills and strokes a piece of text geometry, using group opacity
   * if the selection style requires it.
   */
  void FillAndStrokeGeometry();

  /**
   * Fills a piece of text geometry.
   */
  void FillGeometry();

  /**
   * Strokes a piece of text geometry.
   */
  void StrokeGeometry();

  gfxContext* gfx;
  uint16_t mRenderMode;
  nsTextFrame* mFrame;
  const gfxMatrix& mCanvasTM;

  /**
   * The color that we were last told from one of the path callback functions.
   * This color can be the special NS_SAME_AS_FOREGROUND_COLOR,
   * NS_40PERCENT_FOREGROUND_COLOR and NS_TRANSPARENT colors when we are
   * painting selections or IME decorations.
   */
  nscolor mColor;
};

void
SVGTextDrawPathCallbacks::NotifyBeforeText(nscolor aColor)
{
  mColor = aColor;
  SetupContext();
  gfx->NewPath();
}

void
SVGTextDrawPathCallbacks::NotifyGlyphPathEmitted()
{
  HandleTextGeometry();
  gfx->NewPath();
}

void
SVGTextDrawPathCallbacks::NotifyBeforeSVGGlyphPainted()
{
  gfx->Save();
}

void
SVGTextDrawPathCallbacks::NotifyAfterSVGGlyphPainted()
{
  gfx->Restore();
  gfx->NewPath();
}

void
SVGTextDrawPathCallbacks::NotifyAfterText()
{
  gfx->Restore();
}

void
SVGTextDrawPathCallbacks::NotifyBeforeSelectionBackground(nscolor aColor)
{
  if (mRenderMode != SVGAutoRenderState::NORMAL) {
    // Don't paint selection backgrounds when in a clip path.
    return;
  }

  mColor = aColor;
  gfx->Save();
}

void
SVGTextDrawPathCallbacks::NotifySelectionBackgroundPathEmitted()
{
  if (mRenderMode != SVGAutoRenderState::NORMAL) {
    // Don't paint selection backgrounds when in a clip path.
    return;
  }

  if (SetFillColor()) {
    FillWithOpacity();
  }
  gfx->Restore();
}

void
SVGTextDrawPathCallbacks::NotifyBeforeDecorationLine(nscolor aColor)
{
  mColor = aColor;
  SetupContext();
}

void
SVGTextDrawPathCallbacks::NotifyDecorationLinePathEmitted()
{
  HandleTextGeometry();
  gfx->NewPath();
  gfx->Restore();
}

void
SVGTextDrawPathCallbacks::NotifyBeforeSelectionDecorationLine(nscolor aColor)
{
  if (mRenderMode != SVGAutoRenderState::NORMAL) {
    // Don't paint selection decorations when in a clip path.
    return;
  }

  mColor = aColor;
  gfx->Save();
}

void
SVGTextDrawPathCallbacks::NotifySelectionDecorationLinePathEmitted()
{
  if (mRenderMode != SVGAutoRenderState::NORMAL) {
    // Don't paint selection decorations when in a clip path.
    return;
  }

  FillAndStrokeGeometry();
  gfx->Restore();
}

void
SVGTextDrawPathCallbacks::FillWithOpacity()
{
  gfx->FillWithOpacity(mColor == NS_40PERCENT_FOREGROUND_COLOR ? 0.4 : 1.0);
}

void
SVGTextDrawPathCallbacks::SetupContext()
{
  gfx->Save();

  // XXX This is copied from nsSVGGlyphFrame::Render, but cairo doesn't actually
  // seem to do anything with the antialias mode.  So we can perhaps remove it,
  // or make SetAntialiasMode set cairo text antialiasing too.
  switch (mFrame->StyleSVG()->mTextRendering) {
  case NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED:
    gfx->SetAntialiasMode(gfxContext::MODE_ALIASED);
    break;
  default:
    gfx->SetAntialiasMode(gfxContext::MODE_COVERAGE);
    break;
  }
}

void
SVGTextDrawPathCallbacks::HandleTextGeometry()
{
  if (mRenderMode != SVGAutoRenderState::NORMAL) {
    // We're in a clip path.
    if (mRenderMode == SVGAutoRenderState::CLIP_MASK) {
      gfx->SetColor(gfxRGBA(1.0f, 1.0f, 1.0f, 1.0f));
      gfx->Fill();
    }
  } else {
    // Normal painting.
    gfxContextMatrixAutoSaveRestore saveMatrix(gfx);
    gfx->SetMatrix(mCanvasTM);

    FillAndStrokeGeometry();
  }
}

bool
SVGTextDrawPathCallbacks::SetFillColor()
{
  if (mColor == NS_SAME_AS_FOREGROUND_COLOR ||
      mColor == NS_40PERCENT_FOREGROUND_COLOR) {
    return nsSVGUtils::SetupCairoFillPaint(mFrame, gfx);
  }

  if (mColor == NS_TRANSPARENT) {
    return false;
  }

  gfx->SetColor(gfxRGBA(mColor));
  return true;
}

void
SVGTextDrawPathCallbacks::FillAndStrokeGeometry()
{
  bool pushedGroup = false;
  if (mColor == NS_40PERCENT_FOREGROUND_COLOR) {
    pushedGroup = true;
    gfx->PushGroup(gfxContentType::COLOR_ALPHA);
  }

  uint32_t paintOrder = mFrame->StyleSVG()->mPaintOrder;
  if (paintOrder == NS_STYLE_PAINT_ORDER_NORMAL) {
    FillGeometry();
    StrokeGeometry();
  } else {
    while (paintOrder) {
      uint32_t component =
        paintOrder & ((1 << NS_STYLE_PAINT_ORDER_BITWIDTH) - 1);
      switch (component) {
        case NS_STYLE_PAINT_ORDER_FILL:
          FillGeometry();
          break;
        case NS_STYLE_PAINT_ORDER_STROKE:
          StrokeGeometry();
          break;
      }
      paintOrder >>= NS_STYLE_PAINT_ORDER_BITWIDTH;
    }
  }

  if (pushedGroup) {
    gfx->PopGroupToSource();
    gfx->Paint(0.4);
  }
}

void
SVGTextDrawPathCallbacks::FillGeometry()
{
  if (SetFillColor()) {
    gfx->Fill();
  }
}

void
SVGTextDrawPathCallbacks::StrokeGeometry()
{
  if (mColor == NS_SAME_AS_FOREGROUND_COLOR ||
      mColor == NS_40PERCENT_FOREGROUND_COLOR) {
    // Don't paint the stroke when we are filling with a selection color.
    if (nsSVGUtils::SetupCairoStroke(mFrame, gfx)) {
      gfx->Stroke();
    }
  }
}

//----------------------------------------------------------------------
// SVGTextContextPaint methods:

already_AddRefed<gfxPattern>
SVGTextContextPaint::GetFillPattern(float aOpacity,
                                    const gfxMatrix& aCTM)
{
  return mFillPaint.GetPattern(aOpacity, &nsStyleSVG::mFill, aCTM);
}

already_AddRefed<gfxPattern>
SVGTextContextPaint::GetStrokePattern(float aOpacity,
                                      const gfxMatrix& aCTM)
{
  return mStrokePaint.GetPattern(aOpacity, &nsStyleSVG::mStroke, aCTM);
}

already_AddRefed<gfxPattern>
SVGTextContextPaint::Paint::GetPattern(float aOpacity,
                                       nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                       const gfxMatrix& aCTM)
{
  nsRefPtr<gfxPattern> pattern;
  if (mPatternCache.Get(aOpacity, getter_AddRefs(pattern))) {
    // Set the pattern matrix just in case it was messed with by a previous
    // caller. We should get the same matrix each time a pattern is constructed
    // so this should be fine.
    pattern->SetMatrix(aCTM * mPatternMatrix);
    return pattern.forget();
  }

  switch (mPaintType) {
  case eStyleSVGPaintType_None:
    pattern = new gfxPattern(gfxRGBA(0.0f, 0.0f, 0.0f, 0.0f));
    mPatternMatrix = gfxMatrix();
    break;
  case eStyleSVGPaintType_Color:
    pattern = new gfxPattern(gfxRGBA(NS_GET_R(mPaintDefinition.mColor) / 255.0,
                                     NS_GET_G(mPaintDefinition.mColor) / 255.0,
                                     NS_GET_B(mPaintDefinition.mColor) / 255.0,
                                     NS_GET_A(mPaintDefinition.mColor) / 255.0 * aOpacity));
    mPatternMatrix = gfxMatrix();
    break;
  case eStyleSVGPaintType_Server:
    pattern = mPaintDefinition.mPaintServerFrame->GetPaintServerPattern(mFrame,
                                                                        mContextMatrix,
                                                                        aFillOrStroke,
                                                                        aOpacity);
    {
      // m maps original-user-space to pattern space
      gfxMatrix m = pattern->GetMatrix();
      gfxMatrix deviceToOriginalUserSpace = mContextMatrix;
      deviceToOriginalUserSpace.Invert();
      // mPatternMatrix maps device space to pattern space via original user space
      mPatternMatrix = deviceToOriginalUserSpace * m;
    }
    pattern->SetMatrix(aCTM * mPatternMatrix);
    break;
  case eStyleSVGPaintType_ContextFill:
    pattern = mPaintDefinition.mContextPaint->GetFillPattern(aOpacity, aCTM);
    // Don't cache this. mContextPaint will have cached it anyway. If we
    // cache it, we'll have to compute mPatternMatrix, which is annoying.
    return pattern.forget();
  case eStyleSVGPaintType_ContextStroke:
    pattern = mPaintDefinition.mContextPaint->GetStrokePattern(aOpacity, aCTM);
    // Don't cache this. mContextPaint will have cached it anyway. If we
    // cache it, we'll have to compute mPatternMatrix, which is annoying.
    return pattern.forget();
  default:
    MOZ_ASSERT(false, "invalid paint type");
    return nullptr;
  }

  mPatternCache.Put(aOpacity, pattern);
  return pattern.forget();
}

} // namespace mozilla


// ============================================================================
// SVGTextFrame

// ----------------------------------------------------------------------------
// Display list item

class nsDisplaySVGText : public nsDisplayItem {
public:
  nsDisplaySVGText(nsDisplayListBuilder* aBuilder,
                   SVGTextFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame),
      mDisableSubpixelAA(false)
  {
    MOZ_COUNT_CTOR(nsDisplaySVGText);
    NS_ABORT_IF_FALSE(aFrame, "Must have a frame!");
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySVGText() {
    MOZ_COUNT_DTOR(nsDisplaySVGText);
  }
#endif

  NS_DISPLAY_DECL_NAME("nsDisplaySVGText", TYPE_SVG_TEXT)

  virtual void DisableComponentAlpha() MOZ_OVERRIDE {
    mDisableSubpixelAA = true;
  }
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) MOZ_OVERRIDE {
    bool snap;
    return GetBounds(aBuilder, &snap);
  }
private:
  bool mDisableSubpixelAA;
};

void
nsDisplaySVGText::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                          HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  SVGTextFrame *frame = static_cast<SVGTextFrame*>(mFrame);
  nsPoint pointRelativeToReferenceFrame = aRect.Center();
  // ToReferenceFrame() includes frame->GetPosition(), our user space position.
  nsPoint userSpacePt = pointRelativeToReferenceFrame -
                          (ToReferenceFrame() - frame->GetPosition());

  nsIFrame* target = frame->GetFrameForPoint(userSpacePt);
  if (target) {
    aOutFrames->AppendElement(target);
  }
}

void
nsDisplaySVGText::Paint(nsDisplayListBuilder* aBuilder,
                        nsRenderingContext* aCtx)
{
  gfxContextAutoDisableSubpixelAntialiasing
    disable(aCtx->ThebesContext(), mDisableSubpixelAA);

  // ToReferenceFrame includes our mRect offset, but painting takes
  // account of that too. To avoid double counting, we subtract that
  // here.
  nsPoint offset = ToReferenceFrame() - mFrame->GetPosition();

  aCtx->PushState();
  aCtx->Translate(offset);
  static_cast<SVGTextFrame*>(mFrame)->PaintSVG(aCtx, nullptr);
  aCtx->PopState();
}

// ---------------------------------------------------------------------
// nsQueryFrame methods

NS_QUERYFRAME_HEAD(SVGTextFrame)
  NS_QUERYFRAME_ENTRY(SVGTextFrame)
NS_QUERYFRAME_TAIL_INHERITING(SVGTextFrameBase)

// ---------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) SVGTextFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(SVGTextFrame)

// ---------------------------------------------------------------------
// nsIFrame methods

void
SVGTextFrame::Init(nsIContent*       aContent,
                   nsContainerFrame* aParent,
                   nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVG(nsGkAtoms::text), "Content is not an SVG text");

  SVGTextFrameBase::Init(aContent, aParent, aPrevInFlow);
  AddStateBits((aParent->GetStateBits() & NS_STATE_SVG_CLIPPATH_CHILD) |
               NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_SVG_TEXT);

  mMutationObserver.StartObserving(this);
}

void
SVGTextFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                               const nsRect& aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  if (NS_SUBTREE_DIRTY(this)) {
    // We can sometimes be asked to paint before reflow happens and we
    // have updated mPositions, etc.  In this case, we just avoid
    // painting.
    return;
  }
  aLists.Content()->AppendNewToTop(
    new (aBuilder) nsDisplaySVGText(aBuilder, this));
}

nsresult
SVGTextFrame::AttributeChanged(int32_t aNameSpaceID,
                               nsIAtom* aAttribute,
                               int32_t aModType)
{
  if (aNameSpaceID != kNameSpaceID_None)
    return NS_OK;

  if (aAttribute == nsGkAtoms::transform) {
    // We don't invalidate for transform changes (the layers code does that).
    // Also note that SVGTransformableElement::GetAttributeChangeHint will
    // return nsChangeHint_UpdateOverflow for "transform" attribute changes
    // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.

    if (!(mState & NS_FRAME_FIRST_REFLOW) &&
        mCanvasTM && mCanvasTM->IsSingular()) {
      // We won't have calculated the glyph positions correctly.
      NotifyGlyphMetricsChange();
    }
    mCanvasTM = nullptr;
  } else if (IsGlyphPositioningAttribute(aAttribute) ||
             aAttribute == nsGkAtoms::textLength ||
             aAttribute == nsGkAtoms::lengthAdjust) {
    NotifyGlyphMetricsChange();
  }

  return NS_OK;
}

nsIAtom *
SVGTextFrame::GetType() const
{
  return nsGkAtoms::svgTextFrame;
}

void
SVGTextFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  if (mState & NS_FRAME_IS_NONDISPLAY) {
    // We need this DidSetStyleContext override to handle cases like this:
    //
    //   <defs>
    //     <g>
    //       <mask>
    //         <text>...</text>
    //       </mask>
    //     </g>
    //   </defs>
    //
    // where the <text> is non-display, and a style change occurs on the <defs>,
    // the <g>, the <mask>, or the <text> itself.  If the style change happened
    // on the parent of the <defs>, then in
    // nsSVGDisplayContainerFrame::ReflowSVG, we would find the non-display
    // <defs> container and then call ReflowSVGNonDisplayText on it.  If we do
    // not actually reflow the parent of the <defs>, then without this
    // DidSetStyleContext we would (a) not cause the <text>'s anonymous block
    // child to be reflowed when it is next painted, and (b) not cause the
    // <text> to be repainted anyway since the user of the <mask> would not
    // know it needs to be repainted.
    ScheduleReflowSVGNonDisplayText();
  }
}

void
SVGTextFrame::ReflowSVGNonDisplayText()
{
  MOZ_ASSERT(nsSVGUtils::AnyOuterSVGIsCallingReflowSVG(this),
             "only call ReflowSVGNonDisplayText when an outer SVG frame is "
             "under ReflowSVG");
  MOZ_ASSERT(mState & NS_FRAME_IS_NONDISPLAY,
             "only call ReflowSVGNonDisplayText if the frame is "
             "NS_FRAME_IS_NONDISPLAY");

  // We had a style change, so we mark this frame as dirty so that the next
  // time it is painted, we reflow the anonymous block frame.
  AddStateBits(NS_FRAME_IS_DIRTY);

  // We also need to call InvalidateRenderingObservers, so that if the <text>
  // element is within a <mask>, say, the element referencing the <mask> will
  // be updated, which will then cause this SVGTextFrame to be painted and
  // in doing so cause the anonymous block frame to be reflowed.
  nsSVGEffects::InvalidateRenderingObservers(this);

  // Finally, we need to actually reflow the anonymous block frame and update
  // mPositions, in case we are being reflowed immediately after a DOM
  // mutation that needs frame reconstruction.
  MaybeReflowAnonymousBlockChild();
  UpdateGlyphPositioning();
}

void
SVGTextFrame::ScheduleReflowSVGNonDisplayText()
{
  MOZ_ASSERT(!nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
             "do not call ScheduleReflowSVGNonDisplayText when the outer SVG "
             "frame is under ReflowSVG");
  MOZ_ASSERT(!(mState & NS_STATE_SVG_TEXT_IN_REFLOW),
             "do not call ScheduleReflowSVGNonDisplayText while reflowing the "
             "anonymous block child");

  // We need to find an ancestor frame that we can call FrameNeedsReflow
  // on that will cause the document to be marked as needing relayout,
  // and for that ancestor (or some further ancestor) to be marked as
  // a root to reflow.  We choose the closest ancestor frame that is not
  // NS_FRAME_IS_NONDISPLAY and which is either an outer SVG frame or a
  // non-SVG frame.  (We don't consider displayed SVG frame ancestors toerh
  // than nsSVGOuterSVGFrame, since calling FrameNeedsReflow on those other
  // SVG frames would do a bunch of unnecessary work on the SVG frames up to
  // the nsSVGOuterSVGFrame.)

  nsIFrame* f = this;
  while (f) {
    if (!(f->GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {
      if (NS_SUBTREE_DIRTY(f)) {
        // This is a displayed frame, so if it is already dirty, we will be reflowed
        // soon anyway.  No need to call FrameNeedsReflow again, then.
        return;
      }
      if (!f->IsFrameOfType(eSVG) ||
          (f->GetStateBits() & NS_STATE_IS_OUTER_SVG)) {
        break;
      }
      f->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
    }
    f = f->GetParent();
  }

  MOZ_ASSERT(f, "should have found an ancestor frame to reflow");

  PresContext()->PresShell()->FrameNeedsReflow(
    f, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
}

NS_IMPL_ISUPPORTS(SVGTextFrame::MutationObserver, nsIMutationObserver)

void
SVGTextFrame::MutationObserver::ContentAppended(nsIDocument* aDocument,
                                                nsIContent* aContainer,
                                                nsIContent* aFirstNewContent,
                                                int32_t aNewIndexInContainer)
{
  mFrame->NotifyGlyphMetricsChange();
}

void
SVGTextFrame::MutationObserver::ContentInserted(
                                        nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        nsIContent* aChild,
                                        int32_t aIndexInContainer)
{
  mFrame->NotifyGlyphMetricsChange();
}

void
SVGTextFrame::MutationObserver::ContentRemoved(
                                       nsIDocument *aDocument,
                                       nsIContent* aContainer,
                                       nsIContent* aChild,
                                       int32_t aIndexInContainer,
                                       nsIContent* aPreviousSibling)
{
  mFrame->NotifyGlyphMetricsChange();
}

void
SVGTextFrame::MutationObserver::CharacterDataChanged(
                                                 nsIDocument* aDocument,
                                                 nsIContent* aContent,
                                                 CharacterDataChangeInfo* aInfo)
{
  mFrame->NotifyGlyphMetricsChange();
}

void
SVGTextFrame::MutationObserver::AttributeChanged(
                                                nsIDocument* aDocument,
                                                mozilla::dom::Element* aElement,
                                                int32_t aNameSpaceID,
                                                nsIAtom* aAttribute,
                                                int32_t aModType)
{
  if (!aElement->IsSVG()) {
    return;
  }

  // Attribute changes on this element are handled in
  // SVGTextFrame::AttributeChanged.
  if (aElement == mFrame->GetContent()) {
    return;
  }

  // Attributes changes on descendent elements.
  if (aElement->Tag() == nsGkAtoms::textPath) {
    if (aNameSpaceID == kNameSpaceID_None &&
        aAttribute == nsGkAtoms::startOffset) {
      mFrame->NotifyGlyphMetricsChange();
    } else if (aNameSpaceID == kNameSpaceID_XLink &&
               aAttribute == nsGkAtoms::href) {
      // Blow away our reference, if any
      nsIFrame* childElementFrame = aElement->GetPrimaryFrame();
      if (childElementFrame) {
        childElementFrame->Properties().Delete(nsSVGEffects::HrefProperty());
        mFrame->NotifyGlyphMetricsChange();
      }
    }
  } else {
    if (aNameSpaceID == kNameSpaceID_None &&
        IsGlyphPositioningAttribute(aAttribute)) {
      mFrame->NotifyGlyphMetricsChange();
    }
  }
}

void
SVGTextFrame::FindCloserFrameForSelection(
                                 nsPoint aPoint,
                                 nsIFrame::FrameWithDistance* aCurrentBestFrame)
{
  if (GetStateBits() & NS_FRAME_IS_NONDISPLAY) {
    return;
  }

  UpdateGlyphPositioning();

  nsPresContext* presContext = PresContext();

  // Find the frame that has the closest rendered run rect to aPoint.
  TextRenderedRunIterator it(this);
  for (TextRenderedRun run = it.Current(); run.mFrame; run = it.Next()) {
    uint32_t flags = TextRenderedRun::eIncludeFill |
                     TextRenderedRun::eIncludeStroke |
                     TextRenderedRun::eNoHorizontalOverflow;
    SVGBBox userRect = run.GetUserSpaceRect(presContext, flags);
    if (!userRect.IsEmpty()) {
      nsRect rect = nsSVGUtils::ToCanvasBounds(userRect.ToThebesRect(),
                                               GetCanvasTM(FOR_HIT_TESTING),
                                               presContext);

      if (nsLayoutUtils::PointIsCloserToRect(aPoint, rect,
                                             aCurrentBestFrame->mXDistance,
                                             aCurrentBestFrame->mYDistance)) {
        aCurrentBestFrame->mFrame = run.mFrame;
      }
    }
  }
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

void
SVGTextFrame::NotifySVGChanged(uint32_t aFlags)
{
  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  bool needNewBounds = false;
  bool needGlyphMetricsUpdate = false;
  bool needNewCanvasTM = false;

  if ((aFlags & COORD_CONTEXT_CHANGED) &&
      (mState & NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES)) {
    needGlyphMetricsUpdate = true;
  }

  if (aFlags & TRANSFORM_CHANGED) {
    needNewCanvasTM = true;
    if (mCanvasTM && mCanvasTM->IsSingular()) {
      // We won't have calculated the glyph positions correctly.
      needNewBounds = true;
      needGlyphMetricsUpdate = true;
    }
    if (StyleSVGReset()->mVectorEffect ==
        NS_STYLE_VECTOR_EFFECT_NON_SCALING_STROKE) {
      // Stroke currently contributes to our mRect, and our stroke depends on
      // the transform to our outer-<svg> if |vector-effect:non-scaling-stroke|.
      needNewBounds = true;
    }
  }

  // If the scale at which we computed our mFontSizeScaleFactor has changed by
  // at least a factor of two, reflow the text.  This avoids reflowing text
  // at every tick of a transform animation, but ensures our glyph metrics
  // do not get too far out of sync with the final font size on the screen.
  if (needNewCanvasTM && mLastContextScale != 0.0f) {
    mCanvasTM = nullptr;
    // If we are a non-display frame, then we don't want to call
    // GetCanvasTM(FOR_OUTERSVG_TM), since the context scale does not use it.
    gfxMatrix newTM =
      (mState & NS_FRAME_IS_NONDISPLAY) ? gfxMatrix() :
                                          GetCanvasTM(FOR_OUTERSVG_TM);
    // Compare the old and new context scales.
    float scale = GetContextScale(newTM);
    float change = scale / mLastContextScale;
    if (change >= 2.0f || change <= 0.5f) {
      needNewBounds = true;
      needGlyphMetricsUpdate = true;
    }
  }

  if (needNewBounds) {
    // Ancestor changes can't affect how we render from the perspective of
    // any rendering observers that we may have, so we don't need to
    // invalidate them. We also don't need to invalidate ourself, since our
    // changed ancestor will have invalidated its entire area, which includes
    // our area.
    ScheduleReflowSVG();
  }

  if (needGlyphMetricsUpdate) {
    // If we are positioned using percentage values we need to update our
    // position whenever our viewport's dimensions change.  But only do this if
    // we have been reflowed once, otherwise the glyph positioning will be
    // wrong.  (We need to wait until bidi reordering has been done.)
    if (!(mState & NS_FRAME_FIRST_REFLOW)) {
      NotifyGlyphMetricsChange();
    }
  }
}

/**
 * Gets the offset into a DOM node that the specified caret is positioned at.
 */
static int32_t
GetCaretOffset(nsCaret* aCaret)
{
  nsCOMPtr<nsISelection> selection = aCaret->GetCaretDOMSelection();
  if (!selection) {
    return -1;
  }

  int32_t offset = -1;
  selection->GetAnchorOffset(&offset);
  return offset;
}

/**
 * Returns whether the caret should be painted for a given TextRenderedRun
 * by checking whether the caret is in the range covered by the rendered run.
 *
 * @param aThisRun The TextRenderedRun to be painted.
 * @param aCaret The caret.
 */
static bool
ShouldPaintCaret(const TextRenderedRun& aThisRun, nsCaret* aCaret)
{
  int32_t caretOffset = GetCaretOffset(aCaret);

  if (caretOffset < 0) {
    return false;
  }

  if (uint32_t(caretOffset) >= aThisRun.mTextFrameContentOffset &&
      uint32_t(caretOffset) < aThisRun.mTextFrameContentOffset +
                                aThisRun.mTextFrameContentLength) {
    return true;
  }

  return false;
}

nsresult
SVGTextFrame::PaintSVG(nsRenderingContext* aContext,
                       const nsIntRect *aDirtyRect,
                       nsIFrame* aTransformRoot)
{
  nsIFrame* kid = GetFirstPrincipalChild();
  if (!kid)
    return NS_OK;

  nsPresContext* presContext = PresContext();

  gfxContext *gfx = aContext->ThebesContext();
  gfxMatrix initialMatrix = gfx->CurrentMatrix();

  if (mState & NS_FRAME_IS_NONDISPLAY) {
    // If we are in a canvas DrawWindow call that used the
    // DRAWWINDOW_DO_NOT_FLUSH flag, then we may still have out
    // of date frames.  Just don't paint anything if they are
    // dirty.
    if (presContext->PresShell()->InDrawWindowNotFlushing() &&
        NS_SUBTREE_DIRTY(this)) {
      return NS_OK;
    }
    // Text frames inside <clipPath>, <mask>, etc. will never have had
    // ReflowSVG called on them, so call UpdateGlyphPositioning to do this now.
    UpdateGlyphPositioning();
  } else if (NS_SUBTREE_DIRTY(this)) {
    // If we are asked to paint before reflow has recomputed mPositions etc.
    // directly via PaintSVG, rather than via a display list, then we need
    // to bail out here too.
    return NS_OK;
  }

  gfxMatrix canvasTM = GetCanvasTM(FOR_PAINTING, aTransformRoot);
  if (canvasTM.IsSingular()) {
    NS_WARNING("Can't render text element!");
    return NS_ERROR_FAILURE;
  }

  gfxMatrix matrixForPaintServers(canvasTM);
  matrixForPaintServers.Multiply(initialMatrix);

  // Check if we need to draw anything.
  if (aDirtyRect) {
    NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
                 (mState & NS_FRAME_IS_NONDISPLAY),
                 "Display lists handle dirty rect intersection test");
    nsRect dirtyRect(aDirtyRect->x, aDirtyRect->y,
                     aDirtyRect->width, aDirtyRect->height);

    gfxFloat appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
    gfxRect frameRect(mRect.x / appUnitsPerDevPixel,
                      mRect.y / appUnitsPerDevPixel,
                      mRect.width / appUnitsPerDevPixel,
                      mRect.height / appUnitsPerDevPixel);

    nsRect canvasRect = nsLayoutUtils::RoundGfxRectToAppRect(
        GetCanvasTM(FOR_OUTERSVG_TM).TransformBounds(frameRect), 1);
    if (!canvasRect.Intersects(dirtyRect)) {
      return NS_OK;
    }
  }

  // SVG paints in CSS px, but normally frames paint in dev pixels. Here we
  // multiply a CSS-px-to-dev-pixel factor onto canvasTM so our children paint
  // correctly.
  float cssPxPerDevPx = presContext->
    AppUnitsToFloatCSSPixels(presContext->AppUnitsPerDevPixel());
  gfxMatrix canvasTMForChildren = canvasTM;
  canvasTMForChildren.Scale(cssPxPerDevPx, cssPxPerDevPx);
  initialMatrix.Scale(1 / cssPxPerDevPx, 1 / cssPxPerDevPx);

  gfxContextAutoSaveRestore save(gfx);
  gfx->NewPath();
  gfx->Multiply(canvasTMForChildren);
  gfxMatrix currentMatrix = gfx->CurrentMatrix();

  nsRefPtr<nsCaret> caret = presContext->PresShell()->GetCaret();
  nsIFrame* caretFrame = caret->GetCaretFrame();

  TextRenderedRunIterator it(this, TextRenderedRunIterator::eVisibleFrames);
  TextRenderedRun run = it.Current();
  while (run.mFrame) {
    nsTextFrame* frame = run.mFrame;

    // Determine how much of the left and right edges of the text frame we
    // need to ignore.
    SVGCharClipDisplayItem item(run);

    // Set up the fill and stroke so that SVG glyphs can get painted correctly
    // when they use context-fill etc.
    gfx->SetMatrix(initialMatrix);
    gfxTextContextPaint *outerContextPaint =
      (gfxTextContextPaint*)aContext->GetUserData(&gfxTextContextPaint::sUserDataKey);

    nsAutoPtr<gfxTextContextPaint> contextPaint;
    DrawMode drawMode =
      SetupCairoState(gfx, frame, outerContextPaint,
                      getter_Transfers(contextPaint));

    // Set up the transform for painting the text frame for the substring
    // indicated by the run.
    gfxMatrix runTransform =
      run.GetTransformFromUserSpaceForPainting(presContext, item);
    runTransform.Multiply(currentMatrix);
    gfx->SetMatrix(runTransform);

    if (drawMode != DrawMode(0)) {
      nsRect frameRect = frame->GetVisualOverflowRect();
      bool paintSVGGlyphs;
      if (ShouldRenderAsPath(aContext, frame, paintSVGGlyphs)) {
        SVGTextDrawPathCallbacks callbacks(aContext, frame,
                                           matrixForPaintServers,
                                           paintSVGGlyphs);
        frame->PaintText(aContext, nsPoint(), frameRect, item,
                         contextPaint, &callbacks);
      } else {
        frame->PaintText(aContext, nsPoint(), frameRect, item,
                         contextPaint, nullptr);
      }
    }

    if (frame == caretFrame && ShouldPaintCaret(run, caret)) {
      // XXX Should we be looking at the fill/stroke colours to paint the
      // caret with, rather than using the color property?
      caret->PaintCaret(nullptr, aContext, frame, nsPoint());
      gfx->NewPath();
    }

    run = it.Next();
  }

  return NS_OK;
}

nsIFrame*
SVGTextFrame::GetFrameForPoint(const nsPoint& aPoint)
{
  NS_ASSERTION(GetFirstPrincipalChild(), "must have a child frame");

  if (mState & NS_FRAME_IS_NONDISPLAY) {
    // Text frames inside <clipPath> will never have had ReflowSVG called on
    // them, so call UpdateGlyphPositioning to do this now.  (Text frames
    // inside <mask> and other non-display containers will never need to
    // be hit tested.)
    UpdateGlyphPositioning();
  } else {
    NS_ASSERTION(!NS_SUBTREE_DIRTY(this), "reflow should have happened");
  }

  nsPresContext* presContext = PresContext();

  gfxPoint pointInOuterSVGUserUnits = AppUnitsToGfxUnits(aPoint, presContext);

  TextRenderedRunIterator it(this);
  nsIFrame* hit = nullptr;
  for (TextRenderedRun run = it.Current(); run.mFrame; run = it.Next()) {
    uint16_t hitTestFlags = nsSVGUtils::GetGeometryHitTestFlags(run.mFrame);
    if (!(hitTestFlags & (SVG_HIT_TEST_FILL | SVG_HIT_TEST_STROKE))) {
      continue;
    }

    gfxMatrix m = GetCanvasTM(FOR_HIT_TESTING);
    m.PreMultiply(run.GetTransformFromRunUserSpaceToUserSpace(presContext));
    m.Invert();

    gfxPoint pointInRunUserSpace = m.Transform(pointInOuterSVGUserUnits);
    gfxRect frameRect =
      run.GetRunUserSpaceRect(presContext, TextRenderedRun::eIncludeFill |
                                           TextRenderedRun::eIncludeStroke).ToThebesRect();

    if (Inside(frameRect, pointInRunUserSpace) &&
        nsSVGUtils::HitTestClip(this, aPoint)) {
      hit = run.mFrame;
    }
  }
  return hit;
}

nsRect
SVGTextFrame::GetCoveredRegion()
{
  return nsSVGUtils::TransformFrameRectToOuterSVG(
           mRect, GetCanvasTM(FOR_OUTERSVG_TM), PresContext());
}

void
SVGTextFrame::ReflowSVG()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probaby a wasteful mistake");

  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_FRAME_IS_NONDISPLAY),
                    "ReflowSVG mechanism not designed for this");

  if (!nsSVGUtils::NeedsReflowSVG(this)) {
    NS_ASSERTION(!(mState & NS_STATE_SVG_POSITIONING_DIRTY), "How did this happen?");
    return;
  }

  MaybeReflowAnonymousBlockChild();
  UpdateGlyphPositioning();

  nsPresContext* presContext = PresContext();

  SVGBBox r;
  TextRenderedRunIterator it(this, TextRenderedRunIterator::eAllFrames);
  for (TextRenderedRun run = it.Current(); run.mFrame; run = it.Next()) {
    uint32_t runFlags = 0;
    if (run.mFrame->StyleSVG()->mFill.mType != eStyleSVGPaintType_None) {
      runFlags |= TextRenderedRun::eIncludeFill |
                  TextRenderedRun::eIncludeTextShadow;
    }
    if (nsSVGUtils::HasStroke(run.mFrame)) {
      runFlags |= TextRenderedRun::eIncludeFill |
                  TextRenderedRun::eIncludeTextShadow;
    }
    // Our "visual" overflow rect needs to be valid for building display lists
    // for hit testing, which means that for certain values of 'pointer-events'
    // it needs to include the geometry of the fill or stroke even when the fill/
    // stroke don't actually render (e.g. when stroke="none" or
    // stroke-opacity="0"). GetGeometryHitTestFlags accounts for 'pointer-events'.
    // The text-shadow is not part of the hit-test area.
    uint16_t hitTestFlags = nsSVGUtils::GetGeometryHitTestFlags(run.mFrame);
    if (hitTestFlags & SVG_HIT_TEST_FILL) {
      runFlags |= TextRenderedRun::eIncludeFill;
    }
    if (hitTestFlags & SVG_HIT_TEST_STROKE) {
      runFlags |= TextRenderedRun::eIncludeStroke;
    }

    if (runFlags) {
      r.UnionEdges(run.GetUserSpaceRect(presContext, runFlags));
    }
  }

  if (r.IsEmpty()) {
    mRect.SetEmpty();
  } else {
    mRect =
      nsLayoutUtils::RoundGfxRectToAppRect(r.ToThebesRect(), presContext->AppUnitsPerCSSPixel());

    // Due to rounding issues when we have a transform applied, we sometimes
    // don't include an additional row of pixels.  For now, just inflate our
    // covered region.
    mRect.Inflate(presContext->AppUnitsPerDevPixel());
  }

  if (mState & NS_FRAME_FIRST_REFLOW) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    nsSVGEffects::UpdateEffects(this);
  }

  nsRect overflow = nsRect(nsPoint(0,0), mRect.Size());
  nsOverflowAreas overflowAreas(overflow, overflow);
  FinishAndStoreOverflow(overflowAreas, mRect.Size());

  // Now unset the various reflow bits:
  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);

  // XXX nsSVGContainerFrame::ReflowSVG only looks at its nsISVGChildFrame
  // children, and calls ConsiderChildOverflow on them.  Does it matter
  // that ConsiderChildOverflow won't be called on our children?
  SVGTextFrameBase::ReflowSVG();
}

/**
 * Converts nsSVGUtils::eBBox* flags into TextRenderedRun flags appropriate
 * for the specified rendered run.
 */
static uint32_t
TextRenderedRunFlagsForBBoxContribution(const TextRenderedRun& aRun,
                                        uint32_t aBBoxFlags)
{
  uint32_t flags = 0;
  if ((aBBoxFlags & nsSVGUtils::eBBoxIncludeFillGeometry) ||
      ((aBBoxFlags & nsSVGUtils::eBBoxIncludeFill) &&
       aRun.mFrame->StyleSVG()->mFill.mType != eStyleSVGPaintType_None)) {
    flags |= TextRenderedRun::eIncludeFill;
  }
  if ((aBBoxFlags & nsSVGUtils::eBBoxIncludeStrokeGeometry) ||
      ((aBBoxFlags & nsSVGUtils::eBBoxIncludeStroke) &&
       nsSVGUtils::HasStroke(aRun.mFrame))) {
    flags |= TextRenderedRun::eIncludeStroke;
  }
  return flags;
}

SVGBBox
SVGTextFrame::GetBBoxContribution(const gfx::Matrix &aToBBoxUserspace,
                                  uint32_t aFlags)
{
  NS_ASSERTION(GetFirstPrincipalChild(), "must have a child frame");

  UpdateGlyphPositioning();

  SVGBBox bbox;
  nsPresContext* presContext = PresContext();

  TextRenderedRunIterator it(this);
  for (TextRenderedRun run = it.Current(); run.mFrame; run = it.Next()) {
    uint32_t flags = TextRenderedRunFlagsForBBoxContribution(run, aFlags);
    gfxMatrix m = ThebesMatrix(aToBBoxUserspace);
    SVGBBox bboxForRun =
      run.GetUserSpaceRect(presContext, flags, &m);
    bbox.UnionEdges(bboxForRun);
  }

  return bbox;
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods

gfxMatrix
SVGTextFrame::GetCanvasTM(uint32_t aFor, nsIFrame* aTransformRoot)
{
  if (!(GetStateBits() & NS_FRAME_IS_NONDISPLAY) &&
      !aTransformRoot) {
    if ((aFor == FOR_PAINTING && NS_SVGDisplayListPaintingEnabled()) ||
        (aFor == FOR_HIT_TESTING && NS_SVGDisplayListHitTestingEnabled())) {
      return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(this);
    }
  }
  if (!mCanvasTM) {
    NS_ASSERTION(GetParent(), "null parent");
    NS_ASSERTION(!(aFor == FOR_OUTERSVG_TM &&
                   (GetStateBits() & NS_FRAME_IS_NONDISPLAY)),
                 "should not call GetCanvasTM(FOR_OUTERSVG_TM) when we are "
                 "non-display");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(GetParent());
    dom::SVGTextContentElement *content = static_cast<dom::SVGTextContentElement*>(mContent);

    gfxMatrix tm = content->PrependLocalTransformsTo(
        this == aTransformRoot ? gfxMatrix() :
                                 parent->GetCanvasTM(aFor, aTransformRoot));

    mCanvasTM = new gfxMatrix(tm);
  }
  return *mCanvasTM;
}

//----------------------------------------------------------------------
// SVGTextFrame SVG DOM methods

/**
 * Returns whether the specified node has any non-empty nsTextNodes
 * beneath it.
 */
static bool
HasTextContent(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "expected non-null aContent");

  TextNodeIterator it(aContent);
  for (nsTextNode* text = it.Current(); text; text = it.Next()) {
    if (text->TextLength() != 0) {
      return true;
    }
  }
  return false;
}

/**
 * Returns the number of DOM characters beneath the specified node.
 */
static uint32_t
GetTextContentLength(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "expected non-null aContent");

  uint32_t length = 0;
  TextNodeIterator it(aContent);
  for (nsTextNode* text = it.Current(); text; text = it.Next()) {
    length += text->TextLength();
  }
  return length;
}

int32_t
SVGTextFrame::ConvertTextElementCharIndexToAddressableIndex(
                                                           int32_t aIndex,
                                                           nsIContent* aContent)
{
  CharIterator it(this, CharIterator::eOriginal, aContent);
  if (!it.AdvanceToSubtree()) {
    return -1;
  }
  int32_t result = 0;
  int32_t textElementCharIndex;
  while (!it.AtEnd() &&
         it.IsWithinSubtree()) {
    bool addressable = !it.IsOriginalCharUnaddressable();
    textElementCharIndex = it.TextElementCharIndex();
    it.Next();
    uint32_t delta = it.TextElementCharIndex() - textElementCharIndex;
    aIndex -= delta;
    if (addressable) {
      if (aIndex < 0) {
        return result;
      }
      result += delta;
    }
  }
  return -1;
}

/**
 * Implements the SVG DOM GetNumberOfChars method for the specified
 * text content element.
 */
uint32_t
SVGTextFrame::GetNumberOfChars(nsIContent* aContent)
{
  UpdateGlyphPositioning();

  uint32_t n = 0;
  CharIterator it(this, CharIterator::eAddressable, aContent);
  if (it.AdvanceToSubtree()) {
    while (!it.AtEnd() && it.IsWithinSubtree()) {
      n++;
      it.Next();
    }
  }
  return n;
}

/**
 * Implements the SVG DOM GetComputedTextLength method for the specified
 * text child element.
 */
float
SVGTextFrame::GetComputedTextLength(nsIContent* aContent)
{
  UpdateGlyphPositioning();

  float cssPxPerDevPx = PresContext()->
    AppUnitsToFloatCSSPixels(PresContext()->AppUnitsPerDevPixel());

  nscoord length = 0;
  TextRenderedRunIterator it(this, TextRenderedRunIterator::eAllFrames,
                             aContent);
  for (TextRenderedRun run = it.Current(); run.mFrame; run = it.Next()) {
    length += run.GetAdvanceWidth();
  }

  return PresContext()->AppUnitsToGfxUnits(length) *
           cssPxPerDevPx * mLengthAdjustScaleFactor / mFontSizeScaleFactor;
}

/**
 * Implements the SVG DOM SelectSubString method for the specified
 * text content element.
 */
nsresult
SVGTextFrame::SelectSubString(nsIContent* aContent,
                              uint32_t charnum, uint32_t nchars)
{
  UpdateGlyphPositioning();

  // Convert charnum/nchars from addressable characters relative to
  // aContent to global character indices.
  CharIterator chit(this, CharIterator::eAddressable, aContent);
  if (!chit.AdvanceToSubtree() ||
      !chit.Next(charnum) ||
      chit.IsAfterSubtree()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }
  charnum = chit.TextElementCharIndex();
  nsIContent* content = chit.TextFrame()->GetContent();
  chit.NextWithinSubtree(nchars);
  nchars = chit.TextElementCharIndex() - charnum;

  nsRefPtr<nsFrameSelection> frameSelection = GetFrameSelection();

  frameSelection->HandleClick(content, charnum, charnum + nchars,
                              false, false, false);
  return NS_OK;
}

/**
 * Implements the SVG DOM GetSubStringLength method for the specified
 * text content element.
 */
nsresult
SVGTextFrame::GetSubStringLength(nsIContent* aContent,
                                 uint32_t charnum, uint32_t nchars,
                                 float* aResult)
{
  UpdateGlyphPositioning();

  // Convert charnum/nchars from addressable characters relative to
  // aContent to global character indices.
  CharIterator chit(this, CharIterator::eAddressable, aContent);
  if (!chit.AdvanceToSubtree() ||
      !chit.Next(charnum) ||
      chit.IsAfterSubtree()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  if (nchars == 0) {
    *aResult = 0.0f;
    return NS_OK;
  }

  charnum = chit.TextElementCharIndex();
  chit.NextWithinSubtree(nchars);
  nchars = chit.TextElementCharIndex() - charnum;

  // Find each rendered run that intersects with the range defined
  // by charnum/nchars.
  nscoord textLength = 0;
  TextRenderedRunIterator it(this, TextRenderedRunIterator::eAllFrames);
  TextRenderedRun run = it.Current();
  while (run.mFrame) {
    // If this rendered run is past the substring we are interested in, we
    // are done.
    uint32_t offset = run.mTextElementCharIndex;
    if (offset >= charnum + nchars) {
      break;
    }

    // Intersect the substring we are interested in with the range covered by
    // the rendered run.
    uint32_t length = run.mTextFrameContentLength;
    IntersectInterval(offset, length, charnum, nchars);

    if (length != 0) {
      // Convert offset into an index into the frame.
      offset += run.mTextFrameContentOffset - run.mTextElementCharIndex;

      gfxSkipCharsIterator it =
        run.mFrame->EnsureTextRun(nsTextFrame::eInflated);
      gfxTextRun* textRun = run.mFrame->GetTextRun(nsTextFrame::eInflated);
      ConvertOriginalToSkipped(it, offset, length);

      // Accumulate the advance.
      textLength += textRun->GetAdvanceWidth(offset, length, nullptr);
    }

    run = it.Next();
  }

  nsPresContext* presContext = PresContext();
  float cssPxPerDevPx = presContext->
    AppUnitsToFloatCSSPixels(presContext->AppUnitsPerDevPixel());

  *aResult = presContext->AppUnitsToGfxUnits(textLength) *
               cssPxPerDevPx / mFontSizeScaleFactor;
  return NS_OK;
}

/**
 * Implements the SVG DOM GetCharNumAtPosition method for the specified
 * text content element.
 */
int32_t
SVGTextFrame::GetCharNumAtPosition(nsIContent* aContent,
                                   mozilla::nsISVGPoint* aPoint)
{
  UpdateGlyphPositioning();

  nsPresContext* context = PresContext();

  gfxPoint p(aPoint->X(), aPoint->Y());

  int32_t result = -1;

  TextRenderedRunIterator it(this, TextRenderedRunIterator::eAllFrames, aContent);
  for (TextRenderedRun run = it.Current(); run.mFrame; run = it.Next()) {
    // Hit test this rendered run.  Later runs will override earlier ones.
    int32_t index = run.GetCharNumAtPosition(context, p);
    if (index != -1) {
      result = index + run.mTextElementCharIndex;
    }
  }

  if (result == -1) {
    return result;
  }

  return ConvertTextElementCharIndexToAddressableIndex(result, aContent);
}

/**
 * Implements the SVG DOM GetStartPositionOfChar method for the specified
 * text content element.
 */
nsresult
SVGTextFrame::GetStartPositionOfChar(nsIContent* aContent,
                                     uint32_t aCharNum,
                                     mozilla::nsISVGPoint** aResult)
{
  UpdateGlyphPositioning();

  CharIterator it(this, CharIterator::eAddressable, aContent);
  if (!it.AdvanceToSubtree() ||
      !it.Next(aCharNum)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // We need to return the start position of the whole glyph.
  uint32_t startIndex = it.GlyphStartTextElementCharIndex();

  NS_ADDREF(*aResult =
    new DOMSVGPoint(ToPoint(mPositions[startIndex].mPosition)));
  return NS_OK;
}

/**
 * Implements the SVG DOM GetEndPositionOfChar method for the specified
 * text content element.
 */
nsresult
SVGTextFrame::GetEndPositionOfChar(nsIContent* aContent,
                                   uint32_t aCharNum,
                                   mozilla::nsISVGPoint** aResult)
{
  UpdateGlyphPositioning();

  CharIterator it(this, CharIterator::eAddressable, aContent);
  if (!it.AdvanceToSubtree() ||
      !it.Next(aCharNum)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // We need to return the end position of the whole glyph.
  uint32_t startIndex = it.GlyphStartTextElementCharIndex();

  // Get the advance of the glyph.
  gfxFloat advance = it.GetGlyphAdvance(PresContext());
  if (it.TextRun()->IsRightToLeft()) {
    advance = -advance;
  }

  // The end position is the start position plus the advance in the direction
  // of the glyph's rotation.
  Matrix m =
    Matrix::Rotation(mPositions[startIndex].mAngle) *
    Matrix::Translation(ToPoint(mPositions[startIndex].mPosition));
  Point p = m * Point(advance / mFontSizeScaleFactor, 0);

  NS_ADDREF(*aResult = new DOMSVGPoint(p));
  return NS_OK;
}

/**
 * Implements the SVG DOM GetExtentOfChar method for the specified
 * text content element.
 */
nsresult
SVGTextFrame::GetExtentOfChar(nsIContent* aContent,
                              uint32_t aCharNum,
                              dom::SVGIRect** aResult)
{
  UpdateGlyphPositioning();

  CharIterator it(this, CharIterator::eAddressable, aContent);
  if (!it.AdvanceToSubtree() ||
      !it.Next(aCharNum)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsPresContext* presContext = PresContext();

  float cssPxPerDevPx = presContext->
    AppUnitsToFloatCSSPixels(presContext->AppUnitsPerDevPixel());

  // We need to return the extent of the whole glyph.
  uint32_t startIndex = it.GlyphStartTextElementCharIndex();

  // The ascent and descent gives the height of the glyph.
  gfxFloat ascent, descent;
  GetAscentAndDescentInAppUnits(it.TextFrame(), ascent, descent);

  // Get the advance of the glyph.
  gfxFloat advance = it.GetGlyphAdvance(presContext);
  gfxFloat x = it.TextRun()->IsRightToLeft() ? -advance : 0.0;

  // The horizontal extent is the origin of the glyph plus the advance
  // in the direction of the glyph's rotation.
  gfxMatrix m;
  m.Translate(mPositions[startIndex].mPosition);
  m.Rotate(mPositions[startIndex].mAngle);
  m.Scale(1 / mFontSizeScaleFactor, 1 / mFontSizeScaleFactor);

  gfxRect glyphRect
    (x, -presContext->AppUnitsToGfxUnits(ascent) * cssPxPerDevPx,
     advance, presContext->AppUnitsToGfxUnits(ascent + descent) * cssPxPerDevPx);

  // Transform the glyph's rect into user space.
  gfxRect r = m.TransformBounds(glyphRect);

  NS_ADDREF(*aResult = new dom::SVGRect(aContent, r.x, r.y, r.width, r.height));
  return NS_OK;
}

/**
 * Implements the SVG DOM GetRotationOfChar method for the specified
 * text content element.
 */
nsresult
SVGTextFrame::GetRotationOfChar(nsIContent* aContent,
                                uint32_t aCharNum,
                                float* aResult)
{
  UpdateGlyphPositioning();

  CharIterator it(this, CharIterator::eAddressable, aContent);
  if (!it.AdvanceToSubtree() ||
      !it.Next(aCharNum)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *aResult = mPositions[it.TextElementCharIndex()].mAngle * 180.0 / M_PI;
  return NS_OK;
}

//----------------------------------------------------------------------
// SVGTextFrame text layout methods

/**
 * Given the character position array before values have been filled in
 * to any unspecified positions, and an array of dx/dy values, returns whether
 * a character at a given index should start a new rendered run.
 *
 * @param aPositions The array of character positions before unspecified
 *   positions have been filled in and dx/dy values have been added to them.
 * @param aDeltas The array of dx/dy values.
 * @param aIndex The character index in question.
 */
static bool
ShouldStartRunAtIndex(const nsTArray<CharPosition>& aPositions,
                      const nsTArray<gfxPoint>& aDeltas,
                      uint32_t aIndex)
{
  if (aIndex == 0) {
    return true;
  }

  if (aIndex < aPositions.Length()) {
    // If an explicit x or y value was given, start a new run.
    if (aPositions[aIndex].IsXSpecified() ||
        aPositions[aIndex].IsYSpecified()) {
      return true;
    }

    // If a non-zero rotation was given, or the previous character had a non-
    // zero rotation, start a new run.
    if ((aPositions[aIndex].IsAngleSpecified() &&
         aPositions[aIndex].mAngle != 0.0f) ||
        (aPositions[aIndex - 1].IsAngleSpecified() &&
         (aPositions[aIndex - 1].mAngle != 0.0f))) {
      return true;
    }
  }

  if (aIndex < aDeltas.Length()) {
    // If a non-zero dx or dy value was given, start a new run.
    if (aDeltas[aIndex].x != 0.0 ||
        aDeltas[aIndex].y != 0.0) {
      return true;
    }
  }

  return false;
}

uint32_t
SVGTextFrame::ResolvePositions(nsIContent* aContent,
                               uint32_t aIndex,
                               bool aInTextPath,
                               bool& aForceStartOfChunk,
                               nsTArray<gfxPoint>& aDeltas)
{
  if (aContent->IsNodeOfType(nsINode::eTEXT)) {
    // We found a text node.
    uint32_t length = static_cast<nsTextNode*>(aContent)->TextLength();
    if (length) {
      if (aForceStartOfChunk) {
        // Note this character as starting a new anchored chunk.
        mPositions[aIndex].mStartOfChunk = true;
        aForceStartOfChunk = false;
      }
      uint32_t end = aIndex + length;
      while (aIndex < end) {
        // Record whether each of these characters should start a new rendered
        // run.  That is always the case for characters on a text path.
        //
        // Run boundaries due to rotate="" values are handled in
        // DoGlyphPositioning.
        if (aInTextPath || ShouldStartRunAtIndex(mPositions, aDeltas, aIndex)) {
          mPositions[aIndex].mRunBoundary = true;
        }
        aIndex++;
      }
    }
    return aIndex;
  }

  // Skip past elements that aren't text content elements.
  if (!IsTextContentElement(aContent)) {
    return aIndex;
  }

  if (aContent->Tag() == nsGkAtoms::textPath) {
    // <textPath> elements are as if they are specified with x="0" y="0", but
    // only if they actually have some text content.
    if (HasTextContent(aContent)) {
      mPositions[aIndex].mPosition = gfxPoint();
      mPositions[aIndex].mStartOfChunk = true;
    }
  } else if (aContent->Tag() != nsGkAtoms::a) {
    // We have a text content element that can have x/y/dx/dy/rotate attributes.
    nsSVGElement* element = static_cast<nsSVGElement*>(aContent);

    // Get x, y, dx, dy.
    SVGUserUnitList x, y, dx, dy;
    element->GetAnimatedLengthListValues(&x, &y, &dx, &dy);

    // Get rotate.
    const SVGNumberList* rotate = nullptr;
    SVGAnimatedNumberList* animatedRotate =
      element->GetAnimatedNumberList(nsGkAtoms::rotate);
    if (animatedRotate) {
      rotate = &animatedRotate->GetAnimValue();
    }

    uint32_t count = GetTextContentLength(aContent);
    bool percentages = false;

    // New text anchoring chunks start at each character assigned a position
    // with x="" or y="", or if we forced one with aForceStartOfChunk due to
    // being just after a <textPath>.
    uint32_t newChunkCount = std::max(x.Length(), y.Length());
    if (!newChunkCount && aForceStartOfChunk) {
      newChunkCount = 1;
    }
    for (uint32_t i = 0, j = 0; i < newChunkCount && j < count; j++) {
      if (!mPositions[aIndex + j].mUnaddressable) {
        mPositions[aIndex + j].mStartOfChunk = true;
        i++;
      }
    }

    // Copy dx="" and dy="" values into aDeltas.
    if (!dx.IsEmpty() || !dy.IsEmpty()) {
      // Any unspecified deltas when we grow the array just get left as 0s.
      aDeltas.EnsureLengthAtLeast(aIndex + count);
      for (uint32_t i = 0, j = 0; i < dx.Length() && j < count; j++) {
        if (!mPositions[aIndex + j].mUnaddressable) {
          aDeltas[aIndex + j].x = dx[i];
          percentages = percentages || dx.HasPercentageValueAt(i);
          i++;
        }
      }
      for (uint32_t i = 0, j = 0; i < dy.Length() && j < count; j++) {
        if (!mPositions[aIndex + j].mUnaddressable) {
          aDeltas[aIndex + j].y = dy[i];
          percentages = percentages || dy.HasPercentageValueAt(i);
          i++;
        }
      }
    }

    // Copy x="" and y="" values.
    for (uint32_t i = 0, j = 0; i < x.Length() && j < count; j++) {
      if (!mPositions[aIndex + j].mUnaddressable) {
        mPositions[aIndex + j].mPosition.x = x[i];
        percentages = percentages || x.HasPercentageValueAt(i);
        i++;
      }
    }
    for (uint32_t i = 0, j = 0; i < y.Length() && j < count; j++) {
      if (!mPositions[aIndex + j].mUnaddressable) {
        mPositions[aIndex + j].mPosition.y = y[i];
        percentages = percentages || y.HasPercentageValueAt(i);
        i++;
      }
    }

    // Copy rotate="" values.
    if (rotate && !rotate->IsEmpty()) {
      uint32_t i = 0, j = 0;
      while (i < rotate->Length() && j < count) {
        if (!mPositions[aIndex + j].mUnaddressable) {
          mPositions[aIndex + j].mAngle = M_PI * (*rotate)[i] / 180.0;
          i++;
        }
        j++;
      }
      // Propagate final rotate="" value to the end of this element.
      while (j < count) {
        mPositions[aIndex + j].mAngle = mPositions[aIndex + j - 1].mAngle;
        j++;
      }
    }

    if (percentages) {
      AddStateBits(NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES);
    }
  }

  // Recurse to children.
  bool inTextPath = aInTextPath || aContent->Tag() == nsGkAtoms::textPath;
  for (nsIContent* child = aContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    aIndex = ResolvePositions(child, aIndex, inTextPath, aForceStartOfChunk,
                              aDeltas);
  }

  if (aContent->Tag() == nsGkAtoms::textPath) {
    // Force a new anchored chunk just after a <textPath>.
    aForceStartOfChunk = true;
  }

  return aIndex;
}

bool
SVGTextFrame::ResolvePositions(nsTArray<gfxPoint>& aDeltas,
                               bool aRunPerGlyph)
{
  NS_ASSERTION(mPositions.IsEmpty(), "expected mPositions to be empty");
  RemoveStateBits(NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES);

  CharIterator it(this, CharIterator::eOriginal);
  if (it.AtEnd()) {
    return false;
  }

  // We assume the first character position is (0,0) unless we later see
  // otherwise, and note it as unaddressable if it is.
  bool firstCharUnaddressable = it.IsOriginalCharUnaddressable();
  mPositions.AppendElement(CharPosition::Unspecified(firstCharUnaddressable));

  // Fill in unspecified positions for all remaining characters, noting
  // them as unaddressable if they are.
  uint32_t index = 0;
  while (it.Next()) {
    while (++index < it.TextElementCharIndex()) {
      mPositions.AppendElement(CharPosition::Unspecified(false));
    }
    mPositions.AppendElement(CharPosition::Unspecified(
                                             it.IsOriginalCharUnaddressable()));
  }
  while (++index < it.TextElementCharIndex()) {
    mPositions.AppendElement(CharPosition::Unspecified(false));
  }

  // Recurse over the content and fill in character positions as we go.
  bool forceStartOfChunk = false;
  return ResolvePositions(mContent, 0, aRunPerGlyph,
                          forceStartOfChunk, aDeltas) != 0;
}

void
SVGTextFrame::DetermineCharPositions(nsTArray<nsPoint>& aPositions)
{
  NS_ASSERTION(aPositions.IsEmpty(), "expected aPositions to be empty");

  nsPoint position, lastPosition;

  TextFrameIterator frit(this);
  for (nsTextFrame* frame = frit.Current(); frame; frame = frit.Next()) {
    gfxSkipCharsIterator it = frame->EnsureTextRun(nsTextFrame::eInflated);
    gfxTextRun* textRun = frame->GetTextRun(nsTextFrame::eInflated);

    // Reset the position to the new frame's position.
    position = frit.Position();
    if (textRun->IsRightToLeft()) {
      position.x += frame->GetRect().width;
    }
    position.y += GetBaselinePosition(frame, textRun, frit.DominantBaseline());

    // Any characters not in a frame, e.g. when display:none.
    for (uint32_t i = 0; i < frit.UndisplayedCharacters(); i++) {
      aPositions.AppendElement(position);
    }

    // Any white space characters trimmed at the start of the line of text.
    nsTextFrame::TrimmedOffsets trimmedOffsets =
      frame->GetTrimmedOffsets(frame->GetContent()->GetText(), true);
    while (it.GetOriginalOffset() < trimmedOffsets.mStart) {
      aPositions.AppendElement(position);
      it.AdvanceOriginal(1);
    }

    // If a ligature was started in the previous frame, we should record
    // the ligature's start position, not any partial position.
    while (it.GetOriginalOffset() < frame->GetContentEnd() &&
           !it.IsOriginalCharSkipped() &&
           (!textRun->IsLigatureGroupStart(it.GetSkippedOffset()) ||
            !textRun->IsClusterStart(it.GetSkippedOffset()))) {
      nscoord advance = textRun->GetAdvanceWidth(it.GetSkippedOffset(), 1,
                                                 nullptr);
      position.x += textRun->IsRightToLeft() ? -advance : advance;
      aPositions.AppendElement(lastPosition);
      it.AdvanceOriginal(1);
    }

    // The meat of the text frame.
    while (it.GetOriginalOffset() < frame->GetContentEnd()) {
      aPositions.AppendElement(position);
      if (!it.IsOriginalCharSkipped() &&
          textRun->IsLigatureGroupStart(it.GetSkippedOffset()) &&
          textRun->IsClusterStart(it.GetSkippedOffset())) {
        // A real visible character.
        uint32_t length = ClusterLength(textRun, it);
        nscoord advance = textRun->GetAdvanceWidth(it.GetSkippedOffset(),
                                                   length, nullptr);
        position.x += textRun->IsRightToLeft() ? -advance : advance;
        lastPosition = position;
      }
      it.AdvanceOriginal(1);
    }
  }

  // Finally any characters at the end that are not in a frame.
  for (uint32_t i = 0; i < frit.UndisplayedCharacters(); i++) {
    aPositions.AppendElement(position);
  }
}

/**
 * Physical text-anchor values.
 */
enum TextAnchorSide {
  eAnchorLeft,
  eAnchorMiddle,
  eAnchorRight
};

/**
 * Converts a logical text-anchor value to its physical value, based on whether
 * it is for an RTL frame.
 */
static TextAnchorSide
ConvertLogicalTextAnchorToPhysical(uint8_t aTextAnchor, bool aIsRightToLeft)
{
  NS_ASSERTION(aTextAnchor <= 3, "unexpected value for aTextAnchor");
  if (!aIsRightToLeft)
    return TextAnchorSide(aTextAnchor);
  return TextAnchorSide(2 - aTextAnchor);
}

/**
 * Shifts the recorded character positions for an anchored chunk.
 *
 * @param aCharPositions The recorded character positions.
 * @param aChunkStart The character index the starts the anchored chunk.  This
 *   character's initial position is the anchor point.
 * @param aChunkEnd The character index just after the end of the anchored
 *   chunk.
 * @param aLeftEdge The left-most edge of any of the glyphs within the
 *   anchored chunk.
 * @param aRightEdge The right-most edge of any of the glyphs within the
 *   anchored chunk.
 * @param aAnchorSide The direction to anchor.
 */
static void
ShiftAnchoredChunk(nsTArray<mozilla::CharPosition>& aCharPositions,
                   uint32_t aChunkStart,
                   uint32_t aChunkEnd,
                   gfxFloat aLeftEdge,
                   gfxFloat aRightEdge,
                   TextAnchorSide aAnchorSide)
{
  NS_ASSERTION(aLeftEdge <= aRightEdge, "unexpected anchored chunk edges");
  NS_ASSERTION(aChunkStart < aChunkEnd, "unexpected values for aChunkStart and "
                                        "aChunkEnd");

  gfxFloat shift = aCharPositions[aChunkStart].mPosition.x;
  switch (aAnchorSide) {
    case eAnchorLeft:
      shift -= aLeftEdge;
      break;
    case eAnchorMiddle:
      shift -= (aLeftEdge + aRightEdge) / 2;
      break;
    case eAnchorRight:
      shift -= aRightEdge;
      break;
    default:
      NS_NOTREACHED("unexpected value for aAnchorSide");
  }

  if (shift != 0.0) {
    for (uint32_t i = aChunkStart; i < aChunkEnd; i++) {
      aCharPositions[i].mPosition.x += shift;
    }
  }
}

void
SVGTextFrame::AdjustChunksForLineBreaks()
{
  nsBlockFrame* block = nsLayoutUtils::GetAsBlock(GetFirstPrincipalChild());
  NS_ASSERTION(block, "expected block frame");

  nsBlockFrame::line_iterator line = block->begin_lines();

  CharIterator it(this, CharIterator::eOriginal);
  while (!it.AtEnd() && line != block->end_lines()) {
    if (it.TextFrame() == line->mFirstChild) {
      mPositions[it.TextElementCharIndex()].mStartOfChunk = true;
      line++;
    }
    it.AdvancePastCurrentFrame();
  }
}

void
SVGTextFrame::AdjustPositionsForClusters()
{
  nsPresContext* presContext = PresContext();

  CharIterator it(this, CharIterator::eClusterOrLigatureGroupMiddle);
  while (!it.AtEnd()) {
    // Find the start of the cluster/ligature group.
    uint32_t charIndex = it.TextElementCharIndex();
    uint32_t startIndex = it.GlyphStartTextElementCharIndex();

    mPositions[charIndex].mClusterOrLigatureGroupMiddle = true;

    // Don't allow different rotations on ligature parts.
    bool rotationAdjusted = false;
    double angle = mPositions[startIndex].mAngle;
    if (mPositions[charIndex].mAngle != angle) {
      mPositions[charIndex].mAngle = angle;
      rotationAdjusted = true;
    }

    // Find out the partial glyph advance for this character and update
    // the character position.
    uint32_t partLength =
      charIndex - startIndex - it.GlyphUndisplayedCharacters();
    gfxFloat advance =
      it.GetGlyphPartialAdvance(partLength, presContext) / mFontSizeScaleFactor;
    gfxPoint direction = gfxPoint(cos(angle), sin(angle)) *
                         (it.TextRun()->IsRightToLeft() ? -1.0 : 1.0);
    mPositions[charIndex].mPosition = mPositions[startIndex].mPosition +
                                      direction * advance;

    // Ensure any runs that would end in the middle of a ligature now end just
    // after the ligature.
    if (mPositions[charIndex].mRunBoundary) {
      mPositions[charIndex].mRunBoundary = false;
      if (charIndex + 1 < mPositions.Length()) {
        mPositions[charIndex + 1].mRunBoundary = true;
      }
    } else if (rotationAdjusted) {
      if (charIndex + 1 < mPositions.Length()) {
        mPositions[charIndex + 1].mRunBoundary = true;
      }
    }

    // Ensure any anchored chunks that would begin in the middle of a ligature
    // now begin just after the ligature.
    if (mPositions[charIndex].mStartOfChunk) {
      mPositions[charIndex].mStartOfChunk = false;
      if (charIndex + 1 < mPositions.Length()) {
        mPositions[charIndex + 1].mStartOfChunk = true;
      }
    }

    it.Next();
  }
}

nsIFrame*
SVGTextFrame::GetTextPathPathFrame(nsIFrame* aTextPathFrame)
{
  nsSVGTextPathProperty *property = static_cast<nsSVGTextPathProperty*>
    (aTextPathFrame->Properties().Get(nsSVGEffects::HrefProperty()));

  if (!property) {
    nsIContent* content = aTextPathFrame->GetContent();
    dom::SVGTextPathElement* tp = static_cast<dom::SVGTextPathElement*>(content);
    nsAutoString href;
    tp->mStringAttributes[dom::SVGTextPathElement::HREF].GetAnimValue(href, tp);
    if (href.IsEmpty()) {
      return nullptr; // no URL
    }

    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = content->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              content->GetCurrentDoc(), base);

    property = nsSVGEffects::GetTextPathProperty(targetURI, aTextPathFrame,
                                                 nsSVGEffects::HrefProperty());
    if (!property)
      return nullptr;
  }

  return property->GetReferencedFrame(nsGkAtoms::svgPathGeometryFrame, nullptr);
}

TemporaryRef<Path>
SVGTextFrame::GetTextPath(nsIFrame* aTextPathFrame)
{
  nsIFrame *pathFrame = GetTextPathPathFrame(aTextPathFrame);

  if (!pathFrame) {
    return nullptr;
  }

  nsSVGPathGeometryElement *element =
    static_cast<nsSVGPathGeometryElement*>(pathFrame->GetContent());

  RefPtr<Path> path = element->GetPathForLengthOrPositionMeasuring();
  if (!path) {
    return nullptr;
  }

  gfxMatrix matrix = element->PrependLocalTransformsTo(gfxMatrix());
  if (!matrix.IsIdentity()) {
    RefPtr<PathBuilder> builder =
      path->TransformedCopyToBuilder(ToMatrix(matrix));
    path = builder->Finish();
  }

  return path.forget();
}

gfxFloat
SVGTextFrame::GetOffsetScale(nsIFrame* aTextPathFrame)
{
  nsIFrame *pathFrame = GetTextPathPathFrame(aTextPathFrame);
  if (!pathFrame)
    return 1.0;

  return static_cast<dom::SVGPathElement*>(pathFrame->GetContent())->
    GetPathLengthScale(dom::SVGPathElement::eForTextPath);
}

gfxFloat
SVGTextFrame::GetStartOffset(nsIFrame* aTextPathFrame)
{
  dom::SVGTextPathElement *tp =
    static_cast<dom::SVGTextPathElement*>(aTextPathFrame->GetContent());
  nsSVGLength2 *length =
    &tp->mLengthAttributes[dom::SVGTextPathElement::STARTOFFSET];

  if (length->IsPercentage()) {
    RefPtr<Path> data = GetTextPath(aTextPathFrame);
    return data ?
      length->GetAnimValInSpecifiedUnits() * data->ComputeLength() / 100.0 :
      0.0;
  }
  return length->GetAnimValue(tp) * GetOffsetScale(aTextPathFrame);
}

void
SVGTextFrame::DoTextPathLayout()
{
  nsPresContext* context = PresContext();

  CharIterator it(this, CharIterator::eClusterAndLigatureGroupStart);
  while (!it.AtEnd()) {
    nsIFrame* textPathFrame = it.TextPathFrame();
    if (!textPathFrame) {
      // Skip past this frame if we're not in a text path.
      it.AdvancePastCurrentFrame();
      continue;
    }

    // Get the path itself.
    RefPtr<Path> path = GetTextPath(textPathFrame);
    if (!path) {
      it.AdvancePastCurrentTextPathFrame();
      continue;
    }

    nsIContent* textPath = textPathFrame->GetContent();

    gfxFloat offset = GetStartOffset(textPathFrame);
    Float pathLength = path->ComputeLength();

    // Loop for each text frame in the text path.
    do {
      uint32_t i = it.TextElementCharIndex();
      gfxFloat halfAdvance =
        it.GetGlyphAdvance(context) / mFontSizeScaleFactor / 2.0;
      gfxFloat sign = it.TextRun()->IsRightToLeft() ? -1.0 : 1.0;
      gfxFloat midx = mPositions[i].mPosition.x + sign * halfAdvance + offset;

      // Hide the character if it falls off the end of the path.
      mPositions[i].mHidden = midx < 0 || midx > pathLength;

      // Position the character on the path at the right angle.
      Point tangent; // Unit vector tangent to the point we find.
      Point pt = path->ComputePointAtLength(Float(midx), &tangent);
      Float rotation = atan2f(tangent.y, tangent.x);
      Point normal(-tangent.y, tangent.x); // Unit vector normal to the point.
      Point offsetFromPath = normal * mPositions[i].mPosition.y;
      pt += offsetFromPath;
      Point direction = tangent * sign;
      mPositions[i].mPosition = ThebesPoint(pt) - ThebesPoint(direction) * halfAdvance;
      mPositions[i].mAngle += rotation;

      // Position any characters for a partial ligature.
      for (uint32_t j = i + 1;
           j < mPositions.Length() && mPositions[j].mClusterOrLigatureGroupMiddle;
           j++) {
        gfxPoint partialAdvance =
          ThebesPoint(direction) * it.GetGlyphPartialAdvance(j - i, context) /
                                                         mFontSizeScaleFactor;
        mPositions[j].mPosition = mPositions[i].mPosition + partialAdvance;
        mPositions[j].mAngle = mPositions[i].mAngle;
        mPositions[j].mHidden = mPositions[i].mHidden;
      }
      it.Next();
    } while (it.TextPathFrame() &&
             it.TextPathFrame()->GetContent() == textPath);
  }
}

void
SVGTextFrame::DoAnchoring()
{
  nsPresContext* presContext = PresContext();

  CharIterator it(this, CharIterator::eOriginal);

  // Don't need to worry about skipped or trimmed characters.
  while (!it.AtEnd() &&
         (it.IsOriginalCharSkipped() || it.IsOriginalCharTrimmed())) {
    it.Next();
  }

  uint32_t start = it.TextElementCharIndex();
  while (start < mPositions.Length()) {
    it.AdvanceToCharacter(start);
    nsTextFrame* chunkFrame = it.TextFrame();

    // Measure characters in this chunk to find the left-most and right-most
    // edges of all glyphs within the chunk.
    uint32_t index = it.TextElementCharIndex();
    uint32_t end = start;
    gfxFloat left = std::numeric_limits<gfxFloat>::infinity();
    gfxFloat right = -std::numeric_limits<gfxFloat>::infinity();
    do {
      if (!it.IsOriginalCharSkipped() && !it.IsOriginalCharTrimmed()) {
        gfxFloat advance = it.GetAdvance(presContext) / mFontSizeScaleFactor;
        if (it.TextRun()->IsRightToLeft()) {
          left  = std::min(left,  mPositions[index].mPosition.x - advance);
          right = std::max(right, mPositions[index].mPosition.x);
        } else {
          left  = std::min(left,  mPositions[index].mPosition.x);
          right = std::max(right, mPositions[index].mPosition.x + advance);
        }
      }
      it.Next();
      index = end = it.TextElementCharIndex();
    } while (!it.AtEnd() && !mPositions[end].mStartOfChunk);

    if (left != std::numeric_limits<gfxFloat>::infinity()) {
      bool isRTL =
        chunkFrame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
      TextAnchorSide anchor =
        ConvertLogicalTextAnchorToPhysical(chunkFrame->StyleSVG()->mTextAnchor,
                                           isRTL);

      ShiftAnchoredChunk(mPositions, start, end, left, right, anchor);
    }

    start = it.TextElementCharIndex();
  }
}

void
SVGTextFrame::DoGlyphPositioning()
{
  mPositions.Clear();
  RemoveStateBits(NS_STATE_SVG_POSITIONING_DIRTY);

  nsIFrame* kid = GetFirstPrincipalChild();
  if (kid && NS_SUBTREE_DIRTY(kid)) {
    MOZ_ASSERT(false, "should have already reflowed the kid");
    return;
  }

  // Determine the positions of each character in app units.
  nsTArray<nsPoint> charPositions;
  DetermineCharPositions(charPositions);

  if (charPositions.IsEmpty()) {
    // No characters, so nothing to do.
    return;
  }

  // If the textLength="" attribute was specified, then we need ResolvePositions
  // to record that a new run starts with each glyph.
  SVGTextContentElement* element = static_cast<SVGTextContentElement*>(mContent);
  nsSVGLength2* textLengthAttr =
    element->GetAnimatedLength(nsGkAtoms::textLength);
  bool adjustingTextLength = textLengthAttr->IsExplicitlySet();
  float expectedTextLength = textLengthAttr->GetAnimValue(element);

  if (adjustingTextLength && expectedTextLength < 0.0f) {
    // If textLength="" is less than zero, ignore it.
    adjustingTextLength = false;
  }

  // Get the x, y, dx, dy, rotate values for the subtree.
  nsTArray<gfxPoint> deltas;
  if (!ResolvePositions(deltas, adjustingTextLength)) {
    // If ResolvePositions returned false, it means that there were some
    // characters in the DOM but none of them are displayed.  Clear out
    // mPositions so that we don't attempt to do any painting later.
    mPositions.Clear();
    return;
  }

  // XXX We might be able to do less work when there is at most a single
  // x/y/dx/dy position.

  // Truncate the positioning arrays to the actual number of characters present.
  TruncateTo(deltas, charPositions);
  TruncateTo(mPositions, charPositions);

  // Fill in an unspecified character position at index 0.
  if (!mPositions[0].IsXSpecified()) {
    mPositions[0].mPosition.x = 0.0;
  }
  if (!mPositions[0].IsYSpecified()) {
    mPositions[0].mPosition.y = 0.0;
  }
  if (!mPositions[0].IsAngleSpecified()) {
    mPositions[0].mAngle = 0.0;
  }

  nsPresContext* presContext = PresContext();

  float cssPxPerDevPx = presContext->
    AppUnitsToFloatCSSPixels(presContext->AppUnitsPerDevPixel());
  double factor = cssPxPerDevPx / mFontSizeScaleFactor;

  // Determine how much to compress or expand glyph positions due to
  // textLength="" and lengthAdjust="".
  double adjustment = 0.0;
  mLengthAdjustScaleFactor = 1.0f;
  if (adjustingTextLength) {
    nscoord frameWidth = GetFirstPrincipalChild()->GetRect().width;
    float actualTextLength =
      static_cast<float>(presContext->AppUnitsToGfxUnits(frameWidth) * factor);

    nsRefPtr<SVGAnimatedEnumeration> lengthAdjustEnum = element->LengthAdjust();
    uint16_t lengthAdjust = lengthAdjustEnum->AnimVal();
    switch (lengthAdjust) {
      case SVG_LENGTHADJUST_SPACINGANDGLYPHS:
        // Scale the glyphs and their positions.
        if (actualTextLength > 0) {
          mLengthAdjustScaleFactor = expectedTextLength / actualTextLength;
        }
        break;

      default:
        MOZ_ASSERT(lengthAdjust == SVG_LENGTHADJUST_SPACING);
        // Just add space between each glyph.
        int32_t adjustableSpaces = 0;
        for (uint32_t i = 1; i < mPositions.Length(); i++) {
          if (!mPositions[i].mUnaddressable) {
            adjustableSpaces++;
          }
        }
        if (adjustableSpaces) {
          adjustment = (expectedTextLength - actualTextLength) / adjustableSpaces;
        }
        break;
    }
  }

  // Fill in any unspecified character positions based on the positions recorded
  // in charPositions, and also add in the dx/dy values.
  if (!deltas.IsEmpty()) {
    mPositions[0].mPosition += deltas[0];
  }

  for (uint32_t i = 1; i < mPositions.Length(); i++) {
    // Fill in unspecified x position.
    if (!mPositions[i].IsXSpecified()) {
      nscoord d = charPositions[i].x - charPositions[i - 1].x;
      mPositions[i].mPosition.x =
        mPositions[i - 1].mPosition.x +
        presContext->AppUnitsToGfxUnits(d) * factor * mLengthAdjustScaleFactor;
      if (!mPositions[i].mUnaddressable) {
        mPositions[i].mPosition.x += adjustment;
      }
    }
    // Fill in unspecified y position.
    if (!mPositions[i].IsYSpecified()) {
      nscoord d = charPositions[i].y - charPositions[i - 1].y;
      mPositions[i].mPosition.y =
        mPositions[i - 1].mPosition.y +
        presContext->AppUnitsToGfxUnits(d) * factor;
    }
    // Add in dx/dy.
    if (i < deltas.Length()) {
      mPositions[i].mPosition += deltas[i];
    }
    // Fill in unspecified rotation values.
    if (!mPositions[i].IsAngleSpecified()) {
      mPositions[i].mAngle = 0.0f;
    }
  }

  MOZ_ASSERT(mPositions.Length() == charPositions.Length());

  AdjustChunksForLineBreaks();
  AdjustPositionsForClusters();
  DoAnchoring();
  DoTextPathLayout();
}

bool
SVGTextFrame::ShouldRenderAsPath(nsRenderingContext* aContext,
                                 nsTextFrame* aFrame,
                                 bool& aShouldPaintSVGGlyphs)
{
  // Rendering to a clip path.
  if (SVGAutoRenderState::GetRenderMode(aContext) != SVGAutoRenderState::NORMAL) {
    aShouldPaintSVGGlyphs = false;
    return true;
  }

  aShouldPaintSVGGlyphs = true;

  const nsStyleSVG* style = aFrame->StyleSVG();

  // Fill is a non-solid paint, has a non-default fill-rule or has
  // non-1 opacity.
  if (!(style->mFill.mType == eStyleSVGPaintType_None ||
        (style->mFill.mType == eStyleSVGPaintType_Color &&
         style->mFillOpacity == 1))) {
    return true;
  }

  // Text has a stroke.
  if (style->HasStroke() &&
      SVGContentUtils::CoordToFloat(PresContext(),
                                    static_cast<nsSVGElement*>(mContent),
                                    style->mStrokeWidth) > 0) {
    return true;
  }

  return false;
}

void
SVGTextFrame::ScheduleReflowSVG()
{
  if (mState & NS_FRAME_IS_NONDISPLAY) {
    ScheduleReflowSVGNonDisplayText();
  } else {
    nsSVGUtils::ScheduleReflowSVG(this);
  }
}

void
SVGTextFrame::NotifyGlyphMetricsChange()
{
  AddStateBits(NS_STATE_SVG_POSITIONING_DIRTY);
  nsSVGEffects::InvalidateRenderingObservers(this);
  ScheduleReflowSVG();
}

void
SVGTextFrame::UpdateGlyphPositioning()
{
  nsIFrame* kid = GetFirstPrincipalChild();
  if (!kid) {
    return;
  }

  if (mState & NS_STATE_SVG_POSITIONING_DIRTY) {
    DoGlyphPositioning();
  }
}

void
SVGTextFrame::MaybeReflowAnonymousBlockChild()
{
  nsIFrame* kid = GetFirstPrincipalChild();
  if (!kid)
    return;

  NS_ASSERTION(!(kid->GetStateBits() & NS_FRAME_IN_REFLOW),
               "should not be in reflow when about to reflow again");

  if (NS_SUBTREE_DIRTY(this)) {
    if (mState & NS_FRAME_IS_DIRTY) {
      // If we require a full reflow, ensure our kid is marked fully dirty.
      // (Note that our anonymous nsBlockFrame is not an nsISVGChildFrame, so
      // even when we are called via our ReflowSVG this will not be done for us
      // by nsSVGDisplayContainerFrame::ReflowSVG.)
      kid->AddStateBits(NS_FRAME_IS_DIRTY);
    }
    MOZ_ASSERT(nsSVGUtils::AnyOuterSVGIsCallingReflowSVG(this),
               "should be under ReflowSVG");
    nsPresContext::InterruptPreventer noInterrupts(PresContext());
    DoReflow();
  }
}

void
SVGTextFrame::DoReflow()
{
  // Since we are going to reflow the anonymous block frame, we will
  // need to update mPositions.
  AddStateBits(NS_STATE_SVG_POSITIONING_DIRTY);

  if (mState & NS_FRAME_IS_NONDISPLAY) {
    // Normally, these dirty flags would be cleared in ReflowSVG(), but that
    // doesn't get called for non-display frames. We don't want to reflow our
    // descendants every time SVGTextFrame::PaintSVG makes sure that we have
    // valid positions by calling UpdateGlyphPositioning(), so we need to clear
    // these dirty bits. Note that this also breaks an invalidation loop where
    // our descendants invalidate as they reflow, which invalidates rendering
    // observers, which reschedules the frame that is currently painting by
    // referencing us to paint again. See bug 839958 comment 7. Hopefully we
    // will break that loop more convincingly at some point.
    mState &= ~(NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN);
  }

  nsPresContext *presContext = PresContext();
  nsIFrame* kid = GetFirstPrincipalChild();
  if (!kid)
    return;

  nsRefPtr<nsRenderingContext> renderingContext =
    presContext->PresShell()->CreateReferenceRenderingContext();

  if (UpdateFontSizeScaleFactor()) {
    // If the font size scale factor changed, we need the block to report
    // an updated preferred width.
    kid->MarkIntrinsicWidthsDirty();
  }

  mState |= NS_STATE_SVG_TEXT_IN_REFLOW;

  nscoord width = kid->GetPrefWidth(renderingContext);
  nsHTMLReflowState reflowState(presContext, kid,
                                renderingContext,
                                nsSize(width, NS_UNCONSTRAINEDSIZE));
  nsHTMLReflowMetrics desiredSize(reflowState);
  nsReflowStatus status;

  NS_ASSERTION(reflowState.ComputedPhysicalBorderPadding() == nsMargin(0, 0, 0, 0) &&
               reflowState.ComputedPhysicalMargin() == nsMargin(0, 0, 0, 0),
               "style system should ensure that :-moz-svg-text "
               "does not get styled");

  kid->WillReflow(presContext);
  kid->Reflow(presContext, desiredSize, reflowState, status);
  kid->DidReflow(presContext, &reflowState, nsDidReflowStatus::FINISHED);
  kid->SetSize(nsSize(desiredSize.Width(), desiredSize.Height()));

  mState &= ~NS_STATE_SVG_TEXT_IN_REFLOW;

  TextNodeCorrespondenceRecorder::RecordCorrespondence(this);
}

// Usable font size range in devpixels / user-units
#define CLAMP_MIN_SIZE 8.0
#define CLAMP_MAX_SIZE 200.0
#define PRECISE_SIZE   200.0

bool
SVGTextFrame::UpdateFontSizeScaleFactor()
{
  double oldFontSizeScaleFactor = mFontSizeScaleFactor;

  nsPresContext* presContext = PresContext();

  bool geometricPrecision = false;
  nscoord min = nscoord_MAX,
          max = nscoord_MIN;

  // Find the minimum and maximum font sizes used over all the
  // nsTextFrames.
  TextFrameIterator it(this);
  nsTextFrame* f = it.Current();
  while (f) {
    if (!geometricPrecision) {
      // Unfortunately we can't treat text-rendering:geometricPrecision
      // separately for each text frame.
      geometricPrecision = f->StyleSVG()->mTextRendering ==
                             NS_STYLE_TEXT_RENDERING_GEOMETRICPRECISION;
    }
    nscoord size = f->StyleFont()->mFont.size;
    if (size) {
      min = std::min(min, size);
      max = std::max(max, size);
    }
    f = it.Next();
  }

  if (min == nscoord_MAX) {
    // No text, so no need for scaling.
    mFontSizeScaleFactor = 1.0;
    return mFontSizeScaleFactor != oldFontSizeScaleFactor;
  }

  double minSize = presContext->AppUnitsToFloatCSSPixels(min);

  if (geometricPrecision) {
    // We want to ensure minSize is scaled to PRECISE_SIZE.
    mFontSizeScaleFactor = PRECISE_SIZE / minSize;
    return mFontSizeScaleFactor != oldFontSizeScaleFactor;
  }

  // When we are non-display, we could be painted in different coordinate
  // spaces, and we don't want to have to reflow for each of these.  We
  // just assume that the context scale is 1.0 for them all, so we don't
  // get stuck with a font size scale factor based on whichever referencing
  // frame happens to reflow first.
  double contextScale = 1.0;
  if (!(mState & NS_FRAME_IS_NONDISPLAY)) {
    gfxMatrix m(GetCanvasTM(FOR_OUTERSVG_TM));
    if (!m.IsSingular()) {
      contextScale = GetContextScale(m);
    }
  }
  mLastContextScale = contextScale;

  double maxSize = presContext->AppUnitsToFloatCSSPixels(max);

  // But we want to ignore any scaling required due to HiDPI displays, since
  // regular CSS text frames will still create text runs using the font size
  // in CSS pixels, and we want SVG text to have the same rendering as HTML
  // text for regular font sizes.
  float cssPxPerDevPx =
    presContext->AppUnitsToFloatCSSPixels(presContext->AppUnitsPerDevPixel());
  contextScale *= cssPxPerDevPx;

  double minTextRunSize = minSize * contextScale;
  double maxTextRunSize = maxSize * contextScale;

  if (minTextRunSize >= CLAMP_MIN_SIZE &&
      maxTextRunSize <= CLAMP_MAX_SIZE) {
    // We are already in the ideal font size range for all text frames,
    // so we only have to take into account the contextScale.
    mFontSizeScaleFactor = contextScale;
  } else if (maxSize / minSize > CLAMP_MAX_SIZE / CLAMP_MIN_SIZE) {
    // We can't scale the font sizes so that all of the text frames lie
    // within our ideal font size range, so we treat the minimum as more
    // important and just scale so that minSize = CLAMP_MIN_SIZE.
    mFontSizeScaleFactor = CLAMP_MIN_SIZE / minTextRunSize;
  } else if (minTextRunSize < CLAMP_MIN_SIZE) {
    mFontSizeScaleFactor = CLAMP_MIN_SIZE / minTextRunSize;
  } else {
    mFontSizeScaleFactor = CLAMP_MAX_SIZE / maxTextRunSize;
  }

  return mFontSizeScaleFactor != oldFontSizeScaleFactor;
}

double
SVGTextFrame::GetFontSizeScaleFactor() const
{
  return mFontSizeScaleFactor;
}

/**
 * Take aPoint, which is in the <text> element's user space, and convert
 * it to the appropriate frame user space of aChildFrame according to
 * which rendered run the point hits.
 */
gfxPoint
SVGTextFrame::TransformFramePointToTextChild(const gfxPoint& aPoint,
                                             nsIFrame* aChildFrame)
{
  NS_ASSERTION(aChildFrame &&
               nsLayoutUtils::GetClosestFrameOfType
                 (aChildFrame->GetParent(), nsGkAtoms::svgTextFrame) == this,
               "aChildFrame must be a descendant of this frame");

  UpdateGlyphPositioning();

  nsPresContext* presContext = PresContext();

  // Add in the mRect offset to aPoint, as that will have been taken into
  // account when transforming the point from the ancestor frame down
  // to this one.
  float cssPxPerDevPx = presContext->
    AppUnitsToFloatCSSPixels(presContext->AppUnitsPerDevPixel());
  float factor = presContext->AppUnitsPerCSSPixel();
  gfxPoint framePosition(NSAppUnitsToFloatPixels(mRect.x, factor),
                         NSAppUnitsToFloatPixels(mRect.y, factor));
  gfxPoint pointInUserSpace = aPoint * cssPxPerDevPx + framePosition;

  // Find the closest rendered run for the text frames beneath aChildFrame.
  TextRenderedRunIterator it(this, TextRenderedRunIterator::eAllFrames,
                             aChildFrame);
  TextRenderedRun hit;
  gfxPoint pointInRun;
  nscoord dx = nscoord_MAX;
  nscoord dy = nscoord_MAX;
  for (TextRenderedRun run = it.Current(); run.mFrame; run = it.Next()) {
    uint32_t flags = TextRenderedRun::eIncludeFill |
                     TextRenderedRun::eIncludeStroke |
                     TextRenderedRun::eNoHorizontalOverflow;
    gfxRect runRect = run.GetRunUserSpaceRect(presContext, flags).ToThebesRect();

    gfxPoint pointInRunUserSpace =
      run.GetTransformFromRunUserSpaceToUserSpace(presContext).Invert().
          Transform(pointInUserSpace);

    if (Inside(runRect, pointInRunUserSpace)) {
      // The point was inside the rendered run's rect, so we choose it.
      dx = 0;
      dy = 0;
      pointInRun = pointInRunUserSpace;
      hit = run;
    } else if (nsLayoutUtils::PointIsCloserToRect(pointInRunUserSpace,
                                                  runRect, dx, dy)) {
      // The point was closer to this rendered run's rect than any others
      // we've seen so far.
      pointInRun.x = clamped(pointInRunUserSpace.x,
                             runRect.X(), runRect.XMost());
      pointInRun.y = clamped(pointInRunUserSpace.y,
                             runRect.Y(), runRect.YMost());
      hit = run;
    }
  }

  if (!hit.mFrame) {
    // We didn't find any rendered runs for the frame.
    return aPoint;
  }

  // Return the point in user units relative to the nsTextFrame,
  // but taking into account mFontSizeScaleFactor.
  gfxMatrix m = hit.GetTransformFromRunUserSpaceToFrameUserSpace(presContext);
  m.Scale(mFontSizeScaleFactor, mFontSizeScaleFactor);
  return m.Transform(pointInRun) / cssPxPerDevPx;
}

/**
 * For each rendered run for frames beneath aChildFrame, convert aRect
 * into the run's frame user space and intersect it with the run's
 * frame user space rectangle.  For each of these intersections,
 * then translate them up into aChildFrame's coordinate space
 * and union them all together.
 */
gfxRect
SVGTextFrame::TransformFrameRectToTextChild(const gfxRect& aRect,
                                            nsIFrame* aChildFrame)
{
  NS_ASSERTION(aChildFrame &&
               nsLayoutUtils::GetClosestFrameOfType
                 (aChildFrame->GetParent(), nsGkAtoms::svgTextFrame) == this,
               "aChildFrame must be a descendant of this frame");

  UpdateGlyphPositioning();

  nsPresContext* presContext = PresContext();

  // Add in the mRect offset to aRect, as that will have been taken into
  // account when transforming the rect from the ancestor frame down
  // to this one.
  float cssPxPerDevPx = presContext->
    AppUnitsToFloatCSSPixels(presContext->AppUnitsPerDevPixel());
  float factor = presContext->AppUnitsPerCSSPixel();
  gfxPoint framePosition(NSAppUnitsToFloatPixels(mRect.x, factor),
                         NSAppUnitsToFloatPixels(mRect.y, factor));
  gfxRect incomingRectInUserSpace(aRect.x * cssPxPerDevPx + framePosition.x,
                                  aRect.y * cssPxPerDevPx + framePosition.y,
                                  aRect.width * cssPxPerDevPx,
                                  aRect.height * cssPxPerDevPx);

  // Find each rendered run for text frames beneath aChildFrame.
  gfxRect result;
  TextRenderedRunIterator it(this, TextRenderedRunIterator::eAllFrames,
                             aChildFrame);
  for (TextRenderedRun run = it.Current(); run.mFrame; run = it.Next()) {
    // Convert the incoming rect into frame user space.
    gfxMatrix m;
    m.PreMultiply(run.GetTransformFromRunUserSpaceToUserSpace(presContext).Invert());
    m.PreMultiply(run.GetTransformFromRunUserSpaceToFrameUserSpace(presContext));
    gfxRect incomingRectInFrameUserSpace =
      m.TransformBounds(incomingRectInUserSpace);

    // Intersect it with this run's rectangle.
    uint32_t flags = TextRenderedRun::eIncludeFill |
                     TextRenderedRun::eIncludeStroke;
    SVGBBox runRectInFrameUserSpace = run.GetFrameUserSpaceRect(presContext, flags);
    if (runRectInFrameUserSpace.IsEmpty()) {
      continue;
    }
    gfxRect runIntersectionInFrameUserSpace =
      incomingRectInFrameUserSpace.Intersect(runRectInFrameUserSpace.ToThebesRect());

    if (!runIntersectionInFrameUserSpace.IsEmpty()) {
      // Take the font size scale into account.
      runIntersectionInFrameUserSpace.x *= mFontSizeScaleFactor;
      runIntersectionInFrameUserSpace.y *= mFontSizeScaleFactor;
      runIntersectionInFrameUserSpace.width *= mFontSizeScaleFactor;
      runIntersectionInFrameUserSpace.height *= mFontSizeScaleFactor;

      // Convert it into the coordinate space of aChildFrame.
      nsPoint offset = run.mFrame->GetOffsetTo(aChildFrame);
      gfxRect runIntersection =
        runIntersectionInFrameUserSpace +
          gfxPoint(NSAppUnitsToFloatPixels(offset.x, factor),
                   NSAppUnitsToFloatPixels(offset.y, factor));

      // Union it into the result.
      result.UnionRect(result, runIntersection);
    }
  }

  return result;
}

/**
 * For each rendered run beneath aChildFrame, translate aRect from
 * aChildFrame to the run's text frame, transform it then into
 * the run's frame user space, intersect it with the run's
 * frame user space rect, then transform it up to user space.
 * The result is the union of all of these.
 */
gfxRect
SVGTextFrame::TransformFrameRectFromTextChild(const nsRect& aRect,
                                              nsIFrame* aChildFrame)
{
  NS_ASSERTION(aChildFrame &&
               nsLayoutUtils::GetClosestFrameOfType
                 (aChildFrame->GetParent(), nsGkAtoms::svgTextFrame) == this,
               "aChildFrame must be a descendant of this frame");

  UpdateGlyphPositioning();

  nsPresContext* presContext = PresContext();

  gfxRect result;
  TextRenderedRunIterator it(this, TextRenderedRunIterator::eAllFrames,
                             aChildFrame);
  for (TextRenderedRun run = it.Current(); run.mFrame; run = it.Next()) {
    // First, translate aRect from aChildFrame to this run's frame.
    nsRect rectInTextFrame = aRect + aChildFrame->GetOffsetTo(run.mFrame);

    // Scale it into frame user space.
    gfxRect rectInFrameUserSpace =
      AppUnitsToFloatCSSPixels(gfxRect(rectInTextFrame.x,
                                       rectInTextFrame.y,
                                       rectInTextFrame.width,
                                       rectInTextFrame.height), presContext);

    // Intersect it with the run.
    uint32_t flags = TextRenderedRun::eIncludeFill |
                     TextRenderedRun::eIncludeStroke;
    rectInFrameUserSpace.IntersectRect
      (rectInFrameUserSpace, run.GetFrameUserSpaceRect(presContext, flags).ToThebesRect());

    if (!rectInFrameUserSpace.IsEmpty()) {
      // Transform it up to user space of the <text>, also taking into
      // account the font size scale.
      gfxMatrix m = run.GetTransformFromRunUserSpaceToUserSpace(presContext);
      m.Scale(mFontSizeScaleFactor, mFontSizeScaleFactor);
      gfxRect rectInUserSpace = m.Transform(rectInFrameUserSpace);

      // Union it into the result.
      result.UnionRect(result, rectInUserSpace);
    }
  }

  // Subtract the mRect offset from the result, as our user space for
  // this frame is relative to the top-left of mRect.
  float factor = presContext->AppUnitsPerCSSPixel();
  gfxPoint framePosition(NSAppUnitsToFloatPixels(mRect.x, factor),
                         NSAppUnitsToFloatPixels(mRect.y, factor));

  return result - framePosition;
}

DrawMode
SVGTextFrame::SetupCairoState(gfxContext* aContext,
                              nsIFrame* aFrame,
                              gfxTextContextPaint* aOuterContextPaint,
                              gfxTextContextPaint** aThisContextPaint)
{
  DrawMode toDraw = DrawMode(0);
  SVGTextContextPaint *thisContextPaint = new SVGTextContextPaint();

  if (SetupCairoStroke(aContext, aFrame, aOuterContextPaint, thisContextPaint)) {
    toDraw = DrawMode(int(toDraw) | int(DrawMode::GLYPH_STROKE));
  }

  if (SetupCairoFill(aContext, aFrame, aOuterContextPaint, thisContextPaint)) {
    toDraw = DrawMode(int(toDraw) | int(DrawMode::GLYPH_FILL));
  }

  *aThisContextPaint = thisContextPaint;

  return toDraw;
}

bool
SVGTextFrame::SetupCairoStroke(gfxContext* aContext,
                               nsIFrame* aFrame,
                               gfxTextContextPaint* aOuterContextPaint,
                               SVGTextContextPaint* aThisContextPaint)
{
  const nsStyleSVG *style = aFrame->StyleSVG();
  if (style->mStroke.mType == eStyleSVGPaintType_None) {
    aThisContextPaint->SetStrokeOpacity(0.0f);
    return false;
  }

  nsSVGUtils::SetupCairoStrokeGeometry(aFrame, aContext, aOuterContextPaint);
  float opacity = nsSVGUtils::GetOpacity(style->mStrokeOpacitySource,
                                         style->mStrokeOpacity,
                                         aOuterContextPaint);

  SetupInheritablePaint(aContext, aFrame, opacity, aOuterContextPaint,
                        aThisContextPaint->mStrokePaint, &nsStyleSVG::mStroke,
                        nsSVGEffects::StrokeProperty());

  aThisContextPaint->SetStrokeOpacity(opacity);

  return opacity != 0.0f;
}

bool
SVGTextFrame::SetupCairoFill(gfxContext* aContext,
                             nsIFrame* aFrame,
                             gfxTextContextPaint* aOuterContextPaint,
                             SVGTextContextPaint* aThisContextPaint)
{
  const nsStyleSVG *style = aFrame->StyleSVG();
  if (style->mFill.mType == eStyleSVGPaintType_None) {
    aThisContextPaint->SetFillOpacity(0.0f);
    return false;
  }

  float opacity = nsSVGUtils::GetOpacity(style->mFillOpacitySource,
                                         style->mFillOpacity,
                                         aOuterContextPaint);

  SetupInheritablePaint(aContext, aFrame, opacity, aOuterContextPaint,
                        aThisContextPaint->mFillPaint, &nsStyleSVG::mFill,
                        nsSVGEffects::FillProperty());

  aThisContextPaint->SetFillOpacity(opacity);

  return true;
}

void
SVGTextFrame::SetupInheritablePaint(gfxContext* aContext,
                                    nsIFrame* aFrame,
                                    float& aOpacity,
                                    gfxTextContextPaint* aOuterContextPaint,
                                    SVGTextContextPaint::Paint& aTargetPaint,
                                    nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                    const FramePropertyDescriptor* aProperty)
{
  const nsStyleSVG *style = aFrame->StyleSVG();
  nsSVGPaintServerFrame *ps =
    nsSVGEffects::GetPaintServer(aFrame, &(style->*aFillOrStroke), aProperty);

  if (ps && ps->SetupPaintServer(aContext, aFrame, aFillOrStroke, aOpacity)) {
    aTargetPaint.SetPaintServer(aFrame, aContext->CurrentMatrix(), ps);
  } else if (nsSVGUtils::SetupContextPaint(aContext, aOuterContextPaint,
                                           style->*aFillOrStroke,
                                           aOpacity)) {
    aTargetPaint.SetContextPaint(aOuterContextPaint, (style->*aFillOrStroke).mType);
  } else {
    nscolor color = nsSVGUtils::GetFallbackOrPaintColor(aContext,
                                                        aFrame->StyleContext(),
                                                        aFillOrStroke);
    aTargetPaint.SetColor(color);

    nsRefPtr<gfxPattern> pattern =
      new gfxPattern(gfxRGBA(NS_GET_R(color) / 255.0,
                             NS_GET_G(color) / 255.0,
                             NS_GET_B(color) / 255.0,
                             NS_GET_A(color) / 255.0 * aOpacity));
    aContext->SetPattern(pattern);
  }
}
