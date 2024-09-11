/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/DisposableStackObjectBase.h"

#include "builtin/Array.h"
#include "vm/ArrayObject.h"

#include "vm/Interpreter.h"
#include "vm/JSObject-inl.h"

using namespace js;

/**
 * Explicit Resource Management Proposal
 *
 * 27.4.3.1 AsyncDisposableStack.prototype.adopt ( value, onDisposeAsync )
 * https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-asyncdisposablestack.prototype.adopt
 * Step 5.a
 * 27.3.3.1 DisposableStack.prototype.adopt ( value, onDispose )
 * https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack.prototype.adopt
 * Step 5.a
 */
bool js::AdoptClosure(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);

  JS::Rooted<JSFunction*> callee(cx, &args.callee().as<JSFunction>());
  JS::Rooted<JS::Value> value(
      cx, callee->getExtendedSlot(AdoptClosureSlot_ValueSlot));
  JS::Rooted<JS::Value> onDispose(
      cx, callee->getExtendedSlot(AdoptClosureSlot_OnDisposeSlot));

  // Step 5.a. Return ? Call(onDispose, undefined, « value »).
  return Call(cx, onDispose, JS::UndefinedHandleValue, value, args.rval());
}

bool js::ThrowIfOnDisposeNotCallable(JSContext* cx,
                                     JS::Handle<JS::Value> onDispose) {
  if (IsCallable(onDispose)) {
    return true;
  }

  JS::UniqueChars bytes =
      DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, onDispose, nullptr);
  if (!bytes) {
    return false;
  }

  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_NOT_FUNCTION,
                           bytes.get());

  return false;
}

ArrayObject* DisposableStackObjectBase::getOrCreateDisposeCapability(
    JSContext* cx) {
  ArrayObject* disposablesList = nullptr;

  if (isDisposableResourceStackEmpty()) {
    disposablesList = NewDenseEmptyArray(cx);
    if (!disposablesList) {
      return nullptr;
    }
    setReservedSlot(DISPOSABLE_RESOURCE_STACK_SLOT,
                    ObjectValue(*disposablesList));
  } else {
    disposablesList = nonEmptyDisposableResourceStack();
  }

  return disposablesList;
}

bool DisposableStackObjectBase::isDisposableResourceStackEmpty() const {
  return getReservedSlot(DISPOSABLE_RESOURCE_STACK_SLOT).isUndefined();
}

void DisposableStackObjectBase::clearDisposableResourceStack() {
  setReservedSlot(DISPOSABLE_RESOURCE_STACK_SLOT, JS::UndefinedValue());
}

ArrayObject* DisposableStackObjectBase::nonEmptyDisposableResourceStack()
    const {
  MOZ_ASSERT(!isDisposableResourceStackEmpty());
  return &getReservedSlot(DISPOSABLE_RESOURCE_STACK_SLOT)
              .toObject()
              .as<ArrayObject>();
}

DisposableStackObjectBase::DisposableState DisposableStackObjectBase::state()
    const {
  return DisposableState(uint8_t(getReservedSlot(STATE_SLOT).toInt32()));
}

void DisposableStackObjectBase::setState(DisposableState state) {
  setReservedSlot(STATE_SLOT, JS::Int32Value(int32_t(state)));
}
