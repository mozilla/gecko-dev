/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_INTERFACE_HELPERS_ANDROID_H_
#define WEBRTC_MODULES_UTILITY_INTERFACE_HELPERS_ANDROID_H_

#include <jni.h>
#include <string>

// Abort the process if |jni| has a Java exception pending.
// TODO(henrika): merge with CHECK_JNI_EXCEPTION() in jni_helpers.h.
#define CHECK_EXCEPTION(jni)    \
  CHECK(!jni->ExceptionCheck()) \
      << (jni->ExceptionDescribe(), jni->ExceptionClear(), "")

namespace webrtc {

// Return a |JNIEnv*| usable on this thread or NULL if this thread is detached.
JNIEnv* GetEnv(JavaVM* jvm);

// JNIEnv-helper methods that wraps the API which uses the JNI interface
// pointer (JNIEnv*). It allows us to CHECK success and that no Java exception
// is thrown while calling the method.
jmethodID GetMethodID (
    JNIEnv* jni, jclass c, const std::string& name, const char* signature);

jclass FindClass(JNIEnv* jni, const std::string& name);

jobject NewGlobalRef(JNIEnv* jni, jobject o);

void DeleteGlobalRef(JNIEnv* jni, jobject o);

// Return thread ID as a string.
std::string GetThreadId();

// Return thread ID as string suitable for debug logging.
std::string GetThreadInfo();

// Attach thread to JVM if necessary and detach at scope end if originally
// attached.
class AttachThreadScoped {
 public:
  explicit AttachThreadScoped(JavaVM* jvm);
  ~AttachThreadScoped();
  JNIEnv* env();

 private:
  bool attached_;
  JavaVM* jvm_;
  JNIEnv* env_;
};

// Scoped holder for global Java refs.
template<class T>  // T is jclass, jobject, jintArray, etc.
class ScopedGlobalRef {
 public:
  ScopedGlobalRef(JNIEnv* jni, T obj)
      : jni_(jni), obj_(static_cast<T>(NewGlobalRef(jni, obj))) {}
  ~ScopedGlobalRef() {
    DeleteGlobalRef(jni_, obj_);
  }
  T operator*() const {
    return obj_;
  }
 private:
  JNIEnv* jni_;
  T obj_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_UTILITY_INTERFACE_HELPERS_ANDROID_H_
