/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothhfpmanager_h__
#define mozilla_dom_bluetooth_bluetoothhfpmanager_h__

#include <hardware/bluetooth.h>
#include <hardware/bt_hf.h>

#include "BluetoothCommon.h"
#include "BluetoothHfpManagerBase.h"
#include "BluetoothRilListener.h"
#include "BluetoothSocketObserver.h"
#include "mozilla/ipc/UnixSocket.h"
#include "mozilla/Hal.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothReplyRunnable;
class BluetoothSocket;
class Call;

/**
 * These costants are defined in 4.33.2 "AT Capabilities Re-Used from GSM 07.07
 * and 3GPP 27.007" in Bluetooth hands-free profile 1.6
 */
enum BluetoothCmeError {
  AG_FAILURE = 0,
  NO_CONNECTION_TO_PHONE = 1,
  OPERATION_NOT_ALLOWED = 3,
  OPERATION_NOT_SUPPORTED = 4,
  PIN_REQUIRED = 5,
  SIM_NOT_INSERTED = 10,
  SIM_PIN_REQUIRED = 11,
  SIM_PUK_REQUIRED = 12,
  SIM_FAILURE = 13,
  SIM_BUSY = 14,
  INCORRECT_PASSWORD = 16,
  SIM_PIN2_REQUIRED = 17,
  SIM_PUK2_REQUIRED = 18,
  MEMORY_FULL = 20,
  INVALID_INDEX = 21,
  MEMORY_FAILURE = 23,
  TEXT_STRING_TOO_LONG = 24,
  INVALID_CHARACTERS_IN_TEXT_STRING = 25,
  DIAL_STRING_TOO_LONG = 26,
  INVALID_CHARACTERS_IN_DIAL_STRING = 27,
  NO_NETWORK_SERVICE = 30,
  NETWORK_TIMEOUT = 31,
  NETWORK_NOT_ALLOWED = 32
};

enum PhoneType {
  NONE, // no connection
  GSM,
  CDMA
};

class Call {
public:
  Call();
  void Reset();
  bool IsActive();

  uint16_t mState;
  nsString mNumber;
  bthf_call_direction_t mDirection; // 0: outgoing call; 1: incoming call
  bthf_call_addrtype_t mType;
};

class BluetoothHfpManager : public BluetoothHfpManagerBase
                          , public BatteryObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  BT_DECL_HFP_MGR_BASE
  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("HFP/HSP");
  }

  static BluetoothHfpManager* Get();
  virtual ~BluetoothHfpManager();

  bool ConnectSco();
  bool DisconnectSco();

  /**
   * @param aSend A boolean indicates whether we need to notify headset or not
   */
  void HandleCallStateChanged(uint32_t aCallIndex, uint16_t aCallState,
                              const nsAString& aError, const nsAString& aNumber,
                              const bool aIsOutgoing, const bool aIsConference,
                              bool aSend);
  void HandleIccInfoChanged(uint32_t aClientId);
  void HandleVoiceConnectionChanged(uint32_t aClientId);

  // Bluedroid hfp callback handlers
  void ProcessConnectionState(bthf_connection_state_t aState, bt_bdaddr_t* aBdAddress);
  void ProcessAudioState(bthf_audio_state_t aState, bt_bdaddr_t* aBdAddress);
  void ProcessAnswerCall();
  void ProcessHangupCall();
  void ProcessVolumeControl(bthf_volume_type_t aType, int aVolume);
  void ProcessDialCall(char *aNumber);
  void ProcessDtmfCmd(char aDtmf);
  void ProcessAtChld(bthf_chld_type_t aChld);
  void ProcessAtCnum();
  void ProcessAtCind();
  void ProcessAtCops();
  void ProcessAtClcc();
  void ProcessUnknownAt(char *aAtString);

  // CDMA-specific functions
  void UpdateSecondNumber(const nsAString& aNumber);
  void AnswerWaitingCall();
  void IgnoreWaitingCall();
  void ToggleCalls();

private:
  class GetVolumeTask;
  class CloseScoTask;
  class RespondToBLDNTask;
  class MainThreadTask;

  friend class BluetoothHfpManagerObserver;
  friend class GetVolumeTask;
  friend class CloseScoTask;
  friend class RespondToBLDNTask;
  friend class MainThreadTask;

  BluetoothHfpManager();
  bool Init();
  bool InitHfpInterface();
  void DeinitHfpInterface();
  void Reset();

  void HandleShutdown();
  void HandleVolumeChanged(const nsAString& aData);
  void Notify(const hal::BatteryInformation& aBatteryInfo);

  void NotifyConnectionStateChanged(const nsAString& aType);
  void NotifyDialer(const nsAString& aCommand);

  PhoneType GetPhoneType(const nsAString& aType);
  void ResetCallArray();
  uint32_t FindFirstCall(uint16_t aState);
  uint32_t GetNumberOfCalls(uint16_t aState);
  bthf_call_state_t ConvertToBthfCallState(int aCallState);

  void UpdatePhoneCIND(uint32_t aCallIndex, bool aSend = true);
  void UpdateDeviceCIND();
  void SendCLCC(Call& aCall, int aIndex);
  void SendLine(const char* aMessage);
  void SendResponse(bthf_at_response_t aResponseCode);

  int mConnectionState;
  int mAudioState;
  // Phone CIND
  int mCallSetupState;
  // Device CIND
  int mBattChg;
  int mService;
  int mRoam;
  int mSignal;

  int mCurrentVgs;
  int mCurrentVgm;
  bool mReceiveVgsFlag;
  bool mDialingRequestProcessed;
  PhoneType mPhoneType;
  nsString mDeviceAddress;
  nsString mMsisdn;
  nsString mOperatorName;

  nsTArray<Call> mCurrentCallArray;
  nsAutoPtr<BluetoothRilListener> mListener;
  nsRefPtr<BluetoothProfileController> mController;

  // CDMA-specific variable
  Call mCdmaSecondCall;
};

END_BLUETOOTH_NAMESPACE

#endif
