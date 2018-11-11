/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Linker_h
#define jit_Linker_h

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsgc.h"

#include "jit/ExecutableAllocator.h"
#include "jit/IonCode.h"
#include "jit/JitCompartment.h"
#include "jit/MacroAssembler.h"

namespace js {
namespace jit {

class Linker
{
    MacroAssembler& masm;
    mozilla::Maybe<AutoWritableJitCode> awjc;

    JitCode* fail(JSContext* cx) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

  public:
    explicit Linker(MacroAssembler& masm)
      : masm(masm)
    {
        masm.finish();
    }

    template <AllowGC allowGC>
    JitCode* newCode(JSContext* cx, CodeKind kind, bool hasPatchableBackedges = false);
};

} // namespace jit
} // namespace js

#endif /* jit_Linker_h */
