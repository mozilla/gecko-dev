/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfiler.h"

#ifdef MOZ_BASE_PROFILER

#  include "BaseProfileJSONWriter.h"
#  include "BaseProfilerMarkerPayload.h"
#  include "mozilla/BlocksRingBuffer.h"
#  include "mozilla/leb128iterator.h"
#  include "mozilla/ModuloBuffer.h"
#  include "mozilla/PowerOfTwo.h"

#  include "mozilla/Attributes.h"
#  include "mozilla/Vector.h"

#  if defined(_MSC_VER)
#    include <windows.h>
#    include <mmsystem.h>
#    include <process.h>
#  else
#    include <time.h>
#    include <unistd.h>
#  endif

#  include <algorithm>
#  include <atomic>
#  include <thread>
#  include <type_traits>

using namespace mozilla;

MOZ_MAYBE_UNUSED static void SleepMilli(unsigned aMilliseconds) {
#  if defined(_MSC_VER)
  Sleep(aMilliseconds);
#  else
  struct timespec ts;
  ts.tv_sec = aMilliseconds / 1000;
  ts.tv_nsec = long(aMilliseconds % 1000) * 1000000;
  struct timespec tr;
  while (nanosleep(&ts, &tr)) {
    if (errno == EINTR) {
      ts = tr;
    } else {
      printf("nanosleep() -> %s\n", strerror(errno));
      exit(1);
    }
  }
#  endif
}

void TestPowerOfTwoMask() {
  printf("TestPowerOfTwoMask...\n");

  static_assert(MakePowerOfTwoMask<uint32_t, 0>().MaskValue() == 0, "");
  constexpr PowerOfTwoMask<uint32_t> c0 = MakePowerOfTwoMask<uint32_t, 0>();
  MOZ_RELEASE_ASSERT(c0.MaskValue() == 0);

  static_assert(MakePowerOfTwoMask<uint32_t, 0xFFu>().MaskValue() == 0xFFu, "");
  constexpr PowerOfTwoMask<uint32_t> cFF =
      MakePowerOfTwoMask<uint32_t, 0xFFu>();
  MOZ_RELEASE_ASSERT(cFF.MaskValue() == 0xFFu);

  static_assert(
      MakePowerOfTwoMask<uint32_t, 0xFFFFFFFFu>().MaskValue() == 0xFFFFFFFFu,
      "");
  constexpr PowerOfTwoMask<uint32_t> cFFFFFFFF =
      MakePowerOfTwoMask<uint32_t, 0xFFFFFFFFu>();
  MOZ_RELEASE_ASSERT(cFFFFFFFF.MaskValue() == 0xFFFFFFFFu);

  struct TestDataU32 {
    uint32_t mInput;
    uint32_t mMask;
  };
  // clang-format off
  TestDataU32 tests[] = {
    { 0, 0 },
    { 1, 1 },
    { 2, 3 },
    { 3, 3 },
    { 4, 7 },
    { 5, 7 },
    { (1u << 31) - 1, (1u << 31) - 1 },
    { (1u << 31), uint32_t(-1) },
    { (1u << 31) + 1, uint32_t(-1) },
    { uint32_t(-1), uint32_t(-1) }
  };
  // clang-format on
  for (const TestDataU32& test : tests) {
    PowerOfTwoMask<uint32_t> p2m(test.mInput);
    MOZ_RELEASE_ASSERT(p2m.MaskValue() == test.mMask);
    for (const TestDataU32& inner : tests) {
      if (p2m.MaskValue() != uint32_t(-1)) {
        MOZ_RELEASE_ASSERT((inner.mInput % p2m) ==
                           (inner.mInput % (p2m.MaskValue() + 1)));
      }
      MOZ_RELEASE_ASSERT((inner.mInput & p2m) == (inner.mInput % p2m));
      MOZ_RELEASE_ASSERT((p2m & inner.mInput) == (inner.mInput & p2m));
    }
  }

  printf("TestPowerOfTwoMask done\n");
}

void TestPowerOfTwo() {
  printf("TestPowerOfTwo...\n");

  static_assert(MakePowerOfTwo<uint32_t, 1>().Value() == 1, "");
  constexpr PowerOfTwo<uint32_t> c1 = MakePowerOfTwo<uint32_t, 1>();
  MOZ_RELEASE_ASSERT(c1.Value() == 1);
  static_assert(MakePowerOfTwo<uint32_t, 1>().Mask().MaskValue() == 0, "");

  static_assert(MakePowerOfTwo<uint32_t, 128>().Value() == 128, "");
  constexpr PowerOfTwo<uint32_t> c128 = MakePowerOfTwo<uint32_t, 128>();
  MOZ_RELEASE_ASSERT(c128.Value() == 128);
  static_assert(MakePowerOfTwo<uint32_t, 128>().Mask().MaskValue() == 127, "");

  static_assert(MakePowerOfTwo<uint32_t, 0x80000000u>().Value() == 0x80000000u,
                "");
  constexpr PowerOfTwo<uint32_t> cMax = MakePowerOfTwo<uint32_t, 0x80000000u>();
  MOZ_RELEASE_ASSERT(cMax.Value() == 0x80000000u);
  static_assert(
      MakePowerOfTwo<uint32_t, 0x80000000u>().Mask().MaskValue() == 0x7FFFFFFFu,
      "");

  struct TestDataU32 {
    uint32_t mInput;
    uint32_t mValue;
    uint32_t mMask;
  };
  // clang-format off
  TestDataU32 tests[] = {
    { 0, 1, 0 },
    { 1, 1, 0 },
    { 2, 2, 1 },
    { 3, 4, 3 },
    { 4, 4, 3 },
    { 5, 8, 7 },
    { (1u << 31) - 1, (1u << 31), (1u << 31) - 1 },
    { (1u << 31), (1u << 31), (1u << 31) - 1 },
    { (1u << 31) + 1, (1u << 31), (1u << 31) - 1 },
    { uint32_t(-1), (1u << 31), (1u << 31) - 1 }
  };
  // clang-format on
  for (const TestDataU32& test : tests) {
    PowerOfTwo<uint32_t> p2(test.mInput);
    MOZ_RELEASE_ASSERT(p2.Value() == test.mValue);
    MOZ_RELEASE_ASSERT(p2.MaskValue() == test.mMask);
    PowerOfTwoMask<uint32_t> p2m = p2.Mask();
    MOZ_RELEASE_ASSERT(p2m.MaskValue() == test.mMask);
    for (const TestDataU32& inner : tests) {
      MOZ_RELEASE_ASSERT((inner.mInput % p2) == (inner.mInput % p2.Value()));
    }
  }

  printf("TestPowerOfTwo done\n");
}

void TestLEB128() {
  printf("TestLEB128...\n");

  MOZ_RELEASE_ASSERT(ULEB128MaxSize<uint8_t>() == 2);
  MOZ_RELEASE_ASSERT(ULEB128MaxSize<uint16_t>() == 3);
  MOZ_RELEASE_ASSERT(ULEB128MaxSize<uint32_t>() == 5);
  MOZ_RELEASE_ASSERT(ULEB128MaxSize<uint64_t>() == 10);

  struct TestDataU64 {
    uint64_t mValue;
    unsigned mSize;
    const char* mBytes;
  };
  // clang-format off
  TestDataU64 tests[] = {
    // Small numbers should keep their normal byte representation.
    {                  0u,  1, "\0" },
    {                  1u,  1, "\x01" },

    // 0111 1111 (127, or 0x7F) is the highest number that fits into a single
    // LEB128 byte. It gets encoded as 0111 1111, note the most significant bit
    // is off.
    {               0x7Fu,  1, "\x7F" },

    // Next number: 128, or 0x80.
    //   Original data representation:  1000 0000
    //     Broken up into groups of 7:         1  0000000
    // Padded with 0 (msB) or 1 (lsB):  00000001 10000000
    //            Byte representation:  0x01     0x80
    //            Little endian order:  -> 0x80 0x01
    {               0x80u,  2, "\x80\x01" },

    // Next: 129, or 0x81 (showing that we don't lose low bits.)
    //   Original data representation:  1000 0001
    //     Broken up into groups of 7:         1  0000001
    // Padded with 0 (msB) or 1 (lsB):  00000001 10000001
    //            Byte representation:  0x01     0x81
    //            Little endian order:  -> 0x81 0x01
    {               0x81u,  2, "\x81\x01" },

    // Highest 8-bit number: 255, or 0xFF.
    //   Original data representation:  1111 1111
    //     Broken up into groups of 7:         1  1111111
    // Padded with 0 (msB) or 1 (lsB):  00000001 11111111
    //            Byte representation:  0x01     0xFF
    //            Little endian order:  -> 0xFF 0x01
    {               0xFFu,  2, "\xFF\x01" },

    // Next: 256, or 0x100.
    //   Original data representation:  1 0000 0000
    //     Broken up into groups of 7:        10  0000000
    // Padded with 0 (msB) or 1 (lsB):  00000010 10000000
    //            Byte representation:  0x10     0x80
    //            Little endian order:  -> 0x80 0x02
    {              0x100u,  2, "\x80\x02" },

    // Highest 32-bit number: 0xFFFFFFFF (8 bytes, all bits set).
    // Original: 1111 1111 1111 1111 1111 1111 1111 1111
    // Groups:     1111  1111111  1111111  1111111  1111111
    // Padded: 00001111 11111111 11111111 11111111 11111111
    // Bytes:  0x0F     0xFF     0xFF     0xFF     0xFF
    // Little Endian: -> 0xFF 0xFF 0xFF 0xFF 0x0F
    {         0xFFFFFFFFu,  5, "\xFF\xFF\xFF\xFF\x0F" },

    // Highest 64-bit number: 0xFFFFFFFFFFFFFFFF (16 bytes, all bits set).
    // 64 bits, that's 9 groups of 7 bits, plus 1 (most significant) bit.
    { 0xFFFFFFFFFFFFFFFFu, 10, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01" }
  };
  // clang-format on

  for (const TestDataU64& test : tests) {
    MOZ_RELEASE_ASSERT(ULEB128Size(test.mValue) == test.mSize);
    // Prepare a buffer that can accomodate the largest-possible LEB128.
    uint8_t buffer[ULEB128MaxSize<uint64_t>()];
    // Use a pointer into the buffer as iterator.
    uint8_t* p = buffer;
    // And write the LEB128.
    WriteULEB128(test.mValue, p);
    // Pointer (iterator) should have advanced just past the expected LEB128
    // size.
    MOZ_RELEASE_ASSERT(p == buffer + test.mSize);
    // Check expected bytes.
    for (unsigned i = 0; i < test.mSize; ++i) {
      MOZ_RELEASE_ASSERT(buffer[i] == uint8_t(test.mBytes[i]));
    }
    // Move pointer (iterator) back to start of buffer.
    p = buffer;
    // And read the LEB128 we wrote above.
    uint64_t read = ReadULEB128<uint64_t>(p);
    // Pointer (iterator) should have also advanced just past the expected
    // LEB128 size.
    MOZ_RELEASE_ASSERT(p == buffer + test.mSize);
    // And check the read value.
    MOZ_RELEASE_ASSERT(read == test.mValue);
  }

  printf("TestLEB128 done\n");
}

static void TestModuloBuffer(ModuloBuffer<>& mb, uint32_t MBSize) {
  using MB = ModuloBuffer<>;

  MOZ_RELEASE_ASSERT(mb.BufferLength().Value() == MBSize);

  // Iterator comparisons.
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) == mb.ReaderAt(2));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) != mb.ReaderAt(3));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) < mb.ReaderAt(3));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) <= mb.ReaderAt(2));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) <= mb.ReaderAt(3));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(3) > mb.ReaderAt(2));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) >= mb.ReaderAt(2));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(3) >= mb.ReaderAt(2));

  // Iterators indices don't wrap around (even though they may be pointing at
  // the same location).
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) != mb.ReaderAt(MBSize + 2));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(MBSize + 2) != mb.ReaderAt(2));

  // Dereference.
  static_assert(std::is_same<decltype(*mb.ReaderAt(0)), const MB::Byte&>::value,
                "Dereferencing from a reader should return const Byte*");
  static_assert(std::is_same<decltype(*mb.WriterAt(0)), MB::Byte&>::value,
                "Dereferencing from a writer should return Byte*");
  // Contiguous between 0 and MBSize-1.
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(MBSize - 1) ==
                     &*mb.ReaderAt(0) + (MBSize - 1));
  // Wraps around.
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(MBSize) == &*mb.ReaderAt(0));
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(MBSize + MBSize - 1) ==
                     &*mb.ReaderAt(MBSize - 1));
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(MBSize + MBSize) == &*mb.ReaderAt(0));
  // Power of 2 modulo wrapping.
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(uint32_t(-1)) == &*mb.ReaderAt(MBSize - 1));
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(static_cast<MB::Index>(-1)) ==
                     &*mb.ReaderAt(MBSize - 1));

  // Arithmetic.
  MB::Reader arit = mb.ReaderAt(0);
  MOZ_RELEASE_ASSERT(++arit == mb.ReaderAt(1));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(1));

  MOZ_RELEASE_ASSERT(--arit == mb.ReaderAt(0));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(0));

  MOZ_RELEASE_ASSERT(arit++ == mb.ReaderAt(0));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(1));

  MOZ_RELEASE_ASSERT(arit-- == mb.ReaderAt(1));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(0));

  MOZ_RELEASE_ASSERT(arit + 3 == mb.ReaderAt(3));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(0));

  MOZ_RELEASE_ASSERT(4 + arit == mb.ReaderAt(4));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(0));

  // (Can't have assignments inside asserts, hence the split.)
  const bool checkPlusEq = ((arit += 3) == mb.ReaderAt(3));
  MOZ_RELEASE_ASSERT(checkPlusEq);
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(3));

  MOZ_RELEASE_ASSERT((arit - 2) == mb.ReaderAt(1));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(3));

  const bool checkMinusEq = ((arit -= 2) == mb.ReaderAt(1));
  MOZ_RELEASE_ASSERT(checkMinusEq);
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(1));

  // Random access.
  MOZ_RELEASE_ASSERT(&arit[3] == &*(arit + 3));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(1));

  // Iterator difference.
  MOZ_RELEASE_ASSERT(mb.ReaderAt(3) - mb.ReaderAt(1) == 2);
  MOZ_RELEASE_ASSERT(mb.ReaderAt(1) - mb.ReaderAt(3) == MB::Index(-2));

  // Only testing Writer, as Reader is just a subset with no code differences.
  MB::Writer it = mb.WriterAt(0);
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 0);

  // Write two characters at the start.
  it.WriteObject('x');
  it.WriteObject('y');

  // Backtrack to read them.
  it -= 2;
  // PeekObject should read without moving.
  MOZ_RELEASE_ASSERT(it.PeekObject<char>() == 'x');
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 0);
  // ReadObject should read and move past the character.
  MOZ_RELEASE_ASSERT(it.ReadObject<char>() == 'x');
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 1);
  MOZ_RELEASE_ASSERT(it.PeekObject<char>() == 'y');
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 1);
  MOZ_RELEASE_ASSERT(it.ReadObject<char>() == 'y');
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 2);

  // Checking that a reader can be created from a writer.
  MB::Reader it2(it);
  MOZ_RELEASE_ASSERT(it2.CurrentIndex() == 2);
  // Or assigned.
  it2 = it;
  MOZ_RELEASE_ASSERT(it2.CurrentIndex() == 2);

  // Iterator traits.
  static_assert(std::is_same<std::iterator_traits<MB::Reader>::difference_type,
                             MB::Index>::value,
                "ModuloBuffer::Reader::difference_type should be Index");
  static_assert(std::is_same<std::iterator_traits<MB::Reader>::value_type,
                             MB::Byte>::value,
                "ModuloBuffer::Reader::value_type should be Byte");
  static_assert(std::is_same<std::iterator_traits<MB::Reader>::pointer,
                             const MB::Byte*>::value,
                "ModuloBuffer::Reader::pointer should be const Byte*");
  static_assert(std::is_same<std::iterator_traits<MB::Reader>::reference,
                             const MB::Byte&>::value,
                "ModuloBuffer::Reader::reference should be const Byte&");
  static_assert(std::is_base_of<
                    std::input_iterator_tag,
                    std::iterator_traits<MB::Reader>::iterator_category>::value,
                "ModuloBuffer::Reader::iterator_category should be derived "
                "from input_iterator_tag");
  static_assert(std::is_base_of<
                    std::forward_iterator_tag,
                    std::iterator_traits<MB::Reader>::iterator_category>::value,
                "ModuloBuffer::Reader::iterator_category should be derived "
                "from forward_iterator_tag");
  static_assert(std::is_base_of<
                    std::bidirectional_iterator_tag,
                    std::iterator_traits<MB::Reader>::iterator_category>::value,
                "ModuloBuffer::Reader::iterator_category should be derived "
                "from bidirectional_iterator_tag");
  static_assert(
      std::is_same<std::iterator_traits<MB::Reader>::iterator_category,
                   std::random_access_iterator_tag>::value,
      "ModuloBuffer::Reader::iterator_category should be "
      "random_access_iterator_tag");

  // Use as input iterator by std::string constructor (which is only considered
  // with proper input iterators.)
  std::string s(mb.ReaderAt(0), mb.ReaderAt(2));
  MOZ_RELEASE_ASSERT(s == "xy");

  // Write 4-byte number at index 2.
  it.WriteObject(int32_t(123));
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 6);
  // And another, which should now wrap around (but index continues on.)
  it.WriteObject(int32_t(456));
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == MBSize + 2);
  // Even though index==MBSize+2, we can read the object we wrote at 2.
  MOZ_RELEASE_ASSERT(it.ReadObject<int32_t>() == 123);
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == MBSize + 6);
  // And similarly, index MBSize+6 points at the same location as index 6.
  MOZ_RELEASE_ASSERT(it.ReadObject<int32_t>() == 456);
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == MBSize + MBSize + 2);
}

void TestModuloBuffer() {
  printf("TestModuloBuffer...\n");

  // Testing ModuloBuffer with default template arguments.
  using MB = ModuloBuffer<>;

  // Only 8-byte buffers, to easily test wrap-around.
  constexpr uint32_t MBSize = 8;

  // MB with self-allocated heap buffer.
  MB mbByLength(MakePowerOfTwo32<MBSize>());
  TestModuloBuffer(mbByLength, MBSize);

  // MB taking ownership of a provided UniquePtr to a buffer.
  auto uniqueBuffer = MakeUnique<uint8_t[]>(MBSize);
  MB mbByUniquePtr(MakeUnique<uint8_t[]>(MBSize), MakePowerOfTwo32<MBSize>());
  TestModuloBuffer(mbByUniquePtr, MBSize);

  // MB using part of a buffer on the stack. The buffer is three times the
  // required size: The middle third is where ModuloBuffer will work, the first
  // and last thirds are only used to later verify that ModuloBuffer didn't go
  // out of its bounds.
  uint8_t buffer[MBSize * 3];
  // Pre-fill the buffer with a known pattern, so we can later see what changed.
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }
  MB mbByBuffer(&buffer[MBSize], MakePowerOfTwo32<MBSize>());
  TestModuloBuffer(mbByBuffer, MBSize);

  // Check that only the provided stack-based sub-buffer was modified.
  uint32_t changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  // Check that move-construction is allowed. This verifies that we do not
  // crash from a double free, when `mbByBuffer` and `mbByStolenBuffer` are both
  // destroyed at the end of this function.
  MB mbByStolenBuffer = std::move(mbByBuffer);
  TestModuloBuffer(mbByStolenBuffer, MBSize);

  // Check that only the provided stack-based sub-buffer was modified.
  changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  // This test function does a `ReadInto` as directed, and checks that the
  // result is the same as if the copy had been done manually byte-by-byte.
  // `TestReadInto(3, 7, 2)` copies from index 3 to index 7, 2 bytes long.
  // Return the output string (from `ReadInto`) for external checks.
  auto TestReadInto = [](MB::Index aReadFrom, MB::Index aWriteTo,
                         MB::Length aBytes) {
    constexpr uint32_t TRISize = 16;

    // Prepare an input buffer, all different elements.
    uint8_t input[TRISize + 1] = "ABCDEFGHIJKLMNOP";
    const MB mbInput(input, MakePowerOfTwo32<TRISize>());

    // Prepare an output buffer, different from input.
    uint8_t output[TRISize + 1] = "abcdefghijklmnop";
    MB mbOutput(output, MakePowerOfTwo32<TRISize>());

    // Run ReadInto.
    auto writer = mbOutput.WriterAt(aWriteTo);
    mbInput.ReaderAt(aReadFrom).ReadInto(writer, aBytes);

    // Do the same operation manually.
    uint8_t outputCheck[TRISize + 1] = "abcdefghijklmnop";
    MB mbOutputCheck(outputCheck, MakePowerOfTwo32<TRISize>());
    auto readerCheck = mbInput.ReaderAt(aReadFrom);
    auto writerCheck = mbOutputCheck.WriterAt(aWriteTo);
    for (MB::Length i = 0; i < aBytes; ++i) {
      *writerCheck++ = *readerCheck++;
    }

    // Compare the two outputs.
    for (uint32_t i = 0; i < TRISize; ++i) {
#  ifdef TEST_MODULOBUFFER_FAILURE_DEBUG
      // Only used when debugging failures.
      if (output[i] != outputCheck[i]) {
        printf(
            "*** from=%u to=%u bytes=%u i=%u\ninput:  '%s'\noutput: "
            "'%s'\ncheck:  '%s'\n",
            unsigned(aReadFrom), unsigned(aWriteTo), unsigned(aBytes),
            unsigned(i), input, output, outputCheck);
      }
#  endif
      MOZ_RELEASE_ASSERT(output[i] == outputCheck[i]);
    }

#  ifdef TEST_MODULOBUFFER_HELPER
    // Only used when adding more tests.
    printf("*** from=%u to=%u bytes=%u output: %s\n", unsigned(aReadFrom),
           unsigned(aWriteTo), unsigned(aBytes), output);
#  endif

    return std::string(reinterpret_cast<const char*>(output));
  };

  // A few manual checks:
  constexpr uint32_t TRISize = 16;
  MOZ_RELEASE_ASSERT(TestReadInto(0, 0, 0) == "abcdefghijklmnop");
  MOZ_RELEASE_ASSERT(TestReadInto(0, 0, TRISize) == "ABCDEFGHIJKLMNOP");
  MOZ_RELEASE_ASSERT(TestReadInto(0, 5, TRISize) == "LMNOPABCDEFGHIJK");
  MOZ_RELEASE_ASSERT(TestReadInto(5, 0, TRISize) == "FGHIJKLMNOPABCDE");

  // Test everything! (16^3 = 4096, not too much.)
  for (MB::Index r = 0; r < TRISize; ++r) {
    for (MB::Index w = 0; w < TRISize; ++w) {
      for (MB::Length len = 0; len < TRISize; ++len) {
        TestReadInto(r, w, len);
      }
    }
  }

  printf("TestModuloBuffer done\n");
}

// Backdoor into value of BlockIndex, only for unit-testing.
static uint64_t ExtractBlockIndex(const BlocksRingBuffer::BlockIndex bi) {
  uint64_t index;
  static_assert(sizeof(bi) == sizeof(index),
                "BlockIndex expected to only contain a uint64_t");
  memcpy(&index, &bi, sizeof(index));
  return index;
};

void TestBlocksRingBufferAPI() {
  printf("TestBlocksRingBufferAPI...\n");

  // Create a 16-byte buffer, enough to store up to 3 entries (1 byte size + 4
  // bytes uint64_t).
  constexpr uint32_t MBSize = 16;
  uint8_t buffer[MBSize * 3];
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }

  // Start a temporary block to constrain buffer lifetime.
  {
    BlocksRingBuffer rb(BlocksRingBuffer::ThreadSafety::WithMutex,
                        &buffer[MBSize], MakePowerOfTwo32<MBSize>());

#  define VERIFY_START_END_PUSHED_CLEARED(aStart, aEnd, aPushed, aCleared)  \
    {                                                                       \
      BlocksRingBuffer::State state = rb.GetState();                        \
      MOZ_RELEASE_ASSERT(ExtractBlockIndex(state.mRangeStart) == (aStart)); \
      MOZ_RELEASE_ASSERT(ExtractBlockIndex(state.mRangeEnd) == (aEnd));     \
      MOZ_RELEASE_ASSERT(state.mPushedBlockCount == (aPushed));             \
      MOZ_RELEASE_ASSERT(state.mClearedBlockCount == (aCleared));           \
    }

    // All entries will contain one 32-bit number. The resulting blocks will
    // have the following structure:
    // - 1 byte for the LEB128 size of 4
    // - 4 bytes for the number.
    // E.g., if we have entries with `123` and `456`:
    //   .-- Index 0 reserved for empty BlockIndex, nothing there.
    //   | .-- first readable block at index 1
    //   | |.-- first block at index 1
    //   | ||.-- 1 byte for the entry size, which is `4` (32 bits)
    //   | |||  .-- entry starts at index 2, contains 32-bit int
    //   | |||  |             .-- entry and block finish *after* index 5 (so 6)
    //   | |||  |             | .-- second block starts at index 6
    //   | |||  |             | |         etc.
    //   | |||  |             | |                  .-- End readable blocks: 11
    //   v vvv  v             v V                  v
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    //   - S[4 |   int(123)   ] [4 |   int(456)   ]E

    // Empty buffer to start with.
    // Start&end indices still at 1 (0 is reserved for the default BlockIndex{}
    // that cannot point at a valid entry), nothing cleared.
    VERIFY_START_END_PUSHED_CLEARED(1, 1, 0, 0);

    // Default BlockIndex.
    BlocksRingBuffer::BlockIndex bi0;
    if (bi0) {
      MOZ_RELEASE_ASSERT(false, "if (BlockIndex{}) should fail test");
    }
    if (!bi0) {
    } else {
      MOZ_RELEASE_ASSERT(false, "if (!BlockIndex{}) should succeed test");
    }
    MOZ_RELEASE_ASSERT(!bi0);
    MOZ_RELEASE_ASSERT(bi0 == bi0);
    MOZ_RELEASE_ASSERT(bi0 <= bi0);
    MOZ_RELEASE_ASSERT(bi0 >= bi0);
    MOZ_RELEASE_ASSERT(!(bi0 != bi0));
    MOZ_RELEASE_ASSERT(!(bi0 < bi0));
    MOZ_RELEASE_ASSERT(!(bi0 > bi0));

    // Default BlockIndex can be used, but returns no valid entry.
    rb.ReadAt(bi0, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isNothing());
    });

    // Push `1` directly.
    MOZ_RELEASE_ASSERT(ExtractBlockIndex(rb.PutObject(uint32_t(1))) == 1);
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    //   - S[4 |    int(1)    ]E
    VERIFY_START_END_PUSHED_CLEARED(1, 6, 1, 0);

    // Push `2` through ReserveAndPut, check output BlockIndex.
    auto bi2 = rb.ReserveAndPut([]() { return sizeof(uint32_t); },
                                [](BlocksRingBuffer::EntryWriter* aEW) {
                                  MOZ_RELEASE_ASSERT(!!aEW);
                                  aEW->WriteObject(uint32_t(2));
                                  return aEW->CurrentBlockIndex();
                                });
    static_assert(
        std::is_same<decltype(bi2), BlocksRingBuffer::BlockIndex>::value,
        "All index-returning functions should return a "
        "BlocksRingBuffer::BlockIndex");
    MOZ_RELEASE_ASSERT(ExtractBlockIndex(bi2) == 6);
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    //   - S[4 |    int(1)    ] [4 |    int(2)    ]E
    VERIFY_START_END_PUSHED_CLEARED(1, 11, 2, 0);

    // Check single entry at bi2, store next block index.
    auto bi2Next =
        rb.ReadAt(bi2, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
          MOZ_RELEASE_ASSERT(aMaybeReader.isSome());
          MOZ_RELEASE_ASSERT(aMaybeReader->ReadObject<uint32_t>() == 2);
          MOZ_RELEASE_ASSERT(
              aMaybeReader->GetEntryAt(aMaybeReader->NextBlockIndex())
                  .isNothing());
          return aMaybeReader->NextBlockIndex();
        });
    // bi2Next is at the end, nothing to read.
    rb.ReadAt(bi2Next, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isNothing());
    });

    // BlockIndex tests.
    if (bi2) {
    } else {
      MOZ_RELEASE_ASSERT(false,
                         "if (non-default-BlockIndex) should succeed test");
    }
    if (!bi2) {
      MOZ_RELEASE_ASSERT(false,
                         "if (!non-default-BlockIndex) should fail test");
    }

    MOZ_RELEASE_ASSERT(!!bi2);
    MOZ_RELEASE_ASSERT(bi2 == bi2);
    MOZ_RELEASE_ASSERT(bi2 <= bi2);
    MOZ_RELEASE_ASSERT(bi2 >= bi2);
    MOZ_RELEASE_ASSERT(!(bi2 != bi2));
    MOZ_RELEASE_ASSERT(!(bi2 < bi2));
    MOZ_RELEASE_ASSERT(!(bi2 > bi2));

    MOZ_RELEASE_ASSERT(bi0 != bi2);
    MOZ_RELEASE_ASSERT(bi0 < bi2);
    MOZ_RELEASE_ASSERT(bi0 <= bi2);
    MOZ_RELEASE_ASSERT(!(bi0 == bi2));
    MOZ_RELEASE_ASSERT(!(bi0 > bi2));
    MOZ_RELEASE_ASSERT(!(bi0 >= bi2));

    MOZ_RELEASE_ASSERT(bi2 != bi0);
    MOZ_RELEASE_ASSERT(bi2 > bi0);
    MOZ_RELEASE_ASSERT(bi2 >= bi0);
    MOZ_RELEASE_ASSERT(!(bi2 == bi0));
    MOZ_RELEASE_ASSERT(!(bi2 < bi0));
    MOZ_RELEASE_ASSERT(!(bi2 <= bi0));

    MOZ_RELEASE_ASSERT(bi2 != bi2Next);
    MOZ_RELEASE_ASSERT(bi2 < bi2Next);
    MOZ_RELEASE_ASSERT(bi2 <= bi2Next);
    MOZ_RELEASE_ASSERT(!(bi2 == bi2Next));
    MOZ_RELEASE_ASSERT(!(bi2 > bi2Next));
    MOZ_RELEASE_ASSERT(!(bi2 >= bi2Next));

    MOZ_RELEASE_ASSERT(bi2Next != bi2);
    MOZ_RELEASE_ASSERT(bi2Next > bi2);
    MOZ_RELEASE_ASSERT(bi2Next >= bi2);
    MOZ_RELEASE_ASSERT(!(bi2Next == bi2));
    MOZ_RELEASE_ASSERT(!(bi2Next < bi2));
    MOZ_RELEASE_ASSERT(!(bi2Next <= bi2));

    // Push `3` through Put, check writer output
    // is returned to the initial caller.
    auto put3 =
        rb.Put(sizeof(uint32_t), [&](BlocksRingBuffer::EntryWriter* aEW) {
          MOZ_RELEASE_ASSERT(!!aEW);
          aEW->WriteObject(uint32_t(3));
          return float(ExtractBlockIndex(aEW->CurrentBlockIndex()));
        });
    static_assert(std::is_same<decltype(put3), float>::value,
                  "Expect float as returned by callback.");
    MOZ_RELEASE_ASSERT(put3 == 11.0);
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15 (16)
    //   - S[4 |    int(1)    ] [4 |    int(2)    ] [4 |    int(3)    ]E
    VERIFY_START_END_PUSHED_CLEARED(1, 16, 3, 0);

    // Re-Read single entry at bi2, should now have a next entry.
    rb.ReadAt(bi2, [&](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isSome());
      MOZ_RELEASE_ASSERT(aMaybeReader->ReadObject<uint32_t>() == 2);
      MOZ_RELEASE_ASSERT(aMaybeReader->NextBlockIndex() == bi2Next);
      MOZ_RELEASE_ASSERT(aMaybeReader->GetNextEntry().isSome());
      MOZ_RELEASE_ASSERT(
          aMaybeReader->GetEntryAt(aMaybeReader->NextBlockIndex()).isSome());
      MOZ_RELEASE_ASSERT(
          aMaybeReader->GetNextEntry()->CurrentBlockIndex() ==
          aMaybeReader->GetEntryAt(aMaybeReader->NextBlockIndex())
              ->CurrentBlockIndex());
      MOZ_RELEASE_ASSERT(
          aMaybeReader->GetEntryAt(aMaybeReader->NextBlockIndex())
              ->ReadObject<uint32_t>() == 3);
    });

    // Check that we have `1` to `3`.
    uint32_t count = 0;
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aReader) {
      MOZ_RELEASE_ASSERT(aReader.ReadObject<uint32_t>() == ++count);
    });
    MOZ_RELEASE_ASSERT(count == 3);

    // Push `4`, store its BlockIndex for later.
    // This will wrap around, and clear the first entry.
    BlocksRingBuffer::BlockIndex bi4 = rb.PutObject(uint32_t(4));
    // Before:
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15 (16)
    //   - S[4 |    int(1)    ] [4 |    int(2)    ] [4 |    int(3)    ]E
    // 1. First entry cleared:
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15 (16)
    //   -   ?   ?   ?   ?   ? S[4 |    int(2)    ] [4 |    int(3)    ]E
    // 2. New entry starts at 15 and wraps around: (shown on separate line)
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15 (16)
    //   -   ?   ?   ?   ?   ? S[4 |    int(2)    ] [4 |    int(3)    ]
    //  16  17  18  19  20  21  ...
    //  [4 |    int(4)    ]E
    // (collapsed)
    //  16  17  18  19  20  21   6   7   8   9  10  11  12  13  14  15 (16)
    //  [4 |    int(4)    ]E ? S[4 |    int(2)    ] [4 |    int(3)    ]
    VERIFY_START_END_PUSHED_CLEARED(6, 21, 4, 1);

    // Check that we have `2` to `4`.
    count = 1;
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aReader) {
      MOZ_RELEASE_ASSERT(aReader.ReadObject<uint32_t>() == ++count);
    });
    MOZ_RELEASE_ASSERT(count == 4);

    // Push 5 through Put, no returns.
    // This will clear the second entry.
    // Check that the EntryWriter can access bi4 but not bi2.
    auto bi5_6 =
        rb.Put(sizeof(uint32_t), [&](BlocksRingBuffer::EntryWriter* aEW) {
          MOZ_RELEASE_ASSERT(!!aEW);
          aEW->WriteObject(uint32_t(5));
          MOZ_RELEASE_ASSERT(aEW->GetEntryAt(bi2).isNothing());
          MOZ_RELEASE_ASSERT(aEW->GetEntryAt(bi4).isSome());
          MOZ_RELEASE_ASSERT(aEW->GetEntryAt(bi4)->CurrentBlockIndex() == bi4);
          MOZ_RELEASE_ASSERT(aEW->GetEntryAt(bi4)->ReadObject<uint32_t>() == 4);
          return MakePair(aEW->CurrentBlockIndex(), aEW->BlockEndIndex());
        });
    auto& bi5 = bi5_6.first();
    auto& bi6 = bi5_6.second();
    //  16  17  18  19  20  21  22  23  24  25  26  11  12  13  14  15 (16)
    //  [4 |    int(4)    ] [4 |    int(5)    ]E ? S[4 |    int(3)    ]
    VERIFY_START_END_PUSHED_CLEARED(11, 26, 5, 2);

    // Read single entry at bi2, should now gracefully fail.
    rb.ReadAt(bi2, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isNothing());
    });

    // Read single entry at bi5.
    rb.ReadAt(bi5, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isSome());
      MOZ_RELEASE_ASSERT(aMaybeReader->ReadObject<uint32_t>() == 5);
      MOZ_RELEASE_ASSERT(
          aMaybeReader->GetEntryAt(aMaybeReader->NextBlockIndex()).isNothing());
    });

    rb.Read([&](BlocksRingBuffer::Reader* aReader) {
      MOZ_RELEASE_ASSERT(!!aReader);
      // begin() and end() should be at the range edges (verified above).
      MOZ_RELEASE_ASSERT(
          ExtractBlockIndex(aReader->begin().CurrentBlockIndex()) == 11);
      MOZ_RELEASE_ASSERT(
          ExtractBlockIndex(aReader->end().CurrentBlockIndex()) == 26);
      // Null BlockIndex clamped to the beginning.
      MOZ_RELEASE_ASSERT(aReader->At(bi0) == aReader->begin());
      // Cleared block index clamped to the beginning.
      MOZ_RELEASE_ASSERT(aReader->At(bi2) == aReader->begin());
      // At(begin) same as begin().
      MOZ_RELEASE_ASSERT(aReader->At(aReader->begin().CurrentBlockIndex()) ==
                         aReader->begin());
      // bi5 at expected position.
      MOZ_RELEASE_ASSERT(
          ExtractBlockIndex(aReader->At(bi5).CurrentBlockIndex()) == 21);
      // bi6 at expected position at the end.
      MOZ_RELEASE_ASSERT(aReader->At(bi6) == aReader->end());
      // At(end) same as end().
      MOZ_RELEASE_ASSERT(aReader->At(aReader->end().CurrentBlockIndex()) ==
                         aReader->end());
    });

    // Check that we have `3` to `5`.
    count = 2;
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aReader) {
      MOZ_RELEASE_ASSERT(aReader.ReadObject<uint32_t>() == ++count);
    });
    MOZ_RELEASE_ASSERT(count == 5);

    // Clear everything before `4`, this should clear `3`.
    rb.ClearBefore(bi4);
    //  16  17  18  19  20  21  22  23  24  25  26  11  12  13  14  15
    // S[4 |    int(4)    ] [4 |    int(5)    ]E ?   ?   ?   ?   ?   ?
    VERIFY_START_END_PUSHED_CLEARED(16, 26, 5, 3);

    // Check that we have `4` to `5`.
    count = 3;
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aReader) {
      MOZ_RELEASE_ASSERT(aReader.ReadObject<uint32_t>() == ++count);
    });
    MOZ_RELEASE_ASSERT(count == 5);

    // Clear everything before `4` again, nothing to clear.
    rb.ClearBefore(bi4);
    VERIFY_START_END_PUSHED_CLEARED(16, 26, 5, 3);

    // Clear everything, this should clear `4` and `5`, and bring the start
    // index where the end index currently is.
    rb.ClearBefore(bi6);
    //  16  17  18  19  20  21  22  23  24  25  26  11  12  13  14  15
    //   ?   ?   ?   ?   ?   ?   ?   ?   ?   ? SE?   ?   ?   ?   ?   ?
    VERIFY_START_END_PUSHED_CLEARED(26, 26, 5, 5);

    // Check that we have nothing to read.
    rb.ReadEach([&](auto&&) { MOZ_RELEASE_ASSERT(false); });

    // Read single entry at bi5, should now gracefully fail.
    rb.ReadAt(bi5, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isNothing());
    });

    // Clear everything before now-cleared `4`, nothing to clear.
    rb.ClearBefore(bi4);
    VERIFY_START_END_PUSHED_CLEARED(26, 26, 5, 5);

    // Push `6` directly.
    MOZ_RELEASE_ASSERT(rb.PutObject(uint32_t(6)) == bi6);
    //  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31
    //   ?   ?   ?   ?   ?   ?   ?   ?   ?   ? S[4 |    int(6)    ]E ?
    VERIFY_START_END_PUSHED_CLEARED(26, 31, 6, 5);

    {
      // Create a 2nd buffer and fill it with `7` and `8`.
      uint8_t buffer2[MBSize];
      BlocksRingBuffer rb2(BlocksRingBuffer::ThreadSafety::WithoutMutex,
                           buffer2, MakePowerOfTwo32<MBSize>());
      rb2.PutObject(uint32_t(7));
      rb2.PutObject(uint32_t(8));
      // Main buffer shouldn't have changed.
      VERIFY_START_END_PUSHED_CLEARED(26, 31, 6, 5);

      // Append contents of rb2 to rb, this should end up being the same as
      // pushing the two numbers.
      rb.AppendContents(rb2);
      //  32  33  34  35  36  37  38  39  40  41  26  27  28  29  30  31
      //      int(7)    ] [4 |    int(8)    ]E ? S[4 |    int(6)    ] [4 |
      VERIFY_START_END_PUSHED_CLEARED(26, 41, 8, 5);

      // Append contents of rb2 to rb again, to verify that rb2 was not modified
      // above. This should clear `6` and the first `7`.
      rb.AppendContents(rb2);
      //  48  49  50  51  36  37  38  39  40  41  42  43  44  45  46  47
      //  int(8)    ]E ? S[4 |    int(8)    ] [4 |    int(7)    ] [4 |
      VERIFY_START_END_PUSHED_CLEARED(36, 51, 10, 7);

      // End of block where rb2 lives, to verify that it is not needed anymore
      // for its copied values to survive in rb.
    }
    VERIFY_START_END_PUSHED_CLEARED(36, 51, 10, 7);

    // bi6 should now have been cleared.
    rb.ReadAt(bi6, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isNothing());
    });

    // Check that we have `8`, `7`, `8`.
    count = 0;
    uint32_t expected[3] = {8, 7, 8};
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aReader) {
      MOZ_RELEASE_ASSERT(count < 3);
      MOZ_RELEASE_ASSERT(aReader.ReadObject<uint32_t>() == expected[count++]);
    });
    MOZ_RELEASE_ASSERT(count == 3);

    // End of block where rb lives, BlocksRingBuffer destructor should call
    // entry destructor for remaining entries.
  }

  // Check that only the provided stack-based sub-buffer was modified.
  uint32_t changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  printf("TestBlocksRingBufferAPI done\n");
}

void TestBlocksRingBufferUnderlyingBufferChanges() {
  printf("TestBlocksRingBufferUnderlyingBufferChanges...\n");

  // Out-of-session BlocksRingBuffer to start with.
  BlocksRingBuffer rb(BlocksRingBuffer::ThreadSafety::WithMutex);

  // Block index to read at. Initially "null", but may be changed below.
  BlocksRingBuffer::BlockIndex bi;

  // Test all rb APIs when rb is out-of-session and therefore doesn't have an
  // underlying buffer.
  auto testOutOfSession = [&]() {
    MOZ_RELEASE_ASSERT(rb.BufferLength().isNothing());
    BlocksRingBuffer::State state = rb.GetState();
    // When out-of-session, range start and ends are the same, and there are no
    // pushed&cleared blocks.
    MOZ_RELEASE_ASSERT(state.mRangeStart == state.mRangeEnd);
    MOZ_RELEASE_ASSERT(state.mPushedBlockCount == 0);
    MOZ_RELEASE_ASSERT(state.mClearedBlockCount == 0);
    // `Put()` functions run the callback with `Nothing`.
    int32_t ran = 0;
    rb.Put(1, [&](BlocksRingBuffer::EntryWriter* aMaybeEntryWriter) {
      MOZ_RELEASE_ASSERT(!aMaybeEntryWriter);
      ++ran;
    });
    MOZ_RELEASE_ASSERT(ran == 1);
    // `PutFrom` won't do anything, and returns the null BlockIndex.
    MOZ_RELEASE_ASSERT(rb.PutFrom(&ran, sizeof(ran)) ==
                       BlocksRingBuffer::BlockIndex{});
    MOZ_RELEASE_ASSERT(rb.PutObject(ran) == BlocksRingBuffer::BlockIndex{});
    // `Read()` functions run the callback with `Nothing`.
    ran = 0;
    rb.Read([&](BlocksRingBuffer::Reader* aReader) {
      MOZ_RELEASE_ASSERT(!aReader);
      ++ran;
    });
    MOZ_RELEASE_ASSERT(ran == 1);
    ran = 0;
    rb.ReadAt(BlocksRingBuffer::BlockIndex{},
              [&](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeEntryReader) {
                MOZ_RELEASE_ASSERT(aMaybeEntryReader.isNothing());
                ++ran;
              });
    MOZ_RELEASE_ASSERT(ran == 1);
    ran = 0;
    rb.ReadAt(bi,
              [&](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeEntryReader) {
                MOZ_RELEASE_ASSERT(aMaybeEntryReader.isNothing());
                ++ran;
              });
    MOZ_RELEASE_ASSERT(ran == 1);
    // `ReadEach` shouldn't run the callback (nothing to read).
    rb.ReadEach([](auto&&) { MOZ_RELEASE_ASSERT(false); });
  };

  // As `testOutOfSession()` attempts to modify the buffer, we run it twice to
  // make sure one run doesn't influence the next one.
  testOutOfSession();
  testOutOfSession();

  rb.ClearBefore(bi);
  testOutOfSession();
  testOutOfSession();

  rb.Clear();
  testOutOfSession();
  testOutOfSession();

  rb.Reset();
  testOutOfSession();
  testOutOfSession();

  constexpr uint32_t MBSize = 32;

  rb.Set(MakePowerOfTwo<BlocksRingBuffer::Length, MBSize>());

  constexpr bool EMPTY = true;
  constexpr bool NOT_EMPTY = false;
  // Test all rb APIs when rb has an underlying buffer.
  auto testInSession = [&](bool aExpectEmpty) {
    MOZ_RELEASE_ASSERT(rb.BufferLength().isSome());
    BlocksRingBuffer::State state = rb.GetState();
    if (aExpectEmpty) {
      MOZ_RELEASE_ASSERT(state.mRangeStart == state.mRangeEnd);
      MOZ_RELEASE_ASSERT(state.mPushedBlockCount == 0);
      MOZ_RELEASE_ASSERT(state.mClearedBlockCount == 0);
    } else {
      MOZ_RELEASE_ASSERT(state.mRangeStart < state.mRangeEnd);
      MOZ_RELEASE_ASSERT(state.mPushedBlockCount > 0);
      MOZ_RELEASE_ASSERT(state.mClearedBlockCount <= state.mPushedBlockCount);
    }
    int32_t ran = 0;
    // The following three `Put...` will write three int32_t of value 1.
    bi = rb.Put(sizeof(ran),
                [&](BlocksRingBuffer::EntryWriter* aMaybeEntryWriter) {
                  MOZ_RELEASE_ASSERT(!!aMaybeEntryWriter);
                  ++ran;
                  aMaybeEntryWriter->WriteObject(ran);
                  return aMaybeEntryWriter->CurrentBlockIndex();
                });
    MOZ_RELEASE_ASSERT(ran == 1);
    MOZ_RELEASE_ASSERT(rb.PutFrom(&ran, sizeof(ran)) !=
                       BlocksRingBuffer::BlockIndex{});
    MOZ_RELEASE_ASSERT(rb.PutObject(ran) != BlocksRingBuffer::BlockIndex{});
    ran = 0;
    rb.Read([&](BlocksRingBuffer::Reader* aReader) {
      MOZ_RELEASE_ASSERT(!!aReader);
      ++ran;
    });
    MOZ_RELEASE_ASSERT(ran == 1);
    ran = 0;
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aEntryReader) {
      MOZ_RELEASE_ASSERT(aEntryReader.RemainingBytes() == sizeof(ran));
      MOZ_RELEASE_ASSERT(aEntryReader.ReadObject<decltype(ran)>() == 1);
      ++ran;
    });
    MOZ_RELEASE_ASSERT(ran >= 3);
    ran = 0;
    rb.ReadAt(BlocksRingBuffer::BlockIndex{},
              [&](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeEntryReader) {
                MOZ_RELEASE_ASSERT(aMaybeEntryReader.isNothing());
                ++ran;
              });
    MOZ_RELEASE_ASSERT(ran == 1);
    ran = 0;
    rb.ReadAt(bi,
              [&](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeEntryReader) {
                MOZ_RELEASE_ASSERT(aMaybeEntryReader.isNothing() == !bi);
                ++ran;
              });
    MOZ_RELEASE_ASSERT(ran == 1);
  };

  testInSession(EMPTY);
  testInSession(NOT_EMPTY);

  rb.Set(MakePowerOfTwo<BlocksRingBuffer::Length, 32>());
  MOZ_RELEASE_ASSERT(rb.BufferLength().isSome());
  rb.ReadEach([](auto&&) { MOZ_RELEASE_ASSERT(false); });

  testInSession(EMPTY);
  testInSession(NOT_EMPTY);

  rb.Reset();
  testOutOfSession();
  testOutOfSession();

  uint8_t buffer[MBSize * 3];
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }

  rb.Set(&buffer[MBSize], MakePowerOfTwo<BlocksRingBuffer::Length, MBSize>());
  MOZ_RELEASE_ASSERT(rb.BufferLength().isSome());
  rb.ReadEach([](auto&&) { MOZ_RELEASE_ASSERT(false); });

  testInSession(EMPTY);
  testInSession(NOT_EMPTY);

  rb.Reset();
  testOutOfSession();
  testOutOfSession();

  rb.Set(&buffer[MBSize], MakePowerOfTwo<BlocksRingBuffer::Length, MBSize>());
  MOZ_RELEASE_ASSERT(rb.BufferLength().isSome());
  rb.ReadEach([](auto&&) { MOZ_RELEASE_ASSERT(false); });

  testInSession(EMPTY);
  testInSession(NOT_EMPTY);

  // Remove the current underlying buffer, this should clear all entries.
  rb.Reset();

  // Check that only the provided stack-based sub-buffer was modified.
  uint32_t changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  testOutOfSession();
  testOutOfSession();

  printf("TestBlocksRingBufferUnderlyingBufferChanges done\n");
}

void TestBlocksRingBufferThreading() {
  printf("TestBlocksRingBufferThreading...\n");

  constexpr uint32_t MBSize = 8192;
  uint8_t buffer[MBSize * 3];
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }
  BlocksRingBuffer rb(BlocksRingBuffer::ThreadSafety::WithMutex,
                      &buffer[MBSize], MakePowerOfTwo32<MBSize>());

  // Start reader thread.
  std::atomic<bool> stopReader{false};
  std::thread reader([&]() {
    for (;;) {
      BlocksRingBuffer::State state = rb.GetState();
      printf(
          "Reader: range=%llu..%llu (%llu bytes) pushed=%llu cleared=%llu "
          "(alive=%llu)\n",
          static_cast<unsigned long long>(ExtractBlockIndex(state.mRangeStart)),
          static_cast<unsigned long long>(ExtractBlockIndex(state.mRangeEnd)),
          static_cast<unsigned long long>(ExtractBlockIndex(state.mRangeEnd)) -
              static_cast<unsigned long long>(
                  ExtractBlockIndex(state.mRangeStart)),
          static_cast<unsigned long long>(state.mPushedBlockCount),
          static_cast<unsigned long long>(state.mClearedBlockCount),
          static_cast<unsigned long long>(state.mPushedBlockCount -
                                          state.mClearedBlockCount));
      if (stopReader) {
        break;
      }
      ::SleepMilli(1);
    }
  });

  // Start writer threads.
  constexpr int ThreadCount = 32;
  std::thread threads[ThreadCount];
  for (int threadNo = 0; threadNo < ThreadCount; ++threadNo) {
    threads[threadNo] = std::thread(
        [&](int aThreadNo) {
          ::SleepMilli(1);
          constexpr int pushCount = 1024;
          for (int push = 0; push < pushCount; ++push) {
            // Reserve as many bytes as the thread number (but at least enough
            // to store an int), and write an increasing int.
            rb.Put(std::max(aThreadNo, int(sizeof(push))),
                   [&](BlocksRingBuffer::EntryWriter* aEW) {
                     MOZ_RELEASE_ASSERT(!!aEW);
                     aEW->WriteObject(aThreadNo * 1000000 + push);
                     *aEW += aEW->RemainingBytes();
                   });
          }
        },
        threadNo);
  }

  // Wait for all writer threads to die.
  for (auto&& thread : threads) {
    thread.join();
  }

  // Stop reader thread.
  stopReader = true;
  reader.join();

  // Check that only the provided stack-based sub-buffer was modified.
  uint32_t changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  printf("TestBlocksRingBufferThreading done\n");
}

void TestBlocksRingBufferSerialization() {
  printf("TestBlocksRingBufferSerialization...\n");

  constexpr uint32_t MBSize = 64;
  uint8_t buffer[MBSize * 3];
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }
  BlocksRingBuffer rb(BlocksRingBuffer::ThreadSafety::WithMutex,
                      &buffer[MBSize], MakePowerOfTwo32<MBSize>());

  // Will expect literal string to always have the same address.
#  define THE_ANSWER "The answer is "
  const char* theAnswer = THE_ANSWER;

  rb.PutObjects('0', WrapBlocksRingBufferLiteralCStringPointer(THE_ANSWER), 42,
                std::string(" but pi="), 3.14);
  rb.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    char c0;
    const char* answer;
    int integer;
    std::string str;
    double pi;
    aER.ReadIntoObjects(c0, answer, integer, str, pi);
    MOZ_RELEASE_ASSERT(c0 == '0');
    MOZ_RELEASE_ASSERT(answer == theAnswer);
    MOZ_RELEASE_ASSERT(integer == 42);
    MOZ_RELEASE_ASSERT(str == " but pi=");
    MOZ_RELEASE_ASSERT(pi == 3.14);
  });
  rb.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    char c0 = aER.ReadObject<char>();
    MOZ_RELEASE_ASSERT(c0 == '0');
    const char* answer = aER.ReadObject<const char*>();
    MOZ_RELEASE_ASSERT(answer == theAnswer);
    int integer = aER.ReadObject<int>();
    MOZ_RELEASE_ASSERT(integer == 42);
    std::string str = aER.ReadObject<std::string>();
    MOZ_RELEASE_ASSERT(str == " but pi=");
    double pi = aER.ReadObject<double>();
    MOZ_RELEASE_ASSERT(pi == 3.14);
  });

  rb.Clear();
  // Write an int and store its BlockIndex.
  BlocksRingBuffer::BlockIndex blockIndex = rb.PutObject(123);
  // It should be non-0.
  MOZ_RELEASE_ASSERT(blockIndex != BlocksRingBuffer::BlockIndex{});
  // Write that BlockIndex.
  rb.PutObject(blockIndex);
  rb.Read([&](BlocksRingBuffer::Reader* aR) {
    BlocksRingBuffer::BlockIterator it = aR->begin();
    const BlocksRingBuffer::BlockIterator itEnd = aR->end();
    MOZ_RELEASE_ASSERT(it != itEnd);
    MOZ_RELEASE_ASSERT((*it).ReadObject<int>() == 123);
    ++it;
    MOZ_RELEASE_ASSERT(it != itEnd);
    MOZ_RELEASE_ASSERT((*it).ReadObject<BlocksRingBuffer::BlockIndex>() ==
                       blockIndex);
    ++it;
    MOZ_RELEASE_ASSERT(it == itEnd);
  });

  rb.Clear();
  rb.PutObjects(std::make_tuple(
      '0', WrapBlocksRingBufferLiteralCStringPointer(THE_ANSWER), 42,
      std::string(" but pi="), 3.14));
  rb.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    MOZ_RELEASE_ASSERT(aER.ReadObject<char>() == '0');
    MOZ_RELEASE_ASSERT(aER.ReadObject<const char*>() == theAnswer);
    MOZ_RELEASE_ASSERT(aER.ReadObject<int>() == 42);
    MOZ_RELEASE_ASSERT(aER.ReadObject<std::string>() == " but pi=");
    MOZ_RELEASE_ASSERT(aER.ReadObject<double>() == 3.14);
  });

  rb.Clear();
  rb.PutObjects(MakeTuple('0',
                          WrapBlocksRingBufferLiteralCStringPointer(THE_ANSWER),
                          42, std::string(" but pi="), 3.14));
  rb.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    MOZ_RELEASE_ASSERT(aER.ReadObject<char>() == '0');
    MOZ_RELEASE_ASSERT(aER.ReadObject<const char*>() == theAnswer);
    MOZ_RELEASE_ASSERT(aER.ReadObject<int>() == 42);
    MOZ_RELEASE_ASSERT(aER.ReadObject<std::string>() == " but pi=");
    MOZ_RELEASE_ASSERT(aER.ReadObject<double>() == 3.14);
  });

  rb.Clear();
  {
    UniqueFreePtr<char> ufps(strdup(THE_ANSWER));
    rb.PutObjects(ufps);
  }
  rb.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    auto ufps = aER.ReadObject<UniqueFreePtr<char>>();
    MOZ_RELEASE_ASSERT(!!ufps);
    MOZ_RELEASE_ASSERT(std::string(THE_ANSWER) == ufps.get());
  });

  rb.Clear();
  int intArray[] = {1, 2, 3, 4, 5};
  rb.PutObjects(MakeSpan(intArray));
  rb.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    int intArrayOut[sizeof(intArray) / sizeof(intArray[0])] = {0};
    auto outSpan = MakeSpan(intArrayOut);
    aER.ReadIntoObject(outSpan);
    for (size_t i = 0; i < sizeof(intArray) / sizeof(intArray[0]); ++i) {
      MOZ_RELEASE_ASSERT(intArrayOut[i] == intArray[i]);
    }
  });

  rb.Clear();
  rb.PutObjects(Maybe<int>(Nothing{}), Maybe<int>(Some(123)));
  rb.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    Maybe<int> mi0, mi1;
    aER.ReadIntoObjects(mi0, mi1);
    MOZ_RELEASE_ASSERT(mi0.isNothing());
    MOZ_RELEASE_ASSERT(mi1.isSome());
    MOZ_RELEASE_ASSERT(*mi1 == 123);
  });

  rb.Clear();
  using V = Variant<int, double, int>;
  V v0(VariantIndex<0>{}, 123);
  V v1(3.14);
  V v2(VariantIndex<2>{}, 456);
  rb.PutObjects(v0, v1, v2);
  rb.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    MOZ_RELEASE_ASSERT(aER.ReadObject<V>() == v0);
    MOZ_RELEASE_ASSERT(aER.ReadObject<V>() == v1);
    MOZ_RELEASE_ASSERT(aER.ReadObject<V>() == v2);
  });

  // 2nd BlocksRingBuffer to contain the 1st one. It has be be more than twice
  // the size.
  constexpr uint32_t MBSize2 = MBSize * 4;
  uint8_t buffer2[MBSize2 * 3];
  for (size_t i = 0; i < MBSize2 * 3; ++i) {
    buffer2[i] = uint8_t('B' + i);
  }
  BlocksRingBuffer rb2(BlocksRingBuffer::ThreadSafety::WithoutMutex,
                       &buffer2[MBSize2], MakePowerOfTwo32<MBSize2>());
  rb2.PutObject(rb);

  // 3rd BlocksRingBuffer deserialized from the 2nd one.
  uint8_t buffer3[MBSize * 3];
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer3[i] = uint8_t('C' + i);
  }
  BlocksRingBuffer rb3(BlocksRingBuffer::ThreadSafety::WithoutMutex,
                       &buffer3[MBSize], MakePowerOfTwo32<MBSize>());
  rb2.ReadEach(
      [&](BlocksRingBuffer::EntryReader& aER) { aER.ReadIntoObject(rb3); });

  // And a 4th heap-allocated one.
  UniquePtr<BlocksRingBuffer> rb4up;
  rb2.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    rb4up = aER.ReadObject<UniquePtr<BlocksRingBuffer>>();
  });
  MOZ_RELEASE_ASSERT(!!rb4up);

  // Clear 1st and 2nd BlocksRingBuffers, to ensure we have made a deep copy
  // into the 3rd&4th ones.
  rb.Clear();
  rb2.Clear();

  // And now the 3rd one should have the same contents as the 1st one had.
  rb3.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    MOZ_RELEASE_ASSERT(aER.ReadObject<V>() == v0);
    MOZ_RELEASE_ASSERT(aER.ReadObject<V>() == v1);
    MOZ_RELEASE_ASSERT(aER.ReadObject<V>() == v2);
  });

  // And 4th.
  rb4up->ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    MOZ_RELEASE_ASSERT(aER.ReadObject<V>() == v0);
    MOZ_RELEASE_ASSERT(aER.ReadObject<V>() == v1);
    MOZ_RELEASE_ASSERT(aER.ReadObject<V>() == v2);
  });

  // In fact, the 3rd and 4th ones should have the same state, because they were
  // created the same way.
  MOZ_RELEASE_ASSERT(rb3.GetState().mRangeStart ==
                     rb4up->GetState().mRangeStart);
  MOZ_RELEASE_ASSERT(rb3.GetState().mRangeEnd == rb4up->GetState().mRangeEnd);
  MOZ_RELEASE_ASSERT(rb3.GetState().mPushedBlockCount ==
                     rb4up->GetState().mPushedBlockCount);
  MOZ_RELEASE_ASSERT(rb3.GetState().mClearedBlockCount ==
                     rb4up->GetState().mClearedBlockCount);

  // Check that only the provided stack-based sub-buffer was modified.
  uint32_t changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffers should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  for (size_t i = 0; i < MBSize2; ++i) {
    MOZ_RELEASE_ASSERT(buffer2[i] == uint8_t('B' + i));
  }
  for (size_t i = MBSize2 * 2; i < MBSize2 * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer2[i] == uint8_t('B' + i));
  }

  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer3[i] == uint8_t('C' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer3[i] == uint8_t('C' + i));
  }

  printf("TestBlocksRingBufferSerialization done\n");
}

class BaseTestMarkerPayload : public baseprofiler::ProfilerMarkerPayload {
 public:
  explicit BaseTestMarkerPayload(int aData) : mData(aData) {}

  int GetData() const { return mData; }

  // Exploded DECL_BASE_STREAM_PAYLOAD, but without `MFBT_API`s.
  static UniquePtr<ProfilerMarkerPayload> Deserialize(
      BlocksRingBuffer::EntryReader& aEntryReader);
  BlocksRingBuffer::Length TagAndSerializationBytes() const override;
  void SerializeTagAndPayload(
      BlocksRingBuffer::EntryWriter& aEntryWriter) const override;
  void StreamPayload(
      ::mozilla::baseprofiler::SpliceableJSONWriter& aWriter,
      const ::mozilla::TimeStamp& aProcessStartTime,
      ::mozilla::baseprofiler::UniqueStacks& aUniqueStacks) const override;

 private:
  BaseTestMarkerPayload(CommonProps&& aProps, int aData)
      : baseprofiler::ProfilerMarkerPayload(std::move(aProps)), mData(aData) {}

  int mData;
};

// static
UniquePtr<baseprofiler::ProfilerMarkerPayload>
BaseTestMarkerPayload::Deserialize(
    BlocksRingBuffer::EntryReader& aEntryReader) {
  CommonProps props = DeserializeCommonProps(aEntryReader);
  int data = aEntryReader.ReadObject<int>();
  return UniquePtr<baseprofiler::ProfilerMarkerPayload>(
      new BaseTestMarkerPayload(std::move(props), data));
}

BlocksRingBuffer::Length BaseTestMarkerPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes() + sizeof(int);
}

void BaseTestMarkerPayload::SerializeTagAndPayload(
    BlocksRingBuffer::EntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mData);
}

void BaseTestMarkerPayload::StreamPayload(
    baseprofiler::SpliceableJSONWriter& aWriter,
    const TimeStamp& aProcessStartTime,
    baseprofiler::UniqueStacks& aUniqueStacks) const {
  aWriter.IntProperty("data", mData);
}

void TestProfilerMarkerSerialization() {
  printf("TestProfilerMarkerSerialization...\n");

  constexpr uint32_t MBSize = 256;
  uint8_t buffer[MBSize * 3];
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }
  BlocksRingBuffer rb(BlocksRingBuffer::ThreadSafety::WithMutex,
                      &buffer[MBSize], MakePowerOfTwo32<MBSize>());

  constexpr int data = 42;
  {
    BaseTestMarkerPayload payload(data);
    rb.PutObject(
        static_cast<const baseprofiler::ProfilerMarkerPayload*>(&payload));
  }

  int read = 0;
  rb.ReadEach([&](BlocksRingBuffer::EntryReader& aER) {
    UniquePtr<baseprofiler::ProfilerMarkerPayload> payload =
        aER.ReadObject<UniquePtr<baseprofiler::ProfilerMarkerPayload>>();
    MOZ_RELEASE_ASSERT(!!payload);
    ++read;
    BaseTestMarkerPayload* testPayload =
        static_cast<BaseTestMarkerPayload*>(payload.get());
    MOZ_RELEASE_ASSERT(testPayload);
    MOZ_RELEASE_ASSERT(testPayload->GetData() == data);
  });
  MOZ_RELEASE_ASSERT(read == 1);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  printf("TestProfilerMarkerSerialization done\n");
}

// Increase the depth, to a maximum (to avoid too-deep recursion).
static constexpr size_t NextDepth(size_t aDepth) {
  constexpr size_t MAX_DEPTH = 128;
  return (aDepth < MAX_DEPTH) ? (aDepth + 1) : aDepth;
}

Atomic<bool, Relaxed, recordreplay::Behavior::DontPreserve> sStopFibonacci;

// Compute fibonacci the hard way (recursively: `f(n)=f(n-1)+f(n-2)`), and
// prevent inlining.
// The template parameter makes each depth be a separate function, to better
// distinguish them in the profiler output.
template <size_t DEPTH = 0>
MOZ_NEVER_INLINE unsigned long long Fibonacci(unsigned long long n) {
  AUTO_BASE_PROFILER_LABEL_DYNAMIC_STRING("fib", OTHER, std::to_string(DEPTH));
  if (n == 0) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }
  if (DEPTH < 5 && sStopFibonacci) {
    return 1'000'000'000;
  }
  TimeStamp start = TimeStamp::NowUnfuzzed();
  static constexpr size_t MAX_MARKER_DEPTH = 10;
  unsigned long long f2 = Fibonacci<NextDepth(DEPTH)>(n - 2);
  if (DEPTH == 0) {
    BASE_PROFILER_ADD_MARKER("Half-way through Fibonacci", OTHER);
  }
  unsigned long long f1 = Fibonacci<NextDepth(DEPTH)>(n - 1);
  if (DEPTH < MAX_MARKER_DEPTH) {
    baseprofiler::profiler_add_text_marker(
        "fib", std::to_string(DEPTH),
        baseprofiler::ProfilingCategoryPair::OTHER, start,
        TimeStamp::NowUnfuzzed());
  }
  return f2 + f1;
}

void TestProfiler() {
  printf("TestProfiler starting -- pid: %d, tid: %d\n",
         baseprofiler::profiler_current_process_id(),
         baseprofiler::profiler_current_thread_id());
  // ::SleepMilli(10000);

  // Test dependencies.
  TestPowerOfTwoMask();
  TestPowerOfTwo();
  TestLEB128();
  TestModuloBuffer();
  TestBlocksRingBufferAPI();
  TestBlocksRingBufferUnderlyingBufferChanges();
  TestBlocksRingBufferThreading();
  TestBlocksRingBufferSerialization();
  TestProfilerMarkerSerialization();

  {
    printf("profiler_init()...\n");
    AUTO_BASE_PROFILER_INIT;

    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_is_active());
    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_thread_is_being_profiled());
    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_thread_is_sleeping());

    printf("profiler_start()...\n");
    Vector<const char*> filters;
    // Profile all registered threads.
    MOZ_RELEASE_ASSERT(filters.append(""));
    const uint32_t features = baseprofiler::ProfilerFeature::Leaf |
                              baseprofiler::ProfilerFeature::StackWalk |
                              baseprofiler::ProfilerFeature::Threads;
    baseprofiler::profiler_start(baseprofiler::BASE_PROFILER_DEFAULT_ENTRIES,
                                 BASE_PROFILER_DEFAULT_INTERVAL, features,
                                 filters.begin(), filters.length());

    MOZ_RELEASE_ASSERT(baseprofiler::profiler_is_active());
    MOZ_RELEASE_ASSERT(baseprofiler::profiler_thread_is_being_profiled());
    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_thread_is_sleeping());

    sStopFibonacci = false;

    std::thread threadFib([]() {
      AUTO_BASE_PROFILER_REGISTER_THREAD("fibonacci");
      SleepMilli(5);
      auto cause =
#  if defined(__linux__) || defined(__ANDROID__)
          // Currently disabled on these platforms, so just return a null.
          decltype(baseprofiler::profiler_get_backtrace()){};
#  else
          baseprofiler::profiler_get_backtrace();
#  endif
      AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE("fibonacci", "First leaf call",
                                           OTHER, std::move(cause));
      static const unsigned long long fibStart = 37;
      printf("Fibonacci(%llu)...\n", fibStart);
      AUTO_BASE_PROFILER_LABEL("Label around Fibonacci", OTHER);
      unsigned long long f = Fibonacci(fibStart);
      printf("Fibonacci(%llu) = %llu\n", fibStart, f);
    });

    std::thread threadCancelFib([]() {
      AUTO_BASE_PROFILER_REGISTER_THREAD("fibonacci canceller");
      SleepMilli(5);
      AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE("fibonacci", "Canceller", OTHER,
                                           nullptr);
      static const int waitMaxSeconds = 10;
      for (int i = 0; i < waitMaxSeconds; ++i) {
        if (sStopFibonacci) {
          AUTO_BASE_PROFILER_LABEL_DYNAMIC_STRING("fibCancel", OTHER,
                                                  std::to_string(i));
          return;
        }
        AUTO_BASE_PROFILER_THREAD_SLEEP;
        SleepMilli(1000);
      }
      AUTO_BASE_PROFILER_LABEL_DYNAMIC_STRING("fibCancel", OTHER,
                                              "Cancelling!");
      sStopFibonacci = true;
    });

    {
      AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE(
          "main thread", "joining fibonacci thread", OTHER, nullptr);
      AUTO_BASE_PROFILER_THREAD_SLEEP;
      threadFib.join();
    }

    {
      AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE(
          "main thread", "joining fibonacci-canceller thread", OTHER, nullptr);
      sStopFibonacci = true;
      AUTO_BASE_PROFILER_THREAD_SLEEP;
      threadCancelFib.join();
    }

    // Just making sure all payloads know how to (de)serialize and stream.
    baseprofiler::profiler_add_marker(
        "TracingMarkerPayload", baseprofiler::ProfilingCategoryPair::OTHER,
        baseprofiler::TracingMarkerPayload("category",
                                           baseprofiler::TRACING_EVENT));

    auto cause =
#  if defined(__linux__) || defined(__ANDROID__)
        // Currently disabled on these platforms, so just return a null.
        decltype(baseprofiler::profiler_get_backtrace()){};
#  else
        baseprofiler::profiler_get_backtrace();
#  endif
    baseprofiler::profiler_add_marker(
        "FileIOMarkerPayload", baseprofiler::ProfilingCategoryPair::OTHER,
        baseprofiler::FileIOMarkerPayload(
            "operation", "source", "filename", TimeStamp::NowUnfuzzed(),
            TimeStamp::NowUnfuzzed(), std::move(cause)));

    baseprofiler::profiler_add_marker(
        "UserTimingMarkerPayload", baseprofiler::ProfilingCategoryPair::OTHER,
        baseprofiler::UserTimingMarkerPayload("name", TimeStamp::NowUnfuzzed(),
                                              Nothing{}));

    baseprofiler::profiler_add_marker(
        "HangMarkerPayload", baseprofiler::ProfilingCategoryPair::OTHER,
        baseprofiler::HangMarkerPayload(TimeStamp::NowUnfuzzed(),
                                        TimeStamp::NowUnfuzzed()));

    baseprofiler::profiler_add_marker(
        "LongTaskMarkerPayload", baseprofiler::ProfilingCategoryPair::OTHER,
        baseprofiler::LongTaskMarkerPayload(TimeStamp::NowUnfuzzed(),
                                            TimeStamp::NowUnfuzzed()));

    {
      std::string s = "text payload";
      baseprofiler::profiler_add_marker(
          "TextMarkerPayload", baseprofiler::ProfilingCategoryPair::OTHER,
          baseprofiler::TextMarkerPayload(s, TimeStamp::NowUnfuzzed(),
                                          TimeStamp::NowUnfuzzed()));
    }

    baseprofiler::profiler_add_marker(
        "LogMarkerPayload", baseprofiler::ProfilingCategoryPair::OTHER,
        baseprofiler::LogMarkerPayload("module", "text",
                                       TimeStamp::NowUnfuzzed()));

    printf("Sleep 1s...\n");
    {
      AUTO_BASE_PROFILER_THREAD_SLEEP;
      SleepMilli(1000);
    }

    Maybe<baseprofiler::ProfilerBufferInfo> info =
        baseprofiler::profiler_get_buffer_info();
    MOZ_RELEASE_ASSERT(info.isSome());
    printf("Profiler buffer range: %llu .. %llu (%llu bytes)\n",
           static_cast<unsigned long long>(info->mRangeStart),
           static_cast<unsigned long long>(info->mRangeEnd),
           // sizeof(ProfileBufferEntry) == 9
           (static_cast<unsigned long long>(info->mRangeEnd) -
            static_cast<unsigned long long>(info->mRangeStart)) *
               9);
    printf("Stats:         min(ns) .. mean(ns) .. max(ns)  [count]\n");
    printf("- Intervals:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mIntervalsNs.min,
           info->mIntervalsNs.sum / info->mIntervalsNs.n,
           info->mIntervalsNs.max, info->mIntervalsNs.n);
    printf("- Overheads:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mOverheadsNs.min,
           info->mOverheadsNs.sum / info->mOverheadsNs.n,
           info->mOverheadsNs.max, info->mOverheadsNs.n);
    printf("  - Locking:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mLockingsNs.min, info->mLockingsNs.sum / info->mLockingsNs.n,
           info->mLockingsNs.max, info->mLockingsNs.n);
    printf("  - Clearning: %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mCleaningsNs.min,
           info->mCleaningsNs.sum / info->mCleaningsNs.n,
           info->mCleaningsNs.max, info->mCleaningsNs.n);
    printf("  - Counters:  %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mCountersNs.min, info->mCountersNs.sum / info->mCountersNs.n,
           info->mCountersNs.max, info->mCountersNs.n);
    printf("  - Threads:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mThreadsNs.min, info->mThreadsNs.sum / info->mThreadsNs.n,
           info->mThreadsNs.max, info->mThreadsNs.n);

    printf("baseprofiler_save_profile_to_file()...\n");
    baseprofiler::profiler_save_profile_to_file("TestProfiler_profile.json");

    printf("profiler_stop()...\n");
    baseprofiler::profiler_stop();

    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_is_active());
    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_thread_is_being_profiled());
    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_thread_is_sleeping());

    printf("profiler_shutdown()...\n");
  }

  printf("TestProfiler done\n");
}

#else  // MOZ_BASE_PROFILER

// Testing that macros are still #defined (but do nothing) when
// MOZ_BASE_PROFILER is disabled.
void TestProfiler() {
  // These don't need to make sense, we just want to know that they're defined
  // and don't do anything.
  AUTO_BASE_PROFILER_INIT;

  // This wouldn't build if the macro did output its arguments.
  AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE(catch, catch, catch, catch);

  AUTO_BASE_PROFILER_LABEL(catch, catch);

  AUTO_BASE_PROFILER_THREAD_SLEEP;
}

#endif  // MOZ_BASE_PROFILER else

int main() {
  // Note that there are two `TestProfiler` functions above, depending on
  // whether MOZ_BASE_PROFILER is #defined.
  TestProfiler();

  return 0;
}
