/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BindingDeclarations.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/SanitizerBinding.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"
#include "Sanitizer.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Sanitizer, mGlobal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Sanitizer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Sanitizer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Sanitizer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* Sanitizer::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return Sanitizer_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<Sanitizer> Sanitizer::New(nsIGlobalObject* aGlobal,
                                           const SanitizerConfig& aConfig,
                                           ErrorResult& aRv) {
  RefPtr<Sanitizer> sanitizer = new Sanitizer(aGlobal);
  return sanitizer.forget();
}

/* static */
already_AddRefed<Sanitizer> Sanitizer::New(nsIGlobalObject* aGlobal,
                                           const SanitizerPresets aConfig,
                                           ErrorResult& aRv) {
  RefPtr<Sanitizer> sanitizer = new Sanitizer(aGlobal);
  return sanitizer.forget();
}

/* static */
already_AddRefed<Sanitizer> Sanitizer::Constructor(
    const GlobalObject& aGlobal,
    const SanitizerConfigOrSanitizerPresets& aConfig, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (aConfig.IsSanitizerConfig()) {
    return New(global, aConfig.GetAsSanitizerConfig(), aRv);
  }
  return New(global, aConfig.GetAsSanitizerPresets(), aRv);
}

void Sanitizer::Get(SanitizerConfig& aConfig) {}

void Sanitizer::AllowElement(
    const StringOrSanitizerElementNamespaceWithAttributes& aElement) {}
void Sanitizer::RemoveElement(
    const StringOrSanitizerElementNamespace& aElement) {}
void Sanitizer::ReplaceElementWithChildren(
    const StringOrSanitizerElementNamespace& aElement) {}
void Sanitizer::AllowAttribute(
    const StringOrSanitizerAttributeNamespace& aAttribute) {}
void Sanitizer::RemoveAttribute(
    const StringOrSanitizerAttributeNamespace& aAttribute) {}
void Sanitizer::SetComments(bool aAllow) {}
void Sanitizer::SetDataAttributes(bool aAllow) {}
void Sanitizer::RemoveUnsafe() {}

RefPtr<DocumentFragment> Sanitizer::SanitizeFragment(
    RefPtr<DocumentFragment> aFragment, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (!window || !window->GetDoc()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  // FIXME(freddyb)
  // (how) can we assert that the supplied doc is indeed inert?
  // TODO: Sanitize
  return aFragment.forget();
}

/* ------ Logging ------ */

void Sanitizer::LogLocalizedString(const char* aName,
                                   const nsTArray<nsString>& aParams,
                                   uint32_t aFlags) {
  uint64_t innerWindowID = 0;
  bool isPrivateBrowsing = true;
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (window && window->GetDoc()) {
    auto* doc = window->GetDoc();
    innerWindowID = doc->InnerWindowID();
    isPrivateBrowsing = doc->IsInPrivateBrowsing();
  }
  nsAutoString logMsg;
  nsContentUtils::FormatLocalizedString(nsContentUtils::eSECURITY_PROPERTIES,
                                        aName, aParams, logMsg);
  LogMessage(logMsg, aFlags, innerWindowID, isPrivateBrowsing);
}

/* static */
void Sanitizer::LogMessage(const nsAString& aMessage, uint32_t aFlags,
                           uint64_t aInnerWindowID, bool aFromPrivateWindow) {
  // Prepending 'Sanitizer' to the outgoing console message
  nsString message;
  message.AppendLiteral(u"Sanitizer: ");
  message.Append(aMessage);

  // Allow for easy distinction in devtools code.
  constexpr auto category = "Sanitizer"_ns;

  if (aInnerWindowID > 0) {
    // Send to content console
    nsContentUtils::ReportToConsoleByWindowID(message, aFlags, category,
                                              aInnerWindowID);
  } else {
    // Send to browser console
    nsContentUtils::LogSimpleConsoleError(message, category, aFromPrivateWindow,
                                          true /* from chrome context */,
                                          aFlags);
  }
}

}  // namespace mozilla::dom
