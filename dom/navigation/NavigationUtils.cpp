/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/NavigationBinding.h"
#include "mozilla/dom/NavigationUtils.h"

namespace mozilla::dom {

/* static */ Maybe<NavigationHistoryBehavior>
NavigationUtils::NavigationHistoryBehavior(NavigationType aNavigationType) {
  switch (aNavigationType) {
    case NavigationType::Push:
      return Some(NavigationHistoryBehavior::Push);
    case NavigationType::Replace:
      return Some(NavigationHistoryBehavior::Replace);
    default:
      break;
  }
  return Nothing();
}

}  // namespace mozilla::dom
