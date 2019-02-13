/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_IPCMESSAGEUTILS_H__
#define __IPC_GLUE_IPCMESSAGEUTILS_H__

#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#ifdef XP_WIN
#include "mozilla/TimeStamp_windows.h"
#endif
#include "mozilla/TypeTraits.h"
#include "mozilla/IntegerTypeTraits.h"

#include <stdint.h>

#include "nsID.h"
#include "nsIWidget.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsTArray.h"
#include "js/StructuredClone.h"
#include "nsCSSProperty.h"

#ifdef _MSC_VER
#pragma warning( disable : 4800 )
#endif

#if !defined(OS_POSIX)
// This condition must be kept in sync with the one in
// ipc_message_utils.h, but this dummy definition of
// base::FileDescriptor acts as a static assert that we only get one
// def or the other (or neither, in which case code using
// FileDescriptor fails to build)
namespace base { struct FileDescriptor { }; }
#endif

namespace mozilla {

// This is a cross-platform approximation to HANDLE, which we expect
// to be typedef'd to void* or thereabouts.
typedef uintptr_t WindowsHandle;

// XXX there are out of place and might be generally useful.  Could
// move to nscore.h or something.
struct void_t {
  bool operator==(const void_t&) const { return true; }
};
struct null_t {
  bool operator==(const null_t&) const { return true; }
};

struct SerializedStructuredCloneBuffer
{
  SerializedStructuredCloneBuffer()
  : data(nullptr), dataLength(0)
  { }

  explicit SerializedStructuredCloneBuffer(const JSAutoStructuredCloneBuffer& aOther)
  {
    *this = aOther;
  }

  bool
  operator==(const SerializedStructuredCloneBuffer& aOther) const
  {
    return this->data == aOther.data &&
           this->dataLength == aOther.dataLength;
  }

  SerializedStructuredCloneBuffer&
  operator=(const JSAutoStructuredCloneBuffer& aOther)
  {
    data = aOther.data();
    dataLength = aOther.nbytes();
    return *this;
  }

  uint64_t* data;
  size_t dataLength;
};

struct OwningSerializedStructuredCloneBuffer : public SerializedStructuredCloneBuffer
{
  OwningSerializedStructuredCloneBuffer()
  {}

  OwningSerializedStructuredCloneBuffer(const OwningSerializedStructuredCloneBuffer&) = delete;

  explicit OwningSerializedStructuredCloneBuffer(const JSAutoStructuredCloneBuffer& aOther)
   : SerializedStructuredCloneBuffer(aOther)
  {}

  ~OwningSerializedStructuredCloneBuffer()
  {
    if (data) {
      js_free(data);
    }
  }

  OwningSerializedStructuredCloneBuffer&
  operator=(const JSAutoStructuredCloneBuffer& aOther)
  {
    SerializedStructuredCloneBuffer::operator=(aOther);
    return *this;
  }

  OwningSerializedStructuredCloneBuffer&
  operator=(const OwningSerializedStructuredCloneBuffer& aOther) = delete;
};

} // namespace mozilla

namespace IPC {

/**
 * Generic enum serializer.
 *
 * Consider using the specializations below, such as ContiguousEnumSerializer.
 *
 * This is a generic serializer for any enum type used in IPDL.
 * Programmers can define ParamTraits<E> for enum type E by deriving
 * EnumSerializer<E, MyEnumValidator> where MyEnumValidator is a struct
 * that has to define a static IsLegalValue function returning whether
 * a given value is a legal value of the enum type at hand.
 *
 * \sa https://developer.mozilla.org/en/IPDL/Type_Serialization
 */
template <typename E, typename EnumValidator>
struct EnumSerializer {
  typedef E paramType;
  typedef typename mozilla::UnsignedStdintTypeForSize<sizeof(paramType)>::Type
          uintParamType;

  static void Write(Message* aMsg, const paramType& aValue) {
    MOZ_ASSERT(EnumValidator::IsLegalValue(aValue));
    WriteParam(aMsg, uintParamType(aValue));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult) {
    uintParamType value;
    if(!ReadParam(aMsg, aIter, &value) ||
       !EnumValidator::IsLegalValue(paramType(value))) {
      return false;
    }
    *aResult = paramType(value);
    return true;
  }
};

template <typename E,
          E MinLegal,
          E HighBound>
class ContiguousEnumValidator
{
  // Silence overzealous -Wtype-limits bug in GCC fixed in GCC 4.8:
  // "comparison of unsigned expression >= 0 is always true"
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11856
  template <typename T>
  static bool IsLessThanOrEqual(T a, T b) { return a <= b; }

public:
  static bool IsLegalValue(E e)
  {
    return IsLessThanOrEqual(MinLegal, e) && e < HighBound;
  }
};

template <typename E,
          E AllBits>
struct BitFlagsEnumValidator
{
  static bool IsLegalValue(E e)
  {
    return (e & AllBits) == e;
  }
};

/**
 * Specialization of EnumSerializer for enums with contiguous enum values.
 *
 * Provide two values: MinLegal, HighBound. An enum value x will be
 * considered legal if MinLegal <= x < HighBound.
 *
 * For example, following is definition of serializer for enum type FOO.
 * \code
 * enum FOO { FOO_FIRST, FOO_SECOND, FOO_LAST, NUM_FOO };
 *
 * template <>
 * struct ParamTraits<FOO>:
 *     public ContiguousEnumSerializer<FOO, FOO_FIRST, NUM_FOO> {};
 * \endcode
 * FOO_FIRST, FOO_SECOND, and FOO_LAST are valid value.
 */
template <typename E,
          E MinLegal,
          E HighBound>
struct ContiguousEnumSerializer
  : EnumSerializer<E,
                   ContiguousEnumValidator<E, MinLegal, HighBound>>
{};

/**
 * Specialization of EnumSerializer for enums representing bit flags.
 *
 * Provide one value: AllBits. An enum value x will be
 * considered legal if (x & AllBits) == x;
 *
 * Example:
 * \code
 * enum FOO {
 *   FOO_FIRST =  1 << 0,
 *   FOO_SECOND = 1 << 1,
 *   FOO_LAST =   1 << 2,
 *   ALL_BITS =   (1 << 3) - 1
 * };
 *
 * template <>
 * struct ParamTraits<FOO>:
 *     public BitFlagsEnumSerializer<FOO, FOO::ALL_BITS> {};
 * \endcode
 */
template <typename E,
          E AllBits>
struct BitFlagsEnumSerializer
  : EnumSerializer<E,
                   BitFlagsEnumValidator<E, AllBits>>
{};

template <>
struct ParamTraits<base::ChildPrivileges>
  : public ContiguousEnumSerializer<base::ChildPrivileges,
                                    base::PRIVILEGES_DEFAULT,
                                    base::PRIVILEGES_LAST>
{ };

template<>
struct ParamTraits<int8_t>
{
  typedef int8_t paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outp;
    if (!aMsg->ReadBytes(aIter, &outp, sizeof(*aResult)))
      return false;

    *aResult = *reinterpret_cast<const paramType*>(outp);
    return true;
  }
};

template<>
struct ParamTraits<uint8_t>
{
  typedef uint8_t paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outp;
    if (!aMsg->ReadBytes(aIter, &outp, sizeof(*aResult)))
      return false;

    *aResult = *reinterpret_cast<const paramType*>(outp);
    return true;
  }
};

#if !defined(OS_POSIX)
// See above re: keeping definitions in sync
template<>
struct ParamTraits<base::FileDescriptor>
{
  typedef base::FileDescriptor paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    NS_RUNTIMEABORT("FileDescriptor isn't meaningful on this platform");
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult) {
    NS_RUNTIMEABORT("FileDescriptor isn't meaningful on this platform");
    return false;
  }
};
#endif  // !defined(OS_POSIX)

template <>
struct ParamTraits<nsACString>
{
  typedef nsACString paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    bool isVoid = aParam.IsVoid();
    aMsg->WriteBool(isVoid);

    if (isVoid)
      // represents a nullptr pointer
      return;

    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isVoid;
    if (!aMsg->ReadBool(aIter, &isVoid))
      return false;

    if (isVoid) {
      aResult->SetIsVoid(true);
      return true;
    }

    uint32_t length;
    if (ReadParam(aMsg, aIter, &length)) {
      const char* buf;
      if (aMsg->ReadBytes(aIter, &buf, length)) {
        aResult->Assign(buf, length);
        return true;
      }
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    if (aParam.IsVoid())
      aLog->append(L"(NULL)");
    else
      aLog->append(UTF8ToWide(aParam.BeginReading()));
  }
};

template <>
struct ParamTraits<nsAString>
{
  typedef nsAString paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    bool isVoid = aParam.IsVoid();
    aMsg->WriteBool(isVoid);

    if (isVoid)
      // represents a nullptr pointer
      return;

    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length * sizeof(char16_t));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isVoid;
    if (!aMsg->ReadBool(aIter, &isVoid))
      return false;

    if (isVoid) {
      aResult->SetIsVoid(true);
      return true;
    }

    uint32_t length;
    if (ReadParam(aMsg, aIter, &length)) {
      const char16_t* buf;
      if (aMsg->ReadBytes(aIter, reinterpret_cast<const char**>(&buf),
                       length * sizeof(char16_t))) {
        aResult->Assign(buf, length);
        return true;
      }
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    if (aParam.IsVoid())
      aLog->append(L"(NULL)");
    else {
#ifdef WCHAR_T_IS_UTF16
      aLog->append(reinterpret_cast<const wchar_t*>(aParam.BeginReading()));
#else
      uint32_t length = aParam.Length();
      for (uint32_t index = 0; index < length; index++) {
        aLog->push_back(std::wstring::value_type(aParam[index]));
      }
#endif
    }
  }
};

template <>
struct ParamTraits<nsCString> : ParamTraits<nsACString>
{
  typedef nsCString paramType;
};

template <>
struct ParamTraits<nsLiteralCString> : ParamTraits<nsACString>
{
  typedef nsLiteralCString paramType;
};

#ifdef MOZILLA_INTERNAL_API

template<>
struct ParamTraits<nsAutoCString> : ParamTraits<nsCString>
{
  typedef nsAutoCString paramType;
};

#endif  // MOZILLA_INTERNAL_API

template <>
struct ParamTraits<nsString> : ParamTraits<nsAString>
{
  typedef nsString paramType;
};

template <>
struct ParamTraits<nsLiteralString> : ParamTraits<nsAString>
{
  typedef nsLiteralString paramType;
};

template <typename E>
struct ParamTraits<FallibleTArray<E> >
{
  typedef FallibleTArray<E> paramType;

  // We write arrays of integer or floating-point data using a single pickling
  // call, rather than writing each element individually.  We deliberately do
  // not use mozilla::IsPod here because it is perfectly reasonable to have
  // a data structure T for which IsPod<T>::value is true, yet also have a
  // ParamTraits<T> specialization.
  static const bool sUseWriteBytes = (mozilla::IsIntegral<E>::value ||
                                      mozilla::IsFloatingPoint<E>::value);

  // Compute the byte length for |aNumElements| of type E.  If that length
  // would overflow an int, return false.  Otherwise, return true and place
  // the byte length in |aTotalLength|.
  //
  // Pickle's ReadBytes/WriteBytes interface takes lengths in ints, hence this
  // dance.
  static bool ByteLengthIsValid(size_t aNumElements, int* aTotalLength) {
    static_assert(sizeof(int) == sizeof(int32_t), "int is an unexpected size!");

    // nsTArray only handles sizes up to INT32_MAX.
    if (aNumElements > size_t(INT32_MAX)) {
      return false;
    }

    int64_t numBytes = static_cast<int64_t>(aNumElements) * sizeof(E);
    if (numBytes > int64_t(INT32_MAX)) {
      return false;
    }

    *aTotalLength = static_cast<int>(numBytes);
    return true;
  }

  static void Write(Message* aMsg, const paramType& aParam)
  {
    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);

    if (sUseWriteBytes) {
      int pickledLength = 0;
      mozilla::DebugOnly<bool> valid = ByteLengthIsValid(length, &pickledLength);
      MOZ_ASSERT(valid);
      aMsg->WriteBytes(aParam.Elements(), pickledLength);
    } else {
      for (uint32_t index = 0; index < length; index++) {
        WriteParam(aMsg, aParam[index]);
      }
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint32_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    if (sUseWriteBytes) {
      int pickledLength = 0;
      if (!ByteLengthIsValid(length, &pickledLength)) {
        return false;
      }

      const char* outdata;
      if (!aMsg->ReadBytes(aIter, &outdata, pickledLength)) {
        return false;
      }

      E* elements = aResult->AppendElements(length, mozilla::fallible);
      if (!elements) {
        return false;
      }

      memcpy(elements, outdata, pickledLength);
    } else {
      if (!aResult->SetCapacity(length, mozilla::fallible)) {
        return false;
      }

      for (uint32_t index = 0; index < length; index++) {
        E* element = aResult->AppendElement(mozilla::fallible);
        MOZ_ASSERT(element);
        if (!ReadParam(aMsg, aIter, element)) {
          return false;
        }
      }
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    for (uint32_t index = 0; index < aParam.Length(); index++) {
      if (index) {
        aLog->append(L" ");
      }
      LogParam(aParam[index], aLog);
    }
  }
};

template<typename E>
struct ParamTraits<InfallibleTArray<E> >
{
  typedef InfallibleTArray<E> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<const FallibleTArray<E>&>(aParam));
  }

  // deserialize the array fallibly, but return an InfallibleTArray
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    FallibleTArray<E> temp;
    if (!ReadParam(aMsg, aIter, &temp))
      return false;

    aResult->SwapElements(temp);
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(static_cast<const FallibleTArray<E>&>(aParam), aLog);
  }
};

template<typename E, size_t N>
struct ParamTraits<nsAutoTArray<E, N>> : ParamTraits<nsTArray<E>>
{
  typedef nsAutoTArray<E, N> paramType;
};

template<>
struct ParamTraits<float>
{
  typedef float paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(paramType));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outFloat;
    if (!aMsg->ReadBytes(aIter, &outFloat, sizeof(float)))
      return false;
    *aResult = *reinterpret_cast<const float*>(outFloat);
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"%g", aParam));
  }
};

template <>
struct ParamTraits<nsCSSProperty>
  : public ContiguousEnumSerializer<nsCSSProperty,
                                    eCSSProperty_UNKNOWN,
                                    eCSSProperty_COUNT>
{};

template<>
struct ParamTraits<mozilla::void_t>
{
  typedef mozilla::void_t paramType;
  static void Write(Message* aMsg, const paramType& aParam) { }
  static bool
  Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    *aResult = paramType();
    return true;
  }
};

template<>
struct ParamTraits<mozilla::null_t>
{
  typedef mozilla::null_t paramType;
  static void Write(Message* aMsg, const paramType& aParam) { }
  static bool
  Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    *aResult = paramType();
    return true;
  }
};

template<>
struct ParamTraits<nsID>
{
  typedef nsID paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.m0);
    WriteParam(aMsg, aParam.m1);
    WriteParam(aMsg, aParam.m2);
    for (unsigned int i = 0; i < mozilla::ArrayLength(aParam.m3); i++) {
      WriteParam(aMsg, aParam.m3[i]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if(!ReadParam(aMsg, aIter, &(aResult->m0)) ||
       !ReadParam(aMsg, aIter, &(aResult->m1)) ||
       !ReadParam(aMsg, aIter, &(aResult->m2)))
      return false;

    for (unsigned int i = 0; i < mozilla::ArrayLength(aResult->m3); i++)
      if (!ReadParam(aMsg, aIter, &(aResult->m3[i])))
        return false;

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(L"{");
    aLog->append(StringPrintf(L"%8.8X-%4.4X-%4.4X-",
                              aParam.m0,
                              aParam.m1,
                              aParam.m2));
    for (unsigned int i = 0; i < mozilla::ArrayLength(aParam.m3); i++)
      aLog->append(StringPrintf(L"%2.2X", aParam.m3[i]));
    aLog->append(L"}");
  }
};

template<>
struct ParamTraits<mozilla::TimeDuration>
{
  typedef mozilla::TimeDuration paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mValue);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mValue);
  };
};

template<>
struct ParamTraits<mozilla::TimeStamp>
{
  typedef mozilla::TimeStamp paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mValue);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mValue);
  };
};

#ifdef XP_WIN

template<>
struct ParamTraits<mozilla::TimeStampValue>
{
  typedef mozilla::TimeStampValue paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mGTC);
    WriteParam(aMsg, aParam.mQPC);
    WriteParam(aMsg, aParam.mHasQPC);
    WriteParam(aMsg, aParam.mIsNull);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mGTC) &&
            ReadParam(aMsg, aIter, &aResult->mQPC) &&
            ReadParam(aMsg, aIter, &aResult->mHasQPC) &&
            ReadParam(aMsg, aIter, &aResult->mIsNull));
  }
};

#endif

template <>
struct ParamTraits<mozilla::SerializedStructuredCloneBuffer>
{
  typedef mozilla::SerializedStructuredCloneBuffer paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.dataLength);
    if (aParam.dataLength) {
      // Structured clone data must be 64-bit aligned.
      aMsg->WriteBytes(aParam.data, aParam.dataLength, sizeof(uint64_t));
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->dataLength)) {
      return false;
    }

    if (aResult->dataLength) {
      const char** buffer =
        const_cast<const char**>(reinterpret_cast<char**>(&aResult->data));
      // Structured clone data must be 64-bit aligned.
      if (!aMsg->ReadBytes(aIter, buffer, aResult->dataLength,
                           sizeof(uint64_t))) {
        return false;
      }
    } else {
      aResult->data = nullptr;
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.dataLength, aLog);
  }
};

template <>
struct ParamTraits<mozilla::OwningSerializedStructuredCloneBuffer>
  : public ParamTraits<mozilla::SerializedStructuredCloneBuffer>
{
  typedef mozilla::OwningSerializedStructuredCloneBuffer paramType;

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ParamTraits<mozilla::SerializedStructuredCloneBuffer>::Read(aMsg, aIter, aResult)) {
      return false;
    }

    if (aResult->data) {
      uint64_t* data = static_cast<uint64_t*>(js_malloc(aResult->dataLength));
      if (!data) {
        return false;
      }
      memcpy(data, aResult->data, aResult->dataLength);
      aResult->data = data;
    }

    return true;
  }
};

template <>
struct ParamTraits<nsIWidget::TouchPointerState>
  : public BitFlagsEnumSerializer<nsIWidget::TouchPointerState,
                                  nsIWidget::TouchPointerState::ALL_BITS>
{
};

template<class T>
struct ParamTraits< mozilla::Maybe<T> >
{
  typedef mozilla::Maybe<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    if (param.isSome()) {
      WriteParam(msg, true);
      WriteParam(msg, param.value());
    } else {
      WriteParam(msg, false);
    }
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    bool isSome;
    if (!ReadParam(msg, iter, &isSome)) {
      return false;
    }
    if (isSome) {
      T tmp;
      if (!ReadParam(msg, iter, &tmp)) {
        return false;
      }
      *result = mozilla::Some(mozilla::Move(tmp));
    } else {
      *result = mozilla::Nothing();
    }
    return true;
  }
};

} /* namespace IPC */

#endif /* __IPC_GLUE_IPCMESSAGEUTILS_H__ */
