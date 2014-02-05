/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEscape.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsUrlClassifierUtils.h"
#include "nsTArray.h"
#include "nsReadableUtils.h"
#include "plbase64.h"
#include "prprf.h"

static char int_to_hex_digit(int32_t i)
{
  NS_ASSERTION((i >= 0) && (i <= 15), "int too big in int_to_hex_digit");
  return static_cast<char>(((i < 10) ? (i + '0') : ((i - 10) + 'A')));
}

static bool
IsDecimal(const nsACString & num)
{
  for (uint32_t i = 0; i < num.Length(); i++) {
    if (!isdigit(num[i])) {
      return false;
    }
  }

  return true;
}

static bool
IsHex(const nsACString & num)
{
  if (num.Length() < 3) {
    return false;
  }

  if (num[0] != '0' || !(num[1] == 'x' || num[1] == 'X')) {
    return false;
  }

  for (uint32_t i = 2; i < num.Length(); i++) {
    if (!isxdigit(num[i])) {
      return false;
    }
  }

  return true;
}

static bool
IsOctal(const nsACString & num)
{
  if (num.Length() < 2) {
    return false;
  }

  if (num[0] != '0') {
    return false;
  }

  for (uint32_t i = 1; i < num.Length(); i++) {
    if (!isdigit(num[i]) || num[i] == '8' || num[i] == '9') {
      return false;
    }
  }

  return true;
}

nsUrlClassifierUtils::nsUrlClassifierUtils() : mEscapeCharmap(nullptr)
{
}

nsresult
nsUrlClassifierUtils::Init()
{
  // Everything but alpha numerics, - and .
  mEscapeCharmap = new Charmap(0xffffffff, 0xfc009fff, 0xf8000001, 0xf8000001,
                               0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
  if (!mEscapeCharmap)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsUrlClassifierUtils, nsIUrlClassifierUtils)

/////////////////////////////////////////////////////////////////////////////
// nsIUrlClassifierUtils

NS_IMETHODIMP
nsUrlClassifierUtils::GetKeyForURI(nsIURI * uri, nsACString & _retval)
{
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
  if (!innerURI)
    innerURI = uri;

  nsAutoCString host;
  innerURI->GetAsciiHost(host);

  if (host.IsEmpty()) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsresult rv = CanonicalizeHostname(host, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString path;
  rv = innerURI->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  // strip out anchors
  int32_t ref = path.FindChar('#');
  if (ref != kNotFound)
    path.SetLength(ref);

  nsAutoCString temp;
  rv = CanonicalizePath(path, temp);
  NS_ENSURE_SUCCESS(rv, rv);

  _retval.Append(temp);

  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// non-interface methods

nsresult
nsUrlClassifierUtils::CanonicalizeHostname(const nsACString & hostname,
                                           nsACString & _retval)
{
  nsAutoCString unescaped;
  if (!NS_UnescapeURL(PromiseFlatCString(hostname).get(),
                      PromiseFlatCString(hostname).Length(),
                      0, unescaped)) {
    unescaped.Assign(hostname);
  }

  nsAutoCString cleaned;
  CleanupHostname(unescaped, cleaned);

  nsAutoCString temp;
  ParseIPAddress(cleaned, temp);
  if (!temp.IsEmpty()) {
    cleaned.Assign(temp);
  }

  ToLowerCase(cleaned);
  SpecialEncode(cleaned, false, _retval);

  return NS_OK;
}


nsresult
nsUrlClassifierUtils::CanonicalizePath(const nsACString & path,
                                       nsACString & _retval)
{
  _retval.Truncate();

  nsAutoCString decodedPath(path);
  nsAutoCString temp;
  while (NS_UnescapeURL(decodedPath.get(), decodedPath.Length(), 0, temp)) {
    decodedPath.Assign(temp);
    temp.Truncate();
  }

  SpecialEncode(decodedPath, true, _retval);
  // XXX: lowercase the path?

  return NS_OK;
}

void
nsUrlClassifierUtils::CleanupHostname(const nsACString & hostname,
                                      nsACString & _retval)
{
  _retval.Truncate();

  const char* curChar = hostname.BeginReading();
  const char* end = hostname.EndReading();
  char lastChar = '\0';
  while (curChar != end) {
    unsigned char c = static_cast<unsigned char>(*curChar);
    if (c == '.' && (lastChar == '\0' || lastChar == '.')) {
      // skip
    } else {
      _retval.Append(*curChar);
    }
    lastChar = c;
    ++curChar;
  }

  // cut off trailing dots
  while (_retval.Length() > 0 && _retval[_retval.Length() - 1] == '.') {
    _retval.SetLength(_retval.Length() - 1);
  }
}

void
nsUrlClassifierUtils::ParseIPAddress(const nsACString & host,
                                     nsACString & _retval)
{
  _retval.Truncate();
  nsACString::const_iterator iter, end;
  host.BeginReading(iter);
  host.EndReading(end);

  if (host.Length() <= 15) {
    // The Windows resolver allows a 4-part dotted decimal IP address to
    // have a space followed by any old rubbish, so long as the total length
    // of the string doesn't get above 15 characters. So, "10.192.95.89 xy"
    // is resolved to 10.192.95.89.
    // If the string length is greater than 15 characters, e.g.
    // "10.192.95.89 xy.wildcard.example.com", it will be resolved through
    // DNS.

    if (FindCharInReadable(' ', iter, end)) {
      end = iter;
    }
  }

  for (host.BeginReading(iter); iter != end; iter++) {
    if (!(isxdigit(*iter) || *iter == 'x' || *iter == 'X' || *iter == '.')) {
      // not an IP
      return;
    }
  }

  host.BeginReading(iter);
  nsTArray<nsCString> parts;
  ParseString(PromiseFlatCString(Substring(iter, end)), '.', parts);
  if (parts.Length() > 4) {
    return;
  }

  // If any potentially-octal numbers (start with 0 but not hex) have
  // non-octal digits, no part of the ip can be in octal
  // XXX: this came from the old javascript implementation, is it really
  // supposed to be like this?
  bool allowOctal = true;
  uint32_t i;

  for (i = 0; i < parts.Length(); i++) {
    const nsCString& part = parts[i];
    if (part[0] == '0') {
      for (uint32_t j = 1; j < part.Length(); j++) {
        if (part[j] == 'x') {
          break;
        }
        if (part[j] == '8' || part[j] == '9') {
          allowOctal = false;
          break;
        }
      }
    }
  }

  for (i = 0; i < parts.Length(); i++) {
    nsAutoCString canonical;

    if (i == parts.Length() - 1) {
      CanonicalNum(parts[i], 5 - parts.Length(), allowOctal, canonical);
    } else {
      CanonicalNum(parts[i], 1, allowOctal, canonical);
    }

    if (canonical.IsEmpty()) {
      _retval.Truncate();
      return;
    }

    if (_retval.IsEmpty()) {
      _retval.Assign(canonical);
    } else {
      _retval.Append('.');
      _retval.Append(canonical);
    }
  }
  return;
}

void
nsUrlClassifierUtils::CanonicalNum(const nsACString& num,
                                   uint32_t bytes,
                                   bool allowOctal,
                                   nsACString& _retval)
{
  _retval.Truncate();

  if (num.Length() < 1) {
    return;
  }

  uint32_t val;
  if (allowOctal && IsOctal(num)) {
    if (PR_sscanf(PromiseFlatCString(num).get(), "%o", &val) != 1) {
      return;
    }
  } else if (IsDecimal(num)) {
    if (PR_sscanf(PromiseFlatCString(num).get(), "%u", &val) != 1) {
      return;
    }
  } else if (IsHex(num)) {
  if (PR_sscanf(PromiseFlatCString(num).get(), num[1] == 'X' ? "0X%x" : "0x%x",
                &val) != 1) {
      return;
    }
  } else {
    return;
  }

  while (bytes--) {
    char buf[20];
    PR_snprintf(buf, sizeof(buf), "%u", val & 0xff);
    if (_retval.IsEmpty()) {
      _retval.Assign(buf);
    } else {
      _retval = nsDependentCString(buf) + NS_LITERAL_CSTRING(".") + _retval;
    }
    val >>= 8;
  }
}

// This function will encode all "special" characters in typical url
// encoding, that is %hh where h is a valid hex digit.  It will also fold
// any duplicated slashes.
bool
nsUrlClassifierUtils::SpecialEncode(const nsACString & url,
                                    bool foldSlashes,
                                    nsACString & _retval)
{
  bool changed = false;
  const char* curChar = url.BeginReading();
  const char* end = url.EndReading();

  unsigned char lastChar = '\0';
  while (curChar != end) {
    unsigned char c = static_cast<unsigned char>(*curChar);
    if (ShouldURLEscape(c)) {
      _retval.Append('%');
      _retval.Append(int_to_hex_digit(c / 16));
      _retval.Append(int_to_hex_digit(c % 16));

      changed = true;
    } else if (foldSlashes && (c == '/' && lastChar == '/')) {
      // skip
    } else {
      _retval.Append(*curChar);
    }
    lastChar = c;
    curChar++;
  }
  return changed;
}

bool
nsUrlClassifierUtils::ShouldURLEscape(const unsigned char c) const
{
  return c <= 32 || c == '%' || c >=127;
}
