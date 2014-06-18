/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CompileInfo_h
#define jit_CompileInfo_h

#include "jsfun.h"

#include "jit/Registers.h"
#include "vm/ScopeObject.h"

namespace js {
namespace jit {

class TempAllocator;

inline unsigned
StartArgSlot(JSScript *script)
{
    // Reserved slots:
    // Slot 0: Scope chain.
    // Slot 1: Return value.

    // When needed:
    // Slot 2: Argumentsobject.

    // Note: when updating this, please also update the assert in SnapshotWriter::startFrame
    return 2 + (script->argumentsHasVarBinding() ? 1 : 0);
}

inline unsigned
CountArgSlots(JSScript *script, JSFunction *fun)
{
    // Slot x + 0: This value.
    // Slot x + 1: Argument 1.
    // ...
    // Slot x + n: Argument n.

    // Note: when updating this, please also update the assert in SnapshotWriter::startFrame
    return StartArgSlot(script) + (fun ? fun->nargs() + 1 : 0);
}


// The compiler at various points needs to be able to store references to the
// current inline path (the sequence of scripts and call-pcs that lead to the
// current function being inlined).
//
// To support this, the top-level IonBuilder keeps a tree that records the
// inlinings done during compilation.
class InlineScriptTree {
    // InlineScriptTree for the caller
    InlineScriptTree *caller_;

    // PC in the caller corresponding to this script.
    jsbytecode *callerPc_;

    // Script for this entry.
    JSScript *script_;

    // Child entries (linked together by nextCallee pointer)
    InlineScriptTree *children_;
    InlineScriptTree *nextCallee_;

  public:
    InlineScriptTree(InlineScriptTree *caller, jsbytecode *callerPc, JSScript *script)
      : caller_(caller), callerPc_(callerPc), script_(script),
        children_(nullptr), nextCallee_(nullptr)
    {}

    static InlineScriptTree *New(TempAllocator *allocator, InlineScriptTree *caller,
                                 jsbytecode *callerPc, JSScript *script);

    InlineScriptTree *addCallee(TempAllocator *allocator, jsbytecode *callerPc,
                                 JSScript *calleeScript);

    InlineScriptTree *caller() const {
        return caller_;
    }

    bool isOutermostCaller() const {
        return caller_ == nullptr;
    }
    InlineScriptTree *outermostCaller() {
        if (isOutermostCaller())
            return this;
        return caller_->outermostCaller();
    }

    jsbytecode *callerPc() const {
        return callerPc_;
    }

    JSScript *script() const {
        return script_;
    }

    InlineScriptTree *children() const {
        return children_;
    }
    InlineScriptTree *nextCallee() const {
        return nextCallee_;
    }
};

class BytecodeSite {
    // InlineScriptTree identifying innermost active function at site.
    InlineScriptTree *tree_;

    // Bytecode address within innermost active function.
    jsbytecode *pc_;

  public:
    BytecodeSite()
      : tree_(nullptr), pc_(nullptr)
    {}

    BytecodeSite(InlineScriptTree *tree, jsbytecode *pc)
      : tree_(tree), pc_(pc)
    {}

    InlineScriptTree *tree() const {
        return tree_;
    }

    jsbytecode *pc() const {
        return pc_;
    }
};


// Contains information about the compilation source for IR being generated.
class CompileInfo
{
  public:
    CompileInfo(JSScript *script, JSFunction *fun, jsbytecode *osrPc, bool constructing,
                ExecutionMode executionMode, bool scriptNeedsArgsObj,
                InlineScriptTree *inlineScriptTree)
      : script_(script), fun_(fun), osrPc_(osrPc), constructing_(constructing),
        executionMode_(executionMode), scriptNeedsArgsObj_(scriptNeedsArgsObj),
        inlineScriptTree_(inlineScriptTree)
    {
        JS_ASSERT_IF(osrPc, JSOp(*osrPc) == JSOP_LOOPENTRY);

        // The function here can flow in from anywhere so look up the canonical
        // function to ensure that we do not try to embed a nursery pointer in
        // jit-code. Precisely because it can flow in from anywhere, it's not
        // guaranteed to be non-lazy. Hence, don't access its script!
        if (fun_) {
            fun_ = fun_->nonLazyScript()->functionNonDelazifying();
            JS_ASSERT(fun_->isTenured());
        }

        osrStaticScope_ = osrPc ? script->getStaticScope(osrPc) : nullptr;

        nimplicit_ = StartArgSlot(script)                   /* scope chain and argument obj */
                   + (fun ? 1 : 0);                         /* this */
        nargs_ = fun ? fun->nargs() : 0;
        nfixedvars_ = script->nfixedvars();
        nlocals_ = script->nfixed();
        nstack_ = script->nslots() - script->nfixed();
        nslots_ = nimplicit_ + nargs_ + nlocals_ + nstack_;
    }

    CompileInfo(unsigned nlocals, ExecutionMode executionMode)
      : script_(nullptr), fun_(nullptr), osrPc_(nullptr), osrStaticScope_(nullptr),
        constructing_(false), executionMode_(executionMode), scriptNeedsArgsObj_(false),
        inlineScriptTree_(nullptr)
    {
        nimplicit_ = 0;
        nargs_ = 0;
        nfixedvars_ = 0;
        nlocals_ = nlocals;
        nstack_ = 1;  /* For FunctionCompiler::pushPhiInput/popPhiOutput */
        nslots_ = nlocals_ + nstack_;
    }

    JSScript *script() const {
        return script_;
    }
    bool compilingAsmJS() const {
        return script() == nullptr;
    }
    JSFunction *funMaybeLazy() const {
        return fun_;
    }
    bool constructing() const {
        return constructing_;
    }
    jsbytecode *osrPc() {
        return osrPc_;
    }
    NestedScopeObject *osrStaticScope() const {
        return osrStaticScope_;
    }
    InlineScriptTree *inlineScriptTree() const {
        return inlineScriptTree_;
    }

    bool hasOsrAt(jsbytecode *pc) {
        JS_ASSERT(JSOp(*pc) == JSOP_LOOPENTRY);
        return pc == osrPc();
    }

    jsbytecode *startPC() const {
        return script_->code();
    }
    jsbytecode *limitPC() const {
        return script_->codeEnd();
    }

    const char *filename() const {
        return script_->filename();
    }

    unsigned lineno() const {
        return script_->lineno();
    }
    unsigned lineno(jsbytecode *pc) const {
        return PCToLineNumber(script_, pc);
    }

    // Script accessors based on PC.

    JSAtom *getAtom(jsbytecode *pc) const {
        return script_->getAtom(GET_UINT32_INDEX(pc));
    }

    PropertyName *getName(jsbytecode *pc) const {
        return script_->getName(GET_UINT32_INDEX(pc));
    }

    inline RegExpObject *getRegExp(jsbytecode *pc) const;

    JSObject *getObject(jsbytecode *pc) const {
        return script_->getObject(GET_UINT32_INDEX(pc));
    }

    inline JSFunction *getFunction(jsbytecode *pc) const;

    const Value &getConst(jsbytecode *pc) const {
        return script_->getConst(GET_UINT32_INDEX(pc));
    }

    jssrcnote *getNote(GSNCache &gsn, jsbytecode *pc) const {
        return GetSrcNote(gsn, script(), pc);
    }

    // Total number of slots: args, locals, and stack.
    unsigned nslots() const {
        return nslots_;
    }

    // Number of slots needed for Scope chain, return value,
    // maybe argumentsobject and this value.
    unsigned nimplicit() const {
        return nimplicit_;
    }
    // Number of arguments (without counting this value).
    unsigned nargs() const {
        return nargs_;
    }
    // Number of slots needed for "fixed vars".  Note that this is only non-zero
    // for function code.
    unsigned nfixedvars() const {
        return nfixedvars_;
    }
    // Number of slots needed for all local variables.  This includes "fixed
    // vars" (see above) and also block-scoped locals.
    unsigned nlocals() const {
        return nlocals_;
    }
    unsigned ninvoke() const {
        return nslots_ - nstack_;
    }

    uint32_t scopeChainSlot() const {
        JS_ASSERT(script());
        return 0;
    }
    uint32_t returnValueSlot() const {
        JS_ASSERT(script());
        return 1;
    }
    uint32_t argsObjSlot() const {
        JS_ASSERT(hasArguments());
        return 2;
    }
    uint32_t thisSlot() const {
        JS_ASSERT(funMaybeLazy());
        JS_ASSERT(nimplicit_ > 0);
        return nimplicit_ - 1;
    }
    uint32_t firstArgSlot() const {
        return nimplicit_;
    }
    uint32_t argSlotUnchecked(uint32_t i) const {
        // During initialization, some routines need to get at arg
        // slots regardless of how regular argument access is done.
        JS_ASSERT(i < nargs_);
        return nimplicit_ + i;
    }
    uint32_t argSlot(uint32_t i) const {
        // This should only be accessed when compiling functions for
        // which argument accesses don't need to go through the
        // argument object.
        JS_ASSERT(!argsObjAliasesFormals());
        return argSlotUnchecked(i);
    }
    uint32_t firstLocalSlot() const {
        return nimplicit_ + nargs_;
    }
    uint32_t localSlot(uint32_t i) const {
        return firstLocalSlot() + i;
    }
    uint32_t firstStackSlot() const {
        return firstLocalSlot() + nlocals();
    }
    uint32_t stackSlot(uint32_t i) const {
        return firstStackSlot() + i;
    }

    uint32_t startArgSlot() const {
        JS_ASSERT(script());
        return StartArgSlot(script());
    }
    uint32_t endArgSlot() const {
        JS_ASSERT(script());
        return CountArgSlots(script(), funMaybeLazy());
    }

    uint32_t totalSlots() const {
        JS_ASSERT(script() && funMaybeLazy());
        return nimplicit() + nargs() + nlocals();
    }

    bool isSlotAliased(uint32_t index, NestedScopeObject *staticScope) const {
        JS_ASSERT(index >= startArgSlot());

        if (funMaybeLazy() && index == thisSlot())
            return false;

        uint32_t arg = index - firstArgSlot();
        if (arg < nargs())
            return script()->formalIsAliased(arg);

        uint32_t local = index - firstLocalSlot();
        if (local < nlocals()) {
            // First, check if this local is a var.
            if (local < nfixedvars())
                return script()->varIsAliased(local);

            // Otherwise, it might be part of a block scope.
            for (; staticScope; staticScope = staticScope->enclosingNestedScope()) {
                if (!staticScope->is<StaticBlockObject>())
                    continue;
                StaticBlockObject &blockObj = staticScope->as<StaticBlockObject>();
                if (blockObj.localOffset() < local) {
                    if (local - blockObj.localOffset() < blockObj.numVariables())
                        return blockObj.isAliased(local - blockObj.localOffset());
                    return false;
                }
            }

            // In this static scope, this var is dead.
            return false;
        }

        JS_ASSERT(index >= firstStackSlot());
        return false;
    }

    bool isSlotAliasedAtEntry(uint32_t index) const {
        return isSlotAliased(index, nullptr);
    }
    bool isSlotAliasedAtOsr(uint32_t index) const {
        return isSlotAliased(index, osrStaticScope());
    }

    bool hasArguments() const {
        return script()->argumentsHasVarBinding();
    }
    bool argumentsAliasesFormals() const {
        return script()->argumentsAliasesFormals();
    }
    bool needsArgsObj() const {
        return scriptNeedsArgsObj_;
    }
    bool argsObjAliasesFormals() const {
        return scriptNeedsArgsObj_ && !script()->strict();
    }

    ExecutionMode executionMode() const {
        return executionMode_;
    }

    bool executionModeIsAnalysis() const {
        return executionMode_ == DefinitePropertiesAnalysis || executionMode_ == ArgumentsUsageAnalysis;
    }

    bool isParallelExecution() const {
        return executionMode_ == ParallelExecution;
    }

    // Returns true if a slot can be observed out-side the current frame while
    // the frame is active on the stack.  This implies that these definitions
    // would have to be executed and that they cannot be removed even if they
    // are unused.
    bool isObservableSlot(uint32_t slot) const {
        if (!funMaybeLazy())
            return false;

        // The |this| value must always be observable.
        if (slot == thisSlot())
            return true;

        // If the function may need an arguments object, then make sure to
        // preserve the scope chain, because it may be needed to construct the
        // arguments object during bailout. If we've already created an
        // arguments object (or got one via OSR), preserve that as well.
        if (hasArguments() && (slot == scopeChainSlot() || slot == argsObjSlot()))
            return true;

        // Function.arguments can be used to access all arguments in non-strict
        // scripts, so we can't optimize out any arguments.
        if ((hasArguments() || !script()->strict()) &&
            firstArgSlot() <= slot && slot - firstArgSlot() < nargs())
        {
            return true;
        }

        return false;
    }

  private:
    unsigned nimplicit_;
    unsigned nargs_;
    unsigned nfixedvars_;
    unsigned nlocals_;
    unsigned nstack_;
    unsigned nslots_;
    JSScript *script_;
    JSFunction *fun_;
    jsbytecode *osrPc_;
    NestedScopeObject *osrStaticScope_;
    bool constructing_;
    ExecutionMode executionMode_;

    // Whether a script needs an arguments object is unstable over compilation
    // since the arguments optimization could be marked as failed on the main
    // thread, so cache a value here and use it throughout for consistency.
    bool scriptNeedsArgsObj_;

    InlineScriptTree *inlineScriptTree_;
};

} // namespace jit
} // namespace js

#endif /* jit_CompileInfo_h */
