/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UiaTextRange.h"

#include "TextLeafRange.h"

namespace mozilla::a11y {

// UiaTextRange

UiaTextRange::UiaTextRange(TextLeafRange& aRange) {
  MOZ_ASSERT(aRange);
  SetRange(aRange);
}

void UiaTextRange::SetRange(const TextLeafRange& aRange) {
  TextLeafPoint start = aRange.Start();
  mStartAcc = MsaaAccessible::GetFrom(start.mAcc);
  MOZ_ASSERT(mStartAcc);
  mStartOffset = start.mOffset;
  TextLeafPoint end = aRange.End();
  mEndAcc = MsaaAccessible::GetFrom(end.mAcc);
  MOZ_ASSERT(mEndAcc);
  mEndOffset = end.mOffset;
}

TextLeafRange UiaTextRange::GetRange() const {
  // Either Accessible might have been shut down because it was removed from the
  // tree. In that case, Acc() will return null, resulting in an invalid
  // TextLeafPoint and thus an invalid TextLeafRange. Any caller is expected to
  // handle this case.
  return TextLeafRange({mStartAcc->Acc(), mStartOffset},
                       {mEndAcc->Acc(), mEndOffset});
}

// IUnknown
IMPL_IUNKNOWN1(UiaTextRange, ITextRangeProvider)

// ITextRangeProvider methods

STDMETHODIMP
UiaTextRange::Clone(__RPC__deref_out_opt ITextRangeProvider** aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::Compare(__RPC__in_opt ITextRangeProvider* aRange,
                      __RPC__out BOOL* aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::CompareEndpoints(enum TextPatternRangeEndpoint aEndpoint,
                               __RPC__in_opt ITextRangeProvider* aTargetRange,
                               enum TextPatternRangeEndpoint aTargetEndpoint,
                               __RPC__out int* aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::ExpandToEnclosingUnit(enum TextUnit aUnit) { return E_NOTIMPL; }

STDMETHODIMP
UiaTextRange::FindAttribute(TEXTATTRIBUTEID aAttributeId, VARIANT aVal,
                            BOOL aBackward,
                            __RPC__deref_out_opt ITextRangeProvider** aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::FindText(__RPC__in BSTR aText, BOOL aBackward, BOOL aIgnoreCase,
                       __RPC__deref_out_opt ITextRangeProvider** aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::GetAttributeValue(TEXTATTRIBUTEID aAttributeId,
                                __RPC__out VARIANT* aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::GetBoundingRectangles(__RPC__deref_out_opt SAFEARRAY** aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::GetEnclosingElement(
    __RPC__deref_out_opt IRawElementProviderSimple** aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::GetText(int aMaxLength, __RPC__deref_out_opt BSTR* aRetVal) {
  if (!aRetVal || aMaxLength < -1) {
    return E_INVALIDARG;
  }
  TextLeafRange range = GetRange();
  if (!range) {
    return CO_E_OBJNOTCONNECTED;
  }
  nsAutoString text;
  for (TextLeafRange segment : range) {
    TextLeafPoint start = segment.Start();
    int segmentLength = segment.End().mOffset - start.mOffset;
    // aMaxLength can be -1 to indicate no maximum.
    if (aMaxLength >= 0) {
      const int remaining = aMaxLength - text.Length();
      if (segmentLength > remaining) {
        segmentLength = remaining;
      }
    }
    start.mAcc->AppendTextTo(text, start.mOffset, segmentLength);
    if (aMaxLength >= 0 && static_cast<int32_t>(text.Length()) >= aMaxLength) {
      break;
    }
  }
  *aRetVal = ::SysAllocString(text.get());
  return S_OK;
}

STDMETHODIMP
UiaTextRange::Move(enum TextUnit aUnit, int aCount, __RPC__out int* aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::MoveEndpointByUnit(enum TextPatternRangeEndpoint aEndpoint,
                                 enum TextUnit aUnit, int aCount,
                                 __RPC__out int* aRetVal) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::MoveEndpointByRange(
    enum TextPatternRangeEndpoint aEndpoint,
    __RPC__in_opt ITextRangeProvider* aTargetRange,
    enum TextPatternRangeEndpoint aTargetEndpoint) {
  return E_NOTIMPL;
}

STDMETHODIMP
UiaTextRange::Select() { return E_NOTIMPL; }

STDMETHODIMP
UiaTextRange::AddToSelection() { return E_NOTIMPL; }

STDMETHODIMP
UiaTextRange::RemoveFromSelection() { return E_NOTIMPL; }

STDMETHODIMP
UiaTextRange::ScrollIntoView(BOOL aAlignToTop) { return E_NOTIMPL; }

STDMETHODIMP
UiaTextRange::GetChildren(__RPC__deref_out_opt SAFEARRAY** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  return S_OK;
}

}  // namespace mozilla::a11y
