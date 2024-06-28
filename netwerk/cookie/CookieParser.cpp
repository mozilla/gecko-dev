/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieParser.h"

#include "nsIURI.h"
#include "nsIURL.h"
#include "prprf.h"

constexpr char ATTRIBUTE_PATH[] = "path";

constexpr auto CONSOLE_CHIPS_CATEGORY = "cookiesCHIPS"_ns;
constexpr auto CONSOLE_OVERSIZE_CATEGORY = "cookiesOversize"_ns;
constexpr auto CONSOLE_REJECTION_CATEGORY = "cookiesRejection"_ns;
constexpr auto CONSOLE_SAMESITE_CATEGORY = "cookieSameSite"_ns;
constexpr auto SAMESITE_MDN_URL =
    "https://developer.mozilla.org/docs/Web/HTTP/Headers/Set-Cookie/"
    u"SameSite"_ns;

namespace mozilla {
namespace net {

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
static inline bool isterminator(char c) { return c == '\n' || c == '\r'; }
static inline bool isvalueseparator(char c) {
  return isterminator(c) || c == ';';
}
static inline bool istokenseparator(char c) {
  return isvalueseparator(c) || c == '=';
}

// Parse a single token/value pair.
// Returns true if a cookie terminator is found, so caller can parse new cookie.
// static
bool CookieParser::GetTokenValue(nsACString::const_char_iterator& aIter,
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
    // if on terminator, increment past & return true to process new cookie
    if (isterminator(*aIter)) {
      ++aIter;
      return true;
    }
    // fall-through: aIter is on ';', increment and return false
    ++aIter;
  }
  return false;
}

static inline void SetSameSiteAttribute(CookieStruct& aCookieData,
                                        int32_t aValue) {
  aCookieData.sameSite() = aValue;
  aCookieData.rawSameSite() = aValue;
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

static inline void SetSameSiteAttributeDefault(CookieStruct& aCookieData) {
  // Set cookie with SameSite attribute that is treated as Default
  // and doesn't requires changing the DB schema.
  aCookieData.sameSite() = nsICookie::SAMESITE_LAX;
  aCookieData.rawSameSite() = nsICookie::SAMESITE_NONE;
}

bool CheckAttributeSize(nsIConsoleReportCollector* aCRC, nsIURI* aHostURI,
                        const CookieStruct& aCookieData, const char* aAttribute,
                        const nsACString& aValue) {
  static uint16_t kMaxAttributeLength = 1024;

  if (aValue.Length() > kMaxAttributeLength) {
    AutoTArray<nsString, 3> params = {NS_ConvertUTF8toUTF16(aCookieData.name()),
                                      NS_ConvertUTF8toUTF16(aAttribute)};

    nsString size;
    size.AppendInt(kMaxAttributeLength);
    params.AppendElement(size);

    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_OVERSIZE_CATEGORY,
        "CookieAttributeIgnored"_ns, params);

    return false;
  }

  return true;
}

// Parses attributes from cookie header. expires/max-age attributes aren't
// folded into the cookie struct here, because we don't know which one to use
// until we've parsed the header.
// static
bool CookieParser::ParseAttributes(nsIConsoleReportCollector* aCRC,
                                   nsIURI* aHostURI, nsCString& aCookieHeader,
                                   CookieStruct& aCookieData,
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

  aCookieData.isSecure() = false;
  aCookieData.isHttpOnly() = false;

  SetSameSiteAttributeDefault(aCookieData);

  nsDependentCSubstring tokenString(cookieStart, cookieStart);
  nsDependentCSubstring tokenValue(cookieStart, cookieStart);
  bool newCookie;
  bool equalsFound;

  // extract cookie <NAME> & <VALUE> (first attribute), and copy the strings.
  // if we find multiple cookies, return for processing
  // note: if there's no '=', we assume token is <VALUE>. this is required by
  //       some sites (see bug 169091).
  // XXX fix the parser to parse according to <VALUE> grammar for this case
  newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue,
                            equalsFound);
  if (equalsFound) {
    aCookieData.name() = tokenString;
    aCookieData.value() = tokenValue;
  } else {
    aCookieData.value() = tokenString;
  }

  // extract remaining attributes
  while (cookieStart != cookieEnd && !newCookie) {
    newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue,
                              equalsFound);

    if (ContainsControlChars(tokenString) || ContainsControlChars(tokenValue)) {
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::errorFlag, CONSOLE_REJECTION_CATEGORY,
          "CookieRejectedInvalidCharAttributes"_ns,
          AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
      return newCookie;
    }

    // decide which attribute we have, and copy the string
    if (tokenString.LowerCaseEqualsLiteral(ATTRIBUTE_PATH)) {
      if (CheckAttributeSize(aCRC, aHostURI, aCookieData, ATTRIBUTE_PATH,
                             tokenValue)) {
        aCookieData.path() = tokenValue;
      }

    } else if (tokenString.LowerCaseEqualsLiteral(kDomain)) {
      if (CheckAttributeSize(aCRC, aHostURI, aCookieData, kDomain,
                             tokenValue)) {
        aCookieData.host() = tokenValue;
      }

    } else if (tokenString.LowerCaseEqualsLiteral(kExpires)) {
      if (CheckAttributeSize(aCRC, aHostURI, aCookieData, kExpires,
                             tokenValue)) {
        aExpires = tokenValue;
      }

    } else if (tokenString.LowerCaseEqualsLiteral(kMaxage)) {
      if (CheckAttributeSize(aCRC, aHostURI, aCookieData, kMaxage,
                             tokenValue)) {
        aMaxage = tokenValue;
      }

      // ignore any tokenValue for isSecure; just set the boolean
    } else if (tokenString.LowerCaseEqualsLiteral(kSecure)) {
      aCookieData.isSecure() = true;

      // ignore any tokenValue for isPartitioned; just set the boolean
    } else if (tokenString.LowerCaseEqualsLiteral(kPartitioned)) {
      aCookieData.isPartitioned() = true;

      // ignore any tokenValue for isHttpOnly (see bug 178993);
      // just set the boolean
    } else if (tokenString.LowerCaseEqualsLiteral(kHttpOnly)) {
      aCookieData.isHttpOnly() = true;

    } else if (tokenString.LowerCaseEqualsLiteral(kSameSite)) {
      if (tokenValue.LowerCaseEqualsLiteral(kSameSiteLax)) {
        SetSameSiteAttribute(aCookieData, nsICookie::SAMESITE_LAX);
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteStrict)) {
        SetSameSiteAttribute(aCookieData, nsICookie::SAMESITE_STRICT);
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteNone)) {
        SetSameSiteAttribute(aCookieData, nsICookie::SAMESITE_NONE);
      } else {
        // Reset to Default if unknown token value (see Bug 1682450)
        SetSameSiteAttributeDefault(aCookieData);
        CookieLogging::LogMessageToConsole(
            aCRC, aHostURI, nsIScriptError::infoFlag, CONSOLE_SAMESITE_CATEGORY,
            "CookieSameSiteValueInvalid2"_ns,
            AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
      }
    }
  }

  // re-assign aCookieHeader, in case we need to process another cookie
  aCookieHeader.Assign(Substring(cookieStart, cookieEnd));

  // If same-site is explicitly set to 'none' but this is not a secure context,
  // let's abort the parsing.
  if (!aCookieData.isSecure() &&
      aCookieData.sameSite() == nsICookie::SAMESITE_NONE) {
    if (StaticPrefs::network_cookie_sameSite_noneRequiresSecure()) {
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::errorFlag, CONSOLE_SAMESITE_CATEGORY,
          "CookieRejectedNonRequiresSecure2"_ns,
          AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
      return newCookie;
    }

    // Still warn about the missing Secure attribute when not enforcing.
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_SAMESITE_CATEGORY,
        "CookieRejectedNonRequiresSecureForBeta3"_ns,
        AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(aCookieData.name()),
                                SAMESITE_MDN_URL});
  }

  // Ensure the partitioned cookie is set with the secure attribute if CHIPS
  // is enabled.
  if (StaticPrefs::network_cookie_CHIPS_enabled() &&
      aCookieData.isPartitioned() && !aCookieData.isSecure()) {
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::errorFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedPartitionedRequiresSecure"_ns,
        AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});

    return newCookie;
  }

  if (aCookieData.rawSameSite() == nsICookie::SAMESITE_NONE &&
      aCookieData.sameSite() == nsICookie::SAMESITE_LAX) {
    bool laxByDefault =
        StaticPrefs::network_cookie_sameSite_laxByDefault() &&
        !nsContentUtils::IsURIInPrefList(
            aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");
    if (laxByDefault) {
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::infoFlag, CONSOLE_SAMESITE_CATEGORY,
          "CookieLaxForced2"_ns,
          AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
    } else {
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::warningFlag,
          CONSOLE_SAMESITE_CATEGORY, "CookieLaxForcedForBeta2"_ns,
          AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(aCookieData.name()),
                                  SAMESITE_MDN_URL});
    }
  }

  // Cookie accepted.
  aAcceptedByParser = true;

  MOZ_ASSERT(Cookie::ValidateSameSite(aCookieData));
  return newCookie;
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

bool CookieParser::CheckPath(CookieStruct& aCookieData,
                             nsIConsoleReportCollector* aCRC,
                             nsIURI* aHostURI) {
  // if a path is given, check the host has permission
  if (aCookieData.path().IsEmpty() || aCookieData.path().First() != '/') {
    nsAutoCString path = GetPathFromURI(aHostURI);
    if (CheckAttributeSize(aCRC, aHostURI, aCookieData, ATTRIBUTE_PATH, path)) {
      aCookieData.path() = path;
    }
  }

  MOZ_ASSERT(CookieCommons::CheckPathSize(aCookieData));

  return !aCookieData.path().Contains('\t');
}

// static
bool CookieParser::HasSecurePrefix(const nsACString& aString) {
  return StringBeginsWith(aString, "__Secure-"_ns,
                          nsCaseInsensitiveCStringComparator);
}

// static
bool CookieParser::HasHostPrefix(const nsACString& aString) {
  return StringBeginsWith(aString, "__Host-"_ns,
                          nsCaseInsensitiveCStringComparator);
}

// CheckPrefixes
//
// Reject cookies whose name starts with the magic prefixes from
// https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-rfc6265bis
// if they do not meet the criteria required by the prefix.
//
// Must not be called until after CheckDomain() and CheckPath() have
// regularized and validated the CookieStruct values!
bool CookieParser::CheckPrefixes(CookieStruct& aCookieData,
                                 bool aSecureRequest) {
  bool hasSecurePrefix = HasSecurePrefix(aCookieData.name());
  bool hasHostPrefix = HasHostPrefix(aCookieData.name());

  if (!hasSecurePrefix && !hasHostPrefix) {
    // not one of the magic prefixes: carry on
    return true;
  }

  if (!aSecureRequest || !aCookieData.isSecure()) {
    // the magic prefixes may only be used from a secure request and
    // the secure attribute must be set on the cookie
    return false;
  }

  if (hasHostPrefix) {
    // The host prefix requires that the path is "/" and that the cookie
    // had no domain attribute. CheckDomain() and CheckPath() MUST be run
    // first to make sure invalid attributes are rejected and to regularlize
    // them. In particular all explicit domain attributes result in a host
    // that starts with a dot, and if the host doesn't start with a dot it
    // correctly matches the true host.
    if (aCookieData.host()[0] == '.' ||
        !aCookieData.path().EqualsLiteral("/")) {
      return false;
    }
  }

  return true;
}

bool CookieParser::GetExpiry(CookieStruct& aCookieData,
                             const nsACString& aExpires,
                             const nsACString& aMaxage, int64_t aCurrentTime,
                             bool aFromHttp) {
  // maxageCap is in seconds.
  // Disabled for HTTP cookies.
  int64_t maxageCap =
      aFromHttp ? 0 : StaticPrefs::privacy_documentCookies_maxage();

  /* Determine when the cookie should expire. This is done by taking the
   * difference between the server time and the time the server wants the cookie
   * to expire, and adding that difference to the client time. This localizes
   * the client time regardless of whether or not the TZ environment variable
   * was set on the client.
   *
   * Note: We need to consider accounting for network lag here, per RFC.
   */
  // check for max-age attribute first; this overrides expires attribute
  if (!aMaxage.IsEmpty()) {
    // obtain numeric value of maxageAttribute
    int64_t maxage;
    int32_t numInts = PR_sscanf(aMaxage.BeginReading(), "%lld", &maxage);

    // default to session cookie if the conversion failed
    if (numInts != 1) {
      return true;
    }

    // if this addition overflows, expiryTime will be less than currentTime
    // and the cookie will be expired - that's okay.
    if (maxageCap) {
      aCookieData.expiry() = aCurrentTime + std::min(maxage, maxageCap);
    } else {
      aCookieData.expiry() = aCurrentTime + maxage;
    }

    // check for expires attribute
  } else if (!aExpires.IsEmpty()) {
    PRTime expires;

    // parse expiry time
    if (PR_ParseTimeString(aExpires.BeginReading(), true, &expires) !=
        PR_SUCCESS) {
      return true;
    }

    // If set-cookie used absolute time to set expiration, and it can't use
    // client time to set expiration.
    // Because if current time be set in the future, but the cookie expire
    // time be set less than current time and more than server time.
    // The cookie item have to be used to the expired cookie.
    if (maxageCap) {
      aCookieData.expiry() = std::min(expires / int64_t(PR_USEC_PER_SEC),
                                      aCurrentTime + maxageCap);
    } else {
      aCookieData.expiry() = expires / int64_t(PR_USEC_PER_SEC);
    }

    // default to session cookie if no attributes found.  Here we don't need to
    // enforce the maxage cap, because session cookies are short-lived by
    // definition.
  } else {
    return true;
  }

  return false;
}

// returns true if 'a' is equal to or a subdomain of 'b',
// assuming no leading dots are present.
static inline bool IsSubdomainOf(const nsACString& a, const nsACString& b) {
  if (a == b) {
    return true;
  }
  if (a.Length() > b.Length()) {
    return a[a.Length() - b.Length() - 1] == '.' && StringEndsWith(a, b);
  }
  return false;
}

// processes domain attribute, and returns true if host has permission to set
// for this domain.
// static
bool CookieParser::CheckDomain(CookieStruct& aCookieData, nsIURI* aHostURI,
                               const nsACString& aBaseDomain,
                               bool aRequireHostMatch) {
  // Note: The logic in this function is mirrored in
  // toolkit/components/extensions/ext-cookies.js:checkSetCookiePermissions().
  // If it changes, please update that function, or file a bug for someone
  // else to do so.

  // get host from aHostURI
  nsAutoCString hostFromURI;
  nsContentUtils::GetHostOrIPv6WithBrackets(aHostURI, hostFromURI);

  // if a domain is given, check the host has permission
  if (!aCookieData.host().IsEmpty()) {
    // Tolerate leading '.' characters, but not if it's otherwise an empty host.
    if (aCookieData.host().Length() > 1 && aCookieData.host().First() == '.') {
      aCookieData.host().Cut(0, 1);
    }

    // switch to lowercase now, to avoid case-insensitive compares everywhere
    ToLowerCase(aCookieData.host());

    // check whether the host is either an IP address, an alias such as
    // 'localhost', an eTLD such as 'co.uk', or the empty string. in these
    // cases, require an exact string match for the domain, and leave the cookie
    // as a non-domain one. bug 105917 originally noted the requirement to deal
    // with IP addresses.
    if (aRequireHostMatch) {
      return hostFromURI.Equals(aCookieData.host());
    }

    // ensure the proposed domain is derived from the base domain; and also
    // that the host domain is derived from the proposed domain (per RFC2109).
    if (IsSubdomainOf(aCookieData.host(), aBaseDomain) &&
        IsSubdomainOf(hostFromURI, aCookieData.host())) {
      // prepend a dot to indicate a domain cookie
      aCookieData.host().InsertLiteral(".", 0);
      return true;
    }

    /*
     * note: RFC2109 section 4.3.2 requires that we check the following:
     * that the portion of host not in domain does not contain a dot.
     * this prevents hosts of the form x.y.co.nz from setting cookies in the
     * entire .co.nz domain. however, it's only a only a partial solution and
     * it breaks sites (IE doesn't enforce it), so we don't perform this check.
     */
    return false;
  }

  // no domain specified, use hostFromURI
  aCookieData.host() = hostFromURI;
  return true;
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
bool CookieParser::CanSetCookie(
    nsIURI* aHostURI, const nsACString& aBaseDomain, CookieStruct& aCookieData,
    bool aRequireHostMatch, CookieStatus aStatus, nsCString& aCookieHeader,
    bool aFromHttp, bool aIsForeignAndNotAddon, bool aPartitionedOnly,
    bool aIsInPrivateBrowsing, nsIConsoleReportCollector* aCRC,
    bool& aSetCookie) {
  MOZ_ASSERT(aHostURI);

  aSetCookie = false;

  // init expiryTime such that session cookies won't prematurely expire
  aCookieData.expiry() = INT64_MAX;

  aCookieData.schemeMap() = CookieCommons::URIToSchemeType(aHostURI);

  // aCookieHeader is an in/out param to point to the next cookie, if
  // there is one. Save the present value for logging purposes
  nsCString savedCookieHeader(aCookieHeader);

  // newCookie says whether there are multiple cookies in the header;
  // so we can handle them separately.
  nsAutoCString expires;
  nsAutoCString maxage;
  bool acceptedByParser = false;
  bool newCookie = ParseAttributes(aCRC, aHostURI, aCookieHeader, aCookieData,
                                   expires, maxage, acceptedByParser);
  if (!acceptedByParser) {
    return newCookie;
  }

  // Collect telemetry on how often secure cookies are set from non-secure
  // origins, and vice-versa.
  //
  // 0 = nonsecure and "http:"
  // 1 = nonsecure and "https:"
  // 2 = secure and "http:"
  // 3 = secure and "https:"
  bool potentiallyTrustworthy =
      nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(aHostURI);

  int64_t currentTimeInUsec = PR_Now();

  // calculate expiry time of cookie.
  aCookieData.isSession() =
      GetExpiry(aCookieData, expires, maxage,
                currentTimeInUsec / PR_USEC_PER_SEC, aFromHttp);
  if (aStatus == STATUS_ACCEPT_SESSION) {
    // force lifetime to session. note that the expiration time, if set above,
    // will still apply.
    aCookieData.isSession() = true;
  }

  // reject cookie if name and value are empty, per RFC6265bis
  if (aCookieData.name().IsEmpty() && aCookieData.value().IsEmpty()) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "cookie name and value are empty");

    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedEmptyNameAndValue"_ns, nsTArray<nsString>());
    return newCookie;
  }

  // reject cookie if it's over the size limit, per RFC2109
  if (!CookieCommons::CheckNameAndValueSize(aCookieData)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "cookie too big (> 4kb)");

    AutoTArray<nsString, 2> params = {
        NS_ConvertUTF8toUTF16(aCookieData.name())};

    nsString size;
    size.AppendInt(kMaxBytesPerCookie);
    params.AppendElement(size);

    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_OVERSIZE_CATEGORY,
        "CookieOversize"_ns, params);
    return newCookie;
  }

  CookieCommons::RecordUnicodeTelemetry(aCookieData);

  // We count SetCookie operations in the parent process only for HTTP set
  // cookies to prevent double counting.
  if (XRE_IsParentProcess() || !aFromHttp) {
    RecordPartitionedTelemetry(aCookieData, aIsForeignAndNotAddon);
  }

  if (!CookieCommons::CheckName(aCookieData)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "invalid name character");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedInvalidCharName"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // domain & path checks
  if (!CheckDomain(aCookieData, aHostURI, aBaseDomain, aRequireHostMatch)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the domain tests");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedInvalidDomain"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  if (!CheckPath(aCookieData, aCRC, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the path tests");
    return newCookie;
  }

  // If a cookie is nameless, then its value must not start with
  // `__Host-` or `__Secure-`
  if (aCookieData.name().IsEmpty() && (HasSecurePrefix(aCookieData.value()) ||
                                       HasHostPrefix(aCookieData.value()))) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed hidden prefix tests");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedInvalidPrefix"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // magic prefix checks. MUST be run after CheckDomain() and CheckPath()
  if (!CheckPrefixes(aCookieData, potentiallyTrustworthy)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the prefix tests");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedInvalidPrefix"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  if (!CookieCommons::CheckValue(aCookieData)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "invalid value character");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedInvalidCharValue"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // if the new cookie is httponly, make sure we're not coming from script
  if (!aFromHttp && aCookieData.isHttpOnly()) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "cookie is httponly; coming from script");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedHttpOnlyButFromScript"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // If the new cookie is non-https and wants to set secure flag,
  // browser have to ignore this new cookie.
  // (draft-ietf-httpbis-cookie-alone section 3.1)
  if (aCookieData.isSecure() && !potentiallyTrustworthy) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                      "non-https cookie can't set secure flag");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedSecureButNonHttps"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // If the new cookie is same-site but in a cross site context,
  // browser must ignore the cookie.
  bool laxByDefault =
      StaticPrefs::network_cookie_sameSite_laxByDefault() &&
      !nsContentUtils::IsURIInPrefList(
          aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");
  auto effectiveSameSite =
      laxByDefault ? aCookieData.sameSite() : aCookieData.rawSameSite();
  if ((effectiveSameSite != nsICookie::SAMESITE_NONE) &&
      aIsForeignAndNotAddon) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the samesite tests");

    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_SAMESITE_CATEGORY,
        "CookieRejectedForNonSameSiteness"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // If the cookie does not have the partitioned attribute,
  // but is foreign we should give the developer a message.
  // If CHIPS isn't required yet, we will warn the console
  // that we have upcoming changes. Otherwise we give a rejection message.
  if (aPartitionedOnly && !aCookieData.isPartitioned() &&
      aIsForeignAndNotAddon) {
    if (StaticPrefs::network_cookie_cookieBehavior_optInPartitioning() ||
        (aIsInPrivateBrowsing &&
         StaticPrefs::
             network_cookie_cookieBehavior_optInPartitioning_pbmode())) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                        "foreign cookies must be partitioned");
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_CHIPS_CATEGORY,
          "CookieForeignNoPartitionedError"_ns,
          AutoTArray<nsString, 1>{
              NS_ConvertUTF8toUTF16(aCookieData.name()),
          });
      return newCookie;
    }
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_CHIPS_CATEGORY,
        "CookieForeignNoPartitionedWarning"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
  }

  aSetCookie = true;
  return newCookie;
}

}  // namespace net
}  // namespace mozilla
