/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_PromiseNativeWrapper_h__
#define mozilla_PromiseNativeWrapper_h__

#include "mozilla/MozPromise.h"
#include "mozilla/dom/Promise.h"
#include "nsISupports.h"
#include "mozilla/dom/PromiseNativeHandler.h"

namespace mozilla {

// Helper for wrapping JS promises in MozPromise so they can be handled from C++
// side.
class PromiseNativeWrapper : public dom::PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  explicit PromiseNativeWrapper(
      MozPromiseHolder<GenericNonExclusivePromise>&& aHolder)
      : mHolder(std::move(aHolder)) {}

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    mHolder.Resolve(true, __func__);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    mHolder.Reject(NS_ERROR_FAILURE, __func__);
  }

  static RefPtr<GenericNonExclusivePromise> ConvertJSPromiseToMozPromise(
      const RefPtr<dom::Promise>& jsPromise);

 private:
  ~PromiseNativeWrapper() = default;
  MozPromiseHolder<GenericNonExclusivePromise> mHolder;
};

}  // namespace mozilla

#endif
