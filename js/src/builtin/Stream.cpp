/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/Stream.h"

#include "js/Stream.h"

#include "gc/Heap.h"
#include "vm/JSContext.h"
#include "vm/SelfHosting.h"

#include "vm/Compartment-inl.h"
#include "vm/List-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

enum ReaderType {
    ReaderType_Default,
    ReaderType_BYOB
};

template<class T>
bool
Is(const HandleValue v)
{
    return v.isObject() && v.toObject().is<T>();
}

template<class T>
bool
IsMaybeWrapped(const HandleValue v)
{
    return v.isObject() && v.toObject().canUnwrapAs<T>();
}

JS::ReadableStreamMode
ReadableStream::mode() const
{
    ReadableStreamController* controller = this->controller();
    if (controller->is<ReadableStreamDefaultController>()) {
        return JS::ReadableStreamMode::Default;
    }
    return controller->as<ReadableByteStreamController>().hasExternalSource()
           ? JS::ReadableStreamMode::ExternalSource
           : JS::ReadableStreamMode::Byte;
}

uint8_t
ReadableStream::embeddingFlags() const
{
    uint8_t flags = controller()->flags() >> ReadableStreamController::EmbeddingFlagsOffset;
    MOZ_ASSERT_IF(flags, mode() == JS::ReadableStreamMode::ExternalSource);
    return flags;
}

/**
 * Checks that |obj| is an unwrapped instance of T or throws an error.
 *
 * This overload must only be used if the caller can ensure that failure to
 * unwrap is the only possible source of exceptions.
 */
template<class T>
static T*
ToUnwrapped(JSContext* cx, JSObject* obj)
{
    if (IsWrapper(obj)) {
        obj = CheckedUnwrap(obj);
        if (!obj) {
            ReportAccessDenied(cx);
            return nullptr;
        }
    }

    return &obj->as<T>();
}

/**
 * Unwrap v as an object of type T, throwing if it can't be unwrapped.
 *
 * This overload must be used only if v is an ObjectValue and the result of a
 * successful unwrap is certain to be of type T.
 */
template <class T>
static T*
ToUnwrapped(JSContext* cx, HandleValue v)
{
    return ToUnwrapped<T>(cx, &v.toObject());
}

/**
 * Returns the stream associated with the given reader.
 */
static MOZ_MUST_USE bool
UnwrapStreamFromReader(JSContext *cx,
                       Handle<ReadableStreamReader*> reader,
                       MutableHandle<ReadableStream*> unwrappedResult)
{
    MOZ_ASSERT(reader->hasStream());
    return UnwrapInternalSlot(cx, reader, ReadableStreamReader::Slot_Stream, unwrappedResult);
}

/**
 * Returns the reader associated with the given stream.
 *
 * Must only be called on ReadableStreams that already have a reader
 * associated with them.
 *
 * If the reader is a wrapper, it will be unwrapped, so the object stored in
 * `unwrappedResult` might not be an object from the currently active
 * compartment.
 */
static MOZ_MUST_USE bool
UnwrapReaderFromStream(JSContext* cx,
                       Handle<ReadableStream*> stream,
                       MutableHandle<ReadableStreamReader*> unwrappedResult)
{
    return UnwrapInternalSlot(cx, stream, ReadableStream::Slot_Reader, unwrappedResult);
}

static MOZ_MUST_USE ReadableStreamReader*
UnwrapReaderFromStreamNoThrow(ReadableStream* stream)
{
    JSObject* readerObj = &stream->getFixedSlot(ReadableStream::Slot_Reader).toObject();
    if (IsProxy(readerObj)) {
        if (JS_IsDeadWrapper(readerObj)) {
            return nullptr;
        }

        readerObj = CheckedUnwrap(readerObj);
        if (!readerObj) {
            return nullptr;
        }
    }

    return &readerObj->as<ReadableStreamReader>();
}

inline static MOZ_MUST_USE JSFunction*
NewHandler(JSContext* cx, Native handler, HandleObject target)
{
    cx->check(target);

    RootedAtom funName(cx, cx->names().empty);
    RootedFunction handlerFun(cx, NewNativeFunction(cx, handler, 0, funName,
                                                    gc::AllocKind::FUNCTION_EXTENDED,
                                                    GenericObject));
    if (!handlerFun) {
        return nullptr;
    }
    handlerFun->setExtendedSlot(0, ObjectValue(*target));
    return handlerFun;
}

template<class T>
inline static MOZ_MUST_USE T*
TargetFromHandler(JSObject& handler)
{
    return &handler.as<JSFunction>().getExtendedSlot(0).toObject().as<T>();
}

inline static MOZ_MUST_USE bool
ResetQueue(JSContext* cx, Handle<ReadableStreamController*> unwrappedContainer);

inline static MOZ_MUST_USE bool
InvokeOrNoop(JSContext* cx, HandleValue O, HandlePropertyName P, HandleValue arg,
             MutableHandleValue rval);

static MOZ_MUST_USE JSObject*
PromiseInvokeOrNoop(JSContext* cx, HandleValue O, HandlePropertyName P, HandleValue arg);

static MOZ_MUST_USE JSObject*
PromiseRejectedWithPendingError(JSContext* cx) {
    RootedValue exn(cx);
    if (!cx->isExceptionPending() || !GetAndClearException(cx, &exn)) {
        // Uncatchable error. This happens when a slow script is killed or a
        // worker is terminated. Propagate the uncatchable error. This will
        // typically kill off the calling asynchronous process: the caller
        // can't hook its continuation to the new rejected promise.
        return nullptr;
    }
    return PromiseObject::unforgeableReject(cx, exn);
}

static void
ReportArgTypeError(JSContext* cx, const char* funName, const char* expectedType, HandleValue arg)
{
    UniqueChars bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, arg, nullptr);
    if (!bytes) {
        return;
    }

    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_NOT_EXPECTED_TYPE, funName,
                             expectedType, bytes.get());
}

static MOZ_MUST_USE bool
ReturnPromiseRejectedWithPendingError(JSContext* cx, const CallArgs& args)
{
    JSObject* promise = PromiseRejectedWithPendingError(cx);
    if (!promise) {
        return false;
    }

    args.rval().setObject(*promise);
    return true;
}

/**
 * Creates a NativeObject to be used as a list and stores it on the given
 * container at the given fixed slot offset.
 *
 * Note: unwrappedContainer does not have to be same-compartment with cx. The
 * new List is created in unwrappedContainer's compartment.
 */
inline static MOZ_MUST_USE bool
SetNewList(JSContext* cx, HandleNativeObject unwrappedContainer, uint32_t slot)
{
    AutoRealm ar(cx, unwrappedContainer);
    NativeObject* list = NewList(cx);
    if (!list) {
        return false;
    }
    unwrappedContainer->setFixedSlot(slot, ObjectValue(*list));
    return true;
}

class ByteStreamChunk : public NativeObject
{
  private:
    enum Slots {
        Slot_Buffer = 0,
        Slot_ByteOffset,
        Slot_ByteLength,
        SlotCount
    };

  public:
    static const Class class_;

    ArrayBufferObject* buffer() {
        return &getFixedSlot(Slot_Buffer).toObject().as<ArrayBufferObject>();
    }
    uint32_t byteOffset() { return getFixedSlot(Slot_ByteOffset).toInt32(); }
    void SetByteOffset(uint32_t offset) {
        setFixedSlot(Slot_ByteOffset, Int32Value(offset));
    }
    uint32_t byteLength() { return getFixedSlot(Slot_ByteLength).toInt32(); }
    void SetByteLength(uint32_t length) {
        setFixedSlot(Slot_ByteLength, Int32Value(length));
    }

    static ByteStreamChunk* create(JSContext* cx, HandleObject buffer, uint32_t byteOffset,
                                   uint32_t byteLength)
    {
        Rooted<ByteStreamChunk*> chunk(cx, NewBuiltinClassInstance<ByteStreamChunk>(cx));
        if (!chunk) {
            return nullptr;
        }

        chunk->setFixedSlot(Slot_Buffer, ObjectValue(*buffer));
        chunk->setFixedSlot(Slot_ByteOffset, Int32Value(byteOffset));
        chunk->setFixedSlot(Slot_ByteLength, Int32Value(byteLength));
        return chunk;
    }
};

const Class ByteStreamChunk::class_ = {
    "ByteStreamChunk",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)
};

class PullIntoDescriptor : public NativeObject
{
  private:
    enum Slots {
        Slot_buffer,
        Slot_ByteOffset,
        Slot_ByteLength,
        Slot_BytesFilled,
        Slot_ElementSize,
        Slot_Ctor,
        Slot_ReaderType,
        SlotCount
    };
  public:
    static const Class class_;

    ArrayBufferObject* buffer() {
        return &getFixedSlot(Slot_buffer).toObject().as<ArrayBufferObject>();
    }
    void setBuffer(ArrayBufferObject* buffer) { setFixedSlot(Slot_buffer, ObjectValue(*buffer)); }
    JSObject* ctor() { return getFixedSlot(Slot_Ctor).toObjectOrNull(); }
    uint32_t byteOffset() const { return getFixedSlot(Slot_ByteOffset).toInt32(); }
    uint32_t byteLength() const { return getFixedSlot(Slot_ByteLength).toInt32(); }
    uint32_t bytesFilled() const { return getFixedSlot(Slot_BytesFilled).toInt32(); }
    void setBytesFilled(int32_t bytes) { setFixedSlot(Slot_BytesFilled, Int32Value(bytes)); }
    uint32_t elementSize() const { return getFixedSlot(Slot_ElementSize).toInt32(); }
    uint32_t readerType() const { return getFixedSlot(Slot_ReaderType).toInt32(); }

    static PullIntoDescriptor* create(JSContext* cx, HandleArrayBufferObject buffer,
                                      uint32_t byteOffset, uint32_t byteLength,
                                      uint32_t bytesFilled, uint32_t elementSize,
                                      HandleObject ctor, uint32_t readerType)
    {
        Rooted<PullIntoDescriptor*> descriptor(cx, NewBuiltinClassInstance<PullIntoDescriptor>(cx));
        if (!descriptor) {
            return nullptr;
        }

        descriptor->setFixedSlot(Slot_buffer, ObjectValue(*buffer));
        descriptor->setFixedSlot(Slot_Ctor, ObjectOrNullValue(ctor));
        descriptor->setFixedSlot(Slot_ByteOffset, Int32Value(byteOffset));
        descriptor->setFixedSlot(Slot_ByteLength, Int32Value(byteLength));
        descriptor->setFixedSlot(Slot_BytesFilled, Int32Value(bytesFilled));
        descriptor->setFixedSlot(Slot_ElementSize, Int32Value(elementSize));
        descriptor->setFixedSlot(Slot_ReaderType, Int32Value(readerType));
        return descriptor;
    }
};

const Class PullIntoDescriptor::class_ = {
    "PullIntoDescriptor",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)
};

class QueueEntry : public NativeObject
{
  private:
    enum Slots {
        Slot_Value = 0,
        Slot_Size,
        SlotCount
    };

  public:
    static const Class class_;

    Value value() { return getFixedSlot(Slot_Value); }
    double size() { return getFixedSlot(Slot_Size).toNumber(); }

    static QueueEntry* create(JSContext* cx, HandleValue value, double size) {
        Rooted<QueueEntry*> entry(cx, NewBuiltinClassInstance<QueueEntry>(cx));
        if (!entry) {
            return nullptr;
        }

        entry->setFixedSlot(Slot_Value, value);
        entry->setFixedSlot(Slot_Size, NumberValue(size));

        return entry;
    }
};

const Class QueueEntry::class_ = {
    "QueueEntry",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)
};

class TeeState : public NativeObject
{
  public:
    /**
     * Memory layout for TeeState instances.
     *
     * The Reason1 and Reason2 slots store opaque values, which might be
     * wrapped objects from other compartments. Since we don't treat them as
     * objects in Streams-specific code, we don't have to worry about that
     * apart from ensuring that the values are properly wrapped before storing
     * them.
     *
     * Promise is always created in TeeState::create below, so is guaranteed
     * to be in the same compartment as the TeeState instance itself.
     *
     * Stream can be from another compartment. It is automatically wrapped
     * before storing it and unwrapped upon retrieval. That means that
     * TeeState consumers need to be able to deal with unwrapped
     * ReadableStream instances from non-current compartments.
     *
     * Branch1 and Branch2 are always created in the same compartment as the
     * TeeState instance, so cannot be from another compartment.
     */
    enum Slots {
        Slot_Flags = 0,
        Slot_Reason1,
        Slot_Reason2,
        Slot_Promise,
        Slot_Stream,
        Slot_Branch1,
        Slot_Branch2,
        SlotCount
    };

  private:
    enum Flags {
        Flag_ClosedOrErrored = 1 << 0,
        Flag_Canceled1 =       1 << 1,
        Flag_Canceled2 =       1 << 2,
        Flag_CloneForBranch2 = 1 << 3,
    };
    uint32_t flags() const { return getFixedSlot(Slot_Flags).toInt32(); }
    void setFlags(uint32_t flags) { setFixedSlot(Slot_Flags, Int32Value(flags)); }

  public:
    static const Class class_;

    bool cloneForBranch2() const { return flags() & Flag_CloneForBranch2; }

    bool closedOrErrored() const { return flags() & Flag_ClosedOrErrored; }
    void setClosedOrErrored() {
        MOZ_ASSERT(!(flags() & Flag_ClosedOrErrored));
        setFlags(flags() | Flag_ClosedOrErrored);
    }

    bool canceled1() const { return flags() & Flag_Canceled1; }
    void setCanceled1(HandleValue reason) {
        MOZ_ASSERT(!(flags() & Flag_Canceled1));
        setFlags(flags() | Flag_Canceled1);
        setFixedSlot(Slot_Reason1, reason);
    }

    bool canceled2() const { return flags() & Flag_Canceled2; }
    void setCanceled2(HandleValue reason) {
        MOZ_ASSERT(!(flags() & Flag_Canceled2));
        setFlags(flags() | Flag_Canceled2);
        setFixedSlot(Slot_Reason2, reason);
    }

    Value reason1() const {
        MOZ_ASSERT(canceled1());
        return getFixedSlot(Slot_Reason1);
    }

    Value reason2() const {
        MOZ_ASSERT(canceled2());
        return getFixedSlot(Slot_Reason2);
    }

    PromiseObject* promise() {
        return &getFixedSlot(Slot_Promise).toObject().as<PromiseObject>();
    }

    ReadableStreamDefaultController* branch1() {
        ReadableStreamDefaultController* controller =
            &getFixedSlot(Slot_Branch1).toObject()
            .as<ReadableStreamDefaultController>();
        MOZ_ASSERT(controller->flags() & ReadableStreamController::Flag_TeeBranch);
        MOZ_ASSERT(controller->isTeeBranch1());
        return controller;
    }
    void setBranch1(ReadableStreamDefaultController* controller) {
        MOZ_ASSERT(controller->flags() & ReadableStreamController::Flag_TeeBranch);
        MOZ_ASSERT(controller->isTeeBranch1());
        setFixedSlot(Slot_Branch1, ObjectValue(*controller));
    }

    ReadableStreamDefaultController* branch2() {
        ReadableStreamDefaultController* controller =
            &getFixedSlot(Slot_Branch2).toObject()
            .as<ReadableStreamDefaultController>();
        MOZ_ASSERT(controller->flags() & ReadableStreamController::Flag_TeeBranch);
        MOZ_ASSERT(controller->isTeeBranch2());
        return controller;
    }
    void setBranch2(ReadableStreamDefaultController* controller) {
        MOZ_ASSERT(controller->flags() & ReadableStreamController::Flag_TeeBranch);
        MOZ_ASSERT(controller->isTeeBranch2());
        setFixedSlot(Slot_Branch2, ObjectValue(*controller));
    }

    static TeeState* create(JSContext* cx, Handle<ReadableStream*> stream) {
        Rooted<TeeState*> state(cx, NewBuiltinClassInstance<TeeState>(cx));
        if (!state) {
            return nullptr;
        }

        Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
        if (!promise) {
            return nullptr;
        }

        state->setFixedSlot(Slot_Flags, Int32Value(0));
        state->setFixedSlot(Slot_Promise, ObjectValue(*promise));
        RootedObject wrappedStream(cx, stream);
        if (!cx->compartment()->wrap(cx, &wrappedStream)) {
            return nullptr;
        }
        state->setFixedSlot(Slot_Stream, ObjectValue(*wrappedStream));

        return state;
    }
};

const Class TeeState::class_ = {
    "TeeState",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)
};

#define CLASS_SPEC(cls, nCtorArgs, nSlots, specFlags, classFlags, classOps) \
const ClassSpec cls::classSpec_ = { \
    GenericCreateConstructor<cls::constructor, nCtorArgs, gc::AllocKind::FUNCTION>, \
    GenericCreatePrototype<cls>, \
    nullptr, \
    nullptr, \
    cls##_methods, \
    cls##_properties, \
    nullptr, \
    specFlags \
}; \
\
const Class cls::class_ = { \
    #cls, \
    JSCLASS_HAS_RESERVED_SLOTS(nSlots) | \
    JSCLASS_HAS_CACHED_PROTO(JSProto_##cls) | \
    classFlags, \
    classOps, \
    &cls::classSpec_ \
}; \
\
const Class cls::protoClass_ = { \
    "object", \
    JSCLASS_HAS_CACHED_PROTO(JSProto_##cls), \
    JS_NULL_CLASS_OPS, \
    &cls::classSpec_ \
};


/*** 3.2. Class ReadableStream *******************************************************************/

// Streams spec, 3.2.3., steps 1-4.
ReadableStream*
ReadableStream::createStream(JSContext* cx, HandleObject proto /* = nullptr */)
{
    Rooted<ReadableStream*> stream(cx, NewObjectWithClassProto<ReadableStream>(cx, proto));
    if (!stream) {
        return nullptr;
    }

    // Step 1: Set this.[[state]] to "readable".
    // Step 2: Set this.[[reader]] and this.[[storedError]] to undefined (implicit).
    // Step 3: Set this.[[disturbed]] to false (implicit).
    // Step 4: Set this.[[readableStreamController]] to undefined (implicit).
    stream->initStateBits(Readable);

    return stream;
}

static MOZ_MUST_USE ReadableStreamDefaultController*
CreateReadableStreamDefaultController(JSContext* cx, Handle<ReadableStream*> stream,
                                      HandleValue underlyingSource, HandleValue size,
                                      HandleValue highWaterMarkVal);

// Streams spec, 3.2.3., steps 1-4, 8.
ReadableStream*
ReadableStream::createDefaultStream(JSContext* cx, HandleValue underlyingSource,
                                    HandleValue size, HandleValue highWaterMark,
                                    HandleObject proto /* = nullptr */)
{
    // Steps 1-4.
    Rooted<ReadableStream*> stream(cx, createStream(cx));
    if (!stream) {
        return nullptr;
    }

    // Step 8.b: Set this.[[readableStreamController]] to
    //           ? Construct(ReadableStreamDefaultController,
    //                       « this, underlyingSource, size,
    //                         highWaterMark »).
    ReadableStreamDefaultController* controller =
        CreateReadableStreamDefaultController(cx, stream, underlyingSource, size, highWaterMark);
    if (!controller) {
        return nullptr;
    }
    stream->setController(controller);
    return stream;
}

static MOZ_MUST_USE ReadableByteStreamController*
CreateReadableByteStreamController(JSContext* cx, Handle<ReadableStream*> stream,
                                   void* underlyingSource);

ReadableStream*
ReadableStream::createExternalSourceStream(JSContext* cx, void* underlyingSource,
                                           uint8_t flags, HandleObject proto /* = nullptr */)
{
    Rooted<ReadableStream*> stream(cx, createStream(cx, proto));
    if (!stream) {
        return nullptr;
    }

    Rooted<ReadableStreamController*> controller(cx);
    controller = CreateReadableByteStreamController(cx, stream, underlyingSource);
    if (!controller) {
        return nullptr;
    }

    stream->setController(controller);
    controller->setEmbeddingFlags(flags);

    return stream;
}

// Streams spec, 3.2.3.
bool
ReadableStream::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedValue underlyingSource(cx, args.get(0));
    RootedValue options(cx, args.get(1));

    // Do argument handling first to keep the right order of error reporting.
    if (underlyingSource.isUndefined()) {
        RootedObject sourceObj(cx, NewBuiltinClassInstance<PlainObject>(cx));
        if (!sourceObj) {
            return false;
        }
        underlyingSource = ObjectValue(*sourceObj);
    }
    RootedValue size(cx);
    RootedValue highWaterMark(cx);

    if (!options.isUndefined()) {
        if (!GetProperty(cx, options, cx->names().size, &size)) {
            return false;
        }

        if (!GetProperty(cx, options, cx->names().highWaterMark, &highWaterMark)) {
            return false;
        }
    }

    if (!ThrowIfNotConstructing(cx, args, "ReadableStream")) {
        return false;
    }

    // Step 5: Let type be ? GetV(underlyingSource, "type").
    RootedValue typeVal(cx);
    if (!GetProperty(cx, underlyingSource, cx->names().type, &typeVal)) {
        return false;
    }

    // Step 6: Let typeString be ? ToString(type).
    RootedString type(cx, ToString<CanGC>(cx, typeVal));
    if (!type) {
        return false;
    }

    int32_t notByteStream;
    if (!CompareStrings(cx, type, cx->names().bytes, &notByteStream)) {
        return false;
    }

    // Step 7.a & 8.a (reordered): If highWaterMark is undefined, let
    //                             highWaterMark be 1 (or 0 for byte streams).
    if (highWaterMark.isUndefined()) {
        highWaterMark = Int32Value(notByteStream ? 1 : 0);
    }

    Rooted<ReadableStream*> stream(cx);

    // Step 7: If typeString is "bytes",
    if (!notByteStream) {
        // Step 7.b: Set this.[[readableStreamController]] to
        //           ? Construct(ReadableByteStreamController,
        //                       « this, underlyingSource, highWaterMark »).
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_BYTES_TYPE_NOT_IMPLEMENTED);
        return false;
    } else if (typeVal.isUndefined()) {
        // Step 8: Otherwise, if type is undefined,
        // Step 8.b: Set this.[[readableStreamController]] to
        //           ? Construct(ReadableStreamDefaultController,
        //                       « this, underlyingSource, size, highWaterMark »).
        stream = createDefaultStream(cx, underlyingSource, size, highWaterMark);
    } else {
        // Step 9: Otherwise, throw a RangeError exception.
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_UNDERLYINGSOURCE_TYPE_WRONG);
        return false;
    }
    if (!stream) {
        return false;
    }

    args.rval().setObject(*stream);
    return true;
}

// Streams spec, 3.2.5.1. get locked
static bool
ReadableStream_locked(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStream(this) is false, throw a TypeError exception.
    Rooted<ReadableStream*> stream(cx);
    if (!UnwrapThisForNonGenericMethod(cx,
                                       args.thisv(),
                                       "ReadableStream",
                                       "get locked",
                                       &stream))
    {
        return false;
    }

    // Step 2: Return ! IsReadableStreamLocked(this).
    args.rval().setBoolean(stream->locked());
    return true;
}

static MOZ_MUST_USE JSObject*
ReadableStreamCancel(JSContext* cx, Handle<ReadableStream*> stream, HandleValue reason);

// Streams spec, 3.2.5.2. cancel ( reason )
static MOZ_MUST_USE bool
ReadableStream_cancel(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStream(this) is false, return a promise rejected
    //         with a TypeError exception.
    Rooted<ReadableStream*> stream(cx);
    if (!UnwrapThisForNonGenericMethod(cx, args.thisv(), "ReadableStream", "cancel", &stream)) {
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 2: If ! IsReadableStreamLocked(this) is true, return a promise
    //         rejected with a TypeError exception.
    if (stream->locked()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_LOCKED_METHOD, "cancel");
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 3: Return ! ReadableStreamCancel(this, reason).
    RootedObject cancelPromise(cx, ::ReadableStreamCancel(cx, stream, args.get(0)));
    if (!cancelPromise) {
        return false;
    }
    args.rval().setObject(*cancelPromise);
    return true;
}

static MOZ_MUST_USE ReadableStreamDefaultReader*
CreateReadableStreamDefaultReader(JSContext* cx, Handle<ReadableStream*> stream);

// Streams spec, 3.2.5.3. getReader()
static bool
ReadableStream_getReader(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStream(this) is false, throw a TypeError exception.
    Rooted<ReadableStream*> stream(cx);
    if (!UnwrapThisForNonGenericMethod(cx, args.thisv(), "ReadableStream", "getReader", &stream)) {
        return false;
    }

    RootedObject reader(cx);

    // Step 2: If mode is undefined, return
    //         ? AcquireReadableStreamDefaultReader(this).
    RootedValue modeVal(cx);
    HandleValue optionsVal = args.get(0);
    if (!optionsVal.isUndefined()) {
        if (!GetProperty(cx, optionsVal, cx->names().mode, &modeVal)) {
            return false;
        }
    }

    if (modeVal.isUndefined()) {
        reader = CreateReadableStreamDefaultReader(cx, stream);
    } else {
        // Step 3: Set mode to ? ToString(mode) (implicit).
        RootedString mode(cx, ToString<CanGC>(cx, modeVal));
        if (!mode) {
            return false;
        }

        // Step 4: If mode is "byob", return ? AcquireReadableStreamBYOBReader(this).
        int32_t notByob;
        if (!CompareStrings(cx, mode, cx->names().byob, &notByob)) {
            return false;
        }
        if (notByob) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLESTREAM_INVALID_READER_MODE);
            // Step 5: Throw a RangeError exception.
            return false;
        }

        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_BYTES_TYPE_NOT_IMPLEMENTED);
    }

    // Reordered second part of steps 2 and 4.
    if (!reader) {
        return false;
    }
    args.rval().setObject(*reader);
    return true;
}

// Streams spec, 3.2.5.4. pipeThrough({ writable, readable }, options)
// Not implemented.

// Streams spec, 3.2.5.5. pipeTo(dest, { preventClose, preventAbort, preventCancel } = {})
// Not implemented.

static MOZ_MUST_USE bool
ReadableStreamTee(JSContext* cx, Handle<ReadableStream*> stream, bool cloneForBranch2,
                  MutableHandle<ReadableStream*> branch1, MutableHandle<ReadableStream*> branch2);

// Streams spec, 3.2.5.6. tee()
static bool
ReadableStream_tee(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStream(this) is false, throw a TypeError exception.
    Rooted<ReadableStream*> stream(cx);
    if (!UnwrapThisForNonGenericMethod(cx, args.thisv(), "ReadableStream", "tee", &stream)) {
        return false;
    }

    // Step 2: Let branches be ? ReadableStreamTee(this, false).
    Rooted<ReadableStream*> branch1(cx);
    Rooted<ReadableStream*> branch2(cx);
    if (!ReadableStreamTee(cx, stream, false, &branch1, &branch2)) {
        return false;
    }

    // Step 3: Return ! CreateArrayFromList(branches).
    RootedNativeObject branches(cx, NewDenseFullyAllocatedArray(cx, 2));
    if (!branches) {
        return false;
    }
    branches->setDenseInitializedLength(2);
    branches->initDenseElement(0, ObjectValue(*branch1));
    branches->initDenseElement(1, ObjectValue(*branch2));

    args.rval().setObject(*branches);
    return true;
}

static const JSFunctionSpec ReadableStream_methods[] = {
    JS_FN("cancel",         ReadableStream_cancel,      1, 0),
    JS_FN("getReader",      ReadableStream_getReader,   0, 0),
    JS_FN("tee",            ReadableStream_tee,         0, 0),
    JS_FS_END
};

static const JSPropertySpec ReadableStream_properties[] = {
    JS_PSG("locked", ReadableStream_locked, 0),
    JS_PS_END
};

CLASS_SPEC(ReadableStream, 0, SlotCount, 0, 0, JS_NULL_CLASS_OPS);


/*** 3.3. General readable stream abstract operations ********************************************/

// Streams spec, 3.3.1. AcquireReadableStreamBYOBReader ( stream )
// Always inlined.

// Streams spec, 3.3.2. AcquireReadableStreamDefaultReader ( stream )
// Always inlined.

// Streams spec, 3.3.3. CreateReadableStream ( startAlgorithm, pullAlgorithm, cancelAlgorithm [, highWaterMark [, sizeAlgorithm ] ] )
// Not implemented.

// Streams spec, 3.3.4. CreateReadableByteStream ( startAlgorithm, pullAlgorithm, cancelAlgorithm [, highWaterMark [, autoAllocateChunkSize ] ] )
// Not implemented.

// Streams spec, 3.3.5. InitializeReadableStream ( stream )
// Not implemented.

// Streams spec, 3.3.6. IsReadableStream ( x )
// Using is<T> instead.

// Streams spec, 3.3.7. IsReadableStreamDisturbed ( stream )
// Using stream->disturbed() instead.

// Streams spec, 3.3.8. IsReadableStreamLocked ( stream )
bool
ReadableStream::locked() const
{
    // Step 1: Assert: ! IsReadableStream(stream) is true (implicit).
    // Step 2: If stream.[[reader]] is undefined, return false.
    // Step 3: Return true.
    // Special-casing for streams with external sources. Those can be locked
    // explicitly via JSAPI, which is indicated by a controller flag.
    // IsReadableStreamLocked is called from the controller's constructor, at
    // which point we can't yet call stream->controller(), but the source also
    // can't be locked yet.
    if (hasController() && controller()->sourceLocked()) {
        return true;
    }
    return hasReader();
}

static MOZ_MUST_USE bool
ReadableStreamDefaultControllerClose(JSContext* cx,
                                     Handle<ReadableStreamDefaultController*> unwrappedController);

static MOZ_MUST_USE bool
ReadableStreamDefaultControllerEnqueue(JSContext* cx,
                                       Handle<ReadableStreamDefaultController*> unwrappedController,
                                       HandleValue chunk);

static bool
TeeReaderReadHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<TeeState*> teeState(cx, TargetFromHandler<TeeState>(args.callee()));
    HandleValue resultVal = args.get(0);

    // Step a: Assert: Type(result) is Object.
    RootedObject result(cx, &resultVal.toObject());

    // Step b: Let value be ? Get(result, "value").
    RootedValue value(cx);
    if (!GetPropertyPure(cx, result, NameToId(cx->names().value), value.address())) {
        return false;
    }

    // Step c: Let done be ? Get(result, "done").
    RootedValue doneVal(cx);
    if (!GetPropertyPure(cx, result, NameToId(cx->names().done), doneVal.address())) {
        return false;
    }

    // Step d: Assert: Type(done) is Boolean.
    bool done = doneVal.toBoolean();

    // Step e: If done is true and teeState.[[closedOrErrored]] is false,
    if (done && !teeState->closedOrErrored()) {
        // Step i: If teeState.[[canceled1]] is false,
        if (!teeState->canceled1()) {
            // Step 1: Perform ! ReadableStreamDefaultControllerClose(branch1).
            Rooted<ReadableStreamDefaultController*> branch1(cx, teeState->branch1());
            if (!ReadableStreamDefaultControllerClose(cx, branch1)) {
                return false;
            }
        }

        // Step ii: If teeState.[[canceled2]] is false,
        if (!teeState->canceled2()) {
            // Step 1: Perform ! ReadableStreamDefaultControllerClose(branch1).
            Rooted<ReadableStreamDefaultController*> branch2(cx, teeState->branch2());
            if (!ReadableStreamDefaultControllerClose(cx, branch2)) {
                return false;
            }
        }

        // Step iii: Set teeState.[[closedOrErrored]] to true.
        teeState->setClosedOrErrored();
    }

    // Step f: If teeState.[[closedOrErrored]] is true, return.
    if (teeState->closedOrErrored()) {
        return true;
    }

    // Step g: Let value1 and value2 be value.
    RootedValue value1(cx, value);
    RootedValue value2(cx, value);

    // Step h: If teeState.[[canceled2]] is false and cloneForBranch2 is
    //         true, set value2 to
    //         ? StructuredDeserialize(StructuredSerialize(value2),
    //                                 the current Realm Record).
    // TODO: add StructuredClone() intrinsic.
    MOZ_ASSERT(!teeState->cloneForBranch2(), "tee(cloneForBranch2=true) should not be exposed");

    // Step i: If teeState.[[canceled1]] is false, perform
    //         ? ReadableStreamDefaultControllerEnqueue(branch1, value1).
    Rooted<ReadableStreamDefaultController*> controller(cx);
    if (!teeState->canceled1()) {
        controller = teeState->branch1();
        if (!ReadableStreamDefaultControllerEnqueue(cx, controller, value1)) {
            return false;
        }
    }

    // Step j: If teeState.[[canceled2]] is false,
    //         perform ? ReadableStreamDefaultControllerEnqueue(branch2, value2).
    if (!teeState->canceled2()) {
        controller = teeState->branch2();
        if (!ReadableStreamDefaultControllerEnqueue(cx, controller, value2)) {
            return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

static MOZ_MUST_USE JSObject*
ReadableStreamDefaultReaderRead(JSContext* cx,
                                Handle<ReadableStreamDefaultReader*> unwrappedReader);

static MOZ_MUST_USE JSObject*
ReadableStreamTee_Pull(JSContext* cx, Handle<TeeState*> unwrappedTeeState)
{
    // Step 1: Let reader be F.[[reader]], branch1 be F.[[branch1]],
    //         branch2 be F.[[branch2]], teeState be F.[[teeState]], and
    //         cloneForBranch2 be F.[[cloneForBranch2]].

    // Step 2: Return the result of transforming
    //         ! ReadableStreamDefaultReaderRead(reader) by a fulfillment
    //         handler which takes the argument result and performs the
    //         following steps:
    Rooted<ReadableStream*> unwrappedStream(cx);
    if (!UnwrapInternalSlot(cx, unwrappedTeeState, TeeState::Slot_Stream, &unwrappedStream)) {
        return nullptr;
    }
    Rooted<ReadableStreamReader*> unwrappedReaderObj(cx);
    if (!UnwrapReaderFromStream(cx, unwrappedStream, &unwrappedReaderObj)) {
        return nullptr;
    }

    Rooted<ReadableStreamDefaultReader*> unwrappedReader(cx,
        &unwrappedReaderObj->as<ReadableStreamDefaultReader>());

    RootedObject readPromise(cx, ::ReadableStreamDefaultReaderRead(cx, unwrappedReader));
    if (!readPromise) {
        return nullptr;
    }

    RootedObject onFulfilled(cx, NewHandler(cx, TeeReaderReadHandler, unwrappedTeeState));
    if (!onFulfilled) {
        return nullptr;
    }

    return JS::CallOriginalPromiseThen(cx, readPromise, onFulfilled, nullptr);
}

/**
 * Cancel a tee'd stream's |branch| with the given |reason_|.
 *
 * Note: can operate on unwrapped values for |teeState| and |branch|.
 *
 * Objects created in the course of this function's operation are always
 * created in the current cx compartment.
 */
static MOZ_MUST_USE JSObject*
ReadableStreamTee_Cancel(JSContext* cx, Handle<TeeState*> teeState,
                         Handle<ReadableStreamDefaultController*> branch, HandleValue reason_)
{
    // Step 1: Let stream be F.[[stream]] and teeState be F.[[teeState]].
    Rooted<ReadableStream*> stream(cx);
    if (!UnwrapInternalSlot(cx, teeState, TeeState::Slot_Stream, &stream)) {
        return nullptr;
    }

    bool bothBranchesCanceled = false;

    // Step 2: Set teeState.[[canceled1]] to true.
    // Step 3: Set teeState.[[reason1]] to reason.
    {
        RootedValue reason(cx, reason_);
        if (reason.isGCThing() &&
            reason.toGCThing()->maybeCompartment() != teeState->compartment())
        {
            AutoRealm ar(cx, teeState);
            if (!cx->compartment()->wrap(cx, &reason)) {
                return nullptr;
            }
        }
        if (branch->isTeeBranch1()) {
            teeState->setCanceled1(reason);
            bothBranchesCanceled = teeState->canceled2();
        } else {
            MOZ_ASSERT(branch->isTeeBranch2());
            teeState->setCanceled2(reason);
            bothBranchesCanceled = teeState->canceled1();
        }
    }

    // Step 4: If teeState.[[canceled1]] is true,
    // Step 4: If teeState.[[canceled2]] is true,
    if (bothBranchesCanceled) {
        // Step a: Let compositeReason be
        //         ! CreateArrayFromList(« teeState.[[reason1]], teeState.[[reason2]] »).
        RootedNativeObject compositeReason(cx, NewDenseFullyAllocatedArray(cx, 2));
        if (!compositeReason) {
            return nullptr;
        }

        compositeReason->setDenseInitializedLength(2);

        RootedValue reason1(cx, teeState->reason1());
        RootedValue reason2(cx, teeState->reason2());
        if (teeState->compartment() != cx->compartment()) {
            if (!cx->compartment()->wrap(cx, &reason1) || !cx->compartment()->wrap(cx, &reason2)) {
                return nullptr;
            }
        }
        compositeReason->initDenseElement(0, reason1);
        compositeReason->initDenseElement(1, reason2);
        RootedValue compositeReasonVal(cx, ObjectValue(*compositeReason));

        Rooted<PromiseObject*> promise(cx, teeState->promise());

        // Step b: Let cancelResult be ! ReadableStreamCancel(stream, compositeReason).
        RootedObject cancelResult(cx, ::ReadableStreamCancel(cx, stream, compositeReasonVal));
        {
            AutoRealm ar(cx, promise);
            if (!cancelResult) {
                if (!RejectPromiseWithPendingError(cx, promise)) {
                    return nullptr;
                }
            } else {
                // Step c: Resolve teeState.[[promise]] with cancelResult.
                RootedValue resultVal(cx, ObjectValue(*cancelResult));
                if (!cx->compartment()->wrap(cx, &resultVal)) {
                    return nullptr;
                }
                if (!PromiseObject::resolve(cx, promise, resultVal)) {
                    return nullptr;
                }
            }
        }
    }

    // Step 5: Return teeState.[[promise]].
    RootedObject promise(cx, teeState->promise());
    if (promise->compartment() != cx->compartment()) {
        if (!cx->compartment()->wrap(cx, &promise)) {
            return nullptr;
        }
    }
    return promise;
}

static MOZ_MUST_USE bool
ReadableStreamDefaultControllerErrorIfNeeded(JSContext* cx,
                                             Handle<ReadableStreamDefaultController*> controller,
                                             HandleValue e);

// Streams spec, 3.3.9. step 18:
// Upon rejection of reader.[[closedPromise]] with reason r,
static bool
TeeReaderClosedHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<TeeState*> teeState(cx, TargetFromHandler<TeeState>(args.callee()));
    HandleValue reason = args.get(0);

    // Step a: If teeState.[[closedOrErrored]] is false, then:
    if (!teeState->closedOrErrored()) {
        // Step a.iii: Set teeState.[[closedOrErrored]] to true.
        // Reordered to ensure that internal errors in the other steps don't
        // leave the teeState in an undefined state.
        teeState->setClosedOrErrored();

        // Step a.i: Perform ! ReadableStreamDefaultControllerErrorIfNeeded(pull.[[branch1]], r).
        Rooted<ReadableStreamDefaultController*> branch1(cx, teeState->branch1());
        if (!ReadableStreamDefaultControllerErrorIfNeeded(cx, branch1, reason)) {
            return false;
        }

        // Step a.ii: Perform ! ReadableStreamDefaultControllerErrorIfNeeded(pull.[[branch2]], r).
        Rooted<ReadableStreamDefaultController*> branch2(cx, teeState->branch2());
        if (!ReadableStreamDefaultControllerErrorIfNeeded(cx, branch2, reason)) {
            return false;
        }
    }

    return true;
}

/**
 * Streams spec, 3.3.9. ReadableStreamTee ( stream, cloneForBranch2 )
 *
 * Note: can operate on unwrapped ReadableStream instances from
 * another compartment. The returned branch streams and their associated
 * controllers  are always created in the current cx compartment.
 */
static MOZ_MUST_USE bool
ReadableStreamTee(JSContext* cx, Handle<ReadableStream*> stream, bool cloneForBranch2,
                  MutableHandle<ReadableStream*> branch1Stream,
                  MutableHandle<ReadableStream*> branch2Stream)
{
    // Step 1: Assert: ! IsReadableStream(stream) is true (implicit).
    // Step 2: Assert: Type(cloneForBranch2) is Boolean (implicit).

    // Step 3: Let reader be ? AcquireReadableStreamDefaultReader(stream).
    Rooted<ReadableStreamDefaultReader*> reader(cx, CreateReadableStreamDefaultReader(cx, stream));
    if (!reader) {
        return false;
    }

    // Step 4: Let teeState be Record {[[closedOrErrored]]: false,
    //                                 [[canceled1]]: false,
    //                                 [[canceled2]]: false,
    //                                 [[reason1]]: undefined,
    //                                 [[reason2]]: undefined,
    //                                 [[promise]]: a new promise}.
    Rooted<TeeState*> teeState(cx, TeeState::create(cx, stream));
    if (!teeState) {
        return false;
    }

    // Steps 5-10 omitted because our implementation works differently.

    // Step 5: Let pull be a new ReadableStreamTee pull function.
    // Step 6: Set pull.[[reader]] to reader, pull.[[teeState]] to teeState, and
    //         pull.[[cloneForBranch2]] to cloneForBranch2.
    // Step 7: Let cancel1 be a new ReadableStreamTee branch 1 cancel function.
    // Step 8: Set cancel1.[[stream]] to stream and cancel1.[[teeState]] to
    //         teeState.

    // Step 9: Let cancel2 be a new ReadableStreamTee branch 2 cancel function.
    // Step 10: Set cancel2.[[stream]] to stream and cancel2.[[teeState]] to
    //          teeState.

    // Step 11: Let underlyingSource1 be ! ObjectCreate(%ObjectPrototype%).
    // Step 12: Perform ! CreateDataProperty(underlyingSource1, "pull", pull).
    // Step 13: Perform ! CreateDataProperty(underlyingSource1, "cancel", cancel1).

    // Step 14: Let branch1Stream be ! Construct(ReadableStream, underlyingSource1).
    RootedValue hwmValue(cx, NumberValue(1));
    RootedValue underlyingSource(cx, ObjectValue(*teeState));
    branch1Stream.set(ReadableStream::createDefaultStream(cx, underlyingSource,
                                                          UndefinedHandleValue,
                                                          hwmValue));
    if (!branch1Stream) {
        return false;
    }

    Rooted<ReadableStreamDefaultController*> branch1(cx);
    branch1 = &branch1Stream->controller()->as<ReadableStreamDefaultController>();
    branch1->setTeeBranch1();
    teeState->setBranch1(branch1);

    // Step 15: Let underlyingSource2 be ! ObjectCreate(%ObjectPrototype%).
    // Step 16: Perform ! CreateDataProperty(underlyingSource2, "pull", pull).
    // Step 17: Perform ! CreateDataProperty(underlyingSource2, "cancel", cancel2).

    // Step 18: Let branch2Stream be ! Construct(ReadableStream, underlyingSource2).
    branch2Stream.set(ReadableStream::createDefaultStream(cx, underlyingSource,
                                                          UndefinedHandleValue,
                                                          hwmValue));
    if (!branch2Stream) {
        return false;
    }

    Rooted<ReadableStreamDefaultController*> branch2(cx);
    branch2 = &branch2Stream->controller()->as<ReadableStreamDefaultController>();
    branch2->setTeeBranch2();
    teeState->setBranch2(branch2);

    // Step 19: Set pull.[[branch1]] to branch1Stream.[[readableStreamController]].
    // Step 20: Set pull.[[branch2]] to branch2Stream.[[readableStreamController]].
    // Our implementation stores the controllers on the TeeState instead.

    // Step 21: Upon rejection of reader.[[closedPromise]] with reason r,
    RootedObject closedPromise(cx, reader->closedPromise());

    RootedObject onRejected(cx, NewHandler(cx, TeeReaderClosedHandler, teeState));
    if (!onRejected) {
        return false;
    }

    if (!JS::AddPromiseReactions(cx, closedPromise, nullptr, onRejected)) {
        return false;
    }

    // Step 22: Return « branch1, branch2 ».
    return true;
}


/*** 3.4. The interface between readable streams and controllers *********************************/

inline static MOZ_MUST_USE bool
AppendToListAtSlot(JSContext* cx, HandleNativeObject container, uint32_t slot, HandleObject obj);

/**
 * Streams spec, 3.4.1. ReadableStreamAddReadIntoRequest ( stream )
 * Streams spec, 3.4.2. ReadableStreamAddReadRequest ( stream )
 *
 * Note: can operate on unwrapped ReadableStream instances from another
 * compartment.
 *
 * Note: The returned Promise is created in the current cx compartment.
 */
static MOZ_MUST_USE JSObject*
ReadableStreamAddReadOrReadIntoRequest(JSContext* cx, Handle<ReadableStream*> stream)
{
    // Step 1: Assert: ! IsReadableStreamBYOBReader(stream.[[reader]]) is true.
    // Skipped: handles both kinds of readers.
    Rooted<ReadableStreamReader*> reader(cx);
    if (!UnwrapReaderFromStream(cx, stream, &reader)) {
        return nullptr;
    }

    // Step 2 of 3.4.2: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT_IF(reader->is<ReadableStreamDefaultReader>(), stream->readable());

    // Step 3: Let promise be a new promise.
    RootedObject promise(cx, PromiseObject::createSkippingExecutor(cx));
    if (!promise) {
        return nullptr;
    }

    // Step 4: Let read{Into}Request be Record {[[promise]]: promise}.
    // Step 5: Append read{Into}Request as the last element of
    //         stream.[[reader]].[[read{Into}Requests]].
    // Since [[promise]] is the Record's only field, we store it directly.
    if (!AppendToListAtSlot(cx, reader, ReadableStreamReader::Slot_Requests, promise)) {
        return nullptr;
    }

    // Step 6: Return promise.
    return promise;
}

static MOZ_MUST_USE JSObject*
ReadableStreamControllerCancelSteps(JSContext* cx, Handle<ReadableStreamController*> controller,
                                    HandleValue reason);

// Used for transforming the result of promise fulfillment/rejection.
static bool
ReturnUndefined(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setUndefined();
    return true;
}

MOZ_MUST_USE bool
ReadableStreamCloseInternal(JSContext* cx, Handle<ReadableStream*> stream);

/**
 * Streams spec, 3.4.3. ReadableStreamCancel ( stream, reason )
 *
 * Note: can operate on unwrapped ReadableStream instances from
 * another compartment. `reason` must be in the cx compartment.
 */
static MOZ_MUST_USE JSObject*
ReadableStreamCancel(JSContext* cx, Handle<ReadableStream*> stream, HandleValue reason)
{
    AssertSameCompartment(cx, reason);

    // Step 1: Set stream.[[disturbed]] to true.
    stream->setDisturbed();

    // Step 2: If stream.[[state]] is "closed", return a new promise resolved
    //         with undefined.
    if (stream->closed()) {
        return PromiseObject::unforgeableResolve(cx, UndefinedHandleValue);
    }

    // Step 3: If stream.[[state]] is "errored", return a new promise rejected
    //         with stream.[[storedError]].
    if (stream->errored()) {
        RootedValue storedError(cx, stream->storedError());
        if (!cx->compartment()->wrap(cx, &storedError)) {
            return nullptr;
        }
        return PromiseObject::unforgeableReject(cx, storedError);
    }

    // Step 4: Perform ! ReadableStreamClose(stream).
    if (!ReadableStreamCloseInternal(cx, stream)) {
        return nullptr;
    }

    // Step 5: Let sourceCancelPromise be
    //         ! stream.[[readableStreamController]].[[CancelSteps]](reason).
    Rooted<ReadableStreamController*> controller(cx, stream->controller());
    RootedObject sourceCancelPromise(cx);
    sourceCancelPromise = ReadableStreamControllerCancelSteps(cx, controller, reason);
    if (!sourceCancelPromise) {
        return nullptr;
    }

    // Step 6: Return the result of transforming sourceCancelPromise by a
    //         fulfillment handler that returns undefined.
    RootedAtom funName(cx, cx->names().empty);
    RootedFunction returnUndefined(cx, NewNativeFunction(cx, ReturnUndefined, 0, funName));
    if (!returnUndefined) {
        return nullptr;
    }
    return JS::CallOriginalPromiseThen(cx, sourceCancelPromise, returnUndefined, nullptr);
}

/**
 * Streams spec, 3.4.4. ReadableStreamClose ( stream )
 *
 * Note: can operate on unwrapped ReadableStream instances from
 * another compartment.
 */
MOZ_MUST_USE bool
ReadableStreamCloseInternal(JSContext* cx, Handle<ReadableStream*> stream)
{
    // Step 1: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 2: Set stream.[[state]] to "closed".
    stream->setClosed();

    // Step 4: If reader is undefined, return (reordered).
    if (!stream->hasReader()) {
        return true;
    }

    // Step 3: Let reader be stream.[[reader]].
    Rooted<ReadableStreamReader*> reader(cx);
    if (!UnwrapReaderFromStream(cx, stream, &reader)) {
        return false;
    }

    // Step 5: If ! IsReadableStreamDefaultReader(reader) is true,
    if (reader->is<ReadableStreamDefaultReader>()) {
        // Step a: Repeat for each readRequest that is an element of
        //         reader.[[readRequests]],
        RootedNativeObject readRequests(cx, reader->requests());
        uint32_t len = readRequests->getDenseInitializedLength();
        RootedObject readRequest(cx);
        RootedObject resultObj(cx);
        RootedValue resultVal(cx);
        for (uint32_t i = 0; i < len; i++) {
            // Step i: Resolve readRequest.[[promise]] with
            //         ! CreateIterResultObject(undefined, true).
            readRequest = &readRequests->getDenseElement(i).toObject();
            if (!cx->compartment()->wrap(cx, &readRequest)) {
                return false;
            }

            resultObj = CreateIterResultObject(cx, UndefinedHandleValue, true);
            if (!resultObj) {
                return false;
            }
            resultVal = ObjectValue(*resultObj);
            if (!ResolvePromise(cx, readRequest, resultVal)) {
                return false;
            }
        }

        // Step b: Set reader.[[readRequests]] to an empty List.
        reader->clearRequests();
    }

    // Step 6: Resolve reader.[[closedPromise]] with undefined.
    // Step 7: Return (implicit).
    RootedObject closedPromise(cx, reader->closedPromise());
    if (!cx->compartment()->wrap(cx, &closedPromise)) {
        return false;
    }
    if (!ResolvePromise(cx, closedPromise, UndefinedHandleValue)) {
        return false;
    }

    if (stream->mode() == JS::ReadableStreamMode::ExternalSource &&
        cx->runtime()->readableStreamClosedCallback)
    {
        // Make sure we're in the stream's compartment.
        AutoRealm ar(cx, stream);
        ReadableStreamController* controller = stream->controller();
        void* source = controller->underlyingSource().toPrivate();
        cx->runtime()->readableStreamClosedCallback(cx, stream, source, stream->embeddingFlags());
    }

    return true;
}

/**
 * Streams spec, 3.4.6. ReadableStreamError ( stream, e )
 *
 * Note: can operate on unwrapped ReadableStream instances from
 * another compartment.
 */
MOZ_MUST_USE bool
ReadableStreamErrorInternal(JSContext* cx, Handle<ReadableStream*> stream, HandleValue e)
{
    // Step 1: Assert: ! IsReadableStream(stream) is true (implicit).

    // Step 2: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 3: Set stream.[[state]] to "errored".
    stream->setErrored();

    // Step 4: Set stream.[[storedError]] to e.
    {
        AutoRealm ar(cx, stream);
        RootedValue wrappedError(cx, e);
        if (!cx->compartment()->wrap(cx, &wrappedError)) {
            return false;
        }
        stream->setStoredError(wrappedError);
    }

    // Step 6: If reader is undefined, return (reordered).
    if (!stream->hasReader()) {
        return true;
    }

    // Step 5: Let reader be stream.[[reader]].
    Rooted<ReadableStreamReader*> reader(cx);
    if (!UnwrapReaderFromStream(cx, stream, &reader)) {
        return false;
    }

    // Steps 7,8: (Identical in our implementation.)
    // Step a: Repeat for each readRequest that is an element of
    //         reader.[[readRequests]],
    RootedNativeObject readRequests(cx, reader->requests());
    RootedObject readRequest(cx);
    RootedValue val(cx);
    uint32_t len = readRequests->getDenseInitializedLength();
    for (uint32_t i = 0; i < len; i++) {
        // Step i: Reject readRequest.[[promise]] with e.
        val = readRequests->getDenseElement(i);
        readRequest = &val.toObject();

        // Responses have to be created in the compartment from which the
        // error was triggered, which might not be the same as the one the
        // request was created in, so we have to wrap requests here.
        if (!cx->compartment()->wrap(cx, &readRequest)) {
            return false;
        }

        if (!RejectPromise(cx, readRequest, e)) {
            return false;
        }
    }

    // Step b: Set reader.[[readRequests]] to a new empty List.
    if (!SetNewList(cx, reader, ReadableStreamReader::Slot_Requests)) {
        return false;
    }

    // Step 9: Reject reader.[[closedPromise]] with e.
    RootedObject closedPromise(cx, reader->closedPromise());

    // The closedPromise might have been created in another compartment.
    // RejectPromise can deal with wrapped Promise objects, but has to be
    // with all arguments in the current compartment, so we do need to wrap
    // the Promise.
    if (!cx->compartment()->wrap(cx, &closedPromise)) {
        return false;
    }
    if (!RejectPromise(cx, closedPromise, e)) {
        return false;
    }

    if (stream->mode() == JS::ReadableStreamMode::ExternalSource &&
        cx->runtime()->readableStreamErroredCallback)
    {
        // Make sure we're in the stream's compartment.
        AutoRealm ar(cx, stream);
        ReadableStreamController* controller = stream->controller();
        void* source = controller->underlyingSource().toPrivate();

        // Ensure that the embedding doesn't have to deal with
        // mixed-compartment arguments to the callback.
        RootedValue error(cx, e);
        if (!cx->compartment()->wrap(cx, &error)) {
            return false;
        }

        cx->runtime()->readableStreamErroredCallback(cx, stream, source,
                                                     stream->embeddingFlags(), error);
    }

    return true;
}

/**
 * Streams spec, 3.4.7. ReadableStreamFulfillReadIntoRequest( stream, chunk, done )
 * Streams spec, 3.4.8. ReadableStreamFulfillReadRequest ( stream, chunk, done )
 * These two spec functions are identical in our implementation.
 *
 * Note: can operate on unwrapped values from other compartments for either
 * |stream| and/or |chunk|. The iteration result object created in the course
 * of this function's operation is created in the current cx compartment.
 */
static MOZ_MUST_USE bool
ReadableStreamFulfillReadOrReadIntoRequest(JSContext* cx, Handle<ReadableStream*> stream,
                                           HandleValue chunk, bool done)
{
    // Step 1: Let reader be stream.[[reader]].
    Rooted<ReadableStreamReader*> reader(cx);
    if (!UnwrapReaderFromStream(cx, stream, &reader)) {
        return false;
    }

    // Step 2: Let readIntoRequest be the first element of
    //         reader.[[readIntoRequests]].
    // Step 3: Remove readIntoRequest from reader.[[readIntoRequests]], shifting
    //         all other elements downward (so that the second becomes the first,
    //         and so on).
    RootedNativeObject readIntoRequests(cx, reader->requests());
    RootedObject readIntoRequest(cx, ShiftFromList<JSObject>(cx, readIntoRequests));
    MOZ_ASSERT(readIntoRequest);
    if (!cx->compartment()->wrap(cx, &readIntoRequest)) {
        return false;
    }

    // Step 4: Resolve readIntoRequest.[[promise]] with
    //         ! CreateIterResultObject(chunk, done).
    RootedValue wrappedChunk(cx, chunk);
    if (!cx->compartment()->wrap(cx, &wrappedChunk)) {
        return false;
    }
    RootedObject iterResult(cx, CreateIterResultObject(cx, wrappedChunk, done));
    if (!iterResult) {
        return false;
    }
    RootedValue val(cx, ObjectValue(*iterResult));
    return ResolvePromise(cx, readIntoRequest, val);
}

// Streams spec, 3.4.9. ReadableStreamGetNumReadIntoRequests ( stream )
// Streams spec, 3.4.10. ReadableStreamGetNumReadRequests ( stream )
// (Identical implementation.)
static uint32_t
ReadableStreamGetNumReadRequests(ReadableStream* stream)
{
    // Step 1: Return the number of elements in
    //         stream.[[reader]].[[readRequests]].
    if (!stream->hasReader()) {
        return 0;
    }

    JS::AutoSuppressGCAnalysis nogc;
    ReadableStreamReader* reader = UnwrapReaderFromStreamNoThrow(stream);

    // Reader is a dead wrapper, treat it as non-existent.
    if (!reader) {
        return 0;
    }

    return reader->requests()->getDenseInitializedLength();
}

// Streams spec 3.4.12. ReadableStreamHasDefaultReader ( stream )
static MOZ_MUST_USE bool
ReadableStreamHasDefaultReader(JSContext* cx, Handle<ReadableStream*> stream, bool* result)
{
    // Step 1: Let reader be stream.[[reader]].
    // Step 2: If reader is undefined, return false.
    if (!stream->hasReader()) {
        *result = false;
        return true;
    }

    // Step 3: If ! ReadableStreamDefaultReader(reader) is false, return false.
    // Step 4: Return true.
    Rooted<ReadableStreamReader*> reader(cx);
    if (!UnwrapReaderFromStream(cx, stream, &reader)) {
        return false;
    }

    *result = reader->is<ReadableStreamDefaultReader>();
    return true;
}


/*** 3.5. Class ReadableStreamDefaultReader ******************************************************/

static MOZ_MUST_USE bool
ReadableStreamReaderGenericInitialize(JSContext* cx,
                                      Handle<ReadableStreamReader*> reader,
                                      Handle<ReadableStream*> stream);

/**
 * Stream spec, 3.5.3. new ReadableStreamDefaultReader ( stream )
 * Steps 2-4.
 *
 * Note: can operate on unwrapped ReadableStream instances from
 * another compartment. The returned object will always be created in the
 * current cx compartment.
 */
static MOZ_MUST_USE ReadableStreamDefaultReader*
CreateReadableStreamDefaultReader(JSContext* cx, Handle<ReadableStream*> stream)
{
    Rooted<ReadableStreamDefaultReader*> reader(cx);
    reader = NewBuiltinClassInstance<ReadableStreamDefaultReader>(cx);
    if (!reader) {
        return nullptr;
    }

    // Step 2: If ! IsReadableStreamLocked(stream) is true, throw a TypeError
    //         exception.
    if (stream->locked()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_READABLESTREAM_LOCKED);
        return nullptr;
    }

    // Step 3: Perform ! ReadableStreamReaderGenericInitialize(this, stream).
    if (!ReadableStreamReaderGenericInitialize(cx, reader, stream)) {
        return nullptr;
    }

    // Step 4: Set this.[[readRequests]] to a new empty List.
    if (!SetNewList(cx, reader, ReadableStreamReader::Slot_Requests)) {
        return nullptr;
    }

    return reader;
}

/**
 * Stream spec, 3.5.3. new ReadableStreamDefaultReader ( stream )
 *
 * Note: can handle ReadableStream instances from another compartment.
 */
bool
ReadableStreamDefaultReader::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "ReadableStreamDefaultReader")) {
        return false;
    }

    // Step 1: If ! IsReadableStream(stream) is false, throw a TypeError exception.
    if (!IsMaybeWrapped<ReadableStream>(args.get(0))) {
        ReportArgTypeError(cx, "ReadableStreamDefaultReader", "ReadableStream", args.get(0));
        return false;
    }

    Rooted<ReadableStream*> stream(cx,
                                   &CheckedUnwrap(&args.get(0).toObject())->as<ReadableStream>());

    RootedObject reader(cx, CreateReadableStreamDefaultReader(cx, stream));
    if (!reader) {
        return false;
    }

    args.rval().setObject(*reader);
    return true;
}

// Streams spec, 3.5.4.1 get closed
static MOZ_MUST_USE bool
ReadableStreamDefaultReader_closed(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStreamDefaultReader(this) is false, return a promise
    //         rejected with a TypeError exception.
    Rooted<ReadableStreamDefaultReader*> reader(cx);
    if (!UnwrapThisForNonGenericMethod(cx,
                                       args.thisv(),
                                       "ReadableStreamDefaultReader",
                                       "get closed",
                                       &reader))
    {
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 2: Return this.[[closedPromise]].
    RootedObject closedPromise(cx, reader->closedPromise());
    if (!cx->compartment()->wrap(cx, &closedPromise)) {
        return false;
    }

    args.rval().setObject(*closedPromise);
    return true;
}

static MOZ_MUST_USE JSObject*
ReadableStreamReaderGenericCancel(JSContext* cx, Handle<ReadableStreamReader*> reader,
                                  HandleValue reason);

// Streams spec, 3.5.4.2. cancel ( reason )
static MOZ_MUST_USE bool
ReadableStreamDefaultReader_cancel(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStreamDefaultReader(this) is false, return a promise
    //         rejected with a TypeError exception.
    Rooted<ReadableStreamDefaultReader*> reader(cx);
    if (!UnwrapThisForNonGenericMethod(cx,
                                       args.thisv(),
                                       "ReadableStreamDefaultReader",
                                       "cancel",
                                       &reader))
    {
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 2: If this.[[ownerReadableStream]] is undefined, return a promise
    //         rejected with a TypeError exception.
    if (!reader->hasStream()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMREADER_NOT_OWNED, "cancel");
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 3: Return ! ReadableStreamReaderGenericCancel(this, reason).
    JSObject* cancelPromise = ReadableStreamReaderGenericCancel(cx, reader, args.get(0));
    if (!cancelPromise) {
        return false;
    }
    args.rval().setObject(*cancelPromise);
    return true;
}

// Streams spec, 3.5.4.3 read ( )
static MOZ_MUST_USE bool
ReadableStreamDefaultReader_read(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStreamDefaultReader(this) is false, return a promise
    //         rejected with a TypeError exception.
    Rooted<ReadableStreamDefaultReader*> reader(cx);
    if (!UnwrapThisForNonGenericMethod(cx,
                                       args.thisv(),
                                       "ReadableStreamDefaultReader",
                                       "read",
                                       &reader))
    {
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 2: If this.[[ownerReadableStream]] is undefined, return a promise
    //         rejected with a TypeError exception.
    if (!reader->hasStream()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMREADER_NOT_OWNED, "read");
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 3: Return ! ReadableStreamDefaultReaderRead(this).
    JSObject* readPromise = ::ReadableStreamDefaultReaderRead(cx, reader);
    if (!readPromise) {
        return false;
    }
    args.rval().setObject(*readPromise);
    return true;
}

static MOZ_MUST_USE bool
ReadableStreamReaderGenericRelease(JSContext* cx, Handle<ReadableStreamReader*> reader);

/**
 * Streams spec, 3.5.4.4. releaseLock ( )
 *
 * Note: can operate on unwrapped ReadableStreamDefaultReader instances from
 * another compartment.
 */
static bool
ReadableStreamDefaultReader_releaseLock(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamDefaultReader(this) is false,
    //         throw a TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<ReadableStreamDefaultReader*> reader(cx);
    if (!UnwrapThisForNonGenericMethod(cx,
                                       args.thisv(),
                                       "ReadableStreamDefaultReader",
                                       "releaseLock",
                                       &reader))
    {
        return false;
    }

    // Step 2: If this.[[ownerReadableStream]] is undefined, return.
    if (!reader->hasStream()) {
        args.rval().setUndefined();
        return true;
    }

    // Step 3: If this.[[readRequests]] is not empty, throw a TypeError exception.
    Value val = reader->getFixedSlot(ReadableStreamReader::Slot_Requests);
    if (!val.isUndefined()) {
        NativeObject* readRequests = &val.toObject().as<NativeObject>();
        uint32_t len = readRequests->getDenseInitializedLength();
        if (len != 0) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLESTREAMREADER_NOT_EMPTY,
                                      "releaseLock");
            return false;
        }
    }

    // Step 4: Perform ! ReadableStreamReaderGenericRelease(this).
    return ReadableStreamReaderGenericRelease(cx, reader);
}

static const JSFunctionSpec ReadableStreamDefaultReader_methods[] = {
    JS_FN("cancel",         ReadableStreamDefaultReader_cancel,         1, 0),
    JS_FN("read",           ReadableStreamDefaultReader_read,           0, 0),
    JS_FN("releaseLock",    ReadableStreamDefaultReader_releaseLock,    0, 0),
    JS_FS_END
};

static const JSPropertySpec ReadableStreamDefaultReader_properties[] = {
    JS_PSG("closed", ReadableStreamDefaultReader_closed, 0),
    JS_PS_END
};

const Class ReadableStreamReader::class_ = {
    "ReadableStreamReader"
};

CLASS_SPEC(ReadableStreamDefaultReader, 1, SlotCount, ClassSpec::DontDefineConstructor, 0,
           JS_NULL_CLASS_OPS);


/*** 3.7. Readable stream reader abstract operations *********************************************/

// Streams spec, 3.7.1. IsReadableStreamDefaultReader ( x )
// Implemented via is<ReadableStreamDefaultReader>()

// Streams spec, 3.7.2. IsReadableStreamBYOBReader ( x )
// Implemented via is<ReadableStreamBYOBReader>()

/**
 * Streams spec, 3.7.3. ReadableStreamReaderGenericCancel ( reader, reason )
 *
 * Note: can operate on unwrapped ReadableStream reader instances from
 * another compartment.
 */
static MOZ_MUST_USE JSObject*
ReadableStreamReaderGenericCancel(JSContext* cx, Handle<ReadableStreamReader*> reader,
                                  HandleValue reason)
{
    // Step 1: Let stream be reader.[[ownerReadableStream]].
    // Step 2: Assert: stream is not undefined (implicit).
    Rooted<ReadableStream*> stream(cx);
    if (!UnwrapStreamFromReader(cx, reader, &stream)) {
        return nullptr;
    }

    // Step 3: Return ! ReadableStreamCancel(stream, reason).
    return ::ReadableStreamCancel(cx, stream, reason);
}

/**
 * Streams spec, 3.7.4. ReadableStreamReaderGenericInitialize ( reader, stream )
 *
 * Note: can operate on unwrapped ReadableStream reader instances from
 * another compartment.
 */
static MOZ_MUST_USE bool
ReadableStreamReaderGenericInitialize(JSContext* cx, Handle<ReadableStreamReader*> reader,
                                      Handle<ReadableStream*> stream)
{
    // Step 1: Set reader.[[ownerReadableStream]] to stream.
    // Step 2: Set stream.[[reader]] to reader.
    if (!IsObjectInContextCompartment(stream, cx)) {
        RootedObject wrappedStream(cx, stream);
        if (!cx->compartment()->wrap(cx, &wrappedStream)) {
            return false;
        }
        reader->setStream(wrappedStream);
        AutoRealm ar(cx, stream);
        RootedObject wrappedReader(cx, reader);
        if (!cx->compartment()->wrap(cx, &wrappedReader)) {
            return false;
        }
        stream->setReader(wrappedReader);
    } else {
        reader->setStream(stream);
        stream->setReader(reader);
    }

    // Step 3: If stream.[[state]] is "readable",
    RootedObject promise(cx);
    if (stream->readable()) {
        // Step a: Set reader.[[closedPromise]] to a new promise.
        promise = PromiseObject::createSkippingExecutor(cx);
    } else if (stream->closed()) {
        // Step 4: Otherwise
        // Step a: If stream.[[state]] is "closed",
        // Step i: Set reader.[[closedPromise]] to a new promise resolved with
        //         undefined.
        promise = PromiseObject::unforgeableResolve(cx, UndefinedHandleValue);
    } else {
        // Step b: Otherwise,
        // Step i: Assert: stream.[[state]] is "errored".
        MOZ_ASSERT(stream->errored());

        // Step ii: Set reader.[[closedPromise]] to a new promise rejected with
        //          stream.[[storedError]].
        RootedValue storedError(cx, stream->storedError());
        if (!cx->compartment()->wrap(cx, &storedError)) {
            return false;
        }
        promise = PromiseObject::unforgeableReject(cx, storedError);
    }

    if (!promise) {
        return false;
    }

    reader->setClosedPromise(promise);
    return true;
}

/**
 * Streams spec, 3.7.5. ReadableStreamReaderGenericRelease ( reader )
 *
 * Note: can operate on unwrapped ReadableStream reader instances from
 * another compartment.
 */
static MOZ_MUST_USE bool
ReadableStreamReaderGenericRelease(JSContext* cx, Handle<ReadableStreamReader*> reader)
{
    // Step 1: Assert: reader.[[ownerReadableStream]] is not undefined.
    Rooted<ReadableStream*> stream(cx);
    if (!UnwrapStreamFromReader(cx, reader, &stream)) {
        return false;
    }

    // Step 2: Assert: reader.[[ownerReadableStream]].[[reader]] is reader.
    MOZ_ASSERT(UnwrapReaderFromStreamNoThrow(stream) == reader);

    // Create an exception to reject promises with below. We don't have a
    // clean way to do this, unfortunately.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_READABLESTREAMREADER_RELEASED);
    RootedValue exn(cx);
    if (!cx->isExceptionPending() || !GetAndClearException(cx, &exn)) {
        // Uncatchable error. Die immediately without resolving
        // reader.[[closedPromise]].
        return false;
    }

    // Step 3: If reader.[[ownerReadableStream]].[[state]] is "readable", reject
    //         reader.[[closedPromise]] with a TypeError exception.
    if (stream->readable()) {
        Rooted<PromiseObject*> closedPromise(cx);
        if (!UnwrapInternalSlot(cx,
                                reader,
                                ReadableStreamReader::Slot_ClosedPromise,
                                &closedPromise))
        {
            return false;
        }

        AutoRealm ar(cx, closedPromise);
        if (!cx->compartment()->wrap(cx, &exn)) {
            return false;
        }
        if (!PromiseObject::reject(cx, closedPromise, exn)) {
            return false;
        }
    } else {
        // Step 4: Otherwise, set reader.[[closedPromise]] to a new promise rejected
        //         with a TypeError exception.
        RootedObject closedPromise(cx, PromiseObject::unforgeableReject(cx, exn));
        if (!closedPromise) {
            return false;
        }

        AutoRealm ar(cx, reader);
        if (!cx->compartment()->wrap(cx, &closedPromise)) {
            return false;
        }
        reader->setClosedPromise(closedPromise);
    }

    // Step 5: Set reader.[[ownerReadableStream]].[[reader]] to undefined.
    stream->clearReader();

    // Step 6: Set reader.[[ownerReadableStream]] to undefined.
    reader->clearStream();

    return true;
}

static MOZ_MUST_USE JSObject*
ReadableStreamControllerPullSteps(JSContext* cx, Handle<ReadableStreamController*> controller);

/**
 * Streams spec, 3.7.7. ReadableStreamDefaultReaderRead ( reader )
 *
 * Note: can operate on unwrapped ReadableStreamDefaultReader instances from
 * another compartment.
 */
static MOZ_MUST_USE JSObject*
ReadableStreamDefaultReaderRead(JSContext* cx,
                                Handle<ReadableStreamDefaultReader*> unwrappedReader)
{
    // Step 1: Let stream be reader.[[ownerReadableStream]].
    // Step 2: Assert: stream is not undefined.
    Rooted<ReadableStream*> unwrappedStream(cx);
    if (!UnwrapStreamFromReader(cx, unwrappedReader, &unwrappedStream)) {
        return nullptr;
    }

    // Step 3: Set stream.[[disturbed]] to true.
    unwrappedStream->setDisturbed();

    // Step 4: If stream.[[state]] is "closed", return a new promise resolved with
    //         ! CreateIterResultObject(undefined, true).
    if (unwrappedStream->closed()) {
        RootedObject iterResult(cx, CreateIterResultObject(cx, UndefinedHandleValue, true));
        if (!iterResult) {
            return nullptr;
        }
        RootedValue iterResultVal(cx, ObjectValue(*iterResult));
        return PromiseObject::unforgeableResolve(cx, iterResultVal);
    }

    // Step 5: If stream.[[state]] is "errored", return a new promise rejected with
    //         stream.[[storedError]].
    if (unwrappedStream->errored()) {
        RootedValue storedError(cx, unwrappedStream->storedError());
        if (!cx->compartment()->wrap(cx, &storedError)) {
            return nullptr;
        }
        return PromiseObject::unforgeableReject(cx, storedError);
    }

    // Step 6: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(unwrappedStream->readable());

    // Step 7: Return ! stream.[[readableStreamController]].[[PullSteps]]().
    Rooted<ReadableStreamController*> unwrappedController(cx,
        unwrappedStream->controller());
    return ReadableStreamControllerPullSteps(cx, unwrappedController);
}


/*** 3.8. Class ReadableStreamDefaultController **************************************************/

inline static MOZ_MUST_USE bool
ReadableStreamControllerCallPullIfNeeded(JSContext* cx,
                                         Handle<ReadableStreamController*> unwrappedController);

// Streams spec, 3.8.3, step 11.a.
// and
// Streams spec, 3.10.3, step 16.a.
static bool
ControllerStartHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<ReadableStreamController*> controller(cx);
    controller = TargetFromHandler<ReadableStreamController>(args.callee());

    // Step i: Set controller.[[started]] to true.
    controller->setStarted();

    // Step ii: Assert: controller.[[pulling]] is false.
    MOZ_ASSERT(!controller->pulling());

    // Step iii: Assert: controller.[[pullAgain]] is false.
    MOZ_ASSERT(!controller->pullAgain());

    // Step iv: Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(controller).
    // or
    // Step iv: Perform ! ReadableByteStreamControllerCallPullIfNeeded((controller).
    if (!ReadableStreamControllerCallPullIfNeeded(cx, controller)) {
        return false;
    }
    args.rval().setUndefined();
    return true;
}

static MOZ_MUST_USE bool
ReadableStreamControllerError(JSContext* cx, Handle<ReadableStreamController*> unwrappedController,
                              HandleValue e);

// Streams spec, 3.8.3, step 11.b.
// and
// Streams spec, 3.10.3, step 16.b.
static bool
ControllerStartFailedHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<ReadableStreamController*> controllerObj(cx);
    controllerObj = TargetFromHandler<ReadableStreamController>(args.callee());

    // 3.8.3, Step 11.b.i:
    // Perform ! ReadableStreamDefaultControllerErrorIfNeeded(controller, r).
    if (controllerObj->is<ReadableStreamDefaultController>()) {
        Rooted<ReadableStreamDefaultController*> controller(cx);
        controller = &controllerObj->as<ReadableStreamDefaultController>();
        return ReadableStreamDefaultControllerErrorIfNeeded(cx, controller, args.get(0));
    }

    // 3.10.3, Step 16.b.i: If stream.[[state]] is "readable", perform
    //                      ! ReadableByteStreamControllerError(controller, r).
    if (controllerObj->stream()->readable()) {
        return ReadableStreamControllerError(cx, controllerObj, args.get(0));
    }

    args.rval().setUndefined();
    return true;
}

static MOZ_MUST_USE bool
ValidateAndNormalizeHighWaterMark(JSContext* cx,
                                  HandleValue highWaterMarkVal,
                                  double* highWaterMark);

static MOZ_MUST_USE bool
ValidateAndNormalizeQueuingStrategy(JSContext* cx,
                                    HandleValue size,
                                    HandleValue highWaterMarkVal,
                                    double* highWaterMark);

/**
 * Streams spec, 3.8.3 new ReadableStreamDefaultController ( stream, underlyingSource,
 *                                                           size, highWaterMark )
 * Steps 3 - 11.
 *
 * Note: can NOT operate on unwrapped ReadableStream instances from
 * another compartment: ReadableStream controllers must be created in the same
 * compartment as the stream.
 */
static MOZ_MUST_USE ReadableStreamDefaultController*
CreateReadableStreamDefaultController(JSContext* cx, Handle<ReadableStream*> stream,
                                      HandleValue underlyingSource, HandleValue size,
                                      HandleValue highWaterMarkVal)
{
    cx->check(stream, underlyingSource, size, highWaterMarkVal);

    Rooted<ReadableStreamDefaultController*> controller(cx);
    controller = NewBuiltinClassInstance<ReadableStreamDefaultController>(cx);
    if (!controller) {
        return nullptr;
    }

    // Step 3: Set this.[[controlledReadableStream]] to stream.
    controller->setStream(stream);

    // Step 4: Set this.[[underlyingSource]] to underlyingSource.
    controller->setUnderlyingSource(underlyingSource);

    // Step 5: Perform ! ResetQueue(this).
    if (!ResetQueue(cx, controller)) {
        return nullptr;
    }

    // Step 6: Set this.[[started]], this.[[closeRequested]], this.[[pullAgain]],
    //         and this.[[pulling]] to false.
    controller->setFlags(0);

    // Step 7: Let normalizedStrategy be
    //         ? ValidateAndNormalizeQueuingStrategy(size, highWaterMark).
    double highWaterMark;
    if (!ValidateAndNormalizeQueuingStrategy(cx, size, highWaterMarkVal, &highWaterMark)) {
        return nullptr;
    }

    // Step 8: Set this.[[strategySize]] to normalizedStrategy.[[size]] and
    //         this.[[strategyHWM]] to normalizedStrategy.[[highWaterMark]].
    controller->setStrategySize(size);
    controller->setStrategyHWM(highWaterMark);

    // Step 9: Let controller be this (implicit).

    // Step 10: Let startResult be
    //          ? InvokeOrNoop(underlyingSource, "start", « this »).
    RootedValue startResult(cx);
    RootedValue controllerVal(cx, ObjectValue(*controller));
    if (!InvokeOrNoop(cx, underlyingSource, cx->names().start, controllerVal, &startResult)) {
        return nullptr;
    }

    // Step 11: Let startPromise be a promise resolved with startResult:
    RootedObject startPromise(cx, PromiseObject::unforgeableResolve(cx, startResult));
    if (!startPromise) {
        return nullptr;
    }

    RootedObject onStartFulfilled(cx, NewHandler(cx, ControllerStartHandler, controller));
    if (!onStartFulfilled) {
        return nullptr;
    }

    RootedObject onStartRejected(cx, NewHandler(cx, ControllerStartFailedHandler, controller));
    if (!onStartRejected) {
        return nullptr;
    }

    if (!JS::AddPromiseReactions(cx, startPromise, onStartFulfilled, onStartRejected)) {
        return nullptr;
    }

    return controller;
}

// Streams spec, 3.8.3.
// new ReadableStreamDefaultController( stream, underlyingSource, size,
//                                      highWaterMark )
bool
ReadableStreamDefaultController::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "ReadableStreamDefaultController")) {
        return false;
    }

    // Step 1: If ! IsReadableStream(stream) is false, throw a TypeError exception.
    HandleValue streamVal = args.get(0);
    if (!Is<ReadableStream>(streamVal)) {
        ReportArgTypeError(cx, "ReadableStreamDefaultController", "ReadableStream",
                           args.get(0));
        return false;
    }

    Rooted<ReadableStream*> stream(cx, &streamVal.toObject().as<ReadableStream>());

    // Step 2: If stream.[[readableStreamController]] is not undefined, throw a
    //         TypeError exception.
    if (stream->hasController()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_CONTROLLER_SET);
        return false;
    }

    // Steps 3-11.
    RootedObject controller(cx, CreateReadableStreamDefaultController(cx, stream, args.get(1),
                                                                      args.get(2), args.get(3)));
    if (!controller) {
        return false;
    }

    args.rval().setObject(*controller);
    return true;
}

static MOZ_MUST_USE double
ReadableStreamControllerGetDesiredSizeUnchecked(ReadableStreamController* controller);

// Streams spec, 3.8.4.1. get desiredSize
// and
// Streams spec, 3.10.4.2. get desiredSize
static bool
ReadableStreamDefaultController_desiredSize(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
    //         TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<ReadableStreamController*> unwrappedController(cx);
    if (!UnwrapThisForNonGenericMethod(cx,
                                       args.thisv(),
                                       "ReadableStreamDefaultController",
                                       "get desiredSize",
                                       &unwrappedController))
    {
        return false;
    }

    // Streams spec, 3.9.8. steps 1-4.
    // 3.9.8. Step 1: Let stream be controller.[[controlledReadableStream]].
    ReadableStream* unwrappedStream = unwrappedController->stream();

    // 3.9.8. Step 2: Let state be stream.[[state]].
    // 3.9.8. Step 3: If state is "errored", return null.
    if (unwrappedStream->errored()) {
        args.rval().setNull();
        return true;
    }

    // 3.9.8. Step 4: If state is "closed", return 0.
    if (unwrappedStream->closed()) {
        args.rval().setInt32(0);
        return true;
    }

    // Step 2: Return ! ReadableStreamDefaultControllerGetDesiredSize(this).
    args.rval().setNumber(ReadableStreamControllerGetDesiredSizeUnchecked(unwrappedController));
    return true;
}

static MOZ_MUST_USE bool
ReadableStreamDefaultControllerClose(JSContext* cx,
                                     Handle<ReadableStreamDefaultController*> unwrappedController);

/**
 * Unified implementation of step 2 of 3.8.4.2 and steps 2-3 of 3.10.4.3.
 *
 * Note: can operate on unwrapped ReadableStreamController instances from
 * another compartment.
 */
static MOZ_MUST_USE bool
VerifyControllerStateForClosing(JSContext* cx,
                                Handle<ReadableStreamController*> unwrappedController)
{
    // Step 2: If this.[[closeRequested]] is true, throw a TypeError exception.
    if (unwrappedController->closeRequested()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_CLOSED, "close");
        return false;
    }

    // Step 3: If this.[[controlledReadableStream]].[[state]] is not "readable",
    //         throw a TypeError exception.
    ReadableStream* unwrappedStream = unwrappedController->stream();
    if (!unwrappedStream->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "close");
        return false;
    }

    return true;
}

/**
 * Streams spec, 3.8.4.2 close()
 */
static bool
ReadableStreamDefaultController_close(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
    //         TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<ReadableStreamDefaultController*> unwrappedController(cx);
    if (!UnwrapThisForNonGenericMethod(cx,
                                       args.thisv(),
                                       "ReadableStreamDefaultController",
                                       "close",
                                       &unwrappedController))
    {
        return false;
    }

    // Steps 2-3.
    if (!VerifyControllerStateForClosing(cx, unwrappedController)) {
        return false;
    }

    // Step 4: Perform ! ReadableStreamDefaultControllerClose(this).
    if (!ReadableStreamDefaultControllerClose(cx, unwrappedController)) {
        return false;
    }
    args.rval().setUndefined();
    return true;
}

/**
 * Streams spec, 3.8.4.3. enqueue ( chunk )
 */
static bool
ReadableStreamDefaultController_enqueue(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
    //         TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<ReadableStreamDefaultController*> unwrappedController(cx);
    if (!UnwrapThisForNonGenericMethod(cx,
                                       args.thisv(),
                                       "ReadableStreamDefaultController",
                                       "enqueue",
                                       &unwrappedController))
    {
        return false;
    }

    // Step 2: If this.[[closeRequested]] is true, throw a TypeError exception.
    if (unwrappedController->closeRequested()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_CLOSED, "enqueue");
        return false;
    }

    // Step 3: If this.[[controlledReadableStream]].[[state]] is not "readable",
    //         throw a TypeError exception.
    if (!unwrappedController->stream()->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "enqueue");
        return false;
    }

    // Step 4: Return ! ReadableStreamDefaultControllerEnqueue(this, chunk).
    if (!ReadableStreamDefaultControllerEnqueue(cx, unwrappedController, args.get(0))) {
        return false;
    }
    args.rval().setUndefined();
    return true;
}

/**
 * Streams spec, 3.8.4.4. error ( e )
 */
static bool
ReadableStreamDefaultController_error(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
    //         TypeError exception.

    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<ReadableStreamDefaultController*> unwrappedController(cx);
    if (!UnwrapThisForNonGenericMethod(cx,
                                       args.thisv(),
                                       "ReadableStreamDefaultController",
                                       "enqueue",
                                       &unwrappedController))
    {
        return false;
    }

    // Step 2: Let stream be this.[[controlledReadableStream]].
    // Step 3: If stream.[[state]] is not "readable", throw a TypeError exception.
    if (!unwrappedController->stream()->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "error");
        return false;
    }

    // Step 4: Perform ! ReadableStreamDefaultControllerError(this, e).
    if (!ReadableStreamControllerError(cx, unwrappedController, args.get(0))) {
        return false;
    }
    args.rval().setUndefined();
    return true;
}

static const JSPropertySpec ReadableStreamDefaultController_properties[] = {
    JS_PSG("desiredSize", ReadableStreamDefaultController_desiredSize, 0),
    JS_PS_END
};

static const JSFunctionSpec ReadableStreamDefaultController_methods[] = {
    JS_FN("close",      ReadableStreamDefaultController_close,      0, 0),
    JS_FN("enqueue",    ReadableStreamDefaultController_enqueue,    1, 0),
    JS_FN("error",      ReadableStreamDefaultController_error,      1, 0),
    JS_FS_END
};

const Class ReadableStreamController::class_ = {
    "ReadableStreamController"
};

CLASS_SPEC(ReadableStreamDefaultController, 4, SlotCount, ClassSpec::DontDefineConstructor, 0,
           JS_NULL_CLASS_OPS);

/**
 * Unified implementation of ReadableStream controllers' [[CancelSteps]] internal
 * methods.
 * Streams spec, 3.8.5.1. [[CancelSteps]] ( reason )
 * and
 * Streams spec, 3.10.5.1. [[CancelSteps]] ( reason )
 *
 * Note: can operate on unwrapped ReadableStreamController instances
 * from another compartment. |reason| must be in the current cx compartment.
 */
static MOZ_MUST_USE JSObject*
ReadableStreamControllerCancelSteps(JSContext* cx,
                                    Handle<ReadableStreamController*> unwrappedController,
                                    HandleValue reason)
{
    AssertSameCompartment(cx, reason);

    // Step 1 of 3.10.5.1: If this.[[pendingPullIntos]] is not empty,
    if (!unwrappedController->is<ReadableStreamDefaultController>()) {
        RootedNativeObject unwrappedPendingPullIntos(cx,
            unwrappedController->as<ReadableByteStreamController>().pendingPullIntos());

        if (unwrappedPendingPullIntos->getDenseInitializedLength() != 0) {
            // Step a: Let firstDescriptor be the first element of
            //         this.[[pendingPullIntos]].
            PullIntoDescriptor* unwrappedDescriptor =
                ToUnwrapped<PullIntoDescriptor>(cx, PeekList<JSObject>(unwrappedPendingPullIntos));
            if (!unwrappedDescriptor) {
                return nullptr;
            }

            // Step b: Set firstDescriptor.[[bytesFilled]] to 0.
            unwrappedDescriptor->setBytesFilled(0);
        }
    }

    RootedValue unwrappedUnderlyingSource(cx);
    unwrappedUnderlyingSource = unwrappedController->underlyingSource();

    // Step 1 of 3.8.5.1, step 2 of 3.10.5.1: Perform ! ResetQueue(this).
    if (!ResetQueue(cx, unwrappedController)) {
        return nullptr;
    }

    // Step 2 of 3.8.5.1, step 3 of 3.10.5.1:
    // Return ! PromiseInvokeOrNoop(this.[[underlying(Byte)Source]],
    //                              "cancel", « reason »)
    // Note: this special-cases the underlying source of tee'd stream's
    // branches. Instead of storing a JSFunction as the "cancel" property on
    // those, we check if the source is a, maybe wrapped, TeeState instance
    // and manually dispatch to the right internal function. TeeState is fully
    // under our control, so this isn't content-observable.
    if (IsMaybeWrapped<TeeState>(unwrappedUnderlyingSource)) {
        Rooted<TeeState*> unwrappedteeState(cx);
        unwrappedteeState = &unwrappedUnderlyingSource.toObject().unwrapAs<TeeState>();
        Rooted<ReadableStreamDefaultController*> unwrappedDefaultController(cx);
        unwrappedDefaultController = &unwrappedController->as<ReadableStreamDefaultController>();
        return ReadableStreamTee_Cancel(cx, unwrappedteeState, unwrappedDefaultController,
                                        reason);
    }

    if (unwrappedController->hasExternalSource()) {
        RootedValue rval(cx);
        {
            AutoRealm ar(cx, unwrappedController);
            Rooted<ReadableStream*> stream(cx, unwrappedController->stream());
            void* source = unwrappedUnderlyingSource.toPrivate();
            RootedValue wrappedReason(cx, reason);
            if (!cx->compartment()->wrap(cx, &wrappedReason)) {
                return nullptr;
            }

            cx->check(stream, wrappedReason);
            rval = cx->runtime()->readableStreamCancelCallback(cx, stream, source,
                                                               stream->embeddingFlags(),
                                                               wrappedReason);
        }

        if (!cx->compartment()->wrap(cx, &rval)) {
            return nullptr;
        }
        return PromiseObject::unforgeableResolve(cx, rval);
    }

    // If the stream and its controller aren't in the cx compartment, we have
    // to ensure that the underlying source is correctly wrapped before
    // operating on it.
    if (!cx->compartment()->wrap(cx, &unwrappedUnderlyingSource)) {
        return nullptr;
    }

    return PromiseInvokeOrNoop(cx, unwrappedUnderlyingSource, cx->names().cancel, reason);
}

inline static MOZ_MUST_USE bool
DequeueValue(JSContext* cx,
             Handle<ReadableStreamController*> unwrappedContainer,
             MutableHandleValue chunk);

/**
 * Streams spec, 3.8.5.2. ReadableStreamDefaultController [[PullSteps]]()
 *
 * Note: can operate on unwrapped ReadableStreamDefaultController instances
 * from another compartment.
 */
static JSObject*
ReadableStreamDefaultControllerPullSteps(JSContext* cx,
                                         Handle<ReadableStreamDefaultController*> unwrappedController)
{
    // Step 1: Let stream be this.[[controlledReadableStream]].
    Rooted<ReadableStream*> unwrappedStream(cx, unwrappedController->stream());

    // Step 2: If this.[[queue]] is not empty,
    RootedNativeObject unwrappedQueue(cx);
    RootedValue val(cx, unwrappedController->getFixedSlot(StreamController::Slot_Queue));
    if (val.isObject()) {
        unwrappedQueue = &val.toObject().as<NativeObject>();
    }

    if (unwrappedQueue && unwrappedQueue->getDenseInitializedLength() != 0) {
        // Step a: Let chunk be ! DequeueValue(this.[[queue]]).
        RootedValue chunk(cx);
        if (!DequeueValue(cx, unwrappedController, &chunk)) {
            return nullptr;
        }

        // Step b: If this.[[closeRequested]] is true and this.[[queue]] is empty,
        //         perform ! ReadableStreamClose(stream).
        if (unwrappedController->closeRequested() &&
            unwrappedQueue->getDenseInitializedLength() == 0)
        {
            if (!ReadableStreamCloseInternal(cx, unwrappedStream)) {
                return nullptr;
            }
        }

        // Step c: Otherwise, perform ! ReadableStreamDefaultControllerCallPullIfNeeded(this).
        else {
            if (!ReadableStreamControllerCallPullIfNeeded(cx, unwrappedController)) {
                return nullptr;
            }
        }

        // Step d: Return a promise resolved with ! CreateIterResultObject(chunk, false).
        cx->check(chunk);
        RootedObject iterResultObj(cx, CreateIterResultObject(cx, chunk, false));
        if (!iterResultObj) {
            return nullptr;
        }
        RootedValue iterResult(cx, ObjectValue(*iterResultObj));
        return PromiseObject::unforgeableResolve(cx, iterResult);
    }

    // Step 3: Let pendingPromise be ! ReadableStreamAddReadRequest(stream).
    RootedObject pendingPromise(cx, ReadableStreamAddReadOrReadIntoRequest(cx, unwrappedStream));
    if (!pendingPromise) {
        return nullptr;
    }

    // Step 4: Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(this).
    if (!ReadableStreamControllerCallPullIfNeeded(cx, unwrappedController)) {
        return nullptr;
    }

    // Step 5: Return pendingPromise.
    return pendingPromise;
}


/*** 3.9. Readable stream default controller abstract operations *********************************/

// Streams spec, 3.9.1. IsReadableStreamDefaultController ( x )
// Implemented via is<ReadableStreamDefaultController>()

// Streams spec, 3.9.2 and 3.12.3. step 7:
// Upon fulfillment of pullPromise,
static bool
ControllerPullHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedValue controllerVal(cx, args.callee().as<JSFunction>().getExtendedSlot(0));
    Rooted<ReadableStreamController*> controller(cx);
    controller = ToUnwrapped<ReadableStreamController>(cx, controllerVal);
    if (!controller) {
        return false;
    }

    bool pullAgain = controller->pullAgain();

    // Step a: Set controller.[[pulling]] to false.
    // Step b.i: Set controller.[[pullAgain]] to false.
    controller->clearPullFlags();

    // Step b: If controller.[[pullAgain]] is true,
    if (pullAgain) {
        // Step ii: Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
        if (!ReadableStreamControllerCallPullIfNeeded(cx, controller)) {
            return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

// Streams spec, 3.9.2 and 3.12.3. step 8:
// Upon rejection of pullPromise with reason e,
static bool
ControllerPullFailedHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    HandleValue e = args.get(0);

    RootedValue controllerVal(cx, args.callee().as<JSFunction>().getExtendedSlot(0));
    Rooted<ReadableStreamController*> controller(cx);
    controller = ToUnwrapped<ReadableStreamController>(cx, controllerVal);
    if (!controller) {
        return false;
    }

    // Step a: If controller.[[controlledReadableStream]].[[state]] is "readable",
    //         perform ! ReadableByteStreamControllerError(controller, e).
    if (controller->stream()->readable()) {
        if (!ReadableStreamControllerError(cx, controller, e)) {
            return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

static bool
ReadableStreamControllerShouldCallPull(ReadableStreamController* controller);

static MOZ_MUST_USE double
ReadableStreamControllerGetDesiredSizeUnchecked(ReadableStreamController* controller);

/**
 * Streams spec, 3.9.2 ReadableStreamDefaultControllerCallPullIfNeeded ( controller )
 * Streams spec, 3.12.3. ReadableByteStreamControllerCallPullIfNeeded ( controller )
 *
 * Note: can operate on unwrapped instances from other compartments for
 * |controller|.
 */
inline static MOZ_MUST_USE bool
ReadableStreamControllerCallPullIfNeeded(JSContext* cx,
                                         Handle<ReadableStreamController*> controller)
{
    // Step 1: Let shouldPull be
    //         ! ReadableByteStreamControllerShouldCallPull(controller).
    bool shouldPull = ReadableStreamControllerShouldCallPull(controller);

    // Step 2: If shouldPull is false, return.
    if (!shouldPull) {
        return true;
    }

    // Step 3: If controller.[[pulling]] is true,
    if (controller->pulling()) {
        // Step a: Set controller.[[pullAgain]] to true.
        controller->setPullAgain();

        // Step b: Return.
        return true;
    }

    // Step 4: Assert: controller.[[pullAgain]] is false.
    MOZ_ASSERT(!controller->pullAgain());

    // Step 5: Set controller.[[pulling]] to true.
    controller->setPulling();

    // Step 6: Let pullPromise be
    //         ! PromiseInvokeOrNoop(controller.[[underlyingByteSource]], "pull", controller).
    RootedObject wrappedController(cx, controller);
    if (!cx->compartment()->wrap(cx, &wrappedController)) {
        return false;
    }
    RootedValue controllerVal(cx, ObjectValue(*wrappedController));
    RootedValue underlyingSource(cx, controller->underlyingSource());
    RootedObject pullPromise(cx);

    if (IsMaybeWrapped<TeeState>(underlyingSource)) {
        Rooted<TeeState*> teeState(cx);
        teeState = &UncheckedUnwrap(&underlyingSource.toObject())->as<TeeState>();
        Rooted<ReadableStream*> stream(cx, controller->stream());
        pullPromise = ReadableStreamTee_Pull(cx, teeState);
    } else if (controller->hasExternalSource()) {
        {
            AutoRealm ar(cx, controller);
            Rooted<ReadableStream*> stream(cx, controller->stream());
            void* source = underlyingSource.toPrivate();
            double desiredSize = ReadableStreamControllerGetDesiredSizeUnchecked(controller);
            cx->runtime()->readableStreamDataRequestCallback(cx,
                                                             stream,
                                                             source,
                                                             stream->embeddingFlags(),
                                                             desiredSize);
        }
        pullPromise = PromiseObject::unforgeableResolve(cx, UndefinedHandleValue);
    } else {
        pullPromise = PromiseInvokeOrNoop(cx, underlyingSource, cx->names().pull, controllerVal);
    }
    if (!pullPromise) {
        return false;
    }

    RootedObject onPullFulfilled(cx, NewHandler(cx, ControllerPullHandler, wrappedController));
    if (!onPullFulfilled) {
        return false;
    }

    RootedObject onPullRejected(cx, NewHandler(cx, ControllerPullFailedHandler, wrappedController));
    if (!onPullRejected) {
        return false;
    }

    return JS::AddPromiseReactions(cx, pullPromise, onPullFulfilled, onPullRejected);

    // Steps 7-8 implemented in functions above.
}

/**
 * Streams spec, 3.9.3. ReadableStreamDefaultControllerShouldCallPull ( controller )
 * Streams spec, 3.12.25. ReadableByteStreamControllerShouldCallPull ( controller )
 *
 * Note: can operate on unwrapped ReadableStream controller instances from
 * another compartment.
 */
static bool
ReadableStreamControllerShouldCallPull(ReadableStreamController* controller)
{
    // Step 1: Let stream be controller.[[controlledReadableStream]].
    ReadableStream* stream = controller->stream();

    // Step 2: If stream.[[state]] is "closed" or stream.[[state]] is "errored",
    //         return false.
    // or, equivalently
    // Step 2: If stream.[[state]] is not "readable", return false.
    if (!stream->readable()) {
        return false;
    }

    // Step 3: If controller.[[closeRequested]] is true, return false.
    if (controller->closeRequested()) {
        return false;
    }

    // Step 4: If controller.[[started]] is false, return false.
    if (!controller->started()) {
        return false;
    }

    // Step 5: If ! IsReadableStreamLocked(stream) is true and
    //         ! ReadableStreamGetNumReadRequests(stream) > 0, return true.
    // Steps 5-6 of 3.12.24 are equivalent in our implementation.
    if (stream->locked() && ReadableStreamGetNumReadRequests(stream) > 0) {
        return true;
    }

    // Step 6: Let desiredSize be ReadableStreamDefaultControllerGetDesiredSize(controller).
    double desiredSize = ReadableStreamControllerGetDesiredSizeUnchecked(controller);

    // Step 7: If desiredSize > 0, return true.
    // Step 8: Return false.
    // Steps 7-8 of 3.12.24 are equivalent in our implementation.
    return desiredSize > 0;
}

/**
 * Streams spec, 3.9.5. ReadableStreamDefaultControllerClose ( controller )
 *
 * Note: can operate on unwrapped ReadableStream controller instances from
 * another compartment.
 */
static MOZ_MUST_USE bool
ReadableStreamDefaultControllerClose(JSContext* cx,
                                     Handle<ReadableStreamDefaultController*> controller)
{
    // Step 1: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, controller->stream());

    // Step 2: Assert: controller.[[closeRequested]] is false.
    MOZ_ASSERT(!controller->closeRequested());

    // Step 3: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 4: Set controller.[[closeRequested]] to true.
    controller->setCloseRequested();

    // Step 5: If controller.[[queue]] is empty, perform ! ReadableStreamClose(stream).
    RootedNativeObject queue(cx, controller->queue());
    if (queue->getDenseInitializedLength() == 0) {
        return ReadableStreamCloseInternal(cx, stream);
    }

    return true;
}

static MOZ_MUST_USE bool
EnqueueValueWithSize(JSContext* cx, Handle<ReadableStreamController*> container, HandleValue value,
                     HandleValue sizeVal);

/**
 * Streams spec, 3.9.6. ReadableStreamDefaultControllerEnqueue ( controller, chunk )
 *
 * Note: can operate on unwrapped instances from other compartments for
 * |controller|. |chunk| must be in the current cx compartment.
 */
static MOZ_MUST_USE bool
ReadableStreamDefaultControllerEnqueue(JSContext* cx,
                                       Handle<ReadableStreamDefaultController*> controller,
                                       HandleValue chunk)
{
    AssertSameCompartment(cx, chunk);

    // Step 1: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, controller->stream());

    // Step 2: Assert: controller.[[closeRequested]] is false.
    MOZ_ASSERT(!controller->closeRequested());

    // Step 3: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 4: If ! IsReadableStreamLocked(stream) is true and
    //         ! ReadableStreamGetNumReadRequests(stream) > 0, perform
    //         ! ReadableStreamFulfillReadRequest(stream, chunk, false).
    if (stream->locked() && ReadableStreamGetNumReadRequests(stream) > 0) {
        if (!ReadableStreamFulfillReadOrReadIntoRequest(cx, stream, chunk, false)) {
            return false;
        }
    } else {
        // Step 5: Otherwise,
        // Step a: Let chunkSize be 1.
        RootedValue chunkSize(cx, NumberValue(1));
        bool success = true;

        // Step b: If controller.[[strategySize]] is not undefined,
        RootedValue strategySize(cx, controller->strategySize());
        if (!strategySize.isUndefined()) {
            // Step i: Set chunkSize to Call(stream.[[strategySize]], undefined, chunk).
            if (!cx->compartment()->wrap(cx, &strategySize)) {
                return false;
            }
            success = Call(cx, strategySize, UndefinedHandleValue, chunk, &chunkSize);
        }

        // Step c: Let enqueueResult be
        //         EnqueueValueWithSize(controller, chunk, chunkSize).
        if (success) {
            success = EnqueueValueWithSize(cx, controller, chunk, chunkSize);
        }

        if (!success) {
            // Step b.ii: If chunkSize is an abrupt completion,
            // and
            // Step d: If enqueueResult is an abrupt completion,
            RootedValue exn(cx);
            if (!cx->isExceptionPending() || !GetAndClearException(cx, &exn)) {
                // Uncatchable error. Die immediately without erroring the
                // stream.
                return false;
            }

            // Step b.ii.1: Perform
            //         ! ReadableStreamDefaultControllerErrorIfNeeded(controller,
            //                                                        chunkSize.[[Value]]).
            if (!ReadableStreamDefaultControllerErrorIfNeeded(cx, controller, exn)) {
                return false;
            }

            // Step b.ii.2: Return chunkSize.
            cx->setPendingException(exn);
            return false;
        }
    }

    // Step 6: Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(controller).
    // Step 7: Return.
    return ReadableStreamControllerCallPullIfNeeded(cx, controller);
}

static MOZ_MUST_USE bool
ReadableByteStreamControllerClearPendingPullIntos(JSContext* cx,
                                                  Handle<ReadableByteStreamController*> controller);

/**
 * Streams spec, 3.9.7. ReadableStreamDefaultControllerError ( controller, e )
 * Streams spec, 3.12.11. ReadableByteStreamControllerError ( controller, e )
 *
 * Note: can operate on unwrapped ReadableStream controller instances from
 * another compartment.
 */
static MOZ_MUST_USE bool
ReadableStreamControllerError(JSContext* cx, Handle<ReadableStreamController*> controller,
                              HandleValue e)
{
    MOZ_ASSERT(!cx->isExceptionPending());
    AssertSameCompartment(cx, e);

    // Step 1: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, controller->stream());

    // Step 2: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 3 of 3.12.10:
    // Perform ! ReadableByteStreamControllerClearPendingPullIntos(controller).
    if (controller->is<ReadableByteStreamController>()) {
        Rooted<ReadableByteStreamController*> byteStreamController(cx);
        byteStreamController = &controller->as<ReadableByteStreamController>();
        if (!ReadableByteStreamControllerClearPendingPullIntos(cx, byteStreamController)) {
            return false;
        }
    }

    // Step 3 (or 4): Perform ! ResetQueue(controller).
    if (!ResetQueue(cx, controller)) {
        return false;
    }

    // Step 4 (or 5): Perform ! ReadableStreamError(stream, e).
    return ReadableStreamErrorInternal(cx, stream, e);
}

/**
 * Streams spec, 3.9.7. ReadableStreamDefaultControllerErrorIfNeeded ( controller, e ) nothrow
 *
 * Note: can operate on unwrapped ReadableStreamDefaultController instances from
 * another compartment.
 */
static MOZ_MUST_USE bool
ReadableStreamDefaultControllerErrorIfNeeded(JSContext* cx,
                                             Handle<ReadableStreamDefaultController*> controller,
                                             HandleValue e)
{
    MOZ_ASSERT(!cx->isExceptionPending());

    // Step 1: If controller.[[controlledReadableStream]].[[state]] is "readable",
    //         perform ! ReadableStreamDefaultControllerError(controller, e).
    Rooted<ReadableStream*> stream(cx, controller->stream());
    if (stream->readable()) {
        return ReadableStreamControllerError(cx, controller, e);
    }
    return true;
}

/**
 * Streams spec, 3.9.8. ReadableStreamDefaultControllerGetDesiredSize ( controller )
 * Streams spec 3.12.14. ReadableByteStreamControllerGetDesiredSize ( controller )
 */
static MOZ_MUST_USE double
ReadableStreamControllerGetDesiredSizeUnchecked(ReadableStreamController* controller)
{
    // Steps 1-4 done at callsites, so only assert that they have been done.
#if DEBUG
    ReadableStream* stream = controller->stream();
    MOZ_ASSERT(!(stream->errored() || stream->closed()));
#endif // DEBUG

    // Step 5: Return controller.[[strategyHWM]] − controller.[[queueTotalSize]].
    return controller->strategyHWM() - controller->queueTotalSize();
}


/*** 3.10. Class ReadableByteStreamController ****************************************************/

/**
 * Streams spec, 3.10.3 new ReadableByteStreamController ( stream, underlyingSource,
 *                                                         highWaterMark )
 * Steps 3 - 16.
 *
 * Note: can NOT operate on unwrapped ReadableStream instances from
 * another compartment: ReadableStream controllers must be created in the same
 * compartment as the stream.
 */
static MOZ_MUST_USE ReadableByteStreamController*
CreateReadableByteStreamController(JSContext* cx, Handle<ReadableStream*> stream,
                                   HandleValue underlyingByteSource,
                                   HandleValue highWaterMarkVal)
{
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = NewBuiltinClassInstance<ReadableByteStreamController>(cx);
    if (!controller) {
        return nullptr;
    }

    // Step 3: Set this.[[controlledReadableStream]] to stream.
    controller->setStream(stream);

    // Step 4: Set this.[[underlyingByteSource]] to underlyingByteSource.
    controller->setUnderlyingSource(underlyingByteSource);

    // Step 5: Set this.[[pullAgain]], and this.[[pulling]] to false.
    controller->setFlags(0);

    // Step 6: Perform ! ReadableByteStreamControllerClearPendingPullIntos(this).
    if (!ReadableByteStreamControllerClearPendingPullIntos(cx, controller)) {
        return nullptr;
    }

    // Step 7: Perform ! ResetQueue(this).
    if (!ResetQueue(cx, controller)) {
        return nullptr;
    }

    // Step 8: Set this.[[started]] and this.[[closeRequested]] to false.
    // These should be false by default, unchanged since step 5.
    MOZ_ASSERT(controller->flags() == 0);

    // Step 9: Set this.[[strategyHWM]] to
    //         ? ValidateAndNormalizeHighWaterMark(highWaterMark).
    double highWaterMark;
    if (!ValidateAndNormalizeHighWaterMark(cx, highWaterMarkVal, &highWaterMark)) {
        return nullptr;
    }
    controller->setStrategyHWM(highWaterMark);

    // Step 10: Let autoAllocateChunkSize be
    //          ? GetV(underlyingByteSource, "autoAllocateChunkSize").
    RootedValue autoAllocateChunkSize(cx);
    if (!GetProperty(cx, underlyingByteSource, cx->names().autoAllocateChunkSize,
                     &autoAllocateChunkSize))
    {
        return nullptr;
    }

    // Step 11: If autoAllocateChunkSize is not undefined,
    if (!autoAllocateChunkSize.isUndefined()) {
        // Step a: If ! IsInteger(autoAllocateChunkSize) is false, or if
        //         autoAllocateChunkSize ≤ 0, throw a RangeError exception.
        if (!IsInteger(autoAllocateChunkSize) || autoAllocateChunkSize.toNumber() <= 0) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLEBYTESTREAMCONTROLLER_BAD_CHUNKSIZE);
            return nullptr;
    }
    }

    // Step 12: Set this.[[autoAllocateChunkSize]] to autoAllocateChunkSize.
    controller->setAutoAllocateChunkSize(autoAllocateChunkSize);

    // Step 13: Set this.[[pendingPullIntos]] to a new empty List.
    if (!SetNewList(cx, controller, ReadableByteStreamController::Slot_PendingPullIntos)) {
        return nullptr;
    }

    // Step 14: Let controller be this (implicit).

    // Step 15: Let startResult be
    //          ? InvokeOrNoop(underlyingSource, "start", « this »).
    RootedValue startResult(cx);
    RootedValue controllerVal(cx, ObjectValue(*controller));
    if (!InvokeOrNoop(cx, underlyingByteSource, cx->names().start, controllerVal, &startResult)) {
        return nullptr;
    }

    // Step 16: Let startPromise be a promise resolved with startResult:
    RootedObject startPromise(cx, PromiseObject::unforgeableResolve(cx, startResult));
    if (!startPromise) {
        return nullptr;
    }

    RootedObject onStartFulfilled(cx, NewHandler(cx, ControllerStartHandler, controller));
    if (!onStartFulfilled) {
        return nullptr;
    }

    RootedObject onStartRejected(cx, NewHandler(cx, ControllerStartFailedHandler, controller));
    if (!onStartRejected) {
        return nullptr;
    }

    if (!JS::AddPromiseReactions(cx, startPromise, onStartFulfilled, onStartRejected)) {
        return nullptr;
    }

    return controller;
}

// Streams spec, 3.10.3.
// new ReadableByteStreamController ( stream, underlyingByteSource,
//                                    highWaterMark )
bool
ReadableByteStreamController::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "ReadableByteStreamController")) {
        return false;
    }

    // Step 1: If ! IsReadableStream(stream) is false, throw a TypeError exception.
    HandleValue streamVal = args.get(0);
    if (!Is<ReadableStream>(streamVal)) {
        ReportArgTypeError(cx, "ReadableStreamDefaultController", "ReadableStream",
                           args.get(0));
        return false;
    }

    Rooted<ReadableStream*> stream(cx, &streamVal.toObject().as<ReadableStream>());

    // Step 2: If stream.[[readableStreamController]] is not undefined, throw a
    //         TypeError exception.
    if (stream->hasController()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_CONTROLLER_SET);
        return false;
    }

    RootedObject controller(cx, CreateReadableByteStreamController(cx, stream, args.get(1),
                                                                   args.get(2)));
    if (!controller) {
        return false;
    }

    args.rval().setObject(*controller);
    return true;
}

// Version of the ReadableByteStreamConstructor that's specialized for
// handling external, embedding-provided, underlying sources.
static MOZ_MUST_USE ReadableByteStreamController*
CreateReadableByteStreamController(JSContext* cx, Handle<ReadableStream*> stream,
                                   void* underlyingSource)
{
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = NewBuiltinClassInstance<ReadableByteStreamController>(cx);
    if (!controller) {
        return nullptr;
    }

    // Step 3: Set this.[[controlledReadableStream]] to stream.
    controller->setStream(stream);

    // Step 4: Set this.[[underlyingByteSource]] to underlyingByteSource.
    controller->setUnderlyingSource(PrivateValue(underlyingSource));

    // Step 5: Set this.[[pullAgain]], and this.[[pulling]] to false.
    controller->setFlags(ReadableStreamController::Flag_ExternalSource);

    // Step 6: Perform ! ReadableByteStreamControllerClearPendingPullIntos(this).
    // Omitted.

    // Step 7: Perform ! ResetQueue(this).
    controller->setQueueTotalSize(0);

    // Step 8: Set this.[[started]] and this.[[closeRequested]] to false.
    // Step 9: Set this.[[strategyHWM]] to
    //         ? ValidateAndNormalizeHighWaterMark(highWaterMark).
    controller->setStrategyHWM(0);

    // Step 10: Let autoAllocateChunkSize be
    //          ? GetV(underlyingByteSource, "autoAllocateChunkSize").
    // Step 11: If autoAllocateChunkSize is not undefined,
    // Step 12: Set this.[[autoAllocateChunkSize]] to autoAllocateChunkSize.
    // Omitted.

    // Step 13: Set this.[[pendingPullIntos]] to a new empty List.
    if (!SetNewList(cx, controller, ReadableByteStreamController::Slot_PendingPullIntos)) {
        return nullptr;
    }

    // Step 14: Let controller be this (implicit).
    // Step 15: Let startResult be
    //          ? InvokeOrNoop(underlyingSource, "start", « this »).
    // Omitted.

    // Step 16: Let startPromise be a promise resolved with startResult:
    RootedObject startPromise(cx, PromiseObject::unforgeableResolve(cx, UndefinedHandleValue));
    if (!startPromise) {
        return nullptr;
    }

    RootedObject onStartFulfilled(cx, NewHandler(cx, ControllerStartHandler, controller));
    if (!onStartFulfilled) {
        return nullptr;
    }

    RootedObject onStartRejected(cx, NewHandler(cx, ControllerStartFailedHandler, controller));
    if (!onStartRejected) {
        return nullptr;
    }

    if (!JS::AddPromiseReactions(cx, startPromise, onStartFulfilled, onStartRejected)) {
        return nullptr;
    }

    return controller;
}

static const JSPropertySpec ReadableByteStreamController_properties[] = {
    JS_PS_END
};

static const JSFunctionSpec ReadableByteStreamController_methods[] = {
    JS_FS_END
};

static void
ReadableByteStreamControllerFinalize(FreeOp* fop, JSObject* obj)
{
    ReadableByteStreamController& controller = obj->as<ReadableByteStreamController>();

    if (controller.getFixedSlot(ReadableStreamController::Slot_Flags).isUndefined()) {
        return;
    }

    if (!controller.hasExternalSource()) {
        return;
    }

    uint8_t embeddingFlags = controller.flags() >> ReadableStreamController::EmbeddingFlagsOffset;

    void* underlyingSource = controller.underlyingSource().toPrivate();
    obj->runtimeFromAnyThread()->readableStreamFinalizeCallback(underlyingSource, embeddingFlags);
}

static const ClassOps ReadableByteStreamControllerClassOps = {
    nullptr,        /* addProperty */
    nullptr,        /* delProperty */
    nullptr,        /* enumerate */
    nullptr,        /* newEnumerate */
    nullptr,        /* resolve */
    nullptr,        /* mayResolve */
    ReadableByteStreamControllerFinalize,
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    nullptr,        /* trace   */
};

CLASS_SPEC(ReadableByteStreamController, 3, SlotCount, ClassSpec::DontDefineConstructor,
           JSCLASS_BACKGROUND_FINALIZE, &ReadableByteStreamControllerClassOps);

// Streams spec, 3.10.5.1. [[CancelSteps]] ()
// Unified with 3.8.5.1 above.

static MOZ_MUST_USE bool
ReadableByteStreamControllerHandleQueueDrain(JSContext* cx,
                                             Handle<ReadableStreamController*> controller);

/**
 * Streams spec, 3.10.5.2. [[PullSteps]] ()
 *
 * Note: can operate on unwrapped instances from other compartments for
 * |controller|. Any instances created in the course of this
 * function's operation are created in the current cx compartment.
 */
static MOZ_MUST_USE JSObject*
ReadableByteStreamControllerPullSteps(JSContext* cx,
                                      Handle<ReadableByteStreamController*> controller)
{
    // Step 1: Let stream be this.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, controller->stream());

    // Step 2: Assert: ! ReadableStreamHasDefaultReader(stream) is true.
#ifdef DEBUG
    bool result;
    if (!ReadableStreamHasDefaultReader(cx, stream, &result)) {
        return nullptr;
    }
    MOZ_ASSERT(result);
#endif

    RootedValue val(cx);
    // Step 3: If this.[[queueTotalSize]] > 0,
    double queueTotalSize = controller->queueTotalSize();
    if (queueTotalSize > 0) {
        // Step 3.a: Assert: ! ReadableStreamGetNumReadRequests(_stream_) is 0.
        MOZ_ASSERT(ReadableStreamGetNumReadRequests(stream) == 0);

        RootedObject view(cx);

        if (stream->mode() == JS::ReadableStreamMode::ExternalSource) {
            void* underlyingSource = controller->underlyingSource().toPrivate();

            view = JS_NewUint8Array(cx, queueTotalSize);
            if (!view) {
                return nullptr;
            }

            size_t bytesWritten;
            {
                AutoRealm ar(cx, stream);
                JS::AutoSuppressGCAnalysis suppressGC(cx);
                JS::AutoCheckCannotGC noGC;
                bool dummy;
                void* buffer = JS_GetArrayBufferViewData(view, &dummy, noGC);

                auto cb = cx->runtime()->readableStreamWriteIntoReadRequestCallback;
                MOZ_ASSERT(cb);
                // TODO: use bytesWritten to correctly update the request's state.
                cb(cx, stream, underlyingSource, stream->embeddingFlags(), buffer,
                   queueTotalSize, &bytesWritten);
            }

            queueTotalSize = queueTotalSize - bytesWritten;
        } else {
            // Step 3.b: Let entry be the first element of this.[[queue]].
            // Step 3.c: Remove entry from this.[[queue]], shifting all other elements
            //           downward (so that the second becomes the first, and so on).
            RootedNativeObject queue(cx, controller->queue());
            Rooted<ByteStreamChunk*> entry(cx);
            entry = ToUnwrapped<ByteStreamChunk>(cx, ShiftFromList<JSObject>(cx, queue));
            if (!entry) {
                return nullptr;
            }

            queueTotalSize = queueTotalSize - entry->byteLength();

            // Step 3.f: Let view be ! Construct(%Uint8Array%, « entry.[[buffer]],
            //                                   entry.[[byteOffset]], entry.[[byteLength]] »).
            // (reordered)
            RootedObject buffer(cx, entry->buffer());
            if (!cx->compartment()->wrap(cx, &buffer)) {
                return nullptr;
            }

            uint32_t byteOffset = entry->byteOffset();
            view = JS_NewUint8ArrayWithBuffer(cx, buffer, byteOffset, entry->byteLength());
            if (!view) {
                return nullptr;
            }
        }

        // Step 3.d: Set this.[[queueTotalSize]] to
        //           this.[[queueTotalSize]] − entry.[[byteLength]].
        // (reordered)
        controller->setQueueTotalSize(queueTotalSize);

        // Step 3.e: Perform ! ReadableByteStreamControllerHandleQueueDrain(this).
        // (reordered)
        if (!ReadableByteStreamControllerHandleQueueDrain(cx, controller)) {
            return nullptr;
        }

        // Step 3.g: Return a promise resolved with ! CreateIterResultObject(view, false).
        val.setObject(*view);
        RootedObject iterResult(cx, CreateIterResultObject(cx, val, false));
        if (!iterResult) {
            return nullptr;
        }
        val.setObject(*iterResult);

        return PromiseObject::unforgeableResolve(cx, val);
    }

    // Step 4: Let autoAllocateChunkSize be this.[[autoAllocateChunkSize]].
    val = controller->autoAllocateChunkSize();

    // Step 5: If autoAllocateChunkSize is not undefined,
    if (!val.isUndefined()) {
        double autoAllocateChunkSize = val.toNumber();

        // Step 5.a: Let buffer be Construct(%ArrayBuffer%, « autoAllocateChunkSize »).
        RootedObject bufferObj(cx, JS_NewArrayBuffer(cx, autoAllocateChunkSize));

        // Step 5.b: If buffer is an abrupt completion,
        //           return a promise rejected with buffer.[[Value]].
        if (!bufferObj) {
            return PromiseRejectedWithPendingError(cx);
        }

        RootedArrayBufferObject buffer(cx, &bufferObj->as<ArrayBufferObject>());

        // Step 5.c: Let pullIntoDescriptor be Record {[[buffer]]: buffer.[[Value]],
        //                                             [[byteOffset]]: 0,
        //                                             [[byteLength]]: autoAllocateChunkSize,
        //                                             [[bytesFilled]]: 0, [[elementSize]]: 1,
        //                                             [[ctor]]: %Uint8Array%,
        //                                             [[readerType]]: `"default"`}.
        RootedObject pullIntoDescriptor(cx);
        pullIntoDescriptor = PullIntoDescriptor::create(cx, buffer, 0,
                                                        autoAllocateChunkSize, 0, 1,
                                                        nullptr,
                                                        ReaderType_Default);
        if (!pullIntoDescriptor) {
            return PromiseRejectedWithPendingError(cx);
        }

        // Step 5.d: Append pullIntoDescriptor as the last element of this.[[pendingPullIntos]].
        if (!AppendToListAtSlot(cx,
                                controller,
                                ReadableByteStreamController::Slot_PendingPullIntos,
                                pullIntoDescriptor))
        {
            return nullptr;
        }
    }

    // Step 6: Let promise be ! ReadableStreamAddReadRequest(stream).
    RootedObject promise(cx, ReadableStreamAddReadOrReadIntoRequest(cx, stream));
    if (!promise) {
        return nullptr;
    }

    // Step 7: Perform ! ReadableByteStreamControllerCallPullIfNeeded(this).
    if (!ReadableStreamControllerCallPullIfNeeded(cx, controller)) {
        return nullptr;
    }

    // Step 8: Return promise.
    return promise;
}

/**
 * Unified implementation of ReadableStream controllers' [[PullSteps]] internal
 * methods.
 * Streams spec, 3.8.5.2. [[PullSteps]] ()
 * and
 * Streams spec, 3.10.5.2. [[PullSteps]] ()
 *
 * Note: can operate on unwrapped ReadableStream controller instances from
 * another compartment.
 */
static MOZ_MUST_USE JSObject*
ReadableStreamControllerPullSteps(JSContext* cx, Handle<ReadableStreamController*> controller)
{
    if (controller->is<ReadableStreamDefaultController>()) {
        Rooted<ReadableStreamDefaultController*> defaultController(cx,
            &controller->as<ReadableStreamDefaultController>());
        return ReadableStreamDefaultControllerPullSteps(cx, defaultController);
    }

    Rooted<ReadableByteStreamController*> byteController(cx,
        &controller->as<ReadableByteStreamController>());
    return ReadableByteStreamControllerPullSteps(cx, byteController);
}


/*** 3.12. Readable stream BYOB controller abstract operations ***********************************/

// Streams spec, 3.12.1. IsReadableStreamBYOBRequest ( x )
// Implemented via is<ReadableStreamBYOBRequest>()

// Streams spec, 3.12.2. IsReadableByteStreamController ( x )
// Implemented via is<ReadableByteStreamController>()

// Streams spec, 3.12.3. ReadableByteStreamControllerCallPullIfNeeded ( controller )
// Unified with 3.9.2 above.

static MOZ_MUST_USE bool
ReadableByteStreamControllerInvalidateBYOBRequest(JSContext* cx,
                                                  Handle<ReadableByteStreamController*> controller);

/**
 * Streams spec, 3.12.5. ReadableByteStreamControllerClearPendingPullIntos ( controller )
 *
 * Note: can operate on unwrapped instances from other compartments for
 * |controller|. The List created in step 2 is guaranteed to be in the same
 * compartment as the controller.
 */
static MOZ_MUST_USE bool
ReadableByteStreamControllerClearPendingPullIntos(JSContext* cx,
                                                  Handle<ReadableByteStreamController*> controller)
{
    // Step 1: Perform ! ReadableByteStreamControllerInvalidateBYOBRequest(controller).
    if (!ReadableByteStreamControllerInvalidateBYOBRequest(cx, controller)) {
        return false;
    }

    // Step 2: Set controller.[[pendingPullIntos]] to a new empty List.
    return SetNewList(cx, controller, ReadableByteStreamController::Slot_PendingPullIntos);
}

/**
 * Streams spec, 3.12.6. ReadableByteStreamControllerClose ( controller )
 *
 * Note: can operate on unwrapped ReadableByteStreamController instances from
 * another compartment.
 */
static MOZ_MUST_USE bool
ReadableByteStreamControllerClose(JSContext* cx, Handle<ReadableByteStreamController*> controller)
{
    // Step 1: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, controller->stream());

    // Step 2: Assert: controller.[[closeRequested]] is false.
    MOZ_ASSERT(!controller->closeRequested());

    // Step 3: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 4: If controller.[[queueTotalSize]] > 0,
    if (controller->queueTotalSize() > 0) {
        // Step a: Set controller.[[closeRequested]] to true.
        controller->setCloseRequested();

        // Step b: Return
        return true;
    }

    // Step 5: If controller.[[pendingPullIntos]] is not empty,
    RootedNativeObject pendingPullIntos(cx, controller->pendingPullIntos());
    if (pendingPullIntos->getDenseInitializedLength() != 0) {
        // Step a: Let firstPendingPullInto be the first element of
        //         controller.[[pendingPullIntos]].
        Rooted<PullIntoDescriptor*> firstPendingPullInto(cx);
        firstPendingPullInto = ToUnwrapped<PullIntoDescriptor>(cx,
                                                               PeekList<JSObject>(pendingPullIntos));
        if (!firstPendingPullInto) {
            return false;
        }

        // Step b: If firstPendingPullInto.[[bytesFilled]] > 0,
        if (firstPendingPullInto->bytesFilled() > 0) {
            // Step i: Let e be a new TypeError exception.
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLEBYTESTREAMCONTROLLER_CLOSE_PENDING_PULL);
            RootedValue e(cx);
            if (!cx->isExceptionPending() || !GetAndClearException(cx, &e)) {
                // Uncatchable error. Die immediately without erroring the
                // stream.
                return false;
            }

            // Step ii: Perform ! ReadableByteStreamControllerError(controller, e).
            if (!ReadableStreamControllerError(cx, controller, e)) {
                return false;
            }

            // Step iii: Throw e.
            cx->setPendingException(e);
            return false;
        }
    }

    // Step 6: Perform ! ReadableStreamClose(stream).
    return ReadableStreamCloseInternal(cx, stream);
}

// Streams spec, 3.12.11. ReadableByteStreamControllerError ( controller, e )
// Unified with 3.9.7 above.

// Streams spec 3.12.14. ReadableByteStreamControllerGetDesiredSize ( controller )
// Unified with 3.9.8 above.

/**
 * Streams spec, 3.12.15. ReadableByteStreamControllerHandleQueueDrain ( controller )
 *
 * Note: can operate on unwrapped instances from other compartments for
 * |controller|.
 */
static MOZ_MUST_USE bool
ReadableByteStreamControllerHandleQueueDrain(JSContext* cx,
                                             Handle<ReadableStreamController*> controller)
{
    MOZ_ASSERT(controller->is<ReadableByteStreamController>());

    // Step 1: Assert: controller.[[controlledReadableStream]].[[state]] is "readable".
    Rooted<ReadableStream*> stream(cx, controller->stream());
    MOZ_ASSERT(stream->readable());

    // Step 2: If controller.[[queueTotalSize]] is 0 and
    //         controller.[[closeRequested]] is true,
    if (controller->queueTotalSize() == 0 && controller->closeRequested()) {
      // Step a: Perform ! ReadableStreamClose(controller.[[controlledReadableStream]]).
      return ReadableStreamCloseInternal(cx, stream);
    }

    // Step 3: Otherwise,
    // Step a: Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
    return ReadableStreamControllerCallPullIfNeeded(cx, controller);
}

enum BYOBRequestSlots {
    BYOBRequestSlot_Controller,
    BYOBRequestSlot_View,
    BYOBRequestSlotCount
};

/**
 * Streams spec 3.12.16. ReadableByteStreamControllerInvalidateBYOBRequest ( controller )
 *
 * Note: can operate on unwrapped instances from other compartments for
 * |controller|.
 */
static MOZ_MUST_USE bool
ReadableByteStreamControllerInvalidateBYOBRequest(JSContext* cx,
                                                  Handle<ReadableByteStreamController*> controller)
{
    // Step 1: If controller.[[byobRequest]] is undefined, return.
    RootedValue byobRequestVal(cx, controller->byobRequest());
    if (byobRequestVal.isUndefined()) {
        return true;
    }

    RootedNativeObject byobRequest(cx, ToUnwrapped<NativeObject>(cx, byobRequestVal));
    if (!byobRequest) {
        return false;
    }

    // Step 2: Set controller.[[byobRequest]].[[associatedReadableByteStreamController]]
    //         to undefined.
    byobRequest->setFixedSlot(BYOBRequestSlot_Controller, UndefinedValue());

    // Step 3: Set controller.[[byobRequest]].[[view]] to undefined.
    byobRequest->setFixedSlot(BYOBRequestSlot_View, UndefinedValue());

    // Step 4: Set controller.[[byobRequest]] to undefined.
    controller->clearBYOBRequest();

    return true;
}

// Streams spec, 3.12.25. ReadableByteStreamControllerShouldCallPull ( controller )
// Unified with 3.9.3 above.


/*** 6.1. Queuing strategies *********************************************************************/

// Streams spec, 6.1.2.2. new ByteLengthQueuingStrategy({ highWaterMark })
bool
js::ByteLengthQueuingStrategy::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject strategy(cx, NewBuiltinClassInstance<ByteLengthQueuingStrategy>(cx));
    if (!strategy) {
        return false;
    }

    RootedObject argObj(cx, ToObject(cx, args.get(0)));
    if (!argObj) {
      return false;
    }

    RootedValue highWaterMark(cx);
    if (!GetProperty(cx, argObj, argObj, cx->names().highWaterMark, &highWaterMark)) {
      return false;
    }

    if (!SetProperty(cx, strategy, cx->names().highWaterMark, highWaterMark)) {
      return false;
    }

    args.rval().setObject(*strategy);
    return true;
}

// Streams spec 6.1.2.3.1. size ( chunk )
bool
ByteLengthQueuingStrategy_size(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: Return ? GetV(chunk, "byteLength").
    return GetProperty(cx, args.get(0), cx->names().byteLength, args.rval());
}

static const JSPropertySpec ByteLengthQueuingStrategy_properties[] = {
    JS_PS_END
};

static const JSFunctionSpec ByteLengthQueuingStrategy_methods[] = {
    JS_FN("size", ByteLengthQueuingStrategy_size, 1, 0),
    JS_FS_END
};

CLASS_SPEC(ByteLengthQueuingStrategy, 1, 0, 0, 0, JS_NULL_CLASS_OPS);

// Streams spec, 6.1.3.2. new CountQueuingStrategy({ highWaterMark })
bool
js::CountQueuingStrategy::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    Rooted<CountQueuingStrategy*> strategy(cx, NewBuiltinClassInstance<CountQueuingStrategy>(cx));
    if (!strategy) {
        return false;
    }

    RootedObject argObj(cx, ToObject(cx, args.get(0)));
    if (!argObj) {
      return false;
    }

    RootedValue highWaterMark(cx);
    if (!GetProperty(cx, argObj, argObj, cx->names().highWaterMark, &highWaterMark)) {
      return false;
    }

    if (!SetProperty(cx, strategy, cx->names().highWaterMark, highWaterMark)) {
      return false;
    }

    args.rval().setObject(*strategy);
    return true;
}

// Streams spec 6.2.3.3.1. size ( chunk )
bool
CountQueuingStrategy_size(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: Return 1.
    args.rval().setInt32(1);
    return true;
}

static const JSPropertySpec CountQueuingStrategy_properties[] = {
    JS_PS_END
};

static const JSFunctionSpec CountQueuingStrategy_methods[] = {
    JS_FN("size", CountQueuingStrategy_size, 0, 0),
    JS_FS_END
};

CLASS_SPEC(CountQueuingStrategy, 1, 0, 0, 0, JS_NULL_CLASS_OPS);

#undef CLASS_SPEC


/*** 6.2. Queue-with-sizes operations ************************************************************/

/**
 * Streams spec, 6.2.1. DequeueValue ( container ) nothrow
 *
 * Note: can operate on unwrapped queue container instances from another
 * compartment. In that case, the returned chunk will be wrapped into the
 * current compartment.
 */
inline static MOZ_MUST_USE bool
DequeueValue(JSContext* cx, Handle<ReadableStreamController*> container, MutableHandleValue chunk)
{
    // Step 1: Assert: container has [[queue]] and [[queueTotalSize]] internal
    //         slots (implicit).
    // Step 2: Assert: queue is not empty.
    RootedNativeObject queue(cx, container->queue());
    MOZ_ASSERT(queue->getDenseInitializedLength() > 0);

    // Step 3. Let pair be the first element of queue.
    // Step 4. Remove pair from queue, shifting all other elements downward
    //         (so that the second becomes the first, and so on).
    Rooted<QueueEntry*> pair(cx, ShiftFromList<QueueEntry>(cx, queue));
    MOZ_ASSERT(pair);

    // Step 5: Set container.[[queueTotalSize]] to
    //         container.[[queueTotalSize]] − pair.[[size]].
    // Step 6: If container.[[queueTotalSize]] < 0, set
    //         container.[[queueTotalSize]] to 0.
    //         (This can occur due to rounding errors.)
    double totalSize = container->queueTotalSize();

    totalSize -= pair->size();
    if (totalSize < 0) {
        totalSize = 0;
    }
    container->setQueueTotalSize(totalSize);

    RootedValue val(cx, pair->value());
    if (container->compartment() != cx->compartment() && !cx->compartment()->wrap(cx, &val)) {
        return false;
    }

    // Step 7: Return pair.[[value]].
    chunk.set(val);
    return true;
}

/**
 * Streams spec, 6.2.2. EnqueueValueWithSize ( container, value, size ) throws
 *
 * Note: can operate on unwrapped queue container instances from another
 * compartment than the current one. In that case, the given value will be
 * wrapped into the container compartment.
 */
static MOZ_MUST_USE bool
EnqueueValueWithSize(JSContext* cx, Handle<ReadableStreamController*> container, HandleValue value,
                     HandleValue sizeVal)
{
    // Step 1: Assert: container has [[queue]] and [[queueTotalSize]] internal
    //         slots (implicit).
    // Step 2: Let size be ? ToNumber(size).
    double size;
    if (!ToNumber(cx, sizeVal, &size)) {
        return false;
    }

    // Step 3: If ! IsFiniteNonNegativeNumber(size) is false, throw a RangeError
    //         exception.
    if (size < 0 || mozilla::IsNaN(size) || mozilla::IsInfinite(size)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_NUMBER_MUST_BE_FINITE_NON_NEGATIVE, "size");
        return false;
    }

    // Step 4: Append Record {[[value]]: value, [[size]]: size} as the last element
    //         of container.[[queue]].
    RootedNativeObject queue(cx, container->queue());

    RootedValue wrappedVal(cx, value);
    {
        AutoRealm ar(cx, container);
        if (!cx->compartment()->wrap(cx, &wrappedVal)) {
            return false;
        }

        QueueEntry* entry = QueueEntry::create(cx, wrappedVal, size);
        if (!entry) {
            return false;
        }
        RootedValue val(cx, ObjectValue(*entry));
        if (!AppendToList(cx, queue, val)) {
            return false;
        }
    }

    // Step 5: Set container.[[queueTotalSize]] to
    //         container.[[queueTotalSize]] + size.
    container->setQueueTotalSize(container->queueTotalSize() + size);

    return true;
}

/**
 * Streams spec, 6.2.4. ResetQueue ( container ) nothrow
 *
 * Note: can operate on unwrapped container instances from another
 * compartment.
 */
inline static MOZ_MUST_USE bool
ResetQueue(JSContext* cx, Handle<ReadableStreamController*> unwrappedContainer)
{
    // Step 1: Assert: container has [[queue]] and [[queueTotalSize]] internal
    //         slots (implicit).
    // Step 2: Set container.[[queue]] to a new empty List.
    if (!SetNewList(cx, unwrappedContainer, StreamController::Slot_Queue)) {
        return false;
    }

    // Step 3: Set container.[[queueTotalSize]] to 0.
    unwrappedContainer->setQueueTotalSize(0);

    return true;
}


/*** 6.3. Miscellaneous operations ***************************************************************/

/**
 * Appends the given |obj| to the given list |container|'s list.
 *
 * Note: can operate on |container| and |obj| combinations from different
 * compartments, in which case |obj| is wrapped before storing it.
 */
inline static MOZ_MUST_USE bool
AppendToListAtSlot(JSContext* cx, HandleNativeObject container, uint32_t slot, HandleObject obj)
{
    RootedValue val(cx, container->getFixedSlot(slot));
    RootedNativeObject list(cx, &val.toObject().as<NativeObject>());

    val = ObjectValue(*obj);

    AutoRealm ar(cx, list);
    if (!cx->compartment()->wrap(cx, &val)) {
        return false;
    }
    return AppendToList(cx, list, val);
}


/**
 * Streams spec, 6.3.2. InvokeOrNoop ( O, P, args )
 */
inline static MOZ_MUST_USE bool
InvokeOrNoop(JSContext* cx, HandleValue O, HandlePropertyName P, HandleValue arg,
             MutableHandleValue rval)
{
    // Step 1: Assert: P is a valid property key (omitted).
    // Step 2: If args was not passed, let args be a new empty List (omitted).
    // Step 3: Let method be ? GetV(O, P).
    RootedValue method(cx);
    if (!GetProperty(cx, O, P, &method)) {
        return false;
    }

    // Step 4: If method is undefined, return.
    if (method.isUndefined()) {
        return true;
    }

    // Step 5: Return ? Call(method, O, args).
    return Call(cx, method, O, arg, rval);
}

/**
 * Streams spec, obsolete (previously 6.4.3) PromiseInvokeOrNoop ( O, P, args )
 * Specialized to one arg, because that's what all stream related callers use.
 */
static MOZ_MUST_USE JSObject*
PromiseInvokeOrNoop(JSContext* cx, HandleValue O, HandlePropertyName P, HandleValue arg)
{
    // Step 1: Assert: O is not undefined.
    MOZ_ASSERT(!O.isUndefined());

    // Step 2: Assert: ! IsPropertyKey(P) is true (implicit).
    // Step 3: Assert: args is a List (omitted).

    // Step 4: Let returnValue be InvokeOrNoop(O, P, args).
    // Step 5: If returnValue is an abrupt completion, return a promise
    //         rejected with returnValue.[[Value]].
    RootedValue returnValue(cx);
    if (!InvokeOrNoop(cx, O, P, arg, &returnValue)) {
        return PromiseRejectedWithPendingError(cx);
    }

    // Step 6: Otherwise, return a promise resolved with returnValue.[[Value]].
    return PromiseObject::unforgeableResolve(cx, returnValue);
}

// Streams spec, 6.3.7. ValidateAndNormalizeHighWaterMark ( highWaterMark )
static MOZ_MUST_USE bool
ValidateAndNormalizeHighWaterMark(JSContext* cx, HandleValue highWaterMarkVal, double* highWaterMark)
{
    // Step 1: Set highWaterMark to ? ToNumber(highWaterMark).
    if (!ToNumber(cx, highWaterMarkVal, highWaterMark)) {
        return false;
    }

    // Step 2: If highWaterMark is NaN, throw a TypeError exception.
    // Step 3: If highWaterMark < 0, throw a RangeError exception.
    if (mozilla::IsNaN(*highWaterMark) || *highWaterMark < 0) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_STREAM_INVALID_HIGHWATERMARK);
        return false;
    }

    // Step 4: Return highWaterMark.
    return true;
}

// Streams spec, obsolete (previously 6.4.6) ValidateAndNormalizeQueuingStrategy ( size, highWaterMark )
static MOZ_MUST_USE bool
ValidateAndNormalizeQueuingStrategy(JSContext* cx, HandleValue size,
                                    HandleValue highWaterMarkVal, double* highWaterMark)
{
    // Step 1: If size is not undefined and ! IsCallable(size) is false, throw a
    //         TypeError exception.
    if (!size.isUndefined() && !IsCallable(size)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_FUNCTION,
                                  "ReadableStream argument options.size");
        return false;
    }

    // Step 2: Let highWaterMark be ? ValidateAndNormalizeHighWaterMark(highWaterMark).
    if (!ValidateAndNormalizeHighWaterMark(cx, highWaterMarkVal, highWaterMark)) {
        return false;
    }

    // Step 3: Return Record {[[size]]: size, [[highWaterMark]]: highWaterMark}.
    return true;
}


/*** API entry points ****************************************************************************/

JS_FRIEND_API(JSObject*)
js::UnwrapReadableStream(JSObject* obj)
{
    if (JSObject* unwrapped = CheckedUnwrap(obj)) {
        return unwrapped->is<ReadableStream>() ? unwrapped : nullptr;
    }
    return nullptr;
}

extern JS_PUBLIC_API(void)
JS::SetReadableStreamCallbacks(JSContext* cx,
                               JS::RequestReadableStreamDataCallback dataRequestCallback,
                               JS::WriteIntoReadRequestBufferCallback writeIntoReadRequestCallback,
                               JS::CancelReadableStreamCallback cancelCallback,
                               JS::ReadableStreamClosedCallback closedCallback,
                               JS::ReadableStreamErroredCallback erroredCallback,
                               JS::ReadableStreamFinalizeCallback finalizeCallback)
{
    MOZ_ASSERT(dataRequestCallback);
    MOZ_ASSERT(writeIntoReadRequestCallback);
    MOZ_ASSERT(cancelCallback);
    MOZ_ASSERT(closedCallback);
    MOZ_ASSERT(erroredCallback);
    MOZ_ASSERT(finalizeCallback);

    JSRuntime* rt = cx->runtime();

    MOZ_ASSERT(!rt->readableStreamDataRequestCallback);
    MOZ_ASSERT(!rt->readableStreamWriteIntoReadRequestCallback);
    MOZ_ASSERT(!rt->readableStreamCancelCallback);
    MOZ_ASSERT(!rt->readableStreamClosedCallback);
    MOZ_ASSERT(!rt->readableStreamErroredCallback);
    MOZ_ASSERT(!rt->readableStreamFinalizeCallback);

    rt->readableStreamDataRequestCallback = dataRequestCallback;
    rt->readableStreamWriteIntoReadRequestCallback = writeIntoReadRequestCallback;
    rt->readableStreamCancelCallback = cancelCallback;
    rt->readableStreamClosedCallback = closedCallback;
    rt->readableStreamErroredCallback = erroredCallback;
    rt->readableStreamFinalizeCallback = finalizeCallback;
}

JS_PUBLIC_API(bool)
JS::HasReadableStreamCallbacks(JSContext* cx)
{
    return cx->runtime()->readableStreamDataRequestCallback;
}

JS_PUBLIC_API(JSObject*)
JS::NewReadableDefaultStreamObject(JSContext* cx,
                                   JS::HandleObject underlyingSource /* = nullptr */,
                                   JS::HandleFunction size /* = nullptr */,
                                   double highWaterMark /* = 1 */,
                                   JS::HandleObject proto /* = nullptr */)
{
    MOZ_ASSERT(!cx->zone()->isAtomsZone());
    AssertHeapIsIdle();
    CHECK_THREAD(cx);

    RootedObject source(cx, underlyingSource);
    if (!source) {
        source = NewBuiltinClassInstance<PlainObject>(cx);
        if (!source) {
            return nullptr;
        }
    }
    RootedValue sourceVal(cx, ObjectValue(*source));
    RootedValue sizeVal(cx, size ? ObjectValue(*size) : UndefinedValue());
    RootedValue highWaterMarkVal(cx, NumberValue(highWaterMark));
    return ReadableStream::createDefaultStream(cx, sourceVal, sizeVal, highWaterMarkVal, proto);
}

JS_PUBLIC_API(JSObject*)
JS::NewReadableExternalSourceStreamObject(JSContext* cx, void* underlyingSource,
                                          uint8_t flags /* = 0 */,
                                          HandleObject proto /* = nullptr */)
{
    MOZ_ASSERT(!cx->zone()->isAtomsZone());
    AssertHeapIsIdle();
    CHECK_THREAD(cx);
    MOZ_ASSERT((uintptr_t(underlyingSource) & 1) == 0,
               "external underlying source pointers must be aligned");
#ifdef DEBUG
    JSRuntime* rt = cx->runtime();
    MOZ_ASSERT(rt->readableStreamDataRequestCallback);
    MOZ_ASSERT(rt->readableStreamWriteIntoReadRequestCallback);
    MOZ_ASSERT(rt->readableStreamCancelCallback);
    MOZ_ASSERT(rt->readableStreamClosedCallback);
    MOZ_ASSERT(rt->readableStreamErroredCallback);
    MOZ_ASSERT(rt->readableStreamFinalizeCallback);
#endif // DEBUG

    return ReadableStream::createExternalSourceStream(cx, underlyingSource, flags, proto);
}

JS_PUBLIC_API(bool)
JS::IsReadableStream(JSObject* obj)
{
    return obj->canUnwrapAs<ReadableStream>();
}

JS_PUBLIC_API(bool)
JS::IsReadableStreamReader(JSObject* obj)
{
    return obj->canUnwrapAs<ReadableStreamDefaultReader>();
}

JS_PUBLIC_API(bool)
JS::IsReadableStreamDefaultReader(JSObject* obj)
{
    return obj->canUnwrapAs<ReadableStreamDefaultReader>();
}

template<class T>
static MOZ_MUST_USE T*
APIToUnwrapped(JSContext* cx, JSObject* obj)
{
    cx->check(obj);
    return ToUnwrapped<T>(cx, obj);
}

JS_PUBLIC_API(bool)
JS::ReadableStreamIsReadable(JSContext* cx, HandleObject streamObj, bool* result)
{
    ReadableStream* stream = APIToUnwrapped<ReadableStream>(cx, streamObj);
    if (!stream) {
        return false;
    }

    *result = stream->readable();
    return true;
}

JS_PUBLIC_API(bool)
JS::ReadableStreamIsLocked(JSContext* cx, HandleObject streamObj, bool* result)
{
    ReadableStream* stream = APIToUnwrapped<ReadableStream>(cx, streamObj);
    if (!stream) {
        return false;
    }

    *result = stream->locked();
    return true;
}

JS_PUBLIC_API(bool)
JS::ReadableStreamIsDisturbed(JSContext* cx, HandleObject streamObj, bool* result)
{
    ReadableStream* stream = APIToUnwrapped<ReadableStream>(cx, streamObj);
    if (!stream) {
        return false;
    }

    *result = stream->disturbed();
    return true;
}

JS_PUBLIC_API(bool)
JS::ReadableStreamGetEmbeddingFlags(JSContext* cx, HandleObject streamObj, uint8_t* flags)
{
    ReadableStream* stream = APIToUnwrapped<ReadableStream>(cx, streamObj);
    if (!stream) {
        return false;
    }

    *flags = stream->embeddingFlags();
    return true;
}

JS_PUBLIC_API(JSObject*)
JS::ReadableStreamCancel(JSContext* cx, HandleObject streamObj, HandleValue reason)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);
    cx->check(reason);

    Rooted<ReadableStream*> stream(cx, APIToUnwrapped<ReadableStream>(cx, streamObj));
    if (!stream) {
        return nullptr;
    }

    return ::ReadableStreamCancel(cx, stream, reason);
}

JS_PUBLIC_API(bool)
JS::ReadableStreamGetMode(JSContext* cx, HandleObject streamObj, JS::ReadableStreamMode* mode)
{
    ReadableStream* stream = APIToUnwrapped<ReadableStream>(cx, streamObj);
    if (!stream) {
        return false;
    }

    *mode = stream->mode();
    return true;
}

JS_PUBLIC_API(JSObject*)
JS::ReadableStreamGetReader(JSContext* cx, HandleObject streamObj, ReadableStreamReaderMode mode)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);

    Rooted<ReadableStream*> stream(cx, APIToUnwrapped<ReadableStream>(cx, streamObj));
    if (!stream) {
        return nullptr;
    }

    JSObject* result = CreateReadableStreamDefaultReader(cx, stream);
    MOZ_ASSERT_IF(result, IsObjectInContextCompartment(result, cx));
    return result;
}

JS_PUBLIC_API(bool)
JS::ReadableStreamGetExternalUnderlyingSource(JSContext* cx, HandleObject streamObj, void** source)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);

    Rooted<ReadableStream*> stream(cx, APIToUnwrapped<ReadableStream>(cx, streamObj));
    if (!stream) {
        return false;
    }

    MOZ_ASSERT(stream->mode() == JS::ReadableStreamMode::ExternalSource);
    if (stream->locked()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_READABLESTREAM_LOCKED);
        return false;
    }
    if (!stream->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE,
                                  "ReadableStreamGetExternalUnderlyingSource");
        return false;
    }

    auto controller = &stream->controller()->as<ReadableByteStreamController>();
    controller->setSourceLocked();
    *source = controller->underlyingSource().toPrivate();
    return true;
}

JS_PUBLIC_API(bool)
JS::ReadableStreamReleaseExternalUnderlyingSource(JSContext* cx, HandleObject streamObj)
{
    ReadableStream* stream = APIToUnwrapped<ReadableStream>(cx, streamObj);
    if (!stream) {
        return false;
    }

    MOZ_ASSERT(stream->mode() == JS::ReadableStreamMode::ExternalSource);
    MOZ_ASSERT(stream->locked());
    MOZ_ASSERT(stream->controller()->sourceLocked());
    stream->controller()->clearSourceLocked();
    return true;
}

JS_PUBLIC_API(bool)
JS::ReadableStreamUpdateDataAvailableFromSource(JSContext* cx, JS::HandleObject streamObj,
                                                uint32_t availableData)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);

    Rooted<ReadableStream*> stream(cx, APIToUnwrapped<ReadableStream>(cx, streamObj));
    if (!stream) {
        return false;
    }

    // This is based on Streams spec 3.10.4.4. enqueue(chunk) steps 1-3 and
    // 3.12.9. ReadableByteStreamControllerEnqueue(controller, chunk) steps
    // 8-9.
    //
    // Adapted to handling updates signaled by the embedding for streams with
    // external underlying sources.
    //
    // The remaining steps of those two functions perform checks and asserts
    // that don't apply to streams with external underlying sources.

    Rooted<ReadableByteStreamController*> controller(cx,
        &stream->controller()->as<ReadableByteStreamController>());

    // Step 2: If this.[[closeRequested]] is true, throw a TypeError exception.
    if (controller->closeRequested()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_CLOSED, "enqueue");
        return false;
    }

    // Step 3: If this.[[controlledReadableStream]].[[state]] is not "readable",
    //         throw a TypeError exception.
    if (!controller->stream()->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "enqueue");
        return false;
    }

    controller->clearPullFlags();

#if DEBUG
    uint32_t oldAvailableData =
        controller->getFixedSlot(StreamController::Slot_TotalSize).toInt32();
#endif // DEBUG
    controller->setQueueTotalSize(availableData);

    // 3.12.9. ReadableByteStreamControllerEnqueue
    // Step 8.a: If ! ReadableStreamGetNumReadRequests(stream) is 0,
    // Reordered because for externally-sourced streams it applies regardless
    // of reader type.
    if (ReadableStreamGetNumReadRequests(stream) == 0) {
        return true;
    }

    // Step 8: If ! ReadableStreamHasDefaultReader(stream) is true
    bool hasDefaultReader;
    if (!ReadableStreamHasDefaultReader(cx, stream, &hasDefaultReader)) {
        return false;
    }
    if (hasDefaultReader) {
        // Step b: Otherwise,
        // Step i: Assert: controller.[[queue]] is empty.
        MOZ_ASSERT(oldAvailableData == 0);

        // Step ii: Let transferredView be
        //          ! Construct(%Uint8Array%, transferredBuffer, byteOffset, byteLength).
        JSObject* viewObj = JS_NewUint8Array(cx, availableData);
        Rooted<ArrayBufferViewObject*> transferredView(cx, &viewObj->as<ArrayBufferViewObject>());
        if (!transferredView) {
            return false;
        }

        void* underlyingSource = controller->underlyingSource().toPrivate();

        size_t bytesWritten;
        {
            AutoRealm ar(cx, stream);
            JS::AutoSuppressGCAnalysis suppressGC(cx);
            JS::AutoCheckCannotGC noGC;
            bool dummy;
            void* buffer = JS_GetArrayBufferViewData(transferredView, &dummy, noGC);
            auto cb = cx->runtime()->readableStreamWriteIntoReadRequestCallback;
            MOZ_ASSERT(cb);
            // TODO: use bytesWritten to correctly update the request's state.
            cb(cx, stream, underlyingSource, stream->embeddingFlags(), buffer,
               availableData, &bytesWritten);
        }

        // Step iii: Perform ! ReadableStreamFulfillReadRequest(stream, transferredView, false).
        RootedValue chunk(cx, ObjectValue(*transferredView));
        if (!ReadableStreamFulfillReadOrReadIntoRequest(cx, stream, chunk, false)) {
            return false;
        }

        controller->setQueueTotalSize(availableData - bytesWritten);
    } else {
        // Step b: Otherwise,
        // Step i: Assert: ! IsReadableStreamLocked(stream) is false.
        MOZ_ASSERT(!stream->locked());

        // Step ii: Perform
        //          ! ReadableByteStreamControllerEnqueueChunkToQueue(controller,
        //                                                            transferredBuffer,
        //                                                            byteOffset,
        //                                                            byteLength).
        // (Not needed for external underlying sources.)
    }

    return true;
}

JS_PUBLIC_API(bool)
JS::ReadableStreamTee(JSContext* cx, HandleObject streamObj,
                      MutableHandleObject branch1Obj, MutableHandleObject branch2Obj)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);

    Rooted<ReadableStream*> stream(cx, APIToUnwrapped<ReadableStream>(cx, streamObj));
    if (!stream) {
        return false;
    }

    Rooted<ReadableStream*> branch1Stream(cx);
    Rooted<ReadableStream*> branch2Stream(cx);
    if (!ReadableStreamTee(cx, stream, false, &branch1Stream, &branch2Stream)) {
        return false;
    }

    branch1Obj.set(branch1Stream);
    branch2Obj.set(branch2Stream);

    return true;
}

JS_PUBLIC_API(bool)
JS::ReadableStreamGetDesiredSize(JSContext* cx, JSObject* streamObj, bool* hasValue, double* value)
{
    ReadableStream* stream = APIToUnwrapped<ReadableStream>(cx, streamObj);
    if (!stream) {
        return false;
    }

    if (stream->errored()) {
        *hasValue = false;
        return true;
    }

    *hasValue = true;

    if (stream->closed()) {
        *value = 0;
        return true;
    }

    *value = ReadableStreamControllerGetDesiredSizeUnchecked(stream->controller());
    return true;
}

JS_PUBLIC_API(bool)
JS::ReadableStreamClose(JSContext* cx, HandleObject streamObj)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);

    Rooted<ReadableStream*> stream(cx, APIToUnwrapped<ReadableStream>(cx, streamObj));
    if (!stream) {
        return false;
    }

    Rooted<ReadableStreamController*> controllerObj(cx, stream->controller());
    if (!VerifyControllerStateForClosing(cx, controllerObj)) {
        return false;
    }

    if (controllerObj->is<ReadableStreamDefaultController>()) {
        Rooted<ReadableStreamDefaultController*> controller(cx);
        controller = &controllerObj->as<ReadableStreamDefaultController>();
        return ReadableStreamDefaultControllerClose(cx, controller);
    }

    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &controllerObj->as<ReadableByteStreamController>();
    return ReadableByteStreamControllerClose(cx, controller);
}

JS_PUBLIC_API(bool)
JS::ReadableStreamEnqueue(JSContext* cx, HandleObject streamObj, HandleValue chunk)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);
    cx->check(chunk);

    Rooted<ReadableStream*> stream(cx, APIToUnwrapped<ReadableStream>(cx, streamObj));
    if (!stream) {
        return false;
    }

    if (stream->mode() != JS::ReadableStreamMode::Default) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_NOT_DEFAULT_CONTROLLER,
                                  "JS::ReadableStreamEnqueue");
        return false;
    }

    Rooted<ReadableStreamDefaultController*> controller(cx);
    controller = &stream->controller()->as<ReadableStreamDefaultController>();

    MOZ_ASSERT(!controller->closeRequested());
    MOZ_ASSERT(stream->readable());

    return ReadableStreamDefaultControllerEnqueue(cx, controller, chunk);
}

JS_PUBLIC_API(bool)
JS::ReadableStreamError(JSContext* cx, HandleObject streamObj, HandleValue error)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);
    cx->check(error);

    Rooted<ReadableStream*> stream(cx, APIToUnwrapped<ReadableStream>(cx, streamObj));
    if (!stream) {
        return false;
    }

    // Step 3: If stream.[[state]] is not "readable", throw a TypeError exception.
    if (!stream->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "error");
        return false;
    }

    // Step 4: Perform ! ReadableStreamDefaultControllerError(this, e).
    Rooted<ReadableStreamController*> controller(cx, stream->controller());
    return ReadableStreamControllerError(cx, controller, error);
}

JS_PUBLIC_API(bool)
JS::ReadableStreamReaderIsClosed(JSContext* cx, HandleObject readerObj, bool* result)
{
    Rooted<ReadableStreamReader*> reader(cx, APIToUnwrapped<ReadableStreamReader>(cx, readerObj));
    if (!reader) {
        return false;
    }

    *result = reader->isClosed();
    return true;
}

JS_PUBLIC_API(bool)
JS::ReadableStreamReaderCancel(JSContext* cx, HandleObject readerObj, HandleValue reason)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);
    cx->check(reason);

    Rooted<ReadableStreamReader*> reader(cx, APIToUnwrapped<ReadableStreamReader>(cx, readerObj));
    if (!reader) {
        return false;
    }

    return ReadableStreamReaderGenericCancel(cx, reader, reason);
}

JS_PUBLIC_API(bool)
JS::ReadableStreamReaderReleaseLock(JSContext* cx, HandleObject readerObj)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);

    Rooted<ReadableStreamReader*> reader(cx, APIToUnwrapped<ReadableStreamReader>(cx, readerObj));
    if (!reader) {
        return false;
    }

#ifdef DEBUG
    Rooted<ReadableStream*> stream(cx);
    if (!UnwrapStreamFromReader(cx, reader, &stream)) {
        return false;
    }
    MOZ_ASSERT(ReadableStreamGetNumReadRequests(stream) == 0);
#endif // DEBUG

    return ReadableStreamReaderGenericRelease(cx, reader);
}

JS_PUBLIC_API(JSObject*)
JS::ReadableStreamDefaultReaderRead(JSContext* cx, HandleObject readerObj)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);

    Rooted<ReadableStreamDefaultReader*> reader(cx);
    reader = APIToUnwrapped<ReadableStreamDefaultReader>(cx, readerObj);
    if (!reader) {
        return nullptr;
    }

    return ::ReadableStreamDefaultReaderRead(cx, reader);
}
