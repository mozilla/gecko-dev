/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsResProtocolHandler_h___
#define nsResProtocolHandler_h___

#include "SubstitutingProtocolHandler.h"

#include "nsIResProtocolHandler.h"
#include "nsInterfaceHashtable.h"
#include "nsWeakReference.h"
#include "nsStandardURL.h"

class nsISubstitutionObserver;

struct SubstitutionMapping;
class nsResProtocolHandler final
    : public nsIResProtocolHandler,
      public mozilla::net::SubstitutingProtocolHandler,
      public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRESPROTOCOLHANDLER

  NS_FORWARD_NSIPROTOCOLHANDLER(mozilla::net::SubstitutingProtocolHandler::)

  nsResProtocolHandler()
      : mozilla::net::SubstitutingProtocolHandler(
            "resource",
            URI_STD | URI_IS_UI_RESOURCE | URI_IS_LOCAL_RESOURCE |
                URI_IS_POTENTIALLY_TRUSTWORTHY,
            /* aEnforceFileOrJar = */ false) {}

  MOZ_MUST_USE nsresult Init();

  NS_IMETHOD SetSubstitution(const nsACString& aRoot,
                             nsIURI* aBaseURI) override;
  NS_IMETHOD SetSubstitutionWithFlags(const nsACString& aRoot, nsIURI* aBaseURI,
                                      uint32_t aFlags) override;

  NS_IMETHOD GetSubstitution(const nsACString& aRoot,
                             nsIURI** aResult) override {
    return mozilla::net::SubstitutingProtocolHandler::GetSubstitution(aRoot,
                                                                      aResult);
  }

  NS_IMETHOD HasSubstitution(const nsACString& aRoot, bool* aResult) override {
    return mozilla::net::SubstitutingProtocolHandler::HasSubstitution(aRoot,
                                                                      aResult);
  }

  NS_IMETHOD ResolveURI(nsIURI* aResURI, nsACString& aResult) override {
    return mozilla::net::SubstitutingProtocolHandler::ResolveURI(aResURI,
                                                                 aResult);
  }

  NS_IMETHOD AddObserver(nsISubstitutionObserver* aObserver) override {
    return mozilla::net::SubstitutingProtocolHandler::AddObserver(aObserver);
  }

  NS_IMETHOD RemoveObserver(nsISubstitutionObserver* aObserver) override {
    return mozilla::net::SubstitutingProtocolHandler::RemoveObserver(aObserver);
  }

 protected:
  MOZ_MUST_USE nsresult GetSubstitutionInternal(const nsACString& aRoot,
                                                nsIURI** aResult,
                                                uint32_t* aFlags) override;
  virtual ~nsResProtocolHandler() = default;

  MOZ_MUST_USE bool ResolveSpecialCases(const nsACString& aHost,
                                        const nsACString& aPath,
                                        const nsACString& aPathname,
                                        nsACString& aResult) override;

 private:
  nsCString mAppURI;
  nsCString mGREURI;
};

#endif /* nsResProtocolHandler_h___ */
