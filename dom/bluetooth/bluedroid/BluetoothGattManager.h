/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothgattmanager_h__
#define mozilla_dom_bluetooth_bluetoothgattmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothInterface.h"
#include "BluetoothProfileManagerBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothGattClient;
class BluetoothReplyRunnable;

class BluetoothGattManager final : public nsIObserver
                                 , public BluetoothGattNotificationHandler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static BluetoothGattManager* Get();
  static void InitGattInterface(BluetoothProfileResultHandler* aRes);
  static void DeinitGattInterface(BluetoothProfileResultHandler* aRes);

  void StartLeScan(const nsTArray<nsString>& aServiceUuids,
                   BluetoothReplyRunnable* aRunnable);

  void StopLeScan(const nsAString& aScanUuid,
                  BluetoothReplyRunnable* aRunnable);

  void Connect(const nsAString& aAppUuid,
               const nsAString& aDeviceAddr,
               BluetoothReplyRunnable* aRunnable);

  void Disconnect(const nsAString& aAppUuid,
                  const nsAString& aDeviceAddr,
                  BluetoothReplyRunnable* aRunnable);

  void Discover(const nsAString& aAppUuid,
                BluetoothReplyRunnable* aRunnable);

  void UnregisterClient(int aClientIf,
                        BluetoothReplyRunnable* aRunnable);

  void ReadRemoteRssi(int aClientIf,
                      const nsAString& aDeviceAddr,
                      BluetoothReplyRunnable* aRunnable);

  void RegisterNotifications(const nsAString& aAppUuid,
                             const BluetoothGattServiceId& aServId,
                             const BluetoothGattId& aCharId,
                             BluetoothReplyRunnable* aRunnable);

  void DeregisterNotifications(const nsAString& aAppUuid,
                               const BluetoothGattServiceId& aServId,
                               const BluetoothGattId& aCharId,
                               BluetoothReplyRunnable* aRunnable);

  void ReadCharacteristicValue(
    const nsAString& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    BluetoothReplyRunnable* aRunnable);

  void WriteCharacteristicValue(
    const nsAString& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattWriteType& aWriteType,
    const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable);

  void ReadDescriptorValue(
    const nsAString& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattId& aDescriptorId,
    BluetoothReplyRunnable* aRunnable);

  void WriteDescriptorValue(
    const nsAString& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattId& aDescriptorId,
    const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable);

private:
  ~BluetoothGattManager();

  class CleanupResultHandler;
  class CleanupResultHandlerRunnable;
  class InitGattResultHandler;
  class RegisterClientResultHandler;
  class UnregisterClientResultHandler;
  class StartLeScanResultHandler;
  class StopLeScanResultHandler;
  class ConnectResultHandler;
  class DisconnectResultHandler;
  class DiscoverResultHandler;
  class ReadRemoteRssiResultHandler;
  class RegisterNotificationsResultHandler;
  class DeregisterNotificationsResultHandler;
  class ReadCharacteristicValueResultHandler;
  class WriteCharacteristicValueResultHandler;
  class ReadDescriptorValueResultHandler;
  class WriteDescriptorValueResultHandler;

  BluetoothGattManager();

  void HandleShutdown();

  void RegisterClientNotification(BluetoothGattStatus aStatus,
                                  int aClientIf,
                                  const BluetoothUuid& aAppUuid) override;

  void ScanResultNotification(
    const nsAString& aBdAddr, int aRssi,
    const BluetoothGattAdvData& aAdvData) override;

  void ConnectNotification(int aConnId,
                           BluetoothGattStatus aStatus,
                           int aClientIf,
                           const nsAString& aBdAddr) override;

  void DisconnectNotification(int aConnId,
                              BluetoothGattStatus aStatus,
                              int aClientIf,
                              const nsAString& aBdAddr) override;

  void SearchCompleteNotification(int aConnId,
                                  BluetoothGattStatus aStatus) override;

  void SearchResultNotification(int aConnId,
                                const BluetoothGattServiceId& aServiceId)
                                override;

  void GetCharacteristicNotification(
    int aConnId, BluetoothGattStatus aStatus,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    const BluetoothGattCharProp& aCharProperty) override;

  void GetDescriptorNotification(
    int aConnId, BluetoothGattStatus aStatus,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    const BluetoothGattId& aDescriptorId) override;

  void GetIncludedServiceNotification(
    int aConnId, BluetoothGattStatus aStatus,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattServiceId& aIncludedServId) override;

  void RegisterNotificationNotification(
    int aConnId, int aIsRegister, BluetoothGattStatus aStatus,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId) override;

  void NotifyNotification(int aConnId,
                          const BluetoothGattNotifyParam& aNotifyParam)
                          override;

  void ReadCharacteristicNotification(int aConnId,
                                      BluetoothGattStatus aStatus,
                                      const BluetoothGattReadParam& aReadParam)
                                      override;

  void WriteCharacteristicNotification(
    int aConnId, BluetoothGattStatus aStatus,
    const BluetoothGattWriteParam& aWriteParam) override;

  void ReadDescriptorNotification(int aConnId,
                                  BluetoothGattStatus aStatus,
                                  const BluetoothGattReadParam& aReadParam)
                                  override;

  void WriteDescriptorNotification(int aConnId,
                                   BluetoothGattStatus aStatus,
                                   const BluetoothGattWriteParam& aWriteParam)
                                   override;

  void ExecuteWriteNotification(int aConnId,
                                BluetoothGattStatus aStatus) override;

  void ReadRemoteRssiNotification(int aClientIf,
                                  const nsAString& aBdAddr,
                                  int aRssi,
                                  BluetoothGattStatus aStatus) override;

  void ListenNotification(BluetoothGattStatus aStatus,
                          int aServerIf) override;

  void ProceedDiscoverProcess(BluetoothGattClient* aClient,
                              const BluetoothGattServiceId& aServiceId);

  static bool mInShutdown;
};

END_BLUETOOTH_NAMESPACE

#endif
