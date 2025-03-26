/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Stopwatch_h__
#define Stopwatch_h__

#include "mozilla/dom/UserInteractionBinding.h"

namespace mozilla {
namespace telemetry {

class UserInteractionStopwatch {
  using GlobalObject = mozilla::dom::GlobalObject;

 public:
  static bool Start(const GlobalObject& aGlobal,
                    const nsAString& aUserInteraction, const nsACString& aValue,
                    JS::Handle<JSObject*> aObj);
  static bool Running(const GlobalObject& aGlobal,
                      const nsAString& aUserInteraction,
                      JS::Handle<JSObject*> aObj);
  static bool Update(const GlobalObject& aGlobal,
                     const nsAString& aUserInteraction,
                     const nsACString& aValue, JS::Handle<JSObject*> aObj);
  static bool Cancel(const GlobalObject& aGlobal,
                     const nsAString& aUserInteraction,
                     JS::Handle<JSObject*> aObj);
  static bool Finish(const GlobalObject& aGlobal,
                     const nsAString& aUserInteraction,
                     JS::Handle<JSObject*> aObj,
                     const dom::Optional<nsACString>& aAdditionalText);
};

}  // namespace telemetry
}  // namespace mozilla

#endif  // Stopwatch_h__
