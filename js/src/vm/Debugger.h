/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Debugger_h
#define vm_Debugger_h

#include "mozilla/DoublyLinkedList.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Range.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Vector.h"

#include "builtin/Promise.h"
#include "ds/TraceableFifo.h"
#include "gc/Barrier.h"
#include "gc/WeakMap.h"
#include "js/Debug.h"
#include "js/GCVariant.h"
#include "js/HashTable.h"
#include "js/Utility.h"
#include "js/Wrapper.h"
#include "vm/GeneratorObject.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/SavedStacks.h"
#include "vm/Stack.h"

namespace js {

/**
 * Tells how the JS engine should resume debuggee execution after firing a
 * debugger hook.  Most debugger hooks get to choose how the debuggee proceeds;
 * see js/src/doc/Debugger/Conventions.md under "Resumption Values".
 *
 * Debugger::processHandlerResult() translates between JavaScript values and
 * this enum.
 *
 * The values `ResumeMode::Throw` and `ResumeMode::Return` are always
 * associated with a value (the exception value or return value). Sometimes
 * this is represented as an explicit `JS::Value` variable or parameter,
 * declared alongside the `ResumeMode`. In other cases, especially when
 * ResumeMode is used as a return type (as in Debugger::onEnterFrame), the
 * value is stashed in `cx`'s pending exception slot or the topmost frame's
 * return value slot.
 */
enum class ResumeMode {
    /**
     * The debuggee should continue unchanged.
     *
     * This corresponds to a resumption value of `undefined`.
     */
    Continue,

    /**
     * Throw an exception in the debuggee.
     *
     * This corresponds to a resumption value of `{throw: <value>}`.
     */
    Throw,

    /**
     * Terminate the debuggee, as if it had been cancelled via the "slow
     * script" ribbon.
     *
     * This corresponds to a resumption value of `null`.
     */
    Terminate,

    /**
     * Force the debuggee to return from the current frame.
     *
     * This corresponds to a resumption value of `{return: <value>}`.
     */
    Return,
};

class Breakpoint;
class DebuggerMemory;
class ScriptedOnStepHandler;
class ScriptedOnPopHandler;
class WasmInstanceObject;

typedef HashSet<ReadBarrieredGlobalObject,
                MovableCellHasher<ReadBarrieredGlobalObject>,
                ZoneAllocPolicy> WeakGlobalObjectSet;

#ifdef DEBUG
extern void
CheckDebuggeeThing(JSScript* script, bool invisibleOk);

extern void
CheckDebuggeeThing(LazyScript* script, bool invisibleOk);

extern void
CheckDebuggeeThing(JSObject* obj, bool invisibleOk);
#endif

/*
 * A weakmap from GC thing keys to JSObject values that supports the keys being
 * in different compartments to the values. All values must be in the same
 * compartment.
 *
 * The purpose of this is to allow the garbage collector to easily find edges
 * from debuggee object compartments to debugger compartments when calculating
 * the compartment groups.  Note that these edges are the inverse of the edges
 * stored in the cross compartment map.
 *
 * The current implementation results in all debuggee object compartments being
 * swept in the same group as the debugger.  This is a conservative approach,
 * and compartments may be unnecessarily grouped, however it results in a
 * simpler and faster implementation.
 *
 * If InvisibleKeysOk is true, then the map can have keys in invisible-to-
 * debugger compartments. If it is false, we assert that such entries are never
 * created.
 *
 * Also note that keys in these weakmaps can be in any compartment, debuggee or
 * not, because they are not deleted when a compartment is no longer a
 * debuggee: the values need to maintain object identity across add/remove/add
 * transitions. (Frames are an exception to the rule. Existing Debugger.Frame
 * objects are killed when debugging is disabled for their compartment, and if
 * it's re-enabled later, new Frame objects are created.)
 */
template <class UnbarrieredKey, bool InvisibleKeysOk=false>
class DebuggerWeakMap : private WeakMap<HeapPtr<UnbarrieredKey>, HeapPtr<JSObject*>>
{
  private:
    typedef HeapPtr<UnbarrieredKey> Key;
    typedef HeapPtr<JSObject*> Value;

    typedef HashMap<JS::Zone*,
                    uintptr_t,
                    DefaultHasher<JS::Zone*>,
                    ZoneAllocPolicy> CountMap;

    CountMap zoneCounts;
    JS::Compartment* compartment;

  public:
    typedef WeakMap<Key, Value> Base;

    explicit DebuggerWeakMap(JSContext* cx)
        : Base(cx),
          zoneCounts(cx->zone()),
          compartment(cx->compartment())
    { }

  public:
    // Expose those parts of HashMap public interface that are used by Debugger
    // methods.

    typedef typename Base::Entry Entry;
    typedef typename Base::Ptr Ptr;
    typedef typename Base::AddPtr AddPtr;
    typedef typename Base::Range Range;
    typedef typename Base::Enum Enum;
    typedef typename Base::Lookup Lookup;

    // Expose WeakMap public interface.

    using Base::lookup;
    using Base::lookupForAdd;
    using Base::remove;
    using Base::all;
    using Base::trace;

    template<typename KeyInput, typename ValueInput>
    bool relookupOrAdd(AddPtr& p, const KeyInput& k, const ValueInput& v) {
        MOZ_ASSERT(v->compartment() == this->compartment);
#ifdef DEBUG
        CheckDebuggeeThing(k, InvisibleKeysOk);
#endif
        MOZ_ASSERT(!Base::has(k));
        if (!incZoneCount(k->zone())) {
            return false;
        }
        bool ok = Base::relookupOrAdd(p, k, v);
        if (!ok) {
            decZoneCount(k->zone());
        }
        return ok;
    }

    template <typename KeyInput>
    bool has(const KeyInput& k) const {
        return !!this->lookup(k);
    }

    void remove(const Lookup& l) {
        MOZ_ASSERT(Base::has(l));
        Base::remove(l);
        decZoneCount(l->zone());
    }

    // Remove entries whose keys satisfy the given predicate.
    template <typename Predicate>
    void removeIf(Predicate test) {
        for (Enum e(*static_cast<Base*>(this)); !e.empty(); e.popFront()) {
            JSObject* key = e.front().key();
            if (test(key)) {
                decZoneCount(key->zoneFromAnyThread());
                e.removeFront();
            }
        }
    }

  public:
    template <void (traceValueEdges)(JSTracer*, JSObject*)>
    void traceCrossCompartmentEdges(JSTracer* tracer) {
        for (Enum e(*static_cast<Base*>(this)); !e.empty(); e.popFront()) {
            traceValueEdges(tracer, e.front().value());
            Key key = e.front().key();
            TraceEdge(tracer, &key, "Debugger WeakMap key");
            if (key != e.front().key()) {
                e.rekeyFront(key);
            }
            key.unsafeSet(nullptr);
        }
    }

    bool hasKeyInZone(JS::Zone* zone) {
        CountMap::Ptr p = zoneCounts.lookup(zone);
        MOZ_ASSERT_IF(p.found(), p->value() > 0);
        return p.found();
    }

  private:
    /* Override sweep method to also update our edge cache. */
    void sweep() override {
        MOZ_ASSERT(CurrentThreadIsPerformingGC());
        for (Enum e(*static_cast<Base*>(this)); !e.empty(); e.popFront()) {
            if (gc::IsAboutToBeFinalized(&e.front().mutableKey())) {
                decZoneCount(e.front().key()->zoneFromAnyThread());
                e.removeFront();
            }
        }
#ifdef DEBUG
        Base::assertEntriesNotAboutToBeFinalized();
#endif
    }

    MOZ_MUST_USE bool incZoneCount(JS::Zone* zone) {
        CountMap::AddPtr p = zoneCounts.lookupForAdd(zone);
        if (!p && !zoneCounts.add(p, zone, 0)) {
            return false;   // OOM'd while adding
        }
        ++p->value();
        return true;
    }

    void decZoneCount(JS::Zone* zone) {
        CountMap::Ptr p = zoneCounts.lookup(zone);
        MOZ_ASSERT(p);
        MOZ_ASSERT(p->value() > 0);
        --p->value();
        if (p->value() == 0) {
            zoneCounts.remove(zone);
        }
    }
};

class LeaveDebuggeeNoExecute;

// Suppresses all debuggee NX checks, i.e., allow all execution. Used to allow
// certain whitelisted operations to execute code.
//
// WARNING
// WARNING Do not use this unless you know what you are doing!
// WARNING
class AutoSuppressDebuggeeNoExecuteChecks
{
    EnterDebuggeeNoExecute** stack_;
    EnterDebuggeeNoExecute* prev_;

  public:
    explicit AutoSuppressDebuggeeNoExecuteChecks(JSContext* cx) {
        stack_ = &cx->noExecuteDebuggerTop.ref();
        prev_ = *stack_;
        *stack_ = nullptr;
    }

    ~AutoSuppressDebuggeeNoExecuteChecks() {
        MOZ_ASSERT(!*stack_);
        *stack_ = prev_;
    }
};

class MOZ_RAII EvalOptions {
    JS::UniqueChars filename_;
    unsigned lineno_ = 1;

  public:
    EvalOptions() = default;
    ~EvalOptions() = default;
    const char* filename() const { return filename_.get(); }
    unsigned lineno() const { return lineno_; }
    MOZ_MUST_USE bool setFilename(JSContext* cx, const char* filename);
    void setLineno(unsigned lineno) { lineno_ = lineno; }
};

/*
 * Env is the type of what ES5 calls "lexical environments" (runtime activations
 * of lexical scopes). This is currently just JSObject, and is implemented by
 * CallObject, LexicalEnvironmentObject, and WithEnvironmentObject, among
 * others--but environments and objects are really two different concepts.
 */
typedef JSObject Env;

// One of a real JSScript, a real LazyScript, or synthesized.
//
// If synthesized, the referent is one of the following:
//
//   1. A WasmInstanceObject, denoting a synthesized toplevel wasm module
//      script.
//   2. A wasm JSFunction, denoting a synthesized wasm function script.
//      NYI!
typedef mozilla::Variant<JSScript*, LazyScript*, WasmInstanceObject*> DebuggerScriptReferent;

// Either a ScriptSourceObject, for ordinary JS, or a WasmInstanceObject,
// denoting the synthesized source of a wasm module.
typedef mozilla::Variant<ScriptSourceObject*, WasmInstanceObject*> DebuggerSourceReferent;

class Debugger : private mozilla::LinkedListElement<Debugger>
{
    friend class Breakpoint;
    friend class DebuggerMemory;
    friend struct JSRuntime::GlobalObjectWatchersLinkAccess<Debugger>;
    friend class SavedStacks;
    friend class ScriptedOnStepHandler;
    friend class ScriptedOnPopHandler;
    friend class mozilla::LinkedListElement<Debugger>;
    friend class mozilla::LinkedList<Debugger>;
    friend bool (::JS_DefineDebuggerObject)(JSContext* cx, JS::HandleObject obj);
    friend bool (::JS::dbg::IsDebugger)(JSObject&);
    friend bool (::JS::dbg::GetDebuggeeGlobals)(JSContext*, JSObject&, AutoObjectVector&);
    friend bool JS::dbg::FireOnGarbageCollectionHookRequired(JSContext* cx);
    friend bool JS::dbg::FireOnGarbageCollectionHook(JSContext* cx,
                                                     JS::dbg::GarbageCollectionEvent::Ptr&& data);

  public:
    enum Hook {
        OnDebuggerStatement,
        OnExceptionUnwind,
        OnNewScript,
        OnEnterFrame,
        OnNewGlobalObject,
        OnNewPromise,
        OnPromiseSettled,
        OnGarbageCollection,
        HookCount
    };
    enum {
        JSSLOT_DEBUG_PROTO_START,
        JSSLOT_DEBUG_FRAME_PROTO = JSSLOT_DEBUG_PROTO_START,
        JSSLOT_DEBUG_ENV_PROTO,
        JSSLOT_DEBUG_OBJECT_PROTO,
        JSSLOT_DEBUG_SCRIPT_PROTO,
        JSSLOT_DEBUG_SOURCE_PROTO,
        JSSLOT_DEBUG_MEMORY_PROTO,
        JSSLOT_DEBUG_PROTO_STOP,
        JSSLOT_DEBUG_HOOK_START = JSSLOT_DEBUG_PROTO_STOP,
        JSSLOT_DEBUG_HOOK_STOP = JSSLOT_DEBUG_HOOK_START + HookCount,
        JSSLOT_DEBUG_MEMORY_INSTANCE = JSSLOT_DEBUG_HOOK_STOP,
        JSSLOT_DEBUG_COUNT
    };

    class ExecutionObservableSet
    {
      public:
        typedef HashSet<Zone*>::Range ZoneRange;

        virtual Zone* singleZone() const { return nullptr; }
        virtual JSScript* singleScriptForZoneInvalidation() const { return nullptr; }
        virtual const HashSet<Zone*>* zones() const { return nullptr; }

        virtual bool shouldRecompileOrInvalidate(JSScript* script) const = 0;
        virtual bool shouldMarkAsDebuggee(FrameIter& iter) const = 0;
    };

    // This enum is converted to and compare with bool values; NotObserving
    // must be 0 and Observing must be 1.
    enum IsObserving {
        NotObserving = 0,
        Observing = 1
    };

    // Return true if the given realm is a debuggee of this debugger,
    // false otherwise.
    bool isDebuggeeUnbarriered(const Realm* realm) const;

    // Return true if this Debugger observed a debuggee that participated in the
    // GC identified by the given GC number. Return false otherwise.
    // May return false negatives if we have hit OOM.
    bool observedGC(uint64_t majorGCNumber) const {
        return observedGCs.has(majorGCNumber);
    }

    // Notify this Debugger that one or more of its debuggees is participating
    // in the GC identified by the given GC number.
    bool debuggeeIsBeingCollected(uint64_t majorGCNumber) {
        return observedGCs.put(majorGCNumber);
    }

    bool isEnabled() const {
        return enabled;
    }

    static SavedFrame* getObjectAllocationSite(JSObject& obj);

    struct AllocationsLogEntry
    {
        AllocationsLogEntry(HandleObject frame, mozilla::TimeStamp when, const char* className,
                            HandleAtom ctorName, size_t size, bool inNursery)
            : frame(frame),
              when(when),
              className(className),
              ctorName(ctorName),
              size(size),
              inNursery(inNursery)
        {
            MOZ_ASSERT_IF(frame, UncheckedUnwrap(frame)->is<SavedFrame>());
        };

        HeapPtr<JSObject*> frame;
        mozilla::TimeStamp when;
        const char* className;
        HeapPtr<JSAtom*> ctorName;
        size_t size;
        bool inNursery;

        void trace(JSTracer* trc) {
            TraceNullableEdge(trc, &frame, "Debugger::AllocationsLogEntry::frame");
            TraceNullableEdge(trc, &ctorName, "Debugger::AllocationsLogEntry::ctorName");
        }
    };

    // Barrier methods so we can have ReadBarriered<Debugger*>.
    static void readBarrier(Debugger* dbg) {
        InternalBarrierMethods<JSObject*>::readBarrier(dbg->object);
    }
    static void writeBarrierPost(Debugger** vp, Debugger* prev, Debugger* next) {}
#ifdef DEBUG
    static bool thingIsNotGray(Debugger* dbg) { return true; }
#endif

  private:
    GCPtrNativeObject object; /* The Debugger object. Strong reference. */
    WeakGlobalObjectSet debuggees; /* Debuggee globals. Cross-compartment weak references. */
    JS::ZoneSet debuggeeZones; /* Set of zones that we have debuggees in. */
    js::GCPtrObject uncaughtExceptionHook; /* Strong reference. */
    bool enabled;
    bool allowUnobservedAsmJS;

    // Whether to enable code coverage on the Debuggee.
    bool collectCoverageInfo;

    template <typename T>
    struct DebuggerLinkAccess {
      static mozilla::DoublyLinkedListElement<T>& Get(T* aThis) {
        return aThis->debuggerLink;
      }
    };

    // List of all js::Breakpoints in this debugger.
    using BreakpointList =
        mozilla::DoublyLinkedList<js::Breakpoint,
                                  DebuggerLinkAccess<js::Breakpoint>>;
    BreakpointList breakpoints;

    // The set of GC numbers for which one or more of this Debugger's observed
    // debuggees participated in.
    using GCNumberSet = HashSet<uint64_t, DefaultHasher<uint64_t>, ZoneAllocPolicy>;
    GCNumberSet observedGCs;

    using AllocationsLog = js::TraceableFifo<AllocationsLogEntry>;

    AllocationsLog allocationsLog;
    bool trackingAllocationSites;
    double allocationSamplingProbability;
    size_t maxAllocationsLogLength;
    bool allocationsLogOverflowed;

    static const size_t DEFAULT_MAX_LOG_LENGTH = 5000;

    MOZ_MUST_USE bool appendAllocationSite(JSContext* cx, HandleObject obj, HandleSavedFrame frame,
                                           mozilla::TimeStamp when);

    /*
     * Recompute the set of debuggee zones based on the set of debuggee globals.
     */
    void recomputeDebuggeeZoneSet();

    /*
     * Return true if there is an existing object metadata callback for the
     * given global's compartment that will prevent our instrumentation of
     * allocations.
     */
    static bool cannotTrackAllocations(const GlobalObject& global);

    /*
     * Return true if the given global is being observed by at least one
     * Debugger that is tracking allocations.
     */
    static bool isObservedByDebuggerTrackingAllocations(const GlobalObject& global);

    /*
     * Add allocations tracking for objects allocated within the given
     * debuggee's compartment. The given debuggee global must be observed by at
     * least one Debugger that is enabled and tracking allocations.
     */
    static MOZ_MUST_USE bool addAllocationsTracking(JSContext* cx, Handle<GlobalObject*> debuggee);

    /*
     * Remove allocations tracking for objects allocated within the given
     * global's compartment. This is a no-op if there are still Debuggers
     * observing this global and who are tracking allocations.
     */
    static void removeAllocationsTracking(GlobalObject& global);

    /*
     * Add or remove allocations tracking for all debuggees.
     */
    MOZ_MUST_USE bool addAllocationsTrackingForAllDebuggees(JSContext* cx);
    void removeAllocationsTrackingForAllDebuggees();

    /*
     * If this Debugger is enabled, and has a onNewGlobalObject handler, then
     * this link is inserted into the list headed by
     * JSRuntime::onNewGlobalObjectWatchers.
     */
    mozilla::DoublyLinkedListElement<Debugger> onNewGlobalObjectWatchersLink;

    /*
     * Map from stack frames that are currently on the stack to Debugger.Frame
     * instances.
     *
     * The keys are always live stack frames. We drop them from this map as
     * soon as they leave the stack (see slowPathOnLeaveFrame) and in
     * removeDebuggee.
     *
     * We don't trace the keys of this map (the frames are on the stack and
     * thus necessarily live), but we do trace the values. It's like a WeakMap
     * that way, but since stack frames are not gc-things, the implementation
     * has to be different.
     */
    typedef HashMap<AbstractFramePtr,
                    HeapPtr<DebuggerFrame*>,
                    DefaultHasher<AbstractFramePtr>,
                    ZoneAllocPolicy> FrameMap;
    FrameMap frames;

    /*
     * Map from generator objects to their Debugger.Frame instances.
     *
     * When a Debugger.Frame is created for a generator frame, it is added to
     * this map and remains there for the lifetime of the generator, whether
     * that frame is on the stack at the moment or not.  This is in addition to
     * the entry in `frames` that exists as long as the generator frame is on
     * the stack.
     *
     * We need to keep the Debugger.Frame object alive to deliver it to the
     * onEnterFrame handler on resume, and to retain onStep and onPop hooks.
     *
     * An entry is present in this table when:
     * -   both the debuggee generator object and the Debugger.Frame object exist
     * -   the Debugger.Frame's owner is still an enabled debugger of
     *     the debuggee compartment
     * regardless of whether the frame is currently suspended. (This list is
     * meant to explain why we update the table in the particular places where
     * we do so.)
     */
    typedef DebuggerWeakMap<JSObject*> GeneratorWeakMap;
    GeneratorWeakMap generatorFrames;

    /* An ephemeral map from JSScript* to Debugger.Script instances. */
    typedef DebuggerWeakMap<JSScript*> ScriptWeakMap;
    ScriptWeakMap scripts;

    using LazyScriptWeakMap = DebuggerWeakMap<LazyScript*>;
    LazyScriptWeakMap lazyScripts;

    using LazyScriptVector = JS::GCVector<LazyScript*>;

    // The map from debuggee source script objects to their Debugger.Source
    // instances.
    typedef DebuggerWeakMap<JSObject*, true> SourceWeakMap;
    SourceWeakMap sources;

    // The map from debuggee objects to their Debugger.Object instances.
    typedef DebuggerWeakMap<JSObject*> ObjectWeakMap;
    ObjectWeakMap objects;

    // The map from debuggee Envs to Debugger.Environment instances.
    ObjectWeakMap environments;

    // The map from WasmInstanceObjects to synthesized Debugger.Script
    // instances.
    typedef DebuggerWeakMap<WasmInstanceObject*> WasmInstanceWeakMap;
    WasmInstanceWeakMap wasmInstanceScripts;

    // The map from WasmInstanceObjects to synthesized Debugger.Source
    // instances.
    WasmInstanceWeakMap wasmInstanceSources;

    // Keep track of tracelogger last drained identifiers to know if there are
    // lost events.
#ifdef NIGHTLY_BUILD
    uint32_t traceLoggerLastDrainedSize;
    uint32_t traceLoggerLastDrainedIteration;
#endif
    uint32_t traceLoggerScriptedCallsLastDrainedSize;
    uint32_t traceLoggerScriptedCallsLastDrainedIteration;

    class QueryBase;
    class ScriptQuery;
    class SourceQuery;
    class ObjectQuery;

    MOZ_MUST_USE bool addDebuggeeGlobal(JSContext* cx, Handle<GlobalObject*> obj);
    void removeDebuggeeGlobal(FreeOp* fop, GlobalObject* global,
                              WeakGlobalObjectSet::Enum* debugEnum);

    enum class CallUncaughtExceptionHook {
        No,
        Yes
    };

    /*
     * Apply the resumption information in (resumeMode, vp) to `frame` in
     * anticipation of returning to the debuggee.
     *
     * This is the usual path for returning from the debugger to the debuggee
     * when we have a resumption value to apply. This does final checks on the
     * result value and exits the debugger's realm by calling `ar.reset()`.
     * Some hooks don't call this because they don't allow the debugger to
     * control resumption; those just call `ar.reset()` and return.
     */
    ResumeMode leaveDebugger(mozilla::Maybe<AutoRealm>& ar,
                             AbstractFramePtr frame,
                             const mozilla::Maybe<HandleValue>& maybeThisv,
                             CallUncaughtExceptionHook callHook,
                             ResumeMode resumeMode,
                             MutableHandleValue vp);

    /*
     * Report and clear the pending exception on ar.context, if any, and return
     * ResumeMode::Terminate.
     */
    ResumeMode reportUncaughtException(mozilla::Maybe<AutoRealm>& ar);

    /*
     * Cope with an error or exception in a debugger hook.
     *
     * If callHook is true, then call the uncaughtExceptionHook, if any. If, in
     * addition, vp is given, then parse the value returned by
     * uncaughtExceptionHook as a resumption value.
     *
     * If there is no uncaughtExceptionHook, or if it fails, report and clear
     * the pending exception on ar.context and return ResumeMode::Terminate.
     *
     * This always calls `ar.reset()`; ar is a parameter because this method
     * must do some things in the debugger realm and some things in the
     * debuggee realm.
     */
    ResumeMode handleUncaughtException(mozilla::Maybe<AutoRealm>& ar);
    ResumeMode handleUncaughtException(mozilla::Maybe<AutoRealm>& ar,
                                       MutableHandleValue vp,
                                       const mozilla::Maybe<HandleValue>& thisVForCheck = mozilla::Nothing(),
                                       AbstractFramePtr frame = NullFramePtr());

    ResumeMode handleUncaughtExceptionHelper(mozilla::Maybe<AutoRealm>& ar,
                                             MutableHandleValue* vp,
                                             const mozilla::Maybe<HandleValue>& thisVForCheck,
                                             AbstractFramePtr frame);

    /*
     * Handle the result of a hook that is expected to return a resumption
     * value <https://wiki.mozilla.org/Debugger#Resumption_Values>. This is
     * called when we return from a debugging hook to debuggee code. The
     * interpreter wants a (ResumeMode, Value) pair telling it how to proceed.
     *
     * Precondition: ar is entered. We are in the debugger compartment.
     *
     * Postcondition: This called ar.reset(). See handleUncaughtException.
     *
     * If `success` is false, the hook failed. If an exception is pending in
     * ar.context(), return handleUncaughtException(ar, vp, callhook).
     * Otherwise just return ResumeMode::Terminate.
     *
     * If `success` is true, there must be no exception pending in ar.context().
     * `rv` may be:
     *
     *     undefined - Return `ResumeMode::Continue` to continue execution
     *         normally.
     *
     *     {return: value} or {throw: value} - Call unwrapDebuggeeValue to
     *         unwrap `value`. Store the result in `*vp` and return
     *         `ResumeMode::Return` or `ResumeMode::Throw`. The interpreter
     *         will force the current frame to return or throw an exception.
     *
     *     null - Return `ResumeMode::Terminate` to terminate the debuggee with
     *         an uncatchable error.
     *
     *     anything else - Make a new TypeError the pending exception and
     *         return handleUncaughtException(ar, vp, callHook).
     */
    ResumeMode processHandlerResult(mozilla::Maybe<AutoRealm>& ar, bool success, const Value& rv,
                                    AbstractFramePtr frame, jsbytecode* pc, MutableHandleValue vp);

    ResumeMode processParsedHandlerResult(mozilla::Maybe<AutoRealm>& ar,
                                          AbstractFramePtr frame, jsbytecode* pc,
                                          bool success, ResumeMode resumeMode,
                                          MutableHandleValue vp);

    GlobalObject* unwrapDebuggeeArgument(JSContext* cx, const Value& v);

    static void traceObject(JSTracer* trc, JSObject* obj);

    void trace(JSTracer* trc);
    friend struct js::GCManagedDeletePolicy<Debugger>;

    void traceForMovingGC(JSTracer* trc);
    void traceCrossCompartmentEdges(JSTracer* tracer);

    static const ClassOps classOps_;

  public:
    static const Class class_;

  private:
    static MOZ_MUST_USE bool getHookImpl(JSContext* cx, CallArgs& args, Debugger& dbg, Hook which);
    static MOZ_MUST_USE bool setHookImpl(JSContext* cx, CallArgs& args, Debugger& dbg, Hook which);

    static bool getEnabled(JSContext* cx, unsigned argc, Value* vp);
    static bool setEnabled(JSContext* cx, unsigned argc, Value* vp);
    static bool getOnDebuggerStatement(JSContext* cx, unsigned argc, Value* vp);
    static bool setOnDebuggerStatement(JSContext* cx, unsigned argc, Value* vp);
    static bool getOnExceptionUnwind(JSContext* cx, unsigned argc, Value* vp);
    static bool setOnExceptionUnwind(JSContext* cx, unsigned argc, Value* vp);
    static bool getOnNewScript(JSContext* cx, unsigned argc, Value* vp);
    static bool setOnNewScript(JSContext* cx, unsigned argc, Value* vp);
    static bool getOnEnterFrame(JSContext* cx, unsigned argc, Value* vp);
    static bool setOnEnterFrame(JSContext* cx, unsigned argc, Value* vp);
    static bool getOnNewGlobalObject(JSContext* cx, unsigned argc, Value* vp);
    static bool setOnNewGlobalObject(JSContext* cx, unsigned argc, Value* vp);
    static bool getOnNewPromise(JSContext* cx, unsigned argc, Value* vp);
    static bool setOnNewPromise(JSContext* cx, unsigned argc, Value* vp);
    static bool getOnPromiseSettled(JSContext* cx, unsigned argc, Value* vp);
    static bool setOnPromiseSettled(JSContext* cx, unsigned argc, Value* vp);
    static bool getUncaughtExceptionHook(JSContext* cx, unsigned argc, Value* vp);
    static bool setUncaughtExceptionHook(JSContext* cx, unsigned argc, Value* vp);
    static bool getAllowUnobservedAsmJS(JSContext* cx, unsigned argc, Value* vp);
    static bool setAllowUnobservedAsmJS(JSContext* cx, unsigned argc, Value* vp);
    static bool getCollectCoverageInfo(JSContext* cx, unsigned argc, Value* vp);
    static bool setCollectCoverageInfo(JSContext* cx, unsigned argc, Value* vp);
    static bool getMemory(JSContext* cx, unsigned argc, Value* vp);
    static bool addDebuggee(JSContext* cx, unsigned argc, Value* vp);
    static bool addAllGlobalsAsDebuggees(JSContext* cx, unsigned argc, Value* vp);
    static bool removeDebuggee(JSContext* cx, unsigned argc, Value* vp);
    static bool removeAllDebuggees(JSContext* cx, unsigned argc, Value* vp);
    static bool hasDebuggee(JSContext* cx, unsigned argc, Value* vp);
    static bool getDebuggees(JSContext* cx, unsigned argc, Value* vp);
    static bool getNewestFrame(JSContext* cx, unsigned argc, Value* vp);
    static bool clearAllBreakpoints(JSContext* cx, unsigned argc, Value* vp);
    static bool findScripts(JSContext* cx, unsigned argc, Value* vp);
    static bool findSources(JSContext* cx, unsigned argc, Value* vp);
    static bool findObjects(JSContext* cx, unsigned argc, Value* vp);
    static bool findAllGlobals(JSContext* cx, unsigned argc, Value* vp);
    static bool makeGlobalObjectReference(JSContext* cx, unsigned argc, Value* vp);
    static bool setupTraceLoggerScriptCalls(JSContext* cx, unsigned argc, Value* vp);
    static bool drainTraceLoggerScriptCalls(JSContext* cx, unsigned argc, Value* vp);
    static bool startTraceLogger(JSContext* cx, unsigned argc, Value* vp);
    static bool endTraceLogger(JSContext* cx, unsigned argc, Value* vp);
    static bool isCompilableUnit(JSContext* cx, unsigned argc, Value* vp);
    static bool recordReplayProcessKind(JSContext* cx, unsigned argc, Value* vp);
#ifdef NIGHTLY_BUILD
    static bool setupTraceLogger(JSContext* cx, unsigned argc, Value* vp);
    static bool drainTraceLogger(JSContext* cx, unsigned argc, Value* vp);
#endif
    static bool adoptDebuggeeValue(JSContext* cx, unsigned argc, Value* vp);
    static bool construct(JSContext* cx, unsigned argc, Value* vp);
    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];
    static const JSFunctionSpec static_methods[];

    static void removeFromFrameMapsAndClearBreakpointsIn(JSContext* cx, AbstractFramePtr frame,
                                                         bool suspending = false);
    static bool updateExecutionObservabilityOfFrames(JSContext* cx, const ExecutionObservableSet& obs,
                                                     IsObserving observing);
    static bool updateExecutionObservabilityOfScripts(JSContext* cx, const ExecutionObservableSet& obs,
                                                      IsObserving observing);
    static bool updateExecutionObservability(JSContext* cx, ExecutionObservableSet& obs,
                                             IsObserving observing);

    template <typename FrameFn /* void (DebuggerFrame*) */>
    static void forEachDebuggerFrame(AbstractFramePtr frame, FrameFn fn);

    /*
     * Return a vector containing all Debugger.Frame instances referring to
     * |frame|. |global| is |frame|'s global object; if nullptr or omitted, we
     * compute it ourselves from |frame|.
     */
    using DebuggerFrameVector = GCVector<DebuggerFrame*>;
    static MOZ_MUST_USE bool getDebuggerFrames(AbstractFramePtr frame,
                                               MutableHandle<DebuggerFrameVector> frames);

  public:
    static MOZ_MUST_USE bool ensureExecutionObservabilityOfOsrFrame(JSContext* cx,
                                                                    InterpreterFrame* frame);

    // Public for DebuggerScript_setBreakpoint.
    static MOZ_MUST_USE bool ensureExecutionObservabilityOfScript(JSContext* cx, JSScript* script);

    // Whether the Debugger instance needs to observe all non-AOT JS
    // execution of its debugees.
    IsObserving observesAllExecution() const;

    // Whether the Debugger instance needs to observe AOT-compiled asm.js
    // execution of its debuggees.
    IsObserving observesAsmJS() const;

    // Whether the Debugger instance needs to observe coverage of any JavaScript
    // execution.
    IsObserving observesCoverage() const;

  private:
    static MOZ_MUST_USE bool ensureExecutionObservabilityOfFrame(JSContext* cx,
                                                                 AbstractFramePtr frame);
    static MOZ_MUST_USE bool ensureExecutionObservabilityOfRealm(JSContext* cx,
                                                                 JS::Realm* realm);

    static bool hookObservesAllExecution(Hook which);

    MOZ_MUST_USE bool updateObservesAllExecutionOnDebuggees(JSContext* cx, IsObserving observing);
    MOZ_MUST_USE bool updateObservesCoverageOnDebuggees(JSContext* cx, IsObserving observing);
    void updateObservesAsmJSOnDebuggees(IsObserving observing);

    JSObject* getHook(Hook hook) const;
    bool hasAnyLiveHooks(JSRuntime* rt) const;

    static MOZ_MUST_USE bool slowPathCheckNoExecute(JSContext* cx, HandleScript script);
    static ResumeMode slowPathOnEnterFrame(JSContext* cx, AbstractFramePtr frame);
    static ResumeMode slowPathOnResumeFrame(JSContext* cx, AbstractFramePtr frame);
    static MOZ_MUST_USE bool slowPathOnLeaveFrame(JSContext* cx, AbstractFramePtr frame,
                                                  jsbytecode* pc, bool ok);
    static MOZ_MUST_USE bool slowPathOnNewGenerator(JSContext* cx, AbstractFramePtr frame,
                                                    Handle<GeneratorObject*> genObj);
    static ResumeMode slowPathOnDebuggerStatement(JSContext* cx, AbstractFramePtr frame);
    static ResumeMode slowPathOnExceptionUnwind(JSContext* cx, AbstractFramePtr frame);
    static void slowPathOnNewScript(JSContext* cx, HandleScript script);
    static void slowPathOnNewWasmInstance(JSContext* cx, Handle<WasmInstanceObject*> wasmInstance);
    static void slowPathOnNewGlobalObject(JSContext* cx, Handle<GlobalObject*> global);
    static MOZ_MUST_USE bool slowPathOnLogAllocationSite(JSContext* cx, HandleObject obj,
                                                         HandleSavedFrame frame,
                                                         mozilla::TimeStamp when,
                                                         GlobalObject::DebuggerVector& dbgs);
    static void slowPathPromiseHook(JSContext* cx, Hook hook, Handle<PromiseObject*> promise);

    template <typename HookIsEnabledFun /* bool (Debugger*) */,
              typename FireHookFun /* ResumeMode (Debugger*) */>
    static ResumeMode dispatchHook(JSContext* cx, HookIsEnabledFun hookIsEnabled,
                                   FireHookFun fireHook);

    ResumeMode fireDebuggerStatement(JSContext* cx, MutableHandleValue vp);
    ResumeMode fireExceptionUnwind(JSContext* cx, MutableHandleValue vp);
    ResumeMode fireEnterFrame(JSContext* cx, MutableHandleValue vp);
    ResumeMode fireNewGlobalObject(JSContext* cx, Handle<GlobalObject*> global, MutableHandleValue vp);
    ResumeMode firePromiseHook(JSContext* cx, Hook hook, HandleObject promise, MutableHandleValue vp);

    NativeObject* newVariantWrapper(JSContext* cx, Handle<DebuggerScriptReferent> referent) {
        return newDebuggerScript(cx, referent);
    }
    NativeObject* newVariantWrapper(JSContext* cx, Handle<DebuggerSourceReferent> referent) {
        return newDebuggerSource(cx, referent);
    }

    /*
     * Helper function to help wrap Debugger objects whose referents may be
     * variants. Currently Debugger.Script and Debugger.Source referents may
     * be variants.
     *
     * Prefer using wrapScript, wrapWasmScript, wrapSource, and wrapWasmSource
     * whenever possible.
     */
    template <typename ReferentVariant, typename Referent, typename Map>
    JSObject* wrapVariantReferent(JSContext* cx, Map& map, Handle<CrossCompartmentKey> key,
                                  Handle<ReferentVariant> referent);
    JSObject* wrapVariantReferent(JSContext* cx, Handle<DebuggerScriptReferent> referent);
    JSObject* wrapVariantReferent(JSContext* cx, Handle<DebuggerSourceReferent> referent);

    /*
     * Allocate and initialize a Debugger.Script instance whose referent is
     * |referent|.
     */
    NativeObject* newDebuggerScript(JSContext* cx, Handle<DebuggerScriptReferent> referent);

    /*
     * Allocate and initialize a Debugger.Source instance whose referent is
     * |referent|.
     */
    NativeObject* newDebuggerSource(JSContext* cx, Handle<DebuggerSourceReferent> referent);

    /*
     * Receive a "new script" event from the engine. A new script was compiled
     * or deserialized.
     */
    void fireNewScript(JSContext* cx, Handle<DebuggerScriptReferent> scriptReferent);

    /*
     * Receive a "garbage collection" event from the engine. A GC cycle with the
     * given data was recently completed.
     */
    void fireOnGarbageCollectionHook(JSContext* cx,
                                     const JS::dbg::GarbageCollectionEvent::Ptr& gcData);

    inline Breakpoint* firstBreakpoint() const;

    static MOZ_MUST_USE bool replaceFrameGuts(JSContext* cx, AbstractFramePtr from,
                                              AbstractFramePtr to,
                                              ScriptFrameIter& iter);

  public:
    Debugger(JSContext* cx, NativeObject* dbg);
    ~Debugger();

    inline const js::GCPtrNativeObject& toJSObject() const;
    inline js::GCPtrNativeObject& toJSObjectRef();
    static inline Debugger* fromJSObject(const JSObject* obj);
    static Debugger* fromChildJSObject(JSObject* obj);

    Zone* zone() const { return toJSObject()->zone(); }

    bool hasMemory() const;
    DebuggerMemory& memory() const;

    WeakGlobalObjectSet::Range allDebuggees() const { return debuggees.all(); }

    /*** Methods for interaction with the GC. *******************************/

    /*
     * A Debugger object is live if:
     *   * the Debugger JSObject is live (Debugger::trace handles this case); OR
     *   * it is in the middle of dispatching an event (the event dispatching
     *     code roots it in this case); OR
     *   * it is enabled, and it is debugging at least one live compartment,
     *     and at least one of the following is true:
     *       - it has a debugger hook installed
     *       - it has a breakpoint set on a live script
     *       - it has a watchpoint set on a live object.
     *
     * Debugger::markIteratively handles the last case. If it finds any Debugger
     * objects that are definitely live but not yet marked, it marks them and
     * returns true. If not, it returns false.
     */
    static void traceIncomingCrossCompartmentEdges(JSTracer* tracer);
    static MOZ_MUST_USE bool markIteratively(GCMarker* marker);
    static void traceAllForMovingGC(JSTracer* trc);
    static void sweepAll(FreeOp* fop);
    static void detachAllDebuggersFromGlobal(FreeOp* fop, GlobalObject* global);
    static void findZoneEdges(JS::Zone* v, gc::ZoneComponentFinder& finder);
#ifdef DEBUG
    static bool isDebuggerCrossCompartmentEdge(JSObject* obj, const js::gc::Cell* cell);
#endif

    // Checks it the current compartment is allowed to execute code.
    static inline MOZ_MUST_USE bool checkNoExecute(JSContext* cx, HandleScript script);

    /*
     * Announce to the debugger that the context has entered a new JavaScript
     * frame, |frame|. Call whatever hooks have been registered to observe new
     * frames.
     */
    static inline ResumeMode onEnterFrame(JSContext* cx, AbstractFramePtr frame);

    /*
     * Like onEnterFrame, but for resuming execution of a generator or async
     * function. `frame` is a new baseline or interpreter frame, but abstractly
     * it can be identified with a particular generator frame that was
     * suspended earlier.
     *
     * There is no separate user-visible Debugger.onResumeFrame hook; this
     * fires .onEnterFrame (again, since we're re-entering the frame).
     *
     * Unfortunately, the interpreter and the baseline JIT arrange for this to
     * be called in different ways. The interpreter calls it from JSOP_RESUME,
     * immediately after pushing the resumed frame; the JIT calls it from
     * JSOP_DEBUGAFTERYIELD, just after the generator resumes. The difference
     * should not be user-visible.
     */
    static inline ResumeMode onResumeFrame(JSContext* cx, AbstractFramePtr frame);

    /*
     * Announce to the debugger a |debugger;| statement on has been
     * encountered on the youngest JS frame on |cx|. Call whatever hooks have
     * been registered to observe this.
     *
     * Note that this method is called for all |debugger;| statements,
     * regardless of the frame's debuggee-ness.
     */
    static inline ResumeMode onDebuggerStatement(JSContext* cx, AbstractFramePtr frame);

    /*
     * Announce to the debugger that an exception has been thrown and propagated
     * to |frame|. Call whatever hooks have been registered to observe this.
     */
    static inline ResumeMode onExceptionUnwind(JSContext* cx, AbstractFramePtr frame);

    /*
     * Announce to the debugger that the thread has exited a JavaScript frame, |frame|.
     * If |ok| is true, the frame is returning normally; if |ok| is false, the frame
     * is throwing an exception or terminating.
     *
     * Change cx's current exception and |frame|'s return value to reflect the changes
     * in behavior the hooks request, if any. Return the new error/success value.
     *
     * This function may be called twice for the same outgoing frame; only the
     * first call has any effect. (Permitting double calls simplifies some
     * cases where an onPop handler's resumption value changes a return to a
     * throw, or vice versa: we can redirect to a complete copy of the
     * alternative path, containing its own call to onLeaveFrame.)
     */
    static inline MOZ_MUST_USE bool onLeaveFrame(JSContext* cx, AbstractFramePtr frame,
                                                 jsbytecode* pc, bool ok);

    /*
     * Announce to the debugger that a generator object has been created,
     * via JSOP_GENERATOR.
     *
     * This does not fire user hooks, but it's needed for debugger bookkeeping.
     */
    static inline MOZ_MUST_USE bool onNewGenerator(JSContext* cx, AbstractFramePtr frame,
                                                   Handle<GeneratorObject*> genObj);

    static inline void onNewScript(JSContext* cx, HandleScript script);
    static inline void onNewWasmInstance(JSContext* cx, Handle<WasmInstanceObject*> wasmInstance);
    static inline void onNewGlobalObject(JSContext* cx, Handle<GlobalObject*> global);
    static inline MOZ_MUST_USE bool onLogAllocationSite(JSContext* cx, JSObject* obj,
                                                        HandleSavedFrame frame, mozilla::TimeStamp when);
    static ResumeMode onTrap(JSContext* cx, MutableHandleValue vp);
    static ResumeMode onSingleStep(JSContext* cx, MutableHandleValue vp);
    static MOZ_MUST_USE bool handleBaselineOsr(JSContext* cx, InterpreterFrame* from,
                                               jit::BaselineFrame* to);
    static MOZ_MUST_USE bool handleIonBailout(JSContext* cx, jit::RematerializedFrame* from,
                                              jit::BaselineFrame* to);
    static void handleUnrecoverableIonBailoutError(JSContext* cx, jit::RematerializedFrame* frame);
    static void propagateForcedReturn(JSContext* cx, AbstractFramePtr frame, HandleValue rval);
    static bool hasLiveHook(GlobalObject* global, Hook which);
    static bool inFrameMaps(AbstractFramePtr frame);

    // Notify any Debugger instances observing this promise's global that a new
    // promise was allocated.
    static inline void onNewPromise(JSContext* cx, Handle<PromiseObject*> promise);

    // Notify any Debugger instances observing this promise's global that the
    // promise has settled (ie, it has either been fulfilled or rejected). Note that
    // this is *not* equivalent to the promise resolution (ie, the promise's fate
    // getting locked in) because you can resolve a promise with another pending
    // promise, in which case neither promise has settled yet.
    //
    // This should never be called on the same promise more than once, because a
    // promise can only make the transition from unsettled to settled once.
    static inline void onPromiseSettled(JSContext* cx, Handle<PromiseObject*> promise);

    /*** Functions for use by Debugger.cpp. *********************************/

    inline bool observesEnterFrame() const;
    inline bool observesNewScript() const;
    inline bool observesNewGlobalObject() const;
    inline bool observesGlobal(GlobalObject* global) const;
    bool observesFrame(AbstractFramePtr frame) const;
    bool observesFrame(const FrameIter& iter) const;
    bool observesScript(JSScript* script) const;
    bool observesWasm(wasm::Instance* instance) const;

    /*
     * If env is nullptr, call vp->setNull() and return true. Otherwise, find
     * or create a Debugger.Environment object for the given Env. On success,
     * store the Environment object in *vp and return true.
     */
    MOZ_MUST_USE bool wrapEnvironment(JSContext* cx, Handle<Env*> env, MutableHandleValue vp);
    MOZ_MUST_USE bool wrapEnvironment(JSContext* cx, Handle<Env*> env,
                                      MutableHandleDebuggerEnvironment result);

    /*
     * Like cx->compartment()->wrap(cx, vp), but for the debugger realm.
     *
     * Preconditions: *vp is a value from a debuggee realm; cx is in the
     * debugger's compartment.
     *
     * If *vp is an object, this produces a (new or existing) Debugger.Object
     * wrapper for it. Otherwise this is the same as Compartment::wrap.
     *
     * If *vp is a magic JS_OPTIMIZED_OUT value, this produces a plain object
     * of the form { optimizedOut: true }.
     *
     * If *vp is a magic JS_OPTIMIZED_ARGUMENTS value signifying missing
     * arguments, this produces a plain object of the form { missingArguments:
     * true }.
     *
     * If *vp is a magic JS_UNINITIALIZED_LEXICAL value signifying an
     * unaccessible uninitialized binding, this produces a plain object of the
     * form { uninitialized: true }.
     */
    MOZ_MUST_USE bool wrapDebuggeeValue(JSContext* cx, MutableHandleValue vp);
    MOZ_MUST_USE bool wrapDebuggeeObject(JSContext* cx, HandleObject obj,
                                         MutableHandleDebuggerObject result);

    /*
     * Unwrap a Debug.Object, without rewrapping it for any particular debuggee
     * compartment.
     *
     * Preconditions: cx is in the debugger compartment. *vp is a value in that
     * compartment. (*vp should be a "debuggee value", meaning it is the
     * debugger's reflection of a value in the debuggee.)
     *
     * If *vp is a Debugger.Object, store the referent in *vp. Otherwise, if *vp
     * is an object, throw a TypeError, because it is not a debuggee
     * value. Otherwise *vp is a primitive, so leave it alone.
     *
     * When passing values from the debuggee to the debugger:
     *     enter debugger compartment;
     *     call wrapDebuggeeValue;  // compartment- and debugger-wrapping
     *
     * When passing values from the debugger to the debuggee:
     *     call unwrapDebuggeeValue;  // debugger-unwrapping
     *     enter debuggee realm;
     *     call cx->compartment()->wrap;  // compartment-rewrapping
     *
     * (Extreme nerd sidebar: Unwrapping happens in two steps because there are
     * two different kinds of symmetry at work: regardless of which direction
     * we're going, we want any exceptions to be created and thrown in the
     * debugger compartment--mirror symmetry. But compartment wrapping always
     * happens in the target compartment--rotational symmetry.)
     */
    MOZ_MUST_USE bool unwrapDebuggeeValue(JSContext* cx, MutableHandleValue vp);
    MOZ_MUST_USE bool unwrapDebuggeeObject(JSContext* cx, MutableHandleObject obj);
    MOZ_MUST_USE bool unwrapPropertyDescriptor(JSContext* cx, HandleObject obj,
                                               MutableHandle<PropertyDescriptor> desc);

    /*
     * Store the Debugger.Frame object for iter in *vp/result.
     *
     * If this Debugger does not already have a Frame object for the frame
     * `iter` points to, a new Frame object is created, and `iter`'s private
     * data is copied into it.
     */
    MOZ_MUST_USE bool getFrame(JSContext* cx, const FrameIter& iter,
                               MutableHandleValue vp);
    MOZ_MUST_USE bool getFrame(JSContext* cx, const FrameIter& iter,
                               MutableHandleDebuggerFrame result);

    /*
     * Set |*resumeMode| and |*value| to a (ResumeMode, Value) pair reflecting a
     * standard SpiderMonkey call state: a boolean success value |ok|, a return
     * value |rv|, and a context |cx| that may or may not have an exception set.
     * If an exception was pending on |cx|, it is cleared (and |ok| is asserted
     * to be false).
     */
    static void resultToCompletion(JSContext* cx, bool ok, const Value& rv,
                                   ResumeMode* resumeMode, MutableHandleValue value);

    /*
     * Set |*result| to a JavaScript completion value corresponding to |resumeMode|
     * and |value|. |value| should be the return value or exception value, not
     * wrapped as a debuggee value. |cx| must be in the debugger compartment.
     */
    MOZ_MUST_USE bool newCompletionValue(JSContext* cx, ResumeMode resumeMode, const Value& value,
                                         MutableHandleValue result);

    /*
     * Precondition: we are in the debuggee realm (ar is entered) and ok is true
     * if the operation in the debuggee realm succeeded, false on error or
     * exception.
     *
     * Postcondition: we are in the debugger realm, having called `ar.reset()`
     * even if an error occurred.
     *
     * On success, a completion value is in vp and ar.context does not have a
     * pending exception. (This ordinarily returns true even if the ok argument
     * is false.)
     */
    MOZ_MUST_USE bool receiveCompletionValue(mozilla::Maybe<AutoRealm>& ar, bool ok,
                                             HandleValue val,
                                             MutableHandleValue vp);

    /*
     * Return the Debugger.Script object for |script|, or create a new one if
     * needed. The context |cx| must be in the debugger realm; |script| must be
     * a script in a debuggee realm.
     */
    JSObject* wrapScript(JSContext* cx, HandleScript script);

    JSObject* wrapLazyScript(JSContext* cx, Handle<LazyScript*> script);

    /*
     * Return the Debugger.Script object for |wasmInstance| (the toplevel
     * script), synthesizing a new one if needed. The context |cx| must be in
     * the debugger compartment; |wasmInstance| must be a WasmInstanceObject in
     * the debuggee realm.
     */
    JSObject* wrapWasmScript(JSContext* cx, Handle<WasmInstanceObject*> wasmInstance);

    /*
     * Return the Debugger.Source object for |source|, or create a new one if
     * needed. The context |cx| must be in the debugger compartment; |source|
     * must be a script source object in a debuggee realm.
     */
    JSObject* wrapSource(JSContext* cx, js::HandleScriptSourceObject source);

    /*
     * Return the Debugger.Source object for |wasmInstance| (the entire module),
     * synthesizing a new one if needed. The context |cx| must be in the
     * debugger compartment; |wasmInstance| must be a WasmInstanceObject in the
     * debuggee realm.
     */
    JSObject* wrapWasmSource(JSContext* cx, Handle<WasmInstanceObject*> wasmInstance);

    /*
     * Add a link between the given generator object and a Debugger.Frame
     * object.  This link is used to make sure the same Debugger.Frame stays
     * associated with a given generator object (or async function activation),
     * even while it is suspended and removed from the stack.
     *
     * The context `cx` and `frameObj` must be in the debugger realm, and
     * `genObj` must be in a debuggee realm.
     *
     * `frameObj` must be this `Debugger`'s debug wrapper for the generator or
     * async function call associated with `genObj`. This activation may
     * or may not actually be on the stack right now.
     */
    MOZ_MUST_USE bool addGeneratorFrame(JSContext* cx,
                                        Handle<GeneratorObject*> genObj,
                                        HandleDebuggerFrame frameObj);

  private:
    Debugger(const Debugger&) = delete;
    Debugger & operator=(const Debugger&) = delete;
};

enum class DebuggerEnvironmentType {
    Declarative,
    With,
    Object
};

class DebuggerEnvironment : public NativeObject
{
  public:
    enum {
        OWNER_SLOT
    };

    static const unsigned RESERVED_SLOTS = 1;

    static const Class class_;

    static NativeObject* initClass(JSContext* cx, HandleObject dbgCtor,
                                   Handle<GlobalObject*> global);
    static DebuggerEnvironment* create(JSContext* cx, HandleObject proto, HandleObject referent,
                                       HandleNativeObject debugger);

    DebuggerEnvironmentType type() const;
    MOZ_MUST_USE bool getParent(JSContext* cx, MutableHandleDebuggerEnvironment result) const;
    MOZ_MUST_USE bool getObject(JSContext* cx, MutableHandleDebuggerObject result) const;
    MOZ_MUST_USE bool getCallee(JSContext* cx, MutableHandleDebuggerObject result) const;
    bool isDebuggee() const;
    bool isOptimized() const;

    static MOZ_MUST_USE bool getNames(JSContext* cx, HandleDebuggerEnvironment environment,
                                      MutableHandle<IdVector> result);
    static MOZ_MUST_USE bool find(JSContext* cx, HandleDebuggerEnvironment environment,
                                  HandleId id, MutableHandleDebuggerEnvironment result);
    static MOZ_MUST_USE bool getVariable(JSContext* cx, HandleDebuggerEnvironment environment,
                                         HandleId id, MutableHandleValue result);
    static MOZ_MUST_USE bool setVariable(JSContext* cx, HandleDebuggerEnvironment environment,
                                         HandleId id, HandleValue value);

  private:
    static const ClassOps classOps_;

    static const JSPropertySpec properties_[];
    static const JSFunctionSpec methods_[];

    Env* referent() const {
        Env* env = static_cast<Env*>(getPrivate());
        MOZ_ASSERT(env);
        return env;
    }

    Debugger* owner() const;

    bool requireDebuggee(JSContext* cx) const;

    static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);

    static MOZ_MUST_USE bool typeGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool parentGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool objectGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool calleeGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool inspectableGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool optimizedOutGetter(JSContext* cx, unsigned argc, Value* vp);

    static MOZ_MUST_USE bool namesMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool findMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool getVariableMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool setVariableMethod(JSContext* cx, unsigned argc, Value* vp);
};

enum class DebuggerFrameType {
    Eval,
    Global,
    Call,
    Module,
    WasmCall
};

enum class DebuggerFrameImplementation {
    Interpreter,
    Baseline,
    Ion,
    Wasm
};

/*
 * A Handler represents a reference to a handler function. These handler
 * functions are called by the Debugger API to notify the user of certain
 * events. For each event type, we define a separate subclass of Handler. This
 * allows users to define a single reference to an object that implements
 * multiple handlers, by inheriting from the appropriate subclasses.
 *
 * A Handler can be stored on a reflection object, in which case the reflection
 * object becomes responsible for managing the lifetime of the Handler. To aid
 * with this, the Handler base class defines several methods, which are to be
 * called by the reflection object at the appropriate time (see below).
 */
struct Handler {
    virtual ~Handler() {}

    /*
     * If the Handler is a reference to a callable JSObject, this method returns
     * the latter. This allows the Handler to be used from JS. Otherwise, this
     * method returns nullptr.
     */
    virtual JSObject* object() const = 0;

    /*
     * Drops the reference to the handler. This method will be called by the
     * reflection object on which the reference is stored when the former is
     * finalized, or the latter replaced.
     */
    virtual void drop() = 0;

    /*
     * Traces the reference to the handler. This method will be called
     * by the reflection object on which the reference is stored whenever the
     * former is traced.
     */
    virtual void trace(JSTracer* tracer) = 0;
};

class DebuggerArguments : public NativeObject {
  public:
    static const Class class_;

    static DebuggerArguments* create(JSContext* cx, HandleObject proto, HandleDebuggerFrame frame);

  private:
    enum {
        FRAME_SLOT
    };

    static const unsigned RESERVED_SLOTS = 1;
};

/*
 * An OnStepHandler represents a handler function that is called when a small
 * amount of progress is made in a frame.
 */
struct OnStepHandler : Handler {
    /*
     * If we have made a small amount of progress in a frame, this method is
     * called with the frame as argument. If succesful, this method should
     * return true, with `resumeMode` and `vp` set to a resumption value
     * specifiying how execution should continue.
     */
    virtual bool onStep(JSContext* cx, HandleDebuggerFrame frame, ResumeMode& resumeMode,
                        MutableHandleValue vp) = 0;
};

class ScriptedOnStepHandler final : public OnStepHandler {
  public:
    explicit ScriptedOnStepHandler(JSObject* object);
    virtual JSObject* object() const override;
    virtual void drop() override;
    virtual void trace(JSTracer* tracer) override;
    virtual bool onStep(JSContext* cx, HandleDebuggerFrame frame, ResumeMode& resumeMode,
                        MutableHandleValue vp) override;

  private:
    HeapPtr<JSObject*> object_;
};

/*
 * An OnPopHandler represents a handler function that is called just before a
 * frame is popped.
 */
struct OnPopHandler : Handler {
    /*
     * If a frame is about the be popped, this method is called with the frame
     * as argument, and `resumeMode` and `vp` set to a completion value specifying
     * how this frame's execution completed. If successful, this method should
     * return true, with `resumeMode` and `vp` set to a resumption value specifying
     * how execution should continue.
     */
    virtual bool onPop(JSContext* cx, HandleDebuggerFrame frame, ResumeMode& resumeMode,
                       MutableHandleValue vp) = 0;
};

class ScriptedOnPopHandler final : public OnPopHandler {
  public:
    explicit ScriptedOnPopHandler(JSObject* object);
    virtual JSObject* object() const override;
    virtual void drop() override;
    virtual void trace(JSTracer* tracer) override;
    virtual bool onPop(JSContext* cx, HandleDebuggerFrame frame, ResumeMode& resumeMode,
                       MutableHandleValue vp) override;

  private:
    HeapPtr<JSObject*> object_;
};

class DebuggerFrame : public NativeObject
{
    friend class DebuggerArguments;
    friend class ScriptedOnStepHandler;
    friend class ScriptedOnPopHandler;

  public:
    static const Class class_;

    static NativeObject* initClass(JSContext* cx, HandleObject dbgCtor,
                                   Handle<GlobalObject*> global);
    static DebuggerFrame* create(JSContext* cx, HandleObject proto, const FrameIter& iter,
                                 HandleNativeObject debugger);
    void freeFrameIterData(FreeOp* fop);

    static MOZ_MUST_USE bool getArguments(JSContext* cx, HandleDebuggerFrame frame,
                                          MutableHandleDebuggerArguments result);
    static MOZ_MUST_USE bool getCallee(JSContext* cx, HandleDebuggerFrame frame,
                                       MutableHandleDebuggerObject result);
    static MOZ_MUST_USE bool getIsConstructing(JSContext* cx, HandleDebuggerFrame frame,
                                               bool& result);
    static MOZ_MUST_USE bool getEnvironment(JSContext* cx, HandleDebuggerFrame frame,
                                            MutableHandleDebuggerEnvironment result);
    static bool getIsGenerator(HandleDebuggerFrame frame);
    static MOZ_MUST_USE bool getOffset(JSContext* cx, HandleDebuggerFrame frame, size_t& result);
    static MOZ_MUST_USE bool getOlder(JSContext* cx, HandleDebuggerFrame frame,
                                      MutableHandleDebuggerFrame result);
    static MOZ_MUST_USE bool getThis(JSContext* cx, HandleDebuggerFrame frame,
                                     MutableHandleValue result);
    static DebuggerFrameType getType(HandleDebuggerFrame frame);
    static DebuggerFrameImplementation getImplementation(HandleDebuggerFrame frame);
    static MOZ_MUST_USE bool setOnStepHandler(JSContext* cx, HandleDebuggerFrame frame,
                                              OnStepHandler* handler);

    static MOZ_MUST_USE bool eval(JSContext* cx, HandleDebuggerFrame frame,
                                  mozilla::Range<const char16_t> chars, HandleObject bindings,
                                  const EvalOptions& options, ResumeMode& resumeMode,
                                  MutableHandleValue value);

    bool isLive() const;
    OnStepHandler* onStepHandler() const;
    OnPopHandler* onPopHandler() const;
    void setOnPopHandler(OnPopHandler* handler);

    /*
     * Called after a generator/async frame is resumed, before exposing this
     * Debugger.Frame object to any hooks.
     */
    bool resume(const FrameIter& iter);

    bool hasAnyLiveHooks() const;

  private:
    static const ClassOps classOps_;

    static const JSPropertySpec properties_[];
    static const JSFunctionSpec methods_[];

    static AbstractFramePtr getReferent(HandleDebuggerFrame frame);
    static MOZ_MUST_USE bool getFrameIter(JSContext* cx, HandleDebuggerFrame frame,
                                          mozilla::Maybe<FrameIter>& result);
    static MOZ_MUST_USE bool requireScriptReferent(JSContext* cx, HandleDebuggerFrame frame);

    static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);

    static MOZ_MUST_USE bool argumentsGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool calleeGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool constructingGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool environmentGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool generatorGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool liveGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool offsetGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool olderGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool thisGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool typeGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool implementationGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool onStepGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool onStepSetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool onPopGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool onPopSetter(JSContext* cx, unsigned argc, Value* vp);

    static MOZ_MUST_USE bool evalMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool evalWithBindingsMethod(JSContext* cx, unsigned argc, Value* vp);

    Debugger* owner() const;
  public:
    FrameIter::Data* frameIterData() const;
};

class DebuggerObject : public NativeObject
{
  public:
    static const Class class_;

    static NativeObject* initClass(JSContext* cx, Handle<GlobalObject*> global,
                                   HandleObject debugCtor);
    static DebuggerObject* create(JSContext* cx, HandleObject proto, HandleObject obj,
                                  HandleNativeObject debugger);

    // Properties
    static MOZ_MUST_USE bool getClassName(JSContext* cx, HandleDebuggerObject object,
                                          MutableHandleString result);
    static MOZ_MUST_USE bool getParameterNames(JSContext* cx, HandleDebuggerObject object,
                                               MutableHandle<StringVector> result);
    static MOZ_MUST_USE bool getBoundTargetFunction(JSContext* cx, HandleDebuggerObject object,
                                                 MutableHandleDebuggerObject result);
    static MOZ_MUST_USE bool getBoundThis(JSContext* cx, HandleDebuggerObject object,
                                          MutableHandleValue result);
    static MOZ_MUST_USE bool getBoundArguments(JSContext* cx, HandleDebuggerObject object,
                                               MutableHandle<ValueVector> result);
    static MOZ_MUST_USE bool getAllocationSite(JSContext* cx, HandleDebuggerObject object,
                                            MutableHandleObject result);
    static MOZ_MUST_USE bool getErrorMessageName(JSContext* cx, HandleDebuggerObject object,
                                                 MutableHandleString result);
    static MOZ_MUST_USE bool getErrorNotes(JSContext* cx, HandleDebuggerObject object,
                                           MutableHandleValue result);
    static MOZ_MUST_USE bool getErrorLineNumber(JSContext* cx, HandleDebuggerObject object,
                                                MutableHandleValue result);
    static MOZ_MUST_USE bool getErrorColumnNumber(JSContext* cx, HandleDebuggerObject object,
                                                  MutableHandleValue result);
    static MOZ_MUST_USE bool getScriptedProxyTarget(JSContext* cx, HandleDebuggerObject object,
                                                    MutableHandleDebuggerObject result);
    static MOZ_MUST_USE bool getScriptedProxyHandler(JSContext* cx, HandleDebuggerObject object,
                                                     MutableHandleDebuggerObject result);
    static MOZ_MUST_USE bool getPromiseValue(JSContext* cx, HandleDebuggerObject object,
                                             MutableHandleValue result);
    static MOZ_MUST_USE bool getPromiseReason(JSContext* cx, HandleDebuggerObject object,
                                              MutableHandleValue result);

    // Methods
    static MOZ_MUST_USE bool isExtensible(JSContext* cx, HandleDebuggerObject object,
                                          bool& result);
    static MOZ_MUST_USE bool isSealed(JSContext* cx, HandleDebuggerObject object, bool& result);
    static MOZ_MUST_USE bool isFrozen(JSContext* cx, HandleDebuggerObject object, bool& result);
    static MOZ_MUST_USE bool getProperty(JSContext* cx, HandleDebuggerObject object,
                                         HandleId id, MutableHandleValue result);
    static MOZ_MUST_USE bool setProperty(JSContext* cx, HandleDebuggerObject object,
                                         HandleId id, HandleValue value,
                                         MutableHandleValue result);
    static MOZ_MUST_USE bool getPrototypeOf(JSContext* cx, HandleDebuggerObject object,
                                            MutableHandleDebuggerObject result);
    static MOZ_MUST_USE bool getOwnPropertyNames(JSContext* cx, HandleDebuggerObject object,
                                                 MutableHandle<IdVector> result);
    static MOZ_MUST_USE bool getOwnPropertySymbols(JSContext* cx, HandleDebuggerObject object,
                                                   MutableHandle<IdVector> result);
    static MOZ_MUST_USE bool getOwnPropertyDescriptor(JSContext* cx, HandleDebuggerObject object,
                                                      HandleId id,
                                                      MutableHandle<PropertyDescriptor> desc);
    static MOZ_MUST_USE bool preventExtensions(JSContext* cx, HandleDebuggerObject object);
    static MOZ_MUST_USE bool seal(JSContext* cx, HandleDebuggerObject object);
    static MOZ_MUST_USE bool freeze(JSContext* cx, HandleDebuggerObject object);
    static MOZ_MUST_USE bool defineProperty(JSContext* cx, HandleDebuggerObject object,
                                            HandleId id, Handle<PropertyDescriptor> desc);
    static MOZ_MUST_USE bool defineProperties(JSContext* cx, HandleDebuggerObject object,
                                              Handle<IdVector> ids,
                                              Handle<PropertyDescriptorVector> descs);
    static MOZ_MUST_USE bool deleteProperty(JSContext* cx, HandleDebuggerObject object,
                                            HandleId id, ObjectOpResult& result);
    static MOZ_MUST_USE bool call(JSContext* cx, HandleDebuggerObject object, HandleValue thisv,
                                  Handle<ValueVector> args, MutableHandleValue result);
    static MOZ_MUST_USE bool forceLexicalInitializationByName(JSContext* cx,
                                                              HandleDebuggerObject object,
                                                              HandleId id, bool& result);
    static MOZ_MUST_USE bool executeInGlobal(JSContext* cx, HandleDebuggerObject object,
                                             mozilla::Range<const char16_t> chars,
                                             HandleObject bindings, const EvalOptions& options,
                                             ResumeMode& resumeMode, MutableHandleValue value);
    static MOZ_MUST_USE bool makeDebuggeeValue(JSContext* cx, HandleDebuggerObject object,
                                               HandleValue value, MutableHandleValue result);
    static MOZ_MUST_USE bool unsafeDereference(JSContext* cx, HandleDebuggerObject object,
                                               MutableHandleObject result);
    static MOZ_MUST_USE bool unwrap(JSContext* cx, HandleDebuggerObject object,
                                    MutableHandleDebuggerObject result);

    // Infallible properties
    bool isCallable() const;
    bool isFunction() const;
    bool isDebuggeeFunction() const;
    bool isBoundFunction() const;
    bool isArrowFunction() const;
    bool isAsyncFunction() const;
    bool isGeneratorFunction() const;
    bool isGlobal() const;
    bool isScriptedProxy() const;
    bool isPromise() const;
    JSAtom* name(JSContext* cx) const;
    JSAtom* displayName(JSContext* cx) const;
    JS::PromiseState promiseState() const;
    double promiseLifetime() const;
    double promiseTimeToResolution() const;

  private:
    enum {
        OWNER_SLOT
    };

    static const unsigned RESERVED_SLOTS = 1;

    static const ClassOps classOps_;

    static const JSPropertySpec properties_[];
    static const JSPropertySpec promiseProperties_[];
    static const JSFunctionSpec methods_[];

    JSObject* referent() const {
        JSObject* obj = (JSObject*) getPrivate();
        MOZ_ASSERT(obj);
        return obj;
    }

    Debugger* owner() const;
    PromiseObject* promise() const;

    static MOZ_MUST_USE bool requireGlobal(JSContext* cx, HandleDebuggerObject object);
    static MOZ_MUST_USE bool requirePromise(JSContext* cx, HandleDebuggerObject object);
    static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);

    // JSNative properties
    static MOZ_MUST_USE bool callableGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool isBoundFunctionGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool isArrowFunctionGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool isAsyncFunctionGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool isGeneratorFunctionGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool protoGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool classGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool nameGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool displayNameGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool parameterNamesGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool scriptGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool environmentGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool boundTargetFunctionGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool boundThisGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool boundArgumentsGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool allocationSiteGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool errorMessageNameGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool errorNotesGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool errorLineNumberGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool errorColumnNumberGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool isProxyGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool proxyTargetGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool proxyHandlerGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool isPromiseGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool promiseStateGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool promiseValueGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool promiseReasonGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool promiseLifetimeGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool promiseTimeToResolutionGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool promiseAllocationSiteGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool promiseResolutionSiteGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool promiseIDGetter(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool promiseDependentPromisesGetter(JSContext* cx, unsigned argc, Value* vp);

    // JSNative methods
    static MOZ_MUST_USE bool isExtensibleMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool isSealedMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool isFrozenMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool getPropertyMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool setPropertyMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool getOwnPropertyNamesMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool getOwnPropertySymbolsMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool getOwnPropertyDescriptorMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool preventExtensionsMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool sealMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool freezeMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool definePropertyMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool definePropertiesMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool deletePropertyMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool callMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool applyMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool asEnvironmentMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool forceLexicalInitializationByNameMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool executeInGlobalMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool executeInGlobalWithBindingsMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool makeDebuggeeValueMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool unsafeDereferenceMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool unwrapMethod(JSContext* cx, unsigned argc, Value* vp);
    static MOZ_MUST_USE bool getErrorReport(JSContext* cx, HandleObject maybeError,
                                            JSErrorReport*& report);
};

class JSBreakpointSite;
class WasmBreakpoint;
class WasmBreakpointSite;

class BreakpointSite {
    friend class Breakpoint;
    friend class ::JSScript;
    friend class Debugger;

  public:
    enum class Type { JS, Wasm };

  private:
    Type type_;

    template <typename T>
    struct SiteLinkAccess {
      static mozilla::DoublyLinkedListElement<T>& Get(T* aThis) {
        return aThis->siteLink;
      }
    };

    // List of all js::Breakpoints at this instruction.
    using BreakpointList =
        mozilla::DoublyLinkedList<js::Breakpoint,
                                  SiteLinkAccess<js::Breakpoint>>;
    BreakpointList breakpoints;
    size_t enabledCount;  /* number of breakpoints in the list that are enabled */

  protected:
    virtual void recompile(FreeOp* fop) = 0;
    inline bool isEnabled() const { return enabledCount > 0; }

  public:
    BreakpointSite(Type type);
    Breakpoint* firstBreakpoint() const;
    virtual ~BreakpointSite() {}
    bool hasBreakpoint(Breakpoint* bp);
    inline Type type() const { return type_; }

    void inc(FreeOp* fop);
    void dec(FreeOp* fop);
    bool isEmpty() const;
    virtual void destroyIfEmpty(FreeOp* fop) = 0;

    inline JSBreakpointSite* asJS();
    inline WasmBreakpointSite* asWasm();
};

/*
 * Each Breakpoint is a member of two linked lists: its debugger's list and its
 * site's list.
 *
 * GC rules:
 *   - script is live, breakpoint exists, and debugger is enabled
 *      ==> debugger is live
 *   - script is live, breakpoint exists, and debugger is live
 *      ==> retain the breakpoint and the handler object is live
 *
 * Debugger::markIteratively implements these two rules. It uses
 * Debugger::hasAnyLiveHooks to check for rule 1.
 *
 * Nothing else causes a breakpoint to be retained, so if its script or
 * debugger is collected, the breakpoint is destroyed during GC sweep phase,
 * even if the debugger compartment isn't being GC'd. This is implemented in
 * Zone::sweepBreakpoints.
 */
class Breakpoint {
    friend class Debugger;
    friend class BreakpointSite;

  public:
    Debugger * const debugger;
    BreakpointSite * const site;
  private:
    /* |handler| is marked unconditionally during minor GC. */
    js::PreBarrieredObject handler;

    /**
     * Link elements for each list this breakpoint can be in.
     */
    mozilla::DoublyLinkedListElement<Breakpoint> debuggerLink;
    mozilla::DoublyLinkedListElement<Breakpoint> siteLink;

  public:
    Breakpoint(Debugger* debugger, BreakpointSite* site, JSObject* handler);

    enum MayDestroySite {
        False,
        True
    };
    void destroy(FreeOp* fop, MayDestroySite mayDestroySite = MayDestroySite::True);

    Breakpoint* nextInDebugger();
    Breakpoint* nextInSite();
    const PreBarrieredObject& getHandler() const { return handler; }
    PreBarrieredObject& getHandlerRef() { return handler; }

    inline WasmBreakpoint* asWasm();
};

class JSBreakpointSite : public BreakpointSite
{
  public:
    JSScript* script;
    jsbytecode * const pc;

  protected:
    void recompile(FreeOp* fop) override;

  public:
    JSBreakpointSite(JSScript* script, jsbytecode* pc);

    void destroyIfEmpty(FreeOp* fop) override;
};

inline JSBreakpointSite*
BreakpointSite::asJS()
{
    MOZ_ASSERT(type() == Type::JS);
    return static_cast<JSBreakpointSite*>(this);
}

class WasmBreakpointSite : public BreakpointSite
{
  public:
    wasm::DebugState* debug;
    uint32_t offset;

  protected:
    void recompile(FreeOp* fop) override;

  public:
    WasmBreakpointSite(wasm::DebugState* debug, uint32_t offset);

    void destroyIfEmpty(FreeOp* fop) override;
};

inline WasmBreakpointSite*
BreakpointSite::asWasm()
{
    MOZ_ASSERT(type() == Type::Wasm);
    return static_cast<WasmBreakpointSite*>(this);
}

class WasmBreakpoint : public Breakpoint
{
  public:
    WasmInstanceObject* wasmInstance;

    WasmBreakpoint(Debugger* debugger, WasmBreakpointSite* site, JSObject* handler,
                   WasmInstanceObject* wasmInstance_)
      : Breakpoint(debugger, site, handler),
        wasmInstance(wasmInstance_)
    {}
};

inline WasmBreakpoint*
Breakpoint::asWasm()
{
    MOZ_ASSERT(site && site->type() == BreakpointSite::Type::Wasm);
    return static_cast<WasmBreakpoint*>(this);
}


Breakpoint*
Debugger::firstBreakpoint() const
{
    if (breakpoints.isEmpty()) {
        return nullptr;
    }
    return &(*breakpoints.begin());
}

const js::GCPtrNativeObject&
Debugger::toJSObject() const
{
    MOZ_ASSERT(object);
    return object;
}

js::GCPtrNativeObject&
Debugger::toJSObjectRef()
{
    MOZ_ASSERT(object);
    return object;
}

bool
Debugger::observesEnterFrame() const
{
    return enabled && getHook(OnEnterFrame);
}

bool
Debugger::observesNewScript() const
{
    return enabled && getHook(OnNewScript);
}

bool
Debugger::observesNewGlobalObject() const
{
    return enabled && getHook(OnNewGlobalObject);
}

bool
Debugger::observesGlobal(GlobalObject* global) const
{
    ReadBarriered<GlobalObject*> debuggee(global);
    return debuggees.has(debuggee);
}

/* static */ void
Debugger::onNewScript(JSContext* cx, HandleScript script)
{
    // We early return in slowPathOnNewScript for self-hosted scripts, so we can
    // ignore those in our assertion here.
    MOZ_ASSERT_IF(!script->realm()->creationOptions().invisibleToDebugger() &&
                  !script->selfHosted(),
                  script->realm()->firedOnNewGlobalObject);

    // The script may not be ready to be interrogated by the debugger.
    if (script->hideScriptFromDebugger()) {
        return;
    }

    if (script->realm()->isDebuggee()) {
        slowPathOnNewScript(cx, script);
    }
}

/* static */ void
Debugger::onNewGlobalObject(JSContext* cx, Handle<GlobalObject*> global)
{
    MOZ_ASSERT(!global->realm()->firedOnNewGlobalObject);
#ifdef DEBUG
    global->realm()->firedOnNewGlobalObject = true;
#endif
    if (!cx->runtime()->onNewGlobalObjectWatchers().isEmpty()) {
        Debugger::slowPathOnNewGlobalObject(cx, global);
    }
}

/* static */ bool
Debugger::onLogAllocationSite(JSContext* cx, JSObject* obj, HandleSavedFrame frame, mozilla::TimeStamp when)
{
    GlobalObject::DebuggerVector* dbgs = cx->global()->getDebuggers();
    if (!dbgs || dbgs->empty()) {
        return true;
    }
    RootedObject hobj(cx, obj);
    return Debugger::slowPathOnLogAllocationSite(cx, hobj, frame, when, *dbgs);
}

MOZ_MUST_USE bool ReportObjectRequired(JSContext* cx);

} /* namespace js */

namespace JS {

template <>
struct DeletePolicy<js::Debugger> : public js::GCManagedDeletePolicy<js::Debugger>
{};

} /* namespace JS */

#endif /* vm_Debugger_h */
