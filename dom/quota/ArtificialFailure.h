/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_ARTIFICIALFAILURE_H_
#define DOM_QUOTA_ARTIFICIALFAILURE_H_

#include <cstdint>

#include "nsIQuotaArtificialFailure.h"

enum class nsresult : uint32_t;

namespace mozilla {

struct Ok;
template <typename V, typename E>
class Result;

}  // namespace mozilla

namespace mozilla::dom::quota {

/**
 * Checks if an artificial failure should be triggered based on the specified
 * category and the configured probability.
 *
 * This method evaluates if the provided failure category matches the
 * categories set in the preferences. If a match is found, it then checks
 * the probability of triggering an artificial failure. A random value is
 * generated to determine if the failure should occur based on this
 * probability. If both the category matches and the random value falls within
 * the defined probability, the method returns an error code indicating the
 * artificial failure. Otherwise, it returns a successful result.
 *
 * @param aCategory - The failure category to check against the configured
 *   categories for triggering an artificial failure. It must have only one bit
 *   set.
 * @returns Result<Ok, nsresult> - An Ok result if no failure occurs; an Err
 *   result containing an error code if an artificial failure is triggered.
 *
 * Note:
 * Consider replacing the preferences with a dedicated class with static
 * methods for entering and leaving artificial failure mode, something like
 * `ChaosMode`. The class would also implement an interface, for example
 * `nsIQuotaArtificialFailure` allowing access from scripts.
 *
 * Example usage:
 * This example demonstrates the usage of `ArtificialFailure` in conjunction
 * with the `QM_TRY` macro to handle potential artificial failures gracefully.
 * The `QM_TRY` macro will return early if an artificial failure occurs, with
 * the corresponding error code from `ArtificialFailure`.
 *
 * ```cpp
 * QM_TRY(ArtificialFailure(
 *     nsIQuotaArtificialFailure::CATEGORY_INITIALIZE_ORIGIN));
 * ```
 */
Result<Ok, nsresult> ArtificialFailure(
    nsIQuotaArtificialFailure::Category aCategory);

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_ARTIFICIALFAILURE_H_
