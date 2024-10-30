/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <numeric>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "ErrorList.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/FixedBufferOutputStream.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/SafeRefPtr.h"
#include "mozilla/dom/quota/DecryptingInputStream_impl.h"
#include "mozilla/dom/quota/DummyCipherStrategy.h"
#include "mozilla/dom/quota/EncryptedBlock.h"
#include "mozilla/dom/quota/EncryptingOutputStream_impl.h"
#include "mozilla/dom/quota/NSSCipherStrategy.h"
#include "mozilla/fallible.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsICloneableInputStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISeekableStream.h"
#include "nsISupports.h"
#include "nsITellableStream.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nscore.h"
#include "nss.h"

namespace mozilla::dom::quota {

// Similar to ArrayBufferInputStream from netwerk/base/ArrayBufferInputStream.h,
// but this is initialized from a Span on construction, rather than lazily from
// a JS ArrayBuffer.
class ArrayBufferInputStream : public nsIInputStream,
                               public nsISeekableStream,
                               public nsICloneableInputStream {
 public:
  explicit ArrayBufferInputStream(mozilla::Span<const uint8_t> aData);
  bool SetCloseOnEOF(bool value) { return mCloseOnEOF = value; }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSITELLABLESTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM

 private:
  virtual ~ArrayBufferInputStream() = default;

  mozilla::UniquePtr<char[]> mArrayBuffer;
  uint32_t mBufferLength;
  uint32_t mPos;
  bool mClosed;
  bool mCloseOnEOF;
};

NS_IMPL_ADDREF(ArrayBufferInputStream);
NS_IMPL_RELEASE(ArrayBufferInputStream);

NS_INTERFACE_MAP_BEGIN(ArrayBufferInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY(nsICloneableInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

ArrayBufferInputStream::ArrayBufferInputStream(
    mozilla::Span<const uint8_t> aData)
    : mArrayBuffer(MakeUnique<char[]>(aData.Length())),
      mBufferLength(aData.Length()),
      mPos(0),
      mClosed(false),
      mCloseOnEOF(false) {
  std::copy(aData.cbegin(), aData.cend(), mArrayBuffer.get());
}

NS_IMETHODIMP
ArrayBufferInputStream::Close() {
  mClosed = true;
  return NS_OK;
}

NS_IMETHODIMP
ArrayBufferInputStream::Available(uint64_t* aCount) {
  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (mArrayBuffer) {
    *aCount = mBufferLength ? mBufferLength - mPos : 0;
  } else {
    *aCount = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
ArrayBufferInputStream::StreamStatus() {
  return mClosed ? NS_BASE_STREAM_CLOSED : NS_OK;
}

NS_IMETHODIMP
ArrayBufferInputStream::Read(char* aBuf, uint32_t aCount,
                             uint32_t* aReadCount) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aReadCount);
}

NS_IMETHODIMP
ArrayBufferInputStream::ReadSegments(nsWriteSegmentFun writer, void* closure,
                                     uint32_t aCount, uint32_t* result) {
  MOZ_RELEASE_ASSERT(result, "null ptr");
  MOZ_RELEASE_ASSERT(mBufferLength >= mPos, "bad stream state");

  if (mClosed) {
    *result = 0;
    return NS_OK;
  }

  MOZ_RELEASE_ASSERT(mArrayBuffer || (mPos == mBufferLength),
                     "stream inited incorrectly");

  *result = 0;
  while (mPos < mBufferLength) {
    uint32_t remaining = mBufferLength - mPos;
    MOZ_RELEASE_ASSERT(mArrayBuffer);

    uint32_t count = std::min(aCount, remaining);
    if (count == 0) {
      break;
    }

    uint32_t written;
    nsresult rv = writer(this, closure, &mArrayBuffer[0] + mPos, *result, count,
                         &written);
    if (NS_FAILED(rv)) {
      // InputStreams do not propagate errors to caller.
      return NS_OK;
    }

    MOZ_RELEASE_ASSERT(
        written <= count,
        "writer should not write more than we asked it to write");
    mPos += written;
    *result += written;
    aCount -= written;
  }

  if (*result == 0 && mCloseOnEOF) {
    Close();
  }

  return NS_OK;
}

NS_IMETHODIMP
ArrayBufferInputStream::IsNonBlocking(bool* aNonBlocking) {
  // Actually, the stream never blocks, but we lie about it because of the
  // assumptions in DecryptingInputStream.
  *aNonBlocking = false;
  return NS_OK;
}

NS_IMETHODIMP ArrayBufferInputStream::Tell(int64_t* const aRetval) {
  MOZ_RELEASE_ASSERT(aRetval);

  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }
  *aRetval = mPos;

  return NS_OK;
}

NS_IMETHODIMP ArrayBufferInputStream::Seek(const int32_t aWhence,
                                           const int64_t aOffset) {
  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  // XXX This is not safe. it's hard to use CheckedInt here, though. As long as
  // the class is only used for testing purposes, that's probably fine.

  int32_t newPos = mPos;
  switch (aWhence) {
    case NS_SEEK_SET:
      newPos = aOffset;
      break;
    case NS_SEEK_CUR:
      newPos += aOffset;
      break;
    case NS_SEEK_END:
      newPos = mBufferLength;
      newPos += aOffset;
      break;
    default:
      return NS_ERROR_ILLEGAL_VALUE;
  }
  if (newPos < 0 || static_cast<uint32_t>(newPos) > mBufferLength) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  mPos = newPos;

  return NS_OK;
}

NS_IMETHODIMP ArrayBufferInputStream::SetEOF() {
  // Truncating is not supported on a read-only stream.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ArrayBufferInputStream::GetCloneable(bool* aCloneable) {
  *aCloneable = true;
  return NS_OK;
}

NS_IMETHODIMP ArrayBufferInputStream::Clone(nsIInputStream** _retval) {
  *_retval = MakeAndAddRef<ArrayBufferInputStream>(
                 AsBytes(Span{mArrayBuffer.get(), mBufferLength}))
                 .take();

  return NS_OK;
}
}  // namespace mozilla::dom::quota

using namespace mozilla;
using namespace mozilla::dom::quota;

class DOM_Quota_EncryptedStream : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    // Do this only once, do not tear it down per test case.
    if (!sNssContext) {
      sNssContext.reset(
          NSS_InitContext("", "", "", "", nullptr,
                          NSS_INIT_READONLY | NSS_INIT_NOCERTDB |
                              NSS_INIT_NOMODDB | NSS_INIT_FORCEOPEN |
                              NSS_INIT_OPTIMIZESPACE | NSS_INIT_NOROOTINIT));
    }
  }

  static void TearDownTestCase() { sNssContext = nullptr; }

 private:
  struct NSSInitContextDeleter {
    void operator()(NSSInitContext* p) { NSS_ShutdownContext(p); }
  };
  MOZ_RUNINIT inline static std::unique_ptr<NSSInitContext,
                                            NSSInitContextDeleter>
      sNssContext;
};

enum struct FlushMode { AfterEachChunk, Never };
enum struct ChunkSize { SingleByte, Unaligned, DataSize };

using PackedTestParams =
    std::tuple<size_t, ChunkSize, ChunkSize, size_t, FlushMode, bool>;

static size_t EffectiveChunkSize(const ChunkSize aChunkSize,
                                 const size_t aDataSize) {
  switch (aChunkSize) {
    case ChunkSize::SingleByte:
      return 1;
    case ChunkSize::Unaligned:
      return 17;
    case ChunkSize::DataSize:
      return aDataSize;
  }
  MOZ_CRASH("Unknown ChunkSize");
}

struct TestParams {
  MOZ_IMPLICIT constexpr TestParams(const PackedTestParams& aPackedParams)
      : mDataSize(std::get<0>(aPackedParams)),
        mWriteChunkSize(std::get<1>(aPackedParams)),
        mReadChunkSize(std::get<2>(aPackedParams)),
        mBlockSize(std::get<3>(aPackedParams)),
        mFlushMode(std::get<4>(aPackedParams)),
        mCloseOnEOF(std::get<5>(aPackedParams)) {}

  constexpr size_t DataSize() const { return mDataSize; }

  size_t EffectiveWriteChunkSize() const {
    return EffectiveChunkSize(mWriteChunkSize, mDataSize);
  }

  size_t EffectiveReadChunkSize() const {
    return EffectiveChunkSize(mReadChunkSize, mDataSize);
  }

  constexpr size_t BlockSize() const { return mBlockSize; }

  constexpr enum FlushMode FlushMode() const { return mFlushMode; }

  constexpr bool CloseOnEOF() const { return mCloseOnEOF; }

 private:
  size_t mDataSize;

  ChunkSize mWriteChunkSize;
  ChunkSize mReadChunkSize;

  size_t mBlockSize;
  enum FlushMode mFlushMode;
  bool mCloseOnEOF;
};

std::string TestParamToString(
    const testing::TestParamInfo<PackedTestParams>& aTestParams) {
  const TestParams& testParams = aTestParams.param;

  static constexpr char kSeparator[] = "_";

  std::stringstream ss;
  ss << "data" << testParams.DataSize() << kSeparator << "writechunk"
     << testParams.EffectiveWriteChunkSize() << kSeparator << "readchunk"
     << testParams.EffectiveReadChunkSize() << kSeparator << "block"
     << testParams.BlockSize() << kSeparator;
  switch (testParams.FlushMode()) {
    case FlushMode::Never:
      ss << "FlushNever";
      break;
    case FlushMode::AfterEachChunk:
      ss << "FlushAfterEachChunk";
      break;
  };
  ss << kSeparator
     << (testParams.CloseOnEOF() ? "closeOnEOF" : "keepOpenOnEOF");
  return ss.str();
}

class ParametrizedCryptTest
    : public DOM_Quota_EncryptedStream,
      public testing::WithParamInterface<PackedTestParams> {};

static auto MakeTestData(const size_t aDataSize) {
  auto data = nsTArray<uint8_t>();
  data.SetLength(aDataSize);
  std::iota(data.begin(), data.end(), 0);
  return data;
}

template <typename CipherStrategy>
static void WriteTestData(nsCOMPtr<nsIOutputStream>&& aBaseOutputStream,
                          const Span<const uint8_t> aData,
                          const size_t aWriteChunkSize, const size_t aBlockSize,
                          const typename CipherStrategy::KeyType& aKey,
                          const FlushMode aFlushMode) {
  auto outStream = MakeSafeRefPtr<EncryptingOutputStream<CipherStrategy>>(
      std::move(aBaseOutputStream), aBlockSize, aKey);

  for (auto remaining = aData; !remaining.IsEmpty();) {
    auto [currentChunk, newRemaining] =
        remaining.SplitAt(std::min(aWriteChunkSize, remaining.Length()));
    remaining = newRemaining;

    uint32_t written;
    EXPECT_EQ(NS_OK, outStream->Write(
                         reinterpret_cast<const char*>(currentChunk.Elements()),
                         currentChunk.Length(), &written));
    EXPECT_EQ(currentChunk.Length(), written);

    if (aFlushMode == FlushMode::AfterEachChunk) {
      outStream->Flush();
    }
  }

  // Close explicitly so we can check the result.
  EXPECT_EQ(NS_OK, outStream->Close());
}

template <typename CipherStrategy>
static void NoExtraChecks(DecryptingInputStream<CipherStrategy>& aInputStream,
                          Span<const uint8_t> aExpectedData,
                          Span<const uint8_t> aRemainder) {}

template <typename CipherStrategy,
          typename ExtraChecks = decltype(NoExtraChecks<CipherStrategy>)>
static void ReadTestData(
    DecryptingInputStream<CipherStrategy>& aDecryptingInputStream,
    const Span<const uint8_t> aExpectedData, const size_t aReadChunkSize,
    const ExtraChecks& aExtraChecks = NoExtraChecks<CipherStrategy>) {
  auto readData = nsTArray<uint8_t>();
  readData.SetLength(aReadChunkSize);

  // sanity check: total file length and expectedData length must always match
  uint64_t availableBytes = 0;
  EXPECT_EQ(NS_OK, aDecryptingInputStream.Available(&availableBytes));
  EXPECT_EQ(aExpectedData.LengthBytes(), availableBytes);

  for (auto remainder = aExpectedData; !remainder.IsEmpty();) {
    auto [currentExpected, newExpectedRemainder] =
        remainder.SplitAt(std::min(aReadChunkSize, remainder.Length()));
    remainder = newExpectedRemainder;

    uint32_t read;
    EXPECT_EQ(NS_OK, aDecryptingInputStream.Read(
                         reinterpret_cast<char*>(readData.Elements()),
                         currentExpected.Length(), &read));
    EXPECT_EQ(currentExpected.Length(), read);
    EXPECT_EQ(currentExpected,
              Span{readData}.First(currentExpected.Length()).AsConst());

    aExtraChecks(aDecryptingInputStream, aExpectedData, remainder);
  }

  // Expect EOF.
  uint32_t read;
  EXPECT_EQ(NS_OK, aDecryptingInputStream.Read(
                       reinterpret_cast<char*>(readData.Elements()),
                       readData.Length(), &read));
  EXPECT_EQ(0u, read);
}

template <typename CipherStrategy,
          typename ExtraChecks = decltype(NoExtraChecks<CipherStrategy>)>
static auto ReadTestData(
    MovingNotNull<nsCOMPtr<nsIInputStream>>&& aBaseInputStream,
    const Span<const uint8_t> aExpectedData, const size_t aReadChunkSize,
    const size_t aBlockSize, const typename CipherStrategy::KeyType& aKey,
    const ExtraChecks& aExtraChecks = NoExtraChecks<CipherStrategy>) {
  auto inStream = MakeSafeRefPtr<DecryptingInputStream<CipherStrategy>>(
      std::move(aBaseInputStream), aBlockSize, aKey);

  ReadTestData(*inStream, aExpectedData, aReadChunkSize, aExtraChecks);

  return inStream;
}

// XXX Change to return the buffer instead.
template <typename CipherStrategy,
          typename ExtraChecks = decltype(NoExtraChecks<CipherStrategy>)>
static RefPtr<FixedBufferOutputStream> DoRoundtripTest(
    const size_t aDataSize, const size_t aWriteChunkSize,
    const size_t aReadChunkSize, const size_t aBlockSize,
    const typename CipherStrategy::KeyType& aKey, const FlushMode aFlushMode,
    bool aCloseOnEOF,
    const ExtraChecks& aExtraChecks = NoExtraChecks<CipherStrategy>) {
  // XXX Add deduction guide for RefPtr from already_AddRefed
  const auto baseOutputStream = WrapNotNull(
      RefPtr<FixedBufferOutputStream>{FixedBufferOutputStream::Create(2048)});

  const auto data = MakeTestData(aDataSize);

  WriteTestData<CipherStrategy>(
      nsCOMPtr<nsIOutputStream>{baseOutputStream.get()}, Span{data},
      aWriteChunkSize, aBlockSize, aKey, aFlushMode);

  const auto baseInputStream =
      MakeRefPtr<ArrayBufferInputStream>(baseOutputStream->WrittenData());

  baseInputStream->SetCloseOnEOF(aCloseOnEOF);

  ReadTestData<CipherStrategy>(
      WrapNotNull(nsCOMPtr<nsIInputStream>{baseInputStream}), Span{data},
      aReadChunkSize, aBlockSize, aKey, aExtraChecks);

  return baseOutputStream;
}

TEST_P(ParametrizedCryptTest, NSSCipherStrategy) {
  using CipherStrategy = NSSCipherStrategy;
  const TestParams& testParams = GetParam();

  auto keyOrErr = CipherStrategy::GenerateKey();
  ASSERT_FALSE(keyOrErr.isErr());

  DoRoundtripTest<CipherStrategy>(
      testParams.DataSize(), testParams.EffectiveWriteChunkSize(),
      testParams.EffectiveReadChunkSize(), testParams.BlockSize(),
      keyOrErr.unwrap(), testParams.FlushMode(), testParams.CloseOnEOF());
}

TEST_P(ParametrizedCryptTest, NSSCipherStrategy_Available) {
  using CipherStrategy = NSSCipherStrategy;
  const TestParams& testParams = GetParam();

  DoRoundtripTest<CipherStrategy>(
      testParams.DataSize(), testParams.EffectiveWriteChunkSize(),
      testParams.EffectiveReadChunkSize(), testParams.BlockSize(),
      CipherStrategy::KeyType{}, testParams.FlushMode(),
      testParams.CloseOnEOF(),
      [](auto& inStream, Span<const uint8_t> expectedData,
         Span<const uint8_t> remainder) {
        // Check that Available tells the right remainder.
        uint64_t available;
        EXPECT_EQ(NS_OK, inStream.Available(&available));
        EXPECT_EQ(remainder.Length(), available);
      });
}

TEST_P(ParametrizedCryptTest, DummyCipherStrategy_CheckOutput) {
  using CipherStrategy = DummyCipherStrategy;
  const TestParams& testParams = GetParam();

  const auto encryptedDataStream = DoRoundtripTest<CipherStrategy>(
      testParams.DataSize(), testParams.EffectiveWriteChunkSize(),
      testParams.EffectiveReadChunkSize(), testParams.BlockSize(),
      CipherStrategy::KeyType{}, testParams.FlushMode(),
      testParams.CloseOnEOF());

  if (HasFailure()) {
    return;
  }

  const auto encryptedData = encryptedDataStream->WrittenData();
  const auto encryptedDataSpan = AsBytes(Span(encryptedData));

  const auto plainTestData = MakeTestData(testParams.DataSize());
  auto encryptedBlock = EncryptedBlock<DummyCipherStrategy::BlockPrefixLength,
                                       DummyCipherStrategy::BasicBlockSize>{
      testParams.BlockSize(),
  };
  for (auto [encryptedRemainder, plainRemainder] =
           std::pair(encryptedDataSpan, Span(plainTestData));
       !encryptedRemainder.IsEmpty();) {
    const auto [currentBlock, newEncryptedRemainder] =
        encryptedRemainder.SplitAt(testParams.BlockSize());
    encryptedRemainder = newEncryptedRemainder;

    std::copy(currentBlock.cbegin(), currentBlock.cend(),
              encryptedBlock.MutableWholeBlock().begin());

    ASSERT_FALSE(plainRemainder.IsEmpty());
    const auto [currentPlain, newPlainRemainder] =
        plainRemainder.SplitAt(encryptedBlock.ActualPayloadLength());
    plainRemainder = newPlainRemainder;

    const auto pseudoIV = encryptedBlock.CipherPrefix();
    const auto payload = encryptedBlock.Payload();

    EXPECT_EQ(Span(DummyCipherStrategy::MakeBlockPrefix()), pseudoIV);

    auto untransformedPayload = nsTArray<uint8_t>();
    untransformedPayload.SetLength(testParams.BlockSize());
    DummyCipherStrategy::DummyTransform(payload, untransformedPayload);

    EXPECT_EQ(
        currentPlain,
        Span(untransformedPayload).AsConst().First(currentPlain.Length()));
  }
}

TEST_P(ParametrizedCryptTest, DummyCipherStrategy_Tell) {
  using CipherStrategy = DummyCipherStrategy;
  const TestParams& testParams = GetParam();

  DoRoundtripTest<CipherStrategy>(
      testParams.DataSize(), testParams.EffectiveWriteChunkSize(),
      testParams.EffectiveReadChunkSize(), testParams.BlockSize(),
      CipherStrategy::KeyType{}, testParams.FlushMode(),
      testParams.CloseOnEOF(),
      [](auto& inStream, Span<const uint8_t> expectedData,
         Span<const uint8_t> remainder) {
        // Check that Tell tells the right position.
        int64_t pos;
        EXPECT_EQ(NS_OK, inStream.Tell(&pos));
        EXPECT_EQ(expectedData.Length() - remainder.Length(),
                  static_cast<uint64_t>(pos));
      });
}

TEST_P(ParametrizedCryptTest, DummyCipherStrategy_Available) {
  using CipherStrategy = DummyCipherStrategy;
  const TestParams& testParams = GetParam();

  DoRoundtripTest<CipherStrategy>(
      testParams.DataSize(), testParams.EffectiveWriteChunkSize(),
      testParams.EffectiveReadChunkSize(), testParams.BlockSize(),
      CipherStrategy::KeyType{}, testParams.FlushMode(),
      testParams.CloseOnEOF(),
      [](auto& inStream, Span<const uint8_t> expectedData,
         Span<const uint8_t> remainder) {
        // Check that Available tells the right remainder.
        uint64_t available;
        EXPECT_EQ(NS_OK, inStream.Available(&available));
        // stream should still be valid.
        EXPECT_EQ(NS_OK, inStream.BaseStreamStatus());
        EXPECT_EQ(remainder.Length(), available);
      });
}

TEST_P(ParametrizedCryptTest, DummyCipherStrategy_Clone) {
  using CipherStrategy = DummyCipherStrategy;
  const TestParams& testParams = GetParam();

  // XXX Add deduction guide for RefPtr from already_AddRefed
  const auto baseOutputStream = WrapNotNull(
      RefPtr<FixedBufferOutputStream>{FixedBufferOutputStream::Create(2048)});

  const auto data = MakeTestData(testParams.DataSize());

  WriteTestData<CipherStrategy>(
      nsCOMPtr<nsIOutputStream>{baseOutputStream.get()}, Span{data},
      testParams.EffectiveWriteChunkSize(), testParams.BlockSize(),
      CipherStrategy::KeyType{}, testParams.FlushMode());

  const auto baseInputStream =
      MakeRefPtr<ArrayBufferInputStream>(baseOutputStream->WrittenData());

  const auto inStream = ReadTestData<CipherStrategy>(
      WrapNotNull(nsCOMPtr<nsIInputStream>{baseInputStream}), Span{data},
      testParams.EffectiveReadChunkSize(), testParams.BlockSize(),
      CipherStrategy::KeyType{});

  nsCOMPtr<nsIInputStream> clonedInputStream;
  EXPECT_EQ(NS_OK, inStream->Clone(getter_AddRefs(clonedInputStream)));

  ReadTestData(
      static_cast<DecryptingInputStream<CipherStrategy>&>(*clonedInputStream),
      Span{data}, testParams.EffectiveReadChunkSize());
}

// XXX This test is actually only parametrized on the block size.
TEST_P(ParametrizedCryptTest, DummyCipherStrategy_IncompleteBlock) {
  using CipherStrategy = DummyCipherStrategy;
  const TestParams& testParams = GetParam();

  // Provide half a block, content doesn't matter.
  nsTArray<uint8_t> data;
  data.SetLength(testParams.BlockSize() / 2);

  const auto baseInputStream = MakeRefPtr<ArrayBufferInputStream>(data);

  const auto inStream = MakeSafeRefPtr<DecryptingInputStream<CipherStrategy>>(
      WrapNotNull(nsCOMPtr<nsIInputStream>{baseInputStream}),
      testParams.BlockSize(), CipherStrategy::KeyType{});

  nsTArray<uint8_t> readData;
  readData.SetLength(testParams.BlockSize());
  uint32_t read;
  EXPECT_EQ(NS_ERROR_CORRUPTED_CONTENT,
            inStream->Read(reinterpret_cast<char*>(readData.Elements()),
                           readData.Length(), &read));
}

TEST_P(ParametrizedCryptTest, zeroInitializedEncryptedBlock) {
  const TestParams& testParams = GetParam();

  using EncryptedBlock = EncryptedBlock<DummyCipherStrategy::BlockPrefixLength,
                                        DummyCipherStrategy::BasicBlockSize>;

  EncryptedBlock encryptedBlock{testParams.BlockSize()};
  auto firstBlock =
      encryptedBlock.WholeBlock().First<DummyCipherStrategy::BasicBlockSize>();
  auto unusedBytesInFirstBlock = firstBlock.from(sizeof(uint16_t));

  EXPECT_TRUE(std::all_of(unusedBytesInFirstBlock.begin(),
                          unusedBytesInFirstBlock.end(),
                          [](const auto& e) { return 0ul == e; }));
}

enum struct SeekOffset {
  Zero,
  MinusHalfDataSize,
  PlusHalfDataSize,
  PlusDataSize,
  MinusDataSize,
  MinusDataSizeAndOne,
  PlusOne,
  MinusOne
};
using SeekOp = std::tuple<int32_t, SeekOffset, nsresult>;

using PackedSeekTestParams =
    std::tuple<size_t, size_t, std::vector<SeekOp>, bool>;

struct SeekTestParams {
  size_t mDataSize;
  size_t mBlockSize;
  std::vector<SeekOp> mSeekOps;
  bool mCloseOnEOF;

  MOZ_IMPLICIT SeekTestParams(const PackedSeekTestParams& aPackedParams)
      : mDataSize(std::get<0>(aPackedParams)),
        mBlockSize(std::get<1>(aPackedParams)),
        mSeekOps(std::get<2>(aPackedParams)),
        mCloseOnEOF(std::get<3>(aPackedParams)) {}
};

std::string SeekTestParamToString(
    const testing::TestParamInfo<PackedSeekTestParams>& aTestParams) {
  const SeekTestParams& testParams = aTestParams.param;

  static constexpr char kSeparator[] = "_";

  std::stringstream ss;
  ss << "data" << testParams.mDataSize << kSeparator << "writechunk"
     << testParams.mBlockSize << kSeparator;
  for (const auto& seekOp : testParams.mSeekOps) {
    switch (std::get<0>(seekOp)) {
      case nsISeekableStream::NS_SEEK_SET:
        ss << "Set";
        break;
      case nsISeekableStream::NS_SEEK_CUR:
        ss << "Cur";
        break;
      case nsISeekableStream::NS_SEEK_END:
        ss << "End";
        break;
      default:
        MOZ_CRASH("Unknown whence");
    };
    switch (std::get<1>(seekOp)) {
      case SeekOffset::Zero:
        ss << "Zero";
        break;
      case SeekOffset::MinusHalfDataSize:
        ss << "MinusHalfDataSize";
        break;
      case SeekOffset::PlusHalfDataSize:
        ss << "PlusHalfDataSize";
        break;
      case SeekOffset::MinusDataSize:
        ss << "MinusDataSize";
        break;
      case SeekOffset::MinusDataSizeAndOne:
        ss << "MinusDataSizeAndOne";
        break;
      case SeekOffset::PlusDataSize:
        ss << "PlusDataSize";
        break;
      case SeekOffset::PlusOne:
        ss << "PlusOne";
        break;
      case SeekOffset::MinusOne:
        ss << "MinusOne";
        break;
    };
  }
  ss << kSeparator << (testParams.mCloseOnEOF ? "closeOnEOF" : "keepOpenOnEOF");

  return ss.str();
}

class ParametrizedSeekCryptTest
    : public DOM_Quota_EncryptedStream,
      public testing::WithParamInterface<PackedSeekTestParams> {
 public:
  template <typename CipherStrategy>
  void DoSeekTest() {
    const SeekTestParams& testParams = GetParam();

    const auto baseOutputStream = WrapNotNull(
        RefPtr<FixedBufferOutputStream>{FixedBufferOutputStream::Create(2048)});

    const auto data = MakeTestData(testParams.mDataSize);

    WriteTestData<CipherStrategy>(
        nsCOMPtr<nsIOutputStream>{baseOutputStream.get()}, Span{data},
        testParams.mDataSize, testParams.mBlockSize,
        typename CipherStrategy::KeyType{}, FlushMode::Never);

    const auto baseInputStream =
        MakeRefPtr<ArrayBufferInputStream>(baseOutputStream->WrittenData());

    const auto inStream = MakeSafeRefPtr<DecryptingInputStream<CipherStrategy>>(
        WrapNotNull(nsCOMPtr<nsIInputStream>{baseInputStream}),
        testParams.mBlockSize, typename CipherStrategy::KeyType{});

    baseInputStream->SetCloseOnEOF(testParams.mCloseOnEOF);

    uint32_t accumulatedOffset = 0;
    for (const auto& seekOp : testParams.mSeekOps) {
      const auto offset = [offsetKind = std::get<1>(seekOp),
                           dataSize = testParams.mDataSize]() -> int64_t {
        switch (offsetKind) {
          case SeekOffset::Zero:
            return 0;
          case SeekOffset::MinusHalfDataSize:
            return -static_cast<int64_t>(dataSize) / 2;
          case SeekOffset::PlusHalfDataSize:
            return static_cast<int64_t>(dataSize) / 2;
          case SeekOffset::MinusDataSize:
            return -static_cast<int64_t>(dataSize);
          case SeekOffset::MinusDataSizeAndOne:
            return -static_cast<int64_t>(dataSize + 1);
          case SeekOffset::PlusDataSize:
            return static_cast<int64_t>(dataSize);
          case SeekOffset::PlusOne:
            return 1;
          case SeekOffset::MinusOne:
            return -1;
        }
        MOZ_CRASH("Unknown SeekOffset");
      }();
      nsresult rv = inStream->Seek(std::get<0>(seekOp), offset);
      EXPECT_EQ(std::get<2>(seekOp), rv);
      if (NS_SUCCEEDED(rv)) {
        switch (std::get<0>(seekOp)) {
          case nsISeekableStream::NS_SEEK_SET:
            accumulatedOffset = offset;
            break;
          case nsISeekableStream::NS_SEEK_CUR:
            accumulatedOffset += offset;
            break;
          case nsISeekableStream::NS_SEEK_END:
            accumulatedOffset = testParams.mDataSize + offset;
            break;
          default:
            MOZ_CRASH("Unknown whence");
        }
      }
    }

    {
      int64_t actualOffset;
      EXPECT_EQ(NS_OK, inStream->Tell(&actualOffset));

      EXPECT_EQ(actualOffset, accumulatedOffset);
    }

    auto readData = nsTArray<uint8_t>();
    readData.SetLength(data.Length());
    uint32_t read;
    EXPECT_EQ(NS_OK,
              inStream->Read(reinterpret_cast<char*>(readData.Elements()),
                             readData.Length(), &read));
    // XXX Or should 'read' indicate the actual number of bytes read,
    // including the encryption overhead?
    EXPECT_EQ(testParams.mDataSize - accumulatedOffset, read);
    EXPECT_EQ(Span{data}.SplitAt(accumulatedOffset).second,
              Span{readData}.First(read).AsConst());

    // For some closeOnEOF combinations, above Read method can lead to stream
    // closure. Skip calling Tell method below if the underlying stream was
    // already closed.
    if (!testParams.mCloseOnEOF ||
        baseInputStream->StreamStatus() != NS_BASE_STREAM_CLOSED) {
      int64_t actualOffset;
      EXPECT_EQ(NS_OK, inStream->Tell(&actualOffset));

      EXPECT_EQ(static_cast<uint64_t>(actualOffset), data.Length());
    }
  }
};

TEST_P(ParametrizedSeekCryptTest, DummyCipherStrategy_Seek) {
  DoSeekTest<DummyCipherStrategy>();
}

TEST_P(ParametrizedSeekCryptTest, NSSCipherStrategy_Seek) {
  DoSeekTest<NSSCipherStrategy>();
}

// The data size 244 has been calculated as 256 (block size) minus 8
// (DummyCipherStrategy::BlockPrefixLength) minus 4
// (DummyCipherStrategy::BasicBlockSize).
// The data size 1012 has been calculated as 1024 (block size) minus 8
// (DummyCipherStrategy::BlockPrefixLength) minus 4
// (DummyCipherStrategy::BasicBlockSize).
static_assert(DummyCipherStrategy::BlockPrefixLength == 8);
static_assert(DummyCipherStrategy::BasicBlockSize == 4);

// The data size 208 has been calculated as 256 (block size) minus 32
// (NSSCipherStrategy::BlockPrefixLength) minus 16
// (NSSCipherStrategy::BasicBlockSize).
// The data size 976 has been calculated as 1024 (block size) minus 32
// (NSSCipherStrategy::BlockPrefixLength) minus 16
// (NSSCipherStrategy::BasicBlockSize).
static_assert(NSSCipherStrategy::BlockPrefixLength == 32);
static_assert(NSSCipherStrategy::BasicBlockSize == 16);

INSTANTIATE_TEST_SUITE_P(
    DOM_Quota_EncryptedStream_Parametrized, ParametrizedCryptTest,
    testing::Combine(
        /* dataSize */ testing::Values(0u, 16u, 208u, 244u, 256u, 512u, 513u,
                                       976u, 1012u),
        /* writeChunkSize */
        testing::Values(ChunkSize::SingleByte, ChunkSize::Unaligned,
                        ChunkSize::DataSize),
        /* readChunkSize */
        testing::Values(ChunkSize::SingleByte, ChunkSize::Unaligned,
                        ChunkSize::DataSize),
        /* blockSize */ testing::Values(256u, 1024u /*, 8192u*/),
        /* flushMode */
        testing::Values(FlushMode::Never, FlushMode::AfterEachChunk),
        /* closeOnEOF */
        testing::Values(true, false)),
    TestParamToString);

INSTANTIATE_TEST_SUITE_P(
    DOM_IndexedDB_EncryptedStream_ParametrizedSeek, ParametrizedSeekCryptTest,
    testing::Combine(
        /* dataSize */ testing::Values(0u, 16u, 208u, 244u, 256u, 512u, 513u,
                                       976u, 1012u),
        /* blockSize */ testing::Values(256u, 1024u /*, 8192u*/),
        /* seekOperations */
        testing::Values(/* NS_SEEK_SET only, single ops */
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_SET,
                                             SeekOffset::PlusDataSize, NS_OK}},
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_SET,
                                             SeekOffset::PlusHalfDataSize,
                                             NS_OK}},
                        /* NS_SEEK_SET only, multiple ops */
                        std::vector<SeekOp>{
                            {nsISeekableStream::NS_SEEK_SET,
                             SeekOffset::PlusHalfDataSize, NS_OK},
                            {nsISeekableStream::NS_SEEK_SET, SeekOffset::Zero,
                             NS_OK}},
                        /* NS_SEEK_CUR only, single ops */
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_CUR,
                                             SeekOffset::Zero, NS_OK}},
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_CUR,
                                             SeekOffset::PlusDataSize, NS_OK}},
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_CUR,
                                             SeekOffset::PlusHalfDataSize,
                                             NS_OK}},
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_CUR,
                                             SeekOffset::MinusOne,
                                             NS_ERROR_ILLEGAL_VALUE}},
                        /* NS_SEEK_END only, single ops */
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_END,
                                             SeekOffset::Zero, NS_OK}},
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_END,
                                             SeekOffset::MinusDataSize, NS_OK}},
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_END,
                                             SeekOffset::MinusDataSizeAndOne,
                                             NS_ERROR_ILLEGAL_VALUE}},
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_END,
                                             SeekOffset::MinusHalfDataSize,
                                             NS_OK}},
                        std::vector<SeekOp>{{nsISeekableStream::NS_SEEK_END,
                                             SeekOffset::PlusOne,
                                             NS_ERROR_ILLEGAL_VALUE}}),
        /* closeOnEOF */
        testing::Values(true, false)),
    SeekTestParamToString);
