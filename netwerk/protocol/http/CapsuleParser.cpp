/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CapsuleParser.h"

#include "CapsuleDecoder.h"
#include "mozilla/ScopeExit.h"

namespace mozilla::net {

CapsuleParser::CapsuleParser(Listener* aListener) : mListener(aListener) {}

bool CapsuleParser::ProcessCapsuleData(const uint8_t* aData, uint32_t aCount) {
  // Prevent reentrant calls: if we're already processing, just return.
  if (mProcessing) {
    return false;
  }
  mProcessing = true;

  auto resetGuard = MakeScopeExit([&]() { mProcessing = false; });

  Span<const uint8_t> input;
  if (!mBuffer.IsEmpty()) {
    mBuffer.AppendElements(aData, aCount);
    input = Span<const uint8_t>(mBuffer.Elements(), mBuffer.Length());
  } else {
    input = Span<const uint8_t>(aData, aCount);
  }
  size_t pos = 0;
  size_t length = input.Length();

  while (true) {
    Span<const uint8_t> toProcess = input.Subspan(pos, length - pos);
    auto result = ParseCapsuleData(toProcess);
    if (result.isErr()) {
      mBuffer.Clear();
      return false;
    }

    size_t processed = result.unwrap();
    if (processed == 0) {
      if (mBuffer.IsEmpty()) {
        // Store the remaining data in mBuffer.
        mBuffer.AppendElements(toProcess.Elements(), toProcess.Length());
      } else {
        // Simply remove the already processed data.
        mBuffer.RemoveElementsAt(0, pos);
      }
      break;
    }

    pos += processed;
  }

  return true;
}

Result<size_t, nsresult> CapsuleParser::ParseCapsuleData(
    Span<const uint8_t> aData) {
  if (aData.IsEmpty()) {
    return 0;
  }

  CapsuleDecoder decoder(aData.Elements(), aData.Length());
  auto type = decoder.DecodeVarint();

  if (!type) {
    return 0;
  }

  CapsuleType capsuleType = static_cast<CapsuleType>(*type);
  auto payloadLength = decoder.DecodeVarint();
  if (!payloadLength) {
    return 0;
  }

  auto payload = decoder.Decode(*payloadLength);
  if (!payload) {
    return 0;
  }

  CapsuleDecoder payloadParser(payload->Elements(), payload->Length());
  auto result =
      ParseCapsulePayload(payloadParser, capsuleType, payload->Length());
  if (result.isErr()) {
    nsresult error = result.unwrapErr();
    mListener->OnCapsuleParseFailure(error);
    return Err(error);
  }

  Capsule capsule = result.unwrap();
  if (!mListener->OnCapsule(std::move(capsule))) {
    return Err(NS_ERROR_FAILURE);
  }

  return decoder.CurrentPos();
}

Result<Capsule, nsresult> CapsuleParser::ParseCapsulePayload(
    CapsuleDecoder& aDecoder, CapsuleType aType, size_t aPayloadLength) {
  switch (aType) {
    case CapsuleType::CLOSE_WEBTRANSPORT_SESSION: {
      if (aPayloadLength < 4) {
        return Err(NS_ERROR_UNEXPECTED);
      }
      Maybe<uint32_t> status = aDecoder.DecodeUint<uint32_t>();
      if (!status) {
        return Err(NS_ERROR_UNEXPECTED);
      }
      // https://www.ietf.org/archive/id/draft-ietf-webtrans-http2-10.html#section-6.12
      // The reason MUST not exceed 1024 bytes.
      if (aDecoder.BytesRemaining() > 1024) {
        return Err(NS_ERROR_UNEXPECTED);
      }
      auto reason = aDecoder.Decode(aPayloadLength - 4);
      if (!reason) {
        return Err(NS_ERROR_UNEXPECTED);
      }
      return Capsule::CloseWebTransportSession(
          *status, nsCString(reinterpret_cast<const char*>(reason->Elements()),
                             reason->Length()));
    }
    case CapsuleType::DRAIN_WEBTRANSPORT_SESSION:
      break;
    case CapsuleType::PADDING:
      break;
    case CapsuleType::WT_RESET_STREAM:
      break;
    case CapsuleType::WT_STOP_SENDING:
      break;
    case CapsuleType::WT_STREAM: {
      auto id = aDecoder.DecodeVarint();
      if (!id) {
        return Err(NS_ERROR_UNEXPECTED);
      }
      nsTArray<uint8_t> data(aDecoder.GetRemaining());
      return Capsule::WebTransportStreamData(*id, false, std::move(data));
    }
    case CapsuleType::WT_STREAM_FIN: {
      auto id = aDecoder.DecodeVarint();
      if (!id) {
        return Err(NS_ERROR_UNEXPECTED);
      }
      nsTArray<uint8_t> data(aDecoder.GetRemaining());
      return Capsule::WebTransportStreamData(*id, true, std::move(data));
    }
    case CapsuleType::WT_MAX_DATA: {
      auto value = aDecoder.DecodeVarint();
      if (!value) {
        return Err(NS_ERROR_UNEXPECTED);
      }
      return Capsule::WebTransportMaxData(*value);
    }
    case CapsuleType::WT_MAX_STREAM_DATA:
      break;
    case CapsuleType::WT_MAX_STREAMS_BIDI:
      break;
    case CapsuleType::WT_MAX_STREAMS_UNIDI:
      break;
    case CapsuleType::WT_DATA_BLOCKED:
      break;
    case CapsuleType::WT_STREAM_DATA_BLOCKED:
      break;
    case CapsuleType::WT_STREAMS_BLOCKED_BIDI:
      break;
    case CapsuleType::WT_STREAMS_BLOCKED_UNIDI:
      break;
    default:
      break;
  }

  return Capsule::Unknown(static_cast<uint64_t>(aType),
                          nsTArray<uint8_t>(aDecoder.GetRemaining()));
}

}  // namespace mozilla::net
