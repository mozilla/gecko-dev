/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A base class which implements nsIStyleSheetLinkingElement and can
 * be subclassed by various content nodes that want to load
 * stylesheets (<style>, <link>, processing instructions, etc).
 */

#ifndef nsStyleLinkElement_h___
#define nsStyleLinkElement_h___

#include "mozilla/Attributes.h"
#include "mozilla/CORSMode.h"
#include "mozilla/CSSStyleSheet.h"
#include "nsCOMPtr.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsTArray.h"

class nsIDocument;
class nsIURI;

namespace mozilla {
namespace dom {
class ShadowRoot;
} // namespace dom
} // namespace mozilla

class nsStyleLinkElement : public nsIStyleSheetLinkingElement
{
public:
  nsStyleLinkElement();
  virtual ~nsStyleLinkElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) MOZ_OVERRIDE = 0;

  mozilla::CSSStyleSheet* GetSheet() const { return mStyleSheet; }

  // nsIStyleSheetLinkingElement  
  NS_IMETHOD SetStyleSheet(mozilla::CSSStyleSheet* aStyleSheet) MOZ_OVERRIDE;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aStyleSheet) MOZ_OVERRIDE;
  NS_IMETHOD InitStyleLinkElement(bool aDontLoadStyle) MOZ_OVERRIDE;
  NS_IMETHOD UpdateStyleSheet(nsICSSLoaderObserver* aObserver,
                              bool* aWillNotify,
                              bool* aIsAlternate) MOZ_OVERRIDE;
  NS_IMETHOD SetEnableUpdates(bool aEnableUpdates) MOZ_OVERRIDE;
  NS_IMETHOD GetCharset(nsAString& aCharset) MOZ_OVERRIDE;

  virtual void OverrideBaseURI(nsIURI* aNewBaseURI) MOZ_OVERRIDE;
  virtual void SetLineNumber(uint32_t aLineNumber) MOZ_OVERRIDE;

  enum RelValue {
    ePREFETCH =     0x00000001,
    eDNS_PREFETCH = 0x00000002,
    eSTYLESHEET =   0x00000004,
    eNEXT =         0x00000008,
    eALTERNATE =    0x00000010,
    eHTMLIMPORT =   0x00000020
  };

  // The return value is a bitwise or of 0 or more RelValues
  static uint32_t ParseLinkTypes(const nsAString& aTypes);

  static bool IsImportEnabled();
  
  void UpdateStyleSheetInternal()
  {
    UpdateStyleSheetInternal(nullptr, nullptr);
  }
protected:
  /**
   * @param aOldDocument should be non-null only if we're updating because we
   *                     removed the node from the document.
   * @param aForceUpdate true will force the update even if the URI has not
   *                     changed.  This should be used in cases when something
   *                     about the content that affects the resulting sheet
   *                     changed but the URI may not have changed.
   */
  nsresult UpdateStyleSheetInternal(nsIDocument *aOldDocument,
                                    mozilla::dom::ShadowRoot *aOldShadowRoot,
                                    bool aForceUpdate = false);

  void UpdateStyleSheetScopedness(bool aIsNowScoped);

  virtual already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline) = 0;
  virtual void GetStyleSheetInfo(nsAString& aTitle,
                                 nsAString& aType,
                                 nsAString& aMedia,
                                 bool* aIsScoped,
                                 bool* aIsAlternate) = 0;

  virtual mozilla::CORSMode GetCORSMode() const
  {
    // Default to no CORS
    return mozilla::CORS_NONE;
  }

  // CC methods
  void Unlink();
  void Traverse(nsCycleCollectionTraversalCallback &cb);

private:
  /**
   * @param aOldDocument should be non-null only if we're updating because we
   *                     removed the node from the document.
   * @param aOldShadowRoot The ShadowRoot that used to contain the style.
   *                     Passed as a parameter because on an update, the node
   *                     is removed from the tree before the sheet is removed
   *                     from the ShadowRoot.
   * @param aForceUpdate true will force the update even if the URI has not
   *                     changed.  This should be used in cases when something
   *                     about the content that affects the resulting sheet
   *                     changed but the URI may not have changed.
   */
  nsresult DoUpdateStyleSheet(nsIDocument* aOldDocument,
                              mozilla::dom::ShadowRoot* aOldShadowRoot,
                              nsICSSLoaderObserver* aObserver,
                              bool* aWillNotify,
                              bool* aIsAlternate,
                              bool aForceUpdate);

  nsRefPtr<mozilla::CSSStyleSheet> mStyleSheet;
protected:
  bool mDontLoadStyle;
  bool mUpdatesEnabled;
  uint32_t mLineNumber;
};

#endif /* nsStyleLinkElement_h___ */

