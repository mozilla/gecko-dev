/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "mozilla/dom/OwnedRustBuffer.h"

namespace mozilla::uniffi {

using dom::ArrayBuffer;

OwnedRustBuffer::OwnedRustBuffer(const RustBuffer& aBuf) : mBuf(aBuf) {}

OwnedRustBuffer OwnedRustBuffer::FromArrayBuffer(
    const ArrayBuffer& aArrayBuffer) {
  return aArrayBuffer.ProcessData(
      [](const Span<uint8_t>& aData,
         JS::AutoCheckCannotGC&&) -> OwnedRustBuffer {
        uint64_t bufLen = aData.Length();

        RustCallStatus status{};
        RustBuffer buf =
            uniffi_rustbuffer_alloc(static_cast<uint64_t>(bufLen), &status);
        buf.len = bufLen;

        // uniffi_rustbuffer_alloc cannot fail within gecko as we build with
        // `panic=abort`, and allocations default to infallible.
        MOZ_RELEASE_ASSERT(status.code == 0,
                           "uniffi_rustbuffer_alloc cannot fail in Gecko");
        memcpy(buf.data, aData.Elements(), bufLen);
        return OwnedRustBuffer(buf);
      });
}

OwnedRustBuffer::OwnedRustBuffer(OwnedRustBuffer&& aOther) : mBuf(aOther.mBuf) {
  aOther.mBuf = RustBuffer{0};
}

OwnedRustBuffer& OwnedRustBuffer::operator=(OwnedRustBuffer&& aOther) {
  if (&aOther != this) {
    FreeData();
  }
  mBuf = aOther.mBuf;
  aOther.mBuf = RustBuffer{0};
  return *this;
}

void OwnedRustBuffer::FreeData() {
  if (IsValid()) {
    RustCallStatus status{};
    uniffi_rustbuffer_free(mBuf, &status);
    MOZ_RELEASE_ASSERT(status.code == 0,
                       "Freeing a rustbuffer should never fail");
    mBuf = {0};
  }
}

OwnedRustBuffer::~OwnedRustBuffer() { FreeData(); }

RustBuffer OwnedRustBuffer::IntoRustBuffer() {
  RustBuffer rv = mBuf;
  mBuf = {};
  return rv;
}

void OwnedRustBuffer::IntoArrayBuffer(JSContext* aCx,
                                      JS::MutableHandle<JSObject*> aResult,
                                      ErrorResult& aError) {
  auto len = mBuf.len;
  void* data = mBuf.data;
  auto userData = MakeUnique<OwnedRustBuffer>(std::move(*this));
  UniquePtr<void, JS::BufferContentsDeleter> dataPtr{
      data, {&ArrayBufferFreeFunc, userData.release()}};

  JS::Rooted<JSObject*> obj(
      aCx, JS::NewExternalArrayBuffer(aCx, len, std::move(dataPtr)));
  if (!obj) {
    aError.NoteJSContextException(aCx);
    return;
  }
  aResult.set(obj);
}

void OwnedRustBuffer::ArrayBufferFreeFunc(void* contents, void* userData) {
  UniquePtr<OwnedRustBuffer> buf{reinterpret_cast<OwnedRustBuffer*>(userData)};
}
}  // namespace mozilla::uniffi
