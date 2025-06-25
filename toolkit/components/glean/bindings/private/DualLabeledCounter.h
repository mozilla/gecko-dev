/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_DualLabeledCounter_h
#define mozilla_glean_DualLabeledCounter_h

#include "nsISupports.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/glean/bindings/Counter.h"

namespace mozilla::glean {

namespace impl {

class DualLabeledCounterMetric {
 public:
  constexpr explicit DualLabeledCounterMetric(uint32_t id) : mId(id) {}

  /**
   * Gets a specific metric for a given key and category.
   *
   * If a set of acceptable labels were specified in the `metrics.yaml` file,
   * and the given label is not in the set, it will be recorded under the
   * special `OTHER_LABEL` label.
   *
   * If a set of acceptable labels was not specified in the `metrics.yaml` file,
   * only the first 16 unique labels will be used.
   * After that, any additional labels will be recorded under the special
   * `OTHER_LABEL` label.
   *
   * This applies to both key labels and category labels.
   *
   * @param aKey - A UTF-8 label of at most 111 bytes of length,
   *               otherwise the metric will be recorded under the special
   *               `OTHER_LABEL` key and an error will be recorded.
   * @param aCategory - A UTF-8 label of at most 111 bytes of length,
   *                    otherwise the metric will be recorded under the special
   *                    `OTHER_LABEL` key and an error will be recorded.
   */
  CounterMetric<CounterType::eDualLabeled> Get(
      const nsACString& aKey, const nsACString& aCategory) const;

 private:
  const uint32_t mId;
};

}  // namespace impl

class GleanDualLabeledCounter final : public GleanMetric {
 public:
  explicit GleanDualLabeledCounter(uint32_t aId, nsISupports* aParent)
      : GleanMetric(aParent), mId(aId) {}

  virtual JSObject* WrapObject(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  already_AddRefed<GleanCounter> Get(const nsACString& aKey,
                                     const nsACString& aCategory);

 private:
  virtual ~GleanDualLabeledCounter() = default;

  const uint32_t mId;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_DualLabeledCounter_h */
