/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_JSContext_inl_h
#define vm_JSContext_inl_h

#include "vm/JSContext.h"

#include "builtin/Object.h"
#include "jit/JitFrames.h"
#include "proxy/Proxy.h"
#ifdef ENABLE_BIGINT
#include "vm/BigIntType.h"
#endif
#include "vm/HelperThreads.h"
#include "vm/Interpreter.h"
#include "vm/Iteration.h"
#include "vm/Realm.h"
#include "vm/SymbolType.h"

namespace js {

class ContextChecks {
  JSContext* cx;

  JS::Realm* realm() const { return cx->realm(); }
  JS::Compartment* compartment() const { return cx->compartment(); }
  JS::Zone* zone() const { return cx->zone(); }

 public:
  explicit ContextChecks(JSContext* cx) : cx(cx) {}

  /*
   * Set a breakpoint here (break js::ContextChecks::fail) to debug
   * realm/compartment/zone mismatches.
   */
  static void fail(JS::Realm* r1, JS::Realm* r2, int argIndex) {
    MOZ_CRASH_UNSAFE_PRINTF("*** Realm mismatch %p vs. %p at argument %d", r1,
                            r2, argIndex);
  }
  static void fail(JS::Compartment* c1, JS::Compartment* c2, int argIndex) {
    MOZ_CRASH_UNSAFE_PRINTF("*** Compartment mismatch %p vs. %p at argument %d",
                            c1, c2, argIndex);
  }
  static void fail(JS::Zone* z1, JS::Zone* z2, int argIndex) {
    MOZ_CRASH_UNSAFE_PRINTF("*** Zone mismatch %p vs. %p at argument %d", z1,
                            z2, argIndex);
  }

  void check(JS::Realm* r, int argIndex) {
    if (r && r != realm()) {
      fail(realm(), r, argIndex);
    }
  }

  void check(JS::Compartment* c, int argIndex) {
    if (c && c != compartment()) {
      fail(compartment(), c, argIndex);
    }
  }

  void check(JS::Zone* z, int argIndex) {
    if (zone() && z != zone()) {
      fail(zone(), z, argIndex);
    }
  }

  void check(JSObject* obj, int argIndex) {
    if (obj) {
      MOZ_ASSERT(JS::ObjectIsNotGray(obj));
      MOZ_ASSERT(!js::gc::IsAboutToBeFinalizedUnbarriered(&obj));
      check(obj->compartment(), argIndex);
    }
  }

  template <typename T>
  void checkAtom(T* thing, int argIndex) {
    static_assert(mozilla::IsSame<T, JSAtom>::value ||
                      mozilla::IsSame<T, JS::Symbol>::value,
                  "Should only be called with JSAtom* or JS::Symbol* argument");

#ifdef DEBUG
    // Atoms which move across zone boundaries need to be marked in the new
    // zone, see JS_MarkCrossZoneId.
    if (zone()) {
      if (!cx->runtime()->gc.atomMarking.atomIsMarked(zone(), thing)) {
        MOZ_CRASH_UNSAFE_PRINTF(
            "*** Atom not marked for zone %p at argument %d", zone(), argIndex);
      }
    }
#endif
  }

  void check(JSString* str, int argIndex) {
    MOZ_ASSERT(JS::CellIsNotGray(str));
    if (str->isAtom()) {
      checkAtom(&str->asAtom(), argIndex);
    } else {
      check(str->zone(), argIndex);
    }
  }

  void check(JS::Symbol* symbol, int argIndex) { checkAtom(symbol, argIndex); }

#ifdef ENABLE_BIGINT
  void check(JS::BigInt* bi, int argIndex) { check(bi->zone(), argIndex); }
#endif

  void check(const js::Value& v, int argIndex) {
    if (v.isObject()) {
      check(&v.toObject(), argIndex);
    } else if (v.isString()) {
      check(v.toString(), argIndex);
    } else if (v.isSymbol()) {
      check(v.toSymbol(), argIndex);
    }
#ifdef ENABLE_BIGINT
    else if (v.isBigInt()) {
      check(v.toBigInt(), argIndex);
    }
#endif
  }

  // Check the contents of any container class that supports the C++
  // iteration protocol, eg GCVector<jsid>.
  template <typename Container>
  typename mozilla::EnableIf<
      mozilla::IsSame<decltype(((Container*)nullptr)->begin()),
                      decltype(((Container*)nullptr)->end())>::value>::Type
  check(const Container& container, int argIndex) {
    for (auto i : container) {
      check(i, argIndex);
    }
  }

  void check(const JS::HandleValueArray& arr, int argIndex) {
    for (size_t i = 0; i < arr.length(); i++) {
      check(arr[i], argIndex);
    }
  }

  void check(const CallArgs& args, int argIndex) {
    for (Value* p = args.base(); p != args.end(); ++p) {
      check(*p, argIndex);
    }
  }

  void check(jsid id, int argIndex) {
    if (JSID_IS_ATOM(id)) {
      checkAtom(JSID_TO_ATOM(id), argIndex);
    } else if (JSID_IS_SYMBOL(id)) {
      checkAtom(JSID_TO_SYMBOL(id), argIndex);
    } else {
      MOZ_ASSERT(!JSID_IS_GCTHING(id));
    }
  }

  void check(JSScript* script, int argIndex) {
    MOZ_ASSERT(JS::CellIsNotGray(script));
    if (script) {
      check(script->realm(), argIndex);
    }
  }

  void check(AbstractFramePtr frame, int argIndex);

  void check(Handle<PropertyDescriptor> desc, int argIndex) {
    check(desc.object(), argIndex);
    if (desc.hasGetterObject()) {
      check(desc.getterObject(), argIndex);
    }
    if (desc.hasSetterObject()) {
      check(desc.setterObject(), argIndex);
    }
    check(desc.value(), argIndex);
  }

  void check(TypeSet::Type type, int argIndex) {
    check(type.maybeCompartment(), argIndex);
  }
};

}  // namespace js

template <class Head, class... Tail>
inline void JSContext::checkImpl(int argIndex, const Head& head,
                                 const Tail&... tail) {
  js::ContextChecks(this).check(head, argIndex);
  checkImpl(argIndex + 1, tail...);
}

template <class... Args>
inline void JSContext::check(const Args&... args) {
#ifdef JS_CRASH_DIAGNOSTICS
  if (contextChecksEnabled()) {
    checkImpl(0, args...);
  }
#endif
}

template <class... Args>
inline void JSContext::releaseCheck(const Args&... args) {
  if (contextChecksEnabled()) {
    checkImpl(0, args...);
  }
}

template <class... Args>
MOZ_ALWAYS_INLINE void JSContext::debugOnlyCheck(const Args&... args) {
#if defined(DEBUG) && defined(JS_CRASH_DIAGNOSTICS)
  if (contextChecksEnabled()) {
    checkImpl(0, args...);
  }
#endif
}

namespace js {

STATIC_PRECONDITION_ASSUME(ubound(args.argv_) >= argc)
MOZ_ALWAYS_INLINE bool CallNativeImpl(JSContext* cx, NativeImpl impl,
                                      const CallArgs& args) {
#ifdef DEBUG
  bool alreadyThrowing = cx->isExceptionPending();
#endif
  cx->check(args);
  bool ok = impl(cx, args);
  if (ok) {
    cx->check(args.rval());
    MOZ_ASSERT_IF(!alreadyThrowing, !cx->isExceptionPending());
  }
  return ok;
}

MOZ_ALWAYS_INLINE bool CallJSGetterOp(JSContext* cx, GetterOp op,
                                      HandleObject obj, HandleId id,
                                      MutableHandleValue vp) {
  if (!CheckRecursionLimit(cx)) {
    return false;
  }

  cx->check(obj, id, vp);
  bool ok = op(cx, obj, id, vp);
  if (ok) {
    cx->check(vp);
  }
  return ok;
}

MOZ_ALWAYS_INLINE bool CallJSSetterOp(JSContext* cx, SetterOp op,
                                      HandleObject obj, HandleId id,
                                      HandleValue v, ObjectOpResult& result) {
  if (!CheckRecursionLimit(cx)) {
    return false;
  }

  cx->check(obj, id, v);
  return op(cx, obj, id, v, result);
}

inline bool CallJSAddPropertyOp(JSContext* cx, JSAddPropertyOp op,
                                HandleObject obj, HandleId id, HandleValue v) {
  if (!CheckRecursionLimit(cx)) {
    return false;
  }

  cx->check(obj, id, v);
  return op(cx, obj, id, v);
}

inline bool CallJSDeletePropertyOp(JSContext* cx, JSDeletePropertyOp op,
                                   HandleObject receiver, HandleId id,
                                   ObjectOpResult& result) {
  if (!CheckRecursionLimit(cx)) {
    return false;
  }

  cx->check(receiver, id);
  if (op) {
    return op(cx, receiver, id, result);
  }
  return result.succeed();
}

MOZ_ALWAYS_INLINE bool CheckForInterrupt(JSContext* cx) {
  MOZ_ASSERT(!cx->isExceptionPending());
  // Add an inline fast-path since we have to check for interrupts in some hot
  // C++ loops of library builtins.
  if (MOZ_UNLIKELY(cx->hasAnyPendingInterrupt())) {
    return cx->handleInterrupt();
  }

  JS_INTERRUPT_POSSIBLY_FAIL();

  return true;
}

} /* namespace js */

inline js::LifoAlloc& JSContext::typeLifoAlloc() {
  return zone()->types.typeLifoAlloc();
}

inline js::Nursery& JSContext::nursery() { return runtime()->gc.nursery(); }

inline void JSContext::minorGC(JS::gcreason::Reason reason) {
  runtime()->gc.minorGC(reason);
}

inline void JSContext::setPendingException(JS::HandleValue v) {
#if defined(NIGHTLY_BUILD)
  do {
    // Do not intercept exceptions if we are already
    // in the exception interceptor. That would lead
    // to infinite recursion.
    if (this->runtime()->errorInterception.isExecuting) {
      break;
    }

    // Check whether we have an interceptor at all.
    if (!this->runtime()->errorInterception.interceptor) {
      break;
    }

    // Make sure that we do not call the interceptor from within
    // the interceptor.
    this->runtime()->errorInterception.isExecuting = true;

    // The interceptor must be infallible.
    const mozilla::DebugOnly<bool> wasExceptionPending =
        this->isExceptionPending();
    this->runtime()->errorInterception.interceptor->interceptError(this, v);
    MOZ_ASSERT(wasExceptionPending == this->isExceptionPending());

    this->runtime()->errorInterception.isExecuting = false;
  } while (false);
#endif  // defined(NIGHTLY_BUILD)

  // overRecursed_ is set after the fact by ReportOverRecursed.
  this->overRecursed_ = false;
  this->throwing = true;
  this->unwrappedException() = v;
  check(v);
}

inline bool JSContext::runningWithTrustedPrincipals() {
  return !realm() || realm()->principals() == runtime()->trustedPrincipals();
}

inline void JSContext::enterRealm(JS::Realm* realm) {
  // We should never enter a realm while in the atoms zone.
  MOZ_ASSERT_IF(zone(), !zone()->isAtomsZone());

  realm->enter();
  setRealm(realm);
}

inline void JSContext::enterAtomsZone() {
  realm_ = nullptr;
  setZone(runtime_->unsafeAtomsZone(), AtomsZone);
}

inline void JSContext::setZone(js::Zone* zone,
                               JSContext::IsAtomsZone isAtomsZone) {
  if (zone_) {
    zone_->addTenuredAllocsSinceMinorGC(allocsThisZoneSinceMinorGC_);
  }

  allocsThisZoneSinceMinorGC_ = 0;

  zone_ = zone;
  if (zone == nullptr) {
    freeLists_ = nullptr;
    return;
  }

  if (isAtomsZone == AtomsZone && helperThread()) {
    MOZ_ASSERT(!zone_->wasGCStarted());
    freeLists_ = atomsZoneFreeLists_;
  } else {
    freeLists_ = &zone_->arenas.freeLists();
  }
}

inline void JSContext::enterRealmOf(JSObject* target) {
  MOZ_ASSERT(JS::CellIsNotGray(target));
  enterRealm(target->nonCCWRealm());
}

inline void JSContext::enterRealmOf(JSScript* target) {
  MOZ_ASSERT(JS::CellIsNotGray(target));
  enterRealm(target->realm());
}

inline void JSContext::enterRealmOf(js::ObjectGroup* target) {
  MOZ_ASSERT(JS::CellIsNotGray(target));
  enterRealm(target->realm());
}

inline void JSContext::enterNullRealm() {
  // We should never enter a realm while in the atoms zone.
  MOZ_ASSERT_IF(zone(), !zone()->isAtomsZone());

  setRealm(nullptr);
}

inline void JSContext::leaveRealm(JS::Realm* oldRealm) {
  // Only call leave() after we've setRealm()-ed away from the current realm.
  JS::Realm* startingRealm = realm_;

  // The current realm should be marked as entered-from-C++ at this point.
  MOZ_ASSERT_IF(startingRealm, startingRealm->hasBeenEnteredIgnoringJit());

  setRealm(oldRealm);

  if (startingRealm) {
    startingRealm->leave();
  }
}

inline void JSContext::leaveAtomsZone(JS::Realm* oldRealm) {
  setRealm(oldRealm);
}

inline void JSContext::setRealm(JS::Realm* realm) {
  realm_ = realm;
  if (realm) {
    // This thread must have exclusive access to the zone.
    MOZ_ASSERT(CurrentThreadCanAccessZone(realm->zone()));
    MOZ_ASSERT(!realm->zone()->isAtomsZone());
    setZone(realm->zone(), NotAtomsZone);
  } else {
    setZone(nullptr, NotAtomsZone);
  }
}

inline void JSContext::setRealmForJitExceptionHandler(JS::Realm* realm) {
  // JIT code enters (same-compartment) realms without calling realm->enter()
  // so we don't call realm->leave() here.
  MOZ_ASSERT(realm->compartment() == compartment());
  realm_ = realm;
}

inline JSScript* JSContext::currentScript(
    jsbytecode** ppc, AllowCrossRealm allowCrossRealm) const {
  if (ppc) {
    *ppc = nullptr;
  }

  js::Activation* act = activation();
  if (!act) {
    return nullptr;
  }

  MOZ_ASSERT(act->cx() == this);

  // Cross-compartment implies cross-realm.
  if (allowCrossRealm == AllowCrossRealm::DontAllow &&
      act->compartment() != compartment()) {
    return nullptr;
  }

  JSScript* script = nullptr;
  jsbytecode* pc = nullptr;
  if (act->isJit()) {
    if (act->hasWasmExitFP()) {
      return nullptr;
    }
    js::jit::GetPcScript(const_cast<JSContext*>(this), &script, &pc);
  } else {
    js::InterpreterFrame* fp = act->asInterpreter()->current();
    MOZ_ASSERT(!fp->runningInJit());
    script = fp->script();
    pc = act->asInterpreter()->regs().pc;
  }

  MOZ_ASSERT(script->containsPC(pc));

  if (allowCrossRealm == AllowCrossRealm::DontAllow &&
      script->realm() != realm()) {
    return nullptr;
  }

  if (ppc) {
    *ppc = pc;
  }
  return script;
}

inline js::RuntimeCaches& JSContext::caches() { return runtime()->caches(); }

inline js::AutoKeepAtoms::AutoKeepAtoms(
    JSContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : cx(cx) {
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  cx->zone()->keepAtoms();
}

inline js::AutoKeepAtoms::~AutoKeepAtoms() { cx->zone()->releaseAtoms(); };

#endif /* vm_JSContext_inl_h */
