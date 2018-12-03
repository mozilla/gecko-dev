/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/BigIntType.h"

#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"
#include "mozilla/Range.h"
#include "mozilla/RangedPtr.h"

#include <gmp.h>
#include <math.h>

#include "jsapi.h"

#include "builtin/BigInt.h"
#include "gc/Allocator.h"
#include "gc/Tracer.h"
#include "js/Initialization.h"
#include "js/Utility.h"
#include "vm/JSContext.h"
#include "vm/SelfHosting.h"

#include "vm/JSContext-inl.h"

using namespace js;

using mozilla::Abs;
using mozilla::BitwiseCast;
using mozilla::CheckedInt;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Range;
using mozilla::RangedPtr;
using mozilla::Some;

// The following functions are wrappers for use with
// mp_set_memory_functions. GMP passes extra arguments to the realloc
// and free functions not needed by the JS allocation interface.
// js_malloc has the signature expected for GMP's malloc function, so no
// wrapper is required.

static void* js_mp_realloc(void* ptr, size_t old_size, size_t new_size) {
  return js_realloc(ptr, new_size);
}

static void js_mp_free(void* ptr, size_t size) { return js_free(ptr); }

static bool memoryFunctionsInitialized = false;

JS_PUBLIC_API void JS::SetGMPMemoryFunctions(JS::GMPAllocFn allocFn,
                                             JS::GMPReallocFn reallocFn,
                                             JS::GMPFreeFn freeFn) {
  MOZ_ASSERT(JS::detail::libraryInitState ==
             JS::detail::InitState::Uninitialized);
  memoryFunctionsInitialized = true;
  mp_set_memory_functions(allocFn, reallocFn, freeFn);
}

void BigInt::init() {
  // Don't override custom allocation functions if
  // JS::SetGMPMemoryFunctions was called.
  if (!memoryFunctionsInitialized) {
    memoryFunctionsInitialized = true;
    mp_set_memory_functions(js_malloc, js_mp_realloc, js_mp_free);
  }
}

BigInt* BigInt::create(JSContext* cx) {
  BigInt* x = Allocate<BigInt>(cx);
  if (!x) {
    return nullptr;
  }
  mpz_init(x->num_);  // to zero
  return x;
}

BigInt* BigInt::createFromDouble(JSContext* cx, double d) {
  BigInt* x = Allocate<BigInt>(cx);
  if (!x) {
    return nullptr;
  }
  mpz_init_set_d(x->num_, d);
  return x;
}

BigInt* BigInt::createFromBoolean(JSContext* cx, bool b) {
  BigInt* x = Allocate<BigInt>(cx);
  if (!x) {
    return nullptr;
  }
  mpz_init_set_ui(x->num_, b);
  return x;
}

BigInt* BigInt::createFromBytes(JSContext* cx, int sign, void* bytes,
                                size_t nbytes) {
  BigInt* x = Allocate<BigInt>(cx);
  if (!x) {
    return nullptr;
  }
  // Initialize num_ to zero before calling mpz_import.
  mpz_init(x->num_);

  if (nbytes == 0) {
    return x;
  }

  mpz_import(x->num_, nbytes,
             -1,  // order: least significant word first
             1,   // size: one byte per "word"
             0,   // endianness: native
             0,   // nail bits: none; use full words
             bytes);
  if (sign < 0) {
    mpz_neg(x->num_, x->num_);
  }
  return x;
}

BigInt* BigInt::createFromInt64(JSContext* cx, int64_t n) {
  BigInt* res = createFromUint64(cx, Abs(n));
  if (!res) {
    return nullptr;
  }

  if (n < 0) {
    mpz_neg(res->num_, res->num_);
  }

  return res;
}

BigInt* BigInt::createFromUint64(JSContext* cx, uint64_t n) {
  BigInt* res = create(cx);
  if (!res) {
    return nullptr;
  }

  // cf. mpz_import parameters in createFromBytes, above.
  mpz_import(res->num_, 1, 1, sizeof(uint64_t), 0, 0, &n);
  return res;
}

// BigInt proposal section 5.1.1
static bool IsInteger(double d) {
  // Step 1 is an assertion checked by the caller.
  // Step 2.
  if (!mozilla::IsFinite(d)) {
    return false;
  }

  // Step 3.
  double i = JS::ToInteger(d);

  // Step 4.
  if (i != d) {
    return false;
  }

  // Step 5.
  return true;
}

// BigInt proposal section 5.1.2
BigInt* js::NumberToBigInt(JSContext* cx, double d) {
  // Step 1 is an assertion checked by the caller.
  // Step 2.
  if (!IsInteger(d)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NUMBER_TO_BIGINT);
    return nullptr;
  }

  // Step 3.
  return BigInt::createFromDouble(cx, d);
}

BigInt* BigInt::copy(JSContext* cx, HandleBigInt x) {
  BigInt* bi = Allocate<BigInt>(cx);
  if (!bi) {
    return nullptr;
  }
  mpz_init_set(bi->num_, x->num_);
  return bi;
}

// BigInt proposal section 1.1.7
BigInt* BigInt::add(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }
  mpz_add(z->num_, x->num_, y->num_);
  return z;
}

// BigInt proposal section 1.1.8
BigInt* BigInt::sub(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }
  mpz_sub(z->num_, x->num_, y->num_);
  return z;
}

// BigInt proposal section 1.1.4
BigInt* BigInt::mul(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }
  mpz_mul(z->num_, x->num_, y->num_);
  return z;
}

// BigInt proposal section 1.1.5
BigInt* BigInt::div(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  // Step 1.
  if (mpz_size(y->num_) == 0) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BIGINT_DIVISION_BY_ZERO);
    return nullptr;
  }

  // Steps 2-3.
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }
  mpz_tdiv_q(z->num_, x->num_, y->num_);
  return z;
}

// BigInt proposal section 1.1.6
BigInt* BigInt::mod(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  // Step 1.
  if (mpz_size(y->num_) == 0) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BIGINT_DIVISION_BY_ZERO);
    return nullptr;
  }

  // Steps 2-4.
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }
  mpz_tdiv_r(z->num_, x->num_, y->num_);
  return z;
}

// BigInt proposal section 1.1.3
BigInt* BigInt::pow(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  // Step 1.
  if (mpz_sgn(y->num_) < 0) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BIGINT_NEGATIVE_EXPONENT);
    return nullptr;
  }

  // Throw a RangeError if the exponent is too large.
  if (!mpz_fits_uint_p(y->num_)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BIGINT_TOO_LARGE);
    return nullptr;
  }
  unsigned long int power = mpz_get_ui(y->num_);

  // Steps 2-3.
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }

  mpz_pow_ui(z->num_, x->num_, power);
  return z;
}

// BigInt proposal section 1.1.1
BigInt* BigInt::neg(JSContext* cx, HandleBigInt x) {
  BigInt* res = create(cx);
  if (!res) {
    return nullptr;
  }
  mpz_neg(res->num_, x->num_);
  return res;
}

// BigInt proposal section 1.1.9. BigInt::leftShift ( x, y )
BigInt* BigInt::lsh(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }

  // Step 1.
  if (mpz_sgn(y->num_) < 0) {
    mpz_fdiv_q_2exp(z->num_, x->num_, mpz_get_ui(y->num_));
    return z;
  }

  // Step 2.
  mpz_mul_2exp(z->num_, x->num_, mpz_get_ui(y->num_));
  return z;
}

// BigInt proposal section 1.1.10. BigInt::signedRightShift ( x, y )
BigInt* BigInt::rsh(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }

  // Step 1 of BigInt::leftShift(x, -y).
  if (mpz_sgn(y->num_) >= 0) {
    mpz_fdiv_q_2exp(z->num_, x->num_, mpz_get_ui(y->num_));
    return z;
  }

  // Step 2 of BigInt::leftShift(x, -y).
  mpz_mul_2exp(z->num_, x->num_, mpz_get_ui(y->num_));
  return z;
}

// BigInt proposal section 1.1.17. BigInt::bitwiseAND ( x, y )
BigInt* BigInt::bitAnd(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }
  mpz_and(z->num_, x->num_, y->num_);
  return z;
}

// BigInt proposal section 1.1.18. BigInt::bitwiseXOR ( x, y )
BigInt* BigInt::bitXor(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }
  mpz_xor(z->num_, x->num_, y->num_);
  return z;
}

// BigInt proposal section 1.1.19. BigInt::bitwiseOR ( x, y )
BigInt* BigInt::bitOr(JSContext* cx, HandleBigInt x, HandleBigInt y) {
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }
  mpz_ior(z->num_, x->num_, y->num_);
  return z;
}

// BigInt proposal section 1.1.2. BigInt::bitwiseNOT ( x )
BigInt* BigInt::bitNot(JSContext* cx, HandleBigInt x) {
  BigInt* z = create(cx);
  if (!z) {
    return nullptr;
  }
  mpz_neg(z->num_, x->num_);
  mpz_sub_ui(z->num_, z->num_, 1);
  return z;
}

int64_t BigInt::toInt64(BigInt* x) { return BitwiseCast<int64_t>(toUint64(x)); }

uint64_t BigInt::toUint64(BigInt* x) {
  static_assert(GMP_LIMB_BITS == 32 || GMP_LIMB_BITS == 64,
                "limbs must be either 32 or 64 bits");

  uint64_t digit;

  if (GMP_LIMB_BITS == 32) {
    uint64_t lo = mpz_getlimbn(x->num_, 0);
    uint64_t hi = mpz_getlimbn(x->num_, 1);
    digit = hi << 32 | lo;
  } else {
    digit = mpz_getlimbn(x->num_, 0);
  }

  // Return the two's complement if x is negative.
  if (mpz_sgn(x->num_) < 0) {
    return ~(digit - 1);
  }

  return digit;
}

BigInt* BigInt::asUintN(JSContext* cx, HandleBigInt x, uint64_t bits) {
  if (bits == 64) {
    return createFromUint64(cx, toUint64(x));
  }

  if (bits == 0) {
    return create(cx);
  }

  // Throw a RangeError if the bits argument is too large to represent using a
  // GMP bit count.
  CheckedInt<mp_bitcnt_t> bitCount = bits;
  if (!bitCount.isValid()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BIGINT_TOO_LARGE);
    return nullptr;
  }

  BigInt* res = create(cx);
  if (!res) {
    return nullptr;
  }

  mpz_fdiv_r_2exp(res->num_, x->num_, bitCount.value());
  return res;
}

BigInt* BigInt::asIntN(JSContext* cx, HandleBigInt x, uint64_t bits) {
  if (bits == 64) {
    return createFromInt64(cx, toInt64(x));
  }

  if (bits == 0) {
    return create(cx);
  }

  // Throw a RangeError if the bits argument is too large to represent using a
  // GMP bit count.
  CheckedInt<mp_bitcnt_t> bitCount = bits;
  if (!bitCount.isValid()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BIGINT_TOO_LARGE);
    return nullptr;
  }

  CheckedInt<mp_bitcnt_t> bitIndex = bitCount - 1;
  MOZ_ASSERT(bitIndex.isValid());

  BigInt* res = create(cx);
  if (!res) {
    return nullptr;
  }

  // Choose the rounding mode based on x's sign bit. mpz_tstbit will simulate
  // sign extension if the requested index is larger than the bit length of x.
  if (mpz_tstbit(x->num_, bitIndex.value())) {
    mpz_cdiv_r_2exp(res->num_, x->num_, bitCount.value());
  } else {
    mpz_fdiv_r_2exp(res->num_, x->num_, bitCount.value());
  }

  return res;
}

static bool ValidBigIntOperands(JSContext* cx, HandleValue lhs,
                                HandleValue rhs) {
  MOZ_ASSERT(lhs.isBigInt() || rhs.isBigInt());

  if (!lhs.isBigInt() || !rhs.isBigInt()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BIGINT_TO_NUMBER);
    return false;
  }

  return true;
}

bool BigInt::add(JSContext* cx, HandleValue lhs, HandleValue rhs,
                 MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::add(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::sub(JSContext* cx, HandleValue lhs, HandleValue rhs,
                 MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::sub(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::mul(JSContext* cx, HandleValue lhs, HandleValue rhs,
                 MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::mul(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::div(JSContext* cx, HandleValue lhs, HandleValue rhs,
                 MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::div(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::mod(JSContext* cx, HandleValue lhs, HandleValue rhs,
                 MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::mod(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::pow(JSContext* cx, HandleValue lhs, HandleValue rhs,
                 MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::pow(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::neg(JSContext* cx, HandleValue operand, MutableHandleValue res) {
  MOZ_ASSERT(operand.isBigInt());

  RootedBigInt operandBigInt(cx, operand.toBigInt());
  BigInt* resBigInt = BigInt::neg(cx, operandBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::lsh(JSContext* cx, HandleValue lhs, HandleValue rhs,
                 MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::lsh(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::rsh(JSContext* cx, HandleValue lhs, HandleValue rhs,
                 MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::rsh(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::bitAnd(JSContext* cx, HandleValue lhs, HandleValue rhs,
                    MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::bitAnd(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::bitXor(JSContext* cx, HandleValue lhs, HandleValue rhs,
                    MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::bitXor(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::bitOr(JSContext* cx, HandleValue lhs, HandleValue rhs,
                   MutableHandleValue res) {
  if (!ValidBigIntOperands(cx, lhs, rhs)) {
    return false;
  }

  RootedBigInt lhsBigInt(cx, lhs.toBigInt());
  RootedBigInt rhsBigInt(cx, rhs.toBigInt());
  BigInt* resBigInt = BigInt::bitOr(cx, lhsBigInt, rhsBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

bool BigInt::bitNot(JSContext* cx, HandleValue operand,
                    MutableHandleValue res) {
  MOZ_ASSERT(operand.isBigInt());

  RootedBigInt operandBigInt(cx, operand.toBigInt());
  BigInt* resBigInt = BigInt::bitNot(cx, operandBigInt);
  if (!resBigInt) {
    return false;
  }
  res.setBigInt(resBigInt);
  return true;
}

// BigInt proposal section 7.3
BigInt* js::ToBigInt(JSContext* cx, HandleValue val) {
  RootedValue v(cx, val);

  // Step 1.
  if (!ToPrimitive(cx, JSTYPE_NUMBER, &v)) {
    return nullptr;
  }

  // Step 2.
  if (v.isBigInt()) {
    return v.toBigInt();
  }

  if (v.isBoolean()) {
    return BigInt::createFromBoolean(cx, v.toBoolean());
  }

  if (v.isString()) {
    RootedString str(cx, v.toString());
    BigInt* bi;
    JS_TRY_VAR_OR_RETURN_NULL(cx, bi, StringToBigInt(cx, str, 0));
    if (!bi) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_BIGINT_INVALID_SYNTAX);
      return nullptr;
    }
    return bi;
  }

  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_BIGINT);
  return nullptr;
}

// ES 2019 draft 6.1.6
double BigInt::numberValue(BigInt* x) {
  // mpz_get_d may cause a hardware overflow trap, so use
  // mpz_get_d_2exp to get the fractional part and exponent
  // separately.
  signed long int exp;
  double d = mpz_get_d_2exp(&exp, x->num_);
  return ldexp(d, exp);
}

bool BigInt::equal(BigInt* lhs, BigInt* rhs) {
  if (lhs == rhs) {
    return true;
  }
  if (mpz_cmp(lhs->num_, rhs->num_) == 0) {
    return true;
  }
  return false;
}

bool BigInt::equal(BigInt* lhs, double rhs) {
  // The result of mpz_cmp_d is undefined for comparisons to NaN.
  if (mozilla::IsNaN(rhs)) {
    return false;
  }
  if (mpz_cmp_d(lhs->num_, rhs) == 0) {
    return true;
  }
  return false;
}

// BigInt proposal section 3.2.5
JS::Result<bool> BigInt::looselyEqual(JSContext* cx, HandleBigInt lhs,
                                      HandleValue rhs) {
  // Step 1.
  if (rhs.isBigInt()) {
    return equal(lhs, rhs.toBigInt());
  }

  // Steps 2-5 (not applicable).

  // Steps 6-7.
  if (rhs.isString()) {
    RootedBigInt rhsBigInt(cx);
    RootedString rhsString(cx, rhs.toString());
    MOZ_TRY_VAR(rhsBigInt, StringToBigInt(cx, rhsString, 0));
    if (!rhsBigInt) {
      return false;
    }
    return equal(lhs, rhsBigInt);
  }

  // Steps 8-9 (not applicable).

  // Steps 10-11.
  if (rhs.isObject()) {
    RootedValue rhsPrimitive(cx, rhs);
    if (!ToPrimitive(cx, &rhsPrimitive)) {
      return cx->alreadyReportedError();
    }
    return looselyEqual(cx, lhs, rhsPrimitive);
  }

  // Step 12.
  if (rhs.isNumber()) {
    return equal(lhs, rhs.toNumber());
  }

  // Step 13.
  return false;
}

// BigInt proposal section 1.1.12. BigInt::lessThan ( x, y )
bool BigInt::lessThan(BigInt* x, BigInt* y) {
  return mpz_cmp(x->num_, y->num_) < 0;
}

Maybe<bool> BigInt::lessThan(BigInt* lhs, double rhs) {
  if (mozilla::IsNaN(rhs)) {
    return Maybe<bool>(Nothing());
  }
  return Some(mpz_cmp_d(lhs->num_, rhs) < 0);
}

Maybe<bool> BigInt::lessThan(double lhs, BigInt* rhs) {
  if (mozilla::IsNaN(lhs)) {
    return Maybe<bool>(Nothing());
  }
  return Some(-mpz_cmp_d(rhs->num_, lhs) < 0);
}

bool BigInt::lessThan(JSContext* cx, HandleBigInt lhs, HandleString rhs,
                      Maybe<bool>& res) {
  RootedBigInt rhsBigInt(cx);
  JS_TRY_VAR_OR_RETURN_FALSE(cx, rhsBigInt, StringToBigInt(cx, rhs, 0));
  if (!rhsBigInt) {
    res = Nothing();
    return true;
  }
  res = Some(lessThan(lhs, rhsBigInt));
  return true;
}

bool BigInt::lessThan(JSContext* cx, HandleString lhs, HandleBigInt rhs,
                      Maybe<bool>& res) {
  RootedBigInt lhsBigInt(cx);
  JS_TRY_VAR_OR_RETURN_FALSE(cx, lhsBigInt, StringToBigInt(cx, lhs, 0));
  if (!lhsBigInt) {
    res = Nothing();
    return true;
  }
  res = Some(lessThan(lhsBigInt, rhs));
  return true;
}

bool BigInt::lessThan(JSContext* cx, HandleValue lhs, HandleValue rhs,
                      Maybe<bool>& res) {
  if (lhs.isBigInt()) {
    if (rhs.isString()) {
      RootedBigInt lhsBigInt(cx, lhs.toBigInt());
      RootedString rhsString(cx, rhs.toString());
      return lessThan(cx, lhsBigInt, rhsString, res);
    }

    if (rhs.isNumber()) {
      res = lessThan(lhs.toBigInt(), rhs.toNumber());
      return true;
    }

    MOZ_ASSERT(rhs.isBigInt());
    res = Some(lessThan(lhs.toBigInt(), rhs.toBigInt()));
    return true;
  }

  MOZ_ASSERT(rhs.isBigInt());
  if (lhs.isString()) {
    RootedString lhsString(cx, lhs.toString());
    RootedBigInt rhsBigInt(cx, rhs.toBigInt());
    return lessThan(cx, lhsString, rhsBigInt, res);
  }

  MOZ_ASSERT(lhs.isNumber());
  res = lessThan(lhs.toNumber(), rhs.toBigInt());
  return true;
}

JSLinearString* BigInt::toString(JSContext* cx, BigInt* x, uint8_t radix) {
  MOZ_ASSERT(2 <= radix && radix <= 36);
  // We need two extra chars for '\0' and potentially '-'.
  size_t strSize = mpz_sizeinbase(x->num_, 10) + 2;
  UniqueChars str(js_pod_malloc<char>(strSize));
  if (!str) {
    ReportOutOfMemory(cx);
    return nullptr;
  }
  mpz_get_str(str.get(), radix, x->num_);

  return NewStringCopyZ<CanGC>(cx, str.get());
}

// BigInt proposal section 7.2
template <typename CharT>
bool js::StringToBigIntImpl(const Range<const CharT>& chars, uint8_t radix,
                            HandleBigInt res) {
  const RangedPtr<const CharT> end = chars.end();
  RangedPtr<const CharT> s = chars.begin();
  Maybe<int8_t> sign;

  s = SkipSpace(s.get(), end.get());

  if (s != end && s[0] == '+') {
    sign.emplace(1);
    s++;
  } else if (s != end && s[0] == '-') {
    sign.emplace(-1);
    s++;
  }

  if (!radix) {
    radix = 10;

    if (end - s >= 2 && s[0] == '0') {
      if (s[1] == 'x' || s[1] == 'X') {
        radix = 16;
        s += 2;
      } else if (s[1] == 'o' || s[1] == 'O') {
        radix = 8;
        s += 2;
      } else if (s[1] == 'b' || s[1] == 'B') {
        radix = 2;
        s += 2;
      }

      if (radix != 10 && s == end) {
        return false;
      }
    }
  }

  if (sign && radix != 10) {
    return false;
  }

  mpz_set_ui(res->num_, 0);

  for (; s < end; s++) {
    unsigned digit;
    if (!mozilla::IsAsciiAlphanumeric(s[0])) {
      s = SkipSpace(s.get(), end.get());
      if (s == end) {
        break;
      }
      return false;
    }
    digit = mozilla::AsciiAlphanumericToNumber(s[0]);
    if (digit >= radix) {
      return false;
    }
    mpz_mul_ui(res->num_, res->num_, radix);
    mpz_add_ui(res->num_, res->num_, digit);
  }

  if (sign.valueOr(1) < 0) {
    mpz_neg(res->num_, res->num_);
  }

  return true;
}

JS::Result<BigInt*, JS::OOM&> js::StringToBigInt(JSContext* cx,
                                                 HandleString str,
                                                 uint8_t radix) {
  RootedBigInt res(cx, BigInt::create(cx));
  if (!res) {
    return cx->alreadyReportedOOM();
  }

  JSLinearString* linear = str->ensureLinear(cx);
  if (!linear) {
    return cx->alreadyReportedOOM();
  }

  {
    JS::AutoCheckCannotGC nogc;
    if (linear->hasLatin1Chars()) {
      if (StringToBigIntImpl(linear->latin1Range(nogc), radix, res)) {
        return res.get();
      }
    } else {
      if (StringToBigIntImpl(linear->twoByteRange(nogc), radix, res)) {
        return res.get();
      }
    }
  }

  return nullptr;
}

BigInt* js::StringToBigInt(JSContext* cx, const Range<const char16_t>& chars) {
  RootedBigInt res(cx, BigInt::create(cx));
  if (!res) {
    return nullptr;
  }

  uint8_t radix = 0;
  if (StringToBigIntImpl(chars, radix, res)) {
    return res.get();
  }

  return nullptr;
}

size_t BigInt::byteLength(BigInt* x) {
  if (mpz_sgn(x->num_) == 0) {
    return 0;
  }
  return JS_HOWMANY(mpz_sizeinbase(x->num_, 2), 8);
}

void BigInt::writeBytes(BigInt* x, RangedPtr<uint8_t> buffer) {
#ifdef DEBUG
  // Check that the buffer being filled is large enough to hold the
  // integer we're writing. The result of the RangedPtr addition is
  // restricted to the buffer's range.
  size_t reprSize = byteLength(x);
  MOZ_ASSERT(buffer + reprSize, "out of bounds access to buffer");
#endif

  size_t count;
  // cf. mpz_import parameters in createFromBytes, above.
  mpz_export(buffer.get(), &count, -1, 1, 0, 0, x->num_);
  MOZ_ASSERT(count == reprSize);
}

void BigInt::finalize(js::FreeOp* fop) { mpz_clear(num_); }

JSAtom* js::BigIntToAtom(JSContext* cx, BigInt* bi) {
  JSString* str = BigInt::toString(cx, bi, 10);
  if (!str) {
    return nullptr;
  }
  return AtomizeString(cx, str);
}

bool BigInt::toBoolean() { return mpz_sgn(num_) != 0; }

int8_t BigInt::sign() { return mpz_sgn(num_); }

js::HashNumber BigInt::hash() {
  const mp_limb_t* limbs = mpz_limbs_read(num_);
  size_t limbCount = mpz_size(num_);
  uint32_t hash = mozilla::HashBytes(limbs, limbCount * sizeof(mp_limb_t));
  hash = mozilla::AddToHash(hash, mpz_sgn(num_));
  return hash;
}

size_t BigInt::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
  // Use the total number of limbs allocated when calculating the size
  // (_mp_alloc), not the number of limbs currently in use (_mp_size).
  // See the Info node `(gmp)Integer Internals` for details.
  mpz_srcptr n = static_cast<mpz_srcptr>(num_);
  return sizeof(*n) + sizeof(mp_limb_t) * n->_mp_alloc;
}

JS::ubi::Node::Size JS::ubi::Concrete<BigInt>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  BigInt& bi = get();
  MOZ_ASSERT(bi.isTenured());
  size_t size = js::gc::Arena::thingSize(bi.asTenured().getAllocKind());
  size += bi.sizeOfExcludingThis(mallocSizeOf);
  return size;
}

template <XDRMode mode>
XDRResult js::XDRBigInt(XDRState<mode>* xdr, MutableHandleBigInt bi) {
  JSContext* cx = xdr->cx();

  uint8_t sign;
  uint32_t length;

  if (mode == XDR_ENCODE) {
    cx->check(bi);
    sign = static_cast<uint8_t>(bi->sign());
    uint64_t sz = BigInt::byteLength(bi);
    // As the maximum source code size is currently UINT32_MAX code units
    // (see BytecodeCompiler::checkLength), any bigint literal's length in
    // word-sized digits will be less than UINT32_MAX as well.  That could
    // change or FoldConstants could start creating these though, so leave
    // this as a release-enabled assert.
    MOZ_RELEASE_ASSERT(sz <= UINT32_MAX);
    length = static_cast<uint32_t>(sz);
  }

  MOZ_TRY(xdr->codeUint8(&sign));
  MOZ_TRY(xdr->codeUint32(&length));

  UniquePtr<uint8_t> buf(cx->pod_malloc<uint8_t>(length));
  if (!buf) {
    ReportOutOfMemory(cx);
    return xdr->fail(JS::TranscodeResult_Throw);
  }

  if (mode == XDR_ENCODE) {
    BigInt::writeBytes(bi, RangedPtr<uint8_t>(buf.get(), length));
  }

  MOZ_TRY(xdr->codeBytes(buf.get(), length));

  if (mode == XDR_DECODE) {
    BigInt* res = BigInt::createFromBytes(cx, static_cast<int8_t>(sign),
                                          buf.get(), length);
    if (!res) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    bi.set(res);
  }

  return Ok();
}

template XDRResult js::XDRBigInt(XDRState<XDR_ENCODE>* xdr,
                                 MutableHandleBigInt bi);

template XDRResult js::XDRBigInt(XDRState<XDR_DECODE>* xdr,
                                 MutableHandleBigInt bi);
