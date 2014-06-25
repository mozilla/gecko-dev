/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNetUtil.h"
#include "nsCRT.h"

#include "nsIFile.h"
#include <algorithm>

#ifdef MOZ_TOOLKIT_SEARCH
#include "nsIBrowserSearchService.h"
#endif

#include "nsIURIFixup.h"
#include "nsDefaultURIFixup.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"

// Used to check if external protocol schemes are usable
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

using namespace mozilla;

/* Implementation file */
NS_IMPL_ISUPPORTS(nsDefaultURIFixup, nsIURIFixup)

static bool sInitializedPrefCaches = false;
static bool sFixTypos = true;
static bool sFixupKeywords = true;

nsDefaultURIFixup::nsDefaultURIFixup()
{
  /* member initializers and constructor code */
}


nsDefaultURIFixup::~nsDefaultURIFixup()
{
  /* destructor code */
}

/* nsIURI createExposableURI (in nsIURI aURI); */
NS_IMETHODIMP
nsDefaultURIFixup::CreateExposableURI(nsIURI *aURI, nsIURI **aReturn)
{
    NS_ENSURE_ARG_POINTER(aURI);
    NS_ENSURE_ARG_POINTER(aReturn);

    bool isWyciwyg = false;
    aURI->SchemeIs("wyciwyg", &isWyciwyg);

    nsAutoCString userPass;
    aURI->GetUserPass(userPass);

    // most of the time we can just AddRef and return
    if (!isWyciwyg && userPass.IsEmpty())
    {
        *aReturn = aURI;
        NS_ADDREF(*aReturn);
        return NS_OK;
    }

    // Rats, we have to massage the URI
    nsCOMPtr<nsIURI> uri;
    if (isWyciwyg)
    {
        nsAutoCString path;
        nsresult rv = aURI->GetPath(path);
        NS_ENSURE_SUCCESS(rv, rv);

        uint32_t pathLength = path.Length();
        if (pathLength <= 2)
        {
            return NS_ERROR_FAILURE;
        }

        // Path is of the form "//123/http://foo/bar", with a variable number of digits.
        // To figure out where the "real" URL starts, search path for a '/', starting at 
        // the third character.
        int32_t slashIndex = path.FindChar('/', 2);
        if (slashIndex == kNotFound)
        {
            return NS_ERROR_FAILURE;
        }

        // Get the charset of the original URI so we can pass it to our fixed up URI.
        nsAutoCString charset;
        aURI->GetOriginCharset(charset);

        rv = NS_NewURI(getter_AddRefs(uri),
                   Substring(path, slashIndex + 1, pathLength - slashIndex - 1),
                   charset.get());
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else
    {
        // clone the URI so zapping user:pass doesn't change the original
        nsresult rv = aURI->Clone(getter_AddRefs(uri));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // hide user:pass unless overridden by pref
    if (Preferences::GetBool("browser.fixup.hide_user_pass", true))
    {
        uri->SetUserPass(EmptyCString());
    }

    // return the fixed-up URI
    *aReturn = uri;
    NS_ADDREF(*aReturn);
    return NS_OK;
}

/* nsIURI createFixupURI (in nsAUTF8String aURIText, in unsigned long aFixupFlags); */
NS_IMETHODIMP
nsDefaultURIFixup::CreateFixupURI(const nsACString& aStringURI, uint32_t aFixupFlags,
                                  nsIInputStream **aPostData, nsIURI **aURI)
{
    NS_ENSURE_ARG(!aStringURI.IsEmpty());
    NS_ENSURE_ARG_POINTER(aURI);

    nsresult rv;
    *aURI = nullptr;

    nsAutoCString uriString(aStringURI);
    uriString.Trim(" ");  // Cleanup the empty spaces that might be on each end.

    // Eliminate embedded newlines, which single-line text fields now allow:
    uriString.StripChars("\r\n");

    NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);

    nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoCString scheme;
    ioService->ExtractScheme(aStringURI, scheme);
    
    // View-source is a pseudo scheme. We're interested in fixing up the stuff
    // after it. The easiest way to do that is to call this method again with the
    // "view-source:" lopped off and then prepend it again afterwards.

    if (scheme.LowerCaseEqualsLiteral("view-source"))
    {
        nsCOMPtr<nsIURI> uri;
        uint32_t newFixupFlags = aFixupFlags & ~FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;

        rv =  CreateFixupURI(Substring(uriString,
                                       sizeof("view-source:") - 1,
                                       uriString.Length() -
                                         (sizeof("view-source:") - 1)),
                             newFixupFlags, aPostData, getter_AddRefs(uri));
        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;
        nsAutoCString spec;
        uri->GetSpec(spec);
        uriString.AssignLiteral("view-source:");
        uriString.Append(spec);
    }
    else {
        // Check for if it is a file URL
        FileURIFixup(uriString, aURI);
        if(*aURI)
            return NS_OK;

#if defined(XP_WIN)
        // Not a file URL, so translate '\' to '/' for convenience in the common protocols
        // e.g. catch
        //
        //   http:\\broken.com\address
        //   http:\\broken.com/blah
        //   broken.com\blah
        //
        // Code will also do partial fix up the following urls
        //
        //   http:\\broken.com\address/somewhere\image.jpg (stops at first forward slash)
        //   http:\\broken.com\blah?arg=somearg\foo.jpg (stops at question mark)
        //   http:\\broken.com#odd\ref (stops at hash)
        //  
        if (scheme.IsEmpty() ||
            scheme.LowerCaseEqualsLiteral("http") ||
            scheme.LowerCaseEqualsLiteral("https") ||
            scheme.LowerCaseEqualsLiteral("ftp"))
        {
            // Walk the string replacing backslashes with forward slashes until
            // the end is reached, or a question mark, or a hash, or a forward
            // slash. The forward slash test is to stop before trampling over
            // URIs which legitimately contain a mix of both forward and
            // backward slashes.
            nsAutoCString::iterator start;
            nsAutoCString::iterator end;
            uriString.BeginWriting(start);
            uriString.EndWriting(end);
            while (start != end) {
                if (*start == '?' || *start == '#' || *start == '/')
                    break;
                if (*start == '\\')
                    *start = '/';
                ++start;
            }
        }
#endif
    }

    if (!sInitializedPrefCaches) {
      // Check if we want to fix up common scheme typos.
      rv = Preferences::AddBoolVarCache(&sFixTypos,
                                        "browser.fixup.typo.scheme",
                                        sFixTypos);
      MOZ_ASSERT(NS_SUCCEEDED(rv),
                "Failed to observe \"browser.fixup.typo.scheme\"");

      rv = Preferences::AddBoolVarCache(&sFixupKeywords, "keyword.enabled",
                                        sFixupKeywords);
      MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to observe \"keyword.enabled\"");
      sInitializedPrefCaches = true;
    }

    // Fix up common scheme typos.
    if (sFixTypos && (aFixupFlags & FIXUP_FLAG_FIX_SCHEME_TYPOS)) {

        // Fast-path for common cases.
        if (scheme.IsEmpty() ||
            scheme.LowerCaseEqualsLiteral("http") ||
            scheme.LowerCaseEqualsLiteral("https") ||
            scheme.LowerCaseEqualsLiteral("ftp") ||
            scheme.LowerCaseEqualsLiteral("file")) {
            // Do nothing.
        } else if (scheme.LowerCaseEqualsLiteral("ttp")) {
            // ttp -> http.
            uriString.Replace(0, 3, "http");
            scheme.AssignLiteral("http");
        } else if (scheme.LowerCaseEqualsLiteral("ttps")) {
            // ttps -> https.
            uriString.Replace(0, 4, "https");
            scheme.AssignLiteral("https");
        } else if (scheme.LowerCaseEqualsLiteral("tps")) {
            // tps -> https.
            uriString.Replace(0, 3, "https");
            scheme.AssignLiteral("https");
        } else if (scheme.LowerCaseEqualsLiteral("ps")) {
            // ps -> https.
            uriString.Replace(0, 2, "https");
            scheme.AssignLiteral("https");
        } else if (scheme.LowerCaseEqualsLiteral("ile")) {
            // ile -> file.
            uriString.Replace(0, 3, "file");
            scheme.AssignLiteral("file");
        } else if (scheme.LowerCaseEqualsLiteral("le")) {
            // le -> file.
            uriString.Replace(0, 2, "file");
            scheme.AssignLiteral("file");
        }
    }

    // Now we need to check whether "scheme" is something we don't
    // really know about.
    nsCOMPtr<nsIProtocolHandler> ourHandler, extHandler;
    
    ioService->GetProtocolHandler(scheme.get(), getter_AddRefs(ourHandler));
    extHandler = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default");
    
    if (ourHandler != extHandler || !PossiblyHostPortUrl(uriString)) {
        // Just try to create an URL out of it
        rv = NS_NewURI(aURI, uriString, nullptr);

        if (!*aURI && rv != NS_ERROR_MALFORMED_URI) {
            return rv;
        }
    }

    if (*aURI && ourHandler == extHandler && sFixupKeywords &&
        (aFixupFlags & FIXUP_FLAG_FIX_SCHEME_TYPOS)) {
        nsCOMPtr<nsIExternalProtocolService> extProtService =
            do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
        if (extProtService) {
            bool handlerExists = false;
            rv = extProtService->ExternalProtocolHandlerExists(scheme.get(), &handlerExists);
            if (NS_FAILED(rv)) {
                return rv;
            }
            // This basically means we're dealing with a theoretically valid
            // URI... but we have no idea how to load it. (e.g. "christmas:humbug")
            // It's more likely the user wants to search, and so we
            // chuck this over to their preferred search provider instead:
            if (!handlerExists) {
                NS_RELEASE(*aURI);
                KeywordToURI(uriString, aPostData, aURI);
            }
        }
    }
    
    if (*aURI) {
        if (aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI)
            MakeAlternateURI(*aURI);
        return NS_OK;
    }

    // See if it is a keyword
    // Test whether keywords need to be fixed up
    if (sFixupKeywords && (aFixupFlags & FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP)) {
        KeywordURIFixup(uriString, aPostData, aURI);
        if(*aURI)
            return NS_OK;
    }

    // Prune duff protocol schemes
    //
    //   ://totallybroken.url.com
    //   //shorthand.url.com
    //
    if (StringBeginsWith(uriString, NS_LITERAL_CSTRING("://")))
    {
        uriString = StringTail(uriString, uriString.Length() - 3);
    }
    else if (StringBeginsWith(uriString, NS_LITERAL_CSTRING("//")))
    {
        uriString = StringTail(uriString, uriString.Length() - 2);
    }

    // Add ftp:// or http:// to front of url if it has no spec
    //
    // Should fix:
    //
    //   no-scheme.com
    //   ftp.no-scheme.com
    //   ftp4.no-scheme.com
    //   no-scheme.com/query?foo=http://www.foo.com
    //
    int32_t schemeDelim = uriString.Find("://",0);
    int32_t firstDelim = uriString.FindCharInSet("/:");
    if (schemeDelim <= 0 ||
        (firstDelim != -1 && schemeDelim > firstDelim)) {
        // find host name
        int32_t hostPos = uriString.FindCharInSet("/:?#");
        if (hostPos == -1) 
            hostPos = uriString.Length();

        // extract host name
        nsAutoCString hostSpec;
        uriString.Left(hostSpec, hostPos);

        // insert url spec corresponding to host name
        if (IsLikelyFTP(hostSpec))
            uriString.InsertLiteral("ftp://", 0);
        else 
            uriString.InsertLiteral("http://", 0);
    } // end if checkprotocol

    rv = NS_NewURI(aURI, uriString, nullptr);

    // Did the caller want us to try an alternative URI?
    // If so, attempt to fixup http://foo into http://www.foo.com

    if (*aURI && aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI) {
        MakeAlternateURI(*aURI);
    }

    // If we still haven't been able to construct a valid URI, try to force a
    // keyword match.  This catches search strings with '.' or ':' in them.
    if (!*aURI && sFixupKeywords)
    {
        KeywordToURI(aStringURI, aPostData, aURI);
        if(*aURI)
            return NS_OK;
    }

    return rv;
}

NS_IMETHODIMP nsDefaultURIFixup::KeywordToURI(const nsACString& aKeyword,
                                              nsIInputStream **aPostData,
                                              nsIURI **aURI)
{
    *aURI = nullptr;
    if (aPostData) {
        *aPostData = nullptr;
    }
    NS_ENSURE_STATE(Preferences::GetRootBranch());

    // Strip leading "?" and leading/trailing spaces from aKeyword
    nsAutoCString keyword(aKeyword);
    if (StringBeginsWith(keyword, NS_LITERAL_CSTRING("?"))) {
        keyword.Cut(0, 1);
    }
    keyword.Trim(" ");

    if (XRE_GetProcessType() == GeckoProcessType_Content) {
        dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
        if (!contentChild) {
            return NS_ERROR_NOT_AVAILABLE;
        }

        ipc::OptionalInputStreamParams postData;
        ipc::OptionalURIParams uri;
        if (!contentChild->SendKeywordToURI(keyword, &postData, &uri)) {
            return NS_ERROR_FAILURE;
        }

        if (aPostData) {
            nsTArray<ipc::FileDescriptor> fds;
            nsCOMPtr<nsIInputStream> temp = DeserializeInputStream(postData, fds);
            temp.forget(aPostData);

            MOZ_ASSERT(fds.IsEmpty());
        }

        nsCOMPtr<nsIURI> temp = DeserializeURI(uri);
        temp.forget(aURI);
        return NS_OK;
    }

#ifdef MOZ_TOOLKIT_SEARCH
    // Try falling back to the search service's default search engine
    nsCOMPtr<nsIBrowserSearchService> searchSvc = do_GetService("@mozilla.org/browser/search-service;1");
    if (searchSvc) {
        nsCOMPtr<nsISearchEngine> defaultEngine;
        searchSvc->GetDefaultEngine(getter_AddRefs(defaultEngine));
        if (defaultEngine) {
            nsCOMPtr<nsISearchSubmission> submission;
            nsAutoString responseType;
            // We allow default search plugins to specify alternate
            // parameters that are specific to keyword searches.
            NS_NAMED_LITERAL_STRING(mozKeywordSearch, "application/x-moz-keywordsearch");
            bool supportsResponseType = false;
            defaultEngine->SupportsResponseType(mozKeywordSearch, &supportsResponseType);
            if (supportsResponseType) {
                responseType.Assign(mozKeywordSearch);
            }

            defaultEngine->GetSubmission(NS_ConvertUTF8toUTF16(keyword),
                                         responseType,
                                         NS_LITERAL_STRING("keyword"),
                                         getter_AddRefs(submission));

            if (submission) {
                nsCOMPtr<nsIInputStream> postData;
                submission->GetPostData(getter_AddRefs(postData));
                if (aPostData) {
                  postData.forget(aPostData);
                } else if (postData) {
                  // The submission specifies POST data (i.e. the search
                  // engine's "method" is POST), but our caller didn't allow
                  // passing post data back. No point passing back a URL that
                  // won't load properly.
                  return NS_ERROR_FAILURE;
                }

                // This notification is meant for Firefox Health Report so it
                // can increment counts from the search engine. The assumption
                // here is that this keyword/submission will eventually result
                // in a search. Since we only generate a URI here, there is the
                // possibility we'll increment the counter without actually
                // incurring a search. A robust solution would involve currying
                // the search engine's name through various function calls.
                nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
                if (obsSvc) {
                  // Note that "keyword-search" refers to a search via the url
                  // bar, not a bookmarks keyword search.
                  obsSvc->NotifyObservers(defaultEngine, "keyword-search", NS_ConvertUTF8toUTF16(keyword).get());
                }

                return submission->GetUri(aURI);
            }
        }
    }
#endif

    // out of options
    return NS_ERROR_NOT_AVAILABLE;
}

bool nsDefaultURIFixup::MakeAlternateURI(nsIURI *aURI)
{
    if (!Preferences::GetRootBranch())
    {
        return false;
    }
    if (!Preferences::GetBool("browser.fixup.alternate.enabled", true))
    {
        return false;
    }

    // Code only works for http. Not for any other protocol including https!
    bool isHttp = false;
    aURI->SchemeIs("http", &isHttp);
    if (!isHttp) {
        return false;
    }

    // Security - URLs with user / password info should NOT be fixed up
    nsAutoCString userpass;
    aURI->GetUserPass(userpass);
    if (!userpass.IsEmpty()) {
        return false;
    }

    nsAutoCString oldHost;
    nsAutoCString newHost;
    aURI->GetHost(oldHost);

    // Count the dots
    int32_t numDots = 0;
    nsReadingIterator<char> iter;
    nsReadingIterator<char> iterEnd;
    oldHost.BeginReading(iter);
    oldHost.EndReading(iterEnd);
    while (iter != iterEnd) {
        if (*iter == '.')
            numDots++;
        ++iter;
    }


    // Get the prefix and suffix to stick onto the new hostname. By default these
    // are www. & .com but they could be any other value, e.g. www. & .org

    nsAutoCString prefix("www.");
    nsAdoptingCString prefPrefix =
        Preferences::GetCString("browser.fixup.alternate.prefix");
    if (prefPrefix)
    {
        prefix.Assign(prefPrefix);
    }

    nsAutoCString suffix(".com");
    nsAdoptingCString prefSuffix =
        Preferences::GetCString("browser.fixup.alternate.suffix");
    if (prefSuffix)
    {
        suffix.Assign(prefSuffix);
    }
    
    if (numDots == 0)
    {
        newHost.Assign(prefix);
        newHost.Append(oldHost);
        newHost.Append(suffix);
    }
    else if (numDots == 1)
    {
        if (!prefix.IsEmpty() &&
                oldHost.EqualsIgnoreCase(prefix.get(), prefix.Length())) {
            newHost.Assign(oldHost);
            newHost.Append(suffix);
        }
        else if (!suffix.IsEmpty()) {
            newHost.Assign(prefix);
            newHost.Append(oldHost);
        }
        else
        {
            // Do nothing
            return false;
        }
    }
    else
    {
        // Do nothing
        return false;
    }

    if (newHost.IsEmpty()) {
        return false;
    }

    // Assign the new host string over the old one
    aURI->SetHost(newHost);
    return true;
}

/**
 * Check if the host name starts with ftp\d*\. and it's not directly followed
 * by the tld.
 */
bool nsDefaultURIFixup::IsLikelyFTP(const nsCString &aHostSpec)
{
    bool likelyFTP = false;
    if (aHostSpec.EqualsIgnoreCase("ftp", 3)) {
        nsACString::const_iterator iter;
        nsACString::const_iterator end;
        aHostSpec.BeginReading(iter);
        aHostSpec.EndReading(end);
        iter.advance(3); // move past the "ftp" part

        while (iter != end)
        {
            if (*iter == '.') {
                // now make sure the name has at least one more dot in it
                ++iter;
                while (iter != end)
                {
                    if (*iter == '.') {
                        likelyFTP = true;
                        break;
                    }
                    ++iter;
                }
                break;
            }
            else if (!nsCRT::IsAsciiDigit(*iter)) {
                break;
            }
            ++iter;
        }
    }
    return likelyFTP;
}

nsresult nsDefaultURIFixup::FileURIFixup(const nsACString& aStringURI, 
                                         nsIURI** aURI)
{
    nsAutoCString uriSpecOut;

    nsresult rv = ConvertFileToStringURI(aStringURI, uriSpecOut);
    if (NS_SUCCEEDED(rv))
    {
        // if this is file url, uriSpecOut is already in FS charset
        if(NS_SUCCEEDED(NS_NewURI(aURI, uriSpecOut.get(), nullptr)))
            return NS_OK;
    } 
    return NS_ERROR_FAILURE;
}

nsresult nsDefaultURIFixup::ConvertFileToStringURI(const nsACString& aIn,
                                                   nsCString& aOut)
{
    bool attemptFixup = false;

#if defined(XP_WIN)
    // Check for \ in the url-string or just a drive (PC)
    if(kNotFound != aIn.FindChar('\\') ||
       (aIn.Length() == 2 && (aIn.Last() == ':' || aIn.Last() == '|')))
    {
        attemptFixup = true;
    }
#elif defined(XP_UNIX)
    // Check if it starts with / (UNIX)
    if(aIn.First() == '/')
    {
        attemptFixup = true;
    }
#else
    // Do nothing (All others for now) 
#endif

    if (attemptFixup)
    {
        // Test if this is a valid path by trying to create a local file
        // object. The URL of that is returned if successful.

        // NOTE: Please be sure to check that the call to NS_NewLocalFile
        //       rejects bad file paths when using this code on a new
        //       platform.

        nsCOMPtr<nsIFile> filePath;
        nsresult rv;

        // this is not the real fix but a temporary fix
        // in order to really fix the problem, we need to change the 
        // nsICmdLineService interface to use wstring to pass paramenters 
        // instead of string since path name and other argument could be
        // in non ascii.(see bug 87127) Since it is too risky to make interface change right
        // now, we decide not to do so now.
        // Therefore, the aIn we receive here maybe already in damage form
        // (e.g. treat every bytes as ISO-8859-1 and cast up to char16_t
        //  while the real data could be in file system charset )
        // we choice the following logic which will work for most of the case.
        // Case will still failed only if it meet ALL the following condiction:
        //    1. running on CJK, Russian, or Greek system, and 
        //    2. user type it from URL bar
        //    3. the file name contains character in the range of 
        //       U+00A1-U+00FF but encode as different code point in file
        //       system charset (e.g. ACP on window)- this is very rare case
        // We should remove this logic and convert to File system charset here
        // once we change nsICmdLineService to use wstring and ensure
        // all the Unicode data come in is correctly converted.
        // XXXbz nsICmdLineService doesn't hand back unicode, so in some cases
        // what we have is actually a "utf8" version of a "utf16" string that's
        // actually byte-expanded native-encoding data.  Someone upstream needs
        // to stop using AssignWithConversion and do things correctly.  See bug
        // 58866 for what happens if we remove this
        // PossiblyByteExpandedFileName check.
        NS_ConvertUTF8toUTF16 in(aIn);
        if (PossiblyByteExpandedFileName(in)) {
          // removes high byte
          rv = NS_NewNativeLocalFile(NS_LossyConvertUTF16toASCII(in), false, getter_AddRefs(filePath));
        }
        else {
          // input is unicode
          rv = NS_NewLocalFile(in, false, getter_AddRefs(filePath));
        }

        if (NS_SUCCEEDED(rv))
        {
            NS_GetURLSpecFromFile(filePath, aOut);
            return NS_OK;
        }
    }

    return NS_ERROR_FAILURE;
}

bool nsDefaultURIFixup::PossiblyHostPortUrl(const nsACString &aUrl)
{
    // Oh dear, the protocol is invalid. Test if the protocol might
    // actually be a url without a protocol:
    //
    //   http://www.faqs.org/rfcs/rfc1738.html
    //   http://www.faqs.org/rfcs/rfc2396.html
    //
    // e.g. Anything of the form:
    //
    //   <hostname>:<port> or
    //   <hostname>:<port>/
    //
    // Where <hostname> is a string of alphanumeric characters and dashes
    // separated by dots.
    // and <port> is a 5 or less digits. This actually breaks the rfc2396
    // definition of a scheme which allows dots in schemes.
    //
    // Note:
    //   People expecting this to work with
    //   <user>:<password>@<host>:<port>/<url-path> will be disappointed!
    //
    // Note: Parser could be a lot tighter, tossing out silly hostnames
    //       such as those containing consecutive dots and so on.

    // Read the hostname which should of the form
    // [a-zA-Z0-9\-]+(\.[a-zA-Z0-9\-]+)*:

    nsACString::const_iterator iterBegin;
    nsACString::const_iterator iterEnd;
    aUrl.BeginReading(iterBegin);
    aUrl.EndReading(iterEnd);
    nsACString::const_iterator iter = iterBegin;

    while (iter != iterEnd)
    {
        uint32_t chunkSize = 0;
        // Parse a chunk of the address
        while (iter != iterEnd &&
               (*iter == '-' ||
                nsCRT::IsAsciiAlpha(*iter) ||
                nsCRT::IsAsciiDigit(*iter)))
        {
            ++chunkSize;
            ++iter;
        }
        if (chunkSize == 0 || iter == iterEnd)
        {
            return false;
        }
        if (*iter == ':')
        {
            // Go onto checking the for the digits
            break;
        }
        if (*iter != '.')
        {
            // Whatever it is, it ain't a hostname!
            return false;
        }
        ++iter;
    }
    if (iter == iterEnd)
    {
        // No point continuing since there is no colon
        return false;
    }
    ++iter;

    // Count the number of digits after the colon and before the
    // next forward slash (or end of string)

    uint32_t digitCount = 0;
    while (iter != iterEnd && digitCount <= 5)
    {
        if (nsCRT::IsAsciiDigit(*iter))
        {
            digitCount++;
        }
        else if (*iter == '/')
        {
            break;
        }
        else
        {
            // Whatever it is, it ain't a port!
            return false;
        }
        ++iter;
    }
    if (digitCount == 0 || digitCount > 5)
    {
        // No digits or more digits than a port would have.
        return false;
    }

    // Yes, it's possibly a host:port url
    return true;
}

bool nsDefaultURIFixup::PossiblyByteExpandedFileName(const nsAString& aIn)
{
    // XXXXX HACK XXXXX : please don't copy this code.
    // There are cases where aIn contains the locale byte chars padded to short
    // (thus the name "ByteExpanded"); whereas other cases 
    // have proper Unicode code points.
    // This is a temporary fix.  Please refer to 58866, 86948

    nsReadingIterator<char16_t> iter;
    nsReadingIterator<char16_t> iterEnd;
    aIn.BeginReading(iter);
    aIn.EndReading(iterEnd);
    while (iter != iterEnd)
    {
        if (*iter >= 0x0080 && *iter <= 0x00FF)
            return true;
        ++iter;
    }
    return false;
}

void nsDefaultURIFixup::KeywordURIFixup(const nsACString & aURIString,
                                        nsIInputStream **aPostData,
                                        nsIURI** aURI)
{
    // These are keyword formatted strings
    // "what is mozilla"
    // "what is mozilla?"
    // "docshell site:mozilla.org" - has no dot/colon in the first space-separated substring
    // "?mozilla" - anything that begins with a question mark
    // "?site:mozilla.org docshell"
    // Things that have a quote before the first dot/colon

    // These are not keyword formatted strings
    // "www.blah.com" - first space-separated substring contains a dot, doesn't start with "?"
    // "www.blah.com stuff"
    // "nonQualifiedHost:80" - first space-separated substring contains a colon, doesn't start with "?"
    // "nonQualifiedHost:80 args"
    // "nonQualifiedHost?"
    // "nonQualifiedHost?args"
    // "nonQualifiedHost?some args"

    // Note: uint32_t(kNotFound) is greater than any actual location
    // in practice.  So if we cast all locations to uint32_t, then a <
    // b guarantees that either b is kNotFound and a is found, or both
    // are found and a found before b.
    uint32_t dotLoc   = uint32_t(aURIString.FindChar('.'));
    uint32_t colonLoc = uint32_t(aURIString.FindChar(':'));
    uint32_t spaceLoc = uint32_t(aURIString.FindChar(' '));
    if (spaceLoc == 0) {
        // Treat this as not found
        spaceLoc = uint32_t(kNotFound);
    }
    uint32_t qMarkLoc = uint32_t(aURIString.FindChar('?'));
    uint32_t quoteLoc = std::min(uint32_t(aURIString.FindChar('"')),
                               uint32_t(aURIString.FindChar('\'')));

    if (((spaceLoc < dotLoc || quoteLoc < dotLoc) &&
         (spaceLoc < colonLoc || quoteLoc < colonLoc) &&
         (spaceLoc < qMarkLoc || quoteLoc < qMarkLoc)) ||
        qMarkLoc == 0)
    {
        KeywordToURI(aURIString, aPostData, aURI);
    }
}


nsresult NS_NewURIFixup(nsIURIFixup **aURIFixup)
{
    nsDefaultURIFixup *fixup = new nsDefaultURIFixup;
    if (fixup == nullptr)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return fixup->QueryInterface(NS_GET_IID(nsIURIFixup), (void **) aURIFixup);
}

