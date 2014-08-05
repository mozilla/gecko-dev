/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2014 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef mozilla_pkix__pkixgtest_h
#define mozilla_pkix__pkixgtest_h

#include <ostream>

#include "gtest/gtest.h"
#include "pkix/pkixnss.h"

// PrintTo must be in the same namespace as the type we're overloading it for.
namespace mozilla { namespace pkix {

inline void
PrintTo(const Result& result, ::std::ostream* os)
{
  const char* stringified = MapResultToName(result);
  if (stringified) {
    *os << stringified;
  } else {
    *os << "mozilla::pkix::Result(" << static_cast<unsigned int>(result) << ")";
  }
}

} } // namespace mozilla::pkix

#endif // mozilla_pkix__pkixgtest_h
