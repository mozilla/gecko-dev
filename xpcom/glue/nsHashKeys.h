/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTHashKeys_h__
#define nsTHashKeys_h__

#include "nsID.h"
#include "nsISupports.h"
#include "nsIHashable.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "pldhash.h"
#include <new>

#include "nsStringGlue.h"
#include "nsCRTGlue.h"
#include "nsUnicharUtils.h"

#include <stdlib.h>
#include <string.h>

#include "mozilla/HashFunctions.h"
#include "mozilla/Move.h"

namespace mozilla {

// These are defined analogously to the HashString overloads in mfbt.

inline uint32_t
HashString(const nsAString& aStr)
{
  return HashString(aStr.BeginReading(), aStr.Length());
}

inline uint32_t
HashString(const nsACString& aStr)
{
  return HashString(aStr.BeginReading(), aStr.Length());
}

} // namespace mozilla

/** @file nsHashKeys.h
 * standard HashKey classes for nsBaseHashtable and relatives. Each of these
 * classes follows the nsTHashtable::EntryType specification
 *
 * Lightweight keytypes provided here:
 * nsStringHashKey
 * nsCStringHashKey
 * nsUint32HashKey
 * nsUint64HashKey
 * nsFloatHashKey
 * nsPtrHashKey
 * nsClearingPtrHashKey
 * nsVoidPtrHashKey
 * nsClearingVoidPtrHashKey
 * nsISupportsHashKey
 * nsIDHashKey
 * nsDepCharHashKey
 * nsCharPtrHashKey
 * nsUnicharPtrHashKey
 * nsHashableHashKey
 * nsGenericHashKey
 */

/**
 * hashkey wrapper using nsAString KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsStringHashKey : public PLDHashEntryHdr
{
public:
  typedef const nsAString& KeyType;
  typedef const nsAString* KeyTypePointer;

  explicit nsStringHashKey(KeyTypePointer aStr) : mStr(*aStr) {}
  nsStringHashKey(const nsStringHashKey& aToCopy) : mStr(aToCopy.mStr) {}
  ~nsStringHashKey() {}

  KeyType GetKey() const { return mStr; }
  bool KeyEquals(const KeyTypePointer aKey) const
  {
    return mStr.Equals(*aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(const KeyTypePointer aKey)
  {
    return mozilla::HashString(*aKey);
  }

#ifdef MOZILLA_INTERNAL_API
  // To avoid double-counting, only measure the string if it is unshared.
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return GetKey().SizeOfExcludingThisMustBeUnshared(aMallocSizeOf);
  }
#endif

  enum { ALLOW_MEMMOVE = true };

private:
  const nsString mStr;
};

#ifdef MOZILLA_INTERNAL_API

/**
 * hashkey wrapper using nsAString KeyType
 *
 * This is internal-API only because nsCaseInsensitiveStringComparator is
 * internal-only.
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsStringCaseInsensitiveHashKey : public PLDHashEntryHdr
{
public:
  typedef const nsAString& KeyType;
  typedef const nsAString* KeyTypePointer;

  explicit nsStringCaseInsensitiveHashKey(KeyTypePointer aStr)
    : mStr(*aStr)
  {
    // take it easy just deal HashKey
  }
  nsStringCaseInsensitiveHashKey(const nsStringCaseInsensitiveHashKey& aToCopy)
    : mStr(aToCopy.mStr)
  {
  }
  ~nsStringCaseInsensitiveHashKey() {}

  KeyType GetKey() const { return mStr; }
  bool KeyEquals(const KeyTypePointer aKey) const
  {
    return mStr.Equals(*aKey, nsCaseInsensitiveStringComparator());
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(const KeyTypePointer aKey)
  {
    nsAutoString tmKey(*aKey);
    ToLowerCase(tmKey);
    return mozilla::HashString(tmKey);
  }
  enum { ALLOW_MEMMOVE = true };

  // To avoid double-counting, only measure the string if it is unshared.
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return GetKey().SizeOfExcludingThisMustBeUnshared(aMallocSizeOf);
  }

private:
  const nsString mStr;
};

#endif

/**
 * hashkey wrapper using nsACString KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsCStringHashKey : public PLDHashEntryHdr
{
public:
  typedef const nsACString& KeyType;
  typedef const nsACString* KeyTypePointer;

  explicit nsCStringHashKey(const nsACString* aStr) : mStr(*aStr) {}
  nsCStringHashKey(const nsCStringHashKey& aToCopy) : mStr(aToCopy.mStr) {}
  ~nsCStringHashKey() {}

  KeyType GetKey() const { return mStr; }
  bool KeyEquals(KeyTypePointer aKey) const { return mStr.Equals(*aKey); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return mozilla::HashString(*aKey);
  }

#ifdef MOZILLA_INTERNAL_API
  // To avoid double-counting, only measure the string if it is unshared.
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return GetKey().SizeOfExcludingThisMustBeUnshared(aMallocSizeOf);
  }
#endif

  enum { ALLOW_MEMMOVE = true };

private:
  const nsCString mStr;
};

/**
 * hashkey wrapper using uint32_t KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsUint32HashKey : public PLDHashEntryHdr
{
public:
  typedef const uint32_t& KeyType;
  typedef const uint32_t* KeyTypePointer;

  explicit nsUint32HashKey(KeyTypePointer aKey) : mValue(*aKey) {}
  nsUint32HashKey(const nsUint32HashKey& aToCopy) : mValue(aToCopy.mValue) {}
  ~nsUint32HashKey() {}

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) { return *aKey; }
  enum { ALLOW_MEMMOVE = true };

private:
  const uint32_t mValue;
};

/**
 * hashkey wrapper using uint64_t KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsUint64HashKey : public PLDHashEntryHdr
{
public:
  typedef const uint64_t& KeyType;
  typedef const uint64_t* KeyTypePointer;

  explicit nsUint64HashKey(KeyTypePointer aKey) : mValue(*aKey) {}
  nsUint64HashKey(const nsUint64HashKey& aToCopy) : mValue(aToCopy.mValue) {}
  ~nsUint64HashKey() {}

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return PLDHashNumber(*aKey);
  }
  enum { ALLOW_MEMMOVE = true };

private:
  const uint64_t mValue;
};

/**
 * hashkey wrapper using float KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsFloatHashKey : public PLDHashEntryHdr
{
public:
  typedef const float& KeyType;
  typedef const float* KeyTypePointer;

  explicit nsFloatHashKey(KeyTypePointer aKey) : mValue(*aKey) {}
  nsFloatHashKey(const nsFloatHashKey& aToCopy) : mValue(aToCopy.mValue) {}
  ~nsFloatHashKey() {}

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return *reinterpret_cast<const uint32_t*>(aKey);
  }
  enum { ALLOW_MEMMOVE = true };

private:
  const float mValue;
};

/**
 * hashkey wrapper using nsISupports* KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsISupportsHashKey : public PLDHashEntryHdr
{
public:
  typedef nsISupports* KeyType;
  typedef const nsISupports* KeyTypePointer;

  explicit nsISupportsHashKey(const nsISupports* aKey)
    : mSupports(const_cast<nsISupports*>(aKey))
  {
  }
  nsISupportsHashKey(const nsISupportsHashKey& aToCopy)
    : mSupports(aToCopy.mSupports)
  {
  }
  ~nsISupportsHashKey() {}

  KeyType GetKey() const { return mSupports; }
  bool KeyEquals(KeyTypePointer aKey) const { return aKey == mSupports; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return NS_PTR_TO_UINT32(aKey) >> 2;
  }
  enum { ALLOW_MEMMOVE = true };

private:
  nsCOMPtr<nsISupports> mSupports;
};

/**
 * hashkey wrapper using refcounted * KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
template<class T>
class nsRefPtrHashKey : public PLDHashEntryHdr
{
public:
  typedef T* KeyType;
  typedef const T* KeyTypePointer;

  explicit nsRefPtrHashKey(const T* aKey) : mKey(const_cast<T*>(aKey)) {}
  nsRefPtrHashKey(const nsRefPtrHashKey& aToCopy) : mKey(aToCopy.mKey) {}
  ~nsRefPtrHashKey() {}

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return aKey == mKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return NS_PTR_TO_UINT32(aKey) >> 2;
  }
  enum { ALLOW_MEMMOVE = true };

private:
  nsRefPtr<T> mKey;
};

template<class T>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsRefPtrHashKey<T>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  CycleCollectionNoteChild(aCallback, aField.GetKey(), aName, aFlags);
}

/**
 * hashkey wrapper using T* KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
template<class T>
class nsPtrHashKey : public PLDHashEntryHdr
{
public:
  typedef T* KeyType;
  typedef const T* KeyTypePointer;

  explicit nsPtrHashKey(const T* aKey) : mKey(const_cast<T*>(aKey)) {}
  nsPtrHashKey(const nsPtrHashKey<T>& aToCopy) : mKey(aToCopy.mKey) {}
  ~nsPtrHashKey() {}

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return aKey == mKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return NS_PTR_TO_UINT32(aKey) >> 2;
  }
  enum { ALLOW_MEMMOVE = true };

protected:
  T* MOZ_NON_OWNING_REF mKey;
};

/**
 * hashkey wrapper using T* KeyType that sets key to nullptr upon
 * destruction. Relevant only in cases where a memory pointer-scanner
 * like valgrind might get confused about stale references.
 *
 * @see nsTHashtable::EntryType for specification
 */

template<class T>
class nsClearingPtrHashKey : public nsPtrHashKey<T>
{
public:
  explicit nsClearingPtrHashKey(const T* aKey) : nsPtrHashKey<T>(aKey) {}
  nsClearingPtrHashKey(const nsClearingPtrHashKey<T>& aToCopy)
    : nsPtrHashKey<T>(aToCopy)
  {
  }
  ~nsClearingPtrHashKey() { nsPtrHashKey<T>::mKey = nullptr; }
};

typedef nsPtrHashKey<const void> nsVoidPtrHashKey;
typedef nsClearingPtrHashKey<const void> nsClearingVoidPtrHashKey;

/**
 * hashkey wrapper using a function pointer KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
template<class T>
class nsFuncPtrHashKey : public PLDHashEntryHdr
{
public:
  typedef T& KeyType;
  typedef const T* KeyTypePointer;

  explicit nsFuncPtrHashKey(const T* aKey) : mKey(*const_cast<T*>(aKey)) {}
  nsFuncPtrHashKey(const nsFuncPtrHashKey<T>& aToCopy) : mKey(aToCopy.mKey) {}
  ~nsFuncPtrHashKey() {}

  KeyType GetKey() const { return const_cast<T&>(mKey); }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return NS_PTR_TO_UINT32(*aKey) >> 2;
  }
  enum { ALLOW_MEMMOVE = true };

protected:
  T mKey;
};

/**
 * hashkey wrapper using nsID KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsIDHashKey : public PLDHashEntryHdr
{
public:
  typedef const nsID& KeyType;
  typedef const nsID* KeyTypePointer;

  explicit nsIDHashKey(const nsID* aInID) : mID(*aInID) {}
  nsIDHashKey(const nsIDHashKey& aToCopy) : mID(aToCopy.mID) {}
  ~nsIDHashKey() {}

  KeyType GetKey() const { return mID; }
  bool KeyEquals(KeyTypePointer aKey) const { return aKey->Equals(mID); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    // Hash the nsID object's raw bytes.
    return mozilla::HashBytes(aKey, sizeof(KeyType));
  }

  enum { ALLOW_MEMMOVE = true };

private:
  const nsID mID;
};

/**
 * hashkey wrapper for "dependent" const char*; this class does not "own"
 * its string pointer.
 *
 * This class must only be used if the strings have a lifetime longer than
 * the hashtable they occupy. This normally occurs only for static
 * strings or strings that have been arena-allocated.
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsDepCharHashKey : public PLDHashEntryHdr
{
public:
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  explicit nsDepCharHashKey(const char* aKey) : mKey(aKey) {}
  nsDepCharHashKey(const nsDepCharHashKey& aToCopy) : mKey(aToCopy.mKey) {}
  ~nsDepCharHashKey() {}

  const char* GetKey() const { return mKey; }
  bool KeyEquals(const char* aKey) const { return !strcmp(mKey, aKey); }

  static const char* KeyToPointer(const char* aKey) { return aKey; }
  static PLDHashNumber HashKey(const char* aKey)
  {
    return mozilla::HashString(aKey);
  }
  enum { ALLOW_MEMMOVE = true };

private:
  const char* mKey;
};

/**
 * hashkey wrapper for const char*; at construction, this class duplicates
 * a string pointed to by the pointer so that it doesn't matter whether or not
 * the string lives longer than the hash table.
 */
class nsCharPtrHashKey : public PLDHashEntryHdr
{
public:
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  explicit nsCharPtrHashKey(const char* aKey) : mKey(strdup(aKey)) {}
  nsCharPtrHashKey(const nsCharPtrHashKey& aToCopy)
    : mKey(strdup(aToCopy.mKey))
  {
  }

  nsCharPtrHashKey(nsCharPtrHashKey&& aOther)
    : mKey(aOther.mKey)
  {
    aOther.mKey = nullptr;
  }

  ~nsCharPtrHashKey()
  {
    if (mKey) {
      free(const_cast<char*>(mKey));
    }
  }

  const char* GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return !strcmp(mKey, aKey); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return mozilla::HashString(aKey);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(mKey);
  }

  enum { ALLOW_MEMMOVE = true };

private:
  const char* mKey;
};

/**
 * hashkey wrapper for const char16_t*; at construction, this class duplicates
 * a string pointed to by the pointer so that it doesn't matter whether or not
 * the string lives longer than the hash table.
 */
class nsUnicharPtrHashKey : public PLDHashEntryHdr
{
public:
  typedef const char16_t* KeyType;
  typedef const char16_t* KeyTypePointer;

  explicit nsUnicharPtrHashKey(const char16_t* aKey) : mKey(NS_strdup(aKey)) {}
  nsUnicharPtrHashKey(const nsUnicharPtrHashKey& aToCopy)
    : mKey(NS_strdup(aToCopy.mKey))
  {
  }

  nsUnicharPtrHashKey(nsUnicharPtrHashKey&& aOther)
    : mKey(aOther.mKey)
  {
    aOther.mKey = nullptr;
  }

  ~nsUnicharPtrHashKey()
  {
    if (mKey) {
      NS_Free(const_cast<char16_t*>(mKey));
    }
  }

  const char16_t* GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return !NS_strcmp(mKey, aKey); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return mozilla::HashString(aKey);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(mKey);
  }

  enum { ALLOW_MEMMOVE = true };

private:
  const char16_t* mKey;
};

/**
 * Hashtable key class to use with objects that support nsIHashable
 */
class nsHashableHashKey : public PLDHashEntryHdr
{
public:
  typedef nsIHashable* KeyType;
  typedef const nsIHashable* KeyTypePointer;

  explicit nsHashableHashKey(const nsIHashable* aKey)
    : mKey(const_cast<nsIHashable*>(aKey))
  {
  }
  nsHashableHashKey(const nsHashableHashKey& aToCopy) : mKey(aToCopy.mKey) {}
  ~nsHashableHashKey() {}

  nsIHashable* GetKey() const { return mKey; }

  bool KeyEquals(const nsIHashable* aKey) const
  {
    bool eq;
    if (NS_SUCCEEDED(mKey->Equals(const_cast<nsIHashable*>(aKey), &eq))) {
      return eq;
    }
    return false;
  }

  static const nsIHashable* KeyToPointer(nsIHashable* aKey) { return aKey; }
  static PLDHashNumber HashKey(const nsIHashable* aKey)
  {
    uint32_t code = 8888; // magic number if GetHashCode fails :-(
#ifdef DEBUG
    nsresult rv =
#endif
      const_cast<nsIHashable*>(aKey)->GetHashCode(&code);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetHashCode should not throw!");
    return code;
  }

  enum { ALLOW_MEMMOVE = true };

private:
  nsCOMPtr<nsIHashable> mKey;
};

/**
 * Hashtable key class to use with objects for which Hash() and operator==()
 * are defined.
 */
template<typename T>
class nsGenericHashKey : public PLDHashEntryHdr
{
public:
  typedef const T& KeyType;
  typedef const T* KeyTypePointer;

  explicit nsGenericHashKey(KeyTypePointer aKey) : mKey(*aKey) {}
  nsGenericHashKey(const nsGenericHashKey<T>& aOther) : mKey(aOther.mKey) {}

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) { return aKey->Hash(); }
  enum { ALLOW_MEMMOVE = true };

private:
  T mKey;
};

#endif // nsTHashKeys_h__
