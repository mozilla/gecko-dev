/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Capsule.h"
#include "CapsuleEncoder.h"
// For Http2Session::LogIO.
#include "Http2Session.h"

namespace mozilla::net {

// static
void Capsule::LogBuffer(const uint8_t* aBuffer, uint32_t aLength) {
#ifdef DEBUG
  Http2Session::LogIO(nullptr, nullptr, "Capsule",
                      reinterpret_cast<const char*>(aBuffer), aLength);
#endif
}

// static
Capsule Capsule::CloseWebTransportSession(uint32_t aStatus,
                                          const nsACString& aReason) {
  CloseWebTransportSessionCapsule capsule;
  capsule.mStatus = aStatus;
  capsule.mReason = aReason;
  return Capsule(std::move(capsule));
}

// static
Capsule Capsule::WebTransportMaxData(uint64_t aValue) {
  WebTransportMaxDataCapsule capsule;
  capsule.mMaxDataSize = aValue;
  return Capsule(std::move(capsule));
}

// static
Capsule Capsule::WebTransportStreamData(uint64_t aID, bool aFin,
                                        nsTArray<uint8_t>&& aData) {
  WebTransportStreamDataCapsule capsule;
  capsule.mID = aID;
  capsule.mFin = aFin;
  capsule.mData.AppendElements(std::move(aData));
  return Capsule(std::move(capsule));
}

// static
Capsule Capsule::WebTransportStreamsBlocked(uint64_t aLimit, bool aBidi) {
  WebTransportStreamsBlockedCapsule capsule;
  capsule.mLimit = aLimit;
  capsule.mBidi = aBidi;
  return Capsule(std::move(capsule));
}

// static
Capsule Capsule::WebTransportMaxStreams(uint64_t aLimit, bool aBidi) {
  WebTransportMaxStreamsCapsule capsule;
  capsule.mLimit = aLimit;
  capsule.mBidi = aBidi;
  return Capsule(std::move(capsule));
}

// static
Capsule Capsule::WebTransportStreamDataBlocked(uint64_t aLimit, uint64_t aID) {
  WebTransportStreamDataBlockedCapsule capsule;
  capsule.mLimit = aLimit;
  capsule.mID = aID;
  return Capsule(std::move(capsule));
}

// static
Capsule Capsule::WebTransportMaxStreamData(uint64_t aLimit, uint64_t aID) {
  WebTransportMaxStreamDataCapsule capsule;
  capsule.mLimit = aLimit;
  capsule.mID = aID;
  return Capsule(std::move(capsule));
}

// static
Capsule Capsule::WebTransportDataBlocked(uint64_t aLimit) {
  WebTransportDataBlockedCapsule capsule;
  capsule.mLimit = aLimit;
  return Capsule(std::move(capsule));
}

// static
Capsule Capsule::Unknown(uint64_t aType, nsTArray<uint8_t>&& aData) {
  UnknownCapsule capsule;
  capsule.mType = aType;
  capsule.mData = std::move(aData);
  return Capsule(std::move(capsule));
}

CapsuleType Capsule::Type() const {
  return mCapsule.match(
      [](const UnknownCapsule& aCapsule) { return aCapsule.Type(); },
      [](const CloseWebTransportSessionCapsule& aCapsule) {
        return aCapsule.Type();
      },
      [](const WebTransportMaxDataCapsule& aCapsule) {
        return aCapsule.Type();
      },
      [](const WebTransportStreamDataCapsule& aCapsule) {
        return aCapsule.Type();
      },
      [](const WebTransportStreamsBlockedCapsule& aCapsule) {
        return aCapsule.Type();
      },
      [](const WebTransportMaxStreamsCapsule& aCapsule) {
        return aCapsule.Type();
      },
      [](const WebTransportStreamDataBlockedCapsule& aCapsule) {
        return aCapsule.Type();
      },
      [](const WebTransportMaxStreamDataCapsule& aCapsule) {
        return aCapsule.Type();
      },
      [](const WebTransportDataBlockedCapsule& aCapsule) {
        return aCapsule.Type();
      });
}

}  // namespace mozilla::net
