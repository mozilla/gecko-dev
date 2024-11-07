/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_NOTIFICATION_IPCUTILS_H_
#define DOM_NOTIFICATION_IPCUTILS_H_

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/NotificationBinding.h"

namespace IPC {

using NotificationDirection = mozilla::dom::NotificationDirection;
template <>
struct ParamTraits<NotificationDirection>
    : public ContiguousEnumSerializerInclusive<NotificationDirection,
                                               NotificationDirection::Auto,
                                               NotificationDirection::Rtl> {};

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::NotificationBehavior, mNoclear,
                                  mNoscreen, mShowOnlyOnce, mSoundFile,
                                  mVibrationPattern);

}  // namespace IPC

#endif  // DOM_NOTIFICATION_IPCUTILS_H_
