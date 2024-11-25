/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationUtils.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/EffectSet.h"
#include "nsDebug.h"
#include "nsAtom.h"
#include "nsIContent.h"
#include "nsLayoutUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsString.h"
#include "xpcpublic.h"  // For xpc::NativeGlobal

using namespace mozilla::dom;

namespace mozilla {

/* static */
void AnimationUtils::LogAsyncAnimationFailure(nsCString& aMessage,
                                              const nsIContent* aContent) {
  if (aContent) {
    aMessage.AppendLiteral(" [");
    aMessage.Append(nsAtomCString(aContent->NodeInfo()->NameAtom()));

    nsAtom* id = aContent->GetID();
    if (id) {
      aMessage.AppendLiteral(" with id '");
      aMessage.Append(nsAtomCString(aContent->GetID()));
      aMessage.Append('\'');
    }
    aMessage.Append(']');
  }
  aMessage.Append('\n');
  printf_stderr("%s", aMessage.get());
}

/* static */
Document* AnimationUtils::GetCurrentRealmDocument(JSContext* aCx) {
  nsGlobalWindowInner* win = xpc::CurrentWindowOrNull(aCx);
  if (!win) {
    return nullptr;
  }
  return win->GetDoc();
}

/* static */
Document* AnimationUtils::GetDocumentFromGlobal(JSObject* aGlobalObject) {
  nsGlobalWindowInner* win = xpc::WindowOrNull(aGlobalObject);
  if (!win) {
    return nullptr;
  }
  return win->GetDoc();
}

/* static */
bool AnimationUtils::FrameHasAnimatedScale(const nsIFrame* aFrame) {
  EffectSet* effectSet = EffectSet::GetForFrame(
      aFrame, nsCSSPropertyIDSet::TransformLikeProperties());
  if (!effectSet) {
    return false;
  }

  for (const dom::KeyframeEffect* effect : *effectSet) {
    if (effect->ContainsAnimatedScale(aFrame)) {
      return true;
    }
  }

  return false;
}

/* static */
bool AnimationUtils::HasCurrentTransitions(
    const Element* aElement, const PseudoStyleRequest& aPseudoRequest) {
  MOZ_ASSERT(aElement);

  EffectSet* effectSet = EffectSet::Get(aElement, aPseudoRequest);
  if (!effectSet) {
    return false;
  }

  for (const dom::KeyframeEffect* effect : *effectSet) {
    // If |effect| is current, it must have an associated Animation
    // so we don't need to null-check the result of GetAnimation().
    if (effect->IsCurrent() && effect->GetAnimation()->AsCSSTransition()) {
      return true;
    }
  }

  return false;
}

/*static*/
std::pair<const Element*, PseudoStyleRequest>
AnimationUtils::GetElementPseudoPair(const Element* aElementOrPseudo) {
  MOZ_ASSERT(aElementOrPseudo);

  if (aElementOrPseudo->IsGeneratedContentContainerForBefore()) {
    return {aElementOrPseudo->GetParent()->AsElement(),
            PseudoStyleRequest::Before()};
  }

  if (aElementOrPseudo->IsGeneratedContentContainerForAfter()) {
    return {aElementOrPseudo->GetParent()->AsElement(),
            PseudoStyleRequest::After()};
  }

  if (aElementOrPseudo->IsGeneratedContentContainerForMarker()) {
    return {aElementOrPseudo->GetParent()->AsElement(),
            PseudoStyleRequest::Marker()};
  }

  const PseudoStyleType type = aElementOrPseudo->GetPseudoElementType();
  if (PseudoStyle::IsViewTransitionPseudoElement(type)) {
    // Note: ::view-transition doesn't have a name, so we check if it has a name
    // first.
    nsAtom* name =
        aElementOrPseudo->HasName()
            ? aElementOrPseudo->GetParsedAttr(nsGkAtoms::name)->GetAtomValue()
            : nullptr;
    return {aElementOrPseudo->GetOwnerDocument()->GetRootElement(),
            PseudoStyleRequest(type, name)};
  }

  return {aElementOrPseudo, PseudoStyleRequest::NotPseudo()};
}

}  // namespace mozilla
