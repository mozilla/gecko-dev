/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ProxyObject.h"

#include "jscompartment.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

using namespace js;

/* static */ ProxyObject *
ProxyObject::New(JSContext *cx, BaseProxyHandler *handler, HandleValue priv, TaggedProto proto_,
                 JSObject *parent_, const ProxyOptions &options)
{
    Rooted<TaggedProto> proto(cx, proto_);
    RootedObject parent(cx, parent_);

    const Class *clasp = options.clasp();

    JS_ASSERT(isValidProxyClass(clasp));
    JS_ASSERT_IF(proto.isObject(), cx->compartment() == proto.toObject()->compartment());
    JS_ASSERT_IF(parent, cx->compartment() == parent->compartment());

    /*
     * Eagerly mark properties unknown for proxies, so we don't try to track
     * their properties and so that we don't need to walk the compartment if
     * their prototype changes later.  But don't do this for DOM proxies,
     * because we want to be able to keep track of them in typesets in useful
     * ways.
     */
    if (proto.isObject() && !options.singleton() && !clasp->isDOMClass()) {
        RootedObject protoObj(cx, proto.toObject());
        if (!JSObject::setNewTypeUnknown(cx, clasp, protoObj))
            return nullptr;
    }

    NewObjectKind newKind = options.singleton() ? SingletonObject : GenericObject;
    gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);

#if 0
    // Background finalization of proxies temporarily disabled. See bug 1008791
    if (handler->finalizeInBackground(priv))
        allocKind = GetBackgroundAllocKind(allocKind);
#endif
    RootedObject obj(cx, NewObjectWithGivenProto(cx, clasp, proto, parent, allocKind, newKind));
    if (!obj)
        return nullptr;

    Rooted<ProxyObject*> proxy(cx, &obj->as<ProxyObject>());
    proxy->initHandler(handler);
    proxy->initCrossCompartmentPrivate(priv);

    /* Don't track types of properties of non-DOM and non-singleton proxies. */
    if (newKind != SingletonObject && !clasp->isDOMClass())
        MarkTypeObjectUnknownProperties(cx, proxy->type());

    return proxy;
}

void
ProxyObject::initCrossCompartmentPrivate(HandleValue priv)
{
    initCrossCompartmentSlot(PRIVATE_SLOT, priv);
}

void
ProxyObject::initHandler(BaseProxyHandler *handler)
{
    initSlot(HANDLER_SLOT, PrivateValue(handler));
}

static void
NukeSlot(ProxyObject *proxy, uint32_t slot)
{
    Value old = proxy->getSlot(slot);
    if (old.isMarkable()) {
        Zone *zone = ZoneOfValue(old);
        AutoMarkInDeadZone amd(zone);
        proxy->setReservedSlot(slot, NullValue());
    } else {
        proxy->setReservedSlot(slot, NullValue());
    }
}

void
ProxyObject::nuke(BaseProxyHandler *handler)
{
    /* Allow people to add their own number of reserved slots beyond the expected 4 */
    unsigned numSlots = JSCLASS_RESERVED_SLOTS(getClass());
    for (unsigned i = 0; i < numSlots; i++)
        NukeSlot(this, i);
    /* Restore the handler as requested after nuking. */
    setHandler(handler);
}
