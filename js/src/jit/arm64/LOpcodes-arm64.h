/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm64_LOpcodes_arm64_h
#define jit_arm64_LOpcodes_arm64_h

#include "jit/shared/LOpcodes-shared.h"

#define LIR_CPU_OPCODE_LIST(_)  \
    _(SoftDivI)                 \
    _(SoftModI)                 \
    _(ModMaskI)                 \
    _(UDiv)                     \
    _(UMod)                     \
    _(SoftUDivOrMod)

#endif /* jit_arm64_LOpcodes_arm64_h */
