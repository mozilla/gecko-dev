/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ArrayBufferObject.h"

#include "mozilla/Alignment.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"

#include <string.h>
#ifndef XP_WIN
# include <sys/mman.h>
#endif

#ifdef MOZ_VALGRIND
# include <valgrind/memcheck.h>
#endif

#include "jsapi.h"
#include "jsarray.h"
#include "jscntxt.h"
#include "jscpucfg.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jstypes.h"
#include "jsutil.h"
#ifdef XP_WIN
# include "jswin.h"
#endif
#include "jswrapper.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "gc/Memory.h"
#include "jit/AsmJS.h"
#include "jit/AsmJSModule.h"
#include "js/MemoryMetrics.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/NumericConversions.h"
#include "vm/SharedArrayObject.h"
#include "vm/WrapperObject.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/Shape-inl.h"

using mozilla::DebugOnly;

using namespace js;
using namespace js::gc;
using namespace js::types;

/*
 * Convert |v| to an array index for an array of length |length| per
 * the Typed Array Specification section 7.0, |subarray|. If successful,
 * the output value is in the range [0, length].
 */
bool
js::ToClampedIndex(JSContext *cx, HandleValue v, uint32_t length, uint32_t *out)
{
    int32_t result;
    if (!ToInt32(cx, v, &result))
        return false;
    if (result < 0) {
        result += length;
        if (result < 0)
            result = 0;
    } else if (uint32_t(result) > length) {
        result = length;
    }
    *out = uint32_t(result);
    return true;
}

/*
 * ArrayBufferObject
 *
 * This class holds the underlying raw buffer that the TypedArrayObject classes
 * access.  It can be created explicitly and passed to a TypedArrayObject, or
 * can be created implicitly by constructing a TypedArrayObject with a size.
 */

/*
 * ArrayBufferObject (base)
 */

const Class ArrayBufferObject::protoClass = {
    "ArrayBufferPrototype",
    JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayBuffer),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

const Class ArrayBufferObject::class_ = {
    "ArrayBuffer",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayBuffer) |
    JSCLASS_BACKGROUND_FINALIZE,
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    ArrayBufferObject::finalize,
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    ArrayBufferObject::obj_trace,
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT
};

const JSFunctionSpec ArrayBufferObject::jsfuncs[] = {
    JS_FN("slice", ArrayBufferObject::fun_slice, 2, JSFUN_GENERIC_NATIVE),
    JS_FS_END
};

const JSFunctionSpec ArrayBufferObject::jsstaticfuncs[] = {
    JS_FN("isView", ArrayBufferObject::fun_isView, 1, 0),
    JS_FS_END
};

bool
js::IsArrayBuffer(HandleValue v)
{
    return v.isObject() &&
           (v.toObject().is<ArrayBufferObject>() ||
            v.toObject().is<SharedArrayBufferObject>());
}

bool
js::IsArrayBuffer(HandleObject obj)
{
    return obj->is<ArrayBufferObject>() || obj->is<SharedArrayBufferObject>();
}

bool
js::IsArrayBuffer(JSObject *obj)
{
    return obj->is<ArrayBufferObject>() || obj->is<SharedArrayBufferObject>();
}

ArrayBufferObject &
js::AsArrayBuffer(HandleObject obj)
{
    JS_ASSERT(IsArrayBuffer(obj));
    if (obj->is<SharedArrayBufferObject>())
        return obj->as<SharedArrayBufferObject>();
    return obj->as<ArrayBufferObject>();
}

ArrayBufferObject &
js::AsArrayBuffer(JSObject *obj)
{
    JS_ASSERT(IsArrayBuffer(obj));
    if (obj->is<SharedArrayBufferObject>())
        return obj->as<SharedArrayBufferObject>();
    return obj->as<ArrayBufferObject>();
}

MOZ_ALWAYS_INLINE bool
ArrayBufferObject::byteLengthGetterImpl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsArrayBuffer(args.thisv()));
    args.rval().setInt32(args.thisv().toObject().as<ArrayBufferObject>().byteLength());
    return true;
}

bool
ArrayBufferObject::byteLengthGetter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsArrayBuffer, byteLengthGetterImpl>(cx, args);
}

bool
ArrayBufferObject::fun_slice_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsArrayBuffer(args.thisv()));

    Rooted<ArrayBufferObject*> thisObj(cx, &args.thisv().toObject().as<ArrayBufferObject>());

    // these are the default values
    uint32_t length = thisObj->byteLength();
    uint32_t begin = 0, end = length;

    if (args.length() > 0) {
        if (!ToClampedIndex(cx, args[0], length, &begin))
            return false;

        if (args.length() > 1) {
            if (!ToClampedIndex(cx, args[1], length, &end))
                return false;
        }
    }

    if (begin > end)
        begin = end;

    JSObject *nobj = createSlice(cx, thisObj, begin, end);
    if (!nobj)
        return false;
    args.rval().setObject(*nobj);
    return true;
}

bool
ArrayBufferObject::fun_slice(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsArrayBuffer, fun_slice_impl>(cx, args);
}

/*
 * ArrayBuffer.isView(obj); ES6 (Dec 2013 draft) 24.1.3.1
 */
bool
ArrayBufferObject::fun_isView(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(args.get(0).isObject() &&
                           JS_IsArrayBufferViewObject(&args.get(0).toObject()));
    return true;
}

/*
 * new ArrayBuffer(byteLength)
 */
bool
ArrayBufferObject::class_constructor(JSContext *cx, unsigned argc, Value *vp)
{
    int32_t nbytes = 0;
    CallArgs args = CallArgsFromVp(argc, vp);
    if (argc > 0 && !ToInt32(cx, args[0], &nbytes))
        return false;

    if (nbytes < 0) {
        /*
         * We're just not going to support arrays that are bigger than what will fit
         * as an integer value; if someone actually ever complains (validly), then we
         * can fix.
         */
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_ARRAY_LENGTH);
        return false;
    }

    JSObject *bufobj = create(cx, uint32_t(nbytes));
    if (!bufobj)
        return false;
    args.rval().setObject(*bufobj);
    return true;
}

/*
 * Note that some callers are allowed to pass in a nullptr cx, so we allocate
 * with the cx if available and fall back to the runtime.  If oldptr is given,
 * it's expected to be a previously-allocated contents pointer that we then
 * realloc.
 */
static void *
AllocateArrayBufferContents(JSContext *maybecx, uint32_t nbytes, void *oldptr = nullptr, size_t oldnbytes = 0)
{
    void *p;

    // if oldptr is given, then we need to do a realloc
    if (oldptr) {
        p = maybecx ? maybecx->runtime()->reallocCanGC(oldptr, nbytes) : js_realloc(oldptr, nbytes);

        // if we grew the array, we need to set the new bytes to 0
        if (p && nbytes > oldnbytes)
            memset(reinterpret_cast<uint8_t *>(p) + oldnbytes, 0, nbytes - oldnbytes);
    } else {
        p = maybecx ? maybecx->runtime()->callocCanGC(nbytes) : js_calloc(nbytes);
    }

    if (!p && maybecx)
        js_ReportOutOfMemory(maybecx);

    return p;
}

ArrayBufferViewObject *
ArrayBufferObject::viewList() const
{
    return reinterpret_cast<ArrayBufferViewObject *>(getSlot(VIEW_LIST_SLOT).toPrivate());
}

void
ArrayBufferObject::setViewListNoBarrier(ArrayBufferViewObject *viewsHead)
{
    setSlot(VIEW_LIST_SLOT, PrivateValue(viewsHead));
}

void
ArrayBufferObject::setViewList(ArrayBufferViewObject *viewsHead)
{
    if (ArrayBufferViewObject *oldHead = viewList())
        ArrayBufferViewObject::writeBarrierPre(oldHead);
    setViewListNoBarrier(viewsHead);
    PostBarrierTypedArrayObject(this);
}

bool
ArrayBufferObject::canNeuter(JSContext *cx)
{
    if (isSharedArrayBuffer())
        return false;

    if (isAsmJSArrayBuffer()) {
        if (!ArrayBufferObject::canNeuterAsmJSArrayBuffer(cx, *this))
            return false;
    }

    return true;
}

/* static */ void
ArrayBufferObject::neuter(JSContext *cx, Handle<ArrayBufferObject*> buffer, void *newData)
{
    JS_ASSERT(buffer->canNeuter(cx));

    // Neuter all views on the buffer, clear out the list of views and the
    // buffer's data.

    for (ArrayBufferViewObject *view = buffer->viewList(); view; view = view->nextView()) {
        view->neuter(newData);

        // Notify compiled jit code that the base pointer has moved.
        MarkObjectStateChange(cx, view);
    }

    if (newData != buffer->dataPointer())
        buffer->setNewOwnedData(cx->runtime()->defaultFreeOp(), newData);

    buffer->setByteLength(0);
    buffer->setViewList(nullptr);
    buffer->setIsNeutered();

    // If this is happening during an incremental GC, remove the buffer from
    // the list of live buffers with multiple views if necessary.
    if (buffer->inLiveList()) {
        ArrayBufferVector &gcLiveArrayBuffers = cx->compartment()->gcLiveArrayBuffers;
        DebugOnly<bool> found = false;
        for (size_t i = 0; i < gcLiveArrayBuffers.length(); i++) {
            if (buffer == gcLiveArrayBuffers[i]) {
                found = true;
                gcLiveArrayBuffers[i] = gcLiveArrayBuffers.back();
                gcLiveArrayBuffers.popBack();
                break;
            }
        }
        JS_ASSERT(found);
        buffer->setInLiveList(false);
    }
}

void
ArrayBufferObject::setNewOwnedData(FreeOp* fop, void *newData)
{
    JS_ASSERT(!isAsmJSArrayBuffer());
    JS_ASSERT(!isSharedArrayBuffer());

    if (ownsData()) {
        JS_ASSERT(newData != dataPointer());
        releaseData(fop);
    }

    setDataPointer(static_cast<uint8_t *>(newData), OwnsData);
}

void
ArrayBufferObject::changeContents(JSContext *cx, void *newData)
{
    // Change buffer contents.
    uint8_t* oldDataPointer = dataPointer();
    setNewOwnedData(cx->runtime()->defaultFreeOp(), newData);

    // Update all views.
    ArrayBufferViewObject *viewListHead = viewList();
    for (ArrayBufferViewObject *view = viewListHead; view; view = view->nextView()) {
        // Watch out for NULL data pointers in views. This means that the view
        // is not fully initialized (in which case it'll be initialized later
        // with the correct pointer).
        uint8_t *viewDataPointer = view->dataPointer();
        if (viewDataPointer) {
            JS_ASSERT(newData);
            ptrdiff_t offset = viewDataPointer - oldDataPointer;
            viewDataPointer = static_cast<uint8_t *>(newData) + offset;
            view->setPrivate(viewDataPointer);
        }

        // Notify compiled jit code that the base pointer has moved.
        MarkObjectStateChange(cx, view);
    }
}

#if defined(JS_CPU_X64)
// Refer to comment above AsmJSMappedSize in AsmJS.h.
JS_STATIC_ASSERT(AsmJSAllocationGranularity == AsmJSPageSize);
#endif

#if defined(JS_ION) && defined(JS_CPU_X64)
/* static */ bool
ArrayBufferObject::prepareForAsmJS(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (buffer->isAsmJSArrayBuffer())
        return true;

    // SharedArrayBuffers are already created with AsmJS support in mind.
    if (buffer->isSharedArrayBuffer())
        return true;

    // Get the entire reserved region (with all pages inaccessible).
    void *data;
# ifdef XP_WIN
    data = VirtualAlloc(nullptr, AsmJSMappedSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!data)
        return false;
# else
    data = mmap(nullptr, AsmJSMappedSize, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (data == MAP_FAILED)
        return false;
# endif

    // Enable access to the valid region.
    JS_ASSERT(buffer->byteLength() % AsmJSAllocationGranularity == 0);
# ifdef XP_WIN
    if (!VirtualAlloc(data, buffer->byteLength(), MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(data, 0, MEM_RELEASE);
        return false;
    }
# else
    size_t validLength = buffer->byteLength();
    if (mprotect(data, validLength, PROT_READ | PROT_WRITE)) {
        munmap(data, AsmJSMappedSize);
        return false;
    }
#   if defined(MOZ_VALGRIND) && defined(VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE)
    // Tell Valgrind/Memcheck to not report accesses in the inaccessible region.
    VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE((unsigned char*)data + validLength,
                                                   AsmJSMappedSize-validLength);
#   endif
# endif

    // Copy over the current contents of the typed array.
    memcpy(data, buffer->dataPointer(), buffer->byteLength());

    // Swap the new elements into the ArrayBufferObject.
    buffer->changeContents(cx, data);
    JS_ASSERT(data == buffer->dataPointer());

    // Mark the ArrayBufferObject so (1) we don't do this again, (2) we know not
    // to js_free the data in the normal way.
    buffer->setIsAsmJSArrayBuffer();

    return true;
}

void
ArrayBufferObject::releaseAsmJSArray(FreeOp *fop)
{
    void *data = dataPointer();

    JS_ASSERT(uintptr_t(data) % AsmJSPageSize == 0);
# ifdef XP_WIN
    VirtualFree(data, 0, MEM_RELEASE);
# else
    munmap(data, AsmJSMappedSize);
#   if defined(MOZ_VALGRIND) && defined(VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE)
    // Tell Valgrind/Memcheck to recommence reporting accesses in the
    // previously-inaccessible region.
    if (AsmJSMappedSize > 0) {
        VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(data, AsmJSMappedSize);
    }
#   endif
# endif
}
#else  /* defined(JS_ION) && defined(JS_CPU_X64) */
bool
ArrayBufferObject::prepareForAsmJS(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (buffer->isAsmJSArrayBuffer())
        return true;

    if (buffer->isSharedArrayBuffer())
        return true;

    if (!ensureNonInline(cx, buffer))
        return false;

    buffer->setIsAsmJSArrayBuffer();
    return true;
}

void
ArrayBufferObject::releaseAsmJSArray(FreeOp *fop)
{
    fop->free_(dataPointer());
}
#endif

bool
ArrayBufferObject::canNeuterAsmJSArrayBuffer(JSContext *cx, ArrayBufferObject &buffer)
{
    JS_ASSERT(!buffer.isSharedArrayBuffer());
#ifdef JS_ION
    AsmJSActivation *act = cx->mainThread().asmJSActivationStackFromOwnerThread();
    for (; act; act = act->prevAsmJS()) {
        if (act->module().maybeHeapBufferObject() == &buffer)
            break;
    }
    if (!act)
        return true;

    return false;
#else
    return true;
#endif
}

void *
ArrayBufferObject::createMappedContents(int fd, size_t offset, size_t length)
{
    return SystemPageAllocator::AllocateMappedContent(fd, offset, length, ARRAY_BUFFER_ALIGNMENT);
}

void
ArrayBufferObject::releaseMappedArray()
{
    if(!isMappedArrayBuffer() || isNeutered())
        return;

    SystemPageAllocator::DeallocateMappedContent(dataPointer(), byteLength());
}

void
ArrayBufferObject::addView(ArrayBufferViewObject *view)
{
    // Note that pre-barriers are not needed here because either the list was
    // previously empty, in which case no pointer is being overwritten, or the
    // list was nonempty and will be made weak during this call (and weak
    // pointers cannot violate the snapshot-at-the-beginning invariant.)

    ArrayBufferViewObject *viewsHead = viewList();
    if (viewsHead == nullptr) {
        // This ArrayBufferObject will have a single view at this point, so it
        // is a strong pointer (it will be marked during tracing.)
        JS_ASSERT(view->nextView() == nullptr);
    } else {
        view->setNextView(viewsHead);
    }

    setViewList(view);
}

uint8_t *
ArrayBufferObject::dataPointer() const
{
    if (isSharedArrayBuffer())
        return (uint8_t *)this->as<SharedArrayBufferObject>().dataPointer();
    return static_cast<uint8_t *>(getSlot(DATA_SLOT).toPrivate());
}

void
ArrayBufferObject::releaseData(FreeOp *fop)
{
    JS_ASSERT(ownsData());

    if (isAsmJSArrayBuffer())
        releaseAsmJSArray(fop);
    else if (isMappedArrayBuffer())
        releaseMappedArray();
    else
        fop->free_(dataPointer());
}

void
ArrayBufferObject::setDataPointer(void *data, OwnsState ownsData)
{
    MOZ_ASSERT_IF(!is<SharedArrayBufferObject>() && !isMappedArrayBuffer(), data != nullptr);
    setSlot(DATA_SLOT, PrivateValue(data));
    setOwnsData(ownsData);
}

size_t
ArrayBufferObject::byteLength() const
{
    return size_t(getSlot(BYTE_LENGTH_SLOT).toDouble());
}

void
ArrayBufferObject::setByteLength(size_t length)
{
    setSlot(BYTE_LENGTH_SLOT, DoubleValue(length));
}

uint32_t
ArrayBufferObject::flags() const
{
    return uint32_t(getSlot(FLAGS_SLOT).toInt32());
}

void
ArrayBufferObject::setFlags(uint32_t flags)
{
    setSlot(FLAGS_SLOT, Int32Value(flags));
}

ArrayBufferObject *
ArrayBufferObject::create(JSContext *cx, uint32_t nbytes, void *data /* = nullptr */,
                          NewObjectKind newKind /* = GenericObject */,
                          bool mapped /* = false */)
{
    JS_ASSERT_IF(mapped, data);

    // If we need to allocate data, try to use a larger object size class so
    // that the array buffer's data can be allocated inline with the object.
    // The extra space will be left unused by the object's fixed slots and
    // available for the buffer's data, see NewObject().
    size_t reservedSlots = JSCLASS_RESERVED_SLOTS(&class_);

    size_t nslots = reservedSlots;
    if (!data) {
        size_t usableSlots = JSObject::MAX_FIXED_SLOTS - reservedSlots;
        if (nbytes <= usableSlots * sizeof(Value)) {
            int newSlots = (nbytes - 1) / sizeof(Value) + 1;
            JS_ASSERT(int(nbytes) <= newSlots * int(sizeof(Value)));
            nslots = reservedSlots + newSlots;
        } else {
            data = AllocateArrayBufferContents(cx, nbytes);
            if (!data)
                return nullptr;
        }
    }

    JS_ASSERT(!(class_.flags & JSCLASS_HAS_PRIVATE));
    gc::AllocKind allocKind = GetGCObjectKind(nslots);

    Rooted<ArrayBufferObject*> obj(cx, NewBuiltinClassInstance<ArrayBufferObject>(cx, allocKind, newKind));
    if (!obj)
        return nullptr;

    JS_ASSERT(obj->getClass() == &class_);

    JS_ASSERT(!gc::IsInsideNursery(obj));

    if (data) {
        obj->initialize(nbytes, data, OwnsData);
        if (mapped)
            obj->setIsMappedArrayBuffer();
    } else {
        void *data = obj->fixedData(reservedSlots);
        memset(data, 0, nbytes);
        obj->initialize(nbytes, data, DoesntOwnData);
    }

    return obj;
}

JSObject *
ArrayBufferObject::createSlice(JSContext *cx, Handle<ArrayBufferObject*> arrayBuffer,
                               uint32_t begin, uint32_t end)
{
    uint32_t bufLength = arrayBuffer->byteLength();
    if (begin > bufLength || end > bufLength || begin > end) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPE_ERR_BAD_ARGS);
        return nullptr;
    }

    uint32_t length = end - begin;

    if (!arrayBuffer->hasData())
        return create(cx, 0);

    ArrayBufferObject *slice = create(cx, length);
    if (!slice)
        return nullptr;
    memcpy(slice->dataPointer(), arrayBuffer->dataPointer() + begin, length);
    return slice;
}

bool
ArrayBufferObject::createDataViewForThisImpl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsArrayBuffer(args.thisv()));

    /*
     * This method is only called for |DataView(alienBuf, ...)| which calls
     * this as |createDataViewForThis.call(alienBuf, ..., DataView.prototype)|,
     * ergo there must be at least two arguments.
     */
    JS_ASSERT(args.length() >= 2);

    Rooted<JSObject*> proto(cx, &args[args.length() - 1].toObject());

    Rooted<JSObject*> buffer(cx, &args.thisv().toObject());

    /*
     * Pop off the passed-along prototype and delegate to normal DataViewObject
     * construction.
     */
    CallArgs frobbedArgs = CallArgsFromVp(args.length() - 1, args.base());
    return DataViewObject::construct(cx, buffer, frobbedArgs, proto);
}

bool
ArrayBufferObject::createDataViewForThis(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsArrayBuffer, createDataViewForThisImpl>(cx, args);
}

/* static */ bool
ArrayBufferObject::ensureNonInline(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (!buffer->ownsData()) {
        void *data = AllocateArrayBufferContents(cx, buffer->byteLength());
        if (!data)
            return false;
        memcpy(data, buffer->dataPointer(), buffer->byteLength());
        buffer->changeContents(cx, data);
    }

    return true;
}

/* static */ void *
ArrayBufferObject::stealContents(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (!buffer->canNeuter(cx)) {
        js_ReportOverRecursed(cx);
        return nullptr;
    }

    void *oldData = buffer->dataPointer();
    void *newData = AllocateArrayBufferContents(cx, buffer->byteLength());
    if (!newData)
        return nullptr;

    if (buffer->hasStealableContents()) {
        buffer->setOwnsData(DoesntOwnData);
        ArrayBufferObject::neuter(cx, buffer, newData);
        return oldData;
    } else {
        memcpy(newData, oldData, buffer->byteLength());
        ArrayBufferObject::neuter(cx, buffer, oldData);
        return newData;
    }

    return oldData;
}

/* static */ void
ArrayBufferObject::addSizeOfExcludingThis(JSObject *obj, mozilla::MallocSizeOf mallocSizeOf, JS::ObjectsExtraSizes *sizes)
{
    ArrayBufferObject &buffer = AsArrayBuffer(obj);

    if (!buffer.ownsData())
        return;

    if (MOZ_UNLIKELY(buffer.isAsmJSArrayBuffer())) {
#if defined (JS_CPU_X64)
        // On x64, ArrayBufferObject::prepareForAsmJS switches the
        // ArrayBufferObject to use mmap'd storage.
        sizes->nonHeapElementsAsmJS += buffer.byteLength();
#else
        sizes->mallocHeapElementsAsmJS += mallocSizeOf(buffer.dataPointer());
#endif
    } else if (MOZ_UNLIKELY(buffer.isMappedArrayBuffer())) {
        sizes->nonHeapElementsMapped += buffer.byteLength();
    } else if (buffer.dataPointer()) {
        sizes->mallocHeapElementsNonAsmJS += mallocSizeOf(buffer.dataPointer());
    }
}

/* static */ void
ArrayBufferObject::finalize(FreeOp *fop, JSObject *obj)
{
    ArrayBufferObject &buffer = obj->as<ArrayBufferObject>();

    if (buffer.ownsData())
        buffer.releaseData(fop);
}

/* static */ void
ArrayBufferObject::obj_trace(JSTracer *trc, JSObject *obj)
{
    if (!IS_GC_MARKING_TRACER(trc) && !trc->runtime()->isHeapMinorCollecting()
#ifdef JSGC_FJGENERATIONAL
        && !trc->runtime()->isFJMinorCollecting()
#endif
        )
    {
        return;
    }

    // ArrayBufferObjects need to maintain a list of possibly-weak pointers to
    // their views. The straightforward way to update the weak pointers would
    // be in the views' finalizers, but giving views finalizers means they
    // cannot be swept in the background. This results in a very high
    // performance cost.  Instead, ArrayBufferObjects with a single view hold a
    // strong pointer to the view. This can entrain garbage when the single
    // view becomes otherwise unreachable while the buffer is still live, but
    // this is expected to be rare. ArrayBufferObjects with 0-1 views are
    // expected to be by far the most common cases. ArrayBufferObjects with
    // multiple views are collected into a linked list during collection, and
    // then swept to prune out their dead views.

    ArrayBufferObject &buffer = AsArrayBuffer(obj);
    ArrayBufferViewObject *viewsHead = buffer.viewList();
    if (!viewsHead)
        return;

    buffer.setViewList(UpdateObjectIfRelocated(trc->runtime(), &viewsHead));

    if (viewsHead->nextView() == nullptr) {
        // Single view: mark it, but only if we're actually doing a GC pass
        // right now. Otherwise, the tracing pass for barrier verification will
        // fail if we add another view and the pointer becomes weak.
        MarkObjectUnbarriered(trc, &viewsHead, "arraybuffer.singleview");
        buffer.setViewListNoBarrier(viewsHead);
    } else {
        // Multiple views: do not mark, but append buffer to list.
        ArrayBufferVector &gcLiveArrayBuffers = buffer.compartment()->gcLiveArrayBuffers;

        // obj_trace may be called multiple times before sweep(), so avoid
        // adding this buffer to the list multiple times.
        if (buffer.inLiveList()) {
#ifdef DEBUG
            bool found = false;
            for (size_t i = 0; i < gcLiveArrayBuffers.length(); i++)
                found |= gcLiveArrayBuffers[i] == &buffer;
            JS_ASSERT(found);
#endif
        } else if (gcLiveArrayBuffers.append(&buffer)) {
            buffer.setInLiveList(true);
        } else {
            CrashAtUnhandlableOOM("OOM while updating live array buffers");
        }
    }
}

/* static */ void
ArrayBufferObject::sweep(JSCompartment *compartment)
{
    JSRuntime *rt = compartment->runtimeFromMainThread();
    ArrayBufferVector &gcLiveArrayBuffers = compartment->gcLiveArrayBuffers;

    for (size_t i = 0; i < gcLiveArrayBuffers.length(); i++) {
        ArrayBufferObject *buffer = gcLiveArrayBuffers[i];

        JS_ASSERT(buffer->inLiveList());
        buffer->setInLiveList(false);

        ArrayBufferViewObject *viewsHead = buffer->viewList();
        JS_ASSERT(viewsHead);
        buffer->setViewList(UpdateObjectIfRelocated(rt, &viewsHead));

        // Rebuild the list of views of the ArrayBufferObject, discarding dead
        // views.  If there is only one view, it will have already been marked.
        ArrayBufferViewObject *prevLiveView = nullptr;
        ArrayBufferViewObject *view = viewsHead;
        while (view) {
            JS_ASSERT(buffer->compartment() == view->compartment());
            ArrayBufferViewObject *nextView = view->nextView();
            if (!IsObjectAboutToBeFinalized(&view)) {
                view->setNextView(prevLiveView);
                prevLiveView = view;
            }
            view = UpdateObjectIfRelocated(rt, &nextView);
        }

        buffer->setViewList(prevLiveView);
    }

    gcLiveArrayBuffers.clear();
}

void
ArrayBufferObject::resetArrayBufferList(JSCompartment *comp)
{
    ArrayBufferVector &gcLiveArrayBuffers = comp->gcLiveArrayBuffers;

    for (size_t i = 0; i < gcLiveArrayBuffers.length(); i++) {
        ArrayBufferObject *buffer = gcLiveArrayBuffers[i];

        JS_ASSERT(buffer->inLiveList());
        buffer->setInLiveList(false);
    }

    gcLiveArrayBuffers.clear();
}

/* static */ bool
ArrayBufferObject::saveArrayBufferList(JSCompartment *comp, ArrayBufferVector &vector)
{
    const ArrayBufferVector &gcLiveArrayBuffers = comp->gcLiveArrayBuffers;

    for (size_t i = 0; i < gcLiveArrayBuffers.length(); i++) {
        if (!vector.append(gcLiveArrayBuffers[i]))
            return false;
    }

    return true;
}

/* static */ void
ArrayBufferObject::restoreArrayBufferLists(ArrayBufferVector &vector)
{
    for (size_t i = 0; i < vector.length(); i++) {
        ArrayBufferObject *buffer = vector[i];

        JS_ASSERT(!buffer->inLiveList());
        buffer->setInLiveList(true);

        buffer->compartment()->gcLiveArrayBuffers.infallibleAppend(buffer);
    }
}

/*
 * ArrayBufferViewObject
 */

/*
 * This method is used to trace TypedArrayObjects and DataViewObjects. We need
 * a custom tracer because some of an ArrayBufferViewObject's reserved slots
 * are weak references, and some need to be updated specially during moving
 * GCs.
 */
/* static */ void
ArrayBufferViewObject::trace(JSTracer *trc, JSObject *obj)
{
    HeapSlot &bufSlot = obj->getReservedSlotRef(BUFFER_SLOT);
    MarkSlot(trc, &bufSlot, "typedarray.buffer");

    // Update obj's data pointer if the array buffer moved. Note that during
    // initialization, bufSlot may still contain |undefined|.
    if (bufSlot.isObject()) {
        ArrayBufferObject &buf = AsArrayBuffer(&bufSlot.toObject());
        int32_t offset = obj->getReservedSlot(BYTEOFFSET_SLOT).toInt32();
        MOZ_ASSERT(buf.dataPointer() != nullptr);
        obj->initPrivate(buf.dataPointer() + offset);
    }

    /* Update NEXT_VIEW_SLOT, if the view moved. */
    IsSlotMarked(&obj->getReservedSlotRef(NEXT_VIEW_SLOT));
}

void
ArrayBufferViewObject::neuter(void *newData)
{
    MOZ_ASSERT(newData != nullptr);
    if (is<DataViewObject>())
        as<DataViewObject>().neuter(newData);
    else if (is<TypedArrayObject>())
        as<TypedArrayObject>().neuter(newData);
    else
        as<TypedObject>().neuter(newData);
}

/* static */ ArrayBufferObject *
ArrayBufferViewObject::bufferObject(JSContext *cx, Handle<ArrayBufferViewObject *> thisObject)
{
    if (thisObject->is<TypedArrayObject>()) {
        Rooted<TypedArrayObject *> typedArray(cx, &thisObject->as<TypedArrayObject>());
        if (!TypedArrayObject::ensureHasBuffer(cx, typedArray))
            return nullptr;
        return thisObject->as<TypedArrayObject>().buffer();
    }
    MOZ_ASSERT(thisObject->is<DataViewObject>());
    return &thisObject->as<DataViewObject>().arrayBuffer();
}

/* JS Friend API */

JS_FRIEND_API(bool)
JS_IsArrayBufferViewObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<ArrayBufferViewObject>() : false;
}

JS_FRIEND_API(JSObject *)
js::UnwrapArrayBufferView(JSObject *obj)
{
    if (JSObject *unwrapped = CheckedUnwrap(obj))
        return unwrapped->is<ArrayBufferViewObject>() ? unwrapped : nullptr;
    return nullptr;
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferByteLength(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? AsArrayBuffer(obj).byteLength() : 0;
}

JS_FRIEND_API(uint8_t *)
JS_GetArrayBufferData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    return AsArrayBuffer(obj).dataPointer();
}

JS_FRIEND_API(uint8_t *)
JS_GetStableArrayBufferData(JSContext *cx, HandleObject objArg)
{
    JSObject *obj = CheckedUnwrap(objArg);
    if (!obj)
        return nullptr;

    Rooted<ArrayBufferObject*> buffer(cx, &AsArrayBuffer(obj));
    if (!ArrayBufferObject::ensureNonInline(cx, buffer))
        return nullptr;

    return buffer->dataPointer();
}

JS_FRIEND_API(bool)
JS_NeuterArrayBuffer(JSContext *cx, HandleObject obj,
                     NeuterDataDisposition changeData)
{
    if (!obj->is<ArrayBufferObject>()) {
        JS_ReportError(cx, "ArrayBuffer object required");
        return false;
    }

    Rooted<ArrayBufferObject*> buffer(cx, &obj->as<ArrayBufferObject>());

    if (!buffer->canNeuter(cx)) {
        js_ReportOverRecursed(cx);
        return false;
    }

    void *newData;
    if (changeData == ChangeData && buffer->hasStealableContents()) {
        newData = AllocateArrayBufferContents(cx, buffer->byteLength());
        if (!newData)
            return false;
    } else {
        newData = buffer->dataPointer();
    }

    ArrayBufferObject::neuter(cx, buffer, newData);
    return true;
}

JS_FRIEND_API(bool)
JS_IsNeuteredArrayBufferObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return false;

    return obj->is<ArrayBufferObject>()
           ? obj->as<ArrayBufferObject>().isNeutered()
           : false;
}

JS_FRIEND_API(JSObject *)
JS_NewArrayBuffer(JSContext *cx, uint32_t nbytes)
{
    JS_ASSERT(nbytes <= INT32_MAX);
    return ArrayBufferObject::create(cx, nbytes);
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayBufferWithContents(JSContext *cx, size_t nbytes, void *contents)
{
    JS_ASSERT(contents);
    return ArrayBufferObject::create(cx, nbytes, contents, TenuredObject, false);
}

JS_PUBLIC_API(void *)
JS_AllocateArrayBufferContents(JSContext *maybecx, uint32_t nbytes)
{
    return AllocateArrayBufferContents(maybecx, nbytes);
}

JS_PUBLIC_API(void *)
JS_ReallocateArrayBufferContents(JSContext *maybecx, uint32_t nbytes, void *oldContents, uint32_t oldNbytes)
{
    return AllocateArrayBufferContents(maybecx, nbytes, oldContents, oldNbytes);
}

JS_FRIEND_API(bool)
JS_IsArrayBufferObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<ArrayBufferObject>() : false;
}

JS_FRIEND_API(JSObject *)
js::UnwrapArrayBuffer(JSObject *obj)
{
    if (JSObject *unwrapped = CheckedUnwrap(obj))
        return unwrapped->is<ArrayBufferObject>() ? unwrapped : nullptr;
    return nullptr;
}

JS_PUBLIC_API(void *)
JS_StealArrayBufferContents(JSContext *cx, HandleObject objArg)
{
    JSObject *obj = CheckedUnwrap(objArg);
    if (!obj)
        return nullptr;

    if (!obj->is<ArrayBufferObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return nullptr;
    }

    Rooted<ArrayBufferObject*> buffer(cx, &obj->as<ArrayBufferObject>());
    return ArrayBufferObject::stealContents(cx, buffer);
}

JS_PUBLIC_API(JSObject *)
JS_NewMappedArrayBufferWithContents(JSContext *cx, size_t nbytes, void *contents)
{
    JS_ASSERT(contents);
    return ArrayBufferObject::create(cx, nbytes, contents, TenuredObject, true);
}

JS_PUBLIC_API(void *)
JS_CreateMappedArrayBufferContents(int fd, size_t offset, size_t length)
{
    return ArrayBufferObject::createMappedContents(fd, offset, length);
}

JS_PUBLIC_API(void)
JS_ReleaseMappedArrayBufferContents(void *contents, size_t length)
{
    SystemPageAllocator::DeallocateMappedContent(contents, length);
}

JS_FRIEND_API(bool)
JS_IsMappedArrayBufferObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return false;

    return obj->is<ArrayBufferObject>()
           ? obj->as<ArrayBufferObject>().isMappedArrayBuffer()
           : false;
}

JS_FRIEND_API(void *)
JS_GetArrayBufferViewData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    return obj->is<DataViewObject>() ? obj->as<DataViewObject>().dataPointer()
                                     : obj->as<TypedArrayObject>().viewData();
}

JS_FRIEND_API(JSObject *)
JS_GetArrayBufferViewBuffer(JSContext *cx, HandleObject objArg)
{
    JSObject *obj = CheckedUnwrap(objArg);
    if (!obj)
        return nullptr;
    Rooted<ArrayBufferViewObject *> viewObject(cx, &obj->as<ArrayBufferViewObject>());
    return ArrayBufferViewObject::bufferObject(cx, viewObject);
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferViewByteLength(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->is<DataViewObject>()
           ? obj->as<DataViewObject>().byteLength()
           : obj->as<TypedArrayObject>().byteLength();
}

JS_FRIEND_API(JSObject *)
JS_GetObjectAsArrayBufferView(JSObject *obj, uint32_t *length, uint8_t **data)
{
    if (!(obj = CheckedUnwrap(obj)))
        return nullptr;
    if (!(obj->is<ArrayBufferViewObject>()))
        return nullptr;

    *length = obj->is<DataViewObject>()
              ? obj->as<DataViewObject>().byteLength()
              : obj->as<TypedArrayObject>().byteLength();

    *data = static_cast<uint8_t*>(obj->is<DataViewObject>()
                                  ? obj->as<DataViewObject>().dataPointer()
                                  : obj->as<TypedArrayObject>().viewData());
    return obj;
}

JS_FRIEND_API(void)
js::GetArrayBufferViewLengthAndData(JSObject *obj, uint32_t *length, uint8_t **data)
{
    MOZ_ASSERT(obj->is<ArrayBufferViewObject>());

    *length = obj->is<DataViewObject>()
              ? obj->as<DataViewObject>().byteLength()
              : obj->as<TypedArrayObject>().byteLength();

    *data = static_cast<uint8_t*>(obj->is<DataViewObject>()
                                  ? obj->as<DataViewObject>().dataPointer()
                                  : obj->as<TypedArrayObject>().viewData());
}

JS_FRIEND_API(JSObject *)
JS_GetObjectAsArrayBuffer(JSObject *obj, uint32_t *length, uint8_t **data)
{
    if (!(obj = CheckedUnwrap(obj)))
        return nullptr;
    if (!IsArrayBuffer(obj))
        return nullptr;

    *length = AsArrayBuffer(obj).byteLength();
    *data = AsArrayBuffer(obj).dataPointer();

    return obj;
}

JS_FRIEND_API(void)
js::GetArrayBufferLengthAndData(JSObject *obj, uint32_t *length, uint8_t **data)
{
    MOZ_ASSERT(IsArrayBuffer(obj));
    *length = AsArrayBuffer(obj).byteLength();
    *data = AsArrayBuffer(obj).dataPointer();
}
