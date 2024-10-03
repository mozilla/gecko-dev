/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventDispatcher.h"

namespace mozilla::widget {

NS_IMPL_ISUPPORTS(EventDispatcher, nsIGeckoViewEventDispatcher)

bool EventDispatcher::HasListener(const char16_t* aEvent) { return false; }

}  // namespace mozilla::widget
