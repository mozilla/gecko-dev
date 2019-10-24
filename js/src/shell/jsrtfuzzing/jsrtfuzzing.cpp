/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shell/jsrtfuzzing/jsrtfuzzing.h"

#include "mozilla/ArrayUtils.h"  // mozilla::ArrayLength
#include "mozilla/ScopeExit.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "FuzzerDefs.h"
#include "FuzzingInterface.h"

#include "jsapi.h"

#include "js/CompilationAndEvaluation.h"  // JS::Evaluate
#include "js/Equality.h"
#include "js/SourceText.h"  // JS::Source{Ownership,Text}

#include "vm/Interpreter.h"
#include "vm/TypedArrayObject.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/JSContext-inl.h"

using namespace js;

namespace js {
namespace shell {

static JSContext* gCx = nullptr;
static std::string gFuzzModuleName;

static void CrashOnPendingException() {
  if (JS_IsExceptionPending(gCx)) {
    RootedValue exn(gCx);
    (void)JS_GetPendingException(gCx, &exn);
    RootedObject stack(gCx, GetPendingExceptionStack(gCx));

    JS_ClearPendingException(gCx);

    js::ErrorReport report(gCx);
    if (!report.init(gCx, exn, js::ErrorReport::WithSideEffects)) {
      fprintf(stderr, "out of memory initializing ErrorReport\n");
      fflush(stderr);
    } else {
      PrintError(gCx, stderr, report.toStringResult(), report.report(),
                 reportWarnings);
      if (!PrintStackTrace(gCx, stack)) {
        fputs("(Unable to print stack trace)\n", stderr);
      }
    }

    MOZ_CRASH("Unhandled exception from JS runtime!");
  }
}

int FuzzJSRuntimeStart(JSContext* cx, int* argc, char*** argv) {
  gCx = cx;
  gFuzzModuleName = getenv("FUZZER");

  int ret = FuzzJSRuntimeInit(argc, argv);
  if (ret) {
    fprintf(stderr, "Fuzzing Interface: Error: Initialize callback failed\n");
    return ret;
  }

#ifdef LIBFUZZER
  fuzzer::FuzzerDriver(&sArgc, &sArgv, FuzzJSRuntimeFuzz);
#elif __AFL_COMPILER
  MOZ_CRASH("AFL is unsupported for JS runtime fuzzing integration");
#endif
  return 0;
}

int FuzzJSRuntimeInit(int* argc, char*** argv) {
  JS::Rooted<JS::Value> v(gCx);
  JS::CompileOptions opts(gCx);

  // Load the fuzzing module specified in the FUZZER environment variable
  JS::EvaluateUtf8Path(gCx, opts, gFuzzModuleName.c_str(), &v);

  // Any errors while loading the fuzzing module should be fatal
  CrashOnPendingException();

  return 0;
}

int FuzzJSRuntimeFuzz(const uint8_t* buf, size_t size) {
  if (!size) {
    return 0;
  }

  JS::Rooted<JSObject*> arr(gCx, JS_NewUint8ClampedArray(gCx, size));
  if (!arr) {
    MOZ_CRASH("OOM");
  }

  do {
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    uint8_t* data = JS_GetUint8ClampedArrayData(arr, &isShared, nogc);
    MOZ_RELEASE_ASSERT(!isShared);
    memcpy(data, buf, size);
  } while (false);

  JS::RootedValue arrVal(gCx, JS::ObjectValue(*arr));
  if (!JS_SetProperty(gCx, gCx->global(), "fuzzBuf", arrVal)) {
    MOZ_CRASH("JS_SetProperty failed");
  }

  JS::Rooted<JS::Value> v(gCx);
  JS::CompileOptions opts(gCx);

  static const char data[] = "JSFuzzIterate();";

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  if (!srcBuf.init(gCx, data, mozilla::ArrayLength(data) - 1,
                   JS::SourceOwnership::Borrowed)) {
    return 0;
  }

  JS::Evaluate(gCx, opts.setFileAndLine(__FILE__, __LINE__), srcBuf, &v);

  // The fuzzing module is required to handle any exceptions
  CrashOnPendingException();

  return 0;
}

}  // namespace shell
}  // namespace js
