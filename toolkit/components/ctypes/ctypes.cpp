/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ctypes.h"

#include "js/CallArgs.h"     // JS::CallArgs
#include "js/ErrorReport.h"  // JS_ReportErrorASCII
#include "js/experimental/CTypes.h"  // JS::CTypesCallbacks, JS::InitCTypesClass, JS::SetCTypesCallbacks
#include "js/MemoryFunctions.h"     // JS_malloc
#include "js/PropertyAndElement.h"  // JS_GetProperty
#include "js/RootingAPI.h"          // JS::Rooted, JS::Handle
#include "js/TypeDecls.h"           // JSContext, JSObject
#include "js/Value.h"               // JS::Value
#include "ErrorList.h"              // NS_OK, NS_ERROR_*
#include "nsError.h"                // NS_FAILED
#include "nsString.h"               // nsAutoCString, nsDependentSubstring
#include "nsNativeCharsetUtils.h"   // NS_CopyUnicodeToNative
#include "xpc_make_class.h"

namespace mozilla::ctypes {

static char* UnicodeToNative(JSContext* cx, const char16_t* source,
                             size_t slen) {
  nsAutoCString native;
  nsDependentSubstring unicode(source, slen);
  nsresult rv = NS_CopyUnicodeToNative(unicode, native);
  if (NS_FAILED(rv)) {
    JS_ReportErrorASCII(cx, "could not convert string to native charset");
    return nullptr;
  }

  char* result = static_cast<char*>(JS_malloc(cx, native.Length() + 1));
  if (!result) {
    return nullptr;
  }

  memcpy(result, native.get(), native.Length() + 1);
  return result;
}

static JS::CTypesCallbacks sCallbacks = {UnicodeToNative};

NS_IMPL_ISUPPORTS(Module, nsIXPCScriptable)

Module::Module() = default;

Module::~Module() = default;

#define XPC_MAP_CLASSNAME Module
#define XPC_MAP_QUOTED_CLASSNAME "Module"
#define XPC_MAP_FLAGS XPC_SCRIPTABLE_WANT_CALL
#include "xpc_map_end.h"

static bool InitCTypesClassAndSetCallbacks(JSContext* cx,
                                           JS::Handle<JSObject*> scope) {
  // Init the ctypes object.
  if (!JS::InitCTypesClass(cx, scope)) {
    return false;
  }

  // Set callbacks for charset conversion and such.
  JS::Rooted<JS::Value> ctypes(cx);
  if (!JS_GetProperty(cx, scope, "ctypes", &ctypes)) {
    return false;
  }

  JS::SetCTypesCallbacks(ctypes.toObjectOrNull(), &sCallbacks);

  return true;
}

NS_IMETHODIMP
Module::Call(nsIXPConnectWrappedNative* wrapper, JSContext* cx, JSObject* obj,
             const JS::CallArgs& args, bool* retval) {
  if (!args.get(0).isObject()) {
    JS_ReportErrorASCII(cx, "Argument must be an object");
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> scope(cx, &args.get(0).toObject());

  if (!InitCTypesClassAndSetCallbacks(cx, scope)) {
    *retval = false;
    return NS_ERROR_FAILURE;
  }

  args.rval().setUndefined();
  *retval = true;
  return NS_OK;
}

}  // namespace mozilla::ctypes
