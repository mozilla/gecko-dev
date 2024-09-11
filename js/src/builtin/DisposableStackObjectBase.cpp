/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/DisposableStackObjectBase.h"

#include "builtin/Array.h"
#include "vm/ArrayObject.h"
#include "vm/JSObject.h"

using namespace js;

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
