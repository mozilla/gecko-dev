/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_android_tests
#define mozilla_widget_android_tests

#if defined(ENABLE_TESTS)

#  include "nsIOhttpClientTest.h"

namespace mozilla::widget {

class OhttpClientTest final : public nsIOhttpClientTest {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOHTTPCLIENTTEST

  OhttpClientTest() = default;

 private:
  ~OhttpClientTest() = default;
};
}  // namespace mozilla::widget

#endif  // defined(ENABLE_TESTS)
#endif  // mozilla_widget_android_tests
