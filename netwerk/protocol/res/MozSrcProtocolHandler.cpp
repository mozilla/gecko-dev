/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Omnijar.h"

#include "MozSrcProtocolHandler.h"

#define MOZSRC_SCHEME "moz-src"

namespace mozilla {
namespace net {

NS_IMPL_QUERY_INTERFACE(MozSrcProtocolHandler, nsISubstitutingProtocolHandler,
                        nsIProtocolHandler, nsISupportsWeakReference)
NS_IMPL_ADDREF_INHERITED(MozSrcProtocolHandler, SubstitutingProtocolHandler)
NS_IMPL_RELEASE_INHERITED(MozSrcProtocolHandler, SubstitutingProtocolHandler)

mozilla::StaticRefPtr<MozSrcProtocolHandler> MozSrcProtocolHandler::sSingleton;

already_AddRefed<MozSrcProtocolHandler> MozSrcProtocolHandler::GetSingleton() {
  if (!sSingleton) {
    RefPtr<MozSrcProtocolHandler> handler = new MozSrcProtocolHandler();
    if (NS_WARN_IF(NS_FAILED(handler->Init()))) {
      return nullptr;
    }
    sSingleton = handler;
    ClearOnShutdown(&sSingleton);
  }
  return do_AddRef(sSingleton);
}

MozSrcProtocolHandler::MozSrcProtocolHandler()
    : SubstitutingProtocolHandler(MOZSRC_SCHEME) {}

nsresult MozSrcProtocolHandler::Init() {
  nsresult rv = mozilla::Omnijar::GetURIString(mozilla::Omnijar::GRE, mGREURI);
  NS_ENSURE_SUCCESS(rv, rv);

  mGREURI.AppendLiteral(MOZSRC_SCHEME);

  return NS_OK;
}

bool MozSrcProtocolHandler::ResolveSpecialCases(const nsACString& aHost,
                                                const nsACString& aPath,
                                                const nsACString& aPathname,
                                                nsACString& aResult) {
  aResult = mGREURI;
  aResult.Append(aPathname);
  return true;
}

nsresult MozSrcProtocolHandler::GetSubstitutionInternal(const nsACString& aRoot,
                                                        nsIURI** aResult) {
  nsAutoCString uri;

  if (!ResolveSpecialCases(aRoot, "/"_ns, "/"_ns, uri)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_NewURI(aResult, uri);
}

}  // namespace net
}  // namespace mozilla
