/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: softtabstop=2:shiftwidth=2:expandtab
 * */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: bcampen@mozilla.com

#include "MediaPipelineFilter.h"

#include "api/rtp_headers.h"
#include "mozilla/Logging.h"

// defined in MediaPipeline.cpp
extern mozilla::LazyLogModule gMediaPipelineLog;

#define DEBUG_LOG(x) MOZ_LOG(gMediaPipelineLog, LogLevel::Debug, x)

namespace mozilla {
MediaPipelineFilter::MediaPipelineFilter(
    const std::vector<webrtc::RtpExtension>& aExtMap)
    : mExtMap(aExtMap) {}

void MediaPipelineFilter::SetRemoteMediaStreamId(
    const Maybe<std::string>& aMid) {
  if (aMid != mRemoteMid) {
    DEBUG_LOG(("MediaPipelineFilter 0x%p added new remote RTP MID: '%s'.", this,
               aMid.valueOr("").c_str()));
    mRemoteMid = aMid;
    mRemoteMidBindings.clear();
  }
}

bool MediaPipelineFilter::Filter(const webrtc::RTPHeader& header) {
  DEBUG_LOG(("MediaPipelineFilter 0x%p inspecting seq# %u SSRC: %u", this,
             header.sequenceNumber, header.ssrc));

  auto fromStreamId = [](const std::string& aId) {
    return Maybe<std::string>(aId.empty() ? Nothing() : Some(aId));
  };

  //
  //  MID Based Filtering
  //

  const auto mid = fromStreamId(header.extension.mid);

  // Check to see if a bound SSRC is moved to a new MID
  if (mRemoteMidBindings.count(header.ssrc) == 1 && mid && mRemoteMid != mid) {
    mRemoteMidBindings.erase(header.ssrc);
  }
  // Bind an SSRC if a matching MID is found
  if (mid && mRemoteMid == mid) {
    DEBUG_LOG(("MediaPipelineFilter 0x%p learned SSRC: %u for MID: '%s'", this,
               header.ssrc, mRemoteMid.value().c_str()));
    mRemoteMidBindings.insert(header.ssrc);
  }
  // Check for matching MID
  if (!mRemoteMidBindings.empty()) {
    MOZ_ASSERT(mRemoteMid != Nothing());
    if (mRemoteMidBindings.count(header.ssrc) == 1) {
      DEBUG_LOG(
          ("MediaPipelineFilter 0x%p SSRC: %u matched for MID: '%s'."
           " passing packet",
           this, header.ssrc, mRemoteMid.value().c_str()));
      return true;
    }
    DEBUG_LOG(
        ("MediaPipelineFilter 0x%p SSRC: %u did not match bound SSRC(s) for"
         " MID: '%s'. ignoring packet",
         this, header.ssrc, mRemoteMid.value().c_str()));
    for (const uint32_t ssrc : mRemoteMidBindings) {
      DEBUG_LOG(("MediaPipelineFilter 0x%p MID %s is associated with SSRC: %u",
                 this, mRemoteMid.value().c_str(), ssrc));
    }
    return false;
  }

  //
  // RTP-STREAM-ID based filtering (for tests only)
  //

  //
  // Remote SSRC based filtering
  //

  if (remote_ssrc_set_.count(header.ssrc)) {
    DEBUG_LOG(
        ("MediaPipelineFilter 0x%p SSRC: %u matched remote SSRC set."
         " passing packet",
         this, header.ssrc));
    return true;
  }
  DEBUG_LOG(
      ("MediaPipelineFilter 0x%p SSRC: %u did not match any of %zu"
       " remote SSRCS.",
       this, header.ssrc, remote_ssrc_set_.size()));

  //
  // PT, payload type, last ditch effort filtering. We only try this if we do
  // not have any ssrcs configured (either by learning them, or negotiation).
  //

  if (receive_payload_type_set_.count(header.payloadType)) {
    DEBUG_LOG(
        ("MediaPipelineFilter 0x%p payload-type: %u matched %zu"
         " unique payload type. learning ssrc. passing packet",
         this, header.ssrc, remote_ssrc_set_.size()));
    // Actual match. We need to update the ssrc map so we can route rtcp
    // sender reports correctly (these use a different payload-type field)
    AddRemoteSSRC(header.ssrc);
    return true;
  }
  DEBUG_LOG(
      ("MediaPipelineFilter 0x%p payload-type: %u did not match any of %zu"
       " unique payload-types.",
       this, header.payloadType, receive_payload_type_set_.size()));
  DEBUG_LOG(
      ("MediaPipelineFilter 0x%p packet failed to match any criteria."
       " ignoring packet",
       this));
  return false;
}

void MediaPipelineFilter::AddRemoteSSRC(uint32_t ssrc) {
  remote_ssrc_set_.insert(ssrc);
}

void MediaPipelineFilter::AddUniqueReceivePT(uint8_t payload_type) {
  receive_payload_type_set_.insert(payload_type);
}

void MediaPipelineFilter::AddDuplicateReceivePT(uint8_t payload_type) {
  duplicate_payload_type_set_.insert(payload_type);
}

void MediaPipelineFilter::Update(const MediaPipelineFilter& filter_update,
                                 bool signalingStable) {
  // We will not stomp the remote_ssrc_set_ if the update has no ssrcs,
  // because we don't want to unlearn any remote ssrcs unless the other end
  // has explicitly given us a new set.
  if (!filter_update.remote_ssrc_set_.empty()) {
    remote_ssrc_set_ = filter_update.remote_ssrc_set_;
    for (const auto& ssrc : remote_ssrc_set_) {
      DEBUG_LOG(
          ("MediaPipelineFilter 0x%p Now bound to remote SSRC %u", this, ssrc));
    }
  }
  // We don't want to overwrite the learned binding unless we have changed MIDs
  // or the update contains a MID binding.
  if (!filter_update.mRemoteMidBindings.empty() ||
      (filter_update.mRemoteMid && filter_update.mRemoteMid != mRemoteMid)) {
    mRemoteMid = filter_update.mRemoteMid;
    mRemoteMidBindings = filter_update.mRemoteMidBindings;
    auto remoteMid = mRemoteMid.valueOrFrom([] { return std::string(); });
    DEBUG_LOG(("MediaPipelineFilter 0x%p Now bound to remote MID %s", this,
               remoteMid.c_str()));
    for (const auto& ssrc : mRemoteMidBindings) {
      DEBUG_LOG((
          "MediaPipelineFilter 0x%p Now bound to remote SSRC %u for remote MID "
          "%s",
          this, ssrc, remoteMid.c_str()));
    }
  }

  // If signaling is stable replace the pt filters, otherwise add to them.
  if (signalingStable) {
    receive_payload_type_set_ = filter_update.receive_payload_type_set_;
    duplicate_payload_type_set_ = filter_update.duplicate_payload_type_set_;
  } else {
    for (const auto& uniquePT : filter_update.receive_payload_type_set_) {
      if (!receive_payload_type_set_.count(uniquePT) &&
          !duplicate_payload_type_set_.count(uniquePT)) {
        AddUniqueReceivePT(uniquePT);
      }
    }
  }
  for (const auto& pt : receive_payload_type_set_) {
    DEBUG_LOG(("MediaPipelineFilter 0x%p Now bound to remote unique PT %u",
               this, pt));
  }
  for (const auto& pt : duplicate_payload_type_set_) {
    DEBUG_LOG(("MediaPipelineFilter 0x%p Now bound to remote duplicate PT %u",
               this, pt));
  }

  // Use extmapping from new filter
  mExtMap = filter_update.mExtMap;
}

}  // end namespace mozilla
