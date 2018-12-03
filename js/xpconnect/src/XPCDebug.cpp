/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"

#include "mozilla/Sprintf.h"

#ifdef XP_WIN
#include <windows.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

static void DebugDump(const char* fmt, ...) {
  char buffer[2048];
  va_list ap;
  va_start(ap, fmt);
  VsprintfLiteral(buffer, fmt, ap);
  va_end(ap);
#ifdef XP_WIN
  if (IsDebuggerPresent()) {
    OutputDebugStringA(buffer);
  }
#elif defined(ANDROID)
  __android_log_write(ANDROID_LOG_DEBUG, "Gecko", buffer);
#endif
  printf("%s", buffer);
}

bool xpc_DumpJSStack(bool showArgs, bool showLocals, bool showThisProps) {
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    printf("there is no JSContext on the stack!\n");
  } else if (JS::UniqueChars buf =
                 xpc_PrintJSStack(cx, showArgs, showLocals, showThisProps)) {
    DebugDump("%s\n", buf.get());
  }
  return true;
}

JS::UniqueChars xpc_PrintJSStack(JSContext* cx, bool showArgs, bool showLocals,
                                 bool showThisProps) {
  JS::AutoSaveExceptionState state(cx);

  JS::UniqueChars buf =
      JS::FormatStackDump(cx, showArgs, showLocals, showThisProps);
  if (!buf) {
    DebugDump("%s", "Failed to format JavaScript stack for dump\n");
  }

  state.restore();
  return buf;
}
