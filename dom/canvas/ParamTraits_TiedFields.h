/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_PARAMTRAITS_TIEDFIELDS_H
#define MOZILLA_PARAMTRAITS_TIEDFIELDS_H

#include "ipc/IPCMessageUtils.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "TiedFields.h"

namespace IPC {

template <class T>
struct ParamTraits_TiedFields {
  static_assert(mozilla::AssertTiedFieldsAreExhaustive<T>());

  static void Write(MessageWriter* const writer, const T& in) {
    const auto& fields = mozilla::TiedFields(in);
    mozilla::MapTuple(fields, [&](const auto& field) {
      WriteParam(writer, field);
      return true;  // ignored
    });
  }

  static bool Read(MessageReader* const reader, T* const out) {
    const auto& fields = mozilla::TiedFields(*out);
    bool ok = true;
    mozilla::MapTuple(fields, [&](auto& field) {
      if (ok) {
        ok &= ReadParam(reader, &field);
      }
      return true;  // ignored
    });
    return ok;
  }
};

// -

template <class U, size_t N>
struct ParamTraits<mozilla::PaddingField<U, N>> final
    : public ParamTraits_TiedFields<mozilla::PaddingField<U, N>> {};

}  // namespace IPC

#endif
