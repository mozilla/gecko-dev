// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mach_ipc_mac.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "mozilla/GeckoArgs.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsDebug.h"

#ifdef XP_MACOSX
#  include <bsm/libbsm.h>
#  include <servers/bootstrap.h>
#endif

namespace {
// Struct for sending a Mach message with a single port.
struct MachSinglePortMessage {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t data;
};

// Struct for receiving a Mach message with a single port.
struct MachSinglePortMessageTrailer : MachSinglePortMessage {
  mach_msg_audit_trailer_t trailer;
};
}  // namespace

//==============================================================================
kern_return_t MachSendPortSendRight(
    mach_port_t endpoint, mach_port_t attachment,
    mozilla::Maybe<mach_msg_timeout_t> opt_timeout,
    mach_msg_type_name_t endpoint_disposition) {
  mach_msg_option_t opts = MACH_SEND_MSG;
  mach_msg_timeout_t timeout = MACH_MSG_TIMEOUT_NONE;
  if (opt_timeout) {
    opts |= MACH_SEND_TIMEOUT;
    timeout = *opt_timeout;
  }

  MachSinglePortMessage send_msg{};
  send_msg.header.msgh_bits =
      MACH_MSGH_BITS(endpoint_disposition, 0) | MACH_MSGH_BITS_COMPLEX;
  send_msg.header.msgh_size = sizeof(send_msg);
  send_msg.header.msgh_remote_port = endpoint;
  send_msg.header.msgh_local_port = MACH_PORT_NULL;
  send_msg.header.msgh_reserved = 0;
  send_msg.header.msgh_id = 0;
  send_msg.body.msgh_descriptor_count = 1;
  send_msg.data.name = attachment;
  send_msg.data.disposition = MACH_MSG_TYPE_COPY_SEND;
  send_msg.data.type = MACH_MSG_PORT_DESCRIPTOR;

  return mach_msg(&send_msg.header, opts, send_msg.header.msgh_size, 0,
                  MACH_PORT_NULL, timeout, MACH_PORT_NULL);
}

//==============================================================================
kern_return_t MachReceivePortSendRight(
    const mozilla::UniqueMachReceiveRight& endpoint,
    mozilla::Maybe<mach_msg_timeout_t> opt_timeout,
    mozilla::UniqueMachSendRight* attachment, audit_token_t* audit_token) {
  mach_msg_option_t opts = MACH_RCV_MSG |
                           MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
                           MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);
  mach_msg_timeout_t timeout = MACH_MSG_TIMEOUT_NONE;
  if (opt_timeout) {
    opts |= MACH_RCV_TIMEOUT;
    timeout = *opt_timeout;
  }

  MachSinglePortMessageTrailer recv_msg{};
  recv_msg.header.msgh_local_port = endpoint.get();
  recv_msg.header.msgh_size = sizeof(recv_msg);

  kern_return_t kr =
      mach_msg(&recv_msg.header, opts, 0, recv_msg.header.msgh_size,
               endpoint.get(), timeout, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    return kr;
  }

  if (NS_WARN_IF(!(recv_msg.header.msgh_bits & MACH_MSGH_BITS_COMPLEX)) ||
      NS_WARN_IF(recv_msg.body.msgh_descriptor_count != 1) ||
      NS_WARN_IF(recv_msg.data.type != MACH_MSG_PORT_DESCRIPTOR) ||
      NS_WARN_IF(recv_msg.data.disposition != MACH_MSG_TYPE_MOVE_SEND) ||
      NS_WARN_IF(recv_msg.header.msgh_size != sizeof(MachSinglePortMessage))) {
    mach_msg_destroy(&recv_msg.header);
    return KERN_FAILURE;  // Invalid message format
  }

  attachment->reset(recv_msg.data.name);
  if (audit_token) {
    *audit_token = recv_msg.trailer.msgh_audit;
  }
  return KERN_SUCCESS;
}

#ifdef XP_MACOSX
static std::string FormatMachError(kern_return_t kr) {
  return StringPrintf("0x%x %s", kr, mach_error_string(kr));
}

//==============================================================================
bool MachChildProcessCheckIn(
    const char* bootstrap_service_name, mach_msg_timeout_t timeout,
    std::vector<mozilla::UniqueMachSendRight>& send_rights) {
  mozilla::UniqueMachSendRight task_sender;
  kern_return_t kr = bootstrap_look_up(bootstrap_port, bootstrap_service_name,
                                       getter_Transfers(task_sender));
  if (kr != KERN_SUCCESS) {
    CHROMIUM_LOG(ERROR) << "child bootstrap_look_up failed: "
                        << FormatMachError(kr);
    return false;
  }

  mozilla::UniqueMachReceiveRight reply_port;
  kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                          getter_Transfers(reply_port));
  if (kr != KERN_SUCCESS) {
    CHROMIUM_LOG(ERROR) << "child mach_port_allocate failed: "
                        << FormatMachError(kr);
    return false;
  }

  // The buffer must be big enough to store a full reply including
  // kMaxPassedMachSendRights port descriptors.
  size_t buffer_size = sizeof(mach_msg_base_t) +
                       sizeof(mach_msg_port_descriptor_t) *
                           mozilla::geckoargs::kMaxPassedMachSendRights +
                       sizeof(mach_msg_trailer_t);
  mozilla::UniquePtr<uint8_t[]> buffer =
      mozilla::MakeUnique<uint8_t[]>(buffer_size);

  // Send a single descriptor - the process mach_task_self() port.
  MachSinglePortMessage* request =
      reinterpret_cast<MachSinglePortMessage*>(buffer.get());
  request->header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) |
      MACH_MSGH_BITS_COMPLEX;
  request->header.msgh_size = sizeof(MachSinglePortMessage);
  request->header.msgh_remote_port = task_sender.get();
  request->header.msgh_local_port = reply_port.get();
  request->body.msgh_descriptor_count = 1;
  request->data.type = MACH_MSG_PORT_DESCRIPTOR;
  request->data.disposition = MACH_MSG_TYPE_COPY_SEND;
  request->data.name = mach_task_self();
  kr = mach_msg(&request->header,
                MACH_SEND_MSG | MACH_RCV_MSG | MACH_SEND_TIMEOUT,
                request->header.msgh_size, buffer_size,
                request->header.msgh_local_port, timeout, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    // NOTE: The request owns no ports, so we don't need to call
    // mach_msg_destroy on error here.
    CHROMIUM_LOG(ERROR) << "child mach_msg failed: " << FormatMachError(kr);
    return false;
  }

  mach_msg_base_t* reply = reinterpret_cast<mach_msg_base_t*>(buffer.get());
  MOZ_RELEASE_ASSERT(reply->header.msgh_bits & MACH_MSGH_BITS_COMPLEX);
  MOZ_RELEASE_ASSERT(reply->body.msgh_descriptor_count <=
                     mozilla::geckoargs::kMaxPassedMachSendRights);

  mach_msg_port_descriptor_t* descrs =
      reinterpret_cast<mach_msg_port_descriptor_t*>(reply + 1);
  for (size_t i = 0; i < reply->body.msgh_descriptor_count; ++i) {
    MOZ_RELEASE_ASSERT(descrs[i].type == MACH_MSG_PORT_DESCRIPTOR);
    MOZ_RELEASE_ASSERT(descrs[i].disposition == MACH_MSG_TYPE_MOVE_SEND);
    send_rights.emplace_back(descrs[i].name);
  }

  return true;
}

//==============================================================================
mozilla::Result<mozilla::Ok, mozilla::ipc::LaunchError>
MachHandleProcessCheckIn(
    mach_port_t endpoint, pid_t child_pid, mach_msg_timeout_t timeout,
    const std::vector<mozilla::UniqueMachSendRight>& send_rights,
    task_t* child_task) {
  using mozilla::Err;
  using mozilla::Ok;
  using mozilla::Result;
  using mozilla::ipc::LaunchError;

  MOZ_ASSERT(send_rights.size() <= mozilla::geckoargs::kMaxPassedMachSendRights,
             "Child process cannot receive more than kMaxPassedMachSendRights "
             "during check-in!");

  // Receive the check-in message from content. This will contain its 'task_t'
  // data, and a reply port which can be used to send the reply message.
  MachSinglePortMessageTrailer request{};
  request.header.msgh_size = sizeof(request);
  request.header.msgh_local_port = endpoint;

  // Ensure that the request from the new child process is cleaned up if we fail
  // in some way, such as to not leak any resources.
  auto destroyRequestMessage =
      mozilla::MakeScopeExit([&] { mach_msg_destroy(&request.header); });

  kern_return_t kr =
      mach_msg(&request.header,
               MACH_RCV_MSG | MACH_RCV_TIMEOUT |
                   MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
                   MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
               0, request.header.msgh_size, endpoint, timeout, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    CHROMIUM_LOG(ERROR) << "parent mach_msg(MACH_RECV_MSG) failed: "
                        << FormatMachError(kr);
    return Err(LaunchError("mach_msg(MACH_RECV_MSG)", kr));
  }

  if (NS_WARN_IF(!(request.header.msgh_bits & MACH_MSGH_BITS_COMPLEX)) ||
      NS_WARN_IF(request.body.msgh_descriptor_count != 1) ||
      NS_WARN_IF(request.data.type != MACH_MSG_PORT_DESCRIPTOR) ||
      NS_WARN_IF(request.data.disposition != MACH_MSG_TYPE_MOVE_SEND) ||
      NS_WARN_IF(request.header.msgh_size != sizeof(MachSinglePortMessage))) {
    CHROMIUM_LOG(ERROR) << "invalid child process check-in message format";
    return Err(LaunchError("invalid child process check-in message format"));
  }

  // Ensure the message was sent by the newly spawned child process.
  if (audit_token_to_pid(request.trailer.msgh_audit) != child_pid) {
    CHROMIUM_LOG(ERROR) << "task_t was not sent by child process";
    return Err(LaunchError("audit_token_to_pid"));
  }

  // Ensure the task_t corresponds to the newly spawned child process.
  pid_t task_pid = -1;
  kr = pid_for_task(request.data.name, &task_pid);
  if (kr != KERN_SUCCESS) {
    CHROMIUM_LOG(ERROR) << "pid_for_task failed: " << FormatMachError(kr);
    return Err(LaunchError("pid_for_task", kr));
  }
  if (task_pid != child_pid) {
    CHROMIUM_LOG(ERROR) << "task_t is not for child process";
    return Err(LaunchError("task_pid"));
  }

  // We've received the task_t for the correct process, reply to the message
  // with any send rights over to that child process which they should have on
  // startup.
  size_t reply_size = sizeof(mach_msg_base_t) +
                      sizeof(mach_msg_port_descriptor_t) * send_rights.size();
  mozilla::UniquePtr<uint8_t[]> buffer =
      mozilla::MakeUnique<uint8_t[]>(reply_size);
  mach_msg_base_t* reply = reinterpret_cast<mach_msg_base_t*>(buffer.get());
  reply->header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0) | MACH_MSGH_BITS_COMPLEX;
  reply->header.msgh_size = reply_size;
  reply->header.msgh_remote_port = request.header.msgh_remote_port;
  reply->body.msgh_descriptor_count = send_rights.size();

  // Fill the descriptors from our mChildArgs.
  mach_msg_port_descriptor_t* descrs =
      reinterpret_cast<mach_msg_port_descriptor_t*>(reply + 1);
  for (size_t i = 0; i < send_rights.size(); ++i) {
    descrs[i].type = MACH_MSG_PORT_DESCRIPTOR;
    descrs[i].disposition = MACH_MSG_TYPE_COPY_SEND;
    descrs[i].name = send_rights[i].get();
  }

  // Send the reply.
  kr = mach_msg(&reply->header, MACH_SEND_MSG, reply->header.msgh_size, 0,
                MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    // NOTE: The only port which `mach_msg_destroy` would destroy is
    // `header.msgh_remote_port`, which is actually owned by `request`, so we
    // don't want to call `mach_msg_destroy` on the reply here.
    //
    // If we ever support passing receive rights, we'll need to make sure to
    // clean them up here, as their ownership must be moved into the message.
    CHROMIUM_LOG(ERROR) << "parent mach_msg(MACH_SEND_MSG) failed: "
                        << FormatMachError(kr);
    return Err(LaunchError("mach_msg(MACH_SEND_MSG)", kr));
  }

  // At this point, we've transferred the reply port, and are now taking the
  // mach task port from the request message (to pass to our caller), no longer
  // destroy the request message on error.
  *child_task = request.data.name;
  destroyRequestMessage.release();

  return Ok();
}
#endif
