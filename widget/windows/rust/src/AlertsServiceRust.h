/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_WINDOWS_RUST_SRC_ALERTSSERVICERUST_H_
#define WIDGET_WINDOWS_RUST_SRC_ALERTSSERVICERUST_H_

#include "ErrorList.h"
#include "nsID.h"

extern "C" {
nsresult new_windows_alerts_service(REFNSIID iid, void** result);
};

#endif  // WIDGET_WINDOWS_RUST_SRC_ALERTSSERVICERUST_H_
