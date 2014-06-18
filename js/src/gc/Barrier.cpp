/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Barrier.h"

#include "jscompartment.h"
#include "jsobj.h"

#include "gc/Zone.h"

namespace js {

void
ValueReadBarrier(const Value &value)
{
    JS_ASSERT(!CurrentThreadIsIonCompiling());
    if (value.isObject())
        JSObject::readBarrier(&value.toObject());
    else if (value.isString())
        JSString::readBarrier(value.toString());
    else
        JS_ASSERT(!value.isMarkable());
}

#ifdef DEBUG
bool
HeapSlot::preconditionForSet(JSObject *owner, Kind kind, uint32_t slot)
{
    return kind == Slot
         ? &owner->getSlotRef(slot) == this
         : &owner->getDenseElement(slot) == (const Value *)this;
}

bool
HeapSlot::preconditionForSet(Zone *zone, JSObject *owner, Kind kind, uint32_t slot)
{
    bool ok = kind == Slot
            ? &owner->getSlotRef(slot) == this
            : &owner->getDenseElement(slot) == (const Value *)this;
    return ok && owner->zone() == zone;
}

bool
HeapSlot::preconditionForWriteBarrierPost(JSObject *obj, Kind kind, uint32_t slot, Value target) const
{
    return kind == Slot
         ? obj->getSlotAddressUnchecked(slot)->get() == target
         : static_cast<HeapSlot *>(obj->getDenseElements() + slot)->get() == target;
}

bool
RuntimeFromMainThreadIsHeapMajorCollecting(JS::shadow::Zone *shadowZone)
{
    return shadowZone->runtimeFromMainThread()->isHeapMajorCollecting();
}

bool
CurrentThreadIsIonCompiling()
{
    return TlsPerThreadData.get()->ionCompiling;
}
#endif // DEBUG

bool
StringIsPermanentAtom(JSString *str)
{
    return str->isPermanentAtom();
}

} // namespace js
