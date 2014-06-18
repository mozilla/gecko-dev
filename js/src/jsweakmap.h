/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsweakmap_h
#define jsweakmap_h

#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsobj.h"

#include "gc/Marking.h"
#include "js/HashTable.h"

namespace js {

// A subclass template of js::HashMap whose keys and values may be garbage-collected. When
// a key is collected, the table entry disappears, dropping its reference to the value.
//
// More precisely:
//
//     A WeakMap entry is collected if and only if either the WeakMap or the entry's key
//     is collected. If an entry is not collected, it remains in the WeakMap and it has a
//     strong reference to the value.
//
// You must call this table's 'trace' method when the object of which it is a part is
// reached by the garbage collection tracer. Once a table is known to be live, the
// implementation takes care of the iterative marking needed for weak tables and removing
// table entries when collection is complete.

// The value for the next pointer for maps not in the map list.
static WeakMapBase * const WeakMapNotInList = reinterpret_cast<WeakMapBase *>(1);

typedef HashSet<WeakMapBase *, DefaultHasher<WeakMapBase *>, SystemAllocPolicy> WeakMapSet;

// Common base class for all WeakMap specializations. The collector uses this to call
// their markIteratively and sweep methods.
class WeakMapBase {
  public:
    WeakMapBase(JSObject *memOf, JSCompartment *c);
    virtual ~WeakMapBase();

    void trace(JSTracer *tracer);

    // Garbage collector entry points.

    // Unmark all weak maps in a compartment.
    static void unmarkCompartment(JSCompartment *c);

    // Check all weak maps in a compartment that have been marked as live in this garbage
    // collection, and mark the values of all entries that have become strong references
    // to them. Return true if we marked any new values, indicating that we need to make
    // another pass. In other words, mark my marked maps' marked members' mid-collection.
    static bool markCompartmentIteratively(JSCompartment *c, JSTracer *tracer);

    // Add zone edges for weakmaps with key delegates in a different zone.
    static bool findZoneEdgesForCompartment(JSCompartment *c);

    // Sweep the weak maps in a compartment, removing dead weak maps and removing
    // entries of live weak maps whose keys are dead.
    static void sweepCompartment(JSCompartment *c);

    // Trace all delayed weak map bindings. Used by the cycle collector.
    static void traceAllMappings(WeakMapTracer *tracer);

    bool isInList() { return next != WeakMapNotInList; }

    // Save information about which weak maps are marked for a compartment.
    static bool saveCompartmentMarkedWeakMaps(JSCompartment *c, WeakMapSet &markedWeakMaps);

    // Restore information about which weak maps are marked for many compartments.
    static void restoreCompartmentMarkedWeakMaps(WeakMapSet &markedWeakMaps);

    // Remove a weakmap from its compartment's weakmaps list.
    static void removeWeakMapFromList(WeakMapBase *weakmap);

  protected:
    // Instance member functions called by the above. Instantiations of WeakMap override
    // these with definitions appropriate for their Key and Value types.
    virtual void nonMarkingTraceKeys(JSTracer *tracer) = 0;
    virtual void nonMarkingTraceValues(JSTracer *tracer) = 0;
    virtual bool markIteratively(JSTracer *tracer) = 0;
    virtual bool findZoneEdges() = 0;
    virtual void sweep() = 0;
    virtual void traceMappings(WeakMapTracer *tracer) = 0;
    virtual void finish() = 0;

    // Object that this weak map is part of, if any.
    JSObject *memberOf;

    // Compartment that this weak map is part of.
    JSCompartment *compartment;

    // Link in a list of all WeakMaps in a compartment, headed by
    // JSCompartment::gcWeakMapList. The last element of the list has nullptr as
    // its next. Maps not in the list have WeakMapNotInList as their next.
    WeakMapBase *next;

    // Whether this object has been traced during garbage collection.
    bool marked;
};

template <class Key, class Value,
          class HashPolicy = DefaultHasher<Key> >
class WeakMap : public HashMap<Key, Value, HashPolicy, RuntimeAllocPolicy>, public WeakMapBase
{
  public:
    typedef HashMap<Key, Value, HashPolicy, RuntimeAllocPolicy> Base;
    typedef typename Base::Enum Enum;
    typedef typename Base::Lookup Lookup;
    typedef typename Base::Range Range;

    explicit WeakMap(JSContext *cx, JSObject *memOf = nullptr)
        : Base(cx->runtime()), WeakMapBase(memOf, cx->compartment()) { }

    bool init(uint32_t len = 16) {
        if (!Base::init(len))
            return false;
        next = compartment->gcWeakMapList;
        compartment->gcWeakMapList = this;
        marked = JS::IsIncrementalGCInProgress(compartment->runtimeFromMainThread());
        return true;
    }

  private:
    bool markValue(JSTracer *trc, Value *x) {
        if (gc::IsMarked(x))
            return false;
        gc::Mark(trc, x, "WeakMap entry value");
        JS_ASSERT(gc::IsMarked(x));
        return true;
    }

    void nonMarkingTraceKeys(JSTracer *trc) {
        for (Enum e(*this); !e.empty(); e.popFront()) {
            Key key(e.front().key());
            gc::Mark(trc, &key, "WeakMap entry key");
            if (key != e.front().key())
                entryMoved(e, key);
        }
    }

    void nonMarkingTraceValues(JSTracer *trc) {
        for (Range r = Base::all(); !r.empty(); r.popFront())
            gc::Mark(trc, &r.front().value(), "WeakMap entry value");
    }

    bool keyNeedsMark(JSObject *key) {
        if (JSWeakmapKeyDelegateOp op = key->getClass()->ext.weakmapKeyDelegateOp) {
            JSObject *delegate = op(key);
            /*
             * Check if the delegate is marked with any color to properly handle
             * gray marking when the key's delegate is black and the map is
             * gray.
             */
            return delegate && gc::IsObjectMarked(&delegate);
        }
        return false;
    }

    bool keyNeedsMark(gc::Cell *cell) {
        return false;
    }

    bool markIteratively(JSTracer *trc) {
        bool markedAny = false;
        for (Enum e(*this); !e.empty(); e.popFront()) {
            /* If the entry is live, ensure its key and value are marked. */
            Key key(e.front().key());
            if (gc::IsMarked(const_cast<Key *>(&key))) {
                if (markValue(trc, &e.front().value()))
                    markedAny = true;
                if (e.front().key() != key)
                    entryMoved(e, key);
            } else if (keyNeedsMark(key)) {
                gc::Mark(trc, &e.front().value(), "WeakMap entry value");
                gc::Mark(trc, &key, "proxy-preserved WeakMap entry key");
                if (e.front().key() != key)
                    entryMoved(e, key);
                markedAny = true;
            }
            key.unsafeSet(nullptr);
        }
        return markedAny;
    }

    bool findZoneEdges() {
        // This is overridden by ObjectValueMap.
        return true;
    }

    void sweep() {
        /* Remove all entries whose keys remain unmarked. */
        for (Enum e(*this); !e.empty(); e.popFront()) {
            Key k(e.front().key());
            if (gc::IsAboutToBeFinalized(&k))
                e.removeFront();
            else if (k != e.front().key())
                entryMoved(e, k);
        }
        /*
         * Once we've swept, all remaining edges should stay within the
         * known-live part of the graph.
         */
        assertEntriesNotAboutToBeFinalized();
    }

    void finish() {
        Base::finish();
    }

    /* memberOf can be nullptr, which means that the map is not part of a JSObject. */
    void traceMappings(WeakMapTracer *tracer) {
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            gc::Cell *key = gc::ToMarkable(r.front().key());
            gc::Cell *value = gc::ToMarkable(r.front().value());
            if (key && value) {
                tracer->callback(tracer, memberOf,
                                 key, gc::TraceKind(r.front().key()),
                                 value, gc::TraceKind(r.front().value()));
            }
        }
    }

    /* Rekey an entry when moved, ensuring we do not trigger barriers. */
    void entryMoved(Enum &eArg, const Key &k) {
        typedef typename HashMap<typename Unbarriered<Key>::type,
                                 typename Unbarriered<Value>::type,
                                 typename Unbarriered<HashPolicy>::type,
                                 RuntimeAllocPolicy>::Enum UnbarrieredEnum;
        UnbarrieredEnum &e = reinterpret_cast<UnbarrieredEnum &>(eArg);
        e.rekeyFront(reinterpret_cast<const typename Unbarriered<Key>::type &>(k));
    }

protected:
    void assertEntriesNotAboutToBeFinalized() {
#if DEBUG
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            Key k(r.front().key());
            JS_ASSERT(!gc::IsAboutToBeFinalized(&k));
            JS_ASSERT(!gc::IsAboutToBeFinalized(&r.front().value()));
            JS_ASSERT(k == r.front().key());
        }
#endif
    }
};

} /* namespace js */

extern JSObject *
js_InitWeakMapClass(JSContext *cx, js::HandleObject obj);

#endif /* jsweakmap_h */
