/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BASE_DATABASE_H_
#define BASE_DATABASE_H_

#include <cassert>

#include "nss.h"

// TODO(mdauer): Add constructor for initializing with DB.
class NSSDatabase {
 public:
  NSSDatabase() { assert(NSS_NoDB_Init(nullptr) == SECSuccess); }
  ~NSSDatabase() { assert(NSS_Shutdown() == SECSuccess); }
};

#endif  // BASE_DATABASE_H_
