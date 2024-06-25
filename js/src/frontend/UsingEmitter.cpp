/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/UsingEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"

using namespace js;
using namespace js::frontend;

UsingEmitter::UsingEmitter(BytecodeEmitter* bce) : bce_(bce) {}

bool UsingEmitter::prepareForDisposableScopeBody() {
  depthAtDisposables_ = bce_->bytecodeSection().stackDepth();
  disposableStart_ = bce_->bytecodeSection().offset();
  if (!bce_->emit1(JSOp::TryUsing)) {
    return false;
  }
  return true;
}

bool UsingEmitter::prepareForAssignment(Kind kind) {
  MOZ_ASSERT(kind == Kind::Sync);

  MOZ_ASSERT(bce_->innermostEmitterScope()->hasDisposables());

  if (!bce_->emit1(JSOp::AddDisposable)) {
    //        [stack] VAL
    return false;
  }

  return true;
}

bool UsingEmitter::prepareForForOfLoopIteration() {
  MOZ_ASSERT(bce_->innermostEmitterScopeNoCheck()->hasDisposables());
  if (!bce_->emit1(JSOp::DisposeDisposables)) {
    return false;
  }
  return true;
}

bool UsingEmitter::prepareForForOfIteratorCloseOnThrow() {
  MOZ_ASSERT(bce_->innermostEmitterScopeNoCheck()->hasDisposables());
  if (!bce_->emit1(JSOp::DisposeDisposables)) {
    return false;
  }
  return true;
}

bool UsingEmitter::emitEnd() {
  MOZ_ASSERT(bce_->innermostEmitterScopeNoCheck()->hasDisposables());
  MOZ_ASSERT(disposableStart_.valid());

  if (!bce_->addTryNote(TryNoteKind::Using, depthAtDisposables_,
                        disposableStart_, bce_->bytecodeSection().offset())) {
    return false;
  }

  if (!bce_->emit1(JSOp::DisposeDisposables)) {
    return false;
  }

  return true;
}
