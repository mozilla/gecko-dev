/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Definitions for managing off-main-thread work using a process wide list
 * of worklist items and pool of threads. Worklist items are engine internal,
 * and are distinct from e.g. web workers.
 */

#ifndef vm_HelperThreads_h
#define vm_HelperThreads_h

#include "mozilla/GuardObjects.h"
#include "mozilla/PodOperations.h"

#include "jscntxt.h"
#include "jslock.h"

#include "frontend/TokenStream.h"
#include "jit/Ion.h"

namespace js {

struct HelperThread;
struct AsmJSParallelTask;
struct ParseTask;
namespace jit {
  class IonBuilder;
}

#ifdef JS_THREADSAFE

// Per-process state for off thread work items.
class GlobalHelperThreadState
{
  public:
    // Number of CPUs to treat this machine as having when creating threads.
    // May be accessed without locking.
    size_t cpuCount;

    // Number of threads to create. May be accessed without locking.
    size_t threadCount;

    typedef Vector<jit::IonBuilder*, 0, SystemAllocPolicy> IonBuilderVector;
    typedef Vector<AsmJSParallelTask*, 0, SystemAllocPolicy> AsmJSParallelTaskVector;
    typedef Vector<ParseTask*, 0, SystemAllocPolicy> ParseTaskVector;
    typedef Vector<SourceCompressionTask*, 0, SystemAllocPolicy> SourceCompressionTaskVector;
    typedef Vector<GCHelperState *, 0, SystemAllocPolicy> GCHelperStateVector;

    // List of available threads, or null if the thread state has not been initialized.
    HelperThread *threads;

  private:
    // The lists below are all protected by |lock|.

    // Ion compilation worklist and finished jobs.
    IonBuilderVector ionWorklist_, ionFinishedList_;

    // AsmJS worklist and finished jobs.
    //
    // Simultaneous AsmJS compilations all service the same AsmJS module.
    // The main thread must pick up finished optimizations and perform codegen.
    // |asmJSCompilationInProgress| is used to avoid triggering compilations
    // for more than one module at a time.
    AsmJSParallelTaskVector asmJSWorklist_, asmJSFinishedList_;

  public:
    // For now, only allow a single parallel asm.js compilation to happen at a
    // time. This avoids race conditions on asmJSWorklist/asmJSFinishedList/etc.
    mozilla::Atomic<bool> asmJSCompilationInProgress;

  private:
    // Script parsing/emitting worklist and finished jobs.
    ParseTaskVector parseWorklist_, parseFinishedList_;

    // Parse tasks waiting for an atoms-zone GC to complete.
    ParseTaskVector parseWaitingOnGC_;

    // Source compression worklist.
    SourceCompressionTaskVector compressionWorklist_;

    // Runtimes which have sweeping / allocating work to do.
    GCHelperStateVector gcHelperWorklist_;

  public:
    GlobalHelperThreadState();

    void ensureInitialized();
    void finish();

    void lock();
    void unlock();

# ifdef DEBUG
    bool isLocked();
# endif

    enum CondVar {
        // For notifying threads waiting for work that they may be able to make progress.
        CONSUMER,

        // For notifying threads doing work that they may be able to make progress.
        PRODUCER
    };

    void wait(CondVar which, uint32_t timeoutMillis = 0);
    void notifyAll(CondVar which);
    void notifyOne(CondVar which);

    // Helper method for removing items from the vectors below while iterating over them.
    template <typename T>
    void remove(T &vector, size_t *index)
    {
        vector[(*index)--] = vector.back();
        vector.popBack();
    }

    IonBuilderVector &ionWorklist() {
        JS_ASSERT(isLocked());
        return ionWorklist_;
    }
    IonBuilderVector &ionFinishedList() {
        JS_ASSERT(isLocked());
        return ionFinishedList_;
    }

    AsmJSParallelTaskVector &asmJSWorklist() {
        JS_ASSERT(isLocked());
        return asmJSWorklist_;
    }
    AsmJSParallelTaskVector &asmJSFinishedList() {
        JS_ASSERT(isLocked());
        return asmJSFinishedList_;
    }

    ParseTaskVector &parseWorklist() {
        JS_ASSERT(isLocked());
        return parseWorklist_;
    }
    ParseTaskVector &parseFinishedList() {
        JS_ASSERT(isLocked());
        return parseFinishedList_;
    }
    ParseTaskVector &parseWaitingOnGC() {
        JS_ASSERT(isLocked());
        return parseWaitingOnGC_;
    }

    SourceCompressionTaskVector &compressionWorklist() {
        JS_ASSERT(isLocked());
        return compressionWorklist_;
    }

    GCHelperStateVector &gcHelperWorklist() {
        JS_ASSERT(isLocked());
        return gcHelperWorklist_;
    }

    bool canStartAsmJSCompile();
    bool canStartIonCompile();
    bool canStartParseTask();
    bool canStartCompressionTask();
    bool canStartGCHelperTask();

    uint32_t harvestFailedAsmJSJobs() {
        JS_ASSERT(isLocked());
        uint32_t n = numAsmJSFailedJobs;
        numAsmJSFailedJobs = 0;
        return n;
    }
    void noteAsmJSFailure(void *func) {
        // Be mindful to signal the main thread after calling this function.
        JS_ASSERT(isLocked());
        if (!asmJSFailedFunction)
            asmJSFailedFunction = func;
        numAsmJSFailedJobs++;
    }
    bool asmJSFailed() const {
        return bool(numAsmJSFailedJobs);
    }
    void resetAsmJSFailureState() {
        numAsmJSFailedJobs = 0;
        asmJSFailedFunction = nullptr;
    }
    void *maybeAsmJSFailedFunction() const {
        return asmJSFailedFunction;
    }

    JSScript *finishParseTask(JSContext *maybecx, JSRuntime *rt, void *token);
    bool compressionInProgress(SourceCompressionTask *task);
    SourceCompressionTask *compressionTaskForSource(ScriptSource *ss);

  private:

    /*
     * Lock protecting all mutable shared state accessed by helper threads, and
     * used by all condition variables.
     */
    PRLock *helperLock;
# ifdef DEBUG
    PRThread *lockOwner;
# endif

    /* Condvars for threads waiting/notifying each other. */
    PRCondVar *consumerWakeup;
    PRCondVar *producerWakeup;

    /*
     * Number of AsmJS jobs that encountered failure for the active module.
     * Their parent is logically the main thread, and this number serves for harvesting.
     */
    uint32_t numAsmJSFailedJobs;

    /*
     * Function index |i| in |Module.function(i)| of first failed AsmJS function.
     * -1 if no function has failed.
     */
    void *asmJSFailedFunction;
};

static inline GlobalHelperThreadState &
HelperThreadState()
{
    extern GlobalHelperThreadState gHelperThreadState;
    return gHelperThreadState;
}

/* Individual helper thread, one allocated per core. */
struct HelperThread
{
    mozilla::Maybe<PerThreadData> threadData;
    PRThread *thread;

    /* Indicate to an idle thread that it should finish executing. */
    bool terminate;

    /* Any builder currently being compiled by Ion on this thread. */
    jit::IonBuilder *ionBuilder;

    /* Any AsmJS data currently being optimized by Ion on this thread. */
    AsmJSParallelTask *asmData;

    /* Any source being parsed/emitted on this thread. */
    ParseTask *parseTask;

    /* Any source being compressed on this thread. */
    SourceCompressionTask *compressionTask;

    /* Any GC state for background sweeping or allocating being performed. */
    GCHelperState *gcHelperState;

    bool idle() const {
        return !ionBuilder && !asmData && !parseTask && !compressionTask && !gcHelperState;
    }

    void destroy();

    void handleAsmJSWorkload();
    void handleIonWorkload();
    void handleParseWorkload();
    void handleCompressionWorkload();
    void handleGCHelperWorkload();

    static void ThreadMain(void *arg);
    void threadLoop();
};

#endif /* JS_THREADSAFE */

/* Methods for interacting with helper threads. */

// Initialize helper threads unless already initialized.
void
EnsureHelperThreadsInitialized(ExclusiveContext *cx);

// This allows the JS shell to override GetCPUCount() when passed the
// --thread-count=N option.
void
SetFakeCPUCount(size_t count);

#ifdef JS_ION

/* Perform MIR optimization and LIR generation on a single function. */
bool
StartOffThreadAsmJSCompile(ExclusiveContext *cx, AsmJSParallelTask *asmData);

/*
 * Schedule an Ion compilation for a script, given a builder which has been
 * generated and read everything needed from the VM state.
 */
bool
StartOffThreadIonCompile(JSContext *cx, jit::IonBuilder *builder);

#endif // JS_ION

/*
 * Cancel a scheduled or in progress Ion compilation for script. If script is
 * nullptr, all compilations for the compartment are cancelled.
 */
void
CancelOffThreadIonCompile(JSCompartment *compartment, JSScript *script);

/* Cancel all scheduled, in progress or finished parses for runtime. */
void
CancelOffThreadParses(JSRuntime *runtime);

/*
 * Start a parse/emit cycle for a stream of source. The characters must stay
 * alive until the compilation finishes.
 */
bool
StartOffThreadParseScript(JSContext *cx, const ReadOnlyCompileOptions &options,
                          const jschar *chars, size_t length,
                          JS::OffThreadCompileCallback callback, void *callbackData);

/*
 * Called at the end of GC to enqueue any Parse tasks that were waiting on an
 * atoms-zone GC to finish.
 */
void
EnqueuePendingParseTasksAfterGC(JSRuntime *rt);

/* Start a compression job for the specified token. */
bool
StartOffThreadCompression(ExclusiveContext *cx, SourceCompressionTask *task);

class AutoLockHelperThreadState
{
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

#ifdef JS_THREADSAFE
  public:
    explicit AutoLockHelperThreadState(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        HelperThreadState().lock();
    }

    ~AutoLockHelperThreadState() {
        HelperThreadState().unlock();
    }
#else
  public:
    AutoLockHelperThreadState(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
#endif
};

class AutoUnlockHelperThreadState
{
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:

    explicit AutoUnlockHelperThreadState(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
#ifdef JS_THREADSAFE
        HelperThreadState().unlock();
#endif
    }

    ~AutoUnlockHelperThreadState()
    {
#ifdef JS_THREADSAFE
        HelperThreadState().lock();
#endif
    }
};

#ifdef JS_ION
struct AsmJSParallelTask
{
    JSRuntime *runtime;     // Associated runtime.
    LifoAlloc lifo;         // Provider of all heap memory used for compilation.
    void *func;             // Really, a ModuleCompiler::Func*
    jit::MIRGenerator *mir; // Passed from main thread to helper.
    jit::LIRGraph *lir;     // Passed from helper to main thread.
    unsigned compileTime;

    explicit AsmJSParallelTask(size_t defaultChunkSize)
      : runtime(nullptr), lifo(defaultChunkSize), func(nullptr), mir(nullptr), lir(nullptr), compileTime(0)
    { }

    void init(JSRuntime *rt, void *func, jit::MIRGenerator *mir) {
        this->runtime = rt;
        this->func = func;
        this->mir = mir;
        this->lir = nullptr;
    }
};
#endif

struct ParseTask
{
    ExclusiveContext *cx;
    OwningCompileOptions options;
    const jschar *chars;
    size_t length;
    LifoAlloc alloc;

    // Rooted pointer to the global object used by 'cx'.
    PersistentRootedObject exclusiveContextGlobal;

    // Saved GC-managed CompileOptions fields that will populate slots in
    // the ScriptSourceObject. We create the ScriptSourceObject in the
    // compilation's temporary compartment, so storing these values there
    // at that point would create cross-compartment references. Instead we
    // hold them here, and install them after merging the compartments.
    PersistentRootedObject optionsElement;
    PersistentRootedScript optionsIntroductionScript;

    // Callback invoked off the main thread when the parse finishes.
    JS::OffThreadCompileCallback callback;
    void *callbackData;

    // Holds the final script between the invocation of the callback and the
    // point where FinishOffThreadScript is called, which will destroy the
    // ParseTask.
    JSScript *script;

    // Any errors or warnings produced during compilation. These are reported
    // when finishing the script.
    Vector<frontend::CompileError *> errors;
    bool overRecursed;

    ParseTask(ExclusiveContext *cx, JSObject *exclusiveContextGlobal,
              JSContext *initCx, const jschar *chars, size_t length,
              JS::OffThreadCompileCallback callback, void *callbackData);
    bool init(JSContext *cx, const ReadOnlyCompileOptions &options);

    void activate(JSRuntime *rt);
    void finish();

    bool runtimeMatches(JSRuntime *rt) {
        return exclusiveContextGlobal->runtimeFromAnyThread() == rt;
    }

    ~ParseTask();
};

#ifdef JS_THREADSAFE
// Return whether, if a new parse task was started, it would need to wait for
// an in-progress GC to complete before starting.
extern bool
OffThreadParsingMustWaitForGC(JSRuntime *rt);
#endif

// Compression tasks are allocated on the stack by their triggering thread,
// which will block on the compression completing as the task goes out of scope
// to ensure it completes at the required time.
struct SourceCompressionTask
{
    friend class ScriptSource;
    friend class HelperThread;

#ifdef JS_THREADSAFE
    // Thread performing the compression.
    HelperThread *helperThread;
#endif

  private:
    // Context from the triggering thread. Don't use this off thread!
    ExclusiveContext *cx;

    ScriptSource *ss;

    // Atomic flag to indicate to a helper thread that it should abort
    // compression on the source.
    mozilla::Atomic<bool, mozilla::Relaxed> abort_;

    // Stores the result of the compression.
    enum ResultType {
        OOM,
        Aborted,
        Success
    } result;
    void *compressed;
    size_t compressedBytes;

  public:
    explicit SourceCompressionTask(ExclusiveContext *cx)
      : cx(cx), ss(nullptr), abort_(false),
        result(OOM), compressed(nullptr), compressedBytes(0)
    {
#ifdef JS_THREADSAFE
        helperThread = nullptr;
#endif
    }

    ~SourceCompressionTask()
    {
        complete();
    }

    ResultType work();
    bool complete();
    void abort() { abort_ = true; }
    bool active() const { return !!ss; }
    ScriptSource *source() { return ss; }
};

} /* namespace js */

#endif /* vm_HelperThreads_h */
