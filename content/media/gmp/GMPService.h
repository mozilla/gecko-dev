/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPService_h_
#define GMPService_h_

#include "mozIGeckoMediaPluginService.h"
#include "GMPParent.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "nsIFile.h"

namespace mozilla {
namespace gmp {

class GeckoMediaPluginService : public mozIGeckoMediaPluginService,
                                public nsIObserver
{
public:
  GeckoMediaPluginService();

  NS_DECL_ISUPPORTS
  NS_DECL_MOZIGECKOMEDIAPLUGINSERVICE
  NS_DECL_NSIOBSERVER

private:
  virtual ~GeckoMediaPluginService();

  GMPParent* SelectPluginFromListForAPI(const nsCString& api, const nsCString& tag);
  GMPParent* SelectPluginForAPI(const nsCString& api, const nsCString& tag);
  void UnloadPlugins();

  void RefreshPluginList();
  void ProcessPossiblePlugin(nsIFile* aDir);
  nsresult SearchDirectory(nsIFile* aSearchDir);
  nsresult GetPossiblePlugins(nsTArray<nsCOMPtr<nsIFile>> &aDirs);
  nsresult GetDirectoriesToSearch(nsTArray<nsCOMPtr<nsIFile>> &aDirs);

  nsTArray<nsRefPtr<GMPParent>> mPlugins;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPService_h_
