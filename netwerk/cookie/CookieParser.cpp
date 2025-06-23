/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieParser.h"
#include "CookieLogging.h"
#include "CookieValidation.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/glean/NetwerkMetrics.h"
#include "mozilla/net/Cookie.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/TextUtils.h"
#include "nsIConsoleReportCollector.h"
#include "nsIScriptError.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "prprf.h"

constexpr char ATTRIBUTE_PATH[] = "path";
constexpr uint64_t ATTRIBUTE_MAX_LENGTH = 1024;

constexpr auto CONSOLE_INVALID_ATTRIBUTE_CATEGORY = "cookieInvalidAttribute"_ns;

namespace mozilla {
namespace net {

CookieParser::CookieParser(nsIConsoleReportCollector* aCRC, nsIURI* aHostURI)
    : mCRC(aCRC), mHostURI(aHostURI) {
  MOZ_COUNT_CTOR(CookieParser);
  MOZ_ASSERT(aCRC);
  MOZ_ASSERT(aHostURI);
}

CookieParser::~CookieParser() {
  MOZ_COUNT_DTOR(CookieParser);

  if (mValidation) {
    mValidation->ReportErrorsAndWarnings(mCRC, mHostURI);
  }

#define COOKIE_LOGGING_WITH_NAME(category, x)                 \
  CookieLogging::LogMessageToConsole(                         \
      mCRC, mHostURI, nsIScriptError::errorFlag, category, x, \
      AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(mCookieData.name())});

  switch (mRejection) {
    case NoRejection:
      break;

    case RejectedInvalidCharAttributes:
      COOKIE_LOGGING_WITH_NAME(CONSOLE_REJECTION_CATEGORY,
                               "CookieRejectedInvalidCharAttributes"_ns);
      break;

    case RejectedHttpOnlyButFromScript:
      COOKIE_LOGGING_WITH_NAME(CONSOLE_REJECTION_CATEGORY,
                               "CookieRejectedHttpOnlyButFromScript"_ns);
      break;

    case RejectedForeignNoPartitionedError:
      COOKIE_LOGGING_WITH_NAME(CONSOLE_CHIPS_CATEGORY,
                               "CookieForeignNoPartitionedError"_ns);
      break;

    case RejectedByPermissionManager:
      COOKIE_LOGGING_WITH_NAME(CONSOLE_REJECTION_CATEGORY,
                               "CookieRejectedByPermissionManager"_ns);
      break;

    case RejectedNonsecureOverSecure:
      COOKIE_LOGGING_WITH_NAME(CONSOLE_REJECTION_CATEGORY,
                               "CookieRejectedNonsecureOverSecure"_ns);
      break;
  }

#undef COOKIE_LOGGING_WITH_NAME

  if (mRejection != NoRejection || !mValidation ||
      mValidation->Result() != nsICookieValidation::eOK) {
    return;
  }

  for (const char* attribute : mWarnings.mAttributeOversize) {
    AutoTArray<nsString, 3> params = {NS_ConvertUTF8toUTF16(mCookieData.name()),
                                      NS_ConvertUTF8toUTF16(attribute)};

    nsString size;
    size.AppendInt(ATTRIBUTE_MAX_LENGTH);
    params.AppendElement(size);

    CookieLogging::LogMessageToConsole(
        mCRC, mHostURI, nsIScriptError::warningFlag, CONSOLE_OVERSIZE_CATEGORY,
        "CookieAttributeIgnored"_ns, params);
  }

  for (const char* attribute : mWarnings.mAttributeOverwritten) {
    CookieLogging::LogMessageToConsole(
        mCRC, mHostURI, nsIScriptError::warningFlag,
        CONSOLE_INVALID_ATTRIBUTE_CATEGORY, "CookieAttributeOverwritten"_ns,
        AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(mCookieData.name()),
                                NS_ConvertUTF8toUTF16(attribute)});
  }

  if (mWarnings.mInvalidSameSiteAttribute) {
    CookieLogging::LogMessageToConsole(
        mCRC, mHostURI, nsIScriptError::infoFlag, CONSOLE_SAMESITE_CATEGORY,
        "CookieSameSiteValueInvalid2"_ns,
        AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(mCookieData.name())});
  }

  if (mWarnings.mInvalidMaxAgeAttribute) {
    CookieLogging::LogMessageToConsole(
        mCRC, mHostURI, nsIScriptError::infoFlag,
        CONSOLE_INVALID_ATTRIBUTE_CATEGORY, "CookieInvalidMaxAgeAttribute"_ns,
        AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(mCookieData.name())});
  }

  if (mWarnings.mForeignNoPartitionedWarning) {
    CookieLogging::LogMessageToConsole(
        mCRC, mHostURI, nsIScriptError::warningFlag, CONSOLE_CHIPS_CATEGORY,
        "CookieForeignNoPartitionedWarning"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(mCookieData.name()),
        });
  }
}

// clang-format off
// The following comment block elucidates the function of ParseAttributes.
/******************************************************************************
 ** Augmented BNF, modified from RFC2109 Section 4.2.2 and RFC2616 Section 2.1
 ** please note: this BNF deviates from both specifications, and reflects this
 ** implementation. <bnf> indicates a reference to the defined grammar "bnf".

 ** Differences from RFC2109/2616 and explanations:
    1. implied *LWS
         The grammar described by this specification is word-based. Except
         where noted otherwise, linear white space (<LWS>) can be included
         between any two adjacent words (token or quoted-string), and
         between adjacent words and separators, without changing the
         interpretation of a field.
       <LWS> according to spec is SP|HT|CR|LF, but here, we allow only SP | HT.

    2. We use CR | LF as cookie separators, not ',' per spec, since ',' is in
       common use inside values.

    3. tokens and values have looser restrictions on allowed characters than
       spec. This is also due to certain characters being in common use inside
       values. We allow only '=' to separate token/value pairs, and ';' to
       terminate tokens or values. <LWS> is allowed within tokens and values
       (see bug 206022).

    4. where appropriate, full <OCTET>s are allowed, where the spec dictates to
       reject control chars or non-ASCII chars. This is erring on the loose
       side, since there's probably no good reason to enforce this strictness.

    5. Attribute "HttpOnly", not covered in the RFCs, is supported
       (see bug 178993).

 ** Begin BNF:
    token         = 1*<any allowed-chars except separators>
    value         = 1*<any allowed-chars except value-sep>
    separators    = ";" | "="
    value-sep     = ";"
    cookie-sep    = CR | LF
    allowed-chars = <any OCTET except cookie-sep>
    OCTET         = <any 8-bit sequence of data>
    LWS           = SP | HT
    CR            = <US-ASCII CR, carriage return (13)>
    LF            = <US-ASCII LF, linefeed (10)>
    SP            = <US-ASCII SP, space (32)>
    HT            = <US-ASCII HT, horizontal-tab (9)>

    set-cookie    = "Set-Cookie:" cookies
    cookies       = cookie *( cookie-sep cookie )
    cookie        = [NAME "="] VALUE *(";" cookie-av)    ; cookie NAME/VALUE must come first
    NAME          = token                                ; cookie name
    VALUE         = value                                ; cookie value
    cookie-av     = token ["=" value]

    valid values for cookie-av (checked post-parsing) are:
    cookie-av     = "Path"    "=" value
                  | "Domain"  "=" value
                  | "Expires" "=" value
                  | "Max-Age" "=" value
                  | "Comment" "=" value
                  | "Version" "=" value
                  | "Partitioned"
                  | "SameSite"
                  | "Secure"
                  | "HttpOnly"

******************************************************************************/
// clang-format on

// helper functions for GetTokenValue
static inline bool iswhitespace(char c) { return c == ' ' || c == '\t'; }
static inline bool isvalueseparator(char c) { return c == ';'; }
static inline bool istokenseparator(char c) {
  return isvalueseparator(c) || c == '=';
}

// Parse a single token/value pair.
// static
void CookieParser::GetTokenValue(nsACString::const_char_iterator& aIter,
                                 nsACString::const_char_iterator& aEndIter,
                                 nsDependentCSubstring& aTokenString,
                                 nsDependentCSubstring& aTokenValue,
                                 bool& aEqualsFound) {
  nsACString::const_char_iterator start;
  nsACString::const_char_iterator lastSpace;
  // initialize value string to clear garbage
  aTokenValue.Rebind(aIter, aIter);

  // find <token>, including any <LWS> between the end-of-token and the
  // token separator. we'll remove trailing <LWS> next
  while (aIter != aEndIter && iswhitespace(*aIter)) {
    ++aIter;
  }
  start = aIter;
  while (aIter != aEndIter && !istokenseparator(*aIter)) {
    ++aIter;
  }

  // remove trailing <LWS>; first check we're not at the beginning
  lastSpace = aIter;
  if (lastSpace != start) {
    while (--lastSpace != start && iswhitespace(*lastSpace)) {
    }
    ++lastSpace;
  }
  aTokenString.Rebind(start, lastSpace);

  aEqualsFound = (*aIter == '=');
  if (aEqualsFound) {
    // find <value>
    while (++aIter != aEndIter && iswhitespace(*aIter)) {
    }

    start = aIter;

    // process <token>
    // just look for ';' to terminate ('=' allowed)
    while (aIter != aEndIter && !isvalueseparator(*aIter)) {
      ++aIter;
    }

    // remove trailing <LWS>; first check we're not at the beginning
    if (aIter != start) {
      lastSpace = aIter;
      while (--lastSpace != start && iswhitespace(*lastSpace)) {
      }

      aTokenValue.Rebind(start, ++lastSpace);
    }
  }

  // aIter is on ';', or terminator, or EOS
  if (aIter != aEndIter) {
    // fall-through: aIter is on ';', increment and return
    ++aIter;
  }
}

// Tests for control characters, defined by RFC 5234 to be %x00-1F / %x7F.
// An exception is made for HTAB as the cookie spec treats that as whitespace.
static bool ContainsControlChars(const nsACString& aString) {
  const auto* start = aString.BeginReading();
  const auto* end = aString.EndReading();

  return std::find_if(start, end, [](unsigned char c) {
           return (c <= 0x1F && c != 0x09) || c == 0x7F;
         }) != end;
}

// static
bool CookieParser::CheckAttributeSize(const nsACString& currentValue,
                                      const char* aAttribute,
                                      const nsACString& aValue,
                                      CookieParser* aParser) {
  if (aValue.Length() > ATTRIBUTE_MAX_LENGTH) {
    if (aParser) {
      aParser->mWarnings.mAttributeOversize.AppendElement(aAttribute);
    }
    return false;
  }

  if (!currentValue.IsEmpty()) {
    if (aParser) {
      aParser->mWarnings.mAttributeOverwritten.AppendElement(aAttribute);
    }
  }

  return true;
}

// Parses attributes from cookie header. expires/max-age attributes aren't
// folded into the cookie struct here, because we don't know which one to use
// until we've parsed the header.
void CookieParser::ParseAttributes(nsCString& aCookieHeader,
                                   nsACString& aExpires, nsACString& aMaxage,
                                   bool& aAcceptedByParser) {
  aAcceptedByParser = false;

  static const char kDomain[] = "domain";
  static const char kExpires[] = "expires";
  static const char kMaxage[] = "max-age";
  static const char kSecure[] = "secure";
  static const char kHttpOnly[] = "httponly";
  static const char kSameSite[] = "samesite";
  static const char kSameSiteLax[] = "lax";
  static const char kSameSiteNone[] = "none";
  static const char kSameSiteStrict[] = "strict";
  static const char kPartitioned[] = "partitioned";

  nsACString::const_char_iterator cookieStart;
  aCookieHeader.BeginReading(cookieStart);

  nsACString::const_char_iterator cookieEnd;
  aCookieHeader.EndReading(cookieEnd);

  mCookieData.isSecure() = false;
  mCookieData.isHttpOnly() = false;
  mCookieData.isPartitioned() = false;
  mCookieData.sameSite() = nsICookie::SAMESITE_UNSET;

  nsDependentCSubstring tokenString(cookieStart, cookieStart);
  nsDependentCSubstring tokenValue(cookieStart, cookieStart);
  bool equalsFound;

  // extract cookie <NAME> & <VALUE> (first attribute), and copy the strings.
  // note: if there's no '=', we assume token is <VALUE>. this is required by
  //       some sites (see bug 169091).
  // XXX fix the parser to parse according to <VALUE> grammar for this case
  GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue, equalsFound);
  if (equalsFound) {
    mCookieData.name() = tokenString;
    mCookieData.value() = tokenValue;
  } else {
    mCookieData.value() = tokenString;
  }

  // extract remaining attributes
  while (cookieStart != cookieEnd) {
    GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue, equalsFound);

    if (ContainsControlChars(tokenString) || ContainsControlChars(tokenValue)) {
      RejectCookie(RejectedInvalidCharAttributes);
      return;
    }

    // decide which attribute we have, and copy the string
    if (tokenString.LowerCaseEqualsLiteral(ATTRIBUTE_PATH)) {
      if (CheckAttributeSize(mCookieData.path(), ATTRIBUTE_PATH, tokenValue,
                             this)) {
        mCookieData.path() = tokenValue;
      }

    } else if (tokenString.LowerCaseEqualsLiteral(kDomain)) {
      if (CheckAttributeSize(mCookieData.host(), kDomain, tokenValue, this)) {
        mCookieData.host() = tokenValue;
      }

    } else if (tokenString.LowerCaseEqualsLiteral(kExpires)) {
      if (CheckAttributeSize(aExpires, kExpires, tokenValue, this)) {
        aExpires = tokenValue;
      }

    } else if (tokenString.LowerCaseEqualsLiteral(kMaxage)) {
      if (CheckAttributeSize(aMaxage, kMaxage, tokenValue, this)) {
        aMaxage = tokenValue;
      }

      // ignore any tokenValue for isSecure; just set the boolean
    } else if (tokenString.LowerCaseEqualsLiteral(kSecure)) {
      mCookieData.isSecure() = true;

      // ignore any tokenValue for isPartitioned; just set the boolean
    } else if (tokenString.LowerCaseEqualsLiteral(kPartitioned)) {
      mCookieData.isPartitioned() = true;

      // ignore any tokenValue for isHttpOnly (see bug 178993);
      // just set the boolean
    } else if (tokenString.LowerCaseEqualsLiteral(kHttpOnly)) {
      mCookieData.isHttpOnly() = true;

    } else if (tokenString.LowerCaseEqualsLiteral(kSameSite)) {
      if (tokenValue.LowerCaseEqualsLiteral(kSameSiteLax)) {
        mCookieData.sameSite() = nsICookie::SAMESITE_LAX;
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteStrict)) {
        mCookieData.sameSite() = nsICookie::SAMESITE_STRICT;
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteNone)) {
        mCookieData.sameSite() = nsICookie::SAMESITE_NONE;
      } else {
        // Reset to Default if unknown token value (see Bug 1682450)
        mCookieData.sameSite() = nsICookie::SAMESITE_UNSET;
        mWarnings.mInvalidSameSiteAttribute = true;
      }
    }
  }

  // Cookie accepted.
  aAcceptedByParser = true;
}

namespace {

nsAutoCString GetPathFromURI(nsIURI* aHostURI) {
  // strip down everything after the last slash to get the path,
  // ignoring slashes in the query string part.
  // if we can QI to nsIURL, that'll take care of the query string portion.
  // otherwise, it's not an nsIURL and can't have a query string, so just find
  // the last slash.
  nsAutoCString path;
  nsCOMPtr<nsIURL> hostURL = do_QueryInterface(aHostURI);
  if (hostURL) {
    hostURL->GetDirectory(path);
  } else {
    aHostURI->GetPathQueryRef(path);
    int32_t slash = path.RFindChar('/');
    if (slash != kNotFound) {
      path.Truncate(slash + 1);
    }
  }

  // strip the right-most %x2F ("/") if the path doesn't contain only 1 '/'.
  int32_t lastSlash = path.RFindChar('/');
  int32_t firstSlash = path.FindChar('/');
  if (lastSlash != firstSlash && lastSlash != kNotFound &&
      lastSlash == static_cast<int32_t>(path.Length() - 1)) {
    path.Truncate(lastSlash);
  }

  return path;
}

}  // namespace

// static
void CookieParser::FixPath(CookieStruct& aCookieData, nsIURI* aHostURI) {
  // if a path is given, check the host has permission
  if (aCookieData.path().IsEmpty() || aCookieData.path().First() != '/') {
    nsAutoCString path = GetPathFromURI(aHostURI);
    if (CheckAttributeSize(aCookieData.path(), ATTRIBUTE_PATH, path)) {
      aCookieData.path() = path;
    }
  }
}

bool CookieParser::ParseMaxAgeAttribute(const nsACString& aMaxage,
                                        int64_t* aValue) {
  MOZ_ASSERT(aValue);

  if (aMaxage.IsEmpty()) {
    return false;
  }

  nsACString::const_char_iterator iter;
  aMaxage.BeginReading(iter);

  nsACString::const_char_iterator end;
  aMaxage.EndReading(end);

  // Negative values mean that the cookie is already expired. Don't bother to
  // parse.
  if (*iter == '-') {
    *aValue = INT64_MIN;
    return true;
  }

  CheckedInt<int64_t> value(0);

  for (; iter != end; ++iter) {
    if (!mozilla::IsAsciiDigit(*iter)) {
      mWarnings.mInvalidMaxAgeAttribute = true;
      return false;
    }

    value *= 10;
    if (!value.isValid()) {
      *aValue = INT64_MAX;
      return true;
    }

    value += *iter - '0';
    if (!value.isValid()) {
      *aValue = INT64_MAX;
      return true;
    }
  }

  *aValue = value.value();
  return true;
}

bool CookieParser::GetExpiry(CookieStruct& aCookieData,
                             const nsACString& aExpires,
                             const nsACString& aMaxage, int64_t aCurrentTime,
                             const nsACString& aDateHeader, bool aFromHttp) {
  int64_t maxageCap = StaticPrefs::network_cookie_maxageCap();

  /* Determine when the cookie should expire. This is done by taking the
   * difference between the server time and the time the server wants the cookie
   * to expire, and adding that difference to the client time. This localizes
   * the client time regardless of whether or not the TZ environment variable
   * was set on the client.
   *
   * Note: We need to consider accounting for network lag here, per RFC.
   */
  // check for max-age attribute first; this overrides expires attribute
  int64_t maxage = 0;
  if (ParseMaxAgeAttribute(aMaxage, &maxage)) {
    if (maxage == INT64_MIN) {
      aCookieData.expiry() = maxage;
    } else {
      CheckedInt<int64_t> value(aCurrentTime);
      value += (maxageCap ? std::min(maxage, maxageCap) : maxage) * 1000;

      aCookieData.expiry() = value.isValid() ? value.value() : INT64_MAX;
    }

    return false;
  }

  // check for expires attribute
  if (!aExpires.IsEmpty()) {
    // parse expiry time
    PRTime expiresTime;
    if (PR_ParseTimeString(aExpires.BeginReading(), true, &expiresTime) !=
        PR_SUCCESS) {
      return true;
    }

    int64_t expires = expiresTime / int64_t(PR_USEC_PER_MSEC);

    // If we have the server time, we can adjust the "expire" attribute value
    // by adding the delta between the server and the local times.  If the
    // current time is set in the future, we can consider valid cookies that
    // are not expired for the server.
    if (!aDateHeader.IsEmpty()) {
      MOZ_ASSERT(aFromHttp);

      PRTime dateHeaderTime;
      if (PR_ParseTimeString(aDateHeader.BeginReading(), true,
                             &dateHeaderTime) == PR_SUCCESS &&
          StaticPrefs::network_cookie_useServerTime()) {
        int64_t serverTime = dateHeaderTime / int64_t(PR_USEC_PER_MSEC);
        int64_t delta = aCurrentTime - serverTime;
        expires += delta;
      }
    }

    // If set-cookie used absolute time to set expiration, and it can't use
    // client time to set expiration.
    // Because if current time be set in the future, but the cookie expire
    // time be set less than current time and more than server time.
    // The cookie item have to be used to the expired cookie.

    aCookieData.expiry() =
        CookieCommons::MaybeReduceExpiry(aCurrentTime, expires);
    return false;
  }

  // default to session cookie if no attributes found.  Here we don't need to
  // enforce the maxage cap, because session cookies are short-lived by
  // definition.
  return true;
}

// static
void CookieParser::FixDomain(CookieStruct& aCookieData, nsIURI* aHostURI,
                             const nsACString& aBaseDomain,
                             bool aRequireHostMatch) {
  // Note: The logic in this function is mirrored in
  // toolkit/components/extensions/ext-cookies.js:checkSetCookiePermissions().
  // If it changes, please update that function, or file a bug for someone
  // else to do so.

  // get host from aHostURI
  nsAutoCString hostFromURI;
  nsContentUtils::GetHostOrIPv6WithBrackets(aHostURI, hostFromURI);

  // no domain specified, use hostFromURI
  if (aCookieData.host().IsEmpty()) {
    aCookieData.host() = hostFromURI;
    return;
  }

  nsCString cookieHost = aCookieData.host();

  // Tolerate leading '.' characters, but not if it's otherwise an empty host.
  if (cookieHost.Length() > 1 && cookieHost.First() == '.') {
    cookieHost.Cut(0, 1);
  }

  // switch to lowercase now, to avoid case-insensitive compares everywhere
  ToLowerCase(cookieHost);

  if (aRequireHostMatch) {
    // check whether the host is either an IP address, an alias such as
    // 'localhost', an eTLD such as 'co.uk', or the empty string. in these
    // cases, require an exact string match for the domain, and leave the cookie
    // as a non-domain one. bug 105917 originally noted the requirement to deal
    // with IP addresses.
    if (hostFromURI.Equals(cookieHost)) {
      aCookieData.host() = cookieHost;
    }

    // If the match fails, we keep the aCookieData.Host() as it was. The
    // Validator will reject the cookie with the correct reason.
    return;
  }

  // ensure the proposed domain is derived from the base domain; and also
  // that the host domain is derived from the proposed domain (per RFC2109).
  if (CookieCommons::IsSubdomainOf(cookieHost, aBaseDomain) &&
      CookieCommons::IsSubdomainOf(hostFromURI, cookieHost)) {
    // prepend a dot to indicate a domain cookie
    cookieHost.InsertLiteral(".", 0);
    aCookieData.host() = cookieHost;
  }

  /*
   * note: RFC2109 section 4.3.2 requires that we check the following:
   * that the portion of host not in domain does not contain a dot.
   * this prevents hosts of the form x.y.co.nz from setting cookies in the
   * entire .co.nz domain. however, it's only a only a partial solution and
   * it breaks sites (IE doesn't enforce it), so we don't perform this check.
   */
}

static void RecordPartitionedTelemetry(const CookieStruct& aCookieData,
                                       bool aIsForeign) {
  mozilla::glean::networking::set_cookie.Add(1);
  if (aCookieData.isPartitioned()) {
    mozilla::glean::networking::set_cookie_partitioned.AddToNumerator(1);
  }
  if (aIsForeign) {
    mozilla::glean::networking::set_cookie_foreign.AddToNumerator(1);
  }
  if (aIsForeign && aCookieData.isPartitioned()) {
    mozilla::glean::networking::set_cookie_foreign_partitioned.AddToNumerator(
        1);
  }
}

// processes a single cookie, and returns true if there are more cookies
// to be processed
void CookieParser::Parse(const nsACString& aBaseDomain, bool aRequireHostMatch,
                         CookieStatus aStatus, nsCString& aCookieHeader,
                         const nsACString& aDateHeader, bool aFromHttp,
                         bool aIsForeignAndNotAddon, bool aPartitionedOnly,
                         bool aIsInPrivateBrowsing, bool aOn3pcbException) {
  MOZ_ASSERT(!mValidation);

  // init expiryTime such that session cookies won't prematurely expire
  mCookieData.expiry() = INT64_MAX;

  mCookieData.schemeMap() = CookieCommons::URIToSchemeType(mHostURI);

  // aCookieHeader is an in/out param to point to the next cookie, if
  // there is one. Save the present value for logging purposes
  mCookieString.Assign(aCookieHeader);

  // newCookie says whether there are multiple cookies in the header;
  // so we can handle them separately.
  nsAutoCString expires;
  nsAutoCString maxage;
  bool acceptedByParser = false;
  ParseAttributes(aCookieHeader, expires, maxage, acceptedByParser);
  if (!acceptedByParser) {
    return;
  }

  int64_t currentTimeInUsec = PR_Now();

  // calculate expiry time of cookie.
  mCookieData.isSession() =
      GetExpiry(mCookieData, expires, maxage,
                currentTimeInUsec / PR_USEC_PER_MSEC, aDateHeader, aFromHttp);
  if (aStatus == STATUS_ACCEPT_SESSION) {
    // force lifetime to session. note that the expiration time, if set above,
    // will still apply.
    mCookieData.isSession() = true;
  }

  FixDomain(mCookieData, mHostURI, aBaseDomain, aRequireHostMatch);
  FixPath(mCookieData, mHostURI);

  // If the cookie is on the 3pcd exception list, we apply partitioned
  // attribute to the cookie.
  if (aOn3pcbException) {
    // We send a warning if the cookie doesn't have the partitioned attribute
    // in the foreign context.
    if (aPartitionedOnly && !mCookieData.isPartitioned() &&
        aIsForeignAndNotAddon) {
      mWarnings.mForeignNoPartitionedWarning = true;
    }

    mCookieData.isPartitioned() = true;
  }

  // If the cookie does not have the partitioned attribute,
  // but is foreign we should give the developer a message.
  // If CHIPS isn't required yet, we will warn the console
  // that we have upcoming changes. Otherwise we give a rejection message.
  if (aPartitionedOnly && !mCookieData.isPartitioned() &&
      aIsForeignAndNotAddon) {
    if (StaticPrefs::network_cookie_cookieBehavior_optInPartitioning() ||
        (aIsInPrivateBrowsing &&
         StaticPrefs::
             network_cookie_cookieBehavior_optInPartitioning_pbmode())) {
      RejectCookie(RejectedForeignNoPartitionedError);
      return;
    }

    mWarnings.mForeignNoPartitionedWarning = true;
  }

  mValidation = CookieValidation::ValidateInContext(
      mCookieData, mHostURI, aBaseDomain, aRequireHostMatch, aFromHttp,
      aIsForeignAndNotAddon, aPartitionedOnly, aIsInPrivateBrowsing);
  MOZ_ASSERT(mValidation);

  if (mValidation->Result() != nsICookieValidation::eOK) {
    return;
  }

  // We count SetCookie operations in the parent process only for HTTP set
  // cookies to prevent double counting.
  if (XRE_IsParentProcess() || !aFromHttp) {
    RecordPartitionedTelemetry(mCookieData, aIsForeignAndNotAddon);
  }
}

void CookieParser::RejectCookie(Rejection aRejection) {
  MOZ_ASSERT(mRejection == NoRejection);
  MOZ_ASSERT(aRejection != NoRejection);
  mRejection = aRejection;
}

void CookieParser::GetCookieString(nsACString& aCookieString) const {
  aCookieString.Assign(mCookieString);
}

}  // namespace net
}  // namespace mozilla
