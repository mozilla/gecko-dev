/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of simple property values within CSS declarations */

#ifndef nsCSSValue_h___
#define nsCSSValue_h___

#include "mozilla/Attributes.h"
#include "mozilla/CORSMode.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/SheetType.h"
#include "mozilla/URLExtraData.h"
#include "mozilla/UniquePtr.h"

#include "nsCSSKeywords.h"
#include "nsCSSPropertyID.h"
#include "nsCoord.h"
#include "nsProxyRelease.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsTArray.h"
#include "nsStyleConsts.h"
#include "nsStyleCoord.h"
#include "gfxFontFamilyList.h"

#include <type_traits>

class imgRequestProxy;
class nsAtom;
class nsIContent;
class nsIDocument;
class nsIPrincipal;
class nsIURI;
class nsPresContext;
template <class T>
class nsPtrHashKey;
struct RawServoCssUrlData;

namespace mozilla {
class CSSStyleSheet;
} // namespace mozilla

// Deletes a linked list iteratively to avoid blowing up the stack (bug 456196).
#define NS_CSS_DELETE_LIST_MEMBER(type_, ptr_, member_)                        \
  {                                                                            \
    type_ *cur = (ptr_)->member_;                                              \
    (ptr_)->member_ = nullptr;                                                 \
    while (cur) {                                                              \
      type_ *dlm_next = cur->member_;                                          \
      cur->member_ = nullptr;                                                  \
      delete cur;                                                              \
      cur = dlm_next;                                                          \
    }                                                                          \
  }
// Ditto, but use NS_RELEASE instead of 'delete' (bug 1221902).
#define NS_CSS_NS_RELEASE_LIST_MEMBER(type_, ptr_, member_)                    \
  {                                                                            \
    type_ *cur = (ptr_)->member_;                                              \
    (ptr_)->member_ = nullptr;                                                 \
    while (cur) {                                                              \
      type_ *dlm_next = cur->member_;                                          \
      cur->member_ = nullptr;                                                  \
      NS_RELEASE(cur);                                                         \
      cur = dlm_next;                                                          \
    }                                                                          \
  }

// Clones a linked list iteratively to avoid blowing up the stack.
// If it fails to clone the entire list then 'to_' is deleted and
// we return null.
#define NS_CSS_CLONE_LIST_MEMBER(type_, from_, member_, to_, args_)            \
  {                                                                            \
    type_ *dest = (to_);                                                       \
    (to_)->member_ = nullptr;                                                  \
    for (const type_ *src = (from_)->member_; src; src = src->member_) {       \
      type_ *clm_clone = src->Clone args_;                                     \
      if (!clm_clone) {                                                        \
        delete (to_);                                                          \
        return nullptr;                                                        \
      }                                                                        \
      dest->member_ = clm_clone;                                               \
      dest = clm_clone;                                                        \
    }                                                                          \
  }

// Forward declaration copied here since ServoBindings.h #includes nsCSSValue.h.
extern "C" {
  RawGeckoURLExtraDataBorrowedMut Servo_CssUrlData_GetExtraData(
    RawServoCssUrlDataBorrowed url);

  bool Servo_CssUrlData_IsLocalRef(RawServoCssUrlDataBorrowed url);
}

namespace mozilla {
namespace css {

struct URLValue final
{
public:
  // aCssUrl must not be null.
  URLValue(already_AddRefed<RawServoCssUrlData> aCssUrl,
           CORSMode aCORSMode)
    : mURIResolved(false)
    , mCssUrl(aCssUrl)
    , mCORSMode(aCORSMode)
  {
    MOZ_ASSERT(mCssUrl);
  }

  // Returns true iff all fields of the two URLValue objects are equal.
  //
  // Only safe to call on the main thread, since this will call Equals on the
  // nsIURI and nsIPrincipal objects stored on the URLValue objects.
  bool Equals(const URLValue& aOther) const;

  // Returns true iff we know for sure, by comparing the mBaseURI pointer,
  // the specified url() value mString, and IsLocalRef(), that these
  // two URLValue objects represent the same computed url() value.
  //
  // Doesn't look at mReferrer or mOriginPrincipal.
  //
  // Safe to call from any thread.
  bool DefinitelyEqualURIs(const URLValue& aOther) const;

  // Smae as DefinitelyEqualURIs but additionally compares the nsIPrincipal
  // pointers of the two URLValue objects.
  bool DefinitelyEqualURIsAndPrincipal(const URLValue& aOther) const;

  nsIURI* GetURI() const;

  bool IsLocalRef() const
  {
    return Servo_CssUrlData_IsLocalRef(mCssUrl);
  }

  bool HasRef() const;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(URLValue)

  // When matching a local ref URL, resolve it against aURI;
  // Otherwise, ignore aURL and return mURI directly.
  already_AddRefed<nsIURI> ResolveLocalRef(nsIURI* aURI) const;
  already_AddRefed<nsIURI> ResolveLocalRef(nsIContent* aContent) const;

  // Serializes mURI as a computed URI value, taking into account IsLocalRef()
  // and serializing just the fragment if true.
  void GetSourceString(nsString& aRef) const;

  nsDependentCSubstring GetString() const;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  imgRequestProxy* LoadImage(nsIDocument* aDocument);

  uint64_t LoadID() const { return mLoadID; }

  CORSMode CorsMode() const { return mCORSMode; }

  URLExtraData* ExtraData() const
  {
    return Servo_CssUrlData_GetExtraData(mCssUrl);
  }

private:
  // mURI stores the lazily resolved URI.  This may be null if the URI is
  // invalid, even once resolved.
  mutable nsCOMPtr<nsIURI> mURI;

  mutable bool mURIResolved;

  RefPtr<RawServoCssUrlData> mCssUrl;

  const CORSMode mCORSMode;

  // A unique, non-reused ID value for this URLValue over the life of the
  // process.  This value is only valid after LoadImage has been called.
  //
  // We use this as a key in some tables in ImageLoader.  This is better than
  // using the pointer value of the ImageValue object, since we can sometimes
  // delete ImageValues OMT but cannot update the ImageLoader tables until
  // we're back on the main thread.  So to avoid dangling pointers that might
  // get re-used by the time we want to update the ImageLoader tables, we use
  // these IDs.
  uint64_t mLoadID = 0;

  ~URLValue();

private:
  URLValue(const URLValue& aOther) = delete;
  URLValue& operator=(const URLValue& aOther) = delete;
};

struct GridNamedArea {
  nsString mName;
  uint32_t mColumnStart;
  uint32_t mColumnEnd;
  uint32_t mRowStart;
  uint32_t mRowEnd;
};

struct GridTemplateAreasValue final {
  // Parsed value
  nsTArray<GridNamedArea> mNamedAreas;

  // Original <string> values. Length gives the number of rows,
  // content makes serialization easier.
  nsTArray<nsString> mTemplates;

  // How many columns grid-template-areas contributes to the explicit grid.
  // http://dev.w3.org/csswg/css-grid/#explicit-grid
  uint32_t mNColumns;

  // How many rows grid-template-areas contributes to the explicit grid.
  // http://dev.w3.org/csswg/css-grid/#explicit-grid
  uint32_t NRows() const {
    return mTemplates.Length();
  }

  GridTemplateAreasValue()
    : mNColumns(0)
    // Default constructors for mNamedAreas and mTemplates: empty arrays.
  {
  }

  bool operator==(const GridTemplateAreasValue& aOther) const
  {
    return mTemplates == aOther.mTemplates;
  }

  bool operator!=(const GridTemplateAreasValue& aOther) const
  {
    return !(*this == aOther);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GridTemplateAreasValue)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // Private destructor to make sure this isn't used as a stack variable
  // or member variable.
  ~GridTemplateAreasValue()
  {
  }

  GridTemplateAreasValue(const GridTemplateAreasValue& aOther) = delete;
  GridTemplateAreasValue&
  operator=(const GridTemplateAreasValue& aOther) = delete;
};

} // namespace css
} // namespace mozilla

enum nsCSSUnit {
  eCSSUnit_Null         = 0,      // (n/a) null unit, value is not specified
  eCSSUnit_Auto         = 1,      // (n/a) value is algorithmic
  eCSSUnit_Inherit      = 2,      // (n/a) value is inherited
  eCSSUnit_Initial      = 3,      // (n/a) value is default UA value
  eCSSUnit_Unset        = 4,      // (n/a) value equivalent to 'initial' if on a reset property, 'inherit' otherwise
  eCSSUnit_None         = 5,      // (n/a) value is none
  eCSSUnit_Normal       = 6,      // (n/a) value is normal (algorithmic, different than auto)
  eCSSUnit_System_Font  = 7,      // (n/a) value is -moz-use-system-font
  eCSSUnit_All          = 8,      // (n/a) value is all
  eCSSUnit_Dummy        = 9,      // (n/a) a fake but specified value, used
                                  //       only in temporary values
  eCSSUnit_DummyInherit = 10,     // (n/a) a fake but specified value, used
                                  //       only in temporary values

  eCSSUnit_String       = 11,     // (char16_t*) a string value
  eCSSUnit_Ident        = 12,     // (char16_t*) a string value
  eCSSUnit_Attr         = 14,     // (char16_t*) a attr(string) value
  eCSSUnit_Element      = 15,     // (char16_t*) an element id

  eCSSUnit_Array        = 20,     // (nsCSSValue::Array*) a list of values
  eCSSUnit_Counter      = 21,     // (nsCSSValue::Array*) a counter(string,[string]) value
  eCSSUnit_Counters     = 22,     // (nsCSSValue::Array*) a counters(string,string[,string]) value
  eCSSUnit_Cubic_Bezier = 23,     // (nsCSSValue::Array*) a list of float values
  eCSSUnit_Steps        = 24,     // (nsCSSValue::Array*) a list of (integer, enumerated)
  eCSSUnit_Symbols      = 25,     // (nsCSSValue::Array*) a symbols(enumerated, symbols) value
  eCSSUnit_Function     = 26,     // (nsCSSValue::Array*) a function with
                                  //  parameters.  First elem of array is name,
                                  //  an nsCSSKeyword as eCSSUnit_Enumerated,
                                  //  the rest of the values are arguments.

  // The top level of a calc() expression is eCSSUnit_Calc.  All
  // remaining eCSSUnit_Calc_* units only occur inside these toplevel
  // calc values.

  // eCSSUnit_Calc has an array with exactly 1 element.  eCSSUnit_Calc
  // exists so we can distinguish calc(2em) from 2em as specified values
  // (but we drop this distinction for nsStyleCoord when we store
  // computed values).
  eCSSUnit_Calc         = 30,     // (nsCSSValue::Array*) calc() value
  // Plus, Minus, Times_* and Divided have arrays with exactly 2
  // elements.  a + b + c + d is grouped as ((a + b) + c) + d
  eCSSUnit_Calc_Plus    = 31,     // (nsCSSValue::Array*) + node within calc()

  eCSSUnit_Pair         = 50,     // (nsCSSValuePair*) pair of values
  eCSSUnit_List         = 53,     // (nsCSSValueList*) list of values
  eCSSUnit_SharedList   = 55,     // (nsCSSValueSharedList*) same as list
                                  //   but reference counted and shared
  eCSSUnit_PairList     = 56,     // (nsCSSValuePairList*) list of value pairs

  // Atom units
  eCSSUnit_AtomIdent    = 60,     // (nsAtom*) for its string as an identifier

  eCSSUnit_Integer      = 70,     // (int) simple value
  eCSSUnit_Enumerated   = 71,     // (int) value has enumerated meaning

  eCSSUnit_Percent      = 100,     // (float) 1.0 == 100%) value is percentage of something
  eCSSUnit_Number       = 101,     // (float) value is numeric (usually multiplier, different behavior than percent)

  // Length units - relative
  // Viewport relative measure
  eCSSUnit_ViewportWidth  = 700,    // (float) 1% of the width of the initial containing block
  eCSSUnit_ViewportHeight = 701,    // (float) 1% of the height of the initial containing block
  eCSSUnit_ViewportMin    = 702,    // (float) smaller of ViewportWidth and ViewportHeight
  eCSSUnit_ViewportMax    = 703,    // (float) larger of ViewportWidth and ViewportHeight

  // Font relative measure
  eCSSUnit_EM           = 800,    // (float) == current font size
  eCSSUnit_XHeight      = 801,    // (float) distance from top of lower case x to baseline
  eCSSUnit_Char         = 802,    // (float) number of characters, used for width with monospace font
  eCSSUnit_RootEM       = 803,    // (float) == root element font size

  // Screen relative measure
  eCSSUnit_Point        = 900,    // (float) 4/3 of a CSS pixel
  eCSSUnit_Inch         = 901,    // (float) 96 CSS pixels
  eCSSUnit_Millimeter   = 902,    // (float) 96/25.4 CSS pixels
  eCSSUnit_Centimeter   = 903,    // (float) 96/2.54 CSS pixels
  eCSSUnit_Pica         = 904,    // (float) 12 points == 16 CSS pixls
  eCSSUnit_Quarter      = 905,    // (float) 96/101.6 CSS pixels
  eCSSUnit_Pixel        = 906,    // (float) CSS pixel unit

  // Angular units
  eCSSUnit_Degree       = 1000,    // (float) 360 per circle

  // Frequency units
  eCSSUnit_Hertz        = 2000,    // (float) 1/seconds
  eCSSUnit_Kilohertz    = 2001,    // (float) 1000 Hertz

  // Time units
  eCSSUnit_Seconds      = 3000,    // (float) Standard time
  eCSSUnit_Milliseconds = 3001,    // (float) 1/1000 second

  // Flexible fraction (CSS Grid)
  eCSSUnit_FlexFraction = 4000,    // (float) Fraction of free space
};

struct nsCSSValuePair;
struct nsCSSValuePair_heap;
struct nsCSSValueList;
struct nsCSSValueList_heap;
struct nsCSSValueSharedList;
struct nsCSSValuePairList;
struct nsCSSValuePairList_heap;

class nsCSSValue {
public:
  struct Array;
  friend struct Array;

  // for valueless units only (null, auto, inherit, none, all, normal)
  explicit nsCSSValue(nsCSSUnit aUnit = eCSSUnit_Null)
    : mUnit(aUnit)
  {
    MOZ_ASSERT(aUnit <= eCSSUnit_DummyInherit, "not a valueless unit");
  }

  nsCSSValue(int32_t aValue, nsCSSUnit aUnit);
  nsCSSValue(float aValue, nsCSSUnit aUnit);
  nsCSSValue(const nsString& aValue, nsCSSUnit aUnit);
  nsCSSValue(Array* aArray, nsCSSUnit aUnit);
  nsCSSValue(const nsCSSValue& aCopy);
  nsCSSValue(nsCSSValue&& aOther)
    : mUnit(aOther.mUnit)
    , mValue(aOther.mValue)
  {
    aOther.mUnit = eCSSUnit_Null;
  }
  template<typename T,
           typename = typename std::enable_if<std::is_enum<T>::value>::type>
  explicit nsCSSValue(T aValue)
    : mUnit(eCSSUnit_Enumerated)
  {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within mValue.mInt");
    mValue.mInt = static_cast<int32_t>(aValue);
  }

  ~nsCSSValue() { Reset(); }

  nsCSSValue&  operator=(const nsCSSValue& aCopy);
  nsCSSValue&  operator=(nsCSSValue&& aCopy);
  bool        operator==(const nsCSSValue& aOther) const;

  bool operator!=(const nsCSSValue& aOther) const
  {
    return !(*this == aOther);
  }

  nsCSSUnit GetUnit() const { return mUnit; }
  bool      IsLengthUnit() const
    { return eCSSUnit_ViewportWidth <= mUnit && mUnit <= eCSSUnit_Pixel; }
  bool      IsLengthPercentCalcUnit() const
    { return IsLengthUnit() || mUnit == eCSSUnit_Percent || IsCalcUnit(); }
  /**
   * What the spec calls relative length units is, for us, split
   * between relative length units and pixel length units.
   *
   * A "relative" length unit is a multiple of some derived metric,
   * such as a font em-size, which itself was controlled by an input CSS
   * length. Relative length units should not be scaled by zooming, since
   * the underlying CSS length would already have been scaled.
   */
  bool      IsRelativeLengthUnit() const
    { return eCSSUnit_EM <= mUnit && mUnit <= eCSSUnit_RootEM; }
  /**
   * A "pixel" length unit is a some multiple of CSS pixels.
   */
  static bool IsPixelLengthUnit(nsCSSUnit aUnit)
    { return eCSSUnit_Point <= aUnit && aUnit <= eCSSUnit_Pixel; }
  bool      IsPixelLengthUnit() const
    { return IsPixelLengthUnit(mUnit); }
  static bool IsPercentLengthUnit(nsCSSUnit aUnit)
    { return aUnit == eCSSUnit_Percent; }
  bool      IsPercentLengthUnit()
    { return IsPercentLengthUnit(mUnit); }
  static bool IsFloatUnit(nsCSSUnit aUnit)
    { return eCSSUnit_Number <= aUnit; }
  bool      IsAngularUnit() const
    { return eCSSUnit_Degree == mUnit; }
  bool      IsFrequencyUnit() const
    { return eCSSUnit_Hertz <= mUnit && mUnit <= eCSSUnit_Kilohertz; }
  bool      IsTimeUnit() const
    { return eCSSUnit_Seconds <= mUnit && mUnit <= eCSSUnit_Milliseconds; }
  bool      IsCalcUnit() const
    { return eCSSUnit_Calc <= mUnit && mUnit <= eCSSUnit_Calc_Plus; }

  bool      UnitHasStringValue() const
    { return eCSSUnit_String <= mUnit && mUnit <= eCSSUnit_Element; }
  bool      UnitHasArrayValue() const
    { return eCSSUnit_Array <= mUnit && mUnit <= eCSSUnit_Calc_Plus; }

  int32_t GetIntValue() const
  {
    MOZ_ASSERT(mUnit == eCSSUnit_Integer ||
               mUnit == eCSSUnit_Enumerated,
               "not an int value");
    return mValue.mInt;
  }

  nsCSSKeyword GetKeywordValue() const
  {
    MOZ_ASSERT(mUnit == eCSSUnit_Enumerated, "not a keyword value");
    return static_cast<nsCSSKeyword>(mValue.mInt);
  }

  float GetPercentValue() const
  {
    MOZ_ASSERT(mUnit == eCSSUnit_Percent, "not a percent value");
    return mValue.mFloat;
  }

  float GetFloatValue() const
  {
    MOZ_ASSERT(eCSSUnit_Number <= mUnit, "not a float value");
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
    return mValue.mFloat;
  }

  float GetAngleValue() const
  {
    MOZ_ASSERT(eCSSUnit_Degree == mUnit, "not an angle value");
    return mValue.mFloat;
  }

  // Converts any angle to radians.
  double GetAngleValueInRadians() const;

  // Converts any angle to degrees.
  double GetAngleValueInDegrees() const;

  nsAString& GetStringValue(nsAString& aBuffer) const
  {
    MOZ_ASSERT(UnitHasStringValue(), "not a string value");
    aBuffer.Truncate();
    uint32_t len = NS_strlen(GetBufferValue(mValue.mString));
    mValue.mString->ToString(len, aBuffer);
    return aBuffer;
  }

  const char16_t* GetStringBufferValue() const
  {
    MOZ_ASSERT(UnitHasStringValue(), "not a string value");
    return GetBufferValue(mValue.mString);
  }

  Array* GetArrayValue() const
  {
    MOZ_ASSERT(UnitHasArrayValue(), "not an array value");
    return mValue.mArray;
  }

  nsCSSValueSharedList* GetSharedListValue() const
  {
    MOZ_ASSERT(mUnit == eCSSUnit_SharedList, "not a shared list value");
    return mValue.mSharedList;
  }

  // bodies of these are below
  inline nsCSSValuePair& GetPairValue();
  inline const nsCSSValuePair& GetPairValue() const;

  inline nsCSSValueList* GetListValue();
  inline const nsCSSValueList* GetListValue() const;

  inline nsCSSValuePairList* GetPairListValue();
  inline const nsCSSValuePairList* GetPairListValue() const;

  // Not making this inline because that would force us to include
  // imgIRequest.h, which leads to REQUIRES hell, since this header is included
  // all over.
  imgRequestProxy* GetImageValue(nsIDocument* aDocument) const;

  // Like GetImageValue, but additionally will pass the imgRequestProxy
  // through nsContentUtils::GetStaticRequest if aPresContent is static.
  already_AddRefed<imgRequestProxy> GetPossiblyStaticImageValue(
      nsIDocument* aDocument, nsPresContext* aPresContext) const;

  nscoord GetPixelLength() const;

  nsAtom* GetAtomValue() const {
    MOZ_ASSERT(mUnit == eCSSUnit_AtomIdent);
    return mValue.mAtom;
  }

  void Reset()  // sets to null
  {
    if (mUnit != eCSSUnit_Null)
      DoReset();
  }
private:
  void DoReset();

public:
  void SetIntValue(int32_t aValue, nsCSSUnit aUnit);
  template<typename T,
           typename = typename std::enable_if<std::is_enum<T>::value>::type>
  void SetEnumValue(T aValue)
  {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within mValue.mInt");
    SetIntValue(static_cast<int32_t>(aValue), eCSSUnit_Enumerated);
  }
  void SetPercentValue(float aValue);
  void SetFloatValue(float aValue, nsCSSUnit aUnit);
  void SetStringValue(const nsString& aValue, nsCSSUnit aUnit);
  void SetAtomIdentValue(already_AddRefed<nsAtom> aValue);
  // converts the nscoord to pixels
  void SetIntegerCoordValue(nscoord aCoord);
  void SetArrayValue(nsCSSValue::Array* aArray, nsCSSUnit aUnit);
  void SetPairValue(const nsCSSValuePair* aPair);
  void SetPairValue(const nsCSSValue& xValue, const nsCSSValue& yValue);
  void SetSharedListValue(nsCSSValueSharedList* aList);
  void SetNoneValue();

  nsStyleCoord::CalcValue GetCalcValue() const;
  void SetCalcValue(const nsStyleCoord::CalcValue&);

  // These are a little different - they allocate storage for you and
  // return a handle.
  nsCSSValueList* SetListValue();
  nsCSSValuePairList* SetPairListValue();

  // Returns an already addrefed buffer.  Guaranteed to return non-null.
  // (Will abort on allocation failure.)
  static already_AddRefed<nsStringBuffer>
    BufferFromString(const nsString& aValue);

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  static void
  AppendAlignJustifyValueToString(int32_t aValue, nsAString& aResult);

private:
  static const char16_t* GetBufferValue(nsStringBuffer* aBuffer) {
    return static_cast<char16_t*>(aBuffer->Data());
  }

protected:
  nsCSSUnit mUnit;
  union {
    int32_t    mInt;
    float      mFloat;
    // Note: the capacity of the buffer may exceed the length of the string.
    // If we're of a string type, mString is not null.
    nsStringBuffer* MOZ_OWNING_REF mString;
    nsAtom* MOZ_OWNING_REF mAtom;
    Array* MOZ_OWNING_REF mArray;
    nsCSSValuePair_heap* MOZ_OWNING_REF mPair;
    nsCSSValueList_heap* MOZ_OWNING_REF mList;
    nsCSSValueSharedList* MOZ_OWNING_REF mSharedList;
    nsCSSValuePairList_heap* MOZ_OWNING_REF mPairList;
  } mValue;
};

struct nsCSSValue::Array final {

  // return |Array| with reference count of zero
  static Array* Create(size_t aItemCount) {
    return new (aItemCount) Array(aItemCount);
  }

  nsCSSValue& operator[](size_t aIndex) {
    MOZ_ASSERT(aIndex < mCount, "out of range");
    return mArray[aIndex];
  }

  const nsCSSValue& operator[](size_t aIndex) const {
    MOZ_ASSERT(aIndex < mCount, "out of range");
    return mArray[aIndex];
  }

  nsCSSValue& Item(size_t aIndex) { return (*this)[aIndex]; }
  const nsCSSValue& Item(size_t aIndex) const { return (*this)[aIndex]; }

  size_t Count() const { return mCount; }

  // callers depend on the items being contiguous
  nsCSSValue* ItemStorage() {
    return this->First();
  }

  bool operator==(const Array& aOther) const
  {
    if (mCount != aOther.mCount)
      return false;
    for (size_t i = 0; i < mCount; ++i)
      if ((*this)[i] != aOther[i])
        return false;
    return true;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Array);
private:

  const size_t mCount;
  // This must be the last sub-object, since we extend this array to
  // be of size mCount; it needs to be a sub-object so it gets proper
  // alignment.
  nsCSSValue mArray[1];

  void* operator new(size_t aSelfSize, size_t aItemCount) CPP_THROW_NEW {
    MOZ_ASSERT(aItemCount > 0, "cannot have a 0 item count");
    return ::operator new(aSelfSize + sizeof(nsCSSValue) * (aItemCount - 1));
  }

  void operator delete(void* aPtr) { ::operator delete(aPtr); }

  nsCSSValue* First() { return mArray; }

  const nsCSSValue* First() const { return mArray; }

#define CSSVALUE_LIST_FOR_EXTRA_VALUES(var)                                   \
  for (nsCSSValue *var = First() + 1, *var##_end = First() + mCount;          \
       var != var##_end; ++var)

  explicit Array(size_t aItemCount)
    : mRefCnt(0)
    , mCount(aItemCount)
  {
    CSSVALUE_LIST_FOR_EXTRA_VALUES(val) {
      new (val) nsCSSValue();
    }
  }

  ~Array()
  {
    CSSVALUE_LIST_FOR_EXTRA_VALUES(val) {
      val->~nsCSSValue();
    }
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

#undef CSSVALUE_LIST_FOR_EXTRA_VALUES

private:
  Array(const Array& aOther) = delete;
  Array& operator=(const Array& aOther) = delete;
};

// Prefer nsCSSValue::Array for lists of fixed size.
struct nsCSSValueList {
  nsCSSValueList() : mNext(nullptr) { MOZ_COUNT_CTOR(nsCSSValueList); }
  ~nsCSSValueList();

  nsCSSValueList* Clone() const;  // makes a deep copy. Infallible.
  void CloneInto(nsCSSValueList* aList) const; // makes a deep copy into aList

  static bool Equal(const nsCSSValueList* aList1,
                    const nsCSSValueList* aList2);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCSSValue      mValue;
  nsCSSValueList* mNext;

private:
  nsCSSValueList(const nsCSSValueList& aCopy) // makes a shallow copy
    : mValue(aCopy.mValue), mNext(nullptr)
  {
    MOZ_COUNT_CTOR(nsCSSValueList);
  }

  // We don't want operator== or operator!= because they wouldn't be
  // null-safe, which is generally what we need.  Use |Equal| method
  // above instead.
  bool operator==(nsCSSValueList const& aOther) const = delete;
  bool operator!=(const nsCSSValueList& aOther) const = delete;
};

// nsCSSValueList_heap differs from nsCSSValueList only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSValueList_heap final : public nsCSSValueList {
  NS_INLINE_DECL_REFCOUNTING(nsCSSValueList_heap)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsCSSValueList_heap()
  {
  }
};

// This is a reference counted list value.  Note that the object is
// a wrapper for the reference count and a pointer to the head of the
// list, whereas the other list types (such as nsCSSValueList) do
// not have such a wrapper.
struct nsCSSValueSharedList final {
  nsCSSValueSharedList()
    : mHead(nullptr)
  {
  }

  // Takes ownership of aList.
  explicit nsCSSValueSharedList(nsCSSValueList* aList)
    : mHead(aList)
  {
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsCSSValueSharedList();

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsCSSValueSharedList)

  bool operator==(nsCSSValueSharedList const& aOther) const;
  bool operator!=(const nsCSSValueSharedList& aOther) const
  { return !(*this == aOther); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCSSValueList* mHead;
};

inline nsCSSValueList*
nsCSSValue::GetListValue()
{
  MOZ_DIAGNOSTIC_ASSERT(mUnit == eCSSUnit_List, "not a list value");
  return mValue.mList;
}

inline const nsCSSValueList*
nsCSSValue::GetListValue() const
{
  MOZ_DIAGNOSTIC_ASSERT(mUnit == eCSSUnit_List, "not a list value");
  return mValue.mList;
}

struct nsCSSValuePair {
  nsCSSValuePair()
  {
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  explicit nsCSSValuePair(nsCSSUnit aUnit)
    : mXValue(aUnit), mYValue(aUnit)
  {
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  nsCSSValuePair(const nsCSSValue& aXValue, const nsCSSValue& aYValue)
    : mXValue(aXValue), mYValue(aYValue)
  {
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  nsCSSValuePair(const nsCSSValuePair& aCopy)
    : mXValue(aCopy.mXValue), mYValue(aCopy.mYValue)
  {
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  ~nsCSSValuePair()
  {
    MOZ_COUNT_DTOR(nsCSSValuePair);
  }

  nsCSSValuePair& operator=(const nsCSSValuePair& aOther) {
    mXValue = aOther.mXValue;
    mYValue = aOther.mYValue;
    return *this;
  }

  bool operator==(const nsCSSValuePair& aOther) const {
    return mXValue == aOther.mXValue &&
           mYValue == aOther.mYValue;
  }

  bool operator!=(const nsCSSValuePair& aOther) const {
    return mXValue != aOther.mXValue ||
           mYValue != aOther.mYValue;
  }

  bool BothValuesEqualTo(const nsCSSValue& aValue) const {
    return mXValue == aValue &&
           mYValue == aValue;
  }

  void SetBothValuesTo(const nsCSSValue& aValue) {
    mXValue = aValue;
    mYValue = aValue;
  }

  void Reset() {
    mXValue.Reset();
    mYValue.Reset();
  }

  bool HasValue() const {
    return mXValue.GetUnit() != eCSSUnit_Null ||
           mYValue.GetUnit() != eCSSUnit_Null;
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCSSValue mXValue;
  nsCSSValue mYValue;
};

// nsCSSValuePair_heap differs from nsCSSValuePair only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSValuePair_heap final : public nsCSSValuePair {
  // forward constructor
  nsCSSValuePair_heap(const nsCSSValue& aXValue, const nsCSSValue& aYValue)
      : nsCSSValuePair(aXValue, aYValue)
  {}

  NS_INLINE_DECL_REFCOUNTING(nsCSSValuePair_heap)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsCSSValuePair_heap()
  {
  }
};

// This has to be here so that the relationship between nsCSSValuePair
// and nsCSSValuePair_heap is visible.
inline nsCSSValuePair&
nsCSSValue::GetPairValue()
{
  MOZ_ASSERT(mUnit == eCSSUnit_Pair, "not a pair value");
  return *mValue.mPair;
}

inline const nsCSSValuePair&
nsCSSValue::GetPairValue() const
{
  MOZ_ASSERT(mUnit == eCSSUnit_Pair, "not a pair value");
  return *mValue.mPair;
}

// Maybe should be replaced with nsCSSValueList and nsCSSValue::Array?
struct nsCSSValuePairList {
  nsCSSValuePairList() : mNext(nullptr) { MOZ_COUNT_CTOR(nsCSSValuePairList); }
  ~nsCSSValuePairList();

  nsCSSValuePairList* Clone() const; // makes a deep copy. Infallible.

  static bool Equal(const nsCSSValuePairList* aList1,
                    const nsCSSValuePairList* aList2);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCSSValue          mXValue;
  nsCSSValue          mYValue;
  nsCSSValuePairList* mNext;

private:
  nsCSSValuePairList(const nsCSSValuePairList& aCopy) // makes a shallow copy
    : mXValue(aCopy.mXValue), mYValue(aCopy.mYValue), mNext(nullptr)
  {
    MOZ_COUNT_CTOR(nsCSSValuePairList);
  }

  // We don't want operator== or operator!= because they wouldn't be
  // null-safe, which is generally what we need.  Use |Equal| method
  // above instead.
  bool operator==(const nsCSSValuePairList& aOther) const = delete;
  bool operator!=(const nsCSSValuePairList& aOther) const = delete;
};

// nsCSSValuePairList_heap differs from nsCSSValuePairList only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSValuePairList_heap final : public nsCSSValuePairList {
  NS_INLINE_DECL_REFCOUNTING(nsCSSValuePairList_heap)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsCSSValuePairList_heap()
  {
  }
};

// This has to be here so that the relationship between nsCSSValuePairList
// and nsCSSValuePairList_heap is visible.
inline nsCSSValuePairList*
nsCSSValue::GetPairListValue()
{
  MOZ_DIAGNOSTIC_ASSERT(mUnit == eCSSUnit_PairList, "not a pairlist value");
  return mValue.mPairList;
}

inline const nsCSSValuePairList*
nsCSSValue::GetPairListValue() const
{
  MOZ_DIAGNOSTIC_ASSERT(mUnit == eCSSUnit_PairList, "not a pairlist value");
  return mValue.mPairList;
}

#endif /* nsCSSValue_h___ */

