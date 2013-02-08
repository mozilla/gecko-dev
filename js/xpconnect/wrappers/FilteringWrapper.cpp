/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et tw=99 ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilteringWrapper.h"
#include "AccessCheck.h"
#include "WaiveXrayWrapper.h"
#include "ChromeObjectWrapper.h"
#include "XrayWrapper.h"
#include "WrapperFactory.h"

#include "XPCWrapper.h"

#include "jsapi.h"

using namespace js;

namespace xpc {

template <typename Base, typename Policy>
FilteringWrapper<Base, Policy>::FilteringWrapper(unsigned flags) : Base(flags)
{
}

template <typename Base, typename Policy>
FilteringWrapper<Base, Policy>::~FilteringWrapper()
{
}

typedef Wrapper::Permission Permission;

static const Permission PermitObjectAccess = Wrapper::PermitObjectAccess;
static const Permission PermitPropertyAccess = Wrapper::PermitPropertyAccess;
static const Permission DenyAccess = Wrapper::DenyAccess;

template <typename Policy>
static bool
Filter(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    size_t w = 0;
    for (size_t n = 0; n < props.length(); ++n) {
        jsid id = props[n];
        Permission perm;
        if (!Policy::check(cx, wrapper, id, Wrapper::GET, perm))
            return false; // Error
        if (perm != DenyAccess)
            props[w++] = id;
    }
    props.resize(w);
    return true;
}

template <typename Policy>
static void
FilterSetter(JSContext *cx, JSObject *wrapper, jsid id, js::PropertyDescriptor *desc)
{
    JSErrorReporter reporter = JS_SetErrorReporter(cx, NULL);
    Permission perm = DenyAccess;
    bool setAllowed = Policy::check(cx, wrapper, id, Wrapper::SET, perm);
    JS_ASSERT_IF(setAllowed, perm != DenyAccess);
    if (!setAllowed || JS_IsExceptionPending(cx)) {
        // On branch, we don't have a good way to differentiate between exceptions
        // we want to throw and exceptions we want to squash. Squash them all.
        JS_ClearPendingException(cx);
        desc->setter = nullptr;
    }
    JS_SetErrorReporter(cx, reporter);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                      bool set, js::PropertyDescriptor *desc)
{
    if (!Base::getPropertyDescriptor(cx, wrapper, id, set, desc))
        return false;
    FilterSetter<Policy>(cx, wrapper, id, desc);
    return true;
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                         bool set, js::PropertyDescriptor *desc)
{
    if (!Base::getOwnPropertyDescriptor(cx, wrapper, id, set, desc))
        return false;
    FilterSetter<Policy>(cx, wrapper, id, desc);
    return true;
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    return Base::getOwnPropertyNames(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    return Base::enumerate(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    return Base::keys(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::iterate(JSContext *cx, JSObject *wrapper, unsigned flags, Value *vp)
{
    // We refuse to trigger the iterator hook across chrome wrappers because
    // we don't know how to censor custom iterator objects. Instead we trigger
    // the default proxy iterate trap, which will ask enumerate() for the list
    // of (censored) ids.
    return js::BaseProxyHandler::iterate(cx, wrapper, flags, vp);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enter(JSContext *cx, JSObject *wrapper, jsid id,
                                      Wrapper::Action act, bool *bp)
{
    Permission perm;
    if (!Policy::check(cx, wrapper, id, act, perm)) {
        *bp = false;
        return false;
    }
    *bp = true;
    if (perm == DenyAccess)
        return false;
    return Base::enter(cx, wrapper, id, act, bp);
}

#define SOW FilteringWrapper<CrossCompartmentSecurityWrapper, OnlyIfSubjectIsSystem>
#define SCSOW FilteringWrapper<SameCompartmentSecurityWrapper, OnlyIfSubjectIsSystem>
#define XOW FilteringWrapper<SecurityXrayXPCWN, CrossOriginAccessiblePropertiesOnly>
#define PXOW FilteringWrapper<SecurityXrayProxy, CrossOriginAccessiblePropertiesOnly>
#define DXOW   FilteringWrapper<SecurityXrayDOM, CrossOriginAccessiblePropertiesOnly>
#define NNXOW FilteringWrapper<CrossCompartmentSecurityWrapper, CrossOriginAccessiblePropertiesOnly>
#define LW    FilteringWrapper<SCSecurityXrayXPCWN, LocationPolicy>
#define XLW   FilteringWrapper<SecurityXrayXPCWN, LocationPolicy>
#define CW FilteringWrapper<SameCompartmentSecurityWrapper, ComponentsObjectPolicy>
#define XCW FilteringWrapper<CrossCompartmentSecurityWrapper, ComponentsObjectPolicy>

template<> SOW SOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                              WrapperFactory::SOW_FLAG);
template<> SCSOW SCSOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                                  WrapperFactory::SOW_FLAG);
template<> XOW XOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                              WrapperFactory::PARTIALLY_TRANSPARENT);
template<> PXOW PXOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                                WrapperFactory::PARTIALLY_TRANSPARENT);
template<> DXOW DXOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                                WrapperFactory::PARTIALLY_TRANSPARENT);
template<> NNXOW NNXOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                                  WrapperFactory::PARTIALLY_TRANSPARENT);
template<> LW  LW::singleton(WrapperFactory::SHADOWING_FORBIDDEN);
template<> XLW XLW::singleton(WrapperFactory::SHADOWING_FORBIDDEN);

template<> CW CW::singleton(0);
template<> XCW XCW::singleton(0);

template class SOW;
template class XOW;
template class PXOW;
template class DXOW;
template class NNXOW;
template class LW;
template class XLW;
template class ChromeObjectWrapperBase;
}
