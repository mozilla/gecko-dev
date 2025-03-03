/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/common/ipc_message_utils.h"
#include "mozilla/ipc/SharedMemoryCursor.h"

namespace IPC {

MessageBufferWriter::MessageBufferWriter(MessageWriter* writer,
                                         uint32_t full_len)
    : writer_(writer) {
  // NOTE: We only write out the `shmem_ok` bool if we're over
  // kMessageBufferShmemThreshold to avoid bloating the size of messages with
  // small buffers.
  if (full_len > kMessageBufferShmemThreshold) {
    auto handle = mozilla::ipc::shared_memory::Create(full_len);
    bool shmem_ok = handle.IsValid();
    writer->WriteBool(shmem_ok);
    if (shmem_ok) {
      shmem_cursor_ = mozilla::MakeUnique<mozilla::ipc::shared_memory::Cursor>(
          std::move(handle));
      MOZ_ASSERT(shmem_cursor_->IsValid());
    } else {
      writer->NoteLargeBufferShmemFailure(full_len);
    }
  }
  remaining_ = full_len;
}

MessageBufferWriter::~MessageBufferWriter() {
  if (remaining_ != 0) {
    writer_->FatalError("didn't fully write message buffer");
  }

  // We couldn't write out the shared memory region until now, as the cursor
  // needs to hold on to the handle to potentially re-map sub-regions while
  // writing.
  if (shmem_cursor_) {
    IPC::WriteParam(writer_, shmem_cursor_->TakeHandle());
  }
}

bool MessageBufferWriter::WriteBytes(const void* data, uint32_t len) {
  MOZ_RELEASE_ASSERT(len == remaining_ || (len % 4) == 0,
                     "all writes except for the final write must be a multiple "
                     "of 4 bytes in length due to padding");
  if (len > remaining_) {
    writer_->FatalError("MessageBufferWriter overrun");
    return false;
  }
  remaining_ -= len;
  // If we're serializing using a shared memory region, `shmem_cursor_` will be
  // initialized.
  if (shmem_cursor_) {
    return shmem_cursor_->Write(data, len);
  }
  return writer_->WriteBytes(data, len);
}

MessageBufferReader::MessageBufferReader(MessageReader* reader,
                                         uint32_t full_len)
    : reader_(reader) {
  // NOTE: We only write out the `shmem_ok` bool if we're over
  // kMessageBufferShmemThreshold to avoid bloating the size of messages with
  // small buffers.
  if (full_len > kMessageBufferShmemThreshold) {
    bool shmem_ok = false;
    if (!reader->ReadBool(&shmem_ok)) {
      reader->FatalError("MessageReader::ReadBool failed!");
      return;
    }
    if (shmem_ok) {
      mozilla::ipc::shared_memory::MutableHandle handle;
      if (!IPC::ReadParam(reader, &handle)) {
        reader->FatalError("failed to read shared memory handle");
        return;
      }
      if (!handle.IsValid()) {
        reader->FatalError("invalid shared memory handle");
        return;
      }
      if (handle.Size() < full_len) {
        reader->FatalError("too small shared memory handle");
        return;
      }
      shmem_cursor_ = mozilla::MakeUnique<mozilla::ipc::shared_memory::Cursor>(
          std::move(handle));
      MOZ_ASSERT(shmem_cursor_->IsValid());
    }
  }
  remaining_ = full_len;
}

MessageBufferReader::~MessageBufferReader() {
  if (remaining_ != 0) {
    reader_->FatalError("didn't fully write message buffer");
  }
}

bool MessageBufferReader::ReadBytesInto(void* data, uint32_t len) {
  MOZ_RELEASE_ASSERT(len == remaining_ || (len % 4) == 0,
                     "all reads except for the final read must be a multiple "
                     "of 4 bytes in length due to padding");
  if (len > remaining_) {
    reader_->FatalError("MessageBufferReader overrun");
    return false;
  }
  remaining_ -= len;
  // If we're serializing using a shared memory region, `shmem_cursor_` will be
  // initialized.
  if (shmem_cursor_) {
    return shmem_cursor_->Read(data, len);
  }
  return reader_->ReadBytesInto(data, len);
}

}  // namespace IPC
