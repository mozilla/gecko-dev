/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_UsingEmitter_h
#define frontend_UsingEmitter_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include "frontend/TryEmitter.h"
#include "vm/CompletionKind.h"
#include "vm/UsingHint.h"

namespace js::frontend {

struct BytecodeEmitter;
class EmitterScope;

class MOZ_STACK_CLASS UsingEmitter {
 private:
  BytecodeEmitter* bce_;

  mozilla::Maybe<TryEmitter> tryEmitter_;

  // TODO: add state transition graph and state
  // management for this emitter. (Bug 1904346)

  bool hasAwaitUsing_ = false;

  [[nodiscard]] bool emitThrowIfException();

  [[nodiscard]] bool emitGetDisposeMethod(UsingHint hint);

  [[nodiscard]] bool emitCreateDisposableResource(UsingHint hint);

  [[nodiscard]] bool emitTakeDisposeCapability();

  [[nodiscard]] bool emitResourcePropertyAccess(TaggedParserAtomIndex prop,
                                                unsigned resourcesFromTop = 1);

  [[nodiscard]] bool emitDisposeLoop(
      EmitterScope& es,
      CompletionKind initialCompletion = CompletionKind::Normal);

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
