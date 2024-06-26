/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_UiaTextRange_h__
#define mozilla_a11y_UiaTextRange_h__

#include "IUnknownImpl.h"
#include "MsaaAccessible.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "nsDirection.h"
#include "objbase.h"
#include "uiautomation.h"

namespace mozilla::a11y {
class TextLeafRange;
class TextLeafPoint;

/**
 * ITextRangeProvider implementation.
 */
class UiaTextRange : public ITextRangeProvider {
 public:
  explicit UiaTextRange(TextLeafRange& aRange);
  virtual ~UiaTextRange() = default;

  // IUnknown
  DECL_IUNKNOWN

  // ITextRangeProvider
  virtual HRESULT STDMETHODCALLTYPE Clone(
      /* [retval][out] */ __RPC__deref_out_opt ITextRangeProvider** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE Compare(
      /* [in] */ __RPC__in_opt ITextRangeProvider* aRange,
      /* [retval][out] */ __RPC__out BOOL* aRetVal);

  virtual HRESULT STDMETHODCALLTYPE CompareEndpoints(
      /* [in] */ enum TextPatternRangeEndpoint aEndpoint,
      /* [in] */ __RPC__in_opt ITextRangeProvider* aTargetRange,
      /* [in] */ enum TextPatternRangeEndpoint aTargetEndpoint,
      /* [retval][out] */ __RPC__out int* aRetVal);

  virtual HRESULT STDMETHODCALLTYPE ExpandToEnclosingUnit(
      /* [in] */ enum TextUnit aUnit);

  virtual HRESULT STDMETHODCALLTYPE FindAttribute(
      /* [in] */ TEXTATTRIBUTEID aAttributeId,
      /* [in] */ VARIANT aVal,
      /* [in] */ BOOL aBackward,
      /* [retval][out] */ __RPC__deref_out_opt ITextRangeProvider** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE FindText(
      /* [in] */ __RPC__in BSTR aText,
      /* [in] */ BOOL aBackward,
      /* [in] */ BOOL aIgnoreCase,
      /* [retval][out] */ __RPC__deref_out_opt ITextRangeProvider** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE GetAttributeValue(
      /* [in] */ TEXTATTRIBUTEID aAttributeId,
      /* [retval][out] */ __RPC__out VARIANT* aRetVal);

  virtual HRESULT STDMETHODCALLTYPE GetBoundingRectangles(
      /* [retval][out] */ __RPC__deref_out_opt SAFEARRAY** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE GetEnclosingElement(
      /* [retval][out] */ __RPC__deref_out_opt IRawElementProviderSimple**
          aRetVal);

  virtual HRESULT STDMETHODCALLTYPE GetText(
      /* [in] */ int aMaxLength,
      /* [retval][out] */ __RPC__deref_out_opt BSTR* aRetVal);

  virtual HRESULT STDMETHODCALLTYPE Move(
      /* [in] */ enum TextUnit aUnit,
      /* [in] */ int aCount,
      /* [retval][out] */ __RPC__out int* aRetVal);

  virtual HRESULT STDMETHODCALLTYPE MoveEndpointByUnit(
      /* [in] */ enum TextPatternRangeEndpoint aEndpoint,
      /* [in] */ enum TextUnit aUnit,
      /* [in] */ int aCount,
      /* [retval][out] */ __RPC__out int* aRetVal);

  virtual HRESULT STDMETHODCALLTYPE MoveEndpointByRange(
      /* [in] */ enum TextPatternRangeEndpoint aEndpoint,
      /* [in] */ __RPC__in_opt ITextRangeProvider* aTargetRange,
      /* [in] */ enum TextPatternRangeEndpoint aTargetEndpoint);

  virtual HRESULT STDMETHODCALLTYPE Select(void);

  virtual HRESULT STDMETHODCALLTYPE AddToSelection(void);

  virtual HRESULT STDMETHODCALLTYPE RemoveFromSelection(void);

  virtual HRESULT STDMETHODCALLTYPE ScrollIntoView(
      /* [in] */ BOOL aAlignToTop);

  virtual HRESULT STDMETHODCALLTYPE GetChildren(
      /* [retval][out] */ __RPC__deref_out_opt SAFEARRAY** aRetVal);

 private:
  void SetRange(const TextLeafRange& aRange);
  TextLeafRange GetRange() const;
  static TextLeafRange GetRangeFrom(ITextRangeProvider* aProvider);
  static TextLeafPoint FindBoundary(const TextLeafPoint& aOrigin,
                                    enum TextUnit aUnit, nsDirection aDirection,
                                    bool aIncludeOrigin = false);
  bool MovePoint(TextLeafPoint& aPoint, enum TextUnit aUnit,
                 const int aRequestedCount, int& aActualCount);
  void SetEndpoint(enum TextPatternRangeEndpoint aEndpoint,
                   const TextLeafPoint& aDest);

  // Accessible doesn't support strong references and so neither does
  // TextLeafRange. Therefore, we hold strong references to MsaaAccessibles. We
  // combine those with offsets to reconstitute the TextLeafRange later.
  RefPtr<MsaaAccessible> mStartAcc;
  int32_t mStartOffset = -1;
  RefPtr<MsaaAccessible> mEndAcc;
  int32_t mEndOffset = -1;
  bool mIsEndOfLineInsertionPoint = false;
};

}  // namespace mozilla::a11y

#endif
