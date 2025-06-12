/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Based on chromium's base/logging.cc

#include "base/logging.h"

#include "base/immediate_crash.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include <cstdio>

namespace logging {

static int g_min_log_level = 0;

bool ShouldCreateLogMessage(int severity) {
  return severity >= g_min_log_level;
}

#if BUILDFLAG(IS_WIN)
// This has already been defined in the header, but defining it again as DWORD
// ensures that the type used in the header is equivalent to DWORD. If not,
// the redefinition is a compile error.
typedef DWORD SystemErrorCode;
#endif

SystemErrorCode GetLastSystemErrorCode() {
#if BUILDFLAG(IS_WIN)
  return ::GetLastError();
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return errno;
#endif
}

// For LOGGING_ERROR and above, always print to stderr.
const int kAlwaysPrintErrorLevel = LOGGING_ERROR;

static bool ShouldLogToStderr(int severity) {
  return severity >= kAlwaysPrintErrorLevel;
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity), file_(file), line_(line) {
}

LogMessage::~LogMessage() {
  // Don't let actions from this method affect the system error after returning.
  base::ScopedClearLastError scoped_clear_last_error;

  stream_ << ' ' << '(' << file_ << ':' << line_ << ')' << std::endl;
  std::string str_newline(stream_.str());
  if (ShouldLogToStderr(severity_)) {
    fputs(str_newline.c_str(), stderr);
  }
  else {
    puts(str_newline.c_str());
  }

  if (severity_ == LOGGING_FATAL) {
    base::ImmediateCrash();
  }
}

BASE_EXPORT std::string SystemErrorCodeToString(SystemErrorCode error_code) {
#if BUILDFLAG(IS_WIN)
  LPWSTR msgbuf = nullptr;
  DWORD len = ::FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error_code, 0, reinterpret_cast<LPWSTR>(&msgbuf), 0, nullptr);
  if (len) {
    std::u16string message = base::WideToUTF16(msgbuf);
    ::LocalFree(msgbuf);
    msgbuf = nullptr;
    // Messages returned by system end with line breaks.
    return base::UTF16ToUTF8(base::CollapseWhitespace(message, true)) +
           base::StringPrintf(" (0x%lX)", error_code);
  }
  return base::StringPrintf("Error (0x%lX) while retrieving error. (0x%lX)",
                            GetLastError(), error_code);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return base::safe_strerror(error_code) +
         base::StringPrintf(" (%d)", error_code);
#endif  // BUILDFLAG(IS_WIN)
}

#if BUILDFLAG(IS_WIN)
Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err)
    : LogMessage(file, line, severity), err_(err) {}

Win32ErrorLogMessage::~Win32ErrorLogMessage() {
  // Don't let actions from this method affect the system error after returning.
  base::ScopedClearLastError scoped_clear_last_error;

  stream() << ": " << SystemErrorCodeToString(err_);
}
#endif

}  // namespace logging
