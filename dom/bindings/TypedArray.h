/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TypedArray_h
#define mozilla_dom_TypedArray_h

#include "jsfriendapi.h"
#include "js/RootingAPI.h"

namespace mozilla {
namespace dom {

/*
 * Various typed array classes for argument conversion.  We have a base class
 * that has a way of initializing a TypedArray from an existing typed array, and
 * a subclass of the base class that supports creation of a relevant typed array
 * or array buffer object.
 */
template<typename T,
         JSObject* UnwrapArray(JSObject*),
         void GetLengthAndData(JSObject*, uint32_t*, T**)>
struct TypedArray_base {
  TypedArray_base(JSObject* obj)
    : mObj(NULL),
      mData(NULL),
      mLength(0),
      mComputed(false)
  {
    Init(obj);
  }

private:
  JSObject* mObj;
  mutable T* mData;
  mutable uint32_t mLength;
  mutable bool mComputed;

public:
  inline bool Init(JSObject* obj)
  {
    MOZ_ASSERT(!inited());
    DoInit(obj);
    return inited();
  }

  inline bool inited() const {
    return !!mObj;
  }

  inline T *Data() const {
    MOZ_ASSERT(mComputed);
    return mData;
  }

  inline uint32_t Length() const {
    MOZ_ASSERT(mComputed);
    return mLength;
  }

  inline JSObject *Obj() const {
    MOZ_ASSERT(inited());
    return mObj;
  }

  inline void ComputeLengthAndData() const
  {
    MOZ_ASSERT(inited());
    MOZ_ASSERT(!mComputed);
    GetLengthAndData(mObj, &mLength, &mData);
    mComputed = true;
  }

protected:
  inline void DoInit(JSObject* obj)
  {
    mObj = UnwrapArray(obj);
  }
};


template<typename T,
         JSObject* UnwrapArray(JSObject*),
         T* GetData(JSObject*),
         void GetLengthAndData(JSObject*, uint32_t*, T**),
         JSObject* CreateNew(JSContext*, uint32_t)>
struct TypedArray : public TypedArray_base<T, UnwrapArray, GetLengthAndData> {
  TypedArray(JSObject* obj) :
    TypedArray_base<T, UnwrapArray, GetLengthAndData>(obj)
  {}

  static inline JSObject*
  Create(JSContext* cx, nsWrapperCache* creator, uint32_t length,
         const T* data = NULL) {
    JS::Rooted<JSObject*> creatorWrapper(cx);
    Maybe<JSAutoCompartment> ac;
    if (creator && (creatorWrapper = creator->GetWrapperPreserveColor())) {
      ac.construct(cx, creatorWrapper);
    }
    JSObject* obj = CreateNew(cx, length);
    if (!obj) {
      return NULL;
    }
    if (data) {
      T* buf = static_cast<T*>(GetData(obj));
      memcpy(buf, data, length*sizeof(T));
    }
    return obj;
  }
};

typedef TypedArray<int8_t, js::UnwrapInt8Array, JS_GetInt8ArrayData,
                   js::GetInt8ArrayLengthAndData, JS_NewInt8Array>
        Int8Array;
typedef TypedArray<uint8_t, js::UnwrapUint8Array, JS_GetUint8ArrayData,
                   js::GetUint8ArrayLengthAndData, JS_NewUint8Array>
        Uint8Array;
typedef TypedArray<uint8_t, js::UnwrapUint8ClampedArray, JS_GetUint8ClampedArrayData,
                   js::GetUint8ClampedArrayLengthAndData, JS_NewUint8ClampedArray>
        Uint8ClampedArray;
typedef TypedArray<int16_t, js::UnwrapInt16Array, JS_GetInt16ArrayData,
                   js::GetInt16ArrayLengthAndData, JS_NewInt16Array>
        Int16Array;
typedef TypedArray<uint16_t, js::UnwrapUint16Array, JS_GetUint16ArrayData,
                   js::GetUint16ArrayLengthAndData, JS_NewUint16Array>
        Uint16Array;
typedef TypedArray<int32_t, js::UnwrapInt32Array, JS_GetInt32ArrayData,
                   js::GetInt32ArrayLengthAndData, JS_NewInt32Array>
        Int32Array;
typedef TypedArray<uint32_t, js::UnwrapUint32Array, JS_GetUint32ArrayData,
                   js::GetUint32ArrayLengthAndData, JS_NewUint32Array>
        Uint32Array;
typedef TypedArray<float, js::UnwrapFloat32Array, JS_GetFloat32ArrayData,
                   js::GetFloat32ArrayLengthAndData, JS_NewFloat32Array>
        Float32Array;
typedef TypedArray<double, js::UnwrapFloat64Array, JS_GetFloat64ArrayData,
                   js::GetFloat64ArrayLengthAndData, JS_NewFloat64Array>
        Float64Array;
typedef TypedArray_base<uint8_t, js::UnwrapArrayBufferView, js::GetArrayBufferViewLengthAndData>
        ArrayBufferView;
typedef TypedArray<uint8_t, js::UnwrapArrayBuffer, JS_GetArrayBufferData,
                   js::GetArrayBufferLengthAndData, JS_NewArrayBuffer>
        ArrayBuffer;

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_TypedArray_h */
