/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_DisposeJumpKind_h
#define vm_DisposeJumpKind_h

#include <stdint.h>  // uint8_t

namespace js {

enum class DisposeJumpKind : uint8_t {
  /*
   * Jump out of interpreter Loop to error handling code
   * if there was an exception during the Dispose Operation.
   */
  JumpOnError,
  /*
   * Do not jump out of the interpreter loop to error handling
   * even if there are errors pending.
   */
  NoJumpOnError,
};

}  // namespace js

#endif /* vm_DisposeJumpKind_h */
