/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/ParseRecordObject.h"

#include "jsapi.h"  // JS_ValueToId, JS_IdToValue
#include "builtin/Object.h"
#include "vm/PlainObject.h"

#include "vm/JSObject-inl.h"  // NewBuiltinClassInstance

using namespace js;

// https://tc39.es/proposal-json-parse-with-source/#sec-json-parse-record

const JSClass ParseRecordObject::class_ = {
    "ParseRecordObject",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount),
};

/* static */
ParseRecordObject* ParseRecordObject::create(JSContext* cx, const Value& val) {
  Rooted<JSONParseNode*> parseNode(cx);
  return ParseRecordObject::create(cx, parseNode, val);
}

/* static */
ParseRecordObject* ParseRecordObject::create(JSContext* cx,
                                             Handle<JSONParseNode*> parseNode,
                                             const Value& val) {
  Rooted<ParseRecordObject*> obj(
      cx, NewObjectWithGivenProto<ParseRecordObject>(cx, nullptr));
  if (!obj) {
    return nullptr;
  }

  if (parseNode) {
    obj->initSlot(ParseNodeSlot, StringValue(parseNode));
  }
  obj->initSlot(ValueSlot, val);
  return obj;
}

JS::PropertyKey ParseRecordObject::getKey(JSContext* cx) const {
  Rooted<Value> slot(cx, getSlot(KeySlot));
  Rooted<JS::PropertyKey> key(cx);
  MOZ_ALWAYS_TRUE(JS_ValueToId(cx, slot, &key));
  return key;
};

bool ParseRecordObject::setKey(JSContext* cx, const JS::PropertyKey& key) {
  Rooted<Value> val(cx);
  if (!JS_IdToValue(cx, key, &val)) {
    return false;
  }
  setSlot(KeySlot, val);
  return true;
}

bool ParseRecordObject::setEntries(JSContext* cx, Handle<EntryMap*> entries) {
  Rooted<JS::IdVector> props(cx, IdVector(cx));
  Rooted<ParseRecordObject*> thisObj(cx, this);
  if (!JS_Enumerate(cx, entries, &props)) {
    return false;
  }
  Rooted<Value> prop(cx);
  for (uint32_t i = 0; i < props.length(); i++) {
    if (!JS_GetPropertyById(cx, entries, props[i], &prop)) {
      return false;
    }
    if (!JS_SetPropertyById(cx, thisObj, props[i], prop)) {
      return false;
    }
  }
  return true;
}

bool ParseRecordObject::getEntries(JSContext* cx,
                                   MutableHandle<EntryMap*> entries) {
  Rooted<JSObject*> thisObj(cx, this);
  int32_t length = 0;
  if (!obj_keys_length(cx, thisObj, length)) {
    return false;
  }
  MOZ_ASSERT(!entries.get());
  if (length) {
    entries.set(thisObj.get());
  }
  return true;
}
