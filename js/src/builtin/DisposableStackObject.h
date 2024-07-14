/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_DisposableStackObject_h
#define builtin_DisposableStackObject_h

#include "vm/JSObject.h"
#include "vm/List.h"
#include "vm/NativeObject.h"

namespace js {

class DisposableStackObject : public NativeObject {
 public:
  enum DisposableState : uint8_t { Pending, Disposed };

  static const JSClass class_;
  static const JSClass protoClass_;

  static constexpr uint32_t DISPOSABLE_RESOURCE_STACK_SLOT = 0;
  static constexpr uint32_t STATE_SLOT = 1;

  static constexpr uint32_t RESERVED_SLOTS = 2;

  static DisposableStackObject* create(
      JSContext* cx, JS::Handle<JSObject*> proto,
      JS::Handle<JS::Value> initialDisposeCapability =
          JS::UndefinedHandleValue);

 private:
  static const ClassSpec classSpec_;
  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];

  ListObject* getOrCreateDisposeCapability(JSContext* cx);
  bool disposeResources(JSContext* cx);
  inline bool isDisposableResourceStackEmpty() const;
  inline void clearDisposableResourceStack();
  inline ListObject* nonEmptyDisposableResourceStack() const;
  inline DisposableState state() const;
  inline void setState(DisposableState state);

  static bool is(JS::Handle<JS::Value> val);
  static bool finishInit(JSContext* cx, JS::Handle<JSObject*> ctor,
                         JS::Handle<JSObject*> proto);

  static bool construct(JSContext* cx, unsigned argc, JS::Value* vp);
  static bool use_impl(JSContext* cx, const JS::CallArgs& args);
  static bool use(JSContext* cx, unsigned argc, JS::Value* vp);
  static bool dispose_impl(JSContext* cx, const JS::CallArgs& args);
  static bool dispose(JSContext* cx, unsigned argc, JS::Value* vp);
  static bool adopt_impl(JSContext* cx, const JS::CallArgs& args);
  static bool adopt(JSContext* cx, unsigned argc, JS::Value* vp);
  static bool defer_impl(JSContext* cx, const JS::CallArgs& args);
  static bool defer(JSContext* cx, unsigned argc, JS::Value* vp);
  static bool move_impl(JSContext* cx, const JS::CallArgs& args);
  static bool move(JSContext* cx, unsigned argc, JS::Value* vp);
  static bool disposed_impl(JSContext* cx, const JS::CallArgs& args);
  static bool disposed(JSContext* cx, unsigned argc, JS::Value* vp);
};

} /* namespace js */

#endif /* builtin_DisposableStackObject_h */
