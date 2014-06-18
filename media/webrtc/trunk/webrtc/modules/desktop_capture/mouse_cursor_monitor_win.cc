/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/mouse_cursor.h"
#include "webrtc/modules/desktop_capture/win/cursor.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

class MouseCursorMonitorWin : public MouseCursorMonitor {
 public:
  explicit MouseCursorMonitorWin(HWND window);
  explicit MouseCursorMonitorWin(ScreenId screen);
  virtual ~MouseCursorMonitorWin();

  virtual void Init(Callback* callback, Mode mode) OVERRIDE;
  virtual void Capture() OVERRIDE;

 private:
  DesktopRect GetScreenRect();

  HWND window_;
  ScreenId screen_;

  Callback* callback_;
  Mode mode_;

  HDC desktop_dc_;

  HCURSOR last_cursor_;
};

MouseCursorMonitorWin::MouseCursorMonitorWin(HWND window)
    : window_(window),
      screen_(kInvalidScreenId),
      callback_(NULL),
      mode_(SHAPE_AND_POSITION),
      desktop_dc_(NULL),
      last_cursor_(NULL) {
}

MouseCursorMonitorWin::MouseCursorMonitorWin(ScreenId screen)
    : window_(NULL),
      screen_(screen),
      callback_(NULL),
      mode_(SHAPE_AND_POSITION),
      desktop_dc_(NULL),
      last_cursor_(NULL) {
  assert(screen >= kFullDesktopScreenId);
}

MouseCursorMonitorWin::~MouseCursorMonitorWin() {
  if (desktop_dc_)
    ReleaseDC(NULL, desktop_dc_);
}

void MouseCursorMonitorWin::Init(Callback* callback, Mode mode) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
  mode_ = mode;

  desktop_dc_ = GetDC(NULL);
}

void MouseCursorMonitorWin::Capture() {
  assert(callback_);

  CURSORINFO cursor_info;
  cursor_info.cbSize = sizeof(CURSORINFO);
  if (!GetCursorInfo(&cursor_info)) {
    LOG_F(LS_ERROR) << "Unable to get cursor info. Error = " << GetLastError();
    return;
  }

  if (last_cursor_ != cursor_info.hCursor) {
    last_cursor_ = cursor_info.hCursor;
    // Note that |cursor_info.hCursor| does not need to be freed.
    scoped_ptr<MouseCursor> cursor(
        CreateMouseCursorFromHCursor(desktop_dc_, cursor_info.hCursor));
    if (cursor.get())
      callback_->OnMouseCursor(cursor.release());
  }

  if (mode_ != SHAPE_AND_POSITION)
    return;

  DesktopVector position(cursor_info.ptScreenPos.x, cursor_info.ptScreenPos.y);
  bool inside = cursor_info.flags == CURSOR_SHOWING;

  if (window_) {
    RECT rect;
    if (!GetWindowRect(window_, &rect)) {
      position.set(0, 0);
      inside = false;
    } else {
      if (inside) {
        HWND windowUnderCursor = WindowFromPoint(cursor_info.ptScreenPos);
        inside = windowUnderCursor ?
            (window_ == GetAncestor(windowUnderCursor, GA_ROOT)) : false;
      }
      position = position.subtract(DesktopVector(rect.left, rect.top));
    }
  } else {
    assert(screen_ != kInvalidScreenId);
    DesktopRect rect = GetScreenRect();
    if (inside)
      inside = rect.Contains(position);
    position = position.subtract(rect.top_left());
  }

  callback_->OnMouseCursorPosition(inside ? INSIDE : OUTSIDE, position);
}

DesktopRect MouseCursorMonitorWin::GetScreenRect() {
  assert(screen_ != kInvalidScreenId);
  if (screen_ == kFullDesktopScreenId) {
    return DesktopRect::MakeXYWH(
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_CYVIRTUALSCREEN));
  }
  DISPLAY_DEVICE device;
  device.cb = sizeof(device);
  BOOL result = EnumDisplayDevices(NULL, screen_, &device, 0);
  if (!result)
    return DesktopRect();

  DEVMODE device_mode;
  device_mode.dmSize = sizeof(device_mode);
  device_mode.dmDriverExtra = 0;
  result = EnumDisplaySettingsEx(
      device.DeviceName, ENUM_CURRENT_SETTINGS, &device_mode, 0);
  if (!result)
    return DesktopRect();

  return DesktopRect::MakeXYWH(
      GetSystemMetrics(SM_XVIRTUALSCREEN) + device_mode.dmPosition.x,
      GetSystemMetrics(SM_YVIRTUALSCREEN) + device_mode.dmPosition.y,
      device_mode.dmPelsWidth,
      device_mode.dmPelsHeight);
}

MouseCursorMonitor* MouseCursorMonitor::CreateForWindow(
    const DesktopCaptureOptions& options, WindowId window) {
  return new MouseCursorMonitorWin(reinterpret_cast<HWND>(window));
}

MouseCursorMonitor* MouseCursorMonitor::CreateForScreen(
    const DesktopCaptureOptions& options,
    ScreenId screen) {
  return new MouseCursorMonitorWin(screen);
}

}  // namespace webrtc
