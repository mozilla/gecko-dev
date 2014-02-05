/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ErrorObject_h_
#define vm_ErrorObject_h_

#include "jsobj.h"

#include "vm/Shape.h"

struct JSExnPrivate;

/*
 * Initialize the exception constructor/prototype hierarchy.
 */
extern JSObject *
js_InitExceptionClasses(JSContext *cx, JS::HandleObject obj);

namespace js {

class ErrorObject : public JSObject
{
    static ErrorObject *
    createProto(JSContext *cx, JS::Handle<GlobalObject*> global, JSExnType type,
                JS::HandleObject proto);

    /* For access to createProto. */
    friend JSObject *
    ::js_InitExceptionClasses(JSContext *cx, JS::HandleObject global);

    /* For access to assignInitialShape. */
    friend bool
    EmptyShape::ensureInitialCustomShape<ErrorObject>(ExclusiveContext *cx,
                                                      Handle<ErrorObject*> obj);

    /*
     * Assign the initial error shape to the empty object.  (This shape does
     * *not* include .message, which must be added separately if needed; see
     * ErrorObject::init.)
     */
    static Shape *
    assignInitialShape(ExclusiveContext *cx, Handle<ErrorObject*> obj);

    static bool
    init(JSContext *cx, Handle<ErrorObject*> obj, JSExnType type,
         ScopedJSFreePtr<JSErrorReport> *errorReport, HandleString fileName, HandleString stack,
         uint32_t lineNumber, uint32_t columnNumber, HandleString message);

  protected:
    static const uint32_t EXNTYPE_SLOT      = 0;
    static const uint32_t ERROR_REPORT_SLOT = EXNTYPE_SLOT + 1;
    static const uint32_t FILENAME_SLOT     = ERROR_REPORT_SLOT + 1;
    static const uint32_t LINENUMBER_SLOT   = FILENAME_SLOT + 1;
    static const uint32_t COLUMNNUMBER_SLOT = LINENUMBER_SLOT + 1;
    static const uint32_t STACK_SLOT        = COLUMNNUMBER_SLOT + 1;
    static const uint32_t MESSAGE_SLOT      = STACK_SLOT + 1;

    static const uint32_t RESERVED_SLOTS = MESSAGE_SLOT + 1;

  public:
    static const Class class_;

    // Create an error of the given type corresponding to the provided location
    // info.  If |message| is non-null, then the error will have a .message
    // property with that value; otherwise the error will have no .message
    // property.
    static ErrorObject *
    create(JSContext *cx, JSExnType type, HandleString stack, HandleString fileName,
           uint32_t lineNumber, uint32_t columnNumber, ScopedJSFreePtr<JSErrorReport> *report,
           HandleString message);

    JSExnType type() const {
        return JSExnType(getReservedSlot(EXNTYPE_SLOT).toInt32());
    }

    JSErrorReport * getErrorReport() const {
        const Value &slot = getReservedSlot(ERROR_REPORT_SLOT);
        if (slot.isUndefined())
            return nullptr;
        return static_cast<JSErrorReport*>(slot.toPrivate());
    }

    JSErrorReport * getOrCreateErrorReport(JSContext *cx);

    JSString * fileName() const {
        return getReservedSlot(FILENAME_SLOT).toString();
    }

    uint32_t lineNumber() const {
        return getReservedSlot(LINENUMBER_SLOT).toInt32();
    }

    uint32_t columnNumber() const {
        return getReservedSlot(COLUMNNUMBER_SLOT).toInt32();
    }

    JSString * stack() const {
        return getReservedSlot(STACK_SLOT).toString();
    }

    JSString * getMessage() const {
        HeapSlot &slot = const_cast<ErrorObject*>(this)->getReservedSlotRef(MESSAGE_SLOT);
        return slot.isString() ? slot.toString() : nullptr;
    }
};

} // namespace js

#endif // vm_ErrorObject_h_
