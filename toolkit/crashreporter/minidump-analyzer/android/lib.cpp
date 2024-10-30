/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <cstddef>
#include <cstdint>
#include <jni.h>

namespace {

struct Utf16String {
  const uint16_t* chars;
  size_t len;
};

extern "C" {
Utf16String minidump_analyzer_analyze(const Utf16String& minidump_path,
                                      const Utf16String& extras_path,
                                      bool all_threads);
void minidump_analyzer_free_result(Utf16String result);
}

struct LocalString : Utf16String {
  LocalString(JNIEnv* env, jstring str) : env(env), str(str) {
    chars = env->GetStringChars(str, nullptr);
    len = env->GetStringLength(str);
  }

  ~LocalString() { env->ReleaseStringChars(str, chars); }

  LocalString(const LocalString&) = delete;
  LocalString& operator=(const LocalString&) = delete;

  JNIEnv* env;
  jstring str;
};

struct ForeignString : Utf16String {
  explicit ForeignString(Utf16String s) : Utf16String(s) {}
  ~ForeignString() { minidump_analyzer_free_result(*this); }

  ForeignString(const ForeignString&) = delete;
  ForeignString& operator=(const ForeignString&) = delete;

  explicit operator bool() const { return chars != nullptr; }

  jstring to_jstring(JNIEnv* env) const { return env->NewString(chars, len); }
};

}  // namespace

extern "C" {
JNIEXPORT jstring JNICALL
Java_mozilla_components_lib_crash_MinidumpAnalyzer_analyze(
    JNIEnv* env, jobject obj, jstring minidump_path, jstring extras_path,
    jboolean all_threads) {
  LocalString minidump_str(env, minidump_path);
  LocalString extras_str(env, extras_path);
  if (auto error = ForeignString(
          minidump_analyzer_analyze(minidump_str, extras_str, all_threads))) {
    return error.to_jstring(env);
  }
  return nullptr;
}
}
