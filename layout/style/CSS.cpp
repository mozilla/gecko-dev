/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object holding utility CSS functions */

#include "CSS.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "nsCSSParser.h"
#include "nsGlobalWindow.h"
#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsStyleUtil.h"
#include "xpcpublic.h"

namespace mozilla {
namespace dom {

struct SupportsParsingInfo
{
  nsIURI* mDocURI;
  nsIURI* mBaseURI;
  nsIPrincipal* mPrincipal;
};

static nsresult
GetParsingInfo(const GlobalObject& aGlobal,
               SupportsParsingInfo& aInfo)
{
  nsGlobalWindow* win = xpc::WindowOrNull(aGlobal.Get());
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc = win->GetDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  aInfo.mDocURI = nsCOMPtr<nsIURI>(doc->GetDocumentURI());
  aInfo.mBaseURI = nsCOMPtr<nsIURI>(doc->GetBaseURI());
  aInfo.mPrincipal = win->GetPrincipal();
  return NS_OK;
}

/* static */ bool
CSS::Supports(const GlobalObject& aGlobal,
              const nsAString& aProperty,
              const nsAString& aValue,
              ErrorResult& aRv)
{
  nsCSSParser parser;
  SupportsParsingInfo info;

  nsresult rv = GetParsingInfo(aGlobal, info);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  return parser.EvaluateSupportsDeclaration(aProperty, aValue, info.mDocURI,
                                            info.mBaseURI, info.mPrincipal);
}

/* static */ bool
CSS::Supports(const GlobalObject& aGlobal,
              const nsAString& aCondition,
              ErrorResult& aRv)
{
  nsCSSParser parser;
  SupportsParsingInfo info;

  nsresult rv = GetParsingInfo(aGlobal, info);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  return parser.EvaluateSupportsCondition(aCondition, info.mDocURI,
                                          info.mBaseURI, info.mPrincipal);
}

/* static */ void
CSS::Escape(const GlobalObject& aGlobal,
            const nsAString& aIdent,
            nsAString& aReturn,
            ErrorResult& aRv)
{
  bool success = nsStyleUtil::AppendEscapedCSSIdent(aIdent, aReturn);

  if (!success) {
    aRv.Throw(NS_ERROR_DOM_INVALID_CHARACTER_ERR);
  }
}

} // dom
} // mozilla
