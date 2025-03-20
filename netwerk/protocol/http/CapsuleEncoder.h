/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_capsule_encoder_h
#define mozilla_net_capsule_encoder_h

#include "mozilla/RefPtr.h"
#include "mozilla/Span.h"
#include "nsTArray.h"

namespace mozilla::net {

class NeqoEncoder;

// A wrapper of neqo_common::Encoder.
class CapsuleEncoder final {
 public:
  CapsuleEncoder();
  ~CapsuleEncoder();

  // Serializes the given capsule into the internal buffer owned by mEncoder.
  void EncodeCapsule(Capsule& aCapsule);
  // Provides external access to the mEncoder's internal buffer which contains
  // the serialized capsule data. This function outputs a pointer to the start
  // of the buffer along with its current length.
  mozilla::Span<const uint8_t> GetBuffer();

 private:
  RefPtr<NeqoEncoder> mEncoder;

  CapsuleEncoder& EncodeByte(uint8_t aData);

  template <typename T>
  CapsuleEncoder& EncodeUint(uint32_t aSize, T aValue);

  template <typename T>
  CapsuleEncoder& EncodeVarint(T aValue);

  CapsuleEncoder& EncodeString(const nsACString& aData);
  CapsuleEncoder& EncodeBuffer(nsTArray<uint8_t>& aData);
  CapsuleEncoder& EncodeBufferWithVarintLen(nsTArray<uint8_t>& aData);

  static size_t VarintLength(uint64_t aValue);
};

}  // namespace mozilla::net

#endif
