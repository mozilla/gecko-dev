/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A class for optional values and in-place lazy construction. */

#ifndef mozilla_Maybe_h
#define mozilla_Maybe_h

#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/Move.h"
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/Poison.h"
#include "mozilla/TypeTraits.h"

#include <new>  // for placement new
#include <ostream>
#include <type_traits>

namespace mozilla {

struct Nothing {};

namespace detail {

// You would think that poisoning Maybe instances could just be a call
// to mozWritePoison.  Unfortunately, using a simple call to
// mozWritePoison generates poor code on MSVC for small structures.  The
// generated code contains (always not-taken) branches and does a bunch
// of setup for `rep stos{l,q}`, even though we know at compile time
// exactly how many words we're poisoning.  Instead, we're going to
// force MSVC to generate the code we want via recursive templates.

// Write the given poisonValue into p at offset*sizeof(uintptr_t).
template <size_t offset>
inline void WritePoisonAtOffset(void* p, const uintptr_t poisonValue) {
  memcpy(static_cast<char*>(p) + offset * sizeof(poisonValue), &poisonValue,
         sizeof(poisonValue));
}

template <size_t Offset, size_t NOffsets>
struct InlinePoisoner {
  static void poison(void* p, const uintptr_t poisonValue) {
    WritePoisonAtOffset<Offset>(p, poisonValue);
    InlinePoisoner<Offset + 1, NOffsets>::poison(p, poisonValue);
  }
};

template <size_t N>
struct InlinePoisoner<N, N> {
  static void poison(void*, const uintptr_t) {
    // All done!
  }
};

// We can't generate inline code for large structures, though, because we'll
// blow out recursive template instantiation limits, and the code would be
// bloated to boot.  So provide a fallback to the out-of-line poisoner.
template <size_t ObjectSize>
struct OutOfLinePoisoner {
  static void poison(void* p, const uintptr_t) {
    mozWritePoison(p, ObjectSize);
  }
};

template <typename T>
inline void PoisonObject(T* p) {
  const uintptr_t POISON = mozPoisonValue();
  Conditional<(sizeof(T) <= 8 * sizeof(POISON)),
              InlinePoisoner<0, sizeof(T) / sizeof(POISON)>,
              OutOfLinePoisoner<sizeof(T)>>::Type::poison(p, POISON);
}

template <typename T>
struct MaybePoisoner {
  static const size_t N = sizeof(T);

  static void poison(void* aPtr) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    if (N >= sizeof(uintptr_t)) {
      PoisonObject(static_cast<typename RemoveCV<T>::Type*>(aPtr));
    }
#endif
    MOZ_MAKE_MEM_UNDEFINED(aPtr, N);
  }
};

}  // namespace detail

/*
 * Maybe is a container class which contains either zero or one elements. It
 * serves two roles. It can represent values which are *semantically* optional,
 * augmenting a type with an explicit 'Nothing' value. In this role, it provides
 * methods that make it easy to work with values that may be missing, along with
 * equality and comparison operators so that Maybe values can be stored in
 * containers. Maybe values can be constructed conveniently in expressions using
 * type inference, as follows:
 *
 *   void doSomething(Maybe<Foo> aFoo) {
 *     if (aFoo)                  // Make sure that aFoo contains a value...
 *       aFoo->takeAction();      // and then use |aFoo->| to access it.
 *   }                            // |*aFoo| also works!
 *
 *   doSomething(Nothing());      // Passes a Maybe<Foo> containing no value.
 *   doSomething(Some(Foo(100))); // Passes a Maybe<Foo> containing |Foo(100)|.
 *
 * You'll note that it's important to check whether a Maybe contains a value
 * before using it, using conversion to bool, |isSome()|, or |isNothing()|. You
 * can avoid these checks, and sometimes write more readable code, using
 * |valueOr()|, |ptrOr()|, and |refOr()|, which allow you to retrieve the value
 * in the Maybe and provide a default for the 'Nothing' case.  You can also use
 * |apply()| to call a function only if the Maybe holds a value, and |map()| to
 * transform the value in the Maybe, returning another Maybe with a possibly
 * different type.
 *
 * Maybe's other role is to support lazily constructing objects without using
 * dynamic storage. A Maybe directly contains storage for a value, but it's
 * empty by default. |emplace()|, as mentioned above, can be used to construct a
 * value in Maybe's storage.  The value a Maybe contains can be destroyed by
 * calling |reset()|; this will happen automatically if a Maybe is destroyed
 * while holding a value.
 *
 * It's a common idiom in C++ to use a pointer as a 'Maybe' type, with a null
 * value meaning 'Nothing' and any other value meaning 'Some'. You can convert
 * from such a pointer to a Maybe value using 'ToMaybe()'.
 *
 * Maybe is inspired by similar types in the standard library of many other
 * languages (e.g. Haskell's Maybe and Rust's Option). In the C++ world it's
 * very similar to std::optional, which was proposed for C++14 and originated in
 * Boost. The most important differences between Maybe and std::optional are:
 *
 *   - std::optional<T> may be compared with T. We deliberately forbid that.
 *   - std::optional allows in-place construction without a separate call to
 *     |emplace()| by using a dummy |in_place_t| value to tag the appropriate
 *     constructor.
 *   - std::optional has |valueOr()|, equivalent to Maybe's |valueOr()|, but
 *     lacks corresponding methods for |refOr()| and |ptrOr()|.
 *   - std::optional lacks |map()| and |apply()|, making it less suitable for
 *     functional-style code.
 *   - std::optional lacks many convenience functions that Maybe has. Most
 *     unfortunately, it lacks equivalents of the type-inferred constructor
 *     functions |Some()| and |Nothing()|.
 */
template <class T>
class MOZ_NON_PARAM MOZ_INHERIT_TYPE_ANNOTATIONS_FROM_TEMPLATE_ARGS Maybe {
  MOZ_ALIGNAS_IN_STRUCT(T) unsigned char mStorage[sizeof(T)];
  char mIsSome;  // not bool -- guarantees minimal space consumption

  // GCC fails due to -Werror=strict-aliasing if |mStorage| is directly cast to
  // T*.  Indirecting through these functions addresses the problem.
  void* data() { return mStorage; }
  const void* data() const { return mStorage; }

  void poisonData() { detail::MaybePoisoner<T>::poison(data()); }

 public:
  using ValueType = T;

  MOZ_ALLOW_TEMPORARY Maybe() : mIsSome(false) {}
  ~Maybe() { reset(); }

  MOZ_ALLOW_TEMPORARY MOZ_IMPLICIT Maybe(Nothing) : mIsSome(false) {}

  Maybe(const Maybe& aOther) : mIsSome(false) {
    if (aOther.mIsSome) {
      emplace(*aOther);
    }
  }

  /**
   * Maybe<T> can be copy-constructed from a Maybe<U> if U is convertible to T.
   */
  template <typename U, typename = typename std::enable_if<
                            std::is_convertible<U, T>::value>::type>
  MOZ_IMPLICIT Maybe(const Maybe<U>& aOther) : mIsSome(false) {
    if (aOther.isSome()) {
      emplace(*aOther);
    }
  }

  Maybe(Maybe&& aOther) : mIsSome(false) {
    if (aOther.mIsSome) {
      emplace(std::move(*aOther));
      aOther.reset();
    }
  }

  /**
   * Maybe<T> can be move-constructed from a Maybe<U> if U is convertible to T.
   */
  template <typename U, typename = typename std::enable_if<
                            std::is_convertible<U, T>::value>::type>
  MOZ_IMPLICIT Maybe(Maybe<U>&& aOther) : mIsSome(false) {
    if (aOther.isSome()) {
      emplace(std::move(*aOther));
      aOther.reset();
    }
  }

  Maybe& operator=(const Maybe& aOther) {
    if (&aOther != this) {
      if (aOther.mIsSome) {
        if (mIsSome) {
          ref() = aOther.ref();
        } else {
          emplace(*aOther);
        }
      } else {
        reset();
      }
    }
    return *this;
  }

  template <typename U, typename = typename std::enable_if<
                            std::is_convertible<U, T>::value>::type>
  Maybe& operator=(const Maybe<U>& aOther) {
    if (aOther.isSome()) {
      if (mIsSome) {
        ref() = aOther.ref();
      } else {
        emplace(*aOther);
      }
    } else {
      reset();
    }
    return *this;
  }

  Maybe& operator=(Maybe&& aOther) {
    MOZ_ASSERT(this != &aOther, "Self-moves are prohibited");

    if (aOther.mIsSome) {
      if (mIsSome) {
        ref() = std::move(aOther.ref());
      } else {
        emplace(std::move(*aOther));
      }
      aOther.reset();
    } else {
      reset();
    }

    return *this;
  }

  template <typename U, typename = typename std::enable_if<
                            std::is_convertible<U, T>::value>::type>
  Maybe& operator=(Maybe<U>&& aOther) {
    if (aOther.isSome()) {
      if (mIsSome) {
        ref() = std::move(aOther.ref());
      } else {
        emplace(std::move(*aOther));
      }
      aOther.reset();
    } else {
      reset();
    }

    return *this;
  }

  /* Methods that check whether this Maybe contains a value */
  explicit operator bool() const { return isSome(); }
  bool isSome() const { return mIsSome; }
  bool isNothing() const { return !mIsSome; }

  /* Returns the contents of this Maybe<T> by value. Unsafe unless |isSome()|.
   */
  T value() const;

  /*
   * Returns the contents of this Maybe<T> by value. If |isNothing()|, returns
   * the default value provided.
   */
  template <typename V>
  T valueOr(V&& aDefault) const {
    if (isSome()) {
      return ref();
    }
    return std::forward<V>(aDefault);
  }

  /*
   * Returns the contents of this Maybe<T> by value. If |isNothing()|, returns
   * the value returned from the function or functor provided.
   */
  template <typename F>
  T valueOrFrom(F&& aFunc) const {
    if (isSome()) {
      return ref();
    }
    return aFunc();
  }

  /* Returns the contents of this Maybe<T> by pointer. Unsafe unless |isSome()|.
   */
  T* ptr();
  const T* ptr() const;

  /*
   * Returns the contents of this Maybe<T> by pointer. If |isNothing()|,
   * returns the default value provided.
   */
  T* ptrOr(T* aDefault) {
    if (isSome()) {
      return ptr();
    }
    return aDefault;
  }

  const T* ptrOr(const T* aDefault) const {
    if (isSome()) {
      return ptr();
    }
    return aDefault;
  }

  /*
   * Returns the contents of this Maybe<T> by pointer. If |isNothing()|,
   * returns the value returned from the function or functor provided.
   */
  template <typename F>
  T* ptrOrFrom(F&& aFunc) {
    if (isSome()) {
      return ptr();
    }
    return aFunc();
  }

  template <typename F>
  const T* ptrOrFrom(F&& aFunc) const {
    if (isSome()) {
      return ptr();
    }
    return aFunc();
  }

  T* operator->();
  const T* operator->() const;

  /* Returns the contents of this Maybe<T> by ref. Unsafe unless |isSome()|. */
  T& ref();
  const T& ref() const;

  /*
   * Returns the contents of this Maybe<T> by ref. If |isNothing()|, returns
   * the default value provided.
   */
  T& refOr(T& aDefault) {
    if (isSome()) {
      return ref();
    }
    return aDefault;
  }

  const T& refOr(const T& aDefault) const {
    if (isSome()) {
      return ref();
    }
    return aDefault;
  }

  /*
   * Returns the contents of this Maybe<T> by ref. If |isNothing()|, returns the
   * value returned from the function or functor provided.
   */
  template <typename F>
  T& refOrFrom(F&& aFunc) {
    if (isSome()) {
      return ref();
    }
    return aFunc();
  }

  template <typename F>
  const T& refOrFrom(F&& aFunc) const {
    if (isSome()) {
      return ref();
    }
    return aFunc();
  }

  T& operator*();
  const T& operator*() const;

  /* If |isSome()|, runs the provided function or functor on the contents of
   * this Maybe. */
  template <typename Func>
  Maybe& apply(Func&& aFunc) {
    if (isSome()) {
      std::forward<Func>(aFunc)(ref());
    }
    return *this;
  }

  template <typename Func>
  const Maybe& apply(Func&& aFunc) const {
    if (isSome()) {
      std::forward<Func>(aFunc)(ref());
    }
    return *this;
  }

  /*
   * If |isSome()|, runs the provided function and returns the result wrapped
   * in a Maybe. If |isNothing()|, returns an empty Maybe value with the same
   * value type as what the provided function would have returned.
   */
  template <typename Func>
  auto map(Func&& aFunc) {
    Maybe<decltype(std::forward<Func>(aFunc)(ref()))> val;
    if (isSome()) {
      val.emplace(std::forward<Func>(aFunc)(ref()));
    }
    return val;
  }

  template <typename Func>
  auto map(Func&& aFunc) const {
    Maybe<decltype(std::forward<Func>(aFunc)(ref()))> val;
    if (isSome()) {
      val.emplace(std::forward<Func>(aFunc)(ref()));
    }
    return val;
  }

  /* If |isSome()|, empties this Maybe and destroys its contents. */
  void reset() {
    if (isSome()) {
      ref().T::~T();
      mIsSome = false;
      poisonData();
    }
  }

  /*
   * Constructs a T value in-place in this empty Maybe<T>'s storage. The
   * arguments to |emplace()| are the parameters to T's constructor.
   */
  template <typename... Args>
  void emplace(Args&&... aArgs);

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const Maybe<T>& aMaybe) {
    if (aMaybe) {
      aStream << aMaybe.ref();
    } else {
      aStream << "<Nothing>";
    }
    return aStream;
  }
};

template <typename T>
T Maybe<T>::value() const {
  MOZ_DIAGNOSTIC_ASSERT(mIsSome);
  return ref();
}

template <typename T>
T* Maybe<T>::ptr() {
  MOZ_DIAGNOSTIC_ASSERT(mIsSome);
  return &ref();
}

template <typename T>
const T* Maybe<T>::ptr() const {
  MOZ_DIAGNOSTIC_ASSERT(mIsSome);
  return &ref();
}

template <typename T>
T* Maybe<T>::operator->() {
  MOZ_DIAGNOSTIC_ASSERT(mIsSome);
  return ptr();
}

template <typename T>
const T* Maybe<T>::operator->() const {
  MOZ_DIAGNOSTIC_ASSERT(mIsSome);
  return ptr();
}

template <typename T>
T& Maybe<T>::ref() {
  MOZ_DIAGNOSTIC_ASSERT(mIsSome);
  return *static_cast<T*>(data());
}

template <typename T>
const T& Maybe<T>::ref() const {
  MOZ_DIAGNOSTIC_ASSERT(mIsSome);
  return *static_cast<const T*>(data());
}

template <typename T>
T& Maybe<T>::operator*() {
  MOZ_DIAGNOSTIC_ASSERT(mIsSome);
  return ref();
}

template <typename T>
const T& Maybe<T>::operator*() const {
  MOZ_DIAGNOSTIC_ASSERT(mIsSome);
  return ref();
}

template <typename T>
template <typename... Args>
void Maybe<T>::emplace(Args&&... aArgs) {
  MOZ_DIAGNOSTIC_ASSERT(!mIsSome);
  ::new (KnownNotNull, data()) T(std::forward<Args>(aArgs)...);
  mIsSome = true;
}

/*
 * Some() creates a Maybe<T> value containing the provided T value. If T has a
 * move constructor, it's used to make this as efficient as possible.
 *
 * Some() selects the type of Maybe it returns by removing any const, volatile,
 * or reference qualifiers from the type of the value you pass to it. This gives
 * it more intuitive behavior when used in expressions, but it also means that
 * if you need to construct a Maybe value that holds a const, volatile, or
 * reference value, you need to use emplace() instead.
 */
template <typename T, typename U = typename std::remove_cv<
                          typename std::remove_reference<T>::type>::type>
Maybe<U> Some(T&& aValue) {
  Maybe<U> value;
  value.emplace(std::forward<T>(aValue));
  return value;
}

template <typename T>
Maybe<typename RemoveCV<typename RemoveReference<T>::Type>::Type> ToMaybe(
    T* aPtr) {
  if (aPtr) {
    return Some(*aPtr);
  }
  return Nothing();
}

/*
 * Two Maybe<T> values are equal if
 * - both are Nothing, or
 * - both are Some, and the values they contain are equal.
 */
template <typename T>
bool operator==(const Maybe<T>& aLHS, const Maybe<T>& aRHS) {
  if (aLHS.isNothing() != aRHS.isNothing()) {
    return false;
  }
  return aLHS.isNothing() || *aLHS == *aRHS;
}

template <typename T>
bool operator!=(const Maybe<T>& aLHS, const Maybe<T>& aRHS) {
  return !(aLHS == aRHS);
}

/*
 * We support comparison to Nothing to allow reasonable expressions like:
 *   if (maybeValue == Nothing()) { ... }
 */
template <typename T>
bool operator==(const Maybe<T>& aLHS, const Nothing& aRHS) {
  return aLHS.isNothing();
}

template <typename T>
bool operator!=(const Maybe<T>& aLHS, const Nothing& aRHS) {
  return !(aLHS == aRHS);
}

template <typename T>
bool operator==(const Nothing& aLHS, const Maybe<T>& aRHS) {
  return aRHS.isNothing();
}

template <typename T>
bool operator!=(const Nothing& aLHS, const Maybe<T>& aRHS) {
  return !(aLHS == aRHS);
}

/*
 * Maybe<T> values are ordered in the same way T values are ordered, except that
 * Nothing comes before anything else.
 */
template <typename T>
bool operator<(const Maybe<T>& aLHS, const Maybe<T>& aRHS) {
  if (aLHS.isNothing()) {
    return aRHS.isSome();
  }
  if (aRHS.isNothing()) {
    return false;
  }
  return *aLHS < *aRHS;
}

template <typename T>
bool operator>(const Maybe<T>& aLHS, const Maybe<T>& aRHS) {
  return !(aLHS < aRHS || aLHS == aRHS);
}

template <typename T>
bool operator<=(const Maybe<T>& aLHS, const Maybe<T>& aRHS) {
  return aLHS < aRHS || aLHS == aRHS;
}

template <typename T>
bool operator>=(const Maybe<T>& aLHS, const Maybe<T>& aRHS) {
  return !(aLHS < aRHS);
}

}  // namespace mozilla

#endif /* mozilla_Maybe_h */
