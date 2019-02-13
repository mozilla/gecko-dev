/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMemory.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsJSUtils.h"

#include "jsapi.h"

#include "mozStoragePrivateHelpers.h"
#include "mozStorageAsyncStatement.h"
#include "mozStorageAsyncStatementParams.h"
#include "mozIStorageStatement.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// AsyncStatementParams

AsyncStatementParams::AsyncStatementParams(AsyncStatement *aStatement)
: mStatement(aStatement)
{
  NS_ASSERTION(mStatement != nullptr, "mStatement is null");
}

NS_IMPL_ISUPPORTS(
  AsyncStatementParams
, mozIStorageStatementParams
, nsIXPCScriptable
)

////////////////////////////////////////////////////////////////////////////////
//// nsIXPCScriptable

#define XPC_MAP_CLASSNAME AsyncStatementParams
#define XPC_MAP_QUOTED_CLASSNAME "AsyncStatementParams"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_RESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h"

NS_IMETHODIMP
AsyncStatementParams::SetProperty(
  nsIXPConnectWrappedNative *aWrapper,
  JSContext *aCtx,
  JSObject *aScopeObj,
  jsid aId,
  JS::Value *_vp,
  bool *_retval
)
{
  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);

  if (JSID_IS_INT(aId)) {
    int idx = JSID_TO_INT(aId);

    nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCtx, *_vp));
    NS_ENSURE_TRUE(variant, NS_ERROR_UNEXPECTED);
    nsresult rv = mStatement->BindByIndex(idx, variant);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (JSID_IS_STRING(aId)) {
    JSString *str = JSID_TO_STRING(aId);
    nsAutoJSString autoStr;
    if (!autoStr.init(aCtx, str)) {
      return NS_ERROR_FAILURE;
    }

    NS_ConvertUTF16toUTF8 name(autoStr);

    nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCtx, *_vp));
    NS_ENSURE_TRUE(variant, NS_ERROR_UNEXPECTED);
    nsresult rv = mStatement->BindByName(name, variant);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementParams::Resolve(nsIXPConnectWrappedNative *aWrapper,
                              JSContext *aCtx,
                              JSObject *aScopeObj,
                              jsid aId,
                              bool *aResolvedp,
                              bool *_retval)
{
  JS::Rooted<JSObject*> scopeObj(aCtx, aScopeObj);

  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);
  // We do not throw at any point after this because we want to allow the
  // prototype chain to be checked for the property.

  bool resolved = false;
  bool ok = true;
  if (JSID_IS_INT(aId)) {
    uint32_t idx = JSID_TO_INT(aId);
    // All indexes are good because we don't know how many parameters there
    // really are.
    ok = ::JS_DefineElement(aCtx, scopeObj, idx, JS::UndefinedHandleValue,
                            JSPROP_RESOLVING);
    resolved = true;
  }
  else if (JSID_IS_STRING(aId)) {
    // We are unable to tell if there's a parameter with this name and so
    // we must assume that there is.  This screws the rest of the prototype
    // chain, but people really shouldn't be depending on this anyways.
    JS::Rooted<jsid> id(aCtx, aId);
    ok = ::JS_DefinePropertyById(aCtx, scopeObj, id, JS::UndefinedHandleValue,
                                 JSPROP_RESOLVING);
    resolved = true;
  }

  *_retval = ok;
  *aResolvedp = resolved && ok;
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
