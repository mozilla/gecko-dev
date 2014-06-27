/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonTypes_h
#define jit_IonTypes_h

#include "mozilla/TypedEnum.h"

#include "jstypes.h"

#include "js/Value.h"

namespace js {
namespace jit {

typedef uint32_t RecoverOffset;
typedef uint32_t SnapshotOffset;
typedef uint32_t BailoutId;

// The maximum size of any buffer associated with an assembler or code object.
// This is chosen to not overflow a signed integer, leaving room for an extra
// bit on offsets.
static const uint32_t MAX_BUFFER_SIZE = (1 << 30) - 1;

// Maximum number of scripted arg slots.
static const uint32_t SNAPSHOT_MAX_NARGS = 127;

static const SnapshotOffset INVALID_RECOVER_OFFSET = uint32_t(-1);
static const SnapshotOffset INVALID_SNAPSHOT_OFFSET = uint32_t(-1);

// Different kinds of bailouts. When extending this enum, make sure to check
// the bits reserved for bailout kinds in Bailouts.h
enum BailoutKind
{
    // Normal bailouts, that don't need to be handled specially when restarting
    // in baseline.

    // An inevitable bailout (MBail instruction or type barrier that always bails)
    Bailout_Inevitable,

    // Bailing out during a VM call. Many possible causes that are hard
    // to distinguish statically at snapshot construction time.
    // We just lump them together.
    Bailout_DuringVMCall,

    // Call to a non-JSFunction (problem for |apply|)
    Bailout_NonJSFunctionCallee,

    // Dynamic scope chain lookup produced |undefined|
    Bailout_DynamicNameNotFound,

    // Input string contains 'arguments' or 'eval'
    Bailout_StringArgumentsEval,

    // Bailout on overflow, but don't immediately invalidate.
    // Used for abs, sub and LoadTypedArrayElement (when loading a uint32 that
    // doesn't fit in an int32).
    Bailout_Overflow,

    // floor, ceiling and round bail if input is NaN, if output would be -0 or
    // doesn't fit in int32 range
    Bailout_Round,

    // Non-primitive value used as input for ToDouble, ToInt32, ToString, etc.
    // For ToInt32, can also mean that input can't be converted without precision
    // loss (e.g. 5.5).
    Bailout_NonPrimitiveInput,

    // For ToInt32, would lose precision when converting (e.g. 5.5).
    Bailout_PrecisionLoss,

    // We tripped a type barrier (object was not in the expected TypeSet)
    Bailout_TypeBarrierO,
    // We tripped a type barrier (value was not in the expected TypeSet)
    Bailout_TypeBarrierV,
    // We tripped a type monitor (wrote an unexpected type in a property)
    Bailout_MonitorTypes,

    // We hit a hole in an array.
    Bailout_Hole,

    // Array access with negative index
    Bailout_NegativeIndex,

    // Pretty specific case:
    //  - need a type barrier on a property write
    //  - all but one of the observed types have property types that reflect the value
    //  - we need to guard that we're not given an object of that one other type
    // also used for the unused GuardClass instruction
    Bailout_ObjectIdentityOrTypeGuard,

    // Unbox expects a given type, bails out if it doesn't get it.
    Bailout_NonInt32Input,
    Bailout_NonNumericInput, // unboxing a double works with int32 too
    Bailout_NonBooleanInput,
    Bailout_NonObjectInput,
    Bailout_NonStringInput,
    Bailout_NonSymbolInput,

    // PJS bailout when writing to a non-thread local object.
    Bailout_GuardThreadExclusive,

    // PJS bailout when encountering MIR unsafe for parallel execution.
    Bailout_ParallelUnsafe,

    // For the initial snapshot when entering a function.
    Bailout_InitialState,

    // END Normal bailouts


    // Bailouts caused by invalid assumptions based on Baseline code.
    // Causes immediate invalidation.

    // Like Bailout_Overflow, but causes immediate invalidation.
    Bailout_OverflowInvalidate,

    // Like NonStringInput, but should cause immediate invalidation.
    // Used for jsop_iternext.
    Bailout_NonStringInputInvalidate,

    // Used for integer division, multiplication and modulo.
    // If there's a remainder, bails to return a double.
    // Can also signal overflow or result of -0.
    // Can also signal division by 0 (returns inf, a double).
    Bailout_DoubleOutput,

    // END Invalid assumptions bailouts


    // A bailout at the very start of a function indicates that there may be
    // a type mismatch in the arguments that necessitates a reflow.
    Bailout_ArgumentCheck,

    // A bailout triggered by a bounds-check failure.
    Bailout_BoundsCheck,
    // A bailout triggered by a neutered typed object.
    Bailout_Neutered,

    // A shape guard based on TI information failed.
    // (We saw an object whose shape does not match that / any of those observed
    // by the baseline IC.)
    Bailout_ShapeGuard,

    // A bailout to baseline from Ion on exception to handle Debugger hooks.
    Bailout_IonExceptionDebugMode,
};

inline const char *
BailoutKindString(BailoutKind kind)
{
    switch (kind) {
      // Normal bailouts.
      case Bailout_Inevitable:
        return "Bailout_Inevitable";
      case Bailout_DuringVMCall:
        return "Bailout_DuringVMCall";
      case Bailout_NonJSFunctionCallee:
        return "Bailout_NonJSFunctionCallee";
      case Bailout_DynamicNameNotFound:
        return "Bailout_DynamicNameNotFound";
      case Bailout_StringArgumentsEval:
        return "Bailout_StringArgumentsEval";
      case Bailout_Overflow:
        return "Bailout_Overflow";
      case Bailout_Round:
        return "Bailout_Round";
      case Bailout_NonPrimitiveInput:
        return "Bailout_NonPrimitiveInput";
      case Bailout_PrecisionLoss:
        return "Bailout_PrecisionLoss";
      case Bailout_TypeBarrierO:
        return "Bailout_TypeBarrierO";
      case Bailout_TypeBarrierV:
        return "Bailout_TypeBarrierV";
      case Bailout_MonitorTypes:
        return "Bailout_MonitorTypes";
      case Bailout_Hole:
        return "Bailout_Hole";
      case Bailout_NegativeIndex:
        return "Bailout_NegativeIndex";
      case Bailout_ObjectIdentityOrTypeGuard:
        return "Bailout_ObjectIdentityOrTypeGuard";
      case Bailout_NonInt32Input:
        return "Bailout_NonInt32Input";
      case Bailout_NonNumericInput:
        return "Bailout_NonNumericInput";
      case Bailout_NonBooleanInput:
        return "Bailout_NonBooleanInput";
      case Bailout_NonObjectInput:
        return "Bailout_NonObjectInput";
      case Bailout_NonStringInput:
        return "Bailout_NonStringInput";
      case Bailout_NonSymbolInput:
        return "Bailout_NonSymbolInput";
      case Bailout_GuardThreadExclusive:
        return "Bailout_GuardThreadExclusive";
      case Bailout_InitialState:
        return "Bailout_InitialState";

      // Bailouts caused by invalid assumptions.
      case Bailout_OverflowInvalidate:
        return "Bailout_OverflowInvalidate";
      case Bailout_NonStringInputInvalidate:
        return "Bailout_NonStringInputInvalidate";
      case Bailout_DoubleOutput:
        return "Bailout_DoubleOutput";

      // Other bailouts.
      case Bailout_ArgumentCheck:
        return "Bailout_ArgumentCheck";
      case Bailout_BoundsCheck:
        return "Bailout_BoundsCheck";
      case Bailout_Neutered:
        return "Bailout_Neutered";
      case Bailout_ShapeGuard:
        return "Bailout_ShapeGuard";
      case Bailout_IonExceptionDebugMode:
        return "Bailout_IonExceptionDebugMode";
      default:
        MOZ_ASSUME_UNREACHABLE("Invalid BailoutKind");
    }
}

static const uint32_t ELEMENT_TYPE_BITS = 5;
static const uint32_t ELEMENT_TYPE_SHIFT = 0;
static const uint32_t ELEMENT_TYPE_MASK = (1 << ELEMENT_TYPE_BITS) - 1;
static const uint32_t VECTOR_SCALE_BITS = 2;
static const uint32_t VECTOR_SCALE_SHIFT = ELEMENT_TYPE_BITS + ELEMENT_TYPE_SHIFT;
static const uint32_t VECTOR_SCALE_MASK = (1 << VECTOR_SCALE_BITS) - 1;

// The ordering of this enumeration is important: Anything < Value is a
// specialized type. Furthermore, anything < String has trivial conversion to
// a number.
enum MIRType
{
    MIRType_Undefined,
    MIRType_Null,
    MIRType_Boolean,
    MIRType_Int32,
    MIRType_Double,
    MIRType_Float32,
    MIRType_String,
    MIRType_Symbol,
    MIRType_Object,
    MIRType_MagicOptimizedArguments, // JS_OPTIMIZED_ARGUMENTS magic value.
    MIRType_MagicOptimizedOut,       // JS_OPTIMIZED_OUT magic value.
    MIRType_MagicHole,               // JS_ELEMENTS_HOLE magic value.
    MIRType_MagicIsConstructing,     // JS_IS_CONSTRUCTING magic value.
    MIRType_Value,
    MIRType_None,                    // Invalid, used as a placeholder.
    MIRType_Slots,                   // A slots vector
    MIRType_Elements,                // An elements vector
    MIRType_Pointer,                 // An opaque pointer that receives no special treatment
    MIRType_Shape,                   // A Shape pointer.
    MIRType_ForkJoinContext,         // js::ForkJoinContext*
    MIRType_Last = MIRType_ForkJoinContext,
    MIRType_Float32x4 = MIRType_Float32 | (2 << VECTOR_SCALE_SHIFT),
    MIRType_Int32x4   = MIRType_Int32   | (2 << VECTOR_SCALE_SHIFT),
    MIRType_Doublex2  = MIRType_Double  | (1 << VECTOR_SCALE_SHIFT)
};

static inline MIRType
ElementType(MIRType type)
{
    JS_STATIC_ASSERT(MIRType_Last <= ELEMENT_TYPE_MASK);
    return static_cast<MIRType>((type >> ELEMENT_TYPE_SHIFT) & ELEMENT_TYPE_MASK);
}

static inline uint32_t
VectorSize(MIRType type)
{
    return 1 << ((type >> VECTOR_SCALE_SHIFT) & VECTOR_SCALE_MASK);
}

static inline MIRType
MIRTypeFromValueType(JSValueType type)
{
    // This function does not deal with magic types. Magic constants should be
    // filtered out in MIRTypeFromValue.
    switch (type) {
      case JSVAL_TYPE_DOUBLE:
        return MIRType_Double;
      case JSVAL_TYPE_INT32:
        return MIRType_Int32;
      case JSVAL_TYPE_UNDEFINED:
        return MIRType_Undefined;
      case JSVAL_TYPE_STRING:
        return MIRType_String;
      case JSVAL_TYPE_SYMBOL:
        return MIRType_Symbol;
      case JSVAL_TYPE_BOOLEAN:
        return MIRType_Boolean;
      case JSVAL_TYPE_NULL:
        return MIRType_Null;
      case JSVAL_TYPE_OBJECT:
        return MIRType_Object;
      case JSVAL_TYPE_UNKNOWN:
        return MIRType_Value;
      default:
        MOZ_ASSUME_UNREACHABLE("unexpected jsval type");
    }
}

static inline JSValueType
ValueTypeFromMIRType(MIRType type)
{
  switch (type) {
    case MIRType_Undefined:
      return JSVAL_TYPE_UNDEFINED;
    case MIRType_Null:
      return JSVAL_TYPE_NULL;
    case MIRType_Boolean:
      return JSVAL_TYPE_BOOLEAN;
    case MIRType_Int32:
      return JSVAL_TYPE_INT32;
    case MIRType_Float32: // Fall through, there's no JSVAL for Float32
    case MIRType_Double:
      return JSVAL_TYPE_DOUBLE;
    case MIRType_String:
      return JSVAL_TYPE_STRING;
    case MIRType_Symbol:
      return JSVAL_TYPE_SYMBOL;
    case MIRType_MagicOptimizedArguments:
    case MIRType_MagicOptimizedOut:
    case MIRType_MagicHole:
    case MIRType_MagicIsConstructing:
      return JSVAL_TYPE_MAGIC;
    default:
      JS_ASSERT(type == MIRType_Object);
      return JSVAL_TYPE_OBJECT;
  }
}

static inline JSValueTag
MIRTypeToTag(MIRType type)
{
    return JSVAL_TYPE_TO_TAG(ValueTypeFromMIRType(type));
}

static inline const char *
StringFromMIRType(MIRType type)
{
  switch (type) {
    case MIRType_Undefined:
      return "Undefined";
    case MIRType_Null:
      return "Null";
    case MIRType_Boolean:
      return "Bool";
    case MIRType_Int32:
      return "Int32";
    case MIRType_Double:
      return "Double";
    case MIRType_Float32:
      return "Float32";
    case MIRType_String:
      return "String";
    case MIRType_Symbol:
      return "Symbol";
    case MIRType_Object:
      return "Object";
    case MIRType_MagicOptimizedArguments:
      return "MagicOptimizedArguments";
    case MIRType_MagicOptimizedOut:
      return "MagicOptimizedOut";
    case MIRType_MagicHole:
      return "MagicHole";
    case MIRType_MagicIsConstructing:
      return "MagicIsConstructing";
    case MIRType_Value:
      return "Value";
    case MIRType_None:
      return "None";
    case MIRType_Slots:
      return "Slots";
    case MIRType_Elements:
      return "Elements";
    case MIRType_Pointer:
      return "Pointer";
    case MIRType_ForkJoinContext:
      return "ForkJoinContext";
    default:
      MOZ_ASSUME_UNREACHABLE("Unknown MIRType.");
  }
}

static inline bool
IsNumberType(MIRType type)
{
    return type == MIRType_Int32 || type == MIRType_Double || type == MIRType_Float32;
}

static inline bool
IsFloatType(MIRType type)
{
    return type == MIRType_Int32 || type == MIRType_Float32;
}

static inline bool
IsFloatingPointType(MIRType type)
{
    return type == MIRType_Double || type == MIRType_Float32;
}

static inline bool
IsNullOrUndefined(MIRType type)
{
    return type == MIRType_Null || type == MIRType_Undefined;
}

#ifdef DEBUG
// Track the pipeline of opcodes which has produced a snapshot.
#define TRACK_SNAPSHOTS 1

// Make sure registers are not modified between an instruction and
// its OsiPoint.
#  if defined(JS_ION)
#    define CHECK_OSIPOINT_REGISTERS 1
#  endif
#endif

enum {
    ArgType_General = 0x1,
    ArgType_Double  = 0x2,
    ArgType_Float32 = 0x3,

    RetType_Shift   = 0x0,
    ArgType_Shift   = 0x2,
    ArgType_Mask    = 0x3
};

enum ABIFunctionType
{
    // VM functions that take 0-9 non-double arguments
    // and return a non-double value.
    Args_General0 = ArgType_General << RetType_Shift,
    Args_General1 = Args_General0 | (ArgType_General << (ArgType_Shift * 1)),
    Args_General2 = Args_General1 | (ArgType_General << (ArgType_Shift * 2)),
    Args_General3 = Args_General2 | (ArgType_General << (ArgType_Shift * 3)),
    Args_General4 = Args_General3 | (ArgType_General << (ArgType_Shift * 4)),
    Args_General5 = Args_General4 | (ArgType_General << (ArgType_Shift * 5)),
    Args_General6 = Args_General5 | (ArgType_General << (ArgType_Shift * 6)),
    Args_General7 = Args_General6 | (ArgType_General << (ArgType_Shift * 7)),
    Args_General8 = Args_General7 | (ArgType_General << (ArgType_Shift * 8)),

    // double f()
    Args_Double_None = ArgType_Double << RetType_Shift,

    // int f(double)
    Args_Int_Double = Args_General0 | (ArgType_Double << ArgType_Shift),

    // float f(float)
    Args_Float32_Float32 = (ArgType_Float32 << RetType_Shift) | (ArgType_Float32 << ArgType_Shift),

    // double f(double)
    Args_Double_Double = Args_Double_None | (ArgType_Double << ArgType_Shift),

    // double f(int)
    Args_Double_Int = Args_Double_None | (ArgType_General << ArgType_Shift),

    // double f(double, int)
    Args_Double_DoubleInt = Args_Double_None |
        (ArgType_General << (ArgType_Shift * 1)) |
        (ArgType_Double << (ArgType_Shift * 2)),

    // double f(double, double)
    Args_Double_DoubleDouble = Args_Double_Double | (ArgType_Double << (ArgType_Shift * 2)),

    // double f(int, double)
    Args_Double_IntDouble = Args_Double_None |
        (ArgType_Double << (ArgType_Shift * 1)) |
        (ArgType_General << (ArgType_Shift * 2)),

    // int f(int, double)
    Args_Int_IntDouble = Args_General0 |
        (ArgType_Double << (ArgType_Shift * 1)) |
        (ArgType_General << (ArgType_Shift * 2))
};

MOZ_BEGIN_ENUM_CLASS(BarrierKind, uint32_t)
    // No barrier is needed.
    NoBarrier,

    // The barrier only has to check the value's type tag is in the TypeSet.
    // Specific object types don't have to be checked.
    TypeTagOnly,

    // Check if the value is in the TypeSet, including the object type if it's
    // an object.
    TypeSet
MOZ_END_ENUM_CLASS(BarrierKind)

} // namespace jit
} // namespace js

#endif /* jit_IonTypes_h */
