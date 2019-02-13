/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_TracingAPI_h
#define js_TracingAPI_h

#include "jsalloc.h"

#include "js/HashTable.h"
#include "js/HeapAPI.h"
#include "js/TraceKind.h"

class JS_PUBLIC_API(JSTracer);

namespace JS {
class JS_PUBLIC_API(CallbackTracer);
template <typename T> class Heap;
template <typename T> class TenuredHeap;

// Returns a static string equivalent of |kind|.
JS_FRIEND_API(const char*)
GCTraceKindToAscii(JS::TraceKind kind);

} // namespace JS

namespace js {
class BaseShape;
class LazyScript;
class ObjectGroup;
namespace jit {
class JitCode;
} // namespace jit
} // namespace js

enum WeakMapTraceKind {
    DoNotTraceWeakMaps = 0,
    TraceWeakMapValues = 1,
    TraceWeakMapKeysValues = 2
};

class JS_PUBLIC_API(JSTracer)
{
  public:
    // Return the runtime set on the tracer.
    JSRuntime* runtime() const { return runtime_; }

    // Return the weak map tracing behavior set on this tracer.
    WeakMapTraceKind eagerlyTraceWeakMaps() const { return eagerlyTraceWeakMaps_; }

    // An intermediate state on the road from C to C++ style dispatch.
    enum class TracerKindTag {
        Marking,
        Tenuring,
        Callback
    };
    bool isMarkingTracer() const { return tag_ == TracerKindTag::Marking; }
    bool isTenuringTracer() const { return tag_ == TracerKindTag::Tenuring; }
    bool isCallbackTracer() const { return tag_ == TracerKindTag::Callback; }
    inline JS::CallbackTracer* asCallbackTracer();

  protected:
    JSTracer(JSRuntime* rt, TracerKindTag tag,
             WeakMapTraceKind weakTraceKind = TraceWeakMapValues)
      : runtime_(rt), tag_(tag), eagerlyTraceWeakMaps_(weakTraceKind)
    {}

  private:
    JSRuntime*          runtime_;
    TracerKindTag       tag_;
    WeakMapTraceKind    eagerlyTraceWeakMaps_;
};

namespace JS {

class AutoTracingName;
class AutoTracingIndex;
class AutoTracingCallback;

class JS_PUBLIC_API(CallbackTracer) : public JSTracer
{
  public:
    CallbackTracer(JSRuntime* rt, WeakMapTraceKind weakTraceKind = TraceWeakMapValues)
      : JSTracer(rt, JSTracer::TracerKindTag::Callback, weakTraceKind),
        contextName_(nullptr), contextIndex_(InvalidIndex), contextFunctor_(nullptr)
    {}

    // Override these methods to receive notification when an edge is visited
    // with the type contained in the callback. The default implementation
    // dispatches to the fully-generic onChild implementation, so for cases that
    // do not care about boxing overhead and do not need the actual edges,
    // just override the generic onChild.
    virtual void onObjectEdge(JSObject** objp) { onChild(JS::GCCellPtr(*objp)); }
    virtual void onStringEdge(JSString** strp) { onChild(JS::GCCellPtr(*strp)); }
    virtual void onSymbolEdge(JS::Symbol** symp) { onChild(JS::GCCellPtr(*symp)); }
    virtual void onScriptEdge(JSScript** scriptp) { onChild(JS::GCCellPtr(*scriptp)); }
    virtual void onShapeEdge(js::Shape** shapep) {
        onChild(JS::GCCellPtr(*shapep, JS::TraceKind::Shape));
    }
    virtual void onObjectGroupEdge(js::ObjectGroup** groupp) {
        onChild(JS::GCCellPtr(*groupp, JS::TraceKind::ObjectGroup));
    }
    virtual void onBaseShapeEdge(js::BaseShape** basep) {
        onChild(JS::GCCellPtr(*basep, JS::TraceKind::BaseShape));
    }
    virtual void onJitCodeEdge(js::jit::JitCode** codep) {
        onChild(JS::GCCellPtr(*codep, JS::TraceKind::JitCode));
    }
    virtual void onLazyScriptEdge(js::LazyScript** lazyp) {
        onChild(JS::GCCellPtr(*lazyp, JS::TraceKind::LazyScript));
    }

    // Override this method to receive notification when a node in the GC
    // heap graph is visited.
    virtual void onChild(const JS::GCCellPtr& thing) = 0;

    // Access to the tracing context:
    // When tracing with a JS::CallbackTracer, we invoke the callback with the
    // edge location and the type of target. This is useful for operating on
    // the edge in the abstract or on the target thing, satisfying most common
    // use cases.  However, some tracers need additional detail about the
    // specific edge that is being traced in order to be useful. Unfortunately,
    // the raw pointer to the edge that we provide is not enough information to
    // infer much of anything useful about that edge.
    //
    // In order to better support use cases that care in particular about edges
    // -- as opposed to the target thing -- tracing implementations are
    // responsible for providing extra context information about each edge they
    // trace, as it is traced. This contains, at a minimum, an edge name and,
    // when tracing an array, the index. Further specialization can be achived
    // (with some complexity), by associating a functor with the tracer so
    // that, when requested, the user can generate totally custom edge
    // descriptions.

    // Returns the current edge's name. It is only valid to call this when
    // inside the trace callback, however, the edge name will always be set.
    const char* contextName() const { MOZ_ASSERT(contextName_); return contextName_; }

    // Returns the current edge's index, if marked as part of an array of edges.
    // This must be called only inside the trace callback. When not tracing an
    // array, the value will be InvalidIndex.
    const static size_t InvalidIndex = size_t(-1);
    size_t contextIndex() const { return contextIndex_; }

    // Build a description of this edge in the heap graph. This call may invoke
    // the context functor, if set, which may inspect arbitrary areas of the
    // heap. On the other hand, the description provided by this method may be
    // substantially more accurate and useful than those provided by only the
    // contextName and contextIndex.
    void getTracingEdgeName(char* buffer, size_t bufferSize);

    // The trace implementation may associate a callback with one or more edges
    // using AutoTracingDetails. This functor is called by getTracingEdgeName
    // and is responsible for providing a textual representation of the
    // currently being traced edge. The callback has access to the full heap,
    // including the currently set tracing context.
    class ContextFunctor {
      public:
        virtual void operator()(CallbackTracer* trc, char* buf, size_t bufsize) = 0;
    };

#ifdef DEBUG
    enum class TracerKind { DoNotCare, Moving, GrayBuffering, VerifyTraceProtoAndIface };
    virtual TracerKind getTracerKind() const { return TracerKind::DoNotCare; }
#endif

    // In C++, overriding a method hides all methods in the base class with
    // that name, not just methods with that signature. Thus, the typed edge
    // methods have to have distinct names to allow us to override them
    // individually, which is freqently useful if, for example, we only want to
    // process only one type of edge.
    void dispatchToOnEdge(JSObject** objp) { onObjectEdge(objp); }
    void dispatchToOnEdge(JSString** strp) { onStringEdge(strp); }
    void dispatchToOnEdge(JS::Symbol** symp) { onSymbolEdge(symp); }
    void dispatchToOnEdge(JSScript** scriptp) { onScriptEdge(scriptp); }
    void dispatchToOnEdge(js::Shape** shapep) { onShapeEdge(shapep); }
    void dispatchToOnEdge(js::ObjectGroup** groupp) { onObjectGroupEdge(groupp); }
    void dispatchToOnEdge(js::BaseShape** basep) { onBaseShapeEdge(basep); }
    void dispatchToOnEdge(js::jit::JitCode** codep) { onJitCodeEdge(codep); }
    void dispatchToOnEdge(js::LazyScript** lazyp) { onLazyScriptEdge(lazyp); }

  private:
    friend class AutoTracingName;
    const char* contextName_;

    friend class AutoTracingIndex;
    size_t contextIndex_;

    friend class AutoTracingDetails;
    ContextFunctor* contextFunctor_;
};

// Set the name portion of the tracer's context for the current edge.
class AutoTracingName
{
    CallbackTracer* trc_;
    const char* prior_;

  public:
    AutoTracingName(CallbackTracer* trc, const char* name) : trc_(trc), prior_(trc->contextName_) {
        MOZ_ASSERT(name);
        trc->contextName_ = name;
    }
    ~AutoTracingName() {
        MOZ_ASSERT(trc_->contextName_);
        trc_->contextName_ = prior_;
    }
};

// Set the index portion of the tracer's context for the current range.
class AutoTracingIndex
{
    CallbackTracer* trc_;

  public:
    explicit AutoTracingIndex(JSTracer* trc, size_t initial = 0) : trc_(nullptr) {
        if (trc->isCallbackTracer()) {
            trc_ = trc->asCallbackTracer();
            MOZ_ASSERT(trc_->contextIndex_ == CallbackTracer::InvalidIndex);
            trc_->contextIndex_ = initial;
        }
    }
    ~AutoTracingIndex() {
        if (trc_) {
            MOZ_ASSERT(trc_->contextIndex_ != CallbackTracer::InvalidIndex);
            trc_->contextIndex_ = CallbackTracer::InvalidIndex;
        }
    }

    void operator++() {
        if (trc_) {
            MOZ_ASSERT(trc_->contextIndex_ != CallbackTracer::InvalidIndex);
            ++trc_->contextIndex_;
        }
    }
};

// Set a context callback for the trace callback to use, if it needs a detailed
// edge description.
class AutoTracingDetails
{
    CallbackTracer* trc_;

  public:
    AutoTracingDetails(JSTracer* trc, CallbackTracer::ContextFunctor& func) : trc_(nullptr) {
        if (trc->isCallbackTracer()) {
            trc_ = trc->asCallbackTracer();
            MOZ_ASSERT(trc_->contextFunctor_ == nullptr);
            trc_->contextFunctor_ = &func;
        }
    }
    ~AutoTracingDetails() {
        if (trc_) {
            MOZ_ASSERT(trc_->contextFunctor_);
            trc_->contextFunctor_ = nullptr;
        }
    }
};

} // namespace JS

JS::CallbackTracer*
JSTracer::asCallbackTracer()
{
    MOZ_ASSERT(isCallbackTracer());
    return static_cast<JS::CallbackTracer*>(this);
}

// The JS_Call*Tracer family of functions traces the given GC thing reference.
// This performs the tracing action configured on the given JSTracer:
// typically calling the JSTracer::callback or marking the thing as live.
//
// The argument to JS_Call*Tracer is an in-out param: when the function
// returns, the garbage collector might have moved the GC thing. In this case,
// the reference passed to JS_Call*Tracer will be updated to the object's new
// location. Callers of this method are responsible for updating any state
// that is dependent on the object's address. For example, if the object's
// address is used as a key in a hashtable, then the object must be removed
// and re-inserted with the correct hash.
//
extern JS_PUBLIC_API(void)
JS_CallValueTracer(JSTracer* trc, JS::Heap<JS::Value>* valuep, const char* name);

extern JS_PUBLIC_API(void)
JS_CallIdTracer(JSTracer* trc, JS::Heap<jsid>* idp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallObjectTracer(JSTracer* trc, JS::Heap<JSObject*>* objp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallStringTracer(JSTracer* trc, JS::Heap<JSString*>* strp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallScriptTracer(JSTracer* trc, JS::Heap<JSScript*>* scriptp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallFunctionTracer(JSTracer* trc, JS::Heap<JSFunction*>* funp, const char* name);

// The following JS_CallUnbarriered*Tracer functions should only be called where
// you know for sure that a heap post barrier is not required.  Use with extreme
// caution!
extern JS_PUBLIC_API(void)
JS_CallUnbarrieredValueTracer(JSTracer* trc, JS::Value* valuep, const char* name);

extern JS_PUBLIC_API(void)
JS_CallUnbarrieredIdTracer(JSTracer* trc, jsid* idp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallUnbarrieredObjectTracer(JSTracer* trc, JSObject** objp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallUnbarrieredStringTracer(JSTracer* trc, JSString** strp, const char* name);

extern JS_PUBLIC_API(void)
JS_CallUnbarrieredScriptTracer(JSTracer* trc, JSScript** scriptp, const char* name);

template <typename HashSetEnum>
inline void
JS_CallHashSetObjectTracer(JSTracer* trc, HashSetEnum& e, JSObject* const& key, const char* name)
{
    JSObject* updated = key;
    JS_CallUnbarrieredObjectTracer(trc, &updated, name);
    if (updated != key)
        e.rekeyFront(updated);
}

// Trace an object that is known to always be tenured.  No post barriers are
// required in this case.
extern JS_PUBLIC_API(void)
JS_CallTenuredObjectTracer(JSTracer* trc, JS::TenuredHeap<JSObject*>* objp, const char* name);

extern JS_PUBLIC_API(void)
JS_TraceChildren(JSTracer* trc, void* thing, JS::TraceKind kind);

extern JS_PUBLIC_API(void)
JS_TraceRuntime(JSTracer* trc);

namespace JS {
typedef js::HashSet<Zone*, js::DefaultHasher<Zone*>, js::SystemAllocPolicy> ZoneSet;
}

// Trace every value within |zones| that is wrapped by a cross-compartment
// wrapper from a zone that is not an element of |zones|.
extern JS_PUBLIC_API(void)
JS_TraceIncomingCCWs(JSTracer* trc, const JS::ZoneSet& zones);

extern JS_PUBLIC_API(void)
JS_GetTraceThingInfo(char* buf, size_t bufsize, JSTracer* trc,
                     void* thing, JS::TraceKind kind, bool includeDetails);

#endif /* js_TracingAPI_h */
