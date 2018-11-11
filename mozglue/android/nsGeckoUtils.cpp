/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <jni.h>

#include <stdlib.h>
#include <fcntl.h>
#include "APKOpen.h"
#include "Zip.h"
#include "mozilla/RefPtr.h"

extern "C"
__attribute__ ((visibility("default")))
void MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_GeckoLoader_putenv(JNIEnv *jenv, jclass, jstring map)
{
    const char* str;
    // XXX: java doesn't give us true UTF8, we should figure out something
    // better to do here
    str = jenv->GetStringUTFChars(map, nullptr);
    if (str == nullptr)
        return;
    putenv(strdup(str));
    jenv->ReleaseStringUTFChars(map, str);
}

extern "C"
__attribute__ ((visibility("default")))
jobject MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_DirectBufferAllocator_nativeAllocateDirectBuffer(JNIEnv *jenv, jclass, jlong size)
{
    jobject buffer = nullptr;
    void* mem = malloc(size);
    if (mem) {
        buffer = jenv->NewDirectByteBuffer(mem, size);
        if (!buffer)
            free(mem);
    }
    return buffer;
}

extern "C"
__attribute__ ((visibility("default")))
void MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_DirectBufferAllocator_nativeFreeDirectBuffer(JNIEnv *jenv, jclass, jobject buf)
{
    free(jenv->GetDirectBufferAddress(buf));
}

extern "C"
__attribute__ ((visibility("default")))
jlong MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_NativeZip_getZip(JNIEnv *jenv, jclass, jstring path)
{
    const char* str;
    str = jenv->GetStringUTFChars(path, nullptr);
    if (!str || !*str) {
        if (str)
            jenv->ReleaseStringUTFChars(path, str);
        JNI_Throw(jenv, "java/lang/IllegalArgumentException", "Invalid path");
        return 0;
    }
    RefPtr<Zip> zip = ZipCollection::GetZip(str);
    jenv->ReleaseStringUTFChars(path, str);
    if (!zip) {
        JNI_Throw(jenv, "java/lang/IllegalArgumentException", "Invalid path or invalid zip");
        return 0;
    }
    return reinterpret_cast<jlong>(zip.forget().take());
}

extern "C"
__attribute__ ((visibility("default")))
jlong MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_NativeZip_getZipFromByteBuffer(JNIEnv *jenv, jclass, jobject buffer)
{
    void *buf = jenv->GetDirectBufferAddress(buffer);
    size_t size = jenv->GetDirectBufferCapacity(buffer);
    RefPtr<Zip> zip = Zip::Create(buf, size);
    if (!zip) {
        JNI_Throw(jenv, "java/lang/IllegalArgumentException", "Invalid zip");
        return 0;
    }
    return reinterpret_cast<jlong>(zip.forget().take());
}

 extern "C"
__attribute__ ((visibility("default")))
void MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_NativeZip__1release(JNIEnv *jenv, jclass, jlong obj)
{
    Zip *zip = (Zip *)obj;
    zip->Release();
}

extern "C"
__attribute__ ((visibility("default")))
jobject MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_NativeZip__1getInputStream(JNIEnv *jenv, jobject jzip, jlong obj, jstring path)
{
    Zip *zip = (Zip *)obj;
    const char* str;
    str = jenv->GetStringUTFChars(path, nullptr);

    Zip::Stream stream;
    bool res = zip->GetStream(str, &stream);
    jenv->ReleaseStringUTFChars(path, str);
    if (!res) {
        return nullptr;
    }
    jobject buf = jenv->NewDirectByteBuffer(const_cast<void *>(stream.GetBuffer()), stream.GetSize());
    if (!buf) {
        JNI_Throw(jenv, "java/lang/RuntimeException", "Failed to create ByteBuffer");
        return nullptr;
    }
    jclass nativeZip = jenv->GetObjectClass(jzip);
    jmethodID method = jenv->GetMethodID(nativeZip, "createInputStream", "(Ljava/nio/ByteBuffer;I)Ljava/io/InputStream;");
    // Since this function is only expected to be called from Java, it is safe
    // to skip exception checking for the method call below, as long as no
    // other Native -> Java call doesn't happen before returning to Java.
    return jenv->CallObjectMethod(jzip, method, buf, (jint) stream.GetType());
}
