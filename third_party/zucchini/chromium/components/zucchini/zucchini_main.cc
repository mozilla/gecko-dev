// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/command_line.h"
#include "base/logging.h"
#if !defined(MOZ_ZUCCHINI)
#include "base/process/memory.h"
#endif  // !defined(MOZ_ZUCCHINI)
#include "build/build_config.h"
#include "components/zucchini/main_utils.h"

#if !defined(MOZ_ZUCCHINI)
#if BUILDFLAG(IS_WIN)
#include "base/win/process_startup_helper.h"
#endif  // BUILDFLAG(IS_WIN)
#endif  // !defined(MOZ_ZUCCHINI)

namespace {

void InitLogging() {
#if !defined(MOZ_ZUCCHINI)
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  settings.log_file_path = nullptr;
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;
  bool logging_res = logging::InitLogging(settings);
  CHECK(logging_res);
#endif  // !defined(MOZ_ZUCCHINI)
}

void InitErrorHandling(const base::CommandLine& command_line) {
#if !defined(MOZ_ZUCCHINI)
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
#if BUILDFLAG(IS_WIN)
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(command_line);
#endif  // BUILDFLAG(IS_WIN)
#endif  // !defined(MOZ_ZUCCHINI)
}

}  // namespace

int main(int argc, const char* argv[]) {
  // Initialize infrastructure from base.
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  InitLogging();
  InitErrorHandling(command_line);
  zucchini::status::Code status =
      RunZucchiniCommand(command_line, std::cout, std::cerr);
  if (!(status == zucchini::status::kStatusSuccess ||
        status == zucchini::status::kStatusInvalidParam)) {
    std::cerr << "Failed with code " << static_cast<int>(status) << std::endl;
  }
  return static_cast<int>(status);
}
