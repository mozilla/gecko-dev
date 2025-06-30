/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/DualLabeledCounter.h"

#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/glean/bindings/GleanJSMetricsLookup.h"
#include "mozilla/glean/bindings/MetricTypes.h"
#include "nsString.h"

namespace mozilla::glean {

namespace impl {

CounterMetric<CounterType::eDualLabeled> DualLabeledCounterMetric::Get(
    const nsACString& aKey, const nsACString& aCategory) const {
  uint32_t submetricId = fog_dual_labeled_counter_get(mId, &aKey, &aCategory);
  return CounterMetric<CounterType::eDualLabeled>(submetricId);
}

}  // namespace impl

/* virtual */
JSObject* GleanDualLabeledCounter::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanDualLabeledCounter_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<GleanCounter> GleanDualLabeledCounter::Get(
    const nsACString& aKey, const nsACString& aCategory) {
  uint32_t submetricId =
      impl::fog_dual_labeled_counter_get(mId, &aKey, &aCategory);

  return MakeAndAddRef<GleanCounter>(submetricId, mParent,
                                     impl::CounterType::eDualLabeled);
}

}  // namespace mozilla::glean
