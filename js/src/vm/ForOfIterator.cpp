/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ForOfIterator.h"

#include "js/Exception.h"
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "vm/Interpreter.h"
#include "vm/Iteration.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"

#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"

using namespace js;
using JS::ForOfIterator;

bool ForOfIterator::init(HandleValue iterable,
                         NonIterableBehavior nonIterableBehavior) {
  JSContext* cx = cx_;
  RootedObject iterableObj(cx, ToObject(cx, iterable));
  if (!iterableObj) {
    return false;
  }

  MOZ_ASSERT(index == NOT_ARRAY);

  if (IsArrayWithDefaultIterator<MustBePacked::No>(iterableObj, cx)) {
    // Array is optimizable.
    index = 0;
    iterator = iterableObj;
    nextMethod.setUndefined();
    return true;
  }

  MOZ_ASSERT(index == NOT_ARRAY);

  RootedValue callee(cx);
  RootedId iteratorId(cx, PropertyKey::Symbol(cx->wellKnownSymbols().iterator));
  if (!GetProperty(cx, iterableObj, iterable, iteratorId, &callee)) {
    return false;
  }

  // If obj[@@iterator] is undefined and we were asked to allow non-iterables,
  // bail out now without setting iterator.  This will make valueIsIterable(),
  // which our caller should check, return false.
  if (nonIterableBehavior == AllowNonIterable && callee.isUndefined()) {
    return true;
  }

  // Throw if obj[@@iterator] isn't callable.
  // js::Invoke is about to check for this kind of error anyway, but it would
  // throw an inscrutable error message about |method| rather than this nice
  // one about |obj|.
  if (!callee.isObject() || !callee.toObject().isCallable()) {
    UniqueChars bytes =
        DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, iterable, nullptr);
    if (!bytes) {
      return false;
    }
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_NOT_ITERABLE,
                             bytes.get());
    return false;
  }

  RootedValue res(cx);
  if (!js::Call(cx, callee, iterable, &res)) {
    return false;
  }

  if (!res.isObject()) {
    return ThrowCheckIsObject(cx, CheckIsObjectKind::GetIterator);
  }

  RootedObject iteratorObj(cx, &res.toObject());
  if (!GetProperty(cx, iteratorObj, iteratorObj, cx->names().next, &res)) {
    return false;
  }

  iterator = iteratorObj;
  nextMethod = res;
  return true;
}

inline bool ForOfIterator::nextFromOptimizedArray(MutableHandleValue vp,
                                                  bool* done) {
  MOZ_ASSERT(index != NOT_ARRAY);

  if (!CheckForInterrupt(cx_)) {
    return false;
  }

  ArrayObject* arr = &iterator->as<ArrayObject>();

  if (index >= arr->length()) {
    vp.setUndefined();
    *done = true;
    return true;
  }
  *done = false;

  // Try to get array element via direct access.
  if (index < arr->getDenseInitializedLength()) {
    vp.set(arr->getDenseElement(index));
    if (!vp.isMagic(JS_ELEMENTS_HOLE)) {
      ++index;
      return true;
    }
  }

  return GetElement(cx_, iterator, iterator, index++, vp);
}

bool ForOfIterator::next(MutableHandleValue vp, bool* done) {
  MOZ_ASSERT(iterator);
  if (index != NOT_ARRAY) {
    return nextFromOptimizedArray(vp, done);
  }

  RootedValue v(cx_);
  if (!js::Call(cx_, nextMethod, iterator, &v)) {
    return false;
  }

  if (!v.isObject()) {
    return ThrowCheckIsObject(cx_, CheckIsObjectKind::IteratorNext);
  }

  RootedObject resultObj(cx_, &v.toObject());
  if (!GetProperty(cx_, resultObj, resultObj, cx_->names().done, &v)) {
    return false;
  }

  *done = ToBoolean(v);
  if (*done) {
    vp.setUndefined();
    return true;
  }

  return GetProperty(cx_, resultObj, resultObj, cx_->names().value, vp);
}

void ForOfIterator::closeThrow() {
  MOZ_ASSERT(iterator);

  // Don't handle uncatchable exceptions to match `for-of` bytecode behavior,
  // which also doesn't run IteratorClose when an interrupt was requested.
  if (!cx_->isExceptionPending()) {
    return;
  }

  // Save the current exception state. The destructor restores the saved
  // exception state, unless there's a new pending exception.
  JS::AutoSaveExceptionState savedExc(cx_);

  // Perform IteratorClose on the iterator.
  MOZ_ALWAYS_TRUE(CloseIterOperation(cx_, iterator, CompletionKind::Throw));

  // CloseIterOperation clears any pending exception.
  MOZ_ASSERT(!cx_->isExceptionPending());
}
