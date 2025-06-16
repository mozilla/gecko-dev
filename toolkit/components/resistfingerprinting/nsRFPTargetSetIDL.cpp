/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRFPTargetSetIDL.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(nsRFPTargetSetIDL, nsIRFPTargetSetIDL)

constexpr uint32_t kBits = 128;  // Number of bits in the set

NS_IMETHODIMP
nsRFPTargetSetIDL::GetNth32BitSet(uint32_t aPart, uint32_t* aValue) {
  if (kBits / 32 <= aPart) {
    return NS_ERROR_INVALID_ARG;
  }

  std::bitset<kBits> bitset = mBits.serialize();
  std::bitset<kBits> mask = std::bitset<kBits>(0xFFFFFFFF);
  std::bitset<kBits> part = (bitset >> (aPart * 32)) & mask;
  *aValue = static_cast<uint32_t>(part.to_ulong());
  return NS_OK;
}

}  // namespace mozilla
