/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothutils_h
#define mozilla_dom_bluetooth_bluetoothutils_h

#include "BluetoothCommon.h"
#include "js/TypeDecls.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothNamedValue;
class BluetoothValue;
class BluetoothReplyRunnable;

bool
SetJsObject(JSContext* aContext,
            const BluetoothValue& aValue,
            JS::Handle<JSObject*> aObj);

bool
BroadcastSystemMessage(const nsAString& aType,
                       const BluetoothValue& aData);

bool
BroadcastSystemMessage(const nsAString& aType,
                       const InfallibleTArray<BluetoothNamedValue>& aData);

void
DispatchBluetoothReply(BluetoothReplyRunnable* aRunnable,
                       const BluetoothValue& aValue,
                       const nsAString& aErrorStr);

void
DispatchStatusChangedEvent(const nsAString& aType,
                           const nsAString& aDeviceAddress,
                           bool aStatus);

//
// BluetoothNamedValue manipulation
//

/**
 * Wrap literal name and value into a BluetoothNamedValue and
 * append it to the array.
 */
void AppendNamedValue(InfallibleTArray<BluetoothNamedValue>& aArray,
                      const char* aName, const BluetoothValue& aValue);

/**
 * Wrap literal name and value into a BluetoothNamedValue and
 * insert it to the array.
 */
void InsertNamedValue(InfallibleTArray<BluetoothNamedValue>& aArray,
                      uint8_t aIndex, const char* aName,
                      const BluetoothValue& aValue);

END_BLUETOOTH_NAMESPACE

#endif
