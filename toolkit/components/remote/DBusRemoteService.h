/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DBusRemoteService_h__
#define __DBusRemoteService_h__

#include "mozilla/Attributes.h"

#ifdef MOZ_ENABLE_DBUS
#include "mozilla/ipc/DBusConnectionRefPtr.h"
#endif

class DBusRemoteService
{
public:
  DBusRemoteService()
#ifdef MOZ_ENABLE_DBUS  
    : mConnection(nullptr)
#endif
    {}

private:
  bool Connect(const char* aAppName, const char* aProfileName);
  void Disconnect();

#ifdef MOZ_ENABLE_DBUS
  RefPtr<DBusConnection> mConnection;
#endif
};

#endif // __nsDBusRemoteService_h__
