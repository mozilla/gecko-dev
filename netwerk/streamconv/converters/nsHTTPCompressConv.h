/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined (__nsHTTPCompressConv__h__)
#define	__nsHTTPCompressConv__h__	1

#include "nsIStreamConverter.h"
#include "nsCOMPtr.h"

#include "zlib.h"

class nsIStringInputStream;

#define NS_HTTPCOMPRESSCONVERTER_CID                \
{                                                   \
    /* 66230b2b-17fa-4bd3-abf4-07986151022d */      \
    0x66230b2b,                                     \
    0x17fa,                                         \
    0x4bd3,                                         \
    {0xab, 0xf4, 0x07, 0x98, 0x61, 0x51, 0x02, 0x2d}\
}


#define	HTTP_DEFLATE_TYPE		"deflate"
#define	HTTP_GZIP_TYPE	        "gzip"
#define	HTTP_X_GZIP_TYPE	    "x-gzip"
#define	HTTP_COMPRESS_TYPE	    "compress"
#define	HTTP_X_COMPRESS_TYPE	"x-compress"
#define	HTTP_IDENTITY_TYPE	    "identity"
#define	HTTP_UNCOMPRESSED_TYPE	"uncompressed"

typedef enum    {
        HTTP_COMPRESS_GZIP,
        HTTP_COMPRESS_DEFLATE,
        HTTP_COMPRESS_COMPRESS,
        HTTP_COMPRESS_IDENTITY
    }   CompressMode;

class nsHTTPCompressConv	: public nsIStreamConverter	{
public:
    // nsISupports methods
    NS_DECL_THREADSAFE_ISUPPORTS

	NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    // nsIStreamConverter methods
    NS_DECL_NSISTREAMCONVERTER


    nsHTTPCompressConv ();

private:

    virtual ~nsHTTPCompressConv ();

    nsIStreamListener   *mListener; // this guy gets the converted data via his OnDataAvailable ()
	CompressMode        mMode;

    unsigned char *mOutBuffer;
    unsigned char *mInpBuffer;

    uint32_t	mOutBufferLen;
    uint32_t	mInpBufferLen;
	
    nsCOMPtr<nsISupports>   mAsyncConvContext;
    nsCOMPtr<nsIStringInputStream>  mStream;

    nsresult do_OnDataAvailable (nsIRequest *request, nsISupports *aContext,
                                 uint64_t aSourceOffset, const char *buffer,
                                 uint32_t aCount);

    bool        mCheckHeaderDone;
    bool        mStreamEnded;
    bool        mStreamInitialized;
    bool        mDummyStreamInitialised;
    bool        mFailUncleanStops;

    z_stream d_stream;
    unsigned mLen, hMode, mSkipCount, mFlags;

    uint32_t check_header (nsIInputStream *iStr, uint32_t streamLen, nsresult *rv);
};


#endif
