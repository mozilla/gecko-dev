/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsCloudStorageGeckoInterface.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCloudStorageGeckoInterface, Init)

NS_DEFINE_NAMED_CID(NS_CLOUDSTORAGEGECKOINTERFACE_CID);

// Build a table of ClassIDs (CIDs) which are implemented by this module.
static const mozilla::Module::CIDEntry kCloudStorageGeckoInterfaceCIDs[] = {
  {    
  &kNS_CLOUDSTORAGEGECKOINTERFACE_CID,         // CID
  false,                                       // service
  nullptr,                                     // factoryproc, usually nullptr.
  nsCloudStorageGeckoInterfaceConstructor      // constructorproc, defined by NS_GENERIC_FACTORY_CONSTRUCTOR
  }, { nullptr }
};

// Build a table which maps contract IDs to CIDs.
static const mozilla::Module::ContractIDEntry kCloudStorageGeckoInterfaceContracts[] = {
  { 
  NS_CLOUDSTORAGEGECKOINTERFACE_CONTRACT_ID, &kNS_CLOUDSTORAGEGECKOINTERFACE_CID },
  { nullptr }
};

static const mozilla::Module kCloudStorageGeckoInterfaceModule = {
  mozilla::Module::kVersion,
  kCloudStorageGeckoInterfaceCIDs,
  kCloudStorageGeckoInterfaceContracts
};

// export “NSModule”
NSMODULE_DEFN(CloudStorageGeckoInterfaceModule) = &kCloudStorageGeckoInterfaceModule;

