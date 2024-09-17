/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_UiaText_h__
#define mozilla_a11y_UiaText_h__

#include "objbase.h"
#include "uiautomation.h"

namespace mozilla::a11y {
class Accessible;

/**
 * ITextProvider implementation.
 */
class UiaText : public ITextProvider {
  // ITextProvider
  virtual HRESULT STDMETHODCALLTYPE GetSelection(
      /* [retval][out] */ __RPC__deref_out_opt SAFEARRAY** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE GetVisibleRanges(
      /* [retval][out] */ __RPC__deref_out_opt SAFEARRAY** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE RangeFromChild(
      /* [in] */ __RPC__in_opt IRawElementProviderSimple* aChildElement,
      /* [retval][out] */ __RPC__deref_out_opt ITextRangeProvider** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE RangeFromPoint(
      /* [in] */ struct UiaPoint aPoint,
      /* [retval][out] */ __RPC__deref_out_opt ITextRangeProvider** aRetVal);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_DocumentRange(
      /* [retval][out] */ __RPC__deref_out_opt ITextRangeProvider** aRetVal);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_SupportedTextSelection(
      /* [retval][out] */ __RPC__out enum SupportedTextSelection* aRetVal);

 private:
  Accessible* Acc() const;
};

}  // namespace mozilla::a11y

#endif
