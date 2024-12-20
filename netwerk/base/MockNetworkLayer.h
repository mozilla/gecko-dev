/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MockNetworkLayer_h__
#define MockNetworkLayer_h__

#include "prerror.h"
#include "prio.h"
#include "ErrorList.h"

namespace mozilla::net {

nsresult AttachMockNetworkLayer(PRFileDesc* fd);

}  // namespace mozilla::net

#endif  // MockNetworkLayer_h__
