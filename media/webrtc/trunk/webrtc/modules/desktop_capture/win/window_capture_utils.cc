/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/window_capture_utils.h"

namespace webrtc {

bool
GetCroppedWindowRect(HWND window,
                     DesktopRect* cropped_rect,
                     DesktopRect* original_rect) {
  RECT rect;
  if (!GetWindowRect(window, &rect)) {
    return false;
  }
  WINDOWPLACEMENT window_placement;
  window_placement.length = sizeof(window_placement);
  if (!GetWindowPlacement(window, &window_placement)) {
    return false;
  }

  *original_rect = DesktopRect::MakeLTRB(
      rect.left, rect.top, rect.right, rect.bottom);

  if (window_placement.showCmd & SW_SHOWMAXIMIZED) {
    DesktopSize border = DesktopSize(GetSystemMetrics(SM_CXSIZEFRAME),
                                     GetSystemMetrics(SM_CYSIZEFRAME));
    *cropped_rect = DesktopRect::MakeLTRB(
        rect.left + border.width(),
        rect.top,
        rect.right - border.width(),
        rect.bottom - border.height());
  } else {
    *cropped_rect = *original_rect;
  }
  return true;
}

}  // namespace webrtc
