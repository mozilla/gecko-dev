/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The long avoided variant support for xpcom. */

#include "nsVariant.h"
#include "prprf.h"
#include "prdtoa.h"
#include <math.h>
#include "nsCycleCollectionParticipant.h"
#include "xpt_struct.h"
#include "nsReadableUtils.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsCRTGlue.h"

/***************************************************************************/
// Helpers for static convert functions...

static nsresult
String2Double(const char* aString, double* aResult)
{
  char* next;
  double value = PR_strtod(aString, &next);
  if (next == aString) {
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }
  *aResult = value;
  return NS_OK;
}

static nsresult
AString2Double(const nsAString& aString, double* aResult)
{
  char* pChars = ToNewCString(aString);
  if (!pChars) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = String2Double(pChars, aResult);
  free(pChars);
  return rv;
}

static nsresult
AUTF8String2Double(const nsAUTF8String& aString, double* aResult)
{
  return String2Double(PromiseFlatUTF8String(aString).get(), aResult);
}

static nsresult
ACString2Double(const nsACString& aString, double* aResult)
{
  return String2Double(PromiseFlatCString(aString).get(), aResult);
}

// Fills outVariant with double, uint32_t, or int32_t.
// Returns NS_OK, an error code, or a non-NS_OK success code
static nsresult
ToManageableNumber(const nsDiscriminatedUnion& aInData,
                   nsDiscriminatedUnion* aOutData)
{
  nsresult rv;

  switch (aInData.mType) {
    // This group results in a int32_t...

#define CASE__NUMBER_INT32(type_, member_)                                    \
    case nsIDataType :: type_ :                                               \
        aOutData->u.mInt32Value = aInData.u. member_ ;                        \
        aOutData->mType = nsIDataType::VTYPE_INT32;                           \
        return NS_OK;

    CASE__NUMBER_INT32(VTYPE_INT8,   mInt8Value)
    CASE__NUMBER_INT32(VTYPE_INT16,  mInt16Value)
    CASE__NUMBER_INT32(VTYPE_INT32,  mInt32Value)
    CASE__NUMBER_INT32(VTYPE_UINT8,  mUint8Value)
    CASE__NUMBER_INT32(VTYPE_UINT16, mUint16Value)
    CASE__NUMBER_INT32(VTYPE_BOOL,   mBoolValue)
    CASE__NUMBER_INT32(VTYPE_CHAR,   mCharValue)
    CASE__NUMBER_INT32(VTYPE_WCHAR,  mWCharValue)

#undef CASE__NUMBER_INT32

    // This group results in a uint32_t...

    case nsIDataType::VTYPE_UINT32:
      aOutData->u.mInt32Value = aInData.u.mUint32Value;
      aOutData->mType = nsIDataType::VTYPE_INT32;
      return NS_OK;

    // This group results in a double...

    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT64:
      // XXX Need boundary checking here.
      // We may need to return NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA
      aOutData->u.mDoubleValue = double(aInData.u.mInt64Value);
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_FLOAT:
      aOutData->u.mDoubleValue = aInData.u.mFloatValue;
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_DOUBLE:
      aOutData->u.mDoubleValue = aInData.u.mDoubleValue;
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      rv = String2Double(aInData.u.str.mStringValue, &aOutData->u.mDoubleValue);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_ASTRING:
      rv = AString2Double(*aInData.u.mAStringValue, &aOutData->u.mDoubleValue);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_UTF8STRING:
      rv = AUTF8String2Double(*aInData.u.mUTF8StringValue,
                              &aOutData->u.mDoubleValue);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_CSTRING:
      rv = ACString2Double(*aInData.u.mCStringValue,
                           &aOutData->u.mDoubleValue);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      rv = AString2Double(nsDependentString(aInData.u.wstr.mWStringValue),
                          &aOutData->u.mDoubleValue);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;

    // This group fails...

    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ID:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
    default:
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }
}

/***************************************************************************/
// Array helpers...

static void
FreeArray(nsDiscriminatedUnion* aData)
{
  NS_ASSERTION(aData->mType == nsIDataType::VTYPE_ARRAY, "bad FreeArray call");
  NS_ASSERTION(aData->u.array.mArrayValue, "bad array");
  NS_ASSERTION(aData->u.array.mArrayCount, "bad array count");

#define CASE__FREE_ARRAY_PTR(type_, ctype_)                                   \
        case nsIDataType:: type_ :                                            \
        {                                                                     \
            ctype_ ** p = (ctype_ **) aData->u.array.mArrayValue;             \
            for(uint32_t i = aData->u.array.mArrayCount; i > 0; p++, i--)     \
                if(*p)                                                        \
                    free((char*)*p);                                \
            break;                                                            \
        }

#define CASE__FREE_ARRAY_IFACE(type_, ctype_)                                 \
        case nsIDataType:: type_ :                                            \
        {                                                                     \
            ctype_ ** p = (ctype_ **) aData->u.array.mArrayValue;             \
            for(uint32_t i = aData->u.array.mArrayCount; i > 0; p++, i--)     \
                if(*p)                                                        \
                    (*p)->Release();                                          \
            break;                                                            \
        }

  switch (aData->u.array.mArrayType) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_UINT64:
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE:
    case nsIDataType::VTYPE_BOOL:
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_WCHAR:
      break;

    // XXX We ASSUME that "array of nsID" means "array of pointers to nsID".
    CASE__FREE_ARRAY_PTR(VTYPE_ID, nsID)
    CASE__FREE_ARRAY_PTR(VTYPE_CHAR_STR, char)
    CASE__FREE_ARRAY_PTR(VTYPE_WCHAR_STR, char16_t)
    CASE__FREE_ARRAY_IFACE(VTYPE_INTERFACE, nsISupports)
    CASE__FREE_ARRAY_IFACE(VTYPE_INTERFACE_IS, nsISupports)

    // The rest are illegal.
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
    default:
      NS_ERROR("bad type in array!");
      break;
  }

  // Free the array memory.
  free((char*)aData->u.array.mArrayValue);

#undef CASE__FREE_ARRAY_PTR
#undef CASE__FREE_ARRAY_IFACE
}

static nsresult
CloneArray(uint16_t aInType, const nsIID* aInIID,
           uint32_t aInCount, void* aInValue,
           uint16_t* aOutType, nsIID* aOutIID,
           uint32_t* aOutCount, void** aOutValue)
{
  NS_ASSERTION(aInCount, "bad param");
  NS_ASSERTION(aInValue, "bad param");
  NS_ASSERTION(aOutType, "bad param");
  NS_ASSERTION(aOutCount, "bad param");
  NS_ASSERTION(aOutValue, "bad param");

  uint32_t allocatedValueCount = 0;
  nsresult rv = NS_OK;
  uint32_t i;

  // First we figure out the size of the elements for the new u.array.

  size_t elementSize;
  size_t allocSize;

  switch (aInType) {
    case nsIDataType::VTYPE_INT8:
      elementSize = sizeof(int8_t);
      break;
    case nsIDataType::VTYPE_INT16:
      elementSize = sizeof(int16_t);
      break;
    case nsIDataType::VTYPE_INT32:
      elementSize = sizeof(int32_t);
      break;
    case nsIDataType::VTYPE_INT64:
      elementSize = sizeof(int64_t);
      break;
    case nsIDataType::VTYPE_UINT8:
      elementSize = sizeof(uint8_t);
      break;
    case nsIDataType::VTYPE_UINT16:
      elementSize = sizeof(uint16_t);
      break;
    case nsIDataType::VTYPE_UINT32:
      elementSize = sizeof(uint32_t);
      break;
    case nsIDataType::VTYPE_UINT64:
      elementSize = sizeof(uint64_t);
      break;
    case nsIDataType::VTYPE_FLOAT:
      elementSize = sizeof(float);
      break;
    case nsIDataType::VTYPE_DOUBLE:
      elementSize = sizeof(double);
      break;
    case nsIDataType::VTYPE_BOOL:
      elementSize = sizeof(bool);
      break;
    case nsIDataType::VTYPE_CHAR:
      elementSize = sizeof(char);
      break;
    case nsIDataType::VTYPE_WCHAR:
      elementSize = sizeof(char16_t);
      break;

    // XXX We ASSUME that "array of nsID" means "array of pointers to nsID".
    case nsIDataType::VTYPE_ID:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      elementSize = sizeof(void*);
      break;

    // The rest are illegal.
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
    default:
      NS_ERROR("bad type in array!");
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }


  // Alloc the u.array.

  allocSize = aInCount * elementSize;
  *aOutValue = moz_xmalloc(allocSize);
  if (!*aOutValue) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Clone the elements.

  switch (aInType) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_UINT64:
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE:
    case nsIDataType::VTYPE_BOOL:
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_WCHAR:
      memcpy(*aOutValue, aInValue, allocSize);
      break;

    case nsIDataType::VTYPE_INTERFACE_IS:
      if (aOutIID) {
        *aOutIID = *aInIID;
      }
    // fall through...
    case nsIDataType::VTYPE_INTERFACE: {
      memcpy(*aOutValue, aInValue, allocSize);

      nsISupports** p = (nsISupports**)*aOutValue;
      for (i = aInCount; i > 0; ++p, --i)
        if (*p) {
          (*p)->AddRef();
        }
      break;
    }

    // XXX We ASSUME that "array of nsID" means "array of pointers to nsID".
    case nsIDataType::VTYPE_ID: {
      nsID** inp  = (nsID**)aInValue;
      nsID** outp = (nsID**)*aOutValue;
      for (i = aInCount; i > 0; --i) {
        nsID* idp = *(inp++);
        if (idp) {
          if (!(*(outp++) = (nsID*)nsMemory::Clone((char*)idp, sizeof(nsID)))) {
            goto bad;
          }
        } else {
          *(outp++) = nullptr;
        }
        allocatedValueCount++;
      }
      break;
    }

    case nsIDataType::VTYPE_CHAR_STR: {
      char** inp  = (char**)aInValue;
      char** outp = (char**)*aOutValue;
      for (i = aInCount; i > 0; i--) {
        char* str = *(inp++);
        if (str) {
          if (!(*(outp++) = (char*)nsMemory::Clone(
                              str, (strlen(str) + 1) * sizeof(char)))) {
            goto bad;
          }
        } else {
          *(outp++) = nullptr;
        }
        allocatedValueCount++;
      }
      break;
    }

    case nsIDataType::VTYPE_WCHAR_STR: {
      char16_t** inp  = (char16_t**)aInValue;
      char16_t** outp = (char16_t**)*aOutValue;
      for (i = aInCount; i > 0; i--) {
        char16_t* str = *(inp++);
        if (str) {
          if (!(*(outp++) = (char16_t*)nsMemory::Clone(
                              str, (NS_strlen(str) + 1) * sizeof(char16_t)))) {
            goto bad;
          }
        } else {
          *(outp++) = nullptr;
        }
        allocatedValueCount++;
      }
      break;
    }

    // The rest are illegal.
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    default:
      NS_ERROR("bad type in array!");
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  *aOutType = aInType;
  *aOutCount = aInCount;
  return NS_OK;

bad:
  if (*aOutValue) {
    char** p = (char**)*aOutValue;
    for (i = allocatedValueCount; i > 0; ++p, --i)
      if (*p) {
        free(*p);
      }
    free((char*)*aOutValue);
    *aOutValue = nullptr;
  }
  return rv;
}

/***************************************************************************/

#define TRIVIAL_DATA_CONVERTER(type_, data_, member_, retval_)                \
    if(data_.mType == nsIDataType :: type_) {                                 \
        *retval_ = data_.u.member_;                                           \
        return NS_OK;                                                         \
    }

#define NUMERIC_CONVERSION_METHOD_BEGIN(type_, Ctype_, name_)                 \
/* static */ nsresult                                                         \
nsVariant::ConvertTo##name_ (const nsDiscriminatedUnion& data,                \
                             Ctype_ *_retval)                                 \
{                                                                             \
    TRIVIAL_DATA_CONVERTER(type_, data, m##name_##Value, _retval)             \
    nsDiscriminatedUnion tempData;                                            \
    nsVariant::Initialize(&tempData);                                         \
    nsresult rv = ToManageableNumber(data, &tempData);                        \
    /*                                                                     */ \
    /* NOTE: rv may indicate a success code that we want to preserve       */ \
    /* For the final return. So all the return cases below should return   */ \
    /* this rv when indicating success.                                    */ \
    /*                                                                     */ \
    if(NS_FAILED(rv))                                                         \
        return rv;                                                            \
    switch(tempData.mType)                                                    \
    {

#define CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(Ctype_)                      \
    case nsIDataType::VTYPE_INT32:                                            \
        *_retval = ( Ctype_ ) tempData.u.mInt32Value;                         \
        return rv;

#define CASE__NUMERIC_CONVERSION_INT32_MIN_MAX(Ctype_, min_, max_)            \
    case nsIDataType::VTYPE_INT32:                                            \
    {                                                                         \
        int32_t value = tempData.u.mInt32Value;                               \
        if(value < min_ || value > max_)                                      \
            return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                         \
        *_retval = ( Ctype_ ) value;                                          \
        return rv;                                                            \
    }

#define CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(Ctype_)                     \
    case nsIDataType::VTYPE_UINT32:                                           \
        *_retval = ( Ctype_ ) tempData.u.mUint32Value;                        \
        return rv;

#define CASE__NUMERIC_CONVERSION_UINT32_MAX(Ctype_, max_)                     \
    case nsIDataType::VTYPE_UINT32:                                           \
    {                                                                         \
        uint32_t value = tempData.u.mUint32Value;                             \
        if(value > max_)                                                      \
            return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                         \
        *_retval = ( Ctype_ ) value;                                          \
        return rv;                                                            \
    }

#define CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(Ctype_)                     \
    case nsIDataType::VTYPE_DOUBLE:                                           \
        *_retval = ( Ctype_ ) tempData.u.mDoubleValue;                        \
        return rv;

#define CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX(Ctype_, min_, max_)           \
    case nsIDataType::VTYPE_DOUBLE:                                           \
    {                                                                         \
        double value = tempData.u.mDoubleValue;                               \
        if(value < min_ || value > max_)                                      \
            return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                         \
        *_retval = ( Ctype_ ) value;                                          \
        return rv;                                                            \
    }

#define CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(Ctype_, min_, max_)       \
    case nsIDataType::VTYPE_DOUBLE:                                           \
    {                                                                         \
        double value = tempData.u.mDoubleValue;                               \
        if(value < min_ || value > max_)                                      \
            return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                         \
        *_retval = ( Ctype_ ) value;                                          \
        return (0.0 == fmod(value,1.0)) ?                                     \
            rv : NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA;                       \
    }

#define CASES__NUMERIC_CONVERSION_NORMAL(Ctype_, min_, max_)                  \
    CASE__NUMERIC_CONVERSION_INT32_MIN_MAX(Ctype_, min_, max_)                \
    CASE__NUMERIC_CONVERSION_UINT32_MAX(Ctype_, max_)                         \
    CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(Ctype_, min_, max_)

#define NUMERIC_CONVERSION_METHOD_END                                         \
    default:                                                                  \
        NS_ERROR("bad type returned from ToManageableNumber");                \
        return NS_ERROR_CANNOT_CONVERT_DATA;                                  \
    }                                                                         \
}

#define NUMERIC_CONVERSION_METHOD_NORMAL(type_, Ctype_, name_, min_, max_)    \
    NUMERIC_CONVERSION_METHOD_BEGIN(type_, Ctype_, name_)                     \
        CASES__NUMERIC_CONVERSION_NORMAL(Ctype_, min_, max_)                  \
    NUMERIC_CONVERSION_METHOD_END

/***************************************************************************/
// These expand into full public methods...

NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_INT8, uint8_t, Int8, (-127 - 1), 127)
NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_INT16, int16_t, Int16, (-32767 - 1), 32767)

NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_INT32, int32_t, Int32)
  CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(int32_t)
  CASE__NUMERIC_CONVERSION_UINT32_MAX(int32_t, 2147483647)
  CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(int32_t, (-2147483647 - 1), 2147483647)
NUMERIC_CONVERSION_METHOD_END

NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_UINT8, uint8_t, Uint8, 0, 255)
NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_UINT16, uint16_t, Uint16, 0, 65535)

NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_UINT32, uint32_t, Uint32)
  CASE__NUMERIC_CONVERSION_INT32_MIN_MAX(uint32_t, 0, 2147483647)
  CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(uint32_t)
  CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(uint32_t, 0, 4294967295U)
NUMERIC_CONVERSION_METHOD_END

// XXX toFloat convertions need to be fixed!
NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_FLOAT, float, Float)
  CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(float)
  CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(float)
  CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(float)
NUMERIC_CONVERSION_METHOD_END

NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_DOUBLE, double, Double)
  CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(double)
  CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(double)
  CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(double)
NUMERIC_CONVERSION_METHOD_END

// XXX toChar convertions need to be fixed!
NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_CHAR, char, Char)
  CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(char)
  CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(char)
  CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(char)
NUMERIC_CONVERSION_METHOD_END

// XXX toWChar convertions need to be fixed!
NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_WCHAR, char16_t, WChar)
  CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(char16_t)
  CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(char16_t)
  CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(char16_t)
NUMERIC_CONVERSION_METHOD_END

#undef NUMERIC_CONVERSION_METHOD_BEGIN
#undef CASE__NUMERIC_CONVERSION_INT32_JUST_CAST
#undef CASE__NUMERIC_CONVERSION_INT32_MIN_MAX
#undef CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST
#undef CASE__NUMERIC_CONVERSION_UINT32_MIN_MAX
#undef CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST
#undef CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX
#undef CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT
#undef CASES__NUMERIC_CONVERSION_NORMAL
#undef NUMERIC_CONVERSION_METHOD_END
#undef NUMERIC_CONVERSION_METHOD_NORMAL

/***************************************************************************/

// Just leverage a numeric converter for bool (but restrict the values).
// XXX Is this really what we want to do?

/* static */ nsresult
nsVariant::ConvertToBool(const nsDiscriminatedUnion& aData, bool* aResult)
{
  TRIVIAL_DATA_CONVERTER(VTYPE_BOOL, aData, mBoolValue, aResult)

  double val;
  nsresult rv = nsVariant::ConvertToDouble(aData, &val);
  if (NS_FAILED(rv)) {
    return rv;
  }
  *aResult = 0.0 != val;
  return rv;
}

/***************************************************************************/

/* static */ nsresult
nsVariant::ConvertToInt64(const nsDiscriminatedUnion& aData, int64_t* aResult)
{
  TRIVIAL_DATA_CONVERTER(VTYPE_INT64, aData, mInt64Value, aResult)
  TRIVIAL_DATA_CONVERTER(VTYPE_UINT64, aData, mUint64Value, aResult)

  nsDiscriminatedUnion tempData;
  nsVariant::Initialize(&tempData);
  nsresult rv = ToManageableNumber(aData, &tempData);
  if (NS_FAILED(rv)) {
    return rv;
  }
  switch (tempData.mType) {
    case nsIDataType::VTYPE_INT32:
      *aResult = tempData.u.mInt32Value;
      return rv;
    case nsIDataType::VTYPE_UINT32:
      *aResult = tempData.u.mUint32Value;
      return rv;
    case nsIDataType::VTYPE_DOUBLE:
      // XXX should check for data loss here!
      *aResult = tempData.u.mDoubleValue;
      return rv;
    default:
      NS_ERROR("bad type returned from ToManageableNumber");
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }
}

/* static */ nsresult
nsVariant::ConvertToUint64(const nsDiscriminatedUnion& aData,
                           uint64_t* aResult)
{
  return nsVariant::ConvertToInt64(aData, (int64_t*)aResult);
}

/***************************************************************************/

static bool
String2ID(const nsDiscriminatedUnion& aData, nsID* aPid)
{
  nsAutoString tempString;
  nsAString* pString;

  switch (aData.mType) {
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      return aPid->Parse(aData.u.str.mStringValue);
    case nsIDataType::VTYPE_CSTRING:
      return aPid->Parse(PromiseFlatCString(*aData.u.mCStringValue).get());
    case nsIDataType::VTYPE_UTF8STRING:
      return aPid->Parse(PromiseFlatUTF8String(*aData.u.mUTF8StringValue).get());
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
      pString = aData.u.mAStringValue;
      break;
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      tempString.Assign(aData.u.wstr.mWStringValue);
      pString = &tempString;
      break;
    default:
      NS_ERROR("bad type in call to String2ID");
      return false;
  }

  char* pChars = ToNewCString(*pString);
  if (!pChars) {
    return false;
  }
  bool result = aPid->Parse(pChars);
  free(pChars);
  return result;
}

/* static */ nsresult
nsVariant::ConvertToID(const nsDiscriminatedUnion& aData, nsID* aResult)
{
  nsID id;

  switch (aData.mType) {
    case nsIDataType::VTYPE_ID:
      *aResult = aData.u.mIDValue;
      return NS_OK;
    case nsIDataType::VTYPE_INTERFACE:
      *aResult = NS_GET_IID(nsISupports);
      return NS_OK;
    case nsIDataType::VTYPE_INTERFACE_IS:
      *aResult = aData.u.iface.mInterfaceID;
      return NS_OK;
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      if (!String2ID(aData, &id)) {
        return NS_ERROR_CANNOT_CONVERT_DATA;
      }
      *aResult = id;
      return NS_OK;
    default:
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }
}

/***************************************************************************/

static nsresult
ToString(const nsDiscriminatedUnion& aData, nsACString& aOutString)
{
  char* ptr;

  switch (aData.mType) {
    // all the stuff we don't handle...
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_WCHAR:
      NS_ERROR("ToString being called for a string type - screwy logic!");
      // fall through...

    // XXX We might want stringified versions of these... ???

    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
      aOutString.Truncate();
      aOutString.SetIsVoid(true);
      return NS_OK;

    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
    default:
      return NS_ERROR_CANNOT_CONVERT_DATA;

    // nsID has its own text formatter.

    case nsIDataType::VTYPE_ID:
      ptr = aData.u.mIDValue.ToString();
      if (!ptr) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      aOutString.Assign(ptr);
      free(ptr);
      return NS_OK;

    // Can't use PR_smprintf for floats, since it's locale-dependent
#define CASE__APPENDFLOAT_NUMBER(type_, member_)                        \
    case nsIDataType :: type_ :                                         \
    {                                                                   \
        nsAutoCString str;                                              \
        str.AppendFloat(aData.u. member_);                              \
        aOutString.Assign(str);                                         \
        return NS_OK;                                                   \
    }

    CASE__APPENDFLOAT_NUMBER(VTYPE_FLOAT,  mFloatValue)
    CASE__APPENDFLOAT_NUMBER(VTYPE_DOUBLE, mDoubleValue)

#undef CASE__APPENDFLOAT_NUMBER

    // the rest can be PR_smprintf'd and use common code.

#define CASE__SMPRINTF_NUMBER(type_, format_, cast_, member_)                 \
    case nsIDataType :: type_ :                                               \
        ptr = PR_smprintf( format_ , (cast_) aData.u. member_ );              \
        break;

    CASE__SMPRINTF_NUMBER(VTYPE_INT8,   "%d",   int,      mInt8Value)
    CASE__SMPRINTF_NUMBER(VTYPE_INT16,  "%d",   int,      mInt16Value)
    CASE__SMPRINTF_NUMBER(VTYPE_INT32,  "%d",   int,      mInt32Value)
    CASE__SMPRINTF_NUMBER(VTYPE_INT64,  "%lld", int64_t,  mInt64Value)

    CASE__SMPRINTF_NUMBER(VTYPE_UINT8,  "%u",   unsigned, mUint8Value)
    CASE__SMPRINTF_NUMBER(VTYPE_UINT16, "%u",   unsigned, mUint16Value)
    CASE__SMPRINTF_NUMBER(VTYPE_UINT32, "%u",   unsigned, mUint32Value)
    CASE__SMPRINTF_NUMBER(VTYPE_UINT64, "%llu", int64_t,  mUint64Value)

    // XXX Would we rather print "true" / "false" ?
    CASE__SMPRINTF_NUMBER(VTYPE_BOOL,   "%d",   int,      mBoolValue)

    CASE__SMPRINTF_NUMBER(VTYPE_CHAR,   "%c",   char,     mCharValue)

#undef CASE__SMPRINTF_NUMBER
  }

  if (!ptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOutString.Assign(ptr);
  PR_smprintf_free(ptr);
  return NS_OK;
}

/* static */ nsresult
nsVariant::ConvertToAString(const nsDiscriminatedUnion& aData,
                            nsAString& aResult)
{
  switch (aData.mType) {
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
      aResult.Assign(*aData.u.mAStringValue);
      return NS_OK;
    case nsIDataType::VTYPE_CSTRING:
      CopyASCIItoUTF16(*aData.u.mCStringValue, aResult);
      return NS_OK;
    case nsIDataType::VTYPE_UTF8STRING:
      CopyUTF8toUTF16(*aData.u.mUTF8StringValue, aResult);
      return NS_OK;
    case nsIDataType::VTYPE_CHAR_STR:
      CopyASCIItoUTF16(aData.u.str.mStringValue, aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR_STR:
      aResult.Assign(aData.u.wstr.mWStringValue);
      return NS_OK;
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      CopyASCIItoUTF16(nsDependentCString(aData.u.str.mStringValue,
                                          aData.u.str.mStringLength),
                       aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      aResult.Assign(aData.u.wstr.mWStringValue, aData.u.wstr.mWStringLength);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR:
      aResult.Assign(aData.u.mWCharValue);
      return NS_OK;
    default: {
      nsAutoCString tempCString;
      nsresult rv = ToString(aData, tempCString);
      if (NS_FAILED(rv)) {
        return rv;
      }
      CopyASCIItoUTF16(tempCString, aResult);
      return NS_OK;
    }
  }
}

/* static */ nsresult
nsVariant::ConvertToACString(const nsDiscriminatedUnion& aData,
                             nsACString& aResult)
{
  switch (aData.mType) {
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
      LossyCopyUTF16toASCII(*aData.u.mAStringValue, aResult);
      return NS_OK;
    case nsIDataType::VTYPE_CSTRING:
      aResult.Assign(*aData.u.mCStringValue);
      return NS_OK;
    case nsIDataType::VTYPE_UTF8STRING:
      // XXX This is an extra copy that should be avoided
      // once Jag lands support for UTF8String and associated
      // conversion methods.
      LossyCopyUTF16toASCII(NS_ConvertUTF8toUTF16(*aData.u.mUTF8StringValue),
                            aResult);
      return NS_OK;
    case nsIDataType::VTYPE_CHAR_STR:
      aResult.Assign(*aData.u.str.mStringValue);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR_STR:
      LossyCopyUTF16toASCII(nsDependentString(aData.u.wstr.mWStringValue),
                            aResult);
      return NS_OK;
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      aResult.Assign(aData.u.str.mStringValue, aData.u.str.mStringLength);
      return NS_OK;
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      LossyCopyUTF16toASCII(nsDependentString(aData.u.wstr.mWStringValue,
                                              aData.u.wstr.mWStringLength),
                            aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR: {
      const char16_t* str = &aData.u.mWCharValue;
      LossyCopyUTF16toASCII(Substring(str, 1), aResult);
      return NS_OK;
    }
    default:
      return ToString(aData, aResult);
  }
}

/* static */ nsresult
nsVariant::ConvertToAUTF8String(const nsDiscriminatedUnion& aData,
                                nsAUTF8String& aResult)
{
  switch (aData.mType) {
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
      CopyUTF16toUTF8(*aData.u.mAStringValue, aResult);
      return NS_OK;
    case nsIDataType::VTYPE_CSTRING:
      // XXX Extra copy, can be removed if we're sure CSTRING can
      //     only contain ASCII.
      CopyUTF16toUTF8(NS_ConvertASCIItoUTF16(*aData.u.mCStringValue),
                      aResult);
      return NS_OK;
    case nsIDataType::VTYPE_UTF8STRING:
      aResult.Assign(*aData.u.mUTF8StringValue);
      return NS_OK;
    case nsIDataType::VTYPE_CHAR_STR:
      // XXX Extra copy, can be removed if we're sure CHAR_STR can
      //     only contain ASCII.
      CopyUTF16toUTF8(NS_ConvertASCIItoUTF16(aData.u.str.mStringValue),
                      aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR_STR:
      CopyUTF16toUTF8(aData.u.wstr.mWStringValue, aResult);
      return NS_OK;
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      // XXX Extra copy, can be removed if we're sure CHAR_STR can
      //     only contain ASCII.
      CopyUTF16toUTF8(NS_ConvertASCIItoUTF16(
        nsDependentCString(aData.u.str.mStringValue,
                           aData.u.str.mStringLength)), aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      CopyUTF16toUTF8(nsDependentString(aData.u.wstr.mWStringValue,
                                        aData.u.wstr.mWStringLength),
                      aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR: {
      const char16_t* str = &aData.u.mWCharValue;
      CopyUTF16toUTF8(Substring(str, 1), aResult);
      return NS_OK;
    }
    default: {
      nsAutoCString tempCString;
      nsresult rv = ToString(aData, tempCString);
      if (NS_FAILED(rv)) {
        return rv;
      }
      // XXX Extra copy, can be removed if we're sure tempCString can
      //     only contain ASCII.
      CopyUTF16toUTF8(NS_ConvertASCIItoUTF16(tempCString), aResult);
      return NS_OK;
    }
  }
}

/* static */ nsresult
nsVariant::ConvertToString(const nsDiscriminatedUnion& aData, char** aResult)
{
  uint32_t ignored;
  return nsVariant::ConvertToStringWithSize(aData, &ignored, aResult);
}

/* static */ nsresult
nsVariant::ConvertToWString(const nsDiscriminatedUnion& aData,
                            char16_t** aResult)
{
  uint32_t ignored;
  return nsVariant::ConvertToWStringWithSize(aData, &ignored, aResult);
}

/* static */ nsresult
nsVariant::ConvertToStringWithSize(const nsDiscriminatedUnion& aData,
                                   uint32_t* aSize, char** aStr)
{
  nsAutoString  tempString;
  nsAutoCString tempCString;
  nsresult rv;

  switch (aData.mType) {
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
      *aSize = aData.u.mAStringValue->Length();
      *aStr = ToNewCString(*aData.u.mAStringValue);
      break;
    case nsIDataType::VTYPE_CSTRING:
      *aSize = aData.u.mCStringValue->Length();
      *aStr = ToNewCString(*aData.u.mCStringValue);
      break;
    case nsIDataType::VTYPE_UTF8STRING: {
      // XXX This is doing 1 extra copy.  Need to fix this
      // when Jag lands UTF8String
      // we want:
      // *aSize = *aData.mUTF8StringValue->Length();
      // *aStr = ToNewCString(*aData.mUTF8StringValue);
      // But this will have to do for now.
      NS_ConvertUTF8toUTF16 tempString(*aData.u.mUTF8StringValue);
      *aSize = tempString.Length();
      *aStr = ToNewCString(tempString);
      break;
    }
    case nsIDataType::VTYPE_CHAR_STR: {
      nsDependentCString cString(aData.u.str.mStringValue);
      *aSize = cString.Length();
      *aStr = ToNewCString(cString);
      break;
    }
    case nsIDataType::VTYPE_WCHAR_STR: {
      nsDependentString string(aData.u.wstr.mWStringValue);
      *aSize = string.Length();
      *aStr = ToNewCString(string);
      break;
    }
    case nsIDataType::VTYPE_STRING_SIZE_IS: {
      nsDependentCString cString(aData.u.str.mStringValue,
                                 aData.u.str.mStringLength);
      *aSize = cString.Length();
      *aStr = ToNewCString(cString);
      break;
    }
    case nsIDataType::VTYPE_WSTRING_SIZE_IS: {
      nsDependentString string(aData.u.wstr.mWStringValue,
                               aData.u.wstr.mWStringLength);
      *aSize = string.Length();
      *aStr = ToNewCString(string);
      break;
    }
    case nsIDataType::VTYPE_WCHAR:
      tempString.Assign(aData.u.mWCharValue);
      *aSize = tempString.Length();
      *aStr = ToNewCString(tempString);
      break;
    default:
      rv = ToString(aData, tempCString);
      if (NS_FAILED(rv)) {
        return rv;
      }
      *aSize = tempCString.Length();
      *aStr = ToNewCString(tempCString);
      break;
  }

  return *aStr ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
/* static */ nsresult
nsVariant::ConvertToWStringWithSize(const nsDiscriminatedUnion& aData,
                                    uint32_t* aSize, char16_t** aStr)
{
  nsAutoString  tempString;
  nsAutoCString tempCString;
  nsresult rv;

  switch (aData.mType) {
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
      *aSize = aData.u.mAStringValue->Length();
      *aStr = ToNewUnicode(*aData.u.mAStringValue);
      break;
    case nsIDataType::VTYPE_CSTRING:
      *aSize = aData.u.mCStringValue->Length();
      *aStr = ToNewUnicode(*aData.u.mCStringValue);
      break;
    case nsIDataType::VTYPE_UTF8STRING: {
      *aStr = UTF8ToNewUnicode(*aData.u.mUTF8StringValue, aSize);
      break;
    }
    case nsIDataType::VTYPE_CHAR_STR: {
      nsDependentCString cString(aData.u.str.mStringValue);
      *aSize = cString.Length();
      *aStr = ToNewUnicode(cString);
      break;
    }
    case nsIDataType::VTYPE_WCHAR_STR: {
      nsDependentString string(aData.u.wstr.mWStringValue);
      *aSize = string.Length();
      *aStr = ToNewUnicode(string);
      break;
    }
    case nsIDataType::VTYPE_STRING_SIZE_IS: {
      nsDependentCString cString(aData.u.str.mStringValue,
                                 aData.u.str.mStringLength);
      *aSize = cString.Length();
      *aStr = ToNewUnicode(cString);
      break;
    }
    case nsIDataType::VTYPE_WSTRING_SIZE_IS: {
      nsDependentString string(aData.u.wstr.mWStringValue,
                               aData.u.wstr.mWStringLength);
      *aSize = string.Length();
      *aStr = ToNewUnicode(string);
      break;
    }
    case nsIDataType::VTYPE_WCHAR:
      tempString.Assign(aData.u.mWCharValue);
      *aSize = tempString.Length();
      *aStr = ToNewUnicode(tempString);
      break;
    default:
      rv = ToString(aData, tempCString);
      if (NS_FAILED(rv)) {
        return rv;
      }
      *aSize = tempCString.Length();
      *aStr = ToNewUnicode(tempCString);
      break;
  }

  return *aStr ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* static */ nsresult
nsVariant::ConvertToISupports(const nsDiscriminatedUnion& aData,
                              nsISupports** aResult)
{
  switch (aData.mType) {
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      if (aData.u.iface.mInterfaceValue) {
        return aData.u.iface.mInterfaceValue->
          QueryInterface(NS_GET_IID(nsISupports), (void**)aResult);
      } else {
        *aResult = nullptr;
        return NS_OK;
      }
    default:
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }
}

/* static */ nsresult
nsVariant::ConvertToInterface(const nsDiscriminatedUnion& aData, nsIID** aIID,
                              void** aInterface)
{
  const nsIID* piid;

  switch (aData.mType) {
    case nsIDataType::VTYPE_INTERFACE:
      piid = &NS_GET_IID(nsISupports);
      break;
    case nsIDataType::VTYPE_INTERFACE_IS:
      piid = &aData.u.iface.mInterfaceID;
      break;
    default:
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  *aIID = (nsIID*)nsMemory::Clone(piid, sizeof(nsIID));
  if (!*aIID) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (aData.u.iface.mInterfaceValue) {
    return aData.u.iface.mInterfaceValue->QueryInterface(*piid,
                                                              aInterface);
  }

  *aInterface = nullptr;
  return NS_OK;
}

/* static */ nsresult
nsVariant::ConvertToArray(const nsDiscriminatedUnion& aData, uint16_t* aType,
                          nsIID* aIID, uint32_t* aCount, void** aPtr)
{
  // XXX perhaps we'd like to add support for converting each of the various
  // types into an array containing one element of that type. We can leverage
  // CloneArray to do this if we want to support this.

  if (aData.mType == nsIDataType::VTYPE_ARRAY) {
    return CloneArray(aData.u.array.mArrayType, &aData.u.array.mArrayInterfaceID,
                      aData.u.array.mArrayCount, aData.u.array.mArrayValue,
                      aType, aIID, aCount, aPtr);
  }
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

/***************************************************************************/
// static setter functions...

#define DATA_SETTER_PROLOGUE(data_)                                           \
    nsVariant::Cleanup(data_);

#define DATA_SETTER_EPILOGUE(data_, type_)                                    \
    data_->mType = nsIDataType :: type_;                                      \
    return NS_OK;

#define DATA_SETTER(data_, type_, member_, value_)                            \
    DATA_SETTER_PROLOGUE(data_)                                               \
    data_->u.member_ = value_;                                                \
    DATA_SETTER_EPILOGUE(data_, type_)

#define DATA_SETTER_WITH_CAST(data_, type_, member_, cast_, value_)           \
    DATA_SETTER_PROLOGUE(data_)                                               \
    data_->u.member_ = cast_ value_;                                          \
    DATA_SETTER_EPILOGUE(data_, type_)


/********************************************/

#define CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(type_)                          \
    {                                                                         \

#define CASE__SET_FROM_VARIANT_VTYPE__GETTER(member_, name_)                  \
        rv = aValue->GetAs##name_ (&(aData->u. member_ ));

#define CASE__SET_FROM_VARIANT_VTYPE__GETTER_CAST(cast_, member_, name_)      \
        rv = aValue->GetAs##name_ ( cast_ &(aData->u. member_ ));

#define CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(type_)                          \
        if(NS_SUCCEEDED(rv))                                                  \
        {                                                                     \
            aData->mType  = nsIDataType :: type_ ;                            \
        }                                                                     \
        break;                                                                \
    }

#define CASE__SET_FROM_VARIANT_TYPE(type_, member_, name_)                    \
    case nsIDataType :: type_ :                                               \
        CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(type_)                          \
        CASE__SET_FROM_VARIANT_VTYPE__GETTER(member_, name_)                  \
        CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(type_)

#define CASE__SET_FROM_VARIANT_VTYPE_CAST(type_, cast_, member_, name_)       \
    case nsIDataType :: type_ :                                               \
        CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(type_)                          \
        CASE__SET_FROM_VARIANT_VTYPE__GETTER_CAST(cast_, member_, name_)      \
        CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(type_)


/* static */ nsresult
nsVariant::SetFromVariant(nsDiscriminatedUnion* aData, nsIVariant* aValue)
{
  uint16_t type;
  nsresult rv;

  nsVariant::Cleanup(aData);

  rv = aValue->GetDataType(&type);
  if (NS_FAILED(rv)) {
    return rv;
  }

  switch (type) {
    CASE__SET_FROM_VARIANT_VTYPE_CAST(VTYPE_INT8, (uint8_t*), mInt8Value,
                                      Int8)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_INT16,  mInt16Value,  Int16)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_INT32,  mInt32Value,  Int32)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_UINT8,  mUint8Value,  Uint8)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_UINT16, mUint16Value, Uint16)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_UINT32, mUint32Value, Uint32)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_FLOAT,  mFloatValue,  Float)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_DOUBLE, mDoubleValue, Double)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_BOOL ,  mBoolValue,   Bool)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_CHAR,   mCharValue,   Char)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_WCHAR,  mWCharValue,  WChar)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_ID,     mIDValue,     ID)

    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_ASTRING);
      aData->u.mAStringValue = new nsString();
      if (!aData->u.mAStringValue) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      rv = aValue->GetAsAString(*aData->u.mAStringValue);
      if (NS_FAILED(rv)) {
        delete aData->u.mAStringValue;
      }
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_ASTRING)

    case nsIDataType::VTYPE_CSTRING:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_CSTRING);
      aData->u.mCStringValue = new nsCString();
      if (!aData->u.mCStringValue) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      rv = aValue->GetAsACString(*aData->u.mCStringValue);
      if (NS_FAILED(rv)) {
        delete aData->u.mCStringValue;
      }
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_CSTRING)

    case nsIDataType::VTYPE_UTF8STRING:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_UTF8STRING);
      aData->u.mUTF8StringValue = new nsUTF8String();
      if (!aData->u.mUTF8StringValue) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      rv = aValue->GetAsAUTF8String(*aData->u.mUTF8StringValue);
      if (NS_FAILED(rv)) {
        delete aData->u.mUTF8StringValue;
      }
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_UTF8STRING)

    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_STRING_SIZE_IS);
      rv = aValue->GetAsStringWithSize(&aData->u.str.mStringLength,
                                       &aData->u.str.mStringValue);
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_STRING_SIZE_IS)

    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_INTERFACE_IS);
      // XXX This iid handling is ugly!
      nsIID* iid;
      rv = aValue->GetAsInterface(&iid, (void**)&aData->u.iface.mInterfaceValue);
      if (NS_SUCCEEDED(rv)) {
        aData->u.iface.mInterfaceID = *iid;
        free((char*)iid);
      }
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_INTERFACE_IS)

    case nsIDataType::VTYPE_ARRAY:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_ARRAY);
      rv = aValue->GetAsArray(&aData->u.array.mArrayType,
                              &aData->u.array.mArrayInterfaceID,
                              &aData->u.array.mArrayCount,
                              &aData->u.array.mArrayValue);
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_ARRAY)

    case nsIDataType::VTYPE_VOID:
      rv = nsVariant::SetToVoid(aData);
      break;
    case nsIDataType::VTYPE_EMPTY_ARRAY:
      rv = nsVariant::SetToEmptyArray(aData);
      break;
    case nsIDataType::VTYPE_EMPTY:
      rv = nsVariant::SetToEmpty(aData);
      break;
    default:
      NS_ERROR("bad type in variant!");
      rv = NS_ERROR_FAILURE;
      break;
  }
  return rv;
}

/* static */ nsresult
nsVariant::SetFromInt8(nsDiscriminatedUnion* aData, uint8_t aValue)
{
  DATA_SETTER_WITH_CAST(aData, VTYPE_INT8, mInt8Value, (uint8_t), aValue)
}
/* static */ nsresult
nsVariant::SetFromInt16(nsDiscriminatedUnion* aData, int16_t aValue)
{
  DATA_SETTER(aData, VTYPE_INT16, mInt16Value, aValue)
}
/* static */ nsresult
nsVariant::SetFromInt32(nsDiscriminatedUnion* aData, int32_t aValue)
{
  DATA_SETTER(aData, VTYPE_INT32, mInt32Value, aValue)
}
/* static */ nsresult
nsVariant::SetFromInt64(nsDiscriminatedUnion* aData, int64_t aValue)
{
  DATA_SETTER(aData, VTYPE_INT64, mInt64Value, aValue)
}
/* static */ nsresult
nsVariant::SetFromUint8(nsDiscriminatedUnion* aData, uint8_t aValue)
{
  DATA_SETTER(aData, VTYPE_UINT8, mUint8Value, aValue)
}
/* static */ nsresult
nsVariant::SetFromUint16(nsDiscriminatedUnion* aData, uint16_t aValue)
{
  DATA_SETTER(aData, VTYPE_UINT16, mUint16Value, aValue)
}
/* static */ nsresult
nsVariant::SetFromUint32(nsDiscriminatedUnion* aData, uint32_t aValue)
{
  DATA_SETTER(aData, VTYPE_UINT32, mUint32Value, aValue)
}
/* static */ nsresult
nsVariant::SetFromUint64(nsDiscriminatedUnion* aData, uint64_t aValue)
{
  DATA_SETTER(aData, VTYPE_UINT64, mUint64Value, aValue)
}
/* static */ nsresult
nsVariant::SetFromFloat(nsDiscriminatedUnion* aData, float aValue)
{
  DATA_SETTER(aData, VTYPE_FLOAT, mFloatValue, aValue)
}
/* static */ nsresult
nsVariant::SetFromDouble(nsDiscriminatedUnion* aData, double aValue)
{
  DATA_SETTER(aData, VTYPE_DOUBLE, mDoubleValue, aValue)
}
/* static */ nsresult
nsVariant::SetFromBool(nsDiscriminatedUnion* aData, bool aValue)
{
  DATA_SETTER(aData, VTYPE_BOOL, mBoolValue, aValue)
}
/* static */ nsresult
nsVariant::SetFromChar(nsDiscriminatedUnion* aData, char aValue)
{
  DATA_SETTER(aData, VTYPE_CHAR, mCharValue, aValue)
}
/* static */ nsresult
nsVariant::SetFromWChar(nsDiscriminatedUnion* aData, char16_t aValue)
{
  DATA_SETTER(aData, VTYPE_WCHAR, mWCharValue, aValue)
}
/* static */ nsresult
nsVariant::SetFromID(nsDiscriminatedUnion* aData, const nsID& aValue)
{
  DATA_SETTER(aData, VTYPE_ID, mIDValue, aValue)
}
/* static */ nsresult
nsVariant::SetFromAString(nsDiscriminatedUnion* aData, const nsAString& aValue)
{
  DATA_SETTER_PROLOGUE(aData);
  if (!(aData->u.mAStringValue = new nsString(aValue))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  DATA_SETTER_EPILOGUE(aData, VTYPE_ASTRING);
}

/* static */ nsresult
nsVariant::SetFromACString(nsDiscriminatedUnion* aData,
                           const nsACString& aValue)
{
  DATA_SETTER_PROLOGUE(aData);
  if (!(aData->u.mCStringValue = new nsCString(aValue))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  DATA_SETTER_EPILOGUE(aData, VTYPE_CSTRING);
}

/* static */ nsresult
nsVariant::SetFromAUTF8String(nsDiscriminatedUnion* aData,
                              const nsAUTF8String& aValue)
{
  DATA_SETTER_PROLOGUE(aData);
  if (!(aData->u.mUTF8StringValue = new nsUTF8String(aValue))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  DATA_SETTER_EPILOGUE(aData, VTYPE_UTF8STRING);
}

/* static */ nsresult
nsVariant::SetFromString(nsDiscriminatedUnion* aData, const char* aValue)
{
  DATA_SETTER_PROLOGUE(aData);
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  return SetFromStringWithSize(aData, strlen(aValue), aValue);
}
/* static */ nsresult
nsVariant::SetFromWString(nsDiscriminatedUnion* aData, const char16_t* aValue)
{
  DATA_SETTER_PROLOGUE(aData);
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  return SetFromWStringWithSize(aData, NS_strlen(aValue), aValue);
}
/* static */ nsresult
nsVariant::SetFromISupports(nsDiscriminatedUnion* aData, nsISupports* aValue)
{
  return SetFromInterface(aData, NS_GET_IID(nsISupports), aValue);
}
/* static */ nsresult
nsVariant::SetFromInterface(nsDiscriminatedUnion* aData, const nsIID& aIID,
                            nsISupports* aValue)
{
  DATA_SETTER_PROLOGUE(aData);
  NS_IF_ADDREF(aValue);
  aData->u.iface.mInterfaceValue = aValue;
  aData->u.iface.mInterfaceID = aIID;
  DATA_SETTER_EPILOGUE(aData, VTYPE_INTERFACE_IS);
}
/* static */ nsresult
nsVariant::SetFromArray(nsDiscriminatedUnion* aData, uint16_t aType,
                        const nsIID* aIID, uint32_t aCount, void* aValue)
{
  DATA_SETTER_PROLOGUE(aData);
  if (!aValue || !aCount) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = CloneArray(aType, aIID, aCount, aValue,
                           &aData->u.array.mArrayType,
                           &aData->u.array.mArrayInterfaceID,
                           &aData->u.array.mArrayCount,
                           &aData->u.array.mArrayValue);
  if (NS_FAILED(rv)) {
    return rv;
  }
  DATA_SETTER_EPILOGUE(aData, VTYPE_ARRAY);
}
/* static */ nsresult
nsVariant::SetFromStringWithSize(nsDiscriminatedUnion* aData, uint32_t aSize,
                                 const char* aValue)
{
  DATA_SETTER_PROLOGUE(aData);
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  if (!(aData->u.str.mStringValue =
          (char*)nsMemory::Clone(aValue, (aSize + 1) * sizeof(char)))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aData->u.str.mStringLength = aSize;
  DATA_SETTER_EPILOGUE(aData, VTYPE_STRING_SIZE_IS);
}
/* static */ nsresult
nsVariant::SetFromWStringWithSize(nsDiscriminatedUnion* aData, uint32_t aSize,
                                  const char16_t* aValue)
{
  DATA_SETTER_PROLOGUE(aData);
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  if (!(aData->u.wstr.mWStringValue =
          (char16_t*)nsMemory::Clone(aValue, (aSize + 1) * sizeof(char16_t)))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aData->u.wstr.mWStringLength = aSize;
  DATA_SETTER_EPILOGUE(aData, VTYPE_WSTRING_SIZE_IS);
}
/* static */ nsresult
nsVariant::AllocateWStringWithSize(nsDiscriminatedUnion* aData, uint32_t aSize)
{
  DATA_SETTER_PROLOGUE(aData);
  if (!(aData->u.wstr.mWStringValue =
          (char16_t*)moz_xmalloc((aSize + 1) * sizeof(char16_t)))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aData->u.wstr.mWStringValue[aSize] = '\0';
  aData->u.wstr.mWStringLength = aSize;
  DATA_SETTER_EPILOGUE(aData, VTYPE_WSTRING_SIZE_IS);
}
/* static */ nsresult
nsVariant::SetToVoid(nsDiscriminatedUnion* aData)
{
  DATA_SETTER_PROLOGUE(aData);
  DATA_SETTER_EPILOGUE(aData, VTYPE_VOID);
}
/* static */ nsresult
nsVariant::SetToEmpty(nsDiscriminatedUnion* aData)
{
  DATA_SETTER_PROLOGUE(aData);
  DATA_SETTER_EPILOGUE(aData, VTYPE_EMPTY);
}
/* static */ nsresult
nsVariant::SetToEmptyArray(nsDiscriminatedUnion* aData)
{
  DATA_SETTER_PROLOGUE(aData);
  DATA_SETTER_EPILOGUE(aData, VTYPE_EMPTY_ARRAY);
}

/***************************************************************************/

/* static */ nsresult
nsVariant::Initialize(nsDiscriminatedUnion* aData)
{
  aData->mType = nsIDataType::VTYPE_EMPTY;
  return NS_OK;
}

/* static */ nsresult
nsVariant::Cleanup(nsDiscriminatedUnion* aData)
{
  switch (aData->mType) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_UINT64:
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE:
    case nsIDataType::VTYPE_BOOL:
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_WCHAR:
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ID:
      break;
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_DOMSTRING:
      delete aData->u.mAStringValue;
      break;
    case nsIDataType::VTYPE_CSTRING:
      delete aData->u.mCStringValue;
      break;
    case nsIDataType::VTYPE_UTF8STRING:
      delete aData->u.mUTF8StringValue;
      break;
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      free((char*)aData->u.str.mStringValue);
      break;
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      free((char*)aData->u.wstr.mWStringValue);
      break;
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      NS_IF_RELEASE(aData->u.iface.mInterfaceValue);
      break;
    case nsIDataType::VTYPE_ARRAY:
      FreeArray(aData);
      break;
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
      break;
    default:
      NS_ERROR("bad type in variant!");
      break;
  }

  aData->mType = nsIDataType::VTYPE_EMPTY;
  return NS_OK;
}

/* static */ void
nsVariant::Traverse(const nsDiscriminatedUnion& aData,
                    nsCycleCollectionTraversalCallback& aCb)
{
  switch (aData.mType) {
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mData");
      aCb.NoteXPCOMChild(aData.u.iface.mInterfaceValue);
      break;
    case nsIDataType::VTYPE_ARRAY:
      switch (aData.u.array.mArrayType) {
        case nsIDataType::VTYPE_INTERFACE:
        case nsIDataType::VTYPE_INTERFACE_IS: {
          nsISupports** p = (nsISupports**)aData.u.array.mArrayValue;
          for (uint32_t i = aData.u.array.mArrayCount; i > 0; ++p, --i) {
            NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mData[i]");
            aCb.NoteXPCOMChild(*p);
          }
        }
        default:
          break;
      }
    default:
      break;
  }
}

/***************************************************************************/
/***************************************************************************/
// members...

NS_IMPL_ISUPPORTS(nsVariant, nsIVariant, nsIWritableVariant)

nsVariant::nsVariant()
  : mWritable(true)
{
  nsVariant::Initialize(&mData);

#ifdef DEBUG
  {
    // Assert that the nsIDataType consts match the values #defined in
    // xpt_struct.h. Bad things happen somewhere if they don't.
    struct THE_TYPES
    {
      uint16_t a;
      uint16_t b;
    };
    static const THE_TYPES array[] = {
      {nsIDataType::VTYPE_INT8              , TD_INT8             },
      {nsIDataType::VTYPE_INT16             , TD_INT16            },
      {nsIDataType::VTYPE_INT32             , TD_INT32            },
      {nsIDataType::VTYPE_INT64             , TD_INT64            },
      {nsIDataType::VTYPE_UINT8             , TD_UINT8            },
      {nsIDataType::VTYPE_UINT16            , TD_UINT16           },
      {nsIDataType::VTYPE_UINT32            , TD_UINT32           },
      {nsIDataType::VTYPE_UINT64            , TD_UINT64           },
      {nsIDataType::VTYPE_FLOAT             , TD_FLOAT            },
      {nsIDataType::VTYPE_DOUBLE            , TD_DOUBLE           },
      {nsIDataType::VTYPE_BOOL              , TD_BOOL             },
      {nsIDataType::VTYPE_CHAR              , TD_CHAR             },
      {nsIDataType::VTYPE_WCHAR             , TD_WCHAR            },
      {nsIDataType::VTYPE_VOID              , TD_VOID             },
      {nsIDataType::VTYPE_ID                , TD_PNSIID           },
      {nsIDataType::VTYPE_DOMSTRING         , TD_DOMSTRING        },
      {nsIDataType::VTYPE_CHAR_STR          , TD_PSTRING          },
      {nsIDataType::VTYPE_WCHAR_STR         , TD_PWSTRING         },
      {nsIDataType::VTYPE_INTERFACE         , TD_INTERFACE_TYPE   },
      {nsIDataType::VTYPE_INTERFACE_IS      , TD_INTERFACE_IS_TYPE},
      {nsIDataType::VTYPE_ARRAY             , TD_ARRAY            },
      {nsIDataType::VTYPE_STRING_SIZE_IS    , TD_PSTRING_SIZE_IS  },
      {nsIDataType::VTYPE_WSTRING_SIZE_IS   , TD_PWSTRING_SIZE_IS },
      {nsIDataType::VTYPE_UTF8STRING        , TD_UTF8STRING       },
      {nsIDataType::VTYPE_CSTRING           , TD_CSTRING          },
      {nsIDataType::VTYPE_ASTRING           , TD_ASTRING          }
    };
    static const int length = sizeof(array) / sizeof(array[0]);
    static bool inited = false;
    if (!inited) {
      for (int i = 0; i < length; ++i) {
        NS_ASSERTION(array[i].a == array[i].b, "bad const declaration");
      }
      inited = true;
    }
  }
#endif
}

nsVariant::~nsVariant()
{
  nsVariant::Cleanup(&mData);
}

// For all the data getters we just forward to the static (and sharable)
// 'ConvertTo' functions.

/* readonly attribute uint16_t dataType; */
NS_IMETHODIMP
nsVariant::GetDataType(uint16_t* aDataType)
{
  *aDataType = mData.mType;
  return NS_OK;
}

/* uint8_t getAsInt8 (); */
NS_IMETHODIMP
nsVariant::GetAsInt8(uint8_t* aResult)
{
  return nsVariant::ConvertToInt8(mData, aResult);
}

/* int16_t getAsInt16 (); */
NS_IMETHODIMP
nsVariant::GetAsInt16(int16_t* aResult)
{
  return nsVariant::ConvertToInt16(mData, aResult);
}

/* int32_t getAsInt32 (); */
NS_IMETHODIMP
nsVariant::GetAsInt32(int32_t* aResult)
{
  return nsVariant::ConvertToInt32(mData, aResult);
}

/* int64_t getAsInt64 (); */
NS_IMETHODIMP
nsVariant::GetAsInt64(int64_t* aResult)
{
  return nsVariant::ConvertToInt64(mData, aResult);
}

/* uint8_t getAsUint8 (); */
NS_IMETHODIMP
nsVariant::GetAsUint8(uint8_t* aResult)
{
  return nsVariant::ConvertToUint8(mData, aResult);
}

/* uint16_t getAsUint16 (); */
NS_IMETHODIMP
nsVariant::GetAsUint16(uint16_t* aResult)
{
  return nsVariant::ConvertToUint16(mData, aResult);
}

/* uint32_t getAsUint32 (); */
NS_IMETHODIMP
nsVariant::GetAsUint32(uint32_t* aResult)
{
  return nsVariant::ConvertToUint32(mData, aResult);
}

/* uint64_t getAsUint64 (); */
NS_IMETHODIMP
nsVariant::GetAsUint64(uint64_t* aResult)
{
  return nsVariant::ConvertToUint64(mData, aResult);
}

/* float getAsFloat (); */
NS_IMETHODIMP
nsVariant::GetAsFloat(float* aResult)
{
  return nsVariant::ConvertToFloat(mData, aResult);
}

/* double getAsDouble (); */
NS_IMETHODIMP
nsVariant::GetAsDouble(double* aResult)
{
  return nsVariant::ConvertToDouble(mData, aResult);
}

/* bool getAsBool (); */
NS_IMETHODIMP
nsVariant::GetAsBool(bool* aResult)
{
  return nsVariant::ConvertToBool(mData, aResult);
}

/* char getAsChar (); */
NS_IMETHODIMP
nsVariant::GetAsChar(char* aResult)
{
  return nsVariant::ConvertToChar(mData, aResult);
}

/* wchar getAsWChar (); */
NS_IMETHODIMP
nsVariant::GetAsWChar(char16_t* aResult)
{
  return nsVariant::ConvertToWChar(mData, aResult);
}

/* [notxpcom] nsresult getAsID (out nsID retval); */
NS_IMETHODIMP_(nsresult)
nsVariant::GetAsID(nsID* aResult)
{
  return nsVariant::ConvertToID(mData, aResult);
}

/* AString getAsAString (); */
NS_IMETHODIMP
nsVariant::GetAsAString(nsAString& aResult)
{
  return nsVariant::ConvertToAString(mData, aResult);
}

/* DOMString getAsDOMString (); */
NS_IMETHODIMP
nsVariant::GetAsDOMString(nsAString& aResult)
{
  // A DOMString maps to an AString internally, so we can re-use
  // ConvertToAString here.
  return nsVariant::ConvertToAString(mData, aResult);
}

/* ACString getAsACString (); */
NS_IMETHODIMP
nsVariant::GetAsACString(nsACString& aResult)
{
  return nsVariant::ConvertToACString(mData, aResult);
}

/* AUTF8String getAsAUTF8String (); */
NS_IMETHODIMP
nsVariant::GetAsAUTF8String(nsAUTF8String& aResult)
{
  return nsVariant::ConvertToAUTF8String(mData, aResult);
}

/* string getAsString (); */
NS_IMETHODIMP
nsVariant::GetAsString(char** aResult)
{
  return nsVariant::ConvertToString(mData, aResult);
}

/* wstring getAsWString (); */
NS_IMETHODIMP
nsVariant::GetAsWString(char16_t** aResult)
{
  return nsVariant::ConvertToWString(mData, aResult);
}

/* nsISupports getAsISupports (); */
NS_IMETHODIMP
nsVariant::GetAsISupports(nsISupports** aResult)
{
  return nsVariant::ConvertToISupports(mData, aResult);
}

/* jsval getAsJSVal() */
NS_IMETHODIMP
nsVariant::GetAsJSVal(JS::MutableHandleValue)
{
  // Can only get the jsval from an XPCVariant.
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

/* void getAsInterface (out nsIIDPtr iid, [iid_is (iid), retval] out nsQIResult iface); */
NS_IMETHODIMP
nsVariant::GetAsInterface(nsIID** aIID, void** aInterface)
{
  return nsVariant::ConvertToInterface(mData, aIID, aInterface);
}

/* [notxpcom] nsresult getAsArray (out uint16_t type, out nsIID iid, out uint32_t count, out voidPtr ptr); */
NS_IMETHODIMP_(nsresult)
nsVariant::GetAsArray(uint16_t* aType, nsIID* aIID,
                      uint32_t* aCount, void** aPtr)
{
  return nsVariant::ConvertToArray(mData, aType, aIID, aCount, aPtr);
}

/* void getAsStringWithSize (out uint32_t size, [size_is (size), retval] out string str); */
NS_IMETHODIMP
nsVariant::GetAsStringWithSize(uint32_t* aSize, char** aStr)
{
  return nsVariant::ConvertToStringWithSize(mData, aSize, aStr);
}

/* void getAsWStringWithSize (out uint32_t size, [size_is (size), retval] out wstring str); */
NS_IMETHODIMP
nsVariant::GetAsWStringWithSize(uint32_t* aSize, char16_t** aStr)
{
  return nsVariant::ConvertToWStringWithSize(mData, aSize, aStr);
}

/***************************************************************************/

/* attribute bool writable; */
NS_IMETHODIMP
nsVariant::GetWritable(bool* aWritable)
{
  *aWritable = mWritable;
  return NS_OK;
}
NS_IMETHODIMP
nsVariant::SetWritable(bool aWritable)
{
  if (!mWritable && aWritable) {
    return NS_ERROR_FAILURE;
  }
  mWritable = aWritable;
  return NS_OK;
}

/***************************************************************************/

// For all the data setters we just forward to the static (and sharable)
// 'SetFrom' functions.

/* void setAsInt8 (in uint8_t aValue); */
NS_IMETHODIMP
nsVariant::SetAsInt8(uint8_t aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromInt8(&mData, aValue);
}

/* void setAsInt16 (in int16_t aValue); */
NS_IMETHODIMP
nsVariant::SetAsInt16(int16_t aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromInt16(&mData, aValue);
}

/* void setAsInt32 (in int32_t aValue); */
NS_IMETHODIMP
nsVariant::SetAsInt32(int32_t aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromInt32(&mData, aValue);
}

/* void setAsInt64 (in int64_t aValue); */
NS_IMETHODIMP
nsVariant::SetAsInt64(int64_t aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromInt64(&mData, aValue);
}

/* void setAsUint8 (in uint8_t aValue); */
NS_IMETHODIMP
nsVariant::SetAsUint8(uint8_t aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromUint8(&mData, aValue);
}

/* void setAsUint16 (in uint16_t aValue); */
NS_IMETHODIMP
nsVariant::SetAsUint16(uint16_t aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromUint16(&mData, aValue);
}

/* void setAsUint32 (in uint32_t aValue); */
NS_IMETHODIMP
nsVariant::SetAsUint32(uint32_t aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromUint32(&mData, aValue);
}

/* void setAsUint64 (in uint64_t aValue); */
NS_IMETHODIMP
nsVariant::SetAsUint64(uint64_t aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromUint64(&mData, aValue);
}

/* void setAsFloat (in float aValue); */
NS_IMETHODIMP
nsVariant::SetAsFloat(float aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromFloat(&mData, aValue);
}

/* void setAsDouble (in double aValue); */
NS_IMETHODIMP
nsVariant::SetAsDouble(double aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromDouble(&mData, aValue);
}

/* void setAsBool (in bool aValue); */
NS_IMETHODIMP
nsVariant::SetAsBool(bool aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromBool(&mData, aValue);
}

/* void setAsChar (in char aValue); */
NS_IMETHODIMP
nsVariant::SetAsChar(char aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromChar(&mData, aValue);
}

/* void setAsWChar (in wchar aValue); */
NS_IMETHODIMP
nsVariant::SetAsWChar(char16_t aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromWChar(&mData, aValue);
}

/* void setAsID (in nsIDRef aValue); */
NS_IMETHODIMP
nsVariant::SetAsID(const nsID& aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromID(&mData, aValue);
}

/* void setAsAString (in AString aValue); */
NS_IMETHODIMP
nsVariant::SetAsAString(const nsAString& aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromAString(&mData, aValue);
}

/* void setAsDOMString (in DOMString aValue); */
NS_IMETHODIMP
nsVariant::SetAsDOMString(const nsAString& aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  DATA_SETTER_PROLOGUE((&mData));
  if (!(mData.u.mAStringValue = new nsString(aValue))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  DATA_SETTER_EPILOGUE((&mData), VTYPE_DOMSTRING);
}

/* void setAsACString (in ACString aValue); */
NS_IMETHODIMP
nsVariant::SetAsACString(const nsACString& aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromACString(&mData, aValue);
}

/* void setAsAUTF8String (in AUTF8String aValue); */
NS_IMETHODIMP
nsVariant::SetAsAUTF8String(const nsAUTF8String& aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromAUTF8String(&mData, aValue);
}

/* void setAsString (in string aValue); */
NS_IMETHODIMP
nsVariant::SetAsString(const char* aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromString(&mData, aValue);
}

/* void setAsWString (in wstring aValue); */
NS_IMETHODIMP
nsVariant::SetAsWString(const char16_t* aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromWString(&mData, aValue);
}

/* void setAsISupports (in nsISupports aValue); */
NS_IMETHODIMP
nsVariant::SetAsISupports(nsISupports* aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromISupports(&mData, aValue);
}

/* void setAsInterface (in nsIIDRef iid, [iid_is (iid)] in nsQIResult iface); */
NS_IMETHODIMP
nsVariant::SetAsInterface(const nsIID& aIID, void* aInterface)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromInterface(&mData, aIID, (nsISupports*)aInterface);
}

/* [noscript] void setAsArray (in uint16_t type, in nsIIDPtr iid, in uint32_t count, in voidPtr ptr); */
NS_IMETHODIMP
nsVariant::SetAsArray(uint16_t aType, const nsIID* aIID,
                      uint32_t aCount, void* aPtr)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromArray(&mData, aType, aIID, aCount, aPtr);
}

/* void setAsStringWithSize (in uint32_t size, [size_is (size)] in string str); */
NS_IMETHODIMP
nsVariant::SetAsStringWithSize(uint32_t aSize, const char* aStr)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromStringWithSize(&mData, aSize, aStr);
}

/* void setAsWStringWithSize (in uint32_t size, [size_is (size)] in wstring str); */
NS_IMETHODIMP
nsVariant::SetAsWStringWithSize(uint32_t aSize, const char16_t* aStr)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromWStringWithSize(&mData, aSize, aStr);
}

/* void setAsVoid (); */
NS_IMETHODIMP
nsVariant::SetAsVoid()
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetToVoid(&mData);
}

/* void setAsEmpty (); */
NS_IMETHODIMP
nsVariant::SetAsEmpty()
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetToEmpty(&mData);
}

/* void setAsEmptyArray (); */
NS_IMETHODIMP
nsVariant::SetAsEmptyArray()
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetToEmptyArray(&mData);
}

/* void setFromVariant (in nsIVariant aValue); */
NS_IMETHODIMP
nsVariant::SetFromVariant(nsIVariant* aValue)
{
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return nsVariant::SetFromVariant(&mData, aValue);
}
