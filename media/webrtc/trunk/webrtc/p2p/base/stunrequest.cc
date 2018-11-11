/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/stunrequest.h"

#include <algorithm>
#include "webrtc/base/common.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"

namespace cricket {

const uint32 MSG_STUN_SEND = 1;

const int MAX_SENDS = 9;
const int DELAY_UNIT = 100;  // 100 milliseconds
const int DELAY_MAX_FACTOR = 16;

StunRequestManager::StunRequestManager(rtc::Thread* thread)
    : thread_(thread) {
}

StunRequestManager::~StunRequestManager() {
  while (requests_.begin() != requests_.end()) {
    StunRequest *request = requests_.begin()->second;
    requests_.erase(requests_.begin());
    delete request;
  }
}

void StunRequestManager::Send(StunRequest* request) {
  SendDelayed(request, 0);
}

void StunRequestManager::SendDelayed(StunRequest* request, int delay) {
  request->set_manager(this);
  ASSERT(requests_.find(request->id()) == requests_.end());
  request->set_origin(origin_);
  request->Construct();
  requests_[request->id()] = request;
  if (delay > 0) {
    thread_->PostDelayed(delay, request, MSG_STUN_SEND, NULL);
  } else {
    thread_->Send(request, MSG_STUN_SEND, NULL);
  }
}

void StunRequestManager::Remove(StunRequest* request) {
  ASSERT(request->manager() == this);
  RequestMap::iterator iter = requests_.find(request->id());
  if (iter != requests_.end()) {
    ASSERT(iter->second == request);
    requests_.erase(iter);
    thread_->Clear(request);
  }
}

void StunRequestManager::Clear() {
  std::vector<StunRequest*> requests;
  for (RequestMap::iterator i = requests_.begin(); i != requests_.end(); ++i)
    requests.push_back(i->second);

  for (uint32 i = 0; i < requests.size(); ++i) {
    // StunRequest destructor calls Remove() which deletes requests
    // from |requests_|.
    delete requests[i];
  }
}

bool StunRequestManager::CheckResponse(StunMessage* msg) {
  RequestMap::iterator iter = requests_.find(msg->transaction_id());
  if (iter == requests_.end())
    return false;

  StunRequest* request = iter->second;
  if (msg->type() == GetStunSuccessResponseType(request->type())) {
    request->OnResponse(msg);
  } else if (msg->type() == GetStunErrorResponseType(request->type())) {
    request->OnErrorResponse(msg);
  } else {
    LOG(LERROR) << "Received response with wrong type: " << msg->type()
                << " (expecting "
                << GetStunSuccessResponseType(request->type()) << ")";
    return false;
  }

  delete request;
  return true;
}

bool StunRequestManager::CheckResponse(const char* data, size_t size) {
  // Check the appropriate bytes of the stream to see if they match the
  // transaction ID of a response we are expecting.

  if (size < 20)
    return false;

  std::string id;
  id.append(data + kStunTransactionIdOffset, kStunTransactionIdLength);

  RequestMap::iterator iter = requests_.find(id);
  if (iter == requests_.end())
    return false;

  // Parse the STUN message and continue processing as usual.

  rtc::ByteBuffer buf(data, size);
  rtc::scoped_ptr<StunMessage> response(iter->second->msg_->CreateNew());
  if (!response->Read(&buf))
    return false;

  return CheckResponse(response.get());
}

StunRequest::StunRequest()
    : count_(0), timeout_(false), manager_(0),
      msg_(new StunMessage()), tstamp_(0) {
  msg_->SetTransactionID(
      rtc::CreateRandomString(kStunTransactionIdLength));
}

StunRequest::StunRequest(StunMessage* request)
    : count_(0), timeout_(false), manager_(0),
      msg_(request), tstamp_(0) {
  msg_->SetTransactionID(
      rtc::CreateRandomString(kStunTransactionIdLength));
}

StunRequest::~StunRequest() {
  ASSERT(manager_ != NULL);
  if (manager_) {
    manager_->Remove(this);
    manager_->thread_->Clear(this);
  }
  delete msg_;
}

void StunRequest::Construct() {
  if (msg_->type() == 0) {
    if (!origin_.empty()) {
      msg_->AddAttribute(new StunByteStringAttribute(STUN_ATTR_ORIGIN,
          origin_));
    }
    Prepare(msg_);
    ASSERT(msg_->type() != 0);
  }
}

int StunRequest::type() {
  ASSERT(msg_ != NULL);
  return msg_->type();
}

const StunMessage* StunRequest::msg() const {
  return msg_;
}

uint32 StunRequest::Elapsed() const {
  return rtc::TimeSince(tstamp_);
}


void StunRequest::set_manager(StunRequestManager* manager) {
  ASSERT(!manager_);
  manager_ = manager;
}

void StunRequest::OnMessage(rtc::Message* pmsg) {
  ASSERT(manager_ != NULL);
  ASSERT(pmsg->message_id == MSG_STUN_SEND);

  if (timeout_) {
    OnTimeout();
    delete this;
    return;
  }

  tstamp_ = rtc::Time();

  rtc::ByteBuffer buf;
  msg_->Write(&buf);
  manager_->SignalSendPacket(buf.Data(), buf.Length(), this);

  int delay = GetNextDelay();
  manager_->thread_->PostDelayed(delay, this, MSG_STUN_SEND, NULL);
}

int StunRequest::GetNextDelay() {
  int delay = DELAY_UNIT * std::min(1 << count_, DELAY_MAX_FACTOR);
  count_ += 1;
  if (count_ == MAX_SENDS)
    timeout_ = true;
  return delay;
}

}  // namespace cricket
