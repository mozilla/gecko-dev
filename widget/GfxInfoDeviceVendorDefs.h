/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: No include guard.  This is meant to be included to generate different
// code based on how GFXINFO_FEATURE_STATUS is defined, possibly multiple times
// in a single translation unit.

/* clang-format off */

// There is an assumption that this is the first enum
GFXINFO_DEVICE_VENDOR(All, "")
GFXINFO_DEVICE_VENDOR(Intel, "0x8086")
GFXINFO_DEVICE_VENDOR(NVIDIA, "0x10de")
GFXINFO_DEVICE_VENDOR(ATI, "0x1002")
// AMD has 0x1022 but continues to release GPU hardware under ATI.
GFXINFO_DEVICE_VENDOR(Microsoft, "0x1414")
GFXINFO_DEVICE_VENDOR(MicrosoftBasic, "0x00ba")
GFXINFO_DEVICE_VENDOR(MicrosoftHyperV, "0x000b")
GFXINFO_DEVICE_VENDOR(Parallels, "0x1ab8")
GFXINFO_DEVICE_VENDOR(VMWare, "0x15ad")
GFXINFO_DEVICE_VENDOR(VirtualBox, "0x80ee")
GFXINFO_DEVICE_VENDOR(Apple, "0x106b")
GFXINFO_DEVICE_VENDOR(Amazon, "0x1d0f")
// Choose an arbitrary Qualcomm PCI VENdor ID for now.
// TODO: This should be "QCOM" when Windows device ID parsing is reworked.
GFXINFO_DEVICE_VENDOR(Qualcomm, "0x5143")
