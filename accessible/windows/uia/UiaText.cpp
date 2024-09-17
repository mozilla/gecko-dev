/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UiaText.h"

#include "ia2AccessibleHypertext.h"
#include "mozilla/a11y/States.h"
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
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  AutoTArray<TextLeafRange, 1> ranges;
  TextLeafRange::GetSelection(acc, ranges);
  if (ranges.IsEmpty()) {
    // There is no selection. Check if there is a caret.
    if (TextLeafPoint caret = TextLeafPoint::GetCaret(acc)) {
      ranges.EmplaceBack(caret, caret);
    }
  }
  if (!ranges.IsEmpty()) {
    *aRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, ranges.Length());
    LONG indices[1] = {0};
    for (TextLeafRange& range : ranges) {
      // SafeArrayPutElement calls AddRef on the element, so we use a raw
      // pointer here.
      UiaTextRange* uiaRange = new UiaTextRange(range);
      SafeArrayPutElement(*aRetVal, indices, uiaRange);
      ++indices[0];
    }
  }
  return S_OK;
}

STDMETHODIMP
UiaText::GetVisibleRanges(__RPC__deref_out_opt SAFEARRAY** aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaText::RangeFromChild(__RPC__in_opt IRawElementProviderSimple* aChildElement,
                        __RPC__deref_out_opt ITextRangeProvider** aRetVal) {
  if (!aChildElement || !aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  Accessible* child = MsaaAccessible::GetAccessibleFrom(aChildElement);
  if (!child || !acc->IsAncestorOf(child)) {
    return E_INVALIDARG;
  }
  TextLeafRange range = TextLeafRange::FromAccessible(child);
  RefPtr uiaRange = new UiaTextRange(range);
  uiaRange.forget(aRetVal);
  return S_OK;
}

STDMETHODIMP
UiaText::RangeFromPoint(struct UiaPoint aPoint,
                        __RPC__deref_out_opt ITextRangeProvider** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  // Find the deepest accessible node at the given screen coordinates.
  Accessible* child = acc->ChildAtPoint(
      aPoint.x, aPoint.y, Accessible::EWhichChildAtPoint::DeepestChild);
  if (!child) {
    return E_INVALIDARG;
  }

  // Find the closest point within the entirety of the leaf where the screen
  // coordinates lie.
  TextLeafRange leafRange = TextLeafRange::FromAccessible(child);
  TextLeafPoint closestPoint =
      leafRange.TextLeafPointAtScreenPoint(aPoint.x, aPoint.y);
  TextLeafRange range{closestPoint, closestPoint};
  RefPtr uiaRange = new UiaTextRange(range);
  uiaRange.forget(aRetVal);
  return S_OK;
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
  TextLeafRange range = TextLeafRange::FromAccessible(acc);
  RefPtr uiaRange = new UiaTextRange(range);
  uiaRange.forget(aRetVal);
  return S_OK;
}

STDMETHODIMP
UiaText::get_SupportedTextSelection(
    __RPC__out enum SupportedTextSelection* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  if (acc->State() & states::SELECTABLE_TEXT) {
    *aRetVal = SupportedTextSelection_Multiple;
  } else {
    *aRetVal = SupportedTextSelection_None;
  }
  return S_OK;
}
