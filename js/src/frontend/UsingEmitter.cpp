/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/UsingEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"
#include "frontend/TryEmitter.h"
#include "vm/DisposeJumpKind.h"

using namespace js;
using namespace js::frontend;

UsingEmitter::UsingEmitter(BytecodeEmitter* bce) : bce_(bce) {}

bool UsingEmitter::prepareForDisposableScopeBody() {
  tryEmitter_.emplace(bce_, TryEmitter::Kind::TryFinally,
                      TryEmitter::ControlKind::NonSyntactic);
  return tryEmitter_->emitTry();
}

bool UsingEmitter::prepareForAssignment(UsingHint hint) {
  MOZ_ASSERT(hint == UsingHint::Sync);

  MOZ_ASSERT(bce_->innermostEmitterScope()->hasDisposables());

  //        [stack] VAL
  return bce_->emit2(JSOp::AddDisposable, uint8_t(hint));
}

bool UsingEmitter::prepareForForOfLoopIteration() {
  MOZ_ASSERT(bce_->innermostEmitterScopeNoCheck()->hasDisposables());
  return bce_->emit2(JSOp::DisposeDisposables,
                     uint8_t(DisposeJumpKind::JumpOnError));
}

bool UsingEmitter::prepareForForOfIteratorCloseOnThrow() {
  MOZ_ASSERT(bce_->innermostEmitterScopeNoCheck()->hasDisposables());
  return bce_->emit2(JSOp::DisposeDisposables,
                     uint8_t(DisposeJumpKind::NoJumpOnError));
}

bool UsingEmitter::emitNonLocalJump(EmitterScope* present) {
  MOZ_ASSERT(present->hasDisposables());
  return bce_->emit2(JSOp::DisposeDisposables,
                     uint8_t(DisposeJumpKind::JumpOnError));
}

bool UsingEmitter::emitEnd() {
  MOZ_ASSERT(bce_->innermostEmitterScopeNoCheck()->hasDisposables());
  MOZ_ASSERT(tryEmitter_.isSome());

  // Given that we are using NonSyntactic TryEmitter we do
  // not have fallthrough behaviour in the normal completion case
  // see comment on controlInfo_ in TryEmitter.h
  if (!bce_->emit2(JSOp::DisposeDisposables,
                   uint8_t(DisposeJumpKind::JumpOnError))) {
    return false;
  }

#ifdef DEBUG
  // We want to ensure that we have EXC and STACK on the stack
  // and not RESUME_INDEX, non-existence of control info
  // confirms the same.
  MOZ_ASSERT(!tryEmitter_->hasControlInfo());
#endif

  if (!tryEmitter_->emitFinally()) {
    //     [stack] EXC STACK THROWING
    return false;
  }

  if (!bce_->emitDupAt(2)) {
    //     [stack] EXC STACK THROWING STACK
    return false;
  }

  if (!bce_->emitDupAt(2)) {
    //     [stack] EXC STACK THROWING EXC STACK
    return false;
  }

  if (!bce_->emit1(JSOp::ThrowWithStackWithoutJump)) {
    //     [stack] EXC STACK THROWING
    return false;
  }

  if (!bce_->emit2(JSOp::DisposeDisposables,
                   uint8_t(DisposeJumpKind::JumpOnError))) {
    //     [stack] EXC STACK THROWING
    return false;
  }

  // TODO: The additional code emitted by emitEnd is unreachable
  // since we enter the finally only in the error case and
  // DisposeDisposables always throws. Special case the
  // TryEmitter to not emit in this case. (Bug 1908953)
  if (!tryEmitter_->emitEnd()) {
    //     [stack]
    return false;
  }

  return true;
}
