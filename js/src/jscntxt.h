/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS execution context. */

#ifndef jscntxt_h
#define jscntxt_h

#include "mozilla/MemoryReporting.h"

#include "js/Vector.h"
#include "vm/Runtime.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4100) /* Silence unreferenced formal parameter warnings */
#endif

struct DtoaState;

extern void
js_ReportOutOfMemory(js::ThreadSafeContext *cx);

extern void
js_ReportAllocationOverflow(js::ThreadSafeContext *cx);

extern void
js_ReportOverRecursed(js::ThreadSafeContext *cx);

namespace js {

namespace jit {
class IonContext;
class CompileCompartment;
}

struct CallsiteCloneKey {
    /* The original function that we are cloning. */
    JSFunction *original;

    /* The script of the call. */
    JSScript *script;

    /* The offset of the call. */
    uint32_t offset;

    CallsiteCloneKey(JSFunction *f, JSScript *s, uint32_t o) : original(f), script(s), offset(o) {}

    typedef CallsiteCloneKey Lookup;

    static inline uint32_t hash(CallsiteCloneKey key) {
        return uint32_t(size_t(key.script->offsetToPC(key.offset)) ^ size_t(key.original));
    }

    static inline bool match(const CallsiteCloneKey &a, const CallsiteCloneKey &b) {
        return a.script == b.script && a.offset == b.offset && a.original == b.original;
    }
};

typedef HashMap<CallsiteCloneKey,
                ReadBarrieredFunction,
                CallsiteCloneKey,
                SystemAllocPolicy> CallsiteCloneTable;

JSFunction *
ExistingCloneFunctionAtCallsite(const CallsiteCloneTable &table, JSFunction *fun,
                                JSScript *script, jsbytecode *pc);

JSFunction *CloneFunctionAtCallsite(JSContext *cx, HandleFunction fun,
                                    HandleScript script, jsbytecode *pc);

typedef HashSet<JSObject *> ObjectSet;
typedef HashSet<Shape *> ShapeSet;

/* Detects cycles when traversing an object graph. */
class AutoCycleDetector
{
    JSContext *cx;
    RootedObject obj;
    bool cyclic;
    uint32_t hashsetGenerationAtInit;
    ObjectSet::AddPtr hashsetAddPointer;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoCycleDetector(JSContext *cx, HandleObject objArg
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), obj(cx, objArg), cyclic(true)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoCycleDetector();

    bool init();

    bool foundCycle() { return cyclic; }
};

/* Updates references in the cycle detection set if the GC moves them. */
extern void
TraceCycleDetectionSet(JSTracer *trc, ObjectSet &set);

struct AutoResolving;
class DtoaCache;
class ForkJoinContext;
class RegExpStatics;

namespace frontend { struct CompileError; }

/*
 * Execution Context Overview:
 *
 * Several different structures may be used to provide a context for operations
 * on the VM. Each context is thread local, but varies in what data it can
 * access and what other threads may be running.
 *
 * - ThreadSafeContext is used by threads operating in one compartment which
 * may run in parallel with other threads operating on the same or other
 * compartments.
 *
 * - ExclusiveContext is used by threads operating in one compartment/zone,
 * where other threads may operate in other compartments, but *not* the same
 * compartment or zone which the ExclusiveContext is in. A thread with an
 * ExclusiveContext may enter the atoms compartment and atomize strings, in
 * which case a lock is used.
 *
 * - JSContext is used only by the runtime's main thread. The context may
 * operate in any compartment or zone which is not used by an ExclusiveContext
 * or ThreadSafeContext, and will only run in parallel with threads using such
 * contexts.
 *
 * An ExclusiveContext coerces to a ThreadSafeContext, and a JSContext coerces
 * to an ExclusiveContext or ThreadSafeContext.
 *
 * Contexts which are a ThreadSafeContext but not an ExclusiveContext are used
 * to represent a ForkJoinContext, the per-thread parallel context used in PJS.
 */

struct ThreadSafeContext : ContextFriendFields,
                           public MallocProvider<ThreadSafeContext>
{
    friend struct StackBaseShape;
    friend class Activation;
    friend UnownedBaseShape *BaseShape::lookupUnowned(ThreadSafeContext *cx,
                                                      const StackBaseShape &base);
    friend Shape *JSObject::lookupChildProperty(ThreadSafeContext *cx,
                                                JS::HandleObject obj, js::HandleShape parent,
                                                js::StackShape &child);

  public:
    enum ContextKind {
        Context_JS,
        Context_Exclusive,
        Context_ForkJoin
    };

  private:
    ContextKind contextKind_;

  public:
    PerThreadData *perThreadData;

    ThreadSafeContext(JSRuntime *rt, PerThreadData *pt, ContextKind kind);

    bool isJSContext() const {
        return contextKind_ == Context_JS;
    }

    JSContext *maybeJSContext() const {
        if (isJSContext())
            return (JSContext *) this;
        return nullptr;
    }

    JSContext *asJSContext() const {
        // Note: there is no way to perform an unchecked coercion from a
        // ThreadSafeContext to a JSContext. This ensures that trying to use
        // the context as a JSContext off the main thread will nullptr crash
        // rather than race.
        JS_ASSERT(isJSContext());
        return maybeJSContext();
    }

    // In some cases we could potentially want to do operations that require a
    // JSContext while running off the main thread. While this should never
    // actually happen, the wide enough API for working off the main thread
    // makes such operations impossible to rule out. Rather than blindly using
    // asJSContext() and crashing afterwards, this method may be used to watch
    // for such cases and produce either a soft failure in release builds or
    // an assertion failure in debug builds.
    bool shouldBeJSContext() const {
        JS_ASSERT(isJSContext());
        return isJSContext();
    }

    bool isExclusiveContext() const {
        return contextKind_ == Context_JS || contextKind_ == Context_Exclusive;
    }

    ExclusiveContext *maybeExclusiveContext() const {
        if (isExclusiveContext())
            return (ExclusiveContext *) this;
        return nullptr;
    }

    ExclusiveContext *asExclusiveContext() const {
        JS_ASSERT(isExclusiveContext());
        return maybeExclusiveContext();
    }

    bool isForkJoinContext() const;
    ForkJoinContext *asForkJoinContext();

    // The generational GC nursery may only be used on the main thread.
#ifdef JSGC_GENERATIONAL
    inline bool hasNursery() const {
        return isJSContext();
    }

    inline js::Nursery &nursery() {
        JS_ASSERT(hasNursery());
        return runtime_->gc.nursery;
    }
#endif

    /*
     * Allocator used when allocating GCThings on this context. If we are a
     * JSContext, this is the Zone allocator of the JSContext's zone.
     * Otherwise, this is a per-thread allocator.
     *
     * This does not live in PerThreadData because the notion of an allocator
     * is only per-thread when off the main thread. The runtime (and the main
     * thread) can have more than one zone, each with its own allocator, and
     * it's up to the context to specify what compartment and zone we are
     * operating in.
     */
  protected:
    Allocator *allocator_;

  public:
    static size_t offsetOfAllocator() { return offsetof(ThreadSafeContext, allocator_); }

    inline Allocator *allocator() const;

    // Allocations can only trigger GC when running on the main thread.
    inline AllowGC allowGC() const { return isJSContext() ? CanGC : NoGC; }

    template <typename T>
    bool isInsideCurrentZone(T thing) const {
        return thing->zoneFromAnyThread() == zone_;
    }

    template <typename T>
    inline bool isInsideCurrentCompartment(T thing) const {
        return thing->compartment() == compartment_;
    }

    template <typename T>
    inline bool isThreadLocal(T thing) const;

    void *onOutOfMemory(void *p, size_t nbytes) {
        return runtime_->onOutOfMemory(p, nbytes, maybeJSContext());
    }

    /* Clear the pending exception (if any) due to OOM. */
    void recoverFromOutOfMemory();

    inline void updateMallocCounter(size_t nbytes) {
        // Note: this is racy.
        runtime_->updateMallocCounter(zone_, nbytes);
    }

    void reportAllocationOverflow() {
        js_ReportAllocationOverflow(this);
    }

    // Accessors for immutable runtime data.
    JSAtomState &names() { return *runtime_->commonNames; }
    StaticStrings &staticStrings() { return *runtime_->staticStrings; }
    AtomSet &permanentAtoms() { return *runtime_->permanentAtoms; }
    const JS::AsmJSCacheOps &asmJSCacheOps() { return runtime_->asmJSCacheOps; }
    PropertyName *emptyString() { return runtime_->emptyString; }
    FreeOp *defaultFreeOp() { return runtime_->defaultFreeOp(); }
    void *runtimeAddressForJit() { return runtime_; }
    void *stackLimitAddress(StackKind kind) { return &runtime_->mainThread.nativeStackLimit[kind]; }
    void *stackLimitAddressForJitCode(StackKind kind);
    size_t gcSystemPageSize() { return runtime_->gc.pageAllocator.systemPageSize(); }
    bool signalHandlersInstalled() const { return runtime_->signalHandlersInstalled(); }
    bool jitSupportsFloatingPoint() const { return runtime_->jitSupportsFloatingPoint; }

    // Thread local data that may be accessed freely.
    DtoaState *dtoaState() {
        return perThreadData->dtoaState;
    }
};

struct HelperThread;

class ExclusiveContext : public ThreadSafeContext
{
    friend class gc::ArenaLists;
    friend class AutoCompartment;
    friend class AutoLockForExclusiveAccess;
    friend struct StackBaseShape;
    friend void JSScript::initCompartment(ExclusiveContext *cx);
    friend class jit::IonContext;

    // The thread on which this context is running, if this is not a JSContext.
    HelperThread *helperThread_;

  public:

    ExclusiveContext(JSRuntime *rt, PerThreadData *pt, ContextKind kind)
      : ThreadSafeContext(rt, pt, kind),
        helperThread_(nullptr),
        enterCompartmentDepth_(0)
    {}

    /*
     * "Entering" a compartment changes cx->compartment (which changes
     * cx->global). Note that this does not push any InterpreterFrame which means
     * that it is possible for cx->fp()->compartment() != cx->compartment.
     * This is not a problem since, in general, most places in the VM cannot
     * know that they were called from script (e.g., they may have been called
     * through the JSAPI via JS_CallFunction) and thus cannot expect fp.
     *
     * Compartments should be entered/left in a LIFO fasion. The depth of this
     * enter/leave stack is maintained by enterCompartmentDepth_ and queried by
     * hasEnteredCompartment.
     *
     * To enter a compartment, code should prefer using AutoCompartment over
     * manually calling cx->enterCompartment/leaveCompartment.
     */
  protected:
    unsigned            enterCompartmentDepth_;
    inline void setCompartment(JSCompartment *comp);
  public:
    bool hasEnteredCompartment() const {
        return enterCompartmentDepth_ > 0;
    }
#ifdef DEBUG
    unsigned getEnterCompartmentDepth() const {
        return enterCompartmentDepth_;
    }
#endif

    inline void enterCompartment(JSCompartment *c);
    inline void enterNullCompartment();
    inline void leaveCompartment(JSCompartment *oldCompartment);

    void setHelperThread(HelperThread *helperThread);
    HelperThread *helperThread() const { return helperThread_; }

    // Threads with an ExclusiveContext may freely access any data in their
    // compartment and zone.
    JSCompartment *compartment() const {
        JS_ASSERT_IF(runtime_->isAtomsCompartment(compartment_),
                     runtime_->currentThreadHasExclusiveAccess());
        return compartment_;
    }
    JS::Zone *zone() const {
        JS_ASSERT_IF(!compartment(), !zone_);
        JS_ASSERT_IF(compartment(), js::GetCompartmentZone(compartment()) == zone_);
        return zone_;
    }

    // Zone local methods that can be used freely from an ExclusiveContext.
    types::TypeObject *getNewType(const Class *clasp, TaggedProto proto, JSFunction *fun = nullptr);
    types::TypeObject *getSingletonType(const Class *clasp, TaggedProto proto);
    inline js::LifoAlloc &typeLifoAlloc();

    // Current global. This is only safe to use within the scope of the
    // AutoCompartment from which it's called.
    inline js::Handle<js::GlobalObject*> global() const;

    // Methods to access runtime data that must be protected by locks.
    frontend::ParseMapPool &parseMapPool() {
        return runtime_->parseMapPool();
    }
    AtomSet &atoms() {
        return runtime_->atoms();
    }
    JSCompartment *atomsCompartment() {
        return runtime_->atomsCompartment();
    }
    ScriptDataTable &scriptDataTable() {
        return runtime_->scriptDataTable();
    }

    // Methods specific to any HelperThread for the context.
    frontend::CompileError &addPendingCompileError();
    void addPendingOverRecursed();
};

} /* namespace js */

struct JSContext : public js::ExclusiveContext,
                   public mozilla::LinkedListElement<JSContext>
{
    explicit JSContext(JSRuntime *rt);
    ~JSContext();

    JSRuntime *runtime() const { return runtime_; }
    js::PerThreadData &mainThread() const { return runtime()->mainThread; }

    static size_t offsetOfRuntime() {
        return offsetof(JSContext, runtime_);
    }

    friend class js::ExclusiveContext;
    friend class JS::AutoSaveExceptionState;

  private:
    /* Exception state -- the exception member is a GC root by definition. */
    bool                throwing;            /* is there a pending exception? */
    js::Value           unwrappedException_; /* most-recently-thrown exception */

    /* Per-context options. */
    JS::ContextOptions  options_;

    // True if propagating a forced return from an interrupt handler during
    // debug mode.
    bool                propagatingForcedReturn_;

  public:
    int32_t             reportGranularity;  /* see vm/Probes.h */

    js::AutoResolving   *resolvingList;

    /* True if generating an error, to prevent runaway recursion. */
    bool                generatingError;

    /* See JS_SaveFrameChain/JS_RestoreFrameChain. */
  private:
    struct SavedFrameChain {
        SavedFrameChain(JSCompartment *comp, unsigned count)
          : compartment(comp), enterCompartmentCount(count) {}
        JSCompartment *compartment;
        unsigned enterCompartmentCount;
    };
    typedef js::Vector<SavedFrameChain, 1, js::SystemAllocPolicy> SaveStack;
    SaveStack           savedFrameChains_;
  public:
    bool saveFrameChain();
    void restoreFrameChain();

    /*
     * When no compartments have been explicitly entered, the context's
     * compartment will be set to the compartment of the "default compartment
     * object".
     */
  private:
    JSObject *defaultCompartmentObject_;
  public:
    inline void setDefaultCompartmentObject(JSObject *obj);
    inline void setDefaultCompartmentObjectIfUnset(JSObject *obj);
    JSObject *maybeDefaultCompartmentObject() const {
        JS_ASSERT(!options().noDefaultCompartmentObject());
        return defaultCompartmentObject_;
    }

    /* State for object and array toSource conversion. */
    js::ObjectSet       cycleDetectorSet;

    /* Per-context optional error reporter. */
    JSErrorReporter     errorReporter;

    /* Client opaque pointers. */
    void                *data;
    void                *data2;

  public:

    /*
     * Return:
     * - The newest scripted frame's version, if there is such a frame.
     * - The version from the compartment.
     * - The default version.
     *
     * Note: if this ever shows up in a profile, just add caching!
     */
    JSVersion findVersion() const;

    const JS::ContextOptions &options() const {
        return options_;
    }

    JS::ContextOptions &options() {
        return options_;
    }

    js::LifoAlloc &tempLifoAlloc() { return runtime()->tempLifoAlloc; }

#ifdef JS_THREADSAFE
    unsigned            outstandingRequests;/* number of JS_BeginRequest calls
                                               without the corresponding
                                               JS_EndRequest. */
#endif

    /* Location to stash the iteration value between JSOP_MOREITER and JSOP_ITERNEXT. */
    js::Value           iterValue;

    bool jitIsBroken;

    void updateJITEnabled();

    /* Whether this context has JS frames on the stack. */
    bool currentlyRunning() const;

    bool currentlyRunningInInterpreter() const {
        return mainThread().activation()->isInterpreter();
    }
    bool currentlyRunningInJit() const {
        return mainThread().activation()->isJit();
    }
    js::InterpreterFrame *interpreterFrame() const {
        return mainThread().activation()->asInterpreter()->current();
    }
    js::InterpreterRegs &interpreterRegs() const {
        return mainThread().activation()->asInterpreter()->regs();
    }

    /*
     * Get the topmost script and optional pc on the stack. By default, this
     * function only returns a JSScript in the current compartment, returning
     * nullptr if the current script is in a different compartment. This
     * behavior can be overridden by passing ALLOW_CROSS_COMPARTMENT.
     */
    enum MaybeAllowCrossCompartment {
        DONT_ALLOW_CROSS_COMPARTMENT = false,
        ALLOW_CROSS_COMPARTMENT = true
    };
    inline JSScript *currentScript(jsbytecode **pc = nullptr,
                                   MaybeAllowCrossCompartment = DONT_ALLOW_CROSS_COMPARTMENT) const;

#ifdef MOZ_TRACE_JSCALLS
    /* Function entry/exit debugging callback. */
    JSFunctionCallback    functionCallback;

    void doFunctionCallback(const JSFunction *fun,
                            const JSScript *scr,
                            int entering) const
    {
        if (functionCallback)
            functionCallback(fun, scr, this, entering);
    }
#endif

  private:
    /* Innermost-executing generator or null if no generator are executing. */
    JSGenerator *innermostGenerator_;
  public:
    JSGenerator *innermostGenerator() const { return innermostGenerator_; }
    void enterGenerator(JSGenerator *gen);
    void leaveGenerator(JSGenerator *gen);

    bool isExceptionPending() {
        return throwing;
    }

    MOZ_WARN_UNUSED_RESULT
    bool getPendingException(JS::MutableHandleValue rval);

    bool isThrowingOutOfMemory();

    void setPendingException(js::Value v);

    void clearPendingException() {
        throwing = false;
        unwrappedException_.setUndefined();
    }

    bool isPropagatingForcedReturn() const { return propagatingForcedReturn_; }
    void setPropagatingForcedReturn() { propagatingForcedReturn_ = true; }
    void clearPropagatingForcedReturn() { propagatingForcedReturn_ = false; }

#ifdef DEBUG
    /*
     * Controls whether a quadratic-complexity assertion is performed during
     * stack iteration; defaults to true.
     */
    bool stackIterAssertionEnabled;
#endif

    /*
     * See JS_SetTrustedPrincipals in jsapi.h.
     * Note: !cx->compartment is treated as trusted.
     */
    bool runningWithTrustedPrincipals() const;

    JS_FRIEND_API(size_t) sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    void mark(JSTracer *trc);

  private:
    /*
     * The allocation code calls the function to indicate either OOM failure
     * when p is null or that a memory pressure counter has reached some
     * threshold when p is not null. The function takes the pointer and not
     * a boolean flag to minimize the amount of code in its inlined callers.
     */
    JS_FRIEND_API(void) checkMallocGCPressure(void *p);
}; /* struct JSContext */

namespace js {

struct AutoResolving {
  public:
    enum Kind {
        LOOKUP,
        WATCH
    };

    AutoResolving(JSContext *cx, HandleObject obj, HandleId id, Kind kind = LOOKUP
                  MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : context(cx), object(obj), id(id), kind(kind), link(cx->resolvingList)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        JS_ASSERT(obj);
        cx->resolvingList = this;
    }

    ~AutoResolving() {
        JS_ASSERT(context->resolvingList == this);
        context->resolvingList = link;
    }

    bool alreadyStarted() const {
        return link && alreadyStartedSlow();
    }

  private:
    bool alreadyStartedSlow() const;

    JSContext           *const context;
    HandleObject        object;
    HandleId            id;
    Kind                const kind;
    AutoResolving       *const link;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/*
 * Enumerate all contexts in a runtime.
 */
class ContextIter {
    JSContext *iter;

public:
    explicit ContextIter(JSRuntime *rt) {
        iter = rt->contextList.getFirst();
    }

    bool done() const {
        return !iter;
    }

    void next() {
        JS_ASSERT(!done());
        iter = iter->getNext();
    }

    JSContext *get() const {
        JS_ASSERT(!done());
        return iter;
    }

    operator JSContext *() const {
        return get();
    }

    JSContext *operator ->() const {
        return get();
    }
};

/*
 * Create and destroy functions for JSContext, which is manually allocated
 * and exclusively owned.
 */
extern JSContext *
NewContext(JSRuntime *rt, size_t stackChunkSize);

enum DestroyContextMode {
    DCM_NO_GC,
    DCM_FORCE_GC,
    DCM_NEW_FAILED
};

extern void
DestroyContext(JSContext *cx, DestroyContextMode mode);

enum ErrorArgumentsType {
    ArgumentsAreUnicode,
    ArgumentsAreASCII
};


/*
 * Loads and returns a self-hosted function by name. For performance, define
 * the property name in vm/CommonPropertyNames.h.
 *
 * Defined in SelfHosting.cpp.
 */
JSFunction *
SelfHostedFunction(JSContext *cx, HandlePropertyName propName);

} /* namespace js */

#ifdef va_start
extern bool
js_ReportErrorVA(JSContext *cx, unsigned flags, const char *format, va_list ap);

extern bool
js_ReportErrorNumberVA(JSContext *cx, unsigned flags, JSErrorCallback callback,
                       void *userRef, const unsigned errorNumber,
                       js::ErrorArgumentsType argumentsType, va_list ap);

extern bool
js_ReportErrorNumberUCArray(JSContext *cx, unsigned flags, JSErrorCallback callback,
                            void *userRef, const unsigned errorNumber,
                            const jschar **args);
#endif

extern bool
js_ExpandErrorArguments(js::ExclusiveContext *cx, JSErrorCallback callback,
                        void *userRef, const unsigned errorNumber,
                        char **message, JSErrorReport *reportp,
                        js::ErrorArgumentsType argumentsType, va_list ap);

namespace js {

/* |callee| requires a usage string provided by JS_DefineFunctionsWithHelp. */
extern void
ReportUsageError(JSContext *cx, HandleObject callee, const char *msg);

/*
 * Prints a full report and returns true if the given report is non-nullptr
 * and the report doesn't have the JSREPORT_WARNING flag set or reportWarnings
 * is true.
 * Returns false otherwise, printing just the message if the report is nullptr.
 */
extern bool
PrintError(JSContext *cx, FILE *file, const char *message, JSErrorReport *report,
           bool reportWarnings);

/*
 * Send a JSErrorReport to the errorReporter callback.
 */
void
CallErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);

} /* namespace js */

extern void
js_ReportIsNotDefined(JSContext *cx, const char *name);

/*
 * Report an attempt to access the property of a null or undefined value (v).
 */
extern bool
js_ReportIsNullOrUndefined(JSContext *cx, int spindex, js::HandleValue v,
                           js::HandleString fallback);

extern void
js_ReportMissingArg(JSContext *cx, js::HandleValue v, unsigned arg);

/*
 * Report error using js_DecompileValueGenerator(cx, spindex, v, fallback) as
 * the first argument for the error message. If the error message has less
 * then 3 arguments, use null for arg1 or arg2.
 */
extern bool
js_ReportValueErrorFlags(JSContext *cx, unsigned flags, const unsigned errorNumber,
                         int spindex, js::HandleValue v, js::HandleString fallback,
                         const char *arg1, const char *arg2);

#define js_ReportValueError(cx,errorNumber,spindex,v,fallback)                \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, nullptr, nullptr))

#define js_ReportValueError2(cx,errorNumber,spindex,v,fallback,arg1)          \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, arg1, nullptr))

#define js_ReportValueError3(cx,errorNumber,spindex,v,fallback,arg1,arg2)     \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, arg1, arg2))

extern const JSErrorFormatString js_ErrorFormatString[JSErr_Limit];

char *
js_strdup(js::ExclusiveContext *cx, const char *s);

#ifdef JS_THREADSAFE
# define JS_ASSERT_REQUEST_DEPTH(cx)  JS_ASSERT((cx)->runtime()->requestDepth >= 1)
#else
# define JS_ASSERT_REQUEST_DEPTH(cx)  ((void) 0)
#endif

namespace js {

/*
 * Invoke the interrupt callback and return false if the current execution
 * is to be terminated.
 */
bool
InvokeInterruptCallback(JSContext *cx);

bool
HandleExecutionInterrupt(JSContext *cx);

/*
 * Process any pending interrupt requests. Long-running inner loops in C++ must
 * call this periodically to make sure they are interruptible --- that is, to
 * make sure they do not prevent the slow script dialog from appearing.
 *
 * This can run a full GC or call the interrupt callback, which could do
 * anything. In the browser, it displays the slow script dialog.
 *
 * If this returns true, the caller can continue; if false, the caller must
 * break out of its loop. This happens if, for example, the user clicks "Stop
 * script" on the slow script dialog; treat it as an uncatchable error.
 */
inline bool
CheckForInterrupt(JSContext *cx)
{
    JS_ASSERT_REQUEST_DEPTH(cx);
    return !cx->runtime()->interrupt || InvokeInterruptCallback(cx);
}

/************************************************************************/

class AutoStringVector : public AutoVectorRooter<JSString *>
{
  public:
    explicit AutoStringVector(JSContext *cx
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<JSString *>(cx, STRINGVECTOR)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoPropertyNameVector : public AutoVectorRooter<PropertyName *>
{
  public:
    explicit AutoPropertyNameVector(JSContext *cx
                                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<PropertyName *>(cx, STRINGVECTOR)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoShapeVector : public AutoVectorRooter<Shape *>
{
  public:
    explicit AutoShapeVector(JSContext *cx
                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<Shape *>(cx, SHAPEVECTOR)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoObjectObjectHashMap : public AutoHashMapRooter<JSObject *, JSObject *>
{
  public:
    explicit AutoObjectObjectHashMap(JSContext *cx
                                     MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoHashMapRooter<JSObject *, JSObject *>(cx, OBJOBJHASHMAP)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoObjectUnsigned32HashMap : public AutoHashMapRooter<JSObject *, uint32_t>
{
  public:
    explicit AutoObjectUnsigned32HashMap(JSContext *cx
                                         MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoHashMapRooter<JSObject *, uint32_t>(cx, OBJU32HASHMAP)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoObjectHashSet : public AutoHashSetRooter<JSObject *>
{
  public:
    explicit AutoObjectHashSet(JSContext *cx
                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoHashSetRooter<JSObject *>(cx, OBJHASHSET)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/* AutoArrayRooter roots an external array of Values. */
class AutoArrayRooter : private JS::AutoGCRooter
{
  public:
    AutoArrayRooter(JSContext *cx, size_t len, Value *vec
                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : JS::AutoGCRooter(cx, len), array(vec)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        JS_ASSERT(tag_ >= 0);
    }

    void changeLength(size_t newLength) {
        tag_ = ptrdiff_t(newLength);
        JS_ASSERT(tag_ >= 0);
    }

    void changeArray(Value *newArray, size_t newLength) {
        changeLength(newLength);
        array = newArray;
    }

    Value *start() {
        return array;
    }

    size_t length() {
        JS_ASSERT(tag_ >= 0);
        return size_t(tag_);
    }

    MutableHandleValue handleAt(size_t i) {
        JS_ASSERT(i < size_t(tag_));
        return MutableHandleValue::fromMarkedLocation(&array[i]);
    }
    HandleValue handleAt(size_t i) const {
        JS_ASSERT(i < size_t(tag_));
        return HandleValue::fromMarkedLocation(&array[i]);
    }
    MutableHandleValue operator[](size_t i) {
        JS_ASSERT(i < size_t(tag_));
        return MutableHandleValue::fromMarkedLocation(&array[i]);
    }
    HandleValue operator[](size_t i) const {
        JS_ASSERT(i < size_t(tag_));
        return HandleValue::fromMarkedLocation(&array[i]);
    }

    friend void JS::AutoGCRooter::trace(JSTracer *trc);

  private:
    Value *array;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoAssertNoException
{
#ifdef DEBUG
    JSContext *cx;
    bool hadException;
#endif

  public:
    explicit AutoAssertNoException(JSContext *cx)
#ifdef DEBUG
      : cx(cx),
        hadException(cx->isExceptionPending())
#endif
    {
    }

    ~AutoAssertNoException()
    {
        JS_ASSERT_IF(!hadException, !cx->isExceptionPending());
    }
};

/*
 * FIXME bug 647103 - replace these *AllocPolicy names.
 */
class ContextAllocPolicy
{
    ThreadSafeContext *const cx_;

  public:
    MOZ_IMPLICIT ContextAllocPolicy(ThreadSafeContext *cx) : cx_(cx) {}
    ThreadSafeContext *context() const { return cx_; }
    void *malloc_(size_t bytes) { return cx_->malloc_(bytes); }
    void *calloc_(size_t bytes) { return cx_->calloc_(bytes); }
    void *realloc_(void *p, size_t oldBytes, size_t bytes) { return cx_->realloc_(p, oldBytes, bytes); }
    void free_(void *p) { js_free(p); }
    void reportAllocOverflow() const { js_ReportAllocationOverflow(cx_); }
};

/* Exposed intrinsics so that Ion may inline them. */
bool intrinsic_ToObject(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_ToInteger(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_ToString(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_IsCallable(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_ThrowError(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_NewDenseArray(JSContext *cx, unsigned argc, Value *vp);

bool intrinsic_UnsafePutElements(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_DefineDataProperty(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_UnsafeSetReservedSlot(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_UnsafeGetReservedSlot(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_HaveSameClass(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_IsPackedArray(JSContext *cx, unsigned argc, Value *vp);

bool intrinsic_ShouldForceSequential(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_NewParallelArray(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_ForkJoinGetSlice(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_InParallelSection(JSContext *cx, unsigned argc, Value *vp);

bool intrinsic_ObjectIsTypedObject(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_ObjectIsTransparentTypedObject(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_ObjectIsOpaqueTypedObject(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_ObjectIsTypeDescr(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_TypeDescrIsSimpleType(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_TypeDescrIsArrayType(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_TypeDescrIsUnsizedArrayType(JSContext *cx, unsigned argc, Value *vp);
bool intrinsic_TypeDescrIsSizedArrayType(JSContext *cx, unsigned argc, Value *vp);

class AutoLockForExclusiveAccess
{
#ifdef JS_THREADSAFE
    JSRuntime *runtime;

    void init(JSRuntime *rt) {
        runtime = rt;
        if (runtime->numExclusiveThreads) {
            runtime->assertCanLock(ExclusiveAccessLock);
            PR_Lock(runtime->exclusiveAccessLock);
#ifdef DEBUG
            runtime->exclusiveAccessOwner = PR_GetCurrentThread();
#endif
        } else {
            JS_ASSERT(!runtime->mainThreadHasExclusiveAccess);
            runtime->mainThreadHasExclusiveAccess = true;
        }
    }

  public:
    explicit AutoLockForExclusiveAccess(ExclusiveContext *cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        init(cx->runtime_);
    }
    explicit AutoLockForExclusiveAccess(JSRuntime *rt MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        init(rt);
    }
    ~AutoLockForExclusiveAccess() {
        if (runtime->numExclusiveThreads) {
            JS_ASSERT(runtime->exclusiveAccessOwner == PR_GetCurrentThread());
            runtime->exclusiveAccessOwner = nullptr;
            PR_Unlock(runtime->exclusiveAccessLock);
        } else {
            JS_ASSERT(runtime->mainThreadHasExclusiveAccess);
            runtime->mainThreadHasExclusiveAccess = false;
        }
    }
#else // JS_THREADSAFE
  public:
    AutoLockForExclusiveAccess(ExclusiveContext *cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
    AutoLockForExclusiveAccess(JSRuntime *rt MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoLockForExclusiveAccess() {
        // An empty destructor is needed to avoid warnings from clang about
        // unused local variables of this type.
    }
#endif // JS_THREADSAFE

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace js */

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif /* jscntxt_h */
