/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_TENSOR_H_
#define DOM_TENSOR_H_

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ONNXBinding.h"
#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/onnxruntime_c_api.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom {
class Promise;

class Tensor final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Tensor)

 public:
  // Used when created from js using a regular js array, containing numbers.
  Tensor(const GlobalObject& global, const nsACString& type,
         const nsTArray<uint8_t>& data, const Sequence<int32_t>& dims);
  // Used when created from JS, e.g. input tensor, with a type array (it can be
  // of any type)
  Tensor(const GlobalObject& global, const nsACString& type,
         const ArrayBufferView& data, const Sequence<int32_t>& dims);
  // Used when created from C++, e.g. output tensor
  Tensor(const GlobalObject& aGlobal, ONNXTensorElementDataType aType,
         nsTArray<uint8_t> aData, nsTArray<int64_t> aDims);
  static already_AddRefed<Tensor> Constructor(
      const GlobalObject& global, const nsACString& type,
      const ArrayBufferViewOrAnySequence& data, const Sequence<int32_t>& dims,
      ErrorResult& aRv);

 protected:
  ~Tensor() = default;

 public:
  nsIGlobalObject* GetParentObject() const { return mGlobal; };
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  void GetDims(nsTArray<int32_t>& aRetVal);
  void SetDims(const nsTArray<int32_t>& aVal);
  void GetType(nsCString& aRetVal) const;
  void GetData(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal) const;
  TensorDataLocation Location() const;
  already_AddRefed<Promise> GetData(const Optional<bool>& releaseData);

  void Dispose();
  uint8_t* Data() { return mData.Elements(); }
  size_t Size() { return mData.Length(); }
  int32_t* Dims() { return mDims.Elements(); }
  size_t DimsSize() { return mDims.Length(); }

  ONNXTensorElementDataType Type() const;
  nsCString TypeString() const;
  nsLiteralCString ONNXTypeToString(ONNXTensorElementDataType aType) const;
  nsCString ToString() const;
  static ONNXTensorElementDataType StringToONNXDataType(
      const nsACString& aString);
  static size_t DataTypeSize(ONNXTensorElementDataType aType);

 private:
  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsCString mType;
  nsTArray<uint8_t> mData;
  nsTArray<int32_t> mDims;
};

}  // namespace mozilla::dom

#endif  // DOM_TENSOR_H_
