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
[[nodiscard]] static bool IsOptimizableInitForMapOrSet(
    JSContext* cx, JSNative addOrSetNative,
    Handle<NativeObject*> mapOrSetObject, Handle<Value> iterable,
    bool* optimized) {
  constexpr bool isMap = ProtoKey == JSProto_Map || ProtoKey == JSProto_WeakMap;
  constexpr bool isSet = ProtoKey == JSProto_Set || ProtoKey == JSProto_WeakSet;
  static_assert(isMap != isSet, "must be either a Map or a Set");

  MOZ_ASSERT(!*optimized);

  if (!iterable.isObject()) {
    return true;
  }

  if (!IsPackedArray(&iterable.toObject())) {
    return true;
  }
  Rooted<ArrayObject*> array(cx, &iterable.toObject().as<ArrayObject>());

  // For the Map and WeakMap constructors, ensure the elements are also packed
  // arrays with at least two elements (key and value).
  //
  // Limit this to relatively short arrays to avoid adding overhead for large
  // arrays in the worst case, when this check fails for one of the last
  // elements.
  if constexpr (isMap) {
    size_t len = array->length();
    static constexpr size_t MaxLength = 100;
    if (len > MaxLength) {
      return true;
    }
    for (size_t i = 0; i < len; i++) {
      Value elem = array->getDenseElement(i);
      if (!elem.isObject()) {
        return true;
      }
      JSObject* obj = &elem.toObject();
      if (!IsPackedArray(obj) || obj->as<ArrayObject>().length() < 2) {
        return true;
      }
    }
  }

  // Ensures mapOrSetObject's prototype is the canonical prototype.
  JSObject* proto = mapOrSetObject->staticPrototype();
  MOZ_ASSERT(proto);
  if (proto != cx->global()->maybeGetPrototype(ProtoKey)) {
    return true;
  }

  // Look up the 'add' or 'set' property on the prototype object.
  auto* nproto = &proto->as<NativeObject>();
  PropertyName* propName = isSet ? cx->names().add : cx->names().set;
  mozilla::Maybe<PropertyInfo> prop = nproto->lookup(cx, propName);
  if (prop.isNothing() || !prop->isDataProperty()) {
    return true;
  }

  // Get the property value and ensure it's the canonical 'add' or 'set'
  // function.
  Value propVal = nproto->getSlot(prop->slot());
  if (!IsNativeFunction(propVal, addOrSetNative)) {
    return true;
  }

  ForOfPIC::Chain* stubChain = ForOfPIC::getOrCreate(cx);
  if (!stubChain) {
    return false;
  }

  return stubChain->tryOptimizeArray(cx, array, optimized);
}

}  // namespace js

#endif /* builtin_MapObject_inl_h */
