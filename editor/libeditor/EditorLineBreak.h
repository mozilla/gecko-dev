/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EditorLineBreak_h
#define EditorLineBreak_h

#include "EditorDOMPoint.h"
#include "EditorForwards.h"
#include "EditorUtils.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Text.h"

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsTextFragment.h"

namespace mozilla {

class AutoTrackLineBreak;

/******************************************************************************
 * EditorLineBreakBase stores <br> or a preformatted line break position.
 * This cannot represent no line break.  Therefore, if a method may not return
 * a line break, they need to use Maybe.
 ******************************************************************************/
template <typename ContentType>
class EditorLineBreakBase {
  using SelfType = EditorLineBreakBase<ContentType>;

 public:
  using HTMLBRElement = dom::HTMLBRElement;
  using Text = dom::Text;

  explicit EditorLineBreakBase(const HTMLBRElement& aBRElement)
      : mContent(const_cast<HTMLBRElement*>(&aBRElement)) {}
  explicit EditorLineBreakBase(RefPtr<HTMLBRElement>&& aBRElement);
  explicit EditorLineBreakBase(RefPtr<dom::Element>&& aBRElement);
  explicit EditorLineBreakBase(nsCOMPtr<nsIContent>&& aBRElement);
  EditorLineBreakBase(const Text& aText, uint32_t aOffset)
      : mContent(const_cast<Text*>(&aText)), mOffsetInText(Some(aOffset)) {}
  EditorLineBreakBase(RefPtr<Text>&& aText, uint32_t aOffset);
  EditorLineBreakBase(nsCOMPtr<nsIContent>&& aText, uint32_t aOffset);

  [[nodiscard]] static SelfType AtLastChar(const Text& aText) {
    MOZ_RELEASE_ASSERT(aText.TextDataLength());
    return SelfType(aText, aText.TextDataLength() - 1u);
  }
  [[nodiscard]] static SelfType AtLastChar(RefPtr<Text>&& aText) {
    MOZ_RELEASE_ASSERT(aText);
    MOZ_RELEASE_ASSERT(aText->TextDataLength());
    const uint32_t lastCharIndex = aText->TextDataLength() - 1u;
    return SelfType(std::forward<RefPtr<Text>>(aText), lastCharIndex);
  }
  [[nodiscard]] static SelfType AtLastChar(nsCOMPtr<nsIContent>&& aText) {
    MOZ_RELEASE_ASSERT(aText);
    MOZ_RELEASE_ASSERT(aText->IsText());
    MOZ_RELEASE_ASSERT(aText->AsText()->TextDataLength());
    const uint32_t lastCharIndex = aText->AsText()->TextDataLength() - 1u;
    return SelfType(std::forward<nsCOMPtr<nsIContent>>(aText), lastCharIndex);
  }

  [[nodiscard]] bool IsInComposedDoc() const {
    return mContent->IsInComposedDoc();
  }

  template <typename EditorDOMPointType>
  [[nodiscard]] EditorDOMPointType To() const {
    static_assert(std::is_same<EditorDOMPointType, EditorRawDOMPoint>::value ||
                  std::is_same<EditorDOMPointType, EditorDOMPoint>::value);
    return mOffsetInText.isSome()
               ? EditorDOMPointType(mContent, mOffsetInText.value())
               : EditorDOMPointType(mContent);
  }

  [[nodiscard]] bool IsHTMLBRElement() const {
    MOZ_ASSERT_IF(!mOffsetInText, mContent->IsHTMLElement(nsGkAtoms::br));
    return mOffsetInText.isNothing();
  }
  [[nodiscard]] bool IsPreformattedLineBreak() const {
    MOZ_ASSERT_IF(mOffsetInText, mContent->IsText());
    return mOffsetInText.isSome();
  }
  [[nodiscard]] bool TextIsOnlyPreformattedLineBreak() const {
    return IsPreformattedLineBreak() && !Offset() &&
           TextRef().TextDataLength() == 1u;
  }

  [[nodiscard]] nsIContent& ContentRef() const { return *mContent; }

  [[nodiscard]] HTMLBRElement& BRElementRef() const {
    MOZ_DIAGNOSTIC_ASSERT(IsHTMLBRElement());
    MOZ_DIAGNOSTIC_ASSERT(GetBRElement());
    return *GetBRElement();
  }
  [[nodiscard]] HTMLBRElement* GetBRElement() const {
    return HTMLBRElement::FromNode(mContent);
  }
  [[nodiscard]] Text& TextRef() const {
    MOZ_DIAGNOSTIC_ASSERT(IsPreformattedLineBreak());
    MOZ_DIAGNOSTIC_ASSERT(GetText());
    return *GetText();
  }
  [[nodiscard]] Text* GetText() const { return Text::FromNode(mContent); }
  [[nodiscard]] uint32_t Offset() const {
    MOZ_ASSERT(IsPreformattedLineBreak());
    return mOffsetInText.value();
  }
  [[nodiscard]] bool CharAtOffsetIsLineBreak() const {
    MOZ_DIAGNOSTIC_ASSERT(IsPreformattedLineBreak());
    return *mOffsetInText < TextRef().TextDataLength() &&
           TextRef().TextFragment().CharAt(*mOffsetInText) == '\n';
  }

 private:
  ContentType mContent;
  Maybe<uint32_t> mOffsetInText;

  friend class AutoTrackLineBreak;
};

using EditorLineBreak = EditorLineBreakBase<nsCOMPtr<nsIContent>>;
using EditorRawLineBreak = EditorLineBreakBase<nsIContent*>;

template <>
inline EditorLineBreakBase<nsCOMPtr<nsIContent>>::EditorLineBreakBase(
    RefPtr<HTMLBRElement>&& aBRElement)
    : mContent(aBRElement.forget()) {
  MOZ_RELEASE_ASSERT(mContent);
}

template <>
inline EditorLineBreakBase<nsCOMPtr<nsIContent>>::EditorLineBreakBase(
    RefPtr<dom::Element>&& aBRElement)
    : mContent(aBRElement.forget()) {
  MOZ_RELEASE_ASSERT(mContent);
  MOZ_RELEASE_ASSERT(mContent->IsHTMLElement(nsGkAtoms::br));
}

template <>
inline EditorLineBreakBase<nsCOMPtr<nsIContent>>::EditorLineBreakBase(
    nsCOMPtr<nsIContent>&& aBRElement)
    : mContent(aBRElement.forget()) {
  MOZ_RELEASE_ASSERT(mContent);
  MOZ_RELEASE_ASSERT(mContent->IsHTMLElement(nsGkAtoms::br));
}

template <>
inline EditorLineBreakBase<nsCOMPtr<nsIContent>>::EditorLineBreakBase(
    RefPtr<Text>&& aText, uint32_t aOffset)
    : mContent(std::move(aText)), mOffsetInText(Some(aOffset)) {
  MOZ_RELEASE_ASSERT(mContent);
  MOZ_ASSERT(EditorUtils::IsNewLinePreformatted(*mContent));
  MOZ_RELEASE_ASSERT(GetText()->TextDataLength() > aOffset);
  MOZ_RELEASE_ASSERT(CharAtOffsetIsLineBreak());
}

template <>
inline EditorLineBreakBase<nsCOMPtr<nsIContent>>::EditorLineBreakBase(
    nsCOMPtr<nsIContent>&& aText, uint32_t aOffset)
    : mContent(aText.forget()), mOffsetInText(Some(aOffset)) {
  MOZ_RELEASE_ASSERT(mContent);
  MOZ_RELEASE_ASSERT(mContent->IsText());
  MOZ_ASSERT(EditorUtils::IsNewLinePreformatted(*mContent));
  MOZ_RELEASE_ASSERT(TextRef().TextDataLength() > aOffset);
  MOZ_ASSERT(CharAtOffsetIsLineBreak());
}

template <>
inline EditorLineBreakBase<nsIContent*>::EditorLineBreakBase(
    const HTMLBRElement& aBRElement)
    : mContent(const_cast<HTMLBRElement*>(&aBRElement)) {}

template <>
inline EditorLineBreakBase<nsIContent*>::EditorLineBreakBase(
    RefPtr<HTMLBRElement>&& aBRElement)
    : mContent(aBRElement) {
  MOZ_RELEASE_ASSERT(mContent);
  aBRElement = nullptr;
}

template <>
inline EditorLineBreakBase<nsIContent*>::EditorLineBreakBase(
    RefPtr<dom::Element>&& aBRElement)
    : mContent(aBRElement) {
  MOZ_RELEASE_ASSERT(mContent);
  MOZ_RELEASE_ASSERT(mContent->IsHTMLElement(nsGkAtoms::br));
  aBRElement = nullptr;
}

template <>
inline EditorLineBreakBase<nsIContent*>::EditorLineBreakBase(
    nsCOMPtr<nsIContent>&& aBRElement)
    : mContent(aBRElement) {
  MOZ_RELEASE_ASSERT(mContent);
  MOZ_RELEASE_ASSERT(mContent->IsHTMLElement(nsGkAtoms::br));
  aBRElement = nullptr;
}

template <>
inline EditorLineBreakBase<nsIContent*>::EditorLineBreakBase(
    RefPtr<Text>&& aText, uint32_t aOffset)
    : mContent(aText), mOffsetInText(Some(aOffset)) {
  MOZ_RELEASE_ASSERT(mContent);
  MOZ_ASSERT(EditorUtils::IsNewLinePreformatted(*mContent));
  MOZ_RELEASE_ASSERT(GetText()->TextDataLength() > aOffset);
  MOZ_RELEASE_ASSERT(CharAtOffsetIsLineBreak());
  aText = nullptr;
}

template <>
inline EditorLineBreakBase<nsIContent*>::EditorLineBreakBase(
    nsCOMPtr<nsIContent>&& aText, uint32_t aOffset)
    : mContent(aText), mOffsetInText(Some(aOffset)) {
  MOZ_RELEASE_ASSERT(mContent);
  MOZ_RELEASE_ASSERT(mContent->IsText());
  MOZ_ASSERT(EditorUtils::IsNewLinePreformatted(*mContent));
  MOZ_RELEASE_ASSERT(GetText()->TextDataLength() > aOffset);
  MOZ_ASSERT(CharAtOffsetIsLineBreak());
  aText = nullptr;
}

}  // namespace mozilla

#endif  // #ifndef EditorLineBreak_h
