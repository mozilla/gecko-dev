/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_MOZPROMISEUTILS_H_
#define DOM_QUOTA_MOZPROMISEUTILS_H_

#include "mozilla/MozPromise.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::quota {

namespace detail {

template <typename T>
struct IsExclusiveMozPromise {
  static constexpr bool value = false;
};

// Specialization for MozPromise
template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
struct IsExclusiveMozPromise<
    MozPromise<ResolveValueT, RejectValueT, IsExclusive>> {
  static constexpr bool value = IsExclusive;
};

}  // namespace detail

template <typename T, typename U, typename F>
auto Map(RefPtr<U> aPromise, F&& aFunc)
    -> std::enable_if_t<
        detail::IsExclusiveMozPromise<RemoveSmartPointer<U>>::value,
        RefPtr<T>> {
  return aPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [func =
           std::forward<F>(aFunc)](typename U::ResolveOrRejectValue&& aValue) {
        if (aValue.IsReject()) {
          return T::CreateAndReject(aValue.RejectValue(), __func__);
        }

        auto value = func(std::move(aValue));

        return T::CreateAndResolve(value, __func__);
      });
}

template <typename T, typename U, typename F>
auto Map(RefPtr<U> aPromise, F&& aFunc)
    -> std::enable_if_t<
        !detail::IsExclusiveMozPromise<RemoveSmartPointer<U>>::value,
        RefPtr<T>> {
  return aPromise->Then(GetCurrentSerialEventTarget(), __func__,
                        [func = std::forward<F>(aFunc)](
                            const typename U::ResolveOrRejectValue& aValue) {
                          if (aValue.IsReject()) {
                            return T::CreateAndReject(aValue.RejectValue(),
                                                      __func__);
                          }

                          auto value = func(aValue);

                          return T::CreateAndResolve(value, __func__);
                        });
}

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_MOZPROMISEUTILS_H_
