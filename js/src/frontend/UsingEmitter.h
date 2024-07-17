/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_UsingEmitter_h
#define frontend_UsingEmitter_h

#include "mozilla/Attributes.h"

#include "frontend/BytecodeOffset.h"
#include "vm/UsingHint.h"

namespace js::frontend {

struct BytecodeEmitter;
class EmitterScope;

class MOZ_STACK_CLASS UsingEmitter {
 private:
  BytecodeEmitter* bce_;

  int depthAtDisposables_ = -1;

  BytecodeOffset disposableStart_ = BytecodeOffset::invalidOffset();

  // TODO: add state transition graph and state
  // management for this emitter. (Bug 1904346)

 public:
  explicit UsingEmitter(BytecodeEmitter* bce);

  [[nodiscard]] bool prepareForDisposableScopeBody();

  [[nodiscard]] bool prepareForAssignment(UsingHint hint);

  [[nodiscard]] bool prepareForForOfLoopIteration();

  [[nodiscard]] bool prepareForForOfIteratorCloseOnThrow();

  [[nodiscard]] bool emitNonLocalJump(EmitterScope* present);

  [[nodiscard]] bool emitEnd();
};

}  // namespace js::frontend

#endif  // frontend_UsingEmitter_h
