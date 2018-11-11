// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Deal with the differences between Microsoft and GNU implemenations
// of hash_map. Allows all platforms to use |base::hash_map| and
// |base::hash_set|.
//  eg:
//   base::hash_map<int> my_map;
//   base::hash_set<int> my_set;
//
// NOTE: It is an explicit non-goal of this class to provide a generic hash
// function for pointers.  If you want to hash a pointers to a particular class,
// please define the template specialization elsewhere (for example, in its
// header file) and keep it specific to just pointers to that class.  This is
// because identity hashes are not desirable for all types that might show up
// in containers as pointers.

#ifndef BASE_CONTAINERS_HASH_TABLES_H_
#define BASE_CONTAINERS_HASH_TABLES_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/strings/string16.h"
#include "build/build_config.h"

#if defined(COMPILER_MSVC)
#include <unordered_map>
#include <unordered_set>

#define BASE_HASH_NAMESPACE std

#elif defined(COMPILER_GCC)

#define BASE_HASH_NAMESPACE base_hash

// This is a hack to disable the gcc 4.4 warning about hash_map and hash_set
// being deprecated.  We can get rid of this when we upgrade to VS2008 and we
// can use <tr1/unordered_map> and <tr1/unordered_set>.
#ifdef __DEPRECATED
#define CHROME_OLD__DEPRECATED __DEPRECATED
#undef __DEPRECATED
#endif

#include <ext/hash_map>
#include <ext/hash_set>
#define BASE_HASH_IMPL_NAMESPACE __gnu_cxx

#include <string>

#ifdef CHROME_OLD__DEPRECATED
#define __DEPRECATED CHROME_OLD__DEPRECATED
#undef CHROME_OLD__DEPRECATED
#endif

namespace BASE_HASH_NAMESPACE {

// The pre-standard hash behaves like C++11's std::hash, except around pointers.
// const char* is specialized to hash the C string and hash functions for
// general T* are missing. Define a BASE_HASH_NAMESPACE::hash which aligns with
// the C++11 behavior.

template<typename T>
struct hash {
  std::size_t operator()(const T& value) const {
    return BASE_HASH_IMPL_NAMESPACE::hash<T>()(value);
  }
};

template<typename T>
struct hash<T*> {
  std::size_t operator()(T* value) const {
    return BASE_HASH_IMPL_NAMESPACE::hash<uintptr_t>()(
        reinterpret_cast<uintptr_t>(value));
  }
};

// The GNU C++ library provides identity hash functions for many integral types,
// but not for |long long|.  This hash function will truncate if |size_t| is
// narrower than |long long|.  This is probably good enough for what we will
// use it for.

#define DEFINE_TRIVIAL_HASH(integral_type) \
    template<> \
    struct hash<integral_type> { \
      std::size_t operator()(integral_type value) const { \
        return static_cast<std::size_t>(value); \
      } \
    }

DEFINE_TRIVIAL_HASH(long long);
DEFINE_TRIVIAL_HASH(unsigned long long);

#undef DEFINE_TRIVIAL_HASH

// Implement string hash functions so that strings of various flavors can
// be used as keys in STL maps and sets.  The hash algorithm comes from the
// GNU C++ library, in <tr1/functional>.  It is duplicated here because GCC
// versions prior to 4.3.2 are unable to compile <tr1/functional> when RTTI
// is disabled, as it is in our build.

#define DEFINE_STRING_HASH(string_type) \
    template<> \
    struct hash<string_type> { \
      std::size_t operator()(const string_type& s) const { \
        std::size_t result = 0; \
        for (string_type::const_iterator i = s.begin(); i != s.end(); ++i) \
          result = (result * 131) + *i; \
        return result; \
      } \
    }

DEFINE_STRING_HASH(std::string);
DEFINE_STRING_HASH(base::string16);

#undef DEFINE_STRING_HASH

}  // namespace BASE_HASH_NAMESPACE

#else  // COMPILER
#error define BASE_HASH_NAMESPACE for your compiler
#endif  // COMPILER

namespace base {

// On MSVC, use the C++11 containers.
#if defined(COMPILER_MSVC)

template<class Key, class T,
         class Hash = std::hash<Key>,
         class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<std::pair<const Key, T>>>
using hash_map = std::unordered_map<Key, T, Hash, Pred, Alloc>;

template<class Key, class T,
         class Hash = std::hash<Key>,
         class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<std::pair<const Key, T>>>
using hash_multimap = std::unordered_multimap<Key, T, Hash, Pred, Alloc>;

template<class Key,
         class Hash = std::hash<Key>,
         class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<Key>>
using hash_multiset = std::unordered_multiset<Key, Hash, Pred, Alloc>;

template<class Key,
         class Hash = std::hash<Key>,
         class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<Key>>
using hash_set = std::unordered_set<Key, Hash, Pred, Alloc>;

#else  // !COMPILER_MSVC

// Otherwise, use the pre-standard ones, but override the default hash to match
// C++11.
template<class Key, class T,
         class Hash = BASE_HASH_NAMESPACE::hash<Key>,
         class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<std::pair<const Key, T>>>
using hash_map = BASE_HASH_IMPL_NAMESPACE::hash_map<Key, T, Hash, Pred, Alloc>;

template<class Key, class T,
         class Hash = BASE_HASH_NAMESPACE::hash<Key>,
         class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<std::pair<const Key, T>>>
using hash_multimap =
    BASE_HASH_IMPL_NAMESPACE::hash_multimap<Key, T, Hash, Pred, Alloc>;

template<class Key,
         class Hash = BASE_HASH_NAMESPACE::hash<Key>,
         class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<Key>>
using hash_multiset =
    BASE_HASH_IMPL_NAMESPACE::hash_multiset<Key, Hash, Pred, Alloc>;

template<class Key,
         class Hash = BASE_HASH_NAMESPACE::hash<Key>,
         class Pred = std::equal_to<Key>,
         class Alloc = std::allocator<Key>>
using hash_set = BASE_HASH_IMPL_NAMESPACE::hash_set<Key, Hash, Pred, Alloc>;

#undef BASE_HASH_IMPL_NAMESPACE

#endif  // COMPILER_MSVC

// Implement hashing for pairs of at-most 32 bit integer values.
// When size_t is 32 bits, we turn the 64-bit hash code into 32 bits by using
// multiply-add hashing. This algorithm, as described in
// Theorem 4.3.3 of the thesis "Über die Komplexität der Multiplikation in
// eingeschränkten Branchingprogrammmodellen" by Woelfel, is:
//
//   h32(x32, y32) = (h64(x32, y32) * rand_odd64 + rand16 * 2^16) % 2^64 / 2^32
//
// Contact danakj@chromium.org for any questions.
inline std::size_t HashInts32(uint32_t value1, uint32_t value2) {
  uint64_t value1_64 = value1;
  uint64_t hash64 = (value1_64 << 32) | value2;

  if (sizeof(std::size_t) >= sizeof(uint64_t))
    return static_cast<std::size_t>(hash64);

  uint64_t odd_random = 481046412LL << 32 | 1025306955LL;
  uint32_t shift_random = 10121U << 16;

  hash64 = hash64 * odd_random + shift_random;
  std::size_t high_bits = static_cast<std::size_t>(
      hash64 >> (8 * (sizeof(uint64_t) - sizeof(std::size_t))));
  return high_bits;
}

// Implement hashing for pairs of up-to 64-bit integer values.
// We use the compound integer hash method to produce a 64-bit hash code, by
// breaking the two 64-bit inputs into 4 32-bit values:
// http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
// Then we reduce our result to 32 bits if required, similar to above.
inline std::size_t HashInts64(uint64_t value1, uint64_t value2) {
  uint32_t short_random1 = 842304669U;
  uint32_t short_random2 = 619063811U;
  uint32_t short_random3 = 937041849U;
  uint32_t short_random4 = 3309708029U;

  uint32_t value1a = static_cast<uint32_t>(value1 & 0xffffffff);
  uint32_t value1b = static_cast<uint32_t>((value1 >> 32) & 0xffffffff);
  uint32_t value2a = static_cast<uint32_t>(value2 & 0xffffffff);
  uint32_t value2b = static_cast<uint32_t>((value2 >> 32) & 0xffffffff);

  uint64_t product1 = static_cast<uint64_t>(value1a) * short_random1;
  uint64_t product2 = static_cast<uint64_t>(value1b) * short_random2;
  uint64_t product3 = static_cast<uint64_t>(value2a) * short_random3;
  uint64_t product4 = static_cast<uint64_t>(value2b) * short_random4;

  uint64_t hash64 = product1 + product2 + product3 + product4;

  if (sizeof(std::size_t) >= sizeof(uint64_t))
    return static_cast<std::size_t>(hash64);

  uint64_t odd_random = 1578233944LL << 32 | 194370989LL;
  uint32_t shift_random = 20591U << 16;

  hash64 = hash64 * odd_random + shift_random;
  std::size_t high_bits = static_cast<std::size_t>(
      hash64 >> (8 * (sizeof(uint64_t) - sizeof(std::size_t))));
  return high_bits;
}

template<typename T1, typename T2>
inline std::size_t HashPair(T1 value1, T2 value2) {
  // This condition is expected to be compile-time evaluated and optimised away
  // in release builds.
  if (sizeof(T1) > sizeof(uint32_t) || (sizeof(T2) > sizeof(uint32_t)))
    return HashInts64(value1, value2);

  return HashInts32(value1, value2);
}

}  // namespace base

namespace BASE_HASH_NAMESPACE {

// Implement methods for hashing a pair of integers, so they can be used as
// keys in STL containers.

template<typename Type1, typename Type2>
struct hash<std::pair<Type1, Type2> > {
  std::size_t operator()(std::pair<Type1, Type2> value) const {
    return base::HashPair(value.first, value.second);
  }
};

}  // namespace BASE_HASH_NAMESPACE

#undef DEFINE_PAIR_HASH_FUNCTION_START
#undef DEFINE_PAIR_HASH_FUNCTION_END

#endif  // BASE_CONTAINERS_HASH_TABLES_H_
