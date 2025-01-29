/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TRACE_H
#define GECKO_TRACE_H

#include "mozilla/Logging.h"

namespace mozilla::gecko_trace {

/**
 * @brief Propagates the log level from the Mozilla logging system to the
 * OpenTelemetry internal logging system.
 */
void SetOpenTelemetryInternalLogLevel(mozilla::LogLevel aLogLevel);

/**
 * @brief Initializes the GeckoTrace component.
 *
 * Sets up the OpenTelemetry tracer provider and span processors based on
 * environment variables. Ensures the component is initialized only once,
 * logging a warning on redundant calls.
 *
 * The following environment variables control tracing behavior:
 * - `GECKO_TRACE_EXPORT_SPANS_TO_STDOUT`: If set, exports spans to standard
 * output for debugging purposes.
 *
 * @note Thread-safe and idempotent.
 */
void Init();

};  // namespace mozilla::gecko_trace

#endif  // !GECKO_TRACE_H
