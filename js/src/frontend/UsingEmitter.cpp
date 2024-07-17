/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/UsingEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"
#include "vm/DisposeJumpKind.h"

using namespace js;
using namespace js::frontend;

UsingEmitter::UsingEmitter(BytecodeEmitter* bce) : bce_(bce) {}

bool UsingEmitter::prepareForDisposableScopeBody() {
  depthAtDisposables_ = bce_->bytecodeSection().stackDepth();
  disposableStart_ = bce_->bytecodeSection().offset();
  return bce_->emit1(JSOp::TryUsing);
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
  MOZ_ASSERT(disposableStart_.valid());

  if (!bce_->addTryNote(TryNoteKind::Using, depthAtDisposables_,
                        disposableStart_, bce_->bytecodeSection().offset())) {
    return false;
  }

  if (!bce_->emit2(JSOp::DisposeDisposables,
                   uint8_t(DisposeJumpKind::JumpOnError))) {
    return false;
  }

  return true;
}
