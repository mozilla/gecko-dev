/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS bytecode generation.
 */

#include "frontend/BytecodeEmitter.h"

#include "mozilla/Casting.h"    // mozilla::AssertedCast
#include "mozilla/DebugOnly.h"  // mozilla::DebugOnly
#include "mozilla/FloatingPoint.h"  // mozilla::NumberEqualsInt32, mozilla::NumberIsInt32
#include "mozilla/Maybe.h"          // mozilla::{Maybe,Nothing,Some}
#include "mozilla/PodOperations.h"  // mozilla::PodCopy
#include "mozilla/Sprintf.h"        // SprintfLiteral
#include "mozilla/Unused.h"         // mozilla::Unused
#include "mozilla/Variant.h"        // mozilla::AsVariant

#include <string.h>

#include "jsnum.h"    // NumberToAtom
#include "jstypes.h"  // JS_BIT
#include "jsutil.h"   // Min

#include "ds/Nestable.h"                         // Nestable
#include "frontend/BytecodeControlStructures.h"  // NestableControl, BreakableControl, LabelControl, LoopControl, TryFinallyControl
#include "frontend/CallOrNewEmitter.h"           // CallOrNewEmitter
#include "frontend/CForEmitter.h"                // CForEmitter
#include "frontend/DefaultEmitter.h"             // DefaultEmitter
#include "frontend/DoWhileEmitter.h"             // DoWhileEmitter
#include "frontend/ElemOpEmitter.h"              // ElemOpEmitter
#include "frontend/EmitterScope.h"               // EmitterScope
#include "frontend/ExpressionStatementEmitter.h"  // ExpressionStatementEmitter
#include "frontend/ForInEmitter.h"                // ForInEmitter
#include "frontend/ForOfEmitter.h"                // ForOfEmitter
#include "frontend/ForOfLoopControl.h"            // ForOfLoopControl
#include "frontend/FunctionEmitter.h"  // FunctionEmitter, FunctionScriptEmitter, FunctionParamsEmitter
#include "frontend/IfEmitter.h"     // IfEmitter, InternalIfEmitter, CondEmitter
#include "frontend/LabelEmitter.h"  // LabelEmitter
#include "frontend/LexicalScopeEmitter.h"  // LexicalScopeEmitter
#include "frontend/ModuleSharedContext.h"  // ModuleSharedContext
#include "frontend/NameFunctions.h"        // NameFunctions
#include "frontend/NameOpEmitter.h"        // NameOpEmitter
#include "frontend/ObjectEmitter.h"  // PropertyEmitter, ObjectEmitter, ClassEmitter
#include "frontend/ParseNode.h"  // ParseNodeKind, ParseNode and subclasses, ObjectBox
#include "frontend/Parser.h"         // Parser
#include "frontend/PropOpEmitter.h"  // PropOpEmitter
#include "frontend/SwitchEmitter.h"  // SwitchEmitter
#include "frontend/TDZCheckCache.h"  // TDZCheckCache
#include "frontend/TryEmitter.h"     // TryEmitter
#include "frontend/WhileEmitter.h"   // WhileEmitter
#include "js/CompileOptions.h"       // TransitiveCompileOptions, CompileOptions
#include "vm/AsyncFunction.h"        // AsyncFunctionResolveKind
#include "vm/BytecodeUtil.h"  // IsArgOp, IsLocalOp, SET_UINT24, SET_ICINDEX, BytecodeFallsThrough, BytecodeIsJumpTarget
#include "vm/GeneratorObject.h"  // AbstractGeneratorObject
#include "vm/JSAtom.h"           // JSAtom, js_*_str
#include "vm/JSContext.h"        // JSContext
#include "vm/JSFunction.h"       // FunctionPrefixKind, JSFunction,
#include "vm/JSScript.h"  // JSScript, ScopeNote, ScriptSourceObject, FieldInitializers, JSScript, LazyScript
#include "vm/Opcodes.h"   // JSOP_*
#include "wasm/AsmJS.h"   // IsAsmJSModule

#include "debugger/DebugAPI-inl.h"  // DebugAPI
#include "vm/JSObject-inl.h"        // JSObject

using namespace js;
using namespace js::frontend;

using mozilla::AssertedCast;
using mozilla::AsVariant;
using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::NumberEqualsInt32;
using mozilla::NumberIsInt32;
using mozilla::PodCopy;
using mozilla::Some;
using mozilla::Unused;

static bool ParseNodeRequiresSpecialLineNumberNotes(ParseNode* pn) {
  // The few node types listed below are exceptions to the usual
  // location-source-note-emitting code in BytecodeEmitter::emitTree().
  // Single-line `while` loops and C-style `for` loops require careful
  // handling to avoid strange stepping behavior.
  // Functions usually shouldn't have location information (bug 1431202).

  ParseNodeKind kind = pn->getKind();
  return kind == ParseNodeKind::WhileStmt || kind == ParseNodeKind::ForStmt ||
         kind == ParseNodeKind::Function;
}

BytecodeEmitter::BytecodeEmitter(
    BytecodeEmitter* parent, SharedContext* sc, HandleScript script,
    Handle<LazyScript*> lazyScript, uint32_t line, uint32_t column,
    EmitterMode emitterMode,
    FieldInitializers fieldInitializers /* = FieldInitializers::Invalid() */)
    : sc(sc),
      cx(sc->cx_),
      parent(parent),
      script(cx, script),
      lazyScript(cx, lazyScript),
      bytecodeSection_(cx, line),
      perScriptData_(cx),
      fieldInitializers_(fieldInitializers),
      firstLine(line),
      firstColumn(column),
      emitterMode(emitterMode) {
  MOZ_ASSERT_IF(emitterMode == LazyFunction, lazyScript);

  if (sc->isFunctionBox()) {
    // Functions have IC entries for type monitoring |this| and arguments.
    bytecodeSection().setNumICEntries(sc->asFunctionBox()->nargs() + 1);
  }
}

BytecodeEmitter::BytecodeEmitter(BytecodeEmitter* parent,
                                 BCEParserHandle* handle, SharedContext* sc,
                                 HandleScript script,
                                 Handle<LazyScript*> lazyScript, uint32_t line,
                                 uint32_t column, EmitterMode emitterMode,
                                 FieldInitializers fieldInitializers)
    : BytecodeEmitter(parent, sc, script, lazyScript, line, column, emitterMode,
                      fieldInitializers) {
  parser = handle;
  instrumentationKinds = parser->options().instrumentationKinds;
}

BytecodeEmitter::BytecodeEmitter(BytecodeEmitter* parent,
                                 const EitherParser& parser, SharedContext* sc,
                                 HandleScript script,
                                 Handle<LazyScript*> lazyScript, uint32_t line,
                                 uint32_t column, EmitterMode emitterMode,
                                 FieldInitializers fieldInitializers)
    : BytecodeEmitter(parent, sc, script, lazyScript, line, column, emitterMode,
                      fieldInitializers) {
  ep_.emplace(parser);
  this->parser = ep_.ptr();
  instrumentationKinds = this->parser->options().instrumentationKinds;
}

void BytecodeEmitter::initFromBodyPosition(TokenPos bodyPosition) {
  setScriptStartOffsetIfUnset(bodyPosition.begin);
  setFunctionBodyEndPos(bodyPosition.end);
}

bool BytecodeEmitter::init() { return perScriptData_.init(cx); }

bool BytecodeEmitter::init(TokenPos bodyPosition) {
  initFromBodyPosition(bodyPosition);
  return init();
}

template <typename T>
T* BytecodeEmitter::findInnermostNestableControl() const {
  return NestableControl::findNearest<T>(innermostNestableControl);
}

template <typename T, typename Predicate /* (T*) -> bool */>
T* BytecodeEmitter::findInnermostNestableControl(Predicate predicate) const {
  return NestableControl::findNearest<T>(innermostNestableControl, predicate);
}

NameLocation BytecodeEmitter::lookupName(JSAtom* name) {
  return innermostEmitterScope()->lookup(this, name);
}

Maybe<NameLocation> BytecodeEmitter::locationOfNameBoundInScope(
    JSAtom* name, EmitterScope* target) {
  return innermostEmitterScope()->locationBoundInScope(name, target);
}

Maybe<NameLocation> BytecodeEmitter::locationOfNameBoundInFunctionScope(
    JSAtom* name, EmitterScope* source) {
  EmitterScope* funScope = source;
  while (!funScope->scope(this)->is<FunctionScope>()) {
    funScope = funScope->enclosingInFrame();
  }
  return source->locationBoundInScope(name, funScope);
}

bool BytecodeEmitter::markStepBreakpoint() {
  if (inPrologue()) {
    return true;
  }

  if (!emitInstrumentation(InstrumentationKind::Breakpoint)) {
    return false;
  }

  if (!newSrcNote(SRC_STEP_SEP)) {
    return false;
  }

  if (!newSrcNote(SRC_BREAKPOINT)) {
    return false;
  }

  // We track the location of the most recent separator for use in
  // markSimpleBreakpoint. Note that this means that the position must already
  // be set before markStepBreakpoint is called.
  bytecodeSection().updateSeparatorPosition();

  return true;
}

bool BytecodeEmitter::markSimpleBreakpoint() {
  if (inPrologue()) {
    return true;
  }

  // If a breakable call ends up being the same location as the most recent
  // expression start, we need to skip marking it breakable in order to avoid
  // having two breakpoints with the same line/column position.
  // Note: This assumes that the position for the call has already been set.
  if (!bytecodeSection().isDuplicateLocation()) {
    if (!emitInstrumentation(InstrumentationKind::Breakpoint)) {
      return false;
    }

    if (!newSrcNote(SRC_BREAKPOINT)) {
      return false;
    }
  }

  return true;
}

bool BytecodeEmitter::emitCheck(JSOp op, ptrdiff_t delta,
                                BytecodeOffset* offset) {
  size_t oldLength = bytecodeSection().code().length();
  *offset = BytecodeOffset(oldLength);

  size_t newLength = oldLength + size_t(delta);
  if (MOZ_UNLIKELY(newLength > MaxBytecodeLength)) {
    ReportAllocationOverflow(cx);
    return false;
  }

  if (!bytecodeSection().code().growByUninitialized(delta)) {
    return false;
  }

  if (BytecodeOpHasTypeSet(op)) {
    bytecodeSection().incrementNumTypeSets();
  }

  if (BytecodeOpHasIC(op)) {
    // Even if every bytecode op is a JOF_IC op and the function has ARGC_LIMIT
    // arguments, numICEntries cannot overflow.
    static_assert(MaxBytecodeLength + 1 /* this */ + ARGC_LIMIT <= UINT32_MAX,
                  "numICEntries must not overflow");
    bytecodeSection().incrementNumICEntries();
  }

  return true;
}

#ifdef DEBUG
bool BytecodeEmitter::checkStrictOrSloppy(JSOp op) {
  if (IsCheckStrictOp(op) && !sc->strict()) {
    return false;
  }
  if (IsCheckSloppyOp(op) && sc->strict()) {
    return false;
  }
  return true;
}
#endif

bool BytecodeEmitter::emit1(JSOp op) {
  MOZ_ASSERT(checkStrictOrSloppy(op));

  BytecodeOffset offset;
  if (!emitCheck(op, 1, &offset)) {
    return false;
  }

  jsbytecode* code = bytecodeSection().code(offset);
  code[0] = jsbytecode(op);
  bytecodeSection().updateDepth(offset);
  return true;
}

bool BytecodeEmitter::emit2(JSOp op, uint8_t op1) {
  MOZ_ASSERT(checkStrictOrSloppy(op));

  BytecodeOffset offset;
  if (!emitCheck(op, 2, &offset)) {
    return false;
  }

  jsbytecode* code = bytecodeSection().code(offset);
  code[0] = jsbytecode(op);
  code[1] = jsbytecode(op1);
  bytecodeSection().updateDepth(offset);
  return true;
}

bool BytecodeEmitter::emit3(JSOp op, jsbytecode op1, jsbytecode op2) {
  MOZ_ASSERT(checkStrictOrSloppy(op));

  /* These should filter through emitVarOp. */
  MOZ_ASSERT(!IsArgOp(op));
  MOZ_ASSERT(!IsLocalOp(op));

  BytecodeOffset offset;
  if (!emitCheck(op, 3, &offset)) {
    return false;
  }

  jsbytecode* code = bytecodeSection().code(offset);
  code[0] = jsbytecode(op);
  code[1] = op1;
  code[2] = op2;
  bytecodeSection().updateDepth(offset);
  return true;
}

bool BytecodeEmitter::emitN(JSOp op, size_t extra, BytecodeOffset* offset) {
  MOZ_ASSERT(checkStrictOrSloppy(op));
  ptrdiff_t length = 1 + ptrdiff_t(extra);

  BytecodeOffset off;
  if (!emitCheck(op, length, &off)) {
    return false;
  }

  jsbytecode* code = bytecodeSection().code(off);
  code[0] = jsbytecode(op);
  /* The remaining |extra| bytes are set by the caller */

  /*
   * Don't updateDepth if op's use-count comes from the immediate
   * operand yet to be stored in the extra bytes after op.
   */
  if (CodeSpec[op].nuses >= 0) {
    bytecodeSection().updateDepth(off);
  }

  if (offset) {
    *offset = off;
  }
  return true;
}

bool BytecodeEmitter::emitJumpTargetOp(JSOp op, BytecodeOffset* off) {
  MOZ_ASSERT(BytecodeIsJumpTarget(op));

  uint32_t numEntries = bytecodeSection().numICEntries();

  if (!emitN(op, CodeSpec[op].length - 1, off)) {
    return false;
  }

  SET_ICINDEX(bytecodeSection().code(*off), numEntries);
  return true;
}

bool BytecodeEmitter::emitJumpTarget(JumpTarget* target) {
  BytecodeOffset off = bytecodeSection().offset();

  // Alias consecutive jump targets.
  if (bytecodeSection().lastTargetOffset().valid() &&
      off == bytecodeSection().lastTargetOffset() +
                 BytecodeOffsetDiff(JSOP_JUMPTARGET_LENGTH)) {
    target->offset = bytecodeSection().lastTargetOffset();
    return true;
  }

  target->offset = off;
  bytecodeSection().setLastTargetOffset(off);

  BytecodeOffset opOff;
  return emitJumpTargetOp(JSOP_JUMPTARGET, &opOff);
}

bool BytecodeEmitter::emitJumpNoFallthrough(JSOp op, JumpList* jump) {
  BytecodeOffset offset;
  if (!emitCheck(op, 5, &offset)) {
    return false;
  }

  jsbytecode* code = bytecodeSection().code(offset);
  code[0] = jsbytecode(op);
  MOZ_ASSERT(!jump->offset.valid() ||
             (0 <= jump->offset.value() && jump->offset < offset));
  jump->push(bytecodeSection().code(BytecodeOffset(0)), offset);
  bytecodeSection().updateDepth(offset);
  return true;
}

bool BytecodeEmitter::emitJump(JSOp op, JumpList* jump) {
  if (!emitJumpNoFallthrough(op, jump)) {
    return false;
  }
  if (BytecodeFallsThrough(op)) {
    JumpTarget fallthrough;
    if (!emitJumpTarget(&fallthrough)) {
      return false;
    }
  }
  return true;
}

bool BytecodeEmitter::emitBackwardJump(JSOp op, JumpTarget target,
                                       JumpList* jump,
                                       JumpTarget* fallthrough) {
  if (!emitJumpNoFallthrough(op, jump)) {
    return false;
  }
  patchJumpsToTarget(*jump, target);

  // Unconditionally create a fallthrough for closing iterators, and as a
  // target for break statements.
  if (!emitJumpTarget(fallthrough)) {
    return false;
  }
  return true;
}

void BytecodeEmitter::patchJumpsToTarget(JumpList jump, JumpTarget target) {
  MOZ_ASSERT(
      !jump.offset.valid() ||
      (0 <= jump.offset.value() && jump.offset <= bytecodeSection().offset()));
  MOZ_ASSERT(0 <= target.offset.value() &&
             target.offset <= bytecodeSection().offset());
  MOZ_ASSERT_IF(
      jump.offset.valid() &&
          target.offset + BytecodeOffsetDiff(4) <= bytecodeSection().offset(),
      BytecodeIsJumpTarget(JSOp(*bytecodeSection().code(target.offset))));
  jump.patchAll(bytecodeSection().code(BytecodeOffset(0)), target);
}

bool BytecodeEmitter::emitJumpTargetAndPatch(JumpList jump) {
  if (!jump.offset.valid()) {
    return true;
  }
  JumpTarget target;
  if (!emitJumpTarget(&target)) {
    return false;
  }
  patchJumpsToTarget(jump, target);
  return true;
}

bool BytecodeEmitter::emitCall(JSOp op, uint16_t argc,
                               const Maybe<uint32_t>& sourceCoordOffset) {
  if (sourceCoordOffset.isSome()) {
    if (!updateSourceCoordNotes(*sourceCoordOffset)) {
      return false;
    }
  }
  return emit3(op, ARGC_LO(argc), ARGC_HI(argc));
}

bool BytecodeEmitter::emitCall(JSOp op, uint16_t argc, ParseNode* pn) {
  return emitCall(op, argc, pn ? Some(pn->pn_pos.begin) : Nothing());
}

bool BytecodeEmitter::emitDupAt(unsigned slotFromTop, unsigned count) {
  MOZ_ASSERT(slotFromTop < unsigned(bytecodeSection().stackDepth()));
  MOZ_ASSERT(slotFromTop + 1 >= count);

  if (slotFromTop == 0 && count == 1) {
    return emit1(JSOP_DUP);
  }

  if (slotFromTop == 1 && count == 2) {
    return emit1(JSOP_DUP2);
  }

  if (slotFromTop >= JS_BIT(24)) {
    reportError(nullptr, JSMSG_TOO_MANY_LOCALS);
    return false;
  }

  for (unsigned i = 0; i < count; i++) {
    BytecodeOffset off;
    if (!emitN(JSOP_DUPAT, 3, &off)) {
      return false;
    }

    jsbytecode* pc = bytecodeSection().code(off);
    SET_UINT24(pc, slotFromTop);
  }

  return true;
}

bool BytecodeEmitter::emitPopN(unsigned n) {
  MOZ_ASSERT(n != 0);

  if (n == 1) {
    return emit1(JSOP_POP);
  }

  // 2 JSOP_POPs (2 bytes) are shorter than JSOP_POPN (3 bytes).
  if (n == 2) {
    return emit1(JSOP_POP) && emit1(JSOP_POP);
  }

  return emitUint16Operand(JSOP_POPN, n);
}

bool BytecodeEmitter::emitCheckIsObj(CheckIsObjectKind kind) {
  return emit2(JSOP_CHECKISOBJ, uint8_t(kind));
}

bool BytecodeEmitter::emitCheckIsCallable(CheckIsCallableKind kind) {
  return emit2(JSOP_CHECKISCALLABLE, uint8_t(kind));
}

static inline unsigned LengthOfSetLine(unsigned line) {
  return 1 /* SRC_SETLINE */ + (line > SN_4BYTE_OFFSET_MASK ? 4 : 1);
}

/* Updates line number notes, not column notes. */
bool BytecodeEmitter::updateLineNumberNotes(uint32_t offset) {
  // Don't emit line/column number notes in the prologue.
  if (inPrologue()) {
    return true;
  }

  ErrorReporter* er = &parser->errorReporter();
  bool onThisLine;
  if (!er->isOnThisLine(offset, bytecodeSection().currentLine(), &onThisLine)) {
    er->errorNoOffset(JSMSG_OUT_OF_MEMORY);
    return false;
  }

  if (!onThisLine) {
    unsigned line = er->lineAt(offset);
    unsigned delta = line - bytecodeSection().currentLine();

    /*
     * Encode any change in the current source line number by using
     * either several SRC_NEWLINE notes or just one SRC_SETLINE note,
     * whichever consumes less space.
     *
     * NB: We handle backward line number deltas (possible with for
     * loops where the update part is emitted after the body, but its
     * line number is <= any line number in the body) here by letting
     * unsigned delta_ wrap to a very large number, which triggers a
     * SRC_SETLINE.
     */
    bytecodeSection().setCurrentLine(line, offset);
    if (delta >= LengthOfSetLine(line)) {
      if (!newSrcNote2(SRC_SETLINE, ptrdiff_t(line))) {
        return false;
      }
    } else {
      do {
        if (!newSrcNote(SRC_NEWLINE)) {
          return false;
        }
      } while (--delta != 0);
    }

    bytecodeSection().updateSeparatorPositionIfPresent();
  }
  return true;
}

/* Updates the line number and column number information in the source notes. */
bool BytecodeEmitter::updateSourceCoordNotes(uint32_t offset) {
  if (!updateLineNumberNotes(offset)) {
    return false;
  }

  // Don't emit line/column number notes in the prologue.
  if (inPrologue()) {
    return true;
  }

  uint32_t columnIndex = parser->errorReporter().columnAt(offset);
  ptrdiff_t colspan =
      ptrdiff_t(columnIndex) - ptrdiff_t(bytecodeSection().lastColumn());
  if (colspan != 0) {
    // If the column span is so large that we can't store it, then just
    // discard this information. This can happen with minimized or otherwise
    // machine-generated code. Even gigantic column numbers are still
    // valuable if you have a source map to relate them to something real;
    // but it's better to fail soft here.
    if (!SN_REPRESENTABLE_COLSPAN(colspan)) {
      return true;
    }
    if (!newSrcNote2(SRC_COLSPAN, SN_COLSPAN_TO_OFFSET(colspan))) {
      return false;
    }
    bytecodeSection().setLastColumn(columnIndex, offset);
    bytecodeSection().updateSeparatorPositionIfPresent();
  }
  return true;
}

Maybe<uint32_t> BytecodeEmitter::getOffsetForLoop(ParseNode* nextpn) {
  if (!nextpn) {
    return Nothing();
  }

  // Try to give the JSOP_LOOPHEAD and JSOP_LOOPENTRY the same line number as
  // the next instruction. nextpn is often a block, in which case the next
  // instruction typically comes from the first statement inside.
  if (nextpn->is<LexicalScopeNode>()) {
    nextpn = nextpn->as<LexicalScopeNode>().scopeBody();
  }
  if (nextpn->isKind(ParseNodeKind::StatementList)) {
    if (ParseNode* firstStatement = nextpn->as<ListNode>().head()) {
      nextpn = firstStatement;
    }
  }

  return Some(nextpn->pn_pos.begin);
}

bool BytecodeEmitter::emitUint16Operand(JSOp op, uint32_t operand) {
  MOZ_ASSERT(operand <= UINT16_MAX);
  if (!emit3(op, UINT16_LO(operand), UINT16_HI(operand))) {
    return false;
  }
  return true;
}

bool BytecodeEmitter::emitUint32Operand(JSOp op, uint32_t operand) {
  BytecodeOffset off;
  if (!emitN(op, 4, &off)) {
    return false;
  }
  SET_UINT32(bytecodeSection().code(off), operand);
  return true;
}

namespace {

class NonLocalExitControl {
 public:
  enum Kind {
    // IteratorClose is handled especially inside the exception unwinder.
    Throw,

    // A 'continue' statement does not call IteratorClose for the loop it
    // is continuing, i.e. excluding the target loop.
    Continue,

    // A 'break' or 'return' statement does call IteratorClose for the
    // loop it is breaking out of or returning from, i.e. including the
    // target loop.
    Break,
    Return
  };

 private:
  BytecodeEmitter* bce_;
  const uint32_t savedScopeNoteIndex_;
  const int savedDepth_;
  uint32_t openScopeNoteIndex_;
  Kind kind_;

  NonLocalExitControl(const NonLocalExitControl&) = delete;

  MOZ_MUST_USE bool leaveScope(EmitterScope* scope);

 public:
  NonLocalExitControl(BytecodeEmitter* bce, Kind kind)
      : bce_(bce),
        savedScopeNoteIndex_(bce->bytecodeSection().scopeNoteList().length()),
        savedDepth_(bce->bytecodeSection().stackDepth()),
        openScopeNoteIndex_(bce->innermostEmitterScope()->noteIndex()),
        kind_(kind) {}

  ~NonLocalExitControl() {
    for (uint32_t n = savedScopeNoteIndex_;
         n < bce_->bytecodeSection().scopeNoteList().length(); n++) {
      bce_->bytecodeSection().scopeNoteList().recordEnd(
          n, bce_->bytecodeSection().offset());
    }
    bce_->bytecodeSection().setStackDepth(savedDepth_);
  }

  MOZ_MUST_USE bool prepareForNonLocalJump(NestableControl* target);

  MOZ_MUST_USE bool prepareForNonLocalJumpToOutermost() {
    return prepareForNonLocalJump(nullptr);
  }
};

bool NonLocalExitControl::leaveScope(EmitterScope* es) {
  if (!es->leave(bce_, /* nonLocal = */ true)) {
    return false;
  }

  // As we pop each scope due to the non-local jump, emit notes that
  // record the extent of the enclosing scope. These notes will have
  // their ends recorded in ~NonLocalExitControl().
  uint32_t enclosingScopeIndex = ScopeNote::NoScopeIndex;
  if (es->enclosingInFrame()) {
    enclosingScopeIndex = es->enclosingInFrame()->index();
  }
  if (!bce_->bytecodeSection().scopeNoteList().append(
          enclosingScopeIndex, bce_->bytecodeSection().offset(),
          openScopeNoteIndex_)) {
    return false;
  }
  openScopeNoteIndex_ = bce_->bytecodeSection().scopeNoteList().length() - 1;

  return true;
}

/*
 * Emit additional bytecode(s) for non-local jumps.
 */
bool NonLocalExitControl::prepareForNonLocalJump(NestableControl* target) {
  EmitterScope* es = bce_->innermostEmitterScope();
  int npops = 0;

  AutoCheckUnstableEmitterScope cues(bce_);

  // For 'continue', 'break', and 'return' statements, emit IteratorClose
  // bytecode inline. 'continue' statements do not call IteratorClose for
  // the loop they are continuing.
  bool emitIteratorClose =
      kind_ == Continue || kind_ == Break || kind_ == Return;
  bool emitIteratorCloseAtTarget = emitIteratorClose && kind_ != Continue;

  auto flushPops = [&npops](BytecodeEmitter* bce) {
    if (npops && !bce->emitPopN(npops)) {
      return false;
    }
    npops = 0;
    return true;
  };

  // If we are closing multiple for-of loops, the resulting FOR_OF_ITERCLOSE
  // trynotes must be appropriately nested. Each FOR_OF_ITERCLOSE starts when
  // we close the corresponding for-of iterator, and continues until the
  // actual jump.
  Vector<BytecodeOffset, 4> forOfIterCloseScopeStarts(bce_->cx);

  // Walk the nestable control stack and patch jumps.
  for (NestableControl* control = bce_->innermostNestableControl;
       control != target; control = control->enclosing()) {
    // Walk the scope stack and leave the scopes we entered. Leaving a scope
    // may emit administrative ops like JSOP_POPLEXICALENV but never anything
    // that manipulates the stack.
    for (; es != control->emitterScope(); es = es->enclosingInFrame()) {
      if (!leaveScope(es)) {
        return false;
      }
    }

    switch (control->kind()) {
      case StatementKind::Finally: {
        TryFinallyControl& finallyControl = control->as<TryFinallyControl>();
        if (finallyControl.emittingSubroutine()) {
          /*
           * There's a [exception or hole, retsub pc-index] pair and the
           * possible return value on the stack that we need to pop.
           */
          npops += 3;
        } else {
          if (!flushPops(bce_)) {
            return false;
          }
          if (!bce_->emitGoSub(&finallyControl.gosubs)) {
            //      [stack] ...
            return false;
          }
        }
        break;
      }

      case StatementKind::ForOfLoop:
        if (emitIteratorClose) {
          if (!flushPops(bce_)) {
            return false;
          }
          BytecodeOffset tryNoteStart;
          ForOfLoopControl& loopinfo = control->as<ForOfLoopControl>();
          if (!loopinfo.emitPrepareForNonLocalJumpFromScope(
                  bce_, *es,
                  /* isTarget = */ false, &tryNoteStart)) {
            //      [stack] ...
            return false;
          }
          if (!forOfIterCloseScopeStarts.append(tryNoteStart)) {
            return false;
          }
        } else {
          // The iterator next method, the iterator, and the current
          // value are on the stack.
          npops += 3;
        }
        break;

      case StatementKind::ForInLoop:
        if (!flushPops(bce_)) {
          return false;
        }

        // The iterator and the current value are on the stack.
        if (!bce_->emit1(JSOP_POP)) {
          //        [stack] ... ITER
          return false;
        }
        if (!bce_->emit1(JSOP_ENDITER)) {
          //        [stack] ...
          return false;
        }
        break;

      default:
        break;
    }
  }

  if (!flushPops(bce_)) {
    return false;
  }

  if (target && emitIteratorCloseAtTarget && target->is<ForOfLoopControl>()) {
    BytecodeOffset tryNoteStart;
    ForOfLoopControl& loopinfo = target->as<ForOfLoopControl>();
    if (!loopinfo.emitPrepareForNonLocalJumpFromScope(bce_, *es,
                                                      /* isTarget = */ true,
                                                      &tryNoteStart)) {
      //            [stack] ... UNDEF UNDEF UNDEF
      return false;
    }
    if (!forOfIterCloseScopeStarts.append(tryNoteStart)) {
      return false;
    }
  }

  EmitterScope* targetEmitterScope =
      target ? target->emitterScope() : bce_->varEmitterScope;
  for (; es != targetEmitterScope; es = es->enclosingInFrame()) {
    if (!leaveScope(es)) {
      return false;
    }
  }

  // Close FOR_OF_ITERCLOSE trynotes.
  BytecodeOffset end = bce_->bytecodeSection().offset();
  for (BytecodeOffset start : forOfIterCloseScopeStarts) {
    if (!bce_->addTryNote(JSTRY_FOR_OF_ITERCLOSE, 0, start, end)) {
      return false;
    }
  }

  return true;
}

}  // anonymous namespace

bool BytecodeEmitter::emitGoto(NestableControl* target, JumpList* jumplist,
                               SrcNoteType noteType) {
  NonLocalExitControl nle(this, noteType == SRC_CONTINUE
                                    ? NonLocalExitControl::Continue
                                    : NonLocalExitControl::Break);

  if (!nle.prepareForNonLocalJump(target)) {
    return false;
  }

  if (noteType != SRC_NULL) {
    if (!newSrcNote(noteType)) {
      return false;
    }
  }

  return emitJump(JSOP_GOTO, jumplist);
}

Scope* BytecodeEmitter::innermostScope() const {
  return innermostEmitterScope()->scope(this);
}

bool BytecodeEmitter::emitIndex32(JSOp op, uint32_t index) {
  MOZ_ASSERT(checkStrictOrSloppy(op));

  const size_t len = 1 + UINT32_INDEX_LEN;
  MOZ_ASSERT(len == size_t(CodeSpec[op].length));

  BytecodeOffset offset;
  if (!emitCheck(op, len, &offset)) {
    return false;
  }

  jsbytecode* code = bytecodeSection().code(offset);
  code[0] = jsbytecode(op);
  SET_UINT32_INDEX(code, index);
  bytecodeSection().updateDepth(offset);
  return true;
}

bool BytecodeEmitter::emitIndexOp(JSOp op, uint32_t index) {
  MOZ_ASSERT(checkStrictOrSloppy(op));

  const size_t len = CodeSpec[op].length;
  MOZ_ASSERT(len >= 1 + UINT32_INDEX_LEN);

  BytecodeOffset offset;
  if (!emitCheck(op, len, &offset)) {
    return false;
  }

  jsbytecode* code = bytecodeSection().code(offset);
  code[0] = jsbytecode(op);
  SET_UINT32_INDEX(code, index);
  bytecodeSection().updateDepth(offset);
  return true;
}

bool BytecodeEmitter::emitAtomOp(JSAtom* atom, JSOp op,
                                 ShouldInstrument shouldInstrument) {
  MOZ_ASSERT(atom);

  // .generator lookups should be emitted as JSOP_GETALIASEDVAR instead of
  // JSOP_GETNAME etc, to bypass |with| objects on the scope chain.
  // It's safe to emit .this lookups though because |with| objects skip
  // those.
  MOZ_ASSERT_IF(op == JSOP_GETNAME || op == JSOP_GETGNAME,
                atom != cx->names().dotGenerator);

  if (op == JSOP_GETPROP && atom == cx->names().length) {
    /* Specialize length accesses for the interpreter. */
    op = JSOP_LENGTH;
  }

  uint32_t index;
  if (!makeAtomIndex(atom, &index)) {
    return false;
  }

  return emitAtomOp(index, op, shouldInstrument);
}

bool BytecodeEmitter::emitAtomOp(uint32_t atomIndex, JSOp op,
                                 ShouldInstrument shouldInstrument) {
  MOZ_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);

  if (shouldInstrument != ShouldInstrument::No &&
      !emitInstrumentationForOpcode(op, atomIndex)) {
    return false;
  }

  return emitIndexOp(op, atomIndex);
}

bool BytecodeEmitter::emitInternedScopeOp(uint32_t index, JSOp op) {
  MOZ_ASSERT(JOF_OPTYPE(op) == JOF_SCOPE);
  MOZ_ASSERT(index < perScriptData().gcThingList().length());
  return emitIndex32(op, index);
}

bool BytecodeEmitter::emitInternedObjectOp(uint32_t index, JSOp op) {
  MOZ_ASSERT(JOF_OPTYPE(op) == JOF_OBJECT);
  MOZ_ASSERT(index < perScriptData().gcThingList().length());
  return emitIndex32(op, index);
}

bool BytecodeEmitter::emitObjectOp(ObjectBox* objbox, JSOp op) {
  uint32_t index;
  if (!perScriptData().gcThingList().append(objbox, &index)) {
    return false;
  }

  return emitInternedObjectOp(index, op);
}

bool BytecodeEmitter::emitObjectPairOp(ObjectBox* objbox1, ObjectBox* objbox2,
                                       JSOp op) {
  uint32_t index1, index2;
  if (!perScriptData().gcThingList().append(objbox1, &index1) ||
      !perScriptData().gcThingList().append(objbox2, &index2)) {
    return false;
  }

  return emitInternedObjectOp(index1, op);
}

bool BytecodeEmitter::emitRegExp(uint32_t index) {
  return emitIndex32(JSOP_REGEXP, index);
}

bool BytecodeEmitter::emitLocalOp(JSOp op, uint32_t slot) {
  MOZ_ASSERT(JOF_OPTYPE(op) != JOF_ENVCOORD);
  MOZ_ASSERT(IsLocalOp(op));

  BytecodeOffset off;
  if (!emitN(op, LOCALNO_LEN, &off)) {
    return false;
  }

  SET_LOCALNO(bytecodeSection().code(off), slot);
  return true;
}

bool BytecodeEmitter::emitArgOp(JSOp op, uint16_t slot) {
  MOZ_ASSERT(IsArgOp(op));
  BytecodeOffset off;
  if (!emitN(op, ARGNO_LEN, &off)) {
    return false;
  }

  SET_ARGNO(bytecodeSection().code(off), slot);
  return true;
}

bool BytecodeEmitter::emitEnvCoordOp(JSOp op, EnvironmentCoordinate ec) {
  MOZ_ASSERT(JOF_OPTYPE(op) == JOF_ENVCOORD);

  unsigned n = ENVCOORD_HOPS_LEN + ENVCOORD_SLOT_LEN;
  MOZ_ASSERT(int(n) + 1 /* op */ == CodeSpec[op].length);

  BytecodeOffset off;
  if (!emitN(op, n, &off)) {
    return false;
  }

  jsbytecode* pc = bytecodeSection().code(off);
  SET_ENVCOORD_HOPS(pc, ec.hops());
  pc += ENVCOORD_HOPS_LEN;
  SET_ENVCOORD_SLOT(pc, ec.slot());
  pc += ENVCOORD_SLOT_LEN;
  return true;
}

JSOp BytecodeEmitter::strictifySetNameOp(JSOp op) {
  switch (op) {
    case JSOP_SETNAME:
      if (sc->strict()) {
        op = JSOP_STRICTSETNAME;
      }
      break;
    case JSOP_SETGNAME:
      if (sc->strict()) {
        op = JSOP_STRICTSETGNAME;
      }
      break;
    default:;
  }
  return op;
}

bool BytecodeEmitter::checkSideEffects(ParseNode* pn, bool* answer) {
  if (!CheckRecursionLimit(cx)) {
    return false;
  }

restart:

  switch (pn->getKind()) {
    // Trivial cases with no side effects.
    case ParseNodeKind::EmptyStmt:
    case ParseNodeKind::TrueExpr:
    case ParseNodeKind::FalseExpr:
    case ParseNodeKind::NullExpr:
    case ParseNodeKind::RawUndefinedExpr:
    case ParseNodeKind::Elision:
    case ParseNodeKind::Generator:
      MOZ_ASSERT(pn->is<NullaryNode>());
      *answer = false;
      return true;

    case ParseNodeKind::ObjectPropertyName:
    case ParseNodeKind::PrivateName:  // no side effects, unlike
                                      // ParseNodeKind::Name
    case ParseNodeKind::StringExpr:
    case ParseNodeKind::TemplateStringExpr:
      MOZ_ASSERT(pn->is<NameNode>());
      *answer = false;
      return true;

    case ParseNodeKind::RegExpExpr:
      MOZ_ASSERT(pn->is<RegExpLiteral>());
      *answer = false;
      return true;

    case ParseNodeKind::NumberExpr:
      MOZ_ASSERT(pn->is<NumericLiteral>());
      *answer = false;
      return true;

    case ParseNodeKind::BigIntExpr:
      MOZ_ASSERT(pn->is<BigIntLiteral>());
      *answer = false;
      return true;

    // |this| can throw in derived class constructors, including nested arrow
    // functions or eval.
    case ParseNodeKind::ThisExpr:
      MOZ_ASSERT(pn->is<UnaryNode>());
      *answer = sc->needsThisTDZChecks();
      return true;

    // Trivial binary nodes with more token pos holders.
    case ParseNodeKind::NewTargetExpr:
    case ParseNodeKind::ImportMetaExpr: {
      MOZ_ASSERT(pn->as<BinaryNode>().left()->isKind(ParseNodeKind::PosHolder));
      MOZ_ASSERT(
          pn->as<BinaryNode>().right()->isKind(ParseNodeKind::PosHolder));
      *answer = false;
      return true;
    }

    case ParseNodeKind::BreakStmt:
      MOZ_ASSERT(pn->is<BreakStatement>());
      *answer = true;
      return true;

    case ParseNodeKind::ContinueStmt:
      MOZ_ASSERT(pn->is<ContinueStatement>());
      *answer = true;
      return true;

    case ParseNodeKind::DebuggerStmt:
      MOZ_ASSERT(pn->is<DebuggerStatement>());
      *answer = true;
      return true;

    // Watch out for getters!
    case ParseNodeKind::DotExpr:
      MOZ_ASSERT(pn->is<BinaryNode>());
      *answer = true;
      return true;

    // Unary cases with side effects only if the child has them.
    case ParseNodeKind::TypeOfExpr:
    case ParseNodeKind::VoidExpr:
    case ParseNodeKind::NotExpr:
      return checkSideEffects(pn->as<UnaryNode>().kid(), answer);

    // Even if the name expression is effect-free, performing ToPropertyKey on
    // it might not be effect-free:
    //
    //   RegExp.prototype.toString = () => { throw 42; };
    //   ({ [/regex/]: 0 }); // ToPropertyKey(/regex/) throws 42
    //
    //   function Q() {
    //     ({ [new.target]: 0 });
    //   }
    //   Q.toString = () => { throw 17; };
    //   new Q; // new.target will be Q, ToPropertyKey(Q) throws 17
    case ParseNodeKind::ComputedName:
      MOZ_ASSERT(pn->is<UnaryNode>());
      *answer = true;
      return true;

    // Looking up or evaluating the associated name could throw.
    case ParseNodeKind::TypeOfNameExpr:
      MOZ_ASSERT(pn->is<UnaryNode>());
      *answer = true;
      return true;

    // This unary case has side effects on the enclosing object, sure.  But
    // that's not the question this function answers: it's whether the
    // operation may have a side effect on something *other* than the result
    // of the overall operation in which it's embedded.  The answer to that
    // is no, because an object literal having a mutated prototype only
    // produces a value, without affecting anything else.
    case ParseNodeKind::MutateProto:
      return checkSideEffects(pn->as<UnaryNode>().kid(), answer);

    // Unary cases with obvious side effects.
    case ParseNodeKind::PreIncrementExpr:
    case ParseNodeKind::PostIncrementExpr:
    case ParseNodeKind::PreDecrementExpr:
    case ParseNodeKind::PostDecrementExpr:
    case ParseNodeKind::ThrowStmt:
      MOZ_ASSERT(pn->is<UnaryNode>());
      *answer = true;
      return true;

    // These might invoke valueOf/toString, even with a subexpression without
    // side effects!  Consider |+{ valueOf: null, toString: null }|.
    case ParseNodeKind::BitNotExpr:
    case ParseNodeKind::PosExpr:
    case ParseNodeKind::NegExpr:
      MOZ_ASSERT(pn->is<UnaryNode>());
      *answer = true;
      return true;

    // This invokes the (user-controllable) iterator protocol.
    case ParseNodeKind::Spread:
      MOZ_ASSERT(pn->is<UnaryNode>());
      *answer = true;
      return true;

    case ParseNodeKind::InitialYield:
    case ParseNodeKind::YieldStarExpr:
    case ParseNodeKind::YieldExpr:
    case ParseNodeKind::AwaitExpr:
      MOZ_ASSERT(pn->is<UnaryNode>());
      *answer = true;
      return true;

    // Deletion generally has side effects, even if isolated cases have none.
    case ParseNodeKind::DeleteNameExpr:
    case ParseNodeKind::DeletePropExpr:
    case ParseNodeKind::DeleteElemExpr:
      MOZ_ASSERT(pn->is<UnaryNode>());
      *answer = true;
      return true;

    // Deletion of a non-Reference expression has side effects only through
    // evaluating the expression.
    case ParseNodeKind::DeleteExpr: {
      ParseNode* expr = pn->as<UnaryNode>().kid();
      return checkSideEffects(expr, answer);
    }

    case ParseNodeKind::ExpressionStmt:
      return checkSideEffects(pn->as<UnaryNode>().kid(), answer);

    // Binary cases with obvious side effects.
    case ParseNodeKind::InitExpr:
      *answer = true;
      return true;

    case ParseNodeKind::AssignExpr:
    case ParseNodeKind::AddAssignExpr:
    case ParseNodeKind::SubAssignExpr:
    case ParseNodeKind::BitOrAssignExpr:
    case ParseNodeKind::BitXorAssignExpr:
    case ParseNodeKind::BitAndAssignExpr:
    case ParseNodeKind::LshAssignExpr:
    case ParseNodeKind::RshAssignExpr:
    case ParseNodeKind::UrshAssignExpr:
    case ParseNodeKind::MulAssignExpr:
    case ParseNodeKind::DivAssignExpr:
    case ParseNodeKind::ModAssignExpr:
    case ParseNodeKind::PowAssignExpr:
      MOZ_ASSERT(pn->is<AssignmentNode>());
      *answer = true;
      return true;

    case ParseNodeKind::SetThis:
      MOZ_ASSERT(pn->is<BinaryNode>());
      *answer = true;
      return true;

    case ParseNodeKind::StatementList:
    // Strict equality operations and logical operators are well-behaved and
    // perform no conversions.
    case ParseNodeKind::OrExpr:
    case ParseNodeKind::AndExpr:
    case ParseNodeKind::StrictEqExpr:
    case ParseNodeKind::StrictNeExpr:
    // Any subexpression of a comma expression could be effectful.
    case ParseNodeKind::CommaExpr:
      MOZ_ASSERT(!pn->as<ListNode>().empty());
      MOZ_FALLTHROUGH;
    // Subcomponents of a literal may be effectful.
    case ParseNodeKind::ArrayExpr:
    case ParseNodeKind::ObjectExpr:
      for (ParseNode* item : pn->as<ListNode>().contents()) {
        if (!checkSideEffects(item, answer)) {
          return false;
        }
        if (*answer) {
          return true;
        }
      }
      return true;

    // Most other binary operations (parsed as lists in SpiderMonkey) may
    // perform conversions triggering side effects.  Math operations perform
    // ToNumber and may fail invoking invalid user-defined toString/valueOf:
    // |5 < { toString: null }|.  |instanceof| throws if provided a
    // non-object constructor: |null instanceof null|.  |in| throws if given
    // a non-object RHS: |5 in null|.
    case ParseNodeKind::BitOrExpr:
    case ParseNodeKind::BitXorExpr:
    case ParseNodeKind::BitAndExpr:
    case ParseNodeKind::EqExpr:
    case ParseNodeKind::NeExpr:
    case ParseNodeKind::LtExpr:
    case ParseNodeKind::LeExpr:
    case ParseNodeKind::GtExpr:
    case ParseNodeKind::GeExpr:
    case ParseNodeKind::InstanceOfExpr:
    case ParseNodeKind::InExpr:
    case ParseNodeKind::LshExpr:
    case ParseNodeKind::RshExpr:
    case ParseNodeKind::UrshExpr:
    case ParseNodeKind::AddExpr:
    case ParseNodeKind::SubExpr:
    case ParseNodeKind::MulExpr:
    case ParseNodeKind::DivExpr:
    case ParseNodeKind::ModExpr:
    case ParseNodeKind::PowExpr:
      MOZ_ASSERT(pn->as<ListNode>().count() >= 2);
      *answer = true;
      return true;

    case ParseNodeKind::PropertyDefinition:
    case ParseNodeKind::Case: {
      BinaryNode* node = &pn->as<BinaryNode>();
      if (!checkSideEffects(node->left(), answer)) {
        return false;
      }
      if (*answer) {
        return true;
      }
      return checkSideEffects(node->right(), answer);
    }

    // More getters.
    case ParseNodeKind::ElemExpr:
      MOZ_ASSERT(pn->is<BinaryNode>());
      *answer = true;
      return true;

    // These affect visible names in this code, or in other code.
    case ParseNodeKind::ImportDecl:
    case ParseNodeKind::ExportFromStmt:
    case ParseNodeKind::ExportDefaultStmt:
      MOZ_ASSERT(pn->is<BinaryNode>());
      *answer = true;
      return true;

    // Likewise.
    case ParseNodeKind::ExportStmt:
      MOZ_ASSERT(pn->is<UnaryNode>());
      *answer = true;
      return true;

    case ParseNodeKind::CallImportExpr:
      MOZ_ASSERT(pn->is<BinaryNode>());
      *answer = true;
      return true;

    // Every part of a loop might be effect-free, but looping infinitely *is*
    // an effect.  (Language lawyer trivia: C++ says threads can be assumed
    // to exit or have side effects, C++14 [intro.multithread]p27, so a C++
    // implementation's equivalent of the below could set |*answer = false;|
    // if all loop sub-nodes set |*answer = false|!)
    case ParseNodeKind::DoWhileStmt:
    case ParseNodeKind::WhileStmt:
    case ParseNodeKind::ForStmt:
      MOZ_ASSERT(pn->is<BinaryNode>());
      *answer = true;
      return true;

    // Declarations affect the name set of the relevant scope.
    case ParseNodeKind::VarStmt:
    case ParseNodeKind::ConstDecl:
    case ParseNodeKind::LetDecl:
      MOZ_ASSERT(pn->is<ListNode>());
      *answer = true;
      return true;

    case ParseNodeKind::IfStmt:
    case ParseNodeKind::ConditionalExpr: {
      TernaryNode* node = &pn->as<TernaryNode>();
      if (!checkSideEffects(node->kid1(), answer)) {
        return false;
      }
      if (*answer) {
        return true;
      }
      if (!checkSideEffects(node->kid2(), answer)) {
        return false;
      }
      if (*answer) {
        return true;
      }
      if ((pn = node->kid3())) {
        goto restart;
      }
      return true;
    }

    // Function calls can invoke non-local code.
    case ParseNodeKind::NewExpr:
    case ParseNodeKind::CallExpr:
    case ParseNodeKind::TaggedTemplateExpr:
    case ParseNodeKind::SuperCallExpr:
      MOZ_ASSERT(pn->is<BinaryNode>());
      *answer = true;
      return true;

    // Function arg lists can contain arbitrary expressions. Technically
    // this only causes side-effects if one of the arguments does, but since
    // the call being made will always trigger side-effects, it isn't needed.
    case ParseNodeKind::Arguments:
      MOZ_ASSERT(pn->is<ListNode>());
      *answer = true;
      return true;

    case ParseNodeKind::PipelineExpr:
      MOZ_ASSERT(pn->as<ListNode>().count() >= 2);
      *answer = true;
      return true;

    // Classes typically introduce names.  Even if no name is introduced,
    // the heritage and/or class body (through computed property names)
    // usually have effects.
    case ParseNodeKind::ClassDecl:
      MOZ_ASSERT(pn->is<ClassNode>());
      *answer = true;
      return true;

    // |with| calls |ToObject| on its expression and so throws if that value
    // is null/undefined.
    case ParseNodeKind::WithStmt:
      MOZ_ASSERT(pn->is<BinaryNode>());
      *answer = true;
      return true;

    case ParseNodeKind::ReturnStmt:
      MOZ_ASSERT(pn->is<BinaryNode>());
      *answer = true;
      return true;

    case ParseNodeKind::Name:
      MOZ_ASSERT(pn->is<NameNode>());
      *answer = true;
      return true;

    // Shorthands could trigger getters: the |x| in the object literal in
    // |with ({ get x() { throw 42; } }) ({ x });|, for example, triggers
    // one.  (Of course, it isn't necessary to use |with| for a shorthand to
    // trigger a getter.)
    case ParseNodeKind::Shorthand:
      MOZ_ASSERT(pn->is<BinaryNode>());
      *answer = true;
      return true;

    case ParseNodeKind::Function:
      MOZ_ASSERT(pn->is<FunctionNode>());
      /*
       * A named function, contrary to ES3, is no longer effectful, because
       * we bind its name lexically (using JSOP_CALLEE) instead of creating
       * an Object instance and binding a readonly, permanent property in it
       * (the object and binding can be detected and hijacked or captured).
       * This is a bug fix to ES3; it is fixed in ES3.1 drafts.
       */
      *answer = false;
      return true;

    case ParseNodeKind::Module:
      *answer = false;
      return true;

    case ParseNodeKind::TryStmt: {
      TryNode* tryNode = &pn->as<TryNode>();
      if (!checkSideEffects(tryNode->body(), answer)) {
        return false;
      }
      if (*answer) {
        return true;
      }
      if (LexicalScopeNode* catchScope = tryNode->catchScope()) {
        if (!checkSideEffects(catchScope, answer)) {
          return false;
        }
        if (*answer) {
          return true;
        }
      }
      if (ParseNode* finallyBlock = tryNode->finallyBlock()) {
        if (!checkSideEffects(finallyBlock, answer)) {
          return false;
        }
      }
      return true;
    }

    case ParseNodeKind::Catch: {
      BinaryNode* catchClause = &pn->as<BinaryNode>();
      if (ParseNode* name = catchClause->left()) {
        if (!checkSideEffects(name, answer)) {
          return false;
        }
        if (*answer) {
          return true;
        }
      }
      return checkSideEffects(catchClause->right(), answer);
    }

    case ParseNodeKind::SwitchStmt: {
      SwitchStatement* switchStmt = &pn->as<SwitchStatement>();
      if (!checkSideEffects(&switchStmt->discriminant(), answer)) {
        return false;
      }
      return *answer ||
             checkSideEffects(&switchStmt->lexicalForCaseList(), answer);
    }

    case ParseNodeKind::LabelStmt:
      return checkSideEffects(pn->as<LabeledStatement>().statement(), answer);

    case ParseNodeKind::LexicalScope:
      return checkSideEffects(pn->as<LexicalScopeNode>().scopeBody(), answer);

    // We could methodically check every interpolated expression, but it's
    // probably not worth the trouble.  Treat template strings as effect-free
    // only if they don't contain any substitutions.
    case ParseNodeKind::TemplateStringListExpr: {
      ListNode* list = &pn->as<ListNode>();
      MOZ_ASSERT(!list->empty());
      MOZ_ASSERT((list->count() % 2) == 1,
                 "template strings must alternate template and substitution "
                 "parts");
      *answer = list->count() > 1;
      return true;
    }

    // This should be unreachable but is left as-is for now.
    case ParseNodeKind::ParamsBody:
      *answer = true;
      return true;

    case ParseNodeKind::ForIn:                // by ParseNodeKind::For
    case ParseNodeKind::ForOf:                // by ParseNodeKind::For
    case ParseNodeKind::ForHead:              // by ParseNodeKind::For
    case ParseNodeKind::ClassMethod:          // by ParseNodeKind::ClassDecl
    case ParseNodeKind::ClassField:           // by ParseNodeKind::ClassDecl
    case ParseNodeKind::ClassNames:           // by ParseNodeKind::ClassDecl
    case ParseNodeKind::ClassMemberList:      // by ParseNodeKind::ClassDecl
    case ParseNodeKind::ImportSpecList:       // by ParseNodeKind::Import
    case ParseNodeKind::ImportSpec:           // by ParseNodeKind::Import
    case ParseNodeKind::ExportBatchSpecStmt:  // by ParseNodeKind::Export
    case ParseNodeKind::ExportSpecList:       // by ParseNodeKind::Export
    case ParseNodeKind::ExportSpec:           // by ParseNodeKind::Export
    case ParseNodeKind::CallSiteObj:       // by ParseNodeKind::TaggedTemplate
    case ParseNodeKind::PosHolder:         // by ParseNodeKind::NewTarget
    case ParseNodeKind::SuperBase:         // by ParseNodeKind::Elem and others
    case ParseNodeKind::PropertyNameExpr:  // by ParseNodeKind::Dot
      MOZ_CRASH("handled by parent nodes");

    case ParseNodeKind::LastUnused:
    case ParseNodeKind::Limit:
      MOZ_CRASH("invalid node kind");
  }

  MOZ_CRASH(
      "invalid, unenumerated ParseNodeKind value encountered in "
      "BytecodeEmitter::checkSideEffects");
}

bool BytecodeEmitter::isInLoop() {
  return findInnermostNestableControl<LoopControl>();
}

bool BytecodeEmitter::checkSingletonContext() {
  return script->treatAsRunOnce() && !sc->isFunctionBox() && !isInLoop();
}

bool BytecodeEmitter::checkRunOnceContext() {
  return checkSingletonContext() || (!isInLoop() && isRunOnceLambda());
}

bool BytecodeEmitter::needsImplicitThis() {
  // Short-circuit if there is an enclosing 'with' scope.
  if (sc->inWith()) {
    return true;
  }

  // Otherwise see if the current point is under a 'with'.
  for (EmitterScope* es = innermostEmitterScope(); es;
       es = es->enclosingInFrame()) {
    if (es->scope(this)->kind() == ScopeKind::With) {
      return true;
    }
  }

  return false;
}

bool BytecodeEmitter::emitThisEnvironmentCallee() {
  // Get the innermost enclosing function that has a |this| binding.

  // Directly load callee from the frame if possible.
  if (sc->isFunctionBox() && !sc->asFunctionBox()->isArrow()) {
    return emit1(JSOP_CALLEE);
  }

  // We have to load the callee from the environment chain.
  unsigned numHops = 0;
  for (ScopeIter si(innermostScope()); si; si++) {
    if (si.hasSyntacticEnvironment() && si.scope()->is<FunctionScope>()) {
      JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();
      if (!fun->isArrow()) {
        break;
      }
    }
    if (si.scope()->hasEnvironment()) {
      numHops++;
    }
  }

  static_assert(ENVCOORD_HOPS_LIMIT - 1 <= UINT8_MAX,
                "JSOP_ENVCALLEE operand size should match ENVCOORD_HOPS_LIMIT");

  // Note: we need to check numHops here because we don't call
  // checkEnvironmentChainLength in all cases (like 'eval').
  if (numHops >= ENVCOORD_HOPS_LIMIT - 1) {
    reportError(nullptr, JSMSG_TOO_DEEP, js_function_str);
    return false;
  }

  return emit2(JSOP_ENVCALLEE, numHops);
}

bool BytecodeEmitter::emitSuperBase() {
  if (!emitThisEnvironmentCallee()) {
    return false;
  }

  return emit1(JSOP_SUPERBASE);
}

void BytecodeEmitter::tellDebuggerAboutCompiledScript(JSContext* cx) {
  // Note: when parsing off thread the resulting scripts need to be handed to
  // the debugger after rejoining to the main thread.
  if (cx->isHelperThreadContext()) {
    return;
  }

  // Lazy scripts are never top level (despite always being invoked with a
  // nullptr parent), and so the hook should never be fired.
  if (emitterMode != LazyFunction && !parent) {
    DebugAPI::onNewScript(cx, script);
  }
}

void BytecodeEmitter::reportNeedMoreArgsError(ParseNode* pn,
                                              const char* errorName,
                                              const char* requiredArgs,
                                              const char* pluralizer,
                                              const ListNode* argsList) {
  char actualArgsStr[40];
  SprintfLiteral(actualArgsStr, "%u", argsList->count());
  reportError(pn, JSMSG_MORE_ARGS_NEEDED, errorName, requiredArgs, pluralizer,
              actualArgsStr);
}

void BytecodeEmitter::reportError(ParseNode* pn, unsigned errorNumber, ...) {
  uint32_t offset = pn ? pn->pn_pos.begin : *scriptStartOffset;

  va_list args;
  va_start(args, errorNumber);

  parser->errorReporter().errorWithNotesAtVA(nullptr, AsVariant(offset),
                                             errorNumber, &args);

  va_end(args);
}

void BytecodeEmitter::reportError(const Maybe<uint32_t>& maybeOffset,
                                  unsigned errorNumber, ...) {
  uint32_t offset = maybeOffset ? *maybeOffset : *scriptStartOffset;

  va_list args;
  va_start(args, errorNumber);

  parser->errorReporter().errorWithNotesAtVA(nullptr, AsVariant(offset),
                                             errorNumber, &args);

  va_end(args);
}

bool BytecodeEmitter::reportExtraWarning(ParseNode* pn, unsigned errorNumber,
                                         ...) {
  uint32_t offset = pn ? pn->pn_pos.begin : *scriptStartOffset;

  va_list args;
  va_start(args, errorNumber);

  bool result = parser->errorReporter().extraWarningWithNotesAtVA(
      nullptr, AsVariant(offset), errorNumber, &args);

  va_end(args);
  return result;
}

bool BytecodeEmitter::reportExtraWarning(const Maybe<uint32_t>& maybeOffset,
                                         unsigned errorNumber, ...) {
  uint32_t offset = maybeOffset ? *maybeOffset : *scriptStartOffset;

  va_list args;
  va_start(args, errorNumber);

  bool result = parser->errorReporter().extraWarningWithNotesAtVA(
      nullptr, AsVariant(offset), errorNumber, &args);

  va_end(args);
  return result;
}

bool BytecodeEmitter::emitNewInit() {
  const size_t len = 1 + UINT32_INDEX_LEN;
  BytecodeOffset offset;
  if (!emitCheck(JSOP_NEWINIT, len, &offset)) {
    return false;
  }

  jsbytecode* code = bytecodeSection().code(offset);
  code[0] = JSOP_NEWINIT;
  code[1] = 0;
  code[2] = 0;
  code[3] = 0;
  code[4] = 0;
  bytecodeSection().updateDepth(offset);
  return true;
}

bool BytecodeEmitter::iteratorResultShape(uint32_t* shape) {
  // No need to do any guessing for the object kind, since we know exactly how
  // many properties we plan to have.
  gc::AllocKind kind = gc::GetGCObjectKind(2);
  RootedPlainObject obj(
      cx, NewBuiltinClassInstance<PlainObject>(cx, kind, TenuredObject));
  if (!obj) {
    return false;
  }

  Rooted<jsid> value_id(cx, NameToId(cx->names().value));
  Rooted<jsid> done_id(cx, NameToId(cx->names().done));
  if (!NativeDefineDataProperty(cx, obj, value_id, UndefinedHandleValue,
                                JSPROP_ENUMERATE)) {
    return false;
  }
  if (!NativeDefineDataProperty(cx, obj, done_id, UndefinedHandleValue,
                                JSPROP_ENUMERATE)) {
    return false;
  }

  ObjectBox* objbox = parser->newObjectBox(obj);
  if (!objbox) {
    return false;
  }

  return perScriptData().gcThingList().append(objbox, shape);
}

bool BytecodeEmitter::emitPrepareIteratorResult() {
  uint32_t shape;
  if (!iteratorResultShape(&shape)) {
    return false;
  }
  return emitIndex32(JSOP_NEWOBJECT, shape);
}

bool BytecodeEmitter::emitFinishIteratorResult(bool done) {
  uint32_t value_id;
  if (!makeAtomIndex(cx->names().value, &value_id)) {
    return false;
  }
  uint32_t done_id;
  if (!makeAtomIndex(cx->names().done, &done_id)) {
    return false;
  }

  if (!emitIndex32(JSOP_INITPROP, value_id)) {
    return false;
  }
  if (!emit1(done ? JSOP_TRUE : JSOP_FALSE)) {
    return false;
  }
  if (!emitIndex32(JSOP_INITPROP, done_id)) {
    return false;
  }
  return true;
}

bool BytecodeEmitter::emitGetNameAtLocation(Handle<JSAtom*> name,
                                            const NameLocation& loc) {
  NameOpEmitter noe(this, name, loc, NameOpEmitter::Kind::Get);
  if (!noe.emitGet()) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitGetName(NameNode* name) {
  RootedAtom nameAtom(cx, name->name());
  return emitGetName(nameAtom);
}

bool BytecodeEmitter::emitTDZCheckIfNeeded(HandleAtom name,
                                           const NameLocation& loc) {
  // Dynamic accesses have TDZ checks built into their VM code and should
  // never emit explicit TDZ checks.
  MOZ_ASSERT(loc.hasKnownSlot());
  MOZ_ASSERT(loc.isLexical());

  Maybe<MaybeCheckTDZ> check =
      innermostTDZCheckCache->needsTDZCheck(this, name);
  if (!check) {
    return false;
  }

  // We've already emitted a check in this basic block.
  if (*check == DontCheckTDZ) {
    return true;
  }

  if (loc.kind() == NameLocation::Kind::FrameSlot) {
    if (!emitLocalOp(JSOP_CHECKLEXICAL, loc.frameSlot())) {
      return false;
    }
  } else {
    if (!emitEnvCoordOp(JSOP_CHECKALIASEDLEXICAL,
                        loc.environmentCoordinate())) {
      return false;
    }
  }

  return innermostTDZCheckCache->noteTDZCheck(this, name, DontCheckTDZ);
}

bool BytecodeEmitter::emitPropLHS(PropertyAccess* prop) {
  MOZ_ASSERT(!prop->isSuper());

  ParseNode* expr = &prop->expression();

  if (!expr->is<PropertyAccess>() || expr->as<PropertyAccess>().isSuper()) {
    // The non-optimized case.
    return emitTree(expr);
  }

  // If the object operand is also a dotted property reference, reverse the
  // list linked via expression() temporarily so we can iterate over it from
  // the bottom up (reversing again as we go), to avoid excessive recursion.
  PropertyAccess* pndot = &expr->as<PropertyAccess>();
  ParseNode* pnup = nullptr;
  ParseNode* pndown;
  for (;;) {
    // Reverse pndot->expression() to point up, not down.
    pndown = &pndot->expression();
    pndot->setExpression(pnup);
    if (!pndown->is<PropertyAccess>() ||
        pndown->as<PropertyAccess>().isSuper()) {
      break;
    }
    pnup = pndot;
    pndot = &pndown->as<PropertyAccess>();
  }

  // pndown is a primary expression, not a dotted property reference.
  if (!emitTree(pndown)) {
    return false;
  }

  while (true) {
    // Walk back up the list, emitting annotated name ops.
    if (!emitAtomOp(pndot->key().atom(), JSOP_GETPROP, ShouldInstrument::Yes)) {
      return false;
    }

    // Reverse the pndot->expression() link again.
    pnup = pndot->maybeExpression();
    pndot->setExpression(pndown);
    pndown = pndot;
    if (!pnup) {
      break;
    }
    pndot = &pnup->as<PropertyAccess>();
  }
  return true;
}

bool BytecodeEmitter::emitPropIncDec(UnaryNode* incDec) {
  PropertyAccess* prop = &incDec->kid()->as<PropertyAccess>();
  bool isSuper = prop->isSuper();
  ParseNodeKind kind = incDec->getKind();
  PropOpEmitter poe(
      this,
      kind == ParseNodeKind::PostIncrementExpr
          ? PropOpEmitter::Kind::PostIncrement
          : kind == ParseNodeKind::PreIncrementExpr
                ? PropOpEmitter::Kind::PreIncrement
                : kind == ParseNodeKind::PostDecrementExpr
                      ? PropOpEmitter::Kind::PostDecrement
                      : PropOpEmitter::Kind::PreDecrement,
      isSuper ? PropOpEmitter::ObjKind::Super : PropOpEmitter::ObjKind::Other);
  if (!poe.prepareForObj()) {
    return false;
  }
  if (isSuper) {
    UnaryNode* base = &prop->expression().as<UnaryNode>();
    if (!emitGetThisForSuperBase(base)) {
      //            [stack] THIS
      return false;
    }
  } else {
    if (!emitPropLHS(prop)) {
      //            [stack] OBJ
      return false;
    }
  }
  if (!poe.emitIncDec(prop->key().atom())) {
    //              [stack] RESULT
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitNameIncDec(UnaryNode* incDec) {
  MOZ_ASSERT(incDec->kid()->isKind(ParseNodeKind::Name));

  ParseNodeKind kind = incDec->getKind();
  NameNode* name = &incDec->kid()->as<NameNode>();
  RootedAtom nameAtom(cx, name->atom());
  NameOpEmitter noe(this, nameAtom,
                    kind == ParseNodeKind::PostIncrementExpr
                        ? NameOpEmitter::Kind::PostIncrement
                        : kind == ParseNodeKind::PreIncrementExpr
                              ? NameOpEmitter::Kind::PreIncrement
                              : kind == ParseNodeKind::PostDecrementExpr
                                    ? NameOpEmitter::Kind::PostDecrement
                                    : NameOpEmitter::Kind::PreDecrement);
  if (!noe.emitIncDec()) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitElemOpBase(JSOp op,
                                     ShouldInstrument shouldInstrument) {
  if (shouldInstrument != ShouldInstrument::No &&
      !emitInstrumentationForOpcode(op, 0)) {
    return false;
  }

  if (!emit1(op)) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitElemObjAndKey(PropertyByValue* elem, bool isSuper,
                                        ElemOpEmitter& eoe) {
  if (isSuper) {
    if (!eoe.prepareForObj()) {
      //            [stack]
      return false;
    }
    UnaryNode* base = &elem->expression().as<UnaryNode>();
    if (!emitGetThisForSuperBase(base)) {
      //            [stack] THIS
      return false;
    }
    if (!eoe.prepareForKey()) {
      //            [stack] THIS
      return false;
    }
    if (!emitTree(&elem->key())) {
      //            [stack] THIS KEY
      return false;
    }

    return true;
  }

  if (!eoe.prepareForObj()) {
    //              [stack]
    return false;
  }
  if (!emitTree(&elem->expression())) {
    //              [stack] OBJ
    return false;
  }
  if (!eoe.prepareForKey()) {
    //              [stack] OBJ? OBJ
    return false;
  }
  if (!emitTree(&elem->key())) {
    //              [stack] OBJ? OBJ KEY
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitElemIncDec(UnaryNode* incDec) {
  PropertyByValue* elemExpr = &incDec->kid()->as<PropertyByValue>();
  bool isSuper = elemExpr->isSuper();
  ParseNodeKind kind = incDec->getKind();
  ElemOpEmitter eoe(
      this,
      kind == ParseNodeKind::PostIncrementExpr
          ? ElemOpEmitter::Kind::PostIncrement
          : kind == ParseNodeKind::PreIncrementExpr
                ? ElemOpEmitter::Kind::PreIncrement
                : kind == ParseNodeKind::PostDecrementExpr
                      ? ElemOpEmitter::Kind::PostDecrement
                      : ElemOpEmitter::Kind::PreDecrement,
      isSuper ? ElemOpEmitter::ObjKind::Super : ElemOpEmitter::ObjKind::Other);
  if (!emitElemObjAndKey(elemExpr, isSuper, eoe)) {
    //              [stack] # if Super
    //              [stack] THIS KEY
    //              [stack] # otherwise
    //              [stack] OBJ KEY
    return false;
  }
  if (!eoe.emitIncDec()) {
    //              [stack] RESULT
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitCallIncDec(UnaryNode* incDec) {
  MOZ_ASSERT(incDec->isKind(ParseNodeKind::PreIncrementExpr) ||
             incDec->isKind(ParseNodeKind::PostIncrementExpr) ||
             incDec->isKind(ParseNodeKind::PreDecrementExpr) ||
             incDec->isKind(ParseNodeKind::PostDecrementExpr));

  ParseNode* call = incDec->kid();
  MOZ_ASSERT(call->isKind(ParseNodeKind::CallExpr));
  if (!emitTree(call)) {
    //              [stack] CALLRESULT
    return false;
  }
  if (!emit1(JSOP_TONUMERIC)) {
    //              [stack] N
    return false;
  }

  // The increment/decrement has no side effects, so proceed to throw for
  // invalid assignment target.
  return emitUint16Operand(JSOP_THROWMSG, JSMSG_ASSIGN_TO_CALL);
}

bool BytecodeEmitter::emitDouble(double d) {
  BytecodeOffset offset;
  if (!emitCheck(JSOP_DOUBLE, 9, &offset)) {
    return false;
  }

  jsbytecode* code = bytecodeSection().code(offset);
  code[0] = jsbytecode(JSOP_DOUBLE);
  SET_INLINE_VALUE(code, DoubleValue(d));
  bytecodeSection().updateDepth(offset);
  return true;
}

bool BytecodeEmitter::emitNumberOp(double dval) {
  int32_t ival;
  if (NumberIsInt32(dval, &ival)) {
    if (ival == 0) {
      return emit1(JSOP_ZERO);
    }
    if (ival == 1) {
      return emit1(JSOP_ONE);
    }
    if ((int)(int8_t)ival == ival) {
      return emit2(JSOP_INT8, uint8_t(int8_t(ival)));
    }

    uint32_t u = uint32_t(ival);
    if (u < JS_BIT(16)) {
      if (!emitUint16Operand(JSOP_UINT16, u)) {
        return false;
      }
    } else if (u < JS_BIT(24)) {
      BytecodeOffset off;
      if (!emitN(JSOP_UINT24, 3, &off)) {
        return false;
      }
      SET_UINT24(bytecodeSection().code(off), u);
    } else {
      BytecodeOffset off;
      if (!emitN(JSOP_INT32, 4, &off)) {
        return false;
      }
      SET_INT32(bytecodeSection().code(off), ival);
    }
    return true;
  }

  return emitDouble(dval);
}

/*
 * Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047.
 * LLVM is deciding to inline this function which uses a lot of stack space
 * into emitTree which is recursive and uses relatively little stack space.
 */
MOZ_NEVER_INLINE bool BytecodeEmitter::emitSwitch(SwitchStatement* switchStmt) {
  LexicalScopeNode& lexical = switchStmt->lexicalForCaseList();
  MOZ_ASSERT(lexical.isKind(ParseNodeKind::LexicalScope));
  ListNode* cases = &lexical.scopeBody()->as<ListNode>();
  MOZ_ASSERT(cases->isKind(ParseNodeKind::StatementList));

  SwitchEmitter se(this);
  if (!se.emitDiscriminant(Some(switchStmt->discriminant().pn_pos.begin))) {
    return false;
  }

  if (!markStepBreakpoint()) {
    return false;
  }
  if (!emitTree(&switchStmt->discriminant())) {
    return false;
  }

  // Enter the scope before pushing the switch BreakableControl since all
  // breaks are under this scope.

  if (!lexical.isEmptyScope()) {
    if (!se.emitLexical(lexical.scopeBindings())) {
      return false;
    }

    // A switch statement may contain hoisted functions inside its
    // cases. The PNX_FUNCDEFS flag is propagated from the STATEMENTLIST
    // bodies of the cases to the case list.
    if (cases->hasTopLevelFunctionDeclarations()) {
      for (ParseNode* item : cases->contents()) {
        CaseClause* caseClause = &item->as<CaseClause>();
        ListNode* statements = caseClause->statementList();
        if (statements->hasTopLevelFunctionDeclarations()) {
          if (!emitHoistedFunctionsInList(statements)) {
            return false;
          }
        }
      }
    }
  } else {
    MOZ_ASSERT(!cases->hasTopLevelFunctionDeclarations());
  }

  SwitchEmitter::TableGenerator tableGen(this);
  uint32_t caseCount = cases->count() - (switchStmt->hasDefault() ? 1 : 0);
  if (caseCount == 0) {
    tableGen.finish(0);
  } else {
    for (ParseNode* item : cases->contents()) {
      CaseClause* caseClause = &item->as<CaseClause>();
      if (caseClause->isDefault()) {
        continue;
      }

      ParseNode* caseValue = caseClause->caseExpression();

      if (caseValue->getKind() != ParseNodeKind::NumberExpr) {
        tableGen.setInvalid();
        break;
      }

      int32_t i;
      if (!NumberEqualsInt32(caseValue->as<NumericLiteral>().value(), &i)) {
        tableGen.setInvalid();
        break;
      }

      if (!tableGen.addNumber(i)) {
        return false;
      }
    }

    tableGen.finish(caseCount);
  }

  if (!se.validateCaseCount(caseCount)) {
    return false;
  }

  bool isTableSwitch = tableGen.isValid();
  if (isTableSwitch) {
    if (!se.emitTable(tableGen)) {
      return false;
    }
  } else {
    if (!se.emitCond()) {
      return false;
    }

    // Emit code for evaluating cases and jumping to case statements.
    for (ParseNode* item : cases->contents()) {
      CaseClause* caseClause = &item->as<CaseClause>();
      if (caseClause->isDefault()) {
        continue;
      }

      if (!se.prepareForCaseValue()) {
        return false;
      }

      ParseNode* caseValue = caseClause->caseExpression();
      // If the expression is a literal, suppress line number emission so
      // that debugging works more naturally.
      if (!emitTree(
              caseValue, ValueUsage::WantValue,
              caseValue->isLiteral() ? SUPPRESS_LINENOTE : EMIT_LINENOTE)) {
        return false;
      }

      if (!se.emitCaseJump()) {
        return false;
      }
    }
  }

  // Emit code for each case's statements.
  for (ParseNode* item : cases->contents()) {
    CaseClause* caseClause = &item->as<CaseClause>();
    if (caseClause->isDefault()) {
      if (!se.emitDefaultBody()) {
        return false;
      }
    } else {
      if (isTableSwitch) {
        ParseNode* caseValue = caseClause->caseExpression();
        MOZ_ASSERT(caseValue->isKind(ParseNodeKind::NumberExpr));

        NumericLiteral* literal = &caseValue->as<NumericLiteral>();
#ifdef DEBUG
        // Use NumberEqualsInt32 here because switches compare using
        // strict equality, which will equate -0 and +0.  In contrast
        // NumberIsInt32 would return false for -0.
        int32_t v;
        MOZ_ASSERT(mozilla::NumberEqualsInt32(literal->value(), &v));
#endif
        int32_t i = int32_t(literal->value());

        if (!se.emitCaseBody(i, tableGen)) {
          return false;
        }
      } else {
        if (!se.emitCaseBody()) {
          return false;
        }
      }
    }

    if (!emitTree(caseClause->statementList())) {
      return false;
    }
  }

  if (!se.emitEnd()) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::isRunOnceLambda() {
  if (lazyScript) {
    // NOTE: The TreatAsRunOnce flag on LazyScript was computed without a
    // complete 'funbox' so we must compute the shouldSuppressRunOnce
    // conditions now that we have the full parse info.
    return lazyScript->treatAsRunOnce() &&
           !sc->asFunctionBox()->shouldSuppressRunOnce();
  }

  return parent && parent->emittingRunOnceLambda &&
         !sc->asFunctionBox()->shouldSuppressRunOnce();
}

bool BytecodeEmitter::allocateResumeIndex(BytecodeOffset offset,
                                          uint32_t* resumeIndex) {
  static constexpr uint32_t MaxResumeIndex = JS_BITMASK(24);

  static_assert(
      MaxResumeIndex < uint32_t(AbstractGeneratorObject::RESUME_INDEX_RUNNING),
      "resumeIndex should not include magic AbstractGeneratorObject "
      "resumeIndex values");
  static_assert(
      MaxResumeIndex <= INT32_MAX / sizeof(uintptr_t),
      "resumeIndex * sizeof(uintptr_t) must fit in an int32. JIT code relies "
      "on this when loading resume entries from BaselineScript");

  *resumeIndex = bytecodeSection().resumeOffsetList().length();
  if (*resumeIndex > MaxResumeIndex) {
    reportError(nullptr, JSMSG_TOO_MANY_RESUME_INDEXES);
    return false;
  }

  return bytecodeSection().resumeOffsetList().append(offset.value());
}

bool BytecodeEmitter::allocateResumeIndexRange(
    mozilla::Span<BytecodeOffset> offsets, uint32_t* firstResumeIndex) {
  *firstResumeIndex = 0;

  for (size_t i = 0, len = offsets.size(); i < len; i++) {
    uint32_t resumeIndex;
    if (!allocateResumeIndex(offsets[i], &resumeIndex)) {
      return false;
    }
    if (i == 0) {
      *firstResumeIndex = resumeIndex;
    }
  }

  return true;
}

bool BytecodeEmitter::emitYieldOp(JSOp op) {
  // All yield operations pop or suspend the current frame.
  if (!emitInstrumentation(InstrumentationKind::Exit)) {
    return false;
  }

  if (op == JSOP_FINALYIELDRVAL) {
    return emit1(JSOP_FINALYIELDRVAL);
  }

  MOZ_ASSERT(op == JSOP_INITIALYIELD || op == JSOP_YIELD || op == JSOP_AWAIT);

  BytecodeOffset off;
  if (!emitN(op, 3, &off)) {
    return false;
  }

  if (op == JSOP_INITIALYIELD || op == JSOP_YIELD) {
    bytecodeSection().addNumYields();
  }

  uint32_t resumeIndex;
  if (!allocateResumeIndex(bytecodeSection().offset(), &resumeIndex)) {
    return false;
  }

  SET_RESUMEINDEX(bytecodeSection().code(off), resumeIndex);

  if (!emitInstrumentation(InstrumentationKind::Entry)) {
    return false;
  }

  BytecodeOffset unusedOffset;
  return emitJumpTargetOp(JSOP_AFTERYIELD, &unusedOffset);
}

bool BytecodeEmitter::emitSetThis(BinaryNode* setThisNode) {
  // ParseNodeKind::SetThis is used to update |this| after a super() call
  // in a derived class constructor.

  MOZ_ASSERT(setThisNode->isKind(ParseNodeKind::SetThis));
  MOZ_ASSERT(setThisNode->left()->isKind(ParseNodeKind::Name));

  RootedAtom name(cx, setThisNode->left()->as<NameNode>().name());

  // The 'this' binding is not lexical, but due to super() semantics this
  // initialization needs to be treated as a lexical one.
  NameLocation loc = lookupName(name);
  NameLocation lexicalLoc;
  if (loc.kind() == NameLocation::Kind::FrameSlot) {
    lexicalLoc = NameLocation::FrameSlot(BindingKind::Let, loc.frameSlot());
  } else if (loc.kind() == NameLocation::Kind::EnvironmentCoordinate) {
    EnvironmentCoordinate coord = loc.environmentCoordinate();
    uint8_t hops = AssertedCast<uint8_t>(coord.hops());
    lexicalLoc = NameLocation::EnvironmentCoordinate(BindingKind::Let, hops,
                                                     coord.slot());
  } else {
    MOZ_ASSERT(loc.kind() == NameLocation::Kind::Dynamic);
    lexicalLoc = loc;
  }

  NameOpEmitter noe(this, name, lexicalLoc, NameOpEmitter::Kind::Initialize);
  if (!noe.prepareForRhs()) {
    //              [stack]
    return false;
  }

  // Emit the new |this| value.
  if (!emitTree(setThisNode->right())) {
    //              [stack] NEWTHIS
    return false;
  }

  // Get the original |this| and throw if we already initialized
  // it. Do *not* use the NameLocation argument, as that's the special
  // lexical location below to deal with super() semantics.
  if (!emitGetName(name)) {
    //              [stack] NEWTHIS THIS
    return false;
  }
  if (!emit1(JSOP_CHECKTHISREINIT)) {
    //              [stack] NEWTHIS THIS
    return false;
  }
  if (!emit1(JSOP_POP)) {
    //              [stack] NEWTHIS
    return false;
  }
  if (!noe.emitAssignment()) {
    //              [stack] NEWTHIS
    return false;
  }

  if (!emitInitializeInstanceFields()) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::defineHoistedTopLevelFunctions(ParseNode* body) {
  MOZ_ASSERT(inPrologue());
  MOZ_ASSERT(sc->isGlobalContext() || (sc->isEvalContext() && !sc->strict()));
  MOZ_ASSERT(body->is<LexicalScopeNode>() || body->is<ListNode>());

  if (body->is<LexicalScopeNode>()) {
    body = body->as<LexicalScopeNode>().scopeBody();
    MOZ_ASSERT(body->is<ListNode>());
  }

  if (!body->as<ListNode>().hasTopLevelFunctionDeclarations()) {
    return true;
  }

  return emitHoistedFunctionsInList(&body->as<ListNode>());
}

bool BytecodeEmitter::emitScript(ParseNode* body) {
  AutoFrontendTraceLog traceLog(cx, TraceLogger_BytecodeEmission,
                                parser->errorReporter(), body);

  setScriptStartOffsetIfUnset(body->pn_pos.begin);

  MOZ_ASSERT(inPrologue());

  TDZCheckCache tdzCache(this);
  EmitterScope emitterScope(this);
  if (sc->isGlobalContext()) {
    if (!emitterScope.enterGlobal(this, sc->asGlobalContext())) {
      return false;
    }
  } else if (sc->isEvalContext()) {
    if (!emitterScope.enterEval(this, sc->asEvalContext())) {
      return false;
    }
  } else {
    MOZ_ASSERT(sc->isModuleContext());
    if (!emitterScope.enterModule(this, sc->asModuleContext())) {
      return false;
    }
  }

  setFunctionBodyEndPos(body->pn_pos.end);

  bool isSloppyEval = sc->isEvalContext() && !sc->strict();
  if (isSloppyEval && body->is<LexicalScopeNode>() &&
      !body->as<LexicalScopeNode>().isEmptyScope()) {
    // Sloppy eval scripts may need to emit DEFFUNs in the prologue. If there is
    // an immediately enclosed lexical scope, we need to enter the lexical
    // scope in the prologue for the DEFFUNs to pick up the right
    // environment chain.
    EmitterScope lexicalEmitterScope(this);
    LexicalScopeNode* scope = &body->as<LexicalScopeNode>();

    if (!lexicalEmitterScope.enterLexical(this, ScopeKind::Lexical,
                                          scope->scopeBindings())) {
      return false;
    }

    if (!defineHoistedTopLevelFunctions(scope->scopeBody())) {
      return false;
    }

    if (!switchToMain()) {
      return false;
    }

    ParseNode* scopeBody = scope->scopeBody();
    if (!emitLexicalScopeBody(scopeBody, EMIT_LINENOTE)) {
      return false;
    }

    if (!updateSourceCoordNotes(scopeBody->pn_pos.end)) {
      return false;
    }

    if (!lexicalEmitterScope.leave(this)) {
      return false;
    }
  } else {
    if (sc->isGlobalContext() || isSloppyEval) {
      if (!defineHoistedTopLevelFunctions(body)) {
        return false;
      }
    }

    if (!switchToMain()) {
      return false;
    }

    if (!emitTree(body)) {
      return false;
    }

    if (!updateSourceCoordNotes(body->pn_pos.end)) {
      return false;
    }
  }
  if (!markSimpleBreakpoint()) {
    return false;
  }

  if (!emitReturnRval()) {
    return false;
  }

  if (!emitterScope.leave(this)) {
    return false;
  }

  if (!NameFunctions(cx, body)) {
    return false;
  }

  if (!JSScript::fullyInitFromEmitter(cx, script, this)) {
    return false;
  }

  tellDebuggerAboutCompiledScript(cx);

  return true;
}

bool BytecodeEmitter::emitFunctionScript(FunctionNode* funNode,
                                         TopLevelFunction isTopLevel) {
  MOZ_ASSERT(inPrologue());
  ListNode* paramsBody = &funNode->body()->as<ListNode>();
  MOZ_ASSERT(paramsBody->isKind(ParseNodeKind::ParamsBody));
  FunctionBox* funbox = sc->asFunctionBox();
  AutoFrontendTraceLog traceLog(cx, TraceLogger_BytecodeEmission,
                                parser->errorReporter(), funbox);

  MOZ_ASSERT((fieldInitializers_.valid) ==
             (funbox->kind() == FunctionFlags::FunctionKind::ClassConstructor));

  setScriptStartOffsetIfUnset(paramsBody->pn_pos.begin);

  //                [stack]

  FunctionScriptEmitter fse(this, funbox, Some(paramsBody->pn_pos.begin),
                            Some(paramsBody->pn_pos.end));
  if (!fse.prepareForParameters()) {
    //              [stack]
    return false;
  }

  if (!emitFunctionFormalParameters(paramsBody)) {
    //              [stack]
    return false;
  }

  if (!fse.prepareForBody()) {
    //              [stack]
    return false;
  }

  if (!emitTree(paramsBody->last())) {
    //              [stack]
    return false;
  }

  if (!fse.emitEndBody()) {
    //              [stack]
    return false;
  }

  if (isTopLevel == TopLevelFunction::Yes) {
    if (!NameFunctions(cx, funNode)) {
      return false;
    }
  }

  if (!fse.initScript()) {
    return false;
  }

  script->setFieldInitializers(fieldInitializers_);

  return true;
}

bool BytecodeEmitter::emitDestructuringLHSRef(ParseNode* target,
                                              size_t* emitted) {
  *emitted = 0;

  if (target->isKind(ParseNodeKind::Spread)) {
    target = target->as<UnaryNode>().kid();
  } else if (target->isKind(ParseNodeKind::AssignExpr)) {
    target = target->as<AssignmentNode>().left();
  }

  // No need to recur into ParseNodeKind::Array and
  // ParseNodeKind::Object subpatterns here, since
  // emitSetOrInitializeDestructuring does the recursion when
  // setting or initializing value.  Getting reference doesn't recur.
  if (target->isKind(ParseNodeKind::Name) ||
      target->isKind(ParseNodeKind::ArrayExpr) ||
      target->isKind(ParseNodeKind::ObjectExpr)) {
    return true;
  }

#ifdef DEBUG
  int depth = bytecodeSection().stackDepth();
#endif

  switch (target->getKind()) {
    case ParseNodeKind::DotExpr: {
      PropertyAccess* prop = &target->as<PropertyAccess>();
      bool isSuper = prop->isSuper();
      PropOpEmitter poe(this, PropOpEmitter::Kind::SimpleAssignment,
                        isSuper ? PropOpEmitter::ObjKind::Super
                                : PropOpEmitter::ObjKind::Other);
      if (!poe.prepareForObj()) {
        return false;
      }
      if (isSuper) {
        UnaryNode* base = &prop->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {
          //        [stack] THIS SUPERBASE
          return false;
        }
        // SUPERBASE is pushed onto THIS in poe.prepareForRhs below.
        *emitted = 2;
      } else {
        if (!emitTree(&prop->expression())) {
          //        [stack] OBJ
          return false;
        }
        *emitted = 1;
      }
      if (!poe.prepareForRhs()) {
        //          [stack] # if Super
        //          [stack] THIS SUPERBASE
        //          [stack] # otherwise
        //          [stack] OBJ
        return false;
      }
      break;
    }

    case ParseNodeKind::ElemExpr: {
      PropertyByValue* elem = &target->as<PropertyByValue>();
      bool isSuper = elem->isSuper();
      ElemOpEmitter eoe(this, ElemOpEmitter::Kind::SimpleAssignment,
                        isSuper ? ElemOpEmitter::ObjKind::Super
                                : ElemOpEmitter::ObjKind::Other);
      if (!emitElemObjAndKey(elem, isSuper, eoe)) {
        //          [stack] # if Super
        //          [stack] THIS KEY
        //          [stack] # otherwise
        //          [stack] OBJ KEY
        return false;
      }
      if (isSuper) {
        // SUPERBASE is pushed onto KEY in eoe.prepareForRhs below.
        *emitted = 3;
      } else {
        *emitted = 2;
      }
      if (!eoe.prepareForRhs()) {
        //          [stack] # if Super
        //          [stack] THIS KEY SUPERBASE
        //          [stack] # otherwise
        //          [stack] OBJ KEY
        return false;
      }
      break;
    }

    case ParseNodeKind::CallExpr:
      MOZ_ASSERT_UNREACHABLE(
          "Parser::reportIfNotValidSimpleAssignmentTarget "
          "rejects function calls as assignment "
          "targets in destructuring assignments");
      break;

    default:
      MOZ_CRASH("emitDestructuringLHSRef: bad lhs kind");
  }

  MOZ_ASSERT(bytecodeSection().stackDepth() == depth + int(*emitted));

  return true;
}

bool BytecodeEmitter::emitSetOrInitializeDestructuring(
    ParseNode* target, DestructuringFlavor flav) {
  // Now emit the lvalue opcode sequence. If the lvalue is a nested
  // destructuring initialiser-form, call ourselves to handle it, then pop
  // the matched value. Otherwise emit an lvalue bytecode sequence followed
  // by an assignment op.
  if (target->isKind(ParseNodeKind::Spread)) {
    target = target->as<UnaryNode>().kid();
  } else if (target->isKind(ParseNodeKind::AssignExpr)) {
    target = target->as<AssignmentNode>().left();
  }
  if (target->isKind(ParseNodeKind::ArrayExpr) ||
      target->isKind(ParseNodeKind::ObjectExpr)) {
    if (!emitDestructuringOps(&target->as<ListNode>(), flav)) {
      return false;
    }
    // Per its post-condition, emitDestructuringOps has left the
    // to-be-destructured value on top of the stack.
    if (!emit1(JSOP_POP)) {
      return false;
    }
  } else {
    switch (target->getKind()) {
      case ParseNodeKind::Name: {
        RootedAtom name(cx, target->as<NameNode>().name());
        NameLocation loc;
        NameOpEmitter::Kind kind;
        switch (flav) {
          case DestructuringFlavor::Declaration:
            loc = lookupName(name);
            kind = NameOpEmitter::Kind::Initialize;
            break;
          case DestructuringFlavor::FormalParameterInVarScope: {
            // If there's an parameter expression var scope, the
            // destructuring declaration needs to initialize the name in
            // the function scope. The innermost scope is the var scope,
            // and its enclosing scope is the function scope.
            EmitterScope* funScope =
                innermostEmitterScope()->enclosingInFrame();
            loc = *locationOfNameBoundInScope(name, funScope);
            kind = NameOpEmitter::Kind::Initialize;
            break;
          }

          case DestructuringFlavor::Assignment:
            loc = lookupName(name);
            kind = NameOpEmitter::Kind::SimpleAssignment;
            break;
        }

        NameOpEmitter noe(this, name, loc, kind);
        if (!noe.prepareForRhs()) {
          //        [stack] V ENV?
          return false;
        }
        if (noe.emittedBindOp()) {
          // This is like ordinary assignment, but with one difference.
          //
          // In `a = b`, we first determine a binding for `a` (using
          // JSOP_BINDNAME or JSOP_BINDGNAME), then we evaluate `b`, then
          // a JSOP_SETNAME instruction.
          //
          // In `[a] = [b]`, per spec, `b` is evaluated first, then we
          // determine a binding for `a`. Then we need to do assignment--
          // but the operands are on the stack in the wrong order for
          // JSOP_SETPROP, so we have to add a JSOP_SWAP.
          //
          // In the cases where we are emitting a name op, emit a swap
          // because of this.
          if (!emit1(JSOP_SWAP)) {
            //      [stack] ENV V
            return false;
          }
        } else {
          // In cases of emitting a frame slot or environment slot,
          // nothing needs be done.
        }
        if (!noe.emitAssignment()) {
          //        [stack] V
          return false;
        }

        break;
      }

      case ParseNodeKind::DotExpr: {
        // The reference is already pushed by emitDestructuringLHSRef.
        //          [stack] # if Super
        //          [stack] THIS SUPERBASE VAL
        //          [stack] # otherwise
        //          [stack] OBJ VAL
        PropertyAccess* prop = &target->as<PropertyAccess>();
        bool isSuper = prop->isSuper();
        PropOpEmitter poe(this, PropOpEmitter::Kind::SimpleAssignment,
                          isSuper ? PropOpEmitter::ObjKind::Super
                                  : PropOpEmitter::ObjKind::Other);
        if (!poe.skipObjAndRhs()) {
          return false;
        }
        //          [stack] # VAL
        if (!poe.emitAssignment(prop->key().atom())) {
          return false;
        }
        break;
      }

      case ParseNodeKind::ElemExpr: {
        // The reference is already pushed by emitDestructuringLHSRef.
        //          [stack] # if Super
        //          [stack] THIS KEY SUPERBASE VAL
        //          [stack] # otherwise
        //          [stack] OBJ KEY VAL
        PropertyByValue* elem = &target->as<PropertyByValue>();
        bool isSuper = elem->isSuper();
        ElemOpEmitter eoe(this, ElemOpEmitter::Kind::SimpleAssignment,
                          isSuper ? ElemOpEmitter::ObjKind::Super
                                  : ElemOpEmitter::ObjKind::Other);
        if (!eoe.skipObjAndKeyAndRhs()) {
          return false;
        }
        if (!eoe.emitAssignment()) {
          //        [stack] VAL
          return false;
        }
        break;
      }

      case ParseNodeKind::CallExpr:
        MOZ_ASSERT_UNREACHABLE(
            "Parser::reportIfNotValidSimpleAssignmentTarget "
            "rejects function calls as assignment "
            "targets in destructuring assignments");
        break;

      default:
        MOZ_CRASH("emitSetOrInitializeDestructuring: bad lhs kind");
    }

    // Pop the assigned value.
    if (!emit1(JSOP_POP)) {
      //            [stack] # empty
      return false;
    }
  }

  return true;
}

bool BytecodeEmitter::emitIteratorNext(
    const Maybe<uint32_t>& callSourceCoordOffset,
    IteratorKind iterKind /* = IteratorKind::Sync */,
    bool allowSelfHosted /* = false */) {
  MOZ_ASSERT(allowSelfHosted || emitterMode != BytecodeEmitter::SelfHosting,
             ".next() iteration is prohibited in self-hosted code because it "
             "can run user-modifiable iteration code");

  //                [stack] ... NEXT ITER
  MOZ_ASSERT(bytecodeSection().stackDepth() >= 2);

  if (!emitCall(JSOP_CALL, 0, callSourceCoordOffset)) {
    //              [stack] ... RESULT
    return false;
  }

  if (iterKind == IteratorKind::Async) {
    if (!emitAwaitInInnermostScope()) {
      //            [stack] ... RESULT
      return false;
    }
  }

  if (!emitCheckIsObj(CheckIsObjectKind::IteratorNext)) {
    //              [stack] ... RESULT
    return false;
  }
  return true;
}

bool BytecodeEmitter::emitPushNotUndefinedOrNull() {
  //                [stack] V
  MOZ_ASSERT(bytecodeSection().stackDepth() > 0);

  if (!emit1(JSOP_DUP)) {
    //              [stack] V V
    return false;
  }
  if (!emit1(JSOP_UNDEFINED)) {
    //              [stack] V V UNDEFINED
    return false;
  }
  if (!emit1(JSOP_STRICTNE)) {
    //              [stack] V NEQ
    return false;
  }

  JumpList undefinedOrNullJump;
  if (!emitJump(JSOP_AND, &undefinedOrNullJump)) {
    //              [stack] V NEQ
    return false;
  }

  if (!emit1(JSOP_POP)) {
    //              [stack] V
    return false;
  }
  if (!emit1(JSOP_DUP)) {
    //              [stack] V V
    return false;
  }
  if (!emit1(JSOP_NULL)) {
    //              [stack] V V NULL
    return false;
  }
  if (!emit1(JSOP_STRICTNE)) {
    //              [stack] V NEQ
    return false;
  }

  if (!emitJumpTargetAndPatch(undefinedOrNullJump)) {
    //              [stack] V NOT-UNDEF-OR-NULL
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitIteratorCloseInScope(
    EmitterScope& currentScope,
    IteratorKind iterKind /* = IteratorKind::Sync */,
    CompletionKind completionKind /* = CompletionKind::Normal */,
    bool allowSelfHosted /* = false */) {
  MOZ_ASSERT(
      allowSelfHosted || emitterMode != BytecodeEmitter::SelfHosting,
      ".close() on iterators is prohibited in self-hosted code because it "
      "can run user-modifiable iteration code");

  // Generate inline logic corresponding to IteratorClose (ES 7.4.6).
  //
  // Callers need to ensure that the iterator object is at the top of the
  // stack.

  if (!emit1(JSOP_DUP)) {
    //              [stack] ... ITER ITER
    return false;
  }

  // Step 3.
  //
  // Get the "return" method.
  if (!emitAtomOp(cx->names().return_, JSOP_CALLPROP)) {
    //              [stack] ... ITER RET
    return false;
  }

  // Step 4.
  //
  // Do nothing if "return" is undefined or null.
  InternalIfEmitter ifReturnMethodIsDefined(this);
  if (!emitPushNotUndefinedOrNull()) {
    //              [stack] ... ITER RET NOT-UNDEF-OR-NULL
    return false;
  }

  if (!ifReturnMethodIsDefined.emitThenElse()) {
    //              [stack] ... ITER RET
    return false;
  }

  if (completionKind == CompletionKind::Throw) {
    // 7.4.6 IteratorClose ( iterator, completion )
    //   ...
    //   3. Let return be ? GetMethod(iterator, "return").
    //   4. If return is undefined, return Completion(completion).
    //   5. Let innerResult be Call(return, iterator, « »).
    //   6. If completion.[[Type]] is throw, return Completion(completion).
    //   7. If innerResult.[[Type]] is throw, return
    //      Completion(innerResult).
    //
    // For CompletionKind::Normal case, JSOP_CALL for step 5 checks if RET
    // is callable, and throws if not.  Since step 6 doesn't match and
    // error handling in step 3 and step 7 can be merged.
    //
    // For CompletionKind::Throw case, an error thrown by JSOP_CALL for
    // step 5 is ignored by try-catch.  So we should check if RET is
    // callable here, outside of try-catch, and the throw immediately if
    // not.
    CheckIsCallableKind kind = CheckIsCallableKind::IteratorReturn;
    if (!emitCheckIsCallable(kind)) {
      //            [stack] ... ITER RET
      return false;
    }
  }

  // Steps 5, 8.
  //
  // Call "return" if it is not undefined or null, and check that it returns
  // an Object.
  if (!emit1(JSOP_SWAP)) {
    //              [stack] ... RET ITER
    return false;
  }

  Maybe<TryEmitter> tryCatch;

  if (completionKind == CompletionKind::Throw) {
    tryCatch.emplace(this, TryEmitter::Kind::TryCatch,
                     TryEmitter::ControlKind::NonSyntactic);

    // Mutate stack to balance stack for try-catch.
    if (!emit1(JSOP_UNDEFINED)) {
      //            [stack] ... RET ITER UNDEF
      return false;
    }
    if (!tryCatch->emitTry()) {
      //            [stack] ... RET ITER UNDEF
      return false;
    }
    if (!emitDupAt(2, 2)) {
      //            [stack] ... RET ITER UNDEF RET
      return false;
    }
  }

  if (!emitCall(JSOP_CALL, 0)) {
    //              [stack] ... ... RESULT
    return false;
  }

  if (iterKind == IteratorKind::Async) {
    if (completionKind != CompletionKind::Throw) {
      // Await clobbers rval, so save the current rval.
      if (!emit1(JSOP_GETRVAL)) {
        //          [stack] ... ... RESULT RVAL
        return false;
      }
      if (!emit1(JSOP_SWAP)) {
        //          [stack] ... ... RVAL RESULT
        return false;
      }
    }
    if (!emitAwaitInScope(currentScope)) {
      //            [stack] ... ... RVAL? RESULT
      return false;
    }
  }

  if (completionKind == CompletionKind::Throw) {
    if (!emit1(JSOP_SWAP)) {
      //            [stack] ... RET ITER RESULT UNDEF
      return false;
    }
    if (!emit1(JSOP_POP)) {
      //            [stack] ... RET ITER RESULT
      return false;
    }

    if (!tryCatch->emitCatch()) {
      //            [stack] ... RET ITER RESULT EXC
      return false;
    }

    // Just ignore the exception thrown by call and await.
    if (!emit1(JSOP_POP)) {
      //            [stack] ... RET ITER RESULT
      return false;
    }

    if (!tryCatch->emitEnd()) {
      //            [stack] ... RET ITER RESULT
      return false;
    }

    // Restore stack.
    if (!emit2(JSOP_UNPICK, 2)) {
      //            [stack] ... RESULT RET ITER
      return false;
    }
    if (!emitPopN(2)) {
      //            [stack] ... RESULT
      return false;
    }
  } else {
    if (!emitCheckIsObj(CheckIsObjectKind::IteratorReturn)) {
      //            [stack] ... RVAL? RESULT
      return false;
    }

    if (iterKind == IteratorKind::Async) {
      if (!emit1(JSOP_SWAP)) {
        //          [stack] ... RESULT RVAL
        return false;
      }
      if (!emit1(JSOP_SETRVAL)) {
        //          [stack] ... RESULT
        return false;
      }
    }
  }

  if (!ifReturnMethodIsDefined.emitElse()) {
    //              [stack] ... ITER RET
    return false;
  }

  if (!emit1(JSOP_POP)) {
    //              [stack] ... ITER
    return false;
  }

  if (!ifReturnMethodIsDefined.emitEnd()) {
    return false;
  }

  return emit1(JSOP_POP);
  //                [stack] ...
}

template <typename InnerEmitter>
bool BytecodeEmitter::wrapWithDestructuringTryNote(int32_t iterDepth,
                                                   InnerEmitter emitter) {
  MOZ_ASSERT(bytecodeSection().stackDepth() >= iterDepth);

  // Pad a nop at the beginning of the bytecode covered by the trynote so
  // that when unwinding environments, we may unwind to the scope
  // corresponding to the pc *before* the start, in case the first bytecode
  // emitted by |emitter| is the start of an inner scope. See comment above
  // UnwindEnvironmentToTryPc.
  if (!emit1(JSOP_TRY_DESTRUCTURING)) {
    return false;
  }

  BytecodeOffset start = bytecodeSection().offset();
  if (!emitter(this)) {
    return false;
  }
  BytecodeOffset end = bytecodeSection().offset();
  if (start != end) {
    return addTryNote(JSTRY_DESTRUCTURING, iterDepth, start, end);
  }
  return true;
}

bool BytecodeEmitter::emitDefault(ParseNode* defaultExpr, ParseNode* pattern) {
  //                [stack] VALUE

  DefaultEmitter de(this);
  if (!de.prepareForDefault()) {
    //              [stack]
    return false;
  }
  if (!emitInitializer(defaultExpr, pattern)) {
    //              [stack] DEFAULTVALUE
    return false;
  }
  if (!de.emitEnd()) {
    //              [stack] VALUE/DEFAULTVALUE
    return false;
  }
  return true;
}

bool BytecodeEmitter::emitAnonymousFunctionWithName(ParseNode* node,
                                                    HandleAtom name) {
  MOZ_ASSERT(node->isDirectRHSAnonFunction());

  if (node->is<FunctionNode>()) {
    if (!emitTree(node)) {
      return false;
    }

    // Function doesn't have 'name' property at this point.
    // Set function's name at compile time.
    return setFunName(node->as<FunctionNode>().funbox()->function(), name);
  }

  MOZ_ASSERT(node->is<ClassNode>());

  return emitClass(&node->as<ClassNode>(), ClassNameKind::InferredName, name);
}

bool BytecodeEmitter::emitAnonymousFunctionWithComputedName(
    ParseNode* node, FunctionPrefixKind prefixKind) {
  MOZ_ASSERT(node->isDirectRHSAnonFunction());

  if (node->is<FunctionNode>()) {
    if (!emitTree(node)) {
      //            [stack] NAME FUN
      return false;
    }
    if (!emitDupAt(1)) {
      //            [stack] NAME FUN NAME
      return false;
    }
    if (!emit2(JSOP_SETFUNNAME, uint8_t(prefixKind))) {
      //            [stack] NAME FUN
      return false;
    }
    return true;
  }

  MOZ_ASSERT(node->is<ClassNode>());
  MOZ_ASSERT(prefixKind == FunctionPrefixKind::None);

  return emitClass(&node->as<ClassNode>(), ClassNameKind::ComputedName);
}

bool BytecodeEmitter::setFunName(JSFunction* fun, JSAtom* name) {
  // The inferred name may already be set if this function is an interpreted
  // lazy function and we OOM'ed after we set the inferred name the first
  // time.
  if (fun->hasInferredName()) {
    MOZ_ASSERT(fun->isInterpretedLazy());
    MOZ_ASSERT(fun->inferredName() == name);

    return true;
  }

  fun->setInferredName(name);
  return true;
}

bool BytecodeEmitter::emitInitializer(ParseNode* initializer,
                                      ParseNode* pattern) {
  if (initializer->isDirectRHSAnonFunction()) {
    MOZ_ASSERT(!pattern->isInParens());
    RootedAtom name(cx, pattern->as<NameNode>().name());
    if (!emitAnonymousFunctionWithName(initializer, name)) {
      return false;
    }
  } else {
    if (!emitTree(initializer)) {
      return false;
    }
  }

  return true;
}

bool BytecodeEmitter::emitDestructuringOpsArray(ListNode* pattern,
                                                DestructuringFlavor flav) {
  MOZ_ASSERT(pattern->isKind(ParseNodeKind::ArrayExpr));
  MOZ_ASSERT(bytecodeSection().stackDepth() != 0);

  // Here's pseudo code for |let [a, b, , c=y, ...d] = x;|
  //
  // Lines that are annotated "covered by trynote" mean that upon throwing
  // an exception, IteratorClose is called on iter only if done is false.
  //
  //   let x, y;
  //   let a, b, c, d;
  //   let iter, next, lref, result, done, value; // stack values
  //
  //   iter = x[Symbol.iterator]();
  //   next = iter.next;
  //
  //   // ==== emitted by loop for a ====
  //   lref = GetReference(a);              // covered by trynote
  //
  //   result = Call(next, iter);
  //   done = result.done;
  //
  //   if (done)
  //     value = undefined;
  //   else
  //     value = result.value;
  //
  //   SetOrInitialize(lref, value);        // covered by trynote
  //
  //   // ==== emitted by loop for b ====
  //   lref = GetReference(b);              // covered by trynote
  //
  //   if (done) {
  //     value = undefined;
  //   } else {
  //     result = Call(next, iter);
  //     done = result.done;
  //     if (done)
  //       value = undefined;
  //     else
  //       value = result.value;
  //   }
  //
  //   SetOrInitialize(lref, value);        // covered by trynote
  //
  //   // ==== emitted by loop for elision ====
  //   if (done) {
  //     value = undefined;
  //   } else {
  //     result = Call(next, iter);
  //     done = result.done;
  //     if (done)
  //       value = undefined;
  //     else
  //       value = result.value;
  //   }
  //
  //   // ==== emitted by loop for c ====
  //   lref = GetReference(c);              // covered by trynote
  //
  //   if (done) {
  //     value = undefined;
  //   } else {
  //     result = Call(next, iter);
  //     done = result.done;
  //     if (done)
  //       value = undefined;
  //     else
  //       value = result.value;
  //   }
  //
  //   if (value === undefined)
  //     value = y;                         // covered by trynote
  //
  //   SetOrInitialize(lref, value);        // covered by trynote
  //
  //   // ==== emitted by loop for d ====
  //   lref = GetReference(d);              // covered by trynote
  //
  //   if (done)
  //     value = [];
  //   else
  //     value = [...iter];
  //
  //   SetOrInitialize(lref, value);        // covered by trynote
  //
  //   // === emitted after loop ===
  //   if (!done)
  //      IteratorClose(iter);

  // Use an iterator to destructure the RHS, instead of index lookup. We
  // must leave the *original* value on the stack.
  if (!emit1(JSOP_DUP)) {
    //              [stack] ... OBJ OBJ
    return false;
  }
  if (!emitIterator()) {
    //              [stack] ... OBJ NEXT ITER
    return false;
  }

  // For an empty pattern [], call IteratorClose unconditionally. Nothing
  // else needs to be done.
  if (!pattern->head()) {
    if (!emit1(JSOP_SWAP)) {
      //            [stack] ... OBJ ITER NEXT
      return false;
    }
    if (!emit1(JSOP_POP)) {
      //            [stack] ... OBJ ITER
      return false;
    }

    return emitIteratorCloseInInnermostScope();
    //              [stack] ... OBJ
  }

  // Push an initial FALSE value for DONE.
  if (!emit1(JSOP_FALSE)) {
    //              [stack] ... OBJ NEXT ITER FALSE
    return false;
  }

  // JSTRY_DESTRUCTURING expects the iterator and the done value
  // to be the second to top and the top of the stack, respectively.
  // IteratorClose is called upon exception only if done is false.
  int32_t tryNoteDepth = bytecodeSection().stackDepth();

  for (ParseNode* member : pattern->contents()) {
    bool isFirst = member == pattern->head();
    DebugOnly<bool> hasNext = !!member->pn_next;

    size_t emitted = 0;

    // Spec requires LHS reference to be evaluated first.
    ParseNode* lhsPattern = member;
    if (lhsPattern->isKind(ParseNodeKind::AssignExpr)) {
      lhsPattern = lhsPattern->as<AssignmentNode>().left();
    }

    bool isElision = lhsPattern->isKind(ParseNodeKind::Elision);
    if (!isElision) {
      auto emitLHSRef = [lhsPattern, &emitted](BytecodeEmitter* bce) {
        return bce->emitDestructuringLHSRef(lhsPattern, &emitted);
        //          [stack] ... OBJ NEXT ITER DONE LREF*
      };
      if (!wrapWithDestructuringTryNote(tryNoteDepth, emitLHSRef)) {
        return false;
      }
    }

    // Pick the DONE value to the top of the stack.
    if (emitted) {
      if (!emit2(JSOP_PICK, emitted)) {
        //          [stack] ... OBJ NEXT ITER LREF* DONE
        return false;
      }
    }

    if (isFirst) {
      // If this element is the first, DONE is always FALSE, so pop it.
      //
      // Non-first elements should emit if-else depending on the
      // member pattern, below.
      if (!emit1(JSOP_POP)) {
        //          [stack] ... OBJ NEXT ITER LREF*
        return false;
      }
    }

    if (member->isKind(ParseNodeKind::Spread)) {
      InternalIfEmitter ifThenElse(this);
      if (!isFirst) {
        // If spread is not the first element of the pattern,
        // iterator can already be completed.
        //          [stack] ... OBJ NEXT ITER LREF* DONE

        if (!ifThenElse.emitThenElse()) {
          //        [stack] ... OBJ NEXT ITER LREF*
          return false;
        }

        if (!emitUint32Operand(JSOP_NEWARRAY, 0)) {
          //        [stack] ... OBJ NEXT ITER LREF* ARRAY
          return false;
        }
        if (!ifThenElse.emitElse()) {
          //        [stack] ... OBJ NEXT ITER LREF*
          return false;
        }
      }

      // If iterator is not completed, create a new array with the rest
      // of the iterator.
      if (!emitDupAt(emitted + 1, 2)) {
        //          [stack] ... OBJ NEXT ITER LREF* NEXT
        return false;
      }
      if (!emitUint32Operand(JSOP_NEWARRAY, 0)) {
        //          [stack] ... OBJ NEXT ITER LREF* NEXT ITER ARRAY
        return false;
      }
      if (!emitNumberOp(0)) {
        //          [stack] ... OBJ NEXT ITER LREF* NEXT ITER ARRAY INDEX
        return false;
      }
      if (!emitSpread()) {
        //          [stack] ... OBJ NEXT ITER LREF* ARRAY INDEX
        return false;
      }
      if (!emit1(JSOP_POP)) {
        //          [stack] ... OBJ NEXT ITER LREF* ARRAY
        return false;
      }

      if (!isFirst) {
        if (!ifThenElse.emitEnd()) {
          return false;
        }
        MOZ_ASSERT(ifThenElse.pushed() == 1);
      }

      // At this point the iterator is done. Unpick a TRUE value for DONE above
      // ITER.
      if (!emit1(JSOP_TRUE)) {
        //          [stack] ... OBJ NEXT ITER LREF* ARRAY TRUE
        return false;
      }
      if (!emit2(JSOP_UNPICK, emitted + 1)) {
        //          [stack] ... OBJ NEXT ITER TRUE LREF* ARRAY
        return false;
      }

      auto emitAssignment = [member, flav](BytecodeEmitter* bce) {
        return bce->emitSetOrInitializeDestructuring(member, flav);
        //          [stack] ... OBJ NEXT ITER TRUE
      };
      if (!wrapWithDestructuringTryNote(tryNoteDepth, emitAssignment)) {
        return false;
      }

      MOZ_ASSERT(!hasNext);
      break;
    }

    ParseNode* pndefault = nullptr;
    if (member->isKind(ParseNodeKind::AssignExpr)) {
      pndefault = member->as<AssignmentNode>().right();
    }

    MOZ_ASSERT(!member->isKind(ParseNodeKind::Spread));

    InternalIfEmitter ifAlreadyDone(this);
    if (!isFirst) {
      //            [stack] ... OBJ NEXT ITER LREF* DONE

      if (!ifAlreadyDone.emitThenElse()) {
        //          [stack] ... OBJ NEXT ITER LREF*
        return false;
      }

      if (!emit1(JSOP_UNDEFINED)) {
        //          [stack] ... OBJ NEXT ITER LREF* UNDEF
        return false;
      }
      if (!emit1(JSOP_NOP_DESTRUCTURING)) {
        //          [stack] ... OBJ NEXT ITER LREF* UNDEF
        return false;
      }

      // The iterator is done. Unpick a TRUE value for DONE above ITER.
      if (!emit1(JSOP_TRUE)) {
        //          [stack] ... OBJ NEXT ITER LREF* UNDEF TRUE
        return false;
      }
      if (!emit2(JSOP_UNPICK, emitted + 1)) {
        //          [stack] ... OBJ NEXT ITER TRUE LREF* UNDEF
        return false;
      }

      if (!ifAlreadyDone.emitElse()) {
        //          [stack] ... OBJ NEXT ITER LREF*
        return false;
      }
    }

    if (!emitDupAt(emitted + 1, 2)) {
      //            [stack] ... OBJ NEXT ITER LREF* NEXT
      return false;
    }
    if (!emitIteratorNext(Some(pattern->pn_pos.begin))) {
      //            [stack] ... OBJ NEXT ITER LREF* RESULT
      return false;
    }
    if (!emit1(JSOP_DUP)) {
      //            [stack] ... OBJ NEXT ITER LREF* RESULT RESULT
      return false;
    }
    if (!emitAtomOp(cx->names().done, JSOP_GETPROP)) {
      //            [stack] ... OBJ NEXT ITER LREF* RESULT DONE
      return false;
    }

    if (!emit1(JSOP_DUP)) {
      //            [stack] ... OBJ NEXT ITER LREF* RESULT DONE DONE
      return false;
    }
    if (!emit2(JSOP_UNPICK, emitted + 2)) {
      //            [stack] ... OBJ NEXT ITER DONE LREF* RESULT DONE
      return false;
    }

    InternalIfEmitter ifDone(this);
    if (!ifDone.emitThenElse()) {
      //            [stack] ... OBJ NEXT ITER DONE LREF* RESULT
      return false;
    }

    if (!emit1(JSOP_POP)) {
      //            [stack] ... OBJ NEXT ITER DONE LREF*
      return false;
    }
    if (!emit1(JSOP_UNDEFINED)) {
      //            [stack] ... OBJ NEXT ITER DONE LREF* UNDEF
      return false;
    }
    if (!emit1(JSOP_NOP_DESTRUCTURING)) {
      //            [stack] ... OBJ NEXT ITER DONE LREF* UNDEF
      return false;
    }

    if (!ifDone.emitElse()) {
      //            [stack] ... OBJ NEXT ITER DONE LREF* RESULT
      return false;
    }

    if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) {
      //            [stack] ... OBJ NEXT ITER DONE LREF* VALUE
      return false;
    }

    if (!ifDone.emitEnd()) {
      return false;
    }
    MOZ_ASSERT(ifDone.pushed() == 0);

    if (!isFirst) {
      if (!ifAlreadyDone.emitEnd()) {
        return false;
      }
      MOZ_ASSERT(ifAlreadyDone.pushed() == 2);
    }

    if (pndefault) {
      auto emitDefault = [pndefault, lhsPattern](BytecodeEmitter* bce) {
        return bce->emitDefault(pndefault, lhsPattern);
        //          [stack] ... OBJ NEXT ITER DONE LREF* VALUE
      };

      if (!wrapWithDestructuringTryNote(tryNoteDepth, emitDefault)) {
        return false;
      }
    }

    if (!isElision) {
      auto emitAssignment = [lhsPattern, flav](BytecodeEmitter* bce) {
        return bce->emitSetOrInitializeDestructuring(lhsPattern, flav);
        //          [stack] ... OBJ NEXT ITER DONE
      };

      if (!wrapWithDestructuringTryNote(tryNoteDepth, emitAssignment)) {
        return false;
      }
    } else {
      if (!emit1(JSOP_POP)) {
        //          [stack] ... OBJ NEXT ITER DONE
        return false;
      }
    }
  }

  // The last DONE value is on top of the stack. If not DONE, call
  // IteratorClose.
  //                [stack] ... OBJ NEXT ITER DONE

  InternalIfEmitter ifDone(this);
  if (!ifDone.emitThenElse()) {
    //              [stack] ... OBJ NEXT ITER
    return false;
  }
  if (!emitPopN(2)) {
    //              [stack] ... OBJ
    return false;
  }
  if (!ifDone.emitElse()) {
    //              [stack] ... OBJ NEXT ITER
    return false;
  }
  if (!emit1(JSOP_SWAP)) {
    //              [stack] ... OBJ ITER NEXT
    return false;
  }
  if (!emit1(JSOP_POP)) {
    //              [stack] ... OBJ ITER
    return false;
  }
  if (!emitIteratorCloseInInnermostScope()) {
    //              [stack] ... OBJ
    return false;
  }
  if (!ifDone.emitEnd()) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitComputedPropertyName(UnaryNode* computedPropName) {
  MOZ_ASSERT(computedPropName->isKind(ParseNodeKind::ComputedName));
  return emitTree(computedPropName->kid()) && emit1(JSOP_TOID);
}

bool BytecodeEmitter::emitDestructuringOpsObject(ListNode* pattern,
                                                 DestructuringFlavor flav) {
  MOZ_ASSERT(pattern->isKind(ParseNodeKind::ObjectExpr));

  //                [stack] ... RHS
  MOZ_ASSERT(bytecodeSection().stackDepth() > 0);

  if (!emit1(JSOP_CHECKOBJCOERCIBLE)) {
    //              [stack] ... RHS
    return false;
  }

  bool needsRestPropertyExcludedSet =
      pattern->count() > 1 && pattern->last()->isKind(ParseNodeKind::Spread);
  if (needsRestPropertyExcludedSet) {
    if (!emitDestructuringObjRestExclusionSet(pattern)) {
      //            [stack] ... RHS SET
      return false;
    }

    if (!emit1(JSOP_SWAP)) {
      //            [stack] ... SET RHS
      return false;
    }
  }

  for (ParseNode* member : pattern->contents()) {
    ParseNode* subpattern;
    if (member->isKind(ParseNodeKind::MutateProto) ||
        member->isKind(ParseNodeKind::Spread)) {
      subpattern = member->as<UnaryNode>().kid();
    } else {
      MOZ_ASSERT(member->isKind(ParseNodeKind::PropertyDefinition) ||
                 member->isKind(ParseNodeKind::Shorthand));
      subpattern = member->as<BinaryNode>().right();
    }

    ParseNode* lhs = subpattern;
    MOZ_ASSERT_IF(member->isKind(ParseNodeKind::Spread),
                  !lhs->isKind(ParseNodeKind::AssignExpr));
    if (lhs->isKind(ParseNodeKind::AssignExpr)) {
      lhs = lhs->as<AssignmentNode>().left();
    }

    size_t emitted;
    if (!emitDestructuringLHSRef(lhs, &emitted)) {
      //            [stack] ... SET? RHS LREF*
      return false;
    }

    // Duplicate the value being destructured to use as a reference base.
    if (!emitDupAt(emitted)) {
      //            [stack] ... SET? RHS LREF* RHS
      return false;
    }

    if (member->isKind(ParseNodeKind::Spread)) {
      if (!updateSourceCoordNotes(member->pn_pos.begin)) {
        return false;
      }

      if (!emitNewInit()) {
        //          [stack] ... SET? RHS LREF* RHS TARGET
        return false;
      }
      if (!emit1(JSOP_DUP)) {
        //          [stack] ... SET? RHS LREF* RHS TARGET TARGET
        return false;
      }
      if (!emit2(JSOP_PICK, 2)) {
        //          [stack] ... SET? RHS LREF* TARGET TARGET RHS
        return false;
      }

      if (needsRestPropertyExcludedSet) {
        if (!emit2(JSOP_PICK, emitted + 4)) {
          //        [stack] ... RHS LREF* TARGET TARGET RHS SET
          return false;
        }
      }

      CopyOption option = needsRestPropertyExcludedSet ? CopyOption::Filtered
                                                       : CopyOption::Unfiltered;
      if (!emitCopyDataProperties(option)) {
        //          [stack] ... RHS LREF* TARGET
        return false;
      }

      // Destructure TARGET per this member's lhs.
      if (!emitSetOrInitializeDestructuring(lhs, flav)) {
        //          [stack] ... RHS
        return false;
      }

      MOZ_ASSERT(member == pattern->last(), "Rest property is always last");
      break;
    }

    // Now push the property name currently being matched, which is the
    // current property name "label" on the left of a colon in the object
    // initialiser.
    bool needsGetElem = true;

    if (member->isKind(ParseNodeKind::MutateProto)) {
      if (!emitAtomOp(cx->names().proto, JSOP_GETPROP)) {
        //          [stack] ... SET? RHS LREF* PROP
        return false;
      }
      needsGetElem = false;
    } else {
      MOZ_ASSERT(member->isKind(ParseNodeKind::PropertyDefinition) ||
                 member->isKind(ParseNodeKind::Shorthand));

      ParseNode* key = member->as<BinaryNode>().left();
      if (key->isKind(ParseNodeKind::NumberExpr)) {
        if (!emitNumberOp(key->as<NumericLiteral>().value())) {
          //        [stack]... SET? RHS LREF* RHS KEY
          return false;
        }
      } else if (key->isKind(ParseNodeKind::ObjectPropertyName) ||
                 key->isKind(ParseNodeKind::StringExpr)) {
        if (!emitAtomOp(key->as<NameNode>().atom(), JSOP_GETPROP,
                        ShouldInstrument::Yes)) {
          //        [stack] ... SET? RHS LREF* PROP
          return false;
        }
        needsGetElem = false;
      } else {
        if (!emitComputedPropertyName(&key->as<UnaryNode>())) {
          //        [stack] ... SET? RHS LREF* RHS KEY
          return false;
        }

        // Add the computed property key to the exclusion set.
        if (needsRestPropertyExcludedSet) {
          if (!emitDupAt(emitted + 3)) {
            //      [stack] ... SET RHS LREF* RHS KEY SET
            return false;
          }
          if (!emitDupAt(1)) {
            //      [stack] ... SET RHS LREF* RHS KEY SET KEY
            return false;
          }
          if (!emit1(JSOP_UNDEFINED)) {
            //      [stack] ... SET RHS LREF* RHS KEY SET KEY UNDEFINED
            return false;
          }
          if (!emit1(JSOP_INITELEM)) {
            //      [stack] ... SET RHS LREF* RHS KEY SET
            return false;
          }
          if (!emit1(JSOP_POP)) {
            //      [stack] ... SET RHS LREF* RHS KEY
            return false;
          }
        }
      }
    }

    // Get the property value if not done already.
    if (needsGetElem && !emitElemOpBase(JSOP_GETELEM)) {
      //            [stack] ... SET? RHS LREF* PROP
      return false;
    }

    if (subpattern->isKind(ParseNodeKind::AssignExpr)) {
      if (!emitDefault(subpattern->as<AssignmentNode>().right(), lhs)) {
        //          [stack] ... SET? RHS LREF* VALUE
        return false;
      }
    }

    // Destructure PROP per this member's lhs.
    if (!emitSetOrInitializeDestructuring(subpattern, flav)) {
      //            [stack] ... SET? RHS
      return false;
    }
  }

  return true;
}

bool BytecodeEmitter::emitDestructuringObjRestExclusionSet(ListNode* pattern) {
  MOZ_ASSERT(pattern->isKind(ParseNodeKind::ObjectExpr));
  MOZ_ASSERT(pattern->last()->isKind(ParseNodeKind::Spread));

  BytecodeOffset offset = bytecodeSection().offset();
  if (!emitNewInit()) {
    return false;
  }

  // Try to construct the shape of the object as we go, so we can emit a
  // JSOP_NEWOBJECT with the final shape instead.
  // In the case of computed property names and indices, we cannot fix the
  // shape at bytecode compile time. When the shape cannot be determined,
  // |obj| is nulled out.

  // No need to do any guessing for the object kind, since we know the upper
  // bound of how many properties we plan to have.
  gc::AllocKind kind = gc::GetGCObjectKind(pattern->count() - 1);
  RootedPlainObject obj(
      cx, NewBuiltinClassInstance<PlainObject>(cx, kind, TenuredObject));
  if (!obj) {
    return false;
  }

  RootedAtom pnatom(cx);
  for (ParseNode* member : pattern->contents()) {
    if (member->isKind(ParseNodeKind::Spread)) {
      break;
    }

    bool isIndex = false;
    if (member->isKind(ParseNodeKind::MutateProto)) {
      pnatom.set(cx->names().proto);
    } else {
      ParseNode* key = member->as<BinaryNode>().left();
      if (key->isKind(ParseNodeKind::NumberExpr)) {
        if (!emitNumberOp(key->as<NumericLiteral>().value())) {
          return false;
        }
        isIndex = true;
      } else if (key->isKind(ParseNodeKind::ObjectPropertyName) ||
                 key->isKind(ParseNodeKind::StringExpr)) {
        pnatom.set(key->as<NameNode>().atom());
      } else {
        // Otherwise this is a computed property name which needs to
        // be added dynamically.
        obj.set(nullptr);
        continue;
      }
    }

    // Initialize elements with |undefined|.
    if (!emit1(JSOP_UNDEFINED)) {
      return false;
    }

    if (isIndex) {
      obj.set(nullptr);
      if (!emit1(JSOP_INITELEM)) {
        return false;
      }
    } else {
      uint32_t index;
      if (!makeAtomIndex(pnatom, &index)) {
        return false;
      }

      if (obj) {
        MOZ_ASSERT(!obj->inDictionaryMode());
        Rooted<jsid> id(cx, AtomToId(pnatom));
        if (!NativeDefineDataProperty(cx, obj, id, UndefinedHandleValue,
                                      JSPROP_ENUMERATE)) {
          return false;
        }
        if (obj->inDictionaryMode()) {
          obj.set(nullptr);
        }
      }

      if (!emitIndex32(JSOP_INITPROP, index)) {
        return false;
      }
    }
  }

  if (obj) {
    // The object survived and has a predictable shape: update the
    // original bytecode.
    if (!replaceNewInitWithNewObject(obj, offset)) {
      return false;
    }
  }

  return true;
}

bool BytecodeEmitter::emitDestructuringOps(ListNode* pattern,
                                           DestructuringFlavor flav) {
  if (pattern->isKind(ParseNodeKind::ArrayExpr)) {
    return emitDestructuringOpsArray(pattern, flav);
  }
  return emitDestructuringOpsObject(pattern, flav);
}

bool BytecodeEmitter::emitTemplateString(ListNode* templateString) {
  bool pushedString = false;

  for (ParseNode* item : templateString->contents()) {
    bool isString = (item->getKind() == ParseNodeKind::StringExpr ||
                     item->getKind() == ParseNodeKind::TemplateStringExpr);

    // Skip empty strings. These are very common: a template string like
    // `${a}${b}` has three empty strings and without this optimization
    // we'd emit four JSOP_ADD operations instead of just one.
    if (isString && item->as<NameNode>().atom()->empty()) {
      continue;
    }

    if (!isString) {
      // We update source notes before emitting the expression
      if (!updateSourceCoordNotes(item->pn_pos.begin)) {
        return false;
      }
    }

    if (!emitTree(item)) {
      return false;
    }

    if (!isString) {
      // We need to convert the expression to a string
      if (!emit1(JSOP_TOSTRING)) {
        return false;
      }
    }

    if (pushedString) {
      // We've pushed two strings onto the stack. Add them together, leaving
      // just one.
      if (!emit1(JSOP_ADD)) {
        return false;
      }
    } else {
      pushedString = true;
    }
  }

  if (!pushedString) {
    // All strings were empty, this can happen for something like `${""}`.
    // Just push an empty string.
    if (!emitAtomOp(cx->names().empty, JSOP_STRING)) {
      return false;
    }
  }

  return true;
}

bool BytecodeEmitter::emitDeclarationList(ListNode* declList) {
  for (ParseNode* decl : declList->contents()) {
    ParseNode* pattern;
    ParseNode* initializer;
    if (decl->isKind(ParseNodeKind::Name)) {
      pattern = decl;
      initializer = nullptr;
    } else {
      AssignmentNode* assignNode = &decl->as<AssignmentNode>();
      pattern = assignNode->left();
      initializer = assignNode->right();
    }

    if (pattern->isKind(ParseNodeKind::Name)) {
      // initializer can be null here.
      if (!emitSingleDeclaration(declList, &pattern->as<NameNode>(),
                                 initializer)) {
        return false;
      }
    } else {
      MOZ_ASSERT(pattern->isKind(ParseNodeKind::ArrayExpr) ||
                 pattern->isKind(ParseNodeKind::ObjectExpr));
      MOZ_ASSERT(initializer != nullptr);

      if (!updateSourceCoordNotes(initializer->pn_pos.begin)) {
        return false;
      }
      if (!markStepBreakpoint()) {
        return false;
      }
      if (!emitTree(initializer)) {
        return false;
      }

      if (!emitDestructuringOps(&pattern->as<ListNode>(),
                                DestructuringFlavor::Declaration)) {
        return false;
      }

      if (!emit1(JSOP_POP)) {
        return false;
      }
    }
  }
  return true;
}

bool BytecodeEmitter::emitSingleDeclaration(ListNode* declList, NameNode* decl,
                                            ParseNode* initializer) {
  MOZ_ASSERT(decl->isKind(ParseNodeKind::Name));

  // Nothing to do for initializer-less 'var' declarations, as there's no TDZ.
  if (!initializer && declList->isKind(ParseNodeKind::VarStmt)) {
    return true;
  }

  RootedAtom nameAtom(cx, decl->name());
  NameOpEmitter noe(this, nameAtom, NameOpEmitter::Kind::Initialize);
  if (!noe.prepareForRhs()) {
    //              [stack] ENV?
    return false;
  }
  if (!initializer) {
    // Lexical declarations are initialized to undefined without an
    // initializer.
    MOZ_ASSERT(declList->isKind(ParseNodeKind::LetDecl),
               "var declarations without initializers handled above, "
               "and const declarations must have initializers");
    if (!emit1(JSOP_UNDEFINED)) {
      //            [stack] ENV? UNDEF
      return false;
    }
  } else {
    MOZ_ASSERT(initializer);

    if (!updateSourceCoordNotes(initializer->pn_pos.begin)) {
      return false;
    }
    if (!markStepBreakpoint()) {
      return false;
    }
    if (!emitInitializer(initializer, decl)) {
      //            [stack] ENV? V
      return false;
    }
  }
  if (!noe.emitAssignment()) {
    //              [stack] V
    return false;
  }
  if (!emit1(JSOP_POP)) {
    //              [stack]
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitAssignmentRhs(ParseNode* rhs,
                                        HandleAtom anonFunctionName) {
  if (rhs->isDirectRHSAnonFunction()) {
    if (anonFunctionName) {
      return emitAnonymousFunctionWithName(rhs, anonFunctionName);
    }
    return emitAnonymousFunctionWithComputedName(rhs, FunctionPrefixKind::None);
  }
  return emitTree(rhs);
}

// The RHS value to assign is already on the stack, i.e., the next enumeration
// value in a for-in or for-of loop. Offset is the location in the stack of the
// already-emitted rhs. If we emitted a BIND[G]NAME, then the scope is on the
// top of the stack and we need to dig one deeper to get the right RHS value.
bool BytecodeEmitter::emitAssignmentRhs(uint8_t offset) {
  if (offset != 1) {
    return emit2(JSOP_PICK, offset - 1);
  }

  return true;
}

static inline JSOp CompoundAssignmentParseNodeKindToJSOp(ParseNodeKind pnk) {
  switch (pnk) {
    case ParseNodeKind::InitExpr:
      return JSOP_NOP;
    case ParseNodeKind::AssignExpr:
      return JSOP_NOP;
    case ParseNodeKind::AddAssignExpr:
      return JSOP_ADD;
    case ParseNodeKind::SubAssignExpr:
      return JSOP_SUB;
    case ParseNodeKind::BitOrAssignExpr:
      return JSOP_BITOR;
    case ParseNodeKind::BitXorAssignExpr:
      return JSOP_BITXOR;
    case ParseNodeKind::BitAndAssignExpr:
      return JSOP_BITAND;
    case ParseNodeKind::LshAssignExpr:
      return JSOP_LSH;
    case ParseNodeKind::RshAssignExpr:
      return JSOP_RSH;
    case ParseNodeKind::UrshAssignExpr:
      return JSOP_URSH;
    case ParseNodeKind::MulAssignExpr:
      return JSOP_MUL;
    case ParseNodeKind::DivAssignExpr:
      return JSOP_DIV;
    case ParseNodeKind::ModAssignExpr:
      return JSOP_MOD;
    case ParseNodeKind::PowAssignExpr:
      return JSOP_POW;
    default:
      MOZ_CRASH("unexpected compound assignment op");
  }
}

bool BytecodeEmitter::emitAssignmentOrInit(ParseNodeKind kind, ParseNode* lhs,
                                           ParseNode* rhs) {
  JSOp compoundOp = CompoundAssignmentParseNodeKindToJSOp(kind);
  bool isCompound = compoundOp != JSOP_NOP;
  bool isInit = kind == ParseNodeKind::InitExpr;

  MOZ_ASSERT_IF(isInit, lhs->isKind(ParseNodeKind::DotExpr) ||
                            lhs->isKind(ParseNodeKind::ElemExpr));

  // Name assignments are handled separately because choosing ops and when
  // to emit BINDNAME is involved and should avoid duplication.
  if (lhs->isKind(ParseNodeKind::Name)) {
    NameNode* nameNode = &lhs->as<NameNode>();
    RootedAtom name(cx, nameNode->name());
    NameOpEmitter noe(this, name,
                      isCompound ? NameOpEmitter::Kind::CompoundAssignment
                                 : NameOpEmitter::Kind::SimpleAssignment);
    if (!noe.prepareForRhs()) {
      //            [stack] ENV? VAL?
      return false;
    }

    if (rhs) {
      if (!emitAssignmentRhs(rhs, name)) {
        //          [stack] ENV? VAL? RHS
        return false;
      }
    } else {
      uint8_t offset = noe.emittedBindOp() ? 2 : 1;
      // Assumption: Things with pre-emitted RHS values never need to be named.
      if (!emitAssignmentRhs(offset)) {
        //          [stack] ENV? VAL? RHS
        return false;
      }
    }

    // Emit the compound assignment op if there is one.
    if (isCompound) {
      if (!emit1(compoundOp)) {
        //          [stack] ENV? VAL
        return false;
      }
    }
    if (!noe.emitAssignment()) {
      //            [stack] VAL
      return false;
    }

    return true;
  }

  Maybe<PropOpEmitter> poe;
  Maybe<ElemOpEmitter> eoe;

  // Deal with non-name assignments.
  uint8_t offset = 1;

  RootedAtom anonFunctionName(cx);
  switch (lhs->getKind()) {
    case ParseNodeKind::DotExpr: {
      PropertyAccess* prop = &lhs->as<PropertyAccess>();
      bool isSuper = prop->isSuper();
      poe.emplace(this,
                  isCompound ? PropOpEmitter::Kind::CompoundAssignment
                             : isInit ? PropOpEmitter::Kind::PropInit
                                      : PropOpEmitter::Kind::SimpleAssignment,
                  isSuper ? PropOpEmitter::ObjKind::Super
                          : PropOpEmitter::ObjKind::Other);
      if (!poe->prepareForObj()) {
        return false;
      }
      anonFunctionName = &prop->name();
      if (isSuper) {
        UnaryNode* base = &prop->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {
          //        [stack] THIS SUPERBASE
          return false;
        }
        // SUPERBASE is pushed onto THIS later in poe->emitGet below.
        offset += 2;
      } else {
        if (!emitTree(&prop->expression())) {
          //        [stack] OBJ
          return false;
        }
        offset += 1;
      }
      break;
    }
    case ParseNodeKind::ElemExpr: {
      PropertyByValue* elem = &lhs->as<PropertyByValue>();
      bool isSuper = elem->isSuper();
      eoe.emplace(this,
                  isCompound ? ElemOpEmitter::Kind::CompoundAssignment
                             : isInit ? ElemOpEmitter::Kind::PropInit
                                      : ElemOpEmitter::Kind::SimpleAssignment,
                  isSuper ? ElemOpEmitter::ObjKind::Super
                          : ElemOpEmitter::ObjKind::Other);
      if (!emitElemObjAndKey(elem, isSuper, *eoe)) {
        //          [stack] # if Super
        //          [stack] THIS KEY
        //          [stack] # otherwise
        //          [stack] OBJ KEY
        return false;
      }
      if (isSuper) {
        // SUPERBASE is pushed onto KEY in eoe->emitGet below.
        offset += 3;
      } else {
        offset += 2;
      }
      break;
    }
    case ParseNodeKind::ArrayExpr:
    case ParseNodeKind::ObjectExpr:
      break;
    case ParseNodeKind::CallExpr:
      if (!emitTree(lhs)) {
        return false;
      }

      // Assignment to function calls is forbidden, but we have to make the
      // call first.  Now we can throw.
      if (!emitUint16Operand(JSOP_THROWMSG, JSMSG_ASSIGN_TO_CALL)) {
        return false;
      }

      // Rebalance the stack to placate stack-depth assertions.
      if (!emit1(JSOP_POP)) {
        return false;
      }
      break;
    default:
      MOZ_ASSERT(0);
  }

  if (isCompound) {
    MOZ_ASSERT(rhs);
    switch (lhs->getKind()) {
      case ParseNodeKind::DotExpr: {
        PropertyAccess* prop = &lhs->as<PropertyAccess>();
        if (!poe->emitGet(prop->key().atom())) {
          //        [stack] # if Super
          //        [stack] THIS SUPERBASE PROP
          //        [stack] # otherwise
          //        [stack] OBJ PROP
          return false;
        }
        break;
      }
      case ParseNodeKind::ElemExpr: {
        if (!eoe->emitGet()) {
          //        [stack] KEY THIS OBJ ELEM
          return false;
        }
        break;
      }
      case ParseNodeKind::CallExpr:
        // We just emitted a JSOP_THROWMSG and popped the call's return
        // value.  Push a random value to make sure the stack depth is
        // correct.
        if (!emit1(JSOP_NULL)) {
          //        [stack] NULL
          return false;
        }
        break;
      default:;
    }
  }

  switch (lhs->getKind()) {
    case ParseNodeKind::DotExpr:
      if (!poe->prepareForRhs()) {
        //          [stack] # if Simple Assignment with Super
        //          [stack] THIS SUPERBASE
        //          [stack] # if Simple Assignment with other
        //          [stack] OBJ
        //          [stack] # if Compound Assignment with Super
        //          [stack] THIS SUPERBASE PROP
        //          [stack] # if Compound Assignment with other
        //          [stack] OBJ PROP
        return false;
      }
      break;
    case ParseNodeKind::ElemExpr:
      if (!eoe->prepareForRhs()) {
        //          [stack] # if Simple Assignment with Super
        //          [stack] THIS KEY SUPERBASE
        //          [stack] # if Simple Assignment with other
        //          [stack] OBJ KEY
        //          [stack] # if Compound Assignment with Super
        //          [stack] THIS KEY SUPERBASE ELEM
        //          [stack] # if Compound Assignment with other
        //          [stack] OBJ KEY ELEM
        return false;
      }
      break;
    default:
      break;
  }

  // Purpose of anonFunctionName:
  // In normal property assignments (`obj.x = function(){}`), the anonymous
  // function does not have a computed name, and rhs->isDirectRHSAnonFunction()
  // will be false (and anonFunctionName will not be used). However, in field
  // initializers (`class C { x = function(){} }`), field initialization is
  // implemented via a property or elem assignment (where we are now), and
  // rhs->isDirectRHSAnonFunction() is set - so we'll assign the name of the
  // function.
  if (rhs) {
    if (!emitAssignmentRhs(rhs, anonFunctionName)) {
      //            [stack] ... VAL? RHS
      return false;
    }
  } else {
    // Assumption: Things with pre-emitted RHS values never need to be named.
    if (!emitAssignmentRhs(offset)) {
      //            [stack] ... VAL? RHS
      return false;
    }
  }

  /* If += etc., emit the binary operator with a source note. */
  if (isCompound) {
    if (!newSrcNote(SRC_ASSIGNOP)) {
      return false;
    }
    if (!emit1(compoundOp)) {
      //            [stack] ... VAL
      return false;
    }
  }

  /* Finally, emit the specialized assignment bytecode. */
  switch (lhs->getKind()) {
    case ParseNodeKind::DotExpr: {
      PropertyAccess* prop = &lhs->as<PropertyAccess>();
      if (!poe->emitAssignment(prop->key().atom())) {
        //          [stack] VAL
        return false;
      }

      poe.reset();
      break;
    }
    case ParseNodeKind::CallExpr:
      // We threw above, so nothing to do here.
      break;
    case ParseNodeKind::ElemExpr: {
      if (!eoe->emitAssignment()) {
        //          [stack] VAL
        return false;
      }

      eoe.reset();
      break;
    }
    case ParseNodeKind::ArrayExpr:
    case ParseNodeKind::ObjectExpr:
      if (!emitDestructuringOps(&lhs->as<ListNode>(),
                                DestructuringFlavor::Assignment)) {
        return false;
      }
      break;
    default:
      MOZ_ASSERT(0);
  }
  return true;
}

bool ParseNode::getConstantValue(JSContext* cx,
                                 AllowConstantObjects allowObjects,
                                 MutableHandleValue vp, Value* compare,
                                 size_t ncompare, NewObjectKind newKind) {
  MOZ_ASSERT(newKind == TenuredObject || newKind == SingletonObject);

  switch (getKind()) {
    case ParseNodeKind::NumberExpr:
      vp.setNumber(as<NumericLiteral>().value());
      return true;
    case ParseNodeKind::BigIntExpr:
      vp.setBigInt(as<BigIntLiteral>().value());
      return true;
    case ParseNodeKind::TemplateStringExpr:
    case ParseNodeKind::StringExpr:
      vp.setString(as<NameNode>().atom());
      return true;
    case ParseNodeKind::TrueExpr:
      vp.setBoolean(true);
      return true;
    case ParseNodeKind::FalseExpr:
      vp.setBoolean(false);
      return true;
    case ParseNodeKind::NullExpr:
      vp.setNull();
      return true;
    case ParseNodeKind::RawUndefinedExpr:
      vp.setUndefined();
      return true;
    case ParseNodeKind::CallSiteObj:
    case ParseNodeKind::ArrayExpr: {
      unsigned count;
      ParseNode* pn;

      if (allowObjects == DontAllowObjects) {
        vp.setMagic(JS_GENERIC_MAGIC);
        return true;
      }

      ObjectGroup::NewArrayKind arrayKind = ObjectGroup::NewArrayKind::Normal;
      if (allowObjects == ForCopyOnWriteArray) {
        arrayKind = ObjectGroup::NewArrayKind::CopyOnWrite;
        allowObjects = DontAllowObjects;
      }

      if (getKind() == ParseNodeKind::CallSiteObj) {
        count = as<CallSiteNode>().count() - 1;
        pn = as<CallSiteNode>().head()->pn_next;
      } else {
        MOZ_ASSERT(!as<ListNode>().hasNonConstInitializer());
        count = as<ListNode>().count();
        pn = as<ListNode>().head();
      }

      RootedValueVector values(cx);
      if (!values.appendN(MagicValue(JS_ELEMENTS_HOLE), count)) {
        return false;
      }
      size_t idx;
      for (idx = 0; pn; idx++, pn = pn->pn_next) {
        if (!pn->getConstantValue(cx, allowObjects, values[idx], values.begin(),
                                  idx)) {
          return false;
        }
        if (values[idx].isMagic(JS_GENERIC_MAGIC)) {
          vp.setMagic(JS_GENERIC_MAGIC);
          return true;
        }
      }
      MOZ_ASSERT(idx == count);

      ArrayObject* obj = ObjectGroup::newArrayObject(
          cx, values.begin(), values.length(), newKind, arrayKind);
      if (!obj) {
        return false;
      }

      if (!CombineArrayElementTypes(cx, obj, compare, ncompare)) {
        return false;
      }

      vp.setObject(*obj);
      return true;
    }
    case ParseNodeKind::ObjectExpr: {
      MOZ_ASSERT(!as<ListNode>().hasNonConstInitializer());

      if (allowObjects == DontAllowObjects) {
        vp.setMagic(JS_GENERIC_MAGIC);
        return true;
      }
      MOZ_ASSERT(allowObjects == AllowObjects);

      Rooted<IdValueVector> properties(cx, IdValueVector(cx));

      RootedValue value(cx), idvalue(cx);
      for (ParseNode* item : as<ListNode>().contents()) {
        // MutateProto and Spread, both are unary, cannot appear here.
        BinaryNode* prop = &item->as<BinaryNode>();
        if (!prop->right()->getConstantValue(cx, allowObjects, &value)) {
          return false;
        }
        if (value.isMagic(JS_GENERIC_MAGIC)) {
          vp.setMagic(JS_GENERIC_MAGIC);
          return true;
        }

        ParseNode* key = prop->left();
        if (key->isKind(ParseNodeKind::NumberExpr)) {
          idvalue = NumberValue(key->as<NumericLiteral>().value());
        } else {
          MOZ_ASSERT(key->isKind(ParseNodeKind::ObjectPropertyName) ||
                     key->isKind(ParseNodeKind::StringExpr));
          MOZ_ASSERT(key->as<NameNode>().atom() != cx->names().proto);
          idvalue = StringValue(key->as<NameNode>().atom());
        }

        RootedId id(cx);
        if (!ValueToId<CanGC>(cx, idvalue, &id)) {
          return false;
        }

        if (!properties.append(IdValuePair(id, value))) {
          return false;
        }
      }

      JSObject* obj = ObjectGroup::newPlainObject(cx, properties.begin(),
                                                  properties.length(), newKind);
      if (!obj) {
        return false;
      }

      if (!CombinePlainObjectPropertyTypes(cx, obj, compare, ncompare)) {
        return false;
      }

      vp.setObject(*obj);
      return true;
    }
    default:
      MOZ_CRASH("Unexpected node");
  }
  return false;
}

bool BytecodeEmitter::emitSingletonInitialiser(ListNode* objOrArray) {
  NewObjectKind newKind = (objOrArray->getKind() == ParseNodeKind::ObjectExpr)
                              ? SingletonObject
                              : TenuredObject;

  RootedValue value(cx);
  if (!objOrArray->getConstantValue(cx, ParseNode::AllowObjects, &value,
                                    nullptr, 0, newKind)) {
    return false;
  }

  MOZ_ASSERT_IF(newKind == SingletonObject, value.toObject().isSingleton());

  ObjectBox* objbox = parser->newObjectBox(&value.toObject());
  if (!objbox) {
    return false;
  }

  return emitObjectOp(objbox, JSOP_OBJECT);
}

bool BytecodeEmitter::emitCallSiteObject(CallSiteNode* callSiteObj) {
  RootedValue value(cx);
  if (!callSiteObj->getConstantValue(cx, ParseNode::AllowObjects, &value)) {
    return false;
  }

  MOZ_ASSERT(value.isObject());

  ObjectBox* objbox1 = parser->newObjectBox(&value.toObject());
  if (!objbox1) {
    return false;
  }

  if (!callSiteObj->getRawArrayValue(cx, &value)) {
    return false;
  }

  MOZ_ASSERT(value.isObject());

  ObjectBox* objbox2 = parser->newObjectBox(&value.toObject());
  if (!objbox2) {
    return false;
  }

  hasCallSiteObj = true;
  return emitObjectPairOp(objbox1, objbox2, JSOP_CALLSITEOBJ);
}

bool BytecodeEmitter::emitCatch(BinaryNode* catchClause) {
  // We must be nested under a try-finally statement.
  MOZ_ASSERT(innermostNestableControl->is<TryFinallyControl>());

  ParseNode* param = catchClause->left();
  if (!param) {
    // Catch parameter was omitted; just discard the exception.
    if (!emit1(JSOP_POP)) {
      return false;
    }
  } else {
    switch (param->getKind()) {
      case ParseNodeKind::ArrayExpr:
      case ParseNodeKind::ObjectExpr:
        if (!emitDestructuringOps(&param->as<ListNode>(),
                                  DestructuringFlavor::Declaration)) {
          return false;
        }
        if (!emit1(JSOP_POP)) {
          return false;
        }
        break;

      case ParseNodeKind::Name:
        if (!emitLexicalInitialization(&param->as<NameNode>())) {
          return false;
        }
        if (!emit1(JSOP_POP)) {
          return false;
        }
        break;

      default:
        MOZ_ASSERT(0);
    }
  }

  /* Emit the catch body. */
  return emitTree(catchClause->right());
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See the
// comment on EmitSwitch.
MOZ_NEVER_INLINE bool BytecodeEmitter::emitTry(TryNode* tryNode) {
  LexicalScopeNode* catchScope = tryNode->catchScope();
  ParseNode* finallyNode = tryNode->finallyBlock();

  TryEmitter::Kind kind;
  if (catchScope) {
    if (finallyNode) {
      kind = TryEmitter::Kind::TryCatchFinally;
    } else {
      kind = TryEmitter::Kind::TryCatch;
    }
  } else {
    MOZ_ASSERT(finallyNode);
    kind = TryEmitter::Kind::TryFinally;
  }
  TryEmitter tryCatch(this, kind, TryEmitter::ControlKind::Syntactic);

  if (!tryCatch.emitTry()) {
    return false;
  }

  if (!emitTree(tryNode->body())) {
    return false;
  }

  // If this try has a catch block, emit it.
  if (catchScope) {
    // The emitted code for a catch block looks like:
    //
    // [pushlexicalenv]             only if any local aliased
    // exception
    // setlocal 0; pop              assign or possibly destructure exception
    // < catch block contents >
    // debugleaveblock
    // [poplexicalenv]              only if any local aliased
    // if there is a finally block:
    //   gosub <finally>
    //   goto <after finally>
    if (!tryCatch.emitCatch()) {
      return false;
    }

    // Emit the lexical scope and catch body.
    if (!emitTree(catchScope)) {
      return false;
    }
  }

  // Emit the finally handler, if there is one.
  if (finallyNode) {
    if (!tryCatch.emitFinally(Some(finallyNode->pn_pos.begin))) {
      return false;
    }

    if (!emitTree(finallyNode)) {
      return false;
    }
  }

  if (!tryCatch.emitEnd()) {
    return false;
  }

  return true;
}

MOZ_MUST_USE bool BytecodeEmitter::emitGoSub(JumpList* jump) {
  // Emit the following:
  //
  //   JSOP_FALSE
  //   JSOP_RESUMEINDEX <resumeIndex>
  //   JSOP_GOSUB <target>
  //  resumeOffset:
  //   JSOP_JUMPTARGET
  //
  // The order is important: the Baseline Interpreter relies on JSOP_JUMPTARGET
  // setting the frame's ICEntry when resuming at resumeOffset.

  if (!emit1(JSOP_FALSE)) {
    return false;
  }

  BytecodeOffset off;
  if (!emitN(JSOP_RESUMEINDEX, 3, &off)) {
    return false;
  }

  if (!emitJumpNoFallthrough(JSOP_GOSUB, jump)) {
    return false;
  }

  uint32_t resumeIndex;
  if (!allocateResumeIndex(bytecodeSection().offset(), &resumeIndex)) {
    return false;
  }

  SET_RESUMEINDEX(bytecodeSection().code(off), resumeIndex);

  JumpTarget target;
  return emitJumpTarget(&target);
}

bool BytecodeEmitter::emitIf(TernaryNode* ifNode) {
  IfEmitter ifThenElse(this);

  if (!ifThenElse.emitIf(Some(ifNode->kid1()->pn_pos.begin))) {
    return false;
  }

if_again:
  if (!markStepBreakpoint()) {
    return false;
  }

  /* Emit code for the condition before pushing stmtInfo. */
  if (!emitTree(ifNode->kid1())) {
    return false;
  }

  ParseNode* elseNode = ifNode->kid3();
  if (elseNode) {
    if (!ifThenElse.emitThenElse()) {
      return false;
    }
  } else {
    if (!ifThenElse.emitThen()) {
      return false;
    }
  }

  /* Emit code for the then part. */
  if (!emitTree(ifNode->kid2())) {
    return false;
  }

  if (elseNode) {
    if (elseNode->isKind(ParseNodeKind::IfStmt)) {
      ifNode = &elseNode->as<TernaryNode>();

      if (!ifThenElse.emitElseIf(Some(ifNode->kid1()->pn_pos.begin))) {
        return false;
      }

      goto if_again;
    }

    if (!ifThenElse.emitElse()) {
      return false;
    }

    /* Emit code for the else part. */
    if (!emitTree(elseNode)) {
      return false;
    }
  }

  if (!ifThenElse.emitEnd()) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitHoistedFunctionsInList(ListNode* stmtList) {
  MOZ_ASSERT(stmtList->hasTopLevelFunctionDeclarations());

  // We can call this multiple times for sloppy eval scopes.
  if (stmtList->emittedTopLevelFunctionDeclarations()) {
    return true;
  }

  stmtList->setEmittedTopLevelFunctionDeclarations();

  for (ParseNode* stmt : stmtList->contents()) {
    ParseNode* maybeFun = stmt;

    if (!sc->strict()) {
      while (maybeFun->isKind(ParseNodeKind::LabelStmt)) {
        maybeFun = maybeFun->as<LabeledStatement>().statement();
      }
    }

    if (maybeFun->is<FunctionNode>() &&
        maybeFun->as<FunctionNode>().functionIsHoisted()) {
      if (!emitTree(maybeFun)) {
        return false;
      }
    }
  }

  return true;
}

bool BytecodeEmitter::emitLexicalScopeBody(
    ParseNode* body, EmitLineNumberNote emitLineNote /* = EMIT_LINENOTE */) {
  if (body->isKind(ParseNodeKind::StatementList) &&
      body->as<ListNode>().hasTopLevelFunctionDeclarations()) {
    // This block contains function statements whose definitions are
    // hoisted to the top of the block. Emit these as a separate pass
    // before the rest of the block.
    if (!emitHoistedFunctionsInList(&body->as<ListNode>())) {
      return false;
    }
  }

  // Line notes were updated by emitLexicalScope or emitScript.
  return emitTree(body, ValueUsage::WantValue, emitLineNote);
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool BytecodeEmitter::emitLexicalScope(
    LexicalScopeNode* lexicalScope) {
  LexicalScopeEmitter lse(this);

  ParseNode* body = lexicalScope->scopeBody();
  if (lexicalScope->isEmptyScope()) {
    if (!lse.emitEmptyScope()) {
      return false;
    }

    if (!emitLexicalScopeBody(body)) {
      return false;
    }

    if (!lse.emitEnd()) {
      return false;
    }

    return true;
  }

  // We are about to emit some bytecode for what the spec calls "declaration
  // instantiation". Assign these instructions to the opening `{` of the
  // block. (Using the location of each declaration we're instantiating is
  // too weird when stepping in the debugger.)
  if (!ParseNodeRequiresSpecialLineNumberNotes(body)) {
    if (!updateSourceCoordNotes(lexicalScope->pn_pos.begin)) {
      return false;
    }
  }

  ScopeKind kind;
  if (body->isKind(ParseNodeKind::Catch)) {
    BinaryNode* catchNode = &body->as<BinaryNode>();
    kind =
        (!catchNode->left() || catchNode->left()->isKind(ParseNodeKind::Name))
            ? ScopeKind::SimpleCatch
            : ScopeKind::Catch;
  } else {
    kind = lexicalScope->kind();
  }

  if (!lse.emitScope(kind, lexicalScope->scopeBindings())) {
    return false;
  }

  if (body->isKind(ParseNodeKind::ForStmt)) {
    // for loops need to emit {FRESHEN,RECREATE}LEXICALENV if there are
    // lexical declarations in the head. Signal this by passing a
    // non-nullptr lexical scope.
    if (!emitFor(&body->as<ForNode>(), &lse.emitterScope())) {
      return false;
    }
  } else {
    if (!emitLexicalScopeBody(body, SUPPRESS_LINENOTE)) {
      return false;
    }
  }

  if (!lse.emitEnd()) {
    return false;
  }
  return true;
}

bool BytecodeEmitter::emitWith(BinaryNode* withNode) {
  // Ensure that the column of the 'with' is set properly.
  if (!updateSourceCoordNotes(withNode->left()->pn_pos.begin)) {
    return false;
  }

  if (!markStepBreakpoint()) {
    return false;
  }

  if (!emitTree(withNode->left())) {
    return false;
  }

  EmitterScope emitterScope(this);
  if (!emitterScope.enterWith(this)) {
    return false;
  }

  if (!emitTree(withNode->right())) {
    return false;
  }

  return emitterScope.leave(this);
}

bool BytecodeEmitter::emitCopyDataProperties(CopyOption option) {
  DebugOnly<int32_t> depth = bytecodeSection().stackDepth();

  uint32_t argc;
  if (option == CopyOption::Filtered) {
    MOZ_ASSERT(depth > 2);
    //              [stack] TARGET SOURCE SET
    argc = 3;

    if (!emitAtomOp(cx->names().CopyDataProperties, JSOP_GETINTRINSIC)) {
      //            [stack] TARGET SOURCE SET COPYDATAPROPERTIES
      return false;
    }
  } else {
    MOZ_ASSERT(depth > 1);
    //              [stack] TARGET SOURCE
    argc = 2;

    if (!emitAtomOp(cx->names().CopyDataPropertiesUnfiltered,
                    JSOP_GETINTRINSIC)) {
      //            [stack] TARGET SOURCE COPYDATAPROPERTIES
      return false;
    }
  }

  if (!emit1(JSOP_UNDEFINED)) {
    //              [stack] TARGET SOURCE SET? COPYDATAPROPERTIES
    //                    UNDEFINED
    return false;
  }
  if (!emit2(JSOP_PICK, argc + 1)) {
    //              [stack] SOURCE SET? COPYDATAPROPERTIES UNDEFINED
    //                    TARGET
    return false;
  }
  if (!emit2(JSOP_PICK, argc + 1)) {
    //              [stack] SET? COPYDATAPROPERTIES UNDEFINED TARGET
    //                    SOURCE
    return false;
  }
  if (option == CopyOption::Filtered) {
    if (!emit2(JSOP_PICK, argc + 1)) {
      //            [stack] COPYDATAPROPERTIES UNDEFINED TARGET SOURCE SET
      return false;
    }
  }
  if (!emitCall(JSOP_CALL_IGNORES_RV, argc)) {
    //              [stack] IGNORED
    return false;
  }

  if (!emit1(JSOP_POP)) {
    //              [stack]
    return false;
  }

  MOZ_ASSERT(depth - int(argc) == bytecodeSection().stackDepth());
  return true;
}

bool BytecodeEmitter::emitBigIntOp(BigIntLiteral* bigint) {
  uint32_t index;
  if (!perScriptData().gcThingList().append(bigint, &index)) {
    return false;
  }
  return emitIndex32(JSOP_BIGINT, index);
}

bool BytecodeEmitter::emitIterator() {
  // Convert iterable to iterator.
  if (!emit1(JSOP_DUP)) {
    //              [stack] OBJ OBJ
    return false;
  }
  if (!emit2(JSOP_SYMBOL, uint8_t(JS::SymbolCode::iterator))) {
    //              [stack] OBJ OBJ @@ITERATOR
    return false;
  }
  if (!emitElemOpBase(JSOP_CALLELEM)) {
    //              [stack] OBJ ITERFN
    return false;
  }
  if (!emit1(JSOP_SWAP)) {
    //              [stack] ITERFN OBJ
    return false;
  }
  if (!emitCall(JSOP_CALLITER, 0)) {
    //              [stack] ITER
    return false;
  }
  if (!emitCheckIsObj(CheckIsObjectKind::GetIterator)) {
    //              [stack] ITER
    return false;
  }
  if (!emit1(JSOP_DUP)) {
    //              [stack] ITER ITER
    return false;
  }
  if (!emitAtomOp(cx->names().next, JSOP_GETPROP)) {
    //              [stack] ITER NEXT
    return false;
  }
  if (!emit1(JSOP_SWAP)) {
    //              [stack] NEXT ITER
    return false;
  }
  return true;
}

bool BytecodeEmitter::emitAsyncIterator() {
  // Convert iterable to iterator.
  if (!emit1(JSOP_DUP)) {
    //              [stack] OBJ OBJ
    return false;
  }
  if (!emit2(JSOP_SYMBOL, uint8_t(JS::SymbolCode::asyncIterator))) {
    //              [stack] OBJ OBJ @@ASYNCITERATOR
    return false;
  }
  if (!emitElemOpBase(JSOP_CALLELEM)) {
    //              [stack] OBJ ITERFN
    return false;
  }

  InternalIfEmitter ifAsyncIterIsUndefined(this);
  if (!emitPushNotUndefinedOrNull()) {
    //              [stack] OBJ ITERFN !UNDEF-OR-NULL
    return false;
  }
  if (!emit1(JSOP_NOT)) {
    //              [stack] OBJ ITERFN UNDEF-OR-NULL
    return false;
  }
  if (!ifAsyncIterIsUndefined.emitThenElse()) {
    //              [stack] OBJ ITERFN
    return false;
  }

  if (!emit1(JSOP_POP)) {
    //              [stack] OBJ
    return false;
  }
  if (!emit1(JSOP_DUP)) {
    //              [stack] OBJ OBJ
    return false;
  }
  if (!emit2(JSOP_SYMBOL, uint8_t(JS::SymbolCode::iterator))) {
    //              [stack] OBJ OBJ @@ITERATOR
    return false;
  }
  if (!emitElemOpBase(JSOP_CALLELEM)) {
    //              [stack] OBJ ITERFN
    return false;
  }
  if (!emit1(JSOP_SWAP)) {
    //              [stack] ITERFN OBJ
    return false;
  }
  if (!emitCall(JSOP_CALLITER, 0)) {
    //              [stack] ITER
    return false;
  }
  if (!emitCheckIsObj(CheckIsObjectKind::GetIterator)) {
    //              [stack] ITER
    return false;
  }

  if (!emit1(JSOP_DUP)) {
    //              [stack] ITER ITER
    return false;
  }
  if (!emitAtomOp(cx->names().next, JSOP_GETPROP)) {
    //              [stack] ITER SYNCNEXT
    return false;
  }

  if (!emit1(JSOP_TOASYNCITER)) {
    //              [stack] ITER
    return false;
  }

  if (!ifAsyncIterIsUndefined.emitElse()) {
    //              [stack] OBJ ITERFN
    return false;
  }

  if (!emit1(JSOP_SWAP)) {
    //              [stack] ITERFN OBJ
    return false;
  }
  if (!emitCall(JSOP_CALLITER, 0)) {
    //              [stack] ITER
    return false;
  }
  if (!emitCheckIsObj(CheckIsObjectKind::GetIterator)) {
    //              [stack] ITER
    return false;
  }

  if (!ifAsyncIterIsUndefined.emitEnd()) {
    //              [stack] ITER
    return false;
  }

  if (!emit1(JSOP_DUP)) {
    //              [stack] ITER ITER
    return false;
  }
  if (!emitAtomOp(cx->names().next, JSOP_GETPROP)) {
    //              [stack] ITER NEXT
    return false;
  }
  if (!emit1(JSOP_SWAP)) {
    //              [stack] NEXT ITER
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitSpread(bool allowSelfHosted) {
  LoopControl loopInfo(this, StatementKind::Spread);

  // Jump down to the loop condition to minimize overhead assuming at least
  // one iteration, as the other loop forms do.  Annotate so IonMonkey can
  // find the loop-closing jump.
  unsigned noteIndex;
  if (!newSrcNote(SRC_FOR_OF, &noteIndex)) {
    return false;
  }

  // Jump down to the loop condition to minimize overhead, assuming at least
  // one iteration.  (This is also what we do for loops; whether this
  // assumption holds for spreads is an unanswered question.)
  if (!loopInfo.emitEntryJump(this)) {
    //              [stack] NEXT ITER ARR I (during the goto)
    return false;
  }

  if (!loopInfo.emitLoopHead(this, Nothing())) {
    //              [stack] NEXT ITER ARR I
    return false;
  }

  // When we enter the goto above, we have NEXT ITER ARR I on the stack. But
  // when we reach this point on the loop backedge (if spreading produces at
  // least one value), we've additionally pushed a RESULT iteration value.
  // Increment manually to reflect this.
  bytecodeSection().setStackDepth(bytecodeSection().stackDepth() + 1);

  {
#ifdef DEBUG
    auto loopDepth = bytecodeSection().stackDepth();
#endif

    // Emit code to assign result.value to the iteration variable.
    if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) {
      //            [stack] NEXT ITER ARR I VALUE
      return false;
    }
    if (!emit1(JSOP_INITELEM_INC)) {
      //            [stack] NEXT ITER ARR (I+1)
      return false;
    }

    MOZ_ASSERT(bytecodeSection().stackDepth() == loopDepth - 1);

    // Spread operations can't contain |continue|, so don't bother setting loop
    // and enclosing "update" offsets, as we do with for-loops.

    // COME FROM the beginning of the loop to here.
    if (!loopInfo.emitLoopEntry(this, Nothing())) {
      //            [stack] NEXT ITER ARR I
      return false;
    }

    if (!emitDupAt(3, 2)) {
      //            [stack] NEXT ITER ARR I NEXT
      return false;
    }
    if (!emitIteratorNext(Nothing(), IteratorKind::Sync, allowSelfHosted)) {
      //            [stack] ITER ARR I RESULT
      return false;
    }
    if (!emit1(JSOP_DUP)) {
      //            [stack] NEXT ITER ARR I RESULT RESULT
      return false;
    }
    if (!emitAtomOp(cx->names().done, JSOP_GETPROP)) {
      //            [stack] NEXT ITER ARR I RESULT DONE
      return false;
    }

    if (!loopInfo.emitLoopEnd(this, JSOP_IFEQ)) {
      //            [stack] NEXT ITER ARR I RESULT
      return false;
    }

    MOZ_ASSERT(bytecodeSection().stackDepth() == loopDepth);
  }

  // Let Ion know where the closing jump of this loop is.
  if (!setSrcNoteOffset(noteIndex, SrcNote::ForOf::BackJumpOffset,
                        loopInfo.loopEndOffsetFromEntryJump())) {
    return false;
  }

  // No breaks or continues should occur in spreads.
  MOZ_ASSERT(!loopInfo.breaks.offset.valid());
  MOZ_ASSERT(!loopInfo.continues.offset.valid());

  if (!addTryNote(JSTRY_FOR_OF, bytecodeSection().stackDepth(),
                  loopInfo.headOffset(), loopInfo.breakTargetOffset())) {
    return false;
  }

  if (!emit2(JSOP_PICK, 4)) {
    //              [stack] ITER ARR FINAL_INDEX RESULT NEXT
    return false;
  }
  if (!emit2(JSOP_PICK, 4)) {
    //              [stack] ARR FINAL_INDEX RESULT NEXT ITER
    return false;
  }

  return emitPopN(3);
  //                [stack] ARR FINAL_INDEX
}

bool BytecodeEmitter::emitInitializeForInOrOfTarget(TernaryNode* forHead) {
  MOZ_ASSERT(forHead->isKind(ParseNodeKind::ForIn) ||
             forHead->isKind(ParseNodeKind::ForOf));

  MOZ_ASSERT(bytecodeSection().stackDepth() >= 1,
             "must have a per-iteration value for initializing");

  ParseNode* target = forHead->kid1();
  MOZ_ASSERT(!forHead->kid2());

  // If the for-in/of loop didn't have a variable declaration, per-loop
  // initialization is just assigning the iteration value to a target
  // expression.
  if (!parser->astGenerator().isDeclarationList(target)) {
    return emitAssignmentOrInit(ParseNodeKind::AssignExpr, target, nullptr);
    //              [stack] ... ITERVAL
  }

  // Otherwise, per-loop initialization is (possibly) declaration
  // initialization.  If the declaration is a lexical declaration, it must be
  // initialized.  If the declaration is a variable declaration, an
  // assignment to that name (which does *not* necessarily assign to the
  // variable!) must be generated.

  if (!updateSourceCoordNotes(target->pn_pos.begin)) {
    return false;
  }

  MOZ_ASSERT(target->isForLoopDeclaration());
  target = parser->astGenerator().singleBindingFromDeclaration(
      &target->as<ListNode>());

  NameNode* nameNode = nullptr;
  if (target->isKind(ParseNodeKind::Name)) {
    nameNode = &target->as<NameNode>();
  } else if (target->isKind(ParseNodeKind::AssignExpr) ||
             target->isKind(ParseNodeKind::InitExpr)) {
    BinaryNode* assignNode = &target->as<BinaryNode>();
    if (assignNode->left()->is<NameNode>()) {
      nameNode = &assignNode->left()->as<NameNode>();
    }
  }

  if (nameNode) {
    RootedAtom nameAtom(cx, nameNode->name());
    NameOpEmitter noe(this, nameAtom, NameOpEmitter::Kind::Initialize);
    if (!noe.prepareForRhs()) {
      return false;
    }
    if (noe.emittedBindOp()) {
      // Per-iteration initialization in for-in/of loops computes the
      // iteration value *before* initializing.  Thus the initializing
      // value may be buried under a bind-specific value on the stack.
      // Swap it to the top of the stack.
      MOZ_ASSERT(bytecodeSection().stackDepth() >= 2);
      if (!emit1(JSOP_SWAP)) {
        return false;
      }
    } else {
      // In cases of emitting a frame slot or environment slot,
      // nothing needs be done.
      MOZ_ASSERT(bytecodeSection().stackDepth() >= 1);
    }
    if (!noe.emitAssignment()) {
      return false;
    }

    // The caller handles removing the iteration value from the stack.
    return true;
  }

  MOZ_ASSERT(
      !target->isKind(ParseNodeKind::AssignExpr) &&
          !target->isKind(ParseNodeKind::InitExpr),
      "for-in/of loop destructuring declarations can't have initializers");

  MOZ_ASSERT(target->isKind(ParseNodeKind::ArrayExpr) ||
             target->isKind(ParseNodeKind::ObjectExpr));
  return emitDestructuringOps(&target->as<ListNode>(),
                              DestructuringFlavor::Declaration);
}

bool BytecodeEmitter::emitForOf(ForNode* forOfLoop,
                                const EmitterScope* headLexicalEmitterScope) {
  MOZ_ASSERT(forOfLoop->isKind(ParseNodeKind::ForStmt));

  TernaryNode* forOfHead = forOfLoop->head();
  MOZ_ASSERT(forOfHead->isKind(ParseNodeKind::ForOf));

  unsigned iflags = forOfLoop->iflags();
  IteratorKind iterKind =
      (iflags & JSITER_FORAWAITOF) ? IteratorKind::Async : IteratorKind::Sync;
  MOZ_ASSERT_IF(iterKind == IteratorKind::Async, sc->asFunctionBox());
  MOZ_ASSERT_IF(iterKind == IteratorKind::Async,
                sc->asFunctionBox()->isAsync());

  ParseNode* forHeadExpr = forOfHead->kid3();

  // Certain builtins (e.g. Array.from) are implemented in self-hosting
  // as for-of loops.
  bool allowSelfHostedIter = false;
  if (emitterMode == BytecodeEmitter::SelfHosting &&
      forHeadExpr->isKind(ParseNodeKind::CallExpr) &&
      forHeadExpr->as<BinaryNode>().left()->isName(
          cx->names().allowContentIter)) {
    allowSelfHostedIter = true;
  }

  ForOfEmitter forOf(this, headLexicalEmitterScope, allowSelfHostedIter,
                     iterKind);

  if (!forOf.emitIterated()) {
    //              [stack]
    return false;
  }

  if (!updateSourceCoordNotes(forHeadExpr->pn_pos.begin)) {
    return false;
  }
  if (!markStepBreakpoint()) {
    return false;
  }
  if (!emitTree(forHeadExpr)) {
    //              [stack] ITERABLE
    return false;
  }

  if (headLexicalEmitterScope) {
    DebugOnly<ParseNode*> forOfTarget = forOfHead->kid1();
    MOZ_ASSERT(forOfTarget->isKind(ParseNodeKind::LetDecl) ||
               forOfTarget->isKind(ParseNodeKind::ConstDecl));
  }

  if (!forOf.emitInitialize(Some(forOfHead->pn_pos.begin))) {
    //              [stack] NEXT ITER VALUE
    return false;
  }

  if (!emitInitializeForInOrOfTarget(forOfHead)) {
    //              [stack] NEXT ITER VALUE
    return false;
  }

  if (!forOf.emitBody()) {
    //              [stack] NEXT ITER UNDEF
    return false;
  }

  // Perform the loop body.
  ParseNode* forBody = forOfLoop->body();
  if (!emitTree(forBody)) {
    //              [stack] NEXT ITER UNDEF
    return false;
  }

  if (!forOf.emitEnd(Some(forHeadExpr->pn_pos.begin))) {
    //              [stack]
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitForIn(ForNode* forInLoop,
                                const EmitterScope* headLexicalEmitterScope) {
  TernaryNode* forInHead = forInLoop->head();
  MOZ_ASSERT(forInHead->isKind(ParseNodeKind::ForIn));

  ForInEmitter forIn(this, headLexicalEmitterScope);

  // Annex B: Evaluate the var-initializer expression if present.
  // |for (var i = initializer in expr) { ... }|
  ParseNode* forInTarget = forInHead->kid1();
  if (parser->astGenerator().isDeclarationList(forInTarget)) {
    ParseNode* decl = parser->astGenerator().singleBindingFromDeclaration(
        &forInTarget->as<ListNode>());
    if (decl->isKind(ParseNodeKind::AssignExpr) ||
        decl->isKind(ParseNodeKind::InitExpr)) {
      BinaryNode* assignNode = &decl->as<BinaryNode>();
      if (assignNode->left()->is<NameNode>()) {
        NameNode* nameNode = &assignNode->left()->as<NameNode>();
        ParseNode* initializer = assignNode->right();
        MOZ_ASSERT(
            forInTarget->isKind(ParseNodeKind::VarStmt),
            "for-in initializers are only permitted for |var| declarations");

        if (!updateSourceCoordNotes(decl->pn_pos.begin)) {
          return false;
        }

        RootedAtom nameAtom(cx, nameNode->name());
        NameOpEmitter noe(this, nameAtom, NameOpEmitter::Kind::Initialize);
        if (!noe.prepareForRhs()) {
          return false;
        }
        if (!emitInitializer(initializer, nameNode)) {
          return false;
        }
        if (!noe.emitAssignment()) {
          return false;
        }

        // Pop the initializer.
        if (!emit1(JSOP_POP)) {
          return false;
        }
      }
    }
  }

  if (!forIn.emitIterated()) {
    //              [stack]
    return false;
  }

  // Evaluate the expression being iterated.
  ParseNode* expr = forInHead->kid3();

  if (!updateSourceCoordNotes(expr->pn_pos.begin)) {
    return false;
  }
  if (!markStepBreakpoint()) {
    return false;
  }
  if (!emitTree(expr)) {
    //              [stack] EXPR
    return false;
  }

  MOZ_ASSERT(forInLoop->iflags() == 0);

  MOZ_ASSERT_IF(headLexicalEmitterScope,
                forInTarget->isKind(ParseNodeKind::LetDecl) ||
                    forInTarget->isKind(ParseNodeKind::ConstDecl));

  if (!forIn.emitInitialize()) {
    //              [stack] ITER ITERVAL
    return false;
  }

  if (!emitInitializeForInOrOfTarget(forInHead)) {
    //              [stack] ITER ITERVAL
    return false;
  }

  if (!forIn.emitBody()) {
    //              [stack] ITER ITERVAL
    return false;
  }

  // Perform the loop body.
  ParseNode* forBody = forInLoop->body();
  if (!emitTree(forBody)) {
    //              [stack] ITER ITERVAL
    return false;
  }

  if (!forIn.emitEnd(Some(forInHead->pn_pos.begin))) {
    //              [stack]
    return false;
  }

  return true;
}

/* C-style `for (init; cond; update) ...` loop. */
bool BytecodeEmitter::emitCStyleFor(
    ForNode* forNode, const EmitterScope* headLexicalEmitterScope) {
  TernaryNode* forHead = forNode->head();
  ParseNode* forBody = forNode->body();
  ParseNode* init = forHead->kid1();
  ParseNode* cond = forHead->kid2();
  ParseNode* update = forHead->kid3();
  bool isLet = init && init->isKind(ParseNodeKind::LetDecl);

  CForEmitter cfor(this, isLet ? headLexicalEmitterScope : nullptr);

  if (!cfor.emitInit(init ? Some(init->pn_pos.begin) : Nothing())) {
    //              [stack]
    return false;
  }

  // If the head of this for-loop declared any lexical variables, the parser
  // wrapped this ParseNodeKind::For node in a ParseNodeKind::LexicalScope
  // representing the implicit scope of those variables. By the time we get
  // here, we have already entered that scope. So far, so good.
  if (init) {
    // Emit the `init` clause, whether it's an expression or a variable
    // declaration. (The loop variables were hoisted into an enclosing
    // scope, but we still need to emit code for the initializers.)
    if (init->isForLoopDeclaration()) {
      if (!emitTree(init)) {
        //          [stack]
        return false;
      }
    } else {
      if (!updateSourceCoordNotes(init->pn_pos.begin)) {
        return false;
      }
      if (!markStepBreakpoint()) {
        return false;
      }

      // 'init' is an expression, not a declaration. emitTree left its
      // value on the stack.
      if (!emitTree(init, ValueUsage::IgnoreValue)) {
        //          [stack] VAL
        return false;
      }
      if (!emit1(JSOP_POP)) {
        //          [stack]
        return false;
      }
    }
  }

  if (!cfor.emitBody(
          cond ? CForEmitter::Cond::Present : CForEmitter::Cond::Missing,
          getOffsetForLoop(forBody))) {
    //              [stack]
    return false;
  }

  if (!emitTree(forBody)) {
    //              [stack]
    return false;
  }

  if (!cfor.emitUpdate(
          update ? CForEmitter::Update::Present : CForEmitter::Update::Missing,
          update ? Some(update->pn_pos.begin) : Nothing())) {
    //              [stack]
    return false;
  }

  // Check for update code to do before the condition (if any).
  if (update) {
    if (!updateSourceCoordNotes(update->pn_pos.begin)) {
      return false;
    }
    if (!markStepBreakpoint()) {
      return false;
    }
    if (!emitTree(update, ValueUsage::IgnoreValue)) {
      //            [stack] VAL
      return false;
    }
  }

  if (!cfor.emitCond(Some(forNode->pn_pos.begin),
                     cond ? Some(cond->pn_pos.begin) : Nothing(),
                     Some(forNode->pn_pos.end))) {
    //              [stack]
    return false;
  }

  if (cond) {
    if (!updateSourceCoordNotes(cond->pn_pos.begin)) {
      return false;
    }
    if (!markStepBreakpoint()) {
      return false;
    }
    if (!emitTree(cond)) {
      //            [stack] VAL
      return false;
    }
  }

  if (!cfor.emitEnd()) {
    //              [stack]
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitFor(ForNode* forNode,
                              const EmitterScope* headLexicalEmitterScope) {
  if (forNode->head()->isKind(ParseNodeKind::ForHead)) {
    return emitCStyleFor(forNode, headLexicalEmitterScope);
  }

  if (!updateLineNumberNotes(forNode->pn_pos.begin)) {
    return false;
  }

  if (forNode->head()->isKind(ParseNodeKind::ForIn)) {
    return emitForIn(forNode, headLexicalEmitterScope);
  }

  MOZ_ASSERT(forNode->head()->isKind(ParseNodeKind::ForOf));
  return emitForOf(forNode, headLexicalEmitterScope);
}

MOZ_NEVER_INLINE bool BytecodeEmitter::emitFunction(
    FunctionNode* funNode, bool needsProto /* = false */,
    ListNode* classContentsIfConstructor /* = nullptr */) {
  FunctionBox* funbox = funNode->funbox();

  MOZ_ASSERT((classContentsIfConstructor != nullptr) ==
             (funbox->kind() == FunctionFlags::FunctionKind::ClassConstructor));

  //                [stack]

  FunctionEmitter fe(this, funbox, funNode->syntaxKind(),
                     funNode->functionIsHoisted()
                         ? FunctionEmitter::IsHoisted::Yes
                         : FunctionEmitter::IsHoisted::No);

  // Set the |wasEmitted| flag in the funbox once the function has been
  // emitted. Function definitions that need hoisting to the top of the
  // function will be seen by emitFunction in two places.
  if (funbox->wasEmitted) {
    if (!fe.emitAgain()) {
      //            [stack]
      return false;
    }
    MOZ_ASSERT(funNode->functionIsHoisted());
    return true;
  }

  if (funbox->isInterpreted()) {
    if (funbox->isInterpretedLazy()) {
      if (!fe.emitLazy()) {
        //          [stack] FUN?
        return false;
      }

      if (classContentsIfConstructor) {
        funbox->setFieldInitializers(
            setupFieldInitializers(classContentsIfConstructor));
      }

      return true;
    }

    if (!fe.prepareForNonLazy()) {
      //            [stack]
      return false;
    }

    // Inherit most things (principals, version, etc) from the
    // parent.  Use default values for the rest.
    Rooted<JSScript*> parent(cx, script);
    MOZ_ASSERT(parent->mutedErrors() == parser->options().mutedErrors());
    const JS::TransitiveCompileOptions& transitiveOptions = parser->options();
    JS::CompileOptions options(cx, transitiveOptions);

    Rooted<ScriptSourceObject*> sourceObject(cx, script->sourceObject());
    RootedFunction fun(cx, funbox->function());
    RootedScript innerScript(
        cx, JSScript::Create(cx, fun, options, sourceObject, funbox->bufStart,
                             funbox->bufEnd, funbox->toStringStart,
                             funbox->toStringEnd));
    if (!innerScript) {
      return false;
    }

    EmitterMode nestedMode = emitterMode;
    if (nestedMode == BytecodeEmitter::LazyFunction) {
      MOZ_ASSERT(lazyScript->isBinAST());
      nestedMode = BytecodeEmitter::Normal;
    }

    FieldInitializers fieldInitializers = FieldInitializers::Invalid();
    if (classContentsIfConstructor) {
      fieldInitializers = setupFieldInitializers(classContentsIfConstructor);
    }

    BytecodeEmitter bce2(this, parser, funbox, innerScript,
                         /* lazyScript = */ nullptr, funbox->startLine,
                         funbox->startColumn, nestedMode, fieldInitializers);
    if (!bce2.init(funNode->pn_pos)) {
      return false;
    }

    /* We measured the max scope depth when we parsed the function. */
    if (!bce2.emitFunctionScript(funNode, TopLevelFunction::No)) {
      return false;
    }

    // fieldInitializers are copied to the JSScript inside BytecodeEmitter

    if (funbox->isLikelyConstructorWrapper()) {
      innerScript->setLikelyConstructorWrapper();
    }

    if (!fe.emitNonLazyEnd()) {
      //            [stack] FUN?
      return false;
    }

    return true;
  }

  if (!fe.emitAsmJSModule()) {
    //              [stack]
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitDo(BinaryNode* doNode) {
  ParseNode* bodyNode = doNode->left();

  DoWhileEmitter doWhile(this);
  if (!doWhile.emitBody(Some(doNode->pn_pos.begin),
                        getOffsetForLoop(bodyNode))) {
    return false;
  }

  if (!emitTree(bodyNode)) {
    return false;
  }

  if (!doWhile.emitCond()) {
    return false;
  }

  ParseNode* condNode = doNode->right();
  if (!updateSourceCoordNotes(condNode->pn_pos.begin)) {
    return false;
  }
  if (!markStepBreakpoint()) {
    return false;
  }
  if (!emitTree(condNode)) {
    return false;
  }

  if (!doWhile.emitEnd()) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitWhile(BinaryNode* whileNode) {
  ParseNode* bodyNode = whileNode->right();

  WhileEmitter wh(this);
  if (!wh.emitBody(Some(whileNode->pn_pos.begin), getOffsetForLoop(bodyNode),
                   Some(whileNode->pn_pos.end))) {
    return false;
  }

  if (!emitTree(bodyNode)) {
    return false;
  }

  ParseNode* condNode = whileNode->left();
  if (!wh.emitCond(getOffsetForLoop(condNode))) {
    return false;
  }

  if (!updateSourceCoordNotes(condNode->pn_pos.begin)) {
    return false;
  }
  if (!markStepBreakpoint()) {
    return false;
  }
  if (!emitTree(condNode)) {
    return false;
  }

  if (!wh.emitEnd()) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitBreak(PropertyName* label) {
  BreakableControl* target;
  SrcNoteType noteType;
  if (label) {
    // Any statement with the matching label may be the break target.
    auto hasSameLabel = [label](LabelControl* labelControl) {
      return labelControl->label() == label;
    };
    target = findInnermostNestableControl<LabelControl>(hasSameLabel);
    noteType = SRC_BREAK2LABEL;
  } else {
    auto isNotLabel = [](BreakableControl* control) {
      return !control->is<LabelControl>();
    };
    target = findInnermostNestableControl<BreakableControl>(isNotLabel);
    noteType =
        (target->kind() == StatementKind::Switch) ? SRC_SWITCHBREAK : SRC_BREAK;
  }

  return emitGoto(target, &target->breaks, noteType);
}

bool BytecodeEmitter::emitContinue(PropertyName* label) {
  LoopControl* target = nullptr;
  if (label) {
    // Find the loop statement enclosed by the matching label.
    NestableControl* control = innermostNestableControl;
    while (!control->is<LabelControl>() ||
           control->as<LabelControl>().label() != label) {
      if (control->is<LoopControl>()) {
        target = &control->as<LoopControl>();
      }
      control = control->enclosing();
    }
  } else {
    target = findInnermostNestableControl<LoopControl>();
  }
  return emitGoto(target, &target->continues, SRC_CONTINUE);
}

bool BytecodeEmitter::emitGetFunctionThis(NameNode* thisName) {
  MOZ_ASSERT(sc->thisBinding() == ThisBinding::Function);
  MOZ_ASSERT(thisName->isName(cx->names().dotThis));

  return emitGetFunctionThis(Some(thisName->pn_pos.begin));
}

bool BytecodeEmitter::emitGetFunctionThis(
    const mozilla::Maybe<uint32_t>& offset) {
  if (offset) {
    if (!updateLineNumberNotes(*offset)) {
      return false;
    }
  }

  if (!emitGetName(cx->names().dotThis)) {
    //              [stack] THIS
    return false;
  }
  if (sc->needsThisTDZChecks()) {
    if (!emit1(JSOP_CHECKTHIS)) {
      //            [stack] THIS
      return false;
    }
  }

  return true;
}

bool BytecodeEmitter::emitGetThisForSuperBase(UnaryNode* superBase) {
  MOZ_ASSERT(superBase->isKind(ParseNodeKind::SuperBase));
  NameNode* nameNode = &superBase->kid()->as<NameNode>();
  return emitGetFunctionThis(nameNode);
  //                [stack] THIS
}

bool BytecodeEmitter::emitThisLiteral(ThisLiteral* pn) {
  if (ParseNode* kid = pn->kid()) {
    NameNode* thisName = &kid->as<NameNode>();
    return emitGetFunctionThis(thisName);
    //              [stack] THIS
  }

  if (sc->thisBinding() == ThisBinding::Module) {
    return emit1(JSOP_UNDEFINED);
    //              [stack] UNDEF
  }

  MOZ_ASSERT(sc->thisBinding() == ThisBinding::Global);
  return emit1(JSOP_GLOBALTHIS);
  //                [stack] THIS
}

bool BytecodeEmitter::emitCheckDerivedClassConstructorReturn() {
  MOZ_ASSERT(lookupName(cx->names().dotThis).hasKnownSlot());
  if (!emitGetName(cx->names().dotThis)) {
    return false;
  }
  if (!emit1(JSOP_CHECKRETURN)) {
    return false;
  }
  return true;
}

bool BytecodeEmitter::emitReturn(UnaryNode* returnNode) {
  if (!updateSourceCoordNotes(returnNode->pn_pos.begin)) {
    return false;
  }

  bool needsIteratorResult =
      sc->isFunctionBox() && sc->asFunctionBox()->needsIteratorResult();
  if (needsIteratorResult) {
    if (!emitPrepareIteratorResult()) {
      return false;
    }
  }

  if (!updateSourceCoordNotes(returnNode->pn_pos.begin)) {
    return false;
  }
  if (!markStepBreakpoint()) {
    return false;
  }

  /* Push a return value */
  if (ParseNode* expr = returnNode->kid()) {
    if (!emitTree(expr)) {
      return false;
    }

    if (sc->asFunctionBox()->isAsync() && sc->asFunctionBox()->isGenerator()) {
      if (!emitAwaitInInnermostScope()) {
        return false;
      }
    }
  } else {
    /* No explicit return value provided */
    if (!emit1(JSOP_UNDEFINED)) {
      return false;
    }
  }

  if (needsIteratorResult) {
    if (!emitFinishIteratorResult(true)) {
      return false;
    }
  }

  // We know functionBodyEndPos is set because "return" is only
  // valid in a function, and so we've passed through
  // emitFunctionScript.
  if (!updateSourceCoordNotes(*functionBodyEndPos)) {
    return false;
  }

  /*
   * EmitNonLocalJumpFixup may add fixup bytecode to close open try
   * blocks having finally clauses and to exit intermingled let blocks.
   * We can't simply transfer control flow to our caller in that case,
   * because we must gosub to those finally clauses from inner to outer,
   * with the correct stack pointer (i.e., after popping any with,
   * for/in, etc., slots nested inside the finally's try).
   *
   * In this case we mutate JSOP_RETURN into JSOP_SETRVAL and add an
   * extra JSOP_RETRVAL after the fixups.
   */
  BytecodeOffset top = bytecodeSection().offset();

  bool needsFinalYield =
      sc->isFunctionBox() && sc->asFunctionBox()->needsFinalYield();
  bool isDerivedClassConstructor =
      sc->isFunctionBox() && sc->asFunctionBox()->isDerivedClassConstructor();

  if (!emit1((needsFinalYield || isDerivedClassConstructor) ? JSOP_SETRVAL
                                                            : JSOP_RETURN)) {
    return false;
  }

  // Make sure that we emit this before popping the blocks in
  // prepareForNonLocalJump, to ensure that the error is thrown while the
  // scope-chain is still intact.
  if (isDerivedClassConstructor) {
    if (!emitCheckDerivedClassConstructorReturn()) {
      return false;
    }
  }

  NonLocalExitControl nle(this, NonLocalExitControl::Return);

  if (!nle.prepareForNonLocalJumpToOutermost()) {
    return false;
  }

  if (needsFinalYield) {
    // We know that .generator is on the function scope, as we just exited
    // all nested scopes.
    NameLocation loc = *locationOfNameBoundInFunctionScope(
        cx->names().dotGenerator, varEmitterScope);

    // Resolve the return value before emitting the final yield.
    if (sc->asFunctionBox()->needsPromiseResult()) {
      if (!emit1(JSOP_GETRVAL)) {
        //          [stack] RVAL
        return false;
      }
      if (!emitGetNameAtLocation(cx->names().dotGenerator, loc)) {
        //          [stack] RVAL GEN
        return false;
      }
      if (!emit2(JSOP_ASYNCRESOLVE,
                 uint8_t(AsyncFunctionResolveKind::Fulfill))) {
        //          [stack] PROMISE
        return false;
      }
      if (!emit1(JSOP_SETRVAL)) {
        //          [stack]
        return false;
      }
    }

    if (!emitGetNameAtLocation(cx->names().dotGenerator, loc)) {
      return false;
    }
    if (!emitYieldOp(JSOP_FINALYIELDRVAL)) {
      return false;
    }
  } else if (isDerivedClassConstructor) {
    MOZ_ASSERT(bytecodeSection().code()[top.value()] == JSOP_SETRVAL);
    if (!emitReturnRval()) {
      return false;
    }
  } else if (top + BytecodeOffsetDiff(JSOP_RETURN_LENGTH) !=
                 bytecodeSection().offset() ||
             // If we are instrumenting, make sure we use RETRVAL and add any
             // instrumentation for the frame exit.
             instrumentationKinds) {
    bytecodeSection().code()[top.value()] = JSOP_SETRVAL;
    if (!emitReturnRval()) {
      return false;
    }
  }

  return true;
}

bool BytecodeEmitter::emitGetDotGeneratorInScope(EmitterScope& currentScope) {
  NameLocation loc = *locationOfNameBoundInFunctionScope(
      cx->names().dotGenerator, &currentScope);
  return emitGetNameAtLocation(cx->names().dotGenerator, loc);
}

bool BytecodeEmitter::emitInitialYield(UnaryNode* yieldNode) {
  if (!emitTree(yieldNode->kid())) {
    return false;
  }

  if (!emitYieldOp(JSOP_INITIALYIELD)) {
    return false;
  }

  if (!emit1(JSOP_POP)) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitYield(UnaryNode* yieldNode) {
  MOZ_ASSERT(sc->isFunctionBox());
  MOZ_ASSERT(sc->asFunctionBox()->isGenerator());
  MOZ_ASSERT(yieldNode->isKind(ParseNodeKind::YieldExpr));

  bool needsIteratorResult = sc->asFunctionBox()->needsIteratorResult();
  if (needsIteratorResult) {
    if (!emitPrepareIteratorResult()) {
      //            [stack] ITEROBJ
      return false;
    }
  }
  if (ParseNode* expr = yieldNode->kid()) {
    if (!emitTree(expr)) {
      //            [stack] ITEROBJ? VAL
      return false;
    }
  } else {
    if (!emit1(JSOP_UNDEFINED)) {
      //            [stack] ITEROBJ? UNDEFINED
      return false;
    }
  }

  // 25.5.3.7 AsyncGeneratorYield step 5.
  if (sc->asFunctionBox()->isAsync()) {
    MOZ_ASSERT(!needsIteratorResult);
    if (!emitAwaitInInnermostScope()) {
      //            [stack] RESULT
      return false;
    }
  }

  if (needsIteratorResult) {
    if (!emitFinishIteratorResult(false)) {
      //            [stack] ITEROBJ
      return false;
    }
  }

  if (!emitGetDotGeneratorInInnermostScope()) {
    //              [stack] # if needsIteratorResult
    //              [stack] ITEROBJ .GENERATOR
    //              [stack] # else
    //              [stack] RESULT .GENERATOR
    return false;
  }

  if (!emitYieldOp(JSOP_YIELD)) {
    //              [stack] YIELDRESULT
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitAwaitInInnermostScope(UnaryNode* awaitNode) {
  MOZ_ASSERT(sc->isFunctionBox());
  MOZ_ASSERT(awaitNode->isKind(ParseNodeKind::AwaitExpr));

  if (!emitTree(awaitNode->kid())) {
    return false;
  }
  return emitAwaitInInnermostScope();
}

bool BytecodeEmitter::emitAwaitInScope(EmitterScope& currentScope) {
  if (!emit1(JSOP_TRYSKIPAWAIT)) {
    //              [stack] VALUE_OR_RESOLVED CANSKIP
    return false;
  }

  if (!emit1(JSOP_NOT)) {
    //              [stack] VALUE_OR_RESOLVED !CANSKIP
    return false;
  }

  InternalIfEmitter ifCanSkip(this);
  if (!ifCanSkip.emitThen()) {
    //              [stack] VALUE_OR_RESOLVED
    return false;
  }

  if (sc->asFunctionBox()->needsPromiseResult()) {
    if (!emitGetDotGeneratorInScope(currentScope)) {
      //            [stack] VALUE GENERATOR
      return false;
    }
    if (!emit1(JSOP_ASYNCAWAIT)) {
      //            [stack] PROMISE
      return false;
    }
  }

  if (!emitGetDotGeneratorInScope(currentScope)) {
    //              [stack] VALUE|PROMISE GENERATOR
    return false;
  }
  if (!emitYieldOp(JSOP_AWAIT)) {
    //              [stack] RESOLVED
    return false;
  }

  if (!ifCanSkip.emitEnd()) {
    return false;
  }

  MOZ_ASSERT(ifCanSkip.popped() == 0);

  return true;
}

// ES2019 draft rev 49b781ec80117b60f73327ef3054703a3111e40c
// 14.4.14 Runtime Semantics: Evaluation
// YieldExpression : yield* AssignmentExpression
bool BytecodeEmitter::emitYieldStar(ParseNode* iter) {
  MOZ_ASSERT(sc->isFunctionBox());
  MOZ_ASSERT(sc->asFunctionBox()->isGenerator());

  // Step 1.
  IteratorKind iterKind =
      sc->asFunctionBox()->isAsync() ? IteratorKind::Async : IteratorKind::Sync;
  bool needsIteratorResult = sc->asFunctionBox()->needsIteratorResult();

  // Steps 2-5.
  if (!emitTree(iter)) {
    //              [stack] ITERABLE
    return false;
  }
  if (iterKind == IteratorKind::Async) {
    if (!emitAsyncIterator()) {
      //            [stack] NEXT ITER
      return false;
    }
  } else {
    if (!emitIterator()) {
      //            [stack] NEXT ITER
      return false;
    }
  }

  // Step 6.
  // Initial send value is undefined.
  if (!emit1(JSOP_UNDEFINED)) {
    //              [stack] NEXT ITER RECEIVED
    return false;
  }

  int32_t savedDepthTemp;
  int32_t startDepth = bytecodeSection().stackDepth();
  MOZ_ASSERT(startDepth >= 3);

  TryEmitter tryCatch(this, TryEmitter::Kind::TryCatchFinally,
                      TryEmitter::ControlKind::NonSyntactic);
  if (!tryCatch.emitJumpOverCatchAndFinally()) {
    //              [stack] NEXT ITER RESULT
    return false;
  }

  JumpTarget tryStart;
  if (!emitJumpTarget(&tryStart)) {
    //              [stack] NEXT ITER RESULT
    return false;
  }

  if (iterKind == IteratorKind::Async) {
    // Step 7.a.vi.
    // Step 7.b.ii.7.
    // Step 7.c.ix.
    //   Call to IteratorValue.
    if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) {
      //            [stack] NEXT ITER RESULT
      return false;
    }
  }

  if (!tryCatch.emitTry()) {
    //              [stack] NEXT ITER RESULT
    return false;
  }

  MOZ_ASSERT(bytecodeSection().stackDepth() == startDepth);

  // Step 7.a.vi.
  // Step 7.b.ii.7.
  // Step 7.c.ix.
  //   25.5.3.7 AsyncGeneratorYield, step 5.
  if (iterKind == IteratorKind::Async) {
    if (!emitAwaitInInnermostScope()) {
      //            [stack] NEXT ITER RESULT
      return false;
    }
  }

  // Steps 7.a.vi-vii.
  // Steps 7.b.ii.7-8.
  // Steps 7.c.ix-x.
  // Load the generator object.
  if (!emitGetDotGeneratorInInnermostScope()) {
    //              [stack] NEXT ITER RESULT GENOBJ
    return false;
  }

  // Yield RESULT as-is, without re-boxing.
  if (!emitYieldOp(JSOP_YIELD)) {
    //              [stack] NEXT ITER RECEIVED
    return false;
  }

  if (!tryCatch.emitCatch()) {
    //              [stack] NEXT ITER RESULT EXCEPTION
    return false;
  }

  // The exception was already pushed by emitCatch().
  MOZ_ASSERT(bytecodeSection().stackDepth() == startDepth + 1);

  if (!emitDupAt(2)) {
    //              [stack] NEXT ITER RESULT EXCEPTION ITER
    return false;
  }
  if (!emit1(JSOP_DUP)) {
    //              [stack] NEXT ITER RESULT EXCEPTION ITER ITER
    return false;
  }
  if (!emitAtomOp(cx->names().throw_, JSOP_CALLPROP)) {
    //              [stack] NEXT ITER RESULT EXCEPTION ITER THROW
    return false;
  }

  savedDepthTemp = bytecodeSection().stackDepth();
  InternalIfEmitter ifThrowMethodIsNotDefined(this);
  if (!emitPushNotUndefinedOrNull()) {
    //              [stack] NEXT ITER RESULT EXCEPTION ITER THROW
    //                    NOT-UNDEF-OR-NULL
    return false;
  }

  if (!ifThrowMethodIsNotDefined.emitThenElse()) {
    //              [stack] NEXT ITER RESULT EXCEPTION ITER THROW
    return false;
  }

  //                [stack] NEXT ITER OLDRESULT EXCEPTION ITER THROW

  // Step 7.b.ii.1.
  // RESULT = ITER.throw(EXCEPTION)
  if (!emit1(JSOP_SWAP)) {
    //              [stack] NEXT ITER OLDRESULT EXCEPTION THROW ITER
    return false;
  }
  if (!emit2(JSOP_PICK, 2)) {
    //              [stack] NEXT ITER OLDRESULT THROW ITER EXCEPTION
    return false;
  }
  if (!emitCall(JSOP_CALL, 1, iter)) {
    //              [stack] NEXT ITER OLDRESULT RESULT
    return false;
  }

  // Step 7.b.ii.2.
  if (iterKind == IteratorKind::Async) {
    if (!emitAwaitInInnermostScope()) {
      //            [stack] NEXT ITER OLDRESULT RESULT
      return false;
    }
  }

  // Step 7.b.ii.4.
  if (!emitCheckIsObj(CheckIsObjectKind::IteratorThrow)) {
    //              [stack] NEXT ITER OLDRESULT RESULT
    return false;
  }
  if (!emit1(JSOP_SWAP)) {
    //              [stack] NEXT ITER RESULT OLDRESULT
    return false;
  }
  if (!emit1(JSOP_POP)) {
    //              [stack] NEXT ITER RESULT
    return false;
  }
  MOZ_ASSERT(bytecodeSection().stackDepth() == startDepth);

  JumpList checkResult;
  // Note that there is no GOSUB to the finally block here. If the iterator has
  // a "throw" method, it does not perform IteratorClose.
  if (!emitJump(JSOP_GOTO, &checkResult)) {
    //              [stack] NEXT ITER RESULT
    //              [stack] # goto checkResult
    return false;
  }

  bytecodeSection().setStackDepth(savedDepthTemp);
  if (!ifThrowMethodIsNotDefined.emitElse()) {
    //              [stack] NEXT ITER RESULT EXCEPTION ITER THROW
    return false;
  }

  if (!emit1(JSOP_POP)) {
    //              [stack] NEXT ITER RESULT EXCEPTION ITER
    return false;
  }
  // Steps 7.b.iii.1-4.
  //
  // If the iterator does not have a "throw" method, it calls IteratorClose
  // and then throws a TypeError.
  if (!emitIteratorCloseInInnermostScope(iterKind)) {
    //              [stack] NEXT ITER RESULT EXCEPTION
    return false;
  }
  // Steps 7.b.iii.5-6.
  if (!emitUint16Operand(JSOP_THROWMSG, JSMSG_ITERATOR_NO_THROW)) {
    //              [stack] NEXT ITER RESULT EXCEPTION
    //              [stack] # throw
    return false;
  }

  bytecodeSection().setStackDepth(savedDepthTemp);
  if (!ifThrowMethodIsNotDefined.emitEnd()) {
    return false;
  }

  bytecodeSection().setStackDepth(startDepth);
  if (!tryCatch.emitFinally()) {
    return false;
  }

  // Step 7.c.i.
  //
  // Call iterator.return() for receiving a "forced return" completion from
  // the generator.

  InternalIfEmitter ifGeneratorClosing(this);
  if (!emit1(JSOP_ISGENCLOSING)) {
    //              [stack] NEXT ITER RESULT FTYPE FVALUE CLOSING
    return false;
  }
  if (!ifGeneratorClosing.emitThen()) {
    //              [stack] NEXT ITER RESULT FTYPE FVALUE
    return false;
  }

  // Step 7.c.ii.
  //
  // Get the "return" method.
  if (!emitDupAt(3)) {
    //              [stack] NEXT ITER RESULT FTYPE FVALUE ITER
    return false;
  }
  if (!emit1(JSOP_DUP)) {
    //              [stack] NEXT ITER RESULT FTYPE FVALUE ITER ITER
    return false;
  }
  if (!emitAtomOp(cx->names().return_, JSOP_CALLPROP)) {
    //              [stack] NEXT ITER RESULT FTYPE FVALUE ITER RET
    return false;
  }

  // Step 7.c.iii.
  //
  // Do nothing if "return" is undefined or null.
  InternalIfEmitter ifReturnMethodIsDefined(this);
  if (!emitPushNotUndefinedOrNull()) {
    //              [stack] NEXT ITER RESULT FTYPE FVALUE ITER RET
    //                    NOT-UNDEF-OR-NULL
    return false;
  }

  // Step 7.c.iv.
  //
  // Call "return" with the argument passed to Generator.prototype.return,
  // which is currently in rval.value.
  if (!ifReturnMethodIsDefined.emitThenElse()) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE ITER RET
    return false;
  }
  if (!emit1(JSOP_SWAP)) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE RET ITER
    return false;
  }
  if (!emit1(JSOP_GETRVAL)) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE RET ITER RVAL
    return false;
  }
  if (needsIteratorResult) {
    if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) {
      //            [stack] NEXT ITER OLDRESULT FTYPE FVALUE RET ITER
      //                  VALUE
      return false;
    }
  }
  if (!emitCall(JSOP_CALL, 1)) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE RESULT
    return false;
  }

  // Step 7.c.v.
  if (iterKind == IteratorKind::Async) {
    if (!emitAwaitInInnermostScope()) {
      //            [stack] ... FTYPE FVALUE RESULT
      return false;
    }
  }

  // Step 7.c.vi.
  if (!emitCheckIsObj(CheckIsObjectKind::IteratorReturn)) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE RESULT
    return false;
  }

  // Steps 7.c.vii-x.
  //
  // Check if the returned object from iterator.return() is done. If not,
  // continuing yielding.
  InternalIfEmitter ifReturnDone(this);
  if (!emit1(JSOP_DUP)) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE RESULT RESULT
    return false;
  }
  if (!emitAtomOp(cx->names().done, JSOP_GETPROP)) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE RESULT DONE
    return false;
  }
  if (!ifReturnDone.emitThenElse()) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE RESULT
    return false;
  }
  if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE VALUE
    return false;
  }
  if (needsIteratorResult) {
    if (!emitPrepareIteratorResult()) {
      //            [stack] NEXT ITER OLDRESULT FTYPE FVALUE VALUE RESULT
      return false;
    }
    if (!emit1(JSOP_SWAP)) {
      //            [stack] NEXT ITER OLDRESULT FTYPE FVALUE RESULT VALUE
      return false;
    }
    if (!emitFinishIteratorResult(true)) {
      //            [stack] NEXT ITER OLDRESULT FTYPE FVALUE RESULT
      return false;
    }
  }
  if (!emit1(JSOP_SETRVAL)) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE
    return false;
  }
  savedDepthTemp = bytecodeSection().stackDepth();
  if (!ifReturnDone.emitElse()) {
    //              [stack] NEXT ITER OLDRESULT FTYPE FVALUE RESULT
    return false;
  }
  if (!emit2(JSOP_UNPICK, 3)) {
    //              [stack] NEXT ITER RESULT OLDRESULT FTYPE FVALUE
    return false;
  }
  if (!emitPopN(3)) {
    //              [stack] NEXT ITER RESULT
    return false;
  }
  {
    // goto tryStart;
    JumpList beq;
    JumpTarget breakTarget;
    if (!emitBackwardJump(JSOP_GOTO, tryStart, &beq, &breakTarget)) {
      //            [stack] NEXT ITER RESULT
      return false;
    }
  }
  bytecodeSection().setStackDepth(savedDepthTemp);
  if (!ifReturnDone.emitEnd()) {
    return false;
  }

  if (!ifReturnMethodIsDefined.emitElse()) {
    //              [stack] NEXT ITER RESULT FTYPE FVALUE ITER RET
    return false;
  }
  if (!emitPopN(2)) {
    //              [stack] NEXT ITER RESULT FTYPE FVALUE
    return false;
  }
  if (iterKind == IteratorKind::Async) {
    // Step 7.c.iii.2.
    if (!emit1(JSOP_GETRVAL)) {
      //            [stack] NEXT ITER RESULT FTYPE FVALUE RVAL
      return false;
    }
    if (!emitAwaitInInnermostScope()) {
      //            [stack] NEXT ITER RESULT FTYPE FVALUE NEWRVAL
      return false;
    }
    if (!emit1(JSOP_SETRVAL)) {
      //            [stack] NEXT ITER RESULT FTYPE FVALUE
      return false;
    }
  }
  if (!ifReturnMethodIsDefined.emitEnd()) {
    return false;
  }

  if (!ifGeneratorClosing.emitEnd()) {
    return false;
  }

  if (!tryCatch.emitEnd()) {
    return false;
  }

  //                [stack] NEXT ITER RECEIVED

  // Step 7.a.i.
  // After the try-catch-finally block: send the received value to the iterator.
  // result = iter.next(received)
  if (!emit2(JSOP_UNPICK, 2)) {
    //              [stack] RECEIVED NEXT ITER
    return false;
  }
  if (!emit1(JSOP_DUP2)) {
    //              [stack] RECEIVED NEXT ITER NEXT ITER
    return false;
  }
  if (!emit2(JSOP_PICK, 4)) {
    //              [stack] NEXT ITER NEXT ITER RECEIVED
    return false;
  }
  if (!emitCall(JSOP_CALL, 1, iter)) {
    //              [stack] NEXT ITER RESULT
    return false;
  }

  // Step 7.a.ii.
  if (iterKind == IteratorKind::Async) {
    if (!emitAwaitInInnermostScope()) {
      //            [stack] NEXT ITER RESULT
      return false;
    }
  }

  // Step 7.a.iii.
  if (!emitCheckIsObj(CheckIsObjectKind::IteratorNext)) {
    //              [stack] NEXT ITER RESULT
    return false;
  }
  MOZ_ASSERT(bytecodeSection().stackDepth() == startDepth);

  // Steps 7.a.iv-v.
  // Steps 7.b.ii.5-6.
  if (!emitJumpTargetAndPatch(checkResult)) {
    //              [stack] NEXT ITER RESULT
    //              [stack] # checkResult:
    return false;
  }

  //                [stack] NEXT ITER RESULT

  // if (!result.done) goto tryStart;
  if (!emit1(JSOP_DUP)) {
    //              [stack] NEXT ITER RESULT RESULT
    return false;
  }
  if (!emitAtomOp(cx->names().done, JSOP_GETPROP)) {
    //              [stack] NEXT ITER RESULT DONE
    return false;
  }
  // if (!DONE) goto tryStart;
  {
    JumpList beq;
    JumpTarget breakTarget;
    if (!emitBackwardJump(JSOP_IFEQ, tryStart, &beq, &breakTarget)) {
      //            [stack] NEXT ITER RESULT
      return false;
    }
  }

  // result.value
  if (!emit2(JSOP_UNPICK, 2)) {
    //              [stack] RESULT NEXT ITER
    return false;
  }
  if (!emitPopN(2)) {
    //              [stack] RESULT
    return false;
  }
  if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) {
    //              [stack] VALUE
    return false;
  }

  MOZ_ASSERT(bytecodeSection().stackDepth() == startDepth - 2);

  return true;
}

bool BytecodeEmitter::emitStatementList(ListNode* stmtList) {
  for (ParseNode* stmt : stmtList->contents()) {
    if (!emitTree(stmt)) {
      return false;
    }
  }
  return true;
}

bool BytecodeEmitter::emitExpressionStatement(UnaryNode* exprStmt) {
  MOZ_ASSERT(exprStmt->isKind(ParseNodeKind::ExpressionStmt));

  /*
   * Top-level or called-from-a-native JS_Execute/EvaluateScript,
   * debugger, and eval frames may need the value of the ultimate
   * expression statement as the script's result, despite the fact
   * that it appears useless to the compiler.
   *
   * API users may also set the JSOPTION_NO_SCRIPT_RVAL option when
   * calling JS_Compile* to suppress JSOP_SETRVAL.
   */
  bool wantval = false;
  bool useful = false;
  if (sc->isFunctionBox()) {
    MOZ_ASSERT(!script->noScriptRval());
  } else {
    useful = wantval = !script->noScriptRval();
  }

  /* Don't eliminate expressions with side effects. */
  ParseNode* expr = exprStmt->kid();
  if (!useful) {
    if (!checkSideEffects(expr, &useful)) {
      return false;
    }

    /*
     * Don't eliminate apparently useless expressions if they are labeled
     * expression statements. The startOffset() test catches the case
     * where we are nesting in emitTree for a labeled compound statement.
     */
    if (innermostNestableControl &&
        innermostNestableControl->is<LabelControl>() &&
        innermostNestableControl->as<LabelControl>().startOffset() >=
            bytecodeSection().offset()) {
      useful = true;
    }
  }

  if (useful) {
    ValueUsage valueUsage =
        wantval ? ValueUsage::WantValue : ValueUsage::IgnoreValue;
    ExpressionStatementEmitter ese(this, valueUsage);
    if (!ese.prepareForExpr(Some(exprStmt->pn_pos.begin))) {
      return false;
    }
    if (!markStepBreakpoint()) {
      return false;
    }
    if (!emitTree(expr, valueUsage)) {
      return false;
    }
    if (!ese.emitEnd()) {
      return false;
    }
  } else if (exprStmt->isDirectivePrologueMember()) {
    // Don't complain about directive prologue members; just don't emit
    // their code.
  } else {
    if (JSAtom* atom = exprStmt->isStringExprStatement()) {
      // Warn if encountering a non-directive prologue member string
      // expression statement, that is inconsistent with the current
      // directive prologue.  That is, a script *not* starting with
      // "use strict" should warn for any "use strict" statements seen
      // later in the script, because such statements are misleading.
      const char* directive = nullptr;
      if (atom == cx->names().useStrict) {
        if (!sc->strictScript) {
          directive = js_useStrict_str;
        }
      } else if (atom == cx->names().useAsm) {
        if (sc->isFunctionBox()) {
          if (IsAsmJSModule(sc->asFunctionBox()->function())) {
            directive = js_useAsm_str;
          }
        }
      }

      if (directive) {
        if (!reportExtraWarning(expr, JSMSG_CONTRARY_NONDIRECTIVE, directive)) {
          return false;
        }
      }
    } else {
      if (!reportExtraWarning(expr, JSMSG_USELESS_EXPR)) {
        return false;
      }
    }
  }

  return true;
}

bool BytecodeEmitter::emitDeleteName(UnaryNode* deleteNode) {
  MOZ_ASSERT(deleteNode->isKind(ParseNodeKind::DeleteNameExpr));

  NameNode* nameExpr = &deleteNode->kid()->as<NameNode>();
  MOZ_ASSERT(nameExpr->isKind(ParseNodeKind::Name));

  return emitAtomOp(nameExpr->atom(), JSOP_DELNAME);
}

bool BytecodeEmitter::emitDeleteProperty(UnaryNode* deleteNode) {
  MOZ_ASSERT(deleteNode->isKind(ParseNodeKind::DeletePropExpr));

  PropertyAccess* propExpr = &deleteNode->kid()->as<PropertyAccess>();
  PropOpEmitter poe(this, PropOpEmitter::Kind::Delete,
                    propExpr->as<PropertyAccess>().isSuper()
                        ? PropOpEmitter::ObjKind::Super
                        : PropOpEmitter::ObjKind::Other);
  if (propExpr->isSuper()) {
    // The expression |delete super.foo;| has to evaluate |super.foo|,
    // which could throw if |this| hasn't yet been set by a |super(...)|
    // call or the super-base is not an object, before throwing a
    // ReferenceError for attempting to delete a super-reference.
    UnaryNode* base = &propExpr->expression().as<UnaryNode>();
    if (!emitGetThisForSuperBase(base)) {
      //            [stack] THIS
      return false;
    }
  } else {
    if (!poe.prepareForObj()) {
      return false;
    }
    if (!emitPropLHS(propExpr)) {
      //            [stack] OBJ
      return false;
    }
  }

  if (!poe.emitDelete(propExpr->key().atom())) {
    //              [stack] # if Super
    //              [stack] THIS
    //              [stack] # otherwise
    //              [stack] SUCCEEDED
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitDeleteElement(UnaryNode* deleteNode) {
  MOZ_ASSERT(deleteNode->isKind(ParseNodeKind::DeleteElemExpr));

  PropertyByValue* elemExpr = &deleteNode->kid()->as<PropertyByValue>();
  bool isSuper = elemExpr->isSuper();
  ElemOpEmitter eoe(
      this, ElemOpEmitter::Kind::Delete,
      isSuper ? ElemOpEmitter::ObjKind::Super : ElemOpEmitter::ObjKind::Other);
  if (isSuper) {
    // The expression |delete super[foo];| has to evaluate |super[foo]|,
    // which could throw if |this| hasn't yet been set by a |super(...)|
    // call, or trigger side-effects when evaluating ToPropertyKey(foo),
    // or also throw when the super-base is not an object, before throwing
    // a ReferenceError for attempting to delete a super-reference.
    if (!eoe.prepareForObj()) {
      //            [stack]
      return false;
    }

    UnaryNode* base = &elemExpr->expression().as<UnaryNode>();
    if (!emitGetThisForSuperBase(base)) {
      //            [stack] THIS
      return false;
    }
    if (!eoe.prepareForKey()) {
      //            [stack] THIS
      return false;
    }
    if (!emitTree(&elemExpr->key())) {
      //            [stack] THIS KEY
      return false;
    }
  } else {
    if (!emitElemObjAndKey(elemExpr, false, eoe)) {
      //            [stack] OBJ KEY
      return false;
    }
  }
  if (!eoe.emitDelete()) {
    //              [stack] # if Super
    //              [stack] THIS
    //              [stack] # otherwise
    //              [stack] SUCCEEDED
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitDeleteExpression(UnaryNode* deleteNode) {
  MOZ_ASSERT(deleteNode->isKind(ParseNodeKind::DeleteExpr));

  ParseNode* expression = deleteNode->kid();

  // If useless, just emit JSOP_TRUE; otherwise convert |delete <expr>| to
  // effectively |<expr>, true|.
  bool useful = false;
  if (!checkSideEffects(expression, &useful)) {
    return false;
  }

  if (useful) {
    if (!emitTree(expression)) {
      return false;
    }
    if (!emit1(JSOP_POP)) {
      return false;
    }
  }

  return emit1(JSOP_TRUE);
}

static const char* SelfHostedCallFunctionName(JSAtom* name, JSContext* cx) {
  if (name == cx->names().callFunction) {
    return "callFunction";
  }
  if (name == cx->names().callContentFunction) {
    return "callContentFunction";
  }
  if (name == cx->names().constructContentFunction) {
    return "constructContentFunction";
  }

  MOZ_CRASH("Unknown self-hosted call function name");
}

bool BytecodeEmitter::emitSelfHostedCallFunction(CallNode* callNode) {
  // Special-casing of callFunction to emit bytecode that directly
  // invokes the callee with the correct |this| object and arguments.
  // callFunction(fun, thisArg, arg0, arg1) thus becomes:
  // - emit lookup for fun
  // - emit lookup for thisArg
  // - emit lookups for arg0, arg1
  //
  // argc is set to the amount of actually emitted args and the
  // emitting of args below is disabled by setting emitArgs to false.
  NameNode* calleeNode = &callNode->left()->as<NameNode>();
  ListNode* argsList = &callNode->right()->as<ListNode>();

  const char* errorName = SelfHostedCallFunctionName(calleeNode->name(), cx);

  if (argsList->count() < 2) {
    reportNeedMoreArgsError(calleeNode, errorName, "2", "s", argsList);
    return false;
  }

  JSOp callOp = callNode->callOp();
  if (callOp != JSOP_CALL) {
    reportError(callNode, JSMSG_NOT_CONSTRUCTOR, errorName);
    return false;
  }

  bool constructing =
      calleeNode->name() == cx->names().constructContentFunction;
  ParseNode* funNode = argsList->head();
  if (constructing) {
    callOp = JSOP_NEW;
  } else if (funNode->isName(cx->names().std_Function_apply)) {
    callOp = JSOP_FUNAPPLY;
  }

  if (!emitTree(funNode)) {
    return false;
  }

#ifdef DEBUG
  if (emitterMode == BytecodeEmitter::SelfHosting &&
      calleeNode->name() == cx->names().callFunction) {
    if (!emit1(JSOP_DEBUGCHECKSELFHOSTED)) {
      return false;
    }
  }
#endif

  ParseNode* thisOrNewTarget = funNode->pn_next;
  if (constructing) {
    // Save off the new.target value, but here emit a proper |this| for a
    // constructing call.
    if (!emit1(JSOP_IS_CONSTRUCTING)) {
      return false;
    }
  } else {
    // It's |this|, emit it.
    if (!emitTree(thisOrNewTarget)) {
      return false;
    }
  }

  for (ParseNode* argpn = thisOrNewTarget->pn_next; argpn;
       argpn = argpn->pn_next) {
    if (!emitTree(argpn)) {
      return false;
    }
  }

  if (constructing) {
    if (!emitTree(thisOrNewTarget)) {
      return false;
    }
  }

  uint32_t argc = argsList->count() - 2;
  if (!emitCall(callOp, argc)) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitSelfHostedResumeGenerator(BinaryNode* callNode) {
  ListNode* argsList = &callNode->right()->as<ListNode>();

  // Syntax: resumeGenerator(gen, value, 'next'|'throw'|'return')
  if (argsList->count() != 3) {
    reportNeedMoreArgsError(callNode, "resumeGenerator", "3", "s", argsList);
    return false;
  }

  ParseNode* genNode = argsList->head();
  if (!emitTree(genNode)) {
    return false;
  }

  ParseNode* valNode = genNode->pn_next;
  if (!emitTree(valNode)) {
    return false;
  }

  ParseNode* kindNode = valNode->pn_next;
  MOZ_ASSERT(kindNode->isKind(ParseNodeKind::StringExpr));
  uint8_t operand = uint8_t(AbstractGeneratorObject::getResumeKind(
      cx, kindNode->as<NameNode>().atom()));
  MOZ_ASSERT(!kindNode->pn_next);

  if (!emit2(JSOP_RESUME, operand)) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitSelfHostedForceInterpreter() {
  // JSScript::hasForceInterpreterOp() relies on JSOP_FORCEINTERPRETER being the
  // first bytecode op in the script.
  MOZ_ASSERT(bytecodeSection().code().empty());

  if (!emit1(JSOP_FORCEINTERPRETER)) {
    return false;
  }
  if (!emit1(JSOP_UNDEFINED)) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitSelfHostedAllowContentIter(BinaryNode* callNode) {
  ListNode* argsList = &callNode->right()->as<ListNode>();

  if (argsList->count() != 1) {
    reportNeedMoreArgsError(callNode, "allowContentIter", "1", "", argsList);
    return false;
  }

  // We're just here as a sentinel. Pass the value through directly.
  return emitTree(argsList->head());
}

bool BytecodeEmitter::emitSelfHostedDefineDataProperty(BinaryNode* callNode) {
  ListNode* argsList = &callNode->right()->as<ListNode>();

  // Only optimize when 3 arguments are passed.
  MOZ_ASSERT(argsList->count() == 3);

  ParseNode* objNode = argsList->head();
  if (!emitTree(objNode)) {
    return false;
  }

  ParseNode* idNode = objNode->pn_next;
  if (!emitTree(idNode)) {
    return false;
  }

  ParseNode* valNode = idNode->pn_next;
  if (!emitTree(valNode)) {
    return false;
  }

  // This will leave the object on the stack instead of pushing |undefined|,
  // but that's fine because the self-hosted code doesn't use the return
  // value.
  return emit1(JSOP_INITELEM);
}

bool BytecodeEmitter::emitSelfHostedHasOwn(BinaryNode* callNode) {
  ListNode* argsList = &callNode->right()->as<ListNode>();

  if (argsList->count() != 2) {
    reportNeedMoreArgsError(callNode, "hasOwn", "2", "s", argsList);
    return false;
  }

  ParseNode* idNode = argsList->head();
  if (!emitTree(idNode)) {
    return false;
  }

  ParseNode* objNode = idNode->pn_next;
  if (!emitTree(objNode)) {
    return false;
  }

  return emit1(JSOP_HASOWN);
}

bool BytecodeEmitter::emitSelfHostedGetPropertySuper(BinaryNode* callNode) {
  ListNode* argsList = &callNode->right()->as<ListNode>();

  if (argsList->count() != 3) {
    reportNeedMoreArgsError(callNode, "getPropertySuper", "3", "s", argsList);
    return false;
  }

  ParseNode* objNode = argsList->head();
  ParseNode* idNode = objNode->pn_next;
  ParseNode* receiverNode = idNode->pn_next;

  if (!emitTree(receiverNode)) {
    return false;
  }

  if (!emitTree(idNode)) {
    return false;
  }

  if (!emitTree(objNode)) {
    return false;
  }

  return emitElemOpBase(JSOP_GETELEM_SUPER);
}

bool BytecodeEmitter::isRestParameter(ParseNode* expr) {
  if (!sc->isFunctionBox()) {
    return false;
  }

  FunctionBox* funbox = sc->asFunctionBox();
  RootedFunction fun(cx, funbox->function());
  if (!funbox->hasRest()) {
    return false;
  }

  if (!expr->isKind(ParseNodeKind::Name)) {
    if (emitterMode == BytecodeEmitter::SelfHosting &&
        expr->isKind(ParseNodeKind::CallExpr)) {
      BinaryNode* callNode = &expr->as<BinaryNode>();
      ParseNode* calleeNode = callNode->left();
      if (calleeNode->isName(cx->names().allowContentIter)) {
        return isRestParameter(callNode->right()->as<ListNode>().head());
      }
    }
    return false;
  }

  JSAtom* name = expr->as<NameNode>().name();
  Maybe<NameLocation> paramLoc = locationOfNameBoundInFunctionScope(name);
  if (paramLoc && lookupName(name) == *paramLoc) {
    FunctionScope::Data* bindings = funbox->functionScopeBindings();
    if (bindings->nonPositionalFormalStart > 0) {
      // |paramName| can be nullptr when the rest destructuring syntax is
      // used: `function f(...[]) {}`.
      JSAtom* paramName =
          bindings->trailingNames[bindings->nonPositionalFormalStart - 1]
              .name();
      return paramName && name == paramName;
    }
  }

  return false;
}

bool BytecodeEmitter::emitCalleeAndThis(ParseNode* callee, ParseNode* call,
                                        CallOrNewEmitter& cone) {
  switch (callee->getKind()) {
    case ParseNodeKind::Name: {
      RootedAtom nameAtom(cx, callee->as<NameNode>().name());
      if (!cone.emitNameCallee(nameAtom)) {
        //          [stack] CALLEE THIS
        return false;
      }
      break;
    }
    case ParseNodeKind::DotExpr: {
      MOZ_ASSERT(emitterMode != BytecodeEmitter::SelfHosting);
      PropertyAccess* prop = &callee->as<PropertyAccess>();
      bool isSuper = prop->isSuper();

      PropOpEmitter& poe = cone.prepareForPropCallee(isSuper);
      if (!poe.prepareForObj()) {
        return false;
      }
      if (isSuper) {
        UnaryNode* base = &prop->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {
          //        [stack] THIS
          return false;
        }
      } else {
        if (!emitPropLHS(prop)) {
          //        [stack] OBJ
          return false;
        }
      }
      if (!poe.emitGet(prop->key().atom())) {
        //          [stack] CALLEE THIS?
        return false;
      }

      break;
    }
    case ParseNodeKind::ElemExpr: {
      MOZ_ASSERT(emitterMode != BytecodeEmitter::SelfHosting);
      PropertyByValue* elem = &callee->as<PropertyByValue>();
      bool isSuper = elem->isSuper();

      ElemOpEmitter& eoe = cone.prepareForElemCallee(isSuper);
      if (!emitElemObjAndKey(elem, isSuper, eoe)) {
        //          [stack] # if Super
        //          [stack] THIS? THIS KEY
        //          [stack] # otherwise
        //          [stack] OBJ? OBJ KEY
        return false;
      }
      if (!eoe.emitGet()) {
        //          [stack] CALLEE? THIS
        return false;
      }

      break;
    }
    case ParseNodeKind::Function:
      if (!cone.prepareForFunctionCallee()) {
        return false;
      }
      if (!emitTree(callee)) {
        //          [stack] CALLEE
        return false;
      }
      break;
    case ParseNodeKind::SuperBase:
      MOZ_ASSERT(call->isKind(ParseNodeKind::SuperCallExpr));
      MOZ_ASSERT(parser->astGenerator().isSuperBase(callee));
      if (!cone.emitSuperCallee()) {
        //          [stack] CALLEE THIS
        return false;
      }
      break;
    default:
      if (!cone.prepareForOtherCallee()) {
        return false;
      }
      if (!emitTree(callee)) {
        return false;
      }
      break;
  }

  if (!cone.emitThis()) {
    //              [stack] CALLEE THIS
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitPipeline(ListNode* node) {
  MOZ_ASSERT(node->count() >= 2);

  if (!emitTree(node->head())) {
    //              [stack] ARG
    return false;
  }

  ParseNode* callee = node->head()->pn_next;
  CallOrNewEmitter cone(this, JSOP_CALL, CallOrNewEmitter::ArgumentsKind::Other,
                        ValueUsage::WantValue);
  do {
    if (!emitCalleeAndThis(callee, node, cone)) {
      //            [stack] ARG CALLEE THIS
      return false;
    }
    if (!emit2(JSOP_PICK, 2)) {
      //            [stack] CALLEE THIS ARG
      return false;
    }
    if (!cone.emitEnd(1, Some(node->pn_pos.begin))) {
      //            [stack] RVAL
      return false;
    }

    cone.reset();
  } while ((callee = callee->pn_next));

  return true;
}

bool BytecodeEmitter::emitArguments(ListNode* argsList, bool isCall,
                                    bool isSpread, CallOrNewEmitter& cone) {
  uint32_t argc = argsList->count();
  if (argc >= ARGC_LIMIT) {
    reportError(argsList,
                isCall ? JSMSG_TOO_MANY_FUN_ARGS : JSMSG_TOO_MANY_CON_ARGS);
    return false;
  }
  if (!isSpread) {
    if (!cone.prepareForNonSpreadArguments()) {
      //            [stack] CALLEE THIS
      return false;
    }
    for (ParseNode* arg : argsList->contents()) {
      if (!emitTree(arg)) {
        //          [stack] CALLEE THIS ARG*
        return false;
      }
    }
  } else {
    if (cone.wantSpreadOperand()) {
      UnaryNode* spreadNode = &argsList->head()->as<UnaryNode>();
      if (!emitTree(spreadNode->kid())) {
        //          [stack] CALLEE THIS ARG0
        return false;
      }
    }
    if (!cone.emitSpreadArgumentsTest()) {
      //            [stack] CALLEE THIS
      return false;
    }
    if (!emitArray(argsList->head(), argc)) {
      //            [stack] CALLEE THIS ARR
      return false;
    }
  }

  return true;
}

bool BytecodeEmitter::emitCallOrNew(
    CallNode* callNode, ValueUsage valueUsage /* = ValueUsage::WantValue */) {
  /*
   * Emit callable invocation or operator new (constructor call) code.
   * First, emit code for the left operand to evaluate the callable or
   * constructable object expression.
   *
   * For operator new, we emit JSOP_GETPROP instead of JSOP_CALLPROP, etc.
   * This is necessary to interpose the lambda-initialized method read
   * barrier -- see the code in jsinterp.cpp for JSOP_LAMBDA followed by
   * JSOP_{SET,INIT}PROP.
   *
   * Then (or in a call case that has no explicit reference-base
   * object) we emit JSOP_UNDEFINED to produce the undefined |this|
   * value required for calls (which non-strict mode functions
   * will box into the global object).
   */
  bool isCall = callNode->isKind(ParseNodeKind::CallExpr) ||
                callNode->isKind(ParseNodeKind::TaggedTemplateExpr);
  ParseNode* calleeNode = callNode->left();
  ListNode* argsList = &callNode->right()->as<ListNode>();
  bool isSpread = JOF_OPTYPE(callNode->callOp()) == JOF_BYTE;
  if (calleeNode->isKind(ParseNodeKind::Name) &&
      emitterMode == BytecodeEmitter::SelfHosting && !isSpread) {
    // Calls to "forceInterpreter", "callFunction",
    // "callContentFunction", or "resumeGenerator" in self-hosted
    // code generate inline bytecode.
    PropertyName* calleeName = calleeNode->as<NameNode>().name();
    if (calleeName == cx->names().callFunction ||
        calleeName == cx->names().callContentFunction ||
        calleeName == cx->names().constructContentFunction) {
      return emitSelfHostedCallFunction(callNode);
    }
    if (calleeName == cx->names().resumeGenerator) {
      return emitSelfHostedResumeGenerator(callNode);
    }
    if (calleeName == cx->names().forceInterpreter) {
      return emitSelfHostedForceInterpreter();
    }
    if (calleeName == cx->names().allowContentIter) {
      return emitSelfHostedAllowContentIter(callNode);
    }
    if (calleeName == cx->names().defineDataPropertyIntrinsic &&
        argsList->count() == 3) {
      return emitSelfHostedDefineDataProperty(callNode);
    }
    if (calleeName == cx->names().hasOwn) {
      return emitSelfHostedHasOwn(callNode);
    }
    if (calleeName == cx->names().getPropertySuper) {
      return emitSelfHostedGetPropertySuper(callNode);
    }
    // Fall through
  }

  JSOp op = callNode->callOp();
  uint32_t argc = argsList->count();
  CallOrNewEmitter cone(
      this, op,
      isSpread && (argc == 1) &&
              isRestParameter(argsList->head()->as<UnaryNode>().kid())
          ? CallOrNewEmitter::ArgumentsKind::SingleSpreadRest
          : CallOrNewEmitter::ArgumentsKind::Other,
      valueUsage);
  if (!emitCalleeAndThis(calleeNode, callNode, cone)) {
    //              [stack] CALLEE THIS
    return false;
  }
  if (!emitArguments(argsList, isCall, isSpread, cone)) {
    //              [stack] CALLEE THIS ARGS...
    return false;
  }

  ParseNode* coordNode = callNode;
  if (op == JSOP_CALL || op == JSOP_SPREADCALL || op == JSOP_FUNCALL ||
      op == JSOP_FUNAPPLY) {
    // Default to using the location of the `(` itself.
    // obj[expr]() // expression
    //          ^  // column coord
    coordNode = argsList;

    switch (calleeNode->getKind()) {
      case ParseNodeKind::DotExpr:
        // Use the position of a property access identifier.
        //
        // obj().aprop() // expression
        //       ^       // column coord
        //
        // Note: Because of the constant folding logic in FoldElement,
        // this case also applies for constant string properties.
        //
        // obj()['aprop']() // expression
        //       ^          // column coord
        coordNode = &calleeNode->as<PropertyAccess>().key();
        break;
      case ParseNodeKind::Name: {
        // Use the start of callee name unless it is at a separator
        // or has no args.
        //
        // 2 + obj()   // expression
        //     ^       // column coord
        //
        if (argsList->empty() ||
            !bytecodeSection().atSeparator(calleeNode->pn_pos.begin)) {
          // Use the start of callee names.
          coordNode = calleeNode;
        }
        break;
      }

      default:
        break;
    }
  }
  if (!cone.emitEnd(argc, Some(coordNode->pn_pos.begin))) {
    //              [stack] RVAL
    return false;
  }

  return true;
}

static const JSOp ParseNodeKindToJSOp[] = {
    // JSOP_NOP is for pipeline operator which does not emit its own JSOp
    // but has highest precedence in binary operators
    JSOP_NOP,    JSOP_OR,       JSOP_AND, JSOP_BITOR,    JSOP_BITXOR,
    JSOP_BITAND, JSOP_STRICTEQ, JSOP_EQ,  JSOP_STRICTNE, JSOP_NE,
    JSOP_LT,     JSOP_LE,       JSOP_GT,  JSOP_GE,       JSOP_INSTANCEOF,
    JSOP_IN,     JSOP_LSH,      JSOP_RSH, JSOP_URSH,     JSOP_ADD,
    JSOP_SUB,    JSOP_MUL,      JSOP_DIV, JSOP_MOD,      JSOP_POW};

static inline JSOp BinaryOpParseNodeKindToJSOp(ParseNodeKind pnk) {
  MOZ_ASSERT(pnk >= ParseNodeKind::BinOpFirst);
  MOZ_ASSERT(pnk <= ParseNodeKind::BinOpLast);
  return ParseNodeKindToJSOp[size_t(pnk) - size_t(ParseNodeKind::BinOpFirst)];
}

bool BytecodeEmitter::emitRightAssociative(ListNode* node) {
  // ** is the only right-associative operator.
  MOZ_ASSERT(node->isKind(ParseNodeKind::PowExpr));

  // Right-associative operator chain.
  for (ParseNode* subexpr : node->contents()) {
    if (!emitTree(subexpr)) {
      return false;
    }
  }
  for (uint32_t i = 0; i < node->count() - 1; i++) {
    if (!emit1(JSOP_POW)) {
      return false;
    }
  }
  return true;
}

bool BytecodeEmitter::emitLeftAssociative(ListNode* node) {
  // Left-associative operator chain.
  if (!emitTree(node->head())) {
    return false;
  }
  JSOp op = BinaryOpParseNodeKindToJSOp(node->getKind());
  ParseNode* nextExpr = node->head()->pn_next;
  do {
    if (!emitTree(nextExpr)) {
      return false;
    }
    if (!emit1(op)) {
      return false;
    }
  } while ((nextExpr = nextExpr->pn_next));
  return true;
}

bool BytecodeEmitter::emitLogical(ListNode* node) {
  MOZ_ASSERT(node->isKind(ParseNodeKind::OrExpr) ||
             node->isKind(ParseNodeKind::AndExpr));

  /*
   * JSOP_OR converts the operand on the stack to boolean, leaves the original
   * value on the stack and jumps if true; otherwise it falls into the next
   * bytecode, which pops the left operand and then evaluates the right operand.
   * The jump goes around the right operand evaluation.
   *
   * JSOP_AND converts the operand on the stack to boolean and jumps if false;
   * otherwise it falls into the right operand's bytecode.
   */

  TDZCheckCache tdzCache(this);

  /* Left-associative operator chain: avoid too much recursion. */
  ParseNode* expr = node->head();
  if (!emitTree(expr)) {
    return false;
  }
  JSOp op = node->isKind(ParseNodeKind::OrExpr) ? JSOP_OR : JSOP_AND;
  JumpList jump;
  if (!emitJump(op, &jump)) {
    return false;
  }
  if (!emit1(JSOP_POP)) {
    return false;
  }

  /* Emit nodes between the head and the tail. */
  while ((expr = expr->pn_next)->pn_next) {
    if (!emitTree(expr)) {
      return false;
    }
    if (!emitJump(op, &jump)) {
      return false;
    }
    if (!emit1(JSOP_POP)) {
      return false;
    }
  }
  if (!emitTree(expr)) {
    return false;
  }

  if (!emitJumpTargetAndPatch(jump)) {
    return false;
  }
  return true;
}

bool BytecodeEmitter::emitSequenceExpr(
    ListNode* node, ValueUsage valueUsage /* = ValueUsage::WantValue */) {
  for (ParseNode* child = node->head();; child = child->pn_next) {
    if (!updateSourceCoordNotes(child->pn_pos.begin)) {
      return false;
    }
    if (!emitTree(child,
                  child->pn_next ? ValueUsage::IgnoreValue : valueUsage)) {
      return false;
    }
    if (!child->pn_next) {
      break;
    }
    if (!emit1(JSOP_POP)) {
      return false;
    }
  }
  return true;
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool BytecodeEmitter::emitIncOrDec(UnaryNode* incDec) {
  switch (incDec->kid()->getKind()) {
    case ParseNodeKind::DotExpr:
      return emitPropIncDec(incDec);
    case ParseNodeKind::ElemExpr:
      return emitElemIncDec(incDec);
    case ParseNodeKind::CallExpr:
      return emitCallIncDec(incDec);
    default:
      return emitNameIncDec(incDec);
  }
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool BytecodeEmitter::emitLabeledStatement(
    const LabeledStatement* labeledStmt) {
  RootedAtom name(cx, labeledStmt->label());
  LabelEmitter label(this);
  if (!label.emitLabel(name)) {
    return false;
  }
  if (!emitTree(labeledStmt->statement())) {
    return false;
  }
  if (!label.emitEnd()) {
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitConditionalExpression(
    ConditionalExpression& conditional,
    ValueUsage valueUsage /* = ValueUsage::WantValue */) {
  CondEmitter cond(this);
  if (!cond.emitCond()) {
    return false;
  }

  if (!emitTree(&conditional.condition())) {
    return false;
  }

  if (!cond.emitThenElse()) {
    return false;
  }

  if (!emitTree(&conditional.thenExpression(), valueUsage)) {
    return false;
  }

  if (!cond.emitElse()) {
    return false;
  }

  if (!emitTree(&conditional.elseExpression(), valueUsage)) {
    return false;
  }

  if (!cond.emitEnd()) {
    return false;
  }
  MOZ_ASSERT(cond.pushed() == 1);

  return true;
}

bool BytecodeEmitter::emitPropertyList(ListNode* obj, PropertyEmitter& pe,
                                       PropListType type) {
  //                [stack] CTOR? OBJ

  size_t curFieldKeyIndex = 0;
  for (ParseNode* propdef : obj->contents()) {
    if (propdef->is<ClassField>()) {
      MOZ_ASSERT(type == ClassBody);
      // Only handle computing field keys here: the .initializers lambda array
      // is created elsewhere.
      ClassField* field = &propdef->as<ClassField>();
      if (field->name().getKind() == ParseNodeKind::ComputedName) {
        if (!emitGetName(cx->names().dotFieldKeys)) {
          //        [stack] CTOR? OBJ ARRAY
          return false;
        }

        ParseNode* nameExpr = field->name().as<UnaryNode>().kid();

        if (!emitTree(nameExpr)) {
          //        [stack] CTOR? OBJ ARRAY KEY
          return false;
        }

        if (!emit1(JSOP_TOID)) {
          //        [stack] CTOR? OBJ ARRAY KEY
          return false;
        }

        if (!emitUint32Operand(JSOP_INITELEM_ARRAY, curFieldKeyIndex)) {
          //        [stack] CTOR? OBJ ARRAY
          return false;
        }

        if (!emit1(JSOP_POP)) {
          //        [stack] CTOR? OBJ
          return false;
        }

        curFieldKeyIndex++;
      }
      continue;
    }

    if (propdef->is<LexicalScopeNode>()) {
      // Constructors are sometimes wrapped in LexicalScopeNodes. As we already
      // handled emitting the constructor, skip it.
      MOZ_ASSERT(propdef->as<LexicalScopeNode>().scopeBody()->isKind(
          ParseNodeKind::ClassMethod));
      continue;
    }

    // Handle __proto__: v specially because *only* this form, and no other
    // involving "__proto__", performs [[Prototype]] mutation.
    if (propdef->isKind(ParseNodeKind::MutateProto)) {
      //            [stack] OBJ
      MOZ_ASSERT(type == ObjectLiteral);
      if (!pe.prepareForProtoValue(Some(propdef->pn_pos.begin))) {
        //          [stack] OBJ
        return false;
      }
      if (!emitTree(propdef->as<UnaryNode>().kid())) {
        //          [stack] OBJ PROTO
        return false;
      }
      if (!pe.emitMutateProto()) {
        //          [stack] OBJ
        return false;
      }
      continue;
    }

    if (propdef->isKind(ParseNodeKind::Spread)) {
      MOZ_ASSERT(type == ObjectLiteral);
      //            [stack] OBJ
      if (!pe.prepareForSpreadOperand(Some(propdef->pn_pos.begin))) {
        //          [stack] OBJ OBJ
        return false;
      }
      if (!emitTree(propdef->as<UnaryNode>().kid())) {
        //          [stack] OBJ OBJ VAL
        return false;
      }
      if (!pe.emitSpread()) {
        //          [stack] OBJ
        return false;
      }
      continue;
    }

    BinaryNode* prop = &propdef->as<BinaryNode>();

    ParseNode* key = prop->left();
    ParseNode* propVal = prop->right();
    AccessorType accessorType;
    if (prop->is<ClassMethod>()) {
      accessorType = prop->as<ClassMethod>().accessorType();
    } else if (prop->is<PropertyDefinition>()) {
      accessorType = prop->as<PropertyDefinition>().accessorType();
    } else {
      accessorType = AccessorType::None;
    }

    auto emitValue = [this, &key, &propVal, accessorType, &pe]() {
      //            [stack] CTOR? OBJ CTOR? KEY?

      if (propVal->isDirectRHSAnonFunction()) {
        if (key->isKind(ParseNodeKind::NumberExpr)) {
          MOZ_ASSERT(accessorType == AccessorType::None);

          NumericLiteral* literal = &key->as<NumericLiteral>();
          RootedAtom keyAtom(cx, NumberToAtom(cx, literal->value()));
          if (!keyAtom) {
            return false;
          }
          if (!emitAnonymousFunctionWithName(propVal, keyAtom)) {
            //      [stack] CTOR? OBJ CTOR? KEY VAL
            return false;
          }
        } else if (key->isKind(ParseNodeKind::ObjectPropertyName) ||
                   key->isKind(ParseNodeKind::StringExpr)) {
          MOZ_ASSERT(accessorType == AccessorType::None);

          RootedAtom keyAtom(cx, key->as<NameNode>().atom());
          if (!emitAnonymousFunctionWithName(propVal, keyAtom)) {
            //      [stack] CTOR? OBJ CTOR? VAL
            return false;
          }
        } else {
          MOZ_ASSERT(key->isKind(ParseNodeKind::ComputedName));

          FunctionPrefixKind prefix = accessorType == AccessorType::None
                                          ? FunctionPrefixKind::None
                                          : accessorType == AccessorType::Getter
                                                ? FunctionPrefixKind::Get
                                                : FunctionPrefixKind::Set;

          if (!emitAnonymousFunctionWithComputedName(propVal, prefix)) {
            //      [stack] CTOR? OBJ CTOR? KEY VAL
            return false;
          }
        }
      } else {
        if (!emitTree(propVal)) {
          //        [stack] CTOR? OBJ CTOR? KEY? VAL
          return false;
        }
      }

      if (propVal->is<FunctionNode>() &&
          propVal->as<FunctionNode>().funbox()->needsHomeObject()) {
        MOZ_ASSERT(propVal->as<FunctionNode>()
                       .funbox()
                       ->function()
                       ->allowSuperProperty());

        if (!pe.emitInitHomeObject()) {
          //        [stack] CTOR? OBJ CTOR? KEY? FUN
          return false;
        }
      }
      return true;
    };

    PropertyEmitter::Kind kind =
        (type == ClassBody && propdef->as<ClassMethod>().isStatic())
            ? PropertyEmitter::Kind::Static
            : PropertyEmitter::Kind::Prototype;
    if (key->isKind(ParseNodeKind::NumberExpr)) {
      //            [stack] CTOR? OBJ
      if (!pe.prepareForIndexPropKey(Some(propdef->pn_pos.begin), kind)) {
        //          [stack] CTOR? OBJ CTOR?
        return false;
      }
      if (!emitNumberOp(key->as<NumericLiteral>().value())) {
        //          [stack] CTOR? OBJ CTOR? KEY
        return false;
      }
      if (!pe.prepareForIndexPropValue()) {
        //          [stack] CTOR? OBJ CTOR? KEY
        return false;
      }
      if (!emitValue()) {
        //          [stack] CTOR? OBJ CTOR? KEY VAL
        return false;
      }

      switch (accessorType) {
        case AccessorType::None:
          if (!pe.emitInitIndexProp()) {
            //      [stack] CTOR? OBJ
            return false;
          }
          break;
        case AccessorType::Getter:
          if (!pe.emitInitIndexGetter()) {
            //      [stack] CTOR? OBJ
            return false;
          }
          break;
        case AccessorType::Setter:
          if (!pe.emitInitIndexSetter()) {
            //      [stack] CTOR? OBJ
            return false;
          }
          break;
        default:
          MOZ_CRASH("Invalid op");
      }

      continue;
    }

    if (key->isKind(ParseNodeKind::ObjectPropertyName) ||
        key->isKind(ParseNodeKind::StringExpr)) {
      //            [stack] CTOR? OBJ

      // emitClass took care of constructor already.
      if (type == ClassBody &&
          key->as<NameNode>().atom() == cx->names().constructor &&
          !propdef->as<ClassMethod>().isStatic()) {
        continue;
      }

      if (!pe.prepareForPropValue(Some(propdef->pn_pos.begin), kind)) {
        //          [stack] CTOR? OBJ CTOR?
        return false;
      }
      if (!emitValue()) {
        //          [stack] CTOR? OBJ CTOR? VAL
        return false;
      }

      RootedAtom keyAtom(cx, key->as<NameNode>().atom());
      switch (accessorType) {
        case AccessorType::None:
          if (!pe.emitInitProp(keyAtom)) {
            //      [stack] CTOR? OBJ
            return false;
          }
          break;
        case AccessorType::Getter:
          if (!pe.emitInitGetter(keyAtom)) {
            //      [stack] CTOR? OBJ
            return false;
          }
          break;
        case AccessorType::Setter:
          if (!pe.emitInitSetter(keyAtom)) {
            //      [stack] CTOR? OBJ
            return false;
          }
          break;
        default:
          MOZ_CRASH("Invalid op");
      }

      continue;
    }

    MOZ_ASSERT(key->isKind(ParseNodeKind::ComputedName));

    //              [stack] CTOR? OBJ

    if (!pe.prepareForComputedPropKey(Some(propdef->pn_pos.begin), kind)) {
      //            [stack] CTOR? OBJ CTOR?
      return false;
    }
    if (!emitTree(key->as<UnaryNode>().kid())) {
      //            [stack] CTOR? OBJ CTOR? KEY
      return false;
    }
    if (!pe.prepareForComputedPropValue()) {
      //            [stack] CTOR? OBJ CTOR? KEY
      return false;
    }
    if (!emitValue()) {
      //            [stack] CTOR? OBJ CTOR? KEY VAL
      return false;
    }

    switch (accessorType) {
      case AccessorType::None:
        if (!pe.emitInitComputedProp()) {
          //        [stack] CTOR? OBJ
          return false;
        }
        break;
      case AccessorType::Getter:
        if (!pe.emitInitComputedGetter()) {
          //        [stack] CTOR? OBJ
          return false;
        }
        break;
      case AccessorType::Setter:
        if (!pe.emitInitComputedSetter()) {
          //        [stack] CTOR? OBJ
          return false;
        }
        break;
      default:
        MOZ_CRASH("Invalid op");
    }
  }

  return true;
}

FieldInitializers BytecodeEmitter::setupFieldInitializers(
    ListNode* classMembers) {
  size_t numFields = 0;
  for (ParseNode* propdef : classMembers->contents()) {
    if (propdef->is<ClassField>()) {
      FunctionNode* initializer = propdef->as<ClassField>().initializer();
      // Don't include fields without initializers.
      if (initializer != nullptr) {
        numFields++;
      }
    }
  }
  return FieldInitializers(numFields);
}

// Purpose of .fieldKeys:
// Computed field names (`["x"] = 2;`) must be ran at class-evaluation time, not
// object construction time. The transformation to do so is roughly as follows:
//
// class C {
//   [keyExpr] = valueExpr;
// }
// -->
// let .fieldKeys = [keyExpr];
// let .initializers = [
//   () => {
//     this[.fieldKeys[0]] = valueExpr;
//   }
// ];
// class C {
//   constructor() {
//     .initializers[0]();
//   }
// }
//
// BytecodeEmitter::emitCreateFieldKeys does `let .fieldKeys = [...];`
// BytecodeEmitter::emitPropertyList fills in the elements of the array.
// See GeneralParser::fieldInitializer for the `this[.fieldKeys[0]]` part.
bool BytecodeEmitter::emitCreateFieldKeys(ListNode* obj) {
  size_t numFieldKeys = 0;
  for (ParseNode* propdef : obj->contents()) {
    if (propdef->is<ClassField>()) {
      ClassField* field = &propdef->as<ClassField>();
      if (field->name().getKind() == ParseNodeKind::ComputedName) {
        numFieldKeys++;
      }
    }
  }

  if (numFieldKeys == 0) {
    return true;
  }

  NameOpEmitter noe(this, cx->names().dotFieldKeys.toHandle(),
                    NameOpEmitter::Kind::Initialize);
  if (!noe.prepareForRhs()) {
    return false;
  }

  if (!emitUint32Operand(JSOP_NEWARRAY, numFieldKeys)) {
    //              [stack] ARRAY
    return false;
  }

  if (!noe.emitAssignment()) {
    //              [stack] ARRAY
    return false;
  }

  if (!emit1(JSOP_POP)) {
    //              [stack]
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitCreateFieldInitializers(ClassEmitter& ce,
                                                  ListNode* obj) {
  //          [stack] HOMEOBJ HERITAGE?
  FieldInitializers fieldInitializers = setupFieldInitializers(obj);
  MOZ_ASSERT(fieldInitializers.valid);
  size_t numFields = fieldInitializers.numFieldInitializers;

  if (numFields == 0) {
    return true;
  }

  if (!ce.prepareForFieldInitializers(numFields)) {
    //          [stack] HOMEOBJ HERITAGE? ARRAY
    return false;
  }

  for (ParseNode* propdef : obj->contents()) {
    if (propdef->is<ClassField>()) {
      if (FunctionNode* initializer = propdef->as<ClassField>().initializer()) {
        if (!emitTree(initializer)) {
          //          [stack] HOMEOBJ HERITAGE? ARRAY LAMBDA
          return false;
        }
        if (initializer->funbox()->needsHomeObject()) {
          MOZ_ASSERT(initializer->funbox()->function()->allowSuperProperty());
          if (!ce.emitFieldInitializerHomeObject()) {
            //          [stack] CTOR OBJ ARRAY LAMBDA
            return false;
          }
        }
        if (!ce.emitStoreFieldInitializer()) {
          //          [stack] HOMEOBJ HERITAGE? ARRAY
          return false;
        }
      }
    }
  }

  if (!ce.emitFieldInitializersEnd()) {
    //              [stack] HOMEOBJ HERITAGE?
    return false;
  }

  return true;
}

const FieldInitializers& BytecodeEmitter::findFieldInitializersForCall() {
  for (BytecodeEmitter* current = this; current; current = current->parent) {
    if (current->sc->isFunctionBox()) {
      FunctionBox* box = current->sc->asFunctionBox();
      if (box->kind() == FunctionFlags::FunctionKind::ClassConstructor) {
        const FieldInitializers& fieldInitializers =
            current->getFieldInitializers();
        MOZ_ASSERT(fieldInitializers.valid);
        return fieldInitializers;
      }
    }
  }

  for (ScopeIter si(innermostScope()); si; si++) {
    if (si.scope()->is<FunctionScope>()) {
      JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();
      if (fun->kind() == FunctionFlags::FunctionKind::ClassConstructor) {
        const FieldInitializers& fieldInitializers =
            fun->isInterpretedLazy()
                ? fun->lazyScript()->getFieldInitializers()
                : fun->nonLazyScript()->getFieldInitializers();
        MOZ_ASSERT(fieldInitializers.valid);
        return fieldInitializers;
      }
    }
  }

  MOZ_CRASH("Constructor for field initializers not found.");
}

bool BytecodeEmitter::emitInitializeInstanceFields() {
  const FieldInitializers& fieldInitializers = findFieldInitializersForCall();
  size_t numFields = fieldInitializers.numFieldInitializers;

  if (numFields == 0) {
    return true;
  }

  if (!emitGetName(cx->names().dotInitializers)) {
    //              [stack] ARRAY
    return false;
  }

  for (size_t fieldIndex = 0; fieldIndex < numFields; fieldIndex++) {
    if (fieldIndex < numFields - 1) {
      // We DUP to keep the array around (it is consumed in the bytecode below)
      // for next iterations of this loop, except for the last iteration, which
      // avoids an extra POP at the end of the loop.
      if (!emit1(JSOP_DUP)) {
        //          [stack] ARRAY ARRAY
        return false;
      }
    }

    if (!emitNumberOp(fieldIndex)) {
      //            [stack] ARRAY? ARRAY INDEX
      return false;
    }

    // Don't use CALLELEM here, because the receiver of the call != the receiver
    // of this getelem. (Specifically, the call receiver is `this`, and the
    // receiver of this getelem is `.initializers`)
    if (!emit1(JSOP_GETELEM)) {
      //            [stack] ARRAY? FUNC
      return false;
    }

    // This is guaranteed to run after super(), so we don't need TDZ checks.
    if (!emitGetName(cx->names().dotThis)) {
      //            [stack] ARRAY? FUNC THIS
      return false;
    }

    if (!emitCall(JSOP_CALL_IGNORES_RV, 0)) {
      //            [stack] ARRAY? RVAL
      return false;
    }

    if (!emit1(JSOP_POP)) {
      //            [stack] ARRAY?
      return false;
    }
  }

  return true;
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool BytecodeEmitter::emitObject(ListNode* objNode) {
  if (!objNode->hasNonConstInitializer() && objNode->head() &&
      checkSingletonContext()) {
    return emitSingletonInitialiser(objNode);
  }

  //                [stack]

  ObjectEmitter oe(this);
  if (!oe.emitObject(objNode->count())) {
    //              [stack] OBJ
    return false;
  }
  if (!emitPropertyList(objNode, oe, ObjectLiteral)) {
    //              [stack] OBJ
    return false;
  }
  if (!oe.emitEnd()) {
    //              [stack] OBJ
    return false;
  }

  return true;
}

bool BytecodeEmitter::replaceNewInitWithNewObject(JSObject* obj,
                                                  BytecodeOffset offset) {
  ObjectBox* objbox = parser->newObjectBox(obj);
  if (!objbox) {
    return false;
  }

  static_assert(
      JSOP_NEWINIT_LENGTH == JSOP_NEWOBJECT_LENGTH,
      "newinit and newobject must have equal length to edit in-place");

  uint32_t index;
  if (!perScriptData().gcThingList().append(objbox, &index)) {
    return false;
  }

  jsbytecode* code = bytecodeSection().code(offset);

  MOZ_ASSERT(code[0] == JSOP_NEWINIT);
  code[0] = JSOP_NEWOBJECT;
  SET_UINT32(code, index);

  return true;
}

bool BytecodeEmitter::emitArrayLiteral(ListNode* array) {
  if (!array->hasNonConstInitializer() && array->head()) {
    if (checkSingletonContext()) {
      // Bake in the object entirely if it will only be created once.
      return emitSingletonInitialiser(array);
    }

    // If the array consists entirely of primitive values, make a
    // template object with copy on write elements that can be reused
    // every time the initializer executes. Don't do this if the array is
    // small: copying the elements lazily is not worth it in that case.
    static const size_t MinElementsForCopyOnWrite = 5;
    if (emitterMode != BytecodeEmitter::SelfHosting &&
        array->count() >= MinElementsForCopyOnWrite) {
      RootedValue value(cx);
      if (!array->getConstantValue(cx, ParseNode::ForCopyOnWriteArray,
                                   &value)) {
        return false;
      }
      if (!value.isMagic(JS_GENERIC_MAGIC)) {
        // Note: the group of the template object might not yet reflect
        // that the object has copy on write elements. When the
        // interpreter or JIT compiler fetches the template, it should
        // use ObjectGroup::getOrFixupCopyOnWriteObject to make sure the
        // group for the template is accurate. We don't do this here as we
        // want to use ObjectGroup::allocationSiteGroup, which requires a
        // finished script.
        JSObject* obj = &value.toObject();
        MOZ_ASSERT(obj->is<ArrayObject>() &&
                   obj->as<ArrayObject>().denseElementsAreCopyOnWrite());

        ObjectBox* objbox = parser->newObjectBox(obj);
        if (!objbox) {
          return false;
        }

        return emitObjectOp(objbox, JSOP_NEWARRAY_COPYONWRITE);
      }
    }
  }

  return emitArray(array->head(), array->count());
}

bool BytecodeEmitter::emitArray(ParseNode* arrayHead, uint32_t count) {
  /*
   * Emit code for [a, b, c] that is equivalent to constructing a new
   * array and in source order evaluating each element value and adding
   * it to the array, without invoking latent setters.  We use the
   * JSOP_NEWINIT and JSOP_INITELEM_ARRAY bytecodes to ignore setters and
   * to avoid dup'ing and popping the array as each element is added, as
   * JSOP_SETELEM/JSOP_SETPROP would do.
   */

  uint32_t nspread = 0;
  for (ParseNode* elem = arrayHead; elem; elem = elem->pn_next) {
    if (elem->isKind(ParseNodeKind::Spread)) {
      nspread++;
    }
  }

  // Array literal's length is limited to NELEMENTS_LIMIT in parser.
  static_assert(NativeObject::MAX_DENSE_ELEMENTS_COUNT <= INT32_MAX,
                "array literals' maximum length must not exceed limits "
                "required by BaselineCompiler::emit_JSOP_NEWARRAY, "
                "BaselineCompiler::emit_JSOP_INITELEM_ARRAY, "
                "and DoSetElemFallback's handling of JSOP_INITELEM_ARRAY");
  MOZ_ASSERT(count >= nspread);
  MOZ_ASSERT(count <= NativeObject::MAX_DENSE_ELEMENTS_COUNT,
             "the parser must throw an error if the array exceeds maximum "
             "length");

  // For arrays with spread, this is a very pessimistic allocation, the
  // minimum possible final size.
  if (!emitUint32Operand(JSOP_NEWARRAY, count - nspread)) {
    //              [stack] ARRAY
    return false;
  }

  ParseNode* elem = arrayHead;
  uint32_t index;
  bool afterSpread = false;
  for (index = 0; elem; index++, elem = elem->pn_next) {
    if (!afterSpread && elem->isKind(ParseNodeKind::Spread)) {
      afterSpread = true;
      if (!emitNumberOp(index)) {
        //          [stack] ARRAY INDEX
        return false;
      }
    }
    if (!updateSourceCoordNotes(elem->pn_pos.begin)) {
      return false;
    }

    bool allowSelfHostedIter = false;
    if (elem->isKind(ParseNodeKind::Elision)) {
      if (!emit1(JSOP_HOLE)) {
        return false;
      }
    } else {
      ParseNode* expr;
      if (elem->isKind(ParseNodeKind::Spread)) {
        expr = elem->as<UnaryNode>().kid();

        if (emitterMode == BytecodeEmitter::SelfHosting &&
            expr->isKind(ParseNodeKind::CallExpr) &&
            expr->as<BinaryNode>().left()->isName(
                cx->names().allowContentIter)) {
          allowSelfHostedIter = true;
        }
      } else {
        expr = elem;
      }
      if (!emitTree(expr)) {
        //          [stack] ARRAY INDEX? VALUE
        return false;
      }
    }
    if (elem->isKind(ParseNodeKind::Spread)) {
      if (!emitIterator()) {
        //          [stack] ARRAY INDEX NEXT ITER
        return false;
      }
      if (!emit2(JSOP_PICK, 3)) {
        //          [stack] INDEX NEXT ITER ARRAY
        return false;
      }
      if (!emit2(JSOP_PICK, 3)) {
        //          [stack] NEXT ITER ARRAY INDEX
        return false;
      }
      if (!emitSpread(allowSelfHostedIter)) {
        //          [stack] ARRAY INDEX
        return false;
      }
    } else if (afterSpread) {
      if (!emit1(JSOP_INITELEM_INC)) {
        return false;
      }
    } else {
      if (!emitUint32Operand(JSOP_INITELEM_ARRAY, index)) {
        return false;
      }
    }
  }
  MOZ_ASSERT(index == count);
  if (afterSpread) {
    if (!emit1(JSOP_POP)) {
      //            [stack] ARRAY
      return false;
    }
  }
  return true;
}

static inline JSOp UnaryOpParseNodeKindToJSOp(ParseNodeKind pnk) {
  switch (pnk) {
    case ParseNodeKind::ThrowStmt:
      return JSOP_THROW;
    case ParseNodeKind::VoidExpr:
      return JSOP_VOID;
    case ParseNodeKind::NotExpr:
      return JSOP_NOT;
    case ParseNodeKind::BitNotExpr:
      return JSOP_BITNOT;
    case ParseNodeKind::PosExpr:
      return JSOP_POS;
    case ParseNodeKind::NegExpr:
      return JSOP_NEG;
    default:
      MOZ_CRASH("unexpected unary op");
  }
}

bool BytecodeEmitter::emitUnary(UnaryNode* unaryNode) {
  if (!updateSourceCoordNotes(unaryNode->pn_pos.begin)) {
    return false;
  }
  if (!emitTree(unaryNode->kid())) {
    return false;
  }
  return emit1(UnaryOpParseNodeKindToJSOp(unaryNode->getKind()));
}

bool BytecodeEmitter::emitTypeof(UnaryNode* typeofNode, JSOp op) {
  MOZ_ASSERT(op == JSOP_TYPEOF || op == JSOP_TYPEOFEXPR);

  if (!updateSourceCoordNotes(typeofNode->pn_pos.begin)) {
    return false;
  }

  if (!emitTree(typeofNode->kid())) {
    return false;
  }

  return emit1(op);
}

bool BytecodeEmitter::emitFunctionFormalParameters(ListNode* paramsBody) {
  ParseNode* funBody = paramsBody->last();
  FunctionBox* funbox = sc->asFunctionBox();

  bool hasRest = funbox->hasRest();

  FunctionParamsEmitter fpe(this, funbox);
  for (ParseNode* arg = paramsBody->head(); arg != funBody;
       arg = arg->pn_next) {
    ParseNode* bindingElement = arg;
    ParseNode* initializer = nullptr;
    if (arg->isKind(ParseNodeKind::AssignExpr) ||
        arg->isKind(ParseNodeKind::InitExpr)) {
      bindingElement = arg->as<BinaryNode>().left();
      initializer = arg->as<BinaryNode>().right();
    }
    bool hasInitializer = !!initializer;
    bool isRest = hasRest && arg->pn_next == funBody;
    bool isDestructuring = !bindingElement->isKind(ParseNodeKind::Name);

    // Left-hand sides are either simple names or destructuring patterns.
    MOZ_ASSERT(bindingElement->isKind(ParseNodeKind::Name) ||
               bindingElement->isKind(ParseNodeKind::ArrayExpr) ||
               bindingElement->isKind(ParseNodeKind::ObjectExpr));

    auto emitDefaultInitializer = [this, &initializer, &bindingElement]() {
      //            [stack]

      if (!this->emitInitializer(initializer, bindingElement)) {
        //          [stack] DEFAULT
        return false;
      }
      return true;
    };

    auto emitDestructuring = [this, &fpe, &bindingElement]() {
      //            [stack] ARG

      // If there's an parameter expression var scope, the destructuring
      // declaration needs to initialize the name in the function scope,
      // which is not the innermost scope.
      if (!this->emitDestructuringOps(&bindingElement->as<ListNode>(),
                                      fpe.getDestructuringFlavor())) {
        //          [stack] ARG
        return false;
      }

      return true;
    };

    if (isRest) {
      if (isDestructuring) {
        if (!fpe.prepareForDestructuringRest()) {
          //        [stack]
          return false;
        }
        if (!emitDestructuring()) {
          //        [stack]
          return false;
        }
        if (!fpe.emitDestructuringRestEnd()) {
          //        [stack]
          return false;
        }
      } else {
        RootedAtom paramName(cx, bindingElement->as<NameNode>().name());
        if (!fpe.emitRest(paramName)) {
          //        [stack]
          return false;
        }
      }

      continue;
    }

    if (isDestructuring) {
      if (hasInitializer) {
        if (!fpe.prepareForDestructuringDefaultInitializer()) {
          //        [stack]
          return false;
        }
        if (!emitDefaultInitializer()) {
          //        [stack]
          return false;
        }
        if (!fpe.prepareForDestructuringDefault()) {
          //        [stack]
          return false;
        }
        if (!emitDestructuring()) {
          //        [stack]
          return false;
        }
        if (!fpe.emitDestructuringDefaultEnd()) {
          //        [stack]
          return false;
        }
      } else {
        if (!fpe.prepareForDestructuring()) {
          //        [stack]
          return false;
        }
        if (!emitDestructuring()) {
          //        [stack]
          return false;
        }
        if (!fpe.emitDestructuringEnd()) {
          //        [stack]
          return false;
        }
      }

      continue;
    }

    if (hasInitializer) {
      if (!fpe.prepareForDefault()) {
        //          [stack]
        return false;
      }
      if (!emitDefaultInitializer()) {
        //          [stack]
        return false;
      }
      RootedAtom paramName(cx, bindingElement->as<NameNode>().name());
      if (!fpe.emitDefaultEnd(paramName)) {
        //          [stack]
        return false;
      }

      continue;
    }

    RootedAtom paramName(cx, bindingElement->as<NameNode>().name());
    if (!fpe.emitSimple(paramName)) {
      //            [stack]
      return false;
    }
  }

  return true;
}

bool BytecodeEmitter::emitInitializeFunctionSpecialNames() {
  FunctionBox* funbox = sc->asFunctionBox();

  //                [stack]

  auto emitInitializeFunctionSpecialName =
      [](BytecodeEmitter* bce, HandlePropertyName name, JSOp op) {
        // A special name must be slotful, either on the frame or on the
        // call environment.
        MOZ_ASSERT(bce->lookupName(name).hasKnownSlot());

        NameOpEmitter noe(bce, name, NameOpEmitter::Kind::Initialize);
        if (!noe.prepareForRhs()) {
          //        [stack]
          return false;
        }
        if (!bce->emit1(op)) {
          //        [stack] THIS/ARGUMENTS
          return false;
        }
        if (!noe.emitAssignment()) {
          //        [stack] THIS/ARGUMENTS
          return false;
        }
        if (!bce->emit1(JSOP_POP)) {
          //        [stack]
          return false;
        }

        return true;
      };

  // Do nothing if the function doesn't have an arguments binding.
  if (funbox->argumentsHasLocalBinding()) {
    if (!emitInitializeFunctionSpecialName(this, cx->names().arguments,
                                           JSOP_ARGUMENTS)) {
      //            [stack]
      return false;
    }
  }

  // Do nothing if the function doesn't have a this-binding (this
  // happens for instance if it doesn't use this/eval or if it's an
  // arrow function).
  if (funbox->hasThisBinding()) {
    if (!emitInitializeFunctionSpecialName(this, cx->names().dotThis,
                                           JSOP_FUNCTIONTHIS)) {
      return false;
    }
  }

  // Do nothing if the function doesn't implicitly return a promise result.
  if (funbox->needsPromiseResult()) {
    if (!emitInitializeFunctionSpecialName(this, cx->names().dotGenerator,
                                           JSOP_GENERATOR)) {
      //            [stack]
      return false;
    }
  }
  return true;
}

bool BytecodeEmitter::emitLexicalInitialization(NameNode* name) {
  RootedAtom nameAtom(cx, name->name());
  return emitLexicalInitialization(nameAtom);
}

bool BytecodeEmitter::emitLexicalInitialization(Handle<JSAtom*> name) {
  NameOpEmitter noe(this, name, NameOpEmitter::Kind::Initialize);
  if (!noe.prepareForRhs()) {
    return false;
  }

  // The caller has pushed the RHS to the top of the stack. Assert that the
  // name is lexical and no BIND[G]NAME ops were emitted.
  MOZ_ASSERT(noe.loc().isLexical());
  MOZ_ASSERT(!noe.emittedBindOp());

  if (!noe.emitAssignment()) {
    return false;
  }

  return true;
}

static MOZ_ALWAYS_INLINE ParseNode* FindConstructor(JSContext* cx,
                                                    ListNode* classMethods) {
  for (ParseNode* classElement : classMethods->contents()) {
    ParseNode* unwrappedElement = classElement;
    if (unwrappedElement->is<LexicalScopeNode>()) {
      unwrappedElement = unwrappedElement->as<LexicalScopeNode>().scopeBody();
    }
    if (unwrappedElement->is<ClassMethod>()) {
      ClassMethod& method = unwrappedElement->as<ClassMethod>();
      ParseNode& methodName = method.name();
      if (!method.isStatic() &&
          (methodName.isKind(ParseNodeKind::ObjectPropertyName) ||
           methodName.isKind(ParseNodeKind::StringExpr)) &&
          methodName.as<NameNode>().atom() == cx->names().constructor) {
        return classElement;
      }
    }
  }
  return nullptr;
}

// This follows ES6 14.5.14 (ClassDefinitionEvaluation) and ES6 14.5.15
// (BindingClassDeclarationEvaluation).
bool BytecodeEmitter::emitClass(
    ClassNode* classNode,
    ClassNameKind nameKind /* = ClassNameKind::BindingName */,
    HandleAtom nameForAnonymousClass /* = nullptr */) {
  MOZ_ASSERT((nameKind == ClassNameKind::InferredName) ==
             (nameForAnonymousClass != nullptr));

  ParseNode* heritageExpression = classNode->heritage();
  ListNode* classMembers = classNode->memberList();
  ParseNode* constructor = FindConstructor(cx, classMembers);

  // If |nameKind != ClassNameKind::ComputedName|
  //                [stack]
  // Else
  //                [stack] NAME

  ClassEmitter ce(this);
  RootedAtom innerName(cx);
  ClassEmitter::Kind kind = ClassEmitter::Kind::Expression;
  if (ClassNames* names = classNode->names()) {
    MOZ_ASSERT(nameKind == ClassNameKind::BindingName);
    innerName = names->innerBinding()->name();
    MOZ_ASSERT(innerName);

    if (names->outerBinding()) {
      MOZ_ASSERT(names->outerBinding()->name());
      MOZ_ASSERT(names->outerBinding()->name() == innerName);
      kind = ClassEmitter::Kind::Declaration;
    }
  }

  if (LexicalScopeNode* scopeBindings = classNode->scopeBindings()) {
    if (!ce.emitScope(scopeBindings->scopeBindings())) {
      //            [stack]
      return false;
    }
  }

  // This is kind of silly. In order to the get the home object defined on
  // the constructor, we have to make it second, but we want the prototype
  // on top for EmitPropertyList, because we expect static properties to be
  // rarer. The result is a few more swaps than we would like. Such is life.
  bool isDerived = !!heritageExpression;
  bool hasNameOnStack = nameKind == ClassNameKind::ComputedName;
  if (isDerived) {
    if (!updateSourceCoordNotes(classNode->pn_pos.begin)) {
      return false;
    }
    if (!markStepBreakpoint()) {
      return false;
    }
    if (!emitTree(heritageExpression)) {
      //            [stack] HERITAGE
      return false;
    }
    if (!ce.emitDerivedClass(innerName, nameForAnonymousClass,
                             hasNameOnStack)) {
      //            [stack] HERITAGE HOMEOBJ
      return false;
    }
  } else {
    if (!ce.emitClass(innerName, nameForAnonymousClass, hasNameOnStack)) {
      //            [stack] HOMEOBJ
      return false;
    }
  }

  // Stack currently has HOMEOBJ followed by optional HERITAGE. When HERITAGE
  // is not used, an implicit value of %FunctionPrototype% is implied.
  if (constructor) {
    FunctionNode* ctor;
    // .fieldKeys must be declared outside the scope .initializers is declared
    // in, hence this extra scope.
    Maybe<LexicalScopeEmitter> lse;
    if (constructor->is<LexicalScopeNode>()) {
      if (!constructor->as<LexicalScopeNode>().isEmptyScope()) {
        lse.emplace(this);
        if (!lse->emitScope(
                ScopeKind::Lexical,
                constructor->as<LexicalScopeNode>().scopeBindings())) {
          return false;
        }

        // Any class with field initializers will have a constructor
        if (!emitCreateFieldInitializers(ce, classMembers)) {
          return false;
        }
      }

      ctor = &constructor->as<LexicalScopeNode>()
                  .scopeBody()
                  ->as<ClassMethod>()
                  .method();
    } else {
      ctor = &constructor->as<ClassMethod>().method();
    }

    bool needsHomeObject = ctor->funbox()->needsHomeObject();
    // HERITAGE is consumed inside emitFunction.
    if (!emitFunction(ctor, isDerived, classMembers)) {
      //            [stack] HOMEOBJ CTOR
      return false;
    }
    if (nameKind == ClassNameKind::InferredName) {
      if (!setFunName(ctor->funbox()->function(), nameForAnonymousClass)) {
        return false;
      }
    }
    if (lse.isSome()) {
      if (!lse->emitEnd()) {
        return false;
      }
      lse.reset();
    }
    if (!ce.emitInitConstructor(needsHomeObject)) {
      //            [stack] CTOR HOMEOBJ
      return false;
    }
  } else {
    if (!ce.emitInitDefaultConstructor(Some(classNode->pn_pos.begin),
                                       Some(classNode->pn_pos.end))) {
      //            [stack] CTOR HOMEOBJ
      return false;
    }
  }

  if (!emitCreateFieldKeys(classMembers)) {
    return false;
  }

  if (!emitPropertyList(classMembers, ce, ClassBody)) {
    //              [stack] CTOR HOMEOBJ
    return false;
  }

  if (!ce.emitEnd(kind)) {
    //              [stack] # class declaration
    //              [stack]
    //              [stack] # class expression
    //              [stack] CTOR
    return false;
  }

  return true;
}

bool BytecodeEmitter::emitExportDefault(BinaryNode* exportNode) {
  MOZ_ASSERT(exportNode->isKind(ParseNodeKind::ExportDefaultStmt));

  ParseNode* valueNode = exportNode->left();
  if (valueNode->isDirectRHSAnonFunction()) {
    MOZ_ASSERT(exportNode->right());

    HandlePropertyName name = cx->names().default_;
    if (!emitAnonymousFunctionWithName(valueNode, name)) {
      return false;
    }
  } else {
    if (!emitTree(valueNode)) {
      return false;
    }
  }

  if (ParseNode* binding = exportNode->right()) {
    if (!emitLexicalInitialization(&binding->as<NameNode>())) {
      return false;
    }

    if (!emit1(JSOP_POP)) {
      return false;
    }
  }

  return true;
}

MOZ_NEVER_INLINE bool BytecodeEmitter::emitInstrumentationSlow(
    InstrumentationKind kind,
    const std::function<bool(uint32_t)>& pushOperandsCallback) {
  MOZ_ASSERT(instrumentationKinds);

  if (!(instrumentationKinds & (uint32_t)kind)) {
    return true;
  }

  // Instrumentation is emitted in the form of a call to the realm's
  // instrumentation callback, guarded by a test of whether instrumentation is
  // currently active in the realm. The callback is invoked with the kind of
  // operation which is executing, the current script's instrumentation ID, and
  // the offset of the bytecode location after the instrumentation. Some
  // operation kinds have more arguments, which will be pushed by
  // pushOperandsCallback.

  unsigned initialDepth = bytecodeSection().stackDepth();
  InternalIfEmitter ifEmitter(this);

  if (!emit1(JSOP_INSTRUMENTATION_ACTIVE)) {
    return false;
  }
  //                [stack] ACTIVE

  if (!ifEmitter.emitThen()) {
    return false;
  }
  //                [stack]

  // Push the instrumentation callback for the current realm as the callee.
  if (!emit1(JSOP_INSTRUMENTATION_CALLBACK)) {
    return false;
  }
  //                [stack] CALLBACK

  // Push undefined for the call's |this| value.
  if (!emit1(JSOP_UNDEFINED)) {
    return false;
  }
  //                [stack] CALLBACK UNDEFINED

  JSAtom* atom = RealmInstrumentation::getInstrumentationKindName(cx, kind);
  if (!atom) {
    return false;
  }

  uint32_t index;
  if (!makeAtomIndex(atom, &index)) {
    return false;
  }

  if (!emitAtomOp(index, JSOP_STRING)) {
    return false;
  }
  //                [stack] CALLBACK UNDEFINED KIND

  if (!emit1(JSOP_INSTRUMENTATION_SCRIPT_ID)) {
    return false;
  }
  //                [stack] CALLBACK UNDEFINED KIND SCRIPT

  // Push the offset of the bytecode location following the instrumentation.
  BytecodeOffset updateOffset;
  if (!emitN(JSOP_INT32, 4, &updateOffset)) {
    return false;
  }
  //                [stack] CALLBACK UNDEFINED KIND SCRIPT OFFSET

  unsigned numPushed = bytecodeSection().stackDepth() - initialDepth;

  if (pushOperandsCallback && !pushOperandsCallback(numPushed)) {
    return false;
  }
  //                [stack] CALLBACK UNDEFINED KIND SCRIPT OFFSET ...EXTRA_ARGS

  unsigned argc = bytecodeSection().stackDepth() - initialDepth - 2;
  if (!emitCall(JSOP_CALL_IGNORES_RV, argc)) {
    return false;
  }
  //                [stack] RV

  if (!emit1(JSOP_POP)) {
    return false;
  }
  //                [stack]

  if (!ifEmitter.emitEnd()) {
    return false;
  }

  SET_INT32(bytecodeSection().code(updateOffset),
            bytecodeSection().code().length());

  return true;
}

MOZ_NEVER_INLINE bool BytecodeEmitter::emitInstrumentationForOpcodeSlow(
    JSOp op, uint32_t atomIndex) {
  MOZ_ASSERT(instrumentationKinds);

  switch (op) {
    case JSOP_GETPROP:
    case JSOP_CALLPROP:
    case JSOP_LENGTH:
      return emitInstrumentationSlow(
          InstrumentationKind::GetProperty, [=](uint32_t pushed) {
            return emitDupAt(pushed) && emitAtomOp(atomIndex, JSOP_STRING);
          });
    case JSOP_SETPROP:
    case JSOP_STRICTSETPROP:
      return emitInstrumentationSlow(
          InstrumentationKind::SetProperty, [=](uint32_t pushed) {
            return emitDupAt(pushed + 1) &&
                   emitAtomOp(atomIndex, JSOP_STRING) && emitDupAt(pushed + 2);
          });
    case JSOP_GETELEM:
    case JSOP_CALLELEM:
      return emitInstrumentationSlow(
          InstrumentationKind::GetElement,
          [=](uint32_t pushed) { return emitDupAt(pushed + 1, 2); });
    case JSOP_SETELEM:
    case JSOP_STRICTSETELEM:
      return emitInstrumentationSlow(
          InstrumentationKind::SetElement,
          [=](uint32_t pushed) { return emitDupAt(pushed + 2, 3); });
    default:
      return true;
  }
}

bool BytecodeEmitter::emitTree(
    ParseNode* pn, ValueUsage valueUsage /* = ValueUsage::WantValue */,
    EmitLineNumberNote emitLineNote /* = EMIT_LINENOTE */) {
  if (!CheckRecursionLimit(cx)) {
    return false;
  }

  /* Emit notes to tell the current bytecode's source line number.
     However, a couple trees require special treatment; see the
     relevant emitter functions for details. */
  if (emitLineNote == EMIT_LINENOTE &&
      !ParseNodeRequiresSpecialLineNumberNotes(pn)) {
    if (!updateLineNumberNotes(pn->pn_pos.begin)) {
      return false;
    }
  }

  switch (pn->getKind()) {
    case ParseNodeKind::Function:
      if (!emitFunction(&pn->as<FunctionNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::ParamsBody:
      MOZ_ASSERT_UNREACHABLE(
          "ParamsBody should be handled in emitFunctionScript.");
      break;

    case ParseNodeKind::IfStmt:
      if (!emitIf(&pn->as<TernaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::SwitchStmt:
      if (!emitSwitch(&pn->as<SwitchStatement>())) {
        return false;
      }
      break;

    case ParseNodeKind::WhileStmt:
      if (!emitWhile(&pn->as<BinaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::DoWhileStmt:
      if (!emitDo(&pn->as<BinaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::ForStmt:
      if (!emitFor(&pn->as<ForNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::BreakStmt:
      // Ensure that the column of the 'break' is set properly.
      if (!updateSourceCoordNotes(pn->pn_pos.begin)) {
        return false;
      }
      if (!markStepBreakpoint()) {
        return false;
      }

      if (!emitBreak(pn->as<BreakStatement>().label())) {
        return false;
      }
      break;

    case ParseNodeKind::ContinueStmt:
      // Ensure that the column of the 'continue' is set properly.
      if (!updateSourceCoordNotes(pn->pn_pos.begin)) {
        return false;
      }
      if (!markStepBreakpoint()) {
        return false;
      }

      if (!emitContinue(pn->as<ContinueStatement>().label())) {
        return false;
      }
      break;

    case ParseNodeKind::WithStmt:
      if (!emitWith(&pn->as<BinaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::TryStmt:
      if (!emitTry(&pn->as<TryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::Catch:
      if (!emitCatch(&pn->as<BinaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::VarStmt:
      if (!emitDeclarationList(&pn->as<ListNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::ReturnStmt:
      if (!emitReturn(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::YieldStarExpr:
      if (!emitYieldStar(pn->as<UnaryNode>().kid())) {
        return false;
      }
      break;

    case ParseNodeKind::Generator:
      if (!emit1(JSOP_GENERATOR)) {
        return false;
      }
      break;

    case ParseNodeKind::InitialYield:
      if (!emitInitialYield(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::YieldExpr:
      if (!emitYield(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::AwaitExpr:
      if (!emitAwaitInInnermostScope(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::StatementList:
      if (!emitStatementList(&pn->as<ListNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::EmptyStmt:
      break;

    case ParseNodeKind::ExpressionStmt:
      if (!emitExpressionStatement(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::LabelStmt:
      if (!emitLabeledStatement(&pn->as<LabeledStatement>())) {
        return false;
      }
      break;

    case ParseNodeKind::CommaExpr:
      if (!emitSequenceExpr(&pn->as<ListNode>(), valueUsage)) {
        return false;
      }
      break;

    case ParseNodeKind::InitExpr:
    case ParseNodeKind::AssignExpr:
    case ParseNodeKind::AddAssignExpr:
    case ParseNodeKind::SubAssignExpr:
    case ParseNodeKind::BitOrAssignExpr:
    case ParseNodeKind::BitXorAssignExpr:
    case ParseNodeKind::BitAndAssignExpr:
    case ParseNodeKind::LshAssignExpr:
    case ParseNodeKind::RshAssignExpr:
    case ParseNodeKind::UrshAssignExpr:
    case ParseNodeKind::MulAssignExpr:
    case ParseNodeKind::DivAssignExpr:
    case ParseNodeKind::ModAssignExpr:
    case ParseNodeKind::PowAssignExpr: {
      BinaryNode* assignNode = &pn->as<BinaryNode>();
      if (!emitAssignmentOrInit(assignNode->getKind(), assignNode->left(),
                                assignNode->right())) {
        return false;
      }
      break;
    }

    case ParseNodeKind::ConditionalExpr:
      if (!emitConditionalExpression(pn->as<ConditionalExpression>(),
                                     valueUsage)) {
        return false;
      }
      break;

    case ParseNodeKind::OrExpr:
    case ParseNodeKind::AndExpr:
      if (!emitLogical(&pn->as<ListNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::AddExpr:
    case ParseNodeKind::SubExpr:
    case ParseNodeKind::BitOrExpr:
    case ParseNodeKind::BitXorExpr:
    case ParseNodeKind::BitAndExpr:
    case ParseNodeKind::StrictEqExpr:
    case ParseNodeKind::EqExpr:
    case ParseNodeKind::StrictNeExpr:
    case ParseNodeKind::NeExpr:
    case ParseNodeKind::LtExpr:
    case ParseNodeKind::LeExpr:
    case ParseNodeKind::GtExpr:
    case ParseNodeKind::GeExpr:
    case ParseNodeKind::InExpr:
    case ParseNodeKind::InstanceOfExpr:
    case ParseNodeKind::LshExpr:
    case ParseNodeKind::RshExpr:
    case ParseNodeKind::UrshExpr:
    case ParseNodeKind::MulExpr:
    case ParseNodeKind::DivExpr:
    case ParseNodeKind::ModExpr:
      if (!emitLeftAssociative(&pn->as<ListNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::PowExpr:
      if (!emitRightAssociative(&pn->as<ListNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::PipelineExpr:
      if (!emitPipeline(&pn->as<ListNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::TypeOfNameExpr:
      if (!emitTypeof(&pn->as<UnaryNode>(), JSOP_TYPEOF)) {
        return false;
      }
      break;

    case ParseNodeKind::TypeOfExpr:
      if (!emitTypeof(&pn->as<UnaryNode>(), JSOP_TYPEOFEXPR)) {
        return false;
      }
      break;

    case ParseNodeKind::ThrowStmt:
      if (!updateSourceCoordNotes(pn->pn_pos.begin)) {
        return false;
      }
      if (!markStepBreakpoint()) {
        return false;
      }
      MOZ_FALLTHROUGH;
    case ParseNodeKind::VoidExpr:
    case ParseNodeKind::NotExpr:
    case ParseNodeKind::BitNotExpr:
    case ParseNodeKind::PosExpr:
    case ParseNodeKind::NegExpr:
      if (!emitUnary(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::PreIncrementExpr:
    case ParseNodeKind::PreDecrementExpr:
    case ParseNodeKind::PostIncrementExpr:
    case ParseNodeKind::PostDecrementExpr:
      if (!emitIncOrDec(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::DeleteNameExpr:
      if (!emitDeleteName(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::DeletePropExpr:
      if (!emitDeleteProperty(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::DeleteElemExpr:
      if (!emitDeleteElement(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::DeleteExpr:
      if (!emitDeleteExpression(&pn->as<UnaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::DotExpr: {
      PropertyAccess* prop = &pn->as<PropertyAccess>();
      bool isSuper = prop->isSuper();
      PropOpEmitter poe(this, PropOpEmitter::Kind::Get,
                        isSuper ? PropOpEmitter::ObjKind::Super
                                : PropOpEmitter::ObjKind::Other);
      if (!poe.prepareForObj()) {
        return false;
      }
      if (isSuper) {
        UnaryNode* base = &prop->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {
          //        [stack] THIS
          return false;
        }
      } else {
        if (!emitPropLHS(prop)) {
          //        [stack] OBJ
          return false;
        }
      }
      if (!poe.emitGet(prop->key().atom())) {
        //          [stack] PROP
        return false;
      }
      break;
    }

    case ParseNodeKind::ElemExpr: {
      PropertyByValue* elem = &pn->as<PropertyByValue>();
      bool isSuper = elem->isSuper();
      ElemOpEmitter eoe(this, ElemOpEmitter::Kind::Get,
                        isSuper ? ElemOpEmitter::ObjKind::Super
                                : ElemOpEmitter::ObjKind::Other);
      if (!emitElemObjAndKey(elem, isSuper, eoe)) {
        //          [stack] # if Super
        //          [stack] THIS KEY
        //          [stack] # otherwise
        //          [stack] OBJ KEY
        return false;
      }
      if (!eoe.emitGet()) {
        //          [stack] ELEM
        return false;
      }

      break;
    }

    case ParseNodeKind::NewExpr:
    case ParseNodeKind::TaggedTemplateExpr:
    case ParseNodeKind::CallExpr:
    case ParseNodeKind::SuperCallExpr:
      if (!emitCallOrNew(&pn->as<CallNode>(), valueUsage)) {
        return false;
      }
      break;

    case ParseNodeKind::LexicalScope:
      if (!emitLexicalScope(&pn->as<LexicalScopeNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::ConstDecl:
    case ParseNodeKind::LetDecl:
      if (!emitDeclarationList(&pn->as<ListNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::ImportDecl:
      MOZ_ASSERT(sc->isModuleContext());
      break;

    case ParseNodeKind::ExportStmt: {
      MOZ_ASSERT(sc->isModuleContext());
      UnaryNode* node = &pn->as<UnaryNode>();
      ParseNode* decl = node->kid();
      if (decl->getKind() != ParseNodeKind::ExportSpecList) {
        if (!emitTree(decl)) {
          return false;
        }
      }
      break;
    }

    case ParseNodeKind::ExportDefaultStmt:
      MOZ_ASSERT(sc->isModuleContext());
      if (!emitExportDefault(&pn->as<BinaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::ExportFromStmt:
      MOZ_ASSERT(sc->isModuleContext());
      break;

    case ParseNodeKind::CallSiteObj:
      if (!emitCallSiteObject(&pn->as<CallSiteNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::ArrayExpr:
      if (!emitArrayLiteral(&pn->as<ListNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::ObjectExpr:
      if (!emitObject(&pn->as<ListNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::Name:
      if (!emitGetName(&pn->as<NameNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::TemplateStringListExpr:
      if (!emitTemplateString(&pn->as<ListNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::TemplateStringExpr:
    case ParseNodeKind::StringExpr:
      if (!emitAtomOp(pn->as<NameNode>().atom(), JSOP_STRING)) {
        return false;
      }
      break;

    case ParseNodeKind::NumberExpr:
      if (!emitNumberOp(pn->as<NumericLiteral>().value())) {
        return false;
      }
      break;

    case ParseNodeKind::BigIntExpr:
      if (!emitBigIntOp(&pn->as<BigIntLiteral>())) {
        return false;
      }
      break;

    case ParseNodeKind::RegExpExpr: {
      ObjectBox* obj = pn->as<RegExpLiteral>().objbox();
      uint32_t index;
      if (!perScriptData().gcThingList().append(obj, &index)) {
        return false;
      }
      if (!emitRegExp(index)) {
        return false;
      }
      break;
    }

    case ParseNodeKind::TrueExpr:
      if (!emit1(JSOP_TRUE)) {
        return false;
      }
      break;
    case ParseNodeKind::FalseExpr:
      if (!emit1(JSOP_FALSE)) {
        return false;
      }
      break;
    case ParseNodeKind::NullExpr:
      if (!emit1(JSOP_NULL)) {
        return false;
      }
      break;
    case ParseNodeKind::RawUndefinedExpr:
      if (!emit1(JSOP_UNDEFINED)) {
        return false;
      }
      break;

    case ParseNodeKind::ThisExpr:
      if (!emitThisLiteral(&pn->as<ThisLiteral>())) {
        return false;
      }
      break;

    case ParseNodeKind::DebuggerStmt:
      if (!updateSourceCoordNotes(pn->pn_pos.begin)) {
        return false;
      }
      if (!markStepBreakpoint()) {
        return false;
      }
      if (!emit1(JSOP_DEBUGGER)) {
        return false;
      }
      break;

    case ParseNodeKind::ClassDecl:
      if (!emitClass(&pn->as<ClassNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::NewTargetExpr:
      if (!emit1(JSOP_NEWTARGET)) {
        return false;
      }
      break;

    case ParseNodeKind::ImportMetaExpr:
      if (!emit1(JSOP_IMPORTMETA)) {
        return false;
      }
      break;

    case ParseNodeKind::CallImportExpr:
      if (!emitTree(pn->as<BinaryNode>().right()) ||
          !emit1(JSOP_DYNAMIC_IMPORT)) {
        return false;
      }
      break;

    case ParseNodeKind::SetThis:
      if (!emitSetThis(&pn->as<BinaryNode>())) {
        return false;
      }
      break;

    case ParseNodeKind::PropertyNameExpr:
    case ParseNodeKind::PosHolder:
      MOZ_FALLTHROUGH_ASSERT(
          "Should never try to emit ParseNodeKind::PosHolder or ::Property");

    default:
      MOZ_ASSERT(0);
  }

  return true;
}

static bool AllocSrcNote(JSContext* cx, SrcNotesVector& notes,
                         unsigned* index) {
  size_t oldLength = notes.length();

  if (MOZ_UNLIKELY(oldLength + 1 > MaxSrcNotesLength)) {
    ReportAllocationOverflow(cx);
    return false;
  }

  if (!notes.growByUninitialized(1)) {
    return false;
  }

  *index = oldLength;
  return true;
}

bool BytecodeEmitter::addTryNote(JSTryNoteKind kind, uint32_t stackDepth,
                                 BytecodeOffset start, BytecodeOffset end) {
  MOZ_ASSERT(!inPrologue());
  return bytecodeSection().tryNoteList().append(kind, stackDepth, start, end);
}

bool BytecodeEmitter::newSrcNote(SrcNoteType type, unsigned* indexp) {
  // Prologue shouldn't have source notes.
  MOZ_ASSERT(!inPrologue());
  SrcNotesVector& notes = bytecodeSection().notes();
  unsigned index;
  if (!AllocSrcNote(cx, notes, &index)) {
    return false;
  }

  /*
   * Compute delta from the last annotated bytecode's offset.  If it's too
   * big to fit in sn, allocate one or more xdelta notes and reset sn.
   */
  BytecodeOffset offset = bytecodeSection().offset();
  ptrdiff_t delta = (offset - bytecodeSection().lastNoteOffset()).value();
  bytecodeSection().setLastNoteOffset(offset);
  if (delta >= SN_DELTA_LIMIT) {
    do {
      ptrdiff_t xdelta = Min(delta, SN_XDELTA_MASK);
      SN_MAKE_XDELTA(&notes[index], xdelta);
      delta -= xdelta;
      if (!AllocSrcNote(cx, notes, &index)) {
        return false;
      }
    } while (delta >= SN_DELTA_LIMIT);
  }

  /*
   * Initialize type and delta, then allocate the minimum number of notes
   * needed for type's arity.  Usually, we won't need more, but if an offset
   * does take two bytes, setSrcNoteOffset will grow notes.
   */
  SN_MAKE_NOTE(&notes[index], type, delta);
  for (int n = (int)js_SrcNoteSpec[type].arity; n > 0; n--) {
    if (!newSrcNote(SRC_NULL)) {
      return false;
    }
  }

  if (indexp) {
    *indexp = index;
  }
  return true;
}

bool BytecodeEmitter::newSrcNote2(SrcNoteType type, ptrdiff_t offset,
                                  unsigned* indexp) {
  unsigned index;
  if (!newSrcNote(type, &index)) {
    return false;
  }
  if (!setSrcNoteOffset(index, 0, BytecodeOffsetDiff(offset))) {
    return false;
  }
  if (indexp) {
    *indexp = index;
  }
  return true;
}

bool BytecodeEmitter::newSrcNote3(SrcNoteType type, ptrdiff_t offset1,
                                  ptrdiff_t offset2, unsigned* indexp) {
  unsigned index;
  if (!newSrcNote(type, &index)) {
    return false;
  }
  if (!setSrcNoteOffset(index, 0, BytecodeOffsetDiff(offset1))) {
    return false;
  }
  if (!setSrcNoteOffset(index, 1, BytecodeOffsetDiff(offset2))) {
    return false;
  }
  if (indexp) {
    *indexp = index;
  }
  return true;
}

bool BytecodeEmitter::setSrcNoteOffset(unsigned index, unsigned which,
                                       BytecodeOffsetDiff offset) {
  ptrdiff_t offsetValue = offset.value();

  if (!SN_REPRESENTABLE_OFFSET(offsetValue)) {
    reportError(nullptr, JSMSG_NEED_DIET, js_script_str);
    return false;
  }

  SrcNotesVector& notes = bytecodeSection().notes();

  /* Find the offset numbered which (i.e., skip exactly which offsets). */
  jssrcnote* sn = &notes[index];
  MOZ_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
  MOZ_ASSERT((int)which < js_SrcNoteSpec[SN_TYPE(sn)].arity);
  for (sn++; which; sn++, which--) {
    if (*sn & SN_4BYTE_OFFSET_FLAG) {
      sn += 3;
    }
  }

  /*
   * See if the new offset requires four bytes either by being too big or if
   * the offset has already been inflated (in which case, we need to stay big
   * to not break the srcnote encoding if this isn't the last srcnote).
   */
  if (offsetValue > (ptrdiff_t)SN_4BYTE_OFFSET_MASK ||
      (*sn & SN_4BYTE_OFFSET_FLAG)) {
    /* Maybe this offset was already set to a four-byte value. */
    if (!(*sn & SN_4BYTE_OFFSET_FLAG)) {
      /* Insert three dummy bytes that will be overwritten shortly. */
      if (MOZ_UNLIKELY(notes.length() + 3 > MaxSrcNotesLength)) {
        ReportAllocationOverflow(cx);
        return false;
      }
      jssrcnote dummy = 0;
      if (!(sn = notes.insert(sn, dummy)) || !(sn = notes.insert(sn, dummy)) ||
          !(sn = notes.insert(sn, dummy))) {
        return false;
      }
    }
    *sn++ = (jssrcnote)(SN_4BYTE_OFFSET_FLAG | (offsetValue >> 24));
    *sn++ = (jssrcnote)(offsetValue >> 16);
    *sn++ = (jssrcnote)(offsetValue >> 8);
  }
  *sn = (jssrcnote)offsetValue;
  return true;
}

const JSSrcNoteSpec js_SrcNoteSpec[] = {
#define DEFINE_SRC_NOTE_SPEC(sym, name, arity) {name, arity},
    FOR_EACH_SRC_NOTE_TYPE(DEFINE_SRC_NOTE_SPEC)
#undef DEFINE_SRC_NOTE_SPEC
};

static int SrcNoteArity(jssrcnote* sn) {
  MOZ_ASSERT(SN_TYPE(sn) < SRC_LAST);
  return js_SrcNoteSpec[SN_TYPE(sn)].arity;
}

JS_FRIEND_API unsigned js::SrcNoteLength(jssrcnote* sn) {
  unsigned arity;
  jssrcnote* base;

  arity = SrcNoteArity(sn);
  for (base = sn++; arity; sn++, arity--) {
    if (*sn & SN_4BYTE_OFFSET_FLAG) {
      sn += 3;
    }
  }
  return sn - base;
}

JS_FRIEND_API ptrdiff_t js::GetSrcNoteOffset(jssrcnote* sn, unsigned which) {
  /* Find the offset numbered which (i.e., skip exactly which offsets). */
  MOZ_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
  MOZ_ASSERT((int)which < SrcNoteArity(sn));
  for (sn++; which; sn++, which--) {
    if (*sn & SN_4BYTE_OFFSET_FLAG) {
      sn += 3;
    }
  }
  if (*sn & SN_4BYTE_OFFSET_FLAG) {
    return (ptrdiff_t)(((uint32_t)(sn[0] & SN_4BYTE_OFFSET_MASK) << 24) |
                       (sn[1] << 16) | (sn[2] << 8) | sn[3]);
  }
  return (ptrdiff_t)*sn;
}
