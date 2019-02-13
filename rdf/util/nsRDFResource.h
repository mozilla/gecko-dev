/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRDFResource_h__
#define nsRDFResource_h__

#include "nsCOMPtr.h"
#include "nsIRDFNode.h"
#include "nsIRDFResource.h"
#include "nscore.h"
#include "nsStringGlue.h"
#include "rdf.h"

class nsIRDFService;

/**
 * This simple base class implements nsIRDFResource, and can be used as a
 * superclass for more sophisticated resource implementations.
 */
class nsRDFResource : public nsIRDFResource {
public:

    NS_DECL_THREADSAFE_ISUPPORTS

    // nsIRDFNode methods:
    NS_IMETHOD EqualsNode(nsIRDFNode* aNode, bool* aResult) override;

    // nsIRDFResource methods:
    NS_IMETHOD Init(const char* aURI) override;
    NS_IMETHOD GetValue(char* *aURI) override;
    NS_IMETHOD GetValueUTF8(nsACString& aResult) override;
    NS_IMETHOD GetValueConst(const char** aURI) override;
    NS_IMETHOD EqualsString(const char* aURI, bool* aResult) override;
    NS_IMETHOD GetDelegate(const char* aKey, REFNSIID aIID, void** aResult) override;
    NS_IMETHOD ReleaseDelegate(const char* aKey) override;

    // nsRDFResource methods:
    nsRDFResource(void);

protected:
    virtual ~nsRDFResource(void);
    static nsIRDFService* gRDFService;
    static nsrefcnt gRDFServiceRefCnt;

protected:
    nsCString mURI;

    struct DelegateEntry {
        nsCString             mKey;
        nsCOMPtr<nsISupports> mDelegate;
        DelegateEntry*        mNext;
    };

    DelegateEntry* mDelegates;
};

#endif // nsRDFResource_h__
