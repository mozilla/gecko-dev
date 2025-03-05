/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_nsInstDirLayout_h__
#define mozilla_nsInstDirLayout_h__

#include "nsIInstallationDirLayout.h"

namespace mozilla {

nsresult InitializeInstallationDirLayout();

class InstallationDirLayout final : public nsIInstallationDirLayout {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINSTALLATIONDIRLAYOUT
 public:
  InstallationDirLayout() = default;

 private:
  ~InstallationDirLayout() = default;
};

}  // namespace mozilla

#endif
