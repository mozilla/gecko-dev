/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebTransportFlowControl_h
#define mozilla_net_WebTransportFlowControl_h

#include "Capsule.h"
#include "CapsuleEncoder.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/net/neqo_glue_ffi_generated.h"
#include "WebTransportStreamBase.h"

namespace mozilla::net {

// This is based on `fc::SenderFlowControl` in neqo. Ideally, we would reuse it,
// but `SenderFlowControl` is in a private crate and tightly integrated with
// other internal crates in neqo.
class SenderFlowControlBase {
 public:
  explicit SenderFlowControlBase(uint64_t aInitial) : mLimit(aInitial) {}

  bool Update(uint64_t aNewLimit) {
    MOZ_ASSERT(aNewLimit < UINT64_MAX);
    if (aNewLimit > mLimit) {
      mLimit = aNewLimit;
      mBlockedCapsule = false;
      return true;
    }
    return false;
  }

  void Consume(uint64_t aCount) {
    MOZ_ASSERT(mUsed + aCount <= mLimit);
    mUsed += aCount;
  }

  uint64_t Available() const { return mLimit - mUsed; }

  uint64_t Used() const { return mUsed; }

  void Blocked() {
    if (mLimit >= mBlockedAt) {
      mBlockedAt = mLimit + 1;
      mBlockedCapsule = true;
    }
  }

  // Return whether a blocking Capsule needs to be sent.
  // This is `Some` with the active limit if `blocked` has been called,
  // if a blocking frame has not been sent (or it has been lost), and
  // if the blocking condition remains.
  mozilla::Maybe<uint64_t> BlockedNeeded() const {
    if (mBlockedCapsule && mLimit < mBlockedAt) {
      return Some(mBlockedAt - 1);
    }
    return Nothing();
  }

  void BlockedSent() { mBlockedCapsule = false; }

 protected:
  uint64_t mLimit = 0;
  uint64_t mUsed = 0;
  uint64_t mBlockedAt = 0;
  bool mBlockedCapsule = false;
};

class SenderFlowControlStreamType : public SenderFlowControlBase {
 public:
  SenderFlowControlStreamType(WebTransportStreamType aType, uint64_t aInitial)
      : SenderFlowControlBase(aInitial), mType(aType) {}

  Maybe<CapsuleEncoder> CreateStreamsBlockedCapsule();

 private:
  WebTransportStreamType mType;
};

class LocalStreamLimits {
 public:
  LocalStreamLimits()
      : mBidirectional(WebTransportStreamType::BiDi, 0),
        mUnidirectional(WebTransportStreamType::UniDi, 0) {}

  mozilla::Maybe<StreamId> TakeStreamId(WebTransportStreamType aStreamType) {
    SenderFlowControlStreamType& fc =
        (aStreamType == WebTransportStreamType::BiDi) ? mBidirectional
                                                      : mUnidirectional;

    if (fc.Available() > 0) {
      uint64_t newId = fc.Used();
      fc.Consume(1);
      uint64_t typeBit = (aStreamType == WebTransportStreamType::BiDi) ? 0 : 2;
      return Some(StreamId((newId << 2) + typeBit));
    } else {
      fc.Blocked();
      return Nothing();
    }
  }

  const SenderFlowControlStreamType& operator[](
      WebTransportStreamType aStreamType) const {
    if (aStreamType == WebTransportStreamType::BiDi) {
      return mBidirectional;
    }

    MOZ_ASSERT(aStreamType == WebTransportStreamType::UniDi);
    return mUnidirectional;
  }

  SenderFlowControlStreamType& operator[](WebTransportStreamType aStreamType) {
    if (aStreamType == WebTransportStreamType::BiDi) {
      return mBidirectional;
    }

    MOZ_ASSERT(aStreamType == WebTransportStreamType::UniDi);
    return mUnidirectional;
  }

 private:
  SenderFlowControlStreamType mBidirectional;
  SenderFlowControlStreamType mUnidirectional;
};

class ReceiverFlowControlBase {
 public:
  explicit ReceiverFlowControlBase(uint64_t aMax)
      : mMaxActive(aMax), mMaxAllowed(aMax) {}

  void Retire(uint64_t aRetired) {
    if (aRetired <= mRetired) {
      return;
    }
    mRetired = aRetired;
    if (mRetired + mMaxActive / 2 > mMaxAllowed) {
      mCapsulePending = true;
    }
  }

  void SendFlowControlUpdate() {
    if (mRetired + mMaxActive > mMaxAllowed) {
      mCapsulePending = true;
    }
  }

  bool CapsuleNeeded() const { return mCapsulePending; }
  uint64_t NextLimit() const { return mRetired + mMaxActive; }
  uint64_t MaxActive() const { return mMaxActive; }

  void SetMaxActive(uint64_t aMax) {
    mCapsulePending |= (mMaxActive < aMax);
    mMaxActive = aMax;
  }

  uint64_t Retired() const { return mRetired; }
  uint64_t Consumed() const { return mConsumed; }

  void CapsuleSent(uint64_t aNewMax) {
    mMaxAllowed = aNewMax;
    mCapsulePending = false;
  }

 protected:
  uint64_t mMaxActive = 0;
  uint64_t mMaxAllowed = 0;
  uint64_t mConsumed = 0;
  uint64_t mRetired = 0;
  bool mCapsulePending = false;
};

class ReceiverFlowControlStreamType : public ReceiverFlowControlBase {
 public:
  ReceiverFlowControlStreamType(WebTransportStreamType aStreamType,
                                uint64_t aMax)
      : ReceiverFlowControlBase(aMax), mType(aStreamType) {}

  Maybe<CapsuleEncoder> CreateMaxStreamsCapsule();

  bool CheckAllowed(uint64_t aNewEnd) const { return aNewEnd < mMaxAllowed; }

  void AddRetired(uint64_t aCount) {
    mRetired += aCount;
    if (aCount > 0) {
      SendFlowControlUpdate();
    }
  }

 private:
  WebTransportStreamType mType = WebTransportStreamType::BiDi;
};

class RemoteStreamLimit {
 public:
  RemoteStreamLimit(WebTransportStreamType aStreamType, uint64_t aMaxStreams)
      : mStreamsFC(aStreamType, aMaxStreams) {
    uint64_t typeBit = (aStreamType == WebTransportStreamType::BiDi) ? 0 : 2;
    // Server initiated stream starts with 1.
    mNextStreamId = StreamId(typeBit + 1);
  }

  bool IsAllowed(StreamId aStreamId) const {
    uint64_t streamIndex = aStreamId >> 2;
    return mStreamsFC.CheckAllowed(streamIndex);
  }

  Result<bool, nsresult> IsNewStream(StreamId aStreamId) const {
    if (!IsAllowed(aStreamId)) {
      return Err(NS_ERROR_NOT_AVAILABLE);
    }

    return aStreamId >= mNextStreamId;
  }

  StreamId TakeStreamId() {
    StreamId newStream = mNextStreamId;
    mNextStreamId.Next();
    MOZ_ASSERT(IsAllowed(newStream));
    return newStream;
  }

  ReceiverFlowControlStreamType& FlowControl() { return mStreamsFC; }
  const ReceiverFlowControlStreamType& FlowControl() const {
    return mStreamsFC;
  }

 private:
  ReceiverFlowControlStreamType mStreamsFC;
  StreamId mNextStreamId{1u};
};

class RemoteStreamLimits {
 public:
  RemoteStreamLimits(uint64_t aBidiMax, uint64_t aUniMax)
      : mBidi(WebTransportStreamType::BiDi, aBidiMax),
        mUni(WebTransportStreamType::UniDi, aUniMax) {}

  RemoteStreamLimit& operator[](WebTransportStreamType aType) {
    return aType == WebTransportStreamType::BiDi ? mBidi : mUni;
  }

  const RemoteStreamLimit& operator[](WebTransportStreamType aType) const {
    return aType == WebTransportStreamType::BiDi ? mBidi : mUni;
  }

 private:
  RemoteStreamLimit mBidi;
  RemoteStreamLimit mUni;
};

}  // namespace mozilla::net

#endif
