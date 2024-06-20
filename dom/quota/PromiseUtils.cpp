/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/PromiseUtils.h"

#include "jsapi.h"
#include "MainThreadUtils.h"
#include "nsDebug.h"
#include "xpcpublic.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom::quota {

nsresult CreatePromise(JSContext* aContext, Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContext);

  nsIGlobalObject* global =
      xpc::NativeGlobal(JS::CurrentGlobalOrNull(aContext));
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(global, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }

  promise.forget(aPromise);
  return NS_OK;
}

}  // namespace mozilla::dom::quota
