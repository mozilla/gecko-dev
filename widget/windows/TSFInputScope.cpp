/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TSFInputScope.h"

#include "TSFUtils.h"
#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gIMELog;  // defined in TSFUtils.cpp

namespace mozilla::widget {

TSFInputScope::TSFInputScope(const nsTArray<InputScope>& aList)
    : mInputScopes(aList.Clone()) {
  MOZ_LOG(
      gIMELog, LogLevel::Info,
      ("0x%p TSFInputScope(%s)", this, AutoInputScopesCString(aList).get()));
}

STDMETHODIMP TSFInputScope::GetInputScopes(InputScope** pprgInputScopes,
                                           UINT* pcCount) {
  uint32_t count = (mInputScopes.IsEmpty() ? 1 : mInputScopes.Length());

  InputScope* pScope = (InputScope*)CoTaskMemAlloc(sizeof(InputScope) * count);
  NS_ENSURE_TRUE(pScope, E_OUTOFMEMORY);

  if (mInputScopes.IsEmpty()) {
    *pScope = IS_DEFAULT;
    *pcCount = 1;
    *pprgInputScopes = pScope;
    return S_OK;
  }

  *pcCount = 0;

  for (uint32_t idx = 0; idx < count; idx++) {
    *(pScope + idx) = mInputScopes[idx];
    (*pcCount)++;
  }

  *pprgInputScopes = pScope;
  return S_OK;
}

}  // namespace mozilla::widget
