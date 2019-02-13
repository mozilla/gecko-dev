/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCommandParams_h__
#define nsCommandParams_h__

#include "nsString.h"
#include "nsICommandParams.h"
#include "nsCOMPtr.h"
#include "pldhash.h"

class nsCommandParams : public nsICommandParams
{
public:
  nsCommandParams();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMMANDPARAMS

protected:
  virtual ~nsCommandParams();

  struct HashEntry : public PLDHashEntryHdr
  {
    nsCString mEntryName;

    uint8_t mEntryType;
    union
    {
      bool mBoolean;
      int32_t mLong;
      double mDouble;
      nsString* mString;
      nsCString* mCString;
    } mData;

    nsCOMPtr<nsISupports> mISupports;

    HashEntry(uint8_t aType, const char* aEntryName)
      : mEntryName(aEntryName)
      , mEntryType(aType)
    {
      Reset(mEntryType);
    }

    HashEntry(const HashEntry& aRHS)
      : mEntryType(aRHS.mEntryType)
    {
      Reset(mEntryType);
      switch (mEntryType) {
        case eBooleanType:
          mData.mBoolean = aRHS.mData.mBoolean;
          break;
        case eLongType:
          mData.mLong = aRHS.mData.mLong;
          break;
        case eDoubleType:
          mData.mDouble = aRHS.mData.mDouble;
          break;
        case eWStringType:
          NS_ASSERTION(aRHS.mData.mString, "Source entry has no string");
          mData.mString = new nsString(*aRHS.mData.mString);
          break;
        case eStringType:
          NS_ASSERTION(aRHS.mData.mCString, "Source entry has no string");
          mData.mCString = new nsCString(*aRHS.mData.mCString);
          break;
        case eISupportsType:
          mISupports = aRHS.mISupports.get();
          break;
        default:
          NS_ERROR("Unknown type");
      }
    }

    ~HashEntry() { Reset(eNoType); }

    void Reset(uint8_t aNewType)
    {
      switch (mEntryType) {
        case eNoType:
          break;
        case eBooleanType:
          mData.mBoolean = false;
          break;
        case eLongType:
          mData.mLong = 0;
          break;
        case eDoubleType:
          mData.mDouble = 0.0;
          break;
        case eWStringType:
          delete mData.mString;
          mData.mString = nullptr;
          break;
        case eISupportsType:
          mISupports = nullptr;
          break;
        case eStringType:
          delete mData.mCString;
          mData.mCString = nullptr;
          break;
        default:
          NS_ERROR("Unknown type");
      }
      mEntryType = aNewType;
    }
  };

  HashEntry* GetNamedEntry(const char* aName);
  HashEntry* GetOrMakeEntry(const char* aName, uint8_t aEntryType);

protected:
  static PLDHashNumber HashKey(PLDHashTable* aTable, const void* aKey);

  static bool HashMatchEntry(PLDHashTable* aTable,
                             const PLDHashEntryHdr* aEntry, const void* aKey);

  static void HashMoveEntry(PLDHashTable* aTable, const PLDHashEntryHdr* aFrom,
                            PLDHashEntryHdr* aTo);

  static void HashClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);

  PLDHashTable mValuesHash;

  static const PLDHashTableOps sHashOps;
};

#endif // nsCommandParams_h__
