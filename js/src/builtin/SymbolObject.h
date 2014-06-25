/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_SymbolObject_h
#define builtin_SymbolObject_h

#include "jsobj.h"

#include "vm/Symbol.h"

namespace js {

class SymbolObject : public JSObject
{
    /* Stores this Symbol object's [[PrimitiveValue]]. */
    static const unsigned PRIMITIVE_VALUE_SLOT = 0;

  public:
    static const unsigned RESERVED_SLOTS = 1;

    static const Class class_;

    static JSObject *initClass(JSContext *cx, js::HandleObject obj);

    /*
     * Creates a new Symbol object boxing the given primitive Symbol.  The
     * object's [[Prototype]] is determined from context.
     */
    static SymbolObject *create(JSContext *cx, JS::Symbol *symbol);

    JS::Symbol *unbox() const {
        return getFixedSlot(PRIMITIVE_VALUE_SLOT).toSymbol();
    }

  private:
    inline void setPrimitiveValue(JS::Symbol *symbol) {
        setFixedSlot(PRIMITIVE_VALUE_SLOT, SymbolValue(symbol));
    }

    static bool construct(JSContext *cx, unsigned argc, Value *vp);

    static bool convert(JSContext *cx, HandleObject obj, JSType type, MutableHandleValue vp);

    // Static methods.
    static bool for_(JSContext *cx, unsigned argc, Value *vp);
    static bool keyFor(JSContext *cx, unsigned argc, Value *vp);

    // Methods defined on Symbol.prototype.
    static bool toString_impl(JSContext *cx, CallArgs args);
    static bool toString(JSContext *cx, unsigned argc, Value *vp);
    static bool valueOf_impl(JSContext *cx, CallArgs args);
    static bool valueOf(JSContext *cx, unsigned argc, Value *vp);

    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];
    static const JSFunctionSpec staticMethods[];
};

} /* namespace js */

extern JSObject *
js_InitSymbolClass(JSContext *cx, js::HandleObject obj);

#endif /* builtin_SymbolObject_h */
