/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_GVAUTOPLAYREQUESTUTILS_H_
#define DOM_MEDIA_GVAUTOPLAYREQUESTUTILS_H_

#include <cstdint>

#include "mozilla/DefineEnum.h"

namespace mozilla {
namespace dom {

MOZ_DEFINE_ENUM_CLASS_WITH_BASE_AND_TOSTRING(GVAutoplayRequestType, bool,
                                             (eINAUDIBLE, eAUDIBLE));

MOZ_DEFINE_ENUM_CLASS_WITH_BASE_AND_TOSTRING(GVAutoplayRequestStatus, uint32_t,
                                             (eUNKNOWN, eALLOWED, eDENIED,
                                              ePENDING));

}  // namespace dom
}  // namespace mozilla

#endif
