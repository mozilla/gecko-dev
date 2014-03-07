/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement global service to track stack of JSContext. */

#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "nsDOMJSUtils.h"
#include "nsNullPrincipal.h"
#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace JS;
using namespace xpc;
using mozilla::dom::DestroyProtoAndIfaceCache;

/***************************************************************************/

XPCJSContextStack::~XPCJSContextStack()
{
    if (mSafeJSContext) {
        {
            JSAutoRequest ar(mSafeJSContext);
            JS_RemoveObjectRoot(mSafeJSContext, &mSafeJSContextGlobal);
        }
        mSafeJSContextGlobal = nullptr;
        JS_DestroyContextNoGC(mSafeJSContext);
        mSafeJSContext = nullptr;
    }
}

JSContext*
XPCJSContextStack::Pop()
{
    MOZ_ASSERT(!mStack.IsEmpty());

    uint32_t idx = mStack.Length() - 1; // The thing we're popping

    JSContext *cx = mStack[idx].cx;

    mStack.RemoveElementAt(idx);
    if (idx == 0) {
        js::Debug_SetActiveJSContext(mRuntime->Runtime(), nullptr);
        return cx;
    }

    --idx; // Advance to new top of the stack

    XPCJSContextInfo &e = mStack[idx];
    if (e.cx && e.savedFrameChain) {
        // Pop() can be called outside any request for e.cx.
        JSAutoRequest ar(e.cx);
        JS_RestoreFrameChain(e.cx);
        e.savedFrameChain = false;
    }
    js::Debug_SetActiveJSContext(mRuntime->Runtime(), e.cx);
    return cx;
}

bool
XPCJSContextStack::Push(JSContext *cx)
{
    js::Debug_SetActiveJSContext(mRuntime->Runtime(), cx);
    if (mStack.Length() == 0) {
        mStack.AppendElement(cx);
        return true;
    }

    XPCJSContextInfo &e = mStack[mStack.Length() - 1];
    if (e.cx) {
        // The cx we're pushing is also stack-top. In general we still need to
        // call JS_SaveFrameChain here. But if that would put us in a
        // compartment that's same-origin with the current one, we can skip it.
        nsIScriptSecurityManager* ssm = XPCWrapper::GetSecurityManager();
        if ((e.cx == cx) && ssm) {
            // DOM JSContexts don't store their default compartment object on
            // the cx, so in those cases we need to fetch it via the scx
            // instead. And in some cases (i.e. the SafeJSContext), we have no
            // default compartment object at all.
            RootedObject defaultScope(cx, GetDefaultScopeFromJSContext(cx));
            if (defaultScope) {
                nsIPrincipal *currentPrincipal =
                  GetCompartmentPrincipal(js::GetContextCompartment(cx));
                nsIPrincipal *defaultPrincipal = GetObjectPrincipal(defaultScope);
                if (currentPrincipal->Equals(defaultPrincipal)) {
                    mStack.AppendElement(cx);
                    return true;
                }
            }
        }

        {
            // Push() can be called outside any request for e.cx.
            JSAutoRequest ar(e.cx);
            if (!JS_SaveFrameChain(e.cx))
                return false;
            e.savedFrameChain = true;
        }
    }

    mStack.AppendElement(cx);
    return true;
}

bool
XPCJSContextStack::HasJSContext(JSContext *cx)
{
    for (uint32_t i = 0; i < mStack.Length(); i++)
        if (cx == mStack[i].cx)
            return true;
    return false;
}

static bool
SafeGlobalResolve(JSContext *cx, HandleObject obj, HandleId id)
{
    bool resolved;
    return JS_ResolveStandardClass(cx, obj, id, &resolved);
}

static void
SafeFinalize(JSFreeOp *fop, JSObject* obj)
{
    SandboxPrivate* sop =
        static_cast<SandboxPrivate*>(xpc_GetJSPrivate(obj));
    sop->ForgetGlobalObject();
    NS_IF_RELEASE(sop);
    DestroyProtoAndIfaceCache(obj);
}

const JSClass xpc::SafeJSContextGlobalClass = {
    "global_for_XPCJSContextStack_SafeJSContext",
    XPCONNECT_GLOBAL_FLAGS,
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, SafeGlobalResolve, JS_ConvertStub, SafeFinalize,
    nullptr, nullptr, nullptr, TraceXPCGlobal
};

JSContext*
XPCJSContextStack::GetSafeJSContext()
{
    MOZ_ASSERT(mSafeJSContext);
    return mSafeJSContext;
}

JSObject*
XPCJSContextStack::GetSafeJSContextGlobal()
{
    MOZ_ASSERT(mSafeJSContextGlobal);
    return mSafeJSContextGlobal;
}

JSContext*
XPCJSContextStack::InitSafeJSContext()
{
    MOZ_ASSERT(!mSafeJSContext);

    // Start by getting the principal holder and principal for this
    // context.  If we can't manage that, don't bother with the rest.
    nsRefPtr<nsNullPrincipal> principal = new nsNullPrincipal();
    nsresult rv = principal->Init();
    if (NS_FAILED(rv))
        MOZ_CRASH();

    nsXPConnect* xpc = nsXPConnect::XPConnect();
    JSRuntime *rt = xpc->GetRuntime()->Runtime();
    if (!rt)
        MOZ_CRASH();

    mSafeJSContext = JS_NewContext(rt, 8192);
    if (!mSafeJSContext)
        MOZ_CRASH();
    JSAutoRequest req(mSafeJSContext);
    ContextOptionsRef(mSafeJSContext).setNoDefaultCompartmentObject(true);
#ifdef DEBUG
    ContextOptionsRef(mSafeJSContext).setExtraWarnings(true);
#endif

    JS_SetErrorReporter(mSafeJSContext, xpc::SystemErrorReporter);

    JS::CompartmentOptions options;
    options.setZone(JS::SystemZone);
    mSafeJSContextGlobal = CreateGlobalObject(mSafeJSContext,
                                              &SafeJSContextGlobalClass,
                                              principal, options);
    if (!mSafeJSContextGlobal)
        MOZ_CRASH();
    JS_AddNamedObjectRoot(mSafeJSContext, &mSafeJSContextGlobal, "SafeJSContext global");

    // Note: make sure to set the private before calling
    // InitClasses
    nsRefPtr<SandboxPrivate> sp = new SandboxPrivate(principal, mSafeJSContextGlobal);
    JS_SetPrivate(mSafeJSContextGlobal, sp.forget().get());

    if (NS_FAILED(xpc->InitClasses(mSafeJSContext, mSafeJSContextGlobal)))
        MOZ_CRASH();

    JS::RootedObject glob(mSafeJSContext, mSafeJSContextGlobal);
    JS_FireOnNewGlobalObject(mSafeJSContext, glob);

    return mSafeJSContext;
}
