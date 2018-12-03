/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileBufferEntry_h
#define ProfileBufferEntry_h

#include <ostream>
#include "GeckoProfiler.h"
#include "platform.h"
#include "ProfileJSONWriter.h"
#include "ProfilerBacktrace.h"
#include "mozilla/RefPtr.h"
#include <string>
#include <map>
#include "js/ProfilingFrameIterator.h"
#include "js/TrackedOptimizationInfo.h"
#include "nsHashKeys.h"
#include "nsDataHashtable.h"
#include "mozilla/Maybe.h"
#include "mozilla/Vector.h"
#include "gtest/MozGtestFriend.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/UniquePtr.h"
#include "nsClassHashtable.h"
#include "mozilla/Variant.h"
#include "nsTArray.h"

class ProfilerMarker;

// NOTE!  If you add entries, you need to verify if they need to be added to the
// switch statement in DuplicateLastSample!
#define FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(MACRO)                   \
  MACRO(Category, int)                                              \
  MACRO(CollectionStart, double)                                    \
  MACRO(CollectionEnd, double)                                      \
  MACRO(Label, const char*)                                         \
  MACRO(FrameFlags, uint64_t)                                       \
  MACRO(DynamicStringFragment, char*) /* char[kNumChars], really */ \
  MACRO(JitReturnAddr, void*)                                       \
  MACRO(LineNumber, int)                                            \
  MACRO(ColumnNumber, int)                                          \
  MACRO(NativeLeafAddr, void*)                                      \
  MACRO(Marker, ProfilerMarker*)                                    \
  MACRO(Pause, double)                                              \
  MACRO(Responsiveness, double)                                     \
  MACRO(Resume, double)                                             \
  MACRO(ThreadId, int)                                              \
  MACRO(Time, double)                                               \
  MACRO(ResidentMemory, uint64_t)                                   \
  MACRO(UnsharedMemory, uint64_t)                                   \
  MACRO(CounterId, void*)                                           \
  MACRO(CounterKey, uint64_t)                                       \
  MACRO(Number, uint64_t)                                           \
  MACRO(Count, int64_t)

// NB: Packing this structure has been shown to cause SIGBUS issues on ARM.
#if !defined(GP_ARCH_arm)
#pragma pack(push, 1)
#endif

class ProfileBufferEntry {
 public:
  enum class Kind : uint8_t {
    INVALID = 0,
#define KIND(k, t) k,
    FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(KIND)
#undef KIND
        LIMIT
  };

  ProfileBufferEntry();

  // This is equal to sizeof(double), which is the largest non-char variant in
  // |u|.
  static const size_t kNumChars = 8;

 private:
  // aString must be a static string.
  ProfileBufferEntry(Kind aKind, const char* aString);
  ProfileBufferEntry(Kind aKind, char aChars[kNumChars]);
  ProfileBufferEntry(Kind aKind, void* aPtr);
  ProfileBufferEntry(Kind aKind, ProfilerMarker* aMarker);
  ProfileBufferEntry(Kind aKind, double aDouble);
  ProfileBufferEntry(Kind aKind, int64_t aInt64);
  ProfileBufferEntry(Kind aKind, uint64_t aUint64);
  ProfileBufferEntry(Kind aKind, int aInt);

 public:
#define CTOR(k, t)                            \
  static ProfileBufferEntry k(t aVal) {       \
    return ProfileBufferEntry(Kind::k, aVal); \
  }
  FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(CTOR)
#undef CTOR

  Kind GetKind() const { return mKind; }

#define IS_KIND(k, t) \
  bool Is##k() const { return mKind == Kind::k; }
  FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(IS_KIND)
#undef IS_KIND

 private:
  FRIEND_TEST(ThreadProfile, InsertOneEntry);
  FRIEND_TEST(ThreadProfile, InsertOneEntryWithTinyBuffer);
  FRIEND_TEST(ThreadProfile, InsertEntriesNoWrap);
  FRIEND_TEST(ThreadProfile, InsertEntriesWrap);
  FRIEND_TEST(ThreadProfile, MemoryMeasure);
  friend class ProfileBuffer;

  Kind mKind;
  union {
    const char* mString;
    char mChars[kNumChars];
    void* mPtr;
    ProfilerMarker* mMarker;
    double mDouble;
    int mInt;
    int64_t mInt64;
    uint64_t mUint64;
  } u;
};

#if !defined(GP_ARCH_arm)
// Packed layout: 1 byte for the tag + 8 bytes for the value.
static_assert(sizeof(ProfileBufferEntry) == 9, "bad ProfileBufferEntry size");
#pragma pack(pop)
#endif

class UniqueJSONStrings {
 public:
  UniqueJSONStrings();
  explicit UniqueJSONStrings(const UniqueJSONStrings& aOther);

  void SpliceStringTableElements(SpliceableJSONWriter& aWriter) {
    aWriter.TakeAndSplice(mStringTableWriter.WriteFunc());
  }

  void WriteProperty(mozilla::JSONWriter& aWriter, const char* aName,
                     const char* aStr) {
    aWriter.IntProperty(aName, GetOrAddIndex(aStr));
  }

  void WriteElement(mozilla::JSONWriter& aWriter, const char* aStr) {
    aWriter.IntElement(GetOrAddIndex(aStr));
  }

  uint32_t GetOrAddIndex(const char* aStr);

 private:
  SpliceableChunkedJSONWriter mStringTableWriter;
  nsDataHashtable<nsCStringHashKey, uint32_t> mStringToIndexMap;
};

// Contains all the information about JIT frames that is needed to stream stack
// frames for JitReturnAddr entries in the profiler buffer.
// Every return address (void*) is mapped to one or more JITFrameKeys, and
// every JITFrameKey is mapped to a JSON string for that frame.
// mRangeStart and mRangeEnd describe the range in the buffer for which this
// mapping is valid. Only JitReturnAddr entries within that buffer range can be
// processed using this JITFrameInfoForBufferRange object.
struct JITFrameInfoForBufferRange final {
  JITFrameInfoForBufferRange Clone() const;

  uint64_t mRangeStart;
  uint64_t mRangeEnd;  // mRangeEnd marks the first invalid index.

  struct JITFrameKey {
    uint32_t Hash() const;
    bool operator==(const JITFrameKey& aOther) const;
    bool operator!=(const JITFrameKey& aOther) const {
      return !(*this == aOther);
    }

    void* mCanonicalAddress;
    uint32_t mDepth;
  };
  nsClassHashtable<nsPtrHashKey<void>, nsTArray<JITFrameKey>>
      mJITAddressToJITFramesMap;
  nsClassHashtable<nsGenericHashKey<JITFrameKey>, nsCString>
      mJITFrameToFrameJSONMap;
};

// Contains JITFrameInfoForBufferRange objects for multiple profiler buffer
// ranges.
struct JITFrameInfo final {
  JITFrameInfo() : mUniqueStrings(mozilla::MakeUnique<UniqueJSONStrings>()) {}

  MOZ_IMPLICIT JITFrameInfo(const JITFrameInfo& aOther);

  // Creates a new JITFrameInfoForBufferRange object in mRanges by looking up
  // information about the provided JIT return addresses using aCx.
  // Addresses are provided like this:
  // The caller of AddInfoForRange supplies a function in aJITAddressProvider.
  // This function will be called once, synchronously, with an
  // aJITAddressConsumer argument, which is a function that needs to be called
  // for every address. That function can be called multiple times for the same
  // address.
  void AddInfoForRange(
      uint64_t aRangeStart, uint64_t aRangeEnd, JSContext* aCx,
      const std::function<void(const std::function<void(void*)>&)>&
          aJITAddressProvider);

  // Returns whether the information stored in this object is still relevant
  // for any entries in the buffer.
  bool HasExpired(uint64_t aCurrentBufferRangeStart) const {
    if (mRanges.IsEmpty()) {
      // No information means no relevant information. Allow this object to be
      // discarded.
      return true;
    }
    return mRanges.LastElement().mRangeEnd <= aCurrentBufferRangeStart;
  }

  // The array of ranges of JIT frame information, sorted by buffer position.
  // Ranges are non-overlapping.
  // The JSON of the cached frames can contain string indexes, which refer
  // to strings in mUniqueStrings.
  nsTArray<JITFrameInfoForBufferRange> mRanges;

  // The string table which contains strings used in the frame JSON that's
  // cached in mRanges.
  mozilla::UniquePtr<UniqueJSONStrings> mUniqueStrings;
};

class UniqueStacks {
 public:
  struct FrameKey {
    explicit FrameKey(const char* aLocation)
        : mData(NormalFrameData{nsCString(aLocation), false, mozilla::Nothing(),
                                mozilla::Nothing()}) {}

    FrameKey(nsCString&& aLocation, bool aRelevantForJS,
             const mozilla::Maybe<unsigned>& aLine,
             const mozilla::Maybe<unsigned>& aColumn,
             const mozilla::Maybe<unsigned>& aCategory)
        : mData(NormalFrameData{aLocation, aRelevantForJS, aLine, aColumn,
                                aCategory}) {}

    FrameKey(void* aJITAddress, uint32_t aJITDepth, uint32_t aRangeIndex)
        : mData(JITFrameData{aJITAddress, aJITDepth, aRangeIndex}) {}

    FrameKey(const FrameKey& aToCopy) = default;

    uint32_t Hash() const;
    bool operator==(const FrameKey& aOther) const {
      return mData == aOther.mData;
    }

    struct NormalFrameData {
      bool operator==(const NormalFrameData& aOther) const;

      nsCString mLocation;
      bool mRelevantForJS;
      mozilla::Maybe<unsigned> mLine;
      mozilla::Maybe<unsigned> mColumn;
      mozilla::Maybe<unsigned> mCategory;
    };
    struct JITFrameData {
      bool operator==(const JITFrameData& aOther) const;

      void* mCanonicalAddress;
      uint32_t mDepth;
      uint32_t mRangeIndex;
    };
    mozilla::Variant<NormalFrameData, JITFrameData> mData;
  };

  struct StackKey {
    mozilla::Maybe<uint32_t> mPrefixStackIndex;
    uint32_t mFrameIndex;

    explicit StackKey(uint32_t aFrame)
        : mFrameIndex(aFrame), mHash(mozilla::HashGeneric(aFrame)) {}

    StackKey(const StackKey& aPrefix, uint32_t aPrefixStackIndex,
             uint32_t aFrame)
        : mPrefixStackIndex(mozilla::Some(aPrefixStackIndex)),
          mFrameIndex(aFrame),
          mHash(mozilla::AddToHash(aPrefix.mHash, aFrame)) {}

    uint32_t Hash() const { return mHash; }

    bool operator==(const StackKey& aOther) const {
      return mPrefixStackIndex == aOther.mPrefixStackIndex &&
             mFrameIndex == aOther.mFrameIndex;
    }

   private:
    uint32_t mHash;
  };

  explicit UniqueStacks(JITFrameInfo&& aJITFrameInfo);

  // Return a StackKey for aFrame as the stack's root frame (no prefix).
  MOZ_MUST_USE StackKey BeginStack(const FrameKey& aFrame);

  // Return a new StackKey that is obtained by appending aFrame to aStack.
  MOZ_MUST_USE StackKey AppendFrame(const StackKey& aStack,
                                    const FrameKey& aFrame);

  // Look up frame keys for the given JIT address, and ensure that our frame
  // table has entries for the returned frame keys. The JSON for these frames
  // is taken from mJITInfoRanges.
  // aBufferPosition is needed in order to look up the correct JIT frame info
  // object in mJITInfoRanges.
  MOZ_MUST_USE mozilla::Maybe<nsTArray<UniqueStacks::FrameKey>>
  LookupFramesForJITAddressFromBufferPos(void* aJITAddress,
                                         uint64_t aBufferPosition);

  MOZ_MUST_USE uint32_t GetOrAddFrameIndex(const FrameKey& aFrame);
  MOZ_MUST_USE uint32_t GetOrAddStackIndex(const StackKey& aStack);

  void SpliceFrameTableElements(SpliceableJSONWriter& aWriter);
  void SpliceStackTableElements(SpliceableJSONWriter& aWriter);

 private:
  void StreamNonJITFrame(const FrameKey& aFrame);
  void StreamStack(const StackKey& aStack);

 public:
  mozilla::UniquePtr<UniqueJSONStrings> mUniqueStrings;

 private:
  SpliceableChunkedJSONWriter mFrameTableWriter;
  nsDataHashtable<nsGenericHashKey<FrameKey>, uint32_t> mFrameToIndexMap;

  SpliceableChunkedJSONWriter mStackTableWriter;
  nsDataHashtable<nsGenericHashKey<StackKey>, uint32_t> mStackToIndexMap;

  nsTArray<JITFrameInfoForBufferRange> mJITInfoRanges;
};

//
// Thread profile JSON Format
// --------------------------
//
// The profile contains much duplicate information. The output JSON of the
// profile attempts to deduplicate strings, frames, and stack prefixes, to cut
// down on size and to increase JSON streaming speed. Deduplicated values are
// streamed as indices into their respective tables.
//
// Further, arrays of objects with the same set of properties (e.g., samples,
// frames) are output as arrays according to a schema instead of an object
// with property names. A property that is not present is represented in the
// array as null or undefined.
//
// The format of the thread profile JSON is shown by the following example
// with 1 sample and 1 marker:
//
// {
//   "name": "Foo",
//   "tid": 42,
//   "samples":
//   {
//     "schema":
//     {
//       "stack": 0,          /* index into stackTable */
//       "time": 1,           /* number */
//       "responsiveness": 2, /* number */
//     },
//     "data":
//     [
//       [ 1, 0.0, 0.0 ]      /* { stack: 1, time: 0.0, responsiveness: 0.0 } */
//     ]
//   },
//
//   "markers":
//   {
//     "schema":
//     {
//       "name": 0,           /* index into stringTable */
//       "time": 1,           /* number */
//       "data": 2            /* arbitrary JSON */
//     },
//     "data":
//     [
//       [ 3, 0.1 ]           /* { name: 'example marker', time: 0.1 } */
//     ]
//   },
//
//   "stackTable":
//   {
//     "schema":
//     {
//       "prefix": 0,         /* index into stackTable */
//       "frame": 1           /* index into frameTable */
//     },
//     "data":
//     [
//       [ null, 0 ],         /* (root) */
//       [ 0,    1 ]          /* (root) > foo.js */
//     ]
//   },
//
//   "frameTable":
//   {
//     "schema":
//     {
//       "location": 0,       /* index into stringTable */
//       "implementation": 1, /* index into stringTable */
//       "optimizations": 2,  /* arbitrary JSON */
//       "line": 3,           /* number */
//       "column": 4,         /* number */
//       "category": 5        /* number */
//     },
//     "data":
//     [
//       [ 0 ],               /* { location: '(root)' } */
//       [ 1, 2 ]             /* { location: 'foo.js',
//                                 implementation: 'baseline' } */
//     ]
//   },
//
//   "stringTable":
//   [
//     "(root)",
//     "foo.js",
//     "baseline",
//     "example marker"
//   ]
// }
//
// Process:
// {
//   "name": "Bar",
//   "pid": 24,
//   "threads":
//   [
//     <0-N threads from above>
//   ],
//   "counters": /* includes the memory counter */
//   [
//     {
//       "name": "qwerty",
//       "category": "uiop",
//       "description": "this is qwerty uiop",
//       "sample_groups:
//       [
//         {
//           "id": 42, /* number (thread id, or object identifier (tab), etc) */
//           "samples:
//           {
//             "schema":
//             {
//               "time": 1,   /* number */
//               "number": 2, /* number (of times the counter was touched) */
//               "count": 3   /* number (total for the counter) */
//             },
//             "data":
//             [
//               [ 0.1, 1824,
//                 454622 ]   /* { time: 0.1, number: 1824, count: 454622 } */
//             ]
//           },
//         },
//         /* more sample-group objects with different id's */
//       ]
//     },
//     /* more counters */
//   ],
//   "memory":
//   {
//     "initial_heap": 12345678,
//     "samples:
//     {
//       "schema":
//       {
//         "time": 1,            /* number */
//         "rss": 2,             /* number */
//         "uss": 3              /* number */
//       },
//       "data":
//       [
//         /* { time: 0.1, rss: 12345678, uss: 87654321} */
//         [ 0.1, 12345678, 87654321 ]
//       ]
//     },
//   },
// }
//
#endif /* ndef ProfileBufferEntry_h */
