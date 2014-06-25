/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJARChannel_h__
#define nsJARChannel_h__

#include "nsIJARChannel.h"
#include "nsIJARURI.h"
#include "nsIInputStreamPump.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIStreamListener.h"
#include "nsIRemoteOpenFileListener.h"
#include "nsIZipReader.h"
#include "nsIDownloader.h"
#include "nsILoadGroup.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsHashPropertyBag.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "prlog.h"

class nsJARInputThunk;

//-----------------------------------------------------------------------------

class nsJARChannel : public nsIJARChannel
                   , public nsIDownloadObserver
                   , public nsIStreamListener
                   , public nsIRemoteOpenFileListener
                   , public nsIThreadRetargetableRequest
                   , public nsIThreadRetargetableStreamListener
                   , public nsHashPropertyBag
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIJARCHANNEL
    NS_DECL_NSIDOWNLOADOBSERVER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREMOTEOPENFILELISTENER
    NS_DECL_NSITHREADRETARGETABLEREQUEST
    NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

    nsJARChannel();

    nsresult Init(nsIURI *uri);

private:
    virtual ~nsJARChannel();

    nsresult CreateJarInput(nsIZipReaderCache *, nsJARInputThunk **);
    nsresult LookupFile();
    nsresult OpenLocalFile();
    void NotifyError(nsresult aError);

    void FireOnProgress(uint64_t aProgress);

#if defined(PR_LOGGING)
    nsCString                       mSpec;
#endif

    bool                            mOpened;

    nsCOMPtr<nsIJARURI>             mJarURI;
    nsCOMPtr<nsIURI>                mOriginalURI;
    nsCOMPtr<nsIURI>                mAppURI;
    nsCOMPtr<nsISupports>           mOwner;
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsISupports>           mSecurityInfo;
    nsCOMPtr<nsIProgressEventSink>  mProgressSink;
    nsCOMPtr<nsILoadGroup>          mLoadGroup;
    nsCOMPtr<nsIStreamListener>     mListener;
    nsCOMPtr<nsISupports>           mListenerContext;
    nsCString                       mContentType;
    nsCString                       mContentCharset;
    nsCString                       mContentDispositionHeader;
    /* mContentDisposition is uninitialized if mContentDispositionHeader is
     * empty */
    uint32_t                        mContentDisposition;
    int64_t                         mContentLength;
    uint32_t                        mLoadFlags;
    nsresult                        mStatus;
    bool                            mIsPending;
    bool                            mIsUnsafe;
    bool                            mOpeningRemote;

    nsCOMPtr<nsIStreamListener>     mDownloader;
    nsCOMPtr<nsIInputStreamPump>    mPump;
    // mRequest is only non-null during OnStartRequest, so we'll have a pointer
    // to the request if we get called back via RetargetDeliveryTo.
    nsCOMPtr<nsIRequest>            mRequest;
    nsCOMPtr<nsIFile>               mJarFile;
    nsCOMPtr<nsIURI>                mJarBaseURI;
    nsCString                       mJarEntry;
    nsCString                       mInnerJarEntry;
};

#endif // nsJARChannel_h__
