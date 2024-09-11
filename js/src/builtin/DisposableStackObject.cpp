/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/DisposableStackObject.h"

#include "builtin/Array.h"
#include "builtin/DisposableStackObjectBase.h"
#include "js/friend/ErrorMessages.h"
#include "js/PropertyAndElement.h"
#include "js/PropertySpec.h"
#include "vm/BytecodeUtil.h"
#include "vm/DisposableRecord.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/UsingHint.h"

#include "vm/NativeObject-inl.h"

using namespace js;

/* static */ DisposableStackObject* DisposableStackObject::create(
    JSContext* cx, JS::Handle<JSObject*> proto,
    JS::Handle<JS::Value>
        initialDisposeCapability /* = JS::UndefinedHandleValue */) {
  DisposableStackObject* obj =
      NewObjectWithClassProto<DisposableStackObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  MOZ_ASSERT(initialDisposeCapability.isUndefined() ||
             initialDisposeCapability.isObject());
  MOZ_ASSERT_IF(initialDisposeCapability.isObject(),
                initialDisposeCapability.toObject().is<ArrayObject>());

  obj->initReservedSlot(DisposableStackObject::DISPOSABLE_RESOURCE_STACK_SLOT,
                        initialDisposeCapability);
  obj->initReservedSlot(
      DisposableStackObject::STATE_SLOT,
      JS::Int32Value(int32_t(DisposableStackObject::DisposableState::Pending)));

  return obj;
}

/**
 * Explicit Resource Management Proposal
 * 27.3.1.1 DisposableStack ( )
 * https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack
 */
/* static */ bool DisposableStackObject::construct(JSContext* cx, unsigned argc,
                                                   JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1. If NewTarget is undefined, throw a TypeError exception.
  if (!ThrowIfNotConstructing(cx, args, "DisposableStack")) {
    return false;
  }

  // Step 2. Let disposableStack be ? OrdinaryCreateFromConstructor(NewTarget,
  // "%DisposableStack.prototype%", « [[DisposableState]], [[DisposeCapability]]
  // »).
  // Step 3. Set disposableStack.[[DisposableState]] to pending.
  // Step 4. Set disposableStack.[[DisposeCapability]] to
  // NewDisposeCapability().
  JS::Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_DisposableStack,
                                          &proto)) {
    return false;
  }

  DisposableStackObject* obj = DisposableStackObject::create(cx, proto);
  if (!obj) {
    return false;
  }

  // Step 5. Return disposableStack.
  args.rval().setObject(*obj);
  return true;
}

/* static */ bool DisposableStackObject::is(JS::Handle<JS::Value> val) {
  return val.isObject() && val.toObject().is<DisposableStackObject>();
}

/**
 * Explicit Resource Management Proposal
 * 27.3.3.6 DisposableStack.prototype.use ( value )
 * https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack.prototype.use
 */
/* static */ bool DisposableStackObject::use_impl(JSContext* cx,
                                                  const JS::CallArgs& args) {
  // Step 1. Let disposableStack be the this value.
  JS::Rooted<DisposableStackObject*> disposableStack(
      cx, &args.thisv().toObject().as<DisposableStackObject>());

  // Step 2. Perform ? RequireInternalSlot(disposableStack,
  // [[DisposableState]]).
  // Step 3. If disposableStack.[[DisposableState]] is disposed, throw a
  // ReferenceError exception.
  if (disposableStack->state() == DisposableState::Disposed) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DISPOSABLE_STACK_DISPOSED);
    return false;
  }

  // Step 4. Perform ?
  // AddDisposableResource(disposableStack.[[DisposeCapability]], value,
  // sync-dispose).
  JS::Rooted<ArrayObject*> disposeCapability(
      cx, disposableStack->getOrCreateDisposeCapability(cx));
  if (!disposeCapability) {
    return false;
  }

  JS::Rooted<JS::Value> val(cx, args.get(0));
  if (!AddDisposableResource(cx, disposeCapability, val, UsingHint::Sync,
                             JS::NothingHandleValue)) {
    return false;
  }

  // Step 5. Return value.
  args.rval().set(val);
  return true;
}

/* static */ bool DisposableStackObject::use(JSContext* cx, unsigned argc,
                                             JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  return JS::CallNonGenericMethod<is, use_impl>(cx, args);
}

// Explicit Resource Management Proposal
// DisposeResources ( disposeCapability, completion )
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposeresources
//
// This implementation of DisposeResources is specifically for DisposableStack.
// This implements the subset of steps that specific for sync disposals.
//
// TODO: Consider unifying the implementation of DisposeResources by introducing
// a special call like syntax that directly generates bytecode (Bug 1917491).
template <typename ClearFn>
MOZ_ALWAYS_INLINE bool DisposeResources(
    JSContext* cx, JS::Handle<ArrayObject*> disposeCapability, ClearFn clear) {
  uint32_t index = disposeCapability->getDenseInitializedLength();

  // hadError and latestException correspond to the completion value.
  MOZ_ASSERT(!cx->isExceptionPending());
  bool hadError = false;
  JS::Rooted<JS::Value> latestException(cx);

  // Step 3. For each element resource of
  // disposeCapability.[[DisposableResourceStack]], in reverse list order, do
  while (index) {
    --index;
    JS::Value val = disposeCapability->getDenseElement(index);

    MOZ_ASSERT(val.isObject());

    JS::Rooted<DisposableRecordObject*> resource(
        cx, &val.toObject().as<DisposableRecordObject>());

    // Step 3.a. Let value be resource.[[ResourceValue]].
    JS::Rooted<JS::Value> value(cx, resource->getObject());

    // Step 3.b. Let hint be resource.[[Hint]].
    // Step 3.c. Let method be resource.[[DisposeMethod]].
    JS::Rooted<JS::Value> method(cx, resource->getMethod());

    // Step 3.e. If method is not undefined, then
    if (method.isUndefined()) {
      continue;
    }

    // Step 3.e.i. Let result be Completion(Call(method, value)).
    JS::Rooted<JS::Value> rval(cx);
    if (!Call(cx, method, value, &rval)) {
      // Step 3.e.iii. If result is a throw completion, then
      if (hadError) {
        // Step 3.e.iii.1.a. Set result to result.[[Value]].
        JS::Rooted<JS::Value> result(cx);
        if (!cx->getPendingException(&result)) {
          return false;
        }
        cx->clearPendingException();

        // Step 3.e.iii.1.b. Let suppressed be completion.[[Value]].
        JS::Rooted<JS::Value> suppressed(cx, latestException);

        // Steps 3.e.iii.1.c-e.
        ErrorObject* errorObj = CreateSuppressedError(cx, result, suppressed);
        if (!errorObj) {
          return false;
        }
        // Step 3.e.iii.1.f. Set completion to ThrowCompletion(error).
        latestException.set(ObjectValue(*errorObj));
      } else {
        // Step 3.e.iii.2. Else,
        // Step 3.e.iii.2.a. Set completion to result.
        hadError = true;
        if (cx->isExceptionPending()) {
          if (!cx->getPendingException(&latestException)) {
            return false;
          }
          cx->clearPendingException();
        }
      }
    }
  }

  // Step 6. Set disposeCapability.[[DisposableResourceStack]] to a new empty
  // List.
  clear();

  // Step 7. Return ? completion.
  if (hadError) {
    cx->setPendingException(latestException, ShouldCaptureStack::Maybe);
    return false;
  }

  return true;
}

// Explicit Resource Management Proposal
// 27.3.3.3 DisposableStack.prototype.dispose ( )
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack.prototype.dispose
// Step 4-5.
bool DisposableStackObject::disposeResources(JSContext* cx) {
  MOZ_ASSERT(state() == DisposableState::Pending);

  // Step 4. Set disposableStack.[[DisposableState]] to disposed.
  setState(DisposableState::Disposed);

  // Step 5. Return ? DisposeResources(disposableStack.[[DisposeCapability]],
  // NormalCompletion(undefined)).
  if (isDisposableResourceStackEmpty()) {
    return true;
  }

  JS::Rooted<ArrayObject*> disposeCapability(cx,
                                             nonEmptyDisposableResourceStack());

  auto clearFn = [&]() { clearDisposableResourceStack(); };

  return DisposeResources(cx, disposeCapability, clearFn);
}

/**
 * Explicit Resource Management Proposal
 * 27.3.3.3 DisposableStack.prototype.dispose ( )
 * https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack.prototype.dispose
 */
/* static */ bool DisposableStackObject::dispose_impl(
    JSContext* cx, const JS::CallArgs& args) {
  // Step 1. Let disposableStack be the this value.
  JS::Rooted<DisposableStackObject*> disposableStack(
      cx, &args.thisv().toObject().as<DisposableStackObject>());

  // Step 2. Perform ? RequireInternalSlot(disposableStack,
  // [[DisposableState]]).
  // Step 3. If disposableStack.[[DisposableState]] is disposed, return
  // undefined.
  if (disposableStack->state() == DisposableState::Disposed) {
    args.rval().setUndefined();
    return true;
  }

  // Steps 4-5.
  if (!disposableStack->disposeResources(cx)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

/* static */ bool DisposableStackObject::dispose(JSContext* cx, unsigned argc,
                                                 JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  return JS::CallNonGenericMethod<is, dispose_impl>(cx, args);
}

/**
 * Explicit Resource Management Proposal
 * 27.3.3.2 DisposableStack.prototype.defer ( onDispose )
 * https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack.prototype.defer
 */
/* static */ bool DisposableStackObject::defer_impl(JSContext* cx,
                                                    const JS::CallArgs& args) {
  // Step 1. Let disposableStack be the this value.
  JS::Rooted<DisposableStackObject*> disposableStack(
      cx, &args.thisv().toObject().as<DisposableStackObject>());

  // Step 2. Perform ? RequireInternalSlot(disposableStack,
  // [[DisposableState]]).
  // Step 3. If disposableStack.[[DisposableState]] is disposed, throw a
  // ReferenceError exception.
  if (disposableStack->state() == DisposableState::Disposed) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DISPOSABLE_STACK_DISPOSED);
    return false;
  }

  // Step 4. If IsCallable(onDispose) is false, throw a TypeError exception.
  JS::Handle<JS::Value> onDispose = args.get(0);
  if (!ThrowIfOnDisposeNotCallable(cx, onDispose)) {
    return false;
  }

  // Step 5. Perform ?
  // AddDisposableResource(disposableStack.[[DisposeCapability]], undefined,
  // sync-dispose, onDispose).
  JS::Rooted<ArrayObject*> disposeCapability(
      cx, disposableStack->getOrCreateDisposeCapability(cx));
  if (!disposeCapability) {
    return false;
  }

  JS::Rooted<mozilla::Maybe<JS::Value>> onDisposeVal(cx,
                                                     mozilla::Some(onDispose));
  if (!AddDisposableResource(cx, disposeCapability, JS::UndefinedHandleValue,
                             UsingHint::Sync, onDisposeVal)) {
    return false;
  }

  // Step 6. Return undefined.
  args.rval().setUndefined();
  return true;
}

/* static */ bool DisposableStackObject::defer(JSContext* cx, unsigned argc,
                                               JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  return JS::CallNonGenericMethod<is, defer_impl>(cx, args);
}

/**
 * Explicit Resource Management Proposal
 * 27.3.3.5 DisposableStack.prototype.move ( )
 * https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack.prototype.move
 */
/* static */ bool DisposableStackObject::move_impl(JSContext* cx,
                                                   const JS::CallArgs& args) {
  // Step 1. Let disposableStack be the this value.
  JS::Rooted<DisposableStackObject*> disposableStack(
      cx, &args.thisv().toObject().as<DisposableStackObject>());

  // Step 2. Perform ? RequireInternalSlot(disposableStack,
  // [[DisposableState]]).
  // Step 3. If disposableStack.[[DisposableState]] is disposed, throw a
  // ReferenceError exception.
  if (disposableStack->state() == DisposableState::Disposed) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DISPOSABLE_STACK_DISPOSED);
    return false;
  }

  // Step 4. Let newDisposableStack be ?
  // OrdinaryCreateFromConstructor(%DisposableStack%,
  // "%DisposableStack.prototype%", « [[DisposableState]], [[DisposeCapability]]
  // »).
  // Step 5. Set newDisposableStack.[[DisposableState]] to pending.
  // Step 6. Set newDisposableStack.[[DisposeCapability]] to
  // disposableStack.[[DisposeCapability]].
  JS::Rooted<JS::Value> existingDisposeCapability(
      cx, disposableStack->getReservedSlot(
              DisposableStackObject::DISPOSABLE_RESOURCE_STACK_SLOT));
  DisposableStackObject* newDisposableStack =
      DisposableStackObject::create(cx, nullptr, existingDisposeCapability);
  if (!newDisposableStack) {
    return false;
  }

  // Step 7. Set disposableStack.[[DisposeCapability]] to
  // NewDisposeCapability().
  disposableStack->clearDisposableResourceStack();

  // Step 8. Set disposableStack.[[DisposableState]] to disposed.
  disposableStack->setState(DisposableState::Disposed);

  // Step 9. Return newDisposableStack.
  args.rval().setObject(*newDisposableStack);
  return true;
}

/* static */ bool DisposableStackObject::move(JSContext* cx, unsigned argc,
                                              JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  return JS::CallNonGenericMethod<is, move_impl>(cx, args);
}

/**
 * Explicit Resource Management Proposal
 * 27.3.3.1 DisposableStack.prototype.adopt ( value, onDispose )
 * https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack.prototype.adopt
 */
/* static */ bool DisposableStackObject::adopt_impl(JSContext* cx,
                                                    const JS::CallArgs& args) {
  // Step 1. Let disposableStack be the this value.
  JS::Rooted<DisposableStackObject*> disposableStack(
      cx, &args.thisv().toObject().as<DisposableStackObject>());

  // Step 2. Perform ? RequireInternalSlot(disposableStack,
  // [[DisposableState]]).
  // Step 3. If disposableStack.[[DisposableState]] is disposed, throw a
  // ReferenceError exception.
  if (disposableStack->state() == DisposableState::Disposed) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DISPOSABLE_STACK_DISPOSED);
    return false;
  }

  // Step 4. If IsCallable(onDispose) is false, throw a TypeError exception.
  JS::Handle<JS::Value> onDispose = args.get(1);
  if (!ThrowIfOnDisposeNotCallable(cx, onDispose)) {
    return false;
  }

  // Step 5. Let closure be a new Abstract Closure with no parameters that
  // captures value and onDispose and performs the following steps when called:
  // Step 5.a. (see AdoptClosure)
  // Step 6. Let F be CreateBuiltinFunction(closure, 0, "", « »).
  JS::Handle<PropertyName*> funName = cx->names().empty_;
  JS::Rooted<JSFunction*> F(
      cx, NewNativeFunction(cx, AdoptClosure, 0, funName,
                            gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
  if (!F) {
    return false;
  }
  JS::Handle<JS::Value> value = args.get(0);
  F->initExtendedSlot(AdoptClosureSlot_ValueSlot, value);
  F->initExtendedSlot(AdoptClosureSlot_OnDisposeSlot, onDispose);

  // Step 7. Perform ?
  // AddDisposableResource(disposableStack.[[DisposeCapability]], undefined,
  // sync-dispose, F).
  JS::Rooted<ArrayObject*> disposeCapability(
      cx, disposableStack->getOrCreateDisposeCapability(cx));
  if (!disposeCapability) {
    return false;
  }

  JS::Rooted<mozilla::Maybe<JS::Value>> FVal(cx,
                                             mozilla::Some(ObjectValue(*F)));
  if (!AddDisposableResource(cx, disposeCapability, JS::UndefinedHandleValue,
                             UsingHint::Sync, FVal)) {
    return false;
  }

  // Step 8. Return value.
  args.rval().set(value);
  return true;
}

/* static */ bool DisposableStackObject::adopt(JSContext* cx, unsigned argc,
                                               JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  return JS::CallNonGenericMethod<is, adopt_impl>(cx, args);
}

/**
 * Explicit Resource Management Proposal
 * 27.3.3.4 get DisposableStack.prototype.disposed
 * https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-get-disposablestack.prototype.disposed
 */
/* static */ bool DisposableStackObject::disposed_impl(
    JSContext* cx, const JS::CallArgs& args) {
  // Step 1. Let disposableStack be the this value.
  JS::Rooted<DisposableStackObject*> disposableStack(
      cx, &args.thisv().toObject().as<DisposableStackObject>());

  // Step 2. Perform ? RequireInternalSlot(disposableStack,
  // [[DisposableState]]).
  // Step 3. If disposableStack.[[DisposableState]] is disposed, return true.
  // Step 4. Otherwise, return false.
  args.rval().setBoolean(disposableStack->state() == DisposableState::Disposed);
  return true;
}

/* static */ bool DisposableStackObject::disposed(JSContext* cx, unsigned argc,
                                                  JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  return JS::CallNonGenericMethod<is, disposed_impl>(cx, args);
}

/* static */ bool DisposableStackObject::finishInit(
    JSContext* cx, JS::Handle<JSObject*> ctor, JS::Handle<JSObject*> proto) {
  JS::Handle<NativeObject*> nativeProto = proto.as<NativeObject>();

  JS::Rooted<JS::Value> disposeFn(cx);
  JS::Rooted<JS::PropertyKey> disposeId(cx, NameToId(cx->names().dispose));
  if (!NativeGetProperty(cx, nativeProto, disposeId, &disposeFn)) {
    return false;
  }

  // Explicit Resource Management Proposal
  // 27.3.3.7 DisposableStack.prototype [ @@dispose ] ( )
  // https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack.prototype-%40%40dispose
  // The initial value of the @@dispose property is
  // %DisposableStack.prototype.dispose%, defined in 27.3.3.3.
  JS::Rooted<JS::PropertyKey> disposeSym(
      cx, JS::PropertyKey::Symbol(cx->wellKnownSymbols().dispose));
  return NativeDefineDataProperty(cx, nativeProto, disposeSym, disposeFn, 0);
}

const JSPropertySpec DisposableStackObject::properties[] = {
    JS_STRING_SYM_PS(toStringTag, "DisposableStack", JSPROP_READONLY),
    JS_PSG("disposed", disposed, 0),
    JS_PS_END,
};

const JSFunctionSpec DisposableStackObject::methods[] = {
    JS_FN("use", DisposableStackObject::use, 1, 0),
    JS_FN("dispose", DisposableStackObject::dispose, 0, 0),
    JS_FN("defer", DisposableStackObject::defer, 1, 0),
    JS_FN("move", DisposableStackObject::move, 0, 0),
    JS_FN("adopt", DisposableStackObject::adopt, 2, 0),
    // @@dispose is re-defined in finishInit so that it has the
    // same identity as |dispose|.
    JS_SYM_FN(dispose, DisposableStackObject::dispose, 0, 0),
    JS_FS_END,
};

const ClassSpec DisposableStackObject::classSpec_ = {
    GenericCreateConstructor<DisposableStackObject::construct, 0,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<DisposableStackObject>,
    nullptr,
    nullptr,
    DisposableStackObject::methods,
    DisposableStackObject::properties,
    DisposableStackObject::finishInit,
};

const JSClass DisposableStackObject::class_ = {
    "DisposableStack",
    JSCLASS_HAS_RESERVED_SLOTS(DisposableStackObject::RESERVED_SLOTS) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_DisposableStack),
    JS_NULL_CLASS_OPS,
    &DisposableStackObject::classSpec_,
};

const JSClass DisposableStackObject::protoClass_ = {
    "DisposableStack.prototype",
    JSCLASS_HAS_CACHED_PROTO(JSProto_DisposableStack),
    JS_NULL_CLASS_OPS,
    &DisposableStackObject::classSpec_,
};
