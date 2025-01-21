/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UserNavigationInvolvement_h___
#define mozilla_dom_UserNavigationInvolvement_h___
#include <cstdint>
namespace mozilla::dom {

// See https://bugzilla.mozilla.org/show_bug.cgi?id=1903552.
// https://html.spec.whatwg.org/#user-navigation-involvement
enum class UserNavigationInvolvement : uint8_t {
  None = 0,
  Activation = 1,
  BrowserUI = 2
};
}  // namespace mozilla::dom

#endif
