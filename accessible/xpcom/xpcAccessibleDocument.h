/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleDocument_h_
#define mozilla_a11y_xpcAccessibleDocument_h_

#include "nsIAccessibleDocument.h"

#include "DocAccessible.h"
#include "nsAccessibilityService.h"
#include "xpcAccessibleApplication.h"
#include "xpcAccessibleHyperText.h"

namespace mozilla {
namespace a11y {

/**
 * XPCOM wrapper around DocAccessible class.
 */
class xpcAccessibleDocument : public xpcAccessibleHyperText,
                              public nsIAccessibleDocument
{
public:
  explicit xpcAccessibleDocument(DocAccessible* aIntl) :
    xpcAccessibleHyperText(aIntl), mCache(kDefaultCacheLength) { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(xpcAccessibleDocument,
                                           xpcAccessibleGeneric)

  // nsIAccessibleDocument
  NS_IMETHOD GetURL(nsAString& aURL) final override;
  NS_IMETHOD GetTitle(nsAString& aTitle) final override;
  NS_IMETHOD GetMimeType(nsAString& aType) final override;
  NS_IMETHOD GetDocType(nsAString& aType) final override;
  NS_IMETHOD GetDOMDocument(nsIDOMDocument** aDOMDocument) final override;
  NS_IMETHOD GetWindow(nsIDOMWindow** aDOMWindow) final override;
  NS_IMETHOD GetParentDocument(nsIAccessibleDocument** aDocument)
    final override;
  NS_IMETHOD GetChildDocumentCount(uint32_t* aCount) final override;
  NS_IMETHOD GetChildDocumentAt(uint32_t aIndex,
                                nsIAccessibleDocument** aDocument)
    final override;
  NS_IMETHOD GetVirtualCursor(nsIAccessiblePivot** aVirtualCursor)
    final override;

  /**
   * Return XPCOM wrapper for the internal accessible.
   */
  xpcAccessibleGeneric* GetAccessible(Accessible* aAccessible);

  virtual void Shutdown() override;

protected:
  virtual ~xpcAccessibleDocument() {}

private:
  DocAccessible* Intl() { return mIntl->AsDoc(); }

  void NotifyOfShutdown(Accessible* aAccessible)
  {
    xpcAccessibleGeneric* xpcAcc = mCache.GetWeak(aAccessible);
    if (xpcAcc)
      xpcAcc->Shutdown();

    mCache.Remove(aAccessible);
  }

  friend class DocManager;
  friend class DocAccessible;

  xpcAccessibleDocument(const xpcAccessibleDocument&) = delete;
  xpcAccessibleDocument& operator =(const xpcAccessibleDocument&) = delete;

  nsRefPtrHashtable<nsPtrHashKey<const Accessible>, xpcAccessibleGeneric> mCache;
};

inline xpcAccessibleGeneric*
ToXPC(Accessible* aAccessible)
{
  if (!aAccessible)
    return nullptr;

  if (aAccessible->IsApplication())
    return XPCApplicationAcc();

  xpcAccessibleDocument* xpcDoc =
    GetAccService()->GetXPCDocument(aAccessible->Document());
  return xpcDoc ? xpcDoc->GetAccessible(aAccessible) : nullptr;
}

inline xpcAccessibleHyperText*
ToXPCText(HyperTextAccessible* aAccessible)
{
  if (!aAccessible)
    return nullptr;

  xpcAccessibleDocument* xpcDoc =
    GetAccService()->GetXPCDocument(aAccessible->Document());
  return static_cast<xpcAccessibleHyperText*>(xpcDoc->GetAccessible(aAccessible));
}

inline xpcAccessibleDocument*
ToXPCDocument(DocAccessible* aAccessible)
{
  return GetAccService()->GetXPCDocument(aAccessible);
}

} // namespace a11y
} // namespace mozilla

#endif
