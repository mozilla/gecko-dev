/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/UsingEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"

using namespace js;
using namespace js::frontend;

UsingEmitter::UsingEmitter(BytecodeEmitter* bce) : bce_(bce) {}

bool UsingEmitter::prepareForAssignment(Kind kind) {
  MOZ_ASSERT(kind == Kind::Sync);

  bce_->innermostEmitterScope()->setHasDisposables();

  if (!bce_->emit1(JSOp::AddDisposable)) {
    //        [stack] VAL
    return false;
  }

  return true;
}

bool UsingEmitter::emitEnd() {
  MOZ_ASSERT(bce_->innermostEmitterScopeNoCheck()->hasDisposables());
  if (!bce_->emit1(JSOp::DisposeDisposables)) {
    return false;
  }
  return true;
}
