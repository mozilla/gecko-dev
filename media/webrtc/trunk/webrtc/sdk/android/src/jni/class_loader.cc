/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/class_loader.h"

#include <algorithm>
#include <string>

#include "rtc_base/checks.h"
#include "sdk/android/generated_base_jni/jni/WebRtcClassLoader_jni.h"

// Abort the process if |jni| has a Java exception pending. This macros uses the
// comma operator to execute ExceptionDescribe and ExceptionClear ignoring their
// return values and sending "" to the error stream.
#define CHECK_EXCEPTION(jni)        \
  RTC_CHECK(!jni->ExceptionCheck()) \
      << (jni->ExceptionDescribe(), jni->ExceptionClear(), "")

namespace webrtc {
namespace jni {

namespace {

class ClassLoader {
 public:
  explicit ClassLoader(JNIEnv* env) {
    class_loader_class_ = reinterpret_cast<jclass>(
        env->NewGlobalRef(env->FindClass("java/lang/ClassLoader")));
    CHECK_EXCEPTION(env);
    load_class_method_ =
        env->GetMethodID(class_loader_class_, "loadClass",
                         "(Ljava/lang/String;)Ljava/lang/Class;");
    CHECK_EXCEPTION(env);
    class_loader_ =
        env->NewGlobalRef(Java_WebRtcClassLoader_getClassLoader(env));
    CHECK_EXCEPTION(env);
  }

  jclass FindClass(JNIEnv* env, const char* c_name) {
    // ClassLoader.loadClass expects a classname with components separated by
    // dots instead of the slashes that JNIEnv::FindClass expects.
    std::string name(c_name);
    std::replace(name.begin(), name.end(), '/', '.');
    jstring jstr = env->NewStringUTF(name.c_str());
    const jclass clazz = static_cast<jclass>(
        env->CallObjectMethod(class_loader_, load_class_method_, jstr));
    CHECK_EXCEPTION(env);
    return clazz;
  }

 private:
  jclass class_loader_class_;
  jmethodID load_class_method_;
  jobject class_loader_;
};

static ClassLoader* g_class_loader = nullptr;

}  // namespace

void InitClassLoader(JNIEnv* env) {
  RTC_CHECK(g_class_loader == nullptr);
  g_class_loader = new ClassLoader(env);
}

jclass GetClass(JNIEnv* env, const char* name) {
  // The class loader will be null in the JNI code called from the ClassLoader
  // ctor when we are bootstrapping ourself.
  return (g_class_loader == nullptr) ? env->FindClass(name)
                                     : g_class_loader->FindClass(env, name);
}

}  // namespace jni
}  // namespace webrtc
