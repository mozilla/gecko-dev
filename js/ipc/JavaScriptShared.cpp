/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JavaScriptShared.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/TabChild.h"
#include "jsfriendapi.h"
#include "xpcprivate.h"
#include "mozilla/Preferences.h"

using namespace js;
using namespace JS;
using namespace mozilla;
using namespace mozilla::jsipc;

IdToObjectMap::IdToObjectMap()
  : table_(SystemAllocPolicy())
{
}

bool
IdToObjectMap::init()
{
    if (table_.initialized())
        return true;
    return table_.init(32);
}

void
IdToObjectMap::trace(JSTracer *trc)
{
    for (Table::Range r(table_.all()); !r.empty(); r.popFront()) {
        DebugOnly<JSObject *> prior = r.front().value().get();
        JS_CallHeapObjectTracer(trc, &r.front().value(), "ipc-object");
        MOZ_ASSERT(r.front().value() == prior);
    }
}

void
IdToObjectMap::finalize(JSFreeOp *fop)
{
    for (Table::Enum e(table_); !e.empty(); e.popFront()) {
        DebugOnly<JSObject *> prior = e.front().value().get();
        if (JS_IsAboutToBeFinalized(&e.front().value()))
            e.removeFront();
        else
            MOZ_ASSERT(e.front().value() == prior);
    }
}

JSObject *
IdToObjectMap::find(ObjectId id)
{
    Table::Ptr p = table_.lookup(id);
    if (!p)
        return nullptr;
    return p->value();
}

bool
IdToObjectMap::add(ObjectId id, JSObject *obj)
{
    return table_.put(id, obj);
}

void
IdToObjectMap::remove(ObjectId id)
{
    table_.remove(id);
}

ObjectToIdMap::ObjectToIdMap()
  : table_(nullptr)
{
}

ObjectToIdMap::~ObjectToIdMap()
{
    if (table_) {
        dom::AddForDeferredFinalization<Table, nsAutoPtr>(table_);
        table_ = nullptr;
    }
}

bool
ObjectToIdMap::init()
{
    if (table_)
        return true;

    table_ = new Table(SystemAllocPolicy());
    return table_ && table_->init(32);
}

void
ObjectToIdMap::finalize(JSFreeOp *fop)
{
    for (Table::Enum e(*table_); !e.empty(); e.popFront()) {
        JSObject *obj = e.front().key();
        if (JS_IsAboutToBeFinalizedUnbarriered(&obj))
            e.removeFront();
        else
            MOZ_ASSERT(obj == e.front().key());
    }
}

ObjectId
ObjectToIdMap::find(JSObject *obj)
{
    Table::Ptr p = table_->lookup(obj);
    if (!p)
        return 0;
    return p->value();
}

bool
ObjectToIdMap::add(JSContext *cx, JSObject *obj, ObjectId id)
{
    if (!table_->put(obj, id))
        return false;
    JS_StoreObjectPostBarrierCallback(cx, keyMarkCallback, obj, table_);
    return true;
}

/*
 * This function is called during minor GCs for each key in the HashMap that has
 * been moved.
 */
/* static */ void
ObjectToIdMap::keyMarkCallback(JSTracer *trc, JSObject *key, void *data)
{
    Table *table = static_cast<Table*>(data);
    JSObject *prior = key;
    JS_CallObjectTracer(trc, &key, "ObjectIdCache::table_ key");
    table->rekeyIfMoved(prior, key);
}

void
ObjectToIdMap::remove(JSObject *obj)
{
    table_->remove(obj);
}

bool JavaScriptShared::sLoggingInitialized;
bool JavaScriptShared::sLoggingEnabled;
bool JavaScriptShared::sStackLoggingEnabled;

JavaScriptShared::JavaScriptShared(JSRuntime *rt)
  : rt_(rt),
    refcount_(1),
    lastId_(0)
{
    if (!sLoggingInitialized) {
        sLoggingInitialized = true;
        Preferences::AddBoolVarCache(&sLoggingEnabled,
                                     "dom.ipc.cpows.log.enabled", false);
        Preferences::AddBoolVarCache(&sStackLoggingEnabled,
                                     "dom.ipc.cpows.log.stack", false);
    }
}

bool
JavaScriptShared::init()
{
    if (!objects_.init())
        return false;
    if (!cpows_.init())
        return false;
    if (!objectIds_.init())
        return false;

    return true;
}

void
JavaScriptShared::decref()
{
    refcount_--;
    if (!refcount_)
        delete this;
}

void
JavaScriptShared::incref()
{
    refcount_++;
}

bool
JavaScriptShared::convertIdToGeckoString(JSContext *cx, JS::HandleId id, nsString *to)
{
    RootedValue idval(cx);
    if (!JS_IdToValue(cx, id, &idval))
        return false;

    RootedString str(cx, ToString(cx, idval));
    if (!str)
        return false;

    const jschar *chars = JS_GetStringCharsZ(cx, str);
    if (!chars)
        return false;

    *to = chars;
    return true;
}

bool
JavaScriptShared::convertGeckoStringToId(JSContext *cx, const nsString &from, JS::MutableHandleId to)
{
    RootedString str(cx, JS_NewUCStringCopyN(cx, from.BeginReading(), from.Length()));
    if (!str)
        return false;

    return JS_StringToId(cx, str, to);
}

bool
JavaScriptShared::toVariant(JSContext *cx, JS::HandleValue from, JSVariant *to)
{
    switch (JS_TypeOfValue(cx, from)) {
      case JSTYPE_VOID:
        *to = UndefinedVariant();
        return true;

      case JSTYPE_OBJECT:
      case JSTYPE_FUNCTION:
      {
        RootedObject obj(cx, from.toObjectOrNull());
        if (!obj) {
            MOZ_ASSERT(from == JSVAL_NULL);
            *to = NullVariant();
            return true;
        }

        if (xpc_JSObjectIsID(cx, obj)) {
            JSIID iid;
            const nsID *id = xpc_JSObjectToID(cx, obj);
            ConvertID(*id, &iid);
            *to = iid;
            return true;
        }

        ObjectVariant objVar;
        if (!toObjectVariant(cx, obj, &objVar))
            return false;
        *to = objVar;
        return true;
      }

      case JSTYPE_STRING:
      {
        nsDependentJSString dep;
        if (!dep.init(cx, from))
            return false;
        *to = dep;
        return true;
      }

      case JSTYPE_NUMBER:
        if (from.isInt32())
            *to = double(from.toInt32());
        else
            *to = from.toDouble();
        return true;

      case JSTYPE_BOOLEAN:
        *to = from.toBoolean();
        return true;

      default:
        MOZ_ASSERT(false);
        return false;
    }
}

bool
JavaScriptShared::fromVariant(JSContext *cx, const JSVariant &from, MutableHandleValue to)
{
    switch (from.type()) {
        case JSVariant::TUndefinedVariant:
          to.set(UndefinedValue());
          return true;

        case JSVariant::TNullVariant:
          to.set(NullValue());
          return true;

        case JSVariant::TObjectVariant:
        {
          JSObject *obj = fromObjectVariant(cx, from.get_ObjectVariant());
          if (!obj)
              return false;
          to.set(ObjectValue(*obj));
          return true;
        }

        case JSVariant::Tdouble:
          to.set(JS_NumberValue(from.get_double()));
          return true;

        case JSVariant::Tbool:
          to.set(BOOLEAN_TO_JSVAL(from.get_bool()));
          return true;

        case JSVariant::TnsString:
        {
          const nsString &old = from.get_nsString();
          JSString *str = JS_NewUCStringCopyN(cx, old.BeginReading(), old.Length());
          if (!str)
              return false;
          to.set(StringValue(str));
          return true;
        }

        case JSVariant::TJSIID:
        {
          nsID iid;
          const JSIID &id = from.get_JSIID();
          ConvertID(id, &iid);

          JSCompartment *compartment = GetContextCompartment(cx);
          RootedObject global(cx, JS_GetGlobalForCompartmentOrNull(cx, compartment));
          JSObject *obj = xpc_NewIDObject(cx, global, iid);
          if (!obj)
              return false;
          to.set(ObjectValue(*obj));
          return true;
        }

        default:
          return false;
    }
}

/* static */ void
JavaScriptShared::ConvertID(const nsID &from, JSIID *to)
{
    to->m0() = from.m0;
    to->m1() = from.m1;
    to->m2() = from.m2;
    to->m3_0() = from.m3[0];
    to->m3_1() = from.m3[1];
    to->m3_2() = from.m3[2];
    to->m3_3() = from.m3[3];
    to->m3_4() = from.m3[4];
    to->m3_5() = from.m3[5];
    to->m3_6() = from.m3[6];
    to->m3_7() = from.m3[7];
}

/* static */ void
JavaScriptShared::ConvertID(const JSIID &from, nsID *to)
{
    to->m0 = from.m0();
    to->m1 = from.m1();
    to->m2 = from.m2();
    to->m3[0] = from.m3_0();
    to->m3[1] = from.m3_1();
    to->m3[2] = from.m3_2();
    to->m3[3] = from.m3_3();
    to->m3[4] = from.m3_4();
    to->m3[5] = from.m3_5();
    to->m3[6] = from.m3_6();
    to->m3[7] = from.m3_7();
}

JSObject *
JavaScriptShared::findObjectById(JSContext *cx, uint32_t objId)
{
    RootedObject obj(cx, findObjectById(objId));
    if (!obj) {
        JS_ReportError(cx, "operation not possible on dead CPOW");
        return nullptr;
    }

    // Objects are stored in objects_ unwrapped. We want to wrap the object
    // before returning it so that all operations happen on Xray wrappers. If
    // the object is a DOM element, we try to obtain the corresponding
    // TabChildGlobal and wrap in that.
    RootedObject global(cx, GetGlobalForObjectCrossCompartment(obj));
    nsCOMPtr<nsIGlobalObject> nativeGlobal = xpc::GetNativeForGlobal(global);
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(nativeGlobal);
    if (window) {
        dom::TabChild *tabChild = dom::TabChild::GetFrom(window);
        if (tabChild) {
            nsCOMPtr<nsIContentFrameMessageManager> mm;
            tabChild->GetMessageManager(getter_AddRefs(mm));
            nsCOMPtr<nsIGlobalObject> tabChildNativeGlobal = do_QueryInterface(mm);
            RootedObject tabChildGlobal(cx, tabChildNativeGlobal->GetGlobalJSObject());
            JSAutoCompartment ac(cx, tabChildGlobal);
            if (!JS_WrapObject(cx, &obj))
                return nullptr;
            return obj;
        }
    }

    // If there's no TabChildGlobal, we use the junk scope.
    JSAutoCompartment ac(cx, xpc::GetJunkScope());
    if (!JS_WrapObject(cx, &obj))
        return nullptr;
    return obj;
}

static const uint64_t DefaultPropertyOp = 1;
static const uint64_t GetterOnlyPropertyStub = 2;
static const uint64_t UnknownPropertyOp = 3;

bool
JavaScriptShared::fromDescriptor(JSContext *cx, Handle<JSPropertyDescriptor> desc,
                                 PPropertyDescriptor *out)
{
    out->attrs() = desc.attributes();
    if (!toVariant(cx, desc.value(), &out->value()))
        return false;

    JS_ASSERT(desc.object());
    if (!toObjectVariant(cx, desc.object(), &out->obj()))
        return false;

    if (!desc.getter()) {
        out->getter() = 0;
    } else if (desc.hasGetterObject()) {
        JSObject *getter = desc.getterObject();
        ObjectVariant objVar;
        if (!toObjectVariant(cx, getter, &objVar))
            return false;
        out->getter() = objVar;
    } else {
        if (desc.getter() == JS_PropertyStub)
            out->getter() = DefaultPropertyOp;
        else
            out->getter() = UnknownPropertyOp;
    }

    if (!desc.setter()) {
        out->setter() = 0;
    } else if (desc.hasSetterObject()) {
        JSObject *setter = desc.setterObject();
        ObjectVariant objVar;
        if (!toObjectVariant(cx, setter, &objVar))
            return false;
        out->setter() = objVar;
    } else {
        if (desc.setter() == JS_StrictPropertyStub)
            out->setter() = DefaultPropertyOp;
        else if (desc.setter() == js_GetterOnlyPropertyStub)
            out->setter() = GetterOnlyPropertyStub;
        else
            out->setter() = UnknownPropertyOp;
    }

    return true;
}

bool
UnknownPropertyStub(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    JS_ReportError(cx, "getter could not be wrapped via CPOWs");
    return false;
}

bool
UnknownStrictPropertyStub(JSContext *cx, HandleObject obj, HandleId id, bool strict, MutableHandleValue vp)
{
    JS_ReportError(cx, "setter could not be wrapped via CPOWs");
    return false;
}

bool
JavaScriptShared::toDescriptor(JSContext *cx, const PPropertyDescriptor &in,
                               MutableHandle<JSPropertyDescriptor> out)
{
    out.setAttributes(in.attrs());
    if (!fromVariant(cx, in.value(), out.value()))
        return false;
    Rooted<JSObject*> obj(cx);
    obj = fromObjectVariant(cx, in.obj());
    if (!obj)
        return false;
    out.object().set(obj);

    if (in.getter().type() == GetterSetter::Tuint64_t && !in.getter().get_uint64_t()) {
        out.setGetter(nullptr);
    } else if (in.attrs() & JSPROP_GETTER) {
        Rooted<JSObject*> getter(cx);
        getter = fromObjectVariant(cx, in.getter().get_ObjectVariant());
        if (!getter)
            return false;
        out.setGetter(JS_DATA_TO_FUNC_PTR(JSPropertyOp, getter.get()));
    } else {
        if (in.getter().get_uint64_t() == DefaultPropertyOp)
            out.setGetter(JS_PropertyStub);
        else
            out.setGetter(UnknownPropertyStub);
    }

    if (in.setter().type() == GetterSetter::Tuint64_t && !in.setter().get_uint64_t()) {
        out.setSetter(nullptr);
    } else if (in.attrs() & JSPROP_SETTER) {
        Rooted<JSObject*> setter(cx);
        setter = fromObjectVariant(cx, in.setter().get_ObjectVariant());
        if (!setter)
            return false;
        out.setSetter(JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, setter.get()));
    } else {
        if (in.setter().get_uint64_t() == DefaultPropertyOp)
            out.setSetter(JS_StrictPropertyStub);
        else if (in.setter().get_uint64_t() == GetterOnlyPropertyStub)
            out.setSetter(js_GetterOnlyPropertyStub);
        else
            out.setSetter(UnknownStrictPropertyStub);
    }

    return true;
}

bool
CpowIdHolder::ToObject(JSContext *cx, JS::MutableHandleObject objp)
{
    return js_->Unwrap(cx, cpows_, objp);
}

bool
JavaScriptShared::Unwrap(JSContext *cx, const InfallibleTArray<CpowEntry> &aCpows,
                         JS::MutableHandleObject objp)
{
    objp.set(nullptr);

    if (!aCpows.Length())
        return true;

    RootedObject obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    if (!obj)
        return false;

    RootedValue v(cx);
    RootedString str(cx);
    for (size_t i = 0; i < aCpows.Length(); i++) {
        const nsString &name = aCpows[i].name();

        if (!fromVariant(cx, aCpows[i].value(), &v))
            return false;

        if (!JS_DefineUCProperty(cx,
                                 obj,
                                 name.BeginReading(),
                                 name.Length(),
                                 v,
                                 JSPROP_ENUMERATE))
        {
            return false;
        }
    }

    objp.set(obj);
    return true;
}

bool
JavaScriptShared::Wrap(JSContext *cx, HandleObject aObj, InfallibleTArray<CpowEntry> *outCpows)
{
    if (!aObj)
        return true;

    AutoIdArray ids(cx, JS_Enumerate(cx, aObj));
    if (!ids)
        return false;

    RootedId id(cx);
    RootedValue v(cx);
    for (size_t i = 0; i < ids.length(); i++) {
        id = ids[i];

        nsString str;
        if (!convertIdToGeckoString(cx, id, &str))
            return false;

        if (!JS_GetPropertyById(cx, aObj, id, &v))
            return false;

        JSVariant var;
        if (!toVariant(cx, v, &var))
            return false;

        outCpows->AppendElement(CpowEntry(str, var));
    }

    return true;
}

