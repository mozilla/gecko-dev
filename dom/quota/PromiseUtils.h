/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_PROMISEUTILS_H_
#define DOM_QUOTA_PROMISEUTILS_H_

#include "ErrorList.h"
#include "jstypes.h"

struct JS_PUBLIC_API JSContext;

namespace mozilla::dom {
class Promise;
}

namespace mozilla::dom::quota {

nsresult CreatePromise(JSContext* aContext, Promise** aPromise);

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_PROMISEUTILS_H_
