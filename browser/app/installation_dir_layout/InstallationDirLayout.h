/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#ifndef INSTALL_DIR_LAYOUT_H
#define INSTALL_DIR_LAYOUT_H

enum class InstallationDirLayoutType { Single, Versioned };

InstallationDirLayoutType GetInstallationDirLayoutType();

#endif  // INSTALL_DIR_LAYOUT_H
