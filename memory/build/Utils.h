/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Utils_h
#define Utils_h

#include <cstring>
#include <type_traits>

#ifdef XP_WIN
#  include <io.h>  // for _write()
#endif

#include "mozilla/CheckedInt.h"
#include "mozilla/TemplateLib.h"

// Helper for log2 of powers of 2 at compile time.
template <size_t N>
struct Log2 : mozilla::tl::CeilingLog2<N> {
  using mozilla::tl::CeilingLog2<N>::value;
  static_assert(1ULL << value == N, "Number is not a power of 2");
};
#define LOG2(N) Log2<N>::value

enum class Order {
  eLess = -1,
  eEqual = 0,
  eGreater = 1,
};

// Compare two integers. Returns whether the first integer is Less,
// Equal or Greater than the second integer.
template <typename T>
Order CompareInt(T aValue1, T aValue2) {
  static_assert(std::is_integral_v<T>, "Type must be integral");
  if (aValue1 < aValue2) {
    return Order::eLess;
  }
  if (aValue1 > aValue2) {
    return Order::eGreater;
  }
  return Order::eEqual;
}

// Compare two addresses. Returns whether the first address is Less,
// Equal or Greater than the second address.
template <typename T>
Order CompareAddr(T* aAddr1, T* aAddr2) {
  return CompareInt(uintptr_t(aAddr1), uintptr_t(aAddr2));
}

// Helper for (fast) comparison of fractions without involving divisions or
// floats.
class Fraction {
 public:
  explicit constexpr Fraction(size_t aNumerator, size_t aDenominator)
      : mNumerator(aNumerator), mDenominator(aDenominator) {}

  MOZ_IMPLICIT constexpr Fraction(long double aValue)
      // We use an arbitrary power of two as denominator that provides enough
      // precision for our use case.
      : mNumerator(aValue * 4096), mDenominator(4096) {}

  inline bool operator<(const Fraction& aOther) const {
#ifndef MOZ_DEBUG
    // We are comparing A / B < C / D, with all A, B, C and D being positive
    // numbers. Multiplying both sides with B * D, we have:
    // (A * B * D) / B < (C * B * D) / D, which can then be simplified as
    // A * D < C * B. When can thus compare our fractions without actually
    // doing any division.
    // This however assumes the multiplied quantities are small enough not
    // to overflow the multiplication. We use CheckedInt on debug builds
    // to enforce the assumption.
    return mNumerator * aOther.mDenominator < aOther.mNumerator * mDenominator;
#else
    mozilla::CheckedInt<size_t> numerator(mNumerator);
    mozilla::CheckedInt<size_t> denominator(mDenominator);
    // value() asserts when the multiplication overflowed.
    size_t lhs = (numerator * aOther.mDenominator).value();
    size_t rhs = (aOther.mNumerator * denominator).value();
    return lhs < rhs;
#endif
  }

  inline bool operator>(const Fraction& aOther) const { return aOther < *this; }

  inline bool operator>=(const Fraction& aOther) const {
    return !(*this < aOther);
  }

  inline bool operator<=(const Fraction& aOther) const {
    return !(*this > aOther);
  }

  inline bool operator==(const Fraction& aOther) const {
#ifndef MOZ_DEBUG
    // Same logic as operator<
    return mNumerator * aOther.mDenominator == aOther.mNumerator * mDenominator;
#else
    mozilla::CheckedInt<size_t> numerator(mNumerator);
    mozilla::CheckedInt<size_t> denominator(mDenominator);
    size_t lhs = (numerator * aOther.mDenominator).value();
    size_t rhs = (aOther.mNumerator * denominator).value();
    return lhs == rhs;
#endif
  }

  inline bool operator!=(const Fraction& aOther) const {
    return !(*this == aOther);
  }

 private:
  size_t mNumerator;
  size_t mDenominator;
};

// Fast division
//
// During deallocation we want to divide by the size class.  This class
// provides a routine and sets up a constant as follows.
//
// To divide by a number D that is not a power of two we multiply by (2^17 /
// D) and then right shift by 17 positions.
//
//   X / D
//
// becomes
//
//   (X * m) >> p
//
// Where m is calculated during the FastDivisor constructor similarly to:
//
//   m = 2^p / D
//
template <typename T>
class FastDivisor {
 private:
  // The shift amount (p) is chosen to minimise the size of m while
  // working for divisors up to 65536 in steps of 16.  I arrived at 17
  // experimentally.  I wanted a low number to minimise the range of m
  // so it can fit in a uint16_t, 16 didn't work but 17 worked perfectly.
  //
  // We'd need to increase this if we allocated memory on smaller boundaries
  // than 16.
  static const unsigned p = 17;

  // We can fit the inverted divisor in 16 bits, but we template it here for
  // convenience.
  T m;

 public:
  // Needed so mBins can be constructed.
  FastDivisor() : m(0) {}

  FastDivisor(unsigned div, unsigned max) {
    MOZ_ASSERT(div <= max);

    // divide_inv_shift is large enough.
    MOZ_ASSERT((1U << p) >= div);

    // The calculation here for m is formula 26 from Section
    // 10-9 "Unsigned Division by Divisors >= 1" in
    // Henry S. Warren, Jr.'s Hacker's Delight, 2nd Ed.
    unsigned m_ = ((1U << p) + div - 1 - (((1U << p) - 1) % div)) / div;

    // Make sure that max * m does not overflow.
    MOZ_DIAGNOSTIC_ASSERT(max < UINT_MAX / m_);

    MOZ_ASSERT(m_ <= std::numeric_limits<T>::max());
    m = static_cast<T>(m_);

    // Initialisation made m non-zero.
    MOZ_ASSERT(m);

    // Test that all the divisions in the range we expected would work.
#ifdef MOZ_DEBUG
    for (unsigned num = 0; num < max; num += div) {
      MOZ_ASSERT(num / div == divide(num));
    }
#endif
  }

  // Note that this always occurs in uint32_t regardless of m's type.  If m is
  // a uint16_t it will be zero-extended before the multiplication.  We also use
  // uint32_t rather than something that could possibly be larger because it is
  // most-likely the cheapest multiplication.
  inline uint32_t divide(uint32_t num) const {
    // Check that m was initialised.
    MOZ_ASSERT(m);
    return (num * m) >> p;
  }
};

template <typename T>
unsigned inline operator/(unsigned num, FastDivisor<T> divisor) {
  return divisor.divide(num);
}

// Return the offset between a and the nearest aligned address at or below a.
#define ALIGNMENT_ADDR2OFFSET(a, alignment) \
  ((size_t)((uintptr_t)(a) & ((alignment) - 1)))

// Return the smallest alignment multiple that is >= s.
#define ALIGNMENT_CEILING(s, alignment) \
  (((s) + ((alignment) - 1)) & (~((alignment) - 1)))

static inline const char* _getprogname(void) { return "<jemalloc>"; }

#ifdef XP_WIN
#  define STDERR_FILENO 2
#else
#  define _write write
#endif
inline void _malloc_message(const char* p) {
  // Pretend to check _write() errors to suppress gcc warnings about
  // warn_unused_result annotations in some versions of glibc headers.
  if (_write(STDERR_FILENO, p, (unsigned int)strlen(p)) < 0) {
    return;
  }
}

template <typename... Args>
static void _malloc_message(const char* p, Args... args) {
  _malloc_message(p);
  _malloc_message(args...);
}

#endif
