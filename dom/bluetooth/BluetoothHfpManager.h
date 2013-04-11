/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothhfpmanager_h__
#define mozilla_dom_bluetooth_bluetoothhfpmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothSocketObserver.h"
#include "BluetoothRilListener.h"
#include "mozilla/ipc/UnixSocket.h"
#include "nsIObserver.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHfpManagerObserver;
class BluetoothReplyRunnable;
class BluetoothSocket;

class BluetoothHfpManager : public BluetoothSocketObserver
{
public:
  static BluetoothHfpManager* Get();

  ~BluetoothHfpManager();
  virtual void ReceiveSocketData(
    BluetoothSocket* aSocket,
    nsAutoPtr<mozilla::ipc::UnixSocketRawData>& aMessage) MOZ_OVERRIDE;
  virtual void OnConnectSuccess(BluetoothSocket* aSocket) MOZ_OVERRIDE;
  virtual void OnConnectError(BluetoothSocket* aSocket) MOZ_OVERRIDE;
  virtual void OnDisconnect(BluetoothSocket* aSocket) MOZ_OVERRIDE;

  bool Connect(const nsAString& aDeviceObjectPath,
               const bool aIsHandsfree,
               BluetoothReplyRunnable* aRunnable);
  void Disconnect();
  bool SendLine(const char* aMessage);
  bool SendCommand(const char* aCommand, const int aValue);
  void CallStateChanged(int aCallIndex, int aCallState,
                        const char* aNumber, bool aIsActive);
  void EnumerateCallState(int aCallIndex, int aCallState,
                          const char* aNumber, bool aIsActive);
  void SetupCIND(int aCallIndex, int aCallState,
                 const char* aPhoneNumber, bool aInitial);
  bool Listen();
  void SetVolume(int aVolume);
  bool IsConnected();

private:
  friend class BluetoothHfpManagerObserver;
  BluetoothHfpManager();
  nsresult HandleIccInfoChanged();
  nsresult HandleShutdown();
  nsresult HandleVolumeChanged(const nsAString& aData);
  nsresult HandleVoiceConnectionChanged();

  bool Init();
  void Cleanup();
  void NotifyDialer(const nsAString& aCommand);
  void NotifySettings();

  int mCurrentVgs;
  int mCurrentCallIndex;
  bool mCLIP;
  bool mReceiveVgsFlag;
  nsString mDevicePath;
  nsString mMsisdn;
  SocketConnectionStatus mPrevSocketStatus;
  nsTArray<int> mCurrentCallStateArray;
  nsAutoPtr<BluetoothRilListener> mListener;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
  nsRefPtr<BluetoothSocket> mSocket;
};

END_BLUETOOTH_NAMESPACE

#endif
