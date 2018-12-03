/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include "platform.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Logging.h"

#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

// JS
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/TrackedOptimizationInfo.h"

// Self
#include "ProfileBufferEntry.h"

using namespace mozilla;

////////////////////////////////////////////////////////////////////////
// BEGIN ProfileBufferEntry

ProfileBufferEntry::ProfileBufferEntry() : mKind(Kind::INVALID) {
  u.mString = nullptr;
}

// aString must be a static string.
ProfileBufferEntry::ProfileBufferEntry(Kind aKind, const char* aString)
    : mKind(aKind) {
  u.mString = aString;
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, char aChars[kNumChars])
    : mKind(aKind) {
  memcpy(u.mChars, aChars, kNumChars);
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, void* aPtr) : mKind(aKind) {
  u.mPtr = aPtr;
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, ProfilerMarker* aMarker)
    : mKind(aKind) {
  u.mMarker = aMarker;
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, double aDouble)
    : mKind(aKind) {
  u.mDouble = aDouble;
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, int aInt) : mKind(aKind) {
  u.mInt = aInt;
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, int64_t aInt64)
    : mKind(aKind) {
  u.mInt64 = aInt64;
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, uint64_t aUint64)
    : mKind(aKind) {
  u.mUint64 = aUint64;
}

// END ProfileBufferEntry
////////////////////////////////////////////////////////////////////////

class JSONSchemaWriter {
  JSONWriter& mWriter;
  uint32_t mIndex;

 public:
  explicit JSONSchemaWriter(JSONWriter& aWriter) : mWriter(aWriter), mIndex(0) {
    aWriter.StartObjectProperty("schema",
                                SpliceableJSONWriter::SingleLineStyle);
  }

  void WriteField(const char* aName) { mWriter.IntProperty(aName, mIndex++); }

  ~JSONSchemaWriter() { mWriter.EndObject(); }
};

struct TypeInfo {
  Maybe<nsCString> mKeyedBy;
  Maybe<nsCString> mName;
  Maybe<nsCString> mLocation;
  Maybe<unsigned> mLineNumber;
};

template <typename LambdaT>
class ForEachTrackedOptimizationTypeInfoLambdaOp
    : public JS::ForEachTrackedOptimizationTypeInfoOp {
 public:
  // aLambda needs to be a function with the following signature:
  // void lambda(JS::TrackedTypeSite site, const char* mirType,
  //             const nsTArray<TypeInfo>& typeset)
  // aLambda will be called once per entry.
  explicit ForEachTrackedOptimizationTypeInfoLambdaOp(LambdaT&& aLambda)
      : mLambda(aLambda) {}

  // This is called 0 or more times per entry, *before* operator() is called
  // for that entry.
  void readType(const char* keyedBy, const char* name, const char* location,
                const Maybe<unsigned>& lineno) override {
    TypeInfo info = {keyedBy ? Some(nsCString(keyedBy)) : Nothing(),
                     name ? Some(nsCString(name)) : Nothing(),
                     location ? Some(nsCString(location)) : Nothing(), lineno};
    mTypesetForUpcomingEntry.AppendElement(std::move(info));
  }

  void operator()(JS::TrackedTypeSite site, const char* mirType) override {
    nsTArray<TypeInfo> typeset(std::move(mTypesetForUpcomingEntry));
    mLambda(site, mirType, typeset);
  }

 private:
  nsTArray<TypeInfo> mTypesetForUpcomingEntry;
  LambdaT mLambda;
};

template <typename LambdaT>
ForEachTrackedOptimizationTypeInfoLambdaOp<LambdaT>
MakeForEachTrackedOptimizationTypeInfoLambdaOp(LambdaT&& aLambda) {
  return ForEachTrackedOptimizationTypeInfoLambdaOp<LambdaT>(
      std::move(aLambda));
}

// As mentioned in ProfileBufferEntry.h, the JSON format contains many
// arrays whose elements are laid out according to various schemas to help
// de-duplication. This RAII class helps write these arrays by keeping track of
// the last non-null element written and adding the appropriate number of null
// elements when writing new non-null elements. It also automatically opens and
// closes an array element on the given JSON writer.
//
// You grant the AutoArraySchemaWriter exclusive access to the JSONWriter and
// the UniqueJSONStrings objects for the lifetime of AutoArraySchemaWriter. Do
// not access them independently while the AutoArraySchemaWriter is alive.
// If you need to add complex objects, call FreeFormElement(), which will give
// you temporary access to the writer.
//
// Example usage:
//
//     // Define the schema of elements in this type of array: [FOO, BAR, BAZ]
//     enum Schema : uint32_t {
//       FOO = 0,
//       BAR = 1,
//       BAZ = 2
//     };
//
//     AutoArraySchemaWriter writer(someJsonWriter, someUniqueStrings);
//     if (shouldWriteFoo) {
//       writer.IntElement(FOO, getFoo());
//     }
//     ... etc ...
//
//     The elements need to be added in-order.
class MOZ_RAII AutoArraySchemaWriter {
 public:
  AutoArraySchemaWriter(SpliceableJSONWriter& aWriter,
                        UniqueJSONStrings& aStrings)
      : mJSONWriter(aWriter), mStrings(&aStrings), mNextFreeIndex(0) {
    mJSONWriter.StartArrayElement(SpliceableJSONWriter::SingleLineStyle);
  }

  explicit AutoArraySchemaWriter(SpliceableJSONWriter& aWriter)
      : mJSONWriter(aWriter), mStrings(nullptr), mNextFreeIndex(0) {
    mJSONWriter.StartArrayElement(SpliceableJSONWriter::SingleLineStyle);
  }

  ~AutoArraySchemaWriter() { mJSONWriter.EndArray(); }

  void IntElement(uint64_t aIndex, uint64_t aValue) {
    FillUpTo(aIndex);
    mJSONWriter.IntElement(aValue);
  }

  void DoubleElement(uint32_t aIndex, double aValue) {
    FillUpTo(aIndex);
    mJSONWriter.DoubleElement(aValue);
  }

  void BoolElement(uint32_t aIndex, bool aValue) {
    FillUpTo(aIndex);
    mJSONWriter.BoolElement(aValue);
  }

  void StringElement(uint32_t aIndex, const char* aValue) {
    MOZ_RELEASE_ASSERT(mStrings);
    FillUpTo(aIndex);
    mStrings->WriteElement(mJSONWriter, aValue);
  }

  // Write an element using a callback that takes a JSONWriter& and a
  // UniqueJSONStrings&.
  template <typename LambdaT>
  void FreeFormElement(uint32_t aIndex, LambdaT aCallback) {
    MOZ_RELEASE_ASSERT(mStrings);
    FillUpTo(aIndex);
    aCallback(mJSONWriter, *mStrings);
  }

 private:
  void FillUpTo(uint32_t aIndex) {
    MOZ_ASSERT(aIndex >= mNextFreeIndex);
    mJSONWriter.NullElements(aIndex - mNextFreeIndex);
    mNextFreeIndex = aIndex + 1;
  }

  SpliceableJSONWriter& mJSONWriter;
  UniqueJSONStrings* mStrings;
  uint32_t mNextFreeIndex;
};

template <typename LambdaT>
class ForEachTrackedOptimizationAttemptsLambdaOp
    : public JS::ForEachTrackedOptimizationAttemptOp {
 public:
  explicit ForEachTrackedOptimizationAttemptsLambdaOp(LambdaT&& aLambda)
      : mLambda(std::move(aLambda)) {}
  void operator()(JS::TrackedStrategy aStrategy,
                  JS::TrackedOutcome aOutcome) override {
    mLambda(aStrategy, aOutcome);
  }

 private:
  LambdaT mLambda;
};

template <typename LambdaT>
ForEachTrackedOptimizationAttemptsLambdaOp<LambdaT>
MakeForEachTrackedOptimizationAttemptsLambdaOp(LambdaT&& aLambda) {
  return ForEachTrackedOptimizationAttemptsLambdaOp<LambdaT>(
      std::move(aLambda));
}

UniqueJSONStrings::UniqueJSONStrings() { mStringTableWriter.StartBareList(); }

UniqueJSONStrings::UniqueJSONStrings(const UniqueJSONStrings& aOther) {
  mStringTableWriter.StartBareList();
  if (aOther.mStringToIndexMap.Count() > 0) {
    for (auto iter = aOther.mStringToIndexMap.ConstIter(); !iter.Done();
         iter.Next()) {
      mStringToIndexMap.Put(iter.Key(), iter.Data());
    }
    UniquePtr<char[]> stringTableJSON =
        aOther.mStringTableWriter.WriteFunc()->CopyData();
    mStringTableWriter.Splice(stringTableJSON.get());
  }
}

uint32_t UniqueJSONStrings::GetOrAddIndex(const char* aStr) {
  nsDependentCString str(aStr);

  uint32_t count = mStringToIndexMap.Count();
  auto entry = mStringToIndexMap.LookupForAdd(str);
  if (entry) {
    MOZ_ASSERT(entry.Data() < count);
    return entry.Data();
  }

  entry.OrInsert([&] { return count; });
  mStringTableWriter.StringElement(aStr);
  return count;
}

UniqueStacks::StackKey UniqueStacks::BeginStack(const FrameKey& aFrame) {
  return StackKey(GetOrAddFrameIndex(aFrame));
}

UniqueStacks::StackKey UniqueStacks::AppendFrame(const StackKey& aStack,
                                                 const FrameKey& aFrame) {
  return StackKey(aStack, GetOrAddStackIndex(aStack),
                  GetOrAddFrameIndex(aFrame));
}

uint32_t JITFrameInfoForBufferRange::JITFrameKey::Hash() const {
  uint32_t hash = 0;
  hash = AddToHash(hash, mCanonicalAddress);
  hash = AddToHash(hash, mDepth);
  return hash;
}

bool JITFrameInfoForBufferRange::JITFrameKey::operator==(
    const JITFrameKey& aOther) const {
  return mCanonicalAddress == aOther.mCanonicalAddress &&
         mDepth == aOther.mDepth;
}

template <class KeyClass, class T>
void CopyClassHashtable(nsClassHashtable<KeyClass, T>& aDest,
                        const nsClassHashtable<KeyClass, T>& aSrc) {
  for (auto iter = aSrc.ConstIter(); !iter.Done(); iter.Next()) {
    const T& objRef = *iter.Data();
    aDest.LookupOrAdd(iter.Key(), objRef);
  }
}

JITFrameInfoForBufferRange JITFrameInfoForBufferRange::Clone() const {
  nsClassHashtable<nsPtrHashKey<void>, nsTArray<JITFrameKey>>
      jitAddressToJITFramesMap;
  nsClassHashtable<nsGenericHashKey<JITFrameKey>, nsCString>
      jitFrameToFrameJSONMap;
  CopyClassHashtable(jitAddressToJITFramesMap, mJITAddressToJITFramesMap);
  CopyClassHashtable(jitFrameToFrameJSONMap, mJITFrameToFrameJSONMap);
  return JITFrameInfoForBufferRange{mRangeStart, mRangeEnd,
                                    std::move(jitAddressToJITFramesMap),
                                    std::move(jitFrameToFrameJSONMap)};
}

JITFrameInfo::JITFrameInfo(const JITFrameInfo& aOther)
    : mUniqueStrings(MakeUnique<UniqueJSONStrings>(*aOther.mUniqueStrings)) {
  for (const JITFrameInfoForBufferRange& range : aOther.mRanges) {
    mRanges.AppendElement(range.Clone());
  }
}

bool UniqueStacks::FrameKey::NormalFrameData::operator==(
    const NormalFrameData& aOther) const {
  return mLocation == aOther.mLocation &&
         mRelevantForJS == aOther.mRelevantForJS && mLine == aOther.mLine &&
         mColumn == aOther.mColumn && mCategory == aOther.mCategory;
}

bool UniqueStacks::FrameKey::JITFrameData::operator==(
    const JITFrameData& aOther) const {
  return mCanonicalAddress == aOther.mCanonicalAddress &&
         mDepth == aOther.mDepth && mRangeIndex == aOther.mRangeIndex;
}

uint32_t UniqueStacks::FrameKey::Hash() const {
  uint32_t hash = 0;
  if (mData.is<NormalFrameData>()) {
    const NormalFrameData& data = mData.as<NormalFrameData>();
    if (!data.mLocation.IsEmpty()) {
      hash = AddToHash(hash, HashString(data.mLocation.get()));
    }
    hash = AddToHash(hash, data.mRelevantForJS);
    if (data.mLine.isSome()) {
      hash = AddToHash(hash, *data.mLine);
    }
    if (data.mColumn.isSome()) {
      hash = AddToHash(hash, *data.mColumn);
    }
    if (data.mCategory.isSome()) {
      hash = AddToHash(hash, *data.mCategory);
    }
  } else {
    const JITFrameData& data = mData.as<JITFrameData>();
    hash = AddToHash(hash, data.mCanonicalAddress);
    hash = AddToHash(hash, data.mDepth);
    hash = AddToHash(hash, data.mRangeIndex);
  }
  return hash;
}

// Consume aJITFrameInfo by stealing its string table and its JIT frame info
// ranges. The JIT frame info contains JSON which refers to strings from the
// JIT frame info's string table, so our string table needs to have the same
// strings at the same indices.
UniqueStacks::UniqueStacks(JITFrameInfo&& aJITFrameInfo)
    : mUniqueStrings(std::move(aJITFrameInfo.mUniqueStrings)),
      mJITInfoRanges(std::move(aJITFrameInfo.mRanges)) {
  mFrameTableWriter.StartBareList();
  mStackTableWriter.StartBareList();
}

uint32_t UniqueStacks::GetOrAddStackIndex(const StackKey& aStack) {
  uint32_t count = mStackToIndexMap.Count();
  auto entry = mStackToIndexMap.LookupForAdd(aStack);
  if (entry) {
    MOZ_ASSERT(entry.Data() < count);
    return entry.Data();
  }

  entry.OrInsert([&] { return count; });
  StreamStack(aStack);
  return count;
}

template <typename RangeT, typename PosT>
struct PositionInRangeComparator final {
  bool Equals(const RangeT& aRange, PosT aPos) const {
    return aRange.mRangeStart <= aPos && aPos < aRange.mRangeEnd;
  }

  bool LessThan(const RangeT& aRange, PosT aPos) const {
    return aRange.mRangeEnd <= aPos;
  }
};

Maybe<nsTArray<UniqueStacks::FrameKey>>
UniqueStacks::LookupFramesForJITAddressFromBufferPos(void* aJITAddress,
                                                     uint64_t aBufferPos) {
  size_t rangeIndex = mJITInfoRanges.BinaryIndexOf(
      aBufferPos,
      PositionInRangeComparator<JITFrameInfoForBufferRange, uint64_t>());
  MOZ_RELEASE_ASSERT(
      rangeIndex != mJITInfoRanges.NoIndex,
      "Buffer position of jit address needs to be in one of the ranges");

  using JITFrameKey = JITFrameInfoForBufferRange::JITFrameKey;

  const JITFrameInfoForBufferRange& jitFrameInfoRange =
      mJITInfoRanges[rangeIndex];
  const nsTArray<JITFrameKey>* jitFrameKeys =
      jitFrameInfoRange.mJITAddressToJITFramesMap.Get(aJITAddress);
  if (!jitFrameKeys) {
    return Nothing();
  }

  // Map the array of JITFrameKeys to an array of FrameKeys, and ensure that
  // each of the FrameKeys exists in mFrameToIndexMap.
  nsTArray<FrameKey> frameKeys;
  for (const JITFrameKey& jitFrameKey : *jitFrameKeys) {
    FrameKey frameKey(jitFrameKey.mCanonicalAddress, jitFrameKey.mDepth,
                      rangeIndex);
    uint32_t index = mFrameToIndexMap.Count();
    auto entry = mFrameToIndexMap.LookupForAdd(frameKey);
    if (!entry) {
      // We need to add this frame to our frame table. The JSON for this frame
      // already exists in jitFrameInfoRange, we just need to splice it into
      // the frame table and give it an index.
      const nsCString* frameJSON =
          jitFrameInfoRange.mJITFrameToFrameJSONMap.Get(jitFrameKey);
      MOZ_RELEASE_ASSERT(frameJSON, "Should have cached JSON for this frame");
      mFrameTableWriter.Splice(frameJSON->get());
      entry.OrInsert([&] { return index; });
    }
    frameKeys.AppendElement(std::move(frameKey));
  }
  return Some(std::move(frameKeys));
}

uint32_t UniqueStacks::GetOrAddFrameIndex(const FrameKey& aFrame) {
  uint32_t count = mFrameToIndexMap.Count();
  auto entry = mFrameToIndexMap.LookupForAdd(aFrame);
  if (entry) {
    MOZ_ASSERT(entry.Data() < count);
    return entry.Data();
  }

  entry.OrInsert([&] { return count; });
  StreamNonJITFrame(aFrame);
  return count;
}

void UniqueStacks::SpliceFrameTableElements(SpliceableJSONWriter& aWriter) {
  mFrameTableWriter.EndBareList();
  aWriter.TakeAndSplice(mFrameTableWriter.WriteFunc());
}

void UniqueStacks::SpliceStackTableElements(SpliceableJSONWriter& aWriter) {
  mStackTableWriter.EndBareList();
  aWriter.TakeAndSplice(mStackTableWriter.WriteFunc());
}

void UniqueStacks::StreamStack(const StackKey& aStack) {
  enum Schema : uint32_t { PREFIX = 0, FRAME = 1 };

  AutoArraySchemaWriter writer(mStackTableWriter, *mUniqueStrings);
  if (aStack.mPrefixStackIndex.isSome()) {
    writer.IntElement(PREFIX, *aStack.mPrefixStackIndex);
  }
  writer.IntElement(FRAME, aStack.mFrameIndex);
}

void UniqueStacks::StreamNonJITFrame(const FrameKey& aFrame) {
  using NormalFrameData = FrameKey::NormalFrameData;

  enum Schema : uint32_t {
    LOCATION = 0,
    RELEVANT_FOR_JS = 1,
    IMPLEMENTATION = 2,
    OPTIMIZATIONS = 3,
    LINE = 4,
    COLUMN = 5,
    CATEGORY = 6
  };

  AutoArraySchemaWriter writer(mFrameTableWriter, *mUniqueStrings);

  const NormalFrameData& data = aFrame.mData.as<NormalFrameData>();
  writer.StringElement(LOCATION, data.mLocation.get());
  writer.BoolElement(RELEVANT_FOR_JS, data.mRelevantForJS);
  if (data.mLine.isSome()) {
    writer.IntElement(LINE, *data.mLine);
  }
  if (data.mColumn.isSome()) {
    writer.IntElement(COLUMN, *data.mColumn);
  }
  if (data.mCategory.isSome()) {
    writer.IntElement(CATEGORY, *data.mCategory);
  }
}

static void StreamJITFrameOptimizations(
    SpliceableJSONWriter& aWriter, UniqueJSONStrings& aUniqueStrings,
    JSContext* aContext, const JS::ProfiledFrameHandle& aJITFrame) {
  aWriter.StartObjectElement();
  {
    aWriter.StartArrayProperty("types");
    {
      auto op = MakeForEachTrackedOptimizationTypeInfoLambdaOp(
          [&](JS::TrackedTypeSite site, const char* mirType,
              const nsTArray<TypeInfo>& typeset) {
            aWriter.StartObjectElement();
            {
              aUniqueStrings.WriteProperty(aWriter, "site",
                                           JS::TrackedTypeSiteString(site));
              aUniqueStrings.WriteProperty(aWriter, "mirType", mirType);

              if (!typeset.IsEmpty()) {
                aWriter.StartArrayProperty("typeset");
                for (const TypeInfo& typeInfo : typeset) {
                  aWriter.StartObjectElement();
                  {
                    aUniqueStrings.WriteProperty(aWriter, "keyedBy",
                                                 typeInfo.mKeyedBy->get());
                    if (typeInfo.mName) {
                      aUniqueStrings.WriteProperty(aWriter, "name",
                                                   typeInfo.mName->get());
                    }
                    if (typeInfo.mLocation) {
                      aUniqueStrings.WriteProperty(aWriter, "location",
                                                   typeInfo.mLocation->get());
                    }
                    if (typeInfo.mLineNumber.isSome()) {
                      aWriter.IntProperty("line", *typeInfo.mLineNumber);
                    }
                  }
                  aWriter.EndObject();
                }
                aWriter.EndArray();
              }
            }
            aWriter.EndObject();
          });
      aJITFrame.forEachOptimizationTypeInfo(op);
    }
    aWriter.EndArray();

    JS::Rooted<JSScript*> script(aContext);
    jsbytecode* pc;
    aWriter.StartObjectProperty("attempts");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("strategy");
        schema.WriteField("outcome");
      }

      aWriter.StartArrayProperty("data");
      {
        auto op = MakeForEachTrackedOptimizationAttemptsLambdaOp(
            [&](JS::TrackedStrategy strategy, JS::TrackedOutcome outcome) {
              enum Schema : uint32_t { STRATEGY = 0, OUTCOME = 1 };

              AutoArraySchemaWriter writer(aWriter, aUniqueStrings);
              writer.StringElement(STRATEGY,
                                   JS::TrackedStrategyString(strategy));
              writer.StringElement(OUTCOME, JS::TrackedOutcomeString(outcome));
            });
        aJITFrame.forEachOptimizationAttempt(op, script.address(), &pc);
      }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    if (JSAtom* name = js::GetPropertyNameFromPC(script, pc)) {
      char buf[512];
      JS_PutEscapedFlatString(buf, ArrayLength(buf), js::AtomToFlatString(name),
                              0);
      aUniqueStrings.WriteProperty(aWriter, "propertyName", buf);
    }

    unsigned line, column;
    line = JS_PCToLineNumber(script, pc, &column);
    aWriter.IntProperty("line", line);
    aWriter.IntProperty("column", column);
  }
  aWriter.EndObject();
}

static void StreamJITFrame(JSContext* aContext, SpliceableJSONWriter& aWriter,
                           UniqueJSONStrings& aUniqueStrings,
                           const JS::ProfiledFrameHandle& aJITFrame) {
  enum Schema : uint32_t {
    LOCATION = 0,
    RELEVANT_FOR_JS = 1,
    IMPLEMENTATION = 2,
    OPTIMIZATIONS = 3,
    LINE = 4,
    COLUMN = 5,
    CATEGORY = 6
  };

  AutoArraySchemaWriter writer(aWriter, aUniqueStrings);

  writer.StringElement(LOCATION, aJITFrame.label());
  writer.BoolElement(RELEVANT_FOR_JS, false);

  JS::ProfilingFrameIterator::FrameKind frameKind = aJITFrame.frameKind();
  MOZ_ASSERT(frameKind == JS::ProfilingFrameIterator::Frame_Ion ||
             frameKind == JS::ProfilingFrameIterator::Frame_Baseline);
  writer.StringElement(
      IMPLEMENTATION,
      frameKind == JS::ProfilingFrameIterator::Frame_Ion ? "ion" : "baseline");

  if (aJITFrame.hasTrackedOptimizations()) {
    writer.FreeFormElement(
        OPTIMIZATIONS,
        [&](SpliceableJSONWriter& aWriter, UniqueJSONStrings& aUniqueStrings) {
          StreamJITFrameOptimizations(aWriter, aUniqueStrings, aContext,
                                      aJITFrame);
        });
  }
}

struct CStringWriteFunc : public JSONWriteFunc {
  nsACString& mBuffer;  // The struct must not outlive this buffer
  explicit CStringWriteFunc(nsACString& aBuffer) : mBuffer(aBuffer) {}

  void Write(const char* aStr) override { mBuffer.Append(aStr); }
};

static nsCString JSONForJITFrame(JSContext* aContext,
                                 const JS::ProfiledFrameHandle& aJITFrame,
                                 UniqueJSONStrings& aUniqueStrings) {
  nsCString json;
  SpliceableJSONWriter writer(MakeUnique<CStringWriteFunc>(json));
  StreamJITFrame(aContext, writer, aUniqueStrings, aJITFrame);
  return json;
}

void JITFrameInfo::AddInfoForRange(
    uint64_t aRangeStart, uint64_t aRangeEnd, JSContext* aCx,
    const std::function<void(const std::function<void(void*)>&)>&
        aJITAddressProvider) {
  if (aRangeStart == aRangeEnd) {
    return;
  }

  MOZ_RELEASE_ASSERT(aRangeStart < aRangeEnd);

  if (!mRanges.IsEmpty()) {
    const JITFrameInfoForBufferRange& prevRange = mRanges.LastElement();
    MOZ_RELEASE_ASSERT(prevRange.mRangeEnd <= aRangeStart,
                       "Ranges must be non-overlapping and added in-order.");
  }

  using JITFrameKey = JITFrameInfoForBufferRange::JITFrameKey;

  nsClassHashtable<nsPtrHashKey<void>, nsTArray<JITFrameKey>>
      jitAddressToJITFrameMap;
  nsClassHashtable<nsGenericHashKey<JITFrameKey>, nsCString>
      jitFrameToFrameJSONMap;

  aJITAddressProvider([&](void* aJITAddress) {
    // Make sure that we have cached data for aJITAddress.
    if (!jitAddressToJITFrameMap.Contains(aJITAddress)) {
      nsTArray<JITFrameKey>& jitFrameKeys =
          *jitAddressToJITFrameMap.LookupOrAdd(aJITAddress);
      for (JS::ProfiledFrameHandle handle :
           JS::GetProfiledFrames(aCx, aJITAddress)) {
        uint32_t depth = jitFrameKeys.Length();
        JITFrameKey jitFrameKey{handle.canonicalAddress(), depth};
        if (!jitFrameToFrameJSONMap.Contains(jitFrameKey)) {
          nsCString& json = *jitFrameToFrameJSONMap.LookupOrAdd(jitFrameKey);
          json = JSONForJITFrame(aCx, handle, *mUniqueStrings);
        }
        jitFrameKeys.AppendElement(jitFrameKey);
      }
    }
  });

  mRanges.AppendElement(JITFrameInfoForBufferRange{
      aRangeStart, aRangeEnd, std::move(jitAddressToJITFrameMap),
      std::move(jitFrameToFrameJSONMap)});
}

struct ProfileSample {
  uint32_t mStack;
  double mTime;
  Maybe<double> mResponsiveness;
  Maybe<double> mRSS;
  Maybe<double> mUSS;
};

static void WriteSample(SpliceableJSONWriter& aWriter,
                        UniqueJSONStrings& aUniqueStrings,
                        const ProfileSample& aSample) {
  enum Schema : uint32_t {
    STACK = 0,
    TIME = 1,
    RESPONSIVENESS = 2,
    RSS = 3,
    USS = 4
  };

  AutoArraySchemaWriter writer(aWriter, aUniqueStrings);

  writer.IntElement(STACK, aSample.mStack);

  writer.DoubleElement(TIME, aSample.mTime);

  if (aSample.mResponsiveness.isSome()) {
    writer.DoubleElement(RESPONSIVENESS, *aSample.mResponsiveness);
  }

  if (aSample.mRSS.isSome()) {
    writer.DoubleElement(RSS, *aSample.mRSS);
  }

  if (aSample.mUSS.isSome()) {
    writer.DoubleElement(USS, *aSample.mUSS);
  }
}

class EntryGetter {
 public:
  explicit EntryGetter(const ProfileBuffer& aBuffer,
                       uint64_t aInitialReadPos = 0)
      : mBuffer(aBuffer), mReadPos(aBuffer.mRangeStart) {
    if (aInitialReadPos != 0) {
      MOZ_RELEASE_ASSERT(aInitialReadPos >= aBuffer.mRangeStart &&
                         aInitialReadPos <= aBuffer.mRangeEnd);
      mReadPos = aInitialReadPos;
    }
  }

  bool Has() const { return mReadPos != mBuffer.mRangeEnd; }
  const ProfileBufferEntry& Get() const { return mBuffer.GetEntry(mReadPos); }
  void Next() { mReadPos++; }
  uint64_t CurPos() { return mReadPos; }

 private:
  const ProfileBuffer& mBuffer;
  uint64_t mReadPos;
};

// The following grammar shows legal sequences of profile buffer entries.
// The sequences beginning with a ThreadId entry are known as "samples".
//
// (
//   ( /* Samples */
//     ThreadId
//     Time
//     ( NativeLeafAddr
//     | Label FrameFlags? DynamicStringFragment* LineNumber? Category?
//     | JitReturnAddr
//     )+
//     Marker*
//     Responsiveness?
//     ResidentMemory?
//     UnsharedMemory?
//   )
//   | ( ResidentMemory UnsharedMemory? Time)  /* Memory */
//   | ( /* Counters */
//       CounterId
//       Time
//       (
//         CounterKey
//         Count
//         Number?
//       )*
//     )
//   | CollectionStart
//   | CollectionEnd
//   | Pause
//   | Resume
// )*
//
// The most complicated part is the stack entry sequence that begins with
// Label. Here are some examples.
//
// - ProfilingStack frames without a dynamic string:
//
//     Label("js::RunScript")
//     Category(ProfilingStackFrame::Category::JS)
//
//     Label("XREMain::XRE_main")
//     LineNumber(4660)
//     Category(ProfilingStackFrame::Category::OTHER)
//
//     Label("ElementRestyler::ComputeStyleChangeFor")
//     LineNumber(3003)
//     Category(ProfilingStackFrame::Category::CSS)
//
// - ProfilingStack frames with a dynamic string:
//
//     Label("nsObserverService::NotifyObservers")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("domwindo")
//     DynamicStringFragment("wopened")
//     LineNumber(291)
//     Category(ProfilingStackFrame::Category::OTHER)
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_JS_FRAME))
//     DynamicStringFragment("closeWin")
//     DynamicStringFragment("dow (chr")
//     DynamicStringFragment("ome://gl")
//     DynamicStringFragment("obal/con")
//     DynamicStringFragment("tent/glo")
//     DynamicStringFragment("balOverl")
//     DynamicStringFragment("ay.js:5)")
//     DynamicStringFragment("")          # this string holds the closing '\0'
//     LineNumber(25)
//     Category(ProfilingStackFrame::Category::JS)
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_JS_FRAME))
//     DynamicStringFragment("bound (s")
//     DynamicStringFragment("elf-host")
//     DynamicStringFragment("ed:914)")
//     LineNumber(945)
//     Category(ProfilingStackFrame::Category::JS)
//
// - A profiling stack frame with a dynamic string, but with privacy enabled:
//
//     Label("nsObserverService::NotifyObservers")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("(private")
//     DynamicStringFragment(")")
//     LineNumber(291)
//     Category(ProfilingStackFrame::Category::OTHER)
//
// - A profiling stack frame with an overly long dynamic string:
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("(too lon")
//     DynamicStringFragment("g)")
//     LineNumber(100)
//     Category(ProfilingStackFrame::Category::NETWORK)
//
// - A wasm JIT frame:
//
//     Label("")
//     FrameFlags(uint64_t(0))
//     DynamicStringFragment("wasm-fun")
//     DynamicStringFragment("ction[87")
//     DynamicStringFragment("36] (blo")
//     DynamicStringFragment("b:http:/")
//     DynamicStringFragment("/webasse")
//     DynamicStringFragment("mbly.org")
//     DynamicStringFragment("/3dc5759")
//     DynamicStringFragment("4-ce58-4")
//     DynamicStringFragment("626-975b")
//     DynamicStringFragment("-08ad116")
//     DynamicStringFragment("30bc1:38")
//     DynamicStringFragment("29856)")
//
// - A JS frame in a synchronous sample:
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("u (https")
//     DynamicStringFragment("://perf-")
//     DynamicStringFragment("html.io/")
//     DynamicStringFragment("ac0da204")
//     DynamicStringFragment("aaa44d75")
//     DynamicStringFragment("a800.bun")
//     DynamicStringFragment("dle.js:2")
//     DynamicStringFragment("5)")

// Because this is a format entirely internal to the Profiler, any parsing
// error indicates a bug in the ProfileBuffer writing or the parser itself,
// or possibly flaky hardware.
#define ERROR_AND_CONTINUE(msg)                            \
  {                                                        \
    fprintf(stderr, "ProfileBuffer parse error: %s", msg); \
    MOZ_ASSERT(false, msg);                                \
    continue;                                              \
  }

void ProfileBuffer::StreamSamplesToJSON(SpliceableJSONWriter& aWriter,
                                        int aThreadId, double aSinceTime,
                                        UniqueStacks& aUniqueStacks) const {
  UniquePtr<char[]> dynStrBuf = MakeUnique<char[]>(kMaxFrameKeyLength);

  EntryGetter e(*this);

  for (;;) {
    // This block skips entries until we find the start of the next sample.
    // This is useful in three situations.
    //
    // - The circular buffer overwrites old entries, so when we start parsing
    //   we might be in the middle of a sample, and we must skip forward to the
    //   start of the next sample.
    //
    // - We skip samples that don't have an appropriate ThreadId or Time.
    //
    // - We skip range Pause, Resume, CollectionStart, Marker, Counter
    //   and CollectionEnd entries between samples.
    while (e.Has()) {
      if (e.Get().IsThreadId()) {
        break;
      } else {
        e.Next();
      }
    }

    if (!e.Has()) {
      break;
    }

    if (e.Get().IsThreadId()) {
      int threadId = e.Get().u.mInt;
      e.Next();

      // Ignore samples that are for the wrong thread.
      if (threadId != aThreadId) {
        continue;
      }
    } else {
      // Due to the skip_to_next_sample block above, if we have an entry here
      // it must be a ThreadId entry.
      MOZ_CRASH();
    }

    ProfileSample sample;

    if (e.Has() && e.Get().IsTime()) {
      sample.mTime = e.Get().u.mDouble;
      e.Next();

      // Ignore samples that are too old.
      if (sample.mTime < aSinceTime) {
        continue;
      }
    } else {
      ERROR_AND_CONTINUE("expected a Time entry");
    }

    UniqueStacks::StackKey stack =
        aUniqueStacks.BeginStack(UniqueStacks::FrameKey("(root)"));

    int numFrames = 0;
    while (e.Has()) {
      if (e.Get().IsNativeLeafAddr()) {
        numFrames++;

        // Bug 753041: We need a double cast here to tell GCC that we don't
        // want to sign extend 32-bit addresses starting with 0xFXXXXXX.
        unsigned long long pc = (unsigned long long)(uintptr_t)e.Get().u.mPtr;
        char buf[20];
        SprintfLiteral(buf, "%#llx", pc);
        stack = aUniqueStacks.AppendFrame(stack, UniqueStacks::FrameKey(buf));
        e.Next();

      } else if (e.Get().IsLabel()) {
        numFrames++;

        const char* label = e.Get().u.mString;
        e.Next();

        using FrameFlags = js::ProfilingStackFrame::Flags;
        uint32_t frameFlags = 0;
        if (e.Has() && e.Get().IsFrameFlags()) {
          frameFlags = uint32_t(e.Get().u.mUint64);
          e.Next();
        }

        bool relevantForJS = frameFlags & uint32_t(FrameFlags::RELEVANT_FOR_JS);

        // Copy potential dynamic string fragments into dynStrBuf, so that
        // dynStrBuf will then contain the entire dynamic string.
        size_t i = 0;
        dynStrBuf[0] = '\0';
        while (e.Has()) {
          if (e.Get().IsDynamicStringFragment()) {
            for (size_t j = 0; j < ProfileBufferEntry::kNumChars; j++) {
              const char* chars = e.Get().u.mChars;
              if (i < kMaxFrameKeyLength) {
                dynStrBuf[i] = chars[j];
                i++;
              }
            }
            e.Next();
          } else {
            break;
          }
        }
        dynStrBuf[kMaxFrameKeyLength - 1] = '\0';
        bool hasDynamicString = (i != 0);

        nsCString frameLabel;
        if (label[0] != '\0' && hasDynamicString) {
          if (frameFlags & uint32_t(FrameFlags::STRING_TEMPLATE_METHOD)) {
            frameLabel.AppendPrintf("%s.%s", label, dynStrBuf.get());
          } else if (frameFlags &
                     uint32_t(FrameFlags::STRING_TEMPLATE_GETTER)) {
            frameLabel.AppendPrintf("get %s.%s", label, dynStrBuf.get());
          } else if (frameFlags &
                     uint32_t(FrameFlags::STRING_TEMPLATE_SETTER)) {
            frameLabel.AppendPrintf("set %s.%s", label, dynStrBuf.get());
          } else {
            frameLabel.AppendPrintf("%s %s", label, dynStrBuf.get());
          }
        } else if (hasDynamicString) {
          frameLabel.Append(dynStrBuf.get());
        } else {
          frameLabel.Append(label);
        }

        Maybe<unsigned> line;
        if (e.Has() && e.Get().IsLineNumber()) {
          line = Some(unsigned(e.Get().u.mInt));
          e.Next();
        }

        Maybe<unsigned> column;
        if (e.Has() && e.Get().IsColumnNumber()) {
          column = Some(unsigned(e.Get().u.mInt));
          e.Next();
        }

        Maybe<unsigned> category;
        if (e.Has() && e.Get().IsCategory()) {
          category = Some(unsigned(e.Get().u.mInt));
          e.Next();
        }

        stack = aUniqueStacks.AppendFrame(
            stack, UniqueStacks::FrameKey(std::move(frameLabel), relevantForJS,
                                          line, column, category));

      } else if (e.Get().IsJitReturnAddr()) {
        numFrames++;

        // A JIT frame may expand to multiple frames due to inlining.
        void* pc = e.Get().u.mPtr;
        const Maybe<nsTArray<UniqueStacks::FrameKey>>& frameKeys =
            aUniqueStacks.LookupFramesForJITAddressFromBufferPos(pc,
                                                                 e.CurPos());
        MOZ_RELEASE_ASSERT(frameKeys,
                           "Attempting to stream samples for a buffer range "
                           "for which we don't have JITFrameInfo?");
        for (const UniqueStacks::FrameKey& frameKey : *frameKeys) {
          stack = aUniqueStacks.AppendFrame(stack, frameKey);
        }

        e.Next();

      } else {
        break;
      }
    }

    if (numFrames == 0) {
      // It is possible to have empty stacks if native stackwalking is
      // disabled. Skip samples with empty stacks. (See Bug 1497985).
      // Thus, don't use ERROR_AND_CONTINUE, but just continue.
      continue;
    }

    sample.mStack = aUniqueStacks.GetOrAddStackIndex(stack);

    // Skip over the markers. We process them in StreamMarkersToJSON().
    while (e.Has()) {
      if (e.Get().IsMarker()) {
        e.Next();
      } else {
        break;
      }
    }

    if (e.Has() && e.Get().IsResponsiveness()) {
      sample.mResponsiveness = Some(e.Get().u.mDouble);
      e.Next();
    }

    if (e.Has() && e.Get().IsResidentMemory()) {
      sample.mRSS = Some(e.Get().u.mDouble);
      e.Next();
    }

    if (e.Has() && e.Get().IsUnsharedMemory()) {
      sample.mUSS = Some(e.Get().u.mDouble);
      e.Next();
    }

    WriteSample(aWriter, *aUniqueStacks.mUniqueStrings, sample);
  }
}

void ProfileBuffer::AddJITInfoForRange(uint64_t aRangeStart, int aThreadId,
                                       JSContext* aContext,
                                       JITFrameInfo& aJITFrameInfo) const {
  // We can only process JitReturnAddr entries if we have a JSContext.
  MOZ_RELEASE_ASSERT(aContext);

  aRangeStart = std::max(aRangeStart, mRangeStart);
  aJITFrameInfo.AddInfoForRange(
      aRangeStart, mRangeEnd, aContext,
      [&](const std::function<void(void*)>& aJITAddressConsumer) {
        // Find all JitReturnAddr entries in the given range for the given
        // thread, and call aJITAddressConsumer with those addresses.

        EntryGetter e(*this, aRangeStart);
        while (true) {
          // Advance to the next ThreadId entry.
          while (e.Has() && !e.Get().IsThreadId()) {
            e.Next();
          }
          if (!e.Has()) {
            break;
          }

          MOZ_ASSERT(e.Get().IsThreadId());
          int threadId = e.Get().u.mInt;
          e.Next();

          // Ignore samples that are for a different thread.
          if (threadId != aThreadId) {
            continue;
          }

          while (e.Has() && !e.Get().IsThreadId()) {
            if (e.Get().IsJitReturnAddr()) {
              aJITAddressConsumer(e.Get().u.mPtr);
            }
            e.Next();
          }
        }
      });
}

void ProfileBuffer::StreamMarkersToJSON(SpliceableJSONWriter& aWriter,
                                        int aThreadId,
                                        const TimeStamp& aProcessStartTime,
                                        double aSinceTime,
                                        UniqueStacks& aUniqueStacks) const {
  EntryGetter e(*this);

  // Stream all markers whose threadId matches aThreadId. We skip other entries,
  // because we process them in StreamSamplesToJSON().
  //
  // NOTE: The ThreadId of a marker is determined by its GetThreadId() method,
  // rather than ThreadId buffer entries, as markers can be added outside of
  // samples.
  while (e.Has()) {
    if (e.Get().IsMarker()) {
      const ProfilerMarker* marker = e.Get().u.mMarker;
      if (marker->GetTime() >= aSinceTime &&
          marker->GetThreadId() == aThreadId) {
        marker->StreamJSON(aWriter, aProcessStartTime, aUniqueStacks);
      }
    }
    e.Next();
  }
}

struct CounterKeyedSample {
  double mTime;
  uint64_t mNumber;
  int64_t mCount;
};

typedef nsTArray<CounterKeyedSample> CounterKeyedSamples;

static LazyLogModule sFuzzyfoxLog("Fuzzyfox");

typedef nsDataHashtable<nsUint64HashKey, CounterKeyedSamples> CounterMap;

void ProfileBuffer::StreamCountersToJSON(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         double aSinceTime) const {
  // Because this is a format entirely internal to the Profiler, any parsing
  // error indicates a bug in the ProfileBuffer writing or the parser itself,
  // or possibly flaky hardware.

  EntryGetter e(*this);
  enum Schema : uint32_t { TIME = 0, NUMBER = 1, COUNT = 2 };

  // Stream all counters. We skip other entries, because we process them in
  // StreamSamplesToJSON()/etc.
  //
  // Valid sequence in the buffer:
  // CounterID
  // Time
  // ( CounterKey Count Number? )*
  //
  // And the JSON (example):
  // "counters": {
  //  "name": "malloc",
  //  "category": "Memory",
  //  "description": "Amount of allocated memory",
  //  "sample_groups": {
  //   "id": 0,
  //   "samples": {
  //    "schema": {"time": 0, "number": 1, "count": 2},
  //    "data": [
  //     [
  //      16117.033968000002,
  //      2446216,
  //      6801320
  //     ],
  //     [
  //      16118.037638,
  //      2446216,
  //      6801320
  //     ],
  //    ],
  //   }
  //  }
  // },

  // Build the map of counters and populate it
  nsDataHashtable<nsVoidPtrHashKey, CounterMap> counters;

  while (e.Has()) {
    // skip all non-Counters, including if we start in the middle of a counter
    if (e.Get().IsCounterId()) {
      void* id = e.Get().u.mPtr;
      CounterMap& counter = counters.GetOrInsert(id);
      e.Next();
      if (!e.Has() || !e.Get().IsTime()) {
        ERROR_AND_CONTINUE("expected a Time entry");
      }
      double time = e.Get().u.mDouble;
      if (time >= aSinceTime) {
        e.Next();
        while (e.Has() && e.Get().IsCounterKey()) {
          uint64_t key = e.Get().u.mUint64;
          CounterKeyedSamples& data = counter.GetOrInsert(key);
          e.Next();
          if (!e.Has() || !e.Get().IsCount()) {
            ERROR_AND_CONTINUE("expected a Count entry");
          }
          int64_t count = e.Get().u.mUint64;
          e.Next();
          uint64_t number;
          if (!e.Has() || !e.Get().IsNumber()) {
            number = 0;
          } else {
            number = e.Get().u.mInt64;
          }
          CounterKeyedSample sample = {time, number, count};
          data.AppendElement(sample);
        }
      } else {
        // skip counter sample - only need to skip the initial counter
        // id, then let the loop at the top skip the rest
      }
    }
    e.Next();
  }
  // we have a map of a map of counter entries; dump them to JSON
  if (counters.Count() == 0) {
    return;
  }

  aWriter.StartArrayProperty("counters");
  for (auto iter = counters.Iter(); !iter.Done(); iter.Next()) {
    CounterMap& counter = iter.Data();
    const BaseProfilerCount* base_counter =
        static_cast<const BaseProfilerCount*>(iter.Key());

    aWriter.Start();
    aWriter.StringProperty("name", base_counter->mLabel);
    aWriter.StringProperty("category", base_counter->mCategory);
    aWriter.StringProperty("description", base_counter->mDescription);

    aWriter.StartObjectProperty("sample_groups");
    for (auto counter_iter = counter.Iter(); !counter_iter.Done();
         counter_iter.Next()) {
      CounterKeyedSamples& samples = counter_iter.Data();
      uint64_t key = counter_iter.Key();

      size_t size = samples.Length();
      if (size == 0) {
        continue;
      }
      aWriter.IntProperty("id", static_cast<int64_t>(key));
      aWriter.StartObjectProperty("samples");
      {
        // XXX Can we assume a missing count means 0?
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("time");
        schema.WriteField("number");
        schema.WriteField("count");
      }

      aWriter.StartArrayProperty("data");
      uint64_t previousNumber = 0;
      int64_t previousCount = 0;
      for (size_t i = 0; i < size; i++) {
        // Encode as deltas, and only encode if different than the last sample
        if (i == 0 || samples[i].mNumber != previousNumber ||
            samples[i].mCount != previousCount) {
          if (i != 0 && samples[i].mTime >= samples[i - 1].mTime) {
            MOZ_LOG(sFuzzyfoxLog, mozilla::LogLevel::Error,
                    ("Fuzzyfox Profiler Assertion: %f >= %f", samples[i].mTime,
                     samples[i - 1].mTime));
          }
          MOZ_ASSERT(i == 0 || samples[i].mTime >= samples[i - 1].mTime);
          MOZ_ASSERT(samples[i].mNumber >= previousNumber);

          aWriter.StartArrayElement(SpliceableJSONWriter::SingleLineStyle);
          aWriter.DoubleElement(samples[i].mTime);
          aWriter.IntElement(samples[i].mNumber - previousNumber);  // uint64_t
          aWriter.IntElement(samples[i].mCount - previousCount);    // int64_t
          aWriter.EndArray();
          previousNumber = samples[i].mNumber;
          previousCount = samples[i].mCount;
        }
      }
      aWriter.EndArray();   // data
      aWriter.EndObject();  // samples
    }
    aWriter.EndObject();  // sample groups
    aWriter.End();        // for each counter
  }
  aWriter.EndArray();  // counters
}

void ProfileBuffer::StreamMemoryToJSON(SpliceableJSONWriter& aWriter,
                                       const TimeStamp& aProcessStartTime,
                                       double aSinceTime) const {
  enum Schema : uint32_t { TIME = 0, RSS = 1, USS = 2 };

  EntryGetter e(*this);

  aWriter.StartObjectProperty("memory");
  // Stream all memory (rss/uss) data. We skip other entries, because we
  // process them in StreamSamplesToJSON()/etc.
  aWriter.IntProperty("initial_heap", 0);  // XXX FIX
  aWriter.StartObjectProperty("samples");
  {
    JSONSchemaWriter schema(aWriter);
    schema.WriteField("time");
    schema.WriteField("rss");
    schema.WriteField("uss");
  }

  aWriter.StartArrayProperty("data");
  int64_t previous_rss = 0;
  int64_t previous_uss = 0;
  while (e.Has()) {
    // valid sequence: Resident, Unshared?, Time
    if (e.Get().IsResidentMemory()) {
      int64_t rss = e.Get().u.mInt64;
      int64_t uss = 0;
      e.Next();
      if (e.Has()) {
        if (e.Get().IsUnsharedMemory()) {
          uss = e.Get().u.mDouble;
          e.Next();
          if (!e.Has()) {
            break;
          }
        }
        if (e.Get().IsTime()) {
          double time = e.Get().u.mDouble;
          if (time >= aSinceTime &&
              (previous_rss != rss || previous_uss != uss)) {
            aWriter.StartArrayElement(SpliceableJSONWriter::SingleLineStyle);
            aWriter.DoubleElement(time);
            aWriter.IntElement(rss);  // int64_t
            if (uss != 0) {
              aWriter.IntElement(uss);  // int64_t
            }
            aWriter.EndArray();
            previous_rss = rss;
            previous_uss = uss;
          }
        } else {
          ERROR_AND_CONTINUE("expected a Time entry");
        }
      }
    }
    e.Next();
  }
  aWriter.EndArray();   // data
  aWriter.EndObject();  // samples
  aWriter.EndObject();  // memory
}
#undef ERROR_AND_CONTINUE

static void AddPausedRange(SpliceableJSONWriter& aWriter, const char* aReason,
                           const Maybe<double>& aStartTime,
                           const Maybe<double>& aEndTime) {
  aWriter.Start();
  if (aStartTime) {
    aWriter.DoubleProperty("startTime", *aStartTime);
  } else {
    aWriter.NullProperty("startTime");
  }
  if (aEndTime) {
    aWriter.DoubleProperty("endTime", *aEndTime);
  } else {
    aWriter.NullProperty("endTime");
  }
  aWriter.StringProperty("reason", aReason);
  aWriter.End();
}

void ProfileBuffer::StreamPausedRangesToJSON(SpliceableJSONWriter& aWriter,
                                             double aSinceTime) const {
  EntryGetter e(*this);

  Maybe<double> currentPauseStartTime;
  Maybe<double> currentCollectionStartTime;

  while (e.Has()) {
    if (e.Get().IsPause()) {
      currentPauseStartTime = Some(e.Get().u.mDouble);
    } else if (e.Get().IsResume()) {
      AddPausedRange(aWriter, "profiler-paused", currentPauseStartTime,
                     Some(e.Get().u.mDouble));
      currentPauseStartTime = Nothing();
    } else if (e.Get().IsCollectionStart()) {
      currentCollectionStartTime = Some(e.Get().u.mDouble);
    } else if (e.Get().IsCollectionEnd()) {
      AddPausedRange(aWriter, "collecting", currentCollectionStartTime,
                     Some(e.Get().u.mDouble));
      currentCollectionStartTime = Nothing();
    }
    e.Next();
  }

  if (currentPauseStartTime) {
    AddPausedRange(aWriter, "profiler-paused", currentPauseStartTime,
                   Nothing());
  }
  if (currentCollectionStartTime) {
    AddPausedRange(aWriter, "collecting", currentCollectionStartTime,
                   Nothing());
  }
}

bool ProfileBuffer::DuplicateLastSample(int aThreadId,
                                        const TimeStamp& aProcessStartTime,
                                        Maybe<uint64_t>& aLastSample) {
  if (aLastSample && *aLastSample < mRangeStart) {
    // The last sample is no longer within the buffer range, so we cannot use
    // it. Reset the stored buffer position to Nothing().
    aLastSample.reset();
  }

  if (!aLastSample) {
    return false;
  }

  uint64_t lastSampleStartPos = *aLastSample;

  MOZ_RELEASE_ASSERT(GetEntry(lastSampleStartPos).IsThreadId() &&
                     GetEntry(lastSampleStartPos).u.mInt == aThreadId);

  aLastSample = Some(AddThreadIdEntry(aThreadId));

  EntryGetter e(*this, lastSampleStartPos + 1);

  // Go through the whole entry and duplicate it, until we find the next one.
  while (e.Has()) {
    switch (e.Get().GetKind()) {
      case ProfileBufferEntry::Kind::Pause:
      case ProfileBufferEntry::Kind::Resume:
      case ProfileBufferEntry::Kind::CollectionStart:
      case ProfileBufferEntry::Kind::CollectionEnd:
      case ProfileBufferEntry::Kind::ThreadId:
        // We're done.
        return true;
      case ProfileBufferEntry::Kind::Time:
        // Copy with new time
        AddEntry(ProfileBufferEntry::Time(
            (TimeStamp::Now() - aProcessStartTime).ToMilliseconds()));
        break;
      case ProfileBufferEntry::Kind::Marker:
      case ProfileBufferEntry::Kind::ResidentMemory:
      case ProfileBufferEntry::Kind::UnsharedMemory:
      case ProfileBufferEntry::Kind::CounterKey:
      case ProfileBufferEntry::Kind::Number:
      case ProfileBufferEntry::Kind::Count:
      case ProfileBufferEntry::Kind::Responsiveness:
        // Don't copy anything not part of a thread's stack sample
        break;
      case ProfileBufferEntry::Kind::CounterId:
        // CounterId is normally followed by Time - if so, we'd like
        // to skip it.  If we duplicate Time, it won't hurt anything, just
        // waste buffer space (and this can happen if the CounterId has
        // fallen off the end of the buffer, but Time (and Number/Count)
        // are still in the buffer).
        e.Next();
        if (e.Has() && e.Get().GetKind() != ProfileBufferEntry::Kind::Time) {
          // this would only happen if there was an invalid sequence
          // in the buffer.  Don't skip it.
          continue;
        }
        // we've skipped Time
        break;
      default: {
        // Copy anything else we don't know about.
        ProfileBufferEntry entry = e.Get();
        AddEntry(entry);
        break;
      }
    }
    e.Next();
  }
  return true;
}

void ProfileBuffer::DiscardSamplesBeforeTime(double aTime) {
  EntryGetter e(*this);
  for (;;) {
    // This block skips entries until we find the start of the next sample.
    // This is useful in three situations.
    //
    // - The circular buffer overwrites old entries, so when we start parsing
    //   we might be in the middle of a sample, and we must skip forward to the
    //   start of the next sample.
    //
    // - We skip samples that don't have an appropriate ThreadId or Time.
    //
    // - We skip range Pause, Resume, CollectionStart, Marker, and CollectionEnd
    //   entries between samples.
    while (e.Has()) {
      if (e.Get().IsThreadId()) {
        break;
      } else {
        e.Next();
      }
    }

    if (!e.Has()) {
      break;
    }

    MOZ_RELEASE_ASSERT(e.Get().IsThreadId());
    uint64_t sampleStartPos = e.CurPos();
    e.Next();

    if (e.Has() && e.Get().IsTime()) {
      double sampleTime = e.Get().u.mDouble;

      if (sampleTime >= aTime) {
        // This is the first sample within the window of time that we want to
        // keep. Throw away all samples before sampleStartPos and return.
        mRangeStart = sampleStartPos;
        return;
      }
    }
  }
}

// END ProfileBuffer
////////////////////////////////////////////////////////////////////////
