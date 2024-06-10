/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SECURITY_CSPVIOLATION_H_
#define DOM_SECURITY_CSPVIOLATION_H_

#include <cstdint>

namespace mozilla::dom {
// Represents parts of <https://w3c.github.io/webappsec-csp/#violation>.
struct CSPViolationData {
  const uint32_t mViolatedPolicyIndex;
};
}  // namespace mozilla::dom

#endif  // DOM_SECURITY_CSPVIOLATION_H_
