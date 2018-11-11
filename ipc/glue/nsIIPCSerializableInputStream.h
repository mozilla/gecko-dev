/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_nsIIPCSerializableInputStream_h
#define mozilla_ipc_nsIIPCSerializableInputStream_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {
namespace ipc {

class FileDescriptor;
class InputStreamParams;

} // namespace ipc
} // namespace mozilla

#define NS_IIPCSERIALIZABLEINPUTSTREAM_IID \
  {0xb0211b14, 0xea6d, 0x40d4, {0x87, 0xb5, 0x7b, 0xe3, 0xdf, 0xac, 0x09, 0xd1}}

class NS_NO_VTABLE nsIIPCSerializableInputStream : public nsISupports
{
public:
  typedef nsTArray<mozilla::ipc::FileDescriptor>
          FileDescriptorArray;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IIPCSERIALIZABLEINPUTSTREAM_IID)

  virtual void
  Serialize(mozilla::ipc::InputStreamParams& aParams,
            FileDescriptorArray& aFileDescriptors) = 0;

  virtual bool
  Deserialize(const mozilla::ipc::InputStreamParams& aParams,
              const FileDescriptorArray& aFileDescriptors) = 0;

  // The number of bytes that are expected to be written when this
  // stream is serialized. A value of Some(N) indicates that N bytes
  // will be written to the IPC buffer, and will be used to decide
  // upon an optimal transmission mechanism. A value of Nothing
  // indicates that either serializing this stream will not require
  // serializing its contents (eg. a file-backed stream, or a stream
  // backed by an IPC actor), or the length of the stream's contents
  // cannot be determined.
  virtual mozilla::Maybe<uint64_t>
  ExpectedSerializedLength() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIIPCSerializableInputStream,
                              NS_IIPCSERIALIZABLEINPUTSTREAM_IID)

#define NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM                                  \
  virtual void                                                                 \
  Serialize(mozilla::ipc::InputStreamParams&,                                  \
            FileDescriptorArray&) override;                                    \
                                                                               \
  virtual bool                                                                 \
  Deserialize(const mozilla::ipc::InputStreamParams&,                          \
              const FileDescriptorArray&) override;                            \
                                                                               \
  virtual mozilla::Maybe<uint64_t>                                             \
  ExpectedSerializedLength() override;

#define NS_FORWARD_NSIIPCSERIALIZABLEINPUTSTREAM(_to)                          \
  virtual void                                                                 \
  Serialize(mozilla::ipc::InputStreamParams& aParams,                          \
            FileDescriptorArray& aFileDescriptors) override                    \
  {                                                                            \
    _to Serialize(aParams, aFileDescriptors);                                  \
  }                                                                            \
                                                                               \
  virtual bool                                                                 \
  Deserialize(const mozilla::ipc::InputStreamParams& aParams,                  \
              const FileDescriptorArray& aFileDescriptors) override            \
  {                                                                            \
    return _to Deserialize(aParams, aFileDescriptors);                         \
  }                                                                            \
                                                                               \
  virtual mozilla::Maybe<uint64_t>                                             \
  ExpectedSerializedLength() override                                          \
  {                                                                            \
    return _to ExpectedSerializedLength();                                     \
  }

#define NS_FORWARD_SAFE_NSIIPCSERIALIZABLEINPUTSTREAM(_to)                     \
  virtual void                                                                 \
  Serialize(mozilla::ipc::InputStreamParams& aParams,                          \
            FileDescriptorArray& aFileDescriptors) override                    \
  {                                                                            \
    if (_to) {                                                                 \
      _to->Serialize(aParams, aFileDescriptors);                               \
    }                                                                          \
  }                                                                            \
                                                                               \
  virtual bool                                                                 \
  Deserialize(const mozilla::ipc::InputStreamParams& aParams,                  \
              const FileDescriptorArray& aFileDescriptors) override            \
  {                                                                            \
    return _to ? _to->Deserialize(aParams, aFileDescriptors) : false;          \
  }                                                                            \
                                                                               \
  virtual mozilla::Maybe<uint64_t>                                             \
  ExpectedSerializedLength() override                                          \
  {                                                                            \
    return _to ? _to->ExpectedSerializedLength() : Nothing();                  \
  }

#endif // mozilla_ipc_nsIIPCSerializableInputStream_h
