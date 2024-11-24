/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIWidgetListener_h__
#define nsIWidgetListener_h__

#include <stdint.h>

#include "mozilla/EventForwards.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/TimeStamp.h"

#include "nsRegionFwd.h"
#include "Units.h"

class nsView;
class nsIWidget;
class nsIAppWindow;

namespace mozilla {
class PresShell;
}  // namespace mozilla

/**
 * sizemode is an adjunct to widget size
 */
enum nsSizeMode {
  nsSizeMode_Normal = 0,
  nsSizeMode_Minimized,
  nsSizeMode_Maximized,
  nsSizeMode_Fullscreen,
  nsSizeMode_Invalid
};

/**
 * different types of (top-level) window z-level positioning
 */
enum nsWindowZ {
  nsWindowZTop = 0,  // on top
  nsWindowZBottom,   // on bottom
  nsWindowZRelative  // just below some specified widget
};

class nsIWidgetListener {
 public:
  /**
   * If this listener is for an nsIAppWindow, return it. If this is null, then
   * this is likely a listener for a view, which can be determined using
   * GetView. If both methods return null, this will be an nsWebBrowser.
   */
  virtual nsIAppWindow* GetAppWindow() { return nullptr; }

  /** If this listener is for an nsView, return it. */
  virtual nsView* GetView() { return nullptr; }

  /** Return the presshell for this widget listener. */
  virtual mozilla::PresShell* GetPresShell() { return nullptr; }

  /**
   * Called when a window is moved to location (x, y). Returns true if the
   * notification was handled. Coordinates are outer window screen coordinates.
   */
  enum class ByMoveToRect : bool { No, Yes };
  virtual bool WindowMoved(nsIWidget* aWidget, int32_t aX, int32_t aY,
                           ByMoveToRect) {
    return false;
  }

  /**
   * Called when a window is resized to (width, height). Returns true if the
   * notification was handled. Coordinates are outer window screen coordinates.
   */
  virtual bool WindowResized(nsIWidget* aWidget, int32_t aWidth,
                             int32_t aHeight) {
    return false;
  }

  /**
   * Called when the size mode (minimized, maximized, fullscreen) is changed.
   */
  virtual void SizeModeChanged(nsSizeMode aSizeMode) {}

#if defined(MOZ_WIDGET_ANDROID)
  virtual void DynamicToolbarMaxHeightChanged(mozilla::ScreenIntCoord aHeight) {
  }
  virtual void DynamicToolbarOffsetChanged(mozilla::ScreenIntCoord aOffset) {}
  /** Called when the software keyboard appears/disappears. */
  virtual void KeyboardHeightChanged(mozilla::ScreenIntCoord aHeight) {}
#endif

  /** Called when the macOS titlebar is shown while in fullscreen. */
  virtual void MacFullscreenMenubarOverlapChanged(
      mozilla::DesktopCoord aOverlapAmount) {}

  /**
   * Called when the occlusion state is changed.
   */
  virtual void OcclusionStateChanged(bool aIsFullyOccluded) {}

  /** Called when the window is activated and focused. */
  virtual void WindowActivated() {}

  /** Called when the window is deactivated and no longer focused. */
  virtual void WindowDeactivated() {}

  /**
   * Called when the show/hide toolbar button on the Mac titlebar is pressed.
   */
  virtual void OSToolbarButtonPressed() {}

  /**
   * Called when a request is made to close the window. Returns true if the
   * notification was handled. Returns true if the notification was handled.
   */
  virtual bool RequestWindowClose(nsIWidget* aWidget) { return false; }

  /*
   * Indicate that a paint is about to occur on this window. This is called
   * at a time when it's OK to change the geometry of this widget or of
   * other widgets. Must be called before every call to PaintWindow.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual void WillPaintWindow(nsIWidget* aWidget) {}

  /**
   * Paint the specified region of the window. Returns true if the
   * notification was handled.
   * This is called at a time when it is not OK to change the geometry of
   * this widget or of other widgets.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual bool PaintWindow(nsIWidget* aWidget,
                           mozilla::LayoutDeviceIntRegion aRegion) {
    return false;
  }
  /**
   * Indicates that a paint occurred.
   * This is called at a time when it is OK to change the geometry of
   * this widget or of other widgets.
   * Must be called after every call to PaintWindow.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual void DidPaintWindow() {}

  virtual void DidCompositeWindow(mozilla::layers::TransactionId aTransactionId,
                                  const mozilla::TimeStamp& aCompositeStart,
                                  const mozilla::TimeStamp& aCompositeEnd) {}

  /** Request that layout schedules a repaint on the next refresh driver tick.
   */
  virtual void RequestRepaint() {}

  /**
   * Returns true if this is a popup that should not be visible. If this
   * is a popup that is visible, not a popup or this state is unknown,
   * returns false.
   */
  virtual bool ShouldNotBeVisible() { return false; }

  /** Handle an event. */
  virtual nsEventStatus HandleEvent(mozilla::WidgetGUIEvent* aEvent,
                                    bool aUseAttachedEvents) {
    return nsEventStatus_eIgnore;
  }

  /** Called when safe area insets are changed. */
  virtual void SafeAreaInsetsChanged(
      const mozilla::LayoutDeviceIntMargin& aSafeAreaInsets) {}
};

#endif
