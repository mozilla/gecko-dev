/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GIFFTFwd_h
#define mozilla_glean_GIFFTFwd_h

namespace mozilla::Telemetry {
enum class ScalarID : uint32_t;
enum HistogramID : uint32_t;
}  // namespace mozilla::Telemetry

namespace TelemetryScalar {
void Add(mozilla::Telemetry::ScalarID aId, uint32_t aValue);
void Set(mozilla::Telemetry::ScalarID aId, uint32_t aValue);
void Set(mozilla::Telemetry::ScalarID aId, const nsAString& aValue);
void Set(mozilla::Telemetry::ScalarID aId, bool aValue);

void Add(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
         uint32_t aValue);
void Set(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
         uint32_t aValue);
void Set(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, bool aValue);
}  // namespace TelemetryScalar

namespace TelemetryHistogram {
uint8_t GetHistogramType(mozilla::Telemetry::HistogramID aId);
void Accumulate(mozilla::Telemetry::HistogramID aHistogram, uint32_t aSample);
void Accumulate(mozilla::Telemetry::HistogramID aHistogram,
                const nsTArray<uint32_t>& aSamples);
void Accumulate(mozilla::Telemetry::HistogramID aID, const nsCString& aKey,
                uint32_t aSample);
void AccumulateCategorical(mozilla::Telemetry::HistogramID aId,
                           const nsCString& aLabel);
}  // namespace TelemetryHistogram

#endif /* mozilla_glean_GIFFTFwd_h */
