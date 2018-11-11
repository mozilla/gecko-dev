/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCCallbackHelper.h"

#include "TouchActionHelper.h"
#include "gfxPlatform.h" // For gfxPlatform::UseTiling
#include "gfxPrefs.h"
#include "LayersLogging.h"  // For Stringify
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/TouchEvents.h"
#include "nsContainerFrame.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsPrintfCString.h"
#include "nsRefreshDriver.h"
#include "nsString.h"
#include "nsView.h"
#include "Layers.h"

// #define APZCCH_LOGGING 1
#ifdef APZCCH_LOGGING
#define APZCCH_LOG(...) printf_stderr("APZCCH: " __VA_ARGS__)
#else
#define APZCCH_LOG(...)
#endif

namespace mozilla {
namespace layers {

using dom::TabParent;

uint64_t APZCCallbackHelper::sLastTargetAPZCNotificationInputBlock = uint64_t(-1);

ScreenMargin
APZCCallbackHelper::AdjustDisplayPortForScrollDelta(
    const RepaintRequest& aRequest,
    const CSSPoint& aActualScrollOffset)
{
  // Correct the display-port by the difference between the requested scroll
  // offset and the resulting scroll offset after setting the requested value.
  ScreenPoint shift =
      (aRequest.GetScrollOffset() - aActualScrollOffset) *
      aRequest.DisplayportPixelsPerCSSPixel();
  ScreenMargin margins = aRequest.GetDisplayPortMargins();
  margins.left -= shift.x;
  margins.right += shift.x;
  margins.top -= shift.y;
  margins.bottom += shift.y;
  return margins;
}

static ScreenMargin
RecenterDisplayPort(const ScreenMargin& aDisplayPort)
{
  ScreenMargin margins = aDisplayPort;
  margins.right = margins.left = margins.LeftRight() / 2;
  margins.top = margins.bottom = margins.TopBottom() / 2;
  return margins;
}

static already_AddRefed<nsIPresShell>
GetPresShell(const nsIContent* aContent)
{
  nsCOMPtr<nsIPresShell> result;
  if (nsIDocument* doc = aContent->GetComposedDoc()) {
    result = doc->GetShell();
  }
  return result.forget();
}

static CSSPoint
ScrollFrameTo(nsIScrollableFrame* aFrame, const RepaintRequest& aRequest, bool& aSuccessOut)
{
  aSuccessOut = false;
  CSSPoint targetScrollPosition = aRequest.IsRootContent()
    ? aRequest.GetViewport().TopLeft()
    : aRequest.GetScrollOffset();

  if (!aFrame) {
    return targetScrollPosition;
  }

  CSSPoint geckoScrollPosition = CSSPoint::FromAppUnits(aFrame->GetScrollPosition());

  // If the repaint request was triggered due to a previous main-thread scroll
  // offset update sent to the APZ, then we don't need to do another scroll here
  // and we can just return.
  if (!aRequest.GetScrollOffsetUpdated()) {
    return geckoScrollPosition;
  }

  // If this frame is overflow:hidden, then the expectation is that it was
  // sized in a way that respects its scrollable boundaries. For the root
  // frame, this means that it cannot be scrolled in such a way that it moves
  // the layout viewport. For a non-root frame, this means that it cannot be
  // scrolled at all.
  //
  // In either case, |targetScrollPosition| should be the same as
  // |geckoScrollPosition| here.
  //
  // However, this is slightly racy. We query the overflow property of the
  // scroll frame at the time the repaint request arrives at the main thread
  // (i.e., right now), but APZ made the decision of whether or not to allow
  // scrolling based on the information it had at the time it processed the
  // scroll event. The overflow property could have changed at some time
  // between the two events and so APZ may have computed a scrollable region
  // that is larger than what is actually allowed.
  //
  // Currently, we allow the scroll position to change even though the frame is
  // overflow:hidden (that is, we take |targetScrollPosition|). If this turns
  // out to be problematic, an alternative solution would be to ignore the
  // scroll position change (that is, use |geckoScrollPosition|).
  if (aFrame->GetScrollStyles().mVertical == NS_STYLE_OVERFLOW_HIDDEN &&
      targetScrollPosition.y != geckoScrollPosition.y) {
    NS_WARNING(nsPrintfCString(
          "APZCCH: targetScrollPosition.y (%f) != geckoScrollPosition.y (%f)",
          targetScrollPosition.y, geckoScrollPosition.y).get());
  }
  if (aFrame->GetScrollStyles().mHorizontal == NS_STYLE_OVERFLOW_HIDDEN &&
      targetScrollPosition.x != geckoScrollPosition.x) {
    NS_WARNING(nsPrintfCString(
          "APZCCH: targetScrollPosition.x (%f) != geckoScrollPosition.x (%f)",
          targetScrollPosition.x, geckoScrollPosition.x).get());
  }

  // If the scrollable frame is currently in the middle of an async or smooth
  // scroll then we don't want to interrupt it (see bug 961280).
  // Also if the scrollable frame got a scroll request from a higher priority origin
  // since the last layers update, then we don't want to push our scroll request
  // because we'll clobber that one, which is bad.
  bool scrollInProgress = APZCCallbackHelper::IsScrollInProgress(aFrame);
  if (!scrollInProgress) {
    aFrame->ScrollToCSSPixelsApproximate(targetScrollPosition, nsGkAtoms::apz);
    geckoScrollPosition = CSSPoint::FromAppUnits(aFrame->GetScrollPosition());
    aSuccessOut = true;
  }
  // Return the final scroll position after setting it so that anything that relies
  // on it can have an accurate value. Note that even if we set it above re-querying it
  // is a good idea because it may have gotten clamped or rounded.
  return geckoScrollPosition;
}

/**
 * Scroll the scroll frame associated with |aContent| to the scroll position
 * requested in |aRequest|.
 *
 * Any difference between the requested and actual scroll positions is used to
 * update the callback-transform stored on the content, and return a new
 * display port.
 */
static ScreenMargin
ScrollFrame(nsIContent* aContent,
            const RepaintRequest& aRequest)
{
  // Scroll the window to the desired spot
  nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aRequest.GetScrollId());
  if (sf) {
    sf->ResetScrollInfoIfGeneration(aRequest.GetScrollGeneration());
    sf->SetScrollableByAPZ(!aRequest.IsScrollInfoLayer());
    if (sf->IsRootScrollFrameOfDocument()) {
      if (nsCOMPtr<nsIPresShell> shell = GetPresShell(aContent)) {
        shell->SetVisualViewportOffset(CSSPoint::ToAppUnits(aRequest.GetScrollOffset()));
      }
    }
  }
  bool scrollUpdated = false;
  ScreenMargin displayPortMargins = aRequest.GetDisplayPortMargins();
  CSSPoint apzScrollOffset = aRequest.GetScrollOffset();
  CSSPoint actualScrollOffset = ScrollFrameTo(sf, aRequest, scrollUpdated);

  if (scrollUpdated) {
    if (aRequest.IsScrollInfoLayer()) {
      // In cases where the APZ scroll offset is different from the content scroll
      // offset, we want to interpret the margins as relative to the APZ scroll
      // offset except when the frame is not scrollable by APZ. Therefore, if the
      // layer is a scroll info layer, we leave the margins as-is and they will
      // be interpreted as relative to the content scroll offset.
      if (nsIFrame* frame = aContent->GetPrimaryFrame()) {
        frame->SchedulePaint();
      }
    } else {
      // Correct the display port due to the difference between mScrollOffset and the
      // actual scroll offset.
      displayPortMargins = APZCCallbackHelper::AdjustDisplayPortForScrollDelta(aRequest, actualScrollOffset);
    }
  } else if (aRequest.IsRootContent() &&
             aRequest.GetScrollOffset() != aRequest.GetViewport().TopLeft()) {
    // APZ uses the visual viewport's offset to calculate where to place the
    // display port, so the display port is misplaced when a pinch zoom occurs.
    //
    // We need to force a display port adjustment in the following paint to
    // account for a difference between mScrollOffset and the actual scroll
    // offset in repaints requested by AsyncPanZoomController::NotifyLayersUpdated.
    displayPortMargins = APZCCallbackHelper::AdjustDisplayPortForScrollDelta(aRequest, actualScrollOffset);
  } else {
    // For whatever reason we couldn't update the scroll offset on the scroll frame,
    // which means the data APZ used for its displayport calculation is stale. Fall
    // back to a sane default behaviour. Note that we don't tile-align the recentered
    // displayport because tile-alignment depends on the scroll position, and the
    // scroll position here is out of our control. See bug 966507 comment 21 for a
    // more detailed explanation.
    displayPortMargins = RecenterDisplayPort(aRequest.GetDisplayPortMargins());
  }

  // APZ transforms inputs assuming we applied the exact scroll offset it
  // requested (|apzScrollOffset|). Since we may not have, record the difference
  // between what APZ asked for and what we actually applied, and apply it to
  // input events to compensate.
  // Note that if the main-thread had a change in its scroll position, we don't
  // want to record that difference here, because it can be large and throw off
  // input events by a large amount. It is also going to be transient, because
  // any main-thread scroll position change will be synced to APZ and we will
  // get another repaint request when APZ confirms. In the interval while this
  // is happening we can just leave the callback transform as it was.
  bool mainThreadScrollChanged =
    sf && sf->CurrentScrollGeneration() != aRequest.GetScrollGeneration() && nsLayoutUtils::CanScrollOriginClobberApz(sf->LastScrollOrigin());
  if (aContent && !mainThreadScrollChanged) {
    CSSPoint scrollDelta = apzScrollOffset - actualScrollOffset;
    aContent->SetProperty(nsGkAtoms::apzCallbackTransform, new CSSPoint(scrollDelta),
                          nsINode::DeleteProperty<CSSPoint>);
  }

  return displayPortMargins;
}

static void
SetDisplayPortMargins(nsIPresShell* aPresShell,
                      nsIContent* aContent,
                      ScreenMargin aDisplayPortMargins,
                      CSSSize aDisplayPortBase)
{
  if (!aContent) {
    return;
  }

  bool hadDisplayPort = nsLayoutUtils::HasDisplayPort(aContent);
  nsLayoutUtils::SetDisplayPortMargins(aContent, aPresShell, aDisplayPortMargins, 0);
  if (!hadDisplayPort) {
    nsLayoutUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
        aContent->GetPrimaryFrame(), nsLayoutUtils::RepaintMode::Repaint);
  }

  nsRect base(0, 0,
              aDisplayPortBase.width * AppUnitsPerCSSPixel(),
              aDisplayPortBase.height * AppUnitsPerCSSPixel());
  nsLayoutUtils::SetDisplayPortBaseIfNotSet(aContent, base);
}

static void
SetPaintRequestTime(nsIContent* aContent, const TimeStamp& aPaintRequestTime)
{
  aContent->SetProperty(nsGkAtoms::paintRequestTime,
                        new TimeStamp(aPaintRequestTime),
                        nsINode::DeleteProperty<TimeStamp>);
}

void
APZCCallbackHelper::UpdateRootFrame(const RepaintRequest& aRequest)
{
  if (aRequest.GetScrollId() == ScrollableLayerGuid::NULL_SCROLL_ID) {
    return;
  }
  nsIContent* content = nsLayoutUtils::FindContentFor(aRequest.GetScrollId());
  if (!content) {
    return;
  }

  nsCOMPtr<nsIPresShell> shell = GetPresShell(content);
  if (!shell || aRequest.GetPresShellId() != shell->GetPresShellId()) {
    return;
  }

  MOZ_ASSERT(aRequest.GetUseDisplayPortMargins());

  if (gfxPrefs::APZAllowZooming() && aRequest.GetScrollOffsetUpdated()) {
    // If zooming is disabled then we don't really want to let APZ fiddle
    // with these things. In theory setting the resolution here should be a
    // no-op, but setting the visual viewport size is bad because it can cause a
    // stale value to be returned by window.innerWidth/innerHeight (see bug 1187792).
    //
    // We also skip this codepath unless the metrics has a scroll offset update
    // type other eNone, because eNone just means that this repaint request
    // was triggered by APZ in response to a main-thread update. In this
    // scenario we don't want to update the main-thread resolution because
    // it can trigger unnecessary reflows.

    float presShellResolution = shell->GetResolution();

    // If the pres shell resolution has changed on the content side side
    // the time this repaint request was fired, consider this request out of date
    // and drop it; setting a zoom based on the out-of-date resolution can have
    // the effect of getting us stuck with the stale resolution.
    if (!FuzzyEqualsMultiplicative(presShellResolution, aRequest.GetPresShellResolution())) {
      return;
    }

    // The pres shell resolution is updated by the the async zoom since the
    // last paint.
    presShellResolution = aRequest.GetPresShellResolution()
                        * aRequest.GetAsyncZoom().scale;
    shell->SetResolutionAndScaleTo(presShellResolution);
  }

  // Do this as late as possible since scrolling can flush layout. It also
  // adjusts the display port margins, so do it before we set those.
  ScreenMargin displayPortMargins = ScrollFrame(content, aRequest);

  SetDisplayPortMargins(shell,
    content,
    displayPortMargins,
    aRequest.CalculateCompositedSizeInCssPixels());
  SetPaintRequestTime(content, aRequest.GetPaintRequestTime());
}

void
APZCCallbackHelper::UpdateSubFrame(const RepaintRequest& aRequest)
{
  if (aRequest.GetScrollId() == ScrollableLayerGuid::NULL_SCROLL_ID) {
    return;
  }
  nsIContent* content = nsLayoutUtils::FindContentFor(aRequest.GetScrollId());
  if (!content) {
    return;
  }

  MOZ_ASSERT(aRequest.GetUseDisplayPortMargins());

  // We don't currently support zooming for subframes, so nothing extra
  // needs to be done beyond the tasks common to this and UpdateRootFrame.
  ScreenMargin displayPortMargins = ScrollFrame(content, aRequest);
  if (nsCOMPtr<nsIPresShell> shell = GetPresShell(content)) {
    SetDisplayPortMargins(shell,
      content,
      displayPortMargins,
      aRequest.CalculateCompositedSizeInCssPixels());
  }
  SetPaintRequestTime(content, aRequest.GetPaintRequestTime());
}

bool
APZCCallbackHelper::GetOrCreateScrollIdentifiers(nsIContent* aContent,
                                                 uint32_t* aPresShellIdOut,
                                                 ScrollableLayerGuid::ViewID* aViewIdOut)
{
    if (!aContent) {
      return false;
    }
    *aViewIdOut = nsLayoutUtils::FindOrCreateIDFor(aContent);
    if (nsCOMPtr<nsIPresShell> shell = GetPresShell(aContent)) {
      *aPresShellIdOut = shell->GetPresShellId();
      return true;
    }
    return false;
}

void
APZCCallbackHelper::InitializeRootDisplayport(nsIPresShell* aPresShell)
{
  // Create a view-id and set a zero-margin displayport for the root element
  // of the root document in the chrome process. This ensures that the scroll
  // frame for this element gets an APZC, which in turn ensures that all content
  // in the chrome processes is covered by an APZC.
  // The displayport is zero-margin because this element is generally not
  // actually scrollable (if it is, APZC will set proper margins when it's
  // scrolled).
  if (!aPresShell) {
    return;
  }

  MOZ_ASSERT(aPresShell->GetDocument());
  nsIContent* content = aPresShell->GetDocument()->GetDocumentElement();
  if (!content) {
    return;
  }

  uint32_t presShellId;
  ScrollableLayerGuid::ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(content, &presShellId, &viewId)) {
    nsPresContext* pc = aPresShell->GetPresContext();
    // This code is only correct for root content or toplevel documents.
    MOZ_ASSERT(!pc || pc->IsRootContentDocument() || !pc->GetParentPresContext());
    nsIFrame* frame = aPresShell->GetRootScrollFrame();
    if (!frame) {
      frame = aPresShell->GetRootFrame();
    }
    nsRect baseRect;
    if (frame) {
      baseRect =
        nsRect(nsPoint(0, 0), nsLayoutUtils::CalculateCompositionSizeForFrame(frame));
    } else if (pc) {
      baseRect = nsRect(nsPoint(0, 0), pc->GetVisibleArea().Size());
    }
    nsLayoutUtils::SetDisplayPortBaseIfNotSet(content, baseRect);
    // Note that we also set the base rect that goes with these margins in
    // nsRootBoxFrame::BuildDisplayList.
    nsLayoutUtils::SetDisplayPortMargins(content, aPresShell, ScreenMargin(), 0,
        nsLayoutUtils::RepaintMode::DoNotRepaint);
    nsLayoutUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
        content->GetPrimaryFrame(), nsLayoutUtils::RepaintMode::DoNotRepaint);
  }
}

nsPresContext*
APZCCallbackHelper::GetPresContextForContent(nsIContent* aContent)
{
  nsIDocument* doc = aContent->GetComposedDoc();
  if (!doc) {
      return nullptr;
  }
  nsIPresShell* shell = doc->GetShell();
  if (!shell) {
      return nullptr;
  }
  return shell->GetPresContext();
}

nsIPresShell*
APZCCallbackHelper::GetRootContentDocumentPresShellForContent(nsIContent* aContent)
{
    nsPresContext* context = GetPresContextForContent(aContent);
    if (!context) {
        return nullptr;
    }
    context = context->GetToplevelContentDocumentPresContext();
    if (!context) {
        return nullptr;
    }
    return context->PresShell();
}

static nsIPresShell*
GetRootDocumentPresShell(nsIContent* aContent)
{
    nsIDocument* doc = aContent->GetComposedDoc();
    if (!doc) {
        return nullptr;
    }
    nsIPresShell* shell = doc->GetShell();
    if (!shell) {
        return nullptr;
    }
    nsPresContext* context = shell->GetPresContext();
    if (!context) {
        return nullptr;
    }
    context = context->GetRootPresContext();
    if (!context) {
        return nullptr;
    }
    return context->PresShell();
}

CSSPoint
APZCCallbackHelper::ApplyCallbackTransform(const CSSPoint& aInput,
                                           const ScrollableLayerGuid& aGuid)
{
    CSSPoint input = aInput;
    if (aGuid.mScrollId == ScrollableLayerGuid::NULL_SCROLL_ID) {
        return input;
    }
    nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aGuid.mScrollId);
    if (!content) {
        return input;
    }

    // First, scale inversely by the root content document's pres shell
    // resolution to cancel the scale-to-resolution transform that the
    // compositor adds to the layer with the pres shell resolution. The points
    // sent to Gecko by APZ don't have this transform unapplied (unlike other
    // compositor-side transforms) because APZ doesn't know about it.
    if (nsIPresShell* shell = GetRootDocumentPresShell(content)) {
        input = input / shell->GetResolution();
    }

    // This represents any resolution on the Root Content Document (RCD)
    // that's not on the Root Document (RD). That is, on platforms where
    // RCD == RD, it's 1, and on platforms where RCD != RD, it's the RCD
    // resolution. 'input' has this resolution applied, but the scroll
    // delta retrieved below do not, so we need to apply them to the
    // delta before adding the delta to 'input'. (Technically, deltas
    // from scroll frames outside the RCD would already have this
    // resolution applied, but we don't have such scroll frames in
    // practice.)
    float nonRootResolution = 1.0f;
    if (nsIPresShell* shell = GetRootContentDocumentPresShellForContent(content)) {
      nonRootResolution = shell->GetCumulativeNonRootScaleResolution();
    }
    // Now apply the callback-transform. This is only approximately correct,
    // see the comment on GetCumulativeApzCallbackTransform for details.
    CSSPoint transform = nsLayoutUtils::GetCumulativeApzCallbackTransform(content->GetPrimaryFrame());
    return input + transform * nonRootResolution;
}

LayoutDeviceIntPoint
APZCCallbackHelper::ApplyCallbackTransform(const LayoutDeviceIntPoint& aPoint,
                                           const ScrollableLayerGuid& aGuid,
                                           const CSSToLayoutDeviceScale& aScale)
{
    LayoutDevicePoint point = LayoutDevicePoint(aPoint.x, aPoint.y);
    point = ApplyCallbackTransform(point / aScale, aGuid) * aScale;
    return LayoutDeviceIntPoint::Round(point);
}

void
APZCCallbackHelper::ApplyCallbackTransform(WidgetEvent& aEvent,
                                           const ScrollableLayerGuid& aGuid,
                                           const CSSToLayoutDeviceScale& aScale)
{
  if (aEvent.AsTouchEvent()) {
    WidgetTouchEvent& event = *(aEvent.AsTouchEvent());
    for (size_t i = 0; i < event.mTouches.Length(); i++) {
      event.mTouches[i]->mRefPoint = ApplyCallbackTransform(
          event.mTouches[i]->mRefPoint, aGuid, aScale);
    }
  } else {
    aEvent.mRefPoint = ApplyCallbackTransform(aEvent.mRefPoint, aGuid, aScale);
  }
}

nsEventStatus
APZCCallbackHelper::DispatchWidgetEvent(WidgetGUIEvent& aEvent)
{
  nsEventStatus status = nsEventStatus_eConsumeNoDefault;
  if (aEvent.mWidget) {
    aEvent.mWidget->DispatchEvent(&aEvent, status);
  }
  return status;
}

nsEventStatus
APZCCallbackHelper::DispatchSynthesizedMouseEvent(EventMessage aMsg,
                                                  uint64_t aTime,
                                                  const LayoutDevicePoint& aRefPoint,
                                                  Modifiers aModifiers,
                                                  int32_t aClickCount,
                                                  nsIWidget* aWidget)
{
  MOZ_ASSERT(aMsg == eMouseMove || aMsg == eMouseDown ||
             aMsg == eMouseUp || aMsg == eMouseLongTap);

  WidgetMouseEvent event(true, aMsg, aWidget,
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
  event.mRefPoint = LayoutDeviceIntPoint::Truncate(aRefPoint.x, aRefPoint.y);
  event.mTime = aTime;
  event.button = WidgetMouseEvent::eLeftButton;
  event.inputSource = dom::MouseEvent_Binding::MOZ_SOURCE_TOUCH;
  if (aMsg == eMouseLongTap) {
    event.mFlags.mOnlyChromeDispatch = true;
  }
  event.mIgnoreRootScrollFrame = true;
  if (aMsg != eMouseMove) {
    event.mClickCount = aClickCount;
  }
  event.mModifiers = aModifiers;
  // Real touch events will generate corresponding pointer events. We set
  // convertToPointer to false to prevent the synthesized mouse events generate
  // pointer events again.
  event.convertToPointer = false;
  return DispatchWidgetEvent(event);
}

bool
APZCCallbackHelper::DispatchMouseEvent(const nsCOMPtr<nsIPresShell>& aPresShell,
                                       const nsString& aType,
                                       const CSSPoint& aPoint,
                                       int32_t aButton,
                                       int32_t aClickCount,
                                       int32_t aModifiers,
                                       bool aIgnoreRootScrollFrame,
                                       unsigned short aInputSourceArg,
                                       uint32_t aPointerId)
{
  NS_ENSURE_TRUE(aPresShell, true);

  bool defaultPrevented = false;
  nsContentUtils::SendMouseEvent(aPresShell, aType, aPoint.x, aPoint.y,
      aButton, nsIDOMWindowUtils::MOUSE_BUTTONS_NOT_SPECIFIED, aClickCount,
      aModifiers, aIgnoreRootScrollFrame, 0, aInputSourceArg, aPointerId, false,
      &defaultPrevented, false, /* aIsWidgetEventSynthesized = */ false);
  return defaultPrevented;
}


void
APZCCallbackHelper::FireSingleTapEvent(const LayoutDevicePoint& aPoint,
                                       Modifiers aModifiers,
                                       int32_t aClickCount,
                                       nsIWidget* aWidget)
{
  if (aWidget->Destroyed()) {
    return;
  }
  APZCCH_LOG("Dispatching single-tap component events to %s\n",
    Stringify(aPoint).c_str());
  int time = 0;
  DispatchSynthesizedMouseEvent(eMouseMove, time, aPoint, aModifiers, aClickCount, aWidget);
  DispatchSynthesizedMouseEvent(eMouseDown, time, aPoint, aModifiers, aClickCount, aWidget);
  DispatchSynthesizedMouseEvent(eMouseUp, time, aPoint, aModifiers, aClickCount, aWidget);
}

static dom::Element*
GetDisplayportElementFor(nsIScrollableFrame* aScrollableFrame)
{
  if (!aScrollableFrame) {
    return nullptr;
  }
  nsIFrame* scrolledFrame = aScrollableFrame->GetScrolledFrame();
  if (!scrolledFrame) {
    return nullptr;
  }
  // |scrolledFrame| should at this point be the root content frame of the
  // nearest ancestor scrollable frame. The element corresponding to this
  // frame should be the one with the displayport set on it, so find that
  // element and return it.
  nsIContent* content = scrolledFrame->GetContent();
  MOZ_ASSERT(content->IsElement()); // roc says this must be true
  return content->AsElement();
}


static dom::Element*
GetRootDocumentElementFor(nsIWidget* aWidget)
{
  // This returns the root element that ChromeProcessController sets the
  // displayport on during initialization.
  if (nsView* view = nsView::GetViewFor(aWidget)) {
    if (nsIPresShell* shell = view->GetPresShell()) {
      MOZ_ASSERT(shell->GetDocument());
      return shell->GetDocument()->GetDocumentElement();
    }
  }
  return nullptr;
}

static nsIFrame*
UpdateRootFrameForTouchTargetDocument(nsIFrame* aRootFrame)
{
#if defined(MOZ_WIDGET_ANDROID)
  // Re-target so that the hit test is performed relative to the frame for the
  // Root Content Document instead of the Root Document which are different in
  // Android. See bug 1229752 comment 16 for an explanation of why this is necessary.
  if (nsIDocument* doc = aRootFrame->PresShell()->GetPrimaryContentDocument()) {
    if (nsIPresShell* shell = doc->GetShell()) {
      if (nsIFrame* frame = shell->GetRootFrame()) {
        return frame;
      }
    }
  }
#endif
  return aRootFrame;
}

// Determine the scrollable target frame for the given point and add it to
// the target list. If the frame doesn't have a displayport, set one.
// Return whether or not a displayport was set.
static bool
PrepareForSetTargetAPZCNotification(nsIWidget* aWidget,
                                    const ScrollableLayerGuid& aGuid,
                                    nsIFrame* aRootFrame,
                                    const LayoutDeviceIntPoint& aRefPoint,
                                    nsTArray<ScrollableLayerGuid>* aTargets)
{
  ScrollableLayerGuid guid(aGuid.mLayersId, 0, ScrollableLayerGuid::NULL_SCROLL_ID);
  nsPoint point =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(aWidget, aRefPoint, aRootFrame);
  uint32_t flags = 0;
  if (gfxPrefs::APZAllowZooming()) {
    // If zooming is enabled, we need IGNORE_ROOT_SCROLL_FRAME for correct
    // hit testing. Otherwise, don't use it because it interferes with
    // hit testing for some purposes such as scrollbar dragging (this will
    // need to be fixed before enabling zooming by default on desktop).
    flags = nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME;
  }
  nsIFrame* target =
    nsLayoutUtils::GetFrameForPoint(aRootFrame, point, flags);
  nsIScrollableFrame* scrollAncestor = target
    ? nsLayoutUtils::GetAsyncScrollableAncestorFrame(target)
    : aRootFrame->PresShell()->GetRootScrollFrameAsScrollable();

  // Assuming that if there's no scrollAncestor, there's already a displayPort.
  nsCOMPtr<dom::Element> dpElement = scrollAncestor
    ? GetDisplayportElementFor(scrollAncestor)
    : GetRootDocumentElementFor(aWidget);

#ifdef APZCCH_LOGGING
  nsAutoString dpElementDesc;
  if (dpElement) {
    dpElement->Describe(dpElementDesc);
  }
  APZCCH_LOG("For event at %s found scrollable element %p (%s)\n",
      Stringify(aRefPoint).c_str(), dpElement.get(),
      NS_LossyConvertUTF16toASCII(dpElementDesc).get());
#endif

  bool guidIsValid = APZCCallbackHelper::GetOrCreateScrollIdentifiers(
    dpElement, &(guid.mPresShellId), &(guid.mScrollId));
  aTargets->AppendElement(guid);

  if (!guidIsValid || nsLayoutUtils::HasDisplayPort(dpElement)) {
    return false;
  }

  if (!scrollAncestor) {
    // This can happen if the document element gets swapped out after ChromeProcessController
    // runs InitializeRootDisplayport. In this case let's try to set a displayport again and
    // bail out on this operation.
    APZCCH_LOG("Widget %p's document element %p didn't have a displayport\n",
        aWidget, dpElement.get());
    APZCCallbackHelper::InitializeRootDisplayport(aRootFrame->PresShell());
    return false;
  }

  APZCCH_LOG("%p didn't have a displayport, so setting one...\n", dpElement.get());
  bool activated = nsLayoutUtils::CalculateAndSetDisplayPortMargins(
      scrollAncestor, nsLayoutUtils::RepaintMode::Repaint);
  if (!activated) {
    return false;
  }

  nsIFrame* frame = do_QueryFrame(scrollAncestor);
  nsLayoutUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(frame,
    nsLayoutUtils::RepaintMode::Repaint);

  return true;
}

static void
SendLayersDependentApzcTargetConfirmation(nsIPresShell* aShell, uint64_t aInputBlockId,
                                          const nsTArray<ScrollableLayerGuid>& aTargets)
{
  LayerManager* lm = aShell->GetLayerManager();
  if (!lm) {
    return;
  }

  if (WebRenderLayerManager* wrlm = lm->AsWebRenderLayerManager()) {
    if (WebRenderBridgeChild* wrbc = wrlm->WrBridge()) {
      wrbc->SendSetConfirmedTargetAPZC(aInputBlockId, aTargets);
    }
    return;
  }

  ShadowLayerForwarder* lf = lm->AsShadowForwarder();
  if (!lf) {
    return;
  }

  LayerTransactionChild* shadow = lf->GetShadowManager();
  if (!shadow) {
    return;
  }

  shadow->SendSetConfirmedTargetAPZC(aInputBlockId, aTargets);
}

DisplayportSetListener::DisplayportSetListener(nsIWidget* aWidget,
                                               nsIPresShell* aPresShell,
                                               const uint64_t& aInputBlockId,
                                               const nsTArray<ScrollableLayerGuid>& aTargets)
  : mWidget(aWidget)
  , mPresShell(aPresShell)
  , mInputBlockId(aInputBlockId)
  , mTargets(aTargets)
{
}

DisplayportSetListener:: ~DisplayportSetListener()
{
}

bool
DisplayportSetListener::Register()
{
  if (mPresShell->AddPostRefreshObserver(this)) {
    APZCCH_LOG("Successfully registered post-refresh observer\n");
    return true;
  }
  // In case of failure just send the notification right away
  APZCCH_LOG("Sending target APZCs for input block %" PRIu64 "\n", mInputBlockId);
  mWidget->SetConfirmedTargetAPZC(mInputBlockId, mTargets);
  return false;
}

void
DisplayportSetListener::DidRefresh() {
  if (!mPresShell) {
    MOZ_ASSERT_UNREACHABLE("Post-refresh observer fired again after failed attempt at unregistering it");
    return;
  }

  APZCCH_LOG("Got refresh, sending target APZCs for input block %" PRIu64 "\n", mInputBlockId);
  SendLayersDependentApzcTargetConfirmation(mPresShell, mInputBlockId, std::move(mTargets));

  if (!mPresShell->RemovePostRefreshObserver(this)) {
    MOZ_ASSERT_UNREACHABLE("Unable to unregister post-refresh observer! Leaking it instead of leaving garbage registered");
    // Graceful handling, just in case...
    mPresShell = nullptr;
    return;
  }

  delete this;
}

UniquePtr<DisplayportSetListener>
APZCCallbackHelper::SendSetTargetAPZCNotification(nsIWidget* aWidget,
                                                  nsIDocument* aDocument,
                                                  const WidgetGUIEvent& aEvent,
                                                  const ScrollableLayerGuid& aGuid,
                                                  uint64_t aInputBlockId)
{
  if (!aWidget || !aDocument) {
    return nullptr;
  }
  if (aInputBlockId == sLastTargetAPZCNotificationInputBlock) {
    // We have already confirmed the target APZC for a previous event of this
    // input block. If we activated a scroll frame for this input block,
    // sending another target APZC confirmation would be harmful, as it might
    // race the original confirmation (which needs to go through a layers
    // transaction).
    APZCCH_LOG("Not resending target APZC confirmation for input block %" PRIu64 "\n", aInputBlockId);
    return nullptr;
  }
  sLastTargetAPZCNotificationInputBlock = aInputBlockId;
  if (nsIPresShell* shell = aDocument->GetShell()) {
    if (nsIFrame* rootFrame = shell->GetRootFrame()) {
      rootFrame = UpdateRootFrameForTouchTargetDocument(rootFrame);

      bool waitForRefresh = false;
      nsTArray<ScrollableLayerGuid> targets;

      if (const WidgetTouchEvent* touchEvent = aEvent.AsTouchEvent()) {
        for (size_t i = 0; i < touchEvent->mTouches.Length(); i++) {
          waitForRefresh |= PrepareForSetTargetAPZCNotification(aWidget, aGuid,
              rootFrame, touchEvent->mTouches[i]->mRefPoint, &targets);
        }
      } else if (const WidgetWheelEvent* wheelEvent = aEvent.AsWheelEvent()) {
        waitForRefresh = PrepareForSetTargetAPZCNotification(aWidget, aGuid,
            rootFrame, wheelEvent->mRefPoint, &targets);
      } else if (const WidgetMouseEvent* mouseEvent = aEvent.AsMouseEvent()) {
        waitForRefresh = PrepareForSetTargetAPZCNotification(aWidget, aGuid,
            rootFrame, mouseEvent->mRefPoint, &targets);
      }
      // TODO: Do other types of events need to be handled?

      if (!targets.IsEmpty()) {
        if (waitForRefresh) {
          APZCCH_LOG("At least one target got a new displayport, need to wait for refresh\n");
          return MakeUnique<DisplayportSetListener>(aWidget, shell, aInputBlockId, std::move(targets));
        }
        APZCCH_LOG("Sending target APZCs for input block %" PRIu64 "\n", aInputBlockId);
        aWidget->SetConfirmedTargetAPZC(aInputBlockId, targets);
      }
    }
  }
  return nullptr;
}

void
APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(
        nsIWidget* aWidget,
        nsIDocument* aDocument,
        const WidgetTouchEvent& aEvent,
        uint64_t aInputBlockId,
        const SetAllowedTouchBehaviorCallback& aCallback)
{
  if (nsIPresShell* shell = aDocument->GetShell()) {
    if (nsIFrame* rootFrame = shell->GetRootFrame()) {
      rootFrame = UpdateRootFrameForTouchTargetDocument(rootFrame);

      nsTArray<TouchBehaviorFlags> flags;
      for (uint32_t i = 0; i < aEvent.mTouches.Length(); i++) {
        flags.AppendElement(
          TouchActionHelper::GetAllowedTouchBehavior(aWidget,
                rootFrame, aEvent.mTouches[i]->mRefPoint));
      }
      aCallback(aInputBlockId, std::move(flags));
    }
  }
}

void
APZCCallbackHelper::NotifyMozMouseScrollEvent(const ScrollableLayerGuid::ViewID& aScrollId, const nsString& aEvent)
{
  nsCOMPtr<nsIContent> targetContent = nsLayoutUtils::FindContentFor(aScrollId);
  if (!targetContent) {
    return;
  }
  nsCOMPtr<nsIDocument> ownerDoc = targetContent->OwnerDoc();
  if (!ownerDoc) {
    return;
  }

  nsContentUtils::DispatchTrustedEvent(
    ownerDoc, targetContent,
    aEvent,
    CanBubble::eYes,
    Cancelable::eYes);
}

void
APZCCallbackHelper::NotifyFlushComplete(nsIPresShell* aShell)
{
  MOZ_ASSERT(NS_IsMainThread());
  // In some cases, flushing the APZ state to the main thread doesn't actually
  // trigger a flush and repaint (this is an intentional optimization - the stuff
  // visible to the user is still correct). However, reftests update their
  // snapshot based on invalidation events that are emitted during paints,
  // so we ensure that we kick off a paint when an APZ flush is done. Note that
  // only chrome/testing code can trigger this behaviour.
  if (aShell && aShell->GetRootFrame()) {
    aShell->GetRootFrame()->SchedulePaint(nsIFrame::PAINT_DEFAULT, false);
  }

  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);
  observerService->NotifyObservers(nullptr, "apz-repaints-flushed", nullptr);
}

/* static */ bool
APZCCallbackHelper::IsScrollInProgress(nsIScrollableFrame* aFrame)
{
  return aFrame->IsProcessingAsyncScroll()
         || nsLayoutUtils::CanScrollOriginClobberApz(aFrame->LastScrollOrigin())
         || aFrame->LastSmoothScrollOrigin();
}

/* static */ void
APZCCallbackHelper::NotifyAsyncScrollbarDragRejected(const ScrollableLayerGuid::ViewID& aScrollId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (nsIScrollableFrame* scrollFrame = nsLayoutUtils::FindScrollableFrameFor(aScrollId)) {
    scrollFrame->AsyncScrollbarDragRejected();
  }
}

/* static */ void
APZCCallbackHelper::NotifyAsyncAutoscrollRejected(const ScrollableLayerGuid::ViewID& aScrollId)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);

  nsAutoString data;
  data.AppendInt(aScrollId);
  observerService->NotifyObservers(nullptr, "autoscroll-rejected-by-apz", data.get());
}

/* static */ void
APZCCallbackHelper::CancelAutoscroll(const ScrollableLayerGuid::ViewID& aScrollId)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);

  nsAutoString data;
  data.AppendInt(aScrollId);
  observerService->NotifyObservers(nullptr, "apz:cancel-autoscroll", data.get());
}

/* static */ void
APZCCallbackHelper::NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                                       LayoutDeviceCoord aSpanChange,
                                       Modifiers aModifiers,
                                       nsIWidget* aWidget)
{
  EventMessage msg;
  switch (aType) {
    case PinchGestureInput::PINCHGESTURE_START:
      msg = eMagnifyGestureStart;
      break;
    case PinchGestureInput::PINCHGESTURE_SCALE:
      msg = eMagnifyGestureUpdate;
      break;
    case PinchGestureInput::PINCHGESTURE_END:
      msg = eMagnifyGesture;
      break;
  }

  WidgetSimpleGestureEvent event(true, msg, aWidget);
  event.mDelta = aSpanChange;
  event.mModifiers = aModifiers;
  DispatchWidgetEvent(event);
}

} // namespace layers
} // namespace mozilla

