/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Tracer.h"

#include "mozilla/DebugOnly.h"

#include "jsapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jsutil.h"
#include "NamespaceImports.h"

#include "gc/GCInternals.h"
#include "gc/Marking.h"

#include "vm/Symbol.h"

#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;
using mozilla::DebugOnly;

JS_PUBLIC_API(void)
JS_CallValueTracer(JSTracer *trc, Value *valuep, const char *name)
{
    MarkValueUnbarriered(trc, valuep, name);
}

JS_PUBLIC_API(void)
JS_CallIdTracer(JSTracer *trc, jsid *idp, const char *name)
{
    MarkIdUnbarriered(trc, idp, name);
}

JS_PUBLIC_API(void)
JS_CallObjectTracer(JSTracer *trc, JSObject **objp, const char *name)
{
    MarkObjectUnbarriered(trc, objp, name);
}

JS_PUBLIC_API(void)
JS_CallStringTracer(JSTracer *trc, JSString **strp, const char *name)
{
    MarkStringUnbarriered(trc, strp, name);
}

JS_PUBLIC_API(void)
JS_CallScriptTracer(JSTracer *trc, JSScript **scriptp, const char *name)
{
    MarkScriptUnbarriered(trc, scriptp, name);
}

JS_PUBLIC_API(void)
JS_CallHeapValueTracer(JSTracer *trc, JS::Heap<JS::Value> *valuep, const char *name)
{
    MarkValueUnbarriered(trc, valuep->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallHeapIdTracer(JSTracer *trc, JS::Heap<jsid> *idp, const char *name)
{
    MarkIdUnbarriered(trc, idp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallHeapObjectTracer(JSTracer *trc, JS::Heap<JSObject *> *objp, const char *name)
{
    MarkObjectUnbarriered(trc, objp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallHeapStringTracer(JSTracer *trc, JS::Heap<JSString *> *strp, const char *name)
{
    MarkStringUnbarriered(trc, strp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallHeapScriptTracer(JSTracer *trc, JS::Heap<JSScript *> *scriptp, const char *name)
{
    MarkScriptUnbarriered(trc, scriptp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallHeapFunctionTracer(JSTracer *trc, JS::Heap<JSFunction *> *funp, const char *name)
{
    MarkObjectUnbarriered(trc, funp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallTenuredObjectTracer(JSTracer *trc, JS::TenuredHeap<JSObject *> *objp, const char *name)
{
    JSObject *obj = objp->getPtr();
    if (!obj)
        return;

    trc->setTracingLocation((void*)objp);
    MarkObjectUnbarriered(trc, &obj, name);

    objp->setPtr(obj);
}

JS_PUBLIC_API(void)
JS_TraceChildren(JSTracer *trc, void *thing, JSGCTraceKind kind)
{
    js::TraceChildren(trc, thing, kind);
}

JS_PUBLIC_API(void)
JS_TraceRuntime(JSTracer *trc)
{
    AssertHeapIsIdle(trc->runtime());
    TraceRuntime(trc);
}

static size_t
CountDecimalDigits(size_t num)
{
    size_t numDigits = 0;
    do {
        num /= 10;
        numDigits++;
    } while (num > 0);

    return numDigits;
}

JS_PUBLIC_API(void)
JS_GetTraceThingInfo(char *buf, size_t bufsize, JSTracer *trc, void *thing,
                     JSGCTraceKind kind, bool details)
{
    const char *name = nullptr; /* silence uninitialized warning */
    size_t n;

    if (bufsize == 0)
        return;

    switch (kind) {
      case JSTRACE_OBJECT:
      {
        name = static_cast<JSObject *>(thing)->getClass()->name;
        break;
      }

      case JSTRACE_STRING:
        name = ((JSString *)thing)->isDependent()
               ? "substring"
               : "string";
        break;

      case JSTRACE_SYMBOL:
        name = "symbol";
        break;

      case JSTRACE_SCRIPT:
        name = "script";
        break;

      case JSTRACE_LAZY_SCRIPT:
        name = "lazyscript";
        break;

      case JSTRACE_JITCODE:
        name = "jitcode";
        break;

      case JSTRACE_SHAPE:
        name = "shape";
        break;

      case JSTRACE_BASE_SHAPE:
        name = "base_shape";
        break;

      case JSTRACE_TYPE_OBJECT:
        name = "type_object";
        break;
    }

    n = strlen(name);
    if (n > bufsize - 1)
        n = bufsize - 1;
    js_memcpy(buf, name, n + 1);
    buf += n;
    bufsize -= n;
    *buf = '\0';

    if (details && bufsize > 2) {
        switch (kind) {
          case JSTRACE_OBJECT:
          {
            JSObject *obj = (JSObject *)thing;
            if (obj->is<JSFunction>()) {
                JSFunction *fun = &obj->as<JSFunction>();
                if (fun->displayAtom()) {
                    *buf++ = ' ';
                    bufsize--;
                    PutEscapedString(buf, bufsize, fun->displayAtom(), 0);
                }
            } else if (obj->getClass()->flags & JSCLASS_HAS_PRIVATE) {
                JS_snprintf(buf, bufsize, " %p", obj->getPrivate());
            } else {
                JS_snprintf(buf, bufsize, " <no private>");
            }
            break;
          }

          case JSTRACE_STRING:
          {
            *buf++ = ' ';
            bufsize--;
            JSString *str = (JSString *)thing;

            if (str->isLinear()) {
                bool willFit = str->length() + strlen("<length > ") +
                               CountDecimalDigits(str->length()) < bufsize;

                n = JS_snprintf(buf, bufsize, "<length %d%s> ",
                                (int)str->length(),
                                willFit ? "" : " (truncated)");
                buf += n;
                bufsize -= n;

                PutEscapedString(buf, bufsize, &str->asLinear(), 0);
            } else {
                JS_snprintf(buf, bufsize, "<rope: length %d>", (int)str->length());
            }
            break;
          }

          case JSTRACE_SYMBOL:
          {
            JS::Symbol *sym = static_cast<JS::Symbol *>(thing);
            if (JSString *desc = sym->description()) {
                if (desc->isLinear()) {
                    *buf++ = ' ';
                    bufsize--;
                    PutEscapedString(buf, bufsize, &desc->asLinear(), 0);
                } else {
                    JS_snprintf(buf, bufsize, "<nonlinear desc>");
                }
            } else {
                JS_snprintf(buf, bufsize, "<null>");
            }
            break;
          }

          case JSTRACE_SCRIPT:
          {
            JSScript *script = static_cast<JSScript *>(thing);
            JS_snprintf(buf, bufsize, " %s:%u", script->filename(), unsigned(script->lineno()));
            break;
          }

          case JSTRACE_LAZY_SCRIPT:
          case JSTRACE_JITCODE:
          case JSTRACE_SHAPE:
          case JSTRACE_BASE_SHAPE:
          case JSTRACE_TYPE_OBJECT:
            break;
        }
    }
    buf[bufsize - 1] = '\0';
}

JSTracer::JSTracer(JSRuntime *rt, JSTraceCallback traceCallback,
                   WeakMapTraceKind weakTraceKind /* = TraceWeakMapValues */)
  : callback(traceCallback)
  , runtime_(rt)
  , debugPrinter_(nullptr)
  , debugPrintArg_(nullptr)
  , debugPrintIndex_(size_t(-1))
  , eagerlyTraceWeakMaps_(weakTraceKind)
#ifdef JS_GC_ZEAL
  , realLocation_(nullptr)
#endif
{
}

bool
JSTracer::hasTracingDetails() const
{
    return debugPrinter_ || debugPrintArg_;
}

const char *
JSTracer::tracingName(const char *fallback) const
{
    JS_ASSERT(hasTracingDetails());
    return debugPrinter_ ? fallback : (const char *)debugPrintArg_;
}

const char *
JSTracer::getTracingEdgeName(char *buffer, size_t bufferSize)
{
    if (debugPrinter_) {
        debugPrinter_(this, buffer, bufferSize);
        return buffer;
    }
    if (debugPrintIndex_ != size_t(-1)) {
        JS_snprintf(buffer, bufferSize, "%s[%lu]",
                    (const char *)debugPrintArg_,
                    debugPrintIndex_);
        return buffer;
    }
    return (const char*)debugPrintArg_;
}

JSTraceNamePrinter
JSTracer::debugPrinter() const
{
    return debugPrinter_;
}

const void *
JSTracer::debugPrintArg() const
{
    return debugPrintArg_;
}

size_t
JSTracer::debugPrintIndex() const
{
    return debugPrintIndex_;
}

void
JSTracer::setTraceCallback(JSTraceCallback traceCallback)
{
    callback = traceCallback;
}

#ifdef JS_GC_ZEAL
void
JSTracer::setTracingLocation(void *location)
{
    if (!realLocation_ || !location)
        realLocation_ = location;
}

void
JSTracer::unsetTracingLocation()
{
    realLocation_ = nullptr;
}

void **
JSTracer::tracingLocation(void **thingp)
{
    return realLocation_ ? (void **)realLocation_ : thingp;
}
#endif

bool
MarkStack::init(JSGCMode gcMode)
{
    setBaseCapacity(gcMode);

    JS_ASSERT(!stack_);
    uintptr_t *newStack = js_pod_malloc<uintptr_t>(baseCapacity_);
    if (!newStack)
        return false;

    setStack(newStack, 0, baseCapacity_);
    return true;
}

void
MarkStack::setBaseCapacity(JSGCMode mode)
{
    switch (mode) {
      case JSGC_MODE_GLOBAL:
      case JSGC_MODE_COMPARTMENT:
        baseCapacity_ = NON_INCREMENTAL_MARK_STACK_BASE_CAPACITY;
        break;
      case JSGC_MODE_INCREMENTAL:
        baseCapacity_ = INCREMENTAL_MARK_STACK_BASE_CAPACITY;
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("bad gc mode");
    }

    if (baseCapacity_ > maxCapacity_)
        baseCapacity_ = maxCapacity_;
}

void
MarkStack::setMaxCapacity(size_t maxCapacity)
{
    JS_ASSERT(isEmpty());
    maxCapacity_ = maxCapacity;
    if (baseCapacity_ > maxCapacity_)
        baseCapacity_ = maxCapacity_;

    reset();
}

void
MarkStack::reset()
{
    if (capacity() == baseCapacity_) {
        // No size change; keep the current stack.
        setStack(stack_, 0, baseCapacity_);
        return;
    }

    uintptr_t *newStack = (uintptr_t *)js_realloc(stack_, sizeof(uintptr_t) * baseCapacity_);
    if (!newStack) {
        // If the realloc fails, just keep using the existing stack; it's
        // not ideal but better than failing.
        newStack = stack_;
        baseCapacity_ = capacity();
    }
    setStack(newStack, 0, baseCapacity_);
}

bool
MarkStack::enlarge(unsigned count)
{
    size_t newCapacity = Min(maxCapacity_, capacity() * 2);
    if (newCapacity < capacity() + count)
        return false;

    size_t tosIndex = position();

    uintptr_t *newStack = (uintptr_t *)js_realloc(stack_, sizeof(uintptr_t) * newCapacity);
    if (!newStack)
        return false;

    setStack(newStack, tosIndex, newCapacity);
    return true;
}

void
MarkStack::setGCMode(JSGCMode gcMode)
{
    // The mark stack won't be resized until the next call to reset(), but
    // that will happen at the end of the next GC.
    setBaseCapacity(gcMode);
}

size_t
MarkStack::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(stack_);
}

/*
 * DoNotTraceWeakMaps: the GC is recomputing the liveness of WeakMap entries,
 * so we delay visting entries.
 */
GCMarker::GCMarker(JSRuntime *rt)
  : JSTracer(rt, nullptr, DoNotTraceWeakMaps),
    stack(size_t(-1)),
    color(BLACK),
    unmarkedArenaStackTop(nullptr),
    markLaterArenas(0),
    grayBufferState(GRAY_BUFFER_UNUSED),
    started(false)
{
}

bool
GCMarker::init(JSGCMode gcMode)
{
    return stack.init(gcMode);
}

void
GCMarker::start()
{
    JS_ASSERT(!started);
    started = true;
    color = BLACK;

    JS_ASSERT(!unmarkedArenaStackTop);
    JS_ASSERT(markLaterArenas == 0);

}

void
GCMarker::stop()
{
    JS_ASSERT(isDrained());

    JS_ASSERT(started);
    started = false;

    JS_ASSERT(!unmarkedArenaStackTop);
    JS_ASSERT(markLaterArenas == 0);

    /* Free non-ballast stack memory. */
    stack.reset();

    resetBufferedGrayRoots();
    grayBufferState = GRAY_BUFFER_UNUSED;
}

void
GCMarker::reset()
{
    color = BLACK;

    stack.reset();
    JS_ASSERT(isMarkStackEmpty());

    while (unmarkedArenaStackTop) {
        ArenaHeader *aheader = unmarkedArenaStackTop;
        JS_ASSERT(aheader->hasDelayedMarking);
        JS_ASSERT(markLaterArenas);
        unmarkedArenaStackTop = aheader->getNextDelayedMarking();
        aheader->unsetDelayedMarking();
        aheader->markOverflow = 0;
        aheader->allocatedDuringIncremental = 0;
        markLaterArenas--;
    }
    JS_ASSERT(isDrained());
    JS_ASSERT(!markLaterArenas);
}

void
GCMarker::markDelayedChildren(ArenaHeader *aheader)
{
    if (aheader->markOverflow) {
        bool always = aheader->allocatedDuringIncremental;
        aheader->markOverflow = 0;

        for (ArenaCellIterUnderGC i(aheader); !i.done(); i.next()) {
            Cell *t = i.getCell();
            if (always || t->isMarked()) {
                t->markIfUnmarked();
                JS_TraceChildren(this, t, MapAllocToTraceKind(aheader->getAllocKind()));
            }
        }
    } else {
        JS_ASSERT(aheader->allocatedDuringIncremental);
        PushArena(this, aheader);
    }
    aheader->allocatedDuringIncremental = 0;
    /*
     * Note that during an incremental GC we may still be allocating into
     * aheader. However, prepareForIncrementalGC sets the
     * allocatedDuringIncremental flag if we continue marking.
     */
}

bool
GCMarker::markDelayedChildren(SliceBudget &budget)
{
    gcstats::MaybeAutoPhase ap;
    if (runtime()->gc.state() == MARK)
        ap.construct(runtime()->gc.stats, gcstats::PHASE_MARK_DELAYED);

    JS_ASSERT(unmarkedArenaStackTop);
    do {
        /*
         * If marking gets delayed at the same arena again, we must repeat
         * marking of its things. For that we pop arena from the stack and
         * clear its hasDelayedMarking flag before we begin the marking.
         */
        ArenaHeader *aheader = unmarkedArenaStackTop;
        JS_ASSERT(aheader->hasDelayedMarking);
        JS_ASSERT(markLaterArenas);
        unmarkedArenaStackTop = aheader->getNextDelayedMarking();
        aheader->unsetDelayedMarking();
        markLaterArenas--;
        markDelayedChildren(aheader);

        budget.step(150);
        if (budget.isOverBudget())
            return false;
    } while (unmarkedArenaStackTop);
    JS_ASSERT(!markLaterArenas);

    return true;
}

#ifdef DEBUG
void
GCMarker::checkZone(void *p)
{
    JS_ASSERT(started);
    DebugOnly<Cell *> cell = static_cast<Cell *>(p);
    JS_ASSERT_IF(cell->isTenured(), cell->tenuredZone()->isCollecting());
}
#endif

bool
GCMarker::hasBufferedGrayRoots() const
{
    return grayBufferState == GRAY_BUFFER_OK;
}

void
GCMarker::startBufferingGrayRoots()
{
    JS_ASSERT(grayBufferState == GRAY_BUFFER_UNUSED);
    grayBufferState = GRAY_BUFFER_OK;
    for (GCZonesIter zone(runtime()); !zone.done(); zone.next())
        JS_ASSERT(zone->gcGrayRoots.empty());

    JS_ASSERT(!callback);
    callback = GrayCallback;
    JS_ASSERT(IS_GC_MARKING_TRACER(this));
}

void
GCMarker::endBufferingGrayRoots()
{
    JS_ASSERT(callback == GrayCallback);
    callback = nullptr;
    JS_ASSERT(IS_GC_MARKING_TRACER(this));
    JS_ASSERT(grayBufferState == GRAY_BUFFER_OK ||
              grayBufferState == GRAY_BUFFER_FAILED);
}

void
GCMarker::resetBufferedGrayRoots()
{
    for (GCZonesIter zone(runtime()); !zone.done(); zone.next())
        zone->gcGrayRoots.clearAndFree();
}

void
GCMarker::markBufferedGrayRoots(JS::Zone *zone)
{
    JS_ASSERT(grayBufferState == GRAY_BUFFER_OK);
    JS_ASSERT(zone->isGCMarkingGray());

    for (GrayRoot *elem = zone->gcGrayRoots.begin(); elem != zone->gcGrayRoots.end(); elem++) {
#ifdef DEBUG
        setTracingDetails(elem->debugPrinter, elem->debugPrintArg, elem->debugPrintIndex);
#endif
        void *tmp = elem->thing;
        setTracingLocation((void *)&elem->thing);
        MarkKind(this, &tmp, elem->kind);
        JS_ASSERT(tmp == elem->thing);
    }
}

void
GCMarker::appendGrayRoot(void *thing, JSGCTraceKind kind)
{
    JS_ASSERT(started);

    if (grayBufferState == GRAY_BUFFER_FAILED)
        return;

    GrayRoot root(thing, kind);
#ifdef DEBUG
    root.debugPrinter = debugPrinter();
    root.debugPrintArg = debugPrintArg();
    root.debugPrintIndex = debugPrintIndex();
#endif

    Zone *zone = static_cast<Cell *>(thing)->tenuredZone();
    if (zone->isCollecting()) {
        zone->maybeAlive = true;
        if (!zone->gcGrayRoots.append(root)) {
            resetBufferedGrayRoots();
            grayBufferState = GRAY_BUFFER_FAILED;
        }
    }
}

void
GCMarker::GrayCallback(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    JS_ASSERT(thingp);
    JS_ASSERT(*thingp);
    GCMarker *gcmarker = static_cast<GCMarker *>(trc);
    gcmarker->appendGrayRoot(*thingp, kind);
}

size_t
GCMarker::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t size = stack.sizeOfExcludingThis(mallocSizeOf);
    for (ZonesIter zone(runtime(), WithAtoms); !zone.done(); zone.next())
        size += zone->gcGrayRoots.sizeOfExcludingThis(mallocSizeOf);
    return size;
}

void
js::SetMarkStackLimit(JSRuntime *rt, size_t limit)
{
    JS_ASSERT(!rt->isHeapBusy());
    AutoStopVerifyingBarriers pauseVerification(rt, false);
    rt->gc.marker.setMaxCapacity(limit);
}

