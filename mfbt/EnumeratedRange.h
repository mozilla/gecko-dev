/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Iterator over contiguous enum values */

/*
 * Implements generator functions that create a range to iterate over the values
 * of a scoped or unscoped enum. Unlike IntegerRange, which can only function on
 * the underlying integral type, the elements of the generated sequence will
 * have the type of the enum in question.
 *
 * Note that the enum values should be contiguous in the iterated range;
 * unfortunately there exists no way for EnumeratedRange to enforce this
 * either dynamically or at compile time.
 */

#ifndef mozilla_EnumeratedRange_h
#define mozilla_EnumeratedRange_h

#include "mozilla/IntegerRange.h"
#include "mozilla/IntegerTypeTraits.h"

namespace mozilla {

namespace detail {

template<typename IntTypeT, typename EnumTypeT>
class EnumeratedIterator
{
public:
  typedef const EnumTypeT ValueType;
  typedef typename MakeSigned<IntTypeT>::Type DifferenceType;

  template<typename EnumType>
  explicit EnumeratedIterator(EnumType aCurrent)
    : mCurrent(aCurrent) { }

  template<typename IntType, typename EnumType>
  EnumeratedIterator(const EnumeratedIterator<IntType, EnumType>& aOther)
    : mCurrent(aOther.mCurrent) { }

  // Since operator* is required to return a reference, we return
  // a reference to our member here.
  const EnumTypeT& operator*() const { return mCurrent; }

  /* Increment and decrement operators */

  EnumeratedIterator& operator++()
  {
    mCurrent = EnumTypeT(IntTypeT(mCurrent) + IntTypeT(1));
    return *this;
  }
  EnumeratedIterator& operator--()
  {
    mCurrent = EnumTypeT(IntTypeT(mCurrent) - IntTypeT(1));
    return *this;
  }
  EnumeratedIterator operator++(int)
  {
    auto ret = *this;
    mCurrent = EnumTypeT(IntTypeT(mCurrent) + IntTypeT(1));
    return ret;
  }
  EnumeratedIterator operator--(int)
  {
    auto ret = *this;
    mCurrent = EnumTypeT(IntTypeT(mCurrent) - IntTypeT(1));
    return ret;
  }

  EnumeratedIterator operator+(DifferenceType aN) const
  {
    return EnumeratedIterator(EnumTypeT(IntTypeT(mCurrent) + aN));
  }
  EnumeratedIterator operator-(DifferenceType aN) const
  {
    return EnumeratedIterator(EnumTypeT(IntTypeT(mCurrent) - aN));
  }
  EnumeratedIterator& operator+=(DifferenceType aN)
  {
    mCurrent = EnumTypeT(IntTypeT(mCurrent) + aN);
    return *this;
  }
  EnumeratedIterator& operator-=(DifferenceType aN)
  {
    mCurrent = EnumTypeT(IntTypeT(mCurrent) - aN);
    return *this;
  }

  /* Comparison operators */

  template<typename IntType, typename EnumType>
  friend bool operator==(const EnumeratedIterator<IntType, EnumType>& aIter1,
                         const EnumeratedIterator<IntType, EnumType>& aIter2);
  template<typename IntType, typename EnumType>
  friend bool operator!=(const EnumeratedIterator<IntType, EnumType>& aIter1,
                         const EnumeratedIterator<IntType, EnumType>& aIter2);
  template<typename IntType, typename EnumType>
  friend bool operator<(const EnumeratedIterator<IntType, EnumType>& aIter1,
                        const EnumeratedIterator<IntType, EnumType>& aIter2);
  template<typename IntType, typename EnumType>
  friend bool operator<=(const EnumeratedIterator<IntType, EnumType>& aIter1,
                         const EnumeratedIterator<IntType, EnumType>& aIter2);
  template<typename IntType, typename EnumType>
  friend bool operator>(const EnumeratedIterator<IntType, EnumType>& aIter1,
                        const EnumeratedIterator<IntType, EnumType>& aIter2);
  template<typename IntType, typename EnumType>
  friend bool operator>=(const EnumeratedIterator<IntType, EnumType>& aIter1,
                         const EnumeratedIterator<IntType, EnumType>& aIter2);

private:
  EnumTypeT mCurrent;
};

template<typename IntType, typename EnumType>
bool operator==(const EnumeratedIterator<IntType, EnumType>& aIter1,
                const EnumeratedIterator<IntType, EnumType>& aIter2)
{
  return aIter1.mCurrent == aIter2.mCurrent;
}

template<typename IntType, typename EnumType>
bool operator!=(const EnumeratedIterator<IntType, EnumType>& aIter1,
                const EnumeratedIterator<IntType, EnumType>& aIter2)
{
  return aIter1.mCurrent != aIter2.mCurrent;
}

template<typename IntType, typename EnumType>
bool operator<(const EnumeratedIterator<IntType, EnumType>& aIter1,
               const EnumeratedIterator<IntType, EnumType>& aIter2)
{
  return aIter1.mCurrent < aIter2.mCurrent;
}

template<typename IntType, typename EnumType>
bool operator<=(const EnumeratedIterator<IntType, EnumType>& aIter1,
                const EnumeratedIterator<IntType, EnumType>& aIter2)
{
  return aIter1.mCurrent <= aIter2.mCurrent;
}

template<typename IntType, typename EnumType>
bool operator>(const EnumeratedIterator<IntType, EnumType>& aIter1,
               const EnumeratedIterator<IntType, EnumType>& aIter2)
{
  return aIter1.mCurrent > aIter2.mCurrent;
}

template<typename IntType, typename EnumType>
bool operator>=(const EnumeratedIterator<IntType, EnumType>& aIter1,
                const EnumeratedIterator<IntType, EnumType>& aIter2)
{
  return aIter1.mCurrent >= aIter2.mCurrent;
}

template<typename IntTypeT, typename EnumTypeT>
class EnumeratedRange
{
public:
  typedef EnumeratedIterator<IntTypeT, EnumTypeT> iterator;
  typedef EnumeratedIterator<IntTypeT, EnumTypeT> const_iterator;
  typedef ReverseIterator<iterator> reverse_iterator;
  typedef ReverseIterator<const_iterator> const_reverse_iterator;

  template<typename EnumType>
  EnumeratedRange(EnumType aBegin, EnumType aEnd)
    : mBegin(aBegin), mEnd(aEnd) { }

  iterator begin() const { return iterator(mBegin); }
  const_iterator cbegin() const { return begin(); }
  iterator end() const { return iterator(mEnd); }
  const_iterator cend() const { return end(); }
  reverse_iterator rbegin() const { return reverse_iterator(mEnd); }
  const_reverse_iterator crbegin() const { return rbegin(); }
  reverse_iterator rend() const { return reverse_iterator(mBegin); }
  const_reverse_iterator crend() const { return rend(); }

private:
  EnumTypeT mBegin;
  EnumTypeT mEnd;
};

} // namespace detail

#ifdef __GNUC__
// Enums can have an unsigned underlying type, which makes some of the
// comparisons below always true or always false. Temporarily disable
// -Wtype-limits to avoid breaking -Werror builds.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wtype-limits"
#endif

// Create a range to iterate from aBegin to aEnd, exclusive.
template<typename IntType, typename EnumType>
inline detail::EnumeratedRange<IntType, EnumType>
MakeEnumeratedRange(EnumType aBegin, EnumType aEnd)
{
#ifdef DEBUG
  typedef typename MakeUnsigned<IntType>::Type UnsignedType;
#endif
  static_assert(sizeof(IntType) >= sizeof(EnumType),
                "IntType should be at least as big as EnumType!");
  MOZ_ASSERT(aBegin <= aEnd, "Cannot generate invalid, unbounded range!");
  MOZ_ASSERT_IF(aBegin < EnumType(0), IsSigned<IntType>::value);
  MOZ_ASSERT_IF(aBegin >= EnumType(0) && IsSigned<IntType>::value,
                UnsignedType(aEnd) <= UnsignedType(MaxValue<IntType>::value));
  return detail::EnumeratedRange<IntType, EnumType>(aBegin, aEnd);
}

// Create a range to iterate from EnumType(0) to aEnd, exclusive. EnumType(0)
// should exist, but note that there is no way for us to ensure that it does!
template<typename IntType, typename EnumType>
inline detail::EnumeratedRange<IntType, EnumType>
MakeEnumeratedRange(EnumType aEnd)
{
  return MakeEnumeratedRange<IntType>(EnumType(0), aEnd);
}

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

} // namespace mozilla

#endif // mozilla_EnumeratedRange_h

