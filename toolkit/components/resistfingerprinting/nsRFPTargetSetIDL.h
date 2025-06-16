/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RFPTargetSetIDL_h__
#define mozilla_RFPTargetSetIDL_h__

#include "nsIRFPTargetSetIDL.h"
#include "nsRFPService.h"

namespace mozilla {

class nsRFPTargetSetIDL final : public nsIRFPTargetSetIDL {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRFPTARGETSETIDL

 public:
  nsRFPTargetSetIDL() = default;
  explicit nsRFPTargetSetIDL(RFPTargetSet& aBits) : mBits(aBits) {};

  RFPTargetSet ToRFPTargetSet() const { return mBits; }

 private:
  ~nsRFPTargetSetIDL() = default;

  RFPTargetSet mBits;
};
}  // namespace mozilla

#endif  // mozilla_RFPTargetSetIDL_h__
