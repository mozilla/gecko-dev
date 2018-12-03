/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsBoxFrame.h"
#include "nsDOMAttributeMap.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsContainerFrame.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsITheme.h"
#include "nsIServiceManager.h"
#include "nsBoxLayout.h"
#include "FrameLayerBuilder.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/dom/Element.h"
#include <algorithm>

using namespace mozilla;

nsresult nsBox::BeginXULLayout(nsBoxLayoutState& aState) {
  // mark ourselves as dirty so no child under us
  // can post an incremental layout.
  // XXXldb Is this still needed?
  AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);

  if (GetStateBits() & NS_FRAME_IS_DIRTY) {
    // If the parent is dirty, all the children are dirty (ReflowInput
    // does this too).
    nsIFrame* box;
    for (box = GetChildXULBox(this); box; box = GetNextXULBox(box))
      box->AddStateBits(NS_FRAME_IS_DIRTY);
  }

  // Another copy-over from ReflowInput.
  // Since we are in reflow, we don't need to store these properties anymore.
  DeleteProperty(UsedBorderProperty());
  DeleteProperty(UsedPaddingProperty());
  DeleteProperty(UsedMarginProperty());

  return NS_OK;
}

NS_IMETHODIMP
nsBox::DoXULLayout(nsBoxLayoutState& aState) { return NS_OK; }

nsresult nsBox::EndXULLayout(nsBoxLayoutState& aState) {
  return SyncLayout(aState);
}

bool nsBox::gGotTheme = false;
StaticRefPtr<nsITheme> nsBox::gTheme;

nsBox::nsBox(ClassID aID) : nsIFrame(aID) {
  MOZ_COUNT_CTOR(nsBox);
  if (!gGotTheme) {
    gTheme = do_GetNativeTheme();
    if (gTheme) {
      gGotTheme = true;
    }
  }
}

nsBox::~nsBox() {
  // NOTE:  This currently doesn't get called for |nsBoxToBlockAdaptor|
  // objects, so don't rely on putting anything here.
  MOZ_COUNT_DTOR(nsBox);
}

/* static */ void nsBox::Shutdown() {
  gGotTheme = false;
  gTheme = nullptr;
}

nsresult nsBox::XULRelayoutChildAtOrdinal(nsIFrame* aChild) { return NS_OK; }

nsresult nsIFrame::GetXULClientRect(nsRect& aClientRect) {
  aClientRect = mRect;
  aClientRect.MoveTo(0, 0);

  nsMargin borderPadding;
  GetXULBorderAndPadding(borderPadding);

  aClientRect.Deflate(borderPadding);

  if (aClientRect.width < 0) aClientRect.width = 0;

  if (aClientRect.height < 0) aClientRect.height = 0;

  return NS_OK;
}

void nsBox::SetXULBounds(nsBoxLayoutState& aState, const nsRect& aRect,
                         bool aRemoveOverflowAreas) {
  nsRect rect(mRect);

  uint32_t flags = GetXULLayoutFlags();

  uint32_t stateFlags = aState.LayoutFlags();

  flags |= stateFlags;

  if ((flags & NS_FRAME_NO_MOVE_FRAME) == NS_FRAME_NO_MOVE_FRAME)
    SetSize(aRect.Size());
  else
    SetRect(aRect);

  // Nuke the overflow area. The caller is responsible for restoring
  // it if necessary.
  if (aRemoveOverflowAreas) {
    // remove the previously stored overflow area
    ClearOverflowRects();
  }

  if (!(flags & NS_FRAME_NO_MOVE_VIEW)) {
    nsContainerFrame::PositionFrameView(this);
    if ((rect.x != aRect.x) || (rect.y != aRect.y))
      nsContainerFrame::PositionChildViews(this);
  }
}

nsresult nsIFrame::GetXULBorderAndPadding(nsMargin& aBorderAndPadding) {
  aBorderAndPadding.SizeTo(0, 0, 0, 0);
  nsresult rv = GetXULBorder(aBorderAndPadding);
  if (NS_FAILED(rv)) return rv;

  nsMargin padding;
  rv = GetXULPadding(padding);
  if (NS_FAILED(rv)) return rv;

  aBorderAndPadding += padding;

  return rv;
}

nsresult nsBox::GetXULBorder(nsMargin& aMargin) {
  aMargin.SizeTo(0, 0, 0, 0);

  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->HasAppearance() && gTheme) {
    // Go to the theme for the border.
    nsPresContext* context = PresContext();
    if (gTheme->ThemeSupportsWidget(context, this, disp->mAppearance)) {
      LayoutDeviceIntMargin margin = gTheme->GetWidgetBorder(
          context->DeviceContext(), this, disp->mAppearance);
      aMargin =
          LayoutDevicePixel::ToAppUnits(margin, context->AppUnitsPerDevPixel());
      return NS_OK;
    }
  }

  aMargin = StyleBorder()->GetComputedBorder();

  return NS_OK;
}

nsresult nsBox::GetXULPadding(nsMargin& aPadding) {
  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->HasAppearance() && gTheme) {
    // Go to the theme for the padding.
    nsPresContext* context = PresContext();
    if (gTheme->ThemeSupportsWidget(context, this, disp->mAppearance)) {
      LayoutDeviceIntMargin padding;
      bool useThemePadding = gTheme->GetWidgetPadding(
          context->DeviceContext(), this, disp->mAppearance, &padding);
      if (useThemePadding) {
        aPadding = LayoutDevicePixel::ToAppUnits(
            padding, context->AppUnitsPerDevPixel());
        return NS_OK;
      }
    }
  }

  aPadding.SizeTo(0, 0, 0, 0);
  StylePadding()->GetPadding(aPadding);

  return NS_OK;
}

nsresult nsBox::GetXULMargin(nsMargin& aMargin) {
  aMargin.SizeTo(0, 0, 0, 0);
  StyleMargin()->GetMargin(aMargin);

  return NS_OK;
}

void nsBox::SizeNeedsRecalc(nsSize& aSize) {
  aSize.width = -1;
  aSize.height = -1;
}

void nsBox::CoordNeedsRecalc(nscoord& aFlex) { aFlex = -1; }

bool nsBox::DoesNeedRecalc(const nsSize& aSize) {
  return (aSize.width == -1 || aSize.height == -1);
}

bool nsBox::DoesNeedRecalc(nscoord aCoord) { return (aCoord == -1); }

nsSize nsBox::GetXULPrefSize(nsBoxLayoutState& aState) {
  NS_ASSERTION(aState.GetRenderingContext(), "must have rendering context");

  nsSize pref(0, 0);
  DISPLAY_PREF_SIZE(this, pref);

  if (IsXULCollapsed()) return pref;

  AddBorderAndPadding(pref);
  bool widthSet, heightSet;
  nsIFrame::AddXULPrefSize(this, pref, widthSet, heightSet);

  nsSize minSize = GetXULMinSize(aState);
  nsSize maxSize = GetXULMaxSize(aState);
  return BoundsCheck(minSize, pref, maxSize);
}

nsSize nsBox::GetXULMinSize(nsBoxLayoutState& aState) {
  NS_ASSERTION(aState.GetRenderingContext(), "must have rendering context");

  nsSize min(0, 0);
  DISPLAY_MIN_SIZE(this, min);

  if (IsXULCollapsed()) return min;

  AddBorderAndPadding(min);
  bool widthSet, heightSet;
  nsIFrame::AddXULMinSize(aState, this, min, widthSet, heightSet);
  return min;
}

nsSize nsBox::GetXULMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState) {
  return nsSize(0, 0);
}

nsSize nsBox::GetXULMaxSize(nsBoxLayoutState& aState) {
  NS_ASSERTION(aState.GetRenderingContext(), "must have rendering context");

  nsSize maxSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  DISPLAY_MAX_SIZE(this, maxSize);

  if (IsXULCollapsed()) return maxSize;

  AddBorderAndPadding(maxSize);
  bool widthSet, heightSet;
  nsIFrame::AddXULMaxSize(this, maxSize, widthSet, heightSet);
  return maxSize;
}

nscoord nsBox::GetXULFlex() {
  nscoord flex = 0;

  nsIFrame::AddXULFlex(this, flex);

  return flex;
}

uint32_t nsIFrame::GetXULOrdinal() {
  uint32_t ordinal = StyleXUL()->mBoxOrdinal;

  // When present, attribute value overrides CSS.
  nsIContent* content = GetContent();
  if (content && content->IsXULElement()) {
    nsresult error;
    nsAutoString value;

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::ordinal, value);
    if (!value.IsEmpty()) {
      ordinal = value.ToInteger(&error);
    }
  }

  return ordinal;
}

nscoord nsBox::GetXULBoxAscent(nsBoxLayoutState& aState) {
  if (IsXULCollapsed()) return 0;

  return GetXULPrefSize(aState).height;
}

bool nsBox::IsXULCollapsed() {
  return StyleVisibility()->mVisible == NS_STYLE_VISIBILITY_COLLAPSE;
}

nsresult nsIFrame::XULLayout(nsBoxLayoutState& aState) {
  NS_ASSERTION(aState.GetRenderingContext(), "must have rendering context");

  nsBox* box = static_cast<nsBox*>(this);
  DISPLAY_LAYOUT(box);

  box->BeginXULLayout(aState);

  box->DoXULLayout(aState);

  box->EndXULLayout(aState);

  return NS_OK;
}

bool nsBox::DoesClipChildren() {
  const nsStyleDisplay* display = StyleDisplay();
  NS_ASSERTION((display->mOverflowY == NS_STYLE_OVERFLOW_CLIP) ==
                   (display->mOverflowX == NS_STYLE_OVERFLOW_CLIP),
               "If one overflow is clip, the other should be too");
  return display->mOverflowX == NS_STYLE_OVERFLOW_CLIP;
}

nsresult nsBox::SyncLayout(nsBoxLayoutState& aState) {
  /*
  if (IsXULCollapsed()) {
    CollapseChild(aState, this, true);
    return NS_OK;
  }
  */

  if (GetStateBits() & NS_FRAME_IS_DIRTY) XULRedraw(aState);

  RemoveStateBits(NS_FRAME_HAS_DIRTY_CHILDREN | NS_FRAME_IS_DIRTY |
                  NS_FRAME_FIRST_REFLOW | NS_FRAME_IN_REFLOW);

  nsPresContext* presContext = aState.PresContext();

  uint32_t flags = GetXULLayoutFlags();

  uint32_t stateFlags = aState.LayoutFlags();

  flags |= stateFlags;

  nsRect visualOverflow;

  if (ComputesOwnOverflowArea()) {
    visualOverflow = GetVisualOverflowRect();
  } else {
    nsRect rect(nsPoint(0, 0), GetSize());
    nsOverflowAreas overflowAreas(rect, rect);
    if (!DoesClipChildren() && !IsXULCollapsed()) {
      // See if our child frames caused us to overflow after being laid
      // out. If so, store the overflow area.  This normally can't happen
      // in XUL, but it can happen with the CSS 'outline' property and
      // possibly with other exotic stuff (e.g. relatively positioned
      // frames in HTML inside XUL).
      nsLayoutUtils::UnionChildOverflow(this, overflowAreas);
    }

    FinishAndStoreOverflow(overflowAreas, GetSize());
    visualOverflow = overflowAreas.VisualOverflow();
  }

  nsView* view = GetView();
  if (view) {
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    nsContainerFrame::SyncFrameViewAfterReflow(presContext, this, view,
                                               visualOverflow, flags);
  }

  return NS_OK;
}

nsresult nsIFrame::XULRedraw(nsBoxLayoutState& aState) {
  if (aState.PaintingDisabled()) return NS_OK;

  // nsStackLayout, at least, expects us to repaint descendants even
  // if a damage rect is provided
  InvalidateFrameSubtree();

  return NS_OK;
}

bool nsIFrame::AddXULPrefSize(nsIFrame* aBox, nsSize& aSize, bool& aWidthSet,
                              bool& aHeightSet) {
  aWidthSet = false;
  aHeightSet = false;

  // add in the css min, max, pref
  const nsStylePosition* position = aBox->StylePosition();

  // see if the width or height was specifically set
  // XXX Handle eStyleUnit_Enumerated?
  // (Handling the eStyleUnit_Enumerated types requires
  // GetXULPrefSize/GetXULMinSize methods that don't consider
  // (min-/max-/)(width/height) properties.)
  const nsStyleCoord& width = position->mWidth;
  if (width.GetUnit() == eStyleUnit_Coord) {
    aSize.width = width.GetCoordValue();
    aWidthSet = true;
  } else if (width.IsCalcUnit()) {
    if (!width.CalcHasPercent()) {
      // pass 0 for percentage basis since we know there are no %s
      aSize.width = width.ComputeComputedCalc(0);
      if (aSize.width < 0) aSize.width = 0;
      aWidthSet = true;
    }
  }

  const nsStyleCoord& height = position->mHeight;
  if (height.GetUnit() == eStyleUnit_Coord) {
    aSize.height = height.GetCoordValue();
    aHeightSet = true;
  } else if (height.IsCalcUnit()) {
    if (!height.CalcHasPercent()) {
      // pass 0 for percentage basis since we know there are no %s
      aSize.height = height.ComputeComputedCalc(0);
      if (aSize.height < 0) aSize.height = 0;
      aHeightSet = true;
    }
  }

  nsIContent* content = aBox->GetContent();
  // ignore 'height' and 'width' attributes if the actual element is not XUL
  // For example, we might be magic XUL frames whose primary content is an HTML
  // <select>
  if (content && content->IsXULElement()) {
    nsAutoString value;
    nsresult error;

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::width, value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      aSize.width = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      aWidthSet = true;
    }

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::height, value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      aSize.height =
          nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      aHeightSet = true;
    }
  }

  return (aWidthSet && aHeightSet);
}

// This returns the scrollbar width we want to use when either native
// theme is disabled, or the native theme claims that it doesn't support
// scrollbar.
static nscoord GetScrollbarWidthNoTheme(nsIFrame* aBox) {
  ComputedStyle* scrollbarStyle = nsLayoutUtils::StyleForScrollbar(aBox);
  switch (scrollbarStyle->StyleUIReset()->mScrollbarWidth) {
    default:
    case StyleScrollbarWidth::Auto:
      return 12 * AppUnitsPerCSSPixel();
    case StyleScrollbarWidth::Thin:
      return 6 * AppUnitsPerCSSPixel();
    case StyleScrollbarWidth::None:
      return 0;
  }
}

bool nsIFrame::AddXULMinSize(nsBoxLayoutState& aState, nsIFrame* aBox,
                             nsSize& aSize, bool& aWidthSet, bool& aHeightSet) {
  aWidthSet = false;
  aHeightSet = false;

  bool canOverride = true;

  // See if a native theme wants to supply a minimum size.
  const nsStyleDisplay* display = aBox->StyleDisplay();
  if (display->HasAppearance()) {
    nsITheme* theme = aState.PresContext()->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aState.PresContext(), aBox,
                                            display->mAppearance)) {
      LayoutDeviceIntSize size;
      theme->GetMinimumWidgetSize(aState.PresContext(), aBox,
                                  display->mAppearance, &size, &canOverride);
      if (size.width) {
        aSize.width = aState.PresContext()->DevPixelsToAppUnits(size.width);
        aWidthSet = true;
      }
      if (size.height) {
        aSize.height = aState.PresContext()->DevPixelsToAppUnits(size.height);
        aHeightSet = true;
      }
    } else {
      switch (display->mAppearance) {
        case StyleAppearance::ScrollbarVertical:
          aSize.width = GetScrollbarWidthNoTheme(aBox);
          aWidthSet = true;
          break;
        case StyleAppearance::ScrollbarHorizontal:
          aSize.height = GetScrollbarWidthNoTheme(aBox);
          aHeightSet = true;
          break;
        default:
          break;
      }
    }
  }

  // add in the css min, max, pref
  const nsStylePosition* position = aBox->StylePosition();

  // same for min size. Unfortunately min size is always set to 0. So for now
  // we will assume 0 (as a coord) means not set.
  const nsStyleCoord& minWidth = position->mMinWidth;
  if ((minWidth.GetUnit() == eStyleUnit_Coord &&
       minWidth.GetCoordValue() != 0) ||
      (minWidth.IsCalcUnit() && !minWidth.CalcHasPercent())) {
    nscoord min = minWidth.ComputeCoordPercentCalc(0);
    if (!aWidthSet || (min > aSize.width && canOverride)) {
      aSize.width = min;
      aWidthSet = true;
    }
  } else if (minWidth.GetUnit() == eStyleUnit_Percent) {
    NS_ASSERTION(minWidth.GetPercentValue() == 0.0f,
                 "Non-zero percentage values not currently supported");
    aSize.width = 0;
    aWidthSet = true;  // FIXME: should we really do this for
                       // nonzero values?
  }
  // XXX Handle eStyleUnit_Enumerated?
  // (Handling the eStyleUnit_Enumerated types requires
  // GetXULPrefSize/GetXULMinSize methods that don't consider
  // (min-/max-/)(width/height) properties.
  // calc() with percentage is treated like '0' (unset)

  const nsStyleCoord& minHeight = position->mMinHeight;
  if ((minHeight.GetUnit() == eStyleUnit_Coord &&
       minHeight.GetCoordValue() != 0) ||
      (minHeight.IsCalcUnit() && !minHeight.CalcHasPercent())) {
    nscoord min = minHeight.ComputeCoordPercentCalc(0);
    if (!aHeightSet || (min > aSize.height && canOverride)) {
      aSize.height = min;
      aHeightSet = true;
    }
  } else if (minHeight.GetUnit() == eStyleUnit_Percent) {
    NS_ASSERTION(position->mMinHeight.GetPercentValue() == 0.0f,
                 "Non-zero percentage values not currently supported");
    aSize.height = 0;
    aHeightSet = true;  // FIXME: should we really do this for
                        // nonzero values?
  }
  // calc() with percentage is treated like '0' (unset)

  nsIContent* content = aBox->GetContent();
  if (content && content->IsXULElement()) {
    nsAutoString value;
    nsresult error;

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::minwidth,
                                  value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      nscoord val = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      if (val > aSize.width) aSize.width = val;
      aWidthSet = true;
    }

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::minheight,
                                  value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      nscoord val = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      if (val > aSize.height) aSize.height = val;

      aHeightSet = true;
    }
  }

  return (aWidthSet && aHeightSet);
}

bool nsIFrame::AddXULMaxSize(nsIFrame* aBox, nsSize& aSize, bool& aWidthSet,
                             bool& aHeightSet) {
  aWidthSet = false;
  aHeightSet = false;

  // add in the css min, max, pref
  const nsStylePosition* position = aBox->StylePosition();

  // and max
  // see if the width or height was specifically set
  // XXX Handle eStyleUnit_Enumerated?
  // (Handling the eStyleUnit_Enumerated types requires
  // GetXULPrefSize/GetXULMinSize methods that don't consider
  // (min-/max-/)(width/height) properties.)
  const nsStyleCoord maxWidth = position->mMaxWidth;
  if (maxWidth.ConvertsToLength()) {
    aSize.width = maxWidth.ComputeCoordPercentCalc(0);
    aWidthSet = true;
  }
  // percentages and calc() with percentages are treated like 'none'

  const nsStyleCoord& maxHeight = position->mMaxHeight;
  if (maxHeight.ConvertsToLength()) {
    aSize.height = maxHeight.ComputeCoordPercentCalc(0);
    aHeightSet = true;
  }
  // percentages and calc() with percentages are treated like 'none'

  nsIContent* content = aBox->GetContent();
  if (content && content->IsXULElement()) {
    nsAutoString value;
    nsresult error;

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::maxwidth,
                                  value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      nscoord val = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      aSize.width = val;
      aWidthSet = true;
    }

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::maxheight,
                                  value);
    if (!value.IsEmpty()) {
      value.Trim("%");

      nscoord val = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      aSize.height = val;

      aHeightSet = true;
    }
  }

  return (aWidthSet || aHeightSet);
}

bool nsIFrame::AddXULFlex(nsIFrame* aBox, nscoord& aFlex) {
  bool flexSet = false;

  // get the flexibility
  aFlex = aBox->StyleXUL()->mBoxFlex;

  // attribute value overrides CSS
  nsIContent* content = aBox->GetContent();
  if (content && content->IsXULElement()) {
    nsresult error;
    nsAutoString value;

    content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::flex, value);
    if (!value.IsEmpty()) {
      value.Trim("%");
      aFlex = value.ToInteger(&error);
      flexSet = true;
    }
  }

  if (aFlex < 0) aFlex = 0;
  if (aFlex >= nscoord_MAX) aFlex = nscoord_MAX - 1;

  return flexSet || aFlex > 0;
}

void nsBox::AddBorderAndPadding(nsSize& aSize) {
  AddBorderAndPadding(this, aSize);
}

void nsBox::AddBorderAndPadding(nsIFrame* aBox, nsSize& aSize) {
  nsMargin borderPadding(0, 0, 0, 0);
  aBox->GetXULBorderAndPadding(borderPadding);
  AddMargin(aSize, borderPadding);
}

void nsBox::AddMargin(nsIFrame* aChild, nsSize& aSize) {
  nsMargin margin(0, 0, 0, 0);
  aChild->GetXULMargin(margin);
  AddMargin(aSize, margin);
}

void nsBox::AddMargin(nsSize& aSize, const nsMargin& aMargin) {
  if (aSize.width != NS_INTRINSICSIZE)
    aSize.width += aMargin.left + aMargin.right;

  if (aSize.height != NS_INTRINSICSIZE)
    aSize.height += aMargin.top + aMargin.bottom;
}

nscoord nsBox::BoundsCheck(nscoord aMin, nscoord aPref, nscoord aMax) {
  if (aPref > aMax) aPref = aMax;

  if (aPref < aMin) aPref = aMin;

  return aPref;
}

nsSize nsBox::BoundsCheckMinMax(const nsSize& aMinSize,
                                const nsSize& aMaxSize) {
  return nsSize(std::max(aMaxSize.width, aMinSize.width),
                std::max(aMaxSize.height, aMinSize.height));
}

nsSize nsBox::BoundsCheck(const nsSize& aMinSize, const nsSize& aPrefSize,
                          const nsSize& aMaxSize) {
  return nsSize(
      BoundsCheck(aMinSize.width, aPrefSize.width, aMaxSize.width),
      BoundsCheck(aMinSize.height, aPrefSize.height, aMaxSize.height));
}

/*static*/ nsIFrame* nsBox::GetChildXULBox(const nsIFrame* aFrame) {
  // box layout ends at box-wrapped frames, so don't allow these frames
  // to report child boxes.
  return aFrame->IsXULBoxFrame() ? aFrame->PrincipalChildList().FirstChild()
                                 : nullptr;
}

/*static*/ nsIFrame* nsBox::GetNextXULBox(const nsIFrame* aFrame) {
  return aFrame->GetParent() && aFrame->GetParent()->IsXULBoxFrame()
             ? aFrame->GetNextSibling()
             : nullptr;
}

/*static*/ nsIFrame* nsBox::GetParentXULBox(const nsIFrame* aFrame) {
  return aFrame->GetParent() && aFrame->GetParent()->IsXULBoxFrame()
             ? aFrame->GetParent()
             : nullptr;
}
