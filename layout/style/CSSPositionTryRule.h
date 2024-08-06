/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSPositionTryRule_h
#define mozilla_dom_CSSPositionTryRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"

#include "nsDOMCSSDeclaration.h"
#include "nsICSSDeclaration.h"

namespace mozilla {
class DeclarationBlock;

namespace dom {

class CSSPositionTryRule;
class CSSPositionTryRuleDeclaration final : public nsDOMCSSDeclaration {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  css::Rule* GetParentRule() final;
  nsINode* GetAssociatedNode() const final;
  nsISupports* GetParentObject() const final;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

 protected:
  DeclarationBlock* GetOrCreateCSSDeclaration(
      Operation aOperation, DeclarationBlock** aCreated) final;
  nsresult SetCSSDeclaration(DeclarationBlock* aDecl,
                             MutationClosureData* aClosureData) final;
  nsDOMCSSDeclaration::ParsingEnvironment GetParsingEnvironment(
      nsIPrincipal* aSubjectPrincipal) const final;

 private:
  // For accessing the constructor.
  friend class CSSPositionTryRule;

  explicit CSSPositionTryRuleDeclaration(
      already_AddRefed<StyleLockedDeclarationBlock> aDecls);
  void SetRawAfterClone(RefPtr<StyleLockedDeclarationBlock>);

  ~CSSPositionTryRuleDeclaration();

  inline CSSPositionTryRule* Rule();
  inline const CSSPositionTryRule* Rule() const;

  RefPtr<DeclarationBlock> mDecls;
};

class CSSPositionTryRule final : public css::Rule {
 public:
  CSSPositionTryRule(RefPtr<StyleLockedPositionTryRule> aRawRule,
                     StyleSheet* aSheet, css::Rule* aParentRule, uint32_t aLine,
                     uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(CSSPositionTryRule,
                                                         css::Rule)

  bool IsCCLeaf() const final;

  StyleLockedPositionTryRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<StyleLockedPositionTryRule>);

  // WebIDL interfaces
  StyleCssRuleType Type() const final;
  void GetCssText(nsACString& aCssText) const final;
  CSSPositionTryRuleDeclaration* Style() { return &mDecls; }
  void GetName(nsACString& aName);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

 private:
  ~CSSPositionTryRule() = default;

  // For computing the offset of mDecls.
  friend class CSSPositionTryRuleDeclaration;

  RefPtr<StyleLockedPositionTryRule> mRawRule;
  CSSPositionTryRuleDeclaration mDecls;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CSSPositionTryRule_h
