/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsiter_h
#define jsiter_h

/*
 * JavaScript iterators.
 */

#include "mozilla/MemoryReporting.h"

#include "jscntxt.h"

#include "gc/Barrier.h"
#include "vm/Stack.h"

/*
 * For cacheable native iterators, whether the iterator is currently active.
 * Not serialized by XDR.
 */
#define JSITER_ACTIVE       0x1000
#define JSITER_UNREUSABLE   0x2000

namespace js {

struct NativeIterator
{
    HeapPtrObject obj;                  // Object being iterated.
    JSObject *iterObj_;                 // Internal iterator object.
    HeapPtr<JSFlatString> *props_array;
    HeapPtr<JSFlatString> *props_cursor;
    HeapPtr<JSFlatString> *props_end;
    Shape **shapes_array;
    uint32_t shapes_length;
    uint32_t shapes_key;
    uint32_t flags;

  private:
    /* While in compartment->enumerators, these form a doubly linked list. */
    NativeIterator *next_;
    NativeIterator *prev_;

  public:
    bool isKeyIter() const {
        return (flags & JSITER_FOREACH) == 0;
    }

    inline HeapPtr<JSFlatString> *begin() const {
        return props_array;
    }

    inline HeapPtr<JSFlatString> *end() const {
        return props_end;
    }

    size_t numKeys() const {
        return end() - begin();
    }

    JSObject *iterObj() const {
        return iterObj_;
    }
    HeapPtr<JSFlatString> *current() const {
        JS_ASSERT(props_cursor < props_end);
        return props_cursor;
    }

    NativeIterator *next() {
        return next_;
    }

    static inline size_t offsetOfNext() {
        return offsetof(NativeIterator, next_);
    }
    static inline size_t offsetOfPrev() {
        return offsetof(NativeIterator, prev_);
    }

    void incCursor() {
        props_cursor = props_cursor + 1;
    }
    void link(NativeIterator *other) {
        /* A NativeIterator cannot appear in the enumerator list twice. */
        JS_ASSERT(!next_ && !prev_);
        JS_ASSERT(flags & JSITER_ENUMERATE);

        this->next_ = other;
        this->prev_ = other->prev_;
        other->prev_->next_ = this;
        other->prev_ = this;
    }
    void unlink() {
        JS_ASSERT(flags & JSITER_ENUMERATE);

        next_->prev_ = prev_;
        prev_->next_ = next_;
        next_ = nullptr;
        prev_ = nullptr;
    }

    static NativeIterator *allocateSentinel(JSContext *cx);
    static NativeIterator *allocateIterator(JSContext *cx, uint32_t slength,
                                            const js::AutoIdVector &props);
    void init(JSObject *obj, JSObject *iterObj, unsigned flags, uint32_t slength, uint32_t key);

    void mark(JSTracer *trc);

    static void destroy(NativeIterator *iter) {
        js_free(iter);
    }
};

class PropertyIteratorObject : public JSObject
{
  public:
    static const Class class_;

    NativeIterator *getNativeIterator() const {
        return static_cast<js::NativeIterator *>(getPrivate());
    }
    void setNativeIterator(js::NativeIterator *ni) {
        setPrivate(ni);
    }

    size_t sizeOfMisc(mozilla::MallocSizeOf mallocSizeOf) const;

  private:
    static void trace(JSTracer *trc, JSObject *obj);
    static void finalize(FreeOp *fop, JSObject *obj);
};

class ArrayIteratorObject : public JSObject
{
  public:
    static const Class class_;
};

class StringIteratorObject : public JSObject
{
  public:
    static const Class class_;
};

bool
VectorToIdArray(JSContext *cx, AutoIdVector &props, JSIdArray **idap);

bool
GetIterator(JSContext *cx, HandleObject obj, unsigned flags, MutableHandleValue vp);

JSObject *
GetIteratorObject(JSContext *cx, HandleObject obj, unsigned flags);

bool
VectorToKeyIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &props,
                    MutableHandleValue vp);

bool
VectorToValueIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &props,
                      MutableHandleValue vp);

/*
 * Creates either a key or value iterator, depending on flags. For a value
 * iterator, performs value-lookup to convert the given list of jsids.
 */
bool
EnumeratedIdVectorToIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &props,
                             MutableHandleValue vp);

/*
 * Convert the value stored in *vp to its iteration object. The flags should
 * contain JSITER_ENUMERATE if js::ValueToIterator is called when enumerating
 * for-in semantics are required, and when the caller can guarantee that the
 * iterator will never be exposed to scripts.
 */
bool
ValueToIterator(JSContext *cx, unsigned flags, MutableHandleValue vp);

bool
CloseIterator(JSContext *cx, HandleObject iterObj);

bool
UnwindIteratorForException(JSContext *cx, js::HandleObject obj);

void
UnwindIteratorForUncatchableException(JSContext *cx, JSObject *obj);

bool
IteratorConstructor(JSContext *cx, unsigned argc, Value *vp);

} /* namespace js */

extern bool
js_SuppressDeletedProperty(JSContext *cx, js::HandleObject obj, jsid id);

extern bool
js_SuppressDeletedElement(JSContext *cx, js::HandleObject obj, uint32_t index);

extern bool
js_SuppressDeletedElements(JSContext *cx, js::HandleObject obj, uint32_t begin, uint32_t end);

/*
 * IteratorMore() indicates whether another value is available. It might
 * internally call iterobj.next() and then cache the value until its
 * picked up by IteratorNext(). The value is cached in the current context.
 */
extern bool
js_IteratorMore(JSContext *cx, js::HandleObject iterobj, js::MutableHandleValue rval);

extern bool
js_IteratorNext(JSContext *cx, js::HandleObject iterobj, js::MutableHandleValue rval);

extern bool
js_ThrowStopIteration(JSContext *cx);

namespace js {

/*
 * Create an object of the form { value: VALUE, done: DONE }.
 * ES6 draft from 2013-09-05, section 25.4.3.4.
 */
extern JSObject *
CreateItrResultObject(JSContext *cx, js::HandleValue value, bool done);

} /* namespace js */

/*
 * Generator state codes.
 */
enum JSGeneratorState
{
    JSGEN_NEWBORN,  /* not yet started */
    JSGEN_OPEN,     /* started by a .next() or .send(undefined) call */
    JSGEN_RUNNING,  /* currently executing via .next(), etc., call */
    JSGEN_CLOSING,  /* close method is doing asynchronous return */
    JSGEN_CLOSED    /* closed, cannot be started or closed again */
};

struct JSGenerator
{
    js::HeapPtrObject   obj;
    JSGeneratorState    state;
    js::InterpreterRegs regs;
    JSGenerator         *prevGenerator;
    js::StackFrame      *fp;
    js::HeapValue       stackSnapshot[1];
};

extern JSObject *
js_NewGenerator(JSContext *cx, const js::InterpreterRegs &regs);

extern JSObject *
js_InitIteratorClasses(JSContext *cx, js::HandleObject obj);

#endif /* jsiter_h */
