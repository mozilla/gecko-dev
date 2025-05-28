/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeWin.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

namespace mozilla::widget {

nsNativeThemeWin::nsNativeThemeWin() : Theme(ScrollbarStyle()) {}

}  // namespace mozilla::widget

///////////////////////////////////////////
// Creation Routine
///////////////////////////////////////////

already_AddRefed<Theme> do_CreateNativeThemeDoNotUseDirectly() {
  return do_AddRef(new nsNativeThemeWin());
}
