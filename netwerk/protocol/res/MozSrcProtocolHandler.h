/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MozSrcProtocolHandler_h___
#define MozSrcProtocolHandler_h___

#include "nsIProtocolHandler.h"
#include "nsISubstitutingProtocolHandler.h"
#include "SubstitutingProtocolHandler.h"

namespace mozilla {
namespace net {

class MozSrcProtocolHandler final : public nsISubstitutingProtocolHandler,
                                    public SubstitutingProtocolHandler,
                                    public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIPROTOCOLHANDLER(SubstitutingProtocolHandler::)
  NS_FORWARD_NSISUBSTITUTINGPROTOCOLHANDLER(SubstitutingProtocolHandler::)

  static already_AddRefed<MozSrcProtocolHandler> GetSingleton();

  MozSrcProtocolHandler();

 protected:
  ~MozSrcProtocolHandler() = default;

  [[nodiscard]] virtual bool ResolveSpecialCases(const nsACString& aHost,
                                                 const nsACString& aPath,
                                                 const nsACString& aPathname,
                                                 nsACString& aResult) override;

  [[nodiscard]] nsresult GetSubstitutionInternal(const nsACString& aRoot,
                                                 nsIURI** aResult) override;

 private:
  static mozilla::StaticRefPtr<MozSrcProtocolHandler> sSingleton;
  nsresult Init();

  nsCString mGREURI;
};

}  // namespace net
}  // namespace mozilla

#endif /* MozSrcProtocolHandler_h___ */
