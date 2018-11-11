/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/mips64/BaselineCompiler-mips64.h"

using namespace js;
using namespace js::jit;

BaselineCompilerMIPS64::BaselineCompilerMIPS64(JSContext* cx, TempAllocator& alloc,
                                               JSScript* script)
  : BaselineCompilerMIPSShared(cx, alloc, script)
{
}
