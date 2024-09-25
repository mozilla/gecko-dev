/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PromiseNativeWrapper.h"

namespace mozilla {

NS_IMPL_ISUPPORTS0(PromiseNativeWrapper);

RefPtr<GenericNonExclusivePromise>
PromiseNativeWrapper::ConvertJSPromiseToMozPromise(
    const RefPtr<dom::Promise>& jsPromise) {
  MozPromiseHolder<GenericNonExclusivePromise> holder;
  RefPtr<GenericNonExclusivePromise> mozPromise = holder.Ensure(__func__);

  // The handler will resolve mozPromise once jsPromise resolves.
  RefPtr<PromiseNativeWrapper> handler =
      new PromiseNativeWrapper(std::move(holder));
  jsPromise->AppendNativeHandler(handler);

  return mozPromise.forget();
}

}  // namespace mozilla
