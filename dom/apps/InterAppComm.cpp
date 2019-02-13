/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterAppComm.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"
#include "nsJSPrincipals.h"
#include "mozilla/Preferences.h"
#include "AccessCheck.h"

using namespace mozilla::dom;

/* static */ bool
InterAppComm::EnabledForScope(JSContext* /* unused */,
                              JS::Handle<JSObject*> /* unused */)
{
  // Disable the constructors if they're disabled by the preference for sure.
  if (!Preferences::GetBool("dom.inter-app-communication-api.enabled", false)) {
    return false;
  }

  // Only expose the constructors to the chrome codes for Gecko internal uses.
  // The content pages shouldn't be aware of the constructors.
  return nsContentUtils::ThreadsafeIsCallerChrome();
}
