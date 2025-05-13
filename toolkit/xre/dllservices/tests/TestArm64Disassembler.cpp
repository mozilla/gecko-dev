/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>

#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/interceptor/Arm64.h"

#define TEST_FAILED(format, ...)                                            \
  do {                                                                      \
    wprintf(L"TEST-FAILED | TestArm64Disassembler | " format __VA_OPT__(, ) \
                __VA_ARGS__);                                               \
    return false;                                                           \
  } while (0)

#define TEST_PASS(format, ...)                                            \
  do {                                                                    \
    wprintf(L"TEST-PASS | TestArm64Disassembler | " format __VA_OPT__(, ) \
                __VA_ARGS__);                                             \
  } while (0)

using namespace mozilla;
using namespace mozilla::interceptor::arm64;

bool TestCheckForPCRelAdrp() {
  // A real-world example from bug 1964688 comment 5:
  // 00007ff9`59a7ea80 d0dfff11 adrp xip1,00007ff9`19a60000
  Result<LoadOrBranch, PCRelCheckError> result =
      CheckForPCRel(0x7ff959a7ea80, 0xd0dfff11);
  if (result.isErr()) {
    auto error = result.unwrapErr();
    TEST_FAILED(
        "Failed to recognize adrp as a PC-relative instruction with a "
        "decoder, got PCRelCheckError %d.\n",
        error);
  }

  auto loadOrBranch = result.unwrap();
  if (loadOrBranch.mType != LoadOrBranch::Type::Load) {
    TEST_FAILED("Computed an incorrect LoadOrBranch::Type for adrp, got %d.\n",
                loadOrBranch.mType);
  }
  // xip1 is a synonym for x17
  if (loadOrBranch.mDestReg != 17) {
    TEST_FAILED(
        "Computed an incorrect destination register for adrp, got %d.\n",
        loadOrBranch.mDestReg);
  }
  if (loadOrBranch.mAbsAddress != 0x7ff919a60000) {
    TEST_FAILED("Computed a wrong absolute address for adrp, got address %p.\n",
                loadOrBranch.mAbsAddress);
  }

  TEST_PASS(
      "Properly recognized adrp as a PC-relative load instruction with a "
      "working decoder.\n");
  return true;
}

bool TestCheckForPCRelAdr() {
  // Fictional example with adr:
  // 00007ff959a7ea80 50dfff11 adr x17, #0x7ff959a3ea62
  Result<LoadOrBranch, PCRelCheckError> result =
      CheckForPCRel(0x7ff959a7ea80, 0x50dfff11);

  // For the moment we expect to recognize adr instructions but we don't have
  // a decoder
  if (!result.isErr()) {
    TEST_FAILED(
        "Unexpectedly recognized adr as a PC-relative instruction with a "
        "decoder. If you have implemented a decoder for this instruction, "
        "please update TestArm64Disassembler.cpp.\n");
  }

  auto error = result.unwrapErr();
  if (error != PCRelCheckError::NoDecoderAvailable) {
    TEST_FAILED(
        "Failed to recognize adr as a PC-relative instruction, got "
        "PCRelCheckError %d.\n",
        error);
  }

  TEST_PASS(
      "Properly recognized adr as a PC-relative instruction without a "
      "decoder.\n");
  return true;
}

int wmain(int argc, wchar_t* argv[]) {
  if (!TestCheckForPCRelAdrp() || !TestCheckForPCRelAdr()) {
    return -1;
  }
  return 0;
}
