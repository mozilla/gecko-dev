/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigationUtils_h___
#define mozilla_dom_NavigationUtils_h___

#include "mozilla/Maybe.h"

namespace mozilla::dom {
enum class NavigationType : uint8_t;
enum class NavigationHistoryBehavior : uint8_t;

class NavigationUtils {
 public:
  static Maybe<NavigationHistoryBehavior> NavigationHistoryBehavior(
      NavigationType aNavigationType);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NavigationUtils_h___
