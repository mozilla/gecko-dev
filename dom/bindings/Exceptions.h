/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Exceptions_h__
#define mozilla_dom_Exceptions_h__

// DOM exception throwing machinery (for both main thread and workers).

#include <stdint.h>
#include "jspubtd.h"
#include "nsIException.h"

class nsIStackFrame;
class nsPIDOMWindow;
template <class T>
struct already_AddRefed;

namespace mozilla {
namespace dom {

class Exception;

bool
Throw(JSContext* cx, nsresult rv, const char* sz = nullptr);

// Create, throw and report an exception to a given window.
void
ThrowAndReport(nsPIDOMWindow* aWindow, nsresult aRv,
               const char* aMessage = nullptr);

bool
ThrowExceptionObject(JSContext* aCx, Exception* aException);

bool
ThrowExceptionObject(JSContext* aCx, nsIException* aException);

// Create an exception object for the given nsresult and message but
// don't set it pending on aCx.  This never returns null.
already_AddRefed<Exception>
CreateException(JSContext* aCx, nsresult aRv, const char* aMessage = nullptr);

already_AddRefed<nsIStackFrame>
GetCurrentJSStack();

// Throwing a TypeError on an ErrorResult may result in SpiderMonkey using its
// own error reporting mechanism instead of just setting the exception on the
// context.  This happens if no script is running. Bug 1107777 adds a flag that
// forcibly turns this behaviour off. This is a stack helper to set the flag.
class MOZ_STACK_CLASS AutoForceSetExceptionOnContext {
private:
  JSContext* mCx;
  bool mOldValue;
public:
  explicit AutoForceSetExceptionOnContext(JSContext* aCx);
  ~AutoForceSetExceptionOnContext();
};

// Internal stuff not intended to be widely used.
namespace exceptions {

// aMaxDepth can be used to define a maximal depth for the stack trace. If the
// value is -1, a default maximal depth will be selected.
already_AddRefed<nsIStackFrame>
CreateStack(JSContext* aCx, int32_t aMaxDepth = -1);

} // namespace exceptions
} // namespace dom
} // namespace mozilla

#endif
