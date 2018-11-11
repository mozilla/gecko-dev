/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLBodyElement.h"
#include "mozilla/dom/HTMLBodyElementBinding.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsHTMLStyleSheet.h"
#include "nsIEditor.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "nsIDocShell.h"
#include "nsRuleWalker.h"
#include "nsGlobalWindow.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Body)

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------

BodyRule::BodyRule(HTMLBodyElement* aPart)
  : mPart(aPart)
{
}

BodyRule::~BodyRule()
{
}

NS_IMPL_ISUPPORTS(BodyRule, nsIStyleRule)

/* virtual */ void
BodyRule::MapRuleInfoInto(nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Margin)) || !mPart)
    return; // We only care about margins.

  int32_t bodyMarginWidth  = -1;
  int32_t bodyMarginHeight = -1;
  int32_t bodyTopMargin = -1;
  int32_t bodyBottomMargin = -1;
  int32_t bodyLeftMargin = -1;
  int32_t bodyRightMargin = -1;

  // check the mode (fortunately, the ruleData has a presContext for us to use!)
  NS_ASSERTION(aData->mPresContext, "null presContext in ruleNode was unexpected");
  nsCompatibility mode = aData->mPresContext->CompatibilityMode();


  const nsAttrValue* value;
  if (mPart->GetAttrCount() > 0) {
    // if marginwidth/marginheight are set, reflect them as 'margin'
    value = mPart->GetParsedAttr(nsGkAtoms::marginwidth);
    if (value && value->Type() == nsAttrValue::eInteger) {
      bodyMarginWidth = value->GetIntegerValue();
      if (bodyMarginWidth < 0) bodyMarginWidth = 0;
      nsCSSValue* marginLeft = aData->ValueForMarginLeft();
      if (marginLeft->GetUnit() == eCSSUnit_Null)
        marginLeft->SetFloatValue((float)bodyMarginWidth, eCSSUnit_Pixel);
      nsCSSValue* marginRight = aData->ValueForMarginRight();
      if (marginRight->GetUnit() == eCSSUnit_Null)
        marginRight->SetFloatValue((float)bodyMarginWidth, eCSSUnit_Pixel);
    }

    value = mPart->GetParsedAttr(nsGkAtoms::marginheight);
    if (value && value->Type() == nsAttrValue::eInteger) {
      bodyMarginHeight = value->GetIntegerValue();
      if (bodyMarginHeight < 0) bodyMarginHeight = 0;
      nsCSSValue* marginTop = aData->ValueForMarginTop();
      if (marginTop->GetUnit() == eCSSUnit_Null)
        marginTop->SetFloatValue((float)bodyMarginHeight, eCSSUnit_Pixel);
      nsCSSValue* marginBottom = aData->ValueForMarginBottom();
      if (marginBottom->GetUnit() == eCSSUnit_Null)
        marginBottom->SetFloatValue((float)bodyMarginHeight, eCSSUnit_Pixel);
    }

      // topmargin (IE-attribute)
    value = mPart->GetParsedAttr(nsGkAtoms::topmargin);
    if (value && value->Type() == nsAttrValue::eInteger) {
      bodyTopMargin = value->GetIntegerValue();
      if (bodyTopMargin < 0) bodyTopMargin = 0;
      nsCSSValue* marginTop = aData->ValueForMarginTop();
      if (marginTop->GetUnit() == eCSSUnit_Null)
        marginTop->SetFloatValue((float)bodyTopMargin, eCSSUnit_Pixel);
    }

      // bottommargin (IE-attribute)
    value = mPart->GetParsedAttr(nsGkAtoms::bottommargin);
    if (value && value->Type() == nsAttrValue::eInteger) {
      bodyBottomMargin = value->GetIntegerValue();
      if (bodyBottomMargin < 0) bodyBottomMargin = 0;
      nsCSSValue* marginBottom = aData->ValueForMarginBottom();
      if (marginBottom->GetUnit() == eCSSUnit_Null)
        marginBottom->SetFloatValue((float)bodyBottomMargin, eCSSUnit_Pixel);
    }

      // leftmargin (IE-attribute)
    value = mPart->GetParsedAttr(nsGkAtoms::leftmargin);
    if (value && value->Type() == nsAttrValue::eInteger) {
      bodyLeftMargin = value->GetIntegerValue();
      if (bodyLeftMargin < 0) bodyLeftMargin = 0;
      nsCSSValue* marginLeft = aData->ValueForMarginLeft();
      if (marginLeft->GetUnit() == eCSSUnit_Null)
        marginLeft->SetFloatValue((float)bodyLeftMargin, eCSSUnit_Pixel);
    }

      // rightmargin (IE-attribute)
    value = mPart->GetParsedAttr(nsGkAtoms::rightmargin);
    if (value && value->Type() == nsAttrValue::eInteger) {
      bodyRightMargin = value->GetIntegerValue();
      if (bodyRightMargin < 0) bodyRightMargin = 0;
      nsCSSValue* marginRight = aData->ValueForMarginRight();
      if (marginRight->GetUnit() == eCSSUnit_Null)
        marginRight->SetFloatValue((float)bodyRightMargin, eCSSUnit_Pixel);
    }

  }

  // if marginwidth or marginheight is set in the <frame> and not set in the <body>
  // reflect them as margin in the <body>
  if (bodyMarginWidth == -1 || bodyMarginHeight == -1) {
    nsCOMPtr<nsIDocShell> docShell(aData->mPresContext->GetDocShell());
    if (docShell) {
      nscoord frameMarginWidth=-1;  // default value
      nscoord frameMarginHeight=-1; // default value
      docShell->GetMarginWidth(&frameMarginWidth); // -1 indicates not set
      docShell->GetMarginHeight(&frameMarginHeight);
      if ((frameMarginWidth >= 0) && (bodyMarginWidth == -1)) { // set in <frame> & not in <body>
        if (eCompatibility_NavQuirks == mode) {
          if ((bodyMarginHeight == -1) && (0 > frameMarginHeight)) // nav quirk
            frameMarginHeight = 0;
        }
      }
      if ((frameMarginHeight >= 0) && (bodyMarginHeight == -1)) { // set in <frame> & not in <body>
        if (eCompatibility_NavQuirks == mode) {
          if ((bodyMarginWidth == -1) && (0 > frameMarginWidth)) // nav quirk
            frameMarginWidth = 0;
        }
      }

      if ((bodyMarginWidth == -1) && (frameMarginWidth >= 0)) {
        nsCSSValue* marginLeft = aData->ValueForMarginLeft();
        if (marginLeft->GetUnit() == eCSSUnit_Null)
          marginLeft->SetFloatValue((float)frameMarginWidth, eCSSUnit_Pixel);
        nsCSSValue* marginRight = aData->ValueForMarginRight();
        if (marginRight->GetUnit() == eCSSUnit_Null)
          marginRight->SetFloatValue((float)frameMarginWidth, eCSSUnit_Pixel);
      }

      if ((bodyMarginHeight == -1) && (frameMarginHeight >= 0)) {
        nsCSSValue* marginTop = aData->ValueForMarginTop();
        if (marginTop->GetUnit() == eCSSUnit_Null)
          marginTop->SetFloatValue((float)frameMarginHeight, eCSSUnit_Pixel);
        nsCSSValue* marginBottom = aData->ValueForMarginBottom();
        if (marginBottom->GetUnit() == eCSSUnit_Null)
          marginBottom->SetFloatValue((float)frameMarginHeight, eCSSUnit_Pixel);
      }
    }
  }
}

/* virtual */ bool
BodyRule::MightMapInheritedStyleData()
{
  return false;
}

/* virtual */ bool
BodyRule::GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty,
                                        nsCSSValue* aValue)
{
  MOZ_ASSERT(false, "GetDiscretelyAnimatedCSSValue is not implemented yet");
  return false;
}

#ifdef DEBUG
/* virtual */ void
BodyRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString indent;
  for (int32_t index = aIndent; --index >= 0; ) {
    indent.AppendLiteral("  ");
  }
  fprintf_stderr(out, "%s[body rule] {}\n", indent.get());
}
#endif

//----------------------------------------------------------------------

HTMLBodyElement::~HTMLBodyElement()
{
  if (mContentStyleRule) {
    mContentStyleRule->mPart = nullptr;
  }
}

JSObject*
HTMLBodyElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLBodyElementBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLBodyElement, nsGenericHTMLElement,
                            nsIDOMHTMLBodyElement)

NS_IMPL_ELEMENT_CLONE(HTMLBodyElement)

NS_IMETHODIMP 
HTMLBodyElement::SetBackground(const nsAString& aBackground)
{
  ErrorResult rv;
  SetBackground(aBackground, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetBackground(nsAString& aBackground)
{
  DOMString background;
  GetBackground(background);
  background.ToString(aBackground);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLBodyElement::SetVLink(const nsAString& aVLink)
{
  ErrorResult rv;
  SetVLink(aVLink, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetVLink(nsAString& aVLink)
{
  DOMString vLink;
  GetVLink(vLink);
  vLink.ToString(aVLink);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLBodyElement::SetALink(const nsAString& aALink)
{
  ErrorResult rv;
  SetALink(aALink, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetALink(nsAString& aALink)
{
  DOMString aLink;
  GetALink(aLink);
  aLink.ToString(aALink);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLBodyElement::SetLink(const nsAString& aLink)
{
  ErrorResult rv;
  SetLink(aLink, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetLink(nsAString& aLink)
{
  DOMString link;
  GetLink(link);
  link.ToString(aLink);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLBodyElement::SetText(const nsAString& aText)
{
  ErrorResult rv;
  SetText(aText, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetText(nsAString& aText)
{
  DOMString text;
  GetText(text);
  text.ToString(aText);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLBodyElement::SetBgColor(const nsAString& aBgColor)
{
  ErrorResult rv;
  SetBgColor(aBgColor, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetBgColor(nsAString& aBgColor)
{
  DOMString bgColor;
  GetBgColor(bgColor);
  bgColor.ToString(aBgColor);
  return NS_OK;
}

bool
HTMLBodyElement::ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::bgcolor ||
        aAttribute == nsGkAtoms::text ||
        aAttribute == nsGkAtoms::link ||
        aAttribute == nsGkAtoms::alink ||
        aAttribute == nsGkAtoms::vlink) {
      return aResult.ParseColor(aValue);
    }
    if (aAttribute == nsGkAtoms::marginwidth ||
        aAttribute == nsGkAtoms::marginheight ||
        aAttribute == nsGkAtoms::topmargin ||
        aAttribute == nsGkAtoms::bottommargin ||
        aAttribute == nsGkAtoms::leftmargin ||
        aAttribute == nsGkAtoms::rightmargin) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
  }

  return nsGenericHTMLElement::ParseBackgroundAttribute(aNamespaceID,
                                                        aAttribute, aValue,
                                                        aResult) ||
         nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLBodyElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mContentStyleRule) {
    mContentStyleRule->mPart = nullptr;
    mContentStyleRule = nullptr;
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);  
}

void
HTMLBodyElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                       nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Display)) {
    // When display if first asked for, go ahead and get our colors set up.
    nsIPresShell *presShell = aData->mPresContext->GetPresShell();
    if (presShell) {
      nsIDocument *doc = presShell->GetDocument();
      if (doc) {
        nsHTMLStyleSheet* styleSheet = doc->GetAttributeStyleSheet();
        if (styleSheet) {
          const nsAttrValue* value;
          nscolor color;
          value = aAttributes->GetAttr(nsGkAtoms::link);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetLinkColor(color);
          }

          value = aAttributes->GetAttr(nsGkAtoms::alink);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetActiveLinkColor(color);
          }

          value = aAttributes->GetAttr(nsGkAtoms::vlink);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetVisitedLinkColor(color);
          }
        }
      }
    }
  }

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    nsCSSValue *colorValue = aData->ValueForColor();
    if (colorValue->GetUnit() == eCSSUnit_Null &&
        aData->mPresContext->UseDocumentColors()) {
      // color: color
      nscolor color;
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::text);
      if (value && value->GetColorValue(color))
        colorValue->SetColorValue(color);
    }
  }

  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

nsMapRuleToAttributesFunc
HTMLBodyElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

NS_IMETHODIMP
HTMLBodyElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  nsGenericHTMLElement::WalkContentStyleRules(aRuleWalker);

  if (!mContentStyleRule && IsInUncomposedDoc()) {
    // XXXbz should this use OwnerDoc() or GetComposedDoc()?
    // sXBL/XBL2 issue!
    mContentStyleRule = new BodyRule(this);
  }
  if (aRuleWalker && mContentStyleRule) {
    aRuleWalker->Forward(mContentStyleRule);
  }
  return NS_OK;
}

NS_IMETHODIMP_(bool)
HTMLBodyElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::link },
    { &nsGkAtoms::vlink },
    { &nsGkAtoms::alink },
    { &nsGkAtoms::text },
    // These aren't mapped through attribute mapping, but they are
    // mapped through a style rule, so it is attribute dependent style.
    // XXXldb But we don't actually replace the body rule when we have
    // dynamic changes...
    { &nsGkAtoms::marginwidth },
    { &nsGkAtoms::marginheight },
    { nullptr },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
    sBackgroundAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

already_AddRefed<nsIEditor>
HTMLBodyElement::GetAssociatedEditor()
{
  nsCOMPtr<nsIEditor> editor = GetEditorInternal();
  if (editor) {
    return editor.forget();
  }

  // Make sure this is the actual body of the document
  if (!IsCurrentBodyElement()) {
    return nullptr;
  }

  // For designmode, try to get document's editor
  nsPresContext* presContext = GetPresContext(eForComposedDoc);
  if (!presContext) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = presContext->GetDocShell();
  if (!docShell) {
    return nullptr;
  }

  docShell->GetEditor(getter_AddRefs(editor));
  return editor.forget();
}

bool
HTMLBodyElement::IsEventAttributeName(nsIAtom *aName)
{
  return nsContentUtils::IsEventAttributeName(aName,
                                              EventNameType_HTML |
                                              EventNameType_HTMLBodyOrFramesetOnly);
}

#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the superclass */
// nsGenericHTMLElement::GetOnError returns
// already_AddRefed<EventHandlerNonNull> while other getters return
// EventHandlerNonNull*, so allow passing in the type to use here.
#define WINDOW_EVENT_HELPER(name_, type_)                                      \
  type_*                                                                       \
  HTMLBodyElement::GetOn##name_()                                              \
  {                                                                            \
    if (nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow()) {              \
      nsGlobalWindow* globalWin = nsGlobalWindow::Cast(win);                   \
      return globalWin->GetOn##name_();                                        \
    }                                                                          \
    return nullptr;                                                            \
  }                                                                            \
  void                                                                         \
  HTMLBodyElement::SetOn##name_(type_* handler)                                \
  {                                                                            \
    nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();                    \
    if (!win) {                                                                \
      return;                                                                  \
    }                                                                          \
                                                                               \
    nsGlobalWindow* globalWin = nsGlobalWindow::Cast(win);                     \
    return globalWin->SetOn##name_(handler);                                   \
  }
#define WINDOW_EVENT(name_, id_, type_, struct_)                               \
  WINDOW_EVENT_HELPER(name_, EventHandlerNonNull)
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_)                         \
  WINDOW_EVENT_HELPER(name_, OnBeforeUnloadEventHandlerNonNull)
#include "mozilla/EventNameList.h" // IWYU pragma: keep
#undef BEFOREUNLOAD_EVENT
#undef WINDOW_EVENT
#undef WINDOW_EVENT_HELPER
#undef EVENT

} // namespace dom
} // namespace mozilla
