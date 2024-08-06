/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/UsingEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"
#include "frontend/IfEmitter.h"
#include "frontend/TryEmitter.h"
#include "frontend/WhileEmitter.h"
#include "vm/CompletionKind.h"
#include "vm/DisposeJumpKind.h"

using namespace js;
using namespace js::frontend;

UsingEmitter::UsingEmitter(BytecodeEmitter* bce) : bce_(bce) {}

// Explicit Resource Management Proposal
// DisposeResources ( disposeCapability, completion )
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposeresources
bool UsingEmitter::emitDisposeLoop(CompletionKind initialCompletion,
                                   DisposeJumpKind jumpKind) {
  MOZ_ASSERT(initialCompletion != CompletionKind::Return);

  // corresponds to completion parameter
  if (initialCompletion == CompletionKind::Throw) {
    if (!bce_->emit1(JSOp::True)) {
      // [stack] THROWING
      return false;
    }
    if (!bce_->emit1(JSOp::Exception)) {
      // [stack] THROWING EXC
      return false;
    }
  } else {
    if (!bce_->emit1(JSOp::False)) {
      // [stack] THROWING
      return false;
    }
    if (!bce_->emit1(JSOp::Undefined)) {
      // [stack] THROWING UNDEF
      return false;
    }
  }

  // We do the iteration in reverse order as per spec,
  // there can be the case when count is 0 and hence index
  // below becomes -1 but the loop condition will ensure
  // no code is executed in that case.
  // Step 6. Set disposeCapability.[[DisposableResourceStack]] to a new empty
  // List.
  if (!bce_->emit1(JSOp::TakeDisposeCapability)) {
    // [stack] THROWING EXC RESOURCES COUNT
    return false;
  }

  if (!bce_->emit1(JSOp::Dec)) {
    // [stack] THROWING EXC RESOURCES INDEX
    return false;
  }

  InternalWhileEmitter wh(bce_);

  // Step 3. For each element resource of
  // disposeCapability.[[DisposableResourceStack]], in reverse list order, do
  if (!wh.emitCond()) {
    // [stack] THROWING EXC RESOURCES INDEX
    return false;
  }

  if (!bce_->emit1(JSOp::Dup)) {
    // [stack] THROWING EXC RESOURCES INDEX INDEX
    return false;
  }

  if (!bce_->emit1(JSOp::Zero)) {
    // [stack] THROWING EXC RESOURCES INDEX INDEX 0
    return false;
  }

  if (!bce_->emit1(JSOp::Ge)) {
    // [stack] THROWING EXC RESOURCES INDEX BOOL
    return false;
  }

  if (!wh.emitBody()) {
    // [stack] THROWING EXC RESOURCES INDEX
    return false;
  }

  if (!bce_->emit1(JSOp::Dup2)) {
    // [stack] THROWING EXC RESOURCES INDEX RESOURCES INDEX
    return false;
  }

  // Step 3.a. Let value be resource.[[ResourceValue]].
  // Step 3.b. Let hint be resource.[[Hint]].
  // Step 3.c. Let method be resource.[[DisposeMethod]].
  // TODO: Breakdown into using dense arrays and
  // js objects (Bug 1911642).
  if (!bce_->emit1(JSOp::GetDisposableRecord)) {
    // [stack] THROWING EXC RESOURCES INDEX HINT METHOD VALUE
    return false;
  }

  if (!bce_->emitDupAt(1)) {
    // [stack] THROWING EXC RESOURCES INDEX HINT METHOD VALUE METHOD
    return false;
  }

  if (!bce_->emit1(JSOp::IsNullOrUndefined)) {
    // [stack] ... HINT METHOD VALUE METHOD IS-UNDEF
    return false;
  }

  InternalIfEmitter ifMethodNotUndefined(bce_);

  // Step 3.e. If method is not undefined, then
  if (!ifMethodNotUndefined.emitThenElse(IfEmitter::ConditionKind::Negative)) {
    // [stack] THROWING EXC RESOURCES INDEX HINT METHOD VALUE METHOD
    return false;
  }

  if (!bce_->emit1(JSOp::Pop)) {
    // [stack] THROWING EXC RESOURCES INDEX HINT METHOD VALUE
    return false;
  }

  TryEmitter tryCall(bce_, TryEmitter::Kind::TryCatch,
                     TryEmitter::ControlKind::NonSyntactic);

  if (!tryCall.emitTry()) {
    // [stack] THROWING EXC RESOURCES INDEX HINT METHOD VALUE
    return false;
  }

  if (!bce_->emit1(JSOp::Dup2)) {
    // [stack] ... METHOD VALUE METHOD VALUE
    return false;
  }

  // Step 3.e.i. Let result be Completion(Call(method, value)).
  if (!bce_->emitCall(JSOp::Call, 0)) {
    // [stack] ... METHOD VALUE RESULT
    return false;
  }

  if (!bce_->emit1(JSOp::Pop)) {
    // [stack] ... METHOD VALUE
    return false;
  }

  // Step 3.e.iii. If result is a throw completion, then
  if (!tryCall.emitCatch()) {
    // [stack] THROWING EXC RESOURCES INDEX HINT METHOD VALUE EXC2
    return false;
  }

  if (!bce_->emitPickN(6)) {
    // [stack] THROWING RESOURCES INDEX HINT METHOD VALUE EXC2 EXC
    return false;
  }

  if (!bce_->emitPickN(7)) {
    // [stack] RESOURCES INDEX HINT METHOD VALUE EXC2 EXC THROWING
    return false;
  }

  InternalIfEmitter ifException(bce_);

  // Step 3.e.iii.1. If completion is a throw completion, then
  if (!ifException.emitThenElse()) {
    // [stack] RESOURCES INDEX HINT METHOD VALUE EXC2 EXC
    return false;
  }

  // Step 3.e.iii.1.a-f
  if (!bce_->emit1(JSOp::CreateSuppressedError)) {
    // [stack] RESOURCES INDEX HINT METHOD VALUE SUPPRESSED
    return false;
  }

  if (!bce_->emitUnpickN(5)) {
    // [stack] SUPPRESSED RESOURCES INDEX HINT METHOD VALUE
    return false;
  }

  if (!bce_->emit1(JSOp::True)) {
    // [stack] SUPPRESSED RESOURCES INDEX HINT METHOD VALUE THROWING
    return false;
  }

  if (!bce_->emitUnpickN(6)) {
    // [stack] THROWING SUPPRESSED RESOURCES INDEX HINT METHOD VALUE
    return false;
  }

  // Step 3.e.iii.2. Else,
  // Step 3.e.iii.2.a. Set completion to result.
  if (!ifException.emitElse()) {
    // [stack] RESOURCES INDEX HINT METHOD VALUE EXC2 EXC
    return false;
  }

  if (!bce_->emit1(JSOp::Pop)) {
    // [stack] RESOURCES INDEX HINT METHOD VALUE EXC2
    return false;
  }

  if (!bce_->emitUnpickN(5)) {
    // [stack] EXC2 RESOURCES INDEX HINT METHOD VALUE
    return false;
  }

  if (!bce_->emit1(JSOp::True)) {
    // [stack] EXC2 RESOURCES INDEX HINT METHOD VALUE THROWING
    return false;
  }

  if (!bce_->emitUnpickN(6)) {
    // [stack] THROWING EXC2 RESOURCES INDEX HINT METHOD VALUE
    return false;
  }

  if (!ifException.emitEnd()) {
    // [stack] THROWING EXC RESOURCES INDEX HINT METHOD VALUE
    return false;
  }

  if (!tryCall.emitEnd()) {
    // [stack] THROWING EXC RESOURCES INDEX HINT METHOD VALUE
    return false;
  }

  if (!bce_->emitPopN(3)) {
    // [stack] THROWING EXC RESOURCES INDEX
    return false;
  }

  if (!ifMethodNotUndefined.emitElse()) {
    // [stack] THROWING EXC RESOURCES INDEX HINT METHOD VALUE METHOD
    return false;
  }

  if (!bce_->emitPopN(4)) {
    // [stack] THROWING EXC RESOURCES INDEX
    return false;
  }

  if (!ifMethodNotUndefined.emitEnd()) {
    // [stack] THROWING EXC RESOURCES INDEX
    return false;
  }

  if (!bce_->emit1(JSOp::Dec)) {
    // [stack] THROWING EXC RESOURCES INDEX
    return false;
  }

  if (!wh.emitEnd()) {
    // [stack] THROWING EXC RESOURCES INDEX
    return false;
  }

  if (!bce_->emitPopN(2)) {
    // [stack] THROWING EXC
    return false;
  }

  // Step 7. Return ? completion.
  if (!bce_->emit1(JSOp::Swap)) {
    // [stack] EXC THROWING
    return false;
  }

  InternalIfEmitter ifThrow(bce_);

  if (!ifThrow.emitThenElse()) {
    // [stack] EXC
    return false;
  }

  if (jumpKind == DisposeJumpKind::JumpOnError) {
    if (!bce_->emit1(JSOp::Throw)) {
      // [stack]
      return false;
    }
  } else {
    if (!bce_->emit1(JSOp::ThrowWithoutJump)) {
      // [stack]
      return false;
    }
  }

  if (!ifThrow.emitElse()) {
    // [stack] EXC
    return false;
  }

  if (!bce_->emit1(JSOp::Pop)) {
    // [stack]
    return false;
  }

  if (!ifThrow.emitEnd()) {
    // [stack]
    return false;
  }

  return true;
}

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
  return emitDisposeLoop();
}

bool UsingEmitter::prepareForForOfIteratorCloseOnThrow() {
  MOZ_ASSERT(bce_->innermostEmitterScopeNoCheck()->hasDisposables());
  return emitDisposeLoop(CompletionKind::Throw, DisposeJumpKind::NoJumpOnError);
}

bool UsingEmitter::emitNonLocalJump(EmitterScope* present) {
  MOZ_ASSERT(present->hasDisposables());
  return emitDisposeLoop();
}

bool UsingEmitter::emitEnd() {
  MOZ_ASSERT(bce_->innermostEmitterScopeNoCheck()->hasDisposables());
  MOZ_ASSERT(tryEmitter_.isSome());

  // Given that we are using NonSyntactic TryEmitter we do
  // not have fallthrough behaviour in the normal completion case
  // see comment on controlInfo_ in TryEmitter.h
  if (!emitDisposeLoop()) {
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

  if (!emitDisposeLoop(CompletionKind::Throw)) {
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
