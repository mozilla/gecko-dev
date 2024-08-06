/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSPositionTryRule.h"
#include "mozilla/dom/CSSPositionTryRuleBinding.h"
#include "mozilla/dom/CSSPositionTryDescriptorsBinding.h"

#include "mozilla/ServoBindings.h"
#include "mozilla/DeclarationBlock.h"

namespace mozilla::dom {

CSSPositionTryRule* CSSPositionTryRuleDeclaration::Rule() {
  return reinterpret_cast<CSSPositionTryRule*>(
      reinterpret_cast<uint8_t*>(this) - offsetof(CSSPositionTryRule, mDecls));
}

const CSSPositionTryRule* CSSPositionTryRuleDeclaration::Rule() const {
  return reinterpret_cast<const CSSPositionTryRule*>(
      reinterpret_cast<const uint8_t*>(this) -
      offsetof(CSSPositionTryRule, mDecls));
}

CSSPositionTryRuleDeclaration::CSSPositionTryRuleDeclaration(
    already_AddRefed<StyleLockedDeclarationBlock> aDecls)
    : mDecls(new DeclarationBlock(std::move(aDecls))) {
  mDecls->SetOwningRule(Rule());
}

CSSPositionTryRuleDeclaration::~CSSPositionTryRuleDeclaration() {
  mDecls->SetOwningRule(nullptr);
}

NS_INTERFACE_MAP_BEGIN(CSSPositionTryRuleDeclaration)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  // We forward the cycle collection interfaces to Rule(), which is
  // never null (in fact, we're part of that object!)
  if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)) ||
      aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    return Rule()->QueryInterface(aIID, aInstancePtr);
  }
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

NS_IMPL_ADDREF_USING_AGGREGATOR(CSSPositionTryRuleDeclaration, Rule())
NS_IMPL_RELEASE_USING_AGGREGATOR(CSSPositionTryRuleDeclaration, Rule())

css::Rule* CSSPositionTryRuleDeclaration::GetParentRule() { return Rule(); }

nsINode* CSSPositionTryRuleDeclaration::GetAssociatedNode() const {
  return Rule()->GetAssociatedDocumentOrShadowRoot();
}

nsISupports* CSSPositionTryRuleDeclaration::GetParentObject() const {
  return Rule()->GetParentObject();
}

JSObject* CSSPositionTryRuleDeclaration::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return CSSPositionTryDescriptors_Binding::Wrap(aCx, this, aGivenProto);
}

DeclarationBlock* CSSPositionTryRuleDeclaration::GetOrCreateCSSDeclaration(
    Operation aOperation, DeclarationBlock** aCreated) {
  if (aOperation != Operation::Read) {
    if (StyleSheet* sheet = Rule()->GetStyleSheet()) {
      sheet->WillDirty();
    }
  }
  return mDecls;
}

void CSSPositionTryRuleDeclaration::SetRawAfterClone(
    RefPtr<StyleLockedDeclarationBlock> aDeclarationBlock) {
  mDecls->SetOwningRule(nullptr);
  mDecls = new DeclarationBlock(aDeclarationBlock.forget());
  mDecls->SetOwningRule(Rule());
}

nsresult CSSPositionTryRuleDeclaration::SetCSSDeclaration(
    DeclarationBlock* aDecl, MutationClosureData* aClosureData) {
  MOZ_ASSERT(aDecl, "must be non-null");
  CSSPositionTryRule* rule = Rule();

  if (aDecl != mDecls) {
    mDecls->SetOwningRule(nullptr);
    Servo_PositionTryRule_SetStyle(rule->Raw(), aDecl->Raw());
    mDecls = aDecl;
    mDecls->SetOwningRule(rule);
  }

  return NS_OK;
}

nsDOMCSSDeclaration::ParsingEnvironment
CSSPositionTryRuleDeclaration::GetParsingEnvironment(
    nsIPrincipal* aSubjectPrincipal) const {
  return GetParsingEnvironmentForRule(Rule(), StyleCssRuleType::PositionTry);
}

CSSPositionTryRule::CSSPositionTryRule(
    RefPtr<StyleLockedPositionTryRule> aRawRule, StyleSheet* aSheet,
    css::Rule* aParentRule, uint32_t aLine, uint32_t aColumn)
    : css::Rule(aSheet, aParentRule, aLine, aColumn),
      mRawRule(std::move(aRawRule)),
      mDecls(Servo_PositionTryRule_GetStyle(mRawRule).Consume()) {}

NS_IMPL_ADDREF_INHERITED(CSSPositionTryRule, css::Rule)
NS_IMPL_RELEASE_INHERITED(CSSPositionTryRule, css::Rule)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSPositionTryRule)
NS_INTERFACE_MAP_END_INHERITING(css::Rule)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSPositionTryRule)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(CSSPositionTryRule, css::Rule)
  // Keep this in sync with IsCCLeaf.

  // Trace the wrapper for our declaration.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecls.TraceWrapper(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CSSPositionTryRule)
  // Keep this in sync with IsCCLeaf.

  // Unlink the wrapper for our declaration.
  //
  // Note that this has to happen before unlinking css::Rule.
  tmp->UnlinkDeclarationWrapper(tmp->mDecls);
  tmp->mDecls.mDecls->SetOwningRule(nullptr);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(css::Rule)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSPositionTryRule, css::Rule)
  // Keep this in sync with IsCCLeaf.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool CSSPositionTryRule::IsCCLeaf() const {
  if (!Rule::IsCCLeaf()) {
    return false;
  }

  return !mDecls.PreservingWrapper();
}

void CSSPositionTryRule::SetRawAfterClone(
    RefPtr<StyleLockedPositionTryRule> aRaw) {
  mRawRule = std::move(aRaw);
  mDecls.SetRawAfterClone(
      Servo_PositionTryRule_GetStyle(mRawRule.get()).Consume());
}

StyleCssRuleType CSSPositionTryRule::Type() const {
  return StyleCssRuleType::PositionTry;
}

size_t CSSPositionTryRule::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  // TODO(dshin)
  return aMallocSizeOf(this);
}

#ifdef DEBUG
void CSSPositionTryRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_PositionTryRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

void CSSPositionTryRule::GetName(nsACString& aName) {
  Servo_PositionTryRule_GetName(mRawRule, &aName);
}

void CSSPositionTryRule::GetCssText(nsACString& aCssText) const {
  Servo_PositionTryRule_GetCssText(mRawRule, &aCssText);
}

JSObject* CSSPositionTryRule::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return CSSPositionTryRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
