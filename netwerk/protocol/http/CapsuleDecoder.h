/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_capsule_decoder_h
#define mozilla_net_capsule_decoder_h

#include "mozilla/Maybe.h"
#include "mozilla/Span.h"

namespace mozilla::net {

class NeqoDecoder;

// CapsuleDecoder provides methods to decode capsule data from a given buffer.
// It does not own the underlying data, so the caller must ensure that the
// buffer remains valid for the lifetime of the CapsuleDecoder instance.
class MOZ_STACK_CLASS CapsuleDecoder final {
 public:
  explicit CapsuleDecoder(const uint8_t* aData, size_t aLength);

  // Return Nothing() when there is not enough data to decode.
  template <typename T>
  Maybe<T> DecodeUint();

  Maybe<uint64_t> DecodeVarint();
  Maybe<mozilla::Span<const uint8_t>> Decode(size_t n);
  mozilla::Span<const uint8_t> GetRemaining();
  size_t BytesRemaining();

  size_t CurrentPos();

 private:
  RefPtr<NeqoDecoder> mDecoder;
};

}  // namespace mozilla::net

#endif
