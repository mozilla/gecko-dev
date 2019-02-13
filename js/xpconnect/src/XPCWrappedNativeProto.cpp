/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Shared proto object for XPCWrappedNative. */

#include "xpcprivate.h"
#include "pratom.h"

using namespace mozilla;

#ifdef DEBUG
int32_t XPCWrappedNativeProto::gDEBUG_LiveProtoCount = 0;
#endif

XPCWrappedNativeProto::XPCWrappedNativeProto(XPCWrappedNativeScope* Scope,
                                             nsIClassInfo* ClassInfo,
                                             uint32_t ClassInfoFlags,
                                             XPCNativeSet* Set)
    : mScope(Scope),
      mJSProtoObject(nullptr),
      mClassInfo(ClassInfo),
      mClassInfoFlags(ClassInfoFlags),
      mSet(Set),
      mScriptableInfo(nullptr)
{
    // This native object lives as long as its associated JSObject - killed
    // by finalization of the JSObject (or explicitly if Init fails).

    MOZ_COUNT_CTOR(XPCWrappedNativeProto);
    MOZ_ASSERT(mScope);

#ifdef DEBUG
    PR_ATOMIC_INCREMENT(&gDEBUG_LiveProtoCount);
#endif
}

XPCWrappedNativeProto::~XPCWrappedNativeProto()
{
    MOZ_ASSERT(!mJSProtoObject, "JSProtoObject still alive");

    MOZ_COUNT_DTOR(XPCWrappedNativeProto);

#ifdef DEBUG
    PR_ATOMIC_DECREMENT(&gDEBUG_LiveProtoCount);
#endif

    // Note that our weak ref to mScope is not to be trusted at this point.

    XPCNativeSet::ClearCacheEntryForClassInfo(mClassInfo);

    delete mScriptableInfo;
}

bool
XPCWrappedNativeProto::Init(const XPCNativeScriptableCreateInfo* scriptableCreateInfo,
                            bool callPostCreatePrototype)
{
    AutoJSContext cx;
    nsIXPCScriptable* callback = scriptableCreateInfo ?
                                 scriptableCreateInfo->GetCallback() :
                                 nullptr;
    if (callback) {
        mScriptableInfo =
            XPCNativeScriptableInfo::Construct(scriptableCreateInfo);
        if (!mScriptableInfo)
            return false;
    }

    const js::Class* jsclazz;

    if (mScriptableInfo) {
        const XPCNativeScriptableFlags& flags(mScriptableInfo->GetFlags());

        if (flags.AllowPropModsToPrototype()) {
            jsclazz = flags.WantCall() ?
                &XPC_WN_ModsAllowed_WithCall_Proto_JSClass :
                &XPC_WN_ModsAllowed_NoCall_Proto_JSClass;
        } else {
            jsclazz = flags.WantCall() ?
                &XPC_WN_NoMods_WithCall_Proto_JSClass :
                &XPC_WN_NoMods_NoCall_Proto_JSClass;
        }
    } else {
        jsclazz = &XPC_WN_NoMods_NoCall_Proto_JSClass;
    }

    JS::RootedObject global(cx, mScope->GetGlobalJSObject());
    JS::RootedObject proto(cx, JS_GetObjectPrototype(cx, global));
    mJSProtoObject = JS_NewObjectWithUniqueType(cx, js::Jsvalify(jsclazz),
                                                proto);

    bool success = !!mJSProtoObject;
    if (success) {
        JS_SetPrivate(mJSProtoObject, this);
        if (callPostCreatePrototype)
            success = CallPostCreatePrototype();
    }

    return success;
}

bool
XPCWrappedNativeProto::CallPostCreatePrototype()
{
    AutoJSContext cx;

    // Nothing to do if we don't have a scriptable callback.
    nsIXPCScriptable* callback = mScriptableInfo ? mScriptableInfo->GetCallback()
                                                 : nullptr;
    if (!callback)
        return true;

    // Call the helper. This can handle being called if it's not implemented,
    // so we don't have to check any sort of "want" here. See xpc_map_end.h.
    nsresult rv = callback->PostCreatePrototype(cx, mJSProtoObject);
    if (NS_FAILED(rv)) {
        JS_SetPrivate(mJSProtoObject, nullptr);
        mJSProtoObject = nullptr;
        XPCThrower::Throw(rv, cx);
        return false;
    }

    return true;
}

void
XPCWrappedNativeProto::JSProtoObjectFinalized(js::FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(obj == mJSProtoObject, "huh?");

    // Only remove this proto from the map if it is the one in the map.
    ClassInfo2WrappedNativeProtoMap* map = GetScope()->GetWrappedNativeProtoMap();
    if (map->Find(mClassInfo) == this)
        map->Remove(mClassInfo);

    GetRuntime()->GetDetachedWrappedNativeProtoMap()->Remove(this);
    GetRuntime()->GetDyingWrappedNativeProtoMap()->Add(this);

    mJSProtoObject.finalize(js::CastToJSFreeOp(fop)->runtime());
}

void
XPCWrappedNativeProto::JSProtoObjectMoved(JSObject* obj, const JSObject* old)
{
    MOZ_ASSERT(mJSProtoObject == old);
    mJSProtoObject.init(obj); // Update without triggering barriers.
}

void
XPCWrappedNativeProto::SystemIsBeingShutDown()
{
    // Note that the instance might receive this call multiple times
    // as we walk to here from various places.

    if (mJSProtoObject) {
        // short circuit future finalization
        JS_SetPrivate(mJSProtoObject, nullptr);
        mJSProtoObject = nullptr;
    }
}

// static
XPCWrappedNativeProto*
XPCWrappedNativeProto::GetNewOrUsed(XPCWrappedNativeScope* scope,
                                    nsIClassInfo* classInfo,
                                    const XPCNativeScriptableCreateInfo* scriptableCreateInfo,
                                    bool callPostCreatePrototype)
{
    AutoJSContext cx;
    MOZ_ASSERT(scope, "bad param");
    MOZ_ASSERT(classInfo, "bad param");

    AutoMarkingWrappedNativeProtoPtr proto(cx);
    ClassInfo2WrappedNativeProtoMap* map = nullptr;

    uint32_t ciFlags;
    if (NS_FAILED(classInfo->GetFlags(&ciFlags)))
        ciFlags = 0;

    map = scope->GetWrappedNativeProtoMap();
    proto = map->Find(classInfo);
    if (proto)
        return proto;

    AutoMarkingNativeSetPtr set(cx);
    set = XPCNativeSet::GetNewOrUsed(classInfo);
    if (!set)
        return nullptr;

    proto = new XPCWrappedNativeProto(scope, classInfo, ciFlags, set);

    if (!proto || !proto->Init(scriptableCreateInfo, callPostCreatePrototype)) {
        delete proto.get();
        return nullptr;
    }

    map->Add(classInfo, proto);

    return proto;
}

void
XPCWrappedNativeProto::DebugDump(int16_t depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("XPCWrappedNativeProto @ %x", this));
    XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("gDEBUG_LiveProtoCount is %d", gDEBUG_LiveProtoCount));
        XPC_LOG_ALWAYS(("mScope @ %x", mScope));
        XPC_LOG_ALWAYS(("mJSProtoObject @ %x", mJSProtoObject.get()));
        XPC_LOG_ALWAYS(("mSet @ %x", mSet));
        XPC_LOG_ALWAYS(("mScriptableInfo @ %x", mScriptableInfo));
        if (depth && mScriptableInfo) {
            XPC_LOG_INDENT();
            XPC_LOG_ALWAYS(("mScriptable @ %x", mScriptableInfo->GetCallback()));
            XPC_LOG_ALWAYS(("mFlags of %x", (uint32_t)mScriptableInfo->GetFlags()));
            XPC_LOG_ALWAYS(("mJSClass @ %x", mScriptableInfo->GetJSClass()));
            XPC_LOG_OUTDENT();
        }
    XPC_LOG_OUTDENT();
#endif
}


