/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_capsule_parser_h
#define mozilla_net_capsule_parser_h

#include "Capsule.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"

namespace mozilla::net {

class CapsuleDecoder;

class CapsuleParser final {
 public:
  class Listener {
   public:
    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

    virtual bool OnCapsule(Capsule&& aCapsule) = 0;
    virtual void OnCapsuleParseFailure(nsresult aError) = 0;

   protected:
    virtual ~Listener() = default;
  };

  explicit CapsuleParser(Listener* aListener);
  ~CapsuleParser() = default;
  // Processes incoming data and attempts to parse complete capsules.
  // For each successfully parsed capsule, Listener::OnCapsule is invoked.
  // If there is insufficient data to form a complete capsule, the remaining
  // data is retained in mBuffer. In case a parsing error occurs,
  // OnCapsuleParseFailure is called and any remaining data in mBuffer is
  // discarded.
  bool ProcessCapsuleData(const uint8_t* aData, uint32_t aCount);

  bool IsBufferEmpty() const { return mBuffer.IsEmpty(); }

 private:
  // Indicates whether ProcessCapsuleData is already running.
  bool mProcessing = false;

  RefPtr<Listener> mListener;
  // The buffer used to store data when we don't have enough to parse one
  // capsule.
  nsTArray<uint8_t> mBuffer;
  // Parse one capsule from |aData|.
  // Returns 0 if the complete capsule is not yet available, returns an error if
  // parsing fails, or returns the number of bytes consumed for the successfully
  // parsed capsule.
  Result<size_t, nsresult> ParseCapsuleData(Span<const uint8_t> aData);
  // Parse the payload part of the capsule. Returns the parsed capsule or an
  // error.
  Result<Capsule, nsresult> ParseCapsulePayload(CapsuleDecoder& aDecoder,
                                                CapsuleType aType,
                                                size_t aPayloadLength);
};

}  // namespace mozilla::net

#endif
