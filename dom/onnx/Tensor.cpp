/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with
 * fmt::ptr(this\) file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Tensor.h"
#include "js/ArrayBuffer.h"
#include "js/BigInt.h"
#include "js/Value.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ONNXBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TypedArray.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Promise.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "onnxruntime_c_api.h"
#include "mozilla/dom/ToJSValue.h"

extern mozilla::LazyLogModule gONNXLog;
#define LOGD(fmt, ...) \
  MOZ_LOG_FMT(gONNXLog, LogLevel::Debug, fmt, ##__VA_ARGS__)

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Tensor, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Tensor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Tensor)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Tensor)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Tensor::Tensor(const GlobalObject& aGlobal, const nsACString& aType,
               const ArrayBufferView& aData, const Sequence<int32_t>& aDims)
    : mType(aType) {
  LOGD("{}", __PRETTY_FUNCTION__);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  mGlobal = global;
  if (!aData.AppendDataTo(mData)) {
    size_t len = aData.ProcessFixedData(
        [&](const Span<uint8_t>& aData) -> size_t { return aData.Length(); });
    LOGD("{} OOM (size: {})", __PRETTY_FUNCTION__, len);
  }
  mDims.AppendElements(aDims);
}

Tensor::Tensor(const GlobalObject& aGlobal, const nsACString& aType,
               const nsTArray<uint8_t>& aData, const Sequence<int32_t>& aDims)
    : mType(aType) {
  LOGD("{} type: {} len: {}", __PRETTY_FUNCTION__, aType, aData.Length());
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  mGlobal = global;
  // Cast to uint8_t. Type is held in mType
  mData.AppendElements(aData);
  mDims.AppendElements(aDims);
}

Tensor::Tensor(const GlobalObject& aGlobal, ONNXTensorElementDataType aType,
               nsTArray<uint8_t> aData, nsTArray<int64_t> aDims)
    : mType(ONNXTypeToString(aType)) {
  LOGD("Output tensor: {} type: {} len: {}", __PRETTY_FUNCTION__,
       ONNXTypeToString(aType), aData.Length());
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  mGlobal = global;
  mData = std::move(aData);
  mDims.AppendElements(aDims);
}

already_AddRefed<Tensor> Tensor::Constructor(
    const GlobalObject& global, const nsACString& type,
    const ArrayBufferViewOrAnySequence& data, const Sequence<int32_t>& dims,
    ErrorResult& aRv) {
  if (data.IsAnySequence()) {
#define CASE_BIGINT(onnx_type, c_type, conversionfn)           \
  case onnx_type: {                                            \
    nsTArray<c_type> values;                                   \
    for (const JS::Value& element : data.GetAsAnySequence()) { \
      JS::BigInt* bigint = element.toBigInt();                 \
      if (bigint) {                                            \
        values.AppendElement(conversionfn(bigint));            \
      } else {                                                 \
        aRv.ThrowTypeError("Inconsistent value in arg 2");     \
        return nullptr;                                        \
      }                                                        \
    }                                                          \
    valuesAsBytes.AppendElements(                              \
        reinterpret_cast<uint8_t*>(values.Elements()),         \
        values.Length() * sizeof(c_type));                     \
    break;                                                     \
  }

#define CASE(onnx_type, c_type, checkfn, conversionfn)                      \
  case onnx_type: {                                                         \
    nsTArray<c_type> values;                                                \
    for (const auto& element : data.GetAsAnySequence()) {                   \
      if (!element.checkfn()) {                                             \
        aRv.ThrowTypeError(                                                 \
            "Inconsistency between type and value in second argument");     \
        return nullptr;                                                     \
      }                                                                     \
      if (std::numeric_limits<c_type>::lowest() > element.conversionfn() || \
          std::numeric_limits<c_type>::max() < element.conversionfn()) {    \
        aRv.ThrowTypeError("Value out of range in arg 2");                  \
        return nullptr;                                                     \
      }                                                                     \
      values.AppendElement(element.conversionfn());                         \
    }                                                                       \
    valuesAsBytes.AppendElements(                                           \
        reinterpret_cast<uint8_t*>(values.Elements()),                      \
        values.Length() * sizeof(c_type));                                  \
    break;                                                                  \
  }

    nsTArray<uint8_t> valuesAsBytes;
    // Assume constant type, lock on the type of the first element.
    switch (StringToONNXDataType(type)) {
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED, uint8_t, isNumber, toDouble)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, float, isNumber, toDouble)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8, uint8_t, isNumber, toDouble)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8, int8_t, isNumber, toDouble)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16, uint16_t, isNumber, toDouble)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16, int16_t, isNumber, toDouble)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32, int32_t, isNumber, toDouble)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING, int8_t, isNumber, toDouble)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16, int16_t, isNumber, toDouble);
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE, double, isNumber, toDouble);
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32, uint32_t, isNumber, toDouble);
      CASE_BIGINT(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, int64_t, ToBigInt64);
      CASE_BIGINT(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64, uint64_t, ToBigUint64);
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: {
        for (const auto& element : data.GetAsAnySequence()) {
          if (!element.isBoolean()) {
            aRv.ThrowTypeError(
                "Inconsistency between type and value in second argument");
            return nullptr;
          }
          valuesAsBytes.AppendElement(element.toBoolean() ? 1 : 0);
        }
        break;
      }
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4:
        MOZ_CRASH("Not handled");
        break;
    }

    auto rv = MakeRefPtr<Tensor>(global, type, valuesAsBytes, dims);

    LOGD("Tensor from sequence<any>: {}", rv->ToString().get());

    return rv.forget();
  }

  auto rv = MakeRefPtr<Tensor>(global, type, data.GetAsArrayBufferView(), dims);
  LOGD("Tensor from TypedArray: {}", rv->ToString().get());
  return rv.forget();
}  // namespace mozilla::dom

#undef CASE
#undef CASE_BIGINT

void Tensor::Dispose() { mData.Clear(); }

void Tensor::SetDims(const nsTArray<int32_t>& aVal) {
  mDims.Clear();
  mDims.AppendElements(aVal);
}

void Tensor::GetDims(nsTArray<int32_t>& aRetVal) {
  aRetVal.AppendElements(mDims);
}

void Tensor::GetType(nsCString& aRetVal) const { aRetVal.Assign(mType); }

void Tensor::GetData(JSContext* aCx,
                     JS::MutableHandle<JSObject*> aRetVal) const {
  LOGD("{} {} type: {} size: {}", __PRETTY_FUNCTION__, fmt::ptr(this),
       mType.get(), mData.Length());

#define CASE(onnx_type, typed_array_type, c_type)                     \
  case ONNX_TENSOR_ELEMENT_DATA_TYPE_##onnx_type: {                   \
    nsTArray<c_type> tmp((c_type*)mData.Elements(),                   \
                         mData.Length() / sizeof(c_type));            \
    dom::TypedArrayCreator<typed_array_type> creator(std::move(tmp)); \
    aRetVal.set(creator.Create(aCx));                                 \
    break;                                                            \
  }

  switch (Type()) {
    CASE(INT8, Int8Array, int8_t)
    CASE(UINT8, Uint8Array, uint8_t)
    CASE(INT16, Int16Array, int16_t)
    CASE(UINT16, Uint16Array, uint16_t)
    CASE(INT32, Int32Array, int32_t)
    CASE(UINT32, Uint32Array, uint32_t)
    CASE(INT64, BigInt64Array, int64_t)
    CASE(UINT64, BigUint64Array, uint64_t)
    CASE(BOOL, Uint8Array, uint8_t)
    CASE(DOUBLE, Float64Array, double)
    CASE(FLOAT, Float32Array, float)
    CASE(STRING, Uint8Array, uint8_t)  // hmmm
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED:
      MOZ_CRASH("Missing ONNX data type to js value");
      break;
  }

#undef CASE
}  // namespace mozilla::dom

TensorDataLocation Tensor::Location() const {
  LOGD("{} {}", __PRETTY_FUNCTION__, fmt::ptr(this));
  return TensorDataLocation::Cpu;
}

already_AddRefed<Promise> Tensor::GetData(const Optional<bool>& releaseData) {
  LOGD("{} {} type: {} size: {}", __PRETTY_FUNCTION__, fmt::ptr(this),
       mType.get(), mData.Length());

  AutoJSContext ctx;

  RefPtr<Promise> p = Promise::CreateInfallible(mGlobal);

  if (releaseData.WasPassed() && releaseData.Value()) {
    size_t lengthBytes = mData.Length();
    UniquePtr<uint8_t[], JS::FreePolicy> tensorData(
        js_pod_arena_malloc<uint8_t>(js::ArrayBufferContentsArena,
                                     lengthBytes));
    PodCopy(tensorData.get(), mData.Elements(), lengthBytes);
    JS::Rooted<JSObject*> data(
        ctx, JS::NewArrayBufferWithContents(ctx, lengthBytes,
                                            std::move(tensorData)));
    JS::Rooted<JS::Value> value(ctx, JS::ObjectValue(*data));
    p->MaybeResolve(value);
    mData.Clear();
  } else {
    size_t lengthBytes = mData.Length();
    UniquePtr<uint8_t[], JS::FreePolicy> tensorData(
        js_pod_arena_malloc<uint8_t>(js::ArrayBufferContentsArena,
                                     lengthBytes));
    PodCopy(tensorData.get(), mData.Elements(), lengthBytes);
    JS::Rooted<JSObject*> data(
        ctx, JS::NewArrayBufferWithContents(ctx, lengthBytes,
                                            std::move(tensorData)));
    JS::Rooted<JS::Value> value(ctx, JS::ObjectValue(*data));
    p->MaybeResolve(value);
  }

  return p.forget();
}

nsCString Tensor::TypeString() const { return ONNXTypeToString(Type()); }

ONNXTensorElementDataType Tensor::StringToONNXDataType(
    const nsACString& aString) {
#define CASE(string, suffix)                         \
  do {                                               \
    if (aString.EqualsASCII(#string)) {              \
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_##suffix; \
    }                                                \
  } while (0);

  CASE(int4, INT4);
  CASE(uint4, UINT4);
  CASE(int8, INT8);
  CASE(uint8, UINT8);
  CASE(int16, INT16);
  CASE(uint16, UINT16);
  CASE(int32, INT32);
  CASE(uint32, UINT32);
  CASE(int64, INT64);
  CASE(uint64, UINT64);
  CASE(float16, FLOAT16);
  CASE(float32, FLOAT);
  CASE(float64, DOUBLE);
  CASE(bool, BOOL);

  MOZ_CRASH("Missing string to ONNX data type value");

#undef CASE
}

ONNXTensorElementDataType Tensor::Type() const {
  return StringToONNXDataType(mType);
}

nsLiteralCString Tensor::ONNXTypeToString(
    ONNXTensorElementDataType aType) const {
  switch (aType) {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED:
      return "undefined"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4:
      return "uint4"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4:
      return "int4"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
      return "uint8"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
      return "int8"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
      return "uint16"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
      return "int16"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
      return "int32"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
      return "int64"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
      return "uint32"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
      return "uint64"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING:
      return "string"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
      return "bool"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
      return "float16"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
      return "bfloat16"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
      return "float32"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
      return "double"_ns;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ:
      MOZ_CRASH("Missing ONNX data type value to string");
      break;
  }
}

nsCString Tensor::ToString() const {
  nsCString rv;
  size_t count = mData.Length() / DataTypeSize(Type());
  rv.AppendFmt(FMT_STRING("{} {} elements, {} bytes, {} dims"), mType, count,
               mData.Length(), mDims.Length());

  if (MOZ_LOG_TEST(gONNXLog, LogLevel::Verbose)) {
    rv.AppendFmt("Dims:\n");
    rv.AppendFmt("{}\n", fmt::join(mDims, ","));
    rv.AppendFmt("Values:\n");

#define CASE(onnx_type, c_type)                                           \
  case onnx_type: {                                                       \
    rv.AppendFmt("{}\n",                                                  \
                 fmt::join(Span((c_type*)mData.Elements(), count), ",")); \
    break;                                                                \
  }

    switch (Type()) {
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED, uint8_t)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, float)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8, uint8_t)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8, int8_t)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16, uint16_t)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16, int16_t)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32, int32_t)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, int64_t)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING, int8_t)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL, int8_t)
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16, int16_t);
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE, double);
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32, uint32_t);
      CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64, uint64_t);
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4:
      case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4:
        MOZ_CRASH("Not handled");
        break;
    }
#undef CASE
  }
  return rv;
}

size_t Tensor::DataTypeSize(ONNXTensorElementDataType aType) {
#define CASE(onnx_type, c_type) \
  do {                          \
    case onnx_type:             \
      return sizeof(c_type);    \
  } while (0);

  switch (aType) {
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED, uint8_t)
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, float)
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8, uint8_t)
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8, int8_t)
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16, uint16_t)
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16, int16_t)
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32, int32_t)
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, int64_t)
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING, int8_t)
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL, int8_t)
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16, int16_t);
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE, double);
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32, uint32_t);
    CASE(ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64, uint64_t);
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4:
      MOZ_CRASH("Not handled");
      break;
  }
#undef CASE
}

JSObject* Tensor::WrapObject(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) {
  return Tensor_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
