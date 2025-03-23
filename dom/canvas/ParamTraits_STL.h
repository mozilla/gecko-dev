/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_PARAMTRAITS_STL_H
#define MOZILLA_PARAMTRAITS_STL_H

#include "ipc/IPCMessageUtils.h"
#include "mozilla/ipc/IPDLParamTraits.h"

#include <memory>

namespace IPC {

template <typename U, size_t N>
struct ParamTraits<std::array<U, N>> final {
  using T = std::array<U, N>;

  static void Write(MessageWriter* const writer, const T& in) {
    for (const auto& v : in) {
      WriteParam(writer, v);
    }
  }

  static bool Read(MessageReader* const reader, T* const out) {
    for (auto& v : *out) {
      if (!ReadParam(reader, &v)) return false;
    }
    return true;
  }
};

// -

template <typename U, size_t N>
struct ParamTraits<U[N]> final {
  using T = U[N];
  static constexpr size_t kByteSize = sizeof(U) * N;

  static_assert(std::is_trivial<U>::value);

  static void Write(MessageWriter* const writer, const T& in) {
    writer->WriteBytes(in, kByteSize);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    if (!reader->HasBytesAvailable(kByteSize)) {
      return false;
    }
    return reader->ReadBytesInto(*out, kByteSize);
  }
};

// -

template <class U>
struct ParamTraits<std::optional<U>> final {
  using T = std::optional<U>;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, bool{in});
    if (in) {
      WriteParam(writer, *in);
    }
  }

  static bool Read(MessageReader* const reader, T* const out) {
    bool isSome;
    if (!ReadParam(reader, &isSome)) return false;

    if (!isSome) {
      out->reset();
      return true;
    }
    out->emplace();
    return ReadParam(reader, &**out);
  }
};

// -

template <class A, class B>
struct ParamTraits<std::pair<A, B>> final {
  using T = std::pair<A, B>;

  static void Write(MessageWriter* const writer, const T& in) {
    WriteParam(writer, in.first);
    WriteParam(writer, in.second);
  }

  static bool Read(MessageReader* const reader, T* const out) {
    bool ok = true;
    ok = ok && ReadParam(reader, &out->first);
    ok = ok && ReadParam(reader, &out->second);
    return ok;
  }
};

// -

template <class K, class V, class H, class E>
struct ParamTraits<std::unordered_map<K, V, H, E>> final {
  using T = std::unordered_map<K, V, H, E>;

  static void Write(MessageWriter* const writer, const T& in) {
    const auto size = uint64_t{in.size()};
    WriteParam(writer, size);
    for (const auto& pair : in) {
      WriteParam(writer, pair);
    }
  }

  static bool Read(MessageReader* const reader, T* const out) {
    out->clear();

    auto size = uint64_t{};
    if (!ReadParam(reader, &size)) return false;

    out->reserve(static_cast<size_t>(size));
    for (const auto i : mozilla::IntegerRange(size)) {
      (void)i;
      auto pair = std::pair<K, V>{};
      if (!ReadParam(reader, &pair)) return false;
      out->insert(pair);
    }
    return true;
  }
};

// -

// template <class... T>
// struct ParamTraits<std::tuple<T...>> final {
//   using T = std::tuple<T...>;
//
//   static void Write(MessageWriter* const writer, const T& in) {
//     mozilla::MapTuple(aArg, [&](const auto& field) {
//       WriteParam(writer, field);
//       return true;  // ignored
//     });
//   }
//
//   static bool Read(MessageReader* const reader, T* const out) {
//     bool ok = true;
//     mozilla::MapTuple(*aArg, [&](auto& field) {
//       ok = ok && ReadParam(reader, &field);
//       return true;  // ignored
//     });
//     return ok;
//   }
// };

}  // namespace IPC

#endif
