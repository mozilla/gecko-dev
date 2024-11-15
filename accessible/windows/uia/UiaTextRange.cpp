/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UiaTextRange.h"

#include "nsAccUtils.h"
#include "TextLeafRange.h"
#include <comdef.h>
#include <unordered_set>

// Handle MinGW builds - see bug 1929755 for more info
#if defined(__MINGW32__) || defined(__MINGW64__) || defined(__MINGW__)
#  include "supplementalMinGWDefinitions.h"
#endif

namespace mozilla::a11y {

// Used internally to safely get a UiaTextRange from a COM pointer provided
// to us by a client.
// {74B8E664-4578-4B52-9CBC-30A7A8271AE8}
static const GUID IID_UiaTextRange = {
    0x74b8e664,
    0x4578,
    0x4b52,
    {0x9c, 0xbc, 0x30, 0xa7, 0xa8, 0x27, 0x1a, 0xe8}};

// Helpers

static TextLeafPoint GetEndpoint(TextLeafRange& aRange,
                                 enum TextPatternRangeEndpoint aEndpoint) {
  if (aEndpoint == TextPatternRangeEndpoint_Start) {
    return aRange.Start();
  }
  return aRange.End();
}

static void RemoveExcludedAccessiblesFromRange(TextLeafRange& aRange) {
  MOZ_ASSERT(aRange);
  TextLeafPoint start = aRange.Start();
  TextLeafPoint end = aRange.End();
  if (start == end) {
    // The range is collapsed. It doesn't include anything.
    return;
  }
  if (end.mOffset != 0) {
    // It is theoretically possible for start to be at the exclusive end of a
    // previous Accessible (i.e. mOffset is its length), so the range doesn't
    // really encompass that Accessible's text and we should thus exclude that
    // Accessible. However, that hasn't been seen in practice yet. If it does
    // occur and cause problems, we should adjust the start point here.
    return;
  }
  // end is at the start of its Accessible. This can happen because we always
  // search for the start of a character, word, etc. Since the end of a range
  // is exclusive, the range doesn't include anything in this Accessible.
  // Move the end back so that it doesn't touch this Accessible at all. This
  // is important when determining what Accessibles lie within this range
  // because otherwise, we'd incorrectly consider an Accessible which the range
  // doesn't actually cover.
  // Move to the previous character.
  end = end.FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious);
  // We want the position immediately after this character in the same
  // Accessible.
  ++end.mOffset;
  if (start <= end) {
    aRange.SetEnd(end);
  }
}

static bool IsUiaEmbeddedObject(const Accessible* aAcc) {
  // "For UI Automation, an embedded object is any element that has non-textual
  // boundaries such as an image, hyperlink, table, or document type"
  // https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-textpattern-and-embedded-objects-overview
  if (aAcc->IsText()) {
    return false;
  }
  switch (aAcc->Role()) {
    case roles::CONTENT_DELETION:
    case roles::CONTENT_INSERTION:
    case roles::EMPHASIS:
    case roles::LANDMARK:
    case roles::MARK:
    case roles::NAVIGATION:
    case roles::NOTE:
    case roles::PARAGRAPH:
    case roles::REGION:
    case roles::SECTION:
    case roles::STRONG:
    case roles::SUBSCRIPT:
    case roles::SUPERSCRIPT:
    case roles::TEXT:
    case roles::TEXT_CONTAINER:
      return false;
    default:
      break;
  }
  return true;
}

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
  // Special handling of the insertion point at the end of a line only makes
  // sense when dealing with the caret, which is a collapsed range.
  mIsEndOfLineInsertionPoint = start == end && start.mIsEndOfLineInsertionPoint;
}

TextLeafRange UiaTextRange::GetRange() const {
  // Either Accessible might have been shut down because it was removed from the
  // tree. In that case, Acc() will return null, resulting in an invalid
  // TextLeafPoint and thus an invalid TextLeafRange. Any caller is expected to
  // handle this case.
  if (mIsEndOfLineInsertionPoint) {
    MOZ_ASSERT(mStartAcc == mEndAcc && mStartOffset == mEndOffset);
    TextLeafPoint point(mStartAcc->Acc(), mStartOffset);
    point.mIsEndOfLineInsertionPoint = true;
    return TextLeafRange(point, point);
  }
  return TextLeafRange({mStartAcc->Acc(), mStartOffset},
                       {mEndAcc->Acc(), mEndOffset});
}

/* static */
TextLeafRange UiaTextRange::GetRangeFrom(ITextRangeProvider* aProvider) {
  if (aProvider) {
    RefPtr<UiaTextRange> uiaRange;
    aProvider->QueryInterface(IID_UiaTextRange, getter_AddRefs(uiaRange));
    if (uiaRange) {
      return uiaRange->GetRange();
    }
  }
  return TextLeafRange();
}

/* static */
TextLeafPoint UiaTextRange::FindBoundary(const TextLeafPoint& aOrigin,
                                         enum TextUnit aUnit,
                                         nsDirection aDirection,
                                         bool aIncludeOrigin) {
  if (aUnit == TextUnit_Page || aUnit == TextUnit_Document) {
    // The UIA documentation is a little inconsistent regarding the Document
    // unit:
    // https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-textpattern-and-embedded-objects-overview
    // First, it says:
    // "Objects backed by the same text store as their container are referred to
    // as "compatible" embedded objects. These objects can be TextPattern
    // objects themselves and, in this case, their text ranges are comparable to
    // text ranges obtained from their container. This enables the providers to
    // expose client information about the individual TextPattern objects as if
    // they were one, large text provider."
    // But later, it says:
    // "For embedded TextPattern objects, the Document unit only recognizes the
    // content contained within that element."
    // If ranges are equivalent regardless of what object they were created
    // from, this doesn't make sense because this would mean that the Document
    // unit would change depending on where the range was positioned at the
    // time. Instead, Gecko restricts the range to an editable text control for
    // ITextProvider::get_DocumentRange, but returns the full document for
    // TextUnit_Document. This is consistent with Microsoft Word and Chromium.
    Accessible* doc = nsAccUtils::DocumentFor(aOrigin.mAcc);
    if (aDirection == eDirPrevious) {
      return TextLeafPoint(doc, 0);
    }
    return TextLeafPoint(doc, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT);
  }
  if (aUnit == TextUnit_Format) {
    // The UIA documentation says that TextUnit_Format aims to define ranges
    // that "include all text that shares all the same attributes."
    // FindTextAttrsStart considers container boundaries to be format boundaries
    // even if UIA may not. UIA's documentation may consider the next container
    // to be part of the same format run, since it may have the same attributes.
    // UIA considers embedded objects to be format boundaries, which is a more
    // restrictive understanding of boundaries than what Gecko implements here.
    return aOrigin.FindTextAttrsStart(aDirection, aIncludeOrigin);
  }
  AccessibleTextBoundary boundary;
  switch (aUnit) {
    case TextUnit_Character:
      boundary = nsIAccessibleText::BOUNDARY_CLUSTER;
      break;
    case TextUnit_Word:
      boundary = nsIAccessibleText::BOUNDARY_WORD_START;
      break;
    case TextUnit_Line:
      boundary = nsIAccessibleText::BOUNDARY_LINE_START;
      break;
    case TextUnit_Paragraph:
      boundary = nsIAccessibleText::BOUNDARY_PARAGRAPH;
      break;
    default:
      return TextLeafPoint();
  }
  return aOrigin.FindBoundary(
      boundary, aDirection,
      aIncludeOrigin ? TextLeafPoint::BoundaryFlags::eIncludeOrigin
                     : TextLeafPoint::BoundaryFlags::eDefaultBoundaryFlags);
}

bool UiaTextRange::MovePoint(TextLeafPoint& aPoint, enum TextUnit aUnit,
                             const int aRequestedCount, int& aActualCount) {
  aActualCount = 0;
  const nsDirection direction = aRequestedCount < 0 ? eDirPrevious : eDirNext;
  while (aActualCount != aRequestedCount) {
    TextLeafPoint oldPoint = aPoint;
    aPoint = FindBoundary(aPoint, aUnit, direction);
    if (!aPoint) {
      return false;  // Unit not supported.
    }
    if (aPoint == oldPoint) {
      break;  // Can't go any further.
    }
    direction == eDirPrevious ? --aActualCount : ++aActualCount;
  }
  return true;
}

void UiaTextRange::SetEndpoint(enum TextPatternRangeEndpoint aEndpoint,
                               const TextLeafPoint& aDest) {
  // Per the UIA documentation:
  // https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-itextrangeprovider-moveendpointbyrange#remarks
  // https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-itextrangeprovider-moveendpointbyunit#remarks
  // "If the endpoint being moved crosses the other endpoint of the same text
  // range, that other endpoint is moved also, resulting in a degenerate (empty)
  // range and ensuring the correct ordering of the endpoints (that is, the
  // start is always less than or equal to the end)."
  TextLeafRange origRange = GetRange();
  MOZ_ASSERT(origRange);
  if (aEndpoint == TextPatternRangeEndpoint_Start) {
    TextLeafPoint end = origRange.End();
    if (end < aDest) {
      end = aDest;
    }
    SetRange({aDest, end});
  } else {
    TextLeafPoint start = origRange.Start();
    if (aDest < start) {
      start = aDest;
    }
    SetRange({start, aDest});
  }
}

// IUnknown
IMPL_IUNKNOWN2(UiaTextRange, ITextRangeProvider, UiaTextRange)

// ITextRangeProvider methods

STDMETHODIMP
UiaTextRange::Clone(__RPC__deref_out_opt ITextRangeProvider** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  TextLeafRange range = GetRange();
  if (!range) {
    return CO_E_OBJNOTCONNECTED;
  }
  RefPtr uiaRange = new UiaTextRange(range);
  uiaRange.forget(aRetVal);
  return S_OK;
}

STDMETHODIMP
UiaTextRange::Compare(__RPC__in_opt ITextRangeProvider* aRange,
                      __RPC__out BOOL* aRetVal) {
  if (!aRange || !aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = GetRange() == GetRangeFrom(aRange);
  return S_OK;
}

STDMETHODIMP
UiaTextRange::CompareEndpoints(enum TextPatternRangeEndpoint aEndpoint,
                               __RPC__in_opt ITextRangeProvider* aTargetRange,
                               enum TextPatternRangeEndpoint aTargetEndpoint,
                               __RPC__out int* aRetVal) {
  if (!aTargetRange || !aRetVal) {
    return E_INVALIDARG;
  }
  TextLeafRange origRange = GetRange();
  if (!origRange) {
    return CO_E_OBJNOTCONNECTED;
  }
  TextLeafPoint origPoint = GetEndpoint(origRange, aEndpoint);
  TextLeafRange targetRange = GetRangeFrom(aTargetRange);
  if (!targetRange) {
    return E_INVALIDARG;
  }
  TextLeafPoint targetPoint = GetEndpoint(targetRange, aTargetEndpoint);
  if (origPoint == targetPoint) {
    *aRetVal = 0;
  } else if (origPoint < targetPoint) {
    *aRetVal = -1;
  } else {
    *aRetVal = 1;
  }
  return S_OK;
}

STDMETHODIMP
UiaTextRange::ExpandToEnclosingUnit(enum TextUnit aUnit) {
  TextLeafRange range = GetRange();
  if (!range) {
    return CO_E_OBJNOTCONNECTED;
  }
  TextLeafPoint origin = range.Start();
  TextLeafPoint start = FindBoundary(origin, aUnit, eDirPrevious,
                                     /* aIncludeOrigin */ true);
  if (!start) {
    return E_FAIL;  // Unit not supported.
  }
  TextLeafPoint end = FindBoundary(origin, aUnit, eDirNext);
  SetRange({start, end});
  return S_OK;
}

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

template <TEXTATTRIBUTEID Attr>
struct AttributeTraits {
  /*
   * To define a trait of this type, define the following members on a
   * specialization of this template struct:
   *   // Define the (Gecko) representation of the attribute type.
   *   using AttrType = <the type associated with the TEXTATTRIBUTEID>;
   *
   *   // Returns the attribute value at the TextLeafPoint, or Nothing{} if none
   *   // can be calculated.
   *   static Maybe<AttrType> GetValue(TextLeafPoint aPoint);
   *
   *   // Return the default value specified by the UIA documentation.
   *   static AttrType DefaultValue();
   *
   *   // Write the given value to the VARIANT output parameter. This may
   *   // require a non-trivial transformation from Gecko's idea of the value
   *   // into VARIANT form.
   *   static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue);
   */
};

template <TEXTATTRIBUTEID Attr>
HRESULT GetAttribute(const TextLeafRange& aRange, VARIANT& aVariant) {
  // Select the traits of the given TEXTATTRIBUTEID. This helps us choose the
  // correct functions to call to handle each attribute.
  using Traits = AttributeTraits<Attr>;
  using AttrType = typename Traits::AttrType;

  // Get the value at the start point. All other runs in the range must match
  // this value, otherwise the result is "mixed."
  const TextLeafPoint end = aRange.End();
  TextLeafPoint current = aRange.Start();
  Maybe<AttrType> val = Traits::GetValue(current);
  if (!val) {
    // Fall back to the UIA-specified default when we don't have an answer.
    val = Some(Traits::DefaultValue());
  }

  // Walk through the range one text attribute run start at a time, poking the
  // start points to check for the requested attribute. Stop before we hit the
  // end since the end point is either:
  //   1. At the start of the one-past-last text attribute run and hence
  //      excluded from the range
  //   2. After the start of the last text attribute run in the range and hence
  //      tested by that last run's start point
  while ((current = current.FindTextAttrsStart(eDirNext)) && current < end) {
    Maybe<AttrType> currentVal = Traits::GetValue(current);
    if (!currentVal) {
      // Fall back to the UIA-specified default when we don't have an answer.
      currentVal = Some(Traits::DefaultValue());
    }
    if (*currentVal != *val) {
      // If the attribute ever changes, then we need to return "[t]he address
      // of the value retrieved by the UiaGetReservedMixedAttributeValue
      // function."
      aVariant.vt = VT_UNKNOWN;
      return UiaGetReservedMixedAttributeValue(&aVariant.punkVal);
    }
  }

  // Write the value to the VARIANT output parameter.
  return Traits::WriteToVariant(aVariant, *val);
}

STDMETHODIMP
UiaTextRange::GetAttributeValue(TEXTATTRIBUTEID aAttributeId,
                                __RPC__out VARIANT* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  VariantInit(aRetVal);
  TextLeafRange range = GetRange();
  if (!range) {
    return CO_E_OBJNOTCONNECTED;
  }

  MOZ_ASSERT(range.Start() <= range.End(), "Range must be valid to proceed.");

  switch (aAttributeId) {
    case UIA_AnnotationTypesAttributeId:
      return GetAttribute<UIA_AnnotationTypesAttributeId>(range, *aRetVal);
    case UIA_FontNameAttributeId:
      return GetAttribute<UIA_FontNameAttributeId>(range, *aRetVal);
    case UIA_FontSizeAttributeId:
      return GetAttribute<UIA_FontSizeAttributeId>(range, *aRetVal);
    case UIA_FontWeightAttributeId:
      return GetAttribute<UIA_FontWeightAttributeId>(range, *aRetVal);
    case UIA_IsHiddenAttributeId:
      return GetAttribute<UIA_IsHiddenAttributeId>(range, *aRetVal);
    case UIA_IsItalicAttributeId:
      return GetAttribute<UIA_IsItalicAttributeId>(range, *aRetVal);
    case UIA_IsReadOnlyAttributeId:
      return GetAttribute<UIA_IsReadOnlyAttributeId>(range, *aRetVal);
    case UIA_StyleIdAttributeId:
      return GetAttribute<UIA_StyleIdAttributeId>(range, *aRetVal);
    case UIA_IsSubscriptAttributeId:
      return GetAttribute<UIA_IsSubscriptAttributeId>(range, *aRetVal);
    case UIA_IsSuperscriptAttributeId:
      return GetAttribute<UIA_IsSuperscriptAttributeId>(range, *aRetVal);
    default:
      // If the attribute isn't supported, return "[t]he address of the value
      // retrieved by the UiaGetReservedNotSupportedValue function."
      aRetVal->vt = VT_UNKNOWN;
      return UiaGetReservedNotSupportedValue(&aRetVal->punkVal);
  }

  MOZ_ASSERT_UNREACHABLE("Unhandled UIA Attribute ID");
  return S_OK;
}

STDMETHODIMP
UiaTextRange::GetBoundingRectangles(__RPC__deref_out_opt SAFEARRAY** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  TextLeafRange range = GetRange();
  if (!range) {
    return CO_E_OBJNOTCONNECTED;
  }

  // Get the rectangles for each line.
  const nsTArray<LayoutDeviceIntRect> lineRects = range.LineRects();

  // For UIA's purposes, the rectangles of this array are four doubles arranged
  // in order {left, top, width, height}.
  SAFEARRAY* rectsVec = SafeArrayCreateVector(VT_R8, 0, lineRects.Length() * 4);
  if (!rectsVec) {
    return E_OUTOFMEMORY;
  }

  // Empty range, return an empty array.
  if (lineRects.IsEmpty()) {
    *aRetVal = rectsVec;
    return S_OK;
  }

  // Get the double array out of the SAFEARRAY so we can write to it directly.
  double* safeArrayData = nullptr;
  HRESULT hr =
      SafeArrayAccessData(rectsVec, reinterpret_cast<void**>(&safeArrayData));
  if (FAILED(hr) || !safeArrayData) {
    SafeArrayDestroy(rectsVec);
    return E_FAIL;
  }

  // Convert the int array to a double array.
  for (size_t index = 0; index < lineRects.Length(); ++index) {
    const LayoutDeviceIntRect& lineRect = lineRects[index];
    safeArrayData[index * 4 + 0] = static_cast<double>(lineRect.x);
    safeArrayData[index * 4 + 1] = static_cast<double>(lineRect.y);
    safeArrayData[index * 4 + 2] = static_cast<double>(lineRect.width);
    safeArrayData[index * 4 + 3] = static_cast<double>(lineRect.height);
  }

  // Release the lock on the data. If that fails, bail out.
  hr = SafeArrayUnaccessData(rectsVec);
  if (FAILED(hr)) {
    SafeArrayDestroy(rectsVec);
    return E_FAIL;
  }

  *aRetVal = rectsVec;
  return S_OK;
}

STDMETHODIMP
UiaTextRange::GetEnclosingElement(
    __RPC__deref_out_opt IRawElementProviderSimple** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  TextLeafRange range = GetRange();
  if (!range) {
    return CO_E_OBJNOTCONNECTED;
  }
  RemoveExcludedAccessiblesFromRange(range);
  if (Accessible* enclosing =
          range.Start().mAcc->GetClosestCommonInclusiveAncestor(
              range.End().mAcc)) {
    RefPtr<IRawElementProviderSimple> uia = MsaaAccessible::GetFrom(enclosing);
    uia.forget(aRetVal);
  }
  return S_OK;
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
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  TextLeafRange range = GetRange();
  if (!range) {
    return CO_E_OBJNOTCONNECTED;
  }
  TextLeafPoint start = range.Start();
  const bool wasCollapsed = start == range.End();
  if (!wasCollapsed) {
    // Per the UIA documentation:
    // https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-itextrangeprovider-move#remarks
    // "For a non-degenerate (non-empty) text range, ITextRangeProvider::Move
    // should normalize and move the text range by performing the following
    // steps. ...
    // 2. If necessary, move the resulting text range backward in the document
    // to the beginning of the requested unit boundary."
    start = FindBoundary(start, aUnit, eDirPrevious, /* aIncludeOrigin */ true);
  }
  if (!MovePoint(start, aUnit, aCount, *aRetVal)) {
    return E_FAIL;
  }
  if (wasCollapsed) {
    // "For a degenerate text range, ITextRangeProvider::Move should simply move
    // the text insertion point by the specified number of text units."
    SetRange({start, start});
  } else {
    // "4. Expand the text range from the degenerate state by moving the ending
    // endpoint forward by one requested text unit boundary."
    TextLeafPoint end = FindBoundary(start, aUnit, eDirNext);
    if (end == start) {
      // start was already at the last boundary. Move start back to the previous
      // boundary.
      start = FindBoundary(start, aUnit, eDirPrevious);
      // In doing that, we ended up moving 1 less unit.
      --*aRetVal;
    }
    SetRange({start, end});
  }
  return S_OK;
}

STDMETHODIMP
UiaTextRange::MoveEndpointByUnit(enum TextPatternRangeEndpoint aEndpoint,
                                 enum TextUnit aUnit, int aCount,
                                 __RPC__out int* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  TextLeafRange range = GetRange();
  if (!range) {
    return CO_E_OBJNOTCONNECTED;
  }
  TextLeafPoint point = GetEndpoint(range, aEndpoint);
  if (!MovePoint(point, aUnit, aCount, *aRetVal)) {
    return E_FAIL;
  }
  SetEndpoint(aEndpoint, point);
  return S_OK;
}

STDMETHODIMP
UiaTextRange::MoveEndpointByRange(
    enum TextPatternRangeEndpoint aEndpoint,
    __RPC__in_opt ITextRangeProvider* aTargetRange,
    enum TextPatternRangeEndpoint aTargetEndpoint) {
  if (!aTargetRange) {
    return E_INVALIDARG;
  }
  TextLeafRange origRange = GetRange();
  if (!origRange) {
    return CO_E_OBJNOTCONNECTED;
  }
  TextLeafRange targetRange = GetRangeFrom(aTargetRange);
  if (!targetRange) {
    return E_INVALIDARG;
  }
  TextLeafPoint dest = GetEndpoint(targetRange, aTargetEndpoint);
  SetEndpoint(aEndpoint, dest);
  return S_OK;
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
  TextLeafRange range = GetRange();
  if (!range) {
    return CO_E_OBJNOTCONNECTED;
  }
  RemoveExcludedAccessiblesFromRange(range);
  Accessible* startAcc = range.Start().mAcc;
  Accessible* endAcc = range.End().mAcc;
  Accessible* common = startAcc->GetClosestCommonInclusiveAncestor(endAcc);
  if (!common) {
    return S_OK;
  }
  // Get all the direct children of `common` from `startAcc` through `endAcc`.
  // Find the index of the direct child containing startAcc.
  int32_t startIndex = -1;
  if (startAcc == common) {
    startIndex = 0;
  } else {
    Accessible* child = startAcc;
    for (;;) {
      Accessible* parent = child->Parent();
      if (parent == common) {
        startIndex = child->IndexInParent();
        break;
      }
      child = parent;
    }
    MOZ_ASSERT(startIndex >= 0);
  }
  // Find the index of the direct child containing endAcc.
  int32_t endIndex = -1;
  if (endAcc == common) {
    endIndex = static_cast<int32_t>(common->ChildCount()) - 1;
  } else {
    Accessible* child = endAcc;
    for (;;) {
      Accessible* parent = child->Parent();
      if (parent == common) {
        endIndex = child->IndexInParent();
        break;
      }
      child = parent;
    }
    MOZ_ASSERT(endIndex >= 0);
  }
  // Now get the children between startIndex and endIndex.
  // We guess 30 children because:
  // 1. It's unlikely that a client would call GetChildren on a very large range
  // because GetChildren is normally only called when reporting content and
  // reporting the entire content of a massive range in one hit isn't ideal for
  // performance.
  // 2. A client is more likely to query the content of a line, paragraph, etc.
  // 3. It seems unlikely that there would be more than 30 children in a line or
  // paragraph, especially because we're only including children that are
  // considered embedded objects by UIA.
  AutoTArray<Accessible*, 30> children;
  for (int32_t i = startIndex; i <= endIndex; ++i) {
    Accessible* child = common->ChildAt(static_cast<uint32_t>(i));
    if (IsUiaEmbeddedObject(child)) {
      children.AppendElement(child);
    }
  }
  *aRetVal = AccessibleArrayToUiaArray(children);
  return S_OK;
}

/*
 * AttributeTraits template specializations
 */

template <>
struct AttributeTraits<UIA_AnnotationTypesAttributeId> {
  // Avoiding nsTHashSet here because it has no operator==.
  using AttrType = std::unordered_set<int32_t>;
  static Maybe<AttrType> GetValue(TextLeafPoint aPoint) {
    // Check all of the given annotations. Build a set of the annotations that
    // are present at the given TextLeafPoint.
    RefPtr<AccAttributes> attrs = aPoint.GetTextAttributes();
    if (!attrs) {
      return {};
    }
    AttrType annotationsAtPoint{};

    // The "invalid" atom as a key in text attributes could have value
    // "spelling", "grammar", or "true". Spelling and grammar map directly to
    // UIA. A non-specific "invalid" indicates a generic data validation error,
    // and is mapped as such.
    if (auto invalid =
            attrs->GetAttribute<RefPtr<nsAtom>>(nsGkAtoms::invalid)) {
      const nsAtom* invalidAtom = invalid->get();
      if (invalidAtom == nsGkAtoms::spelling) {
        annotationsAtPoint.insert(AnnotationType_SpellingError);
      } else if (invalidAtom == nsGkAtoms::grammar) {
        annotationsAtPoint.insert(AnnotationType_GrammarError);
      } else if (invalidAtom == nsGkAtoms::_true) {
        annotationsAtPoint.insert(AnnotationType_DataValidationError);
      }
    }

    // The presence of the "mark" atom as a key in text attributes indicates a
    // highlight at this point.
    if (attrs->GetAttribute<bool>(nsGkAtoms::mark)) {
      annotationsAtPoint.insert(AnnotationType_Highlighted);
    }

    return Some(annotationsAtPoint);
  }

  static AttrType DefaultValue() {
    // Per UIA documentation, the default is an empty collection.
    return {};
  }

  static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue) {
    SAFEARRAY* outputArr =
        SafeArrayCreateVector(VT_I4, 0, static_cast<ULONG>(aValue.size()));
    if (!outputArr) {
      return E_OUTOFMEMORY;
    }

    // Copy the elements from the unordered_set to the SAFEARRAY.
    LONG index = 0;
    for (auto value : aValue) {
      const HRESULT hr = SafeArrayPutElement(outputArr, &index, &value);
      if (FAILED(hr)) {
        SafeArrayDestroy(outputArr);
        return hr;
      }
      ++index;
    }

    aVariant.vt = VT_ARRAY | VT_I4;
    aVariant.parray = outputArr;
    return S_OK;
  }
};

template <>
struct AttributeTraits<UIA_FontWeightAttributeId> {
  using AttrType = int32_t;  // LONG, but AccAttributes only accepts int32_t
  static Maybe<AttrType> GetValue(TextLeafPoint aPoint) {
    RefPtr<AccAttributes> attrs = aPoint.GetTextAttributes();
    if (!attrs) {
      return {};
    }
    return attrs->GetAttribute<AttrType>(nsGkAtoms::fontWeight);
  }

  static AttrType DefaultValue() {
    // See GDI LOGFONT structure and related standards.
    return FW_DONTCARE;
  }

  static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue) {
    aVariant.vt = VT_I4;
    aVariant.lVal = aValue;
    return S_OK;
  }
};

template <>
struct AttributeTraits<UIA_FontSizeAttributeId> {
  using AttrType = FontSize;
  static Maybe<AttrType> GetValue(TextLeafPoint aPoint) {
    RefPtr<AccAttributes> attrs = aPoint.GetTextAttributes();
    if (!attrs) {
      return {};
    }
    return attrs->GetAttribute<AttrType>(nsGkAtoms::font_size);
  }

  static AttrType DefaultValue() { return FontSize{0}; }

  static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue) {
    aVariant.vt = VT_I4;
    aVariant.lVal = aValue.mValue;
    return S_OK;
  }
};

template <>
struct AttributeTraits<UIA_FontNameAttributeId> {
  using AttrType = RefPtr<nsAtom>;
  static Maybe<AttrType> GetValue(TextLeafPoint aPoint) {
    RefPtr<AccAttributes> attrs = aPoint.GetTextAttributes();
    if (!attrs) {
      return {};
    }
    return attrs->GetAttribute<AttrType>(nsGkAtoms::font_family);
  }

  static AttrType DefaultValue() {
    // Default to the empty string (not null).
    return RefPtr<nsAtom>(nsGkAtoms::_empty);
  }

  static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue) {
    if (!aValue) {
      return E_INVALIDARG;
    }
    BSTR valueBStr = ::SysAllocString(aValue->GetUTF16String());
    if (!valueBStr) {
      return E_OUTOFMEMORY;
    }
    aVariant.vt = VT_BSTR;
    aVariant.bstrVal = valueBStr;
    return S_OK;
  }
};

template <>
struct AttributeTraits<UIA_IsItalicAttributeId> {
  using AttrType = bool;
  static Maybe<AttrType> GetValue(TextLeafPoint aPoint) {
    RefPtr<AccAttributes> attrs = aPoint.GetTextAttributes();
    if (!attrs) {
      return {};
    }

    // If the value in the attributes is a RefPtr<nsAtom>, it may be "italic" or
    // "normal"; check whether it is "italic".
    auto atomResult =
        attrs->GetAttribute<RefPtr<nsAtom>>(nsGkAtoms::font_style);
    if (atomResult) {
      MOZ_ASSERT(*atomResult, "Atom must be non-null");
      return Some((*atomResult)->Equals(u"italic"_ns));
    }
    // If the FontSlantStyle is not italic, the value is not stored as an nsAtom
    // in AccAttributes, so there's no need to check further.
    return {};
  }

  static AttrType DefaultValue() { return false; }

  static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue) {
    aVariant = _variant_t(aValue);
    return S_OK;
  }
};

template <>
struct AttributeTraits<UIA_StyleIdAttributeId> {
  using AttrType = int32_t;
  static Maybe<AttrType> GetValue(TextLeafPoint aPoint) {
    Accessible* acc = aPoint.mAcc;
    if (!acc || !acc->Parent()) {
      return {};
    }
    acc = acc->Parent();
    const role role = acc->Role();
    if (role == roles::HEADING) {
      switch (acc->GetLevel(true)) {
        case 1:
          return Some(StyleId_Heading1);
        case 2:
          return Some(StyleId_Heading2);
        case 3:
          return Some(StyleId_Heading3);
        case 4:
          return Some(StyleId_Heading4);
        case 5:
          return Some(StyleId_Heading5);
        case 6:
          return Some(StyleId_Heading6);
        default:
          return {};
      }
    }
    if (role == roles::BLOCKQUOTE) {
      return Some(StyleId_Quote);
    }
    if (role == roles::EMPHASIS) {
      return Some(StyleId_Emphasis);
    }
    return {};
  }

  static AttrType DefaultValue() { return 0; }

  static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue) {
    aVariant.vt = VT_I4;
    aVariant.lVal = aValue;
    return S_OK;
  }
};

template <>
struct AttributeTraits<UIA_IsSubscriptAttributeId> {
  using AttrType = bool;
  static Maybe<AttrType> GetValue(TextLeafPoint aPoint) {
    RefPtr<AccAttributes> attrs = aPoint.GetTextAttributes();
    if (!attrs) {
      return {};
    }
    auto atomResult =
        attrs->GetAttribute<RefPtr<nsAtom>>(nsGkAtoms::textPosition);
    if (atomResult) {
      MOZ_ASSERT(*atomResult, "Atom must be non-null");
      return Some(*atomResult == nsGkAtoms::sub);
    }
    return {};
  }

  static AttrType DefaultValue() { return false; }

  static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue) {
    aVariant = _variant_t(aValue);
    return S_OK;
  }
};

template <>
struct AttributeTraits<UIA_IsSuperscriptAttributeId> {
  using AttrType = bool;
  static Maybe<AttrType> GetValue(TextLeafPoint aPoint) {
    RefPtr<AccAttributes> attrs = aPoint.GetTextAttributes();
    if (!attrs) {
      return {};
    }
    auto atomResult =
        attrs->GetAttribute<RefPtr<nsAtom>>(nsGkAtoms::textPosition);
    if (atomResult) {
      MOZ_ASSERT(*atomResult, "Atom must be non-null");
      return Some((*atomResult)->Equals(NS_ConvertASCIItoUTF16("super")));
    }
    return {};
  }

  static AttrType DefaultValue() { return false; }

  static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue) {
    aVariant = _variant_t(aValue);
    return S_OK;
  }
};

template <>
struct AttributeTraits<UIA_IsHiddenAttributeId> {
  using AttrType = bool;
  static Maybe<AttrType> GetValue(TextLeafPoint aPoint) {
    if (!aPoint.mAcc) {
      return {};
    }
    const uint64_t state = aPoint.mAcc->State();
    return Some(!!(state & states::INVISIBLE));
  }

  static AttrType DefaultValue() { return false; }

  static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue) {
    aVariant = _variant_t(aValue);
    return S_OK;
  }
};

template <>
struct AttributeTraits<UIA_IsReadOnlyAttributeId> {
  using AttrType = bool;
  static Maybe<AttrType> GetValue(TextLeafPoint aPoint) {
    if (!aPoint.mAcc) {
      return {};
    }
    // Check the parent of the leaf, since the leaf itself will never be
    // editable, but the parent may. Check for both text fields and hypertexts,
    // since we might have something like <input> or a contenteditable <span>.
    Accessible* acc = aPoint.mAcc;
    Accessible* parent = acc->Parent();
    if (parent && parent->IsHyperText()) {
      acc = parent;
    } else {
      return Some(true);
    }
    const uint64_t state = acc->State();
    if (state & states::READONLY) {
      return Some(true);
    }
    if (state & states::EDITABLE) {
      return Some(false);
    }
    // Fall back to true if not editable or explicitly marked READONLY.
    return Some(true);
  }

  static AttrType DefaultValue() {
    // UIA says the default is false, but we fall back to true in GetValue since
    // most things on the web are read-only.
    return false;
  }

  static HRESULT WriteToVariant(VARIANT& aVariant, const AttrType& aValue) {
    aVariant = _variant_t(aValue);
    return S_OK;
  }
};

}  // namespace mozilla::a11y
