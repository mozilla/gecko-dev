/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_MapObject_inl_h
#define builtin_MapObject_inl_h

#include "builtin/MapObject.h"

#include "vm/JSObject.h"
#include "vm/NativeObject.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

namespace js {

template <JSProtoKey ProtoKey>
[[nodiscard]] static bool IsOptimizableInitForSet(JSContext* cx,
                                                  JSNative addNative,
                                                  HandleObject setObject,
                                                  HandleValue iterable,
                                                  bool* optimized) {
  MOZ_ASSERT(!*optimized);

  if (!iterable.isObject()) {
    return true;
  }

  RootedObject array(cx, &iterable.toObject());
  if (!IsPackedArray(array)) {
    return true;
  }

  // Ensures setObject's prototype is the canonical prototype.
  JSObject* proto = setObject->staticPrototype();
  MOZ_ASSERT(proto);
  if (proto != cx->global()->maybeGetPrototype(ProtoKey)) {
    return true;
  }

  // Look up the 'add' value on the prototype object.
  auto* nproto = &proto->as<NativeObject>();
  mozilla::Maybe<PropertyInfo> addProp = nproto->lookup(cx, cx->names().add);
  if (addProp.isNothing() || !addProp->isDataProperty()) {
    return true;
  }

  // Get the referred value, ensure it holds the canonical add function.
  Value propVal = nproto->getSlot(addProp->slot());
  if (!IsNativeFunction(propVal, addNative)) {
    return true;
  }

  ForOfPIC::Chain* stubChain = ForOfPIC::getOrCreate(cx);
  if (!stubChain) {
    return false;
  }

  return stubChain->tryOptimizeArray(cx, array.as<ArrayObject>(), optimized);
}

}  // namespace js

#endif /* builtin_MapObject_inl_h */
