/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsarray.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/MathAlgorithms.h"

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfriendapi.h"
#include "jsfun.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jstypes.h"
#include "jsutil.h"

#include "ds/Sort.h"
#include "gc/Heap.h"
#include "vm/ArgumentsObject.h"
#include "vm/ForkJoin.h"
#include "vm/Interpreter.h"
#include "vm/NumericConversions.h"
#include "vm/Shape.h"
#include "vm/StringBuffer.h"

#include "jsatominlines.h"

#include "vm/ArgumentsObject-inl.h"
#include "vm/ArrayObject-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Runtime-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::Abs;
using mozilla::ArrayLength;
using mozilla::CeilingLog2;
using mozilla::DebugOnly;
using mozilla::IsNaN;
using mozilla::PointerRangeSize;

using JS::AutoCheckCannotGC;

bool
js::GetLengthProperty(JSContext *cx, HandleObject obj, uint32_t *lengthp)
{
    if (obj->is<ArrayObject>()) {
        *lengthp = obj->as<ArrayObject>().length();
        return true;
    }

    if (obj->is<ArgumentsObject>()) {
        ArgumentsObject &argsobj = obj->as<ArgumentsObject>();
        if (!argsobj.hasOverriddenLength()) {
            *lengthp = argsobj.initialLength();
            return true;
        }
    }

    RootedValue value(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().length, &value))
        return false;

    if (value.isInt32()) {
        *lengthp = uint32_t(value.toInt32()); // uint32_t cast does ToUint32
        return true;
    }

    return ToUint32(cx, value, lengthp);
}

/*
 * Determine if the id represents an array index.
 *
 * An id is an array index according to ECMA by (15.4):
 *
 * "Array objects give special treatment to a certain class of property names.
 * A property name P (in the form of a string value) is an array index if and
 * only if ToString(ToUint32(P)) is equal to P and ToUint32(P) is not equal
 * to 2^32-1."
 *
 * This means the largest allowed index is actually 2^32-2 (4294967294).
 *
 * In our implementation, it would be sufficient to check for id.isInt32()
 * except that by using signed 31-bit integers we miss the top half of the
 * valid range. This function checks the string representation itself; note
 * that calling a standard conversion routine might allow strings such as
 * "08" or "4.0" as array indices, which they are not.
 *
 */
template <typename CharT>
static bool
StringIsArrayIndex(const CharT *s, uint32_t length, uint32_t *indexp)
{
    const CharT *end = s + length;

    if (length == 0 || length > (sizeof("4294967294") - 1) || !JS7_ISDEC(*s))
        return false;

    uint32_t c = 0, previous = 0;
    uint32_t index = JS7_UNDEC(*s++);

    /* Don't allow leading zeros. */
    if (index == 0 && s != end)
        return false;

    for (; s < end; s++) {
        if (!JS7_ISDEC(*s))
            return false;

        previous = index;
        c = JS7_UNDEC(*s);
        index = 10 * index + c;
    }

    /* Make sure we didn't overflow. */
    if (previous < (MAX_ARRAY_INDEX / 10) || (previous == (MAX_ARRAY_INDEX / 10) &&
        c <= (MAX_ARRAY_INDEX % 10))) {
        JS_ASSERT(index <= MAX_ARRAY_INDEX);
        *indexp = index;
        return true;
    }

    return false;
}

JS_FRIEND_API(bool)
js::StringIsArrayIndex(JSLinearString *str, uint32_t *indexp)
{
    AutoCheckCannotGC nogc;
    return str->hasLatin1Chars()
           ? ::StringIsArrayIndex(str->latin1Chars(nogc), str->length(), indexp)
           : ::StringIsArrayIndex(str->twoByteChars(nogc), str->length(), indexp);
}

static bool
ToId(JSContext *cx, double index, MutableHandleId id)
{
    if (index == uint32_t(index))
        return IndexToId(cx, uint32_t(index), id);

    Value tmp = DoubleValue(index);
    return ValueToId<CanGC>(cx, HandleValue::fromMarkedLocation(&tmp), id);
}

static bool
ToId(JSContext *cx, uint32_t index, MutableHandleId id)
{
    return IndexToId(cx, index, id);
}

/*
 * If the property at the given index exists, get its value into location
 * pointed by vp and set *hole to false. Otherwise set *hole to true and *vp
 * to JSVAL_VOID. This function assumes that the location pointed by vp is
 * properly rooted and can be used as GC-protected storage for temporaries.
 */
template<typename IndexType>
static inline bool
DoGetElement(JSContext *cx, HandleObject obj, HandleObject receiver,
             IndexType index, bool *hole, MutableHandleValue vp)
{
    RootedId id(cx);
    if (!ToId(cx, index, &id))
        return false;

    RootedObject obj2(cx);
    RootedShape prop(cx);
    if (!JSObject::lookupGeneric(cx, obj, id, &obj2, &prop))
        return false;

    if (!prop) {
        vp.setUndefined();
        *hole = true;
    } else {
        if (!JSObject::getGeneric(cx, obj, receiver, id, vp))
            return false;
        *hole = false;
    }
    return true;
}

template<typename IndexType>
static void
AssertGreaterThanZero(IndexType index)
{
    JS_ASSERT(index >= 0);
    JS_ASSERT(index == floor(index));
}

template<>
void
AssertGreaterThanZero(uint32_t index)
{
}

template<typename IndexType>
static bool
GetElement(JSContext *cx, HandleObject obj, HandleObject receiver,
           IndexType index, bool *hole, MutableHandleValue vp)
{
    AssertGreaterThanZero(index);
    if (obj->isNative() && index < obj->getDenseInitializedLength()) {
        vp.set(obj->getDenseElement(uint32_t(index)));
        if (!vp.isMagic(JS_ELEMENTS_HOLE)) {
            *hole = false;
            return true;
        }
    }
    if (obj->is<ArgumentsObject>()) {
        if (obj->as<ArgumentsObject>().maybeGetElement(uint32_t(index), vp)) {
            *hole = false;
            return true;
        }
    }

    return DoGetElement(cx, obj, receiver, index, hole, vp);
}

template<typename IndexType>
static inline bool
GetElement(JSContext *cx, HandleObject obj, IndexType index, bool *hole, MutableHandleValue vp)
{
    return GetElement(cx, obj, obj, index, hole, vp);
}

static bool
GetElementsSlow(JSContext *cx, HandleObject aobj, uint32_t length, Value *vp)
{
    for (uint32_t i = 0; i < length; i++) {
        if (!JSObject::getElement(cx, aobj, aobj, i, MutableHandleValue::fromMarkedLocation(&vp[i])))
            return false;
    }

    return true;
}

bool
js::GetElements(JSContext *cx, HandleObject aobj, uint32_t length, Value *vp)
{
    if (aobj->is<ArrayObject>() && length <= aobj->getDenseInitializedLength() &&
        !ObjectMayHaveExtraIndexedProperties(aobj))
    {
        /* No other indexed properties so hole = undefined */
        const Value *srcbeg = aobj->getDenseElements();
        const Value *srcend = srcbeg + length;
        const Value *src = srcbeg;
        for (Value *dst = vp; src < srcend; ++dst, ++src)
            *dst = src->isMagic(JS_ELEMENTS_HOLE) ? UndefinedValue() : *src;
        return true;
    }

    if (aobj->is<ArgumentsObject>()) {
        ArgumentsObject &argsobj = aobj->as<ArgumentsObject>();
        if (!argsobj.hasOverriddenLength()) {
            if (argsobj.maybeGetElements(0, length, vp))
                return true;
        }
    }

    return GetElementsSlow(cx, aobj, length, vp);
}

/*
 * Set the value of the property at the given index to v assuming v is rooted.
 */
static bool
SetArrayElement(JSContext *cx, HandleObject obj, double index, HandleValue v)
{
    JS_ASSERT(index >= 0);

    if (obj->is<ArrayObject>() && !obj->isIndexed()) {
        Rooted<ArrayObject*> arr(cx, &obj->as<ArrayObject>());
        /* Predicted/prefetched code should favor the remains-dense case. */
        JSObject::EnsureDenseResult result = JSObject::ED_SPARSE;
        do {
            if (index > uint32_t(-1))
                break;
            uint32_t idx = uint32_t(index);
            if (idx >= arr->length() && !arr->lengthIsWritable()) {
                JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, nullptr,
                                             JSMSG_CANT_REDEFINE_ARRAY_LENGTH);
                return false;
            }
            result = arr->ensureDenseElements(cx, idx, 1);
            if (result != JSObject::ED_OK)
                break;
            if (idx >= arr->length())
                arr->setLengthInt32(idx + 1);
            arr->setDenseElementWithType(cx, idx, v);
            return true;
        } while (false);

        if (result == JSObject::ED_FAILED)
            return false;
        JS_ASSERT(result == JSObject::ED_SPARSE);
    }

    RootedId id(cx);
    if (!ToId(cx, index, &id))
        return false;

    RootedValue tmp(cx, v);
    return JSObject::setGeneric(cx, obj, obj, id, &tmp, true);
}

/*
 * Attempt to delete the element |index| from |obj| as if by
 * |obj.[[Delete]](index)|.
 *
 * If an error occurs while attempting to delete the element (that is, the call
 * to [[Delete]] threw), return false.
 *
 * Otherwise set *succeeded to indicate whether the deletion attempt succeeded
 * (that is, whether the call to [[Delete]] returned true or false).  (Deletes
 * generally fail only when the property is non-configurable, but proxies may
 * implement different semantics.)
 */
static bool
DeleteArrayElement(JSContext *cx, HandleObject obj, double index, bool *succeeded)
{
    JS_ASSERT(index >= 0);
    JS_ASSERT(floor(index) == index);

    if (obj->is<ArrayObject>() && !obj->isIndexed()) {
        if (index <= UINT32_MAX) {
            uint32_t idx = uint32_t(index);
            if (idx < obj->getDenseInitializedLength()) {
                obj->markDenseElementsNotPacked(cx);
                obj->setDenseElement(idx, MagicValue(JS_ELEMENTS_HOLE));
                if (!js_SuppressDeletedElement(cx, obj, idx))
                    return false;
            }
        }

        *succeeded = true;
        return true;
    }

    RootedId id(cx);
    if (!ToId(cx, index, &id))
        return false;
    return JSObject::deleteGeneric(cx, obj, id, succeeded);
}

/* ES6 20130308 draft 9.3.5 */
static bool
DeletePropertyOrThrow(JSContext *cx, HandleObject obj, double index)
{
    bool succeeded;
    if (!DeleteArrayElement(cx, obj, index, &succeeded))
        return false;
    if (succeeded)
        return true;

    RootedId id(cx);
    RootedValue indexv(cx, NumberValue(index));
    if (!ValueToId<CanGC>(cx, indexv, &id))
        return false;
    return obj->reportNotConfigurable(cx, id, JSREPORT_ERROR);
}

bool
js::SetLengthProperty(JSContext *cx, HandleObject obj, double length)
{
    RootedValue v(cx, NumberValue(length));
    return JSObject::setProperty(cx, obj, obj, cx->names().length, &v, true);
}

/*
 * Since SpiderMonkey supports cross-class prototype-based delegation, we have
 * to be careful about the length getter and setter being called on an object
 * not of Array class. For the getter, we search obj's prototype chain for the
 * array that caused this getter to be invoked. In the setter case to overcome
 * the JSPROP_SHARED attribute, we must define a shadowing length property.
 */
static bool
array_length_getter(JSContext *cx, HandleObject obj_, HandleId id, MutableHandleValue vp)
{
    RootedObject obj(cx, obj_);
    do {
        if (obj->is<ArrayObject>()) {
            vp.setNumber(obj->as<ArrayObject>().length());
            return true;
        }
        if (!JSObject::getProto(cx, obj, &obj))
            return false;
    } while (obj);
    return true;
}

static bool
array_length_setter(JSContext *cx, HandleObject obj, HandleId id, bool strict, MutableHandleValue vp)
{
    if (!obj->is<ArrayObject>()) {
        return JSObject::defineProperty(cx, obj, cx->names().length, vp,
                                        nullptr, nullptr, JSPROP_ENUMERATE);
    }

    Rooted<ArrayObject*> arr(cx, &obj->as<ArrayObject>());
    MOZ_ASSERT(arr->lengthIsWritable(),
               "setter shouldn't be called if property is non-writable");
    return ArraySetLength<SequentialExecution>(cx, arr, id, JSPROP_PERMANENT, vp, strict);
}

struct ReverseIndexComparator
{
    bool operator()(const uint32_t& a, const uint32_t& b, bool *lessOrEqualp) {
        MOZ_ASSERT(a != b, "how'd we get duplicate indexes?");
        *lessOrEqualp = b <= a;
        return true;
    }
};

template <ExecutionMode mode>
bool
js::CanonicalizeArrayLengthValue(typename ExecutionModeTraits<mode>::ContextType cx,
                                 HandleValue v, uint32_t *newLen)
{
    double d;

    if (mode == ParallelExecution) {
        if (v.isObject())
            return false;

        if (!NonObjectToUint32(cx, v, newLen))
            return false;

        if (!NonObjectToNumber(cx, v, &d))
            return false;
    } else {
        if (!ToUint32(cx->asJSContext(), v, newLen))
            return false;

        if (!ToNumber(cx->asJSContext(), v, &d))
            return false;
    }

    if (d == *newLen)
        return true;

    if (cx->isJSContext())
        JS_ReportErrorNumber(cx->asJSContext(), js_GetErrorMessage, nullptr,
                             JSMSG_BAD_ARRAY_LENGTH);
    return false;
}

template bool
js::CanonicalizeArrayLengthValue<SequentialExecution>(JSContext *cx,
                                                      HandleValue v, uint32_t *newLen);
template bool
js::CanonicalizeArrayLengthValue<ParallelExecution>(ForkJoinContext *cx,
                                                    HandleValue v, uint32_t *newLen);

/* ES6 20130308 draft 8.4.2.4 ArraySetLength */
template <ExecutionMode mode>
bool
js::ArraySetLength(typename ExecutionModeTraits<mode>::ContextType cxArg,
                   Handle<ArrayObject*> arr, HandleId id,
                   unsigned attrs, HandleValue value, bool setterIsStrict)
{
    MOZ_ASSERT(cxArg->isThreadLocal(arr));
    MOZ_ASSERT(id == NameToId(cxArg->names().length));

    /* Steps 1-2 are irrelevant in our implementation. */

    /* Steps 3-5. */
    uint32_t newLen;
    if (!CanonicalizeArrayLengthValue<mode>(cxArg, value, &newLen))
        return false;

    // Abort if we're being asked to change enumerability or configurability.
    // (The length property of arrays is non-configurable, so such attempts
    // must fail.)  This behavior is spread throughout the ArraySetLength spec
    // algorithm, but we only need check it once as our array implementation
    // is internally so different from the spec algorithm.  (ES5 and ES6 define
    // behavior by delegating to the default define-own-property algorithm --
    // OrdinaryDefineOwnProperty in ES6, the default [[DefineOwnProperty]] in
    // ES5 -- but we reimplement all the conflict-detection bits ourselves here
    // so that we can use a customized length representation.)
    if (!(attrs & JSPROP_PERMANENT) || (attrs & JSPROP_ENUMERATE)) {
        if (!setterIsStrict)
            return true;
        // Bail for strict mode in parallel execution, as we need to go back
        // to sequential mode to throw the error.
        if (mode == ParallelExecution)
            return false;
        return Throw(cxArg->asJSContext(), id, JSMSG_CANT_REDEFINE_PROP);
    }

    /* Steps 6-7. */
    bool lengthIsWritable = arr->lengthIsWritable();
#ifdef DEBUG
    {
        RootedShape lengthShape(cxArg, arr->nativeLookupPure(id));
        MOZ_ASSERT(lengthShape);
        MOZ_ASSERT(lengthShape->writable() == lengthIsWritable);
    }
#endif

    uint32_t oldLen = arr->length();

    /* Steps 8-9 for arrays with non-writable length. */
    if (!lengthIsWritable) {
        if (newLen == oldLen)
            return true;

        if (!cxArg->isJSContext())
            return false;

        if (setterIsStrict) {
            return JS_ReportErrorFlagsAndNumber(cxArg->asJSContext(),
                                                JSREPORT_ERROR, js_GetErrorMessage, nullptr,
                                                JSMSG_CANT_REDEFINE_ARRAY_LENGTH);
        }

        return JSObject::reportReadOnly(cxArg->asJSContext(), id,
                                        JSREPORT_STRICT | JSREPORT_WARNING);
    }

    /* Step 8. */
    bool succeeded = true;
    do {
        // The initialized length and capacity of an array only need updating
        // when non-hole elements are added or removed, which doesn't happen
        // when array length stays the same or increases.
        if (newLen >= oldLen)
            break;

        // Attempt to propagate dense-element optimization tricks, if possible,
        // and avoid the generic (and accordingly slow) deletion code below.
        // We can only do this if there are only densely-indexed elements.
        // Once there's a sparse indexed element, there's no good way to know,
        // save by enumerating all the properties to find it.  But we *have* to
        // know in case that sparse indexed element is non-configurable, as
        // that element must prevent any deletions below it.  Bug 586842 should
        // fix this inefficiency by moving indexed storage to be entirely
        // separate from non-indexed storage.
        if (!arr->isIndexed()) {
            uint32_t oldCapacity = arr->getDenseCapacity();
            uint32_t oldInitializedLength = arr->getDenseInitializedLength();
            MOZ_ASSERT(oldCapacity >= oldInitializedLength);
            if (oldInitializedLength > newLen)
                arr->setDenseInitializedLength(newLen);
            if (oldCapacity > newLen)
                arr->shrinkElements(cxArg, newLen);

            // We've done the work of deleting any dense elements needing
            // deletion, and there are no sparse elements.  Thus we can skip
            // straight to defining the length.
            break;
        }

        // Bail from parallel execution if need to perform step 15, which is
        // unsafe and isn't a common case.
        if (mode == ParallelExecution)
            return false;

        JSContext *cx = cxArg->asJSContext();

        // Step 15.
        //
        // Attempt to delete all elements above the new length, from greatest
        // to least.  If any of these deletions fails, we're supposed to define
        // the length to one greater than the index that couldn't be deleted,
        // *with the property attributes specified*.  This might convert the
        // length to be not the value specified, yet non-writable.  (You may be
        // forgiven for thinking these are interesting semantics.)  Example:
        //
        //   var arr =
        //     Object.defineProperty([0, 1, 2, 3], 1, { writable: false });
        //   Object.defineProperty(arr, "length",
        //                         { value: 0, writable: false });
        //
        // will convert |arr| to an array of non-writable length two, then
        // throw a TypeError.
        //
        // We implement this behavior, in the relevant lops below, by setting
        // |succeeded| to false.  Then we exit the loop, define the length
        // appropriately, and only then throw a TypeError, if necessary.
        uint32_t gap = oldLen - newLen;
        const uint32_t RemoveElementsFastLimit = 1 << 24;
        if (gap < RemoveElementsFastLimit) {
            // If we're removing a relatively small number of elements, just do
            // it exactly by the spec.
            while (newLen < oldLen) {
                /* Step 15a. */
                oldLen--;

                /* Steps 15b-d. */
                bool deleteSucceeded;
                if (!JSObject::deleteElement(cx, arr, oldLen, &deleteSucceeded))
                    return false;
                if (!deleteSucceeded) {
                    newLen = oldLen + 1;
                    succeeded = false;
                    break;
                }
            }
        } else {
            // If we're removing a large number of elements from an array
            // that's probably sparse, try a different tack.  Get all the own
            // property names, sift out the indexes in the deletion range into
            // a vector, sort the vector greatest to least, then delete the
            // indexes greatest to least using that vector.  See bug 322135.
            //
            // This heuristic's kind of a huge guess -- "large number of
            // elements" and "probably sparse" are completely unprincipled
            // predictions.  In the long run, bug 586842 will support the right
            // fix: store sparse elements in a sorted data structure that
            // permits fast in-reverse-order traversal and concurrent removals.

            Vector<uint32_t> indexes(cx);
            {
                AutoIdVector props(cx);
                if (!GetPropertyNames(cx, arr, JSITER_OWNONLY | JSITER_HIDDEN, &props))
                    return false;

                for (size_t i = 0; i < props.length(); i++) {
                    if (!CheckForInterrupt(cx))
                        return false;

                    uint32_t index;
                    if (!js_IdIsIndex(props[i], &index))
                        continue;

                    if (index >= newLen && index < oldLen) {
                        if (!indexes.append(index))
                            return false;
                    }
                }
            }

            uint32_t count = indexes.length();
            {
                // We should use radix sort to be O(n), but this is uncommon
                // enough that we'll punt til someone complains.
                Vector<uint32_t> scratch(cx);
                if (!scratch.resize(count))
                    return false;
                MOZ_ALWAYS_TRUE(MergeSort(indexes.begin(), count, scratch.begin(),
                                          ReverseIndexComparator()));
            }

            uint32_t index = UINT32_MAX;
            for (uint32_t i = 0; i < count; i++) {
                MOZ_ASSERT(indexes[i] < index, "indexes should never repeat");
                index = indexes[i];

                /* Steps 15b-d. */
                bool deleteSucceeded;
                if (!JSObject::deleteElement(cx, arr, index, &deleteSucceeded))
                    return false;
                if (!deleteSucceeded) {
                    newLen = index + 1;
                    succeeded = false;
                    break;
                }
            }
        }
    } while (false);

    /* Steps 12, 16. */

    // Yes, we totally drop a non-stub getter/setter from a defineProperty
    // API call on the floor here.  Given that getter/setter will go away in
    // the long run, with accessors replacing them both internally and at the
    // API level, just run with this.
    RootedShape lengthShape(cxArg, mode == ParallelExecution
                            ? arr->nativeLookupPure(id)
                            : arr->nativeLookup(cxArg->asJSContext(), id));
    if (!JSObject::changeProperty<mode>(cxArg, arr, lengthShape, attrs,
                                        JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_SHARED,
                                        array_length_getter, array_length_setter))
    {
        return false;
    }

    if (mode == ParallelExecution) {
        // Overflowing int32 requires changing TI state.
        if (newLen > INT32_MAX)
            return false;
        arr->setLengthInt32(newLen);
    } else {
        JSContext *cx = cxArg->asJSContext();
        arr->setLength(cx, newLen);
    }


    // All operations past here until the |!succeeded| code must be infallible,
    // so that all element fields remain properly synchronized.

    // Trim the initialized length, if needed, to preserve the <= length
    // invariant.  (Capacity was already reduced during element deletion, if
    // necessary.)
    ObjectElements *header = arr->getElementsHeader();
    header->initializedLength = Min(header->initializedLength, newLen);

    if (attrs & JSPROP_READONLY) {
        header->setNonwritableArrayLength();

        // When an array's length becomes non-writable, writes to indexes
        // greater than or equal to the length don't change the array.  We
        // handle this with a check for non-writable length in most places.
        // But in JIT code every check counts -- so we piggyback the check on
        // the already-required range check for |index < capacity| by making
        // capacity of arrays with non-writable length never exceed the length.
        if (arr->getDenseCapacity() > newLen) {
            arr->shrinkElements(cxArg, newLen);
            arr->getElementsHeader()->capacity = newLen;
        }
    }

    if (setterIsStrict && !succeeded) {
        // We can't have arrived here under ParallelExecution, as we have
        // returned from the function before step 15 above.
        JSContext *cx = cxArg->asJSContext();
        RootedId elementId(cx);
        if (!IndexToId(cx, newLen - 1, &elementId))
            return false;
        return arr->reportNotConfigurable(cx, elementId);
    }

    return true;
}

template bool
js::ArraySetLength<SequentialExecution>(JSContext *cx, Handle<ArrayObject*> arr,
                                        HandleId id, unsigned attrs, HandleValue value,
                                        bool setterIsStrict);
template bool
js::ArraySetLength<ParallelExecution>(ForkJoinContext *cx, Handle<ArrayObject*> arr,
                                      HandleId id, unsigned attrs, HandleValue value,
                                      bool setterIsStrict);

bool
js::WouldDefinePastNonwritableLength(ThreadSafeContext *cx,
                                     HandleObject obj, uint32_t index, bool strict,
                                     bool *definesPast)
{
    if (!obj->is<ArrayObject>()) {
        *definesPast = false;
        return true;
    }

    Rooted<ArrayObject*> arr(cx, &obj->as<ArrayObject>());
    uint32_t length = arr->length();
    if (index < length) {
        *definesPast = false;
        return true;
    }

    if (arr->lengthIsWritable()) {
        *definesPast = false;
        return true;
    }

    *definesPast = true;

    // Error in strict mode code or warn with strict option.
    unsigned flags = strict ? JSREPORT_ERROR : (JSREPORT_STRICT | JSREPORT_WARNING);
    if (cx->isForkJoinContext())
        return cx->asForkJoinContext()->reportError(flags);

    if (!cx->isJSContext())
        return true;

    JSContext *ncx = cx->asJSContext();

    if (!strict && !ncx->compartment()->options().extraWarnings(ncx))
        return true;

    // XXX include the index and maybe array length in the error message
    return JS_ReportErrorFlagsAndNumber(ncx, flags, js_GetErrorMessage, nullptr,
                                        JSMSG_CANT_DEFINE_PAST_ARRAY_LENGTH);
}

static bool
array_addProperty(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    Rooted<ArrayObject*> arr(cx, &obj->as<ArrayObject>());

    uint32_t index;
    if (!js_IdIsIndex(id, &index))
        return true;

    uint32_t length = arr->length();
    if (index >= length) {
        MOZ_ASSERT(arr->lengthIsWritable(),
                   "how'd this element get added if length is non-writable?");
        arr->setLength(cx, index + 1);
    }
    return true;
}

bool
js::ObjectMayHaveExtraIndexedProperties(JSObject *obj)
{
    /*
     * Whether obj may have indexed properties anywhere besides its dense
     * elements. This includes other indexed properties in its shape hierarchy,
     * and indexed properties or elements along its prototype chain.
     */

    JS_ASSERT(obj->isNative());

    if (obj->isIndexed())
        return true;

    /*
     * Walk up the prototype chain and see if this indexed element already
     * exists. If we hit the end of the prototype chain, it's safe to set the
     * element on the original object.
     */
    while ((obj = obj->getProto()) != nullptr) {
        /*
         * If the prototype is a non-native object (possibly a dense array), or
         * a native object (possibly a slow array) that has indexed properties,
         * return true.
         */
        if (!obj->isNative())
            return true;
        if (obj->isIndexed())
            return true;
        if (obj->getDenseInitializedLength() > 0)
            return true;
        if (obj->is<TypedArrayObject>())
            return true;
    }

    return false;
}

static bool
AddLengthProperty(ExclusiveContext *cx, HandleObject obj)
{
    /*
     * Add the 'length' property for a newly created array,
     * and update the elements to be an empty array owned by the object.
     * The shared emptyObjectElements singleton cannot be used for slow arrays,
     * as accesses to 'length' will use the elements header.
     */

    RootedId lengthId(cx, NameToId(cx->names().length));
    JS_ASSERT(!obj->nativeLookup(cx, lengthId));

    return JSObject::addProperty(cx, obj, lengthId, array_length_getter, array_length_setter,
                                 SHAPE_INVALID_SLOT, JSPROP_PERMANENT | JSPROP_SHARED, 0,
                                 /* allowDictionary = */ false);
}

#if JS_HAS_TOSOURCE

static bool
array_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.thisv().isObject()) {
        ReportIncompatible(cx, args);
        return false;
    }

    Rooted<JSObject*> obj(cx, &args.thisv().toObject());
    RootedValue elt(cx);

    AutoCycleDetector detector(cx, obj);
    if (!detector.init())
        return false;

    StringBuffer sb(cx);

    if (detector.foundCycle()) {
        if (!sb.append("[]"))
            return false;
        goto make_string;
    }

    if (!sb.append('['))
        return false;

    uint32_t length;
    if (!GetLengthProperty(cx, obj, &length))
        return false;

    for (uint32_t index = 0; index < length; index++) {
        bool hole;
        if (!CheckForInterrupt(cx) ||
            !GetElement(cx, obj, index, &hole, &elt)) {
            return false;
        }

        /* Get element's character string. */
        JSString *str;
        if (hole) {
            str = cx->runtime()->emptyString;
        } else {
            str = ValueToSource(cx, elt);
            if (!str)
                return false;
        }

        /* Append element to buffer. */
        if (!sb.append(str))
            return false;
        if (index + 1 != length) {
            if (!sb.append(", "))
                return false;
        } else if (hole) {
            if (!sb.append(','))
                return false;
        }
    }

    /* Finalize the buffer. */
    if (!sb.append(']'))
        return false;

  make_string:
    JSString *str = sb.finishString();
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

#endif

struct EmptySeparatorOp
{
    bool operator()(JSContext *, StringBuffer &sb) { return true; }
};

template <typename CharT>
struct CharSeparatorOp
{
    const CharT sep;
    explicit CharSeparatorOp(CharT sep) : sep(sep) {};
    bool operator()(JSContext *, StringBuffer &sb) { return sb.append(sep); }
};

struct StringSeparatorOp
{
    HandleLinearString sep;

    explicit StringSeparatorOp(HandleLinearString sep) : sep(sep) {}

    bool operator()(JSContext *cx, StringBuffer &sb) {
        return sb.append(sep);
    }
};

template <bool Locale, typename SeparatorOp>
static bool
ArrayJoinKernel(JSContext *cx, SeparatorOp sepOp, HandleObject obj, uint32_t length,
               StringBuffer &sb)
{
    uint32_t i = 0;

    if (!Locale && obj->is<ArrayObject>() && !ObjectMayHaveExtraIndexedProperties(obj)) {
        // This loop handles all elements up to initializedLength. If
        // length > initLength we rely on the second loop to add the
        // other elements.
        uint32_t initLength = obj->getDenseInitializedLength();
        while (i < initLength) {
            if (!CheckForInterrupt(cx))
                return false;

            const Value &elem = obj->getDenseElement(i);

            if (elem.isString()) {
                if (!sb.append(elem.toString()))
                    return false;
            } else if (elem.isNumber()) {
                if (!NumberValueToStringBuffer(cx, elem, sb))
                    return false;
            } else if (elem.isBoolean()) {
                if (!BooleanToStringBuffer(elem.toBoolean(), sb))
                    return false;
            } else if (elem.isObject() || elem.isSymbol()) {
                /*
                 * Object stringifying could modify the initialized length or make
                 * the array sparse. Delegate it to a separate loop to keep this
                 * one tight.
                 *
                 * Symbol stringifying is a TypeError, so into the slow path
                 * with those as well.
                 */
                break;
            } else {
                JS_ASSERT(elem.isMagic(JS_ELEMENTS_HOLE) || elem.isNullOrUndefined());
            }

            if (++i != length && !sepOp(cx, sb))
                return false;
        }
    }

    if (i != length) {
        RootedValue v(cx);
        while (i < length) {
            if (!CheckForInterrupt(cx))
                return false;

            bool hole;
            if (!GetElement(cx, obj, i, &hole, &v))
                return false;
            if (!hole && !v.isNullOrUndefined()) {
                if (Locale) {
                    JSObject *robj = ToObject(cx, v);
                    if (!robj)
                        return false;
                    RootedId id(cx, NameToId(cx->names().toLocaleString));
                    if (!robj->callMethod(cx, id, 0, nullptr, &v))
                        return false;
                }
                if (!ValueToStringBuffer(cx, v, sb))
                    return false;
            }

            if (++i != length && !sepOp(cx, sb))
                return false;
        }
    }

    return true;
}

template <bool Locale>
static bool
ArrayJoin(JSContext *cx, CallArgs &args)
{
    // This method is shared by Array.prototype.join and
    // Array.prototype.toLocaleString. The steps in ES5 are nearly the same, so
    // the annotations in this function apply to both toLocaleString and join.

    // Step 1
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    AutoCycleDetector detector(cx, obj);
    if (!detector.init())
        return false;

    if (detector.foundCycle()) {
        args.rval().setString(cx->names().empty);
        return true;
    }

    // Steps 2 and 3
    uint32_t length;
    if (!GetLengthProperty(cx, obj, &length))
        return false;

    // Steps 4 and 5
    RootedLinearString sepstr(cx);
    if (!Locale && args.hasDefined(0)) {
        JSString *s = ToString<CanGC>(cx, args[0]);
        if (!s)
            return false;
        sepstr = s->ensureLinear(cx);
        if (!sepstr)
            return false;
    } else {
        sepstr = cx->names().comma;
    }

    JS::Anchor<JSString*> anchor(sepstr);

    // Step 6 is implicit in the loops below.

    // An optimized version of a special case of steps 7-11: when length==1 and
    // the 0th element is a string, ToString() of that element is a no-op and
    // so it can be immediately returned as the result.
    if (length == 1 && !Locale && obj->is<ArrayObject>() &&
        obj->getDenseInitializedLength() == 1)
    {
        const Value &elem0 = obj->getDenseElement(0);
        if (elem0.isString()) {
            args.rval().setString(elem0.toString());
            return true;
        }
    }

    StringBuffer sb(cx);
    if (sepstr->hasTwoByteChars() && !sb.ensureTwoByteChars())
        return false;

    // The separator will be added |length - 1| times, reserve space for that
    // so that we don't have to unnecessarily grow the buffer.
    size_t seplen = sepstr->length();
    if (length > 0 && !sb.reserve(seplen * (length - 1)))
        return false;

    // Various optimized versions of steps 7-10.
    if (seplen == 0) {
        EmptySeparatorOp op;
        if (!ArrayJoinKernel<Locale>(cx, op, obj, length, sb))
            return false;
    } else if (seplen == 1) {
        jschar c = sepstr->latin1OrTwoByteChar(0);
        if (c <= JSString::MAX_LATIN1_CHAR) {
            CharSeparatorOp<Latin1Char> op(c);
            if (!ArrayJoinKernel<Locale>(cx, op, obj, length, sb))
                return false;
        } else {
            CharSeparatorOp<jschar> op(c);
            if (!ArrayJoinKernel<Locale>(cx, op, obj, length, sb))
                return false;
        }
    } else {
        StringSeparatorOp op(sepstr);
        if (!ArrayJoinKernel<Locale>(cx, op, obj, length, sb))
            return false;
    }

    // Step 11
    JSString *str = sb.finishString();
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

/* ES5 15.4.4.2. NB: The algorithm here differs from the one in ES3. */
static bool
array_toString(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    RootedValue join(cx, args.calleev());
    if (!JSObject::getProperty(cx, obj, obj, cx->names().join, &join))
        return false;

    if (!IsCallable(join)) {
        JSString *str = JS_BasicObjectToString(cx, obj);
        if (!str)
            return false;
        args.rval().setString(str);
        return true;
    }

    InvokeArgs args2(cx);
    if (!args2.init(0))
        return false;

    args2.setCallee(join);
    args2.setThis(ObjectValue(*obj));

    /* Do the call. */
    if (!Invoke(cx, args2))
        return false;
    args.rval().set(args2.rval());
    return true;
}

/* ES5 15.4.4.3 */
static bool
array_toLocaleString(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    return ArrayJoin<true>(cx, args);
}

/* ES5 15.4.4.5 */
static bool
array_join(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    return ArrayJoin<false>(cx, args);
}

static inline bool
InitArrayTypes(JSContext *cx, TypeObject *type, const Value *vector, unsigned count)
{
    if (!type->unknownProperties()) {
        AutoEnterAnalysis enter(cx);

        HeapTypeSet *types = type->getProperty(cx, JSID_VOID);
        if (!types)
            return false;

        for (unsigned i = 0; i < count; i++) {
            if (vector[i].isMagic(JS_ELEMENTS_HOLE))
                continue;
            Type valtype = GetValueType(vector[i]);
            types->addType(cx, valtype);
        }
    }
    return true;
}

enum ShouldUpdateTypes
{
    UpdateTypes = true,
    DontUpdateTypes = false
};

/* vector must point to rooted memory. */
static bool
InitArrayElements(JSContext *cx, HandleObject obj, uint32_t start, uint32_t count, const Value *vector, ShouldUpdateTypes updateTypes)
{
    JS_ASSERT(count <= MAX_ARRAY_INDEX);

    if (count == 0)
        return true;

    types::TypeObject *type = obj->getType(cx);
    if (!type)
        return false;
    if (updateTypes && !InitArrayTypes(cx, type, vector, count))
        return false;

    /*
     * Optimize for dense arrays so long as adding the given set of elements
     * wouldn't otherwise make the array slow or exceed a non-writable array
     * length.
     */
    do {
        if (!obj->is<ArrayObject>())
            break;
        if (ObjectMayHaveExtraIndexedProperties(obj))
            break;

        if (obj->shouldConvertDoubleElements())
            break;

        Rooted<ArrayObject*> arr(cx, &obj->as<ArrayObject>());

        if (!arr->lengthIsWritable() && start + count > arr->length())
            break;

        JSObject::EnsureDenseResult result = arr->ensureDenseElements(cx, start, count);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        uint32_t newlen = start + count;
        if (newlen > arr->length())
            arr->setLengthInt32(newlen);

        JS_ASSERT(count < UINT32_MAX / sizeof(Value));
        arr->copyDenseElements(start, vector, count);
        JS_ASSERT_IF(count != 0, !arr->getDenseElement(newlen - 1).isMagic(JS_ELEMENTS_HOLE));
        return true;
    } while (false);

    const Value* end = vector + count;
    while (vector < end && start <= MAX_ARRAY_INDEX) {
        if (!CheckForInterrupt(cx) ||
            !SetArrayElement(cx, obj, start++, HandleValue::fromMarkedLocation(vector++))) {
            return false;
        }
    }

    if (vector == end)
        return true;

    JS_ASSERT(start == MAX_ARRAY_INDEX + 1);
    RootedValue value(cx);
    RootedId id(cx);
    RootedValue indexv(cx);
    double index = MAX_ARRAY_INDEX + 1;
    do {
        value = *vector++;
        indexv = DoubleValue(index);
        if (!ValueToId<CanGC>(cx, indexv, &id) ||
            !JSObject::setGeneric(cx, obj, obj, id, &value, true))
        {
            return false;
        }
        index += 1;
    } while (vector != end);

    return true;
}

static bool
array_reverse(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
        return false;

    do {
        if (!obj->is<ArrayObject>())
            break;
        if (ObjectMayHaveExtraIndexedProperties(obj))
            break;

        /* An empty array or an array with no elements is already reversed. */
        if (len == 0 || obj->getDenseCapacity() == 0) {
            args.rval().setObject(*obj);
            return true;
        }

        /*
         * It's actually surprisingly complicated to reverse an array due to the
         * orthogonality of array length and array capacity while handling
         * leading and trailing holes correctly.  Reversing seems less likely to
         * be a common operation than other array mass-mutation methods, so for
         * now just take a probably-small memory hit (in the absence of too many
         * holes in the array at its start) and ensure that the capacity is
         * sufficient to hold all the elements in the array if it were full.
         */
        JSObject::EnsureDenseResult result = obj->ensureDenseElements(cx, len, 0);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        /* Fill out the array's initialized length to its proper length. */
        obj->ensureDenseInitializedLength(cx, len, 0);

        RootedValue origlo(cx), orighi(cx);

        uint32_t lo = 0, hi = len - 1;
        for (; lo < hi; lo++, hi--) {
            origlo = obj->getDenseElement(lo);
            orighi = obj->getDenseElement(hi);
            obj->setDenseElement(lo, orighi);
            if (orighi.isMagic(JS_ELEMENTS_HOLE) &&
                !js_SuppressDeletedProperty(cx, obj, INT_TO_JSID(lo))) {
                return false;
            }
            obj->setDenseElement(hi, origlo);
            if (origlo.isMagic(JS_ELEMENTS_HOLE) &&
                !js_SuppressDeletedProperty(cx, obj, INT_TO_JSID(hi))) {
                return false;
            }
        }

        /*
         * Per ECMA-262, don't update the length of the array, even if the new
         * array has trailing holes (and thus the original array began with
         * holes).
         */
        args.rval().setObject(*obj);
        return true;
    } while (false);

    RootedValue lowval(cx), hival(cx);
    for (uint32_t i = 0, half = len / 2; i < half; i++) {
        bool hole, hole2;
        if (!CheckForInterrupt(cx) ||
            !GetElement(cx, obj, i, &hole, &lowval) ||
            !GetElement(cx, obj, len - i - 1, &hole2, &hival))
        {
            return false;
        }

        if (!hole && !hole2) {
            if (!SetArrayElement(cx, obj, i, hival))
                return false;
            if (!SetArrayElement(cx, obj, len - i - 1, lowval))
                return false;
        } else if (hole && !hole2) {
            if (!SetArrayElement(cx, obj, i, hival))
                return false;
            if (!DeletePropertyOrThrow(cx, obj, len - i - 1))
                return false;
        } else if (!hole && hole2) {
            if (!DeletePropertyOrThrow(cx, obj, i))
                return false;
            if (!SetArrayElement(cx, obj, len - i - 1, lowval))
                return false;
        } else {
            // No action required.
        }
    }
    args.rval().setObject(*obj);
    return true;
}

namespace {

inline bool
CompareStringValues(JSContext *cx, const Value &a, const Value &b, bool *lessOrEqualp)
{
    if (!CheckForInterrupt(cx))
        return false;

    JSString *astr = a.toString();
    JSString *bstr = b.toString();
    int32_t result;
    if (!CompareStrings(cx, astr, bstr, &result))
        return false;

    *lessOrEqualp = (result <= 0);
    return true;
}

static const uint64_t powersOf10[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000, 1000000000000ULL
};

static inline unsigned
NumDigitsBase10(uint32_t n)
{
    /*
     * This is just floor_log10(n) + 1
     * Algorithm taken from
     * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10
     */
    uint32_t log2 = CeilingLog2(n);
    uint32_t t = log2 * 1233 >> 12;
    return t - (n < powersOf10[t]) + 1;
}

static inline bool
CompareLexicographicInt32(const Value &a, const Value &b, bool *lessOrEqualp)
{
    int32_t aint = a.toInt32();
    int32_t bint = b.toInt32();

    /*
     * If both numbers are equal ... trivial
     * If only one of both is negative --> arithmetic comparison as char code
     * of '-' is always less than any other digit
     * If both numbers are negative convert them to positive and continue
     * handling ...
     */
    if (aint == bint) {
        *lessOrEqualp = true;
    } else if ((aint < 0) && (bint >= 0)) {
        *lessOrEqualp = true;
    } else if ((aint >= 0) && (bint < 0)) {
        *lessOrEqualp = false;
    } else {
        uint32_t auint = Abs(aint);
        uint32_t buint = Abs(bint);

        /*
         *  ... get number of digits of both integers.
         * If they have the same number of digits --> arithmetic comparison.
         * If digits_a > digits_b: a < b*10e(digits_a - digits_b).
         * If digits_b > digits_a: a*10e(digits_b - digits_a) <= b.
         */
        unsigned digitsa = NumDigitsBase10(auint);
        unsigned digitsb = NumDigitsBase10(buint);
        if (digitsa == digitsb) {
            *lessOrEqualp = (auint <= buint);
        } else if (digitsa > digitsb) {
            JS_ASSERT((digitsa - digitsb) < ArrayLength(powersOf10));
            *lessOrEqualp = (uint64_t(auint) < uint64_t(buint) * powersOf10[digitsa - digitsb]);
        } else { /* if (digitsb > digitsa) */
            JS_ASSERT((digitsb - digitsa) < ArrayLength(powersOf10));
            *lessOrEqualp = (uint64_t(auint) * powersOf10[digitsb - digitsa] <= uint64_t(buint));
        }
    }

    return true;
}

template <typename Char1, typename Char2>
static inline bool
CompareSubStringValues(JSContext *cx, const Char1 *s1, size_t len1, const Char2 *s2, size_t len2,
                       bool *lessOrEqualp)
{
    if (!CheckForInterrupt(cx))
        return false;

    if (!s1 || !s2)
        return false;

    int32_t result = CompareChars(s1, len1, s2, len2);
    *lessOrEqualp = (result <= 0);
    return true;
}

struct SortComparatorStrings
{
    JSContext   *const cx;

    explicit SortComparatorStrings(JSContext *cx)
      : cx(cx) {}

    bool operator()(const Value &a, const Value &b, bool *lessOrEqualp) {
        return CompareStringValues(cx, a, b, lessOrEqualp);
    }
};

struct SortComparatorLexicographicInt32
{
    bool operator()(const Value &a, const Value &b, bool *lessOrEqualp) {
        return CompareLexicographicInt32(a, b, lessOrEqualp);
    }
};

struct StringifiedElement
{
    size_t charsBegin;
    size_t charsEnd;
    size_t elementIndex;
};

struct SortComparatorStringifiedElements
{
    JSContext           *const cx;
    const StringBuffer  &sb;

    SortComparatorStringifiedElements(JSContext *cx, const StringBuffer &sb)
      : cx(cx), sb(sb) {}

    bool operator()(const StringifiedElement &a, const StringifiedElement &b, bool *lessOrEqualp) {
        size_t lenA = a.charsEnd - a.charsBegin;
        size_t lenB = b.charsEnd - b.charsBegin;

        if (sb.isUnderlyingBufferLatin1()) {
            return CompareSubStringValues(cx, sb.rawLatin1Begin() + a.charsBegin, lenA,
                                          sb.rawLatin1Begin() + b.charsBegin, lenB,
                                          lessOrEqualp);
        }

        return CompareSubStringValues(cx, sb.rawTwoByteBegin() + a.charsBegin, lenA,
                                      sb.rawTwoByteBegin() + b.charsBegin, lenB,
                                      lessOrEqualp);
    }
};

struct SortComparatorFunction
{
    JSContext          *const cx;
    const Value        &fval;
    FastInvokeGuard    &fig;

    SortComparatorFunction(JSContext *cx, const Value &fval, FastInvokeGuard &fig)
      : cx(cx), fval(fval), fig(fig) { }

    bool operator()(const Value &a, const Value &b, bool *lessOrEqualp);
};

bool
SortComparatorFunction::operator()(const Value &a, const Value &b, bool *lessOrEqualp)
{
    /*
     * array_sort deals with holes and undefs on its own and they should not
     * come here.
     */
    JS_ASSERT(!a.isMagic() && !a.isUndefined());
    JS_ASSERT(!a.isMagic() && !b.isUndefined());

    if (!CheckForInterrupt(cx))
        return false;

    InvokeArgs &args = fig.args();
    if (!args.init(2))
        return false;

    args.setCallee(fval);
    args.setThis(UndefinedValue());
    args[0].set(a);
    args[1].set(b);

    if (!fig.invoke(cx))
        return false;

    double cmp;
    if (!ToNumber(cx, args.rval(), &cmp))
        return false;

    /*
     * XXX eport some kind of error here if cmp is NaN? ECMA talks about
     * 'consistent compare functions' that don't return NaN, but is silent
     * about what the result should be. So we currently ignore it.
     */
    *lessOrEqualp = (IsNaN(cmp) || cmp <= 0);
    return true;
}

struct NumericElement
{
    double dv;
    size_t elementIndex;
};

bool
ComparatorNumericLeftMinusRight(const NumericElement &a, const NumericElement &b,
                                bool *lessOrEqualp)
{
    *lessOrEqualp = (a.dv <= b.dv);
    return true;
}

bool
ComparatorNumericRightMinusLeft(const NumericElement &a, const NumericElement &b,
                                bool *lessOrEqualp)
{
    *lessOrEqualp = (b.dv <= a.dv);
    return true;
}

typedef bool (*ComparatorNumeric)(const NumericElement &a, const NumericElement &b,
                                  bool *lessOrEqualp);

ComparatorNumeric SortComparatorNumerics[] = {
    nullptr,
    nullptr,
    ComparatorNumericLeftMinusRight,
    ComparatorNumericRightMinusLeft
};

bool
ComparatorInt32LeftMinusRight(const Value &a, const Value &b, bool *lessOrEqualp)
{
    *lessOrEqualp = (a.toInt32() <= b.toInt32());
    return true;
}

bool
ComparatorInt32RightMinusLeft(const Value &a, const Value &b, bool *lessOrEqualp)
{
    *lessOrEqualp = (b.toInt32() <= a.toInt32());
    return true;
}

typedef bool (*ComparatorInt32)(const Value &a, const Value &b, bool *lessOrEqualp);

ComparatorInt32 SortComparatorInt32s[] = {
    nullptr,
    nullptr,
    ComparatorInt32LeftMinusRight,
    ComparatorInt32RightMinusLeft
};

// Note: Values for this enum must match up with SortComparatorNumerics
// and SortComparatorInt32s.
enum ComparatorMatchResult {
    Match_Failure = 0,
    Match_None,
    Match_LeftMinusRight,
    Match_RightMinusLeft
};

/*
 * Specialize behavior for comparator functions with particular common bytecode
 * patterns: namely, |return x - y| and |return y - x|.
 */
ComparatorMatchResult
MatchNumericComparator(JSContext *cx, const Value &v)
{
    if (!v.isObject())
        return Match_None;

    JSObject &obj = v.toObject();
    if (!obj.is<JSFunction>())
        return Match_None;

    JSFunction *fun = &obj.as<JSFunction>();
    if (!fun->isInterpreted())
        return Match_None;

    JSScript *script = fun->getOrCreateScript(cx);
    if (!script)
        return Match_Failure;

    jsbytecode *pc = script->code();

    uint16_t arg0, arg1;
    if (JSOp(*pc) != JSOP_GETARG)
        return Match_None;
    arg0 = GET_ARGNO(pc);
    pc += JSOP_GETARG_LENGTH;

    if (JSOp(*pc) != JSOP_GETARG)
        return Match_None;
    arg1 = GET_ARGNO(pc);
    pc += JSOP_GETARG_LENGTH;

    if (JSOp(*pc) != JSOP_SUB)
        return Match_None;
    pc += JSOP_SUB_LENGTH;

    if (JSOp(*pc) != JSOP_RETURN)
        return Match_None;

    if (arg0 == 0 && arg1 == 1)
        return Match_LeftMinusRight;

    if (arg0 == 1 && arg1 == 0)
        return Match_RightMinusLeft;

    return Match_None;
}

template<typename K, typename C>
static inline bool
MergeSortByKey(K keys, size_t len, K scratch, C comparator, AutoValueVector *vec)
{
    MOZ_ASSERT(vec->length() >= len);

    /* Sort keys. */
    if (!MergeSort(keys, len, scratch, comparator))
        return false;

    /*
     * Reorder vec by keys in-place, going element by element.  When an out-of-
     * place element is encountered, move that element to its proper position,
     * displacing whatever element was at *that* point to its proper position,
     * and so on until an element must be moved to the current position.
     *
     * At each outer iteration all elements up to |i| are sorted.  If
     * necessary each inner iteration moves some number of unsorted elements
     * (including |i|) directly to sorted position.  Thus on completion |*vec|
     * is sorted, and out-of-position elements have moved once.  Complexity is
     * Θ(len) + O(len) == O(2*len), with each element visited at most twice.
     */
    for (size_t i = 0; i < len; i++) {
        size_t j = keys[i].elementIndex;
        if (i == j)
            continue; // fixed point

        MOZ_ASSERT(j > i, "Everything less than |i| should be in the right place!");
        Value tv = (*vec)[j];
        do {
            size_t k = keys[j].elementIndex;
            keys[j].elementIndex = j;
            (*vec)[j].set((*vec)[k]);
            j = k;
        } while (j != i);

        // We could assert the loop invariant that |i == keys[i].elementIndex|
        // here if we synced |keys[i].elementIndex|.  But doing so would render
        // the assertion vacuous, so don't bother, even in debug builds.
        (*vec)[i].set(tv);
    }

    return true;
}

/*
 * Sort Values as strings.
 *
 * To minimize #conversions, SortLexicographically() first converts all Values
 * to strings at once, then sorts the elements by these cached strings.
 */
bool
SortLexicographically(JSContext *cx, AutoValueVector *vec, size_t len)
{
    JS_ASSERT(vec->length() >= len);

    StringBuffer sb(cx);
    Vector<StringifiedElement, 0, TempAllocPolicy> strElements(cx);

    /* MergeSort uses the upper half as scratch space. */
    if (!strElements.reserve(2 * len))
        return false;

    /* Convert Values to strings. */
    size_t cursor = 0;
    for (size_t i = 0; i < len; i++) {
        if (!CheckForInterrupt(cx))
            return false;

        if (!ValueToStringBuffer(cx, (*vec)[i], sb))
            return false;

        StringifiedElement el = { cursor, sb.length(), i };
        strElements.infallibleAppend(el);
        cursor = sb.length();
    }

    /* Resize strElements so we can perform MergeSort. */
    JS_ALWAYS_TRUE(strElements.resize(2 * len));

    /* Sort Values in vec alphabetically. */
    return MergeSortByKey(strElements.begin(), len, strElements.begin() + len,
                          SortComparatorStringifiedElements(cx, sb), vec);
}

/*
 * Sort Values as numbers.
 *
 * To minimize #conversions, SortNumerically first converts all Values to
 * numerics at once, then sorts the elements by these cached numerics.
 */
bool
SortNumerically(JSContext *cx, AutoValueVector *vec, size_t len, ComparatorMatchResult comp)
{
    JS_ASSERT(vec->length() >= len);

    Vector<NumericElement, 0, TempAllocPolicy> numElements(cx);

    /* MergeSort uses the upper half as scratch space. */
    if (!numElements.reserve(2 * len))
        return false;

    /* Convert Values to numerics. */
    for (size_t i = 0; i < len; i++) {
        if (!CheckForInterrupt(cx))
            return false;

        double dv;
        if (!ToNumber(cx, (*vec)[i], &dv))
            return false;

        NumericElement el = { dv, i };
        numElements.infallibleAppend(el);
    }

    /* Resize strElements so we can perform MergeSort. */
    JS_ALWAYS_TRUE(numElements.resize(2 * len));

    /* Sort Values in vec numerically. */
    return MergeSortByKey(numElements.begin(), len, numElements.begin() + len,
                          SortComparatorNumerics[comp], vec);
}

} /* namespace anonymous */

bool
js::array_sort(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedValue fvalRoot(cx);
    Value &fval = fvalRoot.get();

    if (args.hasDefined(0)) {
        if (args[0].isPrimitive()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_SORT_ARG);
            return false;
        }
        fval = args[0];     /* non-default compare function */
    } else {
        fval.setNull();
    }

    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
        return false;
    if (len < 2) {
        /* [] and [a] remain unchanged when sorted. */
        args.rval().setObject(*obj);
        return true;
    }

    /*
     * We need a temporary array of 2 * len Value to hold the array elements
     * and the scratch space for merge sort. Check that its size does not
     * overflow size_t, which would allow for indexing beyond the end of the
     * malloc'd vector.
     */
#if JS_BITS_PER_WORD == 32
    if (size_t(len) > size_t(-1) / (2 * sizeof(Value))) {
        js_ReportAllocationOverflow(cx);
        return false;
    }
#endif

    /*
     * Initialize vec as a root. We will clear elements of vec one by
     * one while increasing the rooted amount of vec when we know that the
     * property at the corresponding index exists and its value must be rooted.
     *
     * In this way when sorting a huge mostly sparse array we will not
     * access the tail of vec corresponding to properties that do not
     * exist, allowing OS to avoiding committing RAM. See bug 330812.
     */
    size_t n, undefs;
    {
        AutoValueVector vec(cx);
        if (!vec.reserve(2 * size_t(len)))
            return false;

        /*
         * By ECMA 262, 15.4.4.11, a property that does not exist (which we
         * call a "hole") is always greater than an existing property with
         * value undefined and that is always greater than any other property.
         * Thus to sort holes and undefs we simply count them, sort the rest
         * of elements, append undefs after them and then make holes after
         * undefs.
         */
        undefs = 0;
        bool allStrings = true;
        bool allInts = true;
        RootedValue v(cx);
        for (uint32_t i = 0; i < len; i++) {
            if (!CheckForInterrupt(cx))
                return false;

            /* Clear vec[newlen] before including it in the rooted set. */
            bool hole;
            if (!GetElement(cx, obj, i, &hole, &v))
                return false;
            if (hole)
                continue;
            if (v.isUndefined()) {
                ++undefs;
                continue;
            }
            vec.infallibleAppend(v);
            allStrings = allStrings && v.isString();
            allInts = allInts && v.isInt32();
        }


        /*
         * If the array only contains holes, we're done.  But if it contains
         * undefs, those must be sorted to the front of the array.
         */
        n = vec.length();
        if (n == 0 && undefs == 0) {
            args.rval().setObject(*obj);
            return true;
        }

        /* Here len == n + undefs + number_of_holes. */
        if (fval.isNull()) {
            /*
             * Sort using the default comparator converting all elements to
             * strings.
             */
            if (allStrings) {
                JS_ALWAYS_TRUE(vec.resize(n * 2));
                if (!MergeSort(vec.begin(), n, vec.begin() + n, SortComparatorStrings(cx)))
                    return false;
            } else if (allInts) {
                JS_ALWAYS_TRUE(vec.resize(n * 2));
                if (!MergeSort(vec.begin(), n, vec.begin() + n,
                               SortComparatorLexicographicInt32())) {
                    return false;
                }
            } else {
                if (!SortLexicographically(cx, &vec, n))
                    return false;
            }
        } else {
            ComparatorMatchResult comp = MatchNumericComparator(cx, fval);
            if (comp == Match_Failure)
                return false;

            if (comp != Match_None) {
                if (allInts) {
                    JS_ALWAYS_TRUE(vec.resize(n * 2));
                    if (!MergeSort(vec.begin(), n, vec.begin() + n, SortComparatorInt32s[comp]))
                        return false;
                } else {
                    if (!SortNumerically(cx, &vec, n, comp))
                        return false;
                }
            } else {
                FastInvokeGuard fig(cx, fval);
                MOZ_ASSERT(!InParallelSection(),
                           "Array.sort() can't currently be used from parallel code");
                JS_ALWAYS_TRUE(vec.resize(n * 2));
                if (!MergeSort(vec.begin(), n, vec.begin() + n,
                               SortComparatorFunction(cx, fval, fig)))
                {
                    return false;
                }
            }
        }

        if (!InitArrayElements(cx, obj, 0, uint32_t(n), vec.begin(), DontUpdateTypes))
            return false;
    }

    /* Set undefs that sorted after the rest of elements. */
    while (undefs != 0) {
        --undefs;
        if (!CheckForInterrupt(cx) || !SetArrayElement(cx, obj, n++, UndefinedHandleValue))
            return false;
    }

    /* Re-create any holes that sorted to the end of the array. */
    while (len > n) {
        if (!CheckForInterrupt(cx) || !DeletePropertyOrThrow(cx, obj, --len))
            return false;
    }
    args.rval().setObject(*obj);
    return true;
}

bool
js::NewbornArrayPush(JSContext *cx, HandleObject obj, const Value &v)
{
    Rooted<ArrayObject*> arr(cx, &obj->as<ArrayObject>());

    JS_ASSERT(!v.isMagic());
    JS_ASSERT(arr->lengthIsWritable());

    uint32_t length = arr->length();
    JS_ASSERT(length <= arr->getDenseCapacity());

    if (!arr->ensureElements(cx, length + 1))
        return false;

    arr->setDenseInitializedLength(length + 1);
    arr->setLengthInt32(length + 1);
    arr->initDenseElementWithType(cx, length, v);
    return true;
}

/* ES5 15.4.4.7 */
bool
js::array_push(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    /* Steps 2-3. */
    uint32_t length;
    if (!GetLengthProperty(cx, obj, &length))
        return false;

    /* Fast path for native objects with dense elements. */
    do {
        if (!obj->isNative() || obj->is<TypedArrayObject>())
            break;

        if (obj->is<ArrayObject>() && !obj->as<ArrayObject>().lengthIsWritable())
            break;

        if (ObjectMayHaveExtraIndexedProperties(obj))
            break;

        uint32_t argCount = args.length();
        JSObject::EnsureDenseResult result = obj->ensureDenseElements(cx, length, argCount);
        if (result == JSObject::ED_FAILED)
            return false;

        if (result == JSObject::ED_OK) {
            for (uint32_t i = 0, index = length; i < argCount; index++, i++)
                obj->setDenseElementWithType(cx, index, args[i]);
            uint32_t newlength = length + argCount;
            args.rval().setNumber(newlength);
            if (obj->is<ArrayObject>()) {
                obj->as<ArrayObject>().setLengthInt32(newlength);
                return true;
            }
            return SetLengthProperty(cx, obj, newlength);
        }

        MOZ_ASSERT(result == JSObject::ED_SPARSE);
    } while (false);

    /* Steps 4-5. */
    if (!InitArrayElements(cx, obj, length, args.length(), args.array(), UpdateTypes))
        return false;

    /* Steps 6-7. */
    double newlength = length + double(args.length());
    args.rval().setNumber(newlength);
    return SetLengthProperty(cx, obj, newlength);
}

/* ES6 20130308 draft 15.4.4.6. */
bool
js::array_pop(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    /* Steps 2-3. */
    uint32_t index;
    if (!GetLengthProperty(cx, obj, &index))
        return false;

    /* Steps 4-5. */
    if (index == 0) {
        /* Step 4b. */
        args.rval().setUndefined();
    } else {
        /* Step 5a. */
        index--;

        /* Step 5b, 5e. */
        bool hole;
        if (!GetElement(cx, obj, index, &hole, args.rval()))
            return false;

        /* Step 5c. */
        if (!hole && !DeletePropertyOrThrow(cx, obj, index))
            return false;
    }

    // If this was an array, then there are no elements above the one we just
    // deleted (if we deleted an element).  Thus we can shrink the dense
    // initialized length accordingly.  (This is fine even if the array length
    // is non-writable: length-changing occurs after element-deletion effects.)
    // Don't do anything if this isn't an array, as any deletion above has no
    // effect on any elements after the "last" one indicated by the "length"
    // property.
    if (obj->is<ArrayObject>() && obj->getDenseInitializedLength() > index)
        obj->setDenseInitializedLength(index);

    /* Steps 4a, 5d. */
    return SetLengthProperty(cx, obj, index);
}

void
js::ArrayShiftMoveElements(JSObject *obj)
{
    JS_ASSERT(obj->is<ArrayObject>());
    JS_ASSERT(obj->as<ArrayObject>().lengthIsWritable());

    /*
     * At this point the length and initialized length have already been
     * decremented and the result fetched, so just shift the array elements
     * themselves.
     */
    uint32_t initlen = obj->getDenseInitializedLength();
    obj->moveDenseElementsNoPreBarrier(0, 1, initlen);
}

/* ES5 15.4.4.9 */
bool
js::array_shift(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    /* Steps 2-3. */
    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 4. */
    if (len == 0) {
        /* Step 4a. */
        if (!SetLengthProperty(cx, obj, 0))
            return false;

        /* Step 4b. */
        args.rval().setUndefined();
        return true;
    }

    uint32_t newlen = len - 1;

    /* Fast paths. */
    if (obj->is<ArrayObject>() &&
        obj->getDenseInitializedLength() > 0 &&
        newlen < obj->getDenseCapacity() &&
        !ObjectMayHaveExtraIndexedProperties(obj))
    {
        args.rval().set(obj->getDenseElement(0));
        if (args.rval().isMagic(JS_ELEMENTS_HOLE))
            args.rval().setUndefined();

        obj->moveDenseElements(0, 1, obj->getDenseInitializedLength() - 1);
        obj->setDenseInitializedLength(obj->getDenseInitializedLength() - 1);

        if (!SetLengthProperty(cx, obj, newlen))
            return false;

        return js_SuppressDeletedProperty(cx, obj, INT_TO_JSID(newlen));
    }

    /* Steps 5, 10. */
    bool hole;
    if (!GetElement(cx, obj, uint32_t(0), &hole, args.rval()))
        return false;

    /* Steps 6-7. */
    RootedValue value(cx);
    for (uint32_t i = 0; i < newlen; i++) {
        if (!CheckForInterrupt(cx))
            return false;
        if (!GetElement(cx, obj, i + 1, &hole, &value))
            return false;
        if (hole) {
            if (!DeletePropertyOrThrow(cx, obj, i))
                return false;
        } else {
            if (!SetArrayElement(cx, obj, i, value))
                return false;
        }
    }

    /* Step 8. */
    if (!DeletePropertyOrThrow(cx, obj, newlen))
        return false;

    /* Step 9. */
    return SetLengthProperty(cx, obj, newlen);
}

static bool
array_unshift(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    uint32_t length;
    if (!GetLengthProperty(cx, obj, &length))
        return false;

    double newlen = length;
    if (args.length() > 0) {
        /* Slide up the array to make room for all args at the bottom. */
        if (length > 0) {
            bool optimized = false;
            do {
                if (!obj->is<ArrayObject>())
                    break;
                if (ObjectMayHaveExtraIndexedProperties(obj))
                    break;
                if (!obj->as<ArrayObject>().lengthIsWritable())
                    break;
                JSObject::EnsureDenseResult result = obj->ensureDenseElements(cx, length, args.length());
                if (result != JSObject::ED_OK) {
                    if (result == JSObject::ED_FAILED)
                        return false;
                    JS_ASSERT(result == JSObject::ED_SPARSE);
                    break;
                }
                obj->moveDenseElements(args.length(), 0, length);
                for (uint32_t i = 0; i < args.length(); i++)
                    obj->setDenseElement(i, MagicValue(JS_ELEMENTS_HOLE));
                optimized = true;
            } while (false);

            if (!optimized) {
                double last = length;
                double upperIndex = last + args.length();
                RootedValue value(cx);
                do {
                    --last, --upperIndex;
                    bool hole;
                    if (!CheckForInterrupt(cx))
                        return false;
                    if (!GetElement(cx, obj, last, &hole, &value))
                        return false;
                    if (hole) {
                        if (!DeletePropertyOrThrow(cx, obj, upperIndex))
                            return false;
                    } else {
                        if (!SetArrayElement(cx, obj, upperIndex, value))
                            return false;
                    }
                } while (last != 0);
            }
        }

        /* Copy from args to the bottom of the array. */
        if (!InitArrayElements(cx, obj, 0, args.length(), args.array(), UpdateTypes))
            return false;

        newlen += args.length();
    }
    if (!SetLengthProperty(cx, obj, newlen))
        return false;

    /* Follow Perl by returning the new array length. */
    args.rval().setNumber(newlen);
    return true;
}

static inline void
TryReuseArrayType(JSObject *obj, ArrayObject *narr)
{
    /*
     * Try to change the type of a newly created array narr to the same type
     * as obj. This can only be performed if the original object is an array
     * and has the same prototype.
     */
    JS_ASSERT(narr->getProto()->hasNewType(&ArrayObject::class_, narr->type()));

    if (obj->is<ArrayObject>() && !obj->hasSingletonType() && obj->getProto() == narr->getProto())
        narr->setType(obj->type());
}

/*
 * Returns true if this is a dense array whose |count| properties starting from
 * |startingIndex| may be accessed (get, set, delete) directly through its
 * contiguous vector of elements without fear of getters, setters, etc. along
 * the prototype chain, or of enumerators requiring notification of
 * modifications.
 */
static inline bool
CanOptimizeForDenseStorage(HandleObject arr, uint32_t startingIndex, uint32_t count, JSContext *cx)
{
    /* If the desired properties overflow dense storage, we can't optimize. */
    if (UINT32_MAX - startingIndex < count)
        return false;

    /* There's no optimizing possible if it's not an array. */
    if (!arr->is<ArrayObject>())
        return false;

    /*
     * Don't optimize if the array might be in the midst of iteration.  We
     * rely on this to be able to safely move dense array elements around with
     * just a memmove (see JSObject::moveDenseArrayElements), without worrying
     * about updating any in-progress enumerators for properties implicitly
     * deleted if a hole is moved from one location to another location not yet
     * visited.  See bug 690622.
     *
     * Another potential wrinkle: what if the enumeration is happening on an
     * object which merely has |arr| on its prototype chain?  It turns out this
     * case can't happen, because any dense array used as the prototype of
     * another object is first slowified, for type inference's sake.
     */
    types::TypeObject *arrType = arr->getType(cx);
    if (MOZ_UNLIKELY(!arrType || arrType->hasAllFlags(OBJECT_FLAG_ITERATED)))
        return false;

    /*
     * Now watch out for getters and setters along the prototype chain or in
     * other indexed properties on the object.  (Note that non-writable length
     * is subsumed by the initializedLength comparison.)
     */
    return !ObjectMayHaveExtraIndexedProperties(arr) &&
           startingIndex + count <= arr->getDenseInitializedLength();
}

/* ES5 15.4.4.12. */
bool
js::array_splice(JSContext *cx, unsigned argc, Value *vp)
{
    return array_splice_impl(cx, argc, vp, true);
}

bool
js::array_splice_impl(JSContext *cx, unsigned argc, Value *vp, bool returnValueIsUsed)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    /* Steps 3-4. */
    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 5. */
    double relativeStart;
    if (!ToInteger(cx, args.get(0), &relativeStart))
        return false;

    /* Step 6. */
    uint32_t actualStart;
    if (relativeStart < 0)
        actualStart = Max(len + relativeStart, 0.0);
    else
        actualStart = Min(relativeStart, double(len));

    /* Step 7. */
    uint32_t actualDeleteCount;
    if (args.length() != 1) {
        double deleteCountDouble;
        RootedValue cnt(cx, args.length() >= 2 ? args[1] : Int32Value(0));
        if (!ToInteger(cx, cnt, &deleteCountDouble))
            return false;
        actualDeleteCount = Min(Max(deleteCountDouble, 0.0), double(len - actualStart));
    } else {
        /*
         * Non-standard: if start was specified but deleteCount was omitted,
         * delete to the end of the array.  See bug 668024 for discussion.
         */
        actualDeleteCount = len - actualStart;
    }

    JS_ASSERT(len - actualStart >= actualDeleteCount);

    /* Steps 2, 8-9. */
    Rooted<ArrayObject*> arr(cx);
    if (CanOptimizeForDenseStorage(obj, actualStart, actualDeleteCount, cx)) {
        if (returnValueIsUsed) {
            arr = NewDenseCopiedArray(cx, actualDeleteCount, obj, actualStart);
            if (!arr)
                return false;
            TryReuseArrayType(obj, arr);
        }
    } else {
        arr = NewDenseAllocatedArray(cx, actualDeleteCount);
        if (!arr)
            return false;
        TryReuseArrayType(obj, arr);

        RootedValue fromValue(cx);
        for (uint32_t k = 0; k < actualDeleteCount; k++) {
            bool hole;
            if (!CheckForInterrupt(cx) ||
                !GetElement(cx, obj, actualStart + k, &hole, &fromValue) ||
                (!hole && !JSObject::defineElement(cx, arr, k, fromValue)))
            {
                return false;
            }
        }
    }

    /* Step 11. */
    uint32_t itemCount = (args.length() >= 2) ? (args.length() - 2) : 0;

    if (itemCount < actualDeleteCount) {
        /* Step 12: the array is being shrunk. */
        uint32_t sourceIndex = actualStart + actualDeleteCount;
        uint32_t targetIndex = actualStart + itemCount;
        uint32_t finalLength = len - actualDeleteCount + itemCount;

        if (CanOptimizeForDenseStorage(obj, 0, len, cx)) {
            /* Steps 12(a)-(b). */
            obj->moveDenseElements(targetIndex, sourceIndex, len - sourceIndex);

            /*
             * Update the initialized length. Do so before shrinking so that we
             * can apply the write barrier to the old slots.
             */
            obj->setDenseInitializedLength(finalLength);

            /* Steps 12(c)-(d). */
            obj->shrinkElements(cx, finalLength);
        } else {
            /*
             * This is all very slow if the length is very large. We don't yet
             * have the ability to iterate in sorted order, so we just do the
             * pessimistic thing and let CheckForInterrupt handle the
             * fallout.
             */

            /* Steps 12(a)-(b). */
            RootedValue fromValue(cx);
            for (uint32_t from = sourceIndex, to = targetIndex; from < len; from++, to++) {
                if (!CheckForInterrupt(cx))
                    return false;

                bool hole;
                if (!GetElement(cx, obj, from, &hole, &fromValue))
                    return false;
                if (hole) {
                    if (!DeletePropertyOrThrow(cx, obj, to))
                        return false;
                } else {
                    if (!SetArrayElement(cx, obj, to, fromValue))
                        return false;
                }
            }

            /* Steps 12(c)-(d). */
            for (uint32_t k = len; k > finalLength; k--) {
                if (!DeletePropertyOrThrow(cx, obj, k - 1))
                    return false;
            }
        }
    } else if (itemCount > actualDeleteCount) {
        /* Step 13. */

        /*
         * Optimize only if the array is already dense and we can extend it to
         * its new length.  It would be wrong to extend the elements here for a
         * number of reasons.
         *
         * First, this could cause us to fall into the fast-path below.  This
         * would cause elements to be moved into places past the non-writable
         * length.  And when the dense initialized length is updated, that'll
         * cause the |in| operator to think that those elements actually exist,
         * even though, properly, setting them must fail.
         *
         * Second, extending the elements here will trigger assertions inside
         * ensureDenseElements that the elements aren't being extended past the
         * length of a non-writable array.  This is because extending elements
         * will extend capacity -- which might extend them past a non-writable
         * length, violating the |capacity <= length| invariant for such
         * arrays.  And that would make the various JITted fast-path method
         * implementations of [].push, [].unshift, and so on wrong.
         *
         * If the array length is non-writable, this method *will* throw.  For
         * simplicity, have the slow-path code do it.  (Also note that the slow
         * path may validly *not* throw -- if all the elements being moved are
         * holes.)
         */
        if (obj->is<ArrayObject>()) {
            Rooted<ArrayObject*> arr(cx, &obj->as<ArrayObject>());
            if (arr->lengthIsWritable()) {
                JSObject::EnsureDenseResult res =
                    arr->ensureDenseElements(cx, arr->length(), itemCount - actualDeleteCount);
                if (res == JSObject::ED_FAILED)
                    return false;
            }
        }

        if (CanOptimizeForDenseStorage(obj, len, itemCount - actualDeleteCount, cx)) {
            obj->moveDenseElements(actualStart + itemCount,
                                   actualStart + actualDeleteCount,
                                   len - (actualStart + actualDeleteCount));
            obj->setDenseInitializedLength(len + itemCount - actualDeleteCount);
        } else {
            RootedValue fromValue(cx);
            for (double k = len - actualDeleteCount; k > actualStart; k--) {
                if (!CheckForInterrupt(cx))
                    return false;

                double from = k + actualDeleteCount - 1;
                double to = k + itemCount - 1;

                bool hole;
                if (!GetElement(cx, obj, from, &hole, &fromValue))
                    return false;

                if (hole) {
                    if (!DeletePropertyOrThrow(cx, obj, to))
                        return false;
                } else {
                    if (!SetArrayElement(cx, obj, to, fromValue))
                        return false;
                }
            }
        }
    }

    /* Step 10. */
    Value *items = args.array() + 2;

    /* Steps 14-15. */
    for (uint32_t k = actualStart, i = 0; i < itemCount; i++, k++) {
        if (!SetArrayElement(cx, obj, k, HandleValue::fromMarkedLocation(&items[i])))
            return false;
    }

    /* Step 16. */
    double finalLength = double(len) - actualDeleteCount + itemCount;
    if (!SetLengthProperty(cx, obj, finalLength))
        return false;

    /* Step 17. */
    if (returnValueIsUsed)
        args.rval().setObject(*arr);

    return true;
}

bool
js::array_concat_dense(JSContext *cx, Handle<ArrayObject*> arr1, Handle<ArrayObject*> arr2,
                       Handle<ArrayObject*> result)
{
    uint32_t initlen1 = arr1->getDenseInitializedLength();
    JS_ASSERT(initlen1 == arr1->length());

    uint32_t initlen2 = arr2->getDenseInitializedLength();
    JS_ASSERT(initlen2 == arr2->length());

    /* No overflow here due to nelements limit. */
    uint32_t len = initlen1 + initlen2;

    if (!result->ensureElements(cx, len))
        return false;

    JS_ASSERT(!result->getDenseInitializedLength());
    result->setDenseInitializedLength(len);

    result->initDenseElements(0, arr1->getDenseElements(), initlen1);
    result->initDenseElements(initlen1, arr2->getDenseElements(), initlen2);
    result->setLengthInt32(len);
    return true;
}

/*
 * Python-esque sequence operations.
 */
bool
js::array_concat(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Treat our |this| object as the first argument; see ECMA 15.4.4.4. */
    Value *p = args.array() - 1;

    /* Create a new Array object and root it using *vp. */
    RootedObject aobj(cx, ToObject(cx, args.thisv()));
    if (!aobj)
        return false;

    Rooted<ArrayObject*> narr(cx);
    uint32_t length;
    if (aobj->is<ArrayObject>() && !aobj->isIndexed()) {
        length = aobj->as<ArrayObject>().length();
        uint32_t initlen = aobj->getDenseInitializedLength();
        narr = NewDenseCopiedArray(cx, initlen, aobj, 0);
        if (!narr)
            return false;
        TryReuseArrayType(aobj, narr);
        narr->setLength(cx, length);
        args.rval().setObject(*narr);
        if (argc == 0)
            return true;
        argc--;
        p++;
    } else {
        narr = NewDenseEmptyArray(cx);
        if (!narr)
            return false;
        args.rval().setObject(*narr);
        length = 0;
    }

    /* Loop over [0, argc] to concat args into narr, expanding all Arrays. */
    for (unsigned i = 0; i <= argc; i++) {
        if (!CheckForInterrupt(cx))
            return false;
        HandleValue v = HandleValue::fromMarkedLocation(&p[i]);
        if (v.isObject()) {
            RootedObject obj(cx, &v.toObject());
            if (ObjectClassIs(obj, ESClass_Array, cx)) {
                uint32_t alength;
                if (!GetLengthProperty(cx, obj, &alength))
                    return false;
                RootedValue tmp(cx);
                for (uint32_t slot = 0; slot < alength; slot++) {
                    bool hole;
                    if (!CheckForInterrupt(cx) || !GetElement(cx, obj, slot, &hole, &tmp))
                        return false;

                    /*
                     * Per ECMA 262, 15.4.4.4, step 9, ignore nonexistent
                     * properties.
                     */
                    if (!hole && !SetArrayElement(cx, narr, length + slot, tmp))
                        return false;
                }
                length += alength;
                continue;
            }
        }

        if (!SetArrayElement(cx, narr, length, v))
            return false;
        length++;
    }

    return SetLengthProperty(cx, narr, length);
}

static bool
array_slice(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    uint32_t length;
    if (!GetLengthProperty(cx, obj, &length))
        return false;

    uint32_t begin = 0;
    uint32_t end = length;
    if (args.length() > 0) {
        double d;
        if (!ToInteger(cx, args[0], &d))
            return false;
        if (d < 0) {
            d += length;
            if (d < 0)
                d = 0;
        } else if (d > length) {
            d = length;
        }
        begin = (uint32_t)d;

        if (args.hasDefined(1)) {
            if (!ToInteger(cx, args[1], &d))
                return false;
            if (d < 0) {
                d += length;
                if (d < 0)
                    d = 0;
            } else if (d > length) {
                d = length;
            }
            end = (uint32_t)d;
        }
    }

    if (begin > end)
        begin = end;

    Rooted<ArrayObject*> narr(cx);
    narr = NewDenseAllocatedArray(cx, end - begin);
    if (!narr)
        return false;
    TryReuseArrayType(obj, narr);

    if (obj->is<ArrayObject>() && !ObjectMayHaveExtraIndexedProperties(obj)) {
        if (obj->getDenseInitializedLength() > begin) {
            uint32_t numSourceElements = obj->getDenseInitializedLength() - begin;
            uint32_t initLength = Min(numSourceElements, end - begin);
            narr->setDenseInitializedLength(initLength);
            narr->initDenseElements(0, &obj->getDenseElement(begin), initLength);
        }
        args.rval().setObject(*narr);
        return true;
    }

    if (js::SliceOp op = obj->getOps()->slice) {
        // Ensure that we have dense elements, so that DOM can use js::UnsafeDefineElement.
        JSObject::EnsureDenseResult result = narr->ensureDenseElements(cx, 0, end - begin);
        if (result == JSObject::ED_FAILED)
             return false;

        if (result == JSObject::ED_OK) {
            if (!op(cx, obj, begin, end, narr))
                return false;

            args.rval().setObject(*narr);
            return true;
        }

        // Fallthrough
        JS_ASSERT(result == JSObject::ED_SPARSE);
    }


    if (!SliceSlowly(cx, obj, obj, begin, end, narr))
        return false;

    args.rval().setObject(*narr);
    return true;
}

JS_FRIEND_API(bool)
js::SliceSlowly(JSContext* cx, HandleObject obj, HandleObject receiver,
                uint32_t begin, uint32_t end, HandleObject result)
{
    RootedValue value(cx);
    for (uint32_t slot = begin; slot < end; slot++) {
        bool hole;
        if (!CheckForInterrupt(cx) ||
            !GetElement(cx, obj, receiver, slot, &hole, &value))
        {
            return false;
        }
        if (!hole && !JSObject::defineElement(cx, result, slot - begin, value))
            return false;
    }
    return true;
}

/* ES5 15.4.4.20. */
static bool
array_filter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    /* Step 2-3. */
    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 4. */
    if (args.length() == 0) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return false;
    }
    RootedObject callable(cx, ValueToCallable(cx, args[0], args.length() - 1));
    if (!callable)
        return false;

    /* Step 5. */
    RootedValue thisv(cx, args.length() >= 2 ? args[1] : UndefinedValue());

    /* Step 6. */
    RootedObject arr(cx, NewDenseAllocatedArray(cx, 0));
    if (!arr)
        return false;
    TypeObject *newtype = GetTypeCallerInitObject(cx, JSProto_Array);
    if (!newtype)
        return false;
    arr->setType(newtype);

    /* Step 7. */
    uint32_t k = 0;

    /* Step 8. */
    uint32_t to = 0;

    /* Step 9. */
    JS_ASSERT(!InParallelSection());
    FastInvokeGuard fig(cx, ObjectValue(*callable));
    InvokeArgs &args2 = fig.args();
    RootedValue kValue(cx);
    while (k < len) {
        if (!CheckForInterrupt(cx))
            return false;

        /* Step a, b, and c.i. */
        bool kNotPresent;
        if (!GetElement(cx, obj, k, &kNotPresent, &kValue))
            return false;

        /* Step c.ii-iii. */
        if (!kNotPresent) {
            if (!args2.init(3))
                return false;
            args2.setCallee(ObjectValue(*callable));
            args2.setThis(thisv);
            args2[0].set(kValue);
            args2[1].setNumber(k);
            args2[2].setObject(*obj);
            if (!fig.invoke(cx))
                return false;

            if (ToBoolean(args2.rval())) {
                if (!SetArrayElement(cx, arr, to, kValue))
                    return false;
                to++;
            }
        }

        /* Step d. */
        k++;
    }

    /* Step 10. */
    args.rval().setObject(*arr);
    return true;
}

static bool
array_isArray(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    bool isArray = args.length() > 0 && IsObjectWithClass(args[0], ESClass_Array, cx);
    args.rval().setBoolean(isArray);
    return true;
}

static bool
IsArrayConstructor(const Value &v)
{
    // This must only return true if v is *the* Array constructor for the
    // current compartment; we rely on the fact that any other Array
    // constructor would be represented as a wrapper.
    return v.isObject() &&
           v.toObject().is<JSFunction>() &&
           v.toObject().as<JSFunction>().isNative() &&
           v.toObject().as<JSFunction>().native() == js_Array;
}

static bool
ArrayFromCallArgs(JSContext *cx, RootedTypeObject &type, CallArgs &args)
{
    if (!InitArrayTypes(cx, type, args.array(), args.length()))
        return false;
    JSObject *obj = (args.length() == 0)
        ? NewDenseEmptyArray(cx)
        : NewDenseCopiedArray(cx, args.length(), args.array());
    if (!obj)
        return false;
    obj->setType(type);
    args.rval().setObject(*obj);
    return true;
}

static bool
array_of(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (IsArrayConstructor(args.thisv()) || !IsConstructor(args.thisv())) {
        // IsArrayConstructor(this) will usually be true in practice. This is
        // the most common path.
        RootedTypeObject type(cx, GetTypeCallerInitObject(cx, JSProto_Array));
        if (!type)
            return false;
        return ArrayFromCallArgs(cx, type, args);
    }

    // Step 4.
    RootedObject obj(cx);
    {
        RootedValue v(cx);
        Value argv[1] = {NumberValue(args.length())};
        if (!InvokeConstructor(cx, args.thisv(), 1, argv, v.address()))
            return false;
        obj = ToObject(cx, v);
        if (!obj)
            return false;
    }

    // Step 8.
    for (unsigned k = 0; k < args.length(); k++) {
        if (!JSObject::defineElement(cx, obj, k, args[k]))
            return false;
    }

    // Steps 9-10.
    RootedValue v(cx, NumberValue(args.length()));
    if (!JSObject::setProperty(cx, obj, obj, cx->names().length, &v, true))
        return false;

    // Step 11.
    args.rval().setObject(*obj);
    return true;
}

#define GENERIC JSFUN_GENERIC_NATIVE

static const JSFunctionSpec array_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,      array_toSource,     0,0),
#endif
    JS_FN(js_toString_str,      array_toString,     0,0),
    JS_FN(js_toLocaleString_str,array_toLocaleString,0,0),

    /* Perl-ish methods. */
    JS_FN("join",               array_join,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("reverse",            array_reverse,      0,JSFUN_GENERIC_NATIVE),
    JS_FN("sort",               array_sort,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("push",               array_push,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("pop",                array_pop,          0,JSFUN_GENERIC_NATIVE),
    JS_FN("shift",              array_shift,        0,JSFUN_GENERIC_NATIVE),
    JS_FN("unshift",            array_unshift,      1,JSFUN_GENERIC_NATIVE),
    JS_FN("splice",             array_splice,       2,JSFUN_GENERIC_NATIVE),

    /* Pythonic sequence methods. */
    JS_FN("concat",             array_concat,       1,JSFUN_GENERIC_NATIVE),
    JS_FN("slice",              array_slice,        2,JSFUN_GENERIC_NATIVE),

    JS_SELF_HOSTED_FN("lastIndexOf", "ArrayLastIndexOf", 1,0),
    JS_SELF_HOSTED_FN("indexOf",     "ArrayIndexOf",     1,0),
    JS_SELF_HOSTED_FN("forEach",     "ArrayForEach",     1,0),
    JS_SELF_HOSTED_FN("map",         "ArrayMap",         1,0),
    JS_SELF_HOSTED_FN("reduce",      "ArrayReduce",      1,0),
    JS_SELF_HOSTED_FN("reduceRight", "ArrayReduceRight", 1,0),
    JS_FN("filter",             array_filter,       1,JSFUN_GENERIC_NATIVE),
    JS_SELF_HOSTED_FN("some",        "ArraySome",        1,0),
    JS_SELF_HOSTED_FN("every",       "ArrayEvery",       1,0),

#ifdef ENABLE_PARALLEL_JS
    /* Parallelizable and pure methods. */
    JS_SELF_HOSTED_FN("mapPar",      "ArrayMapPar",      2,0),
    JS_SELF_HOSTED_FN("reducePar",   "ArrayReducePar",   2,0),
    JS_SELF_HOSTED_FN("scanPar",     "ArrayScanPar",     2,0),
    JS_SELF_HOSTED_FN("scatterPar",  "ArrayScatterPar",  5,0),
    JS_SELF_HOSTED_FN("filterPar",   "ArrayFilterPar",   2,0),
#endif

    /* ES6 additions */
    JS_SELF_HOSTED_FN("find",        "ArrayFind",        1,0),
    JS_SELF_HOSTED_FN("findIndex",   "ArrayFindIndex",   1,0),
    JS_SELF_HOSTED_FN("copyWithin",  "ArrayCopyWithin",  3,0),

    JS_SELF_HOSTED_FN("fill",        "ArrayFill",        3,0),

    JS_SELF_HOSTED_FN("@@iterator",  "ArrayValues",      0,0),
    JS_SELF_HOSTED_FN("entries",     "ArrayEntries",     0,0),
    JS_SELF_HOSTED_FN("keys",        "ArrayKeys",        0,0),
    JS_FS_END
};

static const JSFunctionSpec array_static_methods[] = {
    JS_FN("isArray",            array_isArray,      1,0),
    JS_SELF_HOSTED_FN("lastIndexOf", "ArrayStaticLastIndexOf", 2,0),
    JS_SELF_HOSTED_FN("indexOf",     "ArrayStaticIndexOf", 2,0),
    JS_SELF_HOSTED_FN("forEach",     "ArrayStaticForEach", 2,0),
    JS_SELF_HOSTED_FN("map",         "ArrayStaticMap",   2,0),
    JS_SELF_HOSTED_FN("every",       "ArrayStaticEvery", 2,0),
    JS_SELF_HOSTED_FN("some",        "ArrayStaticSome",  2,0),
    JS_SELF_HOSTED_FN("reduce",      "ArrayStaticReduce", 2,0),
    JS_SELF_HOSTED_FN("reduceRight", "ArrayStaticReduceRight", 2,0),
    JS_SELF_HOSTED_FN("from",        "ArrayFrom", 3,0),
    JS_FN("of",                 array_of,           0,0),

#ifdef ENABLE_PARALLEL_JS
    JS_SELF_HOSTED_FN("build",       "ArrayStaticBuild", 2,0),
    /* Parallelizable and pure static methods. */
    JS_SELF_HOSTED_FN("buildPar",    "ArrayStaticBuildPar", 3,0),
#endif

    JS_FS_END
};

/* ES5 15.4.2 */
bool
js_Array(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedTypeObject type(cx, GetTypeCallerInitObject(cx, JSProto_Array));
    if (!type)
        return false;

    if (args.length() != 1 || !args[0].isNumber())
        return ArrayFromCallArgs(cx, type, args);

    uint32_t length;
    if (args[0].isInt32()) {
        int32_t i = args[0].toInt32();
        if (i < 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_ARRAY_LENGTH);
            return false;
        }
        length = uint32_t(i);
    } else {
        double d = args[0].toDouble();
        length = ToUint32(d);
        if (d != double(length)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_ARRAY_LENGTH);
            return false;
        }
    }

    /*
     * Allocate dense elements eagerly for small arrays, to avoid reallocating
     * elements when filling the array.
     */
    bool allocateArray = length <= ArrayObject::EagerAllocationMaxLength;
    RootedObject obj(cx, NewDenseArray(cx, length, type, allocateArray));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static JSObject *
CreateArrayPrototype(JSContext *cx, JSProtoKey key)
{
    JS_ASSERT(key == JSProto_Array);
    RootedObject proto(cx, cx->global()->getOrCreateObjectPrototype(cx));
    if (!proto)
        return nullptr;

    RootedTypeObject type(cx, cx->getNewType(&ArrayObject::class_, TaggedProto(proto)));
    if (!type)
        return nullptr;

    JSObject *metadata = nullptr;
    if (!NewObjectMetadata(cx, &metadata))
        return nullptr;

    RootedShape shape(cx, EmptyShape::getInitialShape(cx, &ArrayObject::class_, TaggedProto(proto),
                                                      proto->getParent(), metadata,
                                                      gc::FINALIZE_OBJECT0));
    if (!shape)
        return nullptr;

    RootedObject arrayProto(cx, JSObject::createArray(cx, gc::FINALIZE_OBJECT4, gc::TenuredHeap, shape, type, 0));
    if (!arrayProto || !JSObject::setSingletonType(cx, arrayProto) || !AddLengthProperty(cx, arrayProto))
        return nullptr;

    /*
     * The default 'new' type of Array.prototype is required by type inference
     * to have unknown properties, to simplify handling of e.g. heterogenous
     * arrays in JSON and script literals and allows setDenseArrayElement to
     * be used without updating the indexed type set for such default arrays.
     */
    if (!JSObject::setNewTypeUnknown(cx, &ArrayObject::class_, arrayProto))
        return nullptr;

    return arrayProto;
}

const Class ArrayObject::class_ = {
    "Array",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Array),
    array_addProperty,
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    nullptr,        /* trace       */
    {
        GenericCreateConstructor<js_Array, 1, JSFunction::FinalizeKind>,
        CreateArrayPrototype,
        array_static_methods,
        array_methods
    }
};

/*
 * Array allocation functions.
 */

static inline bool
EnsureNewArrayElements(ExclusiveContext *cx, JSObject *obj, uint32_t length)
{
    /*
     * If ensureElements creates dynamically allocated slots, then having
     * fixedSlots is a waste.
     */
    DebugOnly<uint32_t> cap = obj->getDenseCapacity();

    if (!obj->ensureElements(cx, length))
        return false;

    JS_ASSERT_IF(cap, !obj->hasDynamicElements());

    return true;
}

template<bool allocateCapacity>
static MOZ_ALWAYS_INLINE ArrayObject *
NewArray(ExclusiveContext *cxArg, uint32_t length,
         JSObject *protoArg, NewObjectKind newKind = GenericObject)
{
    gc::AllocKind allocKind = GuessArrayGCKind(length);
    JS_ASSERT(CanBeFinalizedInBackground(allocKind, &ArrayObject::class_));
    allocKind = GetBackgroundAllocKind(allocKind);

    NewObjectCache::EntryIndex entry = -1;
    if (JSContext *cx = cxArg->maybeJSContext()) {
        NewObjectCache &cache = cx->runtime()->newObjectCache;
        if (newKind == GenericObject &&
            !cx->compartment()->hasObjectMetadataCallback() &&
            cache.lookupGlobal(&ArrayObject::class_, cx->global(), allocKind, &entry))
        {
            gc::InitialHeap heap = GetInitialHeap(newKind, &ArrayObject::class_);
            JSObject *obj = cache.newObjectFromHit<NoGC>(cx, entry, heap);
            if (obj) {
                /* Fixup the elements pointer and length, which may be incorrect. */
                ArrayObject *arr = &obj->as<ArrayObject>();
                arr->setFixedElements();
                arr->setLength(cx, length);
                if (allocateCapacity && !EnsureNewArrayElements(cx, arr, length))
                    return nullptr;
                return arr;
            } else {
                RootedObject proto(cxArg, protoArg);
                obj = cache.newObjectFromHit<CanGC>(cx, entry, heap);
                JS_ASSERT(!obj);
                protoArg = proto;
            }
        }
    }

    RootedObject proto(cxArg, protoArg);
    if (protoArg)
        JS::PoisonPtr(&protoArg);

    if (!proto && !GetBuiltinPrototype(cxArg, JSProto_Array, &proto))
        return nullptr;

    RootedTypeObject type(cxArg, cxArg->getNewType(&ArrayObject::class_, TaggedProto(proto)));
    if (!type)
        return nullptr;

    JSObject *metadata = nullptr;
    if (!NewObjectMetadata(cxArg, &metadata))
        return nullptr;

    /*
     * Get a shape with zero fixed slots, regardless of the size class.
     * See JSObject::createArray.
     */
    RootedShape shape(cxArg, EmptyShape::getInitialShape(cxArg, &ArrayObject::class_,
                                                         TaggedProto(proto), cxArg->global(),
                                                         metadata, gc::FINALIZE_OBJECT0));
    if (!shape)
        return nullptr;

    Rooted<ArrayObject*> arr(cxArg, JSObject::createArray(cxArg, allocKind,
                                                          GetInitialHeap(newKind, &ArrayObject::class_),
                                                          shape, type, length));
    if (!arr)
        return nullptr;

    if (shape->isEmptyShape()) {
        if (!AddLengthProperty(cxArg, arr))
            return nullptr;
        shape = arr->lastProperty();
        EmptyShape::insertInitialShape(cxArg, shape, proto);
    }

    if (newKind == SingletonObject && !JSObject::setSingletonType(cxArg, arr))
        return nullptr;

    if (entry != -1) {
        cxArg->asJSContext()->runtime()->newObjectCache.fillGlobal(entry, &ArrayObject::class_,
                                                                   cxArg->global(), allocKind, arr);
    }

    if (allocateCapacity && !EnsureNewArrayElements(cxArg, arr, length))
        return nullptr;

    probes::CreateObject(cxArg, arr);
    return arr;
}

ArrayObject * JS_FASTCALL
js::NewDenseEmptyArray(JSContext *cx, JSObject *proto /* = nullptr */,
                       NewObjectKind newKind /* = GenericObject */)
{
    return NewArray<false>(cx, 0, proto, newKind);
}

ArrayObject * JS_FASTCALL
js::NewDenseAllocatedArray(ExclusiveContext *cx, uint32_t length, JSObject *proto /* = nullptr */,
                           NewObjectKind newKind /* = GenericObject */)
{
    return NewArray<true>(cx, length, proto, newKind);
}

ArrayObject * JS_FASTCALL
js::NewDenseUnallocatedArray(ExclusiveContext *cx, uint32_t length, JSObject *proto /* = nullptr */,
                             NewObjectKind newKind /* = GenericObject */)
{
    return NewArray<false>(cx, length, proto, newKind);
}

ArrayObject * JS_FASTCALL
js::NewDenseArray(ExclusiveContext *cx, uint32_t length, HandleTypeObject type, bool allocateArray)
{
    NewObjectKind newKind = !type ? SingletonObject : GenericObject;
    if (type && type->shouldPreTenure())
        newKind = TenuredObject;

    // Allocate dense elements eagerly for small arrays, to avoid reallocating
    // elements when filling the array.
    ArrayObject *arr = allocateArray
                       ? NewDenseAllocatedArray(cx, length, nullptr, newKind)
                       : NewDenseUnallocatedArray(cx, length, nullptr, newKind);

    if (!arr)
        return nullptr;

    if (type)
        arr->setType(type);

    // If the length calculation overflowed, make sure that is marked for the
    // new type.
    if (arr->length() > INT32_MAX)
        arr->setLength(cx, arr->length());

    return arr;
}

ArrayObject *
js::NewDenseCopiedArray(JSContext *cx, uint32_t length, HandleObject src, uint32_t elementOffset,
                        JSObject *proto /* = nullptr */)
{
    JS_ASSERT(!src->isIndexed());

    ArrayObject* arr = NewArray<true>(cx, length, proto);
    if (!arr)
        return nullptr;

    JS_ASSERT(arr->getDenseCapacity() >= length);

    const Value* vp = src->getDenseElements() + elementOffset;
    arr->setDenseInitializedLength(vp ? length : 0);

    if (vp)
        arr->initDenseElements(0, vp, length);

    return arr;
}

// values must point at already-rooted Value objects
ArrayObject *
js::NewDenseCopiedArray(JSContext *cx, uint32_t length, const Value *values,
                        JSObject *proto /* = nullptr */, NewObjectKind newKind /* = GenericObject */)
{
    ArrayObject* arr = NewArray<true>(cx, length, proto);
    if (!arr)
        return nullptr;

    JS_ASSERT(arr->getDenseCapacity() >= length);

    arr->setDenseInitializedLength(values ? length : 0);

    if (values)
        arr->initDenseElements(0, values, length);

    return arr;
}

ArrayObject *
js::NewDenseAllocatedArrayWithTemplate(JSContext *cx, uint32_t length, JSObject *templateObject)
{
    gc::AllocKind allocKind = GuessArrayGCKind(length);
    JS_ASSERT(CanBeFinalizedInBackground(allocKind, &ArrayObject::class_));
    allocKind = GetBackgroundAllocKind(allocKind);

    RootedTypeObject type(cx, templateObject->type());
    if (!type)
        return nullptr;

    RootedShape shape(cx, templateObject->lastProperty());
    if (!shape)
        return nullptr;

    gc::InitialHeap heap = GetInitialHeap(GenericObject, &ArrayObject::class_);
    Rooted<ArrayObject *> arr(cx, JSObject::createArray(cx, allocKind, heap, shape, type, length));
    if (!arr)
        return nullptr;

    if (!EnsureNewArrayElements(cx, arr, length))
        return nullptr;

    probes::CreateObject(cx, arr);

    return arr;
}

#ifdef DEBUG
bool
js_ArrayInfo(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj;

    for (unsigned i = 0; i < args.length(); i++) {
        RootedValue arg(cx, args[i]);

        char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, arg, NullPtr());
        if (!bytes)
            return false;
        if (arg.isPrimitive() ||
            !(obj = arg.toObjectOrNull())->is<ArrayObject>()) {
            fprintf(stderr, "%s: not array\n", bytes);
            js_free(bytes);
            continue;
        }
        fprintf(stderr, "%s: (len %u", bytes, obj->as<ArrayObject>().length());
        fprintf(stderr, ", capacity %u", obj->getDenseCapacity());
        fputs(")\n", stderr);
        js_free(bytes);
    }

    args.rval().setUndefined();
    return true;
}
#endif
