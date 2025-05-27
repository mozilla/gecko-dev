/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "intgemm/IntegerGemmIntrinsic.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/TimeStamp.h"

#include <gemmology_fwd.h>
#include "fmt/format.h"

#include "js/ErrorReport.h"
#include "js/HeapAPI.h"
#include "vm/ArrayBufferObject.h"
#include "vm/GeckoProfiler.h"
#include "vm/JSContext.h"
#include "wasm/WasmBuiltins.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmLog.h"

#if defined(USE_AVX512BW)
#  if defined(USE_AVX512VNNI)
#    if defined(USE_AVXVNNI)
#      define SUPPORTED_ARCHS                                                 \
        xsimd::arch_list<xsimd::avx512vnni<xsimd::avx512bw>, xsimd::avx512bw, \
                         xsimd::avxvnni, xsimd::avx2, xsimd::ssse3,           \
                         xsimd::sse2>
#    else
#      define SUPPORTED_ARCHS                                                 \
        xsimd::arch_list<xsimd::avx512vnni<xsimd::avx512bw>, xsimd::avx512bw, \
                         xsimd::avx2, xsimd::ssse3, xsimd::sse2>
#    endif
#  elif defined(USE_AVXVNNI)
#    define SUPPORTED_ARCHS                                          \
      xsimd::arch_list<xsimd::avx512bw, xsimd::avxvnni, xsimd::avx2, \
                       xsimd::ssse3, xsimd::sse2>
#  else
#    define SUPPORTED_ARCHS \
      xsimd::arch_list<xsimd::avx512bw, xsimd::avx2, xsimd::ssse3, xsimd::sse2>
#  endif
#elif defined(USE_AVXVNNI)
#  define SUPPORTED_ARCHS \
    xsimd::arch_list<xsimd::avxvnni, xsimd::avx2, xsimd::ssse3, xsimd::sse2>
#elif defined(USE_AVX2)
#  define SUPPORTED_ARCHS \
    xsimd::arch_list<xsimd::avx2, xsimd::ssse3, xsimd::sse2>
#elif defined(USE_SSSE3)
#  define SUPPORTED_ARCHS xsimd::arch_list<xsimd::ssse3, xsimd::sse2>
#elif defined(USE_SSE2)
#  define SUPPORTED_ARCHS xsimd::arch_list<xsimd::sse2>
#elif defined(USE_NEON) and defined(XSIMD_WITH_NEON64)
#  if defined(USE_NEON_I8MM)
#    define SUPPORTED_ARCHS \
      xsimd::arch_list<xsimd::i8mm<xsimd::neon64>, xsimd::neon64>
#  else
#    define SUPPORTED_ARCHS xsimd::arch_list<xsimd::neon64>
#  endif
#else
#  error no supported architecture
#endif

// Dispatch *at runtime* based on run-time hardware and compile-time
// architectures.
//
// FIXME: Ideally we would not run the dispatch code at each function call.
#define GEMMOLOGY_DISPATCH(FUNC)                                 \
  xsimd::dispatch<SUPPORTED_ARCHS>([](auto arch, auto... args) { \
    return gemmology::Engine<decltype(arch)>::FUNC(args...);     \
  })

template <size_t TextLength = 512, typename CharT = char>
struct AutoProfilerMarker {
  AutoProfilerMarker(js::GeckoProfilerRuntime& profiler, const CharT* name)
      : profiler(profiler), name(name) {
    if (profiler.enabled()) {
      startTime = mozilla::TimeStamp::Now();
    }
  }

  template <typename... Args>
  AutoProfilerMarker(js::GeckoProfilerRuntime& profiler, const CharT* name,
                     fmt::format_string<Args...> aFormatStr, Args&&... aArgs)
      : profiler(profiler), name(name) {
    if (profiler.enabled()) {
      startTime = mozilla::TimeStamp::Now();
      auto [out, size] = fmt::vformat_to_n(
          text, sizeof(text) - 1, aFormatStr,
          fmt::make_format_args<fmt::buffered_context<CharT>>(aArgs...));

      MOZ_ASSERT(size > sizeof(text) - 1,
                 "Truncated marker, consider increasing the buffer");

      *out = 0;
    }
  }
  ~AutoProfilerMarker() {
    if (profiler.enabled()) {
      profiler.markInterval(name, startTime, text,
                            JS::ProfilingCategoryPair::JS);
    }
  }
  js::GeckoProfilerRuntime& profiler;
  const char* name;
  char text[TextLength]{};
  mozilla::TimeStamp startTime;
};

static constexpr uint32_t ARRAY_ALIGNMENT = 64;
static constexpr uint32_t ROWS_A_MULTIPLIER = 1;
static constexpr uint32_t COLUMNS_A_MULTIPLIER = 64;
static constexpr uint32_t ROWS_B_MULTIPLIER = COLUMNS_A_MULTIPLIER;
static constexpr uint32_t COLUMNS_B_MULTIPLIER = 8;
static constexpr uint32_t SELECTED_COLUMNS_B_MULTIPLIER = 8;

size_t GetWasmRawBufferLength(const uint8_t* memBase) {
  const js::WasmArrayRawBuffer* rawBuf =
      js::WasmArrayRawBuffer::fromDataPtr(memBase);
  return rawBuf->byteLength();
}

bool CheckMatrixDimension(uint32_t size, uint32_t sizeMultiplier) {
  // A valid size is a positive integral multiple of Multiplier
  return !((size == 0) || (size % sizeMultiplier != 0));
}

bool CheckMatrixBound(uint32_t input, uint64_t inputSize,
                      size_t wasmBufferSize) {
  mozilla::CheckedUint64 inputUpperLimit(inputSize);
  inputUpperLimit += input;

  // Bound check fails if size overflows or it spans outside the wasm memory
  return !(!inputUpperLimit.isValid() ||
           (inputUpperLimit.value() >= (uint64_t)wasmBufferSize));
}

bool CheckMatrixBoundAndAlignment(uint32_t input, uint64_t inputSize,
                                  size_t wasmBufferSize) {
  // Alignment check: It is sufficient to check alignment for the offset rather
  // than for the actual pointer within wasm memory (as long as following assert
  // is satisfied)
  static_assert(js::gc::PageSize >= ARRAY_ALIGNMENT,
                "PageSize should be bigger than Alignment");
  if (input % ARRAY_ALIGNMENT != 0) {
    return false;
  }

  // Check Bound
  return CheckMatrixBound(input, inputSize, wasmBufferSize);
}

int32_t js::intgemm::IntrI8PrepareB(wasm::Instance* instance,
                                    uint32_t inputMatrixB, float scale,
                                    float zeroPoint, uint32_t rowsB,
                                    uint32_t colsB, uint32_t outputMatrixB,
                                    uint8_t* memBase) {
  MOZ_ASSERT(wasm::SASigIntrI8PrepareB.failureMode ==
             wasm::FailureMode::FailOnNegI32);
  JSContext* cx = instance->cx();
  AutoUnsafeCallWithABI unsafe;

  // Size checks for matricies
  if (!CheckMatrixDimension(rowsB, ROWS_B_MULTIPLIER) ||
      !CheckMatrixDimension(colsB, COLUMNS_B_MULTIPLIER)) {
    return -1;
  }

  // Memory Bound and Alignment checks for matricies
  uint64_t sizeB = (uint64_t)rowsB * (uint64_t)colsB;
  size_t wasmBufferSize = GetWasmRawBufferLength(memBase);
  if (!CheckMatrixBoundAndAlignment(inputMatrixB, sizeB, wasmBufferSize) ||
      !CheckMatrixBoundAndAlignment(outputMatrixB, sizeB, wasmBufferSize)) {
    return -1;
  }

  // Actual call to the 3rd party library (intgemm) for PrepareB
  const float* inputMatrixBPtr =
      reinterpret_cast<const float*>(&memBase[inputMatrixB]);
  int8_t* outputMatrixBPtr = reinterpret_cast<int8_t*>(&memBase[outputMatrixB]);
  AutoProfilerMarker marker(cx->runtime()->geckoProfiler(), "integemm::PreparB",
                            FMT_STRING("rowsB: {} colsB: {} sizeB: {}"), rowsB,
                            colsB, sizeB);
  GEMMOLOGY_DISPATCH(PrepareB)
  (inputMatrixBPtr, outputMatrixBPtr,
   scale,  // Quant Mult
   rowsB, colsB);
  return 0;
}

int32_t js::intgemm::IntrI8PrepareBFromTransposed(
    wasm::Instance* instance, uint32_t inputMatrixBTransposed, float scale,
    float zeroPoint, uint32_t rowsB, uint32_t colsB, uint32_t outputMatrixB,
    uint8_t* memBase) {
  MOZ_ASSERT(wasm::SASigIntrI8PrepareBFromTransposed.failureMode ==
             wasm::FailureMode::FailOnNegI32);
  JSContext* cx = instance->cx();
  AutoUnsafeCallWithABI unsafe;

  // Size checks for matricies
  if (!CheckMatrixDimension(rowsB, ROWS_B_MULTIPLIER) ||
      !CheckMatrixDimension(colsB, COLUMNS_B_MULTIPLIER)) {
    return -1;
  }

  // Memory Bound checks for all matricies
  uint64_t sizeB = (uint64_t)rowsB * (uint64_t)colsB;
  size_t wasmBufferSize = GetWasmRawBufferLength(memBase);
  if (!CheckMatrixBoundAndAlignment(inputMatrixBTransposed, sizeB,
                                    wasmBufferSize) ||
      !CheckMatrixBoundAndAlignment(outputMatrixB, sizeB, wasmBufferSize)) {
    return -1;
  }

  // Actual call to the 3rd party library (intgemm) for PrepareBTransposed
  const float* inputMatrixBTransposedPtr =
      reinterpret_cast<const float*>(&memBase[inputMatrixBTransposed]);
  int8_t* outputMatrixBPtr = reinterpret_cast<int8_t*>(&memBase[outputMatrixB]);
  AutoProfilerMarker marker(
      cx->runtime()->geckoProfiler(), "intgemm::PreparBTransposed",
      FMT_STRING("rowsB: {} colsB: {} sizeB: {}"), rowsB, colsB, sizeB);
  GEMMOLOGY_DISPATCH(PrepareBTransposed)
  (inputMatrixBTransposedPtr, outputMatrixBPtr,
   scale,  // Quant Mult
   rowsB, colsB);
  return 0;
}

int32_t js::intgemm::IntrI8PrepareBFromQuantizedTransposed(
    wasm::Instance* instance, uint32_t inputMatrixBQuantizedTransposed,
    uint32_t rowsB, uint32_t colsB, uint32_t outputMatrixB, uint8_t* memBase) {
  MOZ_ASSERT(wasm::SASigIntrI8PrepareBFromQuantizedTransposed.failureMode ==
             wasm::FailureMode::FailOnNegI32);
  JSContext* cx = instance->cx();
  AutoUnsafeCallWithABI unsafe;

  // Size checks for matricies
  if (!CheckMatrixDimension(rowsB, ROWS_B_MULTIPLIER) ||
      !CheckMatrixDimension(colsB, COLUMNS_B_MULTIPLIER)) {
    return -1;
  }

  // Memory Bound checks for all matricies
  uint64_t sizeB = (uint64_t)rowsB * (uint64_t)colsB;
  size_t wasmBufferSize = GetWasmRawBufferLength(memBase);
  if (!CheckMatrixBoundAndAlignment(inputMatrixBQuantizedTransposed, sizeB,
                                    wasmBufferSize) ||
      !CheckMatrixBoundAndAlignment(outputMatrixB, sizeB, wasmBufferSize)) {
    return -1;
  }

  // Actual call to the 3rd party library (intgemm)
  const int8_t* inputMatrixBQuantizedTransposedPtr =
      reinterpret_cast<const int8_t*>(
          &memBase[inputMatrixBQuantizedTransposed]);
  int8_t* outputMatrixBPtr = reinterpret_cast<int8_t*>(&memBase[outputMatrixB]);
  AutoProfilerMarker marker(cx->runtime()->geckoProfiler(),
                            "intgemm::PrepareBQuantizedTransposed",
                            FMT_STRING("rowsB: {}, colsB: {}"), rowsB, colsB);
  GEMMOLOGY_DISPATCH(PrepareBQuantizedTransposed)
  (inputMatrixBQuantizedTransposedPtr, outputMatrixBPtr, rowsB, colsB);
  return 0;
}

int32_t js::intgemm::IntrI8PrepareA(wasm::Instance* instance,
                                    uint32_t inputMatrixA, float scale,
                                    float zeroPoint, uint32_t rowsA,
                                    uint32_t colsA, uint32_t outputMatrixA,
                                    uint8_t* memBase) {
  MOZ_ASSERT(wasm::SASigIntrI8PrepareA.failureMode ==
             wasm::FailureMode::FailOnNegI32);
  JSContext* cx = instance->cx();
  AutoUnsafeCallWithABI unsafe;

  // Size checks for matricies
  if (!CheckMatrixDimension(rowsA, ROWS_A_MULTIPLIER) ||
      !CheckMatrixDimension(colsA, COLUMNS_A_MULTIPLIER)) {
    return -1;
  }

  // Memory Bound checks for all matricies
  uint64_t sizeA = (uint64_t)rowsA * (uint64_t)colsA;
  size_t wasmBufferSize = GetWasmRawBufferLength(memBase);
  if (!CheckMatrixBoundAndAlignment(inputMatrixA, sizeA, wasmBufferSize) ||
      !CheckMatrixBoundAndAlignment(outputMatrixA, sizeA, wasmBufferSize)) {
    return -1;
  }

  // Actual call to the 3rd party library (intgemm)
  const float* inputMatrixAPtr =
      reinterpret_cast<const float*>(&memBase[inputMatrixA]);
  uint8_t* outputMatrixAPtr = &memBase[outputMatrixA];
  AutoProfilerMarker marker(cx->runtime()->geckoProfiler(), "intgemm::PrepareA",
                            FMT_STRING("rowsA: {}, colsA: {}"), rowsA, colsA);
  GEMMOLOGY_DISPATCH(Shift::PrepareA)
  (inputMatrixAPtr, outputMatrixAPtr, scale, rowsA, colsA);
  return 0;
}

int32_t js::intgemm::IntrI8PrepareBias(
    wasm::Instance* instance, uint32_t inputMatrixBPrepared, float scaleA,
    float zeroPointA, float scaleB, float zeroPointB, uint32_t rowsB,
    uint32_t colsB, uint32_t inputBias, uint32_t output, uint8_t* memBase) {
  MOZ_ASSERT(wasm::SASigIntrI8PrepareBias.failureMode ==
             wasm::FailureMode::FailOnNegI32);
  JSContext* cx = instance->cx();
  AutoUnsafeCallWithABI unsafe;

  // Size checks for matricies
  if (!CheckMatrixDimension(rowsB, ROWS_B_MULTIPLIER) ||
      !CheckMatrixDimension(colsB, COLUMNS_B_MULTIPLIER)) {
    return -1;
  }

  // Memory Bound checks for all matrices
  uint64_t sizeB = (uint64_t)rowsB * (uint64_t)colsB;
  uint64_t sizeBias = colsB;
  size_t wasmBufferSize = GetWasmRawBufferLength(memBase);
  if (!CheckMatrixBoundAndAlignment(inputMatrixBPrepared, sizeB,
                                    wasmBufferSize) ||
      !CheckMatrixBound(output, sizeBias, wasmBufferSize)) {
    return -1;
  }

  // Actual call to the 3rd party library (intgemm)
  const int8_t* inputMatrixBPreparedPtr =
      (const int8_t*)&memBase[inputMatrixBPrepared];
  float* outputPtr = (float*)&memBase[output];
  float unquantFactor =
      (-1) * ((127.0f / scaleA) * (127.0f / scaleB)) / (127.0f);

  if (inputBias) {
    if (!CheckMatrixBound(inputBias, sizeBias, wasmBufferSize)) {
      return -1;
    }
    const float* inputBiasPtr = reinterpret_cast<float*>(&memBase[inputBias]);

    AutoProfilerMarker marker(
        cx->runtime()->geckoProfiler(), "intgemm::PrepareBias w/ input bias",
        FMT_STRING("rowsB: {} colsB: {} sizeB: {}"), rowsB, colsB, sizeB);
    GEMMOLOGY_DISPATCH(Shift::PrepareBias)
    (inputMatrixBPreparedPtr, rowsB, colsB,
     gemmology::callbacks::UnquantizeAndAddBiasAndWrite(
         unquantFactor, inputBiasPtr, outputPtr));
  } else {
    AutoProfilerMarker marker(
        cx->runtime()->geckoProfiler(), "intgemm::PrepareBias",
        FMT_STRING("rowsB: {} colsB: {} sizeB: {}"), rowsB, colsB, sizeB);
    GEMMOLOGY_DISPATCH(Shift::PrepareBias)
    (inputMatrixBPreparedPtr, rowsB, colsB,
     gemmology::callbacks::UnquantizeAndWrite(unquantFactor, outputPtr));
  }
  return 0;
}

int32_t js::intgemm::IntrI8MultiplyAndAddBias(
    wasm::Instance* instance, uint32_t inputMatrixAPrepared, float scaleA,
    float zeroPointA, uint32_t inputMatrixBPrepared, float scaleB,
    float zeroPointB, uint32_t inputBiasPrepared, float unquantMultiplier,
    uint32_t rowsA, uint32_t width, uint32_t colsB, uint32_t output,
    uint8_t* memBase) {
  MOZ_ASSERT(wasm::SASigIntrI8MultiplyAndAddBias.failureMode ==
             wasm::FailureMode::FailOnNegI32);
  JSContext* cx = instance->cx();
  AutoUnsafeCallWithABI unsafe;

  // Size checks for matricies
  if (!CheckMatrixDimension(rowsA, ROWS_A_MULTIPLIER) ||
      !CheckMatrixDimension(width, COLUMNS_A_MULTIPLIER) ||
      !CheckMatrixDimension(colsB, COLUMNS_B_MULTIPLIER)) {
    return -1;
  }

  // Memory Bound checks for all matricies
  uint64_t sizeA = (uint64_t)rowsA * (uint64_t)width;
  uint64_t sizeB = (uint64_t)width * (uint64_t)colsB;
  uint64_t sizeBias = (uint64_t)colsB;
  uint64_t sizeOutput = (uint64_t)rowsA * (uint64_t)colsB;
  size_t wasmBufferSize = GetWasmRawBufferLength(memBase);
  if (!CheckMatrixBoundAndAlignment(inputMatrixAPrepared, sizeA,
                                    wasmBufferSize) ||
      !CheckMatrixBoundAndAlignment(inputMatrixBPrepared, sizeB,
                                    wasmBufferSize) ||
      !CheckMatrixBound(inputBiasPrepared, sizeBias, wasmBufferSize) ||
      !CheckMatrixBound(output, sizeOutput, wasmBufferSize)) {
    return -1;
  }

  // Actual call to the 3rd party library (intgemm)
  const uint8_t* inputMatrixAPreparedPtr = &memBase[inputMatrixAPrepared];
  const int8_t* inputMatrixBPreparedPtr =
      reinterpret_cast<const int8_t*>(&memBase[inputMatrixBPrepared]);
  const float* inputBiasPreparedPtr =
      reinterpret_cast<const float*>(&memBase[inputBiasPrepared]);
  float* outputPtr = reinterpret_cast<float*>(&memBase[output]);
  float unquantFactor = unquantMultiplier / (scaleA * scaleB);

  AutoProfilerMarker marker(
      cx->runtime()->geckoProfiler(), "intgemm::Shift::Multiply",
      FMT_STRING("rowsA: {}, width: {}, colsA: {}"), rowsA, width, colsB);
  GEMMOLOGY_DISPATCH(Shift::Multiply)
  (inputMatrixAPreparedPtr, inputMatrixBPreparedPtr, rowsA, width, colsB,
   gemmology::callbacks::UnquantizeAndAddBiasAndWrite(
       unquantFactor, inputBiasPreparedPtr, outputPtr));
  return 0;
}

int32_t js::intgemm::IntrI8SelectColumnsOfB(wasm::Instance* instance,
                                            uint32_t inputMatrixBPrepared,
                                            uint32_t rowsB, uint32_t colsB,
                                            uint32_t colIndexList,
                                            uint32_t sizeColIndexList,
                                            uint32_t output, uint8_t* memBase) {
  MOZ_ASSERT(wasm::SASigIntrI8SelectColumnsOfB.failureMode ==
             wasm::FailureMode::FailOnNegI32);
  JSContext* cx = instance->cx();
  AutoUnsafeCallWithABI unsafe;

  // Size checks for matricies
  if (!CheckMatrixDimension(rowsB, ROWS_B_MULTIPLIER) ||
      !CheckMatrixDimension(colsB, COLUMNS_B_MULTIPLIER) ||
      !CheckMatrixDimension(sizeColIndexList, SELECTED_COLUMNS_B_MULTIPLIER)) {
    return -1;
  }

  // Memory Bound checks for all matricies
  uint64_t sizeB = (uint64_t)rowsB * (uint64_t)colsB;
  uint64_t sizeOutput = (uint64_t)rowsB * (uint64_t)sizeColIndexList;
  size_t wasmBufferSize = GetWasmRawBufferLength(memBase);
  if (!CheckMatrixBoundAndAlignment(inputMatrixBPrepared, sizeB,
                                    wasmBufferSize) ||
      !CheckMatrixBound(colIndexList, sizeColIndexList, wasmBufferSize) ||
      !CheckMatrixBound(output, sizeOutput, wasmBufferSize)) {
    return -1;
  }

  // Actual call to the 3rd party library (intgemm)
  const int8_t* inputMatrixBPreparedPtr =
      reinterpret_cast<const int8_t*>(&memBase[inputMatrixBPrepared]);
  const uint32_t* colIndexListPtr =
      reinterpret_cast<const uint32_t*>(&memBase[colIndexList]);
  int8_t* outputPtr = reinterpret_cast<int8_t*>(&memBase[output]);
  AutoProfilerMarker marker(
      cx->runtime()->geckoProfiler(), "integemm::SelectColumnsB",
      FMT_STRING("rowsB: {} colsB: {} sizecolList: {}, sizeB: {}"), rowsB,
      colsB, sizeColIndexList, sizeB);
  GEMMOLOGY_DISPATCH(SelectColumnsB)
  (inputMatrixBPreparedPtr, outputPtr, rowsB, colIndexListPtr,
   colIndexListPtr + sizeColIndexList);
  return 0;
}

#undef GEMMOLOGY_DISPATCH
#undef SUPPORTED_ARCHS
