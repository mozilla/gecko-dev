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

// Class for emitting bytecode for disposal loops.
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposeresources
//
// Usage: (check for the return value is omitted for simplicity)
//
//   at the point where the disposal loop is needed
//     DisposalEmitter de(bce, hasAsyncDisposables, initialCompletion);
//     de.prepareForDisposeCapability();
//     emit_DisposeCapability();
//     de.emitEnd(es);
//
class MOZ_STACK_CLASS DisposalEmitter {
 private:
  BytecodeEmitter* bce_;
  bool hasAsyncDisposables_;
  CompletionKind initialCompletion_;

#ifdef DEBUG
  // The state of this emitter.
  //
  // +-------+  prepareForDisposeCapability  +-----------------------------+
  // | Start |------------------------------>| prepareForDisposeCapability |--+
  // +-------+                               +-----------------------------+  |
  //                                                                          |
  //   +----------------------------------------------------------------------+
  //   |
  //   |  emitEnd  +-----+
  //   +---------->| End |
  //               +-----+
  enum class State {
    // The initial state.
    Start,

    // After calling prepareForDisposeCapability.
    DisposeCapability,

    // After calling emitEnd.
    End
  };
  State state_ = State::Start;
#endif

  [[nodiscard]] bool emitResourcePropertyAccess(TaggedParserAtomIndex prop,
                                                unsigned resourcesFromTop = 1);

 public:
  DisposalEmitter(BytecodeEmitter* bce, bool hasAsyncDisposables,
                  CompletionKind initialCompletion)
      : bce_(bce),
        hasAsyncDisposables_(hasAsyncDisposables),
        initialCompletion_(initialCompletion) {}

  [[nodiscard]] bool prepareForDisposeCapability();

  [[nodiscard]] bool emitEnd(EmitterScope& es);
};

class MOZ_STACK_CLASS UsingEmitter {
 private:
  mozilla::Maybe<TryEmitter> tryEmitter_;

  // TODO: add state transition graph and state
  // management for this emitter. (Bug 1904346)

  bool hasAwaitUsing_ = false;

  [[nodiscard]] bool emitThrowIfException();

  [[nodiscard]] bool emitGetDisposeMethod(UsingHint hint);

  [[nodiscard]] bool emitCreateDisposableResource(UsingHint hint);

  [[nodiscard]] bool emitTakeDisposeCapability();

 protected:
  BytecodeEmitter* bce_;

  [[nodiscard]] bool emitDisposeResourcesForEnvironment(
      EmitterScope& es,
      CompletionKind initialCompletion = CompletionKind::Normal);

 public:
  explicit UsingEmitter(BytecodeEmitter* bce);

  bool hasAwaitUsing() const { return hasAwaitUsing_; }

  void setHasAwaitUsing(bool hasAwaitUsing) { hasAwaitUsing_ = hasAwaitUsing; }

  [[nodiscard]] bool prepareForDisposableScopeBody();

  [[nodiscard]] bool prepareForAssignment(UsingHint hint);

  [[nodiscard]] bool prepareForForOfLoopIteration();

  [[nodiscard]] bool prepareForForOfIteratorCloseOnThrow();

  [[nodiscard]] bool emitNonLocalJump(EmitterScope* present);

  [[nodiscard]] bool emitEnd();

  [[nodiscard]] bool emitNonLocalJumpNeedingIteratorClose(
      EmitterScope* present);
};

// This is a version of UsingEmitter specialized to help emit code for
// non-local jumps in for-of loops for closing iterators.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   at the point of IteratorClose inside non-local jump
//     NonLocalIteratorCloseUsingEmitter disposeBeforeIterClose(bce);
//     disposeBeforeIterClose.prepareForIteratorClose(&currentScope);
//     emit_IteratorClose();
//     disposeBeforeIterClose.emitEnd(&currentScope);
//
class MOZ_STACK_CLASS NonLocalIteratorCloseUsingEmitter
    : protected UsingEmitter {
 private:
  mozilla::Maybe<TryEmitter> tryClosingIterator_;

#ifdef DEBUG
  // The state of this emitter.
  //
  // +-------+  prepareForIteratorClose  +-------------------------+
  // | Start |-------------------------->| prepareForIteratorClose |--+
  // +-------+                           +-------------------------+  |
  //                                                                  |
  //   +--------------------------------------------------------------+
  //   |
  //   |  emitEnd  +-----+
  //   +---------->| End |
  //               +-----+
  enum class State {
    // The initial state.
    Start,

    // After calling prepareForIteratorClose.
    IteratorClose,

    // After calling emitEnd.
    End
  };
  State state_ = State::Start;
#endif

 public:
  explicit NonLocalIteratorCloseUsingEmitter(BytecodeEmitter* bce)
      : UsingEmitter(bce) {}

  [[nodiscard]] bool prepareForIteratorClose(EmitterScope& es);

  [[nodiscard]] bool emitEnd();
};

}  // namespace js::frontend

#endif  // frontend_UsingEmitter_h
