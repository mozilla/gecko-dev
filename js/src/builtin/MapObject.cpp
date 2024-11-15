/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/MapObject.h"

#include "jsapi.h"

#include "builtin/OrderedHashTableObject.h"
#include "gc/GCContext.h"
#include "jit/InlinableNatives.h"
#include "js/MapAndSet.h"
#include "js/PropertyAndElement.h"  // JS_DefineFunctions
#include "js/PropertySpec.h"
#include "js/Utility.h"
#include "vm/BigIntType.h"
#include "vm/EqualityOperations.h"  // js::SameValue
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/SelfHosting.h"
#include "vm/SymbolType.h"

#ifdef ENABLE_RECORD_TUPLE
#  include "vm/RecordType.h"
#  include "vm/TupleType.h"
#endif

#include "builtin/OrderedHashTableObject-inl.h"
#include "gc/GCContext-inl.h"
#include "gc/Marking-inl.h"
#include "vm/GeckoProfiler-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::NumberEqualsInt32;

/*** HashableValue **********************************************************/

static PreBarriered<Value> NormalizeDoubleValue(double d) {
  int32_t i;
  if (NumberEqualsInt32(d, &i)) {
    // Normalize int32_t-valued doubles to int32_t for faster hashing and
    // testing. Note: we use NumberEqualsInt32 here instead of NumberIsInt32
    // because we want -0 and 0 to be normalized to the same thing.
    return Int32Value(i);
  }

  // Normalize the sign bit of a NaN.
  return JS::CanonicalizedDoubleValue(d);
}

bool HashableValue::setValue(JSContext* cx, HandleValue v) {
  if (v.isString()) {
    // Atomize so that hash() and operator==() are fast and infallible.
    JSString* str = AtomizeString(cx, v.toString());
    if (!str) {
      return false;
    }
    value = StringValue(str);
  } else if (v.isDouble()) {
    value = NormalizeDoubleValue(v.toDouble());
#ifdef ENABLE_RECORD_TUPLE
  } else if (v.isExtendedPrimitive()) {
    JSObject& obj = v.toExtendedPrimitive();
    if (obj.is<RecordType>()) {
      if (!obj.as<RecordType>().ensureAtomized(cx)) {
        return false;
      }
    } else {
      MOZ_ASSERT(obj.is<TupleType>());
      if (!obj.as<TupleType>().ensureAtomized(cx)) {
        return false;
      }
    }
    value = v;
#endif
  } else {
    value = v;
  }

  MOZ_ASSERT(value.isUndefined() || value.isNull() || value.isBoolean() ||
             value.isNumber() || value.isString() || value.isSymbol() ||
             value.isObject() || value.isBigInt() ||
             IF_RECORD_TUPLE(value.isExtendedPrimitive(), false));
  return true;
}

static HashNumber HashValue(const Value& v,
                            const mozilla::HashCodeScrambler& hcs) {
  // HashableValue::setValue normalizes values so that the SameValue relation
  // on HashableValues is the same as the == relationship on
  // value.asRawBits(). So why not just return that? Security.
  //
  // To avoid revealing GC of atoms, string-based hash codes are computed
  // from the string contents rather than any pointer; to avoid revealing
  // addresses, pointer-based hash codes are computed using the
  // HashCodeScrambler.

  if (v.isString()) {
    return v.toString()->asAtom().hash();
  }
  if (v.isSymbol()) {
    return v.toSymbol()->hash();
  }
  if (v.isBigInt()) {
    return MaybeForwarded(v.toBigInt())->hash();
  }
#ifdef ENABLE_RECORD_TUPLE
  if (v.isExtendedPrimitive()) {
    JSObject* obj = MaybeForwarded(&v.toExtendedPrimitive());
    auto hasher = [&hcs](const Value& v) {
      return HashValue(
          v.isDouble() ? NormalizeDoubleValue(v.toDouble()).get() : v, hcs);
    };

    if (obj->is<RecordType>()) {
      return obj->as<RecordType>().hash(hasher);
    }
    MOZ_ASSERT(obj->is<TupleType>());
    return obj->as<TupleType>().hash(hasher);
  }
#endif
  if (v.isObject()) {
    return hcs.scramble(v.asRawBits());
  }

  MOZ_ASSERT(!v.isGCThing(), "do not reveal pointers via hash codes");
  return mozilla::HashGeneric(v.asRawBits());
}

HashNumber HashableValue::hash(const mozilla::HashCodeScrambler& hcs) const {
  return HashValue(value, hcs);
}

#ifdef ENABLE_RECORD_TUPLE
inline bool SameExtendedPrimitiveType(const PreBarriered<Value>& a,
                                      const PreBarriered<Value>& b) {
  return a.toExtendedPrimitive().getClass() ==
         b.toExtendedPrimitive().getClass();
}
#endif

bool HashableValue::equals(const HashableValue& other) const {
  // Two HashableValues are equal if they have equal bits.
  bool b = (value.asRawBits() == other.value.asRawBits());

  if (!b && (value.type() == other.value.type())) {
    if (value.isBigInt()) {
      // BigInt values are considered equal if they represent the same
      // mathematical value.
      b = BigInt::equal(value.toBigInt(), other.value.toBigInt());
    }
#ifdef ENABLE_RECORD_TUPLE
    else if (value.isExtendedPrimitive() &&
             SameExtendedPrimitiveType(value, other.value)) {
      b = js::SameValueZeroLinear(value, other.value);
    }
#endif
  }

#ifdef DEBUG
  bool same;
  JSContext* cx = TlsContext.get();
  RootedValue valueRoot(cx, value);
  RootedValue otherRoot(cx, other.value);
  MOZ_ASSERT(SameValueZero(cx, valueRoot, otherRoot, &same));
  MOZ_ASSERT(same == b);
#endif
  return b;
}

/*** MapIterator ************************************************************/

namespace {} /* anonymous namespace */

static const JSClassOps MapIteratorObjectClassOps = {
    nullptr,                      // addProperty
    nullptr,                      // delProperty
    nullptr,                      // enumerate
    nullptr,                      // newEnumerate
    nullptr,                      // resolve
    nullptr,                      // mayResolve
    MapIteratorObject::finalize,  // finalize
    nullptr,                      // call
    nullptr,                      // construct
    nullptr,                      // trace
};

static const ClassExtension MapIteratorObjectClassExtension = {
    MapIteratorObject::objectMoved,  // objectMovedOp
};

const JSClass MapIteratorObject::class_ = {
    "Map Iterator",
    JSCLASS_HAS_RESERVED_SLOTS(MapIteratorObject::SlotCount) |
        JSCLASS_FOREGROUND_FINALIZE | JSCLASS_SKIP_NURSERY_FINALIZE,
    &MapIteratorObjectClassOps,
    JS_NULL_CLASS_SPEC,
    &MapIteratorObjectClassExtension,
};

const JSFunctionSpec MapIteratorObject::methods[] = {
    JS_SELF_HOSTED_FN("next", "MapIteratorNext", 0, 0),
    JS_FS_END,
};

static MapObject::Table::Range* MapIteratorObjectRange(NativeObject* obj) {
  MOZ_ASSERT(obj->is<MapIteratorObject>());
  return obj->maybePtrFromReservedSlot<MapObject::Table::Range>(
      MapIteratorObject::RangeSlot);
}

inline MapObject::IteratorKind MapIteratorObject::kind() const {
  int32_t i = getReservedSlot(KindSlot).toInt32();
  MOZ_ASSERT(i == MapObject::Keys || i == MapObject::Values ||
             i == MapObject::Entries);
  return MapObject::IteratorKind(i);
}

/* static */
bool GlobalObject::initMapIteratorProto(JSContext* cx,
                                        Handle<GlobalObject*> global) {
  Rooted<JSObject*> base(
      cx, GlobalObject::getOrCreateIteratorPrototype(cx, global));
  if (!base) {
    return false;
  }
  Rooted<PlainObject*> proto(
      cx, GlobalObject::createBlankPrototypeInheriting<PlainObject>(cx, base));
  if (!proto) {
    return false;
  }
  if (!JS_DefineFunctions(cx, proto, MapIteratorObject::methods) ||
      !DefineToStringTag(cx, proto, cx->names().Map_Iterator_)) {
    return false;
  }
  global->initBuiltinProto(ProtoKind::MapIteratorProto, proto);
  return true;
}

template <typename TableObject>
static inline bool HasRegisteredNurseryRanges(TableObject* t) {
  Value v = t->getReservedSlot(TableObject::RegisteredNurseryRangesSlot);
  return v.toBoolean();
}

template <typename TableObject>
static inline void SetRegisteredNurseryRanges(TableObject* t, bool b) {
  t->setReservedSlot(TableObject::RegisteredNurseryRangesSlot,
                     JS::BooleanValue(b));
}

MapIteratorObject* MapIteratorObject::create(JSContext* cx, HandleObject obj,
                                             MapObject::IteratorKind kind) {
  Handle<MapObject*> mapobj(obj.as<MapObject>());
  Rooted<GlobalObject*> global(cx, &mapobj->global());
  Rooted<JSObject*> proto(
      cx, GlobalObject::getOrCreateMapIteratorPrototype(cx, global));
  if (!proto) {
    return nullptr;
  }

  MapIteratorObject* iterobj =
      NewObjectWithGivenProto<MapIteratorObject>(cx, proto);
  if (!iterobj) {
    return nullptr;
  }

  iterobj->init(mapobj, kind);

  constexpr size_t BufferSize =
      RoundUp(sizeof(MapObject::Table::Range), gc::CellAlignBytes);

  Nursery& nursery = cx->nursery();
  void* buffer =
      nursery.allocateBufferSameLocation(iterobj, BufferSize, js::MallocArena);
  if (!buffer) {
    // Retry with |iterobj| and |buffer| forcibly tenured.
    iterobj = NewTenuredObjectWithGivenProto<MapIteratorObject>(cx, proto);
    if (!iterobj) {
      return nullptr;
    }

    iterobj->init(mapobj, kind);

    buffer = nursery.allocateBufferSameLocation(iterobj, BufferSize,
                                                js::MallocArena);
    if (!buffer) {
      ReportOutOfMemory(cx);
      return nullptr;
    }
  }

  bool insideNursery = IsInsideNursery(iterobj);
  MOZ_ASSERT(insideNursery == nursery.isInside(buffer));

  if (insideNursery && !HasRegisteredNurseryRanges(mapobj.get())) {
    if (!cx->nursery().addMapWithNurseryRanges(mapobj)) {
      ReportOutOfMemory(cx);
      return nullptr;
    }
    SetRegisteredNurseryRanges(mapobj.get(), true);
  }

  auto range = MapObject::Table(mapobj).createRange(buffer, insideNursery);
  iterobj->setReservedSlot(RangeSlot, PrivateValue(range));

  return iterobj;
}

void MapIteratorObject::finalize(JS::GCContext* gcx, JSObject* obj) {
  MOZ_ASSERT(gcx->onMainThread());
  MOZ_ASSERT(!IsInsideNursery(obj));

  auto range = MapIteratorObjectRange(&obj->as<NativeObject>());
  MOZ_ASSERT(!gcx->runtime()->gc.nursery().isInside(range));

  // Bug 1560019: Malloc memory associated with MapIteratorObjects is not
  // currently tracked.
  gcx->deleteUntracked(range);
}

size_t MapIteratorObject::objectMoved(JSObject* obj, JSObject* old) {
  if (!IsInsideNursery(old)) {
    return 0;
  }

  MapIteratorObject* iter = &obj->as<MapIteratorObject>();
  MapObject::Table::Range* range = MapIteratorObjectRange(iter);
  if (!range) {
    return 0;
  }

  Nursery& nursery = iter->runtimeFromMainThread()->gc.nursery();
  if (!nursery.isInside(range)) {
    nursery.removeMallocedBufferDuringMinorGC(range);
  }

  size_t size = RoundUp(sizeof(MapObject::Table::Range), gc::CellAlignBytes);
  AutoEnterOOMUnsafeRegion oomUnsafe;
  void* buffer = nursery.allocateBufferSameLocation(obj, size, js::MallocArena);
  if (!buffer) {
    oomUnsafe.crash("MapIteratorObject::objectMoved");
  }

  MapObject* mapObj = iter->target();

  bool iteratorIsInNursery = IsInsideNursery(obj);
  MOZ_ASSERT(iteratorIsInNursery == nursery.isInside(buffer));
  auto* newRange =
      new (buffer) MapObject::Table::Range(mapObj, *range, iteratorIsInNursery);
  range->~Range();
  iter->setReservedSlot(MapIteratorObject::RangeSlot, PrivateValue(newRange));

  return size;
}

MapObject* MapIteratorObject::target() const {
  Value value = getFixedSlot(TargetSlot);
  if (value.isUndefined()) {
    return nullptr;
  }

  return &MaybeForwarded(&value.toObject())->as<MapObject>();
}

template <typename Range>
static void DestroyRange(JSObject* iterator, Range* range) {
  MOZ_ASSERT(IsInsideNursery(iterator) ==
             iterator->runtimeFromMainThread()->gc.nursery().isInside(range));

  range->~Range();
  if (!IsInsideNursery(iterator)) {
    js_free(range);
  }
}

bool MapIteratorObject::next(MapIteratorObject* mapIterator,
                             ArrayObject* resultPairObj) {
  // IC code calls this directly.
  AutoUnsafeCallWithABI unsafe;

  // Check invariants for inlined GetNextMapEntryForIterator.

  // The array should be tenured, so that post-barrier can be done simply.
  MOZ_ASSERT(resultPairObj->isTenured());

  // The array elements should be fixed.
  MOZ_ASSERT(resultPairObj->hasFixedElements());
  MOZ_ASSERT(resultPairObj->getDenseInitializedLength() == 2);
  MOZ_ASSERT(resultPairObj->getDenseCapacity() >= 2);

  MapObject::Table::Range* range = MapIteratorObjectRange(mapIterator);
  if (!range) {
    return true;
  }

  MapObject* mapObj = mapIterator->target();
  if (range->empty(mapObj)) {
    DestroyRange<MapObject::Table::Range>(mapIterator, range);
    mapIterator->setReservedSlot(RangeSlot, PrivateValue(nullptr));
    return true;
  }

  switch (mapIterator->kind()) {
    case MapObject::Keys:
      resultPairObj->setDenseElement(0, range->front(mapObj).key.get());
      break;

    case MapObject::Values:
      resultPairObj->setDenseElement(1, range->front(mapObj).value);
      break;

    case MapObject::Entries: {
      resultPairObj->setDenseElement(0, range->front(mapObj).key.get());
      resultPairObj->setDenseElement(1, range->front(mapObj).value);
      break;
    }
  }
  range->popFront(mapObj);
  return false;
}

/* static */
JSObject* MapIteratorObject::createResultPair(JSContext* cx) {
  Rooted<ArrayObject*> resultPairObj(
      cx, NewDenseFullyAllocatedArray(cx, 2, TenuredObject));
  if (!resultPairObj) {
    return nullptr;
  }

  resultPairObj->setDenseInitializedLength(2);
  resultPairObj->initDenseElement(0, NullValue());
  resultPairObj->initDenseElement(1, NullValue());

  return resultPairObj;
}

/*** Map ********************************************************************/

struct js::UnbarrieredHashPolicy {
  using Lookup = Value;
  static HashNumber hash(const Lookup& v,
                         const mozilla::HashCodeScrambler& hcs) {
    return HashValue(v, hcs);
  }
  static bool match(const Value& k, const Lookup& l) { return k == l; }
  static bool isEmpty(const Value& v) { return v.isMagic(JS_HASH_KEY_EMPTY); }
  static void makeEmpty(Value* vp) { vp->setMagic(JS_HASH_KEY_EMPTY); }
};

// MapObject::Table, ::UnbarrieredTable and ::PreBarrieredTable must all have
// the same memory layout.
static_assert(sizeof(MapObject::Table::Entry) ==
              sizeof(MapObject::UnbarrieredTable::Entry));
static_assert(sizeof(MapObject::Table::Entry) ==
              sizeof(MapObject::PreBarrieredTable::Entry));

const JSClassOps MapObject::classOps_ = {
    nullptr,   // addProperty
    nullptr,   // delProperty
    nullptr,   // enumerate
    nullptr,   // newEnumerate
    nullptr,   // resolve
    nullptr,   // mayResolve
    finalize,  // finalize
    nullptr,   // call
    nullptr,   // construct
    trace,     // trace
};

const ClassSpec MapObject::classSpec_ = {
    GenericCreateConstructor<MapObject::construct, 0, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<MapObject>,
    MapObject::staticMethods,
    MapObject::staticProperties,
    MapObject::methods,
    MapObject::properties,
    MapObject::finishInit,
};

const ClassExtension MapObject::classExtension_ = {
    MapObject::objectMoved,  // objectMovedOp
};

const JSClass MapObject::class_ = {
    "Map",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(MapObject::SlotCount) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_Map) | JSCLASS_BACKGROUND_FINALIZE |
        JSCLASS_SKIP_NURSERY_FINALIZE,
    &MapObject::classOps_, &MapObject::classSpec_, &MapObject::classExtension_};

const JSClass MapObject::protoClass_ = {
    "Map.prototype",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Map),
    JS_NULL_CLASS_OPS,
    &MapObject::classSpec_,
};

const JSPropertySpec MapObject::properties[] = {
    JS_PSG("size", size, 0),
    JS_STRING_SYM_PS(toStringTag, "Map", JSPROP_READONLY),
    JS_PS_END,
};

const JSFunctionSpec MapObject::methods[] = {
    JS_INLINABLE_FN("get", get, 1, 0, MapGet),
    JS_INLINABLE_FN("has", has, 1, 0, MapHas),
    JS_FN("set", set, 2, 0),
    JS_FN("delete", delete_, 1, 0),
    JS_FN("keys", keys, 0, 0),
    JS_FN("values", values, 0, 0),
    JS_FN("clear", clear, 0, 0),
    JS_SELF_HOSTED_FN("forEach", "MapForEach", 2, 0),
    JS_FN("entries", entries, 0, 0),
    // @@iterator is re-defined in finishInit so that it has the
    // same identity as |entries|.
    JS_SYM_FN(iterator, entries, 0, 0),
    JS_FS_END,
};

const JSPropertySpec MapObject::staticProperties[] = {
    JS_SELF_HOSTED_SYM_GET(species, "$MapSpecies", 0),
    JS_PS_END,
};

const JSFunctionSpec MapObject::staticMethods[] = {
    JS_SELF_HOSTED_FN("groupBy", "MapGroupBy", 2, 0),
    JS_FS_END,
};

/* static */ bool MapObject::finishInit(JSContext* cx, HandleObject ctor,
                                        HandleObject proto) {
  Handle<NativeObject*> nativeProto = proto.as<NativeObject>();

  RootedValue entriesFn(cx);
  RootedId entriesId(cx, NameToId(cx->names().entries));
  if (!NativeGetProperty(cx, nativeProto, entriesId, &entriesFn)) {
    return false;
  }

  // 23.1.3.12 Map.prototype[@@iterator]()
  // The initial value of the @@iterator property is the same function object
  // as the initial value of the "entries" property.
  RootedId iteratorId(cx, PropertyKey::Symbol(cx->wellKnownSymbols().iterator));
  return NativeDefineDataProperty(cx, nativeProto, iteratorId, entriesFn, 0);
}

void MapObject::trace(JSTracer* trc, JSObject* obj) {
  MapObject* mapObj = &obj->as<MapObject>();
  Table(mapObj).trace(trc);
}

using NurseryKeysVector = GCVector<Value, 0, SystemAllocPolicy>;

template <typename TableObject>
static NurseryKeysVector* GetNurseryKeys(TableObject* t) {
  Value value = t->getReservedSlot(TableObject::NurseryKeysSlot);
  return reinterpret_cast<NurseryKeysVector*>(value.toPrivate());
}

template <typename TableObject>
static NurseryKeysVector* AllocNurseryKeys(TableObject* t) {
  MOZ_ASSERT(!GetNurseryKeys(t));
  auto keys = js_new<NurseryKeysVector>();
  if (!keys) {
    return nullptr;
  }

  t->setReservedSlot(TableObject::NurseryKeysSlot, PrivateValue(keys));
  return keys;
}

template <typename TableObject>
static void DeleteNurseryKeys(TableObject* t) {
  auto keys = GetNurseryKeys(t);
  MOZ_ASSERT(keys);
  js_delete(keys);
  t->setReservedSlot(TableObject::NurseryKeysSlot, PrivateValue(nullptr));
}

// A generic store buffer entry that traces all nursery keys for an ordered hash
// map or set.
template <typename ObjectT>
class js::OrderedHashTableRef : public gc::BufferableRef {
  ObjectT* object;

 public:
  explicit OrderedHashTableRef(ObjectT* obj) : object(obj) {}

  void trace(JSTracer* trc) override {
    MOZ_ASSERT(!IsInsideNursery(object));
    NurseryKeysVector* keys = GetNurseryKeys(object);
    MOZ_ASSERT(keys);

    keys->mutableEraseIf([&](Value& key) {
      MOZ_ASSERT(typename ObjectT::UnbarrieredTable(object).hash(key) ==
                 typename ObjectT::Table(object).hash(
                     *reinterpret_cast<const HashableValue*>(&key)));
      MOZ_ASSERT(IsInsideNursery(key.toGCThing()));

      auto result = typename ObjectT::UnbarrieredTable(object).rekeyOneEntry(
          key, [trc](const Value& prior) {
            Value key = prior;
            TraceManuallyBarrieredEdge(trc, &key, "ordered hash table key");
            return key;
          });

      if (result.isNothing()) {
        return true;  // Key removed.
      }

      key = result.value();
      return !IsInsideNursery(key.toGCThing());
    });

    if (!keys->empty()) {
      trc->runtime()->gc.storeBuffer().putGeneric(
          OrderedHashTableRef<ObjectT>(object));
      return;
    }

    DeleteNurseryKeys(object);
  }
};

template <typename ObjectT>
[[nodiscard]] inline static bool PostWriteBarrier(ObjectT* obj,
                                                  const Value& keyValue) {
  MOZ_ASSERT(!IsInsideNursery(obj));

  if (MOZ_LIKELY(!keyValue.hasObjectPayload() && !keyValue.isBigInt())) {
    MOZ_ASSERT_IF(keyValue.isGCThing(), !IsInsideNursery(keyValue.toGCThing()));
    return true;
  }

  if (!IsInsideNursery(keyValue.toGCThing())) {
    return true;
  }

  NurseryKeysVector* keys = GetNurseryKeys(obj);
  if (!keys) {
    keys = AllocNurseryKeys(obj);
    if (!keys) {
      return false;
    }

    keyValue.toGCThing()->storeBuffer()->putGeneric(
        OrderedHashTableRef<ObjectT>(obj));
  }

  return keys->append(keyValue);
}

bool MapObject::getKeysAndValuesInterleaved(
    HandleObject obj, JS::MutableHandle<GCVector<JS::Value>> entries) {
  MapObject* mapObj = &obj->as<MapObject>();
  auto appendEntry = [&entries](auto& entry) {
    return entries.append(entry.key.get()) && entries.append(entry.value);
  };
  return Table(mapObj).forEachEntry(appendEntry);
}

bool MapObject::set(JSContext* cx, HandleObject obj, HandleValue k,
                    HandleValue v) {
  MapObject* mapObject = &obj->as<MapObject>();
  Rooted<HashableValue> key(cx);
  if (!key.setValue(cx, k)) {
    return false;
  }

  return mapObject->setWithHashableKey(cx, key, v);
}

bool MapObject::setWithHashableKey(JSContext* cx, const HashableValue& key,
                                   const Value& value) {
  bool needsPostBarriers = isTenured();
  if (needsPostBarriers) {
    // Use the Table representation which has post barriers.
    if (!PostWriteBarrier(this, key)) {
      ReportOutOfMemory(cx);
      return false;
    }
    if (!Table(this).put(cx, key, value)) {
      return false;
    }
  } else {
    // Use the PreBarrieredTable representation which does not.
    if (!PreBarrieredTable(this).put(cx, key, value)) {
      return false;
    }
  }

  return true;
}

MapObject* MapObject::create(JSContext* cx,
                             HandleObject proto /* = nullptr */) {
  AutoSetNewObjectMetadata metadata(cx);
  MapObject* mapObj = NewObjectWithClassProto<MapObject>(cx, proto);
  if (!mapObj) {
    return nullptr;
  }

  if (!UnbarrieredTable(mapObj).init(cx)) {
    return nullptr;
  }

  mapObj->initReservedSlot(NurseryKeysSlot, PrivateValue(nullptr));
  mapObj->initReservedSlot(RegisteredNurseryRangesSlot, BooleanValue(false));
  return mapObj;
}

size_t MapObject::sizeOfData(mozilla::MallocSizeOf mallocSizeOf) {
  size_t size = 0;
  size += Table(this).sizeOfExcludingObject(mallocSizeOf);
  if (NurseryKeysVector* nurseryKeys = GetNurseryKeys(this)) {
    size += nurseryKeys->sizeOfIncludingThis(mallocSizeOf);
  }
  return size;
}

void MapObject::finalize(JS::GCContext* gcx, JSObject* obj) {
  MapObject* mapObj = &obj->as<MapObject>();
  MOZ_ASSERT(!IsInsideNursery(mapObj));
  MOZ_ASSERT(!UnbarrieredTable(mapObj).hasNurseryRanges());

#ifdef DEBUG
  // If we're finalizing a tenured map then it cannot contain nursery things,
  // because we evicted the nursery at the start of collection and writing a
  // nursery thing into the table would require it to be live, which means it
  // would have been marked.
  UnbarrieredTable(mapObj).forEachEntryUpTo(1000, [](auto& entry) {
    Value key = entry.key;
    MOZ_ASSERT_IF(key.isGCThing(), !IsInsideNursery(key.toGCThing()));
    Value value = entry.value;
    MOZ_ASSERT_IF(value.isGCThing(), !IsInsideNursery(value.toGCThing()));
  });
#endif

  // Finalized tenured maps do not contain nursery GC things, so do not require
  // post barriers. Pre barriers are not required for finalization.
  UnbarrieredTable(mapObj).destroy(gcx);
}

size_t MapObject::objectMoved(JSObject* obj, JSObject* old) {
  auto* mapObj = &obj->as<MapObject>();

  Table(mapObj).updateRangesAfterMove(&old->as<MapObject>());

  if (IsInsideNursery(old)) {
    Nursery& nursery = mapObj->runtimeFromMainThread()->gc.nursery();
    Table(mapObj).maybeMoveBufferOnPromotion(nursery);
  }

  return 0;
}

void MapObject::clearNurseryRangesBeforeMinorGC() {
  Table(this).destroyNurseryRanges();
}

/* static */
MapObject* MapObject::sweepAfterMinorGC(JS::GCContext* gcx, MapObject* mapobj) {
  Nursery& nursery = gcx->runtime()->gc.nursery();
  bool wasInCollectedRegion = nursery.inCollectedRegion(mapobj);
  if (wasInCollectedRegion && !IsForwarded(mapobj)) {
    // This MapObject is dead.
    return nullptr;
  }

  mapobj = MaybeForwarded(mapobj);

  // Keep |mapobj| registered with the nursery if it still has nursery ranges.
  bool hasNurseryRanges = Table(mapobj).hasNurseryRanges();
  SetRegisteredNurseryRanges(mapobj, hasNurseryRanges);
  return hasNurseryRanges ? mapobj : nullptr;
}

bool MapObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSConstructorProfilerEntry pseudoFrame(cx, "Map");
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Map")) {
    return false;
  }

  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_Map, &proto)) {
    return false;
  }

  Rooted<MapObject*> obj(cx, MapObject::create(cx, proto));
  if (!obj) {
    return false;
  }

  if (!args.get(0).isNullOrUndefined()) {
    FixedInvokeArgs<1> args2(cx);
    args2[0].set(args[0]);

    RootedValue thisv(cx, ObjectValue(*obj));
    if (!CallSelfHostedFunction(cx, cx->names().MapConstructorInit, thisv,
                                args2, args2.rval())) {
      return false;
    }
  }

  args.rval().setObject(*obj);
  return true;
}

bool MapObject::is(HandleValue v) {
  return v.isObject() && v.toObject().hasClass(&class_);
}

bool MapObject::is(HandleObject o) { return o->hasClass(&class_); }

#define ARG0_KEY(cx, args, key)  \
  Rooted<HashableValue> key(cx); \
  if (args.length() > 0 && !key.setValue(cx, args[0])) return false

uint32_t MapObject::size(JSContext* cx, HandleObject obj) {
  MapObject* mapObj = &obj->as<MapObject>();
  static_assert(sizeof(Table(mapObj).count()) <= sizeof(uint32_t),
                "map count must be precisely representable as a JS number");
  return Table(mapObj).count();
}

bool MapObject::size_impl(JSContext* cx, const CallArgs& args) {
  RootedObject obj(cx, &args.thisv().toObject());
  args.rval().setNumber(size(cx, obj));
  return true;
}

bool MapObject::size(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Map.prototype", "size");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<MapObject::is, MapObject::size_impl>(cx, args);
}

bool MapObject::get(JSContext* cx, HandleObject obj, HandleValue key,
                    MutableHandleValue rval) {
  Rooted<HashableValue> k(cx);

  if (!k.setValue(cx, key)) {
    return false;
  }

  if (const Table::Entry* p = Table(&obj->as<MapObject>()).get(k)) {
    rval.set(p->value);
  } else {
    rval.setUndefined();
  }

  return true;
}

bool MapObject::get_impl(JSContext* cx, const CallArgs& args) {
  RootedObject obj(cx, &args.thisv().toObject());
  return get(cx, obj, args.get(0), args.rval());
}

bool MapObject::get(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Map.prototype", "get");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<MapObject::is, MapObject::get_impl>(cx, args);
}

bool MapObject::has(JSContext* cx, HandleObject obj, HandleValue key,
                    bool* rval) {
  Rooted<HashableValue> k(cx);

  if (!k.setValue(cx, key)) {
    return false;
  }

  *rval = Table(&obj->as<MapObject>()).has(k);
  return true;
}

bool MapObject::has_impl(JSContext* cx, const CallArgs& args) {
  bool found;
  RootedObject obj(cx, &args.thisv().toObject());
  if (has(cx, obj, args.get(0), &found)) {
    args.rval().setBoolean(found);
    return true;
  }
  return false;
}

bool MapObject::has(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Map.prototype", "has");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<MapObject::is, MapObject::has_impl>(cx, args);
}

bool MapObject::set_impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(MapObject::is(args.thisv()));

  MapObject* obj = &args.thisv().toObject().as<MapObject>();
  ARG0_KEY(cx, args, key);
  if (!obj->setWithHashableKey(cx, key, args.get(1))) {
    return false;
  }

  args.rval().set(args.thisv());
  return true;
}

bool MapObject::set(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Map.prototype", "set");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<MapObject::is, MapObject::set_impl>(cx, args);
}

bool MapObject::delete_(JSContext* cx, HandleObject obj, HandleValue key,
                        bool* rval) {
  MapObject* mapObject = &obj->as<MapObject>();
  Rooted<HashableValue> k(cx);

  if (!k.setValue(cx, key)) {
    return false;
  }

  if (mapObject->isTenured()) {
    *rval = Table(mapObject).remove(cx, k);
  } else {
    *rval = PreBarrieredTable(mapObject).remove(cx, k);
  }
  return true;
}

bool MapObject::delete_impl(JSContext* cx, const CallArgs& args) {
  // MapObject::trace does not trace deleted entries. Incremental GC therefore
  // requires that no HeapPtr<Value> objects pointing to heap values be left
  // alive in the hash table.
  //
  // OrderedHashMapImpl::remove() doesn't destroy the removed entry. It merely
  // calls OrderedHashMapImpl::MapOps::makeEmpty. But that is sufficient,
  // because makeEmpty clears the value by doing e->value = Value(), and in the
  // case of Table, Value() means HeapPtr<Value>(), which is the same as
  // HeapPtr<Value>(UndefinedValue()).
  MOZ_ASSERT(MapObject::is(args.thisv()));
  RootedObject obj(cx, &args.thisv().toObject());

  bool found;
  if (!delete_(cx, obj, args.get(0), &found)) {
    return false;
  }

  args.rval().setBoolean(found);
  return true;
}

bool MapObject::delete_(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Map.prototype", "delete");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<MapObject::is, MapObject::delete_impl>(cx, args);
}

bool MapObject::iterator(JSContext* cx, IteratorKind kind, HandleObject obj,
                         MutableHandleValue iter) {
  Rooted<JSObject*> iterobj(cx, MapIteratorObject::create(cx, obj, kind));
  if (!iterobj) {
    return false;
  }
  iter.setObject(*iterobj);
  return true;
}

bool MapObject::iterator_impl(JSContext* cx, const CallArgs& args,
                              IteratorKind kind) {
  RootedObject obj(cx, &args.thisv().toObject());
  return iterator(cx, kind, obj, args.rval());
}

bool MapObject::keys_impl(JSContext* cx, const CallArgs& args) {
  return iterator_impl(cx, args, Keys);
}

bool MapObject::keys(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Map.prototype", "keys");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod(cx, is, keys_impl, args);
}

bool MapObject::values_impl(JSContext* cx, const CallArgs& args) {
  return iterator_impl(cx, args, Values);
}

bool MapObject::values(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Map.prototype", "values");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod(cx, is, values_impl, args);
}

bool MapObject::entries_impl(JSContext* cx, const CallArgs& args) {
  return iterator_impl(cx, args, Entries);
}

bool MapObject::entries(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Map.prototype", "entries");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod(cx, is, entries_impl, args);
}

bool MapObject::clear_impl(JSContext* cx, const CallArgs& args) {
  RootedObject obj(cx, &args.thisv().toObject());
  args.rval().setUndefined();
  return clear(cx, obj);
}

bool MapObject::clear(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Map.prototype", "clear");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod(cx, is, clear_impl, args);
}

bool MapObject::clear(JSContext* cx, HandleObject obj) {
  MapObject* mapObject = &obj->as<MapObject>();
  if (mapObject->isTenured()) {
    Table(mapObject).clear(cx);
  } else {
    PreBarrieredTable(mapObject).clear(cx);
  }
  return true;
}

/*** SetIterator ************************************************************/

static const JSClassOps SetIteratorObjectClassOps = {
    nullptr,                      // addProperty
    nullptr,                      // delProperty
    nullptr,                      // enumerate
    nullptr,                      // newEnumerate
    nullptr,                      // resolve
    nullptr,                      // mayResolve
    SetIteratorObject::finalize,  // finalize
    nullptr,                      // call
    nullptr,                      // construct
    nullptr,                      // trace
};

static const ClassExtension SetIteratorObjectClassExtension = {
    SetIteratorObject::objectMoved,  // objectMovedOp
};

const JSClass SetIteratorObject::class_ = {
    "Set Iterator",
    JSCLASS_HAS_RESERVED_SLOTS(SetIteratorObject::SlotCount) |
        JSCLASS_FOREGROUND_FINALIZE | JSCLASS_SKIP_NURSERY_FINALIZE,
    &SetIteratorObjectClassOps,
    JS_NULL_CLASS_SPEC,
    &SetIteratorObjectClassExtension,
};

const JSFunctionSpec SetIteratorObject::methods[] = {
    JS_SELF_HOSTED_FN("next", "SetIteratorNext", 0, 0),
    JS_FS_END,
};

static inline SetObject::Table::Range* SetIteratorObjectRange(
    NativeObject* obj) {
  MOZ_ASSERT(obj->is<SetIteratorObject>());
  return obj->maybePtrFromReservedSlot<SetObject::Table::Range>(
      SetIteratorObject::RangeSlot);
}

inline SetObject::IteratorKind SetIteratorObject::kind() const {
  int32_t i = getReservedSlot(KindSlot).toInt32();
  MOZ_ASSERT(i == SetObject::Values || i == SetObject::Entries);
  return SetObject::IteratorKind(i);
}

/* static */
bool GlobalObject::initSetIteratorProto(JSContext* cx,
                                        Handle<GlobalObject*> global) {
  Rooted<JSObject*> base(
      cx, GlobalObject::getOrCreateIteratorPrototype(cx, global));
  if (!base) {
    return false;
  }
  Rooted<PlainObject*> proto(
      cx, GlobalObject::createBlankPrototypeInheriting<PlainObject>(cx, base));
  if (!proto) {
    return false;
  }
  if (!JS_DefineFunctions(cx, proto, SetIteratorObject::methods) ||
      !DefineToStringTag(cx, proto, cx->names().Set_Iterator_)) {
    return false;
  }
  global->initBuiltinProto(ProtoKind::SetIteratorProto, proto);
  return true;
}

SetIteratorObject* SetIteratorObject::create(JSContext* cx, HandleObject obj,
                                             SetObject::IteratorKind kind) {
  MOZ_ASSERT(kind != SetObject::Keys);

  Handle<SetObject*> setobj(obj.as<SetObject>());
  Rooted<GlobalObject*> global(cx, &setobj->global());
  Rooted<JSObject*> proto(
      cx, GlobalObject::getOrCreateSetIteratorPrototype(cx, global));
  if (!proto) {
    return nullptr;
  }

  SetIteratorObject* iterobj =
      NewObjectWithGivenProto<SetIteratorObject>(cx, proto);
  if (!iterobj) {
    return nullptr;
  }

  iterobj->init(setobj, kind);

  constexpr size_t BufferSize =
      RoundUp(sizeof(SetObject::Table::Range), gc::CellAlignBytes);

  Nursery& nursery = cx->nursery();
  void* buffer =
      nursery.allocateBufferSameLocation(iterobj, BufferSize, js::MallocArena);
  if (!buffer) {
    // Retry with |iterobj| and |buffer| forcibly tenured.
    iterobj = NewTenuredObjectWithGivenProto<SetIteratorObject>(cx, proto);
    if (!iterobj) {
      return nullptr;
    }

    iterobj->init(setobj, kind);

    buffer = nursery.allocateBufferSameLocation(iterobj, BufferSize,
                                                js::MallocArena);
    if (!buffer) {
      ReportOutOfMemory(cx);
      return nullptr;
    }
  }

  bool insideNursery = IsInsideNursery(iterobj);
  MOZ_ASSERT(insideNursery == nursery.isInside(buffer));

  if (insideNursery && !HasRegisteredNurseryRanges(setobj.get())) {
    if (!cx->nursery().addSetWithNurseryRanges(setobj)) {
      ReportOutOfMemory(cx);
      return nullptr;
    }
    SetRegisteredNurseryRanges(setobj.get(), true);
  }

  auto range = SetObject::Table(setobj).createRange(buffer, insideNursery);
  iterobj->setReservedSlot(RangeSlot, PrivateValue(range));

  return iterobj;
}

void SetIteratorObject::finalize(JS::GCContext* gcx, JSObject* obj) {
  MOZ_ASSERT(gcx->onMainThread());
  MOZ_ASSERT(!IsInsideNursery(obj));

  auto range = SetIteratorObjectRange(&obj->as<NativeObject>());
  MOZ_ASSERT(!gcx->runtime()->gc.nursery().isInside(range));

  // Bug 1560019: Malloc memory associated with SetIteratorObjects is not
  // currently tracked.
  gcx->deleteUntracked(range);
}

size_t SetIteratorObject::objectMoved(JSObject* obj, JSObject* old) {
  if (!IsInsideNursery(old)) {
    return 0;
  }

  SetIteratorObject* iter = &obj->as<SetIteratorObject>();
  SetObject::Table::Range* range = SetIteratorObjectRange(iter);
  if (!range) {
    return 0;
  }

  Nursery& nursery = iter->runtimeFromMainThread()->gc.nursery();
  if (!nursery.isInside(range)) {
    nursery.removeMallocedBufferDuringMinorGC(range);
  }

  size_t size = RoundUp(sizeof(SetObject::Table::Range), gc::CellAlignBytes);
  ;
  AutoEnterOOMUnsafeRegion oomUnsafe;
  void* buffer = nursery.allocateBufferSameLocation(obj, size, js::MallocArena);
  if (!buffer) {
    oomUnsafe.crash("SetIteratorObject::objectMoved");
  }

  SetObject* setObj = iter->target();

  bool iteratorIsInNursery = IsInsideNursery(obj);
  MOZ_ASSERT(iteratorIsInNursery == nursery.isInside(buffer));
  auto* newRange =
      new (buffer) SetObject::Table::Range(setObj, *range, iteratorIsInNursery);
  range->~Range();
  iter->setReservedSlot(SetIteratorObject::RangeSlot, PrivateValue(newRange));

  return size;
}

SetObject* SetIteratorObject::target() const {
  Value value = getFixedSlot(TargetSlot);
  if (value.isUndefined()) {
    return nullptr;
  }

  return &MaybeForwarded(&value.toObject())->as<SetObject>();
}

bool SetIteratorObject::next(SetIteratorObject* setIterator,
                             ArrayObject* resultObj) {
  // IC code calls this directly.
  AutoUnsafeCallWithABI unsafe;

  // Check invariants for inlined _GetNextSetEntryForIterator.

  // The array should be tenured, so that post-barrier can be done simply.
  MOZ_ASSERT(resultObj->isTenured());

  // The array elements should be fixed.
  MOZ_ASSERT(resultObj->hasFixedElements());
  MOZ_ASSERT(resultObj->getDenseInitializedLength() == 1);
  MOZ_ASSERT(resultObj->getDenseCapacity() >= 1);

  SetObject::Table::Range* range = SetIteratorObjectRange(setIterator);
  if (!range) {
    return true;
  }

  SetObject* setObj = setIterator->target();

  if (range->empty(setObj)) {
    DestroyRange<SetObject::Table::Range>(setIterator, range);
    setIterator->setReservedSlot(RangeSlot, PrivateValue(nullptr));
    return true;
  }

  resultObj->setDenseElement(0, range->front(setObj).get());
  range->popFront(setObj);
  return false;
}

/* static */
JSObject* SetIteratorObject::createResult(JSContext* cx) {
  Rooted<ArrayObject*> resultObj(
      cx, NewDenseFullyAllocatedArray(cx, 1, TenuredObject));
  if (!resultObj) {
    return nullptr;
  }

  resultObj->setDenseInitializedLength(1);
  resultObj->initDenseElement(0, NullValue());

  return resultObj;
}

/*** Set ********************************************************************/

const JSClassOps SetObject::classOps_ = {
    nullptr,   // addProperty
    nullptr,   // delProperty
    nullptr,   // enumerate
    nullptr,   // newEnumerate
    nullptr,   // resolve
    nullptr,   // mayResolve
    finalize,  // finalize
    nullptr,   // call
    nullptr,   // construct
    trace,     // trace
};

const ClassSpec SetObject::classSpec_ = {
    GenericCreateConstructor<SetObject::construct, 0, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<SetObject>,
    nullptr,
    SetObject::staticProperties,
    SetObject::methods,
    SetObject::properties,
    SetObject::finishInit,
};

const ClassExtension SetObject::classExtension_ = {
    SetObject::objectMoved,  // objectMovedOp
};

const JSClass SetObject::class_ = {
    "Set",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(SetObject::SlotCount) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_Set) | JSCLASS_BACKGROUND_FINALIZE |
        JSCLASS_SKIP_NURSERY_FINALIZE,
    &SetObject::classOps_, &SetObject::classSpec_, &SetObject::classExtension_};

const JSClass SetObject::protoClass_ = {
    "Set.prototype",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Set),
    JS_NULL_CLASS_OPS,
    &SetObject::classSpec_,
};

const JSPropertySpec SetObject::properties[] = {
    JS_PSG("size", size, 0),
    JS_STRING_SYM_PS(toStringTag, "Set", JSPROP_READONLY),
    JS_PS_END,
};

const JSFunctionSpec SetObject::methods[] = {
    JS_INLINABLE_FN("has", has, 1, 0, SetHas),
    JS_FN("add", add, 1, 0),
    JS_FN("delete", delete_, 1, 0),
    JS_FN("entries", entries, 0, 0),
    JS_FN("clear", clear, 0, 0),
    JS_SELF_HOSTED_FN("forEach", "SetForEach", 2, 0),
    JS_SELF_HOSTED_FN("union", "SetUnion", 1, 0),
    JS_SELF_HOSTED_FN("difference", "SetDifference", 1, 0),
    JS_SELF_HOSTED_FN("intersection", "SetIntersection", 1, 0),
    JS_SELF_HOSTED_FN("symmetricDifference", "SetSymmetricDifference", 1, 0),
    JS_SELF_HOSTED_FN("isSubsetOf", "SetIsSubsetOf", 1, 0),
    JS_SELF_HOSTED_FN("isSupersetOf", "SetIsSupersetOf", 1, 0),
    JS_SELF_HOSTED_FN("isDisjointFrom", "SetIsDisjointFrom", 1, 0),
    JS_FN("values", values, 0, 0),
    // @@iterator and |keys| re-defined in finishInit so that they have the
    // same identity as |values|.
    JS_FN("keys", values, 0, 0),
    JS_SYM_FN(iterator, values, 0, 0),
    JS_FS_END,
};
// clang-format on

const JSPropertySpec SetObject::staticProperties[] = {
    JS_SELF_HOSTED_SYM_GET(species, "$SetSpecies", 0),
    JS_PS_END,
};

/* static */ bool SetObject::finishInit(JSContext* cx, HandleObject ctor,
                                        HandleObject proto) {
  Handle<NativeObject*> nativeProto = proto.as<NativeObject>();

  RootedValue valuesFn(cx);
  RootedId valuesId(cx, NameToId(cx->names().values));
  if (!NativeGetProperty(cx, nativeProto, valuesId, &valuesFn)) {
    return false;
  }

  // 23.2.3.8 Set.prototype.keys()
  // The initial value of the "keys" property is the same function object
  // as the initial value of the "values" property.
  RootedId keysId(cx, NameToId(cx->names().keys));
  if (!NativeDefineDataProperty(cx, nativeProto, keysId, valuesFn, 0)) {
    return false;
  }

  // 23.2.3.11 Set.prototype[@@iterator]()
  // See above.
  RootedId iteratorId(cx, PropertyKey::Symbol(cx->wellKnownSymbols().iterator));
  return NativeDefineDataProperty(cx, nativeProto, iteratorId, valuesFn, 0);
}

bool SetObject::keys(JSContext* cx, HandleObject obj,
                     JS::MutableHandle<GCVector<JS::Value>> keys) {
  SetObject* setObj = &obj->as<SetObject>();
  auto appendEntry = [&keys](auto& entry) { return keys.append(entry.get()); };
  return Table(setObj).forEachEntry(appendEntry);
}

bool SetObject::add(JSContext* cx, HandleObject obj, HandleValue k) {
  Rooted<HashableValue> key(cx);
  if (!key.setValue(cx, k)) {
    return false;
  }

  SetObject* setObj = &obj->as<SetObject>();
  return setObj->addHashableValue(cx, key);
}

bool SetObject::addHashableValue(JSContext* cx, const HashableValue& value) {
  bool needsPostBarriers = isTenured();
  if (needsPostBarriers && !PostWriteBarrier(this, value)) {
    ReportOutOfMemory(cx);
    return false;
  }
  return Table(this).put(cx, value);
}

SetObject* SetObject::create(JSContext* cx,
                             HandleObject proto /* = nullptr */) {
  AutoSetNewObjectMetadata metadata(cx);
  SetObject* obj = NewObjectWithClassProto<SetObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  if (!UnbarrieredTable(obj).init(cx)) {
    return nullptr;
  }

  obj->initReservedSlot(NurseryKeysSlot, PrivateValue(nullptr));
  obj->initReservedSlot(RegisteredNurseryRangesSlot, BooleanValue(false));
  return obj;
}

void SetObject::trace(JSTracer* trc, JSObject* obj) {
  SetObject* setobj = static_cast<SetObject*>(obj);
  Table(setobj).trace(trc);
}

size_t SetObject::sizeOfData(mozilla::MallocSizeOf mallocSizeOf) {
  size_t size = 0;
  size += Table(this).sizeOfExcludingObject(mallocSizeOf);
  if (NurseryKeysVector* nurseryKeys = GetNurseryKeys(this)) {
    size += nurseryKeys->sizeOfIncludingThis(mallocSizeOf);
  }
  return size;
}

void SetObject::finalize(JS::GCContext* gcx, JSObject* obj) {
  SetObject* setObj = &obj->as<SetObject>();
  MOZ_ASSERT(!IsInsideNursery(setObj));
  MOZ_ASSERT(!UnbarrieredTable(setObj).hasNurseryRanges());

#ifdef DEBUG
  // If we're finalizing a tenured set then it cannot contain nursery things,
  // because we evicted the nursery at the start of collection and writing a
  // nursery thing into the set would require it to be live, which means it
  // would have been marked.
  UnbarrieredTable(setObj).forEachEntryUpTo(1000, [](auto& entry) {
    Value key = entry;
    MOZ_ASSERT_IF(key.isGCThing(), !IsInsideNursery(key.toGCThing()));
  });
#endif

  // Finalized tenured sets do not contain nursery GC things, so do not require
  // post barriers. Pre barriers are not required for finalization.
  UnbarrieredTable(setObj).destroy(gcx);
}

size_t SetObject::objectMoved(JSObject* obj, JSObject* old) {
  auto* setObj = &obj->as<SetObject>();

  Table(setObj).updateRangesAfterMove(&old->as<SetObject>());

  if (IsInsideNursery(old)) {
    Nursery& nursery = setObj->runtimeFromMainThread()->gc.nursery();
    Table(setObj).maybeMoveBufferOnPromotion(nursery);
  }

  return 0;
}

void SetObject::clearNurseryRangesBeforeMinorGC() {
  Table(this).destroyNurseryRanges();
}

/* static */
SetObject* SetObject::sweepAfterMinorGC(JS::GCContext* gcx, SetObject* setobj) {
  Nursery& nursery = gcx->runtime()->gc.nursery();
  bool wasInCollectedRegion = nursery.inCollectedRegion(setobj);
  if (wasInCollectedRegion && !IsForwarded(setobj)) {
    // This SetObject is dead.
    return nullptr;
  }

  setobj = MaybeForwarded(setobj);

  // Keep |setobj| registered with the nursery if it still has nursery ranges.
  bool hasNurseryRanges = Table(setobj).hasNurseryRanges();
  SetRegisteredNurseryRanges(setobj, hasNurseryRanges);
  return hasNurseryRanges ? setobj : nullptr;
}

bool SetObject::isBuiltinAdd(HandleValue add) {
  return IsNativeFunction(add, SetObject::add);
}

bool SetObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSConstructorProfilerEntry pseudoFrame(cx, "Set");
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Set")) {
    return false;
  }

  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_Set, &proto)) {
    return false;
  }

  Rooted<SetObject*> obj(cx, SetObject::create(cx, proto));
  if (!obj) {
    return false;
  }

  if (!args.get(0).isNullOrUndefined()) {
    RootedValue iterable(cx, args[0]);
    bool optimized = false;
    if (!IsOptimizableInitForSet<GlobalObject::getOrCreateSetPrototype,
                                 isBuiltinAdd>(cx, obj, iterable, &optimized)) {
      return false;
    }

    if (optimized) {
      RootedValue keyVal(cx);
      Rooted<HashableValue> key(cx);
      Rooted<ArrayObject*> array(cx, &iterable.toObject().as<ArrayObject>());
      for (uint32_t index = 0; index < array->getDenseInitializedLength();
           ++index) {
        keyVal.set(array->getDenseElement(index));
        MOZ_ASSERT(!keyVal.isMagic(JS_ELEMENTS_HOLE));
        if (!key.setValue(cx, keyVal)) {
          return false;
        }
        if (!obj->addHashableValue(cx, key)) {
          return false;
        }
      }
    } else {
      FixedInvokeArgs<1> args2(cx);
      args2[0].set(args[0]);

      RootedValue thisv(cx, ObjectValue(*obj));
      if (!CallSelfHostedFunction(cx, cx->names().SetConstructorInit, thisv,
                                  args2, args2.rval())) {
        return false;
      }
    }
  }

  args.rval().setObject(*obj);
  return true;
}

bool SetObject::is(HandleValue v) {
  return v.isObject() && v.toObject().hasClass(&class_);
}

bool SetObject::is(HandleObject o) { return o->hasClass(&class_); }

uint32_t SetObject::size(JSContext* cx, HandleObject obj) {
  MOZ_ASSERT(SetObject::is(obj));
  SetObject* setObj = &obj->as<SetObject>();
  static_assert(sizeof(Table(setObj).count()) <= sizeof(uint32_t),
                "set count must be precisely representable as a JS number");
  return Table(setObj).count();
}

bool SetObject::size_impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  SetObject* setObj = &args.thisv().toObject().as<SetObject>();

  static_assert(sizeof(Table(setObj).count()) <= sizeof(uint32_t),
                "set count must be precisely representable as a JS number");
  args.rval().setNumber(Table(setObj).count());
  return true;
}

bool SetObject::size(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Set.prototype", "size");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<SetObject::is, SetObject::size_impl>(cx, args);
}

bool SetObject::has_impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  ARG0_KEY(cx, args, key);
  SetObject* setObj = &args.thisv().toObject().as<SetObject>();
  args.rval().setBoolean(Table(setObj).has(key));
  return true;
}

bool SetObject::has(JSContext* cx, HandleObject obj, HandleValue key,
                    bool* rval) {
  MOZ_ASSERT(SetObject::is(obj));

  Rooted<HashableValue> k(cx);

  if (!k.setValue(cx, key)) {
    return false;
  }

  SetObject* setObj = &obj->as<SetObject>();
  *rval = Table(setObj).has(k);
  return true;
}

bool SetObject::has(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Set.prototype", "has");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<SetObject::is, SetObject::has_impl>(cx, args);
}

bool SetObject::add_impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));
  ARG0_KEY(cx, args, key);
  SetObject* setObj = &args.thisv().toObject().as<SetObject>();
  if (!setObj->addHashableValue(cx, key)) {
    return false;
  }
  args.rval().set(args.thisv());
  return true;
}

bool SetObject::add(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Set.prototype", "add");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<SetObject::is, SetObject::add_impl>(cx, args);
}

bool SetObject::delete_(JSContext* cx, HandleObject obj, HandleValue key,
                        bool* rval) {
  MOZ_ASSERT(SetObject::is(obj));

  Rooted<HashableValue> k(cx);

  if (!k.setValue(cx, key)) {
    return false;
  }

  SetObject* setObj = &obj->as<SetObject>();
  *rval = Table(setObj).remove(cx, k);
  return true;
}

bool SetObject::delete_impl(JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(is(args.thisv()));

  ARG0_KEY(cx, args, key);

  SetObject* setObj = &args.thisv().toObject().as<SetObject>();

  bool found = Table(setObj).remove(cx, key);
  args.rval().setBoolean(found);
  return true;
}

bool SetObject::delete_(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Set.prototype", "delete");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<SetObject::is, SetObject::delete_impl>(cx, args);
}

bool SetObject::iterator(JSContext* cx, IteratorKind kind, HandleObject obj,
                         MutableHandleValue iter) {
  MOZ_ASSERT(SetObject::is(obj));
  Rooted<JSObject*> iterobj(cx, SetIteratorObject::create(cx, obj, kind));
  if (!iterobj) {
    return false;
  }
  iter.setObject(*iterobj);
  return true;
}

bool SetObject::iterator_impl(JSContext* cx, const CallArgs& args,
                              IteratorKind kind) {
  Rooted<SetObject*> setobj(cx, &args.thisv().toObject().as<SetObject>());
  Rooted<JSObject*> iterobj(cx, SetIteratorObject::create(cx, setobj, kind));
  if (!iterobj) {
    return false;
  }
  args.rval().setObject(*iterobj);
  return true;
}

bool SetObject::values_impl(JSContext* cx, const CallArgs& args) {
  return iterator_impl(cx, args, Values);
}

bool SetObject::values(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Set.prototype", "values");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod(cx, is, values_impl, args);
}

bool SetObject::entries_impl(JSContext* cx, const CallArgs& args) {
  return iterator_impl(cx, args, Entries);
}

bool SetObject::entries(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Set.prototype", "entries");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod(cx, is, entries_impl, args);
}

bool SetObject::clear(JSContext* cx, HandleObject obj) {
  MOZ_ASSERT(SetObject::is(obj));
  SetObject* setObj = &obj->as<SetObject>();
  Table(setObj).clear(cx);
  return true;
}

bool SetObject::clear_impl(JSContext* cx, const CallArgs& args) {
  SetObject* setObj = &args.thisv().toObject().as<SetObject>();
  Table(setObj).clear(cx);
  args.rval().setUndefined();
  return true;
}

bool SetObject::clear(JSContext* cx, unsigned argc, Value* vp) {
  AutoJSMethodProfilerEntry pseudoFrame(cx, "Set.prototype", "clear");
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod(cx, is, clear_impl, args);
}

bool SetObject::copy(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);
  MOZ_ASSERT(SetObject::is(args[0]));

  auto* result = SetObject::create(cx);
  if (!result) {
    return false;
  }

  auto* from = &args[0].toObject().as<SetObject>();

  auto addToResult = [cx, result](auto& entry) {
    return result->addHashableValue(cx, entry);
  };
  if (!Table(from).forEachEntry(addToResult)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/*** JS static utility functions ********************************************/

static bool forEach(const char* funcName, JSContext* cx, HandleObject obj,
                    HandleValue callbackFn, HandleValue thisArg) {
  CHECK_THREAD(cx);

  RootedId forEachId(cx, NameToId(cx->names().forEach));
  RootedFunction forEachFunc(
      cx, JS::GetSelfHostedFunction(cx, funcName, forEachId, 2));
  if (!forEachFunc) {
    return false;
  }

  RootedValue fval(cx, ObjectValue(*forEachFunc));
  return Call(cx, fval, obj, callbackFn, thisArg, &fval);
}

// Handles Clear/Size for public jsapi map/set access
template <typename RetT>
RetT CallObjFunc(RetT (*ObjFunc)(JSContext*, HandleObject), JSContext* cx,
                 HandleObject obj) {
  CHECK_THREAD(cx);
  cx->check(obj);

  // Always unwrap, in case this is an xray or cross-compartment wrapper.
  RootedObject unwrappedObj(cx);
  unwrappedObj = UncheckedUnwrap(obj);

  // Enter the realm of the backing object before calling functions on
  // it.
  JSAutoRealm ar(cx, unwrappedObj);
  return ObjFunc(cx, unwrappedObj);
}

// Handles Has/Delete for public jsapi map/set access
bool CallObjFunc(bool (*ObjFunc)(JSContext* cx, HandleObject obj,
                                 HandleValue key, bool* rval),
                 JSContext* cx, HandleObject obj, HandleValue key, bool* rval) {
  CHECK_THREAD(cx);
  cx->check(obj, key);

  // Always unwrap, in case this is an xray or cross-compartment wrapper.
  RootedObject unwrappedObj(cx);
  unwrappedObj = UncheckedUnwrap(obj);
  JSAutoRealm ar(cx, unwrappedObj);

  // If we're working with a wrapped map/set, rewrap the key into the
  // compartment of the unwrapped map/set.
  RootedValue wrappedKey(cx, key);
  if (obj != unwrappedObj) {
    if (!JS_WrapValue(cx, &wrappedKey)) {
      return false;
    }
  }
  return ObjFunc(cx, unwrappedObj, wrappedKey, rval);
}

// Handles iterator generation for public jsapi map/set access
template <typename Iter>
bool CallObjFunc(bool (*ObjFunc)(JSContext* cx, Iter kind, HandleObject obj,
                                 MutableHandleValue iter),
                 JSContext* cx, Iter iterType, HandleObject obj,
                 MutableHandleValue rval) {
  CHECK_THREAD(cx);
  cx->check(obj);

  // Always unwrap, in case this is an xray or cross-compartment wrapper.
  RootedObject unwrappedObj(cx);
  unwrappedObj = UncheckedUnwrap(obj);
  {
    // Retrieve the iterator while in the unwrapped map/set's compartment,
    // otherwise we'll crash on a compartment assert.
    JSAutoRealm ar(cx, unwrappedObj);
    if (!ObjFunc(cx, iterType, unwrappedObj, rval)) {
      return false;
    }
  }

  // If the caller is in a different compartment than the map/set, rewrap the
  // iterator object into the caller's compartment.
  if (obj != unwrappedObj) {
    if (!JS_WrapValue(cx, rval)) {
      return false;
    }
  }
  return true;
}

/*** JS public APIs *********************************************************/

JS_PUBLIC_API JSObject* JS::NewMapObject(JSContext* cx) {
  return MapObject::create(cx);
}

JS_PUBLIC_API uint32_t JS::MapSize(JSContext* cx, HandleObject obj) {
  return CallObjFunc<uint32_t>(&MapObject::size, cx, obj);
}

JS_PUBLIC_API bool JS::MapGet(JSContext* cx, HandleObject obj, HandleValue key,
                              MutableHandleValue rval) {
  CHECK_THREAD(cx);
  cx->check(obj, key, rval);

  // Unwrap the object, and enter its realm. If object isn't wrapped,
  // this is essentially a noop.
  RootedObject unwrappedObj(cx);
  unwrappedObj = UncheckedUnwrap(obj);
  {
    JSAutoRealm ar(cx, unwrappedObj);
    RootedValue wrappedKey(cx, key);

    // If we passed in a wrapper, wrap our key into its compartment now.
    if (obj != unwrappedObj) {
      if (!JS_WrapValue(cx, &wrappedKey)) {
        return false;
      }
    }
    if (!MapObject::get(cx, unwrappedObj, wrappedKey, rval)) {
      return false;
    }
  }

  // If we passed in a wrapper, wrap our return value on the way out.
  if (obj != unwrappedObj) {
    if (!JS_WrapValue(cx, rval)) {
      return false;
    }
  }
  return true;
}

JS_PUBLIC_API bool JS::MapSet(JSContext* cx, HandleObject obj, HandleValue key,
                              HandleValue val) {
  CHECK_THREAD(cx);
  cx->check(obj, key, val);

  // Unwrap the object, and enter its compartment. If object isn't wrapped,
  // this is essentially a noop.
  RootedObject unwrappedObj(cx);
  unwrappedObj = UncheckedUnwrap(obj);
  {
    JSAutoRealm ar(cx, unwrappedObj);

    // If we passed in a wrapper, wrap both key and value before adding to
    // the map
    RootedValue wrappedKey(cx, key);
    RootedValue wrappedValue(cx, val);
    if (obj != unwrappedObj) {
      if (!JS_WrapValue(cx, &wrappedKey) || !JS_WrapValue(cx, &wrappedValue)) {
        return false;
      }
    }
    return MapObject::set(cx, unwrappedObj, wrappedKey, wrappedValue);
  }
}

JS_PUBLIC_API bool JS::MapHas(JSContext* cx, HandleObject obj, HandleValue key,
                              bool* rval) {
  return CallObjFunc(MapObject::has, cx, obj, key, rval);
}

JS_PUBLIC_API bool JS::MapDelete(JSContext* cx, HandleObject obj,
                                 HandleValue key, bool* rval) {
  return CallObjFunc(MapObject::delete_, cx, obj, key, rval);
}

JS_PUBLIC_API bool JS::MapClear(JSContext* cx, HandleObject obj) {
  return CallObjFunc(&MapObject::clear, cx, obj);
}

JS_PUBLIC_API bool JS::MapKeys(JSContext* cx, HandleObject obj,
                               MutableHandleValue rval) {
  return CallObjFunc(&MapObject::iterator, cx, MapObject::Keys, obj, rval);
}

JS_PUBLIC_API bool JS::MapValues(JSContext* cx, HandleObject obj,
                                 MutableHandleValue rval) {
  return CallObjFunc(&MapObject::iterator, cx, MapObject::Values, obj, rval);
}

JS_PUBLIC_API bool JS::MapEntries(JSContext* cx, HandleObject obj,
                                  MutableHandleValue rval) {
  return CallObjFunc(&MapObject::iterator, cx, MapObject::Entries, obj, rval);
}

JS_PUBLIC_API bool JS::MapForEach(JSContext* cx, HandleObject obj,
                                  HandleValue callbackFn, HandleValue thisVal) {
  return forEach("MapForEach", cx, obj, callbackFn, thisVal);
}

JS_PUBLIC_API JSObject* JS::NewSetObject(JSContext* cx) {
  return SetObject::create(cx);
}

JS_PUBLIC_API uint32_t JS::SetSize(JSContext* cx, HandleObject obj) {
  return CallObjFunc<uint32_t>(&SetObject::size, cx, obj);
}

JS_PUBLIC_API bool JS::SetAdd(JSContext* cx, HandleObject obj,
                              HandleValue key) {
  CHECK_THREAD(cx);
  cx->check(obj, key);

  // Unwrap the object, and enter its compartment. If object isn't wrapped,
  // this is essentially a noop.
  RootedObject unwrappedObj(cx);
  unwrappedObj = UncheckedUnwrap(obj);
  {
    JSAutoRealm ar(cx, unwrappedObj);

    // If we passed in a wrapper, wrap key before adding to the set
    RootedValue wrappedKey(cx, key);
    if (obj != unwrappedObj) {
      if (!JS_WrapValue(cx, &wrappedKey)) {
        return false;
      }
    }
    return SetObject::add(cx, unwrappedObj, wrappedKey);
  }
}

JS_PUBLIC_API bool JS::SetHas(JSContext* cx, HandleObject obj, HandleValue key,
                              bool* rval) {
  return CallObjFunc(SetObject::has, cx, obj, key, rval);
}

JS_PUBLIC_API bool JS::SetDelete(JSContext* cx, HandleObject obj,
                                 HandleValue key, bool* rval) {
  return CallObjFunc(SetObject::delete_, cx, obj, key, rval);
}

JS_PUBLIC_API bool JS::SetClear(JSContext* cx, HandleObject obj) {
  return CallObjFunc(&SetObject::clear, cx, obj);
}

JS_PUBLIC_API bool JS::SetKeys(JSContext* cx, HandleObject obj,
                               MutableHandleValue rval) {
  return SetValues(cx, obj, rval);
}

JS_PUBLIC_API bool JS::SetValues(JSContext* cx, HandleObject obj,
                                 MutableHandleValue rval) {
  return CallObjFunc(&SetObject::iterator, cx, SetObject::Values, obj, rval);
}

JS_PUBLIC_API bool JS::SetEntries(JSContext* cx, HandleObject obj,
                                  MutableHandleValue rval) {
  return CallObjFunc(&SetObject::iterator, cx, SetObject::Entries, obj, rval);
}

JS_PUBLIC_API bool JS::SetForEach(JSContext* cx, HandleObject obj,
                                  HandleValue callbackFn, HandleValue thisVal) {
  return forEach("SetForEach", cx, obj, callbackFn, thisVal);
}
