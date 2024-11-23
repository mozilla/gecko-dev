/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_PARAMTRAITS_ISENUMCASE_H
#define MOZILLA_PARAMTRAITS_ISENUMCASE_H

#include "ipc/IPCMessageUtils.h"
#include "IsEnumCase.h"
#include "mozilla/ipc/IPDLParamTraits.h"

namespace IPC {

/*
`IsEnumCase(T) -> bool` guarantees that we never have false negatives or false
positives due to adding or removing enum cases to enums, and forgetting to
update their serializations. Also, it allows enums to be non-continguous, unlike
ContiguousEnumSerializer.
*/

template <class T>
struct ParamTraits_IsEnumCase {
  static bool Write(MessageWriter* const writer, const T& in) {
    MOZ_ASSERT(mozilla::IsEnumCase(in));
    const auto shadow = static_cast<std::underlying_type_t<T>>(in);
    WriteParam(writer, shadow);
    return true;
  }

  static bool Read(MessageReader* const reader, T* const out) {
    auto shadow = std::underlying_type_t<T>{};
    if (!ReadParam(reader, &shadow)) return false;
    const auto e = mozilla::AsEnumCase<T>(shadow);
    if (!e) return false;
    *out = *e;
    return true;
  }
};

}  // namespace IPC

#endif
