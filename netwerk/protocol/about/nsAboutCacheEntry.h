/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAboutCacheEntry_h__
#define nsAboutCacheEntry_h__

#include "nsIAboutModule.h"
#include "nsICacheEntryOpenCallback.h"
#include "nsICacheEntry.h"
#include "nsIStreamListener.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsIAsyncOutputStream;
class nsIInputStream;
class nsILoadContextInfo;
class nsIURI;
class nsCString;

class nsAboutCacheEntry : public nsIAboutModule
                        , public nsICacheEntryOpenCallback
                        , public nsICacheEntryMetaDataVisitor
                        , public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABOUTMODULE
    NS_DECL_NSICACHEENTRYOPENCALLBACK
    NS_DECL_NSICACHEENTRYMETADATAVISITOR
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    nsAboutCacheEntry()
        : mBuffer(nullptr)
        , mWaitingForData(false)
        , mHexDumpState(0)
    {}

private:
    virtual ~nsAboutCacheEntry() {}

    nsresult GetContentStream(nsIURI *, nsIInputStream **);
    nsresult OpenCacheEntry(nsIURI *);
    nsresult OpenCacheEntry();
    nsresult WriteCacheEntryDescription(nsICacheEntry *);
    nsresult WriteCacheEntryUnavailable();
    nsresult ParseURI(nsIURI *uri, nsACString &storageName,
                      nsILoadContextInfo **loadInfo,
                      nsCString &enahnceID, nsIURI **cacheUri);
    void CloseContent();

    static NS_METHOD
    PrintCacheData(nsIInputStream *aInStream,
                   void *aClosure,
                   const char *aFromSegment,
                   uint32_t aToOffset,
                   uint32_t aCount,
                   uint32_t *aWriteCount);

private:
    nsAutoCString mStorageName, mEnhanceId;
    nsCOMPtr<nsILoadContextInfo> mLoadInfo;
    nsCOMPtr<nsIURI> mCacheURI;

    nsCString *mBuffer;
    nsCOMPtr<nsIAsyncOutputStream> mOutputStream;
    bool mWaitingForData;
    uint32_t mHexDumpState;
};

#define NS_ABOUT_CACHE_ENTRY_MODULE_CID              \
{ /* 7fa5237d-b0eb-438f-9e50-ca0166e63788 */         \
    0x7fa5237d,                                      \
    0xb0eb,                                          \
    0x438f,                                          \
    {0x9e, 0x50, 0xca, 0x01, 0x66, 0xe6, 0x37, 0x88} \
}

#endif // nsAboutCacheEntry_h__
