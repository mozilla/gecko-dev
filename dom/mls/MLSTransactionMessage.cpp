/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/Assertions.h"
#include "mozilla/security/mls/mls_gk_ffi_generated.h"
#include "MLSTransactionMessage.h"
#include "mozilla/dom/MLSTransactionMessage.h"

using namespace mozilla::security::mls;

void IPC::ParamTraits<mozilla::security::mls::GkReceived>::Write(
    MessageWriter* aWriter, const paramType& aValue) {
  // Serialize the enum variant tag
  IPC::WriteParam(aWriter, aValue.tag);

  switch (aValue.tag) {
    case paramType::Tag::ApplicationMessage:
      IPC::WriteParam(aWriter, aValue.application_message._0);
      break;
    case paramType::Tag::GroupIdEpoch:
      IPC::WriteParam(aWriter, aValue.group_id_epoch._0);
      break;
    case paramType::Tag::CommitOutput:
      IPC::WriteParam(aWriter, aValue.commit_output._0);
      break;
    default:
      break;
  }
}

bool IPC::ParamTraits<mozilla::security::mls::GkReceived>::Read(
    MessageReader* aReader, paramType* aResult) {
  MOZ_ASSERT(aResult->tag == paramType::Tag::None,
             "Clobbering already-initialized result");

  // Deserialize the tag
  if (!IPC::ReadParam(aReader, &aResult->tag)) {
    // Ensure that the tag has a safe value for the destructor before returning.
    aResult->tag = paramType::Tag::None;
    return false;
  }

  // Use placement-new to initialize the relevant variant of the internal union,
  // then deserialize the bodies.
  switch (aResult->tag) {
    case paramType::Tag::None:
      return true;  // No data payload
    case paramType::Tag::ApplicationMessage:
      new (&aResult->application_message) paramType::ApplicationMessage_Body;
      return IPC::ReadParam(aReader, &aResult->application_message._0);
    case paramType::Tag::GroupIdEpoch:
      new (&aResult->group_id_epoch) paramType::GroupIdEpoch_Body;
      return IPC::ReadParam(aReader, &aResult->group_id_epoch._0);
    case paramType::Tag::CommitOutput:
      new (&aResult->commit_output) paramType::CommitOutput_Body;
      return IPC::ReadParam(aReader, &aResult->commit_output._0);
  }
  return false;
}
