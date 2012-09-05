/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGTests.h"
#include "DOMSVGStringList.h"
#include "nsSVGFeatures.h"
#include "nsSVGSwitchElement.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsStyleUtil.h"
#include "nsSVGUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(DOMSVGTests, nsIDOMSVGTests)

nsIAtom** DOMSVGTests::sStringListNames[3] =
{
  &nsGkAtoms::requiredFeatures,
  &nsGkAtoms::requiredExtensions,
  &nsGkAtoms::systemLanguage,
};

DOMSVGTests::DOMSVGTests()
{
  mStringListAttributes[LANGUAGE].SetIsCommaSeparated(true);
}

/* readonly attribute nsIDOMSVGStringList requiredFeatures; */
NS_IMETHODIMP
DOMSVGTests::GetRequiredFeatures(nsIDOMSVGStringList * *aRequiredFeatures)
{
  nsCOMPtr<nsSVGElement> element = do_QueryInterface(this);
  *aRequiredFeatures = DOMSVGStringList::GetDOMWrapper(
                         &mStringListAttributes[FEATURES], element, true, FEATURES).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGStringList requiredExtensions; */
NS_IMETHODIMP
DOMSVGTests::GetRequiredExtensions(nsIDOMSVGStringList * *aRequiredExtensions)
{
  nsCOMPtr<nsSVGElement> element = do_QueryInterface(this);
  *aRequiredExtensions = DOMSVGStringList::GetDOMWrapper(
                           &mStringListAttributes[EXTENSIONS], element, true, EXTENSIONS).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGStringList systemLanguage; */
NS_IMETHODIMP
DOMSVGTests::GetSystemLanguage(nsIDOMSVGStringList * *aSystemLanguage)
{
  nsCOMPtr<nsSVGElement> element = do_QueryInterface(this);
  *aSystemLanguage = DOMSVGStringList::GetDOMWrapper(
                       &mStringListAttributes[LANGUAGE], element, true, LANGUAGE).get();
  return NS_OK;
}

/* boolean hasExtension (in DOMString extension); */
NS_IMETHODIMP
DOMSVGTests::HasExtension(const nsAString & extension, bool *_retval)
{
  *_retval = nsSVGFeatures::HasExtension(extension);
  return NS_OK;
}

bool
DOMSVGTests::IsConditionalProcessingAttribute(const nsIAtom* aAttribute) const
{
  for (PRUint32 i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == *sStringListNames[i]) {
      return true;
    }
  }
  return false;
}

PRInt32
DOMSVGTests::GetBestLanguagePreferenceRank(const nsSubstring& aAcceptLangs) const
{
  const nsDefaultStringComparator defaultComparator;

  PRInt32 lowestRank = -1;

  for (PRUint32 i = 0; i < mStringListAttributes[LANGUAGE].Length(); i++) {
    nsCharSeparatedTokenizer languageTokenizer(aAcceptLangs, ',');
    PRInt32 index = 0;
    while (languageTokenizer.hasMoreTokens()) {
      const nsSubstring &languageToken = languageTokenizer.nextToken();
      bool exactMatch = (languageToken == mStringListAttributes[LANGUAGE][i]);
      bool prefixOnlyMatch =
        !exactMatch &&
        nsStyleUtil::DashMatchCompare(mStringListAttributes[LANGUAGE][i],
                                      languageTokenizer.nextToken(),
                                      defaultComparator);
      if (index == 0 && exactMatch) {
        // best possible match
        return 0;
      }
      if ((exactMatch || prefixOnlyMatch) &&
          (lowestRank == -1 || 2 * index + prefixOnlyMatch < lowestRank)) {
        lowestRank = 2 * index + prefixOnlyMatch;
      }
      ++index;
    }
  }
  return lowestRank;
}

const nsString * const DOMSVGTests::kIgnoreSystemLanguage = (nsString *) 0x01;

bool
DOMSVGTests::PassesConditionalProcessingTests(const nsString *aAcceptLangs) const
{
  // Required Features
  if (mStringListAttributes[FEATURES].IsExplicitlySet()) {
    if (mStringListAttributes[FEATURES].IsEmpty()) {
      return false;
    }
    nsCOMPtr<nsIContent> content(
      do_QueryInterface(const_cast<DOMSVGTests*>(this)));

    for (PRUint32 i = 0; i < mStringListAttributes[FEATURES].Length(); i++) {
      if (!nsSVGFeatures::HasFeature(content, mStringListAttributes[FEATURES][i])) {
        return false;
      }
    }
  }

  // Required Extensions
  //
  // The requiredExtensions  attribute defines a list of required language
  // extensions. Language extensions are capabilities within a user agent that
  // go beyond the feature set defined in the SVG specification.
  // Each extension is identified by a URI reference.
  // For now, claim that mozilla's SVG implementation supports XHTML and MathML.
  if (mStringListAttributes[EXTENSIONS].IsExplicitlySet()) {
    if (mStringListAttributes[EXTENSIONS].IsEmpty()) {
      return false;
    }
    for (PRUint32 i = 0; i < mStringListAttributes[EXTENSIONS].Length(); i++) {
      if (!nsSVGFeatures::HasExtension(mStringListAttributes[EXTENSIONS][i])) {
        return false;
      }
    }
  }

  if (aAcceptLangs == kIgnoreSystemLanguage) {
    return true;
  }

  // systemLanguage
  //
  // Evaluates to "true" if one of the languages indicated by user preferences
  // exactly equals one of the languages given in the value of this parameter,
  // or if one of the languages indicated by user preferences exactly equals a
  // prefix of one of the languages given in the value of this parameter such
  // that the first tag character following the prefix is "-".
  if (mStringListAttributes[LANGUAGE].IsExplicitlySet()) {
    if (mStringListAttributes[LANGUAGE].IsEmpty()) {
      return false;
    }

    // Get our language preferences
    const nsAutoString acceptLangs(aAcceptLangs ? *aAcceptLangs :
      Preferences::GetLocalizedString("intl.accept_languages"));

    if (acceptLangs.IsEmpty()) {
      NS_WARNING("no default language specified for systemLanguage conditional test");
      return false;
    }

    const nsDefaultStringComparator defaultComparator;

    for (PRUint32 i = 0; i < mStringListAttributes[LANGUAGE].Length(); i++) {
      nsCharSeparatedTokenizer languageTokenizer(acceptLangs, ',');
      while (languageTokenizer.hasMoreTokens()) {
        if (nsStyleUtil::DashMatchCompare(mStringListAttributes[LANGUAGE][i],
                                          languageTokenizer.nextToken(),
                                          defaultComparator)) {
          return true;
        }
      }
    }
    return false;
  }

  return true;
}

bool
DOMSVGTests::ParseConditionalProcessingAttribute(nsIAtom* aAttribute,
                                                 const nsAString& aValue,
                                                 nsAttrValue& aResult)
{
  for (PRUint32 i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == *sStringListNames[i]) {
      nsresult rv = mStringListAttributes[i].SetValue(aValue);
      if (NS_FAILED(rv)) {
        mStringListAttributes[i].Clear();
      }
      MaybeInvalidate();
      return true;
    }
  }
  return false;
}

void
DOMSVGTests::UnsetAttr(const nsIAtom* aAttribute)
{
  for (PRUint32 i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == *sStringListNames[i]) {
      mStringListAttributes[i].Clear();
      MaybeInvalidate();
      return;
    }
  }
}

nsIAtom*
DOMSVGTests::GetAttrName(PRUint8 aAttrEnum) const
{
  return *sStringListNames[aAttrEnum];
}

void
DOMSVGTests::GetAttrValue(PRUint8 aAttrEnum, nsAttrValue& aValue) const
{
  MOZ_ASSERT(aAttrEnum < ArrayLength(sStringListNames),
             "aAttrEnum out of range");
  aValue.SetTo(mStringListAttributes[aAttrEnum], nsnull);
}

void
DOMSVGTests::MaybeInvalidate()
{
  nsCOMPtr<nsSVGElement> element = do_QueryInterface(this);

  nsIContent* parent = element->GetFlattenedTreeParent();

  if (parent &&
      parent->NodeInfo()->Equals(nsGkAtoms::svgSwitch, kNameSpaceID_SVG)) {
    static_cast<nsSVGSwitchElement*>(parent)->MaybeInvalidate();
  }
}
