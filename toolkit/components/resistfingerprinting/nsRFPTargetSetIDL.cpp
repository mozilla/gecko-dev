/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRFPTargetSetIDL.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(nsRFPTargetSetIDL, nsIRFPTargetSetIDL)

NS_IMETHODIMP
nsRFPTargetSetIDL::GetLow(uint64_t* aLow) {
  std::bitset<128> bitset = mBits.serialize();
  std::bitset<128> mask = std::bitset<128>(0xFFFFFFFFFFFFFFFF);
  *aLow = (bitset & mask).to_ullong();
  return NS_OK;
}

NS_IMETHODIMP
nsRFPTargetSetIDL::GetHigh(uint64_t* aHigh) {
  std::bitset<128> bitset = mBits.serialize();
  std::bitset<128> mask = std::bitset<128>(0xFFFFFFFFFFFFFFFF);
  *aHigh = ((bitset >> 64) & mask).to_ullong();
  return NS_OK;
}

NS_IMETHODIMP
nsRFPTargetSetIDL::SetLow(uint64_t aLow) {
  std::bitset<128> bitset = mBits.serialize();
  bitset |= aLow;
  mBits.deserialize(bitset);
  return NS_OK;
}

NS_IMETHODIMP
nsRFPTargetSetIDL::SetHigh(uint64_t aHigh) {
  std::bitset<128> bitset = mBits.serialize();
  std::bitset<128> mask = std::bitset<128>(0xFFFFFFFFFFFFFFFF);
  uint64_t low = (bitset & mask).to_ullong();
  bitset = aHigh;
  bitset <<= 64;
  bitset |= low;
  return NS_OK;
}

}  // namespace mozilla
