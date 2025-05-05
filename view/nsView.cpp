/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsView.h"

#include "mozilla/Attributes.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Likely.h"
#include "mozilla/Poison.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/widget/Screen.h"
#include "nsIWidget.h"
#include "nsViewManager.h"
#include "nsIFrame.h"
#include "nsPresArena.h"
#include "nsXULPopupManager.h"
#include "nsIScreen.h"
#include "nsIWidgetListener.h"
#include "nsContentUtils.h"  // for nsAutoScriptBlocker
#include "nsDocShell.h"
#include "nsLayoutUtils.h"
#include "mozilla/StartupTimeline.h"

using namespace mozilla;
using namespace mozilla::widget;

nsView::nsView(nsViewManager* aViewManager, ViewVisibility aVisibility)
    : mViewManager(aViewManager),
      mParent(nullptr),
      mNextSibling(nullptr),
      mFirstChild(nullptr),
      mFrame(nullptr),
      mVis(aVisibility),
      mPosX(0),
      mPosY(0),
      mWidgetIsTopLevel(false),
      mForcedRepaint(false),
      mNeedsWindowPropertiesSync(false) {
  MOZ_COUNT_CTOR(nsView);

  // Views should be transparent by default. Not being transparent is
  // a promise that the view will paint all its pixels opaquely. Views
  // should make this promise explicitly by calling
  // SetViewContentTransparency.
}

void nsView::DropMouseGrabbing() {
  if (mViewManager->GetPresShell()) {
    PresShell::ClearMouseCaptureOnView(this);
  }
}

nsView::~nsView() {
  MOZ_COUNT_DTOR(nsView);

  while (GetFirstChild()) {
    nsView* child = GetFirstChild();
    if (child->GetViewManager() == mViewManager) {
      child->Destroy();
    } else {
      // just unhook it. Someone else will want to destroy this.
      RemoveChild(child);
    }
  }

  if (mViewManager) {
    DropMouseGrabbing();

    nsView* rootView = mViewManager->GetRootView();

    if (rootView) {
      // Root views can have parents!
      if (mParent) {
        mViewManager->RemoveChild(this);
      }

      if (rootView == this) {
        // Inform the view manager that the root view has gone away...
        mViewManager->SetRootView(nullptr);
      }
    } else if (mParent) {
      mParent->RemoveChild(this);
    }

    mViewManager = nullptr;
  } else if (mParent) {
    mParent->RemoveChild(this);
  }

  if (mPreviousWindow) {
    mPreviousWindow->SetPreviouslyAttachedWidgetListener(nullptr);
  }

  // Destroy and release the widget
  DestroyWidget();

  MOZ_RELEASE_ASSERT(!mFrame);
}

class DestroyWidgetRunnable : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE

  explicit DestroyWidgetRunnable(nsIWidget* aWidget)
      : mozilla::Runnable("DestroyWidgetRunnable"), mWidget(aWidget) {}

 private:
  nsCOMPtr<nsIWidget> mWidget;
};

NS_IMETHODIMP DestroyWidgetRunnable::Run() {
  mWidget->Destroy();
  mWidget = nullptr;
  return NS_OK;
}

void nsView::DestroyWidget() {
  if (mWindow) {
    // If we are not attached to a base window, we're going to tear down our
    // widget here. However, if we're attached to somebody elses widget, we
    // want to leave the widget alone: don't reset the client data or call
    // Destroy. Just clear our event view ptr and free our reference to it.
    if (mWidgetIsTopLevel) {
      mWindow->SetAttachedWidgetListener(nullptr);
    } else {
      mWindow->SetWidgetListener(nullptr);

      nsCOMPtr<nsIRunnable> widgetDestroyer =
          new DestroyWidgetRunnable(mWindow);

      // Don't leak if we happen to arrive here after the main thread
      // has disappeared.
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      if (mainThread) {
        mainThread->Dispatch(widgetDestroyer.forget(), NS_DISPATCH_NORMAL);
      }
    }

    mWindow = nullptr;
  }
}

nsView* nsView::GetViewFor(const nsIWidget* aWidget) {
  MOZ_ASSERT(aWidget, "null widget ptr");

  nsIWidgetListener* listener = aWidget->GetWidgetListener();
  if (listener) {
    if (nsView* view = listener->GetView()) {
      return view;
    }
  }

  listener = aWidget->GetAttachedWidgetListener();
  return listener ? listener->GetView() : nullptr;
}

void nsView::Destroy() {
  this->~nsView();
  mozWritePoison(this, sizeof(*this));
  nsView::operator delete(this);
}

void nsView::SetPosition(nscoord aX, nscoord aY) {
  mDimBounds.MoveBy(aX - mPosX, aY - mPosY);
  mPosX = aX;
  mPosY = aY;

  NS_ASSERTION(GetParent() || (aX == 0 && aY == 0),
               "Don't try to move the root widget to something non-zero");

  ResetWidgetBounds(true, false);
}

void nsView::ResetWidgetBounds(bool aRecurse, bool aForceSync) {
  if (mWindow) {
    if (!aForceSync) {
      // Don't change widget geometry synchronously, since that can
      // cause synchronous painting.
      mViewManager->PostPendingUpdate();
    } else {
      DoResetWidgetBounds(false, true);
    }
    return;
  }

  if (aRecurse) {
    // reposition any widgets under this view
    for (nsView* v = GetFirstChild(); v; v = v->GetNextSibling()) {
      v->ResetWidgetBounds(true, aForceSync);
    }
  }
}

bool nsView::IsEffectivelyVisible() {
  for (nsView* v = this; v; v = v->mParent) {
    if (v->GetVisibility() == ViewVisibility::Hide) return false;
  }
  return true;
}

LayoutDeviceIntRect nsView::CalcWidgetBounds(WindowType aType,
                                             TransparencyMode aTransparency) {
  int32_t p2a = mViewManager->AppUnitsPerDevPixel();

  nsRect viewBounds(mDimBounds);

  nsView* parent = GetParent();
  nsIWidget* parentWidget = nullptr;
  if (parent) {
    nsPoint offset;
    parentWidget = parent->GetNearestWidget(&offset, p2a);
    // make viewBounds be relative to the parent widget, in appunits
    viewBounds += offset;

    if (parentWidget && aType == WindowType::Popup && IsEffectivelyVisible()) {
      // put offset into screen coordinates. (based on client area origin)
      LayoutDeviceIntPoint screenPoint = parentWidget->WidgetToScreenOffset();
      viewBounds += nsPoint(NSIntPixelsToAppUnits(screenPoint.x, p2a),
                            NSIntPixelsToAppUnits(screenPoint.y, p2a));
    }
  }

  // Compute widget bounds in device pixels
  const LayoutDeviceIntRect newBounds = [&] {
    // TODO(emilio): We should probably use outside pixels for transparent
    // windows (not just popups) as well.
    if (aType != WindowType::Popup) {
      return LayoutDeviceIntRect::FromUnknownRect(
          viewBounds.ToNearestPixels(p2a));
    }
    // We use outside pixels for transparent windows if possible, so that we
    // don't truncate the contents. For opaque popups, we use nearest pixels
    // which prevents having pixels not drawn by the frame.
    const bool opaque = aTransparency == TransparencyMode::Opaque;
    const auto idealBounds = LayoutDeviceIntRect::FromUnknownRect(
        opaque ? viewBounds.ToNearestPixels(p2a)
               : viewBounds.ToOutsidePixels(p2a));

    nsIWidget* widget = parentWidget ? parentWidget : mWindow.get();
    if (!widget) {
      return idealBounds;
    }
    const int32_t round = widget->RoundsWidgetCoordinatesTo();
    return nsIWidget::MaybeRoundToDisplayPixels(idealBounds, aTransparency,
                                                round);
  }();

  // Compute where the top-left of our widget ended up relative to the parent
  // widget, in appunits.
  nsPoint roundedOffset(NSIntPixelsToAppUnits(newBounds.X(), p2a),
                        NSIntPixelsToAppUnits(newBounds.Y(), p2a));

  // mViewToWidgetOffset is added to coordinates relative to the view origin
  // to get coordinates relative to the widget.
  // The view origin, relative to the parent widget, is at
  // (mPosX,mPosY) - mDimBounds.TopLeft() + viewBounds.TopLeft().
  // Our widget, relative to the parent widget, is roundedOffset.
  mViewToWidgetOffset = nsPoint(mPosX, mPosY) - mDimBounds.TopLeft() +
                        viewBounds.TopLeft() - roundedOffset;

  return newBounds;
}

LayoutDeviceIntRect nsView::RecalcWidgetBounds() {
  MOZ_ASSERT(mWindow);
  return CalcWidgetBounds(mWindow->GetWindowType(),
                          mWindow->GetTransparencyMode());
}

void nsView::DoResetWidgetBounds(bool aMoveOnly, bool aInvalidateChangedSize) {
  // The geometry of a root view's widget is controlled externally,
  // NOT by sizing or positioning the view
  if (mViewManager->GetRootView() == this) {
    return;
  }

  MOZ_ASSERT(mWindow, "Why was this called??");

  // Hold this ref to make sure it stays alive.
  nsCOMPtr<nsIWidget> widget = mWindow;

  // Stash a copy of these and use them so we can handle this being deleted (say
  // from sync painting/flushing from Show/Move/Resize on the widget).
  LayoutDeviceIntRect newBounds;

  WindowType type = widget->GetWindowType();

  LayoutDeviceIntRect curBounds = widget->GetClientBounds();
  bool invisiblePopup = type == WindowType::Popup &&
                        ((curBounds.IsEmpty() && mDimBounds.IsEmpty()) ||
                         mVis == ViewVisibility::Hide);

  if (invisiblePopup) {
    // We're going to hit the early exit below, avoid calling CalcWidgetBounds.
  } else {
    newBounds = CalcWidgetBounds(type, widget->GetTransparencyMode());
    invisiblePopup = newBounds.IsEmpty();
  }

  bool curVisibility = widget->IsVisible();
  bool newVisibility = !invisiblePopup && IsEffectivelyVisible();
  if (curVisibility && !newVisibility) {
    widget->Show(false);
  }

  if (invisiblePopup) {
    // Don't manipulate empty or hidden popup widgets. For example there's no
    // point moving hidden comboboxes around, or doing X server roundtrips
    // to compute their true screen position. This could mean that
    // WidgetToScreen operations on these widgets don't return up-to-date
    // values, but popup positions aren't reliable anyway because of correction
    // to be on or off-screen.
    return;
  }

  // Apply the widget size constraints to newBounds.
  widget->ConstrainSize(&newBounds.width, &newBounds.height);

  bool changedPos = curBounds.TopLeft() != newBounds.TopLeft();
  bool changedSize = curBounds.Size() != newBounds.Size();

  // Child views are never attached to top level widgets, this is safe.

  // Coordinates are converted to desktop pixels for window Move/Resize APIs,
  // because of the potential for device-pixel coordinate spaces for mixed
  // hidpi/lodpi screens to overlap each other and result in bad placement
  // (bug 814434).

  DesktopToLayoutDeviceScale scale = widget->GetDesktopToDeviceScaleByScreen();

  DesktopRect deskRect = newBounds / scale;
  if (changedPos) {
    if (changedSize && !aMoveOnly) {
      widget->ResizeClient(deskRect, aInvalidateChangedSize);
    } else {
      widget->MoveClient(deskRect.TopLeft());
    }
  } else {
    if (changedSize && !aMoveOnly) {
      widget->ResizeClient(deskRect.Size(), aInvalidateChangedSize);
    }  // else do nothing!
  }

  if (!curVisibility && newVisibility) {
    widget->Show(true);
  }
}

void nsView::SetDimensions(const nsRect& aRect, bool aPaint,
                           bool aResizeWidget) {
  nsRect dims = aRect;
  dims.MoveBy(mPosX, mPosY);

  // Don't use nsRect's operator== here, since it returns true when
  // both rects are empty even if they have different widths and we
  // have cases where that sort of thing matters to us.
  if (mDimBounds.TopLeft() == dims.TopLeft() &&
      mDimBounds.Size() == dims.Size()) {
    return;
  }

  mDimBounds = dims;

  if (aResizeWidget) {
    ResetWidgetBounds(false, false);
  }
}

void nsView::NotifyEffectiveVisibilityChanged(bool aEffectivelyVisible) {
  if (!aEffectivelyVisible) {
    DropMouseGrabbing();
  }

  SetForcedRepaint(true);

  if (mWindow) {
    ResetWidgetBounds(false, false);
  }

  for (nsView* child = mFirstChild; child; child = child->mNextSibling) {
    if (child->mVis == ViewVisibility::Hide) {
      // It was effectively hidden and still is
      continue;
    }
    // Our child is visible if we are
    child->NotifyEffectiveVisibilityChanged(aEffectivelyVisible);
  }
}

void nsView::SetVisibility(ViewVisibility aVisibility) {
  mVis = aVisibility;
  NotifyEffectiveVisibilityChanged(IsEffectivelyVisible());
}

void nsView::InvalidateHierarchy() {
  if (mViewManager->GetRootView() == this) mViewManager->InvalidateHierarchy();

  for (nsView* child = mFirstChild; child; child = child->GetNextSibling())
    child->InvalidateHierarchy();
}

void nsView::InsertChild(nsView* aChild, nsView* aSibling) {
  MOZ_ASSERT(nullptr != aChild, "null ptr");

  if (nullptr != aChild) {
    if (nullptr != aSibling) {
#ifdef DEBUG
      NS_ASSERTION(aSibling->GetParent() == this,
                   "tried to insert view with invalid sibling");
#endif
      // insert after sibling
      aChild->SetNextSibling(aSibling->GetNextSibling());
      aSibling->SetNextSibling(aChild);
    } else {
      aChild->SetNextSibling(mFirstChild);
      mFirstChild = aChild;
    }
    aChild->SetParent(this);

    // If we just inserted a root view, then update the RootViewManager
    // on all view managers in the new subtree.

    nsViewManager* vm = aChild->GetViewManager();
    if (vm->GetRootView() == aChild) {
      aChild->InvalidateHierarchy();
    }
  }
}

void nsView::RemoveChild(nsView* child) {
  MOZ_ASSERT(nullptr != child, "null ptr");

  if (nullptr != child) {
    nsView* prevKid = nullptr;
    nsView* kid = mFirstChild;
    DebugOnly<bool> found = false;
    while (nullptr != kid) {
      if (kid == child) {
        if (nullptr != prevKid) {
          prevKid->SetNextSibling(kid->GetNextSibling());
        } else {
          mFirstChild = kid->GetNextSibling();
        }
        child->SetParent(nullptr);
        found = true;
        break;
      }
      prevKid = kid;
      kid = kid->GetNextSibling();
    }
    NS_ASSERTION(found, "tried to remove non child");

    // If we just removed a root view, then update the RootViewManager
    // on all view managers in the removed subtree.

    nsViewManager* vm = child->GetViewManager();
    if (vm->GetRootView() == child) {
      child->InvalidateHierarchy();
    }
  }
}

struct DefaultWidgetInitData : public widget::InitData {
  DefaultWidgetInitData() : widget::InitData() {
    mWindowType = WindowType::Child;
    mClipChildren = true;
    mClipSiblings = true;
  }
};

nsresult nsView::CreateWidget(nsIWidget* aParent, bool aEnableDragDrop,
                              bool aResetVisibility) {
  AssertNoWindow();

  DefaultWidgetInitData initData;
  LayoutDeviceIntRect trect =
      CalcWidgetBounds(initData.mWindowType, initData.mTransparencyMode);

  if (!aParent && GetParent()) {
    aParent = GetParent()->GetNearestWidget(nullptr);
  }
  if (!aParent) {
    NS_ERROR("nsView::CreateWidget without suitable parent widget??");
    return NS_ERROR_FAILURE;
  }

  mWindow = aParent->CreateChild(trect, initData);
  if (!mWindow) {
    return NS_ERROR_FAILURE;
  }

  InitializeWindow(aEnableDragDrop, aResetVisibility);

  return NS_OK;
}

nsresult nsView::CreateWidgetForPopup(widget::InitData* aWidgetInitData,
                                      nsIWidget* aParent) {
  AssertNoWindow();
  MOZ_ASSERT(aWidgetInitData, "Widget init data required");
  MOZ_ASSERT(aWidgetInitData->mWindowType == WindowType::Popup,
             "Use one of the other CreateWidget methods");
  MOZ_ASSERT(aParent);

  LayoutDeviceIntRect trect = CalcWidgetBounds(
      aWidgetInitData->mWindowType, aWidgetInitData->mTransparencyMode);
  mWindow = aParent->CreateChild(trect, *aWidgetInitData);
  if (!mWindow) {
    return NS_ERROR_FAILURE;
  }
  InitializeWindow(/* aEnableDragDrop = */ true, /* aResetVisibility = */ true);
  return NS_OK;
}

void nsView::InitializeWindow(bool aEnableDragDrop, bool aResetVisibility) {
  MOZ_ASSERT(mWindow, "Must have a window to initialize");

  mWindow->SetWidgetListener(this);

  if (aEnableDragDrop) {
    mWindow->EnableDragDrop(true);
  }

  // make sure visibility state is accurate

  if (aResetVisibility) {
    SetVisibility(GetVisibility());
  }
}

void nsView::SetNeedsWindowPropertiesSync() {
  mNeedsWindowPropertiesSync = true;
  if (mViewManager) {
    mViewManager->PostPendingUpdate();
  }
}

// Attach to a top level widget and start receiving mirrored events.
nsresult nsView::AttachToTopLevelWidget(nsIWidget* aWidget) {
  MOZ_ASSERT(aWidget, "null widget ptr");

  /// XXXjimm This is a temporary workaround to an issue w/document
  // viewer (bug 513162).
  nsIWidgetListener* listener = aWidget->GetAttachedWidgetListener();
  if (listener) {
    nsView* oldView = listener->GetView();
    if (oldView) {
      oldView->DetachFromTopLevelWidget();
    }
  }

  // Note, the previous device context will be released. Detaching
  // will not restore the old one.
  aWidget->AttachViewToTopLevel(!nsIWidget::UsePuppetWidgets());

  mWindow = aWidget;

  mWindow->SetAttachedWidgetListener(this);
  if (mWindow->GetWindowType() != WindowType::Invisible) {
    nsresult rv = mWindow->AsyncEnableDragDrop(true);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mWidgetIsTopLevel = true;

  // Refresh the view bounds
  RecalcWidgetBounds();
  return NS_OK;
}

// Detach this view from an attached widget.
nsresult nsView::DetachFromTopLevelWidget() {
  MOZ_ASSERT(mWidgetIsTopLevel, "Not attached currently!");
  MOZ_ASSERT(mWindow, "null mWindow for DetachFromTopLevelWidget!");

  mWindow->SetAttachedWidgetListener(nullptr);
  nsIWidgetListener* listener = mWindow->GetPreviouslyAttachedWidgetListener();

  if (listener && listener->GetView()) {
    // Ensure the listener doesn't think it's being used anymore
    listener->GetView()->SetPreviousWidget(nullptr);
  }

  // If the new view's frame is paint suppressed then the window
  // will want to use us instead until that's done
  mWindow->SetPreviouslyAttachedWidgetListener(this);

  mPreviousWindow = mWindow;
  mWindow = nullptr;

  mWidgetIsTopLevel = false;

  return NS_OK;
}

void nsView::AssertNoWindow() {
  // XXX: it would be nice to make this a strong assert
  if (MOZ_UNLIKELY(mWindow)) {
    NS_ERROR("We already have a window for this view? BAD");
    mWindow->SetWidgetListener(nullptr);
    mWindow->Destroy();
    mWindow = nullptr;
  }
}

//
// internal window creation functions
//
void nsView::AttachWidgetEventHandler(nsIWidget* aWidget) {
#ifdef DEBUG
  NS_ASSERTION(!aWidget->GetWidgetListener(), "Already have a widget listener");
#endif

  aWidget->SetWidgetListener(this);
}

void nsView::DetachWidgetEventHandler(nsIWidget* aWidget) {
  NS_ASSERTION(!aWidget->GetWidgetListener() ||
                   aWidget->GetWidgetListener()->GetView() == this,
               "Wrong view");
  aWidget->SetWidgetListener(nullptr);
}

#ifdef DEBUG
void nsView::List(FILE* out, int32_t aIndent) const {
  int32_t i;
  for (i = aIndent; --i >= 0;) fputs("  ", out);
  fprintf(out, "%p ", (void*)this);
  if (nullptr != mWindow) {
    nscoord p2a = mViewManager->AppUnitsPerDevPixel();
    LayoutDeviceIntRect rect = mWindow->GetClientBounds();
    nsRect windowBounds = LayoutDeviceIntRect::ToAppUnits(rect, p2a);
    rect = mWindow->GetBounds();
    nsRect nonclientBounds = LayoutDeviceIntRect::ToAppUnits(rect, p2a);
    nsrefcnt widgetRefCnt = mWindow.get()->AddRef() - 1;
    mWindow.get()->Release();
    fprintf(out, "(widget=%p[%" PRIuPTR "] pos={%d,%d,%d,%d}) ", (void*)mWindow,
            widgetRefCnt, nonclientBounds.X(), nonclientBounds.Y(),
            windowBounds.Width(), windowBounds.Height());
  }
  nsRect brect = GetBounds();
  fprintf(out, "{%d,%d,%d,%d} @ %d,%d", brect.X(), brect.Y(), brect.Width(),
          brect.Height(), mPosX, mPosY);
  fprintf(out, " vis=%d frame=%p <\n", int(mVis), mFrame);
  for (nsView* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
    NS_ASSERTION(kid->GetParent() == this, "incorrect parent");
    kid->List(out, aIndent + 1);
  }
  for (i = aIndent; --i >= 0;) fputs("  ", out);
  fputs(">\n", out);
}
#endif  // DEBUG

nsPoint nsView::GetOffsetTo(const nsView* aOther) const {
  return GetOffsetTo(aOther, GetViewManager()->AppUnitsPerDevPixel());
}

nsPoint nsView::GetOffsetTo(const nsView* aOther, const int32_t aAPD) const {
  MOZ_ASSERT(GetParent() || !aOther || aOther->GetParent() || this == aOther,
             "caller of (outer) GetOffsetTo must not pass unrelated views");
  // We accumulate the final result in offset
  nsPoint offset(0, 0);
  // The offset currently accumulated at the current APD
  nsPoint docOffset(0, 0);
  const nsView* v = this;
  nsViewManager* currVM = v->GetViewManager();
  int32_t currAPD = currVM->AppUnitsPerDevPixel();
  const nsView* root = nullptr;
  for (; v != aOther && v; root = v, v = v->GetParent()) {
    nsViewManager* newVM = v->GetViewManager();
    if (newVM != currVM) {
      int32_t newAPD = newVM->AppUnitsPerDevPixel();
      if (newAPD != currAPD) {
        offset += docOffset.ScaleToOtherAppUnits(currAPD, aAPD);
        docOffset.x = docOffset.y = 0;
        currAPD = newAPD;
      }
      currVM = newVM;
    }
    docOffset += v->GetPosition();
  }
  offset += docOffset.ScaleToOtherAppUnits(currAPD, aAPD);

  if (v != aOther) {
    // Looks like aOther wasn't an ancestor of |this|.  So now we have
    // the root-VM-relative position of |this| in |offset|.  Get the
    // root-VM-relative position of aOther and subtract it.
    nsPoint negOffset = aOther->GetOffsetTo(root, aAPD);
    offset -= negOffset;
  }

  return offset;
}

nsPoint nsView::GetOffsetToWidget(nsIWidget* aWidget) const {
  nsPoint pt;
  // Get the view for widget
  nsView* widgetView = GetViewFor(aWidget);
  if (!widgetView) {
    return pt;
  }

  // Get the offset to the widget view in the widget view's APD
  // We get the offset in the widget view's APD first and then convert to our
  // APD afterwards so that we can include the widget view's ViewToWidgetOffset
  // in the sum in its native APD, and then convert the whole thing to our APD
  // so that we don't have to convert the APD of the relatively small
  // ViewToWidgetOffset by itself with a potentially large relative rounding
  // error.
  pt = -widgetView->GetOffsetTo(this);
  // Add in the offset to the widget.
  pt += widgetView->ViewToWidgetOffset();

  // Convert to our appunits.
  int32_t widgetAPD = widgetView->GetViewManager()->AppUnitsPerDevPixel();
  int32_t ourAPD = GetViewManager()->AppUnitsPerDevPixel();
  pt = pt.ScaleToOtherAppUnits(widgetAPD, ourAPD);
  return pt;
}

nsIWidget* nsView::GetNearestWidget(nsPoint* aOffset) const {
  return GetNearestWidget(aOffset, GetViewManager()->AppUnitsPerDevPixel());
}

nsIWidget* nsView::GetNearestWidget(nsPoint* aOffset,
                                    const int32_t aAPD) const {
  // aOffset is based on the view's position, which ignores any chrome on
  // attached parent widgets.

  // We accumulate the final result in pt
  nsPoint pt(0, 0);
  // The offset currently accumulated at the current APD
  nsPoint docPt(0, 0);
  const nsView* v = this;
  nsViewManager* currVM = v->GetViewManager();
  int32_t currAPD = currVM->AppUnitsPerDevPixel();
  for (; v && !v->HasWidget(); v = v->GetParent()) {
    nsViewManager* newVM = v->GetViewManager();
    if (newVM != currVM) {
      int32_t newAPD = newVM->AppUnitsPerDevPixel();
      if (newAPD != currAPD) {
        pt += docPt.ScaleToOtherAppUnits(currAPD, aAPD);
        docPt.x = docPt.y = 0;
        currAPD = newAPD;
      }
      currVM = newVM;
    }
    docPt += v->GetPosition();
  }
  if (!v) {
    if (aOffset) {
      pt += docPt.ScaleToOtherAppUnits(currAPD, aAPD);
      *aOffset = pt;
    }
    return nullptr;
  }

  // pt is now the offset from v's origin to this view's origin.
  // We add the ViewToWidgetOffset to get the offset to the widget.
  if (aOffset) {
    docPt += v->ViewToWidgetOffset();
    pt += docPt.ScaleToOtherAppUnits(currAPD, aAPD);
    *aOffset = pt;
  }
  return v->GetWidget();
}

bool nsView::IsRoot() const {
  NS_ASSERTION(mViewManager != nullptr,
               " View manager is null in nsView::IsRoot()");
  return mViewManager->GetRootView() == this;
}

static bool IsPopupWidget(nsIWidget* aWidget) {
  return aWidget->GetWindowType() == WindowType::Popup;
}

PresShell* nsView::GetPresShell() { return GetViewManager()->GetPresShell(); }

bool nsView::WindowMoved(nsIWidget* aWidget, int32_t x, int32_t y,
                         ByMoveToRect aByMoveToRect) {
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && IsPopupWidget(aWidget)) {
    pm->PopupMoved(mFrame, LayoutDeviceIntPoint(x, y),
                   aByMoveToRect == ByMoveToRect::Yes);
    return true;
  }

  return false;
}

bool nsView::WindowResized(nsIWidget* aWidget, int32_t aWidth,
                           int32_t aHeight) {
  // The root view may not be set if this is the resize associated with
  // window creation
  SetForcedRepaint(true);
  if (this == mViewManager->GetRootView()) {
    RefPtr<nsDeviceContext> devContext = mViewManager->GetDeviceContext();
    // ensure DPI is up-to-date, in case of window being opened and sized
    // on a non-default-dpi display (bug 829963)
    devContext->CheckDPIChange();
    int32_t p2a = devContext->AppUnitsPerDevPixel();
    if (auto* frame = GetFrame()) {
      // Usually the resize would deal with this, but there are some cases (like
      // web-extension popups) where frames might already be correctly sized etc
      // due to a call to e.g. nsDocumentViewer::GetContentSize or so.
      frame->InvalidateFrame();
    }

    mViewManager->SetWindowDimensions(NSIntPixelsToAppUnits(aWidth, p2a),
                                      NSIntPixelsToAppUnits(aHeight, p2a));

    if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
      PresShell* presShell = mViewManager->GetPresShell();
      if (presShell && presShell->GetDocument()) {
        pm->AdjustPopupsOnWindowChange(presShell);
      }
    }

    return true;
  }
  if (IsPopupWidget(aWidget)) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      pm->PopupResized(mFrame, LayoutDeviceIntSize(aWidth, aHeight));
      return true;
    }
  }

  return false;
}

#ifdef MOZ_WIDGET_ANDROID
void nsView::DynamicToolbarMaxHeightChanged(ScreenIntCoord aHeight) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Should be only called for the browser parent process");
  MOZ_ASSERT(this == mViewManager->GetRootView(),
             "Should be called for the root view");

  CallOnAllRemoteChildren(
      [aHeight](dom::BrowserParent* aBrowserParent) -> CallState {
        aBrowserParent->DynamicToolbarMaxHeightChanged(aHeight);
        return CallState::Continue;
      });
}

void nsView::DynamicToolbarOffsetChanged(ScreenIntCoord aOffset) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Should be only called for the browser parent process");
  MOZ_ASSERT(this == mViewManager->GetRootView(),
             "Should be called for the root view");
  CallOnAllRemoteChildren(
      [aOffset](dom::BrowserParent* aBrowserParent) -> CallState {
        // Skip background tabs.
        if (!aBrowserParent->GetDocShellIsActive()) {
          return CallState::Continue;
        }

        aBrowserParent->DynamicToolbarOffsetChanged(aOffset);
        return CallState::Stop;
      });
}

void nsView::KeyboardHeightChanged(ScreenIntCoord aHeight) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Should be only called for the browser parent process");
  MOZ_ASSERT(this == mViewManager->GetRootView(),
             "Should be called for the root view");
  CallOnAllRemoteChildren(
      [aHeight](dom::BrowserParent* aBrowserParent) -> CallState {
        // Skip background tabs.
        if (!aBrowserParent->GetDocShellIsActive()) {
          return CallState::Continue;
        }

        aBrowserParent->KeyboardHeightChanged(aHeight);
        return CallState::Stop;
      });
}

void nsView::AndroidPipModeChanged(bool aPipMode) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Should be only called for the browser parent process");
  MOZ_ASSERT(this == mViewManager->GetRootView(),
             "Should be called for the root view");
  CallOnAllRemoteChildren(
      [aPipMode](dom::BrowserParent* aBrowserParent) -> CallState {
        aBrowserParent->AndroidPipModeChanged(aPipMode);
        return CallState::Continue;
      });
}
#endif

bool nsView::RequestWindowClose(nsIWidget* aWidget) {
  if (mFrame && IsPopupWidget(aWidget) && mFrame->IsMenuPopupFrame()) {
    if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
      pm->HidePopup(mFrame->GetContent()->AsElement(),
                    {HidePopupOption::DeselectMenu});
      return true;
    }
  }

  return false;
}

void nsView::WillPaintWindow(nsIWidget* aWidget) {
  RefPtr<nsViewManager> vm = mViewManager;
  vm->WillPaintWindow(aWidget);
}

bool nsView::PaintWindow(nsIWidget* aWidget, LayoutDeviceIntRegion aRegion) {
  NS_ASSERTION(this == nsView::GetViewFor(aWidget), "wrong view for widget?");

  RefPtr<nsViewManager> vm = mViewManager;
  bool result = vm->PaintWindow(aWidget, aRegion);
  return result;
}

void nsView::DidPaintWindow() {
  RefPtr<nsViewManager> vm = mViewManager;
  vm->DidPaintWindow();
}

void nsView::DidCompositeWindow(mozilla::layers::TransactionId aTransactionId,
                                const TimeStamp& aCompositeStart,
                                const TimeStamp& aCompositeEnd) {
  PresShell* presShell = mViewManager->GetPresShell();
  if (!presShell) {
    return;
  }

  nsAutoScriptBlocker scriptBlocker;

  nsPresContext* context = presShell->GetPresContext();
  nsRootPresContext* rootContext = context->GetRootPresContext();
  if (rootContext) {
    rootContext->NotifyDidPaintForSubtree(aTransactionId, aCompositeEnd);
  }

  mozilla::StartupTimeline::RecordOnce(mozilla::StartupTimeline::FIRST_PAINT2,
                                       aCompositeEnd);

  // If the two timestamps are identical, this was likely a fake composite
  // event which wouldn't be terribly useful to display.
  if (aCompositeStart == aCompositeEnd) {
    return;
  }
}

void nsView::RequestRepaint() {
  if (PresShell* presShell = mViewManager->GetPresShell()) {
    presShell->ScheduleViewManagerFlush();
  }
}

bool nsView::ShouldNotBeVisible() {
  if (mFrame && mFrame->IsMenuPopupFrame()) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    return !pm || !pm->IsPopupOpen(mFrame->GetContent()->AsElement());
  }

  return false;
}

nsEventStatus nsView::HandleEvent(WidgetGUIEvent* aEvent,
                                  bool aUseAttachedEvents) {
  MOZ_ASSERT(nullptr != aEvent->mWidget, "null widget ptr");

  nsEventStatus result = nsEventStatus_eIgnore;
  nsView* view;
  if (aUseAttachedEvents) {
    nsIWidgetListener* listener = aEvent->mWidget->GetAttachedWidgetListener();
    view = listener ? listener->GetView() : nullptr;
  } else {
    view = GetViewFor(aEvent->mWidget);
  }

  if (view) {
    RefPtr<nsViewManager> vm = view->GetViewManager();
    vm->DispatchEvent(aEvent, view, &result);
  }

  return result;
}

void nsView::SafeAreaInsetsChanged(
    const LayoutDeviceIntMargin& aSafeAreaInsets) {
  if (!IsRoot()) {
    return;
  }

  PresShell* presShell = mViewManager->GetPresShell();
  if (!presShell) {
    return;
  }

  LayoutDeviceIntMargin windowSafeAreaInsets;
  const LayoutDeviceIntRect windowRect = mWindow->GetScreenBounds();
  if (nsCOMPtr<nsIScreen> screen = mWindow->GetWidgetScreen()) {
    windowSafeAreaInsets = nsContentUtils::GetWindowSafeAreaInsets(
        screen, aSafeAreaInsets, windowRect);
  }

  presShell->GetPresContext()->SetSafeAreaInsets(windowSafeAreaInsets);

  // https://github.com/w3c/csswg-drafts/issues/4670
  // Actually we don't set this value on sub document. This behaviour is
  // same as Blink.
  CallOnAllRemoteChildren([windowSafeAreaInsets](
                              dom::BrowserParent* aBrowserParent) -> CallState {
    Unused << aBrowserParent->SendSafeAreaInsetsChanged(windowSafeAreaInsets);
    return CallState::Continue;
  });
}

bool nsView::IsPrimaryFramePaintSuppressed() {
  return StaticPrefs::layout_show_previous_page() && mFrame &&
         mFrame->PresShell()->IsPaintingSuppressed();
}

void nsView::CallOnAllRemoteChildren(
    const std::function<CallState(dom::BrowserParent*)>& aCallback) {
  PresShell* presShell = mViewManager->GetPresShell();
  if (!presShell) {
    return;
  }

  dom::Document* document = presShell->GetDocument();
  if (!document) {
    return;
  }

  nsPIDOMWindowOuter* window = document->GetWindow();
  if (!window) {
    return;
  }

  nsContentUtils::CallOnAllRemoteChildren(window, aCallback);
}
