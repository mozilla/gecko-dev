/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothhfpmanagerbase_h__
#define mozilla_dom_bluetooth_bluetoothhfpmanagerbase_h__

#include "BluetoothProfileManagerBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHfpManagerBase : public BluetoothProfileManagerBase
{
public:
  /**
   * Returns true if Sco is connected.
   */
  virtual bool IsScoConnected() = 0;
};

#define BT_DECL_HFP_MGR_BASE                  \
  BT_DECL_PROFILE_MGR_BASE                    \
  virtual bool IsScoConnected() MOZ_OVERRIDE;

END_BLUETOOTH_NAMESPACE

#endif  //#ifndef mozilla_dom_bluetooth_bluetoothhfpmanagerbase_h__
