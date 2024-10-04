/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_RustMessageUtils_h
#define mozilla_ipc_RustMessageUtils_h

#include "chrome/common/ipc_message_utils.h"

// Some macros for rust integrations. See ipc/rust/ipdl_utils
#define MOZ_DEFINE_RUST_PARAMTRAITS(type_, serializer_, deserializer_)        \
  extern "C" uint8_t* serializer_(const type_*, size_t* len, size_t* cap);    \
  extern "C" bool deserializer_(const uint8_t*, size_t len, type_*);          \
                                                                              \
  template <>                                                                 \
  struct IPC::ParamTraits<type_> {                                            \
    using paramType = type_;                                                  \
    static void Write(IPC::MessageWriter* aWriter, const paramType& aParam) { \
      size_t len, cap;                                                        \
      uint8_t* buf = serializer_(&aParam, &cap, &len);                        \
      MOZ_DIAGNOSTIC_ASSERT(buf, #type_ " serialization failed");             \
      WriteParam(aWriter, mozilla::ipc::ByteBuf(buf, len, cap));              \
    }                                                                         \
    static IPC::ReadResult<paramType> Read(IPC::MessageReader* aReader) {     \
      mozilla::ipc::ByteBuf in;                                               \
      IPC::ReadResult<paramType> result;                                      \
      if (!ReadParam(aReader, &in) || !in.mData) {                            \
        return result;                                                        \
      }                                                                       \
      /* TODO: Should be able to initialize `result` in-place instead */      \
      mozilla::AlignedStorage2<paramType> value;                              \
      if (!deserializer_(in.mData, in.mLen, value.addr())) {                  \
        return result;                                                        \
      }                                                                       \
      result = std::move(*value.addr());                                      \
      value.addr()->~paramType();                                             \
      return result;                                                          \
    }                                                                         \
  };

#endif
