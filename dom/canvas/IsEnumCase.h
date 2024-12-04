/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_ISENUMCASE_H
#define MOZILLA_DOM_ISENUMCASE_H

#include <optional>
#include <type_traits>

namespace mozilla {

template <class T>
bool IsEnumCase(T);

template <class E>
inline constexpr std::optional<E> AsEnumCase(
    const std::underlying_type_t<E> raw) {
  const auto ret = static_cast<E>(raw);
  if (!IsEnumCase(ret)) return {};
  return ret;
}

}  // namespace mozilla

#endif
