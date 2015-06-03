/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_cloudstoragegeckointerface_h_
#define mozilla_system_cloudstoragegeckointerface_h_

#include "nsICloudStorageGeckoInterface.h"

#define NS_CLOUDSTORAGEGECKOINTERFACE_CID \
  {0x08569134, 0x0955, 0x11E5, {0x9B, 0xC4, 0xAE, 0x0F, 0x1D, 0x5D, 0x46, 0xB0}}

#define NS_CLOUDSTORAGEGECKOINTERFACE_CONTRACT_ID "@mozilla.org/cloudstoragegeckointerface;1"

class nsCloudStorageGeckoInterface : public nsICloudStorageGeckoInterface
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLOUDSTORAGEGECKOINTERFACE
  
  virtual nsresult Init();
  nsCloudStorageGeckoInterface();

private:
  virtual ~nsCloudStorageGeckoInterface();
};

#endif
