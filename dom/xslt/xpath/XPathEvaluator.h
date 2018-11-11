/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XPathEvaluator_h
#define mozilla_dom_XPathEvaluator_h

#include "nsIDOMXPathEvaluator.h"
#include "nsIWeakReference.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsIDocument.h"

class nsINode;
class txIParseContext;
class txResultRecycler;

namespace mozilla {
namespace dom {

class GlobalObject;
class XPathExpression;
class XPathNSResolver;
class XPathResult;

/**
 * A class for evaluating an XPath expression string
 */
class XPathEvaluator final : public nsIDOMXPathEvaluator
{
    ~XPathEvaluator();

public:
    explicit XPathEvaluator(nsIDocument* aDocument = nullptr);

    NS_DECL_ISUPPORTS

    // nsIDOMXPathEvaluator interface
    NS_DECL_NSIDOMXPATHEVALUATOR

    // WebIDL API
    bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);
    nsIDocument* GetParentObject()
    {
        nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
        return doc;
    }
    static already_AddRefed<XPathEvaluator>
        Constructor(const GlobalObject& aGlobal, ErrorResult& rv);
    XPathExpression*
        CreateExpression(const nsAString& aExpression,
                         XPathNSResolver* aResolver,
                         ErrorResult& rv);
    XPathExpression*
        CreateExpression(const nsAString& aExpression,
                         nsINode* aResolver,
                         ErrorResult& aRv);
    nsINode* CreateNSResolver(nsINode& aNodeResolver)
    {
        return &aNodeResolver;
    }
    already_AddRefed<XPathResult>
        Evaluate(JSContext* aCx, const nsAString& aExpression,
                 nsINode& aContextNode, XPathNSResolver* aResolver,
                 uint16_t aType, JS::Handle<JSObject*> aResult,
                 ErrorResult& rv);
private:
    XPathExpression*
        CreateExpression(const nsAString& aExpression,
                         txIParseContext* aContext,
                         nsIDocument* aDocument,
                         ErrorResult& aRv);

    nsWeakPtr mDocument;
    RefPtr<txResultRecycler> mRecycler;
};

inline nsISupports*
ToSupports(XPathEvaluator* e)
{
    return static_cast<nsIDOMXPathEvaluator*>(e);
}

/* d0a75e02-b5e7-11d5-a7f2-df109fb8a1fc */
#define TRANSFORMIIX_XPATH_EVALUATOR_CID   \
{ 0xd0a75e02, 0xb5e7, 0x11d5, { 0xa7, 0xf2, 0xdf, 0x10, 0x9f, 0xb8, 0xa1, 0xfc } }

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_XPathEvaluator_h */
