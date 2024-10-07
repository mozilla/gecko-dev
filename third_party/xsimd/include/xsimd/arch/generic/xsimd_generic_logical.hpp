/***************************************************************************
 * Copyright (c) Johan Mabille, Sylvain Corlay, Wolf Vollprecht and         *
 * Martin Renou                                                             *
 * Copyright (c) QuantStack                                                 *
 * Copyright (c) Serge Guelton                                              *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#ifndef XSIMD_GENERIC_LOGICAL_HPP
#define XSIMD_GENERIC_LOGICAL_HPP

#include "./xsimd_generic_details.hpp"

#include <climits>

namespace xsimd
{

    namespace kernel
    {

        using namespace types;

        // count
        template <class A, class T>
        XSIMD_INLINE size_t count(batch_bool<T, A> const& self, requires_arch<generic>) noexcept
        {
            uint64_t m = self.mask();
            XSIMD_IF_CONSTEXPR(batch_bool<T, A>::size < 14)
            {
                // https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSet64
                return (m * 0x200040008001ULL & 0x111111111111111ULL) % 0xf;
            }
            else
            {
#if defined __has_builtin
#if __has_builtin(__builtin_popcountg)
#define builtin_popcount(v) __builtin_popcountg(v)
#endif
#endif

#ifdef builtin_popcount
                return builtin_popcount(m);
#else
                // FIXME: we could do better by dispatching to the appropriate
                // popcount instruction depending on the arch...
                XSIMD_IF_CONSTEXPR(batch_bool<T, A>::size <= 32)
                {
                    uint32_t m32 = m;
                    // https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
                    m32 = m32 - ((m32 >> 1) & 0x55555555); // reuse input as temporary
                    m32 = (m32 & 0x33333333) + ((m32 >> 2) & 0x33333333); // temp
                    return (((m32 + (m32 >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24; // count
                }
                else
                {
                    // https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
                    m = m - ((m >> 1) & (uint64_t) ~(uint64_t)0 / 3); // temp
                    m = (m & (uint64_t) ~(uint64_t)0 / 15 * 3) + ((m >> 2) & (uint64_t) ~(uint64_t)0 / 15 * 3); // temp
                    m = (m + (m >> 4)) & (uint64_t) ~(uint64_t)0 / 255 * 15; // temp
                    return (m * ((uint64_t) ~(uint64_t)0 / 255)) >> (sizeof(uint64_t) - 1) * CHAR_BIT; // count
                }
#endif
            }
        }

        // from  mask
        template <class A, class T>
        XSIMD_INLINE batch_bool<T, A> from_mask(batch_bool<T, A> const&, uint64_t mask, requires_arch<generic>) noexcept
        {
            alignas(A::alignment()) bool buffer[batch_bool<T, A>::size];
            // This is inefficient but should never be called. It's just a
            // temporary implementation until arm support is added.
            for (size_t i = 0; i < batch_bool<T, A>::size; ++i)
                buffer[i] = mask & (1ull << i);
            return batch_bool<T, A>::load_aligned(buffer);
        }

        // ge
        template <class A, class T>
        XSIMD_INLINE batch_bool<T, A> ge(batch<T, A> const& self, batch<T, A> const& other, requires_arch<generic>) noexcept
        {
            return other <= self;
        }

        // gt
        template <class A, class T>
        XSIMD_INLINE batch_bool<T, A> gt(batch<T, A> const& self, batch<T, A> const& other, requires_arch<generic>) noexcept
        {
            return other < self;
        }

        // is_even
        template <class A, class T>
        XSIMD_INLINE batch_bool<T, A> is_even(batch<T, A> const& self, requires_arch<generic>) noexcept
        {
            return is_flint(self * T(0.5));
        }

        // is_flint
        template <class A, class T>
        XSIMD_INLINE batch_bool<T, A> is_flint(batch<T, A> const& self, requires_arch<generic>) noexcept
        {
            auto frac = select(isnan(self - self), constants::nan<batch<T, A>>(), self - trunc(self));
            return frac == T(0.);
        }

        // is_odd
        template <class A, class T>
        XSIMD_INLINE batch_bool<T, A> is_odd(batch<T, A> const& self, requires_arch<generic>) noexcept
        {
            return is_even(self - T(1.));
        }

        // isinf
        template <class A, class T, class = typename std::enable_if<std::is_integral<T>::value, void>::type>
        XSIMD_INLINE batch_bool<T, A> isinf(batch<T, A> const&, requires_arch<generic>) noexcept
        {
            return batch_bool<T, A>(false);
        }
        template <class A>
        XSIMD_INLINE batch_bool<float, A> isinf(batch<float, A> const& self, requires_arch<generic>) noexcept
        {
            return abs(self) == std::numeric_limits<float>::infinity();
        }
        template <class A>
        XSIMD_INLINE batch_bool<double, A> isinf(batch<double, A> const& self, requires_arch<generic>) noexcept
        {
            return abs(self) == std::numeric_limits<double>::infinity();
        }

        // isfinite
        template <class A, class T, class = typename std::enable_if<std::is_integral<T>::value, void>::type>
        XSIMD_INLINE batch_bool<T, A> isfinite(batch<T, A> const&, requires_arch<generic>) noexcept
        {
            return batch_bool<T, A>(true);
        }
        template <class A>
        XSIMD_INLINE batch_bool<float, A> isfinite(batch<float, A> const& self, requires_arch<generic>) noexcept
        {
            return (self - self) == 0.f;
        }
        template <class A>
        XSIMD_INLINE batch_bool<double, A> isfinite(batch<double, A> const& self, requires_arch<generic>) noexcept
        {
            return (self - self) == 0.;
        }

        // isnan
        template <class A, class T, class = typename std::enable_if<std::is_integral<T>::value, void>::type>
        XSIMD_INLINE batch_bool<T, A> isnan(batch<T, A> const&, requires_arch<generic>) noexcept
        {
            return batch_bool<T, A>(false);
        }

        // le
        template <class A, class T, class = typename std::enable_if<std::is_integral<T>::value, void>::type>
        XSIMD_INLINE batch_bool<T, A> le(batch<T, A> const& self, batch<T, A> const& other, requires_arch<generic>) noexcept
        {
            return (self < other) || (self == other);
        }

        // neq
        template <class A, class T>
        XSIMD_INLINE batch_bool<T, A> neq(batch<T, A> const& self, batch<T, A> const& other, requires_arch<generic>) noexcept
        {
            return !(other == self);
        }

        // logical_and
        template <class A, class T>
        XSIMD_INLINE batch<T, A> logical_and(batch<T, A> const& self, batch<T, A> const& other, requires_arch<generic>) noexcept
        {
            return detail::apply([](T x, T y) noexcept
                                 { return x && y; },
                                 self, other);
        }

        // logical_or
        template <class A, class T>
        XSIMD_INLINE batch<T, A> logical_or(batch<T, A> const& self, batch<T, A> const& other, requires_arch<generic>) noexcept
        {
            return detail::apply([](T x, T y) noexcept
                                 { return x || y; },
                                 self, other);
        }

        // mask
        template <class A, class T>
        XSIMD_INLINE uint64_t mask(batch_bool<T, A> const& self, requires_arch<generic>) noexcept
        {
            alignas(A::alignment()) bool buffer[batch_bool<T, A>::size];
            self.store_aligned(buffer);
            // This is inefficient but should never be called. It's just a
            // temporary implementation until arm support is added.
            uint64_t res = 0;
            for (size_t i = 0; i < batch_bool<T, A>::size; ++i)
                if (buffer[i])
                    res |= 1ul << i;
            return res;
        }
    }
}

#endif
