/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/RangeAnalysis.h"

#include "mozilla/MathAlgorithms.h"

#include "jsanalyze.h"

#include "jit/Ion.h"
#include "jit/IonAnalysis.h"
#include "jit/IonSpewer.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "vm/NumericConversions.h"

#include "jsopcodeinlines.h"

using namespace js;
using namespace js::jit;

using mozilla::Abs;
using mozilla::CountLeadingZeroes32;
using mozilla::DoubleEqualsInt32;
using mozilla::ExponentComponent;
using mozilla::FloorLog2;
using mozilla::IsInfinite;
using mozilla::IsFinite;
using mozilla::IsNaN;
using mozilla::IsNegative;
using mozilla::NegativeInfinity;
using mozilla::PositiveInfinity;
using mozilla::Swap;
using JS::GenericNaN;

// This algorithm is based on the paper "Eliminating Range Checks Using
// Static Single Assignment Form" by Gough and Klaren.
//
// We associate a range object with each SSA name, and the ranges are consulted
// in order to determine whether overflow is possible for arithmetic
// computations.
//
// An important source of range information that requires care to take
// advantage of is conditional control flow. Consider the code below:
//
// if (x < 0) {
//   y = x + 2000000000;
// } else {
//   if (x < 1000000000) {
//     y = x * 2;
//   } else {
//     y = x - 3000000000;
//   }
// }
//
// The arithmetic operations in this code cannot overflow, but it is not
// sufficient to simply associate each name with a range, since the information
// differs between basic blocks. The traditional dataflow approach would be
// associate ranges with (name, basic block) pairs. This solution is not
// satisfying, since we lose the benefit of SSA form: in SSA form, each
// definition has a unique name, so there is no need to track information about
// the control flow of the program.
//
// The approach used here is to add a new form of pseudo operation called a
// beta node, which associates range information with a value. These beta
// instructions take one argument and additionally have an auxiliary constant
// range associated with them. Operationally, beta nodes are just copies, but
// the invariant expressed by beta node copies is that the output will fall
// inside the range given by the beta node.  Gough and Klaeren refer to SSA
// extended with these beta nodes as XSA form. The following shows the example
// code transformed into XSA form:
//
// if (x < 0) {
//   x1 = Beta(x, [INT_MIN, -1]);
//   y1 = x1 + 2000000000;
// } else {
//   x2 = Beta(x, [0, INT_MAX]);
//   if (x2 < 1000000000) {
//     x3 = Beta(x2, [INT_MIN, 999999999]);
//     y2 = x3*2;
//   } else {
//     x4 = Beta(x2, [1000000000, INT_MAX]);
//     y3 = x4 - 3000000000;
//   }
//   y4 = Phi(y2, y3);
// }
// y = Phi(y1, y4);
//
// We insert beta nodes for the purposes of range analysis (they might also be
// usefully used for other forms of bounds check elimination) and remove them
// after range analysis is performed. The remaining compiler phases do not ever
// encounter beta nodes.

static bool
IsDominatedUse(MBasicBlock *block, MUse *use)
{
    MNode *n = use->consumer();
    bool isPhi = n->isDefinition() && n->toDefinition()->isPhi();

    if (isPhi)
        return block->dominates(n->block()->getPredecessor(use->index()));

    return block->dominates(n->block());
}

static inline void
SpewRange(MDefinition *def)
{
#ifdef DEBUG
    if (IonSpewEnabled(IonSpew_Range) && def->type() != MIRType_None && def->range()) {
        IonSpewHeader(IonSpew_Range);
        def->printName(IonSpewFile);
        fprintf(IonSpewFile, " has range ");
        def->range()->dump(IonSpewFile);
    }
#endif
}

TempAllocator &
RangeAnalysis::alloc() const
{
    return graph_.alloc();
}

void
RangeAnalysis::replaceDominatedUsesWith(MDefinition *orig, MDefinition *dom,
                                            MBasicBlock *block)
{
    for (MUseIterator i(orig->usesBegin()); i != orig->usesEnd(); ) {
        if (i->consumer() != dom && IsDominatedUse(block, *i))
            i = i->consumer()->replaceOperand(i, dom);
        else
            i++;
    }
}

bool
RangeAnalysis::addBetaNodes()
{
    IonSpew(IonSpew_Range, "Adding beta nodes");

    for (PostorderIterator i(graph_.poBegin()); i != graph_.poEnd(); i++) {
        MBasicBlock *block = *i;
        IonSpew(IonSpew_Range, "Looking at block %d", block->id());

        BranchDirection branch_dir;
        MTest *test = block->immediateDominatorBranch(&branch_dir);

        if (!test || !test->getOperand(0)->isCompare())
            continue;

        MCompare *compare = test->getOperand(0)->toCompare();

        // TODO: support unsigned comparisons
        if (compare->compareType() == MCompare::Compare_UInt32)
            continue;

        MDefinition *left = compare->getOperand(0);
        MDefinition *right = compare->getOperand(1);
        double bound;
        double conservativeLower = NegativeInfinity();
        double conservativeUpper = PositiveInfinity();
        MDefinition *val = nullptr;

        JSOp jsop = compare->jsop();

        if (branch_dir == FALSE_BRANCH) {
            jsop = NegateCompareOp(jsop);
            conservativeLower = GenericNaN();
            conservativeUpper = GenericNaN();
        }

        if (left->isConstant() && left->toConstant()->value().isNumber()) {
            bound = left->toConstant()->value().toNumber();
            val = right;
            jsop = ReverseCompareOp(jsop);
        } else if (right->isConstant() && right->toConstant()->value().isNumber()) {
            bound = right->toConstant()->value().toNumber();
            val = left;
        } else if (left->type() == MIRType_Int32 && right->type() == MIRType_Int32) {
            MDefinition *smaller = nullptr;
            MDefinition *greater = nullptr;
            if (jsop == JSOP_LT) {
                smaller = left;
                greater = right;
            } else if (jsop == JSOP_GT) {
                smaller = right;
                greater = left;
            }
            if (smaller && greater) {
                MBeta *beta;
                beta = MBeta::New(alloc(), smaller,
                                  Range::NewInt32Range(alloc(), JSVAL_INT_MIN, JSVAL_INT_MAX-1));
                block->insertBefore(*block->begin(), beta);
                replaceDominatedUsesWith(smaller, beta, block);
                IonSpew(IonSpew_Range, "Adding beta node for smaller %d", smaller->id());
                beta = MBeta::New(alloc(), greater,
                                  Range::NewInt32Range(alloc(), JSVAL_INT_MIN+1, JSVAL_INT_MAX));
                block->insertBefore(*block->begin(), beta);
                replaceDominatedUsesWith(greater, beta, block);
                IonSpew(IonSpew_Range, "Adding beta node for greater %d", greater->id());
            }
            continue;
        } else {
            continue;
        }

        // At this point, one of the operands if the compare is a constant, and
        // val is the other operand.
        JS_ASSERT(val);

        Range comp;
        switch (jsop) {
          case JSOP_LE:
            comp.setDouble(conservativeLower, bound);
            break;
          case JSOP_LT:
            // For integers, if x < c, the upper bound of x is c-1.
            if (val->type() == MIRType_Int32) {
                int32_t intbound;
                if (DoubleEqualsInt32(bound, &intbound) && SafeSub(intbound, 1, &intbound))
                    bound = intbound;
            }
            comp.setDouble(conservativeLower, bound);
            break;
          case JSOP_GE:
            comp.setDouble(bound, conservativeUpper);
            break;
          case JSOP_GT:
            // For integers, if x > c, the lower bound of x is c+1.
            if (val->type() == MIRType_Int32) {
                int32_t intbound;
                if (DoubleEqualsInt32(bound, &intbound) && SafeAdd(intbound, 1, &intbound))
                    bound = intbound;
            }
            comp.setDouble(bound, conservativeUpper);
            break;
          case JSOP_EQ:
            comp.setDouble(bound, bound);
            break;
          default:
            continue; // well, for neq we could have
                      // [-\inf, bound-1] U [bound+1, \inf] but we only use contiguous ranges.
        }

        if (IonSpewEnabled(IonSpew_Range)) {
            IonSpewHeader(IonSpew_Range);
            fprintf(IonSpewFile, "Adding beta node for %d with range ", val->id());
            comp.dump(IonSpewFile);
        }

        MBeta *beta = MBeta::New(alloc(), val, new(alloc()) Range(comp));
        block->insertBefore(*block->begin(), beta);
        replaceDominatedUsesWith(val, beta, block);
    }

    return true;
}

bool
RangeAnalysis::removeBetaNodes()
{
    IonSpew(IonSpew_Range, "Removing beta nodes");

    for (PostorderIterator i(graph_.poBegin()); i != graph_.poEnd(); i++) {
        MBasicBlock *block = *i;
        for (MDefinitionIterator iter(*i); iter; ) {
            MDefinition *def = *iter;
            if (def->isBeta()) {
                MDefinition *op = def->getOperand(0);
                IonSpew(IonSpew_Range, "Removing beta node %d for %d",
                        def->id(), op->id());
                def->replaceAllUsesWith(op);
                iter = block->discardDefAt(iter);
            } else {
                // We only place Beta nodes at the beginning of basic
                // blocks, so if we see something else, we can move on
                // to the next block.
                break;
            }
        }
    }
    return true;
}

void
SymbolicBound::print(Sprinter &sp) const
{
    if (loop)
        sp.printf("[loop] ");
    sum.print(sp);
}

void
SymbolicBound::dump() const
{
    Sprinter sp(GetIonContext()->cx);
    sp.init();
    print(sp);
    fprintf(stderr, "%s\n", sp.string());
}

// Test whether the given range's exponent tells us anything that its lower
// and upper bound values don't.
static bool
IsExponentInteresting(const Range *r)
{
   // If it lacks either a lower or upper bound, the exponent is interesting.
   if (!r->hasInt32Bounds())
       return true;

   // Otherwise if there's no fractional part, the lower and upper bounds,
   // which are integers, are perfectly precise.
   if (!r->canHaveFractionalPart())
       return false;

   // Otherwise, if the bounds are conservatively rounded across a power-of-two
   // boundary, the exponent may imply a tighter range.
   return FloorLog2(Max(Abs(r->lower()), Abs(r->upper()))) > r->exponent();
}

void
Range::print(Sprinter &sp) const
{
    assertInvariants();

    // Floating-point or Integer subset.
    if (canHaveFractionalPart_)
        sp.printf("F");
    else
        sp.printf("I");

    sp.printf("[");

    if (!hasInt32LowerBound_)
        sp.printf("?");
    else
        sp.printf("%d", lower_);
    if (symbolicLower_) {
        sp.printf(" {");
        symbolicLower_->print(sp);
        sp.printf("}");
    }

    sp.printf(", ");

    if (!hasInt32UpperBound_)
        sp.printf("?");
    else
        sp.printf("%d", upper_);
    if (symbolicUpper_) {
        sp.printf(" {");
        symbolicUpper_->print(sp);
        sp.printf("}");
    }

    sp.printf("]");
    if (IsExponentInteresting(this)) {
        if (max_exponent_ == IncludesInfinityAndNaN)
            sp.printf(" (U inf U NaN)", max_exponent_);
        else if (max_exponent_ == IncludesInfinity)
            sp.printf(" (U inf)");
        else
            sp.printf(" (< pow(2, %d+1))", max_exponent_);
    }
}

void
Range::dump(FILE *fp) const
{
    Sprinter sp(GetIonContext()->cx);
    sp.init();
    print(sp);
    fprintf(fp, "%s\n", sp.string());
}

void
Range::dump() const
{
    dump(stderr);
}

Range *
Range::intersect(TempAllocator &alloc, const Range *lhs, const Range *rhs, bool *emptyRange)
{
    *emptyRange = false;

    if (!lhs && !rhs)
        return nullptr;

    if (!lhs)
        return new(alloc) Range(*rhs);
    if (!rhs)
        return new(alloc) Range(*lhs);

    int32_t newLower = Max(lhs->lower_, rhs->lower_);
    int32_t newUpper = Min(lhs->upper_, rhs->upper_);

    // :TODO: This information could be used better. If upper < lower, then we
    // have conflicting constraints. Consider:
    //
    // if (x < 0) {
    //   if (x > 0) {
    //     [Some code.]
    //   }
    // }
    //
    // In this case, the block is dead. Right now, we just disregard this fact
    // and make the range unbounded, rather than empty.
    //
    // Instead, we should use it to eliminate the dead block.
    // (Bug 765127)
    if (newUpper < newLower) {
        // If both ranges can be NaN, the result can still be NaN.
        if (!lhs->canBeNaN() || !rhs->canBeNaN())
            *emptyRange = true;
        return nullptr;
    }

    bool newHasInt32LowerBound = lhs->hasInt32LowerBound_ || rhs->hasInt32LowerBound_;
    bool newHasInt32UpperBound = lhs->hasInt32UpperBound_ || rhs->hasInt32UpperBound_;
    bool newFractional = lhs->canHaveFractionalPart_ && rhs->canHaveFractionalPart_;
    uint16_t newExponent = Min(lhs->max_exponent_, rhs->max_exponent_);

    // NaN is a special value which is neither greater than infinity or less than
    // negative infinity. When we intersect two ranges like [?, 0] and [0, ?], we
    // can end up thinking we have both a lower and upper bound, even though NaN
    // is still possible. In this case, just be conservative, since any case where
    // we can have NaN is not especially interesting.
    if (newHasInt32LowerBound && newHasInt32UpperBound && newExponent == IncludesInfinityAndNaN)
        return nullptr;

    // If one of the ranges has a fractional part and the other doesn't, it's
    // possible that we will have computed a newExponent that's more precise
    // than our newLower and newUpper. This is unusual, so we handle it here
    // instead of in optimize().
    //
    // For example, consider the range F[0,1.5]. Range analysis represents the
    // lower and upper bound as integers, so we'd actually have
    // F[0,2] (< pow(2, 0+1)). In this case, the exponent gives us a slightly
    // more precise upper bound than the integer upper bound.
    //
    // When intersecting such a range with an integer range, the fractional part
    // of the range is dropped. The max exponent of 0 remains valid, so the
    // upper bound needs to be adjusted to 1.
    //
    // When intersecting F[0,2] (< pow(2, 0+1)) with a range like F[2,4],
    // the naive intersection is I[2,2], but since the max exponent tells us
    // that the value is always less than 2, the intersection is actually empty.
    if (lhs->canHaveFractionalPart_ != rhs->canHaveFractionalPart_ ||
        (lhs->canHaveFractionalPart_ &&
         newHasInt32LowerBound && newHasInt32UpperBound &&
         newLower == newUpper))
    {
        refineInt32BoundsByExponent(newExponent, &newLower, &newUpper);

        // If we're intersecting two ranges that don't overlap, this could also
        // push the bounds past each other, since the actual intersection is
        // the empty set.
        if (newLower > newUpper) {
            *emptyRange = true;
            return nullptr;
        }
    }

    return new(alloc) Range(newLower, newHasInt32LowerBound, newUpper, newHasInt32UpperBound,
                            newFractional, newExponent);
}

void
Range::unionWith(const Range *other)
{
    int32_t newLower = Min(lower_, other->lower_);
    int32_t newUpper = Max(upper_, other->upper_);

    bool newHasInt32LowerBound = hasInt32LowerBound_ && other->hasInt32LowerBound_;
    bool newHasInt32UpperBound = hasInt32UpperBound_ && other->hasInt32UpperBound_;
    bool newFractional = canHaveFractionalPart_ || other->canHaveFractionalPart_;
    uint16_t newExponent = Max(max_exponent_, other->max_exponent_);

    rawInitialize(newLower, newHasInt32LowerBound, newUpper, newHasInt32UpperBound,
                  newFractional, newExponent);
}

Range::Range(const MDefinition *def)
  : symbolicLower_(nullptr),
    symbolicUpper_(nullptr)
{
    if (const Range *other = def->range()) {
        // The instruction has range information; use it.
        *this = *other;

        // Simulate the effect of converting the value to its type.
        switch (def->type()) {
          case MIRType_Int32:
            wrapAroundToInt32();
            break;
          case MIRType_Boolean:
            wrapAroundToBoolean();
            break;
          case MIRType_None:
            MOZ_ASSUME_UNREACHABLE("Asking for the range of an instruction with no value");
          default:
            break;
        }
    } else {
        // Otherwise just use type information. We can trust the type here
        // because we don't care what value the instruction actually produces,
        // but what value we might get after we get past the bailouts.
        switch (def->type()) {
          case MIRType_Int32:
            setInt32(JSVAL_INT_MIN, JSVAL_INT_MAX);
            break;
          case MIRType_Boolean:
            setInt32(0, 1);
            break;
          case MIRType_None:
            MOZ_ASSUME_UNREACHABLE("Asking for the range of an instruction with no value");
          default:
            setUnknown();
            break;
        }
    }

    // As a special case, MUrsh is permitted to claim a result type of
    // MIRType_Int32 while actually returning values in [0,UINT32_MAX] without
    // bailouts. If range analysis hasn't ruled out values in
    // (INT32_MAX,UINT32_MAX], set the range to be conservatively correct for
    // use as either a uint32 or an int32.
    if (!hasInt32UpperBound() && def->isUrsh() && def->toUrsh()->bailoutsDisabled())
        lower_ = INT32_MIN;

    assertInvariants();
}

static uint16_t
ExponentImpliedByDouble(double d)
{
    // Handle the special values.
    if (IsNaN(d))
        return Range::IncludesInfinityAndNaN;
    if (IsInfinite(d))
        return Range::IncludesInfinity;

    // Otherwise take the exponent part and clamp it at zero, since the Range
    // class doesn't track fractional ranges.
    return uint16_t(Max(int_fast16_t(0), ExponentComponent(d)));
}

void
Range::setDouble(double l, double h)
{
    // Infer lower_, upper_, hasInt32LowerBound_, and hasInt32UpperBound_.
    if (l >= INT32_MIN && l <= INT32_MAX) {
        lower_ = int32_t(floor(l));
        hasInt32LowerBound_ = true;
    } else {
        lower_ = INT32_MIN;
        hasInt32LowerBound_ = false;
    }
    if (h >= INT32_MIN && h <= INT32_MAX) {
        upper_ = int32_t(ceil(h));
        hasInt32UpperBound_ = true;
    } else {
        upper_ = INT32_MAX;
        hasInt32UpperBound_ = false;
    }

    // Infer max_exponent_.
    uint16_t lExp = ExponentImpliedByDouble(l);
    uint16_t hExp = ExponentImpliedByDouble(h);
    max_exponent_ = Max(lExp, hExp);

    // Infer the canHaveFractionalPart_ field. We can have a fractional part
    // if the range crosses through the neighborhood of zero. We won't have a
    // fractional value if the value is always beyond the point at which
    // double precision can't represent fractional values.
    uint16_t minExp = Min(lExp, hExp);
    bool includesNegative = IsNaN(l) || l < 0;
    bool includesPositive = IsNaN(h) || h > 0;
    bool crossesZero = includesNegative && includesPositive;
    canHaveFractionalPart_ = crossesZero || minExp < MaxTruncatableExponent;

    optimize();
}

static inline bool
MissingAnyInt32Bounds(const Range *lhs, const Range *rhs)
{
    return !lhs->hasInt32LowerBound() || !lhs->hasInt32UpperBound() ||
           !rhs->hasInt32LowerBound() || !rhs->hasInt32UpperBound();
}

Range *
Range::add(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    int64_t l = (int64_t) lhs->lower_ + (int64_t) rhs->lower_;
    if (!lhs->hasInt32LowerBound() || !rhs->hasInt32LowerBound())
        l = NoInt32LowerBound;

    int64_t h = (int64_t) lhs->upper_ + (int64_t) rhs->upper_;
    if (!lhs->hasInt32UpperBound() || !rhs->hasInt32UpperBound())
        h = NoInt32UpperBound;

    // The exponent is at most one greater than the greater of the operands'
    // exponents, except for NaN and infinity cases.
    uint16_t e = Max(lhs->max_exponent_, rhs->max_exponent_);
    if (e <= Range::MaxFiniteExponent)
        ++e;

    // Infinity + -Infinity is NaN.
    if (lhs->canBeInfiniteOrNaN() && rhs->canBeInfiniteOrNaN())
        e = Range::IncludesInfinityAndNaN;

    return new(alloc) Range(l, h, lhs->canHaveFractionalPart() || rhs->canHaveFractionalPart(), e);
}

Range *
Range::sub(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    int64_t l = (int64_t) lhs->lower_ - (int64_t) rhs->upper_;
    if (!lhs->hasInt32LowerBound() || !rhs->hasInt32UpperBound())
        l = NoInt32LowerBound;

    int64_t h = (int64_t) lhs->upper_ - (int64_t) rhs->lower_;
    if (!lhs->hasInt32UpperBound() || !rhs->hasInt32LowerBound())
        h = NoInt32UpperBound;

    // The exponent is at most one greater than the greater of the operands'
    // exponents, except for NaN and infinity cases.
    uint16_t e = Max(lhs->max_exponent_, rhs->max_exponent_);
    if (e <= Range::MaxFiniteExponent)
        ++e;

    // Infinity - Infinity is NaN.
    if (lhs->canBeInfiniteOrNaN() && rhs->canBeInfiniteOrNaN())
        e = Range::IncludesInfinityAndNaN;

    return new(alloc) Range(l, h, lhs->canHaveFractionalPart() || rhs->canHaveFractionalPart(), e);
}

Range *
Range::and_(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    JS_ASSERT(lhs->isInt32());
    JS_ASSERT(rhs->isInt32());

    // If both numbers can be negative, result can be negative in the whole range
    if (lhs->lower() < 0 && rhs->lower() < 0)
        return Range::NewInt32Range(alloc, INT32_MIN, Max(lhs->upper(), rhs->upper()));

    // Only one of both numbers can be negative.
    // - result can't be negative
    // - Upper bound is minimum of both upper range,
    int32_t lower = 0;
    int32_t upper = Min(lhs->upper(), rhs->upper());

    // EXCEPT when upper bound of non negative number is max value,
    // because negative value can return the whole max value.
    // -1 & 5 = 5
    if (lhs->lower() < 0)
       upper = rhs->upper();
    if (rhs->lower() < 0)
        upper = lhs->upper();

    return Range::NewInt32Range(alloc, lower, upper);
}

Range *
Range::or_(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    JS_ASSERT(lhs->isInt32());
    JS_ASSERT(rhs->isInt32());
    // When one operand is always 0 or always -1, it's a special case where we
    // can compute a fully precise result. Handling these up front also
    // protects the code below from calling CountLeadingZeroes32 with a zero
    // operand or from shifting an int32_t by 32.
    if (lhs->lower() == lhs->upper()) {
        if (lhs->lower() == 0)
            return new(alloc) Range(*rhs);
        if (lhs->lower() == -1)
            return new(alloc) Range(*lhs);;
    }
    if (rhs->lower() == rhs->upper()) {
        if (rhs->lower() == 0)
            return new(alloc) Range(*lhs);
        if (rhs->lower() == -1)
            return new(alloc) Range(*rhs);;
    }

    // The code below uses CountLeadingZeroes32, which has undefined behavior
    // if its operand is 0. We rely on the code above to protect it.
    JS_ASSERT_IF(lhs->lower() >= 0, lhs->upper() != 0);
    JS_ASSERT_IF(rhs->lower() >= 0, rhs->upper() != 0);
    JS_ASSERT_IF(lhs->upper() < 0, lhs->lower() != -1);
    JS_ASSERT_IF(rhs->upper() < 0, rhs->lower() != -1);

    int32_t lower = INT32_MIN;
    int32_t upper = INT32_MAX;

    if (lhs->lower() >= 0 && rhs->lower() >= 0) {
        // Both operands are non-negative, so the result won't be less than either.
        lower = Max(lhs->lower(), rhs->lower());
        // The result will have leading zeros where both operands have leading zeros.
        // CountLeadingZeroes32 of a non-negative int32 will at least be 1 to account
        // for the bit of sign.
        upper = int32_t(UINT32_MAX >> Min(CountLeadingZeroes32(lhs->upper()),
                                          CountLeadingZeroes32(rhs->upper())));
    } else {
        // The result will have leading ones where either operand has leading ones.
        if (lhs->upper() < 0) {
            unsigned leadingOnes = CountLeadingZeroes32(~lhs->lower());
            lower = Max(lower, ~int32_t(UINT32_MAX >> leadingOnes));
            upper = -1;
        }
        if (rhs->upper() < 0) {
            unsigned leadingOnes = CountLeadingZeroes32(~rhs->lower());
            lower = Max(lower, ~int32_t(UINT32_MAX >> leadingOnes));
            upper = -1;
        }
    }

    return Range::NewInt32Range(alloc, lower, upper);
}

Range *
Range::xor_(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    JS_ASSERT(lhs->isInt32());
    JS_ASSERT(rhs->isInt32());
    int32_t lhsLower = lhs->lower();
    int32_t lhsUpper = lhs->upper();
    int32_t rhsLower = rhs->lower();
    int32_t rhsUpper = rhs->upper();
    bool invertAfter = false;

    // If either operand is negative, bitwise-negate it, and arrange to negate
    // the result; ~((~x)^y) == x^y. If both are negative the negations on the
    // result cancel each other out; effectively this is (~x)^(~y) == x^y.
    // These transformations reduce the number of cases we have to handle below.
    if (lhsUpper < 0) {
        lhsLower = ~lhsLower;
        lhsUpper = ~lhsUpper;
        Swap(lhsLower, lhsUpper);
        invertAfter = !invertAfter;
    }
    if (rhsUpper < 0) {
        rhsLower = ~rhsLower;
        rhsUpper = ~rhsUpper;
        Swap(rhsLower, rhsUpper);
        invertAfter = !invertAfter;
    }

    // Handle cases where lhs or rhs is always zero specially, because they're
    // easy cases where we can be perfectly precise, and because it protects the
    // CountLeadingZeroes32 calls below from seeing 0 operands, which would be
    // undefined behavior.
    int32_t lower = INT32_MIN;
    int32_t upper = INT32_MAX;
    if (lhsLower == 0 && lhsUpper == 0) {
        upper = rhsUpper;
        lower = rhsLower;
    } else if (rhsLower == 0 && rhsUpper == 0) {
        upper = lhsUpper;
        lower = lhsLower;
    } else if (lhsLower >= 0 && rhsLower >= 0) {
        // Both operands are non-negative. The result will be non-negative.
        lower = 0;
        // To compute the upper value, take each operand's upper value and
        // set all bits that don't correspond to leading zero bits in the
        // other to one. For each one, this gives an upper bound for the
        // result, so we can take the minimum between the two.
        unsigned lhsLeadingZeros = CountLeadingZeroes32(lhsUpper);
        unsigned rhsLeadingZeros = CountLeadingZeroes32(rhsUpper);
        upper = Min(rhsUpper | int32_t(UINT32_MAX >> lhsLeadingZeros),
                    lhsUpper | int32_t(UINT32_MAX >> rhsLeadingZeros));
    }

    // If we bitwise-negated one (but not both) of the operands above, apply the
    // bitwise-negate to the result, completing ~((~x)^y) == x^y.
    if (invertAfter) {
        lower = ~lower;
        upper = ~upper;
        Swap(lower, upper);
    }

    return Range::NewInt32Range(alloc, lower, upper);
}

Range *
Range::not_(TempAllocator &alloc, const Range *op)
{
    JS_ASSERT(op->isInt32());
    return Range::NewInt32Range(alloc, ~op->upper(), ~op->lower());
}

Range *
Range::mul(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    bool fractional = lhs->canHaveFractionalPart() || rhs->canHaveFractionalPart();

    uint16_t exponent;
    if (!lhs->canBeInfiniteOrNaN() && !rhs->canBeInfiniteOrNaN()) {
        // Two finite values.
        exponent = lhs->numBits() + rhs->numBits() - 1;
        if (exponent > Range::MaxFiniteExponent)
            exponent = Range::IncludesInfinity;
    } else if (!lhs->canBeNaN() &&
               !rhs->canBeNaN() &&
               !(lhs->canBeZero() && rhs->canBeInfiniteOrNaN()) &&
               !(rhs->canBeZero() && lhs->canBeInfiniteOrNaN()))
    {
        // Two values that multiplied together won't produce a NaN.
        exponent = Range::IncludesInfinity;
    } else {
        // Could be anything.
        exponent = Range::IncludesInfinityAndNaN;
    }

    if (MissingAnyInt32Bounds(lhs, rhs))
        return new(alloc) Range(NoInt32LowerBound, NoInt32UpperBound, fractional, exponent);
    int64_t a = (int64_t)lhs->lower() * (int64_t)rhs->lower();
    int64_t b = (int64_t)lhs->lower() * (int64_t)rhs->upper();
    int64_t c = (int64_t)lhs->upper() * (int64_t)rhs->lower();
    int64_t d = (int64_t)lhs->upper() * (int64_t)rhs->upper();
    return new(alloc) Range(
        Min( Min(a, b), Min(c, d) ),
        Max( Max(a, b), Max(c, d) ),
        fractional, exponent);
}

Range *
Range::lsh(TempAllocator &alloc, const Range *lhs, int32_t c)
{
    JS_ASSERT(lhs->isInt32());
    int32_t shift = c & 0x1f;

    // If the shift doesn't loose bits or shift bits into the sign bit, we
    // can simply compute the correct range by shifting.
    if ((int32_t)((uint32_t)lhs->lower() << shift << 1 >> shift >> 1) == lhs->lower() &&
        (int32_t)((uint32_t)lhs->upper() << shift << 1 >> shift >> 1) == lhs->upper())
    {
        return Range::NewInt32Range(alloc,
            uint32_t(lhs->lower()) << shift,
            uint32_t(lhs->upper()) << shift);
    }

    return Range::NewInt32Range(alloc, INT32_MIN, INT32_MAX);
}

Range *
Range::rsh(TempAllocator &alloc, const Range *lhs, int32_t c)
{
    JS_ASSERT(lhs->isInt32());
    int32_t shift = c & 0x1f;
    return Range::NewInt32Range(alloc,
        lhs->lower() >> shift,
        lhs->upper() >> shift);
}

Range *
Range::ursh(TempAllocator &alloc, const Range *lhs, int32_t c)
{
    // ursh's left operand is uint32, not int32, but for range analysis we
    // currently approximate it as int32. We assume here that the range has
    // already been adjusted accordingly by our callers.
    JS_ASSERT(lhs->isInt32());

    int32_t shift = c & 0x1f;

    // If the value is always non-negative or always negative, we can simply
    // compute the correct range by shifting.
    if (lhs->isFiniteNonNegative() || lhs->isFiniteNegative()) {
        return Range::NewUInt32Range(alloc,
            uint32_t(lhs->lower()) >> shift,
            uint32_t(lhs->upper()) >> shift);
    }

    // Otherwise return the most general range after the shift.
    return Range::NewUInt32Range(alloc, 0, UINT32_MAX >> shift);
}

Range *
Range::lsh(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    JS_ASSERT(lhs->isInt32());
    JS_ASSERT(rhs->isInt32());
    return Range::NewInt32Range(alloc, INT32_MIN, INT32_MAX);
}

Range *
Range::rsh(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    JS_ASSERT(lhs->isInt32());
    JS_ASSERT(rhs->isInt32());
    return Range::NewInt32Range(alloc, Min(lhs->lower(), 0), Max(lhs->upper(), 0));
}

Range *
Range::ursh(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    // ursh's left operand is uint32, not int32, but for range analysis we
    // currently approximate it as int32. We assume here that the range has
    // already been adjusted accordingly by our callers.
    JS_ASSERT(lhs->isInt32());
    JS_ASSERT(rhs->isInt32());
    return Range::NewUInt32Range(alloc, 0, lhs->isFiniteNonNegative() ? lhs->upper() : UINT32_MAX);
}

Range *
Range::abs(TempAllocator &alloc, const Range *op)
{
    int32_t l = op->lower_;
    int32_t u = op->upper_;

    return new(alloc) Range(Max(Max(int32_t(0), l), u == INT32_MIN ? INT32_MAX : -u),
                            true,
                            Max(Max(int32_t(0), u), l == INT32_MIN ? INT32_MAX : -l),
                            op->hasInt32LowerBound_ && op->hasInt32UpperBound_ && l != INT32_MIN,
                            op->canHaveFractionalPart_,
                            op->max_exponent_);
}

Range *
Range::min(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    // If either operand is NaN, the result is NaN.
    if (lhs->canBeNaN() || rhs->canBeNaN())
        return nullptr;

    return new(alloc) Range(Min(lhs->lower_, rhs->lower_),
                            lhs->hasInt32LowerBound_ && rhs->hasInt32LowerBound_,
                            Min(lhs->upper_, rhs->upper_),
                            lhs->hasInt32UpperBound_ || rhs->hasInt32UpperBound_,
                            lhs->canHaveFractionalPart_ || rhs->canHaveFractionalPart_,
                            Max(lhs->max_exponent_, rhs->max_exponent_));
}

Range *
Range::max(TempAllocator &alloc, const Range *lhs, const Range *rhs)
{
    // If either operand is NaN, the result is NaN.
    if (lhs->canBeNaN() || rhs->canBeNaN())
        return nullptr;

    return new(alloc) Range(Max(lhs->lower_, rhs->lower_),
                            lhs->hasInt32LowerBound_ || rhs->hasInt32LowerBound_,
                            Max(lhs->upper_, rhs->upper_),
                            lhs->hasInt32UpperBound_ && rhs->hasInt32UpperBound_,
                            lhs->canHaveFractionalPart_ || rhs->canHaveFractionalPart_,
                            Max(lhs->max_exponent_, rhs->max_exponent_));
}

bool
Range::negativeZeroMul(const Range *lhs, const Range *rhs)
{
    // The result can only be negative zero if both sides are finite and they
    // have differing signs.
    return (lhs->canBeFiniteNegative() && rhs->canBeFiniteNonNegative()) ||
           (rhs->canBeFiniteNegative() && lhs->canBeFiniteNonNegative());
}

bool
Range::update(const Range *other)
{
    bool changed =
        lower_ != other->lower_ ||
        hasInt32LowerBound_ != other->hasInt32LowerBound_ ||
        upper_ != other->upper_ ||
        hasInt32UpperBound_ != other->hasInt32UpperBound_ ||
        canHaveFractionalPart_ != other->canHaveFractionalPart_ ||
        max_exponent_ != other->max_exponent_;
    if (changed) {
        lower_ = other->lower_;
        hasInt32LowerBound_ = other->hasInt32LowerBound_;
        upper_ = other->upper_;
        hasInt32UpperBound_ = other->hasInt32UpperBound_;
        canHaveFractionalPart_ = other->canHaveFractionalPart_;
        max_exponent_ = other->max_exponent_;
        assertInvariants();
    }

    return changed;
}

///////////////////////////////////////////////////////////////////////////////
// Range Computation for MIR Nodes
///////////////////////////////////////////////////////////////////////////////

void
MPhi::computeRange(TempAllocator &alloc)
{
    if (type() != MIRType_Int32 && type() != MIRType_Double)
        return;

    Range *range = nullptr;
    JS_ASSERT(!isOSRLikeValue(getOperand(0)));
    for (size_t i = 0, e = numOperands(); i < e; i++) {
        if (getOperand(i)->block()->unreachable()) {
            IonSpew(IonSpew_Range, "Ignoring unreachable input %d", getOperand(i)->id());
            continue;
        }

        if (isOSRLikeValue(getOperand(i)))
            continue;

        // Peek at the pre-bailout range so we can take a short-cut; if any of
        // the operands has an unknown range, this phi has an unknown range.
        if (!getOperand(i)->range())
            return;

        Range input(getOperand(i));

        if (range)
            range->unionWith(&input);
        else
            range = new(alloc) Range(input);
    }

    setRange(range);
}

void
MBeta::computeRange(TempAllocator &alloc)
{
    bool emptyRange = false;

    Range opRange(getOperand(0));
    Range *range = Range::intersect(alloc, &opRange, comparison_, &emptyRange);
    if (emptyRange) {
        IonSpew(IonSpew_Range, "Marking block for inst %d unreachable", id());
        block()->setUnreachable();
    } else {
        setRange(range);
    }
}

void
MConstant::computeRange(TempAllocator &alloc)
{
    if (value().isNumber()) {
        double d = value().toNumber();
        setRange(Range::NewDoubleRange(alloc, d, d));
    } else if (value().isBoolean()) {
        bool b = value().toBoolean();
        setRange(Range::NewInt32Range(alloc, b, b));
    }
}

void
MCharCodeAt::computeRange(TempAllocator &alloc)
{
    // ECMA 262 says that the integer will be non-negative and at most 65535.
    setRange(Range::NewInt32Range(alloc, 0, 65535));
}

void
MClampToUint8::computeRange(TempAllocator &alloc)
{
    setRange(Range::NewUInt32Range(alloc, 0, 255));
}

void
MBitAnd::computeRange(TempAllocator &alloc)
{
    Range left(getOperand(0));
    Range right(getOperand(1));
    left.wrapAroundToInt32();
    right.wrapAroundToInt32();

    setRange(Range::and_(alloc, &left, &right));
}

void
MBitOr::computeRange(TempAllocator &alloc)
{
    Range left(getOperand(0));
    Range right(getOperand(1));
    left.wrapAroundToInt32();
    right.wrapAroundToInt32();

    setRange(Range::or_(alloc, &left, &right));
}

void
MBitXor::computeRange(TempAllocator &alloc)
{
    Range left(getOperand(0));
    Range right(getOperand(1));
    left.wrapAroundToInt32();
    right.wrapAroundToInt32();

    setRange(Range::xor_(alloc, &left, &right));
}

void
MBitNot::computeRange(TempAllocator &alloc)
{
    Range op(getOperand(0));
    op.wrapAroundToInt32();

    setRange(Range::not_(alloc, &op));
}

void
MLsh::computeRange(TempAllocator &alloc)
{
    Range left(getOperand(0));
    Range right(getOperand(1));
    left.wrapAroundToInt32();

    MDefinition *rhs = getOperand(1);
    if (!rhs->isConstant()) {
        right.wrapAroundToShiftCount();
        setRange(Range::lsh(alloc, &left, &right));
        return;
    }

    int32_t c = rhs->toConstant()->value().toInt32();
    setRange(Range::lsh(alloc, &left, c));
}

void
MRsh::computeRange(TempAllocator &alloc)
{
    Range left(getOperand(0));
    Range right(getOperand(1));
    left.wrapAroundToInt32();

    MDefinition *rhs = getOperand(1);
    if (!rhs->isConstant()) {
        right.wrapAroundToShiftCount();
        setRange(Range::rsh(alloc, &left, &right));
        return;
    }

    int32_t c = rhs->toConstant()->value().toInt32();
    setRange(Range::rsh(alloc, &left, c));
}

void
MUrsh::computeRange(TempAllocator &alloc)
{
    Range left(getOperand(0));
    Range right(getOperand(1));

    // ursh can be thought of as converting its left operand to uint32, or it
    // can be thought of as converting its left operand to int32, and then
    // reinterpreting the int32 bits as a uint32 value. Both approaches yield
    // the same result. Since we lack support for full uint32 ranges, we use
    // the second interpretation, though it does cause us to be conservative.
    left.wrapAroundToInt32();
    right.wrapAroundToShiftCount();

    MDefinition *rhs = getOperand(1);
    if (!rhs->isConstant()) {
        setRange(Range::ursh(alloc, &left, &right));
    } else {
        int32_t c = rhs->toConstant()->value().toInt32();
        setRange(Range::ursh(alloc, &left, c));
    }

    JS_ASSERT(range()->lower() >= 0);
}

void
MAbs::computeRange(TempAllocator &alloc)
{
    if (specialization_ != MIRType_Int32 && specialization_ != MIRType_Double)
        return;

    Range other(getOperand(0));
    Range *next = Range::abs(alloc, &other);
    if (implicitTruncate_)
        next->wrapAroundToInt32();
    setRange(next);
}

void
MMinMax::computeRange(TempAllocator &alloc)
{
    if (specialization_ != MIRType_Int32 && specialization_ != MIRType_Double)
        return;

    Range left(getOperand(0));
    Range right(getOperand(1));
    setRange(isMax() ? Range::max(alloc, &left, &right) : Range::min(alloc, &left, &right));
}

void
MAdd::computeRange(TempAllocator &alloc)
{
    if (specialization() != MIRType_Int32 && specialization() != MIRType_Double)
        return;
    Range left(getOperand(0));
    Range right(getOperand(1));
    Range *next = Range::add(alloc, &left, &right);
    if (isTruncated())
        next->wrapAroundToInt32();
    setRange(next);
}

void
MSub::computeRange(TempAllocator &alloc)
{
    if (specialization() != MIRType_Int32 && specialization() != MIRType_Double)
        return;
    Range left(getOperand(0));
    Range right(getOperand(1));
    Range *next = Range::sub(alloc, &left, &right);
    if (isTruncated())
        next->wrapAroundToInt32();
    setRange(next);
}

void
MMul::computeRange(TempAllocator &alloc)
{
    if (specialization() != MIRType_Int32 && specialization() != MIRType_Double)
        return;
    Range left(getOperand(0));
    Range right(getOperand(1));
    if (canBeNegativeZero())
        canBeNegativeZero_ = Range::negativeZeroMul(&left, &right);
    Range *next = Range::mul(alloc, &left, &right);
    // Truncated multiplications could overflow in both directions
    if (isTruncated())
        next->wrapAroundToInt32();
    setRange(next);
}

void
MMod::computeRange(TempAllocator &alloc)
{
    if (specialization() != MIRType_Int32 && specialization() != MIRType_Double)
        return;
    Range lhs(getOperand(0));
    Range rhs(getOperand(1));

    // If either operand is a NaN, the result is NaN. This also conservatively
    // handles Infinity cases.
    if (!lhs.hasInt32Bounds() || !rhs.hasInt32Bounds())
        return;

    // If RHS can be zero, the result can be NaN.
    if (rhs.lower() <= 0 && rhs.upper() >= 0)
        return;

    // If both operands are non-negative integers, we can optimize this to an
    // unsigned mod.
    if (specialization() == MIRType_Int32 && lhs.lower() >= 0 && rhs.lower() > 0 &&
        !lhs.canHaveFractionalPart() && !rhs.canHaveFractionalPart())
    {
        unsigned_ = true;
    }

    // For unsigned mod, we have to convert both operands to unsigned.
    // Note that we handled the case of a zero rhs above.
    if (unsigned_) {
        // The result of an unsigned mod will never be unsigned-greater than
        // either operand.
        uint32_t lhsBound = Max<uint32_t>(lhs.lower(), lhs.upper());
        uint32_t rhsBound = Max<uint32_t>(rhs.lower(), rhs.upper());

        // If either range crosses through -1 as a signed value, it could be
        // the maximum unsigned value when interpreted as unsigned. If the range
        // doesn't include -1, then the simple max value we computed above is
        // correct.
        if (lhs.lower() <= -1 && lhs.upper() >= -1)
            lhsBound = UINT32_MAX;
        if (rhs.lower() <= -1 && rhs.upper() >= -1)
            rhsBound = UINT32_MAX;

        // The result will never be equal to the rhs, and we shouldn't have
        // any rounding to worry about.
        JS_ASSERT(!lhs.canHaveFractionalPart() && !rhs.canHaveFractionalPart());
        --rhsBound;

        // This gives us two upper bounds, so we can take the best one.
        setRange(Range::NewUInt32Range(alloc, 0, Min(lhsBound, rhsBound)));
        return;
    }

    // Math.abs(lhs % rhs) == Math.abs(lhs) % Math.abs(rhs).
    // First, the absolute value of the result will always be less than the
    // absolute value of rhs. (And if rhs is zero, the result is NaN).
    int64_t a = Abs<int64_t>(rhs.lower());
    int64_t b = Abs<int64_t>(rhs.upper());
    if (a == 0 && b == 0)
        return;
    int64_t rhsAbsBound = Max(a, b);

    // If the value is known to be integer, less-than abs(rhs) is equivalent
    // to less-than-or-equal abs(rhs)-1. This is important for being able to
    // say that the result of x%256 is an 8-bit unsigned number.
    if (!lhs.canHaveFractionalPart() && !rhs.canHaveFractionalPart())
        --rhsAbsBound;

    // Next, the absolute value of the result will never be greater than the
    // absolute value of lhs.
    int64_t lhsAbsBound = Max(Abs<int64_t>(lhs.lower()), Abs<int64_t>(lhs.upper()));

    // This gives us two upper bounds, so we can take the best one.
    int64_t absBound = Min(lhsAbsBound, rhsAbsBound);

    // Now consider the sign of the result.
    // If lhs is non-negative, the result will be non-negative.
    // If lhs is non-positive, the result will be non-positive.
    int64_t lower = lhs.lower() >= 0 ? 0 : -absBound;
    int64_t upper = lhs.upper() <= 0 ? 0 : absBound;

    setRange(new(alloc) Range(lower, upper, lhs.canHaveFractionalPart() || rhs.canHaveFractionalPart(),
                              Min(lhs.exponent(), rhs.exponent())));
}

void
MDiv::computeRange(TempAllocator &alloc)
{
    if (specialization() != MIRType_Int32 && specialization() != MIRType_Double)
        return;
    Range lhs(getOperand(0));
    Range rhs(getOperand(1));

    // If either operand is a NaN, the result is NaN. This also conservatively
    // handles Infinity cases.
    if (!lhs.hasInt32Bounds() || !rhs.hasInt32Bounds())
        return;

    // Something simple for now: When dividing by a positive rhs, the result
    // won't be further from zero than lhs.
    if (lhs.lower() >= 0 && rhs.lower() >= 1) {
        setRange(new(alloc) Range(0, lhs.upper(), true, lhs.exponent()));
    } else if (unsigned_ && rhs.lower() >= 1) {
        // We shouldn't set the unsigned flag if the inputs can have
        // fractional parts.
        JS_ASSERT(!lhs.canHaveFractionalPart() && !rhs.canHaveFractionalPart());
        // Unsigned division by a non-zero rhs will return a uint32 value.
        setRange(Range::NewUInt32Range(alloc, 0, UINT32_MAX));
    }
}

void
MSqrt::computeRange(TempAllocator &alloc)
{
    Range input(getOperand(0));

    // If either operand is a NaN, the result is NaN. This also conservatively
    // handles Infinity cases.
    if (!input.hasInt32Bounds())
        return;

    // Sqrt of a negative non-zero value is NaN.
    if (input.lower() < 0)
        return;

    // Something simple for now: When taking the sqrt of a positive value, the
    // result won't be further from zero than the input.
    setRange(new(alloc) Range(0, input.upper(), true, input.exponent()));
}

void
MToDouble::computeRange(TempAllocator &alloc)
{
    setRange(new(alloc) Range(getOperand(0)));
}

void
MToFloat32::computeRange(TempAllocator &alloc)
{
}

void
MTruncateToInt32::computeRange(TempAllocator &alloc)
{
    Range *output = new(alloc) Range(getOperand(0));
    output->wrapAroundToInt32();
    setRange(output);
}

void
MToInt32::computeRange(TempAllocator &alloc)
{
    Range *output = new(alloc) Range(getOperand(0));
    output->clampToInt32();
    setRange(output);
}

static Range *GetTypedArrayRange(TempAllocator &alloc, int type)
{
    switch (type) {
      case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED:
      case ScalarTypeRepresentation::TYPE_UINT8:
        return Range::NewUInt32Range(alloc, 0, UINT8_MAX);
      case ScalarTypeRepresentation::TYPE_UINT16:
        return Range::NewUInt32Range(alloc, 0, UINT16_MAX);
      case ScalarTypeRepresentation::TYPE_UINT32:
        return Range::NewUInt32Range(alloc, 0, UINT32_MAX);

      case ScalarTypeRepresentation::TYPE_INT8:
        return Range::NewInt32Range(alloc, INT8_MIN, INT8_MAX);
      case ScalarTypeRepresentation::TYPE_INT16:
        return Range::NewInt32Range(alloc, INT16_MIN, INT16_MAX);
      case ScalarTypeRepresentation::TYPE_INT32:
        return Range::NewInt32Range(alloc, INT32_MIN, INT32_MAX);

      case ScalarTypeRepresentation::TYPE_FLOAT32:
      case ScalarTypeRepresentation::TYPE_FLOAT64:
        break;
    }

  return nullptr;
}

void
MLoadTypedArrayElement::computeRange(TempAllocator &alloc)
{
    // We have an Int32 type and if this is a UInt32 load it may produce a value
    // outside of our range, but we have a bailout to handle those cases.
    setRange(GetTypedArrayRange(alloc, arrayType()));
}

void
MLoadTypedArrayElementStatic::computeRange(TempAllocator &alloc)
{
    // We don't currently use MLoadTypedArrayElementStatic for uint32, so we
    // don't have to worry about it returning a value outside our type.
    JS_ASSERT(typedArray_->type() != ScalarTypeRepresentation::TYPE_UINT32);

    setRange(GetTypedArrayRange(alloc, typedArray_->type()));
}

void
MArrayLength::computeRange(TempAllocator &alloc)
{
    // Array lengths can go up to UINT32_MAX, but we only create MArrayLength
    // nodes when the value is known to be int32 (see the
    // OBJECT_FLAG_LENGTH_OVERFLOW flag).
    setRange(Range::NewUInt32Range(alloc, 0, INT32_MAX));
}

void
MInitializedLength::computeRange(TempAllocator &alloc)
{
    setRange(Range::NewUInt32Range(alloc, 0, JSObject::NELEMENTS_LIMIT));
}

void
MTypedArrayLength::computeRange(TempAllocator &alloc)
{
    setRange(Range::NewUInt32Range(alloc, 0, INT32_MAX));
}

void
MStringLength::computeRange(TempAllocator &alloc)
{
    static_assert(JSString::MAX_LENGTH <= UINT32_MAX,
                  "NewUInt32Range requires a uint32 value");
    setRange(Range::NewUInt32Range(alloc, 0, JSString::MAX_LENGTH));
}

void
MArgumentsLength::computeRange(TempAllocator &alloc)
{
    // This is is a conservative upper bound on what |TooManyArguments| checks.
    // If exceeded, Ion will not be entered in the first place.
    static_assert(SNAPSHOT_MAX_NARGS <= UINT32_MAX,
                  "NewUInt32Range requires a uint32 value");
    setRange(Range::NewUInt32Range(alloc, 0, SNAPSHOT_MAX_NARGS));
}

void
MBoundsCheck::computeRange(TempAllocator &alloc)
{
    // Just transfer the incoming index range to the output. The length() is
    // also interesting, but it is handled as a bailout check, and we're
    // computing a pre-bailout range here.
    setRange(new(alloc) Range(index()));
}

void
MArrayPush::computeRange(TempAllocator &alloc)
{
    // MArrayPush returns the new array length.
    setRange(Range::NewUInt32Range(alloc, 0, UINT32_MAX));
}

void
MMathFunction::computeRange(TempAllocator &alloc)
{
    Range opRange(getOperand(0));
    switch (function()) {
      case Sin:
      case Cos:
        if (!opRange.canBeInfiniteOrNaN())
            setRange(Range::NewDoubleRange(alloc, -1.0, 1.0));
        break;
      case Sign:
        if (!opRange.canBeNaN()) {
            // Note that Math.sign(-0) is -0, and we treat -0 as equal to 0.
            int32_t lower = -1;
            int32_t upper = 1;
            if (opRange.hasInt32LowerBound() && opRange.lower() >= 0)
                lower = 0;
            if (opRange.hasInt32UpperBound() && opRange.upper() <= 0)
                upper = 0;
            setRange(Range::NewInt32Range(alloc, lower, upper));
        }
        break;
    default:
        break;
    }
}

void
MRandom::computeRange(TempAllocator &alloc)
{
    setRange(Range::NewDoubleRange(alloc, 0.0, 1.0));
}

///////////////////////////////////////////////////////////////////////////////
// Range Analysis
///////////////////////////////////////////////////////////////////////////////

bool
RangeAnalysis::markBlocksInLoopBody(MBasicBlock *header, MBasicBlock *backedge)
{
    Vector<MBasicBlock *, 16, IonAllocPolicy> worklist(alloc());

    // Mark the header as being in the loop. This terminates the walk.
    header->mark();

    backedge->mark();
    if (!worklist.append(backedge))
        return false;

    // If we haven't reached the loop header yet, walk up the predecessors
    // we haven't seen already.
    while (!worklist.empty()) {
        MBasicBlock *current = worklist.popCopy();
        for (size_t i = 0; i < current->numPredecessors(); i++) {
            MBasicBlock *pred = current->getPredecessor(i);

            if (pred->isMarked())
                continue;

            pred->mark();
            if (!worklist.append(pred))
                return false;
        }
    }

    return true;
}

bool
RangeAnalysis::analyzeLoop(MBasicBlock *header)
{
    JS_ASSERT(header->hasUniqueBackedge());

    // Try to compute an upper bound on the number of times the loop backedge
    // will be taken. Look for tests that dominate the backedge and which have
    // an edge leaving the loop body.
    MBasicBlock *backedge = header->backedge();

    // Ignore trivial infinite loops.
    if (backedge == header)
        return true;

    if (!markBlocksInLoopBody(header, backedge))
        return false;

    LoopIterationBound *iterationBound = nullptr;

    MBasicBlock *block = backedge;
    do {
        BranchDirection direction;
        MTest *branch = block->immediateDominatorBranch(&direction);

        if (block == block->immediateDominator())
            break;

        block = block->immediateDominator();

        if (branch) {
            direction = NegateBranchDirection(direction);
            MBasicBlock *otherBlock = branch->branchSuccessor(direction);
            if (!otherBlock->isMarked()) {
                iterationBound =
                    analyzeLoopIterationCount(header, branch, direction);
                if (iterationBound)
                    break;
            }
        }
    } while (block != header);

    if (!iterationBound) {
        graph_.unmarkBlocks();
        return true;
    }

#ifdef DEBUG
    if (IonSpewEnabled(IonSpew_Range)) {
        Sprinter sp(GetIonContext()->cx);
        sp.init();
        iterationBound->sum.print(sp);
        IonSpew(IonSpew_Range, "computed symbolic bound on backedges: %s",
                sp.string());
    }
#endif

    // Try to compute symbolic bounds for the phi nodes at the head of this
    // loop, expressed in terms of the iteration bound just computed.

    for (MPhiIterator iter(header->phisBegin()); iter != header->phisEnd(); iter++)
        analyzeLoopPhi(header, iterationBound, *iter);

    if (!mir->compilingAsmJS()) {
        // Try to hoist any bounds checks from the loop using symbolic bounds.

        Vector<MBoundsCheck *, 0, IonAllocPolicy> hoistedChecks(alloc());

        for (ReversePostorderIterator iter(graph_.rpoBegin(header)); iter != graph_.rpoEnd(); iter++) {
            MBasicBlock *block = *iter;
            if (!block->isMarked())
                continue;

            for (MDefinitionIterator iter(block); iter; iter++) {
                MDefinition *def = *iter;
                if (def->isBoundsCheck() && def->isMovable()) {
                    if (tryHoistBoundsCheck(header, def->toBoundsCheck())) {
                        if (!hoistedChecks.append(def->toBoundsCheck()))
                            return false;
                    }
                }
            }
        }

        // Note: replace all uses of the original bounds check with the
        // actual index. This is usually done during bounds check elimination,
        // but in this case it's safe to do it here since the load/store is
        // definitely not loop-invariant, so we will never move it before
        // one of the bounds checks we just added.
        for (size_t i = 0; i < hoistedChecks.length(); i++) {
            MBoundsCheck *ins = hoistedChecks[i];
            ins->replaceAllUsesWith(ins->index());
            ins->block()->discard(ins);
        }
    }

    graph_.unmarkBlocks();
    return true;
}

LoopIterationBound *
RangeAnalysis::analyzeLoopIterationCount(MBasicBlock *header,
                                         MTest *test, BranchDirection direction)
{
    SimpleLinearSum lhs(nullptr, 0);
    MDefinition *rhs;
    bool lessEqual;
    if (!ExtractLinearInequality(test, direction, &lhs, &rhs, &lessEqual))
        return nullptr;

    // Ensure the rhs is a loop invariant term.
    if (rhs && rhs->block()->isMarked()) {
        if (lhs.term && lhs.term->block()->isMarked())
            return nullptr;
        MDefinition *temp = lhs.term;
        lhs.term = rhs;
        rhs = temp;
        if (!SafeSub(0, lhs.constant, &lhs.constant))
            return nullptr;
        lessEqual = !lessEqual;
    }

    JS_ASSERT_IF(rhs, !rhs->block()->isMarked());

    // Ensure the lhs is a phi node from the start of the loop body.
    if (!lhs.term || !lhs.term->isPhi() || lhs.term->block() != header)
        return nullptr;

    // Check that the value of the lhs changes by a constant amount with each
    // loop iteration. This requires that the lhs be written in every loop
    // iteration with a value that is a constant difference from its value at
    // the start of the iteration.

    if (lhs.term->toPhi()->numOperands() != 2)
        return nullptr;

    // The first operand of the phi should be the lhs' value at the start of
    // the first executed iteration, and not a value written which could
    // replace the second operand below during the middle of execution.
    MDefinition *lhsInitial = lhs.term->toPhi()->getOperand(0);
    if (lhsInitial->block()->isMarked())
        return nullptr;

    // The second operand of the phi should be a value written by an add/sub
    // in every loop iteration, i.e. in a block which dominates the backedge.
    MDefinition *lhsWrite = lhs.term->toPhi()->getOperand(1);
    if (lhsWrite->isBeta())
        lhsWrite = lhsWrite->getOperand(0);
    if (!lhsWrite->isAdd() && !lhsWrite->isSub())
        return nullptr;
    if (!lhsWrite->block()->isMarked())
        return nullptr;
    MBasicBlock *bb = header->backedge();
    for (; bb != lhsWrite->block() && bb != header; bb = bb->immediateDominator()) {}
    if (bb != lhsWrite->block())
        return nullptr;

    SimpleLinearSum lhsModified = ExtractLinearSum(lhsWrite);

    // Check that the value of the lhs at the backedge is of the form
    // 'old(lhs) + N'. We can be sure that old(lhs) is the value at the start
    // of the iteration, and not that written to lhs in a previous iteration,
    // as such a previous value could not appear directly in the addition:
    // it could not be stored in lhs as the lhs add/sub executes in every
    // iteration, and if it were stored in another variable its use here would
    // be as an operand to a phi node for that variable.
    if (lhsModified.term != lhs.term)
        return nullptr;

    LinearSum bound(alloc());

    if (lhsModified.constant == 1 && !lessEqual) {
        // The value of lhs is 'initial(lhs) + iterCount' and this will end
        // execution of the loop if 'lhs + lhsN >= rhs'. Thus, an upper bound
        // on the number of backedges executed is:
        //
        // initial(lhs) + iterCount + lhsN == rhs
        // iterCount == rhsN - initial(lhs) - lhsN

        if (rhs) {
            if (!bound.add(rhs, 1))
                return nullptr;
        }
        if (!bound.add(lhsInitial, -1))
            return nullptr;

        int32_t lhsConstant;
        if (!SafeSub(0, lhs.constant, &lhsConstant))
            return nullptr;
        if (!bound.add(lhsConstant))
            return nullptr;
    } else if (lhsModified.constant == -1 && lessEqual) {
        // The value of lhs is 'initial(lhs) - iterCount'. Similar to the above
        // case, an upper bound on the number of backedges executed is:
        //
        // initial(lhs) - iterCount + lhsN == rhs
        // iterCount == initial(lhs) - rhs + lhsN

        if (!bound.add(lhsInitial, 1))
            return nullptr;
        if (rhs) {
            if (!bound.add(rhs, -1))
                return nullptr;
        }
        if (!bound.add(lhs.constant))
            return nullptr;
    } else {
        return nullptr;
    }

    return new(alloc()) LoopIterationBound(header, test, bound);
}

void
RangeAnalysis::analyzeLoopPhi(MBasicBlock *header, LoopIterationBound *loopBound, MPhi *phi)
{
    // Given a bound on the number of backedges taken, compute an upper and
    // lower bound for a phi node that may change by a constant amount each
    // iteration. Unlike for the case when computing the iteration bound
    // itself, the phi does not need to change the same amount every iteration,
    // but is required to change at most N and be either nondecreasing or
    // nonincreasing.

    JS_ASSERT(phi->numOperands() == 2);

    MBasicBlock *preLoop = header->loopPredecessor();
    JS_ASSERT(!preLoop->isMarked() && preLoop->successorWithPhis() == header);

    MBasicBlock *backedge = header->backedge();
    JS_ASSERT(backedge->isMarked() && backedge->successorWithPhis() == header);

    MDefinition *initial = phi->getOperand(preLoop->positionInPhiSuccessor());
    if (initial->block()->isMarked())
        return;

    SimpleLinearSum modified = ExtractLinearSum(phi->getOperand(backedge->positionInPhiSuccessor()));

    if (modified.term != phi || modified.constant == 0)
        return;

    if (!phi->range())
        phi->setRange(new(alloc()) Range());

    LinearSum initialSum(alloc());
    if (!initialSum.add(initial, 1))
        return;

    // The phi may change by N each iteration, and is either nondecreasing or
    // nonincreasing. initial(phi) is either a lower or upper bound for the
    // phi, and initial(phi) + loopBound * N is either an upper or lower bound,
    // at all points within the loop, provided that loopBound >= 0.
    //
    // We are more interested, however, in the bound for phi at points
    // dominated by the loop bound's test; if the test dominates e.g. a bounds
    // check we want to hoist from the loop, using the value of the phi at the
    // head of the loop for this will usually be too imprecise to hoist the
    // check. These points will execute only if the backedge executes at least
    // one more time (as the test passed and the test dominates the backedge),
    // so we know both that loopBound >= 1 and that the phi's value has changed
    // at most loopBound - 1 times. Thus, another upper or lower bound for the
    // phi is initial(phi) + (loopBound - 1) * N, without requiring us to
    // ensure that loopBound >= 0.

    LinearSum limitSum(loopBound->sum);
    if (!limitSum.multiply(modified.constant) || !limitSum.add(initialSum))
        return;

    int32_t negativeConstant;
    if (!SafeSub(0, modified.constant, &negativeConstant) || !limitSum.add(negativeConstant))
        return;

    Range *initRange = initial->range();
    if (modified.constant > 0) {
        if (initRange && initRange->hasInt32LowerBound())
            phi->range()->refineLower(initRange->lower());
        phi->range()->setSymbolicLower(SymbolicBound::New(alloc(), nullptr, initialSum));
        phi->range()->setSymbolicUpper(SymbolicBound::New(alloc(), loopBound, limitSum));
    } else {
        if (initRange && initRange->hasInt32UpperBound())
            phi->range()->refineUpper(initRange->upper());
        phi->range()->setSymbolicUpper(SymbolicBound::New(alloc(), nullptr, initialSum));
        phi->range()->setSymbolicLower(SymbolicBound::New(alloc(), loopBound, limitSum));
    }

    IonSpew(IonSpew_Range, "added symbolic range on %d", phi->id());
    SpewRange(phi);
}

// Whether bound is valid at the specified bounds check instruction in a loop,
// and may be used to hoist ins.
static inline bool
SymbolicBoundIsValid(MBasicBlock *header, MBoundsCheck *ins, const SymbolicBound *bound)
{
    if (!bound->loop)
        return true;
    if (ins->block() == header)
        return false;
    MBasicBlock *bb = ins->block()->immediateDominator();
    while (bb != header && bb != bound->loop->test->block())
        bb = bb->immediateDominator();
    return bb == bound->loop->test->block();
}

// Convert all components of a linear sum *except* its constant to a definition,
// adding any necessary instructions to the end of block.
static inline MDefinition *
ConvertLinearSum(TempAllocator &alloc, MBasicBlock *block, const LinearSum &sum)
{
    MDefinition *def = nullptr;

    for (size_t i = 0; i < sum.numTerms(); i++) {
        LinearTerm term = sum.term(i);
        JS_ASSERT(!term.term->isConstant());
        if (term.scale == 1) {
            if (def) {
                def = MAdd::New(alloc, def, term.term);
                def->toAdd()->setInt32();
                block->insertBefore(block->lastIns(), def->toInstruction());
                def->computeRange(alloc);
            } else {
                def = term.term;
            }
        } else if (term.scale == -1) {
            if (!def) {
                def = MConstant::New(alloc, Int32Value(0));
                block->insertBefore(block->lastIns(), def->toInstruction());
                def->computeRange(alloc);
            }
            def = MSub::New(alloc, def, term.term);
            def->toSub()->setInt32();
            block->insertBefore(block->lastIns(), def->toInstruction());
            def->computeRange(alloc);
        } else {
            JS_ASSERT(term.scale != 0);
            MConstant *factor = MConstant::New(alloc, Int32Value(term.scale));
            block->insertBefore(block->lastIns(), factor);
            MMul *mul = MMul::New(alloc, term.term, factor);
            mul->setInt32();
            block->insertBefore(block->lastIns(), mul);
            mul->computeRange(alloc);
            if (def) {
                def = MAdd::New(alloc, def, mul);
                def->toAdd()->setInt32();
                block->insertBefore(block->lastIns(), def->toInstruction());
                def->computeRange(alloc);
            } else {
                def = mul;
            }
        }
    }

    if (!def) {
        def = MConstant::New(alloc, Int32Value(0));
        block->insertBefore(block->lastIns(), def->toInstruction());
        def->computeRange(alloc);
    }

    return def;
}

bool
RangeAnalysis::tryHoistBoundsCheck(MBasicBlock *header, MBoundsCheck *ins)
{
    // The bounds check's length must be loop invariant.
    if (ins->length()->block()->isMarked())
        return false;

    // The bounds check's index should not be loop invariant (else we would
    // already have hoisted it during LICM).
    SimpleLinearSum index = ExtractLinearSum(ins->index());
    if (!index.term || !index.term->block()->isMarked())
        return false;

    // Check for a symbolic lower and upper bound on the index. If either
    // condition depends on an iteration bound for the loop, only hoist if
    // the bounds check is dominated by the iteration bound's test.
    if (!index.term->range())
        return false;
    const SymbolicBound *lower = index.term->range()->symbolicLower();
    if (!lower || !SymbolicBoundIsValid(header, ins, lower))
        return false;
    const SymbolicBound *upper = index.term->range()->symbolicUpper();
    if (!upper || !SymbolicBoundIsValid(header, ins, upper))
        return false;

    MBasicBlock *preLoop = header->loopPredecessor();
    JS_ASSERT(!preLoop->isMarked());

    MDefinition *lowerTerm = ConvertLinearSum(alloc(), preLoop, lower->sum);
    if (!lowerTerm)
        return false;

    MDefinition *upperTerm = ConvertLinearSum(alloc(), preLoop, upper->sum);
    if (!upperTerm)
        return false;

    // We are checking that index + indexConstant >= 0, and know that
    // index >= lowerTerm + lowerConstant. Thus, check that:
    //
    // lowerTerm + lowerConstant + indexConstant >= 0
    // lowerTerm >= -lowerConstant - indexConstant

    int32_t lowerConstant = 0;
    if (!SafeSub(lowerConstant, index.constant, &lowerConstant))
        return false;
    if (!SafeSub(lowerConstant, lower->sum.constant(), &lowerConstant))
        return false;

    // We are checking that index < boundsLength, and know that
    // index <= upperTerm + upperConstant. Thus, check that:
    //
    // upperTerm + upperConstant < boundsLength

    int32_t upperConstant = index.constant;
    if (!SafeAdd(upper->sum.constant(), upperConstant, &upperConstant))
        return false;

    MBoundsCheckLower *lowerCheck = MBoundsCheckLower::New(alloc(), lowerTerm);
    lowerCheck->setMinimum(lowerConstant);

    MBoundsCheck *upperCheck = MBoundsCheck::New(alloc(), upperTerm, ins->length());
    upperCheck->setMinimum(upperConstant);
    upperCheck->setMaximum(upperConstant);

    // Hoist the loop invariant upper and lower bounds checks.
    preLoop->insertBefore(preLoop->lastIns(), lowerCheck);
    preLoop->insertBefore(preLoop->lastIns(), upperCheck);

    return true;
}

bool
RangeAnalysis::analyze()
{
    IonSpew(IonSpew_Range, "Doing range propagation");

    for (ReversePostorderIterator iter(graph_.rpoBegin()); iter != graph_.rpoEnd(); iter++) {
        MBasicBlock *block = *iter;

        if (block->unreachable())
            continue;

        for (MDefinitionIterator iter(block); iter; iter++) {
            MDefinition *def = *iter;

            def->computeRange(alloc());
            IonSpew(IonSpew_Range, "computing range on %d", def->id());
            SpewRange(def);
        }

        if (block->isLoopHeader()) {
            if (!analyzeLoop(block))
                return false;
        }

        // First pass at collecting range info - while the beta nodes are still
        // around and before truncation.
        for (MInstructionIterator iter(block->begin()); iter != block->end(); iter++) {
            iter->collectRangeInfoPreTrunc();

            // Would have been nice to implement this using collectRangeInfoPreTrunc()
            // methods but it needs the minAsmJSHeapLength().
            if (mir->compilingAsmJS()) {
                uint32_t minHeapLength = mir->minAsmJSHeapLength();
                if (iter->isAsmJSLoadHeap()) {
                    MAsmJSLoadHeap *ins = iter->toAsmJSLoadHeap();
                    Range *range = ins->ptr()->range();
                    if (range && range->hasInt32LowerBound() && range->lower() >= 0 &&
                        range->hasInt32UpperBound() && (uint32_t) range->upper() < minHeapLength) {
                        ins->setSkipBoundsCheck(true);
                    }
                } else if (iter->isAsmJSStoreHeap()) {
                    MAsmJSStoreHeap *ins = iter->toAsmJSStoreHeap();
                    Range *range = ins->ptr()->range();
                    if (range && range->hasInt32LowerBound() && range->lower() >= 0 &&
                        range->hasInt32UpperBound() && (uint32_t) range->upper() < minHeapLength) {
                        ins->setSkipBoundsCheck(true);
                    }
                }
            }
        }
    }

    return true;
}

bool
RangeAnalysis::addRangeAssertions()
{
    if (!js_JitOptions.checkRangeAnalysis)
        return true;

    // Check the computed range for this instruction, if the option is set. Note
    // that this code is quite invasive; it adds numerous additional
    // instructions for each MInstruction with a computed range, and it uses
    // registers, so it also affects register allocation.
    for (ReversePostorderIterator iter(graph_.rpoBegin()); iter != graph_.rpoEnd(); iter++) {
        MBasicBlock *block = *iter;

        for (MInstructionIterator iter(block->begin()); iter != block->end(); iter++) {
            MInstruction *ins = *iter;

            // Perform range checking for all numeric and numeric-like types.
            if (!IsNumberType(ins->type()) &&
                ins->type() != MIRType_Boolean &&
                ins->type() != MIRType_Value)
            {
                continue;
            }

            Range r(ins);

            // Don't insert assertions if there's nothing interesting to assert.
            if (r.isUnknown() || (ins->type() == MIRType_Int32 && r.isUnknownInt32()))
                continue;

            MAssertRange *guard = MAssertRange::New(alloc(), ins, new(alloc()) Range(r));

            // The code that removes beta nodes assumes that it can find them
            // in a contiguous run at the top of each block. Don't insert
            // range assertions in between beta nodes.
            MInstructionIterator insertIter = iter;
            while (insertIter->isBeta())
                insertIter++;

            if (*insertIter == *iter)
                block->insertAfter(*insertIter,  guard);
            else
                block->insertBefore(*insertIter, guard);
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// Range based Truncation
///////////////////////////////////////////////////////////////////////////////

void
Range::clampToInt32()
{
    if (isInt32())
        return;
    int32_t l = hasInt32LowerBound() ? lower() : JSVAL_INT_MIN;
    int32_t h = hasInt32UpperBound() ? upper() : JSVAL_INT_MAX;
    setInt32(l, h);
}

void
Range::wrapAroundToInt32()
{
    if (!hasInt32Bounds()) {
        setInt32(JSVAL_INT_MIN, JSVAL_INT_MAX);
    } else if (canHaveFractionalPart()) {
        canHaveFractionalPart_ = false;

        // Clearing the fractional field may provide an opportunity to refine
        // lower_ or upper_.
        refineInt32BoundsByExponent(max_exponent_, &lower_, &upper_);

        assertInvariants();
    }
}

void
Range::wrapAroundToShiftCount()
{
    wrapAroundToInt32();
    if (lower() < 0 || upper() >= 32)
        setInt32(0, 31);
}

void
Range::wrapAroundToBoolean()
{
    wrapAroundToInt32();
    if (!isBoolean())
        setInt32(0, 1);
}

bool
MDefinition::truncate()
{
    // No procedure defined for truncating this instruction.
    return false;
}

bool
MConstant::truncate()
{
    if (!value_.isDouble())
        return false;

    // Truncate the double to int, since all uses truncates it.
    int32_t res = ToInt32(value_.toDouble());
    value_.setInt32(res);
    setResultType(MIRType_Int32);
    if (range())
        range()->setInt32(res, res);
    return true;
}

bool
MAdd::truncate()
{
    // Remember analysis, needed for fallible checks.
    setTruncated(true);

    if (type() == MIRType_Double || type() == MIRType_Int32) {
        specialization_ = MIRType_Int32;
        setResultType(MIRType_Int32);
        if (range())
            range()->wrapAroundToInt32();
        return true;
    }

    return false;
}

bool
MSub::truncate()
{
    // Remember analysis, needed for fallible checks.
    setTruncated(true);

    if (type() == MIRType_Double || type() == MIRType_Int32) {
        specialization_ = MIRType_Int32;
        setResultType(MIRType_Int32);
        if (range())
            range()->wrapAroundToInt32();
        return true;
    }

    return false;
}

bool
MMul::truncate()
{
    // Remember analysis, needed to remove negative zero checks.
    setTruncated(true);

    if (type() == MIRType_Double || type() == MIRType_Int32) {
        specialization_ = MIRType_Int32;
        setResultType(MIRType_Int32);
        setCanBeNegativeZero(false);
        if (range())
            range()->wrapAroundToInt32();
        return true;
    }

    return false;
}

bool
MDiv::truncate()
{
    // Remember analysis, needed to remove negative zero checks.
    setTruncated(true);

    // Divisions where the lhs and rhs are unsigned and the result is
    // truncated can be lowered more efficiently.
    if (specialization() == MIRType_Int32 && tryUseUnsignedOperands()) {
        unsigned_ = true;
        return true;
    }

    // No modifications.
    return false;
}

bool
MMod::truncate()
{
    // Remember analysis, needed to remove negative zero checks.
    setTruncated(true);

    // As for division, handle unsigned modulus with a truncated result.
    if (specialization() == MIRType_Int32 && tryUseUnsignedOperands()) {
        unsigned_ = true;
        return true;
    }

    // No modifications.
    return false;
}

bool
MToDouble::truncate()
{
    JS_ASSERT(type() == MIRType_Double);

    // We use the return type to flag that this MToDouble should be replaced by
    // a MTruncateToInt32 when modifying the graph.
    setResultType(MIRType_Int32);
    if (range())
        range()->wrapAroundToInt32();

    return true;
}

bool
MLoadTypedArrayElementStatic::truncate()
{
    setInfallible();
    return false;
}

bool
MDefinition::isOperandTruncated(size_t index) const
{
    return false;
}

bool
MTruncateToInt32::isOperandTruncated(size_t index) const
{
    return true;
}

bool
MBinaryBitwiseInstruction::isOperandTruncated(size_t index) const
{
    return true;
}

bool
MAdd::isOperandTruncated(size_t index) const
{
    return isTruncated();
}

bool
MSub::isOperandTruncated(size_t index) const
{
    return isTruncated();
}

bool
MMul::isOperandTruncated(size_t index) const
{
    return isTruncated();
}

bool
MToDouble::isOperandTruncated(size_t index) const
{
    // The return type is used to flag that we are replacing this Double by a
    // Truncate of its operand if needed.
    return type() == MIRType_Int32;
}

bool
MStoreTypedArrayElement::isOperandTruncated(size_t index) const
{
    return index == 2 && !isFloatArray();
}

bool
MStoreTypedArrayElementHole::isOperandTruncated(size_t index) const
{
    return index == 3 && !isFloatArray();
}

bool
MStoreTypedArrayElementStatic::isOperandTruncated(size_t index) const
{
    return index == 1 && !isFloatArray();
}

bool
MCompare::truncate()
{
    if (!isDoubleComparison())
        return false;

    // If both operands are naturally in the int32 range, we can convert from
    // a double comparison to being an int32 comparison.
    if (!Range(lhs()).isInt32() || !Range(rhs()).isInt32())
        return false;

    compareType_ = Compare_Int32;
    return true;
}

bool
MCompare::isOperandTruncated(size_t index) const
{
    return compareType() == Compare_Int32;
}

// Ensure that all observables uses can work with a truncated
// version of the |candidate|'s result.
static bool
AllUsesTruncate(MInstruction *candidate)
{
    // If the value naturally produces an int32 value (before bailout checks)
    // that needs no conversion, we don't have to worry about resume points
    // seeing truncated values.
    bool needsConversion = !candidate->range() || !candidate->range()->isInt32();

    for (MUseIterator use(candidate->usesBegin()); use != candidate->usesEnd(); use++) {
        if (!use->consumer()->isDefinition()) {
            // We can only skip testing resume points, if all original uses are
            // still present, or if the value does not need conversion.
            // Otherwise a branch removed by UCE might rely on the non-truncated
            // value, and any bailout with a truncated value might lead an
            // incorrect value.
            if (candidate->isUseRemoved() && needsConversion)
                return false;
            continue;
        }

        if (!use->consumer()->toDefinition()->isOperandTruncated(use->index()))
            return false;
    }

    return true;
}

static bool
CanTruncate(MInstruction *candidate)
{
    // Compare operations might coerce its inputs to int32 if the ranges are
    // correct.  So we do not need to check if all uses are coerced.
    if (candidate->isCompare())
        return true;

    // Set truncated flag if range analysis ensure that it has no
    // rounding errors and no fractional part. Note that we can't use
    // the MDefinition Range constructor, because we need to know if
    // the value will have rounding errors before any bailout checks.
    const Range *r = candidate->range();
    bool canHaveRoundingErrors = !r || r->canHaveRoundingErrors();

    // Special case integer division: the result of a/b can be infinite
    // but cannot actually have rounding errors induced by truncation.
    if (candidate->isDiv() && candidate->toDiv()->specialization() == MIRType_Int32)
        canHaveRoundingErrors = false;

    if (canHaveRoundingErrors)
        return false;

    // Ensure all observable uses are truncated.
    return AllUsesTruncate(candidate);
}

static void
RemoveTruncatesOnOutput(MInstruction *truncated)
{
    // Compare returns a boolean so it doen't have any output truncates.
    if (truncated->isCompare())
        return;

    JS_ASSERT(truncated->type() == MIRType_Int32);
    JS_ASSERT(Range(truncated).isInt32());

    for (MUseDefIterator use(truncated); use; use++) {
        MDefinition *def = use.def();
        if (!def->isTruncateToInt32() || !def->isToInt32())
            continue;

        def->replaceAllUsesWith(truncated);
    }
}

static void
AdjustTruncatedInputs(TempAllocator &alloc, MInstruction *truncated)
{
    MBasicBlock *block = truncated->block();
    for (size_t i = 0, e = truncated->numOperands(); i < e; i++) {
        if (!truncated->isOperandTruncated(i))
            continue;

        MDefinition *input = truncated->getOperand(i);
        if (input->type() == MIRType_Int32)
            continue;

        if (input->isToDouble() && input->getOperand(0)->type() == MIRType_Int32) {
            JS_ASSERT(input->range()->isInt32());
            truncated->replaceOperand(i, input->getOperand(0));
        } else {
            MTruncateToInt32 *op = MTruncateToInt32::New(alloc, truncated->getOperand(i));
            block->insertBefore(truncated, op);
            truncated->replaceOperand(i, op);
        }
    }

    if (truncated->isToDouble()) {
        truncated->replaceAllUsesWith(truncated->getOperand(0));
        block->discard(truncated);
    }
}

// Iterate backward on all instruction and attempt to truncate operations for
// each instruction which respect the following list of predicates: Has been
// analyzed by range analysis, the range has no rounding errors, all uses cases
// are truncating the result.
//
// If the truncation of the operation is successful, then the instruction is
// queue for later updating the graph to restore the type correctness by
// converting the operands that need to be truncated.
//
// We iterate backward because it is likely that a truncated operation truncates
// some of its operands.
bool
RangeAnalysis::truncate()
{
    IonSpew(IonSpew_Range, "Do range-base truncation (backward loop)");

    Vector<MInstruction *, 16, SystemAllocPolicy> worklist;
    Vector<MBinaryBitwiseInstruction *, 16, SystemAllocPolicy> bitops;

    for (PostorderIterator block(graph_.poBegin()); block != graph_.poEnd(); block++) {
        for (MInstructionReverseIterator iter(block->rbegin()); iter != block->rend(); iter++) {
            if (iter->type() == MIRType_None)
                continue;

            // Remember all bitop instructions for folding after range analysis.
            switch (iter->op()) {
              case MDefinition::Op_BitAnd:
              case MDefinition::Op_BitOr:
              case MDefinition::Op_BitXor:
              case MDefinition::Op_Lsh:
              case MDefinition::Op_Rsh:
              case MDefinition::Op_Ursh:
                if (!bitops.append(static_cast<MBinaryBitwiseInstruction*>(*iter)))
                    return false;
              default:;
            }

            if (!CanTruncate(*iter))
                continue;

            // Truncate this instruction if possible.
            if (!iter->truncate())
                continue;

            // Delay updates of inputs/outputs to avoid creating node which
            // would be removed by the truncation of the next operations.
            iter->setInWorklist();
            if (!worklist.append(*iter))
                return false;
        }
    }

    // Update inputs/outputs of truncated instructions.
    IonSpew(IonSpew_Range, "Do graph type fixup (dequeue)");
    while (!worklist.empty()) {
        MInstruction *ins = worklist.popCopy();
        ins->setNotInWorklist();
        RemoveTruncatesOnOutput(ins);
        AdjustTruncatedInputs(alloc(), ins);
    }

    // Fold any unnecessary bitops in the graph, such as (x | 0) on an integer
    // input. This is done after range analysis rather than during GVN as the
    // presence of the bitop can change which instructions are truncated.
    for (size_t i = 0; i < bitops.length(); i++) {
        MBinaryBitwiseInstruction *ins = bitops[i];
        MDefinition *folded = ins->foldUnnecessaryBitop();
        if (folded != ins)
            ins->replaceAllUsesWith(folded);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// Collect Range information of operands
///////////////////////////////////////////////////////////////////////////////

void
MInArray::collectRangeInfoPreTrunc()
{
    Range indexRange(index());
    if (indexRange.isFiniteNonNegative())
        needsNegativeIntCheck_ = false;
}

void
MLoadElementHole::collectRangeInfoPreTrunc()
{
    Range indexRange(index());
    if (indexRange.isFiniteNonNegative())
        needsNegativeIntCheck_ = false;
}

void
MDiv::collectRangeInfoPreTrunc()
{
    Range lhsRange(lhs());
    if (lhsRange.isFiniteNonNegative())
        canBeNegativeDividend_ = false;
}

void
MMod::collectRangeInfoPreTrunc()
{
    Range lhsRange(lhs());
    if (lhsRange.isFiniteNonNegative())
        canBeNegativeDividend_ = false;
}

void
MBoundsCheckLower::collectRangeInfoPreTrunc()
{
    Range indexRange(index());
    if (indexRange.hasInt32LowerBound() && indexRange.lower() >= minimum_)
        fallible_ = false;
}

void
MCompare::collectRangeInfoPreTrunc()
{
    if (!Range(lhs()).canBeNaN() && !Range(rhs()).canBeNaN())
        operandsAreNeverNaN_ = true;
}

void
MNot::collectRangeInfoPreTrunc()
{
    if (!Range(operand()).canBeNaN())
        operandIsNeverNaN_ = true;
}

void
MPowHalf::collectRangeInfoPreTrunc()
{
    Range inputRange(input());
    if (!inputRange.canBeInfiniteOrNaN() || inputRange.hasInt32LowerBound())
        operandIsNeverNegativeInfinity_ = true;
    if (!inputRange.canBeZero())
        operandIsNeverNegativeZero_ = true;
    if (!inputRange.canBeNaN())
        operandIsNeverNaN_ = true;
}

void
MUrsh::collectRangeInfoPreTrunc()
{
    Range lhsRange(lhs()), rhsRange(rhs());

    // As in MUrsh::computeRange(), convert the inputs.
    lhsRange.wrapAroundToInt32();
    rhsRange.wrapAroundToShiftCount();

    // If the most significant bit of our result is always going to be zero,
    // we can optimize by disabling bailout checks for enforcing an int32 range.
    if (lhsRange.lower() >= 0 || rhsRange.lower() >= 1)
        bailoutsDisabled_ = true;
}

bool
RangeAnalysis::prepareForUCE(bool *shouldRemoveDeadCode)
{
    *shouldRemoveDeadCode = false;

    for (ReversePostorderIterator iter(graph_.rpoBegin()); iter != graph_.rpoEnd(); iter++) {
        MBasicBlock *block = *iter;

        if (!block->unreachable())
            continue;

        MControlInstruction *cond = block->getPredecessor(0)->lastIns();
        if (!cond->isTest())
            continue;

        // Replace the condition of the test control instruction by a constant
        // chosen based which of the successors has the unreachable flag which is
        // added by MBeta::computeRange on its own block.
        MTest *test = cond->toTest();
        MConstant *constant = nullptr;
        if (block == test->ifTrue()) {
            constant = MConstant::New(alloc(), BooleanValue(false));
        } else {
            JS_ASSERT(block == test->ifFalse());
            constant = MConstant::New(alloc(), BooleanValue(true));
        }
        test->block()->insertBefore(test, constant);
        test->replaceOperand(0, constant);
        IonSpew(IonSpew_Range, "Update condition of %d to reflect unreachable branches.",
                test->id());

        *shouldRemoveDeadCode = true;
    }

    return true;
}
