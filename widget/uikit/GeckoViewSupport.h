/* clang-format off */
/* -*- Mode: Objective-C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_GeckoViewSupport_h
#define mozilla_widget_GeckoViewSupport_h

/* This header needs to stay valid Objective-C (not Objective-C++) when
 * built independently because it is used for Swift bridging too. */

#include <GeckoView/GeckoViewSwiftSupport.h>

namespace mozilla::widget {

id<SwiftGeckoViewRuntime> GetSwiftRuntime();

id<GeckoProcessExtension> GetCurrentProcessExtension();

}  // namespace mozilla::widget

#endif /* mozilla_widget_GeckoViewSupport_h */
