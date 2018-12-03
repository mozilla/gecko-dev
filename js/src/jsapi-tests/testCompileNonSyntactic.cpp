/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/GCInternals.h"
#include "js/CompilationAndEvaluation.h"
#include "js/SourceText.h"
#include "jsapi-tests/tests.h"
#include "vm/Monitor.h"
#include "vm/MutexIDs.h"

using namespace JS;
using js::AutoLockMonitor;

struct OffThreadTask {
  OffThreadTask() : monitor(js::mutexid::ShellOffThreadState), token(nullptr) {}

  OffThreadToken* waitUntilDone(JSContext* cx) {
    if (OffThreadParsingMustWaitForGC(cx->runtime())) {
      js::gc::FinishGC(cx);
    }

    AutoLockMonitor alm(monitor);
    while (!token) {
      alm.wait();
    }
    OffThreadToken* result = token;
    token = nullptr;
    return result;
  }

  void markDone(JS::OffThreadToken* tokenArg) {
    AutoLockMonitor alm(monitor);
    token = tokenArg;
    alm.notify();
  }

  static void OffThreadCallback(OffThreadToken* token, void* context) {
    auto self = static_cast<OffThreadTask*>(context);
    self->markDone(token);
  }

  js::Monitor monitor;
  OffThreadToken* token;
};

BEGIN_TEST(testCompileScript) {
  CHECK(testCompile(true));

  CHECK(testCompile(false));
  return true;
}

bool testCompile(bool nonSyntactic) {
  static const char src[] = "42\n";
  static const char16_t src_16[] = u"42\n";

  constexpr size_t length = sizeof(src) - 1;
  static_assert(sizeof(src_16) / sizeof(*src_16) - 1 == length,
                "Source buffers must be same length");

  JS::CompileOptions options(cx);
  options.setNonSyntacticScope(nonSyntactic);

  JS::SourceText<char16_t> buf;
  CHECK(buf.init(cx, src_16, length, JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx);

  // Check explicit non-syntactic compilation first to make sure it doesn't
  // modify our options object.
  CHECK(CompileForNonSyntacticScope(cx, options, buf, &script));
  CHECK_EQUAL(script->hasNonSyntacticScope(), true);

  CHECK(CompileLatin1ForNonSyntacticScope(cx, options, src, length, &script));
  CHECK_EQUAL(script->hasNonSyntacticScope(), true);

  {
    JS::SourceText<char16_t> srcBuf;
    CHECK(srcBuf.init(cx, src_16, length, JS::SourceOwnership::Borrowed));

    CHECK(CompileForNonSyntacticScope(cx, options, srcBuf, &script));
    CHECK_EQUAL(script->hasNonSyntacticScope(), true);
  }

  CHECK(Compile(cx, options, buf, &script));
  CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);

  CHECK(CompileLatin1(cx, options, src, length, &script));
  CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);

  {
    JS::SourceText<char16_t> srcBuf;
    CHECK(srcBuf.init(cx, src_16, length, JS::SourceOwnership::Borrowed));

    CHECK(Compile(cx, options, srcBuf, &script));
    CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);
  }

  options.forceAsync = true;
  OffThreadTask task;
  OffThreadToken* token;

  JS::SourceText<char16_t> srcBuf;
  CHECK(srcBuf.init(cx, src_16, length, JS::SourceOwnership::Borrowed));

  CHECK(CompileOffThread(cx, options, srcBuf, task.OffThreadCallback, &task));
  CHECK(token = task.waitUntilDone(cx));
  CHECK(script = FinishOffThreadScript(cx, token));
  CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);

  return true;
}
END_TEST(testCompileScript);
