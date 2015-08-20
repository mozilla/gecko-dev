/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TelephonyBinding.h"
#include "nsITelephonyService.h"

namespace mozilla {
namespace dom {
namespace telephony {

#define ASSERT_EQUALITY(webidlType, webidlState, xpidlState) \
  static_assert(static_cast<uint32_t>(webidlType::webidlState) == nsITelephonyService::xpidlState, \
  #webidlType "::" #webidlState " should equal to nsITelephonyService::" #xpidlState)

/**
 * Enum TtyMode
 */
#define ASSERT_TTY_MODE_EQUALITY(webidlState, xpidlState) \
  ASSERT_EQUALITY(TtyMode, webidlState, xpidlState)

ASSERT_TTY_MODE_EQUALITY(Off, TTY_MODE_OFF);
ASSERT_TTY_MODE_EQUALITY(Full, TTY_MODE_FULL);
ASSERT_TTY_MODE_EQUALITY(Hco, TTY_MODE_HCO);
ASSERT_TTY_MODE_EQUALITY(Vco, TTY_MODE_VCO);

#undef ASSERT_SMS_TTY_MODE_EQUALITY

} // namespace telephony
} // namespace dom
} // namespace mozilla
