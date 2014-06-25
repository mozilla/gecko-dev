/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/RegExpStatics.h"

#include "vm/RegExpStaticsObject.h"

#include "jsobjinlines.h"

using namespace js;

/*
 * RegExpStatics allocates memory -- in order to keep the statics stored
 * per-global and not leak, we create a js::Class to wrap the C++ instance and
 * provide an appropriate finalizer. We lazily create and store an instance of
 * that js::Class in a global reserved slot.
 */

static void
resc_finalize(FreeOp *fop, JSObject *obj)
{
    RegExpStatics *res = static_cast<RegExpStatics *>(obj->getPrivate());
    fop->delete_(res);
}

static void
resc_trace(JSTracer *trc, JSObject *obj)
{
    void *pdata = obj->getPrivate();
    JS_ASSERT(pdata);
    RegExpStatics *res = static_cast<RegExpStatics *>(pdata);
    res->mark(trc);
}

const Class RegExpStaticsObject::class_ = {
    "RegExpStatics",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    resc_finalize,
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    resc_trace
};

JSObject *
RegExpStatics::create(ExclusiveContext *cx, GlobalObject *parent)
{
    JSObject *obj = NewObjectWithGivenProto(cx, &RegExpStaticsObject::class_, nullptr, parent);
    if (!obj)
        return nullptr;
    RegExpStatics *res = cx->new_<RegExpStatics>();
    if (!res)
        return nullptr;
    obj->setPrivate(static_cast<void *>(res));
    return obj;
}

void
RegExpStatics::markFlagsSet(JSContext *cx)
{
    // Flags set on the RegExp function get propagated to constructed RegExp
    // objects, which interferes with optimizations that inline RegExp cloning
    // or avoid cloning entirely. Scripts making this assumption listen to
    // type changes on RegExp.prototype, so mark a state change to trigger
    // recompilation of all such code (when recompiling, a stub call will
    // always be performed).
    JS_ASSERT_IF(cx->global()->hasRegExpStatics(), this == cx->global()->getRegExpStatics(cx));

    types::MarkTypeObjectFlags(cx, cx->global(), types::OBJECT_FLAG_REGEXP_FLAGS_SET);
}

bool
RegExpStatics::executeLazy(JSContext *cx)
{
    if (!pendingLazyEvaluation)
        return true;

    JS_ASSERT(lazySource);
    JS_ASSERT(matchesInput);
    JS_ASSERT(lazyIndex != size_t(-1));

    /* Retrieve or create the RegExpShared in this compartment. */
    RegExpGuard g(cx);
    if (!cx->compartment()->regExps.get(cx, lazySource, lazyFlags, &g))
        return false;

    /*
     * It is not necessary to call aboutToWrite(): evaluation of
     * implicit copies is safe.
     */

    /* Execute the full regular expression. */
    RootedLinearString input(cx, matchesInput);
    RegExpRunStatus status = g->execute(cx, input, &this->lazyIndex, this->matches);
    if (status == RegExpRunStatus_Error)
        return false;

    /*
     * RegExpStatics are only updated on successful (matching) execution.
     * Re-running the same expression must therefore produce a matching result.
     */
    JS_ASSERT(status == RegExpRunStatus_Success);

    /* Unset lazy state and remove rooted values that now have no use. */
    pendingLazyEvaluation = false;
    lazySource = nullptr;
    lazyIndex = size_t(-1);

    return true;
}
