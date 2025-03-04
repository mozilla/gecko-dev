/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BindingDeclarations.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/SanitizerBinding.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"
#include "nsNameSpaceManager.h"
#include "Sanitizer.h"

namespace mozilla::dom {

using namespace sanitizer;

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

// https://wicg.github.io/sanitizer-api/#sanitizer-constructor
/* static */
already_AddRefed<Sanitizer> Sanitizer::New(nsIGlobalObject* aGlobal,
                                           const SanitizerConfig& aConfig,
                                           ErrorResult& aRv) {
  RefPtr<Sanitizer> sanitizer = new Sanitizer(aGlobal);

  // Step 2. Let valid be the return value of setting configuration on this.
  sanitizer->SetConfig(aConfig, aRv);

  // Step 3. If valid is false, then throw a TypeError.
  if (aRv.Failed()) {
    return nullptr;
  }

  return sanitizer.forget();
}

/* static */
already_AddRefed<Sanitizer> Sanitizer::New(nsIGlobalObject* aGlobal,
                                           const SanitizerPresets aConfig,
                                           ErrorResult& aRv) {
  RefPtr<Sanitizer> sanitizer = new Sanitizer(aGlobal);
  // TODO: Default config
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

// https://wicg.github.io/sanitizer-api/#sanitizer-set-a-configuration
void Sanitizer::SetConfig(const SanitizerConfig& aConfig, ErrorResult& aRv) {
  // Step 1. For each element of configuration["elements"] do:
  if (aConfig.mElements.WasPassed()) {
    for (const auto& element : aConfig.mElements.Value()) {
      // Step 1.1. Call allow an element with element and sanitizer.
      AllowElement(element);
    }
  }

  // Step 2. For each element of configuration["removeElements"] do:
  if (aConfig.mRemoveElements.WasPassed()) {
    for (const auto& element : aConfig.mRemoveElements.Value()) {
      // Step 2.1. Call remove an element with element and sanitizer.
      RemoveElement(element);
    }
  }

  // Step 3. For each element of configuration["replaceWithChildrenElements"]
  // do:
  if (aConfig.mReplaceWithChildrenElements.WasPassed()) {
    for (const auto& element : aConfig.mReplaceWithChildrenElements.Value()) {
      // Step 3.1. Call replace an element with its children with element and
      // sanitizer.
      ReplaceElementWithChildren(element);
    }
  }

  // Step 4. For each attribute of configuration["attributes"] do:
  if (aConfig.mAttributes.WasPassed()) {
    for (const auto& attribute : aConfig.mAttributes.Value()) {
      // Step 4.1. Call allow an attribute with attribute and sanitizer.
      AllowAttribute(attribute);
    }
  }

  // Step 5. For each attribute of configuration["removeAttributes"] do:
  if (aConfig.mRemoveAttributes.WasPassed()) {
    for (const auto& attribute : aConfig.mRemoveAttributes.Value()) {
      // Step 5.1. Call remove an attribute with attribute and sanitizer.
      RemoveAttribute(attribute);
    }
  }

  // Step 6. Call set comments with configuration["comments"] and sanitizer.

  // TODO: This is changing in https://github.com/WICG/sanitizer-api/pull/254
  if (aConfig.mComments.WasPassed()) {
    SetComments(aConfig.mComments.Value());
  }

  // Step 7. Call set data attributes with configuration["dataAttributes"] and
  // sanitizer.
  if (aConfig.mDataAttributes.WasPassed()) {
    SetDataAttributes(aConfig.mDataAttributes.Value());
  }

  // Step 8. Return whether all of the following are true:

  auto isSameSize = [](const auto& aInputConfig, const auto& aProcessedConfig) {
    size_t sizeInput =
        aInputConfig.WasPassed() ? aInputConfig.Value().Length() : 0;
    size_t sizeProcessed = aProcessedConfig.Values().Length();
    return sizeInput == sizeProcessed;
  };

  // TODO: Better error messages. (e.g. show difference before and after?)

  // size of configuration["elements"] equals size of this’s
  // configuration["elements"].
  if (!isSameSize(aConfig.mElements, mElements)) {
    aRv.ThrowTypeError("'elements' changed");
    return;
  }

  // size of configuration["removeElements"] equals size of this’s
  // configuration["removeElements"].
  if (!isSameSize(aConfig.mRemoveElements, mRemoveElements)) {
    aRv.ThrowTypeError("'removeElements' changed");
    return;
  }

  // size of configuration["replaceWithChildrenElements"] equals size of this’s
  // configuration["replaceWithChildrenElements"].
  if (!isSameSize(aConfig.mReplaceWithChildrenElements,
                  mReplaceWithChildrenElements)) {
    aRv.ThrowTypeError("'replaceWithChildrenElements' changed");
    return;
  }

  // size of configuration["attributes"] equals size of this’s
  // configuration["attributes"].
  if (!isSameSize(aConfig.mAttributes, mAttributes)) {
    aRv.ThrowTypeError("'attributes' changed");
    return;
  }

  // size of configuration["removeAttributes"] equals size of this’s
  // configuration["removeAttributes"].
  if (!isSameSize(aConfig.mRemoveAttributes, mRemoveAttributes)) {
    aRv.ThrowTypeError("'removeAttributes' changed");
    return;
  }

  // Either configuration["elements"] or configuration["removeElements"] exist,
  // or neither, but not both.
  if (aConfig.mElements.WasPassed() && aConfig.mRemoveElements.WasPassed()) {
    aRv.ThrowTypeError(
        "'elements' and 'removeElements' are not allowed at the same time");
    return;
  }

  // Either configuration["attributes"] or configuration["removeAttributes"]
  // exist, or neither, but not both.
  if (aConfig.mAttributes.WasPassed() &&
      aConfig.mRemoveAttributes.WasPassed()) {
    aRv.ThrowTypeError(
        "'attributes' and 'removeAttributes' are not allowed at the same time");
    return;
  }
}

void Sanitizer::Get(SanitizerConfig& aConfig) {
  nsTArray<OwningStringOrSanitizerElementNamespaceWithAttributes> elements;
  for (const CanonicalElementWithAttributes& canonical : mElements.Values()) {
    elements.AppendElement()->SetAsSanitizerElementNamespaceWithAttributes() =
        canonical.ToSanitizerElementNamespaceWithAttributes();
  }
  aConfig.mElements.Construct(std::move(elements));

  nsTArray<OwningStringOrSanitizerElementNamespace> removeElements;
  for (const CanonicalName& canonical : mRemoveElements.Values()) {
    removeElements.AppendElement()->SetAsSanitizerElementNamespace() =
        canonical.ToSanitizerElementNamespace();
  }
  aConfig.mRemoveElements.Construct(std::move(removeElements));

  nsTArray<OwningStringOrSanitizerElementNamespace> replaceWithChildrenElements;
  for (const CanonicalName& canonical : mReplaceWithChildrenElements.Values()) {
    replaceWithChildrenElements.AppendElement()
        ->SetAsSanitizerElementNamespace() =
        canonical.ToSanitizerElementNamespace();
  }
  aConfig.mReplaceWithChildrenElements.Construct(
      std::move(replaceWithChildrenElements));

  aConfig.mAttributes.Construct(ToSanitizerAttributes(mAttributes));
  aConfig.mRemoveAttributes.Construct(ToSanitizerAttributes(mRemoveAttributes));

  aConfig.mComments.Construct(mComments);
  aConfig.mDataAttributes.Construct(mDataAttributes);
}

auto& GetAsSanitizerElementNamespace(
    const StringOrSanitizerElementNamespace& aElement) {
  return aElement.GetAsSanitizerElementNamespace();
}
auto& GetAsSanitizerElementNamespace(
    const OwningStringOrSanitizerElementNamespace& aElement) {
  return aElement.GetAsSanitizerElementNamespace();
}
auto& GetAsSanitizerElementNamespace(
    const StringOrSanitizerElementNamespaceWithAttributes& aElement) {
  return aElement.GetAsSanitizerElementNamespaceWithAttributes();
}
auto& GetAsSanitizerElementNamespace(
    const OwningStringOrSanitizerElementNamespaceWithAttributes& aElement) {
  return aElement.GetAsSanitizerElementNamespaceWithAttributes();
}

// https://wicg.github.io/sanitizer-api/#canonicalize-a-sanitizer-element
template <typename SanitizerElement>
static CanonicalName CanonicalizeElement(const SanitizerElement& aElement) {
  // return the result of canonicalize a sanitizer name with element and the
  // HTML namespace as the default namespace.

  // https://wicg.github.io/sanitizer-api/#canonicalize-a-sanitizer-name
  // Step 1. Assert: name is either a DOMString or a dictionary. (implicit)

  // Step 2. If name is a DOMString, then return «[ "name" → name, "namespace" →
  // defaultNamespace]».
  if (aElement.IsString()) {
    RefPtr<nsAtom> nameAtom = NS_AtomizeMainThread(aElement.GetAsString());
    return CanonicalName(nameAtom, nsGkAtoms::nsuri_xhtml);
  }

  // Step 3. Assert: name is a dictionary and name["name"] exists.
  const auto& elem = GetAsSanitizerElementNamespace(aElement);
  MOZ_ASSERT(!elem.mName.IsVoid());

  // Step 4. Return «[
  //  "name" → name["name"],
  //  "namespace" → ( name["namespace"] if it exists, otherwise defaultNamespace
  //  )
  // ]».
  RefPtr<nsAtom> nameAtom = NS_AtomizeMainThread(elem.mName);
  RefPtr<nsAtom> namespaceAtom;
  if (!elem.mNamespace.IsVoid()) {
    namespaceAtom = NS_AtomizeMainThread(elem.mNamespace);
  } else {
    namespaceAtom = nsGkAtoms::nsuri_xhtml;
  }
  return CanonicalName(nameAtom, namespaceAtom);
}

// https://wicg.github.io/sanitizer-api/#canonicalize-a-sanitizer-attribute
template <typename SanitizerAttribute>
static CanonicalName CanonicalizeAttribute(
    const SanitizerAttribute& aAttribute) {
  // return the result of canonicalize a sanitizer name with attribute and null
  // as the default namespace.

  // https://wicg.github.io/sanitizer-api/#canonicalize-a-sanitizer-name
  // Step 1. Assert: name is either a DOMString or a dictionary. (implicit)

  // Step 2. If name is a DOMString, then return «[ "name" → name, "namespace" →
  // defaultNamespace]».
  if (aAttribute.IsString()) {
    RefPtr<nsAtom> nameAtom = NS_AtomizeMainThread(aAttribute.GetAsString());
    return CanonicalName(nameAtom, nullptr);
  }

  // Step 3. Assert: name is a dictionary and name["name"] exists.
  const auto& attr = aAttribute.GetAsSanitizerAttributeNamespace();
  MOZ_ASSERT(!attr.mName.IsVoid());

  // Step 4. Return «[
  //  "name" → name["name"],
  //  "namespace" → ( name["namespace"] if it exists, otherwise defaultNamespace
  //  )
  // ]».
  RefPtr<nsAtom> nameAtom = NS_AtomizeMainThread(attr.mName);
  RefPtr<nsAtom> namespaceAtom = nullptr;
  if (!attr.mNamespace.IsVoid()) {
    namespaceAtom = NS_AtomizeMainThread(attr.mNamespace);
  }
  return CanonicalName(nameAtom, namespaceAtom);
}

// https://wicg.github.io/sanitizer-api/#canonicalize-a-sanitizer-element-with-attributes
template <typename SanitizerElementWithAttributes>
static CanonicalElementWithAttributes CanonicalizeElementWithAttributes(
    const SanitizerElementWithAttributes& aElement) {
  // Step 1. Let result be the result of canonicalize a sanitizer element with
  // element.
  CanonicalElementWithAttributes result =
      CanonicalElementWithAttributes(CanonicalizeElement(aElement));

  // Step 2. If element is a dictionary:
  if (aElement.IsSanitizerElementNamespaceWithAttributes()) {
    auto& elem = aElement.GetAsSanitizerElementNamespaceWithAttributes();

    // Step 2.1. For each attribute in element["attributes"]:
    if (elem.mAttributes.WasPassed()) {
      ListSet<CanonicalName> attributes;
      for (const auto& attribute : elem.mAttributes.Value()) {
        // Step 2.1.1. Add the result of canonicalize a sanitizer attribute with
        // attribute to result["attributes"].
        attributes.Insert(CanonicalizeAttribute(attribute));
      }
      result.mAttributes = Some(std::move(attributes));
    }

    // Step 2.2. For each attribute in element["removeAttributes"]:
    if (elem.mRemoveAttributes.WasPassed()) {
      ListSet<CanonicalName> attributes;
      for (const auto& attribute : elem.mRemoveAttributes.Value()) {
        // Step 2.2.1. Add the result of canonicalize a sanitizer attribute with
        // attribute to result["removeAttributes"].
        attributes.Insert(CanonicalizeAttribute(attribute));
      }
      result.mRemoveAttributes = Some(std::move(attributes));
    }
  }

  // Step 3. Return result.
  return result;
}

// https://wicg.github.io/sanitizer-api/#sanitizerconfig-allow-an-element
template <typename SanitizerElementWithAttributes>
void Sanitizer::AllowElement(const SanitizerElementWithAttributes& aElement) {
  // Step 1. Set element to the result of canonicalize a sanitizer element with
  // attributes with element.
  CanonicalElementWithAttributes element =
      CanonicalizeElementWithAttributes(aElement);

  // Step 2. Remove element from configuration["elements"].
  mElements.Remove(element);

  // Step 4. Remove element from configuration["removeElements"].
  mRemoveElements.Remove(element);

  // Step 5. Remove element from configuration["replaceWithChildrenElements"].
  mReplaceWithChildrenElements.Remove(element);

  // Step 3. Append element to configuration["elements"].
  mElements.Insert(std::move(element));
}

template void Sanitizer::AllowElement(
    const StringOrSanitizerElementNamespaceWithAttributes&);

// https://wicg.github.io/sanitizer-api/#sanitizer-remove-an-element
template <typename SanitizerElement>
void Sanitizer::RemoveElement(const SanitizerElement& aElement) {
  // Step 1. Set element to the result of canonicalize a sanitizer element with
  // element.
  CanonicalName element = CanonicalizeElement(aElement);

  // Step 3. Remove element from configuration["elements"] list.
  mElements.Remove(element);

  // Step 4. Remove element from configuration["replaceWithChildrenElements"].
  mReplaceWithChildrenElements.Remove(element);

  // Step 2. Add element to configuration["removeElements"].
  mRemoveElements.Insert(std::move(element));
}

template void Sanitizer::RemoveElement(
    const StringOrSanitizerElementNamespace&);

// https://wicg.github.io/sanitizer-api/#sanitizer-replace-an-element-with-its-children
template <typename SanitizerElement>
void Sanitizer::ReplaceElementWithChildren(const SanitizerElement& aElement) {
  // Step 1. Set element to the result of canonicalize a sanitizer element with
  // element.
  CanonicalName element = CanonicalizeElement(aElement);

  // Step 3. Remove element from configuration["removeElements"].
  mRemoveElements.Remove(element);

  // Step 4. Remove element from configuration["elements"] list.
  mElements.Remove(element);

  // Step 2. Add element to configuration["replaceWithChildrenElements"].
  mReplaceWithChildrenElements.Insert(std::move(element));
}

template void Sanitizer::ReplaceElementWithChildren(
    const StringOrSanitizerElementNamespace&);

// https://wicg.github.io/sanitizer-api/#sanitizer-allow-an-attribute
template <typename SanitizerAttribute>
void Sanitizer::AllowAttribute(const SanitizerAttribute& aAttribute) {
  // Step 1. Set attribute to the result of canonicalize a sanitizer attribute
  // with attribute.
  CanonicalName attribute = CanonicalizeAttribute(aAttribute);

  // Step 3. Remove attribute from configuration["removeAttributes"].
  mRemoveAttributes.Remove(attribute);

  // Step 2. Add attribute to configuration["attributes"].
  mAttributes.Insert(std::move(attribute));
}

template void Sanitizer::AllowAttribute(
    const StringOrSanitizerAttributeNamespace&);

// https://wicg.github.io/sanitizer-api/#sanitizer-remove-an-attribute
template <typename SanitizerAttribute>
void Sanitizer::RemoveAttribute(const SanitizerAttribute& aAttribute) {
  // Step 1. Set attribute to the result of canonicalize a sanitizer attribute
  // with attribute.
  CanonicalName attribute = CanonicalizeAttribute(aAttribute);

  // Step 3. Remove attribute from configuration["attributes"].
  mAttributes.Remove(attribute);

  // Step 2. Add attribute to configuration["removeAttributes"].
  mRemoveAttributes.Insert(std::move(attribute));
}

template void Sanitizer::RemoveAttribute(
    const StringOrSanitizerAttributeNamespace&);

void Sanitizer::SetComments(bool aAllow) { mComments = aAllow; }
void Sanitizer::SetDataAttributes(bool aAllow) { mDataAttributes = aAllow; }
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

  // XXX Sanitize

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
