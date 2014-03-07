/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineCompiler_h
#define jit_BaselineCompiler_h

#ifdef JS_ION

#include "jit/FixedList.h"
#if defined(JS_CODEGEN_X86)
# include "jit/x86/BaselineCompiler-x86.h"
#elif defined(JS_CODEGEN_X64)
# include "jit/x64/BaselineCompiler-x64.h"
#else
# include "jit/arm/BaselineCompiler-arm.h"
#endif

namespace js {
namespace jit {

#define OPCODE_LIST(_)         \
    _(JSOP_NOP)                \
    _(JSOP_LABEL)              \
    _(JSOP_POP)                \
    _(JSOP_POPN)               \
    _(JSOP_DUPAT)              \
    _(JSOP_ENTERWITH)          \
    _(JSOP_LEAVEWITH)          \
    _(JSOP_DUP)                \
    _(JSOP_DUP2)               \
    _(JSOP_SWAP)               \
    _(JSOP_PICK)               \
    _(JSOP_GOTO)               \
    _(JSOP_IFEQ)               \
    _(JSOP_IFNE)               \
    _(JSOP_AND)                \
    _(JSOP_OR)                 \
    _(JSOP_NOT)                \
    _(JSOP_POS)                \
    _(JSOP_LOOPHEAD)           \
    _(JSOP_LOOPENTRY)          \
    _(JSOP_VOID)               \
    _(JSOP_UNDEFINED)          \
    _(JSOP_HOLE)               \
    _(JSOP_NULL)               \
    _(JSOP_THIS)               \
    _(JSOP_TRUE)               \
    _(JSOP_FALSE)              \
    _(JSOP_ZERO)               \
    _(JSOP_ONE)                \
    _(JSOP_INT8)               \
    _(JSOP_INT32)              \
    _(JSOP_UINT16)             \
    _(JSOP_UINT24)             \
    _(JSOP_DOUBLE)             \
    _(JSOP_STRING)             \
    _(JSOP_OBJECT)             \
    _(JSOP_REGEXP)             \
    _(JSOP_LAMBDA)             \
    _(JSOP_BITOR)              \
    _(JSOP_BITXOR)             \
    _(JSOP_BITAND)             \
    _(JSOP_LSH)                \
    _(JSOP_RSH)                \
    _(JSOP_URSH)               \
    _(JSOP_ADD)                \
    _(JSOP_SUB)                \
    _(JSOP_MUL)                \
    _(JSOP_DIV)                \
    _(JSOP_MOD)                \
    _(JSOP_LT)                 \
    _(JSOP_LE)                 \
    _(JSOP_GT)                 \
    _(JSOP_GE)                 \
    _(JSOP_EQ)                 \
    _(JSOP_NE)                 \
    _(JSOP_STRICTEQ)           \
    _(JSOP_STRICTNE)           \
    _(JSOP_CONDSWITCH)         \
    _(JSOP_CASE)               \
    _(JSOP_DEFAULT)            \
    _(JSOP_LINENO)             \
    _(JSOP_BITNOT)             \
    _(JSOP_NEG)                \
    _(JSOP_NEWARRAY)           \
    _(JSOP_INITELEM_ARRAY)     \
    _(JSOP_NEWOBJECT)          \
    _(JSOP_NEWINIT)            \
    _(JSOP_INITELEM)           \
    _(JSOP_INITELEM_GETTER)    \
    _(JSOP_INITELEM_SETTER)    \
    _(JSOP_MUTATEPROTO)        \
    _(JSOP_INITPROP)           \
    _(JSOP_INITPROP_GETTER)    \
    _(JSOP_INITPROP_SETTER)    \
    _(JSOP_ENDINIT)            \
    _(JSOP_GETELEM)            \
    _(JSOP_SETELEM)            \
    _(JSOP_CALLELEM)           \
    _(JSOP_DELELEM)            \
    _(JSOP_IN)                 \
    _(JSOP_GETGNAME)           \
    _(JSOP_CALLGNAME)          \
    _(JSOP_BINDGNAME)          \
    _(JSOP_SETGNAME)           \
    _(JSOP_SETNAME)            \
    _(JSOP_GETPROP)            \
    _(JSOP_SETPROP)            \
    _(JSOP_CALLPROP)           \
    _(JSOP_DELPROP)            \
    _(JSOP_LENGTH)             \
    _(JSOP_GETXPROP)           \
    _(JSOP_GETALIASEDVAR)      \
    _(JSOP_CALLALIASEDVAR)     \
    _(JSOP_SETALIASEDVAR)      \
    _(JSOP_NAME)               \
    _(JSOP_CALLNAME)           \
    _(JSOP_BINDNAME)           \
    _(JSOP_DELNAME)            \
    _(JSOP_GETINTRINSIC)       \
    _(JSOP_CALLINTRINSIC)      \
    _(JSOP_DEFVAR)             \
    _(JSOP_DEFCONST)           \
    _(JSOP_SETCONST)           \
    _(JSOP_DEFFUN)             \
    _(JSOP_GETLOCAL)           \
    _(JSOP_CALLLOCAL)          \
    _(JSOP_SETLOCAL)           \
    _(JSOP_GETARG)             \
    _(JSOP_CALLARG)            \
    _(JSOP_SETARG)             \
    _(JSOP_CALL)               \
    _(JSOP_FUNCALL)            \
    _(JSOP_FUNAPPLY)           \
    _(JSOP_NEW)                \
    _(JSOP_EVAL)               \
    _(JSOP_IMPLICITTHIS)       \
    _(JSOP_INSTANCEOF)         \
    _(JSOP_TYPEOF)             \
    _(JSOP_TYPEOFEXPR)         \
    _(JSOP_SETCALL)            \
    _(JSOP_THROW)              \
    _(JSOP_TRY)                \
    _(JSOP_FINALLY)            \
    _(JSOP_GOSUB)              \
    _(JSOP_RETSUB)             \
    _(JSOP_PUSHBLOCKSCOPE)     \
    _(JSOP_POPBLOCKSCOPE)      \
    _(JSOP_DEBUGLEAVEBLOCK)    \
    _(JSOP_EXCEPTION)          \
    _(JSOP_DEBUGGER)           \
    _(JSOP_ARGUMENTS)          \
    _(JSOP_RUNONCE)            \
    _(JSOP_REST)               \
    _(JSOP_TOID)               \
    _(JSOP_TABLESWITCH)        \
    _(JSOP_ITER)               \
    _(JSOP_MOREITER)           \
    _(JSOP_ITERNEXT)           \
    _(JSOP_ENDITER)            \
    _(JSOP_CALLEE)             \
    _(JSOP_SETRVAL)            \
    _(JSOP_RETRVAL)            \
    _(JSOP_RETURN)

class BaselineCompiler : public BaselineCompilerSpecific
{
    FixedList<Label>            labels_;
    NonAssertingLabel           return_;
#ifdef JSGC_GENERATIONAL
    NonAssertingLabel           postBarrierSlot_;
#endif

    // Native code offset right before the scope chain is initialized.
    CodeOffsetLabel prologueOffset_;

    // Whether any on stack arguments are modified.
    bool modifiesArguments_;

    Label *labelOf(jsbytecode *pc) {
        return &labels_[script->pcToOffset(pc)];
    }

    // If a script has more |nslots| than this, then emit code to do an
    // early stack check.
    static const unsigned EARLY_STACK_CHECK_SLOT_COUNT = 128;
    bool needsEarlyStackCheck() const {
        return script->nslots() > EARLY_STACK_CHECK_SLOT_COUNT;
    }

  public:
    BaselineCompiler(JSContext *cx, TempAllocator &alloc, HandleScript script);
    bool init();

    MethodStatus compile();

  private:
    MethodStatus emitBody();

    bool emitPrologue();
    bool emitEpilogue();
#ifdef JSGC_GENERATIONAL
    bool emitOutOfLinePostBarrierSlot();
#endif
    bool emitIC(ICStub *stub, bool isForOp);
    bool emitOpIC(ICStub *stub) {
        return emitIC(stub, true);
    }
    bool emitNonOpIC(ICStub *stub) {
        return emitIC(stub, false);
    }

    bool emitStackCheck(bool earlyCheck=false);
    bool emitInterruptCheck();
    bool emitUseCountIncrement(bool allowOsr=true);
    bool emitArgumentTypeChecks();
    bool emitDebugPrologue();
    bool emitDebugTrap();
    bool emitSPSPush();
    void emitSPSPop();

    bool initScopeChain();

    void storeValue(const StackValue *source, const Address &dest,
                    const ValueOperand &scratch);

#define EMIT_OP(op) bool emit_##op();
    OPCODE_LIST(EMIT_OP)
#undef EMIT_OP

    // JSOP_NEG, JSOP_BITNOT
    bool emitUnaryArith();

    // JSOP_BITXOR, JSOP_LSH, JSOP_ADD etc.
    bool emitBinaryArith();

    // Handles JSOP_LT, JSOP_GT, and friends
    bool emitCompare();

    bool emitReturn();

    bool emitToBoolean();
    bool emitTest(bool branchIfTrue);
    bool emitAndOr(bool branchIfTrue);
    bool emitCall();

    bool emitInitPropGetterSetter();
    bool emitInitElemGetterSetter();

    bool emitFormalArgAccess(uint32_t arg, bool get);

    bool addPCMappingEntry(bool addIndexEntry);

    void getScopeCoordinateObject(Register reg);
    Address getScopeCoordinateAddressFromObject(Register objReg, Register reg);
    Address getScopeCoordinateAddress(Register reg);
};

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_BaselineCompiler_h */
