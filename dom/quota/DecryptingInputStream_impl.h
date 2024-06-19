/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_DecryptingInputStream_impl_h
#define mozilla_dom_quota_DecryptingInputStream_impl_h

#include "DecryptingInputStream.h"

#include <algorithm>
#include <cstdio>
#include <type_traits>
#include <utility>
#include "CipherStrategy.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Span.h"
#include "mozilla/fallible.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFileStreams.h"
#include "nsID.h"
#include "nsIFileStreams.h"

namespace mozilla::dom::quota {

template <typename CipherStrategy>
DecryptingInputStream<CipherStrategy>::DecryptingInputStream(
    MovingNotNull<nsCOMPtr<nsIInputStream>> aBaseStream, size_t aBlockSize,
    typename CipherStrategy::KeyType aKey)
    : DecryptingInputStreamBase(std::move(aBaseStream), aBlockSize),
      mKey(aKey) {
  // XXX Move this to a fallible init function.
  MOZ_ALWAYS_SUCCEEDS(mCipherStrategy.Init(CipherMode::Decrypt,
                                           CipherStrategy::SerializeKey(aKey)));

  // This implementation only supports sync base streams.  Verify this in debug
  // builds.
#ifdef DEBUG
  bool baseNonBlocking;
  nsresult rv = (*mBaseStream)->IsNonBlocking(&baseNonBlocking);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(!baseNonBlocking);
#endif
}

template <typename CipherStrategy>
DecryptingInputStream<CipherStrategy>::~DecryptingInputStream() {
  Close();
}

template <typename CipherStrategy>
DecryptingInputStream<CipherStrategy>::DecryptingInputStream()
    : DecryptingInputStreamBase{} {}

template <typename CipherStrategy>
NS_IMETHODIMP DecryptingInputStream<CipherStrategy>::Close() {
  if (!mBaseStream) {
    return NS_OK;
  }

  (*mBaseStream)->Close();
  mBaseStream.destroy();

  mPlainBuffer.Clear();
  mEncryptedBlock.reset();

  return NS_OK;
}

template <typename CipherStrategy>
NS_IMETHODIMP DecryptingInputStream<CipherStrategy>::Available(
    uint64_t* aLengthOut) {
  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  int64_t oldPos, endPos;
  nsresult rv = Tell(&oldPos);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = Seek(SEEK_END, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = Tell(&endPos);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = Seek(SEEK_SET, oldPos);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aLengthOut = endPos - oldPos;
  return NS_OK;
}

template <typename CipherStrategy>
NS_IMETHODIMP DecryptingInputStream<CipherStrategy>::StreamStatus() {
  return mBaseStream ? NS_OK : NS_BASE_STREAM_CLOSED;
}

template <typename CipherStrategy>
NS_IMETHODIMP DecryptingInputStream<CipherStrategy>::ReadSegments(
    nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
    uint32_t* aBytesReadOut) {
  *aBytesReadOut = 0;

  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  nsresult rv;

  // Do not try to use the base stream's ReadSegments here.  Its very
  // unlikely we will get a single buffer that contains all of the encrypted
  // data and therefore would have to copy into our own buffer anyways.
  // Instead, focus on making efficient use of the Read() interface.

  while (aCount > 0) {
    // We have some decrypted data in our buffer.  Provide it to the callers
    // writer function.
    if (mNextByte < mPlainBytes) {
      MOZ_ASSERT(!mPlainBuffer.IsEmpty());
      uint32_t remaining = PlainLength();
      uint32_t numToWrite = std::min(aCount, remaining);
      uint32_t numWritten;
      rv = aWriter(this, aClosure,
                   reinterpret_cast<const char*>(&mPlainBuffer[mNextByte]),
                   *aBytesReadOut, numToWrite, &numWritten);

      // As defined in nsIInputputStream.idl, do not pass writer func errors.
      if (NS_FAILED(rv)) {
        return NS_OK;
      }

      // End-of-file
      if (numWritten == 0) {
        return NS_OK;
      }

      *aBytesReadOut += numWritten;
      mNextByte += numWritten;
      MOZ_ASSERT(mNextByte <= mPlainBytes);

      aCount -= numWritten;

      continue;
    }

    // Otherwise decrypt the next chunk and loop.  Any resulting data will set
    // mPlainBytes and mNextByte which we check at the top of the loop.
    uint32_t bytesRead;
    rv = ParseNextChunk(&bytesRead);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // If we couldn't read anything, then this is eof.
    if (bytesRead == 0) {
      return NS_OK;
    }

    mPlainBytes = bytesRead;
    mNextByte = 0;
  }

  return NS_OK;
}

template <typename CipherStrategy>
nsresult DecryptingInputStream<CipherStrategy>::ParseNextChunk(
    uint32_t* const aBytesReadOut) {
  *aBytesReadOut = 0;

  if (!EnsureBuffers()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Read the data to our internal encrypted buffer.
  auto wholeBlock = mEncryptedBlock->MutableWholeBlock();
  nsresult rv =
      ReadAll(AsWritableChars(wholeBlock).Elements(), wholeBlock.Length(),
              wholeBlock.Length(), aBytesReadOut);
  if (NS_WARN_IF(NS_FAILED(rv)) || *aBytesReadOut == 0) {
    return rv;
  }

  // XXX Do we need to know the actual decrypted size?
  rv = mCipherStrategy.Cipher(mEncryptedBlock->MutableCipherPrefix(),
                              mEncryptedBlock->Payload(),
                              AsWritableBytes(Span{mPlainBuffer}));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aBytesReadOut = mEncryptedBlock->ActualPayloadLength();

  return NS_OK;
}

template <typename CipherStrategy>
nsresult DecryptingInputStream<CipherStrategy>::ReadAll(
    char* aBuf, uint32_t aCount, uint32_t aMinValidCount,
    uint32_t* aBytesReadOut) {
  MOZ_ASSERT(aCount >= aMinValidCount);
  MOZ_ASSERT(mBaseStream);

  *aBytesReadOut = 0;

  uint32_t offset = 0;
  while (aCount > 0) {
    uint32_t bytesRead = 0;
    nsresult rv = (*mBaseStream)->Read(aBuf + offset, aCount, &bytesRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // EOF, but don't immediately return.  We need to validate min read bytes
    // below.
    if (bytesRead == 0) {
      break;
    }

    *aBytesReadOut += bytesRead;
    offset += bytesRead;
    aCount -= bytesRead;
  }

  // Reading zero bytes is not an error.  Its the expected EOF condition.
  // Only compare to the minimum valid count if we read at least one byte.
  if (*aBytesReadOut != 0 && *aBytesReadOut < aMinValidCount) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  return NS_OK;
}

template <typename CipherStrategy>
bool DecryptingInputStream<CipherStrategy>::EnsureBuffers() {
  // Lazily create our two buffers so we can report OOM during stream
  // operation.  These allocations only happens once.  The buffers are reused
  // until the stream is closed.
  if (!mEncryptedBlock) {
    // XXX Do we need to do this fallible (as the comment above suggests)?
    mEncryptedBlock.emplace(*mBlockSize);

    MOZ_ASSERT(mPlainBuffer.IsEmpty());
    if (NS_WARN_IF(!mPlainBuffer.SetLength(mEncryptedBlock->MaxPayloadLength(),
                                           fallible))) {
      return false;
    }
  }

  return true;
}

template <typename CipherStrategy>
nsresult DecryptingInputStream<CipherStrategy>::EnsureDecryptedStreamSize() {
  if (mDecryptedStreamSize) {
    return NS_OK;
  }

  auto decryptedStreamSizeOrErr = [this]() -> Result<int64_t, nsresult> {
    nsresult rv = (*mBaseSeekableStream)->Seek(NS_SEEK_SET, 0);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Err(rv);
    }

    uint64_t baseStreamSize;
    rv = (*mBaseStream)->Available(&baseStreamSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Err(rv);
    }

    if (!baseStreamSize) {
      return 0;
    }

    rv = (*mBaseSeekableStream)
             ->Seek(NS_SEEK_END, -static_cast<int64_t>(*mBlockSize));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Err(rv);
    }

    uint32_t bytesRead;
    rv = ParseNextChunk(&bytesRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Err(rv);
    }
    MOZ_ASSERT(bytesRead);

    mPlainBytes = bytesRead;

    mNextByte = bytesRead;

    int64_t current;
    rv = Tell(&current);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Err(rv);
    }

    return current;
  }();

  if (decryptedStreamSizeOrErr.isErr()) {
    return decryptedStreamSizeOrErr.unwrapErr();
  }

  mDecryptedStreamSize.init(decryptedStreamSizeOrErr.inspect());

  return NS_OK;
}

template <typename CipherStrategy>
NS_IMETHODIMP DecryptingInputStream<CipherStrategy>::Tell(
    int64_t* const aRetval) {
  MOZ_ASSERT(aRetval);

  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (!EnsureBuffers()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int64_t basePosition;
  nsresult rv = (*mBaseSeekableStream)->Tell(&basePosition);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (basePosition == 0) {
    *aRetval = 0;
    return NS_OK;
  }

  MOZ_ASSERT(0 == basePosition % *mBlockSize);

  const auto fullBlocks = basePosition / *mBlockSize;
  MOZ_ASSERT(fullBlocks);

  *aRetval = (fullBlocks - 1) * mEncryptedBlock->MaxPayloadLength() + mNextByte;
  return NS_OK;
}

template <typename CipherStrategy>
NS_IMETHODIMP DecryptingInputStream<CipherStrategy>::Seek(const int32_t aWhence,
                                                          int64_t aOffset) {
  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (!EnsureBuffers()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int64_t baseCurrent;
  nsresult rv = (*mBaseSeekableStream)->Tell(&baseCurrent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Err(rv);
  }

  // Can't call this just in NS_SEEK_CUR case, because ensuring the decrypted
  // size below may change the current position.
  int64_t current;
  rv = Tell(&current);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If there's a failure we need to restore any previous state.
  auto autoRestorePreviousState =
      MakeScopeExit([baseSeekableStream = *mBaseSeekableStream,
                     savedBaseCurrent = baseCurrent,
                     savedPlainBytes = mPlainBytes, savedNextByte = mNextByte,
                     &plainBytes = mPlainBytes, &nextByte = mNextByte] {
        nsresult rv = baseSeekableStream->Seek(NS_SEEK_SET, savedBaseCurrent);
        Unused << NS_WARN_IF(NS_FAILED(rv));
        plainBytes = savedPlainBytes;
        nextByte = savedNextByte;
      });

  rv = EnsureDecryptedStreamSize();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t baseBlocksOffset;
  int64_t nextByteOffset;
  switch (aWhence) {
    case NS_SEEK_CUR:
      // XXX Simplify this without using Tell.
      aOffset += current;
      break;

    case NS_SEEK_SET:
      break;

    case NS_SEEK_END:
      // XXX Simplify this without using Seek/Tell.
      aOffset += *mDecryptedStreamSize;
      break;

    default:
      return NS_ERROR_ILLEGAL_VALUE;
  }

  if (aOffset < 0 || aOffset > *mDecryptedStreamSize) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  baseBlocksOffset = aOffset / mEncryptedBlock->MaxPayloadLength();
  nextByteOffset = aOffset % mEncryptedBlock->MaxPayloadLength();

  // XXX If we remain in the same block as before, we can skip this.
  rv =
      (*mBaseSeekableStream)->Seek(NS_SEEK_SET, baseBlocksOffset * *mBlockSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t readBytes;
  rv = ParseNextChunk(&readBytes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (readBytes == 0 && baseBlocksOffset != 0) {
    mPlainBytes = mEncryptedBlock->MaxPayloadLength();
    mNextByte = mEncryptedBlock->MaxPayloadLength();
  } else {
    mPlainBytes = readBytes;
    mNextByte = nextByteOffset;
  }

  autoRestorePreviousState.release();

  return NS_OK;
}

template <typename CipherStrategy>
NS_IMETHODIMP DecryptingInputStream<CipherStrategy>::Clone(
    nsIInputStream** _retval) {
  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (!(*mBaseCloneableInputStream)->GetCloneable()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> clonedStream;
  nsresult rv =
      (*mBaseCloneableInputStream)->Clone(getter_AddRefs(clonedStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *_retval = MakeAndAddRef<DecryptingInputStream>(
                 WrapNotNull(std::move(clonedStream)), *mBlockSize, *mKey)
                 .take();

  return NS_OK;
}

template <typename CipherStrategy>
void DecryptingInputStream<CipherStrategy>::Serialize(
    mozilla::ipc::InputStreamParams& aParams, uint32_t aMaxSize,
    uint32_t* aSizeUsed) {
  MOZ_ASSERT(mBaseStream);
  MOZ_ASSERT(mBaseIPCSerializableInputStream);

  mozilla::ipc::InputStreamParams baseStreamParams;
  (*mBaseIPCSerializableInputStream)
      ->Serialize(baseStreamParams, aMaxSize, aSizeUsed);

  MOZ_ASSERT(baseStreamParams.type() ==
             mozilla::ipc::InputStreamParams::TFileInputStreamParams);

  mozilla::ipc::EncryptedFileInputStreamParams encryptedFileInputStreamParams;
  encryptedFileInputStreamParams.fileInputStreamParams() =
      std::move(baseStreamParams);
  encryptedFileInputStreamParams.key().AppendElements(
      mCipherStrategy.SerializeKey(*mKey));
  encryptedFileInputStreamParams.blockSize() = *mBlockSize;

  aParams = std::move(encryptedFileInputStreamParams);
}

template <typename CipherStrategy>
bool DecryptingInputStream<CipherStrategy>::Deserialize(
    const mozilla::ipc::InputStreamParams& aParams) {
  MOZ_ASSERT(aParams.type() ==
             mozilla::ipc::InputStreamParams::TEncryptedFileInputStreamParams);
  const auto& params = aParams.get_EncryptedFileInputStreamParams();

  nsCOMPtr<nsIFileInputStream> stream;
  nsFileInputStream::Create(NS_GET_IID(nsIFileInputStream),
                            getter_AddRefs(stream));
  nsCOMPtr<nsIIPCSerializableInputStream> baseSerializable =
      do_QueryInterface(stream);

  if (NS_WARN_IF(
          !baseSerializable->Deserialize(params.fileInputStreamParams()))) {
    return false;
  }

  Init(WrapNotNull<nsCOMPtr<nsIInputStream>>(std::move(stream)),
       params.blockSize());

  auto key = mCipherStrategy.DeserializeKey(params.key());
  if (NS_WARN_IF(!key)) {
    return false;
  }

  mKey.init(*key);
  if (NS_WARN_IF(
          NS_FAILED(mCipherStrategy.Init(CipherMode::Decrypt, params.key())))) {
    return false;
  }

  return true;
}

}  // namespace mozilla::dom::quota

#endif
