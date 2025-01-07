/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NyxFunctions
#define mozilla_dom_NyxFunctions

#include "mozilla/dom/TypedArray.h"

namespace mozilla::dom {

class GlobalObject;

class Nyx final {
 public:
  static void Log(const GlobalObject&, const nsACString& aMsg);

  static bool IsEnabled(const GlobalObject&, const nsACString& aFuzzerName);

  static bool IsReplay(const GlobalObject&);

  static bool IsStarted(const GlobalObject&);

  static void Start(const GlobalObject&);

  static void Release(const GlobalObject&, uint32_t iterations = 1);

  static void GetRawData(const GlobalObject&,
                         JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aRv);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NyxFunctions
