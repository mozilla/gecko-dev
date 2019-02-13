/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsIURIRefObject.h"
#include "nscore.h"

class nsIDOMMozNamedAttrMap;
class nsIDOMNode;

#ifndef nsHTMLURIRefObject_h__
#define nsHTMLURIRefObject_h__

#define NS_URI_REF_OBJECT_CID                          \
{ /* {bdd79df6-1dd1-11b2-b29c-c3d63a58f1d2} */         \
    0xbdd79df6, 0x1dd1, 0x11b2,                        \
    { 0xb2, 0x9c, 0xc3, 0xd6, 0x3a, 0x58, 0xf1, 0xd2 } \
}

class nsHTMLURIRefObject final : public nsIURIRefObject
{
public:
  nsHTMLURIRefObject();

  // Interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

  NS_DECL_NSIURIREFOBJECT

protected:
  virtual ~nsHTMLURIRefObject();

  nsCOMPtr<nsIDOMNode> mNode;
  nsCOMPtr<nsIDOMMozNamedAttrMap> mAttributes;
  uint32_t mCurAttrIndex;
  uint32_t mAttributeCnt;
};

nsresult NS_NewHTMLURIRefObject(nsIURIRefObject** aResult, nsIDOMNode* aNode);

#endif /* nsHTMLURIRefObject_h__ */

