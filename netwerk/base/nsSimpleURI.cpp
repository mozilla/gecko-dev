/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#undef LOG
#include "ipc/IPCMessageUtils.h"

#include "nsSimpleURI.h"
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsURLHelper.h"
#include "nsNetCID.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsEscape.h"
#include "nsError.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TextUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIURIMutator.h"
#include "mozilla/net/MozURL.h"
#include "mozilla/StaticPrefs_network.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);

/* static */
already_AddRefed<nsSimpleURI> nsSimpleURI::From(nsIURI* aURI) {
  RefPtr<nsSimpleURI> uri;
  nsresult rv = aURI->QueryInterface(kThisSimpleURIImplementationCID,
                                     getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return uri.forget();
}

NS_IMPL_CLASSINFO(nsSimpleURI, nullptr, nsIClassInfo::THREADSAFE,
                  NS_SIMPLEURI_CID)
// Empty CI getter. We only need nsIClassInfo for Serialization
NS_IMPL_CI_INTERFACE_GETTER0(nsSimpleURI)

////////////////////////////////////////////////////////////////////////////////
// nsSimpleURI methods:

NS_IMPL_ADDREF(nsSimpleURI)
NS_IMPL_RELEASE(nsSimpleURI)
NS_INTERFACE_TABLE_HEAD(nsSimpleURI)
  NS_INTERFACE_TABLE(nsSimpleURI, nsIURI, nsISerializable)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_IMPL_QUERY_CLASSINFO(nsSimpleURI)
  if (aIID.Equals(kThisSimpleURIImplementationCID)) {
    foundInterface = static_cast<nsIURI*>(this);
  } else
    NS_INTERFACE_MAP_ENTRY(nsISizeOf)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
// nsISerializable methods:

NS_IMETHODIMP
nsSimpleURI::Read(nsIObjectInputStream* aStream) {
  MOZ_ASSERT_UNREACHABLE("Use nsIURIMutator.read() instead");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsSimpleURI::ReadPrivate(nsIObjectInputStream* aStream) {
  nsresult rv;

  bool isMutable;
  rv = aStream->ReadBoolean(&isMutable);
  if (NS_FAILED(rv)) return rv;
  Unused << isMutable;

  nsAutoCString scheme;
  rv = aStream->ReadCString(scheme);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString path;
  rv = aStream->ReadCString(path);
  if (NS_FAILED(rv)) return rv;

  bool isRefValid;
  rv = aStream->ReadBoolean(&isRefValid);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString ref;
  if (isRefValid) {
    rv = aStream->ReadCString(ref);
    if (NS_FAILED(rv)) return rv;
  }

  bool isQueryValid;
  rv = aStream->ReadBoolean(&isQueryValid);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString query;
  if (isQueryValid) {
    rv = aStream->ReadCString(query);
    if (NS_FAILED(rv)) return rv;
  }

  // Re-constitute the spec, and initialize from it.
  nsAutoCString spec = scheme + ":"_ns + path;
  if (isQueryValid) {
    spec += "?"_ns + query;
  }
  if (isRefValid) {
    spec += "#"_ns + ref;
  }
  return SetSpecInternal(spec);
}

NS_IMETHODIMP
nsSimpleURI::Write(nsIObjectOutputStream* aStream) {
  nsresult rv;

  rv = aStream->WriteBoolean(false);  // former mMutable
  if (NS_FAILED(rv)) return rv;

  rv = aStream->WriteCString(Scheme());
  if (NS_FAILED(rv)) return rv;

  rv = aStream->WriteCString(Path());
  if (NS_FAILED(rv)) return rv;

  rv = aStream->WriteBoolean(IsRefValid());
  if (NS_FAILED(rv)) return rv;

  if (IsRefValid()) {
    rv = aStream->WriteCString(Ref());
    if (NS_FAILED(rv)) return rv;
  }

  rv = aStream->WriteBoolean(IsQueryValid());
  if (NS_FAILED(rv)) return rv;

  if (IsQueryValid()) {
    rv = aStream->WriteCString(Query());
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

void nsSimpleURI::Serialize(URIParams& aParams) {
  SimpleURIParams params;

  params.spec() = mSpec;

  aParams = params;
}

bool nsSimpleURI::Deserialize(const URIParams& aParams) {
  if (aParams.type() != URIParams::TSimpleURIParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return false;
  }

  const SimpleURIParams& params = aParams.get_SimpleURIParams();

  nsresult rv = SetSpecInternal(params.spec());
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to set spec from other process");
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// nsIURI methods:

NS_IMETHODIMP
nsSimpleURI::GetSpec(nsACString& result) {
  result = mSpec;
  return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsSimpleURI::GetSpecIgnoringRef(nsACString& result) {
  if (!IsRefValid()) {
    // Optimization: If there is no ref which needs to be trimmed, call
    // `GetSpec` to allow result to share the `mSpec` string buffer.
    return GetSpec(result);
  }

  result = SpecIgnoringRef();
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetDisplaySpec(nsACString& aUnicodeSpec) {
  return GetSpec(aUnicodeSpec);
}

NS_IMETHODIMP
nsSimpleURI::GetDisplayHostPort(nsACString& aUnicodeHostPort) {
  return GetHostPort(aUnicodeHostPort);
}

NS_IMETHODIMP
nsSimpleURI::GetDisplayHost(nsACString& aUnicodeHost) {
  return GetHost(aUnicodeHost);
}

NS_IMETHODIMP
nsSimpleURI::GetDisplayPrePath(nsACString& aPrePath) {
  return GetPrePath(aPrePath);
}

NS_IMETHODIMP
nsSimpleURI::GetHasRef(bool* result) {
  *result = IsRefValid();
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetHasUserPass(bool* result) {
  *result = false;
  return NS_OK;
}

nsresult nsSimpleURI::SetSpecInternal(const nsACString& aSpec,
                                      bool aStripWhitespace) {
  if (StaticPrefs::network_url_max_length() &&
      aSpec.Length() > StaticPrefs::network_url_max_length()) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsAutoCString scheme;
  nsresult rv = net_ExtractURLScheme(aSpec, scheme);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString spec;
  rv = net_FilterAndEscapeURI(
      aSpec, esc_OnlyNonASCII,
      aStripWhitespace ? ASCIIMask::MaskWhitespace() : ASCIIMask::MaskCRLFTab(),
      spec);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Copy the filtered string into `mSpec`. We'll try not to mutate this buffer
  // unless it's required so we can share the (potentially very large data: URI)
  // string buffer.
  mSpec = std::move(spec);
  mPathSep = mSpec.FindChar(':');
  MOZ_ASSERT(mPathSep != kNotFound, "A colon should be in this string");
  mQuerySep = kNotFound;
  mRefSep = kNotFound;

  // Check if `net_ExtractURLScheme` changed the scheme as written, and update
  // `mSpec` if it did.
  if (Scheme() != scheme) {
    if (!mSpec.Replace(SchemeStart(), SchemeLen(), scheme, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mPathSep = scheme.Length();
    MOZ_ASSERT(mSpec.CharAt(mPathSep) == ':');
  }

  // Populate the remaining members.
  return SetPathQueryRefInternal();
}

NS_IMETHODIMP
nsSimpleURI::GetScheme(nsACString& result) {
  result = Scheme();
  return NS_OK;
}

nsresult nsSimpleURI::SetScheme(const nsACString& input) {
  // Strip tabs, newlines, carriage returns from input
  nsAutoCString scheme(input);
  scheme.StripTaggedASCII(ASCIIMask::MaskCRLFTab());
  ToLowerCase(scheme);

  if (!net_IsValidScheme(scheme)) {
    NS_WARNING("the given url scheme contains invalid characters");
    return NS_ERROR_MALFORMED_URI;
  }

  int32_t delta = static_cast<int32_t>(scheme.Length()) - mPathSep;
  mSpec.Replace(SchemeStart(), SchemeLen(), scheme);

  // Adjust the separator indexes to account for the change in scheme length.
  mPathSep += delta;
  MOZ_ASSERT(mSpec.CharAt(mPathSep) == ':');
  if (IsQueryValid()) {
    mQuerySep += delta;
    MOZ_ASSERT(mSpec.CharAt(mQuerySep) == '?');
  }
  if (IsRefValid()) {
    mRefSep += delta;
    MOZ_ASSERT(mSpec.CharAt(mRefSep) == '#');
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetPrePath(nsACString& result) {
  result = Substring(mSpec, 0, PathStart());
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetUserPass(nsACString& result) { return NS_ERROR_FAILURE; }

nsresult nsSimpleURI::SetUserPass(const nsACString& userPass) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetUsername(nsACString& result) { return NS_ERROR_FAILURE; }

nsresult nsSimpleURI::SetUsername(const nsACString& userName) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPassword(nsACString& result) { return NS_ERROR_FAILURE; }

nsresult nsSimpleURI::SetPassword(const nsACString& password) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetHostPort(nsACString& result) {
  // Note: Audit all callers before changing this to return an empty
  // string -- CAPS and UI code may depend on this throwing.
  // Note: If this is changed, change GetAsciiHostPort as well.
  return NS_ERROR_FAILURE;
}

nsresult nsSimpleURI::SetHostPort(const nsACString& aValue) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetHost(nsACString& result) {
  // Note: Audit all callers before changing this to return an empty
  // string -- CAPS and UI code depend on this throwing.
  return NS_ERROR_FAILURE;
}

nsresult nsSimpleURI::SetHost(const nsACString& host) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetPort(int32_t* result) {
  // Note: Audit all callers before changing this to return an empty
  // string -- CAPS and UI code may depend on this throwing.
  return NS_ERROR_FAILURE;
}

nsresult nsSimpleURI::SetPort(int32_t port) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
nsSimpleURI::GetPathQueryRef(nsACString& result) {
  result = Substring(mSpec, PathStart());
  return NS_OK;
}

nsresult nsSimpleURI::SetPathQueryRef(const nsACString& aPath) {
  if (StaticPrefs::network_url_max_length()) {
    CheckedInt<uint32_t> newLength(mSpec.Length());
    newLength -= PathLen();
    newLength += aPath.Length();
    if (!newLength.isValid() ||
        newLength.value() > StaticPrefs::network_url_max_length()) {
      return NS_ERROR_MALFORMED_URI;
    }
  }

  nsAutoCString path;
  nsresult rv = NS_EscapeURL(aPath, esc_OnlyNonASCII, path, fallible);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Clear out the components being replaced. They'll be re-initialized below.
  mQuerySep = kNotFound;
  mRefSep = kNotFound;

  mSpec.Truncate(PathStart());
  if (!mSpec.Append(path, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return SetPathQueryRefInternal();
}

nsresult nsSimpleURI::SetPathQueryRefInternal() {
  MOZ_ASSERT(mSpec.CharAt(mPathSep) == ':');
  MOZ_ASSERT(mQuerySep == kNotFound);
  MOZ_ASSERT(mRefSep == kNotFound);

  // Initialize `mQuerySep` and `mRefSep` if those components are present.
  int32_t pathEnd = mSpec.FindCharInSet("?#", PathStart());
  if (pathEnd != kNotFound) {
    if (mSpec.CharAt(pathEnd) == '?') {
      mQuerySep = pathEnd;
    } else {
      mRefSep = pathEnd;
    }
  }

  if (IsQueryValid()) {
    mRefSep = mSpec.FindChar('#', QueryStart());
  }

  // Unlike the path or query, `mRef` also requires spaces to be escaped.
  if (IsRefValid()) {
    // NOTE: `NS_EscapeURLSpan` could theoretically OOM-fail, but there is no
    // fallible version of `NS_EscapeURL` which won't do an unnecessary copy in
    // the non-escaping case.
    nsAutoCString escapedRef;
    if (NS_EscapeURLSpan(Ref(), esc_OnlyNonASCII | esc_Spaces, escapedRef)) {
      if (!mSpec.Replace(RefStart(), RefLen(), escapedRef, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  if (Scheme() != "javascript"_ns && !IsQueryValid() && !IsRefValid()) {
    TrimTrailingCharactersFromPath();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetRef(nsACString& result) {
  if (!IsRefValid()) {
    result.Truncate();
  } else {
    result = Ref();
  }

  return NS_OK;
}

// NOTE: SetRef("") removes our ref, whereas SetRef("#") sets it to the empty
// string (and will result in .spec and .path having a terminal #).
nsresult nsSimpleURI::SetRef(const nsACString& aRef) {
  if (StaticPrefs::network_url_max_length() &&
      aRef.Length() > StaticPrefs::network_url_max_length()) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsAutoCString ref;
  nsresult rv =
      NS_EscapeURL(aRef, esc_OnlyNonASCII | esc_Spaces, ref, fallible);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (ref.IsEmpty() && !IsRefValid()) {
    return NS_OK;  // nothing to do
  }

  int32_t cutStart;
  int32_t cutLength;
  if (IsRefValid()) {
    cutStart = mRefSep;
    cutLength = RefEnd() - cutStart;
  } else {
    cutStart = mSpec.Length();
    cutLength = 0;
  }

  nsCString prefix;
  if (!ref.IsEmpty() && ref[0] != '#') {
    // The replace includes the `#` character, so ensure it's present in the
    // ref (unless we're removing the ref).
    prefix = "#"_ns;
  }
  if (!mSpec.Replace(cutStart, cutLength, prefix + ref, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (ref.IsEmpty()) {
    mRefSep = kNotFound;
  } else {
    mRefSep = cutStart;
    MOZ_ASSERT(mSpec.CharAt(mRefSep) == '#');
  }

  // Trim trailing invalid chars when ref and query are removed
  if (Scheme() != "javascript"_ns && !IsRefValid() && !IsQueryValid()) {
    TrimTrailingCharactersFromPath();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Equals(nsIURI* other, bool* result) {
  return EqualsInternal(other, eHonorRef, result);
}

NS_IMETHODIMP
nsSimpleURI::EqualsExceptRef(nsIURI* other, bool* result) {
  return EqualsInternal(other, eIgnoreRef, result);
}

/* virtual */
nsresult nsSimpleURI::EqualsInternal(
    nsIURI* other, nsSimpleURI::RefHandlingEnum refHandlingMode, bool* result) {
  NS_ENSURE_ARG_POINTER(other);
  MOZ_ASSERT(result, "null pointer");

  RefPtr<nsSimpleURI> otherUri;
  nsresult rv = other->QueryInterface(kThisSimpleURIImplementationCID,
                                      getter_AddRefs(otherUri));
  if (NS_FAILED(rv)) {
    *result = false;
    return NS_OK;
  }

  *result = EqualsInternal(otherUri, refHandlingMode);
  return NS_OK;
}

bool nsSimpleURI::EqualsInternal(nsSimpleURI* otherUri,
                                 RefHandlingEnum refHandlingMode) {
  if (refHandlingMode != eHonorRef) {
    return SpecIgnoringRef() == otherUri->SpecIgnoringRef();
  }

  return mSpec == otherUri->mSpec;
}

NS_IMETHODIMP
nsSimpleURI::SchemeIs(const char* i_Scheme, bool* o_Equals) {
  MOZ_ASSERT(o_Equals, "null pointer");
  if (!i_Scheme) {
    *o_Equals = false;
    return NS_OK;
  }

  *o_Equals = Scheme().EqualsIgnoreCase(i_Scheme);
  return NS_OK;
}

/* virtual */ already_AddRefed<nsSimpleURI> nsSimpleURI::StartClone() {
  RefPtr<nsSimpleURI> url = new nsSimpleURI();
  return url.forget();
}

nsresult nsSimpleURI::Clone(nsIURI** result) {
  RefPtr<nsSimpleURI> url = StartClone();
  if (!url) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  url->mSpec = mSpec;
  url->mPathSep = mPathSep;
  url->mQuerySep = mQuerySep;
  url->mRefSep = mRefSep;

  url.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::Resolve(const nsACString& relativePath, nsACString& result) {
  nsAutoCString scheme;
  nsresult rv = net_ExtractURLScheme(relativePath, scheme);
  if (NS_SUCCEEDED(rv)) {
    result = relativePath;
    return NS_OK;
  }

  nsAutoCString spec;
  rv = GetAsciiSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If getting the spec fails for some reason, preserve behaviour and just
    // return the relative path.
    result = relativePath;
    return NS_OK;
  }

  RefPtr<MozURL> url;
  rv = MozURL::Init(getter_AddRefs(url), spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If parsing the current url fails, we revert to the previous behaviour
    // and just return the relative path.
    result = relativePath;
    return NS_OK;
  }

  RefPtr<MozURL> url2;
  rv = MozURL::Init(getter_AddRefs(url2), relativePath, url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If parsing the relative url fails, we revert to the previous behaviour
    // and just return the relative path.
    result = relativePath;
    return NS_OK;
  }

  result = url2->Spec();
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiSpec(nsACString& aResult) {
  nsresult rv = GetSpec(aResult);
  if (NS_FAILED(rv)) return rv;
  MOZ_ASSERT(IsAscii(aResult), "The spec should be ASCII");
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiHostPort(nsACString& result) {
  // XXX This behavior mimics GetHostPort.
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleURI::GetAsciiHost(nsACString& result) {
  result.Truncate();
  return NS_OK;
}

//----------------------------------------------------------------------------
// nsSimpleURI::nsISizeOf
//----------------------------------------------------------------------------

size_t nsSimpleURI::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  return mSpec.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

size_t nsSimpleURI::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

NS_IMETHODIMP
nsSimpleURI::GetFilePath(nsACString& aFilePath) {
  aFilePath = Path();
  return NS_OK;
}

nsresult nsSimpleURI::SetFilePath(const nsACString& aFilePath) {
  if (Path().IsEmpty() || Path().First() != '/') {
    // cannot-be-a-base
    return NS_ERROR_MALFORMED_URI;
  }
  const char* current = aFilePath.BeginReading();
  const char* end = aFilePath.EndReading();

  // Only go up to the first ? or # symbol
  for (; current < end; ++current) {
    if (*current == '?' || *current == '#') {
      break;
    }
  }
  return SetPathQueryRef(
      nsDependentCSubstring(aFilePath.BeginReading(), current));
}

NS_IMETHODIMP
nsSimpleURI::GetQuery(nsACString& aQuery) {
  if (!IsQueryValid()) {
    aQuery.Truncate();
  } else {
    aQuery = Query();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleURI::GetHasQuery(bool* result) {
  *result = IsQueryValid();
  return NS_OK;
}

nsresult nsSimpleURI::SetQuery(const nsACString& aQuery) {
  if (StaticPrefs::network_url_max_length() &&
      aQuery.Length() > StaticPrefs::network_url_max_length()) {
    return NS_ERROR_MALFORMED_URI;
  }
  nsAutoCString query;
  nsresult rv = NS_EscapeURL(aQuery, esc_OnlyNonASCII, query, fallible);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (query.IsEmpty() && !IsQueryValid()) {
    return NS_OK;  // nothing to do
  }

  int32_t cutStart;
  int32_t cutLength;
  if (IsQueryValid()) {
    cutStart = mQuerySep;
    cutLength = QueryEnd() - cutStart;
  } else if (IsRefValid()) {
    cutStart = mRefSep;
    cutLength = 0;
  } else {
    cutStart = mSpec.Length();
    cutLength = 0;
  }

  nsCString prefix;
  if (!query.IsEmpty() && query[0] != '?') {
    // The replace includes the `?` character, so ensure it's present in the
    // query (unless we're removing the query).
    prefix = "?"_ns;
  }
  if (!mSpec.Replace(cutStart, cutLength, prefix + query, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Update `mQuerySep` and `mRefSep`.
  if (query.IsEmpty()) {
    mQuerySep = kNotFound;
  } else {
    mQuerySep = cutStart;
    MOZ_ASSERT(mSpec.CharAt(mQuerySep) == '?');
  }
  if (mRefSep != kNotFound) {
    mRefSep -= cutLength;
    mRefSep += prefix.Length() + query.Length();
    MOZ_ASSERT(mSpec.CharAt(mRefSep) == '#');
  }

  // Trim trailing invalid chars when ref and query are removed
  if (Scheme() != "javascript"_ns && !IsRefValid() && !IsQueryValid()) {
    TrimTrailingCharactersFromPath();
  }

  return NS_OK;
}

nsresult nsSimpleURI::SetQueryWithEncoding(const nsACString& aQuery,
                                           const Encoding* aEncoding) {
  return SetQuery(aQuery);
}

void nsSimpleURI::TrimTrailingCharactersFromPath() {
  MOZ_ASSERT(!IsQueryValid());
  MOZ_ASSERT(!IsRefValid());

  const auto* start = mSpec.BeginReading();
  const auto* end = mSpec.EndReading();

  auto charFilter = [](char c) { return static_cast<uint8_t>(c) > 0x20; };
  const auto* newEnd =
      std::find_if(std::reverse_iterator<decltype(end)>(end),
                   std::reverse_iterator<decltype(start)>(start), charFilter)
          .base();

  auto trailCount = std::distance(newEnd, end);
  if (trailCount) {
    mSpec.Truncate(mSpec.Length() - trailCount);
  }
}

// Queries this list of interfaces. If none match, it queries mURI.
NS_IMPL_NSIURIMUTATOR_ISUPPORTS(nsSimpleURI::Mutator, nsIURISetters,
                                nsIURIMutator, nsISerializable,
                                nsISimpleURIMutator)

NS_IMETHODIMP
nsSimpleURI::Mutate(nsIURIMutator** aMutator) {
  RefPtr<nsSimpleURI::Mutator> mutator = new nsSimpleURI::Mutator();
  nsresult rv = mutator->InitFromURI(this);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mutator.forget(aMutator);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
