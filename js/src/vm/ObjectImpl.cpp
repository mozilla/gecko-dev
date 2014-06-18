/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ObjectImpl-inl.h"

#include "gc/Marking.h"
#include "js/Value.h"
#include "vm/Debugger.h"

#include "jsobjinlines.h"
#include "vm/Shape-inl.h"

using namespace js;

using JS::GenericNaN;

PropDesc::PropDesc()
{
    setUndefined();
}

void
PropDesc::setUndefined()
{
    descObj_ = nullptr;
    value_ = UndefinedValue();
    get_ = UndefinedValue();
    set_ = UndefinedValue();
    attrs = 0;
    hasGet_ = false;
    hasSet_ = false;
    hasValue_ = false;
    hasWritable_ = false;
    hasEnumerable_ = false;
    hasConfigurable_ = false;

    isUndefined_ = true;
}


bool
PropDesc::checkGetter(JSContext *cx)
{
    if (hasGet_) {
        if (!IsCallable(get_) && !get_.isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_GET_SET_FIELD,
                                 js_getter_str);
            return false;
        }
    }
    return true;
}

bool
PropDesc::checkSetter(JSContext *cx)
{
    if (hasSet_) {
        if (!IsCallable(set_) && !set_.isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_GET_SET_FIELD,
                                 js_setter_str);
            return false;
        }
    }
    return true;
}

static const ObjectElements emptyElementsHeader(0, 0);

/* Objects with no elements share one empty set of elements. */
HeapSlot *const js::emptyObjectElements =
    reinterpret_cast<HeapSlot *>(uintptr_t(&emptyElementsHeader) + sizeof(ObjectElements));

#ifdef DEBUG

bool
ObjectImpl::canHaveNonEmptyElements()
{
    JSObject *obj = static_cast<JSObject *>(this);
    return isNative() && !obj->is<TypedArrayObject>();
}

#endif // DEBUG

/* static */ bool
ObjectElements::ConvertElementsToDoubles(JSContext *cx, uintptr_t elementsPtr)
{
    /*
     * This function is infallible, but has a fallible interface so that it can
     * be called directly from Ion code. Only arrays can have their dense
     * elements converted to doubles, and arrays never have empty elements.
     */
    HeapSlot *elementsHeapPtr = (HeapSlot *) elementsPtr;
    JS_ASSERT(elementsHeapPtr != emptyObjectElements);

    ObjectElements *header = ObjectElements::fromElements(elementsHeapPtr);
    JS_ASSERT(!header->shouldConvertDoubleElements());

    Value *vp = (Value *) elementsPtr;
    for (size_t i = 0; i < header->initializedLength; i++) {
        if (vp[i].isInt32())
            vp[i].setDouble(vp[i].toInt32());
    }

    header->setShouldConvertDoubleElements();
    return true;
}

#ifdef DEBUG
void
js::ObjectImpl::checkShapeConsistency()
{
    static int throttle = -1;
    if (throttle < 0) {
        if (const char *var = getenv("JS_CHECK_SHAPE_THROTTLE"))
            throttle = atoi(var);
        if (throttle < 0)
            throttle = 0;
    }
    if (throttle == 0)
        return;

    MOZ_ASSERT(isNative());

    Shape *shape = lastProperty();
    Shape *prev = nullptr;

    if (inDictionaryMode()) {
        MOZ_ASSERT(shape->hasTable());

        ShapeTable &table = shape->table();
        for (uint32_t fslot = table.freelist; fslot != SHAPE_INVALID_SLOT;
             fslot = getSlot(fslot).toPrivateUint32()) {
            MOZ_ASSERT(fslot < slotSpan());
        }

        for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
            MOZ_ASSERT_IF(lastProperty() != shape, !shape->hasTable());

            Shape **spp = table.search(shape->propid(), false);
            MOZ_ASSERT(SHAPE_FETCH(spp) == shape);
        }

        shape = lastProperty();
        for (int n = throttle; --n >= 0 && shape; shape = shape->parent) {
            MOZ_ASSERT_IF(shape->slot() != SHAPE_INVALID_SLOT, shape->slot() < slotSpan());
            if (!prev) {
                MOZ_ASSERT(lastProperty() == shape);
                MOZ_ASSERT(shape->listp == &shape_);
            } else {
                MOZ_ASSERT(shape->listp == &prev->parent);
            }
            prev = shape;
        }
    } else {
        for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
            if (shape->hasTable()) {
                ShapeTable &table = shape->table();
                MOZ_ASSERT(shape->parent);
                for (Shape::Range<NoGC> r(shape); !r.empty(); r.popFront()) {
                    Shape **spp = table.search(r.front().propid(), false);
                    MOZ_ASSERT(SHAPE_FETCH(spp) == &r.front());
                }
            }
            if (prev) {
                MOZ_ASSERT(prev->maybeSlot() >= shape->maybeSlot());
                shape->kids.checkConsistency(prev);
            }
            prev = shape;
        }
    }
}
#endif

void
js::ObjectImpl::initializeSlotRange(uint32_t start, uint32_t length)
{
    /*
     * No bounds check, as this is used when the object's shape does not
     * reflect its allocated slots (updateSlotsForSpan).
     */
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRangeUnchecked(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);

    uint32_t offset = start;
    for (HeapSlot *sp = fixedStart; sp < fixedEnd; sp++)
        sp->init(this->asObjectPtr(), HeapSlot::Slot, offset++, UndefinedValue());
    for (HeapSlot *sp = slotsStart; sp < slotsEnd; sp++)
        sp->init(this->asObjectPtr(), HeapSlot::Slot, offset++, UndefinedValue());
}

void
js::ObjectImpl::initSlotRange(uint32_t start, const Value *vector, uint32_t length)
{
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
    for (HeapSlot *sp = fixedStart; sp < fixedEnd; sp++)
        sp->init(this->asObjectPtr(), HeapSlot::Slot, start++, *vector++);
    for (HeapSlot *sp = slotsStart; sp < slotsEnd; sp++)
        sp->init(this->asObjectPtr(), HeapSlot::Slot, start++, *vector++);
}

void
js::ObjectImpl::copySlotRange(uint32_t start, const Value *vector, uint32_t length)
{
    JS::Zone *zone = this->zone();
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
    for (HeapSlot *sp = fixedStart; sp < fixedEnd; sp++)
        sp->set(zone, this->asObjectPtr(), HeapSlot::Slot, start++, *vector++);
    for (HeapSlot *sp = slotsStart; sp < slotsEnd; sp++)
        sp->set(zone, this->asObjectPtr(), HeapSlot::Slot, start++, *vector++);
}

#ifdef DEBUG
bool
js::ObjectImpl::isProxy() const
{
    return asObjectPtr()->is<ProxyObject>();
}

bool
js::ObjectImpl::slotInRange(uint32_t slot, SentinelAllowed sentinel) const
{
    uint32_t capacity = numFixedSlots() + numDynamicSlots();
    if (sentinel == SENTINEL_ALLOWED)
        return slot <= capacity;
    return slot < capacity;
}
#endif /* DEBUG */

// See bug 844580.
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1500
/*
 * Work around a compiler bug in MSVC9 and above, where inlining this function
 * causes stack pointer offsets to go awry and spp to refer to something higher
 * up the stack.
 */
MOZ_NEVER_INLINE
#endif
Shape *
js::ObjectImpl::nativeLookup(ExclusiveContext *cx, jsid id)
{
    MOZ_ASSERT(isNative());
    Shape **spp;
    return Shape::search(cx, lastProperty(), id, &spp);
}

#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

Shape *
js::ObjectImpl::nativeLookupPure(jsid id)
{
    MOZ_ASSERT(isNative());
    return Shape::searchNoHashify(lastProperty(), id);
}

uint32_t
js::ObjectImpl::dynamicSlotsCount(uint32_t nfixed, uint32_t span, const Class *clasp)
{
    if (span <= nfixed)
        return 0;
    span -= nfixed;

    // Increase the slots to SLOT_CAPACITY_MIN to decrease the likelihood
    // the dynamic slots need to get increased again. ArrayObjects ignore
    // this because slots are uncommon in that case.
    if (clasp != &ArrayObject::class_ && span <= SLOT_CAPACITY_MIN)
        return SLOT_CAPACITY_MIN;

    uint32_t slots = mozilla::RoundUpPow2(span);
    MOZ_ASSERT(slots >= span);
    return slots;
}

void
js::ObjectImpl::markChildren(JSTracer *trc)
{
    MarkTypeObject(trc, &type_, "type");

    MarkShape(trc, &shape_, "shape");

    const Class *clasp = type_->clasp();
    JSObject *obj = asObjectPtr();
    if (clasp->trace)
        clasp->trace(trc, obj);

    if (shape_->isNative()) {
        MarkObjectSlots(trc, obj, 0, obj->slotSpan());
        gc::MarkArraySlots(trc, obj->getDenseInitializedLength(), obj->getDenseElements(), "objectElements");
    }
}

void
PropDesc::trace(JSTracer *trc)
{
    if (descObj_)
        gc::MarkObjectRoot(trc, &descObj_, "PropDesc descriptor object");
    gc::MarkValueRoot(trc, &value_, "PropDesc value");
    gc::MarkValueRoot(trc, &get_, "PropDesc get");
    gc::MarkValueRoot(trc, &set_, "PropDesc set");
}
