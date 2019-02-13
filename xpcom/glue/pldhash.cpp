/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Double hashing implementation.
 */
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pldhash.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"
#include "nsDebug.h"     /* for PR_ASSERT */
#include "nsAlgorithm.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ChaosMode.h"

#ifdef PL_DHASHMETER
# define METER(x)       x
#else
# define METER(x)       /* nothing */
#endif

/*
 * The following DEBUG-only code is used to assert that calls to one of
 * table->mOps or to an enumerator do not cause re-entry into a call that
 * can mutate the table.
 */
#ifdef DEBUG

/*
 * Most callers that assert about the recursion level don't care about
 * this magical value because they are asserting that mutation is
 * allowed (and therefore the level is 0 or 1, depending on whether they
 * incremented it).
 *
 * Only the destructor needs to allow this special value.
 */
#define IMMUTABLE_RECURSION_LEVEL UINT32_MAX

#define RECURSION_LEVEL_SAFE_TO_FINISH(table_)                                \
    (table_->mRecursionLevel == 0 ||                                          \
     table_->mRecursionLevel == IMMUTABLE_RECURSION_LEVEL)

#define INCREMENT_RECURSION_LEVEL(table_)                                     \
    do {                                                                      \
        if (table_->mRecursionLevel != IMMUTABLE_RECURSION_LEVEL) {           \
            const uint32_t oldRecursionLevel = table_->mRecursionLevel++;     \
            MOZ_ASSERT(oldRecursionLevel < IMMUTABLE_RECURSION_LEVEL - 1);    \
        }                                                                     \
    } while(0)
#define DECREMENT_RECURSION_LEVEL(table_)                                     \
    do {                                                                      \
        if (table_->mRecursionLevel != IMMUTABLE_RECURSION_LEVEL) {           \
            const uint32_t oldRecursionLevel = table_->mRecursionLevel--;     \
            MOZ_ASSERT(oldRecursionLevel > 0);                                \
        }                                                                     \
    } while(0)

#else

#define INCREMENT_RECURSION_LEVEL(table_)   do { } while(0)
#define DECREMENT_RECURSION_LEVEL(table_)   do { } while(0)

#endif /* defined(DEBUG) */

using namespace mozilla;

PLDHashNumber
PL_DHashStringKey(PLDHashTable* aTable, const void* aKey)
{
  return HashString(static_cast<const char*>(aKey));
}

PLDHashNumber
PL_DHashVoidPtrKeyStub(PLDHashTable* aTable, const void* aKey)
{
  return (PLDHashNumber)(ptrdiff_t)aKey >> 2;
}

bool
PL_DHashMatchEntryStub(PLDHashTable* aTable,
                       const PLDHashEntryHdr* aEntry,
                       const void* aKey)
{
  const PLDHashEntryStub* stub = (const PLDHashEntryStub*)aEntry;

  return stub->key == aKey;
}

bool
PL_DHashMatchStringKey(PLDHashTable* aTable,
                       const PLDHashEntryHdr* aEntry,
                       const void* aKey)
{
  const PLDHashEntryStub* stub = (const PLDHashEntryStub*)aEntry;

  /* XXX tolerate null keys on account of sloppy Mozilla callers. */
  return stub->key == aKey ||
         (stub->key && aKey &&
          strcmp((const char*)stub->key, (const char*)aKey) == 0);
}

MOZ_ALWAYS_INLINE void
PLDHashTable::MoveEntryStub(const PLDHashEntryHdr* aFrom,
                            PLDHashEntryHdr* aTo)
{
  memcpy(aTo, aFrom, mEntrySize);
}

void
PL_DHashMoveEntryStub(PLDHashTable* aTable,
                      const PLDHashEntryHdr* aFrom,
                      PLDHashEntryHdr* aTo)
{
  aTable->MoveEntryStub(aFrom, aTo);
}

MOZ_ALWAYS_INLINE void
PLDHashTable::ClearEntryStub(PLDHashEntryHdr* aEntry)
{
  memset(aEntry, 0, mEntrySize);
}

void
PL_DHashClearEntryStub(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  aTable->ClearEntryStub(aEntry);
}

static const PLDHashTableOps stub_ops = {
  PL_DHashVoidPtrKeyStub,
  PL_DHashMatchEntryStub,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  nullptr
};

const PLDHashTableOps*
PL_DHashGetStubOps(void)
{
  return &stub_ops;
}

static bool
SizeOfEntryStore(uint32_t aCapacity, uint32_t aEntrySize, uint32_t* aNbytes)
{
  uint64_t nbytes64 = uint64_t(aCapacity) * uint64_t(aEntrySize);
  *aNbytes = aCapacity * aEntrySize;
  return uint64_t(*aNbytes) == nbytes64;   // returns false on overflow
}

/*
 * Compute max and min load numbers (entry counts).  We have a secondary max
 * that allows us to overload a table reasonably if it cannot be grown further
 * (i.e. if ChangeTable() fails).  The table slows down drastically if the
 * secondary max is too close to 1, but 0.96875 gives only a slight slowdown
 * while allowing 1.3x more elements.
 */
static inline uint32_t
MaxLoad(uint32_t aCapacity)
{
  return aCapacity - (aCapacity >> 2);  // == aCapacity * 0.75
}
static inline uint32_t
MaxLoadOnGrowthFailure(uint32_t aCapacity)
{
  return aCapacity - (aCapacity >> 5);  // == aCapacity * 0.96875
}
static inline uint32_t
MinLoad(uint32_t aCapacity)
{
  return aCapacity >> 2;                // == aCapacity * 0.25
}

// Compute the minimum capacity (and the Log2 of that capacity) for a table
// containing |aLength| elements while respecting the following contraints:
// - table must be at most 75% full;
// - capacity must be a power of two;
// - capacity cannot be too small.
static inline void
BestCapacity(uint32_t aLength, uint32_t* aCapacityOut,
             uint32_t* aLog2CapacityOut)
{
  // Compute the smallest capacity allowing |aLength| elements to be inserted
  // without rehashing.
  uint32_t capacity = (aLength * 4 + (3 - 1)) / 3; // == ceil(aLength * 4 / 3)
  if (capacity < PL_DHASH_MIN_CAPACITY) {
    capacity = PL_DHASH_MIN_CAPACITY;
  }

  // Round up capacity to next power-of-two.
  uint32_t log2 = CeilingLog2(capacity);
  capacity = 1u << log2;
  MOZ_ASSERT(capacity <= PL_DHASH_MAX_CAPACITY);

  *aCapacityOut = capacity;
  *aLog2CapacityOut = log2;
}

static MOZ_ALWAYS_INLINE uint32_t
HashShift(uint32_t aEntrySize, uint32_t aLength)
{
  if (aLength > PL_DHASH_MAX_INITIAL_LENGTH) {
    MOZ_CRASH("Initial length is too large");
  }

  uint32_t capacity, log2;
  BestCapacity(aLength, &capacity, &log2);

  uint32_t nbytes;
  if (!SizeOfEntryStore(capacity, aEntrySize, &nbytes)) {
    MOZ_CRASH("Initial entry store size is too large");
  }

  // Compute the hashShift value.
  return PL_DHASH_BITS - log2;
}

PLDHashTable::PLDHashTable(const PLDHashTableOps* aOps, uint32_t aEntrySize,
                           uint32_t aLength)
  : mOps(aOps)
  , mHashShift(HashShift(aEntrySize, aLength))
  , mEntrySize(aEntrySize)
  , mEntryCount(0)
  , mRemovedCount(0)
  , mGeneration(0)
  , mEntryStore(nullptr)
#ifdef DEBUG
  , mRecursionLevel(0)
#endif
{
  METER(memset(&mStats, 0, sizeof(mStats)));
}

PLDHashTable&
PLDHashTable::operator=(PLDHashTable&& aOther)
{
  if (this == &aOther) {
    return *this;
  }

  // Destruct |this|.
  this->~PLDHashTable();

  // |mOps| and |mEntrySize| are const so we can't assign them. Instead, we
  // require that they are equal. The justification for this is that they're
  // conceptually part of the type -- indeed, if PLDHashTable was a templated
  // type like nsTHashtable, they *would* be part of the type -- so it only
  // makes sense to assign in cases where they match.
  MOZ_RELEASE_ASSERT(mOps == aOther.mOps);
  MOZ_RELEASE_ASSERT(mEntrySize == aOther.mEntrySize);

  // Move non-const pieces over.
  mHashShift = Move(aOther.mHashShift);
  mEntryCount = Move(aOther.mEntryCount);
  mRemovedCount = Move(aOther.mRemovedCount);
  mGeneration = Move(aOther.mGeneration);
  mEntryStore = Move(aOther.mEntryStore);
#ifdef PL_DHASHMETER
  mStats = Move(aOther.mStats);
#endif
#ifdef DEBUG
  // Atomic<> doesn't have an |operator=(Atomic<>&&)|.
  mRecursionLevel = uint32_t(aOther.mRecursionLevel);
#endif

  // Clear up |aOther| so its destruction will be a no-op.
  aOther.mEntryStore = nullptr;
#ifdef DEBUG
  aOther.mRecursionLevel = 0;
#endif

  return *this;
}

/*
 * Double hashing needs the second hash code to be relatively prime to table
 * size, so we simply make hash2 odd.
 */
#define HASH1(hash0, shift)         ((hash0) >> (shift))
#define HASH2(hash0,log2,shift)     ((((hash0) << (log2)) >> (shift)) | 1)

/*
 * Reserve mKeyHash 0 for free entries and 1 for removed-entry sentinels.  Note
 * that a removed-entry sentinel need be stored only if the removed entry had
 * a colliding entry added after it.  Therefore we can use 1 as the collision
 * flag in addition to the removed-entry sentinel value.  Multiplicative hash
 * uses the high order bits of mKeyHash, so this least-significant reservation
 * should not hurt the hash function's effectiveness much.
 */
#define COLLISION_FLAG              ((PLDHashNumber) 1)
#define MARK_ENTRY_FREE(entry)      ((entry)->mKeyHash = 0)
#define MARK_ENTRY_REMOVED(entry)   ((entry)->mKeyHash = 1)
#define ENTRY_IS_REMOVED(entry)     ((entry)->mKeyHash == 1)
#define ENTRY_IS_LIVE(entry)        ((entry)->mKeyHash >= 2)
#define ENSURE_LIVE_KEYHASH(hash0)  if (hash0 < 2) hash0 -= 2; else (void)0

/* Match an entry's mKeyHash against an unstored one computed from a key. */
#define MATCH_ENTRY_KEYHASH(entry,hash0) \
    (((entry)->mKeyHash & ~COLLISION_FLAG) == (hash0))

/* Compute the address of the indexed entry in table. */
#define ADDRESS_ENTRY(table, index) \
    ((PLDHashEntryHdr *)((table)->mEntryStore + (index) * (table)->mEntrySize))

/* static */ MOZ_ALWAYS_INLINE bool
PLDHashTable::EntryIsFree(PLDHashEntryHdr* aEntry)
{
  return aEntry->mKeyHash == 0;
}

PLDHashTable::~PLDHashTable()
{
  if (!mEntryStore) {
    return;
  }

  INCREMENT_RECURSION_LEVEL(this);

  /* Clear any remaining live entries. */
  char* entryAddr = mEntryStore;
  char* entryLimit = entryAddr + Capacity() * mEntrySize;
  while (entryAddr < entryLimit) {
    PLDHashEntryHdr* entry = (PLDHashEntryHdr*)entryAddr;
    if (ENTRY_IS_LIVE(entry)) {
      METER(mStats.mRemoveEnums++);
      mOps->clearEntry(this, entry);
    }
    entryAddr += mEntrySize;
  }

  DECREMENT_RECURSION_LEVEL(this);
  MOZ_ASSERT(RECURSION_LEVEL_SAFE_TO_FINISH(this));

  /* Free entry storage last. */
  free(mEntryStore);
  mEntryStore = nullptr;
}

void
PLDHashTable::ClearAndPrepareForLength(uint32_t aLength)
{
  // Get these values before the destructor clobbers them.
  const PLDHashTableOps* ops = mOps;
  uint32_t entrySize = mEntrySize;

  this->~PLDHashTable();
  new (this) PLDHashTable(ops, entrySize, aLength);
}

void
PLDHashTable::Clear()
{
  ClearAndPrepareForLength(PL_DHASH_DEFAULT_INITIAL_LENGTH);
}

// If |IsAdd| is true, the return value is always non-null and it may be a
// previously-removed entry. If |IsAdd| is false, the return value is null on a
// miss, and will never be a previously-removed entry on a hit. This
// distinction is a bit grotty but this function is hot enough that these
// differences are worthwhile.
template <PLDHashTable::SearchReason Reason>
PLDHashEntryHdr* PL_DHASH_FASTCALL
PLDHashTable::SearchTable(const void* aKey, PLDHashNumber aKeyHash)
{
  MOZ_ASSERT(mEntryStore);
  METER(mStats.mSearches++);
  NS_ASSERTION(!(aKeyHash & COLLISION_FLAG),
               "!(aKeyHash & COLLISION_FLAG)");

  /* Compute the primary hash address. */
  PLDHashNumber hash1 = HASH1(aKeyHash, mHashShift);
  PLDHashEntryHdr* entry = ADDRESS_ENTRY(this, hash1);

  /* Miss: return space for a new entry. */
  if (EntryIsFree(entry)) {
    METER(mStats.mMisses++);
    return (Reason == ForAdd) ? entry : nullptr;
  }

  /* Hit: return entry. */
  PLDHashMatchEntry matchEntry = mOps->matchEntry;
  if (MATCH_ENTRY_KEYHASH(entry, aKeyHash) &&
      matchEntry(this, entry, aKey)) {
    METER(mStats.mHits++);
    return entry;
  }

  /* Collision: double hash. */
  int sizeLog2 = PL_DHASH_BITS - mHashShift;
  PLDHashNumber hash2 = HASH2(aKeyHash, sizeLog2, mHashShift);
  uint32_t sizeMask = (1u << sizeLog2) - 1;

  /*
   * Save the first removed entry pointer so Add() can recycle it. (Only used
   * if Reason==ForAdd.)
   */
  PLDHashEntryHdr* firstRemoved = nullptr;

  for (;;) {
    if (Reason == ForAdd) {
      if (MOZ_UNLIKELY(ENTRY_IS_REMOVED(entry))) {
        if (!firstRemoved) {
          firstRemoved = entry;
        }
      } else {
        entry->mKeyHash |= COLLISION_FLAG;
      }
    }

    METER(mStats.mSteps++);
    hash1 -= hash2;
    hash1 &= sizeMask;

    entry = ADDRESS_ENTRY(this, hash1);
    if (EntryIsFree(entry)) {
      METER(mStats.mMisses++);
      return (Reason == ForAdd) ? (firstRemoved ? firstRemoved : entry)
                                : nullptr;
    }

    if (MATCH_ENTRY_KEYHASH(entry, aKeyHash) &&
        matchEntry(this, entry, aKey)) {
      METER(mStats.mHits++);
      return entry;
    }
  }

  /* NOTREACHED */
  return nullptr;
}

/*
 * This is a copy of SearchTable, used by ChangeTable, hardcoded to
 *   1. assume |aIsAdd| is true,
 *   2. assume that |aKey| will never match an existing entry, and
 *   3. assume that no entries have been removed from the current table
 *      structure.
 * Avoiding the need for |aKey| means we can avoid needing a way to map
 * entries to keys, which means callers can use complex key types more
 * easily.
 */
PLDHashEntryHdr* PL_DHASH_FASTCALL
PLDHashTable::FindFreeEntry(PLDHashNumber aKeyHash)
{
  METER(mStats.mSearches++);
  MOZ_ASSERT(mEntryStore);
  NS_ASSERTION(!(aKeyHash & COLLISION_FLAG),
               "!(aKeyHash & COLLISION_FLAG)");

  /* Compute the primary hash address. */
  PLDHashNumber hash1 = HASH1(aKeyHash, mHashShift);
  PLDHashEntryHdr* entry = ADDRESS_ENTRY(this, hash1);

  /* Miss: return space for a new entry. */
  if (EntryIsFree(entry)) {
    METER(mStats.mMisses++);
    return entry;
  }

  /* Collision: double hash. */
  int sizeLog2 = PL_DHASH_BITS - mHashShift;
  PLDHashNumber hash2 = HASH2(aKeyHash, sizeLog2, mHashShift);
  uint32_t sizeMask = (1u << sizeLog2) - 1;

  for (;;) {
    NS_ASSERTION(!ENTRY_IS_REMOVED(entry),
                 "!ENTRY_IS_REMOVED(entry)");
    entry->mKeyHash |= COLLISION_FLAG;

    METER(mStats.mSteps++);
    hash1 -= hash2;
    hash1 &= sizeMask;

    entry = ADDRESS_ENTRY(this, hash1);
    if (EntryIsFree(entry)) {
      METER(mStats.mMisses++);
      return entry;
    }
  }

  /* NOTREACHED */
  return nullptr;
}

bool
PLDHashTable::ChangeTable(int32_t aDeltaLog2)
{
  MOZ_ASSERT(mEntryStore);

  /* Look, but don't touch, until we succeed in getting new entry store. */
  int32_t oldLog2 = PL_DHASH_BITS - mHashShift;
  int32_t newLog2 = oldLog2 + aDeltaLog2;
  uint32_t newCapacity = 1u << newLog2;
  if (newCapacity > PL_DHASH_MAX_CAPACITY) {
    return false;
  }

  uint32_t nbytes;
  if (!SizeOfEntryStore(newCapacity, mEntrySize, &nbytes)) {
    return false;   // overflowed
  }

  char* newEntryStore = (char*)malloc(nbytes);
  if (!newEntryStore) {
    return false;
  }

  /* We can't fail from here on, so update table parameters. */
  mHashShift = PL_DHASH_BITS - newLog2;
  mRemovedCount = 0;
  mGeneration++;

  /* Assign the new entry store to table. */
  memset(newEntryStore, 0, nbytes);
  char* oldEntryStore;
  char* oldEntryAddr;
  oldEntryAddr = oldEntryStore = mEntryStore;
  mEntryStore = newEntryStore;
  PLDHashMoveEntry moveEntry = mOps->moveEntry;

  /* Copy only live entries, leaving removed ones behind. */
  uint32_t oldCapacity = 1u << oldLog2;
  for (uint32_t i = 0; i < oldCapacity; ++i) {
    PLDHashEntryHdr* oldEntry = (PLDHashEntryHdr*)oldEntryAddr;
    if (ENTRY_IS_LIVE(oldEntry)) {
      oldEntry->mKeyHash &= ~COLLISION_FLAG;
      PLDHashEntryHdr* newEntry = FindFreeEntry(oldEntry->mKeyHash);
      NS_ASSERTION(EntryIsFree(newEntry), "EntryIsFree(newEntry)");
      moveEntry(this, oldEntry, newEntry);
      newEntry->mKeyHash = oldEntry->mKeyHash;
    }
    oldEntryAddr += mEntrySize;
  }

  free(oldEntryStore);
  return true;
}

MOZ_ALWAYS_INLINE PLDHashNumber
PLDHashTable::ComputeKeyHash(const void* aKey)
{
  MOZ_ASSERT(mEntryStore);

  PLDHashNumber keyHash = mOps->hashKey(this, aKey);
  keyHash *= PL_DHASH_GOLDEN_RATIO;

  /* Avoid 0 and 1 hash codes, they indicate free and removed entries. */
  ENSURE_LIVE_KEYHASH(keyHash);
  keyHash &= ~COLLISION_FLAG;

  return keyHash;
}

MOZ_ALWAYS_INLINE PLDHashEntryHdr*
PLDHashTable::Search(const void* aKey)
{
  INCREMENT_RECURSION_LEVEL(this);

  METER(mStats.mSearches++);

  PLDHashEntryHdr* entry =
    mEntryStore ? SearchTable<ForSearchOrRemove>(aKey, ComputeKeyHash(aKey))
                : nullptr;

  DECREMENT_RECURSION_LEVEL(this);

  return entry;
}

MOZ_ALWAYS_INLINE PLDHashEntryHdr*
PLDHashTable::Add(const void* aKey, const mozilla::fallible_t&)
{
  PLDHashNumber keyHash;
  PLDHashEntryHdr* entry;
  uint32_t capacity;

  MOZ_ASSERT(mRecursionLevel == 0);
  INCREMENT_RECURSION_LEVEL(this);

  // Allocate the entry storage if it hasn't already been allocated.
  if (!mEntryStore) {
    uint32_t nbytes;
    // We already checked this in the constructor, so it must still be true.
    MOZ_RELEASE_ASSERT(SizeOfEntryStore(CapacityFromHashShift(), mEntrySize,
                                        &nbytes));
    mEntryStore = (char*)malloc(nbytes);
    if (!mEntryStore) {
      METER(mStats.mAddFailures++);
      entry = nullptr;
      goto exit;
    }
    memset(mEntryStore, 0, nbytes);
  }

  /*
   * If alpha is >= .75, grow or compress the table.  If aKey is already
   * in the table, we may grow once more than necessary, but only if we
   * are on the edge of being overloaded.
   */
  capacity = Capacity();
  if (mEntryCount + mRemovedCount >= MaxLoad(capacity)) {
    /* Compress if a quarter or more of all entries are removed. */
    int deltaLog2;
    if (mRemovedCount >= capacity >> 2) {
      METER(mStats.mCompresses++);
      deltaLog2 = 0;
    } else {
      METER(mStats.mGrows++);
      deltaLog2 = 1;
    }

    /*
     * Grow or compress the table.  If ChangeTable() fails, allow
     * overloading up to the secondary max.  Once we hit the secondary
     * max, return null.
     */
    if (!ChangeTable(deltaLog2) &&
        mEntryCount + mRemovedCount >= MaxLoadOnGrowthFailure(capacity)) {
      METER(mStats.mAddFailures++);
      entry = nullptr;
      goto exit;
    }
  }

  /*
   * Look for entry after possibly growing, so we don't have to add it,
   * then skip it while growing the table and re-add it after.
   */
  keyHash = ComputeKeyHash(aKey);
  entry = SearchTable<ForAdd>(aKey, keyHash);
  if (!ENTRY_IS_LIVE(entry)) {
    /* Initialize the entry, indicating that it's no longer free. */
    METER(mStats.mAddMisses++);
    if (ENTRY_IS_REMOVED(entry)) {
      METER(mStats.mAddOverRemoved++);
      mRemovedCount--;
      keyHash |= COLLISION_FLAG;
    }
    if (mOps->initEntry) {
      mOps->initEntry(entry, aKey);
    }
    entry->mKeyHash = keyHash;
    mEntryCount++;
  }
  METER(else {
    mStats.mAddHits++;
  });

exit:
  DECREMENT_RECURSION_LEVEL(this);
  return entry;
}

MOZ_ALWAYS_INLINE PLDHashEntryHdr*
PLDHashTable::Add(const void* aKey)
{
  PLDHashEntryHdr* entry = Add(aKey, fallible);
  if (!entry) {
    if (!mEntryStore) {
      // We OOM'd while allocating the initial entry storage.
      uint32_t nbytes;
      (void) SizeOfEntryStore(CapacityFromHashShift(), mEntrySize, &nbytes);
      NS_ABORT_OOM(nbytes);
    } else {
      // We failed to resize the existing entry storage, either due to OOM or
      // because we exceeded the maximum table capacity or size; report it as
      // an OOM. The multiplication by 2 gets us the size we tried to allocate,
      // which is double the current size.
      NS_ABORT_OOM(2 * EntrySize() * EntryCount());
    }
  }
  return entry;
}

MOZ_ALWAYS_INLINE void
PLDHashTable::Remove(const void* aKey)
{
  MOZ_ASSERT(mRecursionLevel == 0);
  INCREMENT_RECURSION_LEVEL(this);

  PLDHashEntryHdr* entry =
    mEntryStore ? SearchTable<ForSearchOrRemove>(aKey, ComputeKeyHash(aKey))
                : nullptr;
  if (entry) {
    /* Clear this entry and mark it as "removed". */
    METER(mStats.mRemoveHits++);
    PL_DHashTableRawRemove(this, entry);

    /* Shrink if alpha is <= .25 and the table isn't too small already. */
    uint32_t capacity = Capacity();
    if (capacity > PL_DHASH_MIN_CAPACITY &&
        mEntryCount <= MinLoad(capacity)) {
      METER(mStats.mShrinks++);
      (void) ChangeTable(-1);
    }
  }
  METER(else {
    mStats.mRemoveMisses++;
  });

  DECREMENT_RECURSION_LEVEL(this);
}

PLDHashEntryHdr* PL_DHASH_FASTCALL
PL_DHashTableSearch(PLDHashTable* aTable, const void* aKey)
{
  return aTable->Search(aKey);
}

PLDHashEntryHdr* PL_DHASH_FASTCALL
PL_DHashTableAdd(PLDHashTable* aTable, const void* aKey,
                 const fallible_t& aFallible)
{
  return aTable->Add(aKey, aFallible);
}

PLDHashEntryHdr* PL_DHASH_FASTCALL
PL_DHashTableAdd(PLDHashTable* aTable, const void* aKey)
{
  return aTable->Add(aKey);
}

void PL_DHASH_FASTCALL
PL_DHashTableRemove(PLDHashTable* aTable, const void* aKey)
{
  aTable->Remove(aKey);
}

MOZ_ALWAYS_INLINE void
PLDHashTable::RawRemove(PLDHashEntryHdr* aEntry)
{
  MOZ_ASSERT(mEntryStore);

  MOZ_ASSERT(mRecursionLevel != IMMUTABLE_RECURSION_LEVEL);

  NS_ASSERTION(ENTRY_IS_LIVE(aEntry), "ENTRY_IS_LIVE(aEntry)");

  /* Load keyHash first in case clearEntry() goofs it. */
  PLDHashNumber keyHash = aEntry->mKeyHash;
  mOps->clearEntry(this, aEntry);
  if (keyHash & COLLISION_FLAG) {
    MARK_ENTRY_REMOVED(aEntry);
    mRemovedCount++;
  } else {
    METER(mStats.mRemoveFrees++);
    MARK_ENTRY_FREE(aEntry);
  }
  mEntryCount--;
}

void
PL_DHashTableRawRemove(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  aTable->RawRemove(aEntry);
}

// Shrink or compress if a quarter or more of all entries are removed, or if the
// table is underloaded according to the minimum alpha, and is not minimal-size
// already.
void
PLDHashTable::ShrinkIfAppropriate()
{
  uint32_t capacity = Capacity();
  if (mRemovedCount >= capacity >> 2 ||
      (capacity > PL_DHASH_MIN_CAPACITY && mEntryCount <= MinLoad(capacity))) {
    METER(mStats.mEnumShrinks++);

    uint32_t log2;
    BestCapacity(mEntryCount, &capacity, &log2);

    int32_t deltaLog2 = log2 - (PL_DHASH_BITS - mHashShift);
    MOZ_ASSERT(deltaLog2 <= 0);

    (void) ChangeTable(deltaLog2);
  }
}

MOZ_ALWAYS_INLINE uint32_t
PLDHashTable::Enumerate(PLDHashEnumerator aEtor, void* aArg)
{
  if (!mEntryStore) {
    return 0;
  }

  INCREMENT_RECURSION_LEVEL(this);

  char* entryAddr = mEntryStore;
  uint32_t capacity = Capacity();
  uint32_t tableSize = capacity * mEntrySize;
  char* entryLimit = mEntryStore + tableSize;
  uint32_t i = 0;
  bool didRemove = false;

  if (ChaosMode::isActive(ChaosMode::HashTableIteration)) {
    // Start iterating at a random point in the hashtable. It would be
    // even more chaotic to iterate in fully random order, but that's a lot
    // more work.
    entryAddr += ChaosMode::randomUint32LessThan(capacity) * mEntrySize;
  }

  for (uint32_t e = 0; e < capacity; ++e) {
    PLDHashEntryHdr* entry = (PLDHashEntryHdr*)entryAddr;
    if (ENTRY_IS_LIVE(entry)) {
      PLDHashOperator op = aEtor(this, entry, i++, aArg);
      if (op & PL_DHASH_REMOVE) {
        METER(mStats.mRemoveEnums++);
        PL_DHashTableRawRemove(this, entry);
        didRemove = true;
      }
      if (op & PL_DHASH_STOP) {
        break;
      }
    }
    entryAddr += mEntrySize;
    if (entryAddr >= entryLimit) {
      entryAddr -= tableSize;
    }
  }

  MOZ_ASSERT(!didRemove || mRecursionLevel == 1);

  // Shrink the table if appropriate. Do this only if we removed above, so
  // non-removing enumerations can count on stable |mEntryStore| until the next
  // Add, Remove, or removing-Enumerate.
  if (didRemove) {
    ShrinkIfAppropriate();
  }

  DECREMENT_RECURSION_LEVEL(this);

  return i;
}

uint32_t
PL_DHashTableEnumerate(PLDHashTable* aTable, PLDHashEnumerator aEtor,
                       void* aArg)
{
  return aTable->Enumerate(aEtor, aArg);
}

struct SizeOfEntryExcludingThisArg
{
  size_t total;
  PLDHashSizeOfEntryExcludingThisFun sizeOfEntryExcludingThis;
  MallocSizeOf mallocSizeOf;
  void* arg;  // the arg passed by the user
};

static PLDHashOperator
SizeOfEntryExcludingThisEnumerator(PLDHashTable* aTable, PLDHashEntryHdr* aHdr,
                                   uint32_t aNumber, void* aArg)
{
  SizeOfEntryExcludingThisArg* e = (SizeOfEntryExcludingThisArg*)aArg;
  e->total += e->sizeOfEntryExcludingThis(aHdr, e->mallocSizeOf, e->arg);
  return PL_DHASH_NEXT;
}

MOZ_ALWAYS_INLINE size_t
PLDHashTable::SizeOfExcludingThis(
    PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
    MallocSizeOf aMallocSizeOf, void* aArg /* = nullptr */) const
{
  if (!mEntryStore) {
    return 0;
  }

  size_t n = 0;
  n += aMallocSizeOf(mEntryStore);
  if (aSizeOfEntryExcludingThis) {
    SizeOfEntryExcludingThisArg arg2 = {
      0, aSizeOfEntryExcludingThis, aMallocSizeOf, aArg
    };
    PL_DHashTableEnumerate(const_cast<PLDHashTable*>(this),
                           SizeOfEntryExcludingThisEnumerator, &arg2);
    n += arg2.total;
  }
  return n;
}

MOZ_ALWAYS_INLINE size_t
PLDHashTable::SizeOfIncludingThis(
    PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
    MallocSizeOf aMallocSizeOf, void* aArg /* = nullptr */) const
{
  return aMallocSizeOf(this) +
         SizeOfExcludingThis(aSizeOfEntryExcludingThis, aMallocSizeOf, aArg);
}

size_t
PL_DHashTableSizeOfExcludingThis(
    const PLDHashTable* aTable,
    PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
    MallocSizeOf aMallocSizeOf, void* aArg /* = nullptr */)
{
  return aTable->SizeOfExcludingThis(aSizeOfEntryExcludingThis,
                                     aMallocSizeOf, aArg);
}

size_t
PL_DHashTableSizeOfIncludingThis(
    const PLDHashTable* aTable,
    PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
    MallocSizeOf aMallocSizeOf, void* aArg /* = nullptr */)
{
  return aTable->SizeOfIncludingThis(aSizeOfEntryExcludingThis,
                                     aMallocSizeOf, aArg);
}

PLDHashTable::Iterator::Iterator(Iterator&& aOther)
  : mTable(aOther.mTable)
  , mCurrent(aOther.mCurrent)
  , mLimit(aOther.mLimit)
{
  // No need to change mRecursionLevel here.
  aOther.mTable = nullptr;
  aOther.mCurrent = nullptr;
  aOther.mLimit = nullptr;
}

PLDHashTable::Iterator::Iterator(const PLDHashTable* aTable)
  : mTable(aTable)
  , mCurrent(mTable->mEntryStore)
  , mLimit(mTable->mEntryStore + mTable->Capacity() * mTable->mEntrySize)
{
  // Make sure that modifications can't simultaneously happen while the
  // iterator is active.
  INCREMENT_RECURSION_LEVEL(mTable);

  // Advance to the first live entry, or to the end if there are none.
  while (IsOnNonLiveEntry()) {
    mCurrent += mTable->mEntrySize;
  }
}

PLDHashTable::Iterator::~Iterator()
{
  if (mTable) {
    DECREMENT_RECURSION_LEVEL(mTable);
  }
}

bool
PLDHashTable::Iterator::Done() const
{
  return mCurrent == mLimit;
}

MOZ_ALWAYS_INLINE bool
PLDHashTable::Iterator::IsOnNonLiveEntry() const
{
  return !Done() && !ENTRY_IS_LIVE(reinterpret_cast<PLDHashEntryHdr*>(mCurrent));
}

PLDHashEntryHdr*
PLDHashTable::Iterator::Get() const
{
  MOZ_ASSERT(!Done());

  PLDHashEntryHdr* entry = reinterpret_cast<PLDHashEntryHdr*>(mCurrent);
  MOZ_ASSERT(ENTRY_IS_LIVE(entry));
  return entry;
}

void
PLDHashTable::Iterator::Next()
{
  MOZ_ASSERT(!Done());

  do {
    mCurrent += mTable->mEntrySize;
  } while (IsOnNonLiveEntry());
}

PLDHashTable::RemovingIterator::RemovingIterator(RemovingIterator&& aOther)
  : Iterator(mozilla::Move(aOther.mTable))
  , mHaveRemoved(aOther.mHaveRemoved)
{
}

PLDHashTable::RemovingIterator::RemovingIterator(PLDHashTable* aTable)
  : Iterator(aTable)
  , mHaveRemoved(false)
{
}

PLDHashTable::RemovingIterator::~RemovingIterator()
{
  if (mHaveRemoved) {
    // Why is this cast needed? In Iterator, |mTable| is const. In
    // RemovingIterator it should be non-const, but it inherits from Iterator
    // so that's not possible. But it's ok because RemovingIterator's
    // constructor takes a pointer to a non-const table in the first place.
    const_cast<PLDHashTable*>(mTable)->ShrinkIfAppropriate();
  }
}

void
PLDHashTable::RemovingIterator::Remove()
{
  METER(mStats.mRemoveEnums++);

  // This cast is needed for the same reason as the one in the destructor.
  const_cast<PLDHashTable*>(mTable)->RawRemove(Get());
  mHaveRemoved = true;
}

#ifdef DEBUG
MOZ_ALWAYS_INLINE void
PLDHashTable::MarkImmutable()
{
  mRecursionLevel = IMMUTABLE_RECURSION_LEVEL;
}

void
PL_DHashMarkTableImmutable(PLDHashTable* aTable)
{
  aTable->MarkImmutable();
}
#endif

#ifdef PL_DHASHMETER
#include <math.h>

void
PLDHashTable::DumpMeter(PLDHashEnumerator aDump, FILE* aFp)
{
  PLDHashNumber hash1, hash2, maxChainHash1, maxChainHash2;
  double sqsum, mean, variance, sigma;
  PLDHashEntryHdr* entry;

  char* entryAddr = mEntryStore;
  int sizeLog2 = PL_DHASH_BITS - mHashShift;
  uint32_t capacity = Capacity();
  uint32_t sizeMask = (1u << sizeLog2) - 1;
  uint32_t chainCount = 0, maxChainLen = 0;
  hash2 = 0;
  sqsum = 0;

  MOZ_ASSERT_IF(capacity > 0, mEntryStore);
  for (uint32_t i = 0; i < capacity; i++) {
    entry = (PLDHashEntryHdr*)entryAddr;
    entryAddr += mEntrySize;
    if (!ENTRY_IS_LIVE(entry)) {
      continue;
    }
    hash1 = HASH1(entry->mKeyHash & ~COLLISION_FLAG, mHashShift);
    PLDHashNumber saveHash1 = hash1;
    PLDHashEntryHdr* probe = ADDRESS_ENTRY(this, hash1);
    uint32_t chainLen = 1;
    if (probe == entry) {
      /* Start of a (possibly unit-length) chain. */
      chainCount++;
    } else {
      hash2 = HASH2(entry->mKeyHash & ~COLLISION_FLAG, sizeLog2, mHashShift);
      do {
        chainLen++;
        hash1 -= hash2;
        hash1 &= sizeMask;
        probe = ADDRESS_ENTRY(this, hash1);
      } while (probe != entry);
    }
    sqsum += chainLen * chainLen;
    if (chainLen > maxChainLen) {
      maxChainLen = chainLen;
      maxChainHash1 = saveHash1;
      maxChainHash2 = hash2;
    }
  }

  if (mEntryCount && chainCount) {
    mean = (double)mEntryCount / chainCount;
    variance = chainCount * sqsum - mEntryCount * mEntryCount;
    if (variance < 0 || chainCount == 1) {
      variance = 0;
    } else {
      variance /= chainCount * (chainCount - 1);
    }
    sigma = sqrt(variance);
  } else {
    mean = sigma = 0;
  }

  fprintf(aFp, "Double hashing statistics:\n");
  fprintf(aFp, "      capacity (in entries): %u\n", Capacity());
  fprintf(aFp, "          number of entries: %u\n", mEntryCount);
  fprintf(aFp, "  number of removed entries: %u\n", mRemovedCount);
  fprintf(aFp, "         number of searches: %u\n", mStats.mSearches);
  fprintf(aFp, "             number of hits: %u\n", mStats.mHits);
  fprintf(aFp, "           number of misses: %u\n", mStats.mMisses);
  fprintf(aFp, "      mean steps per search: %g\n",
          mStats.mSearches ? (double)mStats.mSteps / mStats.mSearches : 0.);
  fprintf(aFp, "     mean hash chain length: %g\n", mean);
  fprintf(aFp, "         standard deviation: %g\n", sigma);
  fprintf(aFp, "  maximum hash chain length: %u\n", maxChainLen);
  fprintf(aFp, "         number of searches: %u\n", mStats.mSearches);
  fprintf(aFp, " adds that made a new entry: %u\n", mStats.mAddMisses);
  fprintf(aFp, "adds that recycled removeds: %u\n", mStats.mAddOverRemoved);
  fprintf(aFp, "   adds that found an entry: %u\n", mStats.mAddHits);
  fprintf(aFp, "               add failures: %u\n", mStats.mAddFailures);
  fprintf(aFp, "             useful removes: %u\n", mStats.mRemoveHits);
  fprintf(aFp, "            useless removes: %u\n", mStats.mRemoveMisses);
  fprintf(aFp, "removes that freed an entry: %u\n", mStats.mRemoveFrees);
  fprintf(aFp, "  removes while enumerating: %u\n", mStats.mRemoveEnums);
  fprintf(aFp, "            number of grows: %u\n", mStats.mGrows);
  fprintf(aFp, "          number of shrinks: %u\n", mStats.mShrinks);
  fprintf(aFp, "       number of compresses: %u\n", mStats.mCompresses);
  fprintf(aFp, "number of enumerate shrinks: %u\n", mStats.mEnumShrinks);

  if (aDump && maxChainLen && hash2) {
    fputs("Maximum hash chain:\n", aFp);
    hash1 = maxChainHash1;
    hash2 = maxChainHash2;
    entry = ADDRESS_ENTRY(this, hash1);
    uint32_t i = 0;
    do {
      if (aDump(this, entry, i++, aFp) != PL_DHASH_NEXT) {
        break;
      }
      hash1 -= hash2;
      hash1 &= sizeMask;
      entry = ADDRESS_ENTRY(this, hash1);
    } while (!EntryIsFree(entry));
  }
}

void
PL_DHashTableDumpMeter(PLDHashTable* aTable, PLDHashEnumerator aDump, FILE* aFp)
{
  aTable->DumpMeter(aDump, aFp);
}
#endif /* PL_DHASHMETER */
