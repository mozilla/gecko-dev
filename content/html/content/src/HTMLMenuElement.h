/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLMenuElement_h
#define mozilla_dom_HTMLMenuElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLMenuElement.h"
#include "nsIHTMLMenu.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

class HTMLMenuElement MOZ_FINAL : public nsGenericHTMLElement,
                                  public nsIDOMHTMLMenuElement,
                                  public nsIHTMLMenu
{
public:
  HTMLMenuElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual ~HTMLMenuElement();

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLMenuElement, menu)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLMenuElement
  NS_DECL_NSIDOMHTMLMENUELEMENT

  // nsIHTMLMenu
  NS_DECL_NSIHTMLMENU

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  uint8_t GetType() const { return mType; }

  // WebIDL

  // The XPCOM GetType is OK for us
  void SetType(const nsAString& aType, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::type, aType, aError);
  }

  // The XPCOM GetLabel is OK for us
  void SetLabel(const nsAString& aLabel, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::label, aLabel, aError);
  }

  bool Compact() const
  {
    return GetBoolAttr(nsGkAtoms::compact);
  }
  void SetCompact(bool aCompact, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::compact, aCompact, aError);
  }

  // The XPCOM SendShowEvent is OK for us

  already_AddRefed<nsIMenuBuilder> CreateBuilder();

  // The XPCOM Build is OK for us

protected:
  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;


protected:
  static bool CanLoadIcon(nsIContent* aContent, const nsAString& aIcon);

  void BuildSubmenu(const nsAString& aLabel,
                    nsIContent* aContent,
                    nsIMenuBuilder* aBuilder);

  void TraverseContent(nsIContent* aContent,
                       nsIMenuBuilder* aBuilder,
                       int8_t& aSeparator);

  void AddSeparator(nsIMenuBuilder* aBuilder, int8_t& aSeparator);

  uint8_t mType;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLMenuElement_h
