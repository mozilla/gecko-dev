/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TSFInputScope_h
#define TSFInputScope_h

#include <inputscope.h>

#include "WinUtils.h"

#include "nsTArray.h"

namespace mozilla::widget {

class TSFInputScope final : public ITfInputScope {
  ~TSFInputScope() {}

 public:
  explicit TSFInputScope(const nsTArray<InputScope>& aList);

  NS_INLINE_DECL_IUNKNOWN_REFCOUNTING(TSFInputScope)

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
    *ppv = nullptr;
    if ((IID_IUnknown == riid) || (IID_ITfInputScope == riid)) {
      *ppv = static_cast<ITfInputScope*>(this);
    }
    if (*ppv) {
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP GetInputScopes(InputScope** pprgInputScopes, UINT* pcCount);
  STDMETHODIMP GetPhrase(BSTR** ppbstrPhrases, UINT* pcCount) {
    return E_NOTIMPL;
  }
  STDMETHODIMP GetRegularExpression(BSTR* pbstrRegExp) { return E_NOTIMPL; }
  STDMETHODIMP GetSRGS(BSTR* pbstrSRGS) { return E_NOTIMPL; }
  STDMETHODIMP GetXML(BSTR* pbstrXML) { return E_NOTIMPL; }

 private:
  nsTArray<InputScope> mInputScopes;
};

}  // namespace mozilla::widget

#endif  // #ifndef TSFInputScope
