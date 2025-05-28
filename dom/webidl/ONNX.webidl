/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Func="InferenceSession::InInferenceProcess", Exposed=(DedicatedWorker,Window)]
interface Tensor {
  [Throws]
  constructor(UTF8String type,
              (ArrayBufferView or sequence<any>) data,
              sequence<long> dims);

  [Cached, Pure]
  attribute sequence<long> dims;
  readonly attribute UTF8String type;
  readonly attribute ArrayBufferView data;
  readonly attribute TensorDataLocation location;
  Promise<any> getData(optional boolean releaseData);
  undefined dispose();
};

// Tensor Data Location
enum TensorDataLocation {
  "none",
  "cpu",
  "cpu-pinned",
  "texture",
  "gpu-buffer",
  "ml-tensor"
};

// Input/Output types
typedef record<UTF8String, Tensor> InferenceSessionTensorMapType;
typedef record<UTF8String, Tensor?> InferenceSessionNullableTensorMapType;
typedef InferenceSessionTensorMapType InferenceSessionFeedsType;
typedef (sequence<UTF8String> or InferenceSessionNullableTensorMapType) InferenceSessionFetchesType;
typedef InferenceSessionTensorMapType InferenceSessionReturnType;

dictionary InferenceSessionRunOptions {
  unsigned short logSeverityLevel = 0; // 0 - 4
  unsigned long logVerbosityLevel = 0;
  boolean terminate = true;
  UTF8String tag = "";
};

dictionary InferenceSessionSessionOptions {
  sequence<any> executionProviders;
  unsigned long intraOpNumThreads = 0;
  unsigned long interOpNumThreads = 0;
  record<UTF8String, unsigned long> freeDimensionOverrides;
  UTF8String graphOptimizationLevel = "all";
  boolean enableCpuMemArena = true;
  boolean enableMemPattern = true;
  UTF8String executionMode = "sequential";
  UTF8String optimizedModelFilePath = "";
  boolean enableProfiling = false;
  UTF8String profileFilePrefix = "";
  UTF8String logId = "";
  unsigned short logSeverityLevel = 4; // 0 - 4
  unsigned long logVerbosityLevel = 0;
  (TensorDataLocation or record<UTF8String, TensorDataLocation>) preferredOutputLocation;
  boolean enableGraphCapture = false;
  record<UTF8String, any> extra;
};

[Func="InferenceSession::InInferenceProcess", Exposed=(DedicatedWorker,Window)]
interface InferenceSession {
  [NewObject]
  Promise<InferenceSessionReturnType> run(InferenceSessionFeedsType feeds, optional InferenceSessionRunOptions options = {});
  [NewObject] static Promise<InferenceSession> create((UTF8String or Uint8Array) uriOrBuffer, optional InferenceSessionSessionOptions options = {});
  [BinaryName=ReleaseSession]
  Promise<undefined> release();
  undefined startProfiling();
  undefined endProfiling();
  [Cached, Pure]
  readonly attribute sequence<UTF8String> inputNames;
  [Cached, Pure]
  readonly attribute sequence<UTF8String> outputNames;
};
