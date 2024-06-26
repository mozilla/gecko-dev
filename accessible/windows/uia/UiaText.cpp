/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UiaText.h"

#include "ia2AccessibleHypertext.h"
#include "TextLeafRange.h"
#include "UiaTextRange.h"

using namespace mozilla;
using namespace mozilla::a11y;

// UiaText

Accessible* UiaText::Acc() const {
  auto* hyp = static_cast<const ia2AccessibleHypertext*>(this);
  return hyp->Acc();
}

// ITextProvider methods

STDMETHODIMP
UiaText::GetSelection(__RPC__deref_out_opt SAFEARRAY** aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaText::GetVisibleRanges(__RPC__deref_out_opt SAFEARRAY** aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaText::RangeFromChild(__RPC__in_opt IRawElementProviderSimple* childElement,
                        __RPC__deref_out_opt ITextRangeProvider** aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaText::RangeFromPoint(struct UiaPoint point,
                        __RPC__deref_out_opt ITextRangeProvider** aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaText::get_DocumentRange(__RPC__deref_out_opt ITextRangeProvider** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  // On the web, the "document range" could either span the entire document or
  // just a text input control, depending on the element on which the Text
  // pattern was queried. See:
  // https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-textpattern-and-embedded-objects-overview#webpage-and-text-input-controls-in-edge
  TextLeafRange range({acc, 0},
                      {acc, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT});
  RefPtr uiaRange = new UiaTextRange(range);
  uiaRange.forget(aRetVal);
  return S_OK;
}

STDMETHODIMP
UiaText::get_SupportedTextSelection(
    __RPC__out enum SupportedTextSelection* aRetVal) {
  return E_NOTIMPL;
}
