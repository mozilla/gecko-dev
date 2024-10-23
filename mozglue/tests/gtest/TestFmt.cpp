/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "fmt/format.h"
#include "fmt/xchar.h"
#include "mozilla/ArrayUtils.h"
#include "nsTArray.h"
#include "nsFmtString.h"
#include "nsString.h"
#include "mozilla/Sprintf.h"

#define snprintf(...)

namespace tfformat {
#include "../glibc_printf_tests/tfformat.c"
}

#undef snprintf

nsCString PrintfToFmtFormat(const char* aPrintfFormatString) {
  nsCString fmtFormat;
  fmtFormat.AppendASCII("{");
  fmtFormat.AppendASCII(aPrintfFormatString);
  // {fmt} uses < to left align, while printf traditionally use -.
  // The order between the sign forcing ("+") or zero-padding ("0") and the
  // alignment ("-" or "<") is also reversed.
  fmtFormat.ReplaceSubstring("+-", "<+");
  fmtFormat.ReplaceSubstring("0-", "<0");
  // {fmt} doesn't support e.g. %4.f to denote 4 digits of integer part, and
  // zero digit of fractional part. Simply replace them by the explicit form
  // with a 0.
  fmtFormat.ReplaceSubstring(".f", ".0f");
  fmtFormat.ReplaceSubstring(".F", ".0F");
  fmtFormat.ReplaceSubstring(".e", ".0e");
  fmtFormat.ReplaceSubstring(".E", ".0E");
  fmtFormat.ReplaceSubstring(".G", ".0G");
  fmtFormat.ReplaceSubstring(".g", ".0g");
  fmtFormat.ReplaceChar('%', ':');
  fmtFormat.AppendASCII("}");
  return fmtFormat;
}

TEST(Fmt, CrossCheckPrintf)
{
  char bufGeckoPrintf[1024] = {};
  // - 1 because the last item is just a zero
  for (uint32_t i = 2; i < std::size(tfformat::sprint_doubles) - 1; i++) {
    if (strstr(tfformat::sprint_doubles[i].format_string, "#") ||
        strstr(tfformat::sprint_doubles[i].format_string, "a")) {
      // Gecko's printf doesn't implement the '#' specifier nor 'a' conversion
      // specifier (hex notation), but {fmt} does.
      // Skip this test-case for the cross-check.
      continue;
    }
    nsCString fmtFormat =
        PrintfToFmtFormat(tfformat::sprint_doubles[i].format_string);
    nsCString withFmt;
    withFmt.AppendFmt(fmtFormat.get(), tfformat::sprint_doubles[i].value);
    SprintfBuf(bufGeckoPrintf, 1024, tfformat::sprint_doubles[i].format_string,
               tfformat::sprint_doubles[i].value);
    if (strcmp(bufGeckoPrintf, withFmt.get()) != 0) {
      fprintf(stderr, "Conversion index %d: %lf, %s -> %s\n", i,
              tfformat::sprint_doubles[i].value,
              tfformat::sprint_doubles[i].format_string, fmtFormat.get());
      fprintf(stderr, "DIFF: |%s|\n vs.   |%s|\n", bufGeckoPrintf,
              withFmt.get());
    }
    ASSERT_STREQ(bufGeckoPrintf, withFmt.get());
  }
}

TEST(Fmt, Sequences)
{
  char bufFmt[1024] = {};
  {
    nsTArray<int> array(4);
    for (uint32_t i = 0; i < 4; i++) {
      array.AppendElement(i);
    }
    auto [out, size] =
        fmt::format_to(bufFmt, FMT_STRING("{}"), fmt::join(array, ", "));
    *out = 0;
    ASSERT_STREQ("0, 1, 2, 3", bufFmt);
  }
  {
    nsTArray<uint8_t> array(4);
    for (uint32_t i = 0; i < 4; i++) {
      array.AppendElement((123 * 5 * (i + 1)) % 255);
    }
    auto [out, size] =
        fmt::format_to(bufFmt, FMT_STRING("{:#04x}"), fmt::join(array, ", "));
    *out = 0;
    ASSERT_STREQ("0x69, 0xd2, 0x3c, 0xa5", bufFmt);
  }
}

struct POD {
  double mA;
  uint64_t mB;
};

struct POD2 {
  double mA;
  uint64_t mB;
};

template <>
class fmt::formatter<POD> : public formatter<string_view> {
 public:
  auto format(const POD& aPod, format_context& aCtx) const {
    std::string temp;
    format_to(std::back_inserter(temp), FMT_STRING("POD: mA: {}, mB: {}"),
              aPod.mA, aPod.mB);
    return formatter<string_view>::format(temp, aCtx);
  }
};

auto format_as(POD2 aInstance) -> std::string {
  return fmt::format(FMT_STRING("POD2: mA: {}, mB: {}"), aInstance.mA,
                     aInstance.mB);
}

TEST(Fmt, PodPrint)
{
  char bufFmt[1024] = {};

  POD p{4.33, 8};
  POD2 p2{4.33, 8};
  {
    auto [out, size] = fmt::format_to(bufFmt, FMT_STRING("{}"), p);
    *out = 0;
    ASSERT_STREQ("POD: mA: 4.33, mB: 8", bufFmt);
  }

  {
    auto [out, size] = fmt::format_to(bufFmt, FMT_STRING("{:>30}"), p);
    *out = 0;
    ASSERT_STREQ("          POD: mA: 4.33, mB: 8", bufFmt);
  }

  {
    auto [out, size] = fmt::format_to(bufFmt, FMT_STRING("{:>30}"), p2);
    *out = 0;
    ASSERT_STREQ("         POD2: mA: 4.33, mB: 8", bufFmt);
  }
}

TEST(Fmt, nsString)
{
  {
    nsFmtCString str(FMT_STRING("{} {} {}"), 4, 4.3, " end");
    ASSERT_STREQ("4 4.3  end", str.get());
  }
  {
    nsFmtString str(FMT_STRING(u"Étonnant {} {} {}"), u"Étienne", 4, 4.3);
    ASSERT_STREQ("Étonnant Étienne 4 4.3", NS_ConvertUTF16toUTF8(str).get());
  }
  {
    nsString str;
    str.AppendFmt(FMT_STRING(u"Étonnant {} {} {}"), u"Étienne", 4, 4.3);
    ASSERT_STREQ("Étonnant Étienne 4 4.3", NS_ConvertUTF16toUTF8(str).get());
  }
  {
    nsCString str;
    str.AppendFmt(FMT_STRING("{} {} {}"), 4, 4.3, " end");
    ASSERT_STREQ("4 4.3  end", str.get());
  }
}

TEST(Fmt, Truncation)
{
  char too_short_buf[16];
  const char* too_long_buf = "asdasdlkasjdashdkajhsdkhaksdjhasd";
  {
    auto [out, truncated] =
        fmt::format_to(too_short_buf, FMT_STRING("{}"), too_long_buf);
    assert(truncated);
    // Overwrite the last char for printing
    too_short_buf[15] = 0;
    fmt::print(FMT_STRING("{} {} {}\n"), too_short_buf, fmt::ptr(out),
               truncated);
  }
  {
    auto [out, size] = fmt::format_to_n(too_short_buf, sizeof(too_short_buf),
                                        FMT_STRING("{}"), too_long_buf);
    assert(size > sizeof(too_short_buf));
    too_short_buf[15] = 0;
    fmt::print(FMT_STRING("{} {} {}\n"), too_short_buf, fmt::ptr(out), size);
  }
}

TEST(Fmt, NullString)
{
  char str[16];
  auto [out, size] = fmt::format_to(str, FMT_STRING("{}"), (char*)nullptr);
  *out = 0;
  ASSERT_STREQ("(null)", str);
}

// ASAN intercepts the underlying fwrite and crashes.
#if defined(__has_feature)
#  if not(__has_feature(address_sanitizer))
TEST(Fmt, IOError)
{
  FILE* duped = fdopen(dup(fileno(stderr)), "w");
  fclose(duped);
  // glibc will crash here on x86 Linux debug
#    if defined(DEBUG) && defined(XP_LINUX) && !defined(__i386__)
  fmt::println(duped, FMT_STRING("Hi {}"), 14);
#    endif
}
#  endif
#endif
