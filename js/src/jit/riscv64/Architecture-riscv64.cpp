/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/riscv64/Architecture-riscv64.h"

#include "jit/FlushICache.h"  // js::jit::FlushICache
#include "jit/RegisterSets.h"
namespace js {
namespace jit {
Registers::Code Registers::FromName(const char* name) {
  for (size_t i = 0; i < Total; i++) {
    if (strcmp(GetName(i), name) == 0) {
      return Code(i);
    }
  }

  return Invalid;
}

FloatRegisters::Code FloatRegisters::FromName(const char* name) {
  for (size_t i = 0; i < Total; i++) {
    if (strcmp(GetName(i), name) == 0) {
      return Code(i);
    }
  }

  return Invalid;
}

void FlushICache(void* code, size_t size) { MOZ_CRASH(); }

bool CPUFlagsHaveBeenComputed() {
  // TODO Add CPU flags support
  // Flags were computed above.
  return true;
}

}
}