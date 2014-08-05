/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Barrier_h
#define gc_Barrier_h

#include "NamespaceImports.h"

#include "gc/Heap.h"
#ifdef JSGC_GENERATIONAL
# include "gc/StoreBuffer.h"
#endif
#include "js/HashTable.h"
#include "js/Id.h"
#include "js/RootingAPI.h"

/*
 * A write barrier is a mechanism used by incremental or generation GCs to
 * ensure that every value that needs to be marked is marked. In general, the
 * write barrier should be invoked whenever a write can cause the set of things
 * traced through by the GC to change. This includes:
 *   - writes to object properties
 *   - writes to array slots
 *   - writes to fields like JSObject::shape_ that we trace through
 *   - writes to fields in private data, like JSGenerator::obj
 *   - writes to non-markable fields like JSObject::private that point to
 *     markable data
 * The last category is the trickiest. Even though the private pointers does not
 * point to a GC thing, changing the private pointer may change the set of
 * objects that are traced by the GC. Therefore it needs a write barrier.
 *
 * Every barriered write should have the following form:
 *   <pre-barrier>
 *   obj->field = value; // do the actual write
 *   <post-barrier>
 * The pre-barrier is used for incremental GC and the post-barrier is for
 * generational GC.
 *
 *                               PRE-BARRIER
 *
 * To understand the pre-barrier, let's consider how incremental GC works. The
 * GC itself is divided into "slices". Between each slice, JS code is allowed to
 * run. Each slice should be short so that the user doesn't notice the
 * interruptions. In our GC, the structure of the slices is as follows:
 *
 * 1. ... JS work, which leads to a request to do GC ...
 * 2. [first GC slice, which performs all root marking and possibly more marking]
 * 3. ... more JS work is allowed to run ...
 * 4. [GC mark slice, which runs entirely in drainMarkStack]
 * 5. ... more JS work ...
 * 6. [GC mark slice, which runs entirely in drainMarkStack]
 * 7. ... more JS work ...
 * 8. [GC marking finishes; sweeping done non-incrementally; GC is done]
 * 9. ... JS continues uninterrupted now that GC is finishes ...
 *
 * Of course, there may be a different number of slices depending on how much
 * marking is to be done.
 *
 * The danger inherent in this scheme is that the JS code in steps 3, 5, and 7
 * might change the heap in a way that causes the GC to collect an object that
 * is actually reachable. The write barrier prevents this from happening. We use
 * a variant of incremental GC called "snapshot at the beginning." This approach
 * guarantees the invariant that if an object is reachable in step 2, then we
 * will mark it eventually. The name comes from the idea that we take a
 * theoretical "snapshot" of all reachable objects in step 2; all objects in
 * that snapshot should eventually be marked. (Note that the write barrier
 * verifier code takes an actual snapshot.)
 *
 * The basic correctness invariant of a snapshot-at-the-beginning collector is
 * that any object reachable at the end of the GC (step 9) must either:
 *   (1) have been reachable at the beginning (step 2) and thus in the snapshot
 *   (2) or must have been newly allocated, in steps 3, 5, or 7.
 * To deal with case (2), any objects allocated during an incremental GC are
 * automatically marked black.
 *
 * This strategy is actually somewhat conservative: if an object becomes
 * unreachable between steps 2 and 8, it would be safe to collect it. We won't,
 * mainly for simplicity. (Also, note that the snapshot is entirely
 * theoretical. We don't actually do anything special in step 2 that we wouldn't
 * do in a non-incremental GC.
 *
 * It's the pre-barrier's job to maintain the snapshot invariant. Consider the
 * write "obj->field = value". Let the prior value of obj->field be
 * value0. Since it's possible that value0 may have been what obj->field
 * contained in step 2, when the snapshot was taken, the barrier marks
 * value0. Note that it only does this if we're in the middle of an incremental
 * GC. Since this is rare, the cost of the write barrier is usually just an
 * extra branch.
 *
 * In practice, we implement the pre-barrier differently based on the type of
 * value0. E.g., see JSObject::writeBarrierPre, which is used if obj->field is
 * a JSObject*. It takes value0 as a parameter.
 *
 *                                POST-BARRIER
 *
 * For generational GC, we want to be able to quickly collect the nursery in a
 * minor collection.  Part of the way this is achieved is to only mark the
 * nursery itself; tenured things, which may form the majority of the heap, are
 * not traced through or marked.  This leads to the problem of what to do about
 * tenured objects that have pointers into the nursery: if such things are not
 * marked, they may be discarded while there are still live objects which
 * reference them. The solution is to maintain information about these pointers,
 * and mark their targets when we start a minor collection.
 *
 * The pointers can be thoughs of as edges in object graph, and the set of edges
 * from the tenured generation into the nursery is know as the remembered set.
 * Post barriers are used to track this remembered set.
 *
 * Whenever a slot which could contain such a pointer is written, we use a write
 * barrier to check if the edge created is in the remembered set, and if so we
 * insert it into the store buffer, which is the collector's representation of
 * the remembered set.  This means than when we come to do a minor collection we
 * can examine the contents of the store buffer and mark any edge targets that
 * are in the nursery.
 *
 *                            IMPLEMENTATION DETAILS
 *
 * Since it would be awkward to change every write to memory into a function
 * call, this file contains a bunch of C++ classes and templates that use
 * operator overloading to take care of barriers automatically. In many cases,
 * all that's necessary to make some field be barriered is to replace
 *     Type *field;
 * with
 *     HeapPtr<Type> field;
 * There are also special classes HeapValue and HeapId, which barrier js::Value
 * and jsid, respectively.
 *
 * One additional note: not all object writes need to be barriered. Writes to
 * newly allocated objects do not need a pre-barrier.  In these cases, we use
 * the "obj->field.init(value)" method instead of "obj->field = value". We use
 * the init naming idiom in many places to signify that a field is being
 * assigned for the first time.
 *
 * For each of pointers, Values and jsids this file implements four classes,
 * illustrated here for the pointer (Ptr) classes:
 *
 * BarrieredBase          abstract base class which provides common operations
 *  |  |  |
 *  |  | PreBarriered     provides pre-barriers only
 *  |  |
 *  | HeapPtr             provides pre- and post-barriers
 *  |
 * RelocatablePtr         provides pre- and post-barriers and is relocatable
 *
 * These classes are designed to be used by the internals of the JS engine.
 * Barriers designed to be used externally are provided in
 * js/public/RootingAPI.h.
 */

class JSAtom;
class JSFlatString;
class JSLinearString;

namespace js {

class PropertyName;

#ifdef DEBUG
bool
RuntimeFromMainThreadIsHeapMajorCollecting(JS::shadow::Zone *shadowZone);

// Barriers can't be triggered during backend Ion compilation, which may run on
// a helper thread.
bool
CurrentThreadIsIonCompiling();
#endif

bool
StringIsPermanentAtom(JSString *str);

namespace gc {

template <typename T>
void
MarkUnbarriered(JSTracer *trc, T **thingp, const char *name);

// Direct value access used by the write barriers and the jits.
void
MarkValueUnbarriered(JSTracer *trc, Value *v, const char *name);

// These three declarations are also present in gc/Marking.h, via the DeclMarker
// macro.  Not great, but hard to avoid.
void
MarkObjectUnbarriered(JSTracer *trc, JSObject **obj, const char *name);
void
MarkStringUnbarriered(JSTracer *trc, JSString **str, const char *name);
void
MarkSymbolUnbarriered(JSTracer *trc, JS::Symbol **sym, const char *name);

// Note that some subclasses (e.g. ObjectImpl) specialize some of these
// methods.
template <typename T>
class BarrieredCell : public gc::Cell
{
  public:
    MOZ_ALWAYS_INLINE JS::Zone *zone() const { return tenuredZone(); }
    MOZ_ALWAYS_INLINE JS::shadow::Zone *shadowZone() const { return JS::shadow::Zone::asShadowZone(zone()); }
    MOZ_ALWAYS_INLINE JS::Zone *zoneFromAnyThread() const { return tenuredZoneFromAnyThread(); }
    MOZ_ALWAYS_INLINE JS::shadow::Zone *shadowZoneFromAnyThread() const {
        return JS::shadow::Zone::asShadowZone(zoneFromAnyThread());
    }

    static MOZ_ALWAYS_INLINE void readBarrier(T *thing) {
#ifdef JSGC_INCREMENTAL
        JS_ASSERT(!CurrentThreadIsIonCompiling());
        JS::shadow::Zone *shadowZone = thing->shadowZoneFromAnyThread();
        if (shadowZone->needsIncrementalBarrier()) {
            MOZ_ASSERT(!RuntimeFromMainThreadIsHeapMajorCollecting(shadowZone));
            T *tmp = thing;
            js::gc::MarkUnbarriered<T>(shadowZone->barrierTracer(), &tmp, "read barrier");
            JS_ASSERT(tmp == thing);
        }
#endif
    }

    static MOZ_ALWAYS_INLINE bool needWriteBarrierPre(JS::Zone *zone) {
#ifdef JSGC_INCREMENTAL
        return JS::shadow::Zone::asShadowZone(zone)->needsIncrementalBarrier();
#else
        return false;
#endif
    }

    static MOZ_ALWAYS_INLINE bool isNullLike(T *thing) { return !thing; }

    static MOZ_ALWAYS_INLINE void writeBarrierPre(T *thing) {
#ifdef JSGC_INCREMENTAL
        JS_ASSERT(!CurrentThreadIsIonCompiling());
        if (isNullLike(thing) || !thing->shadowRuntimeFromAnyThread()->needsIncrementalBarrier())
            return;

        JS::shadow::Zone *shadowZone = thing->shadowZoneFromAnyThread();
        if (shadowZone->needsIncrementalBarrier()) {
            MOZ_ASSERT(!RuntimeFromMainThreadIsHeapMajorCollecting(shadowZone));
            T *tmp = thing;
            js::gc::MarkUnbarriered<T>(shadowZone->barrierTracer(), &tmp, "write barrier");
            JS_ASSERT(tmp == thing);
        }
#endif
    }

    static void writeBarrierPost(T *thing, void *cellp) {}
    static void writeBarrierPostRelocate(T *thing, void *cellp) {}
    static void writeBarrierPostRemove(T *thing, void *cellp) {}
};

} // namespace gc

// Note: the following Zone-getting functions must be equivalent to the zone()
// and shadowZone() functions implemented by the subclasses of BarrieredCell.

static inline JS::shadow::Zone *
ShadowZoneOfString(JSString *str)
{
    return JS::shadow::Zone::asShadowZone(reinterpret_cast<const js::gc::Cell *>(str)->tenuredZone());
}

JS::Zone *
ZoneOfObjectFromAnyThread(const JSObject &obj);

static inline JS::shadow::Zone *
ShadowZoneOfObjectFromAnyThread(JSObject *obj)
{
    return JS::shadow::Zone::asShadowZone(ZoneOfObjectFromAnyThread(*obj));
}

static inline JS::shadow::Zone *
ShadowZoneOfStringFromAnyThread(JSString *str)
{
    return JS::shadow::Zone::asShadowZone(
        reinterpret_cast<const js::gc::Cell *>(str)->tenuredZoneFromAnyThread());
}

static inline JS::shadow::Zone *
ShadowZoneOfSymbolFromAnyThread(JS::Symbol *sym)
{
    return JS::shadow::Zone::asShadowZone(
        reinterpret_cast<const js::gc::Cell *>(sym)->tenuredZoneFromAnyThread());
}

MOZ_ALWAYS_INLINE JS::Zone *
ZoneOfValueFromAnyThread(const JS::Value &value)
{
    JS_ASSERT(value.isMarkable());
    if (value.isObject())
        return ZoneOfObjectFromAnyThread(value.toObject());
    return static_cast<js::gc::Cell *>(value.toGCThing())->tenuredZoneFromAnyThread();
}

void
ValueReadBarrier(const Value &value);

template <typename T>
struct InternalGCMethods {};

template <typename T>
struct InternalGCMethods<T *>
{
    static bool isMarkable(T *v) { return v != nullptr; }

    static void preBarrier(T *v) { T::writeBarrierPre(v); }
    static void preBarrier(Zone *zone, T *v) { T::writeBarrierPre(zone, v); }

    static void postBarrier(T **vp) { T::writeBarrierPost(*vp, vp); }
    static void postBarrierRelocate(T **vp) { T::writeBarrierPostRelocate(*vp, vp); }
    static void postBarrierRemove(T **vp) { T::writeBarrierPostRemove(*vp, vp); }

    static void readBarrier(T *v) { T::readBarrier(v); }
};

template <>
struct InternalGCMethods<Value>
{
    static JSRuntime *runtimeFromAnyThread(const Value &v) {
        JS_ASSERT(v.isMarkable());
        return static_cast<js::gc::Cell *>(v.toGCThing())->runtimeFromAnyThread();
    }
    static JS::shadow::Runtime *shadowRuntimeFromAnyThread(const Value &v) {
        return reinterpret_cast<JS::shadow::Runtime*>(runtimeFromAnyThread(v));
    }
    static JSRuntime *runtimeFromMainThread(const Value &v) {
        JS_ASSERT(v.isMarkable());
        return static_cast<js::gc::Cell *>(v.toGCThing())->runtimeFromMainThread();
    }
    static JS::shadow::Runtime *shadowRuntimeFromMainThread(const Value &v) {
        return reinterpret_cast<JS::shadow::Runtime*>(runtimeFromMainThread(v));
    }

    static bool isMarkable(Value v) { return v.isMarkable(); }

    static void preBarrier(Value v) {
#ifdef JSGC_INCREMENTAL
        JS_ASSERT(!CurrentThreadIsIonCompiling());
        if (v.isMarkable() && shadowRuntimeFromAnyThread(v)->needsIncrementalBarrier())
            preBarrier(ZoneOfValueFromAnyThread(v), v);
#endif
    }

    static void preBarrier(Zone *zone, Value v) {
#ifdef JSGC_INCREMENTAL
        JS_ASSERT(!CurrentThreadIsIonCompiling());
        if (v.isString() && StringIsPermanentAtom(v.toString()))
            return;
        JS::shadow::Zone *shadowZone = JS::shadow::Zone::asShadowZone(zone);
        if (shadowZone->needsIncrementalBarrier()) {
            JS_ASSERT_IF(v.isMarkable(), shadowRuntimeFromMainThread(v)->needsIncrementalBarrier());
            Value tmp(v);
            js::gc::MarkValueUnbarriered(shadowZone->barrierTracer(), &tmp, "write barrier");
            JS_ASSERT(tmp == v);
        }
#endif
    }

    static void postBarrier(Value *vp) {
#ifdef JSGC_GENERATIONAL
        JS_ASSERT(!CurrentThreadIsIonCompiling());
        if (vp->isObject()) {
            gc::StoreBuffer *sb = reinterpret_cast<gc::Cell *>(&vp->toObject())->storeBuffer();
            if (sb)
                sb->putValueFromAnyThread(vp);
        }
#endif
    }

    static void postBarrierRelocate(Value *vp) {
#ifdef JSGC_GENERATIONAL
        JS_ASSERT(!CurrentThreadIsIonCompiling());
        if (vp->isObject()) {
            gc::StoreBuffer *sb = reinterpret_cast<gc::Cell *>(&vp->toObject())->storeBuffer();
            if (sb)
                sb->putRelocatableValueFromAnyThread(vp);
        }
#endif
    }

    static void postBarrierRemove(Value *vp) {
#ifdef JSGC_GENERATIONAL
        JS_ASSERT(vp);
        JS_ASSERT(vp->isMarkable());
        JS_ASSERT(!CurrentThreadIsIonCompiling());
        JSRuntime *rt = static_cast<js::gc::Cell *>(vp->toGCThing())->runtimeFromAnyThread();
        JS::shadow::Runtime *shadowRuntime = JS::shadow::Runtime::asShadowRuntime(rt);
        shadowRuntime->gcStoreBufferPtr()->removeRelocatableValueFromAnyThread(vp);
#endif
    }

    static void readBarrier(const Value &v) { ValueReadBarrier(v); }
};

template <>
struct InternalGCMethods<jsid>
{
    static bool isMarkable(jsid id) { return JSID_IS_STRING(id) || JSID_IS_SYMBOL(id); }

    static void preBarrier(jsid id) {
#ifdef JSGC_INCREMENTAL
        if (JSID_IS_STRING(id)) {
            JSString *str = JSID_TO_STRING(id);
            JS::shadow::Zone *shadowZone = ShadowZoneOfStringFromAnyThread(str);
            if (shadowZone->needsIncrementalBarrier()) {
                js::gc::MarkStringUnbarriered(shadowZone->barrierTracer(), &str, "write barrier");
                JS_ASSERT(str == JSID_TO_STRING(id));
            }
        } else if (JSID_IS_SYMBOL(id)) {
            JS::Symbol *sym = JSID_TO_SYMBOL(id);
            JS::shadow::Zone *shadowZone = ShadowZoneOfSymbolFromAnyThread(sym);
            if (shadowZone->needsIncrementalBarrier()) {
                js::gc::MarkSymbolUnbarriered(shadowZone->barrierTracer(), &sym, "write barrier");
                JS_ASSERT(sym == JSID_TO_SYMBOL(id));
            }
        }
#endif
    }
    static void preBarrier(Zone *zone, jsid id) { preBarrier(id); }

    static void postBarrier(jsid *idp) {}
    static void postBarrierRelocate(jsid *idp) {}
    static void postBarrierRemove(jsid *idp) {}
};

template <typename T>
class BarrieredBaseMixins {};

/*
 * Base class for barriered pointer types.
 */
template <class T>
class BarrieredBase : public BarrieredBaseMixins<T>
{
  protected:
    T value;

    explicit BarrieredBase(T v) : value(v) {}
    ~BarrieredBase() { pre(); }

  public:
    void init(T v) {
        JS_ASSERT(!GCMethods<T>::poisoned(v));
        this->value = v;
    }

    bool operator==(const T &other) const { return value == other; }
    bool operator!=(const T &other) const { return value != other; }

    /* Use this if the automatic coercion to T isn't working. */
    const T &get() const { return value; }

    /*
     * Use these if you want to change the value without invoking the barrier.
     * Obviously this is dangerous unless you know the barrier is not needed.
     */
    T *unsafeGet() { return &value; }
    const T *unsafeGet() const { return &value; }
    void unsafeSet(T v) { value = v; }

    T operator->() const { return value; }

    operator const T &() const { return value; }

    /* For users who need to manually barrier the raw types. */
    static void writeBarrierPre(const T &v) { InternalGCMethods<T>::preBarrier(v); }
    static void writeBarrierPost(const T &v, T *vp) { InternalGCMethods<T>::postBarrier(vp); }

  protected:
    void pre() { InternalGCMethods<T>::preBarrier(value); }
    void pre(Zone *zone) { InternalGCMethods<T>::preBarrier(zone, value); }
};

template <>
class BarrieredBaseMixins<JS::Value> : public ValueOperations<BarrieredBase<JS::Value> >
{
    friend class ValueOperations<BarrieredBase<JS::Value> >;
    const JS::Value * extract() const {
        return static_cast<const BarrieredBase<JS::Value>*>(this)->unsafeGet();
    }
};

/*
 * PreBarriered only automatically handles pre-barriers. Post-barriers must
 * be manually implemented when using this class. HeapPtr and RelocatablePtr
 * should be used in all cases that do not require explicit low-level control
 * of moving behavior, e.g. for HashMap keys.
 */
template <class T>
class PreBarriered : public BarrieredBase<T>
{
  public:
    PreBarriered() : BarrieredBase<T>(GCMethods<T>::initial()) {}
    /*
     * Allow implicit construction for use in generic contexts, such as DebuggerWeakMap::markKeys.
     */
    MOZ_IMPLICIT PreBarriered(T v) : BarrieredBase<T>(v) {}
    explicit PreBarriered(const PreBarriered<T> &v)
      : BarrieredBase<T>(v.value) {}

    /* Use to set the pointer to nullptr. */
    void clear() {
        this->pre();
        this->value = nullptr;
    }

    PreBarriered<T> &operator=(T v) {
        this->pre();
        JS_ASSERT(!GCMethods<T>::poisoned(v));
        this->value = v;
        return *this;
    }

    PreBarriered<T> &operator=(const PreBarriered<T> &v) {
        this->pre();
        JS_ASSERT(!GCMethods<T>::poisoned(v.value));
        this->value = v.value;
        return *this;
    }
};

/*
 * A pre- and post-barriered heap pointer, for use inside the JS engine.
 *
 * Not to be confused with JS::Heap<T>. This is a different class from the
 * external interface and implements substantially different semantics.
 *
 * The post-barriers implemented by this class are faster than those
 * implemented by RelocatablePtr<T> or JS::Heap<T> at the cost of not
 * automatically handling deletion or movement. It should generally only be
 * stored in memory that has GC lifetime. HeapPtr must not be used in contexts
 * where it may be implicitly moved or deleted, e.g. most containers.
 */
template <class T>
class HeapPtr : public BarrieredBase<T>
{
  public:
    HeapPtr() : BarrieredBase<T>(GCMethods<T>::initial()) {}
    explicit HeapPtr(T v) : BarrieredBase<T>(v) { post(); }
    explicit HeapPtr(const HeapPtr<T> &v) : BarrieredBase<T>(v) { post(); }

    void init(T v) {
        JS_ASSERT(!GCMethods<T>::poisoned(v));
        this->value = v;
        post();
    }

    HeapPtr<T> &operator=(T v) {
        this->pre();
        JS_ASSERT(!GCMethods<T>::poisoned(v));
        this->value = v;
        post();
        return *this;
    }

    HeapPtr<T> &operator=(const HeapPtr<T> &v) {
        this->pre();
        JS_ASSERT(!GCMethods<T>::poisoned(v.value));
        this->value = v.value;
        post();
        return *this;
    }

  protected:
    void post() { InternalGCMethods<T>::postBarrier(&this->value); }

    /* Make this friend so it can access pre() and post(). */
    template <class T1, class T2>
    friend inline void
    BarrieredSetPair(Zone *zone,
                     HeapPtr<T1*> &v1, T1 *val1,
                     HeapPtr<T2*> &v2, T2 *val2);

  private:
    /*
     * Unlike RelocatablePtr<T>, HeapPtr<T> must be managed with GC lifetimes.
     * Specifically, the memory used by the pointer itself must be live until
     * at least the next minor GC. For that reason, move semantics are invalid
     * and are deleted here. Please note that not all containers support move
     * semantics, so this does not completely prevent invalid uses.
     */
    HeapPtr(HeapPtr<T> &&) MOZ_DELETE;
    HeapPtr<T> &operator=(HeapPtr<T> &&) MOZ_DELETE;
};

/*
 * ImmutableTenuredPtr is designed for one very narrow case: replacing
 * immutable raw pointers to GC-managed things, implicitly converting to a
 * handle type for ease of use. Pointers encapsulated by this type must:
 *
 *   be immutable (no incremental write barriers),
 *   never point into the nursery (no generational write barriers), and
 *   be traced via MarkRuntime (we use fromMarkedLocation).
 *
 * In short: you *really* need to know what you're doing before you use this
 * class!
 */
template <typename T>
class ImmutableTenuredPtr
{
    T value;

  public:
    operator T() const { return value; }
    T operator->() const { return value; }

    operator Handle<T>() const {
        return Handle<T>::fromMarkedLocation(&value);
    }

    void init(T ptr) {
        JS_ASSERT(ptr->isTenured());
        value = ptr;
    }

    const T * address() { return &value; }
};

/*
 * A pre- and post-barriered heap pointer, for use inside the JS engine.
 *
 * Unlike HeapPtr<T>, it can be used in memory that is not managed by the GC,
 * i.e. in C++ containers.  It is, however, somewhat slower, so should only be
 * used in contexts where this ability is necessary.
 */
template <class T>
class RelocatablePtr : public BarrieredBase<T>
{
  public:
    RelocatablePtr() : BarrieredBase<T>(GCMethods<T>::initial()) {}
    explicit RelocatablePtr(T v) : BarrieredBase<T>(v) {
        if (GCMethods<T>::needsPostBarrier(v))
            post();
    }

    /*
     * For RelocatablePtr, move semantics are equivalent to copy semantics. In
     * C++, a copy constructor taking const-ref is the way to get a single
     * function that will be used for both lvalue and rvalue copies, so we can
     * simply omit the rvalue variant.
     */
    RelocatablePtr(const RelocatablePtr<T> &v) : BarrieredBase<T>(v) {
        if (GCMethods<T>::needsPostBarrier(this->value))
            post();
    }

    ~RelocatablePtr() {
        if (GCMethods<T>::needsPostBarrier(this->value))
            relocate();
    }

    RelocatablePtr<T> &operator=(T v) {
        this->pre();
        JS_ASSERT(!GCMethods<T>::poisoned(v));
        if (GCMethods<T>::needsPostBarrier(v)) {
            this->value = v;
            post();
        } else if (GCMethods<T>::needsPostBarrier(this->value)) {
            relocate();
            this->value = v;
        } else {
            this->value = v;
        }
        return *this;
    }

    RelocatablePtr<T> &operator=(const RelocatablePtr<T> &v) {
        this->pre();
        JS_ASSERT(!GCMethods<T>::poisoned(v.value));
        if (GCMethods<T>::needsPostBarrier(v.value)) {
            this->value = v.value;
            post();
        } else if (GCMethods<T>::needsPostBarrier(this->value)) {
            relocate();
            this->value = v;
        } else {
            this->value = v;
        }

        return *this;
    }

  protected:
    void post() {
#ifdef JSGC_GENERATIONAL
        JS_ASSERT(GCMethods<T>::needsPostBarrier(this->value));
        InternalGCMethods<T>::postBarrierRelocate(&this->value);
#endif
    }

    void relocate() {
#ifdef JSGC_GENERATIONAL
        JS_ASSERT(GCMethods<T>::needsPostBarrier(this->value));
        InternalGCMethods<T>::postBarrierRemove(&this->value);
#endif
    }
};

/*
 * This is a hack for RegExpStatics::updateFromMatch. It allows us to do two
 * barriers with only one branch to check if we're in an incremental GC.
 */
template <class T1, class T2>
static inline void
BarrieredSetPair(Zone *zone,
                 HeapPtr<T1*> &v1, T1 *val1,
                 HeapPtr<T2*> &v2, T2 *val2)
{
    if (T1::needWriteBarrierPre(zone)) {
        v1.pre();
        v2.pre();
    }
    v1.unsafeSet(val1);
    v2.unsafeSet(val2);
    v1.post();
    v2.post();
}

/* Useful for hashtables with a HeapPtr as key. */
template <class T>
struct HeapPtrHasher
{
    typedef HeapPtr<T> Key;
    typedef T Lookup;

    static HashNumber hash(Lookup obj) { return DefaultHasher<T>::hash(obj); }
    static bool match(const Key &k, Lookup l) { return k.get() == l; }
    static void rekey(Key &k, const Key& newKey) { k.unsafeSet(newKey); }
};

/* Specialized hashing policy for HeapPtrs. */
template <class T>
struct DefaultHasher< HeapPtr<T> > : HeapPtrHasher<T> { };

template <class T>
struct PreBarrieredHasher
{
    typedef PreBarriered<T> Key;
    typedef T Lookup;

    static HashNumber hash(Lookup obj) { return DefaultHasher<T>::hash(obj); }
    static bool match(const Key &k, Lookup l) { return k.get() == l; }
    static void rekey(Key &k, const Key& newKey) { k.unsafeSet(newKey); }
};

template <class T>
struct DefaultHasher< PreBarriered<T> > : PreBarrieredHasher<T> { };

/*
 * Incremental GC requires that weak pointers have read barriers. This is mostly
 * an issue for empty shapes stored in JSCompartment. The problem happens when,
 * during an incremental GC, some JS code stores one of the compartment's empty
 * shapes into an object already marked black. Normally, this would not be a
 * problem, because the empty shape would have been part of the initial snapshot
 * when the GC started. However, since this is a weak pointer, it isn't. So we
 * may collect the empty shape even though a live object points to it. To fix
 * this, we mark these empty shapes black whenever they get read out.
 */
template <class T>
class ReadBarriered
{
    T value;

  public:
    ReadBarriered() : value(nullptr) {}
    explicit ReadBarriered(T value) : value(value) {}
    explicit ReadBarriered(const Rooted<T> &rooted) : value(rooted) {}

    T get() const {
        if (!InternalGCMethods<T>::isMarkable(value))
            return GCMethods<T>::initial();
        InternalGCMethods<T>::readBarrier(value);
        return value;
    }

    operator T() const { return get(); }

    T &operator*() const { return *get(); }
    T operator->() const { return get(); }

    T *unsafeGet() { return &value; }
    T const * unsafeGet() const { return &value; }

    void set(T v) { value = v; }
};

class ArrayBufferObject;
class NestedScopeObject;
class DebugScopeObject;
class GlobalObject;
class ScriptSourceObject;
class Shape;
class BaseShape;
class UnownedBaseShape;
namespace jit {
class JitCode;
}
namespace types {
struct TypeObject;
struct TypeObjectAddendum;
}

typedef PreBarriered<JSObject*> PreBarrieredObject;
typedef PreBarriered<JSScript*> PreBarrieredScript;
typedef PreBarriered<jit::JitCode*> PreBarrieredJitCode;
typedef PreBarriered<JSAtom*> PreBarrieredAtom;

typedef RelocatablePtr<JSObject*> RelocatablePtrObject;
typedef RelocatablePtr<JSScript*> RelocatablePtrScript;
typedef RelocatablePtr<NestedScopeObject*> RelocatablePtrNestedScopeObject;

typedef HeapPtr<ArrayBufferObject*> HeapPtrArrayBufferObject;
typedef HeapPtr<BaseShape*> HeapPtrBaseShape;
typedef HeapPtr<JSAtom*> HeapPtrAtom;
typedef HeapPtr<JSFlatString*> HeapPtrFlatString;
typedef HeapPtr<JSFunction*> HeapPtrFunction;
typedef HeapPtr<JSLinearString*> HeapPtrLinearString;
typedef HeapPtr<JSObject*> HeapPtrObject;
typedef HeapPtr<JSScript*> HeapPtrScript;
typedef HeapPtr<JSString*> HeapPtrString;
typedef HeapPtr<PropertyName*> HeapPtrPropertyName;
typedef HeapPtr<Shape*> HeapPtrShape;
typedef HeapPtr<UnownedBaseShape*> HeapPtrUnownedBaseShape;
typedef HeapPtr<jit::JitCode*> HeapPtrJitCode;
typedef HeapPtr<types::TypeObject*> HeapPtrTypeObject;
typedef HeapPtr<types::TypeObjectAddendum*> HeapPtrTypeObjectAddendum;

typedef PreBarriered<Value> PreBarrieredValue;
typedef RelocatablePtr<Value> RelocatableValue;
typedef HeapPtr<Value> HeapValue;

typedef PreBarriered<jsid> PreBarrieredId;
typedef RelocatablePtr<jsid> RelocatableId;
typedef HeapPtr<jsid> HeapId;

typedef ImmutableTenuredPtr<PropertyName*> ImmutablePropertyNamePtr;
typedef ImmutableTenuredPtr<JS::Symbol*> ImmutableSymbolPtr;

typedef ReadBarriered<DebugScopeObject*> ReadBarrieredDebugScopeObject;
typedef ReadBarriered<GlobalObject*> ReadBarrieredGlobalObject;
typedef ReadBarriered<JSFunction*> ReadBarrieredFunction;
typedef ReadBarriered<JSObject*> ReadBarrieredObject;
typedef ReadBarriered<ScriptSourceObject*> ReadBarrieredScriptSourceObject;
typedef ReadBarriered<Shape*> ReadBarrieredShape;
typedef ReadBarriered<UnownedBaseShape*> ReadBarrieredUnownedBaseShape;
typedef ReadBarriered<jit::JitCode*> ReadBarrieredJitCode;
typedef ReadBarriered<types::TypeObject*> ReadBarrieredTypeObject;
typedef ReadBarriered<JSAtom*> ReadBarrieredAtom;
typedef ReadBarriered<JS::Symbol*> ReadBarrieredSymbol;

typedef ReadBarriered<Value> ReadBarrieredValue;

// A pre- and post-barriered Value that is specialized to be aware that it
// resides in a slots or elements vector. This allows it to be relocated in
// memory, but with substantially less overhead than a RelocatablePtr.
class HeapSlot : public BarrieredBase<Value>
{
  public:
    enum Kind {
        Slot = 0,
        Element = 1
    };

    explicit HeapSlot() MOZ_DELETE;

    explicit HeapSlot(JSObject *obj, Kind kind, uint32_t slot, const Value &v)
      : BarrieredBase<Value>(v)
    {
        JS_ASSERT(!IsPoisonedValue(v));
        post(obj, kind, slot, v);
    }

    explicit HeapSlot(JSObject *obj, Kind kind, uint32_t slot, const HeapSlot &s)
      : BarrieredBase<Value>(s.value)
    {
        JS_ASSERT(!IsPoisonedValue(s.value));
        post(obj, kind, slot, s);
    }

    ~HeapSlot() {
        pre();
    }

    void init(JSObject *owner, Kind kind, uint32_t slot, const Value &v) {
        value = v;
        post(owner, kind, slot, v);
    }

#ifdef DEBUG
    bool preconditionForSet(JSObject *owner, Kind kind, uint32_t slot);
    bool preconditionForSet(Zone *zone, JSObject *owner, Kind kind, uint32_t slot);
    bool preconditionForWriteBarrierPost(JSObject *obj, Kind kind, uint32_t slot, Value target) const;
#endif

    void set(JSObject *owner, Kind kind, uint32_t slot, const Value &v) {
        JS_ASSERT(preconditionForSet(owner, kind, slot));
        JS_ASSERT(!IsPoisonedValue(v));
        pre();
        value = v;
        post(owner, kind, slot, v);
    }

    void set(Zone *zone, JSObject *owner, Kind kind, uint32_t slot, const Value &v) {
        JS_ASSERT(preconditionForSet(zone, owner, kind, slot));
        JS_ASSERT(!IsPoisonedValue(v));
        pre(zone);
        value = v;
        post(owner, kind, slot, v);
    }

    /* For users who need to manually barrier the raw types. */
    static void writeBarrierPost(JSObject *owner, Kind kind, uint32_t slot, const Value &target) {
        reinterpret_cast<HeapSlot *>(const_cast<Value *>(&target))->post(owner, kind, slot, target);
    }

  private:
    void post(JSObject *owner, Kind kind, uint32_t slot, const Value &target) {
        JS_ASSERT(preconditionForWriteBarrierPost(owner, kind, slot, target));
#ifdef JSGC_GENERATIONAL
        if (this->value.isObject()) {
            gc::Cell *cell = reinterpret_cast<gc::Cell *>(&this->value.toObject());
            if (cell->storeBuffer())
                cell->storeBuffer()->putSlotFromAnyThread(owner, kind, slot, 1);
        }
#endif
    }
};

static inline const Value *
Valueify(const BarrieredBase<Value> *array)
{
    JS_STATIC_ASSERT(sizeof(HeapValue) == sizeof(Value));
    JS_STATIC_ASSERT(sizeof(HeapSlot) == sizeof(Value));
    return (const Value *)array;
}

static inline HeapValue *
HeapValueify(Value *v)
{
    JS_STATIC_ASSERT(sizeof(HeapValue) == sizeof(Value));
    JS_STATIC_ASSERT(sizeof(HeapSlot) == sizeof(Value));
    return (HeapValue *)v;
}

class HeapSlotArray
{
    HeapSlot *array;

  public:
    explicit HeapSlotArray(HeapSlot *array) : array(array) {}

    operator const Value *() const { return Valueify(array); }
    operator HeapSlot *() const { return array; }

    HeapSlotArray operator +(int offset) const { return HeapSlotArray(array + offset); }
    HeapSlotArray operator +(uint32_t offset) const { return HeapSlotArray(array + offset); }
};

/*
 * Operations on a Heap thing inside the GC need to strip the barriers from
 * pointer operations. This template helps do that in contexts where the type
 * is templatized.
 */
template <typename T> struct Unbarriered {};
template <typename S> struct Unbarriered< PreBarriered<S> > { typedef S *type; };
template <typename S> struct Unbarriered< RelocatablePtr<S> > { typedef S *type; };
template <> struct Unbarriered<PreBarrieredValue> { typedef Value type; };
template <> struct Unbarriered<RelocatableValue> { typedef Value type; };
template <typename S> struct Unbarriered< DefaultHasher< PreBarriered<S> > > {
    typedef DefaultHasher<S *> type;
};

} /* namespace js */

#endif /* gc_Barrier_h */
