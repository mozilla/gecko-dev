/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefCounted.h"

namespace mozilla::detail {

#ifdef MOZ_REFCOUNTED_LEAK_CHECKING
RefCountLogger::LogAddRefFunc RefCountLogger::sLogAddRefFunc = nullptr;
RefCountLogger::LogReleaseFunc RefCountLogger::sLogReleaseFunc = nullptr;
size_t RefCountLogger::sNumStaticCtors = 0;
const char* RefCountLogger::sLastStaticCtorTypeName = nullptr;

MFBT_API void RefCountLogger::SetLeakCheckingFunctions(
    LogAddRefFunc aLogAddRefFunc, LogReleaseFunc aLogReleaseFunc) {
  if (sNumStaticCtors > 0) {
    // RefCountLogger was used before this point. Print a warning, similar to
    // ASSERT_ACTIVITY_IS_LEGAL. We do this here because SpiderMonkey standalone
    // and shell builds don't call this function and we don't want to report any
    // warnings in that case.
    fprintf(stderr,
            "RefCounted objects addrefed/released (static ctor?) total: %zu, "
            "last type: %s\n",
            sNumStaticCtors, sLastStaticCtorTypeName);
    sNumStaticCtors = 0;
    sLastStaticCtorTypeName = nullptr;
  }
  sLogAddRefFunc = aLogAddRefFunc;
  sLogReleaseFunc = aLogReleaseFunc;
}
#endif  // MOZ_REFCOUNTED_LEAK_CHECKING

}  // namespace mozilla::detail
