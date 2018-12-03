/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_AndroidColors_h
#define mozilla_widget_AndroidColors_h

#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace widget {

static const gfx::Color sAndroidBackgroundColor(gfx::Color(1.0f, 1.0f, 1.0f));
static const gfx::Color sAndroidBorderColor(gfx::Color(0.73f, 0.73f, 0.73f));
static const gfx::Color sAndroidCheckColor(gfx::Color(0.19f, 0.21f, 0.23f));
static const gfx::Color sAndroidDisabledColor(gfx::Color(0.88f, 0.88f, 0.88f));
static const gfx::Color sAndroidActiveColor(gfx::Color(0.94f, 0.94f, 0.94f));

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_AndroidColors_h
