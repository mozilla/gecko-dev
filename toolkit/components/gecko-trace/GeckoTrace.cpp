/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <vector>

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/Logging.h"

#ifdef DEBUG
#  include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#endif
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"

#include "GeckoTrace.h"

namespace mozilla::gecko_trace {

namespace otel_trace_api = opentelemetry::trace;
namespace otel_trace_sdk = opentelemetry::sdk::trace;
namespace otel_internal_log = opentelemetry::sdk::common::internal_log;

namespace {
static mozilla::LazyLogModule sOpenTelemetryLog("opentelemetry");

class OtelLogHandler final : public otel_internal_log::LogHandler {
 public:
  void Handle(otel_internal_log::LogLevel aLevel, const char* aFile, int aLine,
              const char* aMsg,
              const opentelemetry::sdk::common::AttributeMap&
                  aAttributes) noexcept override {
    mozilla::LogLevel mozLogLevel;

    switch (aLevel) {
      case otel_internal_log::LogLevel::Error:
        mozLogLevel = mozilla::LogLevel::Error;
        break;
      case otel_internal_log::LogLevel::Warning:
        mozLogLevel = mozilla::LogLevel::Warning;
        break;
      case otel_internal_log::LogLevel::Info:
        mozLogLevel = mozilla::LogLevel::Info;
        break;
      case otel_internal_log::LogLevel::Debug:
        mozLogLevel = mozilla::LogLevel::Debug;
        break;
      default:
        mozLogLevel = mozilla::LogLevel::Disabled;
        break;
    }

    MOZ_LOG(sOpenTelemetryLog, mozLogLevel, ("%s", aMsg));
  };
};
}  // namespace

void SetOpenTelemetryInternalLogLevel(mozilla::LogLevel aLogLevel) {
  otel_internal_log::LogLevel otelLogLevel;

  switch (aLogLevel) {
    case mozilla::LogLevel::Error:
      otelLogLevel = otel_internal_log::LogLevel::Error;
      break;
    case mozilla::LogLevel::Warning:
      otelLogLevel = otel_internal_log::LogLevel::Warning;
      break;
    case mozilla::LogLevel::Info:
      otelLogLevel = otel_internal_log::LogLevel::Info;
      break;
    case mozilla::LogLevel::Debug:
      [[fallthrough]];
    case mozilla::LogLevel::Verbose:
      // OpenTelemetry does not differentiate between debug and verbose
      otelLogLevel = otel_internal_log::LogLevel::Debug;
      break;
    case LogLevel::Disabled:
      otelLogLevel = otel_internal_log::LogLevel::None;
      break;
  }

  otel_internal_log::GlobalLogHandler::SetLogLevel(otelLogLevel);
}

void Init() {
  otel_internal_log::GlobalLogHandler::SetLogHandler(
      std::make_shared<OtelLogHandler>(OtelLogHandler()));

  std::vector<std::unique_ptr<otel_trace_sdk::SpanProcessor>> processors;

#ifdef DEBUG
  if (mozilla::EnvHasValue("GECKO_TRACE_EXPORT_SPANS_TO_STDOUT")) {
    auto ostreamExporter =
        opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
    auto ostreamProcessor = otel_trace_sdk::SimpleSpanProcessorFactory::Create(
        std::move(ostreamExporter));
    processors.push_back(std::move(ostreamProcessor));
  }
#endif

  // We should overload the `otel_trace_sdk::TracerProviderFactory::Create`
  // here once the implementation and testing are more complete.
  std::shared_ptr<otel_trace_api::TracerProvider> provider =
      otel_trace_sdk::TracerProviderFactory::Create(std::move(processors));

  otel_trace_api::Provider::SetTracerProvider(std::move(provider));
}

}  // namespace mozilla::gecko_trace
