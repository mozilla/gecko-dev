/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CapsuleDecoder.h"

#include "ErrorList.h"
#include "mozilla/net/NeqoHttp3Conn.h"

namespace mozilla::net {

CapsuleDecoder::CapsuleDecoder(const uint8_t* aData, size_t aLength) {
  NeqoDecoder::Init(aData, aLength, getter_AddRefs(mDecoder));
}

CapsuleDecoder::~CapsuleDecoder() = default;

template <>
Maybe<uint32_t> CapsuleDecoder::DecodeUint<uint32_t>() {
  uint32_t res = 0;
  if (mDecoder->DecodeUint32(&res)) {
    return Some(res);
  }
  return Nothing();
}

size_t CapsuleDecoder::BytesRemaining() { return mDecoder->Remaining(); }

size_t CapsuleDecoder::CurrentPos() { return mDecoder->Offset(); }

Maybe<uint64_t> CapsuleDecoder::DecodeVarint() {
  uint64_t v = 0;
  if (mDecoder->DecodeVarint(&v)) {
    return Some(v);
  }

  return Nothing();
}

// Decodes arbitrary data: returns a span over the next n bytes, if available.
Maybe<mozilla::Span<const uint8_t>> CapsuleDecoder::Decode(size_t n) {
  const uint8_t* buffer = nullptr;
  uint32_t length = 0;
  if (mDecoder->Decode(n, &buffer, &length)) {
    return Some(mozilla::Span<const uint8_t>(buffer, length));
  }

  return Nothing();
}

mozilla::Span<const uint8_t> CapsuleDecoder::GetRemaining() {
  const uint8_t* buffer = nullptr;
  uint32_t length = 0;
  mDecoder->DecodeRemainder(&buffer, &length);
  return mozilla::Span<const uint8_t>(buffer, length);
}

}  // namespace mozilla::net
