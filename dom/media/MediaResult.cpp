/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaResult.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {

#define EXTENDED_EXCEPTIONS                                              \
  DOMEXCEPTION(AbortError, NS_ERROR_ABORT);                              \
  DOMEXCEPTION(AbortError, NS_ERROR_DOM_MEDIA_ABORT_ERR);                \
  DOMEXCEPTION(RangeError, NS_ERROR_DOM_MEDIA_RANGE_ERR);                \
  DOMEXCEPTION(NotAllowedError, NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR);     \
  DOMEXCEPTION(NotSupportedError, NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR); \
  DOMEXCEPTION(TypeError, NS_ERROR_DOM_MEDIA_TYPE_ERR);

void MediaResult::ThrowTo(ErrorResult& aRv) const {
  switch (mCode) {
#define DOMEXCEPTION(name, code) \
  case code:                     \
    aRv.Throw##name(mMessage);   \
    break;
#include "mozilla/dom/DOMExceptionNames.h"
    EXTENDED_EXCEPTIONS
    default:
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      MOZ_CRASH_UNSAFE_PRINTF("Unhandled result 0x%08x",
                              static_cast<uint32_t>(mCode));
#endif
      aRv.ThrowUnknownError(mMessage);
      break;
  }

#undef DOMEXCEPTION
}

void MediaResult::RejectTo(dom::Promise* aPromise) const {
  switch (mCode) {
#define DOMEXCEPTION(name, code)               \
  case code:                                   \
    aPromise->MaybeRejectWith##name(mMessage); \
    break;
#include "mozilla/dom/DOMExceptionNames.h"
    EXTENDED_EXCEPTIONS
    default:
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      MOZ_CRASH_UNSAFE_PRINTF("Unhandled result 0x%08x",
                              static_cast<uint32_t>(mCode));
#endif
      aPromise->MaybeRejectWithUnknownError(mMessage);
      break;
  }

#undef DOMEXCEPTION
}

}  // namespace mozilla
