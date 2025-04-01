/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"

#include "js/GCAPI.h"

#include "gtest/gtest.h"

using namespace mozilla;

enum HolderKind { SingleZone, MultiZone };

class MyHolder final : public nsScriptObjectTracer, public JSHolderBase {
 public:
  explicit MyHolder(HolderKind kind = SingleZone, size_t value = 0)
      : nsScriptObjectTracer(FlagsForKind(kind)), value(value) {}

  const size_t value;

  NS_IMETHOD_(void) Root(void*) override { MOZ_CRASH(); }
  NS_IMETHOD_(void) Unlink(void*) override { MOZ_CRASH(); }
  NS_IMETHOD_(void) Unroot(void*) override { MOZ_CRASH(); }
  NS_IMETHOD_(void) DeleteCycleCollectable(void*) override { MOZ_CRASH(); }
  NS_IMETHOD_(void)
  Trace(void* aPtr, const TraceCallbacks& aCb, void* aClosure) override {
    MOZ_CRASH();
  }
  NS_IMETHOD TraverseNative(void* aPtr,
                            nsCycleCollectionTraversalCallback& aCb) override {
    MOZ_CRASH();
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_NAME_METHOD(MyHolder)

 private:
  Flags FlagsForKind(HolderKind kind) {
    return kind == MultiZone ? FlagMultiZoneJSHolder
                             : FlagMaybeSingleZoneJSHolder;
  }
};

template <typename Container>
static size_t CountEntries(Container& container) {
  size_t count = 0;
  for (typename Container::Iter i(container); !i.Done(); i.Next()) {
    MOZ_RELEASE_ASSERT(i->mHolder);
    MOZ_RELEASE_ASSERT(i->mTracer);
    count++;
  }
  return count;
}

JS::Zone* DummyZone = reinterpret_cast<JS::Zone*>(1);

JS::Zone* ZoneForKind(HolderKind kind) {
  return kind == MultiZone ? nullptr : DummyZone;
}

// Adapter functions to allow working with JSHolderMap and JSHolderList
// interchangeably.

static bool Has(JSHolderMap& map, MyHolder* holder) { return map.Has(holder); }
static bool Has(JSHolderList& list, MyHolder* holder) {
  JSHolderKey* key = &holder->mJSHolderKey;
  return list.Has(key);
}

static void Put(JSHolderMap& map, MyHolder* holder, JS::Zone* zone) {
  nsScriptObjectTracer* tracer = holder;
  map.Put(holder, tracer, zone);
}
static void Put(JSHolderList& list, MyHolder* holder, JS::Zone* zone) {
  MOZ_RELEASE_ASSERT(!zone);
  nsScriptObjectTracer* tracer = holder;
  JSHolderKey* key = &holder->mJSHolderKey;
  list.Put(holder, tracer, key);
}

static nsScriptObjectTracer* Get(JSHolderMap& map, MyHolder* holder) {
  return map.Get(holder);
}
static nsScriptObjectTracer* Get(JSHolderList& list, MyHolder* holder) {
  JSHolderKey* key = &holder->mJSHolderKey;
  return list.Get(holder, key);
}

static nsScriptObjectTracer* Extract(JSHolderMap& map, MyHolder* holder) {
  return map.Extract(holder);
}
static nsScriptObjectTracer* Extract(JSHolderList& list, MyHolder* holder) {
  JSHolderKey* key = &holder->mJSHolderKey;
  return list.Extract(holder, key);
}

TEST(JSHolderMap, Empty)
{
  JSHolderMap map;
  ASSERT_EQ(CountEntries(map), 0u);
}
TEST(JSHolderList, Empty)
{
  JSHolderList list;
  ASSERT_EQ(CountEntries(list), 0u);
}

template <typename Container>
static void TestAddAndRemove(HolderKind kind) {
  Container container;
  MyHolder holder(kind);
  nsScriptObjectTracer* tracer = &holder;

  ASSERT_FALSE(Has(container, &holder));
  ASSERT_EQ(Extract(container, &holder), nullptr);

  Put(container, &holder, ZoneForKind(kind));
  ASSERT_TRUE(Has(container, &holder));
  ASSERT_EQ(CountEntries(container), 1u);
  ASSERT_EQ(Get(container, &holder), tracer);

  ASSERT_EQ(Extract(container, &holder), tracer);
  ASSERT_EQ(Extract(container, &holder), nullptr);
  ASSERT_FALSE(Has(container, &holder));
  ASSERT_EQ(CountEntries(container), 0u);
}

TEST(JSHolderMap, AddAndRemove)
{
  TestAddAndRemove<JSHolderMap>(SingleZone);
  TestAddAndRemove<JSHolderMap>(MultiZone);
}
TEST(JSHolderList, AddAndRemove)
{
  TestAddAndRemove<JSHolderList>(MultiZone);
}

template <typename Container>
static void TestIterate(HolderKind kind) {
  Container container;
  MyHolder holder(kind, 0);

  Maybe<typename Container::Iter> iter;

  // Iterate an empty container.
  iter.emplace(container);
  ASSERT_TRUE(iter->Done());
  iter.reset();

  // Iterate a container with one entry.
  Put(container, &holder, ZoneForKind(kind));
  iter.emplace(container);
  ASSERT_FALSE(iter->Done());
  ASSERT_EQ(iter->Get().mHolder, &holder);
  iter->Next();
  ASSERT_TRUE(iter->Done());
  iter.reset();

  // Iterate a container with 10 entries.
  constexpr size_t count = 10;
  Vector<UniquePtr<MyHolder>, 0, InfallibleAllocPolicy> holders;
  bool seen[count] = {};
  for (size_t i = 1; i < count; i++) {
    MOZ_ALWAYS_TRUE(
        holders.emplaceBack(mozilla::MakeUnique<MyHolder>(kind, i)));
    Put(container, holders.back().get(), ZoneForKind(kind));
  }
  for (iter.emplace(container); !iter->Done(); iter->Next()) {
    MyHolder* holder = static_cast<MyHolder*>(iter->Get().mHolder);
    size_t value = holder->value;
    ASSERT_TRUE(value < count);
    ASSERT_FALSE(seen[value]);
    seen[value] = true;
  }
  for (const auto& s : seen) {
    ASSERT_TRUE(s);
  }
}

TEST(JSHolderMap, Iterate)
{
  TestIterate<JSHolderMap>(SingleZone);
  TestIterate<JSHolderMap>(MultiZone);
}
TEST(JSHolderList, Iterate)
{
  TestIterate<JSHolderList>(MultiZone);
}

template <typename Container>
static void TestAddRemoveMany(HolderKind kind, size_t count) {
  Container container;

  Vector<UniquePtr<MyHolder>, 0, InfallibleAllocPolicy> holders;
  for (size_t i = 0; i < count; i++) {
    MOZ_ALWAYS_TRUE(holders.emplaceBack(mozilla::MakeUnique<MyHolder>(kind)));
  }

  for (size_t i = 0; i < count; i++) {
    MyHolder* holder = holders[i].get();
    Put(container, holder, ZoneForKind(kind));
  }

  ASSERT_EQ(CountEntries(container), count);

  for (size_t i = 0; i < count; i++) {
    MyHolder* holder = holders[i].get();
    ASSERT_EQ(Extract(container, holder), holder);
  }

  ASSERT_EQ(CountEntries(container), 0u);
}

TEST(JSHolderMap, TestAddRemoveMany)
{
  TestAddRemoveMany<JSHolderMap>(SingleZone, 10000);
  TestAddRemoveMany<JSHolderMap>(MultiZone, 10000);
}
TEST(JSHolderList, TestAddRemoveMany)
{
  TestAddRemoveMany<JSHolderList>(MultiZone, 10000);
}

template <typename Container>
static void TestRemoveWhileIterating(HolderKind kind, size_t count) {
  Container container;
  Vector<UniquePtr<MyHolder>, 0, InfallibleAllocPolicy> holders;
  Maybe<typename Container::Iter> iter;

  for (size_t i = 0; i < count; i++) {
    MOZ_ALWAYS_TRUE(holders.emplaceBack(MakeUnique<MyHolder>(kind)));
  }

  // Iterate a container with one entry but remove it before we get to it.
  MyHolder* holder = holders[0].get();
  Put(container, holder, ZoneForKind(kind));
  iter.emplace(container);
  ASSERT_FALSE(iter->Done());
  ASSERT_EQ(Extract(container, holder), holder);
  iter->UpdateForRemovals();
  ASSERT_TRUE(iter->Done());

  // Check UpdateForRemovals is safe to call on a done iterator.
  iter->UpdateForRemovals();
  ASSERT_TRUE(iter->Done());
  iter.reset();

  // Add many holders and remove them mid way through iteration.

  for (size_t i = 0; i < count; i++) {
    MyHolder* holder = holders[i].get();
    Put(container, holder, ZoneForKind(kind));
  }

  iter.emplace(container);
  for (size_t i = 0; i < count / 2; i++) {
    iter->Next();
    ASSERT_FALSE(iter->Done());
  }

  for (size_t i = 0; i < count; i++) {
    MyHolder* holder = holders[i].get();
    ASSERT_EQ(Extract(container, holder), holder);
  }

  iter->UpdateForRemovals();

  ASSERT_TRUE(iter->Done());
  iter.reset();

  ASSERT_EQ(CountEntries(container), 0u);
}

TEST(JSHolderMap, TestRemoveWhileIterating)
{
  TestRemoveWhileIterating<JSHolderMap>(SingleZone, 10000);
  TestRemoveWhileIterating<JSHolderMap>(MultiZone, 10000);
}
TEST(JSHolderList, TestRemoveWhileIterating)
{
  TestRemoveWhileIterating<JSHolderList>(MultiZone, 10000);
}

class ObjectHolderBase {
 public:
  void SetObject(JSObject* aObject) { mObject = aObject; }

  void ClearObject() { mObject = nullptr; }

  JSObject* GetObject() const { return mObject; }
  JSObject* GetObjectUnbarriered() const { return mObject.unbarrieredGet(); }

  bool ObjectIsGray() const {
    JSObject* obj = mObject.unbarrieredGet();
    MOZ_RELEASE_ASSERT(obj);
    return JS::GCThingIsMarkedGray(JS::GCCellPtr(obj));
  }

 protected:
  JS::Heap<JSObject*> mObject;
};

class ObjectHolder final : public ObjectHolderBase {
 public:
  ObjectHolder() { HoldJSObjects(this); }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ObjectHolder)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(ObjectHolder)

 private:
  ~ObjectHolder() { DropJSObjects(this); }
};

NS_IMPL_CYCLE_COLLECTION_CLASS(ObjectHolder)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ObjectHolder)
  tmp->ClearObject();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ObjectHolder)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ObjectHolder)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mObject)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

class ObjectHolderWithKey final : public ObjectHolderBase, public JSHolderBase {
 public:
  ObjectHolderWithKey() { HoldJSObjectsWithKey(this); }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ObjectHolderWithKey)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(ObjectHolderWithKey)

 private:
  ~ObjectHolderWithKey() { DropJSObjectsWithKey(this); }
};

NS_IMPL_CYCLE_COLLECTION_CLASS(ObjectHolderWithKey)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ObjectHolderWithKey)
  tmp->ClearObject();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ObjectHolderWithKey)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ObjectHolderWithKey)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mObject)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

// Test GC things stored in JS holders are marked as gray roots by the GC.
template <typename Holder>
static void TestHoldersAreMarkedGray(JSContext* cx) {
  RefPtr holder(new Holder);

  JSObject* obj = JS_NewPlainObject(cx);
  ASSERT_TRUE(obj);
  holder->SetObject(obj);
  obj = nullptr;

  JS_GC(cx);

  ASSERT_TRUE(holder->ObjectIsGray());
}

// Test GC things stored in JS holders are updated by compacting GC.
template <typename Holder>
static void TestHoldersAreMoved(JSContext* cx, bool singleZone) {
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  ASSERT_TRUE(obj);

  // Set a property so we can check we have the same object at the end.
  const char* PropertyName = "answer";
  const int32_t PropertyValue = 42;
  JS::RootedValue value(cx, JS::Int32Value(PropertyValue));
  ASSERT_TRUE(JS_SetProperty(cx, obj, PropertyName, value));

  // Ensure the object is tenured.
  JS_GC(cx);

  RefPtr<Holder> holder(new Holder);
  holder->SetObject(obj);

  uintptr_t original = uintptr_t(obj.get());

  if (singleZone) {
    JS::PrepareZoneForGC(cx, js::GetContextZone(cx));
  } else {
    JS::PrepareForFullGC(cx);
  }

  JS::NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::DEBUG_GC);

  // Shrinking DEBUG_GC should move all GC things.
  ASSERT_NE(uintptr_t(holder->GetObject()), original);

  // Both root and holder should have been updated.
  ASSERT_EQ(obj, holder->GetObject());

  // Check it's the object we expect.
  value.setUndefined();
  ASSERT_TRUE(JS_GetProperty(cx, obj, PropertyName, &value));
  ASSERT_EQ(value, JS::Int32Value(PropertyValue));
}

static const JSClass GlobalClass = {"global", JSCLASS_GLOBAL_FLAGS,
                                    &JS::DefaultGlobalClassOps};

static void GetJSContext(JSContext** aContextOut) {
  CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::Get();
  ASSERT_NE(ccjscx, nullptr);
  JSContext* cx = ccjscx->Context();
  ASSERT_NE(cx, nullptr);
  *aContextOut = cx;
}

static void CreateGlobal(JSContext* cx, JS::MutableHandleObject aGlobalOut) {
  JS::RealmOptions options;
  // dummy
  options.behaviors().setReduceTimerPrecisionCallerType(
      JS::RTPCallerTypeToken{0});
  JSObject* global = JS_NewGlobalObject(cx, &GlobalClass, nullptr,
                                        JS::FireOnNewGlobalHook, options);
  ASSERT_NE(global, nullptr);
  aGlobalOut.set(global);
}

TEST(JSHolderMap, GCIntegration)
{
  JSContext* cx;
  GetJSContext(&cx);
  JS::RootedObject global(cx);
  CreateGlobal(cx, &global);
  JSAutoRealm ar(cx, global);
  TestHoldersAreMarkedGray<ObjectHolder>(cx);
  TestHoldersAreMoved<ObjectHolder>(cx, true);
  TestHoldersAreMoved<ObjectHolder>(cx, false);
}
TEST(JSHolderList, GCIntegration)
{
  JSContext* cx;
  GetJSContext(&cx);
  JS::RootedObject global(cx);
  CreateGlobal(cx, &global);
  JSAutoRealm ar(cx, global);
  TestHoldersAreMarkedGray<ObjectHolderWithKey>(cx);
  TestHoldersAreMoved<ObjectHolderWithKey>(cx, true);
  TestHoldersAreMoved<ObjectHolderWithKey>(cx, false);
}
