/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MLSTransactionMessage_h
#define mozilla_dom_MLSTransactionMessage_h

#include "nsTArray.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/security/mls/mls_gk_ffi_generated.h"
#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"

using namespace mozilla::security::mls;

namespace IPC {

template <>
struct ParamTraits<mozilla::security::mls::GkReceived::Tag>
    : public ContiguousEnumSerializerInclusive<
          mozilla::security::mls::GkReceived::Tag,
          mozilla::security::mls::GkReceived::Tag::None,
          mozilla::security::mls::GkReceived::Tag::CommitOutput> {};
template <>
struct ParamTraits<mozilla::security::mls::GkReceived> {
  using paramType = mozilla::security::mls::GkReceived;
  static void Write(MessageWriter* aWriter, const paramType& aValue);
  static bool Read(MessageReader* aReader, paramType* aResult);
};

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::security::mls::GkGroupIdEpoch,
                                  group_id, group_epoch);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::security::mls::GkMlsCommitOutput,
                                  commit, welcome, group_info, ratchet_tree,
                                  identity);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::security::mls::GkClientIdentifiers,
                                  identity, credential);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::security::mls::GkGroupMembers,
                                  group_id, group_epoch, group_members);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::security::mls::GkExporterOutput,
                                  group_id, group_epoch, label, context,
                                  exporter);

};  // namespace IPC

#endif  // mozilla_dom_MLSTransactionMessage_h
