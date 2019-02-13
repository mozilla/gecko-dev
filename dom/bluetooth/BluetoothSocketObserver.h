/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothSocketObserver_h
#define mozilla_dom_bluetooth_BluetoothSocketObserver_h

#include "BluetoothCommon.h"
#include "mozilla/ipc/SocketBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;

class BluetoothSocketObserver
{
public:
  virtual void ReceiveSocketData(
    BluetoothSocket* aSocket,
    nsAutoPtr<mozilla::ipc::UnixSocketBuffer>& aBuffer) = 0;

   /**
    * A callback function which would be called when a socket connection
    * is established successfully. To be more specific, this would be called
    * when socket state changes from CONNECTING/LISTENING to CONNECTED.
    */
  virtual void OnSocketConnectSuccess(BluetoothSocket* aSocket) = 0;

   /**
    * A callback function which would be called when BluetoothSocket::Connect()
    * fails.
    */
  virtual void OnSocketConnectError(BluetoothSocket* aSocket) = 0;

   /**
    * A callback function which would be called when a socket connection
    * is dropped. To be more specific, this would be called when socket state
    * changes from CONNECTED/LISTENING to DISCONNECTED.
    */
  virtual void OnSocketDisconnect(BluetoothSocket* aSocket) = 0;
};

#define BT_DECL_SOCKET_OBSERVER                                             \
public:                                                                     \
  virtual void ReceiveSocketData(BluetoothSocket* aSocket,                  \
    nsAutoPtr<mozilla::ipc::UnixSocketBuffer>& aMessage) override;          \
  virtual void OnSocketConnectSuccess(BluetoothSocket* aSocket) override;   \
  virtual void OnSocketConnectError(BluetoothSocket* aSocket) override;     \
  virtual void OnSocketDisconnect(BluetoothSocket* aSocket) override;

END_BLUETOOTH_NAMESPACE

#endif
