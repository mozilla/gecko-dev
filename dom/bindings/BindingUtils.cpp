/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BindingUtils.h"

#include <algorithm>
#include <stdarg.h>

#include "JavaScriptParent.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Assertions.h"
#include "mozilla/Preferences.h"

#include "AccessCheck.h"
#include "jsfriendapi.h"
#include "js/OldDebugAPI.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIDOMGlobalPropertyInitializer.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIXPConnect.h"
#include "WrapperFactory.h"
#include "xpcprivate.h"
#include "XPCQuickStubs.h"
#include "XrayWrapper.h"
#include "nsPrintfCString.h"
#include "prprf.h"

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/DOMErrorBinding.h"
#include "mozilla/dom/HTMLObjectElement.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/HTMLSharedObjectElement.h"
#include "mozilla/dom/HTMLEmbedElementBinding.h"
#include "mozilla/dom/HTMLAppletElementBinding.h"
#include "mozilla/dom/Promise.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

JSErrorFormatString ErrorFormatString[] = {
#define MSG_DEF(_name, _argc, _str) \
  { _str, _argc, JSEXN_TYPEERR },
#include "mozilla/dom/Errors.msg"
#undef MSG_DEF
};

const JSErrorFormatString*
GetErrorMessage(void* aUserRef, const char* aLocale,
                const unsigned aErrorNumber)
{
  MOZ_ASSERT(aErrorNumber < ArrayLength(ErrorFormatString));
  return &ErrorFormatString[aErrorNumber];
}

bool
ThrowErrorMessage(JSContext* aCx, const ErrNum aErrorNumber, ...)
{
  va_list ap;
  va_start(ap, aErrorNumber);
  JS_ReportErrorNumberVA(aCx, GetErrorMessage, nullptr,
                         static_cast<const unsigned>(aErrorNumber), ap);
  va_end(ap);
  return false;
}

bool
ThrowInvalidThis(JSContext* aCx, const JS::CallArgs& aArgs,
                 const ErrNum aErrorNumber,
                 const char* aInterfaceName)
{
  NS_ConvertASCIItoUTF16 ifaceName(aInterfaceName);
  // This should only be called for DOM methods/getters/setters, which
  // are JSNative-backed functions, so we can assume that
  // JS_ValueToFunction and JS_GetFunctionDisplayId will both return
  // non-null and that JS_GetStringCharsZ returns non-null.
  JS::Rooted<JSFunction*> func(aCx, JS_ValueToFunction(aCx, aArgs.calleev()));
  MOZ_ASSERT(func);
  JS::Rooted<JSString*> funcName(aCx, JS_GetFunctionDisplayId(func));
  MOZ_ASSERT(funcName);
  JS_ReportErrorNumberUC(aCx, GetErrorMessage, nullptr,
                         static_cast<const unsigned>(aErrorNumber),
                         JS_GetStringCharsZ(aCx, funcName),
                         ifaceName.get());
  return false;
}

bool
ThrowInvalidThis(JSContext* aCx, const JS::CallArgs& aArgs,
                 const ErrNum aErrorNumber,
                 prototypes::ID aProtoId)
{
  return ThrowInvalidThis(aCx, aArgs, aErrorNumber,
                          NamesOfInterfacesWithProtos(aProtoId));
}

bool
ThrowNoSetterArg(JSContext* aCx, prototypes::ID aProtoId)
{
  nsPrintfCString errorMessage("%s attribute setter",
                               NamesOfInterfacesWithProtos(aProtoId));
  return ThrowErrorMessage(aCx, MSG_MISSING_ARGUMENTS, errorMessage.get());
}

} // namespace dom

struct ErrorResult::Message {
  nsTArray<nsString> mArgs;
  dom::ErrNum mErrorNumber;
};

void
ErrorResult::ThrowTypeError(const dom::ErrNum errorNumber, ...)
{
  va_list ap;
  va_start(ap, errorNumber);
  if (IsJSException()) {
    // We have rooted our mJSException, and we don't have the info
    // needed to unroot here, so just bail.
    va_end(ap);
    MOZ_ASSERT(false,
               "Ignoring ThrowTypeError call because we have a JS exception");
    return;
  }
  if (IsTypeError()) {
    delete mMessage;
  }
  mResult = NS_ERROR_TYPE_ERR;
  Message* message = new Message();
  message->mErrorNumber = errorNumber;
  uint16_t argCount =
    dom::GetErrorMessage(nullptr, nullptr, errorNumber)->argCount;
  MOZ_ASSERT(argCount <= 10);
  argCount = std::min<uint16_t>(argCount, 10);
  while (argCount--) {
    message->mArgs.AppendElement(*va_arg(ap, nsString*));
  }
  mMessage = message;
  va_end(ap);
}

void
ErrorResult::ReportTypeError(JSContext* aCx)
{
  MOZ_ASSERT(mMessage, "ReportTypeError() can be called only once");

  Message* message = mMessage;
  const uint32_t argCount = message->mArgs.Length();
  const jschar* args[11];
  for (uint32_t i = 0; i < argCount; ++i) {
    args[i] = message->mArgs.ElementAt(i).get();
  }
  args[argCount] = nullptr;

  JS_ReportErrorNumberUCArray(aCx, dom::GetErrorMessage, nullptr,
                              static_cast<const unsigned>(message->mErrorNumber),
                              argCount > 0 ? args : nullptr);

  ClearMessage();
}

void
ErrorResult::ClearMessage()
{
  if (IsTypeError()) {
    delete mMessage;
    mMessage = nullptr;
  }
}

void
ErrorResult::ThrowJSException(JSContext* cx, JS::Handle<JS::Value> exn)
{
  MOZ_ASSERT(mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to throw a JS exception?");

  if (IsTypeError()) {
    delete mMessage;
  }

  // Make sure mJSException is initialized _before_ we try to root it.  But
  // don't set it to exn yet, because we don't want to do that until after we
  // root.
  mJSException = JS::UndefinedValue();
  if (!js::AddRawValueRoot(cx, &mJSException, "ErrorResult::mJSException")) {
    // Don't use NS_ERROR_DOM_JS_EXCEPTION, because that indicates we have
    // in fact rooted mJSException.
    mResult = NS_ERROR_OUT_OF_MEMORY;
  } else {
    mJSException = exn;
    mResult = NS_ERROR_DOM_JS_EXCEPTION;
  }
}

void
ErrorResult::ReportJSException(JSContext* cx)
{
  MOZ_ASSERT(!mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to handle JS exceptions?");

  JS::Rooted<JS::Value> exception(cx, mJSException);
  if (JS_WrapValue(cx, &exception)) {
    JS_SetPendingException(cx, exception);
  }
  mJSException = exception;
  // If JS_WrapValue failed, not much we can do about it...  No matter
  // what, go ahead and unroot mJSException.
  js::RemoveRawValueRoot(cx, &mJSException);
}

void
ErrorResult::ReportJSExceptionFromJSImplementation(JSContext* aCx)
{
  MOZ_ASSERT(!mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to handle JS exceptions?");

  dom::DOMError* domError;
  nsresult rv = UNWRAP_OBJECT(DOMError, &mJSException.toObject(), domError);
  if (NS_FAILED(rv)) {
    // Unwrapping really shouldn't fail here, if mExceptionHandling is set to
    // eRethrowContentExceptions then the CallSetup destructor only stores an
    // exception if it unwraps to DOMError. If we reach this then either
    // mExceptionHandling wasn't set to eRethrowContentExceptions and we
    // shouldn't be calling ReportJSExceptionFromJSImplementation or something
    // went really wrong.
    NS_RUNTIMEABORT("We stored a non-DOMError exception!");
  }

  nsString message;
  domError->GetMessage(message);

  JS_ReportError(aCx, "%hs", message.get());
  js::RemoveRawValueRoot(aCx, &mJSException);

  // We no longer have a useful exception but we do want to signal that an error
  // occured.
  mResult = NS_ERROR_FAILURE;
}

void
ErrorResult::StealJSException(JSContext* cx,
                              JS::MutableHandle<JS::Value> value)
{
  MOZ_ASSERT(!mMightHaveUnreportedJSException,
             "Must call WouldReportJSException unconditionally in all codepaths that might call StealJSException");
  MOZ_ASSERT(IsJSException(), "No exception to steal");

  value.set(mJSException);
  js::RemoveRawValueRoot(cx, &mJSException);
  mResult = NS_OK;
}

void
ErrorResult::ReportNotEnoughArgsError(JSContext* cx,
                                      const char* ifaceName,
                                      const char* memberName)
{
  MOZ_ASSERT(ErrorCode() == NS_ERROR_XPC_NOT_ENOUGH_ARGS);

  nsPrintfCString errorMessage("%s.%s", ifaceName, memberName);
  ThrowErrorMessage(cx, dom::MSG_MISSING_ARGUMENTS, errorMessage.get());
}

namespace dom {

bool
DefineConstants(JSContext* cx, JS::Handle<JSObject*> obj,
                const ConstantSpec* cs)
{
  JS::Rooted<JS::Value> value(cx);
  for (; cs->name; ++cs) {
    value = cs->value;
    bool ok =
      JS_DefineProperty(cx, obj, cs->name, value,
                        JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
    if (!ok) {
      return false;
    }
  }
  return true;
}

static inline bool
Define(JSContext* cx, JS::Handle<JSObject*> obj, const JSFunctionSpec* spec) {
  return JS_DefineFunctions(cx, obj, spec);
}
static inline bool
Define(JSContext* cx, JS::Handle<JSObject*> obj, const JSPropertySpec* spec) {
  return JS_DefineProperties(cx, obj, spec);
}
static inline bool
Define(JSContext* cx, JS::Handle<JSObject*> obj, const ConstantSpec* spec) {
  return DefineConstants(cx, obj, spec);
}

template<typename T>
bool
DefinePrefable(JSContext* cx, JS::Handle<JSObject*> obj,
               const Prefable<T>* props)
{
  MOZ_ASSERT(props);
  MOZ_ASSERT(props->specs);
  do {
    // Define if enabled
    if (props->isEnabled(cx, obj)) {
      if (!Define(cx, obj, props->specs)) {
        return false;
      }
    }
  } while ((++props)->specs);
  return true;
}

bool
DefineUnforgeableAttributes(JSContext* cx, JS::Handle<JSObject*> obj,
                            const Prefable<const JSPropertySpec>* props)
{
  return DefinePrefable(cx, obj, props);
}


// We should use JSFunction objects for interface objects, but we need a custom
// hasInstance hook because we have new interface objects on prototype chains of
// old (XPConnect-based) bindings. Because Function.prototype.toString throws if
// passed a non-Function object we also need to provide our own toString method
// for interface objects.

enum {
  TOSTRING_CLASS_RESERVED_SLOT = 0,
  TOSTRING_NAME_RESERVED_SLOT = 1
};

static bool
InterfaceObjectToString(JSContext* cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> callee(cx, &args.callee());

  if (!args.thisv().isObject()) {
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                         JSMSG_CANT_CONVERT_TO, "null", "object");
    return false;
  }

  JS::Value v = js::GetFunctionNativeReserved(callee,
                                              TOSTRING_CLASS_RESERVED_SLOT);
  const JSClass* clasp = static_cast<const JSClass*>(v.toPrivate());

  v = js::GetFunctionNativeReserved(callee, TOSTRING_NAME_RESERVED_SLOT);
  JSString* jsname = static_cast<JSString*>(v.toString());
  size_t length;
  const jschar* name = JS_GetInternedStringCharsAndLength(jsname, &length);

  if (js::GetObjectJSClass(&args.thisv().toObject()) != clasp) {
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                         JSMSG_INCOMPATIBLE_PROTO,
                         NS_ConvertUTF16toUTF8(name).get(), "toString",
                         "object");
    return false;
  }

  nsString str;
  str.AppendLiteral("function ");
  str.Append(name, length);
  str.AppendLiteral("() {");
  str.Append('\n');
  str.AppendLiteral("    [native code]");
  str.Append('\n');
  str.Append('}');

  return xpc::NonVoidStringToJsval(cx, str, args.rval());
}

bool
Constructor(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JS::Value& v =
    js::GetFunctionNativeReserved(&args.callee(),
                                  CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT);
  const JSNativeHolder* nativeHolder =
    static_cast<const JSNativeHolder*>(v.toPrivate());
  return (nativeHolder->mNative)(cx, argc, vp);
}

static JSObject*
CreateConstructor(JSContext* cx, JS::Handle<JSObject*> global, const char* name,
                  const JSNativeHolder* nativeHolder, unsigned ctorNargs)
{
  JSFunction* fun = js::NewFunctionWithReserved(cx, Constructor, ctorNargs,
                                                JSFUN_CONSTRUCTOR, global,
                                                name);
  if (!fun) {
    return nullptr;
  }

  JSObject* constructor = JS_GetFunctionObject(fun);
  js::SetFunctionNativeReserved(constructor,
                                CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT,
                                js::PrivateValue(const_cast<JSNativeHolder*>(nativeHolder)));
  return constructor;
}

static bool
DefineConstructor(JSContext* cx, JS::Handle<JSObject*> global, const char* name,
                  JS::Handle<JSObject*> constructor)
{
  bool alreadyDefined;
  if (!JS_AlreadyHasOwnProperty(cx, global, name, &alreadyDefined)) {
    return false;
  }

  // This is Enumerable: False per spec.
  return alreadyDefined ||
         JS_DefineProperty(cx, global, name, constructor, 0);
}

static JSObject*
CreateInterfaceObject(JSContext* cx, JS::Handle<JSObject*> global,
                      JS::Handle<JSObject*> constructorProto,
                      const JSClass* constructorClass,
                      const JSNativeHolder* constructorNative,
                      unsigned ctorNargs, const NamedConstructor* namedConstructors,
                      JS::Handle<JSObject*> proto,
                      const NativeProperties* properties,
                      const NativeProperties* chromeOnlyProperties,
                      const char* name, bool defineOnGlobal)
{
  JS::Rooted<JSObject*> constructor(cx);
  if (constructorClass) {
    MOZ_ASSERT(constructorProto);
    constructor = JS_NewObject(cx, constructorClass, constructorProto, global);
  } else {
    MOZ_ASSERT(constructorNative);
    MOZ_ASSERT(constructorProto == JS_GetFunctionPrototype(cx, global));
    constructor = CreateConstructor(cx, global, name, constructorNative,
                                    ctorNargs);
  }
  if (!constructor) {
    return nullptr;
  }

  if (constructorClass) {
    // Have to shadow Function.prototype.toString, since that throws
    // on things that are not js::FunctionClass.
    JS::Rooted<JSFunction*> toString(cx,
      js::DefineFunctionWithReserved(cx, constructor,
                                     "toString",
                                     InterfaceObjectToString,
                                     0, 0));
    if (!toString) {
      return nullptr;
    }

    JSString *str = ::JS_InternString(cx, name);
    if (!str) {
      return nullptr;
    }
    JSObject* toStringObj = JS_GetFunctionObject(toString);
    js::SetFunctionNativeReserved(toStringObj, TOSTRING_CLASS_RESERVED_SLOT,
                                  PRIVATE_TO_JSVAL(const_cast<JSClass *>(constructorClass)));

    js::SetFunctionNativeReserved(toStringObj, TOSTRING_NAME_RESERVED_SLOT,
                                  STRING_TO_JSVAL(str));

    if (!JS_DefineProperty(cx, constructor, "length", ctorNargs,
                           JSPROP_READONLY | JSPROP_PERMANENT)) {
      return nullptr;
    }
  }

  if (properties) {
    if (properties->staticMethods &&
        !DefinePrefable(cx, constructor, properties->staticMethods)) {
      return nullptr;
    }

    if (properties->staticAttributes &&
        !DefinePrefable(cx, constructor, properties->staticAttributes)) {
      return nullptr;
    }

    if (properties->constants &&
        !DefinePrefable(cx, constructor, properties->constants)) {
      return nullptr;
    }
  }

  if (chromeOnlyProperties) {
    if (chromeOnlyProperties->staticMethods &&
        !DefinePrefable(cx, constructor, chromeOnlyProperties->staticMethods)) {
      return nullptr;
    }

    if (chromeOnlyProperties->staticAttributes &&
        !DefinePrefable(cx, constructor,
                        chromeOnlyProperties->staticAttributes)) {
      return nullptr;
    }

    if (chromeOnlyProperties->constants &&
        !DefinePrefable(cx, constructor, chromeOnlyProperties->constants)) {
      return nullptr;
    }
  }

  if (proto && !JS_LinkConstructorAndPrototype(cx, constructor, proto)) {
    return nullptr;
  }

  if (defineOnGlobal && !DefineConstructor(cx, global, name, constructor)) {
    return nullptr;
  }

  if (namedConstructors) {
    int namedConstructorSlot = DOM_INTERFACE_SLOTS_BASE;
    while (namedConstructors->mName) {
      JS::Rooted<JSObject*> namedConstructor(cx,
        CreateConstructor(cx, global, namedConstructors->mName,
                          &namedConstructors->mHolder,
                          namedConstructors->mNargs));
      if (!namedConstructor ||
          !JS_DefineProperty(cx, namedConstructor, "prototype",
                             proto, JSPROP_PERMANENT | JSPROP_READONLY,
                             JS_PropertyStub, JS_StrictPropertyStub) ||
          (defineOnGlobal &&
           !DefineConstructor(cx, global, namedConstructors->mName,
                              namedConstructor))) {
        return nullptr;
      }
      js::SetReservedSlot(constructor, namedConstructorSlot++,
                          JS::ObjectValue(*namedConstructor));
      ++namedConstructors;
    }
  }

  return constructor;
}

bool
DefineWebIDLBindingUnforgeablePropertiesOnXPCObject(JSContext* cx,
                                                    JS::Handle<JSObject*> obj,
                                                    const NativeProperties* properties)
{
  if (properties->unforgeableAttributes &&
      !DefinePrefable(cx, obj, properties->unforgeableAttributes)) {
    return false;
  }

  return true;
}

bool
DefineWebIDLBindingPropertiesOnXPCObject(JSContext* cx,
                                         JS::Handle<JSObject*> obj,
                                         const NativeProperties* properties)
{
  if (properties->methods &&
      !DefinePrefable(cx, obj, properties->methods)) {
    return false;
  }

  if (properties->attributes &&
      !DefinePrefable(cx, obj, properties->attributes)) {
    return false;
  }

  return true;
}

static JSObject*
CreateInterfacePrototypeObject(JSContext* cx, JS::Handle<JSObject*> global,
                               JS::Handle<JSObject*> parentProto,
                               const JSClass* protoClass,
                               const NativeProperties* properties,
                               const NativeProperties* chromeOnlyProperties)
{
  JS::Rooted<JSObject*> ourProto(cx,
    JS_NewObjectWithUniqueType(cx, protoClass, parentProto, global));
  if (!ourProto ||
      !DefineProperties(cx, ourProto, properties, chromeOnlyProperties)) {
    return nullptr;
  }

  return ourProto;
}

bool
DefineProperties(JSContext* cx, JS::Handle<JSObject*> obj,
                 const NativeProperties* properties,
                 const NativeProperties* chromeOnlyProperties)
{
  if (properties) {
    if (properties->methods &&
        !DefinePrefable(cx, obj, properties->methods)) {
      return false;
    }

    if (properties->attributes &&
        !DefinePrefable(cx, obj, properties->attributes)) {
      return false;
    }

    if (properties->constants &&
        !DefinePrefable(cx, obj, properties->constants)) {
      return false;
    }
  }

  if (chromeOnlyProperties) {
    if (chromeOnlyProperties->methods &&
        !DefinePrefable(cx, obj, chromeOnlyProperties->methods)) {
      return false;
    }

    if (chromeOnlyProperties->attributes &&
        !DefinePrefable(cx, obj, chromeOnlyProperties->attributes)) {
      return false;
    }

    if (chromeOnlyProperties->constants &&
        !DefinePrefable(cx, obj, chromeOnlyProperties->constants)) {
      return false;
    }
  }

  return true;
}

void
CreateInterfaceObjects(JSContext* cx, JS::Handle<JSObject*> global,
                       JS::Handle<JSObject*> protoProto,
                       const JSClass* protoClass, JS::Heap<JSObject*>* protoCache,
                       JS::Handle<JSObject*> constructorProto,
                       const JSClass* constructorClass, const JSNativeHolder* constructor,
                       unsigned ctorNargs, const NamedConstructor* namedConstructors,
                       JS::Heap<JSObject*>* constructorCache,
                       const NativeProperties* properties,
                       const NativeProperties* chromeOnlyProperties,
                       const char* name, bool defineOnGlobal)
{
  MOZ_ASSERT(protoClass || constructorClass || constructor,
             "Need at least one class or a constructor!");
  MOZ_ASSERT(!((properties &&
                (properties->methods || properties->attributes)) ||
               (chromeOnlyProperties &&
                (chromeOnlyProperties->methods ||
                 chromeOnlyProperties->attributes))) || protoClass,
             "Methods or properties but no protoClass!");
  MOZ_ASSERT(!((properties &&
                (properties->staticMethods || properties->staticAttributes)) ||
               (chromeOnlyProperties &&
                (chromeOnlyProperties->staticMethods ||
                 chromeOnlyProperties->staticAttributes))) ||
             constructorClass || constructor,
             "Static methods but no constructorClass or constructor!");
  MOZ_ASSERT(bool(name) == bool(constructorClass || constructor),
             "Must have name precisely when we have an interface object");
  MOZ_ASSERT(!constructorClass || !constructor);
  MOZ_ASSERT(!protoClass == !protoCache,
             "If, and only if, there is an interface prototype object we need "
             "to cache it");
  MOZ_ASSERT(!(constructorClass || constructor) == !constructorCache,
             "If, and only if, there is an interface object we need to cache "
             "it");

  JS::Rooted<JSObject*> proto(cx);
  if (protoClass) {
    proto =
      CreateInterfacePrototypeObject(cx, global, protoProto, protoClass,
                                     properties, chromeOnlyProperties);
    if (!proto) {
      return;
    }

    *protoCache = proto;
  }
  else {
    MOZ_ASSERT(!proto);
  }

  JSObject* interface;
  if (constructorClass || constructor) {
    interface = CreateInterfaceObject(cx, global, constructorProto,
                                      constructorClass, constructor,
                                      ctorNargs, namedConstructors, proto,
                                      properties, chromeOnlyProperties, name,
                                      defineOnGlobal);
    if (!interface) {
      if (protoCache) {
        // If we fail we need to make sure to clear the value of protoCache we
        // set above.
        *protoCache = nullptr;
      }
      return;
    }
    *constructorCache = interface;
  }
}

bool
NativeInterface2JSObjectAndThrowIfFailed(JSContext* aCx,
                                         JS::Handle<JSObject*> aScope,
                                         JS::MutableHandle<JS::Value> aRetval,
                                         xpcObjectHelper& aHelper,
                                         const nsIID* aIID,
                                         bool aAllowNativeWrapper)
{
  js::AssertSameCompartment(aCx, aScope);
  nsresult rv;
  // Inline some logic from XPCConvert::NativeInterfaceToJSObject that we need
  // on all threads.
  nsWrapperCache *cache = aHelper.GetWrapperCache();

  if (cache && cache->IsDOMBinding()) {
      JS::Rooted<JSObject*> obj(aCx, cache->GetWrapper());
      if (!obj) {
          obj = cache->WrapObject(aCx);
      }

      if (obj && aAllowNativeWrapper && !JS_WrapObject(aCx, &obj)) {
        return false;
      }

      if (obj) {
        aRetval.setObject(*obj);
        return true;
      }
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!XPCConvert::NativeInterface2JSObject(aRetval, nullptr, aHelper, aIID,
                                            nullptr, aAllowNativeWrapper, &rv)) {
    // I can't tell if NativeInterface2JSObject throws JS exceptions
    // or not.  This is a sloppy stab at the right semantics; the
    // method really ought to be fixed to behave consistently.
    if (!JS_IsExceptionPending(aCx)) {
      Throw(aCx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
    }
    return false;
  }
  return true;
}

bool
TryPreserveWrapper(JSObject* obj)
{
  MOZ_ASSERT(IsDOMObject(obj));

  if (nsISupports* native = UnwrapDOMObjectToISupports(obj)) {
    nsWrapperCache* cache = nullptr;
    CallQueryInterface(native, &cache);
    if (cache) {
      cache->PreserveWrapper(native);
    }
    return true;
  }

  // If this DOMClass is not cycle collected, then it isn't wrappercached,
  // so it does not need to be preserved. If it is cycle collected, then
  // we can't tell if it is wrappercached or not, so we just return false.
  const DOMJSClass* domClass = GetDOMClass(obj);
  return domClass && !domClass->mParticipant;
}

// Can only be called with a DOM JSClass.
bool
InstanceClassHasProtoAtDepth(const js::Class* clasp,
                             uint32_t protoID, uint32_t depth)
{
  const DOMJSClass* domClass = DOMJSClass::FromJSClass(clasp);
  return static_cast<uint32_t>(domClass->mInterfaceChain[depth]) == protoID;
}

// Only set allowNativeWrapper to false if you really know you need it, if in
// doubt use true. Setting it to false disables security wrappers.
bool
XPCOMObjectToJsval(JSContext* cx, JS::Handle<JSObject*> scope,
                   xpcObjectHelper& helper, const nsIID* iid,
                   bool allowNativeWrapper, JS::MutableHandle<JS::Value> rval)
{
  if (!NativeInterface2JSObjectAndThrowIfFailed(cx, scope, rval, helper, iid,
                                                allowNativeWrapper)) {
    return false;
  }

#ifdef DEBUG
  JSObject* jsobj = rval.toObjectOrNull();
  if (jsobj && !js::GetObjectParent(jsobj))
    NS_ASSERTION(js::GetObjectClass(jsobj)->flags & JSCLASS_IS_GLOBAL,
                 "Why did we recreate this wrapper?");
#endif

  return true;
}

bool
VariantToJsval(JSContext* aCx, nsIVariant* aVariant,
               JS::MutableHandle<JS::Value> aRetval)
{
  nsresult rv;
  if (!XPCVariant::VariantDataToJS(aVariant, &rv, aRetval)) {
    // Does it throw?  Who knows
    if (!JS_IsExceptionPending(aCx)) {
      Throw(aCx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
    }
    return false;
  }

  return true;
}

bool
QueryInterface(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JS::Value> thisv(cx, JS_THIS(cx, vp));
  if (thisv.isNull())
    return false;

  // Get the object. It might be a security wrapper, in which case we do a checked
  // unwrap.
  JS::Rooted<JSObject*> origObj(cx, &thisv.toObject());
  JSObject* obj = js::CheckedUnwrap(origObj, /* stopAtOuter = */ false);
  if (!obj) {
      JS_ReportError(cx, "Permission denied to access object");
      return false;
  }

  // Switch this to UnwrapDOMObjectToISupports once our global objects are
  // using new bindings.
  JS::Rooted<JS::Value> val(cx, JS::ObjectValue(*obj));
  nsISupports* native = nullptr;
  nsCOMPtr<nsISupports> nativeRef;
  xpc_qsUnwrapArg<nsISupports>(cx, val, &native,
                               static_cast<nsISupports**>(getter_AddRefs(nativeRef)),
                               &val);
  if (!native) {
    return Throw(cx, NS_ERROR_FAILURE);
  }

  if (argc < 1) {
    return Throw(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
  }

  if (!args[0].isObject()) {
    return Throw(cx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  nsIJSID* iid;
  SelfRef iidRef;
  if (NS_FAILED(xpc_qsUnwrapArg<nsIJSID>(cx, args[0], &iid, &iidRef.ptr,
                                         args[0]))) {
    return Throw(cx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }
  MOZ_ASSERT(iid);

  if (iid->GetID()->Equals(NS_GET_IID(nsIClassInfo))) {
    nsresult rv;
    nsCOMPtr<nsIClassInfo> ci = do_QueryInterface(native, &rv);
    if (NS_FAILED(rv)) {
      return Throw(cx, rv);
    }

    return WrapObject(cx, ci, &NS_GET_IID(nsIClassInfo), args.rval());
  }

  nsCOMPtr<nsISupports> unused;
  nsresult rv = native->QueryInterface(*iid->GetID(), getter_AddRefs(unused));
  if (NS_FAILED(rv)) {
    return Throw(cx, rv);
  }

  *vp = thisv;
  return true;
}

void
GetInterfaceImpl(JSContext* aCx, nsIInterfaceRequestor* aRequestor,
                 nsWrapperCache* aCache, nsIJSID* aIID,
                 JS::MutableHandle<JS::Value> aRetval, ErrorResult& aError)
{
  const nsID* iid = aIID->GetID();

  nsRefPtr<nsISupports> result;
  aError = aRequestor->GetInterface(*iid, getter_AddRefs(result));
  if (aError.Failed()) {
    return;
  }

  if (!WrapObject(aCx, result, iid, aRetval)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

bool
ThrowingConstructor(JSContext* cx, unsigned argc, JS::Value* vp)
{
  return ThrowErrorMessage(cx, MSG_ILLEGAL_CONSTRUCTOR);
}

bool
ThrowConstructorWithoutNew(JSContext* cx, const char* name)
{
  return ThrowErrorMessage(cx, MSG_CONSTRUCTOR_WITHOUT_NEW, name);
}

inline const NativePropertyHooks*
GetNativePropertyHooks(JSContext *cx, JS::Handle<JSObject*> obj,
                       DOMObjectType& type)
{
  const DOMJSClass* domClass = GetDOMClass(obj);
  if (domClass) {
    type = eInstance;
    return domClass->mNativeHooks;
  }

  if (JS_ObjectIsFunction(cx, obj)) {
    MOZ_ASSERT(JS_IsNativeFunction(obj, Constructor));
    type = eInterface;
    const JS::Value& v =
      js::GetFunctionNativeReserved(obj,
                                    CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT);
    const JSNativeHolder* nativeHolder =
      static_cast<const JSNativeHolder*>(v.toPrivate());
    return nativeHolder->mPropertyHooks;
  }

  MOZ_ASSERT(IsDOMIfaceAndProtoClass(js::GetObjectClass(obj)));
  const DOMIfaceAndProtoJSClass* ifaceAndProtoJSClass =
    DOMIfaceAndProtoJSClass::FromJSClass(js::GetObjectClass(obj));
  type = ifaceAndProtoJSClass->mType;
  return ifaceAndProtoJSClass->mNativeHooks;
}

// Try to resolve a property as an unforgeable property from the given
// NativeProperties, if it's there.  nativeProperties is allowed to be null (in
// which case we of course won't resolve anything).
static bool
XrayResolveUnforgeableProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                               JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                               JS::MutableHandle<JSPropertyDescriptor> desc,
                               const NativeProperties* nativeProperties);

static bool
XrayResolveNativeProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                          const NativePropertyHooks* nativePropertyHooks,
                          DOMObjectType type, JS::Handle<JSObject*> obj,
                          JS::Handle<jsid> id,
                          JS::MutableHandle<JSPropertyDescriptor> desc);

bool
XrayResolveOwnProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                       JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                       JS::MutableHandle<JSPropertyDescriptor> desc)
{
  DOMObjectType type;
  const NativePropertyHooks *nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);

  if (type != eInstance) {
    // For prototype objects and interface objects, just return their
    // normal set of properties.
    return XrayResolveNativeProperty(cx, wrapper, nativePropertyHooks, type,
                                     obj, id, desc);
  }

  // Check for unforgeable properties before doing mResolveOwnProperty weirdness
  const NativePropertiesHolder& nativeProperties =
    nativePropertyHooks->mNativeProperties;
  if (!XrayResolveUnforgeableProperty(cx, wrapper, obj, id, desc,
                                      nativeProperties.regular)) {
    return false;
  }
  if (desc.object()) {
    return true;
  }
  if (!XrayResolveUnforgeableProperty(cx, wrapper, obj, id, desc,
                                      nativeProperties.chromeOnly)) {
    return false;
  }
  if (desc.object()) {
    return true;
  }

  return !nativePropertyHooks->mResolveOwnProperty ||
         nativePropertyHooks->mResolveOwnProperty(cx, wrapper, obj, id, desc);
}

static bool
XrayResolveAttribute(JSContext* cx, JS::Handle<JSObject*> wrapper,
                     JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                     const Prefable<const JSPropertySpec>* attributes, jsid* attributeIds,
                     const JSPropertySpec* attributeSpecs, JS::MutableHandle<JSPropertyDescriptor> desc)
{
  for (; attributes->specs; ++attributes) {
    if (attributes->isEnabled(cx, obj)) {
      // Set i to be the index into our full list of ids/specs that we're
      // looking at now.
      size_t i = attributes->specs - attributeSpecs;
      for ( ; attributeIds[i] != JSID_VOID; ++i) {
        if (id == attributeIds[i]) {
          const JSPropertySpec& attrSpec = attributeSpecs[i];
          // Because of centralization, we need to make sure we fault in the
          // JitInfos as well. At present, until the JSAPI changes, the easiest
          // way to do this is wrap them up as functions ourselves.
          desc.setAttributes(attrSpec.flags & ~JSPROP_NATIVE_ACCESSORS);
          // They all have getters, so we can just make it.
          JS::Rooted<JSFunction*> fun(cx,
                                      JS_NewFunctionById(cx, (JSNative)attrSpec.getter.propertyOp.op,
                                                         0, 0, wrapper, id));
          if (!fun)
            return false;
          SET_JITINFO(fun, attrSpec.getter.propertyOp.info);
          JSObject *funobj = JS_GetFunctionObject(fun);
          desc.setGetterObject(funobj);
          desc.attributesRef() |= JSPROP_GETTER;
          if (attrSpec.setter.propertyOp.op) {
            // We have a setter! Make it.
            fun = JS_NewFunctionById(cx, (JSNative)attrSpec.setter.propertyOp.op, 1, 0,
                                     wrapper, id);
            if (!fun)
              return false;
            SET_JITINFO(fun, attrSpec.setter.propertyOp.info);
            funobj = JS_GetFunctionObject(fun);
            desc.setSetterObject(funobj);
            desc.attributesRef() |= JSPROP_SETTER;
          } else {
            desc.setSetter(nullptr);
          }
          desc.object().set(wrapper);
          return true;
        }
      }
    }
  }
  return true;
}

/* static */ bool
XrayResolveUnforgeableProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                               JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                               JS::MutableHandle<JSPropertyDescriptor> desc,
                               const NativeProperties* nativeProperties)
{
  return !nativeProperties || !nativeProperties->unforgeableAttributes ||
         XrayResolveAttribute(cx, wrapper, obj, id,
                              nativeProperties->unforgeableAttributes,
                              nativeProperties->unforgeableAttributeIds,
                              nativeProperties->unforgeableAttributeSpecs,
                              desc);
}

static bool
XrayResolveProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                    JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                    JS::MutableHandle<JSPropertyDescriptor> desc, DOMObjectType type,
                    const NativeProperties* nativeProperties)
{
  const Prefable<const JSFunctionSpec>* methods;
  jsid* methodIds;
  const JSFunctionSpec* methodsSpecs;
  if (type == eInterface) {
    methods = nativeProperties->staticMethods;
    methodIds = nativeProperties->staticMethodIds;
    methodsSpecs = nativeProperties->staticMethodsSpecs;
  } else {
    methods = nativeProperties->methods;
    methodIds = nativeProperties->methodIds;
    methodsSpecs = nativeProperties->methodsSpecs;
  }
  if (methods) {
    const Prefable<const JSFunctionSpec>* method;
    for (method = methods; method->specs; ++method) {
      if (method->isEnabled(cx, obj)) {
        // Set i to be the index into our full list of ids/specs that we're
        // looking at now.
        size_t i = method->specs - methodsSpecs;
        for ( ; methodIds[i] != JSID_VOID; ++i) {
          if (id == methodIds[i]) {
            const JSFunctionSpec& methodSpec = methodsSpecs[i];
            JSFunction *fun;
            if (methodSpec.selfHostedName) {
              fun = JS::GetSelfHostedFunction(cx, methodSpec.selfHostedName, id, methodSpec.nargs);
              if (!fun) {
                return false;
              }
              MOZ_ASSERT(!methodSpec.call.op, "Bad FunctionSpec declaration: non-null native");
              MOZ_ASSERT(!methodSpec.call.info, "Bad FunctionSpec declaration: non-null jitinfo");
            } else {
              fun = JS_NewFunctionById(cx, methodSpec.call.op, methodSpec.nargs, 0, wrapper, id);
              if (!fun) {
                return false;
              }
              SET_JITINFO(fun, methodSpec.call.info);
            }
            JSObject *funobj = JS_GetFunctionObject(fun);
            desc.value().setObject(*funobj);
            desc.setAttributes(methodSpec.flags);
            desc.object().set(wrapper);
            desc.setSetter(nullptr);
            desc.setGetter(nullptr);
           return true;
          }
        }
      }
    }
  }

  if (type == eInterface) {
    if (nativeProperties->staticAttributes) {
      if (!XrayResolveAttribute(cx, wrapper, obj, id,
                                nativeProperties->staticAttributes,
                                nativeProperties->staticAttributeIds,
                                nativeProperties->staticAttributeSpecs, desc)) {
        return false;
      }
      if (desc.object()) {
        return true;
      }
    }
  } else {
    if (nativeProperties->attributes) {
      if (!XrayResolveAttribute(cx, wrapper, obj, id,
                                nativeProperties->attributes,
                                nativeProperties->attributeIds,
                                nativeProperties->attributeSpecs, desc)) {
        return false;
      }
      if (desc.object()) {
        return true;
      }
    }
  }

  if (nativeProperties->constants) {
    const Prefable<const ConstantSpec>* constant;
    for (constant = nativeProperties->constants; constant->specs; ++constant) {
      if (constant->isEnabled(cx, obj)) {
        // Set i to be the index into our full list of ids/specs that we're
        // looking at now.
        size_t i = constant->specs - nativeProperties->constantSpecs;
        for ( ; nativeProperties->constantIds[i] != JSID_VOID; ++i) {
          if (id == nativeProperties->constantIds[i]) {
            desc.setAttributes(JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
            desc.object().set(wrapper);
            desc.value().set(nativeProperties->constantSpecs[i].value);
            return true;
          }
        }
      }
    }
  }

  return true;
}

static bool
ResolvePrototypeOrConstructor(JSContext* cx, JS::Handle<JSObject*> wrapper,
                              JS::Handle<JSObject*> obj,
                              size_t protoAndIfaceCacheIndex, unsigned attrs,
                              JS::MutableHandle<JSPropertyDescriptor> desc)
{
  JS::Rooted<JSObject*> global(cx, js::GetGlobalForObjectCrossCompartment(obj));
  {
    JSAutoCompartment ac(cx, global);
    ProtoAndIfaceCache& protoAndIfaceCache = *GetProtoAndIfaceCache(global);
    JSObject* protoOrIface =
      protoAndIfaceCache.EntrySlotIfExists(protoAndIfaceCacheIndex);
    if (!protoOrIface) {
      return false;
    }
    desc.object().set(wrapper);
    desc.setAttributes(attrs);
    desc.setGetter(JS_PropertyStub);
    desc.setSetter(JS_StrictPropertyStub);
    desc.value().set(JS::ObjectValue(*protoOrIface));
  }
  return JS_WrapPropertyDescriptor(cx, desc);
}

/* static */ bool
XrayResolveNativeProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                          const NativePropertyHooks* nativePropertyHooks,
                          DOMObjectType type, JS::Handle<JSObject*> obj,
                          JS::Handle<jsid> id,
                          JS::MutableHandle<JSPropertyDescriptor> desc)
{
  if (type == eInterface && IdEquals(id, "prototype")) {
    return nativePropertyHooks->mPrototypeID == prototypes::id::_ID_Count ||
           ResolvePrototypeOrConstructor(cx, wrapper, obj,
                                         nativePropertyHooks->mPrototypeID,
                                         JSPROP_PERMANENT | JSPROP_READONLY,
                                         desc);
  }

  if (type == eInterfacePrototype && IdEquals(id, "constructor")) {
    return nativePropertyHooks->mConstructorID == constructors::id::_ID_Count ||
           ResolvePrototypeOrConstructor(cx, wrapper, obj,
                                         nativePropertyHooks->mConstructorID,
                                         0, desc);
  }

  const NativePropertiesHolder& nativeProperties =
    nativePropertyHooks->mNativeProperties;

  if (nativeProperties.regular &&
      !XrayResolveProperty(cx, wrapper, obj, id, desc, type,
                           nativeProperties.regular)) {
    return false;
  }

  if (!desc.object() &&
      nativeProperties.chromeOnly &&
      xpc::AccessCheck::isChrome(js::GetObjectCompartment(wrapper)) &&
      !XrayResolveProperty(cx, wrapper, obj, id, desc, type,
                           nativeProperties.chromeOnly)) {
    return false;
  }

  return true;
}

bool
XrayResolveNativeProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                          JS::Handle<JSObject*> obj,
                          JS::Handle<jsid> id, JS::MutableHandle<JSPropertyDescriptor> desc)
{
  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);

  if (type == eInstance) {
    // Force the type to be eInterfacePrototype, since we need to walk the
    // prototype chain.
    type = eInterfacePrototype;
  }

  if (type == eInterfacePrototype) {
    do {
      if (!XrayResolveNativeProperty(cx, wrapper, nativePropertyHooks, type,
                                     obj, id, desc)) {
        return false;
      }

      if (desc.object()) {
        return true;
      }
    } while ((nativePropertyHooks = nativePropertyHooks->mProtoHooks));

    return true;
  }

  return XrayResolveNativeProperty(cx, wrapper, nativePropertyHooks, type, obj,
                                   id, desc);
}

bool
XrayDefineProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                   JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                   JS::MutableHandle<JSPropertyDescriptor> desc, bool* defined)
{
  if (!js::IsProxy(obj))
      return true;

  MOZ_ASSERT(IsDOMProxy(obj), "What kind of proxy is this?");

  DOMProxyHandler* handler =
    static_cast<DOMProxyHandler*>(js::GetProxyHandler(obj));
  return handler->defineProperty(cx, wrapper, id, desc, defined);
}

bool
XrayEnumerateAttributes(JSContext* cx, JS::Handle<JSObject*> wrapper,
                        JS::Handle<JSObject*> obj,
                        const Prefable<const JSPropertySpec>* attributes,
                        jsid* attributeIds, const JSPropertySpec* attributeSpecs,
                        unsigned flags, JS::AutoIdVector& props)
{
  for (; attributes->specs; ++attributes) {
    if (attributes->isEnabled(cx, obj)) {
      // Set i to be the index into our full list of ids/specs that we're
      // looking at now.
      size_t i = attributes->specs - attributeSpecs;
      for ( ; attributeIds[i] != JSID_VOID; ++i) {
        if (((flags & JSITER_HIDDEN) ||
             (attributeSpecs[i].flags & JSPROP_ENUMERATE)) &&
            !props.append(attributeIds[i])) {
          return false;
        }
      }
    }
  }
  return true;
}

bool
XrayEnumerateProperties(JSContext* cx, JS::Handle<JSObject*> wrapper,
                        JS::Handle<JSObject*> obj,
                        unsigned flags, JS::AutoIdVector& props,
                        DOMObjectType type,
                        const NativeProperties* nativeProperties)
{
  const Prefable<const JSFunctionSpec>* methods;
  jsid* methodIds;
  const JSFunctionSpec* methodsSpecs;
  if (type == eInterface) {
    methods = nativeProperties->staticMethods;
    methodIds = nativeProperties->staticMethodIds;
    methodsSpecs = nativeProperties->staticMethodsSpecs;
  } else {
    methods = nativeProperties->methods;
    methodIds = nativeProperties->methodIds;
    methodsSpecs = nativeProperties->methodsSpecs;
  }
  if (methods) {
    const Prefable<const JSFunctionSpec>* method;
    for (method = methods; method->specs; ++method) {
      if (method->isEnabled(cx, obj)) {
        // Set i to be the index into our full list of ids/specs that we're
        // looking at now.
        size_t i = method->specs - methodsSpecs;
        for ( ; methodIds[i] != JSID_VOID; ++i) {
          if (((flags & JSITER_HIDDEN) ||
               (methodsSpecs[i].flags & JSPROP_ENUMERATE)) &&
              !props.append(methodIds[i])) {
            return false;
          }
        }
      }
    }
  }

  if (type == eInterface) {
    if (nativeProperties->staticAttributes &&
        !XrayEnumerateAttributes(cx, wrapper, obj,
                                 nativeProperties->staticAttributes,
                                 nativeProperties->staticAttributeIds,
                                 nativeProperties->staticAttributeSpecs,
                                 flags, props)) {
      return false;
    }
  } else {
    if (nativeProperties->attributes &&
        !XrayEnumerateAttributes(cx, wrapper, obj,
                                 nativeProperties->attributes,
                                 nativeProperties->attributeIds,
                                 nativeProperties->attributeSpecs,
                                 flags, props)) {
      return false;
    }
    if (nativeProperties->unforgeableAttributes &&
        !XrayEnumerateAttributes(cx, wrapper, obj,
                                 nativeProperties->unforgeableAttributes,
                                 nativeProperties->unforgeableAttributeIds,
                                 nativeProperties->unforgeableAttributeSpecs,
                                 flags, props)) {
      return false;
    }
  }

  if (nativeProperties->constants) {
    const Prefable<const ConstantSpec>* constant;
    for (constant = nativeProperties->constants; constant->specs; ++constant) {
      if (constant->isEnabled(cx, obj)) {
        // Set i to be the index into our full list of ids/specs that we're
        // looking at now.
        size_t i = constant->specs - nativeProperties->constantSpecs;
        for ( ; nativeProperties->constantIds[i] != JSID_VOID; ++i) {
          if (!props.append(nativeProperties->constantIds[i])) {
            return false;
          }
        }
      }
    }
  }

  return true;
}

bool
XrayEnumerateNativeProperties(JSContext* cx, JS::Handle<JSObject*> wrapper,
                              const NativePropertyHooks* nativePropertyHooks,
                              DOMObjectType type, JS::Handle<JSObject*> obj,
                              unsigned flags, JS::AutoIdVector& props)
{
  if (type == eInterface &&
      nativePropertyHooks->mPrototypeID != prototypes::id::_ID_Count &&
      !AddStringToIDVector(cx, props, "prototype")) {
    return false;
  }

  if (type == eInterfacePrototype &&
      nativePropertyHooks->mConstructorID != constructors::id::_ID_Count &&
      (flags & JSITER_HIDDEN) &&
      !AddStringToIDVector(cx, props, "constructor")) {
    return false;
  }

  const NativePropertiesHolder& nativeProperties =
    nativePropertyHooks->mNativeProperties;

  if (nativeProperties.regular &&
      !XrayEnumerateProperties(cx, wrapper, obj, flags, props, type,
                               nativeProperties.regular)) {
    return false;
  }

  if (nativeProperties.chromeOnly &&
      xpc::AccessCheck::isChrome(js::GetObjectCompartment(wrapper)) &&
      !XrayEnumerateProperties(cx, wrapper, obj, flags, props, type,
                               nativeProperties.chromeOnly)) {
    return false;
  }

  return true;
}

bool
XrayEnumerateProperties(JSContext* cx, JS::Handle<JSObject*> wrapper,
                        JS::Handle<JSObject*> obj,
                        unsigned flags, JS::AutoIdVector& props)
{
  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);

  if (type == eInstance) {
    if (nativePropertyHooks->mEnumerateOwnProperties &&
        !nativePropertyHooks->mEnumerateOwnProperties(cx, wrapper, obj,
                                                      props)) {
      return false;
    }

    if (flags & JSITER_OWNONLY) {
      return true;
    }

    // Force the type to be eInterfacePrototype, since we need to walk the
    // prototype chain.
    type = eInterfacePrototype;
  }

  if (type == eInterfacePrototype) {
    do {
      if (!XrayEnumerateNativeProperties(cx, wrapper, nativePropertyHooks, type,
                                         obj, flags, props)) {
        return false;
      }

      if (flags & JSITER_OWNONLY) {
        return true;
      }
    } while ((nativePropertyHooks = nativePropertyHooks->mProtoHooks));

    return true;
  }

  return XrayEnumerateNativeProperties(cx, wrapper, nativePropertyHooks, type,
                                       obj, flags, props);
}

NativePropertyHooks sWorkerNativePropertyHooks = {
  nullptr,
  nullptr,
  {
    nullptr,
    nullptr
  },
  prototypes::id::_ID_Count,
  constructors::id::_ID_Count,
  nullptr
};

bool
GetPropertyOnPrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id, bool* found,
                       JS::Value* vp)
{
  JS::Rooted<JSObject*> proto(cx);
  if (!js::GetObjectProto(cx, proxy, &proto)) {
    return false;
  }
  if (!proto) {
    *found = false;
    return true;
  }

  bool hasProp;
  if (!JS_HasPropertyById(cx, proto, id, &hasProp)) {
    return false;
  }

  *found = hasProp;
  if (!hasProp || !vp) {
    return true;
  }

  JS::Rooted<JS::Value> value(cx);
  if (!JS_ForwardGetPropertyTo(cx, proto, id, proxy, &value)) {
    return false;
  }

  *vp = value;
  return true;
}

bool
HasPropertyOnPrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id)
{
  JS::Rooted<JSObject*> obj(cx, proxy);
  Maybe<JSAutoCompartment> ac;
  if (xpc::WrapperFactory::IsXrayWrapper(obj)) {
    obj = js::UncheckedUnwrap(obj);
    ac.construct(cx, obj);
  }

  bool found;
  // We ignore an error from GetPropertyOnPrototype.  We pass nullptr
  // for vp so that GetPropertyOnPrototype won't actually do a get.
  return !GetPropertyOnPrototype(cx, obj, id, &found, nullptr) || found;
}

bool
AppendNamedPropertyIds(JSContext* cx, JS::Handle<JSObject*> proxy,
                       nsTArray<nsString>& names,
                       bool shadowPrototypeProperties,
                       JS::AutoIdVector& props)
{
  for (uint32_t i = 0; i < names.Length(); ++i) {
    JS::Rooted<JS::Value> v(cx);
    if (!xpc::NonVoidStringToJsval(cx, names[i], &v)) {
      return false;
    }

    JS::Rooted<jsid> id(cx);
    if (!JS_ValueToId(cx, v, &id)) {
      return false;
    }

    if (shadowPrototypeProperties || !HasPropertyOnPrototype(cx, proxy, id)) {
      if (!props.append(id)) {
        return false;
      }
    }
  }

  return true;
}

bool
DictionaryBase::ParseJSON(JSContext* aCx,
                          const nsAString& aJSON,
                          JS::MutableHandle<JS::Value> aVal)
{
  if (aJSON.IsEmpty()) {
    return true;
  }
  return JS_ParseJSON(aCx,
                      static_cast<const jschar*>(PromiseFlatString(aJSON).get()),
                      aJSON.Length(), aVal);
}

static JSString*
ConcatJSString(JSContext* cx, const char* pre, JS::Handle<JSString*> str, const char* post)
{
  if (!str) {
    return nullptr;
  }

  JS::Rooted<JSString*> preString(cx, JS_NewStringCopyN(cx, pre, strlen(pre)));
  JS::Rooted<JSString*> postString(cx, JS_NewStringCopyN(cx, post, strlen(post)));
  if (!preString || !postString) {
    return nullptr;
  }

  preString = JS_ConcatStrings(cx, preString, str);
  if (!preString) {
    return nullptr;
  }

  return JS_ConcatStrings(cx, preString, postString);
}

bool
NativeToString(JSContext* cx, JS::Handle<JSObject*> wrapper,
               JS::Handle<JSObject*> obj, const char* pre,
               const char* post,
               JS::MutableHandle<JS::Value> v)
{
  JS::Rooted<JSPropertyDescriptor> toStringDesc(cx);
  toStringDesc.object().set(nullptr);
  toStringDesc.setAttributes(0);
  toStringDesc.setGetter(nullptr);
  toStringDesc.setSetter(nullptr);
  toStringDesc.value().set(JS::UndefinedValue());
  JS::Rooted<jsid> id(cx,
    nsXPConnect::GetRuntimeInstance()->GetStringID(XPCJSRuntime::IDX_TO_STRING));
  if (!XrayResolveNativeProperty(cx, wrapper, obj, id, &toStringDesc)) {
    return false;
  }

  JS::Rooted<JSString*> str(cx);
  {
    JSAutoCompartment ac(cx, obj);
    if (toStringDesc.object()) {
      JS::Rooted<JS::Value> toString(cx, toStringDesc.value());
      if (!JS_WrapValue(cx, &toString)) {
        return false;
      }
      MOZ_ASSERT(JS_ObjectIsCallable(cx, &toString.toObject()));
      JS::Rooted<JS::Value> toStringResult(cx);
      if (JS_CallFunctionValue(cx, obj, toString, JS::HandleValueArray::empty(),
                               &toStringResult)) {
        str = toStringResult.toString();
      } else {
        str = nullptr;
      }
    } else {
      const js::Class* clasp = js::GetObjectClass(obj);
      if (IsDOMClass(clasp)) {
        str = JS_NewStringCopyZ(cx, clasp->name);
        str = ConcatJSString(cx, "[object ", str, "]");
      } else if (IsDOMIfaceAndProtoClass(clasp)) {
        const DOMIfaceAndProtoJSClass* ifaceAndProtoJSClass =
          DOMIfaceAndProtoJSClass::FromJSClass(clasp);
        str = JS_NewStringCopyZ(cx, ifaceAndProtoJSClass->mToString);
      } else {
        MOZ_ASSERT(JS_IsNativeFunction(obj, Constructor));
        JS::Rooted<JSFunction*> fun(cx, JS_GetObjectFunction(obj));
        str = JS_DecompileFunction(cx, fun, 0);
      }
      str = ConcatJSString(cx, pre, str, post);
    }
  }

  if (!str) {
    return false;
  }

  v.setString(str);
  return JS_WrapValue(cx, v);
}

// Dynamically ensure that two objects don't end up with the same reserved slot.
class MOZ_STACK_CLASS AutoCloneDOMObjectSlotGuard
{
public:
  AutoCloneDOMObjectSlotGuard(JSContext* aCx, JSObject* aOld, JSObject* aNew)
    : mOldReflector(aCx, aOld), mNewReflector(aCx, aNew)
  {
    MOZ_ASSERT(js::GetReservedSlot(aOld, DOM_OBJECT_SLOT) ==
                 js::GetReservedSlot(aNew, DOM_OBJECT_SLOT));
  }

  ~AutoCloneDOMObjectSlotGuard()
  {
    if (js::GetReservedSlot(mOldReflector, DOM_OBJECT_SLOT).toPrivate()) {
      js::SetReservedSlot(mNewReflector, DOM_OBJECT_SLOT,
                          JS::PrivateValue(nullptr));
    }
  }

private:
  JS::Rooted<JSObject*> mOldReflector;
  JS::Rooted<JSObject*> mNewReflector;
};

nsresult
ReparentWrapper(JSContext* aCx, JS::Handle<JSObject*> aObjArg)
{
  js::AssertSameCompartment(aCx, aObjArg);

  // Check if we're near the stack limit before we get anywhere near the
  // transplanting code.
  JS_CHECK_RECURSION(aCx, return NS_ERROR_FAILURE);

  JS::Rooted<JSObject*> aObj(aCx, aObjArg);
  const DOMJSClass* domClass = GetDOMClass(aObj);

  JS::Rooted<JSObject*> oldParent(aCx, JS_GetParent(aObj));
  JS::Rooted<JSObject*> newParent(aCx, domClass->mGetParent(aCx, aObj));

  JSAutoCompartment oldAc(aCx, oldParent);

  JSCompartment* oldCompartment = js::GetObjectCompartment(oldParent);
  JSCompartment* newCompartment = js::GetObjectCompartment(newParent);
  if (oldCompartment == newCompartment) {
    if (!JS_SetParent(aCx, aObj, newParent)) {
      MOZ_CRASH();
    }
    return NS_OK;
  }

  // Telemetry.
  xpc::RecordDonatedNode(oldCompartment);
  xpc::RecordAdoptedNode(newCompartment);

  nsISupports* native = UnwrapDOMObjectToISupports(aObj);
  if (!native) {
    return NS_OK;
  }

  bool isProxy = js::IsProxy(aObj);
  JS::Rooted<JSObject*> expandoObject(aCx);
  if (isProxy) {
    expandoObject = DOMProxyHandler::GetAndClearExpandoObject(aObj);
  }

  JSAutoCompartment newAc(aCx, newParent);

  // First we clone the reflector. We get a copy of its properties and clone its
  // expando chain. The only part that is dangerous here is that if we have to
  // return early we must avoid ending up with two reflectors pointing to the
  // same native. Other than that, the objects we create will just go away.

  JS::Rooted<JSObject*> global(aCx,
                               js::GetGlobalForObjectCrossCompartment(newParent));
  JS::Handle<JSObject*> proto = (domClass->mGetProto)(aCx, global);
  if (!proto) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> newobj(aCx, JS_CloneObject(aCx, aObj, proto, newParent));
  if (!newobj) {
    return NS_ERROR_FAILURE;
  }

  js::SetReservedSlot(newobj, DOM_OBJECT_SLOT,
                      js::GetReservedSlot(aObj, DOM_OBJECT_SLOT));

  // At this point, both |aObj| and |newobj| point to the same native
  // which is bad, because one of them will end up being finalized with a
  // native it does not own. |cloneGuard| ensures that if we exit before
  // clearing |aObj|'s reserved slot the reserved slot of |newobj| will be
  // set to null. |aObj| will go away soon, because we swap it with
  // another object during the transplant and let that object die.
  JS::Rooted<JSObject*> propertyHolder(aCx);
  {
    AutoCloneDOMObjectSlotGuard cloneGuard(aCx, aObj, newobj);

    JS::Rooted<JSObject*> copyFrom(aCx, isProxy ? expandoObject : aObj);
    if (copyFrom) {
      propertyHolder = JS_NewObjectWithGivenProto(aCx, nullptr, JS::NullPtr(),
                                                  newParent);
      if (!propertyHolder) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      if (!JS_CopyPropertiesFrom(aCx, propertyHolder, copyFrom)) {
        return NS_ERROR_FAILURE;
      }
    } else {
      propertyHolder = nullptr;
    }

    // Expandos from other compartments are attached to the target JS object.
    // Copy them over, and let the old ones die a natural death.
    if (!xpc::XrayUtils::CloneExpandoChain(aCx, newobj, aObj)) {
      return NS_ERROR_FAILURE;
    }

    // We've set up |newobj|, so we make it own the native by nulling
    // out the reserved slot of |obj|.
    //
    // NB: It's important to do this _after_ copying the properties to
    // propertyHolder. Otherwise, an object with |foo.x === foo| will
    // crash when JS_CopyPropertiesFrom tries to call wrap() on foo.x.
    js::SetReservedSlot(aObj, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));
  }

  aObj = xpc::TransplantObject(aCx, aObj, newobj);
  if (!aObj) {
    MOZ_CRASH();
  }

  nsWrapperCache* cache = nullptr;
  CallQueryInterface(native, &cache);
  bool preserving = cache->PreservingWrapper();
  cache->SetPreservingWrapper(false);
  cache->SetWrapper(aObj);
  cache->SetPreservingWrapper(preserving);

  if (propertyHolder) {
    JS::Rooted<JSObject*> copyTo(aCx);
    if (isProxy) {
      copyTo = DOMProxyHandler::EnsureExpandoObject(aCx, aObj);
    } else {
      copyTo = aObj;
    }

    if (!copyTo || !JS_CopyPropertiesFrom(aCx, copyTo, propertyHolder)) {
      MOZ_CRASH();
    }
  }

  nsObjectLoadingContent* htmlobject;
  nsresult rv = UNWRAP_OBJECT(HTMLObjectElement, aObj, htmlobject);
  if (NS_FAILED(rv)) {
    rv = UnwrapObject<prototypes::id::HTMLEmbedElement,
                      HTMLSharedObjectElement>(aObj, htmlobject);
    if (NS_FAILED(rv)) {
      rv = UnwrapObject<prototypes::id::HTMLAppletElement,
                        HTMLSharedObjectElement>(aObj, htmlobject);
      if (NS_FAILED(rv)) {
        htmlobject = nullptr;
      }
    }
  }
  if (htmlobject) {
    htmlobject->SetupProtoChain(aCx, aObj);
  }

  // Now we can just fix up the parent and return the wrapper

  if (newParent && !JS_SetParent(aCx, aObj, newParent)) {
    MOZ_CRASH();
  }

  return NS_OK;
}

GlobalObject::GlobalObject(JSContext* aCx, JSObject* aObject)
  : mGlobalJSObject(aCx),
    mCx(aCx),
    mGlobalObject(nullptr)
{
  MOZ_ASSERT(mCx);
  JS::Rooted<JSObject*> obj(aCx, aObject);
  if (js::IsWrapper(obj)) {
    obj = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);
    if (!obj) {
      // We should never end up here on a worker thread, since there shouldn't
      // be any security wrappers to worry about.
      if (!MOZ_LIKELY(NS_IsMainThread())) {
        MOZ_CRASH();
      }

      Throw(aCx, NS_ERROR_XPC_SECURITY_MANAGER_VETO);
      return;
    }
  }

  mGlobalJSObject = js::GetGlobalForObjectCrossCompartment(obj);
}

nsISupports*
GlobalObject::GetAsSupports() const
{
  if (mGlobalObject) {
    return mGlobalObject;
  }

  if (!NS_IsMainThread()) {
    mGlobalObject = UnwrapDOMObjectToISupports(mGlobalJSObject);
    return mGlobalObject;
  }

  JS::Rooted<JS::Value> val(mCx, JS::ObjectValue(*mGlobalJSObject));

  // Switch this to UnwrapDOMObjectToISupports once our global objects are
  // using new bindings.
  nsresult rv = xpc_qsUnwrapArg<nsISupports>(mCx, val, &mGlobalObject,
                                             static_cast<nsISupports**>(getter_AddRefs(mGlobalObjectRef)),
                                             &val);
  if (NS_FAILED(rv)) {
    mGlobalObject = nullptr;
    Throw(mCx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  return mGlobalObject;
}

bool
InterfaceHasInstance(JSContext* cx, JS::Handle<JSObject*> obj,
                     JS::Handle<JSObject*> instance,
                     bool* bp)
{
  const DOMIfaceAndProtoJSClass* clasp =
    DOMIfaceAndProtoJSClass::FromJSClass(js::GetObjectClass(obj));

  const DOMJSClass* domClass = GetDOMClass(js::UncheckedUnwrap(instance, /* stopAtOuter = */ false));

  MOZ_ASSERT(!domClass || clasp->mPrototypeID != prototypes::id::_ID_Count,
             "Why do we have a hasInstance hook if we don't have a prototype "
             "ID?");

  if (domClass &&
      domClass->mInterfaceChain[clasp->mDepth] == clasp->mPrototypeID) {
    *bp = true;
    return true;
  }

  JS::Rooted<JSObject*> unwrapped(cx, js::CheckedUnwrap(instance, true));
  if (unwrapped && jsipc::IsCPOW(unwrapped)) {
    bool boolp = false;
    if (!jsipc::DOMInstanceOf(cx, unwrapped, clasp->mPrototypeID,
                                                clasp->mDepth, &boolp)) {
      return false;
    }
    *bp = boolp;
    return true;
  }

  JS::Rooted<JS::Value> protov(cx);
  DebugOnly<bool> ok = JS_GetProperty(cx, obj, "prototype", &protov);
  MOZ_ASSERT(ok, "Someone messed with our prototype property?");

  JS::Rooted<JSObject*> interfacePrototype(cx, &protov.toObject());
  MOZ_ASSERT(IsDOMIfaceAndProtoClass(js::GetObjectClass(interfacePrototype)),
             "Someone messed with our prototype property?");

  JS::Rooted<JSObject*> proto(cx);
  if (!JS_GetPrototype(cx, instance, &proto)) {
    return false;
  }

  while (proto) {
    if (proto == interfacePrototype) {
      *bp = true;
      return true;
    }

    if (!JS_GetPrototype(cx, proto, &proto)) {
      return false;
    }
  }

  *bp = false;
  return true;
}

bool
InterfaceHasInstance(JSContext* cx, JS::Handle<JSObject*> obj, JS::MutableHandle<JS::Value> vp,
                     bool* bp)
{
  if (!vp.isObject()) {
    *bp = false;
    return true;
  }

  JS::Rooted<JSObject*> instanceObject(cx, &vp.toObject());
  return InterfaceHasInstance(cx, obj, instanceObject, bp);
}

bool
InterfaceHasInstance(JSContext* cx, int prototypeID, int depth,
                     JS::Handle<JSObject*> instance,
                     bool* bp)
{
  const DOMJSClass* domClass = GetDOMClass(js::UncheckedUnwrap(instance));

  MOZ_ASSERT(!domClass || prototypeID != prototypes::id::_ID_Count,
             "Why do we have a hasInstance hook if we don't have a prototype "
             "ID?");

  *bp = (domClass && domClass->mInterfaceChain[depth] == prototypeID);
  return true;
}

bool
ReportLenientThisUnwrappingFailure(JSContext* cx, JSObject* obj)
{
  JS::Rooted<JSObject*> rootedObj(cx, obj);
  GlobalObject global(cx, rootedObj);
  if (global.Failed()) {
    return false;
  }
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(global.GetAsSupports());
  if (window && window->GetDoc()) {
    window->GetDoc()->WarnOnceAbout(nsIDocument::eLenientThis);
  }
  return true;
}

bool
GetWindowForJSImplementedObject(JSContext* cx, JS::Handle<JSObject*> obj,
                                nsPIDOMWindow** window)
{
  // Be very careful to not get tricked here.
  MOZ_ASSERT(NS_IsMainThread());
  if (!xpc::AccessCheck::isChrome(js::GetObjectCompartment(obj))) {
    NS_RUNTIMEABORT("Should have a chrome object here");
  }

  // Look up the content-side object.
  JS::Rooted<JS::Value> domImplVal(cx);
  if (!JS_GetProperty(cx, obj, "__DOM_IMPL__", &domImplVal)) {
    return false;
  }

  if (!domImplVal.isObject()) {
    ThrowErrorMessage(cx, MSG_NOT_OBJECT, "Value");
    return false;
  }

  // Go ahead and get the global from it.  GlobalObject will handle
  // doing unwrapping as needed.
  GlobalObject global(cx, &domImplVal.toObject());
  if (global.Failed()) {
    return false;
  }

  // It's OK if we have null here: that just means the content-side
  // object really wasn't associated with any window.
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(global.GetAsSupports()));
  win.forget(window);
  return true;
}

already_AddRefed<nsPIDOMWindow>
ConstructJSImplementation(JSContext* aCx, const char* aContractId,
                          const GlobalObject& aGlobal,
                          JS::MutableHandle<JSObject*> aObject,
                          ErrorResult& aRv)
{
  // Get the window to use as a parent and for initialization.
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  ConstructJSImplementation(aCx, aContractId, window, aObject, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }
  return window.forget();
}

void
ConstructJSImplementation(JSContext* aCx, const char* aContractId,
                          nsPIDOMWindow* aWindow,
                          JS::MutableHandle<JSObject*> aObject,
                          ErrorResult& aRv)
{
  // Make sure to divorce ourselves from the calling JS while creating and
  // initializing the object, so exceptions from that will get reported
  // properly, since those are never exceptions that a spec wants to be thrown.
  {
    AutoNoJSAPI nojsapi;

    // Get the XPCOM component containing the JS implementation.
    nsCOMPtr<nsISupports> implISupports = do_CreateInstance(aContractId);
    if (!implISupports) {
      NS_WARNING("Failed to get JS implementation for contract");
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    // Initialize the object, if it implements nsIDOMGlobalPropertyInitializer.
    nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi =
      do_QueryInterface(implISupports);
    if (gpi) {
      JS::Rooted<JS::Value> initReturn(aCx);
      nsresult rv = gpi->Init(aWindow, &initReturn);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return;
      }
      // With JS-implemented WebIDL, the return value of init() is not used to determine
      // if init() failed, so init() should only return undefined. Any kind of permission
      // or pref checking must happen by adding an attribute to the WebIDL interface.
      if (!initReturn.isUndefined()) {
        MOZ_ASSERT(false, "The init() method for JS-implemented WebIDL should not return anything");
        MOZ_CRASH();
      }
    }
    // Extract the JS implementation from the XPCOM object.
    nsCOMPtr<nsIXPConnectWrappedJS> implWrapped =
      do_QueryInterface(implISupports);
    MOZ_ASSERT(implWrapped, "Failed to get wrapped JS from XPCOM component.");
    if (!implWrapped) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    aObject.set(implWrapped->GetJSObject());
    if (!aObject) {
      aRv.Throw(NS_ERROR_FAILURE);
    }
  }
}

bool
NonVoidByteStringToJsval(JSContext *cx, const nsACString &str,
                         JS::MutableHandle<JS::Value> rval)
{
    // ByteStrings are not UTF-8 encoded.
    JSString* jsStr = JS_NewStringCopyN(cx, str.Data(), str.Length());

    if (!jsStr)
        return false;

    rval.setString(jsStr);
    return true;
}

bool
ConvertJSValueToByteString(JSContext* cx, JS::Handle<JS::Value> v,
                           JS::MutableHandle<JS::Value> pval, bool nullable,
                           nsACString& result)
{
  JS::Rooted<JSString*> s(cx);
  if (v.isString()) {
    s = v.toString();
  } else {

    if (nullable && v.isNullOrUndefined()) {
      result.SetIsVoid(true);
      return true;
    }

    s = JS::ToString(cx, v);
    if (!s) {
      return false;
    }
    pval.set(JS::StringValue(s));  // Root the new string.
  }

  size_t length;
  const jschar *chars = JS_GetStringCharsZAndLength(cx, s, &length);
  if (!chars) {
    return false;
  }

  // Conversion from Javascript string to ByteString is only valid if all
  // characters < 256.
  for (size_t i = 0; i < length; i++) {
    if (chars[i] > 255) {
      // The largest unsigned 64 bit number (18,446,744,073,709,551,615) has
      // 20 digits, plus one more for the null terminator.
      char index[21];
      static_assert(sizeof(size_t) <= 8, "index array too small");
      PR_snprintf(index, sizeof(index), "%d", i);
      // A jschar is 16 bits long.  The biggest unsigned 16 bit
      // number (65,535) has 5 digits, plus one more for the null
      // terminator.
      char badChar[6];
      static_assert(sizeof(jschar) <= 2, "badChar array too small");
      PR_snprintf(badChar, sizeof(badChar), "%d", chars[i]);
      ThrowErrorMessage(cx, MSG_INVALID_BYTESTRING, index, badChar);
      return false;
    }
  }

  if (length >= UINT32_MAX) {
    return false;
  }
  result.SetCapacity(length+1);
  JS_EncodeStringToBuffer(cx, s, result.BeginWriting(), length);
  result.BeginWriting()[length] = '\0';
  result.SetLength(length);

  return true;
}

bool
IsInPrivilegedApp(JSContext* aCx, JSObject* aObj)
{
  using mozilla::dom::workers::GetWorkerPrivateFromContext;
  if (!NS_IsMainThread()) {
    return GetWorkerPrivateFromContext(aCx)->IsInPrivilegedApp();
  }

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aObj);
  uint16_t appStatus = principal->GetAppStatus();
  return (appStatus == nsIPrincipal::APP_STATUS_CERTIFIED ||
          appStatus == nsIPrincipal::APP_STATUS_PRIVILEGED) ||
          Preferences::GetBool("dom.ignore_webidl_scope_checks", false);
}

bool
IsInCertifiedApp(JSContext* aCx, JSObject* aObj)
{
  using mozilla::dom::workers::GetWorkerPrivateFromContext;
  if (!NS_IsMainThread()) {
    return GetWorkerPrivateFromContext(aCx)->IsInCertifiedApp();
  }

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aObj);
  return principal->GetAppStatus() == nsIPrincipal::APP_STATUS_CERTIFIED ||
         Preferences::GetBool("dom.ignore_webidl_scope_checks", false);
}

#ifdef DEBUG
void
VerifyTraceProtoAndIfaceCacheCalled(JSTracer *trc, void **thingp,
                                    JSGCTraceKind kind)
{
    // We don't do anything here, we only want to verify that
    // TraceProtoAndIfaceCache was called.
}
#endif

void
FinalizeGlobal(JSFreeOp* aFreeOp, JSObject* aObj)
{
  MOZ_ASSERT(js::GetObjectClass(aObj)->flags & JSCLASS_DOM_GLOBAL);
  mozilla::dom::DestroyProtoAndIfaceCache(aObj);
}

bool
ResolveGlobal(JSContext* aCx, JS::Handle<JSObject*> aObj,
              JS::Handle<jsid> aId, JS::MutableHandle<JSObject*> aObjp)
{
  bool resolved;
  if (!JS_ResolveStandardClass(aCx, aObj, aId, &resolved)) {
    return false;
  }

  aObjp.set(resolved ? aObj.get() : nullptr);
  return true;
}

bool
EnumerateGlobal(JSContext* aCx, JS::Handle<JSObject*> aObj)
{
  return JS_EnumerateStandardClasses(aCx, aObj);
}

bool
CheckPermissions(JSContext* aCx, JSObject* aObj, const char* const aPermissions[])
{
  JS::Rooted<JSObject*> rootedObj(aCx, aObj);
  nsPIDOMWindow* window = xpc::WindowGlobalOrNull(rootedObj);
  if (!window) {
    return false;
  }

  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, false);

  do {
    uint32_t permission = nsIPermissionManager::DENY_ACTION;
    permMgr->TestPermissionFromWindow(window, *aPermissions, &permission);
    if (permission == nsIPermissionManager::ALLOW_ACTION) {
      return true;
    }
  } while (*(++aPermissions));
  return false;
}

bool
GenericBindingGetter(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    return ThrowInvalidThis(cx, args,
                            MSG_GETTER_THIS_DOES_NOT_IMPLEMENT_INTERFACE,
                            protoID);
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  void* self;
  {
    nsresult rv = UnwrapObject<void>(obj, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      return ThrowInvalidThis(cx, args,
                              GetInvalidThisErrorForGetter(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO),
                              protoID);
    }
  }

  MOZ_ASSERT(info->type() == JSJitInfo::Getter);
  JSJitGetterOp getter = info->getter;
  bool ok = getter(cx, obj, self, JSJitGetterCallArgs(args));
#ifdef DEBUG
  if (ok) {
    AssertReturnTypeMatchesJitinfo(info, args.rval());
  }
#endif
  return ok;
}

bool
GenericBindingSetter(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    return ThrowInvalidThis(cx, args,
                            MSG_SETTER_THIS_DOES_NOT_IMPLEMENT_INTERFACE,
                            protoID);
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  void* self;
  {
    nsresult rv = UnwrapObject<void>(obj, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      return ThrowInvalidThis(cx, args,
                              GetInvalidThisErrorForSetter(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO),
                              protoID);
    }
  }
  if (args.length() == 0) {
    return ThrowNoSetterArg(cx, protoID);
  }
  MOZ_ASSERT(info->type() == JSJitInfo::Setter);
  JSJitSetterOp setter = info->setter;
  if (!setter(cx, obj, self, JSJitSetterCallArgs(args))) {
    return false;
  }
  args.rval().setUndefined();
#ifdef DEBUG
  AssertReturnTypeMatchesJitinfo(info, args.rval());
#endif
  return true;
}

bool
GenericBindingMethod(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    return ThrowInvalidThis(cx, args,
                            MSG_METHOD_THIS_DOES_NOT_IMPLEMENT_INTERFACE,
                            protoID);
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  void* self;
  {
    nsresult rv = UnwrapObject<void>(obj, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      return ThrowInvalidThis(cx, args,
                              GetInvalidThisErrorForMethod(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO),
                              protoID);
    }
  }
  MOZ_ASSERT(info->type() == JSJitInfo::Method);
  JSJitMethodOp method = info->method;
  bool ok = method(cx, obj, self, JSJitMethodCallArgs(args));
#ifdef DEBUG
  if (ok) {
    AssertReturnTypeMatchesJitinfo(info, args.rval());
  }
#endif
  return ok;
}

bool
GenericPromiseReturningBindingMethod(JSContext* cx, unsigned argc, JS::Value* vp)
{
  // Make sure to save the callee before someone maybe messes with rval().
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> callee(cx, &args.callee());

  // We could invoke GenericBindingMethod here, but that involves an
  // extra call.  Manually inline it instead.
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    ThrowInvalidThis(cx, args,
                     MSG_METHOD_THIS_DOES_NOT_IMPLEMENT_INTERFACE,
                     protoID);
    return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                     args.rval());
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  void* self;
  {
    nsresult rv = UnwrapObject<void>(obj, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      ThrowInvalidThis(cx, args,
                       GetInvalidThisErrorForMethod(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO),
                       protoID);
      return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                       args.rval());
    }
  }
  MOZ_ASSERT(info->type() == JSJitInfo::Method);
  JSJitMethodOp method = info->method;
  bool ok = method(cx, obj, self, JSJitMethodCallArgs(args));
  if (ok) {
#ifdef DEBUG
    AssertReturnTypeMatchesJitinfo(info, args.rval());
#endif
    return true;
  }

  // Promise-returning methods always return objects
  MOZ_ASSERT(info->returnType() == JSVAL_TYPE_OBJECT);
  return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                   args.rval());
}

bool
StaticMethodPromiseWrapper(JSContext* cx, unsigned argc, JS::Value* vp)
{
  // Make sure to save the callee before someone maybe messes with rval().
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> callee(cx, &args.callee());

  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  MOZ_ASSERT(info);
  MOZ_ASSERT(info->type() == JSJitInfo::StaticMethod);

  bool ok = info->staticMethod(cx, argc, vp);
  if (ok) {
    return true;
  }

  return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                   args.rval());
}

bool
ConvertExceptionToPromise(JSContext* cx,
                          JSObject* promiseScope,
                          JS::MutableHandle<JS::Value> rval)
{
  GlobalObject global(cx, promiseScope);
  if (global.Failed()) {
    return false;
  }

  JS::Rooted<JS::Value> exn(cx);
  if (!JS_GetPendingException(cx, &exn)) {
    return false;
  }

  JS_ClearPendingException(cx);
  ErrorResult rv;
  nsRefPtr<Promise> promise = Promise::Reject(global, exn, rv);
  if (rv.Failed()) {
    // We just give up.  Make sure to not leak memory on the
    // ErrorResult, but then just put the original exception back.
    ThrowMethodFailedWithDetails(cx, rv, "", "");
    JS_SetPendingException(cx, exn);
    return false;
  }

  return WrapNewBindingObject(cx, promise, rval);
}

/* static */
void
CreateGlobalOptions<nsGlobalWindow>::TraceGlobal(JSTracer* aTrc, JSObject* aObj)
{
  mozilla::dom::TraceProtoAndIfaceCache(aTrc, aObj);

  // We might be called from a GC during the creation of a global, before we've
  // been able to set up the compartment private or the XPCWrappedNativeScope,
  // so we need to null-check those.
  xpc::CompartmentPrivate* compartmentPrivate = xpc::GetCompartmentPrivate(aObj);
  if (compartmentPrivate && compartmentPrivate->scope) {
    compartmentPrivate->scope->TraceSelf(aTrc);
  }
}

/* static */
bool
CreateGlobalOptions<nsGlobalWindow>::PostCreateGlobal(JSContext* aCx,
                                                      JS::Handle<JSObject*> aGlobal)
{
  return XPCWrappedNativeScope::GetNewOrUsed(aCx, aGlobal);
}

#ifdef DEBUG
void
AssertReturnTypeMatchesJitinfo(const JSJitInfo* aJitInfo,
                               JS::Handle<JS::Value> aValue)
{
  switch (aJitInfo->returnType()) {
  case JSVAL_TYPE_UNKNOWN:
    // Any value is good.
    break;
  case JSVAL_TYPE_DOUBLE:
    // The value could actually be an int32 value as well.
    MOZ_ASSERT(aValue.isNumber());
    break;
  case JSVAL_TYPE_INT32:
    MOZ_ASSERT(aValue.isInt32());
    break;
  case JSVAL_TYPE_UNDEFINED:
    MOZ_ASSERT(aValue.isUndefined());
    break;
  case JSVAL_TYPE_BOOLEAN:
    MOZ_ASSERT(aValue.isBoolean());
    break;
  case JSVAL_TYPE_STRING:
    MOZ_ASSERT(aValue.isString());
    break;
  case JSVAL_TYPE_NULL:
    MOZ_ASSERT(aValue.isNull());
    break;
  case JSVAL_TYPE_OBJECT:
    MOZ_ASSERT(aValue.isObject());
    break;
  default:
    // Someone messed up their jitinfo type.
    MOZ_ASSERT(false, "Unexpected JSValueType stored in jitinfo");
    break;
  }
}
#endif

} // namespace dom
} // namespace mozilla
