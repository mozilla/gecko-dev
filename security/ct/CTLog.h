/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTLog_h
#define CTLog_h

#include <stdint.h>
#include <vector>

namespace mozilla {
namespace ct {

// Signed integer sufficient to store the numeric ID of CT log operators.
// The assigned IDs are 0-based positive integers, so you can use special
// values (such as -1) to indicate a "null" or unknown log ID.
typedef int16_t CTLogOperatorId;

typedef std::vector<CTLogOperatorId> CTLogOperatorList;

}  // namespace ct
}  // namespace mozilla

#endif  // CTLog_h
