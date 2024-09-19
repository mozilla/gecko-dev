/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSNestedDeclarations.h"
#include "mozilla/dom/CSSNestedDeclarationsBinding.h"

#include "mozilla/DeclarationBlock.h"

namespace mozilla::dom {

CSSNestedDeclarationsDeclaration::CSSNestedDeclarationsDeclaration(
    already_AddRefed<StyleLockedDeclarationBlock> aDecls)
    : mDecls(new DeclarationBlock(std::move(aDecls))) {
  mDecls->SetOwningRule(Rule());
}

CSSNestedDeclarationsDeclaration::~CSSNestedDeclarationsDeclaration() {
  mDecls->SetOwningRule(nullptr);
}

// QueryInterface implementation for CSSNestedDeclarationsDeclaration
NS_INTERFACE_MAP_BEGIN(CSSNestedDeclarationsDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // We forward the cycle collection interfaces to Rule(), which is
  // never null (in fact, we're part of that object!)
  if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)) ||
      aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    return Rule()->QueryInterface(aIID, aInstancePtr);
  }
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

NS_IMPL_ADDREF_USING_AGGREGATOR(CSSNestedDeclarationsDeclaration, Rule())
NS_IMPL_RELEASE_USING_AGGREGATOR(CSSNestedDeclarationsDeclaration, Rule())

css::Rule* CSSNestedDeclarationsDeclaration::GetParentRule() { return Rule(); }
nsINode* CSSNestedDeclarationsDeclaration::GetAssociatedNode() const {
  return Rule()->GetAssociatedDocumentOrShadowRoot();
}
nsISupports* CSSNestedDeclarationsDeclaration::GetParentObject() const {
  return Rule()->GetParentObject();
}
DeclarationBlock* CSSNestedDeclarationsDeclaration::GetOrCreateCSSDeclaration(
    Operation aOperation, DeclarationBlock** aCreated) {
  if (aOperation != Operation::Read) {
    if (StyleSheet* sheet = Rule()->GetStyleSheet()) {
      sheet->WillDirty();
    }
  }
  return mDecls;
}

void CSSNestedDeclarationsDeclaration::SetRawAfterClone(
    RefPtr<StyleLockedDeclarationBlock> aRaw) {
  auto block = MakeRefPtr<DeclarationBlock>(aRaw.forget());
  mDecls->SetOwningRule(nullptr);
  mDecls = std::move(block);
  mDecls->SetOwningRule(Rule());
}

nsresult CSSNestedDeclarationsDeclaration::SetCSSDeclaration(
    DeclarationBlock* aDecl, MutationClosureData* aClosureData) {
  CSSNestedDeclarations* rule = Rule();
  if (StyleSheet* sheet = rule->GetStyleSheet()) {
    if (aDecl != mDecls) {
      mDecls->SetOwningRule(nullptr);
      RefPtr<DeclarationBlock> decls = aDecl;
      Servo_NestedDeclarationsRule_SetStyle(rule->Raw(), decls->Raw());
      mDecls = std::move(decls);
      mDecls->SetOwningRule(rule);
    }
    sheet->RuleChanged(rule, StyleRuleChangeKind::StyleRuleDeclarations);
  }
  return NS_OK;
}

nsDOMCSSDeclaration::ParsingEnvironment
CSSNestedDeclarationsDeclaration::GetParsingEnvironment(nsIPrincipal*) const {
  return GetParsingEnvironmentForRule(Rule(),
                                      StyleCssRuleType::NestedDeclarations);
}

CSSNestedDeclarations::CSSNestedDeclarations(
    already_AddRefed<StyleLockedNestedDeclarationsRule> aRawRule,
    StyleSheet* aSheet, css::Rule* aParentRule, uint32_t aLine,
    uint32_t aColumn)
    : css::Rule(aSheet, aParentRule, aLine, aColumn),
      mRawRule(aRawRule),
      mDecls(Servo_NestedDeclarationsRule_GetStyle(mRawRule).Consume()) {}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(CSSNestedDeclarations, css::Rule)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSNestedDeclarations)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(CSSNestedDeclarations, css::Rule)
  // Keep this in sync with IsCCLeaf.

  // Trace the wrapper for our declaration.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecls.TraceWrapper(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CSSNestedDeclarations)
  // Keep this in sync with IsCCLeaf.

  // Unlink the wrapper for our declaration.
  // Note that this has to happen before unlinking css::Rule.
  tmp->UnlinkDeclarationWrapper(tmp->mDecls);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(css::Rule)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSNestedDeclarations,
                                                  css::Rule)
  // Keep this in sync with IsCCLeaf.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool CSSNestedDeclarations::IsCCLeaf() const {
  if (!css::Rule::IsCCLeaf()) {
    return false;
  }
  return !mDecls.PreservingWrapper();
}

void CSSNestedDeclarations::GetCssText(nsACString& aCssText) const {
  Servo_NestedDeclarationsRule_GetCssText(mRawRule, &aCssText);
}

#ifdef DEBUG
void CSSNestedDeclarations::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_NestedDeclarationsRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

StyleCssRuleType CSSNestedDeclarations::Type() const {
  return StyleCssRuleType::NestedDeclarations;
}

size_t CSSNestedDeclarations::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mRawRule
  // - mDecls

  return n;
}

void CSSNestedDeclarations::SetRawAfterClone(
    RefPtr<StyleLockedNestedDeclarationsRule> aRaw) {
  mRawRule = std::move(aRaw);
  mDecls.SetRawAfterClone(
      Servo_NestedDeclarationsRule_GetStyle(mRawRule).Consume());
}

JSObject* CSSNestedDeclarations::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return CSSNestedDeclarations_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
