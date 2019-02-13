/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpResponseHead_h__
#define nsHttpResponseHead_h__

#include "nsHttpHeaderArray.h"
#include "nsHttp.h"
#include "nsString.h"

// This needs to be forward declared here so we can include only this header
// without also including PHttpChannelParams.h
namespace IPC {
    template <typename> struct ParamTraits;
}

namespace mozilla { namespace net {

//-----------------------------------------------------------------------------
// nsHttpResponseHead represents the status line and headers from an HTTP
// response.
//-----------------------------------------------------------------------------

class nsHttpResponseHead
{
public:
    nsHttpResponseHead() : mVersion(NS_HTTP_VERSION_1_1)
                         , mStatus(200)
                         , mContentLength(-1)
                         , mCacheControlPrivate(false)
                         , mCacheControlNoStore(false)
                         , mCacheControlNoCache(false)
                         , mPragmaNoCache(false) {}

    const nsHttpHeaderArray & Headers()   const { return mHeaders; }
    nsHttpHeaderArray    &Headers()             { return mHeaders; }
    nsHttpVersion         Version()       const { return mVersion; }
// X11's Xlib.h #defines 'Status' to 'int' on some systems!
#undef Status
    uint16_t              Status()        const { return mStatus; }
    const nsAFlatCString &StatusText()    const { return mStatusText; }
    int64_t               ContentLength() const { return mContentLength; }
    const nsAFlatCString &ContentType()   const { return mContentType; }
    const nsAFlatCString &ContentCharset() const { return mContentCharset; }
    bool                  Private() const { return mCacheControlPrivate; }
    bool                  NoStore() const { return mCacheControlNoStore; }
    bool                  NoCache() const { return (mCacheControlNoCache || mPragmaNoCache); }
    /**
     * Full length of the entity. For byte-range requests, this may be larger
     * than ContentLength(), which will only represent the requested part of the
     * entity.
     */
    int64_t               TotalEntitySize() const;

    const char *PeekHeader(nsHttpAtom h) const      { return mHeaders.PeekHeader(h); }
    nsresult SetHeader(nsHttpAtom h, const nsACString &v, bool m=false);
    nsresult GetHeader(nsHttpAtom h, nsACString &v) const { return mHeaders.GetHeader(h, v); }
    void     ClearHeader(nsHttpAtom h)              { mHeaders.ClearHeader(h); }
    void     ClearHeaders()                         { mHeaders.Clear(); }

    const char *FindHeaderValue(nsHttpAtom h, const char *v) const
    {
      return mHeaders.FindHeaderValue(h, v);
    }
    bool        HasHeaderValue(nsHttpAtom h, const char *v) const
    {
      return mHeaders.HasHeaderValue(h, v);
    }

    void     SetContentType(const nsACString &s)    { mContentType = s; }
    void     SetContentCharset(const nsACString &s) { mContentCharset = s; }
    void     SetContentLength(int64_t);

    // write out the response status line and headers as a single text block,
    // optionally pruning out transient headers (ie. headers that only make
    // sense the first time the response is handled).
    void     Flatten(nsACString &, bool pruneTransients);

    // parse flattened response head. block must be null terminated. parsing is
    // destructive.
    nsresult Parse(char *block);

    // parse the status line. line must be null terminated.
    void     ParseStatusLine(const char *line);

    // parse a header line. line must be null terminated. parsing is destructive.
    nsresult ParseHeaderLine(const char *line);

    // cache validation support methods
    nsresult ComputeFreshnessLifetime(uint32_t *) const;
    nsresult ComputeCurrentAge(uint32_t now, uint32_t requestTime, uint32_t *result) const;
    bool     MustValidate() const;
    bool     MustValidateIfExpired() const;

    // returns true if the server appears to support byte range requests.
    bool     IsResumable() const;

    // returns true if the Expires header has a value in the past relative to the
    // value of the Date header.
    bool     ExpiresInPast() const;

    // update headers...
    nsresult UpdateHeaders(const nsHttpHeaderArray &headers);

    // reset the response head to it's initial state
    void     Reset();

    // these return failure if the header does not exist.
    nsresult ParseDateHeader(nsHttpAtom header, uint32_t *result) const;
    nsresult GetAgeValue(uint32_t *result) const;
    nsresult GetMaxAgeValue(uint32_t *result) const;
    nsresult GetDateValue(uint32_t *result) const
    {
        return ParseDateHeader(nsHttp::Date, result);
    }
    nsresult GetExpiresValue(uint32_t *result) const ;
    nsresult GetLastModifiedValue(uint32_t *result) const
    {
        return ParseDateHeader(nsHttp::Last_Modified, result);
    }

    bool operator==(const nsHttpResponseHead& aOther) const
    {
        return mHeaders == aOther.mHeaders &&
               mVersion == aOther.mVersion &&
               mStatus == aOther.mStatus &&
               mStatusText == aOther.mStatusText &&
               mContentLength == aOther.mContentLength &&
               mContentType == aOther.mContentType &&
               mContentCharset == aOther.mContentCharset &&
               mCacheControlPrivate == aOther.mCacheControlPrivate &&
               mCacheControlNoCache == aOther.mCacheControlNoCache &&
               mCacheControlNoStore == aOther.mCacheControlNoStore &&
               mPragmaNoCache == aOther.mPragmaNoCache;
    }

private:
    void     AssignDefaultStatusText();
    void     ParseVersion(const char *);
    void     ParseCacheControl(const char *);
    void     ParsePragma(const char *);

private:
    // All members must be copy-constructable and assignable
    nsHttpHeaderArray mHeaders;
    nsHttpVersion     mVersion;
    uint16_t          mStatus;
    nsCString         mStatusText;
    int64_t           mContentLength;
    nsCString         mContentType;
    nsCString         mContentCharset;
    bool              mCacheControlPrivate;
    bool              mCacheControlNoStore;
    bool              mCacheControlNoCache;
    bool              mPragmaNoCache;

    friend struct IPC::ParamTraits<nsHttpResponseHead>;
};
}} // namespace mozilla::net

#endif // nsHttpResponseHead_h__
