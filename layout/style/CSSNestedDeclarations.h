/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CSSNestedDeclarations_h
#define mozilla_CSSNestedDeclarations_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"
#include "nsDOMCSSDeclaration.h"

namespace mozilla {

class DeclarationBlock;

namespace dom {
class DocGroup;
class CSSNestedDeclarations;

class CSSNestedDeclarationsDeclaration final : public nsDOMCSSDeclaration {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  css::Rule* GetParentRule() final;
  nsINode* GetAssociatedNode() const final;
  nsISupports* GetParentObject() const final;

 protected:
  DeclarationBlock* GetOrCreateCSSDeclaration(
      Operation aOperation, DeclarationBlock** aCreated) final;
  nsresult SetCSSDeclaration(DeclarationBlock* aDecl,
                             MutationClosureData* aClosureData) final;
  ParsingEnvironment GetParsingEnvironment(
      nsIPrincipal* aSubjectPrincipal) const final;

 private:
  // For accessing the constructor.
  friend class CSSNestedDeclarations;

  explicit CSSNestedDeclarationsDeclaration(
      already_AddRefed<StyleLockedDeclarationBlock> aDecls);
  ~CSSNestedDeclarationsDeclaration();

  inline CSSNestedDeclarations* Rule();
  inline const CSSNestedDeclarations* Rule() const;

  void SetRawAfterClone(RefPtr<StyleLockedDeclarationBlock>);

  RefPtr<DeclarationBlock> mDecls;
};

class CSSNestedDeclarations final : public css::Rule {
 public:
  CSSNestedDeclarations(
      already_AddRefed<StyleLockedNestedDeclarationsRule> aRawRule,
      StyleSheet* aSheet, css::Rule* aParentRule, uint32_t aLine,
      uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(CSSNestedDeclarations,
                                                         css::Rule)
  bool IsCCLeaf() const final MOZ_MUST_OVERRIDE;

  // WebIDL interface
  StyleCssRuleType Type() const final;
  void GetCssText(nsACString& aCssText) const final;
  nsICSSDeclaration* Style() { return &mDecls; }

  StyleLockedNestedDeclarationsRule* Raw() const { return mRawRule.get(); }
  void SetRawAfterClone(RefPtr<StyleLockedNestedDeclarationsRule>);

  // Methods of mozilla::css::Rule
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~CSSNestedDeclarations() = default;

  void GetSelectorDataAtIndex(uint32_t aSelectorIndex, bool aDesugared,
                              nsACString* aText, uint64_t* aSpecificity);

  // For computing the offset of mDecls.
  friend class CSSNestedDeclarationsDeclaration;

  RefPtr<StyleLockedNestedDeclarationsRule> mRawRule;
  CSSNestedDeclarationsDeclaration mDecls;
};

CSSNestedDeclarations* CSSNestedDeclarationsDeclaration::Rule() {
  return reinterpret_cast<CSSNestedDeclarations*>(
      reinterpret_cast<uint8_t*>(this) -
      offsetof(CSSNestedDeclarations, mDecls));
}

const CSSNestedDeclarations* CSSNestedDeclarationsDeclaration::Rule() const {
  return reinterpret_cast<const CSSNestedDeclarations*>(
      reinterpret_cast<const uint8_t*>(this) -
      offsetof(CSSNestedDeclarations, mDecls));
}

// CSSNestedDeclarations is the only rule type that doesn't end up with "Rule".
// This alias helps for consistency.
using CSSNestedDeclarationsRule = CSSNestedDeclarations;

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_CSSNestedDeclarations_h
