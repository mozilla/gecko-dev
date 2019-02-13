/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowNamedPropertiesHandler.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsDOMClassInfo.h"
#include "nsGlobalWindow.h"
#include "nsHTMLDocument.h"
#include "nsJSUtils.h"
#include "xpcprivate.h"

namespace mozilla {
namespace dom {

static bool
ShouldExposeChildWindow(nsString& aNameBeingResolved, nsIDOMWindow *aChild)
{
  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aChild);
  NS_ENSURE_TRUE(piWin, false);
  Element* e = piWin->GetFrameElementInternal();
  if (e && e->IsInShadowTree()) {
    return false;
  }

  // If we're same-origin with the child, go ahead and expose it.
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aChild);
  NS_ENSURE_TRUE(sop, false);
  if (nsContentUtils::SubjectPrincipal()->Equals(sop->GetPrincipal())) {
    return true;
  }

  // If we're not same-origin, expose it _only_ if the name of the browsing
  // context matches the 'name' attribute of the frame element in the parent.
  // The motivations behind this heuristic are worth explaining here.
  //
  // Historically, all UAs supported global named access to any child browsing
  // context (that is to say, window.dolske returns a child frame where either
  // the "name" attribute on the frame element was set to "dolske", or where
  // the child explicitly set window.name = "dolske").
  //
  // This is problematic because it allows possibly-malicious and unrelated
  // cross-origin subframes to pollute the global namespace of their parent in
  // unpredictable ways (see bug 860494). This is also problematic for browser
  // engines like Servo that want to run cross-origin script on different
  // threads.
  //
  // The naive solution here would be to filter out any cross-origin subframes
  // obtained when doing named lookup in global scope. But that is unlikely to
  // be web-compatible, since it will break named access for consumers that do
  // <iframe name="dolske" src="http://cross-origin.com/sadtrombone.html"> and
  // expect to be able to access the cross-origin subframe via named lookup on
  // the global.
  //
  // The optimal behavior would be to do the following:
  // (a) Look for any child browsing context with name="dolske".
  // (b) If the result is cross-origin, null it out.
  // (c) If we have null, look for a frame element whose 'name' attribute is
  //     "dolske".
  //
  // Unfortunately, (c) would require some engineering effort to be performant
  // in Gecko, and probably in other UAs as well. So we go with a simpler
  // approximation of the above. This approximation will only break sites that
  // rely on their cross-origin subframes setting window.name to a known value,
  // which is unlikely to be very common. And while it does introduce a
  // dependency on cross-origin state when doing global lookups, it doesn't
  // allow the child to arbitrarily pollute the parent namespace, and requires
  // cross-origin communication only in a limited set of cases that can be
  // computed independently by the parent.
  return e && e->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                             aNameBeingResolved, eCaseMatters);
}

bool
WindowNamedPropertiesHandler::getOwnPropDescriptor(JSContext* aCx,
                                                   JS::Handle<JSObject*> aProxy,
                                                   JS::Handle<jsid> aId,
                                                   bool /* unused */,
                                                   JS::MutableHandle<JSPropertyDescriptor> aDesc)
                                                   const
{
  if (!JSID_IS_STRING(aId)) {
    // Nothing to do if we're resolving a non-string property.
    return true;
  }

  bool hasOnPrototype;
  if (!HasPropertyOnPrototype(aCx, aProxy, aId, &hasOnPrototype)) {
    return false;
  }
  if (hasOnPrototype) {
    return true;
  }

  nsAutoJSString str;
  if (!str.init(aCx, JSID_TO_STRING(aId))) {
    return false;
  }

  // Grab the DOM window.
  JS::Rooted<JSObject*> global(aCx, JS_GetGlobalForObject(aCx, aProxy));
  nsGlobalWindow* win = xpc::WindowOrNull(global);
  if (win->Length() > 0) {
    nsCOMPtr<nsIDOMWindow> childWin = win->GetChildWindow(str);
    if (childWin && ShouldExposeChildWindow(str, childWin)) {
      // We found a subframe of the right name. Shadowing via |var foo| in
      // global scope is still allowed, since |var| only looks up |own|
      // properties. But unqualified shadowing will fail, per-spec.
      JS::Rooted<JS::Value> v(aCx);
      if (!WrapObject(aCx, childWin, &v)) {
        return false;
      }
      aDesc.object().set(aProxy);
      aDesc.value().set(v);
      aDesc.setAttributes(JSPROP_ENUMERATE);
      return true;
    }
  }

  // The rest of this function is for HTML documents only.
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(win->GetExtantDoc());
  if (!htmlDoc) {
    return true;
  }
  nsHTMLDocument* document = static_cast<nsHTMLDocument*>(htmlDoc.get());

  Element* element = document->GetElementById(str);
  if (element) {
    JS::Rooted<JS::Value> v(aCx);
    if (!WrapObject(aCx, element, &v)) {
      return false;
    }
    aDesc.object().set(aProxy);
    aDesc.value().set(v);
    aDesc.setAttributes(JSPROP_ENUMERATE);
    return true;
  }

  nsWrapperCache* cache;
  nsISupports* result = document->ResolveName(str, &cache);
  if (!result) {
    return true;
  }

  JS::Rooted<JS::Value> v(aCx);
  if (!WrapObject(aCx, result, cache, nullptr, &v)) {
    return false;
  }
  aDesc.object().set(aProxy);
  aDesc.value().set(v);
  aDesc.setAttributes(JSPROP_ENUMERATE);
  return true;
}

bool
WindowNamedPropertiesHandler::defineProperty(JSContext* aCx,
                                             JS::Handle<JSObject*> aProxy,
                                             JS::Handle<jsid> aId,
                                             JS::Handle<JSPropertyDescriptor> aDesc,
                                             JS::ObjectOpResult &result) const
{
  ErrorResult rv;
  rv.ThrowTypeError(MSG_DEFINEPROPERTY_ON_GSP);
  rv.ReportErrorWithMessage(aCx);
  return false;
}

bool
WindowNamedPropertiesHandler::ownPropNames(JSContext* aCx,
                                           JS::Handle<JSObject*> aProxy,
                                           unsigned flags,
                                           JS::AutoIdVector& aProps) const
{
  // Grab the DOM window.
  nsGlobalWindow* win = xpc::WindowOrNull(JS_GetGlobalForObject(aCx, aProxy));
  nsTArray<nsString> names;
  win->GetSupportedNames(names);
  // Filter out the ones we wouldn't expose from getOwnPropertyDescriptor.
  // We iterate backwards so we can remove things from the list easily.
  for (size_t i = names.Length(); i > 0; ) {
    --i; // Now we're pointing at the next name we want to look at
    nsIDOMWindow* childWin = win->GetChildWindow(names[i]);
    if (!childWin || !ShouldExposeChildWindow(names[i], childWin)) {
      names.RemoveElementAt(i);
    }
  }
  if (!AppendNamedPropertyIds(aCx, aProxy, names, false, aProps)) {
    return false;
  }

  names.Clear();
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(win->GetExtantDoc());
  if (!htmlDoc) {
    return true;
  }
  nsHTMLDocument* document = static_cast<nsHTMLDocument*>(htmlDoc.get());
  document->GetSupportedNames(flags, names);

  JS::AutoIdVector docProps(aCx);
  if (!AppendNamedPropertyIds(aCx, aProxy, names, false, docProps)) {
    return false;
  }

  return js::AppendUnique(aCx, aProps, docProps);
}

bool
WindowNamedPropertiesHandler::delete_(JSContext* aCx,
                                      JS::Handle<JSObject*> aProxy,
                                      JS::Handle<jsid> aId,
                                      JS::ObjectOpResult &aResult) const
{
  return aResult.failCantDeleteWindowNamedProperty();
}

static bool
ResolveWindowNamedProperty(JSContext* aCx, JS::Handle<JSObject*> aWrapper,
                           JS::Handle<JSObject*> aObj, JS::Handle<jsid> aId,
                           JS::MutableHandle<JSPropertyDescriptor> aDesc)
{
  {
    JSAutoCompartment ac(aCx, aObj);
    if (!js::GetProxyHandler(aObj)->getOwnPropertyDescriptor(aCx, aObj, aId,
                                                             aDesc)) {
      return false;
    }
  }

  if (aDesc.object()) {
    aDesc.object().set(aWrapper);

    return JS_WrapPropertyDescriptor(aCx, aDesc);
  }

  return true;
}

static bool
EnumerateWindowNamedProperties(JSContext* aCx, JS::Handle<JSObject*> aWrapper,
                               JS::Handle<JSObject*> aObj,
                               JS::AutoIdVector& aProps)
{
  JSAutoCompartment ac(aCx, aObj);
  return js::GetProxyHandler(aObj)->ownPropertyKeys(aCx, aObj, aProps);
}

const NativePropertyHooks sWindowNamedPropertiesNativePropertyHooks[] = { {
  ResolveWindowNamedProperty,
  EnumerateWindowNamedProperties,
  { nullptr, nullptr },
  prototypes::id::_ID_Count,
  constructors::id::_ID_Count,
  nullptr
} };

static const DOMIfaceAndProtoJSClass WindowNamedPropertiesClass = {
  PROXY_CLASS_DEF("WindowProperties",
                  JSCLASS_IS_DOMIFACEANDPROTOJSCLASS),
  eNamedPropertiesObject,
  sWindowNamedPropertiesNativePropertyHooks,
  "[object WindowProperties]",
  prototypes::id::_ID_Count,
  0,
  EventTargetBinding::GetProtoObject
};

// static
JSObject*
WindowNamedPropertiesHandler::Create(JSContext* aCx,
                                     JS::Handle<JSObject*> aProto)
{
  // Note: since the scope polluter proxy lives on the window's prototype
  // chain, it needs a singleton type to avoid polluting type information
  // for properties on the window.
  JS::Rooted<JSObject*> gsp(aCx);
  js::ProxyOptions options;
  options.setSingleton(true);
  options.setClass(&WindowNamedPropertiesClass.mBase);
  return js::NewProxyObject(aCx, WindowNamedPropertiesHandler::getInstance(),
                            JS::NullHandleValue, aProto,
                            options);
}

} // namespace dom
} // namespace mozilla
