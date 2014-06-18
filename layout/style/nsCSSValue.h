/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of simple property values within CSS declarations */

#ifndef nsCSSValue_h___
#define nsCSSValue_h___

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsCSSKeywords.h"
#include "nsCSSProperty.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsTArray.h"
#include "nsStyleConsts.h"
#include "gfxFontFamilyList.h"

class imgRequestProxy;
class nsCSSStyleSheet;
class nsIDocument;
class nsIPrincipal;
class nsIURI;
class nsPresContext;
template <class T>
class nsPtrHashKey;

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

namespace mozilla {
namespace css {

struct URLValue {
  // Methods are not inline because using an nsIPrincipal means requiring
  // caps, which leads to REQUIRES hell, since this header is included all
  // over.

  // For both constructors aString must not be null.
  // For both constructors aOriginPrincipal must not be null.
  // Construct with a base URI; this will create the actual URI lazily from
  // aString and aBaseURI.
  URLValue(nsStringBuffer* aString, nsIURI* aBaseURI, nsIURI* aReferrer,
           nsIPrincipal* aOriginPrincipal);
  // Construct with the actual URI.
  URLValue(nsIURI* aURI, nsStringBuffer* aString, nsIURI* aReferrer,
           nsIPrincipal* aOriginPrincipal);

  ~URLValue();

  bool operator==(const URLValue& aOther) const;

  // URIEquals only compares URIs and principals (unlike operator==, which
  // also compares the original strings).  URIEquals also assumes that the
  // mURI member of both URL objects is non-null.  Do NOT call this method
  // unless you're sure this is the case.
  bool URIEquals(const URLValue& aOther) const;

  nsIURI* GetURI() const;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // If mURIResolved is false, mURI stores the base URI.
  // If mURIResolved is true, mURI stores the URI we resolve to; this may be
  // null if the URI is invalid.
  mutable nsCOMPtr<nsIURI> mURI;
public:
  nsStringBuffer* mString; // Could use nsRefPtr, but it'd add useless
                           // null-checks; this is never null.
  nsCOMPtr<nsIURI> mReferrer;
  nsCOMPtr<nsIPrincipal> mOriginPrincipal;

  NS_INLINE_DECL_REFCOUNTING(URLValue)

private:
  mutable bool mURIResolved;

  URLValue(const URLValue& aOther) MOZ_DELETE;
  URLValue& operator=(const URLValue& aOther) MOZ_DELETE;
};

struct ImageValue : public URLValue {
  // Not making the constructor and destructor inline because that would
  // force us to include imgIRequest.h, which leads to REQUIRES hell, since
  // this header is included all over.
  // aString must not be null.
  ImageValue(nsIURI* aURI, nsStringBuffer* aString, nsIURI* aReferrer,
             nsIPrincipal* aOriginPrincipal, nsIDocument* aDocument);
  ~ImageValue();

  // Inherit operator== from URLValue

  nsRefPtrHashtable<nsPtrHashKey<nsISupports>, imgRequestProxy> mRequests; 

  // Override AddRef and Release to not only log ourselves correctly, but
  // also so that we delete correctly without a virtual destructor
  NS_INLINE_DECL_REFCOUNTING(ImageValue)
};

struct GridNamedArea {
  nsString mName;
  uint32_t mColumnStart;
  uint32_t mColumnEnd;
  uint32_t mRowStart;
  uint32_t mRowEnd;
};

struct GridTemplateAreasValue MOZ_FINAL {
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

  NS_INLINE_DECL_REFCOUNTING(GridTemplateAreasValue)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // Private destructor to make sure this isn't used as a stack variable
  // or member variable.
  ~GridTemplateAreasValue()
  {
  }

  GridTemplateAreasValue(const GridTemplateAreasValue& aOther) MOZ_DELETE;
  GridTemplateAreasValue&
  operator=(const GridTemplateAreasValue& aOther) MOZ_DELETE;
};

}
}

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
  eCSSUnit_Local_Font   = 15,     // (char16_t*) a local font name
  eCSSUnit_Font_Format  = 16,     // (char16_t*) a font format name
  eCSSUnit_Element      = 17,     // (char16_t*) an element id

  eCSSUnit_Array        = 20,     // (nsCSSValue::Array*) a list of values
  eCSSUnit_Counter      = 21,     // (nsCSSValue::Array*) a counter(string,[string]) value
  eCSSUnit_Counters     = 22,     // (nsCSSValue::Array*) a counters(string,string[,string]) value
  eCSSUnit_Cubic_Bezier = 23,     // (nsCSSValue::Array*) a list of float values
  eCSSUnit_Steps        = 24,     // (nsCSSValue::Array*) a list of (integer, enumerated)
  eCSSUnit_Function     = 25,     // (nsCSSValue::Array*) a function with
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
  eCSSUnit_Calc_Minus   = 32,     // (nsCSSValue::Array*) - within calc
  eCSSUnit_Calc_Times_L = 33,     // (nsCSSValue::Array*) num * val within calc
  eCSSUnit_Calc_Times_R = 34,     // (nsCSSValue::Array*) val * num within calc
  eCSSUnit_Calc_Divided = 35,     // (nsCSSValue::Array*) / within calc

  eCSSUnit_URL          = 40,     // (nsCSSValue::URL*) value
  eCSSUnit_Image        = 41,     // (nsCSSValue::Image*) value
  eCSSUnit_Gradient     = 42,     // (nsCSSValueGradient*) value
  eCSSUnit_TokenStream  = 43,     // (nsCSSValueTokenStream*) value
  eCSSUnit_GridTemplateAreas   = 44,   // (GridTemplateAreasValue*)
                                       // for grid-template-areas

  eCSSUnit_Pair         = 50,     // (nsCSSValuePair*) pair of values
  eCSSUnit_Triplet      = 51,     // (nsCSSValueTriplet*) triplet of values
  eCSSUnit_Rect         = 52,     // (nsCSSRect*) rectangle (four values)
  eCSSUnit_List         = 53,     // (nsCSSValueList*) list of values
  eCSSUnit_ListDep      = 54,     // (nsCSSValueList*) same as List
                                  //   but does not own the list
  eCSSUnit_SharedList   = 55,     // (nsCSSValueSharedList*) same as list
                                  //   but reference counted and shared
  eCSSUnit_PairList     = 56,     // (nsCSSValuePairList*) list of value pairs
  eCSSUnit_PairListDep  = 57,     // (nsCSSValuePairList*) same as PairList
                                  //   but does not own the list

  eCSSUnit_FontFamilyList = 58,   // (FontFamilyList*) value

  eCSSUnit_Integer      = 70,     // (int) simple value
  eCSSUnit_Enumerated   = 71,     // (int) value has enumerated meaning

  eCSSUnit_EnumColor           = 80,   // (int) enumerated color (kColorKTable)
  eCSSUnit_RGBColor            = 81,   // (nscolor) an opaque RGBA value specified as rgb()
  eCSSUnit_RGBAColor           = 82,   // (nscolor) an RGBA value specified as rgba()
  eCSSUnit_HexColor            = 83,   // (nscolor) an opaque RGBA value specified as #rrggbb
  eCSSUnit_ShortHexColor       = 84,   // (nscolor) an opaque RGBA value specified as #rgb
  eCSSUnit_PercentageRGBColor  = 85,   // (nsCSSValueFloatColor*)
  eCSSUnit_PercentageRGBAColor = 86,   // (nsCSSValueFloatColor*)
  eCSSUnit_HSLColor            = 87,   // (nsCSSValueFloatColor*)
  eCSSUnit_HSLAColor           = 88,   // (nsCSSValueFloatColor*)

  eCSSUnit_Percent      = 90,     // (float) 1.0 == 100%) value is percentage of something
  eCSSUnit_Number       = 91,     // (float) value is numeric (usually multiplier, different behavior that percent)

  // Physical length units
  eCSSUnit_PhysicalMillimeter = 200,   // (float) 1/25.4 inch

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
  eCSSUnit_Pixel        = 905,    // (float) CSS pixel unit

  // Angular units
  eCSSUnit_Degree       = 1000,    // (float) 360 per circle
  eCSSUnit_Grad         = 1001,    // (float) 400 per circle
  eCSSUnit_Radian       = 1002,    // (float) 2*pi per circle
  eCSSUnit_Turn         = 1003,    // (float) 1 per circle

  // Frequency units
  eCSSUnit_Hertz        = 2000,    // (float) 1/seconds
  eCSSUnit_Kilohertz    = 2001,    // (float) 1000 Hertz

  // Time units
  eCSSUnit_Seconds      = 3000,    // (float) Standard time
  eCSSUnit_Milliseconds = 3001,    // (float) 1/1000 second

  // Flexible fraction (CSS Grid)
  eCSSUnit_FlexFraction = 4000     // (float) Fraction of free space
};

struct nsCSSValueGradient;
struct nsCSSValuePair;
struct nsCSSValuePair_heap;
struct nsCSSValueTokenStream;
struct nsCSSRect;
struct nsCSSRect_heap;
struct nsCSSValueList;
struct nsCSSValueList_heap;
struct nsCSSValueSharedList;
struct nsCSSValuePairList;
struct nsCSSValuePairList_heap;
struct nsCSSValueTriplet;
struct nsCSSValueTriplet_heap;
class nsCSSValueFloatColor;

class nsCSSValue {
public:
  struct Array;
  friend struct Array;

  friend struct mozilla::css::URLValue;

  friend struct mozilla::css::ImageValue;

  // for valueless units only (null, auto, inherit, none, all, normal)
  explicit nsCSSValue(nsCSSUnit aUnit = eCSSUnit_Null)
    : mUnit(aUnit)
  {
    NS_ABORT_IF_FALSE(aUnit <= eCSSUnit_DummyInherit, "not a valueless unit");
  }

  nsCSSValue(int32_t aValue, nsCSSUnit aUnit);
  nsCSSValue(float aValue, nsCSSUnit aUnit);
  nsCSSValue(const nsString& aValue, nsCSSUnit aUnit);
  nsCSSValue(Array* aArray, nsCSSUnit aUnit);
  explicit nsCSSValue(mozilla::css::URLValue* aValue);
  explicit nsCSSValue(mozilla::css::ImageValue* aValue);
  explicit nsCSSValue(nsCSSValueGradient* aValue);
  explicit nsCSSValue(nsCSSValueTokenStream* aValue);
  explicit nsCSSValue(mozilla::css::GridTemplateAreasValue* aValue);
  explicit nsCSSValue(mozilla::FontFamilyList* aValue);
  nsCSSValue(const nsCSSValue& aCopy);
  ~nsCSSValue() { Reset(); }

  nsCSSValue&  operator=(const nsCSSValue& aCopy);
  bool        operator==(const nsCSSValue& aOther) const;

  bool operator!=(const nsCSSValue& aOther) const
  {
    return !(*this == aOther);
  }

  // Enum for AppendToString's aValueSerialization argument.
  enum Serialization { eNormalized, eAuthorSpecified };

  /**
   * Serialize |this| as a specified value for |aProperty| and append
   * it to |aResult|.
   */
  void AppendToString(nsCSSProperty aProperty, nsAString& aResult,
                      Serialization aValueSerialization) const;

  nsCSSUnit GetUnit() const { return mUnit; }
  bool      IsLengthUnit() const
    { return eCSSUnit_PhysicalMillimeter <= mUnit && mUnit <= eCSSUnit_Pixel; }
  /**
   * A "fixed" length unit is one that means a specific physical length
   * which we try to match based on the physical characteristics of an
   * output device.
   */
  bool      IsFixedLengthUnit() const  
    { return mUnit == eCSSUnit_PhysicalMillimeter; }
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
  bool      IsPixelLengthUnit() const
    { return eCSSUnit_Point <= mUnit && mUnit <= eCSSUnit_Pixel; }
  bool      IsAngularUnit() const  
    { return eCSSUnit_Degree <= mUnit && mUnit <= eCSSUnit_Turn; }
  bool      IsFrequencyUnit() const  
    { return eCSSUnit_Hertz <= mUnit && mUnit <= eCSSUnit_Kilohertz; }
  bool      IsTimeUnit() const  
    { return eCSSUnit_Seconds <= mUnit && mUnit <= eCSSUnit_Milliseconds; }
  bool      IsCalcUnit() const
    { return eCSSUnit_Calc <= mUnit && mUnit <= eCSSUnit_Calc_Divided; }

  bool      UnitHasStringValue() const
    { return eCSSUnit_String <= mUnit && mUnit <= eCSSUnit_Element; }
  bool      UnitHasArrayValue() const
    { return eCSSUnit_Array <= mUnit && mUnit <= eCSSUnit_Calc_Divided; }

  // Checks for the nsCSSValue being of a particular type of color unit:
  //
  //   - IsIntegerColorUnit returns true for:
  //       eCSSUnit_RGBColor             -- rgb(int,int,int)
  //       eCSSUnit_RGBAColor            -- rgba(int,int,int,float)
  //       eCSSUnit_HexColor             -- #rrggbb
  //       eCSSUnit_ShortHexColor        -- #rgb
  //
  //   - IsFLoatColorUnit returns true for:
  //       eCSSUnit_PercentageRGBColor   -- rgb(%,%,%)
  //       eCSSUnit_PercentageRGBAColor  -- rgba(%,%,%,float)
  //       eCSSUnit_HSLColor             -- hsl(float,%,%)
  //       eCSSUnit_HSLAColor            -- hsla(float,%,%,float)
  //
  //   - IsNumericColorUnit returns true for any of the above units.
  //
  // Note that color keywords and system colors are represented by
  // eCSSUnit_EnumColor and eCSSUnit_Ident.
  bool IsIntegerColorUnit() const { return IsIntegerColorUnit(mUnit); }
  bool IsFloatColorUnit() const { return IsFloatColorUnit(mUnit); }
  bool IsNumericColorUnit() const { return IsNumericColorUnit(mUnit); }
  static bool IsIntegerColorUnit(nsCSSUnit aUnit)
  { return eCSSUnit_RGBColor <= aUnit && aUnit <= eCSSUnit_ShortHexColor; }
  static bool IsFloatColorUnit(nsCSSUnit aUnit)
  { return eCSSUnit_PercentageRGBColor <= aUnit &&
           aUnit <= eCSSUnit_HSLAColor; }
  static bool IsNumericColorUnit(nsCSSUnit aUnit)
  { return IsIntegerColorUnit(aUnit) || IsFloatColorUnit(aUnit); }

  int32_t GetIntValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Integer ||
                      mUnit == eCSSUnit_Enumerated ||
                      mUnit == eCSSUnit_EnumColor,
                      "not an int value");
    return mValue.mInt;
  }

  nsCSSKeyword GetKeywordValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Enumerated, "not a keyword value");
    return static_cast<nsCSSKeyword>(mValue.mInt);
  }

  float GetPercentValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Percent, "not a percent value");
    return mValue.mFloat;
  }

  float GetFloatValue() const
  {
    NS_ABORT_IF_FALSE(eCSSUnit_Number <= mUnit, "not a float value");
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
    return mValue.mFloat;
  }

  float GetAngleValue() const
  {
    NS_ABORT_IF_FALSE(eCSSUnit_Degree <= mUnit &&
                 mUnit <= eCSSUnit_Turn, "not an angle value");
    return mValue.mFloat;
  }

  // Converts any angle to radians.
  double GetAngleValueInRadians() const;

  nsAString& GetStringValue(nsAString& aBuffer) const
  {
    NS_ABORT_IF_FALSE(UnitHasStringValue(), "not a string value");
    aBuffer.Truncate();
    uint32_t len = NS_strlen(GetBufferValue(mValue.mString));
    mValue.mString->ToString(len, aBuffer);
    return aBuffer;
  }

  const char16_t* GetStringBufferValue() const
  {
    NS_ABORT_IF_FALSE(UnitHasStringValue(), "not a string value");
    return GetBufferValue(mValue.mString);
  }

  nscolor GetColorValue() const;
  bool IsNonTransparentColor() const;

  Array* GetArrayValue() const
  {
    NS_ABORT_IF_FALSE(UnitHasArrayValue(), "not an array value");
    return mValue.mArray;
  }

  nsIURI* GetURLValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_URL || mUnit == eCSSUnit_Image,
                 "not a URL value");
    return mUnit == eCSSUnit_URL ?
      mValue.mURL->GetURI() : mValue.mImage->GetURI();
  }

  nsCSSValueGradient* GetGradientValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Gradient, "not a gradient value");
    return mValue.mGradient;
  }

  nsCSSValueTokenStream* GetTokenStreamValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_TokenStream, "not a token stream value");
    return mValue.mTokenStream;
  }

  nsCSSValueSharedList* GetSharedListValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_SharedList, "not a shared list value");
    return mValue.mSharedList;
  }

  mozilla::FontFamilyList* GetFontFamilyListValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_FontFamilyList,
                      "not a font family list value");
    NS_ASSERTION(mValue.mFontFamilyList != nullptr,
                 "font family list value should never be null");
    return mValue.mFontFamilyList;
  }

  // bodies of these are below
  inline nsCSSValuePair& GetPairValue();
  inline const nsCSSValuePair& GetPairValue() const;

  inline nsCSSRect& GetRectValue();
  inline const nsCSSRect& GetRectValue() const;

  inline nsCSSValueList* GetListValue();
  inline const nsCSSValueList* GetListValue() const;

  inline nsCSSValuePairList* GetPairListValue();
  inline const nsCSSValuePairList* GetPairListValue() const;

  inline nsCSSValueTriplet& GetTripletValue();
  inline const nsCSSValueTriplet& GetTripletValue() const;


  mozilla::css::URLValue* GetURLStructValue() const
  {
    // Not allowing this for Image values, because if the caller takes
    // a ref to them they won't be able to delete them properly.
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_URL, "not a URL value");
    return mValue.mURL;
  }

  mozilla::css::ImageValue* GetImageStructValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Image, "not an Image value");
    return mValue.mImage;
  }

  mozilla::css::GridTemplateAreasValue* GetGridTemplateAreas() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_GridTemplateAreas,
                      "not a grid-template-areas value");
    return mValue.mGridTemplateAreas;
  }

  const char16_t* GetOriginalURLValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_URL || mUnit == eCSSUnit_Image,
                      "not a URL value");
    return GetBufferValue(mUnit == eCSSUnit_URL ?
                            mValue.mURL->mString :
                            mValue.mImage->mString);
  }

  // Not making this inline because that would force us to include
  // imgIRequest.h, which leads to REQUIRES hell, since this header is included
  // all over.
  imgRequestProxy* GetImageValue(nsIDocument* aDocument) const;

  nscoord GetFixedLength(nsPresContext* aPresContext) const;
  nscoord GetPixelLength() const;

  void Reset()  // sets to null
  {
    if (mUnit != eCSSUnit_Null)
      DoReset();
  }
private:
  void DoReset();

public:
  void SetIntValue(int32_t aValue, nsCSSUnit aUnit);
  void SetPercentValue(float aValue);
  void SetFloatValue(float aValue, nsCSSUnit aUnit);
  void SetStringValue(const nsString& aValue, nsCSSUnit aUnit);
  void SetColorValue(nscolor aValue);
  void SetIntegerColorValue(nscolor aValue, nsCSSUnit aUnit);
  void SetFloatColorValue(float aComponent1,
                          float aComponent2,
                          float aComponent3,
                          float aAlpha, nsCSSUnit aUnit);
  void SetArrayValue(nsCSSValue::Array* aArray, nsCSSUnit aUnit);
  void SetURLValue(mozilla::css::URLValue* aURI);
  void SetImageValue(mozilla::css::ImageValue* aImage);
  void SetGradientValue(nsCSSValueGradient* aGradient);
  void SetTokenStreamValue(nsCSSValueTokenStream* aTokenStream);
  void SetGridTemplateAreas(mozilla::css::GridTemplateAreasValue* aValue);
  void SetFontFamilyListValue(mozilla::FontFamilyList* aFontListValue);
  void SetPairValue(const nsCSSValuePair* aPair);
  void SetPairValue(const nsCSSValue& xValue, const nsCSSValue& yValue);
  void SetSharedListValue(nsCSSValueSharedList* aList);
  void SetDependentListValue(nsCSSValueList* aList);
  void SetDependentPairListValue(nsCSSValuePairList* aList);
  void SetTripletValue(const nsCSSValueTriplet* aTriplet);
  void SetTripletValue(const nsCSSValue& xValue, const nsCSSValue& yValue, const nsCSSValue& zValue);
  void SetAutoValue();
  void SetInheritValue();
  void SetInitialValue();
  void SetUnsetValue();
  void SetNoneValue();
  void SetAllValue();
  void SetNormalValue();
  void SetSystemFontValue();
  void SetDummyValue();
  void SetDummyInheritValue();

  // These are a little different - they allocate storage for you and
  // return a handle.
  nsCSSRect& SetRectValue();
  nsCSSValueList* SetListValue();
  nsCSSValuePairList* SetPairListValue();

  void StartImageLoad(nsIDocument* aDocument) const;  // Only pretend const

  // Initializes as a function value with the specified function id.
  Array* InitFunction(nsCSSKeyword aFunctionId, uint32_t aNumArgs);
  // Checks if this is a function value with the specified function id.
  bool EqualsFunction(nsCSSKeyword aFunctionId) const;

  // Returns an already addrefed buffer.  Guaranteed to return non-null.
  // (Will abort on allocation failure.)
  static already_AddRefed<nsStringBuffer>
    BufferFromString(const nsString& aValue);

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

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
    nsStringBuffer* mString;
    nscolor    mColor;
    Array*     mArray;
    mozilla::css::URLValue* mURL;
    mozilla::css::ImageValue* mImage;
    mozilla::css::GridTemplateAreasValue* mGridTemplateAreas;
    nsCSSValueGradient* mGradient;
    nsCSSValueTokenStream* mTokenStream;
    nsCSSValuePair_heap* mPair;
    nsCSSRect_heap* mRect;
    nsCSSValueTriplet_heap* mTriplet;
    nsCSSValueList_heap* mList;
    nsCSSValueList* mListDependent;
    nsCSSValueSharedList* mSharedList;
    nsCSSValuePairList_heap* mPairList;
    nsCSSValuePairList* mPairListDependent;
    nsCSSValueFloatColor* mFloatColor;
    mozilla::FontFamilyList* mFontFamilyList;
  } mValue;
};

struct nsCSSValue::Array MOZ_FINAL {

  // return |Array| with reference count of zero
  static Array* Create(size_t aItemCount) {
    return new (aItemCount) Array(aItemCount);
  }

  nsCSSValue& operator[](size_t aIndex) {
    NS_ABORT_IF_FALSE(aIndex < mCount, "out of range");
    return mArray[aIndex];
  }

  const nsCSSValue& operator[](size_t aIndex) const {
    NS_ABORT_IF_FALSE(aIndex < mCount, "out of range");
    return mArray[aIndex];
  }

  nsCSSValue& Item(size_t aIndex) { return (*this)[aIndex]; }
  const nsCSSValue& Item(size_t aIndex) const { return (*this)[aIndex]; }

  size_t Count() const { return mCount; }

  bool operator==(const Array& aOther) const
  {
    if (mCount != aOther.mCount)
      return false;
    for (size_t i = 0; i < mCount; ++i)
      if ((*this)[i] != aOther[i])
        return false;
    return true;
  }

  // XXXdholbert This uses a size_t ref count. Should we use a variant
  // of NS_INLINE_DECL_REFCOUNTING that takes a type as an argument?
  void AddRef() {
    if (mRefCnt == size_t(-1)) { // really want SIZE_MAX
      NS_WARNING("refcount overflow, leaking nsCSSValue::Array");
      return;
    }
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "nsCSSValue::Array", sizeof(*this));
  }
  void Release() {
    if (mRefCnt == size_t(-1)) { // really want SIZE_MAX
      NS_WARNING("refcount overflow, leaking nsCSSValue::Array");
      return;
    }
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "nsCSSValue::Array");
    if (mRefCnt == 0)
      delete this;
  }

private:

  size_t mRefCnt;
  const size_t mCount;
  // This must be the last sub-object, since we extend this array to
  // be of size mCount; it needs to be a sub-object so it gets proper
  // alignment.
  nsCSSValue mArray[1];

  void* operator new(size_t aSelfSize, size_t aItemCount) CPP_THROW_NEW {
    NS_ABORT_IF_FALSE(aItemCount > 0, "cannot have a 0 item count");
    return ::operator new(aSelfSize + sizeof(nsCSSValue) * (aItemCount - 1));
  }

  void operator delete(void* aPtr) { ::operator delete(aPtr); }

  nsCSSValue* First() { return mArray; }

  const nsCSSValue* First() const { return mArray; }

#define CSSVALUE_LIST_FOR_EXTRA_VALUES(var)                                   \
  for (nsCSSValue *var = First() + 1, *var##_end = First() + mCount;          \
       var != var##_end; ++var)

  Array(size_t aItemCount)
    : mRefCnt(0)
    , mCount(aItemCount)
  {
    MOZ_COUNT_CTOR(nsCSSValue::Array);
    CSSVALUE_LIST_FOR_EXTRA_VALUES(val) {
      new (val) nsCSSValue();
    }
  }

  ~Array()
  {
    MOZ_COUNT_DTOR(nsCSSValue::Array);
    CSSVALUE_LIST_FOR_EXTRA_VALUES(val) {
      val->~nsCSSValue();
    }
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

#undef CSSVALUE_LIST_FOR_EXTRA_VALUES

private:
  Array(const Array& aOther) MOZ_DELETE;
  Array& operator=(const Array& aOther) MOZ_DELETE;
};

// Prefer nsCSSValue::Array for lists of fixed size.
struct nsCSSValueList {
  nsCSSValueList() : mNext(nullptr) { MOZ_COUNT_CTOR(nsCSSValueList); }
  ~nsCSSValueList();

  nsCSSValueList* Clone() const;  // makes a deep copy
  void CloneInto(nsCSSValueList* aList) const; // makes a deep copy into aList
  void AppendToString(nsCSSProperty aProperty, nsAString& aResult,
                      nsCSSValue::Serialization aValueSerialization) const;

  bool operator==(nsCSSValueList const& aOther) const;
  bool operator!=(const nsCSSValueList& aOther) const
  { return !(*this == aOther); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCSSValue      mValue;
  nsCSSValueList* mNext;

private:
  nsCSSValueList(const nsCSSValueList& aCopy) // makes a shallow copy
    : mValue(aCopy.mValue), mNext(nullptr)
  {
    MOZ_COUNT_CTOR(nsCSSValueList);
  }
};

// nsCSSValueList_heap differs from nsCSSValueList only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSValueList_heap MOZ_FINAL : public nsCSSValueList {
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
struct nsCSSValueSharedList MOZ_FINAL {
  nsCSSValueSharedList()
    : mHead(nullptr)
  {
    MOZ_COUNT_CTOR(nsCSSValueSharedList);
  }

  // Takes ownership of aList.
  nsCSSValueSharedList(nsCSSValueList* aList)
    : mHead(aList)
  {
    MOZ_COUNT_CTOR(nsCSSValueSharedList);
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsCSSValueSharedList();

public:
  NS_INLINE_DECL_REFCOUNTING(nsCSSValueSharedList)

  void AppendToString(nsCSSProperty aProperty, nsAString& aResult,
                      nsCSSValue::Serialization aValueSerialization) const;

  bool operator==(nsCSSValueSharedList const& aOther) const;
  bool operator!=(const nsCSSValueSharedList& aOther) const
  { return !(*this == aOther); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCSSValueList* mHead;
};

// This has to be here so that the relationship between nsCSSValueList
// and nsCSSValueList_heap is visible.
inline nsCSSValueList*
nsCSSValue::GetListValue()
{
  if (mUnit == eCSSUnit_List)
    return mValue.mList;
  else {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_ListDep, "not a pairlist value");
    return mValue.mListDependent;
  }
}

inline const nsCSSValueList*
nsCSSValue::GetListValue() const
{
  if (mUnit == eCSSUnit_List)
    return mValue.mList;
  else {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_ListDep, "not a pairlist value");
    return mValue.mListDependent;
  }
}

struct nsCSSRect {
  nsCSSRect(void);
  nsCSSRect(const nsCSSRect& aCopy);
  ~nsCSSRect();

  void AppendToString(nsCSSProperty aProperty, nsAString& aResult,
                      nsCSSValue::Serialization aValueSerialization) const;

  bool operator==(const nsCSSRect& aOther) const {
    return mTop == aOther.mTop &&
           mRight == aOther.mRight &&
           mBottom == aOther.mBottom &&
           mLeft == aOther.mLeft;
  }

  bool operator!=(const nsCSSRect& aOther) const {
    return mTop != aOther.mTop ||
           mRight != aOther.mRight ||
           mBottom != aOther.mBottom ||
           mLeft != aOther.mLeft;
  }

  void SetAllSidesTo(const nsCSSValue& aValue);

  bool AllSidesEqualTo(const nsCSSValue& aValue) const {
    return mTop == aValue &&
           mRight == aValue &&
           mBottom == aValue &&
           mLeft == aValue;
  }

  void Reset() {
    mTop.Reset();
    mRight.Reset();
    mBottom.Reset();
    mLeft.Reset();
  }

  bool HasValue() const {
    return
      mTop.GetUnit() != eCSSUnit_Null ||
      mRight.GetUnit() != eCSSUnit_Null ||
      mBottom.GetUnit() != eCSSUnit_Null ||
      mLeft.GetUnit() != eCSSUnit_Null;
  }

  nsCSSValue mTop;
  nsCSSValue mRight;
  nsCSSValue mBottom;
  nsCSSValue mLeft;

  typedef nsCSSValue nsCSSRect::*side_type;
  static const side_type sides[4];
};

// nsCSSRect_heap differs from nsCSSRect only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSRect_heap MOZ_FINAL : public nsCSSRect {
  NS_INLINE_DECL_REFCOUNTING(nsCSSRect_heap)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsCSSRect_heap()
  {
  }
};

// This has to be here so that the relationship between nsCSSRect
// and nsCSSRect_heap is visible.
inline nsCSSRect&
nsCSSValue::GetRectValue()
{
  NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Rect, "not a rect value");
  return *mValue.mRect;
}

inline const nsCSSRect&
nsCSSValue::GetRectValue() const
{
  NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Rect, "not a rect value");
  return *mValue.mRect;
}

struct nsCSSValuePair {
  nsCSSValuePair()
  {
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  nsCSSValuePair(nsCSSUnit aUnit)
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

  void AppendToString(nsCSSProperty aProperty, nsAString& aResult,
                      nsCSSValue::Serialization aValueSerialization) const;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCSSValue mXValue;
  nsCSSValue mYValue;
};

// nsCSSValuePair_heap differs from nsCSSValuePair only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSValuePair_heap MOZ_FINAL : public nsCSSValuePair {
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

struct nsCSSValueTriplet {
    nsCSSValueTriplet()
    {
        MOZ_COUNT_CTOR(nsCSSValueTriplet);
    }
    nsCSSValueTriplet(nsCSSUnit aUnit)
        : mXValue(aUnit), mYValue(aUnit), mZValue(aUnit)
    {
        MOZ_COUNT_CTOR(nsCSSValueTriplet);
    }
    nsCSSValueTriplet(const nsCSSValue& aXValue, 
                      const nsCSSValue& aYValue, 
                      const nsCSSValue& aZValue)
        : mXValue(aXValue), mYValue(aYValue), mZValue(aZValue)
    {
        MOZ_COUNT_CTOR(nsCSSValueTriplet);
    }
    nsCSSValueTriplet(const nsCSSValueTriplet& aCopy)
        : mXValue(aCopy.mXValue), mYValue(aCopy.mYValue), mZValue(aCopy.mZValue)
    {
        MOZ_COUNT_CTOR(nsCSSValueTriplet);
    }
    ~nsCSSValueTriplet()
    {
        MOZ_COUNT_DTOR(nsCSSValueTriplet);
    }

    bool operator==(const nsCSSValueTriplet& aOther) const {
        return mXValue == aOther.mXValue &&
               mYValue == aOther.mYValue &&
               mZValue == aOther.mZValue;
    }

    bool operator!=(const nsCSSValueTriplet& aOther) const {
        return mXValue != aOther.mXValue ||
               mYValue != aOther.mYValue ||
               mZValue != aOther.mZValue;
    }

    bool AllValuesEqualTo(const nsCSSValue& aValue) const {
        return mXValue == aValue &&
               mYValue == aValue &&
               mZValue == aValue;
    }

    void SetAllValuesTo(const nsCSSValue& aValue) {
        mXValue = aValue;
        mYValue = aValue;
        mZValue = aValue;
    }

    void Reset() {
        mXValue.Reset();
        mYValue.Reset();
        mZValue.Reset();
    }

    bool HasValue() const {
        return mXValue.GetUnit() != eCSSUnit_Null ||
               mYValue.GetUnit() != eCSSUnit_Null ||
               mZValue.GetUnit() != eCSSUnit_Null;
    }

    void AppendToString(nsCSSProperty aProperty, nsAString& aResult,
                        nsCSSValue::Serialization aValueSerialization) const;

    nsCSSValue mXValue;
    nsCSSValue mYValue;
    nsCSSValue mZValue;
};

// nsCSSValueTriplet_heap differs from nsCSSValueTriplet only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSValueTriplet_heap MOZ_FINAL : public nsCSSValueTriplet {
  // forward constructor
  nsCSSValueTriplet_heap(const nsCSSValue& aXValue, const nsCSSValue& aYValue, const nsCSSValue& aZValue)
    : nsCSSValueTriplet(aXValue, aYValue, aZValue)
  {}

  NS_INLINE_DECL_REFCOUNTING(nsCSSValueTriplet_heap)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsCSSValueTriplet_heap()
  {
  }
};

// This has to be here so that the relationship between nsCSSValuePair
// and nsCSSValuePair_heap is visible.
inline nsCSSValuePair&
nsCSSValue::GetPairValue()
{
  NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Pair, "not a pair value");
  return *mValue.mPair;
}

inline const nsCSSValuePair&
nsCSSValue::GetPairValue() const
{
  NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Pair, "not a pair value");
  return *mValue.mPair;
}

inline nsCSSValueTriplet&
nsCSSValue::GetTripletValue()
{
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Triplet, "not a triplet value");
    return *mValue.mTriplet;
}

inline const nsCSSValueTriplet&
nsCSSValue::GetTripletValue() const
{
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Triplet, "not a triplet value");
    return *mValue.mTriplet;
}

// Maybe should be replaced with nsCSSValueList and nsCSSValue::Array?
struct nsCSSValuePairList {
  nsCSSValuePairList() : mNext(nullptr) { MOZ_COUNT_CTOR(nsCSSValuePairList); }
  ~nsCSSValuePairList();

  nsCSSValuePairList* Clone() const; // makes a deep copy
  void AppendToString(nsCSSProperty aProperty, nsAString& aResult,
                      nsCSSValue::Serialization aValueSerialization) const;

  bool operator==(const nsCSSValuePairList& aOther) const;
  bool operator!=(const nsCSSValuePairList& aOther) const
  { return !(*this == aOther); }

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
};

// nsCSSValuePairList_heap differs from nsCSSValuePairList only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSValuePairList_heap MOZ_FINAL : public nsCSSValuePairList {
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
  if (mUnit == eCSSUnit_PairList)
    return mValue.mPairList;
  else {
    NS_ABORT_IF_FALSE (mUnit == eCSSUnit_PairListDep, "not a pairlist value");
    return mValue.mPairListDependent;
  }
}

inline const nsCSSValuePairList*
nsCSSValue::GetPairListValue() const
{
  if (mUnit == eCSSUnit_PairList)
    return mValue.mPairList;
  else {
    NS_ABORT_IF_FALSE (mUnit == eCSSUnit_PairListDep, "not a pairlist value");
    return mValue.mPairListDependent;
  }
}

struct nsCSSValueGradientStop {
public:
  nsCSSValueGradientStop();
  // needed to keep bloat logs happy when we use the TArray
  // in nsCSSValueGradient
  nsCSSValueGradientStop(const nsCSSValueGradientStop& aOther);
  ~nsCSSValueGradientStop();

  nsCSSValue mLocation;
  nsCSSValue mColor;

  bool operator==(const nsCSSValueGradientStop& aOther) const
  {
    return (mLocation == aOther.mLocation &&
            mColor == aOther.mColor);
  }

  bool operator!=(const nsCSSValueGradientStop& aOther) const
  {
    return !(*this == aOther);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
};

struct nsCSSValueGradient MOZ_FINAL {
  nsCSSValueGradient(bool aIsRadial, bool aIsRepeating);

  // true if gradient is radial, false if it is linear
  bool mIsRadial;
  bool mIsRepeating;
  bool mIsLegacySyntax;
  bool mIsExplicitSize;
  // line position and angle
  nsCSSValuePair mBgPos;
  nsCSSValue mAngle;

  // Only meaningful if mIsRadial is true
private:
  nsCSSValue mRadialValues[2];
public:
  nsCSSValue& GetRadialShape()
  {
    MOZ_ASSERT(!mIsExplicitSize);
    return mRadialValues[0];
  }
  const nsCSSValue& GetRadialShape() const
  {
    MOZ_ASSERT(!mIsExplicitSize);
    return mRadialValues[0];
  }
  nsCSSValue& GetRadialSize()
  {
    MOZ_ASSERT(!mIsExplicitSize);
    return mRadialValues[1];
  }
  const nsCSSValue& GetRadialSize() const
  {
    MOZ_ASSERT(!mIsExplicitSize);
    return mRadialValues[1];
  }
  nsCSSValue& GetRadiusX()
  {
    MOZ_ASSERT(mIsExplicitSize);
    return mRadialValues[0];
  }
  const nsCSSValue& GetRadiusX() const
  {
    MOZ_ASSERT(mIsExplicitSize);
    return mRadialValues[0];
  }
  nsCSSValue& GetRadiusY()
  {
    MOZ_ASSERT(mIsExplicitSize);
    return mRadialValues[1];
  }
  const nsCSSValue& GetRadiusY() const
  {
    MOZ_ASSERT(mIsExplicitSize);
    return mRadialValues[1];
  }

  InfallibleTArray<nsCSSValueGradientStop> mStops;

  bool operator==(const nsCSSValueGradient& aOther) const
  {
    if (mIsRadial != aOther.mIsRadial ||
        mIsRepeating != aOther.mIsRepeating ||
        mIsLegacySyntax != aOther.mIsLegacySyntax ||
        mIsExplicitSize != aOther.mIsExplicitSize ||
        mBgPos != aOther.mBgPos ||
        mAngle != aOther.mAngle ||
        mRadialValues[0] != aOther.mRadialValues[0] ||
        mRadialValues[1] != aOther.mRadialValues[1])
      return false;

    if (mStops.Length() != aOther.mStops.Length())
      return false;

    for (uint32_t i = 0; i < mStops.Length(); i++) {
      if (mStops[i] != aOther.mStops[i])
        return false;
    }

    return true;
  }

  bool operator!=(const nsCSSValueGradient& aOther) const
  {
    return !(*this == aOther);
  }

  NS_INLINE_DECL_REFCOUNTING(nsCSSValueGradient)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsCSSValueGradient()
  {
  }

  nsCSSValueGradient(const nsCSSValueGradient& aOther) MOZ_DELETE;
  nsCSSValueGradient& operator=(const nsCSSValueGradient& aOther) MOZ_DELETE;
};

struct nsCSSValueTokenStream MOZ_FINAL {
  nsCSSValueTokenStream();

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsCSSValueTokenStream();

public:
  bool operator==(const nsCSSValueTokenStream& aOther) const
  {
    bool eq;
    return mPropertyID == aOther.mPropertyID &&
           mShorthandPropertyID == aOther.mShorthandPropertyID &&
           mTokenStream.Equals(aOther.mTokenStream) &&
           (mBaseURI == aOther.mBaseURI ||
            (mBaseURI && aOther.mBaseURI &&
             NS_SUCCEEDED(mBaseURI->Equals(aOther.mBaseURI, &eq)) &&
             eq)) &&
           (mSheetURI == aOther.mSheetURI ||
            (mSheetURI && aOther.mSheetURI &&
             NS_SUCCEEDED(mSheetURI->Equals(aOther.mSheetURI, &eq)) &&
             eq)) &&
           (mSheetPrincipal == aOther.mSheetPrincipal ||
            (mSheetPrincipal && aOther.mSheetPrincipal &&
             NS_SUCCEEDED(mSheetPrincipal->Equals(aOther.mSheetPrincipal,
                                                  &eq)) &&
             eq));
  }

  bool operator!=(const nsCSSValueTokenStream& aOther) const
  {
    return !(*this == aOther);
  }

  NS_INLINE_DECL_REFCOUNTING(nsCSSValueTokenStream)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // The property that has mTokenStream as its unparsed specified value.
  // When a variable reference is used in a shorthand property, a
  // TokenStream value is stored as the specified value for each of its
  // component longhand properties.
  nsCSSProperty mPropertyID;

  // The shorthand property that had a value with a variable reference,
  // which caused the longhand property identified by mPropertyID to have
  // a TokenStream value.
  nsCSSProperty mShorthandPropertyID;

  // The unparsed CSS corresponding to the specified value of the property.
  // When the value of a shorthand property has a variable reference, the
  // same mTokenStream value is used on each of the nsCSSValueTokenStream
  // objects that will be set by parsing the shorthand.
  nsString mTokenStream;

  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIURI> mSheetURI;
  nsCOMPtr<nsIPrincipal> mSheetPrincipal;
  nsCSSStyleSheet* mSheet;
  uint32_t mLineNumber;
  uint32_t mLineOffset;

  // This table is used to hold a reference on to any ImageValue that results
  // from re-parsing this token stream at computed value time.  When properties
  // like background-image contain a normal url(), the Declaration's data block
  // will hold a reference to the ImageValue.  When a token stream is used,
  // the Declaration only holds on to this nsCSSValueTokenStream object, and
  // the ImageValue would only exist for the duration of
  // nsRuleNode::WalkRuleTree, in the AutoCSSValueArray.  So instead when
  // we re-parse a token stream and get an ImageValue, we record it in this
  // table so that the Declaration can be the object that keeps holding
  // a reference to it.
  nsTHashtable<nsRefPtrHashKey<mozilla::css::ImageValue> > mImageValues;

private:
  nsCSSValueTokenStream(const nsCSSValueTokenStream& aOther) MOZ_DELETE;
  nsCSSValueTokenStream& operator=(const nsCSSValueTokenStream& aOther) MOZ_DELETE;
};

class nsCSSValueFloatColor MOZ_FINAL {
public:
  nsCSSValueFloatColor(float aComponent1, float aComponent2, float aComponent3,
                       float aAlpha)
    : mComponent1(aComponent1)
    , mComponent2(aComponent2)
    , mComponent3(aComponent3)
    , mAlpha(aAlpha)
  {
    MOZ_COUNT_CTOR(nsCSSValueFloatColor);
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsCSSValueFloatColor()
  {
    MOZ_COUNT_DTOR(nsCSSValueFloatColor);
  }

public:
  bool operator==(nsCSSValueFloatColor& aOther) const;

  nscolor GetColorValue(nsCSSUnit aUnit) const;
  bool IsNonTransparentColor() const;

  void AppendToString(nsCSSUnit aUnit, nsAString& aResult) const;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  NS_INLINE_DECL_REFCOUNTING(nsCSSValueFloatColor)

private:
  // FIXME: We should not be clamping specified RGB color components.
  float mComponent1;  // 0..1 for RGB, 0..360 for HSL
  float mComponent2;  // 0..1
  float mComponent3;  // 0..1
  float mAlpha;       // 0..1

  nsCSSValueFloatColor(const nsCSSValueFloatColor& aOther) MOZ_DELETE;
  nsCSSValueFloatColor& operator=(const nsCSSValueFloatColor& aOther)
                                                                   MOZ_DELETE;
};

struct nsCSSCornerSizes {
  nsCSSCornerSizes(void);
  nsCSSCornerSizes(const nsCSSCornerSizes& aCopy);
  ~nsCSSCornerSizes();

  // argument is a "full corner" constant from nsStyleConsts.h
  nsCSSValue const & GetCorner(uint32_t aCorner) const {
    return this->*corners[aCorner];
  }
  nsCSSValue & GetCorner(uint32_t aCorner) {
    return this->*corners[aCorner];
  }

  bool operator==(const nsCSSCornerSizes& aOther) const {
    NS_FOR_CSS_FULL_CORNERS(corner) {
      if (this->GetCorner(corner) != aOther.GetCorner(corner))
        return false;
    }
    return true;
  }

  bool operator!=(const nsCSSCornerSizes& aOther) const {
    NS_FOR_CSS_FULL_CORNERS(corner) {
      if (this->GetCorner(corner) != aOther.GetCorner(corner))
        return true;
    }
    return false;
  }

  bool HasValue() const {
    NS_FOR_CSS_FULL_CORNERS(corner) {
      if (this->GetCorner(corner).GetUnit() != eCSSUnit_Null)
        return true;
    }
    return false;
  }

  void Reset();

  nsCSSValue mTopLeft;
  nsCSSValue mTopRight;
  nsCSSValue mBottomRight;
  nsCSSValue mBottomLeft;

protected:
  typedef nsCSSValue nsCSSCornerSizes::*corner_type;
  static const corner_type corners[4];
};

#endif /* nsCSSValue_h___ */

