/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[CheckPermissions="bluetooth"]
interface BluetoothGattDescriptor
{
  readonly attribute BluetoothGattCharacteristic            characteristic;
  readonly attribute DOMString                              uuid;
  readonly attribute ArrayBuffer?                           value;

  [NewObject]
  Promise<ArrayBuffer>  readValue();
  [NewObject]
  Promise<void>         writeValue(ArrayBuffer value);
};
