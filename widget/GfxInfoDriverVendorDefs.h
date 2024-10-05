/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: No include guard.  This is meant to be included to generate different
// code based on how GFXINFO_FEATURE_STATUS is defined, possibly multiple times
// in a single translation unit.

/* clang-format off */

// There is an assumption that this is the first enum
GFXINFO_DRIVER_VENDOR(All, "")
// Wildcard for all Mesa drivers.
GFXINFO_DRIVER_VENDOR(MesaAll, "mesa/all")
// Note that the following list of Mesa drivers is not comprehensive; we pull
// the DRI driver at runtime. These drivers are provided for convenience when
// populating the local blocklist.
GFXINFO_DRIVER_VENDOR(MesaLLVMPipe, "mesa/llvmpipe")
GFXINFO_DRIVER_VENDOR(MesaSoftPipe, "mesa/softpipe")
GFXINFO_DRIVER_VENDOR(MesaSWRast, "mesa/swrast")
GFXINFO_DRIVER_VENDOR(MesaSWUnknown, "mesa/software-unknown")
// AMD
GFXINFO_DRIVER_VENDOR(MesaR600, "mesa/r600")
GFXINFO_DRIVER_VENDOR(MesaRadeonsi, "mesa/radeonsi")
// Nouveau: Open-source nvidia
GFXINFO_DRIVER_VENDOR(MesaNouveau, "mesa/nouveau")
// A generic ID to be provided when we can't determine the DRI driver on Mesa.
GFXINFO_DRIVER_VENDOR(MesaUnknown, "mesa/unknown")
// Wildcard for all non-Mesa drivers.
GFXINFO_DRIVER_VENDOR(NonMesaAll, "non-mesa/all")
// Wildcard for all hardware Mesa drivers.
GFXINFO_DRIVER_VENDOR(HardwareMesaAll, "mesa/hw-all")
// Wildcard for all software Mesa drivers.
GFXINFO_DRIVER_VENDOR(SoftwareMesaAll, "mesa/sw-all")
// Wildcard for all non-Intel/NVIDIA/ATI Mesa drivers.
GFXINFO_DRIVER_VENDOR(MesaNonIntelNvidiaAtiAll, "mesa/non-intel-nvidia-ati-all")
// Running in VM.
GFXINFO_DRIVER_VENDOR(MesaVM, "mesa/vmwgfx")
