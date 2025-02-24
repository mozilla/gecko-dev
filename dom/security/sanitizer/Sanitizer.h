/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Sanitizer_h
#define mozilla_dom_Sanitizer_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/SanitizerBinding.h"
#include "nsString.h"
#include "nsIGlobalObject.h"
#include "nsIParserUtils.h"

// XXX(Bug 1673929) This is not really needed here, but the generated
// SanitizerBinding.cpp needs it and does not include it.
#include "mozilla/dom/Document.h"

class nsISupports;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

class Sanitizer final : public nsISupports, public nsWrapperCache {
  explicit Sanitizer(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
    MOZ_ASSERT(aGlobal);
  }

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Sanitizer);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<Sanitizer> New(nsIGlobalObject* aGlobal,
                                         const SanitizerConfig& aConfig,
                                         ErrorResult& aRv);

  static already_AddRefed<Sanitizer> New(nsIGlobalObject* aGlobal,
                                         const SanitizerPresets aConfig,
                                         ErrorResult& aRv);

  // WebIDL
  static already_AddRefed<Sanitizer> Constructor(
      const GlobalObject& aGlobal,
      const SanitizerConfigOrSanitizerPresets& aConfig, ErrorResult& aRv);

  void Get(SanitizerConfig& aConfig);

  void AllowElement(
      const StringOrSanitizerElementNamespaceWithAttributes& aElement);
  void RemoveElement(const StringOrSanitizerElementNamespace& aElement);
  void ReplaceElementWithChildren(
      const StringOrSanitizerElementNamespace& aElement);
  void AllowAttribute(const StringOrSanitizerAttributeNamespace& aAttribute);
  void RemoveAttribute(const StringOrSanitizerAttributeNamespace& aAttribute);
  void SetComments(bool aAllow);
  void SetDataAttributes(bool aAllow);
  void RemoveUnsafe();

  /**
   * Sanitizes a fragment in place. This assumes that the fragment
   * belongs but an inert document.
   *
   * @param aFragment Fragment to be sanitized in place
   * @return DocumentFragment
   */

  RefPtr<DocumentFragment> SanitizeFragment(RefPtr<DocumentFragment> aFragment,
                                            ErrorResult& aRv);

  /**
   * Logs localized message to either content console or browser console
   * @param aName              Localization key
   * @param aParams            Localization parameters
   * @param aFlags             Logging Flag (see nsIScriptError)
   */
  void LogLocalizedString(const char* aName, const nsTArray<nsString>& aParams,
                          uint32_t aFlags);

 private:
  ~Sanitizer() = default;

  /**
   * Logs localized message to either content console or browser console
   * @param aMessage           Message to log
   * @param aFlags             Logging Flag (see nsIScriptError)
   * @param aInnerWindowID     Inner Window ID (Logged on browser console if 0)
   * @param aFromPrivateWindow If from private window
   */
  static void LogMessage(const nsAString& aMessage, uint32_t aFlags,
                         uint64_t aInnerWindowID, bool aFromPrivateWindow);

  RefPtr<nsIGlobalObject> mGlobal;
};
}  // namespace dom
}  // namespace mozilla

#endif  // ifndef mozilla_dom_Sanitizer_h
