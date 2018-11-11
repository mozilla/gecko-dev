/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ArrayObject_inl_h
#define vm_ArrayObject_inl_h

#include "vm/ArrayObject.h"

#include "gc/GCTrace.h"
#include "vm/String.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

#include "vm/TypeInference-inl.h"

namespace js {

inline void
ArrayObject::setLength(ExclusiveContext* cx, uint32_t length)
{
    MOZ_ASSERT(lengthIsWritable());
    MOZ_ASSERT_IF(length != getElementsHeader()->length, !denseElementsAreFrozen());

    if (length > INT32_MAX) {
        /* Track objects with overflowing lengths in type information. */
        MarkObjectGroupFlags(cx, this, OBJECT_FLAG_LENGTH_OVERFLOW);
    }

    getElementsHeader()->length = length;
}

/* static */ inline ArrayObject*
ArrayObject::createArrayInternal(ExclusiveContext* cx, gc::AllocKind kind, gc::InitialHeap heap,
                                 HandleShape shape, HandleObjectGroup group,
                                 AutoSetNewObjectMetadata&)
{
    // Create a new array and initialize everything except for its elements.
    MOZ_ASSERT(shape && group);
    MOZ_ASSERT(group->clasp() == shape->getObjectClass());
    MOZ_ASSERT(group->clasp() == &ArrayObject::class_);
    MOZ_ASSERT_IF(group->clasp()->hasFinalize(), heap == gc::TenuredHeap);
    MOZ_ASSERT_IF(group->hasUnanalyzedPreliminaryObjects(),
                  heap == js::gc::TenuredHeap);
    MOZ_ASSERT(group->clasp()->shouldDelayMetadataBuilder());

    // Arrays can use their fixed slots to store elements, so can't have shapes
    // which allow named properties to be stored in the fixed slots.
    MOZ_ASSERT(shape->numFixedSlots() == 0);

    size_t nDynamicSlots = dynamicSlotsCount(0, shape->slotSpan(), group->clasp());
    JSObject* obj = Allocate<JSObject>(cx, kind, nDynamicSlots, heap, group->clasp());
    if (!obj)
        return nullptr;

    static_cast<ArrayObject*>(obj)->shape_.init(shape);
    static_cast<ArrayObject*>(obj)->group_.init(group);

    cx->compartment()->setObjectPendingMetadata(cx, obj);
    return &obj->as<ArrayObject>();
}

/* static */ inline ArrayObject*
ArrayObject::finishCreateArray(ArrayObject* obj, HandleShape shape, AutoSetNewObjectMetadata& metadata)
{
    size_t span = shape->slotSpan();
    if (span)
        obj->initializeSlotRange(0, span);

    gc::TraceCreateObject(obj);

    return obj;
}

/* static */ inline ArrayObject*
ArrayObject::createArray(ExclusiveContext* cx, gc::AllocKind kind, gc::InitialHeap heap,
                         HandleShape shape, HandleObjectGroup group,
                         uint32_t length, AutoSetNewObjectMetadata& metadata)
{
    ArrayObject* obj = createArrayInternal(cx, kind, heap, shape, group, metadata);
    if (!obj)
        return nullptr;

    uint32_t capacity = gc::GetGCKindSlots(kind) - ObjectElements::VALUES_PER_HEADER;

    obj->setFixedElements();
    new (obj->getElementsHeader()) ObjectElements(capacity, length);

    return finishCreateArray(obj, shape, metadata);
}

/* static */ inline ArrayObject*
ArrayObject::createCopyOnWriteArray(ExclusiveContext* cx, gc::InitialHeap heap,
                                    HandleArrayObject sharedElementsOwner)
{
    MOZ_ASSERT(sharedElementsOwner->getElementsHeader()->isCopyOnWrite());
    MOZ_ASSERT(sharedElementsOwner->getElementsHeader()->ownerObject() == sharedElementsOwner);

    // Use the smallest allocation kind for the array, as it can't have any
    // fixed slots (see the assert in createArrayInternal) and will not be using
    // its fixed elements.
    gc::AllocKind kind = gc::AllocKind::OBJECT0_BACKGROUND;

    AutoSetNewObjectMetadata metadata(cx);
    RootedShape shape(cx, sharedElementsOwner->lastProperty());
    RootedObjectGroup group(cx, sharedElementsOwner->group());
    ArrayObject* obj = createArrayInternal(cx, kind, heap, shape, group, metadata);
    if (!obj)
        return nullptr;

    obj->elements_ = sharedElementsOwner->getDenseElementsAllowCopyOnWrite();

    return finishCreateArray(obj, shape, metadata);
}

} // namespace js

#endif // vm_ArrayObject_inl_h
