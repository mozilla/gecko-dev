/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_Variant_h__
#define mozilla_storage_Variant_h__

#include "nsIInterfaceRequestor.h"
#include "nsIVariant.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#define VARIANT_BASE_IID                             \
  { /* 78888042-0fa3-4f7a-8b19-7996f99bf1aa */       \
    0x78888042, 0x0fa3, 0x4f7a, {                    \
      0x8b, 0x19, 0x79, 0x96, 0xf9, 0x9b, 0xf1, 0xaa \
    }                                                \
  }

/**
 * This class is used by the storage module whenever an nsIVariant needs to be
 * returned.  We provide traits for the basic sqlite types to make use easier.
 * The following types map to the indicated sqlite type:
 * int64_t   -> INTEGER (use IntegerVariant)
 * double    -> FLOAT (use FloatVariant)
 * nsString  -> TEXT (use TextVariant)
 * nsCString -> TEXT (use UTF8TextVariant)
 * uint8_t[] -> BLOB (use BlobVariant)
 * nullptr   -> NULL (use NullVariant)
 * int64_t[] -> ARRAY (use ArrayOfIntegersVariant)
 * double[]  -> ARRAY (use ArrayOfDoublesVariant)
 * nsCString[]  -> ARRAY (use ArrayOfUTF8StringsVariant)
 *
 * The kvstore component also reuses this class as a common implementation
 * of a simple threadsafe variant for the storage of primitive values only.
 * The BooleanVariant type has been introduced for kvstore use cases and should
 * be enhanced to provide full boolean variant support for mozStorage.
 *
 * Bug 1494102 tracks that work.
 */

namespace mozilla::storage {

////////////////////////////////////////////////////////////////////////////////
//// Base Class

class Variant_base : public nsIVariant, public nsIInterfaceRequestor {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIVARIANT
  NS_DECLARE_STATIC_IID_ACCESSOR(VARIANT_BASE_IID)

  NS_IMETHOD
  GetInterface(const nsIID& aIID, void** aResult) override {
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nullptr;

    // This is used to recognize nsIVariant instances derived from Variant_base
    // from other implementations like XPCVariant that may not be thread-safe.
    if (aIID.Equals(VARIANT_BASE_IID) || aIID.Equals(NS_GET_IID(nsIVariant))) {
      nsCOMPtr<nsIVariant> result(static_cast<nsIVariant*>(this));
      result.forget(aResult);
      return NS_OK;
    }

    return NS_NOINTERFACE;
  }

 protected:
  virtual ~Variant_base() = default;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Variant_base, VARIANT_BASE_IID)

////////////////////////////////////////////////////////////////////////////////
//// Traits

/**
 * Generics
 */

template <typename DataType>
struct variant_traits {
  static inline uint16_t type() { return nsIDataType::VTYPE_EMPTY; }
};

template <typename DataType, bool Adopting = false>
struct variant_storage_traits {
  using ConstructorType = DataType;
  using StorageType = DataType;
  static inline void storage_conversion(const ConstructorType aData,
                                        StorageType* _storage) {
    *_storage = aData;
  }

  static inline void destroy(const StorageType& _storage) {}
};

#define NO_CONVERSION return NS_ERROR_CANNOT_CONVERT_DATA;

template <typename DataType, bool Adopting = false>
struct variant_boolean_traits {
  using StorageType =
      typename variant_storage_traits<DataType, Adopting>::StorageType;
  static inline nsresult asBool(const StorageType&, bool*) { NO_CONVERSION }
};

template <typename DataType, bool Adopting = false>
struct variant_integer_traits {
  using StorageType =
      typename variant_storage_traits<DataType, Adopting>::StorageType;
  static inline nsresult asInt32(const StorageType&, int32_t*) { NO_CONVERSION }
  static inline nsresult asInt64(const StorageType&, int64_t*) { NO_CONVERSION }
};

template <typename DataType, bool Adopting = false>
struct variant_float_traits {
  using StorageType =
      typename variant_storage_traits<DataType, Adopting>::StorageType;
  static inline nsresult asDouble(const StorageType&, double*) { NO_CONVERSION }
};

template <typename DataType, bool Adopting = false>
struct variant_text_traits {
  using StorageType =
      typename variant_storage_traits<DataType, Adopting>::StorageType;
  static inline nsresult asUTF8String(const StorageType&, nsACString&) {
    NO_CONVERSION
  }
  static inline nsresult asString(const StorageType&, nsAString&) {
    NO_CONVERSION
  }
};

template <typename DataType, bool Adopting = false>
struct variant_array_traits {
  using StorageType =
      typename variant_storage_traits<DataType, Adopting>::StorageType;
  static inline nsresult asArray(const StorageType&, uint16_t*, uint32_t*,
                                 void**) {
    NO_CONVERSION
  }
};

#undef NO_CONVERSION

/**
 * BOOLEAN type
 */

template <>
struct variant_traits<bool> {
  static inline uint16_t type() { return nsIDataType::VTYPE_BOOL; }
};
template <>
struct variant_boolean_traits<bool> {
  static inline nsresult asBool(bool aValue, bool* _result) {
    *_result = aValue;
    return NS_OK;
  }

  // NB: It might be worth also providing conversions to int types.

  // NB: It'd be nice to implement asBool conversions for 0 and 1, too.
  // That would let us clean up some conversions in Places, such as:
  // https://searchfox.org/mozilla-central/rev/0640ea80fbc8d48f8b197cd363e2535c95a15eb3/toolkit/components/places/SQLFunctions.cpp#564-565
  // https://searchfox.org/mozilla-central/rev/0640ea80fbc8d48f8b197cd363e2535c95a15eb3/toolkit/components/places/SQLFunctions.cpp#1057
  // https://searchfox.org/mozilla-central/rev/0640ea80fbc8d48f8b197cd363e2535c95a15eb3/toolkit/components/places/nsNavHistory.cpp#3189
};

/**
 * INTEGER types
 */

template <>
struct variant_traits<int64_t> {
  static inline uint16_t type() { return nsIDataType::VTYPE_INT64; }
};
template <>
struct variant_integer_traits<int64_t> {
  static inline nsresult asInt32(int64_t aValue, int32_t* _result) {
    if (aValue > INT32_MAX || aValue < INT32_MIN) {
      return NS_ERROR_CANNOT_CONVERT_DATA;
    }

    *_result = static_cast<int32_t>(aValue);
    return NS_OK;
  }
  static inline nsresult asInt64(int64_t aValue, int64_t* _result) {
    *_result = aValue;
    return NS_OK;
  }
};
// xpcvariant just calls get double for integers...
template <>
struct variant_float_traits<int64_t> {
  static inline nsresult asDouble(int64_t aValue, double* _result) {
    *_result = double(aValue);
    return NS_OK;
  }
};

/**
 * FLOAT types
 */

template <>
struct variant_traits<double> {
  static inline uint16_t type() { return nsIDataType::VTYPE_DOUBLE; }
};
template <>
struct variant_float_traits<double> {
  static inline nsresult asDouble(double aValue, double* _result) {
    *_result = aValue;
    return NS_OK;
  }
};

/**
 * TEXT types
 */

template <>
struct variant_traits<nsString> {
  static inline uint16_t type() { return nsIDataType::VTYPE_ASTRING; }
};
template <>
struct variant_storage_traits<nsString> {
  using ConstructorType = const nsAString&;
  using StorageType = nsString;
  static inline void storage_conversion(ConstructorType aText,
                                        StorageType* _outData) {
    *_outData = aText;
  }
  static inline void destroy(const StorageType& _outData) {}
};
template <>
struct variant_text_traits<nsString> {
  static inline nsresult asUTF8String(const nsString& aValue,
                                      nsACString& _result) {
    CopyUTF16toUTF8(aValue, _result);
    return NS_OK;
  }
  static inline nsresult asString(const nsString& aValue, nsAString& _result) {
    _result = aValue;
    return NS_OK;
  }
};

template <>
struct variant_traits<nsCString> {
  static inline uint16_t type() { return nsIDataType::VTYPE_UTF8STRING; }
};
template <>
struct variant_storage_traits<nsCString> {
  using ConstructorType = const nsACString&;
  using StorageType = nsCString;
  static inline void storage_conversion(ConstructorType aText,
                                        StorageType* _outData) {
    *_outData = aText;
  }
  static inline void destroy(const StorageType& aData) {}
};
template <>
struct variant_text_traits<nsCString> {
  static inline nsresult asUTF8String(const nsCString& aValue,
                                      nsACString& _result) {
    _result = aValue;
    return NS_OK;
  }
  static inline nsresult asString(const nsCString& aValue, nsAString& _result) {
    CopyUTF8toUTF16(aValue, _result);
    return NS_OK;
  }
};

/**
 * ARRAY types
 */

// NOLINTBEGIN(bugprone-macro-parentheses)

#define SPECIALIZE_ARRAY_TO_NUMERIC_VARIANT(Type, DataType)                    \
  template <>                                                                  \
  struct variant_traits<Type[]> {                                              \
    static inline uint16_t type() { return nsIDataType::VTYPE_ARRAY; }         \
  };                                                                           \
                                                                               \
  template <>                                                                  \
  struct variant_storage_traits<Type[], false> {                               \
    using ConstructorType = std::pair<const void*, int>;                       \
    using StorageType = FallibleTArray<Type>;                                  \
    static inline void storage_conversion(ConstructorType aArrayAndLength,     \
                                          StorageType* _outData) {             \
      _outData->Clear();                                                       \
      MOZ_ALWAYS_TRUE(_outData->AppendElements(                                \
          static_cast<const Type*>(aArrayAndLength.first),                     \
          aArrayAndLength.second, fallible));                                  \
    }                                                                          \
    static inline void destroy(const StorageType& _outData) {}                 \
  };                                                                           \
                                                                               \
  template <>                                                                  \
  struct variant_storage_traits<Type[], true> {                                \
    using ConstructorType = std::pair<Type*, int>;                             \
    using StorageType = std::pair<Type*, int>;                                 \
    static inline void storage_conversion(ConstructorType aArrayAndLength,     \
                                          StorageType* _outData) {             \
      *_outData = aArrayAndLength;                                             \
    }                                                                          \
    static inline void destroy(StorageType& aArrayAndLength) {                 \
      if (aArrayAndLength.first) {                                             \
        free(aArrayAndLength.first);                                           \
        aArrayAndLength.first = nullptr;                                       \
      }                                                                        \
    }                                                                          \
  };                                                                           \
                                                                               \
  template <>                                                                  \
  struct variant_array_traits<Type[], false> {                                 \
    static inline nsresult asArray(FallibleTArray<Type>& aData,                \
                                   uint16_t* _type, uint32_t* _size,           \
                                   void** _result) {                           \
      /* For empty arrays, we return nullptr. */                               \
      if (aData.Length() == 0) {                                               \
        *_result = nullptr;                                                    \
        *_type = DataType;                                                     \
        *_size = 0;                                                            \
        return NS_OK;                                                          \
      }                                                                        \
      /* Otherwise, we copy the array. */                                      \
      *_result = moz_xmemdup(aData.Elements(), aData.Length() * sizeof(Type)); \
      *_type = DataType;                                                       \
      *_size = aData.Length();                                                 \
      return NS_OK;                                                            \
    }                                                                          \
  };                                                                           \
                                                                               \
  template <>                                                                  \
  struct variant_array_traits<Type[], true> {                                  \
    static inline nsresult asArray(std::pair<Type*, int>& aData,               \
                                   uint16_t* _type, uint32_t* _size,           \
                                   void** _result) {                           \
      /* For empty arrays, we return nullptr. */                               \
      if (aData.second == 0) {                                                 \
        *_result = nullptr;                                                    \
        *_type = DataType;                                                     \
        *_size = 0;                                                            \
        return NS_OK;                                                          \
      }                                                                        \
      /* Otherwise, transfer the data out. */                                  \
      *_result = aData.first;                                                  \
      aData.first = nullptr;                                                   \
      /* If we asked for it twice, better not use adopting! */                 \
      MOZ_ASSERT(*_result);                                                    \
      *_type = DataType;                                                       \
      *_size = aData.second;                                                   \
      return NS_OK;                                                            \
    }                                                                          \
  }

// NOLINTEND(bugprone-macro-parentheses)

SPECIALIZE_ARRAY_TO_NUMERIC_VARIANT(uint8_t, nsIDataType::VTYPE_UINT8);
SPECIALIZE_ARRAY_TO_NUMERIC_VARIANT(int64_t, nsIDataType::VTYPE_INT64);
SPECIALIZE_ARRAY_TO_NUMERIC_VARIANT(double, nsIDataType::VTYPE_DOUBLE);

template <>
struct variant_traits<nsCString[]> {
  static inline uint16_t type() { return nsIDataType::VTYPE_ARRAY; }
};

template <>
struct variant_storage_traits<nsCString[], false> {
  using ConstructorType = std::pair<const void*, int>;
  using StorageType = FallibleTArray<nsCString>;
  static inline void storage_conversion(ConstructorType aArrayAndLength,
                                        StorageType* _outData) {
    _outData->Clear();
    if (!_outData->SetCapacity(aArrayAndLength.second, fallible)) {
      MOZ_ASSERT_UNREACHABLE("Cannot allocate.");
      return;
    }
    // We can avoid copying the strings as we're asking SQLite to do it on bind
    // by using SQLITE_TRANSIENT.
    const nsCString* str = static_cast<const nsCString*>(aArrayAndLength.first);
    for (int32_t i = 0; i < aArrayAndLength.second; ++i, str++) {
      MOZ_ALWAYS_TRUE(_outData->AppendElement(*str, fallible));
    }
  }

  static inline void destroy(const StorageType& _outData) {}
};

template <>
struct variant_array_traits<nsCString[], false> {
  static inline nsresult asArray(FallibleTArray<nsCString>& aData,
                                 uint16_t* _type, uint32_t* _size,
                                 void** _result) {
    // For empty arrays, we return nullptr.
    if (aData.Length() == 0) {
      *_result = nullptr;
      *_type = nsIDataType::VTYPE_UTF8STRING;
      *_size = 0;
      return NS_OK;
    }
    // Otherwise, we copy the array.
    // This memory will be freed up after Sqlite made its own copy in
    // sqlite3_T_array. The string buffers are owned by mData.
    const char** strings =
        (const char**)moz_xmalloc(sizeof(char*) * aData.Length());
    const char** iter = strings;
    for (const nsCString& str : aData) {
      *iter = str.get();
      iter++;
    }
    *_result = strings;
    *_type = nsIDataType::VTYPE_UTF8STRING;
    *_size = aData.Length();
    return NS_OK;
  }
};

/**
 * nullptr type
 */

class NullVariant : public Variant_base {
 public:
  uint16_t GetDataType() override { return nsIDataType::VTYPE_EMPTY; }

  NS_IMETHOD GetAsAUTF8String(nsACString& _str) override {
    // Return a void string.
    _str.SetIsVoid(true);
    return NS_OK;
  }

  NS_IMETHOD GetAsAString(nsAString& _str) override {
    // Return a void string.
    _str.SetIsVoid(true);
    return NS_OK;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Template Implementation

template <typename DataType, bool Adopting = false>
class Variant final : public Variant_base {
  ~Variant() { variant_storage_traits<DataType, Adopting>::destroy(mData); }

 public:
  explicit Variant(
      const typename variant_storage_traits<DataType, Adopting>::ConstructorType
          aData) {
    variant_storage_traits<DataType, Adopting>::storage_conversion(aData,
                                                                   &mData);
  }

  uint16_t GetDataType() override { return variant_traits<DataType>::type(); }

  NS_IMETHOD GetAsBool(bool* _boolean) override {
    return variant_boolean_traits<DataType, Adopting>::asBool(mData, _boolean);
  }

  NS_IMETHOD GetAsInt32(int32_t* _integer) override {
    return variant_integer_traits<DataType, Adopting>::asInt32(mData, _integer);
  }

  NS_IMETHOD GetAsInt64(int64_t* _integer) override {
    return variant_integer_traits<DataType, Adopting>::asInt64(mData, _integer);
  }

  NS_IMETHOD GetAsDouble(double* _double) override {
    return variant_float_traits<DataType, Adopting>::asDouble(mData, _double);
  }

  NS_IMETHOD GetAsAUTF8String(nsACString& _str) override {
    return variant_text_traits<DataType, Adopting>::asUTF8String(mData, _str);
  }

  NS_IMETHOD GetAsAString(nsAString& _str) override {
    return variant_text_traits<DataType, Adopting>::asString(mData, _str);
  }

  NS_IMETHOD GetAsArray(uint16_t* _type, nsIID*, uint32_t* _size,
                        void** _data) override {
    return variant_array_traits<DataType, Adopting>::asArray(mData, _type,
                                                             _size, _data);
  }

 protected:
  typename variant_storage_traits<DataType, Adopting>::StorageType mData;
};

////////////////////////////////////////////////////////////////////////////////
//// Handy typedefs!  Use these for the right mapping.

// Currently, BooleanVariant is only useful for kvstore.
// Bug 1494102 tracks implementing full boolean variant support for mozStorage.
using BooleanVariant = Variant<bool>;

using IntegerVariant = Variant<int64_t>;
using FloatVariant = Variant<double>;
using TextVariant = Variant<nsString>;
using UTF8TextVariant = Variant<nsCString>;
using BlobVariant = Variant<uint8_t[], false>;
using AdoptedBlobVariant = Variant<uint8_t[], true>;
using ArrayOfIntegersVariant = Variant<int64_t[], false>;
using AdoptedArrayOfIntegersVariant = Variant<int64_t[], true>;
using ArrayOfDoublesVariant = Variant<double[], false>;
using AdoptedArrayOfDoublesVariant = Variant<double[], true>;
using ArrayOfUTF8StringsVariant = Variant<nsCString[], false>;

}  // namespace mozilla::storage

#include "Variant_inl.h"

#endif  // mozilla_storage_Variant_h__
