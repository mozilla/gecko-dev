
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkTDArray_Experimental_DEFINED
#define SkTDArray_Experimental_DEFINED

#include "SkTypes.h"

#ifdef SK_BUILD_FOR_UNIX
#define SK_BUILD_FOR_ADS_12
#endif

#if !defined(SK_BUILD_FOR_ADS_12) && !defined(__x86_64__)
#define SK_SMALLER_ARRAY_TEMPLATE_EXPERIMENT 1
#else
#define SK_SMALLER_ARRAY_TEMPLATE_EXPERIMENT 0
#endif

#if SK_SMALLER_ARRAY_TEMPLATE_EXPERIMENT == 0
#include "SkTDArray.h"
#define SkIntArray(type) SkTDArray<type>
#define SkLongArray(type) SkTDArray<type>
#else

class SkDS32Array {
protected:
    SkDS32Array();
    SkDS32Array(const SkDS32Array& src);
    SkDS32Array(const int32_t src[], U16CPU count);
    SkDS32Array& operator=(const SkDS32Array& src);
    friend int operator==(const SkDS32Array& a, const SkDS32Array& b);
    int32_t* append() { return this->append(1, nullptr); }
    int32_t* append(U16CPU count, const int32_t* src = nullptr);

    int32_t* appendClear()
    {
        int32_t* result = this->append();
        *result = 0;
        return result;
    }

    int find(const int32_t& elem) const;
    int32_t* insert(U16CPU index, U16CPU count, const int32_t* src);
    int rfind(const int32_t& elem) const;
    void swap(SkDS32Array& other);
public:
    bool isEmpty() const { return fCount == 0; }
    int count() const { return fCount; }

    void remove(U16CPU index, U16CPU count = 1)
    {
        SkASSERT(index + count <= fCount);
        fCount = SkToU16(fCount - count);
        memmove(fArray + index, fArray + index + count, sizeof(int32_t) * (fCount - index));
    }

    void reset()
    {
        if (fArray)
        {
            sk_free(fArray);
            fArray = nullptr;
#ifdef SK_DEBUG
            fData = nullptr;
#endif
            fReserve = fCount = 0;
        }
        else
        {
            SkASSERT(fReserve == 0 && fCount == 0);
        }
    }

    void setCount(U16CPU count)
    {
        if (count > fReserve)
            this->growBy(count - fCount);
        else
            fCount = SkToU16(count);
    }
protected:
#ifdef SK_DEBUG
    enum {
        kDebugArraySize = 24
    };
    int32_t(* fData)[kDebugArraySize];
#endif
    int32_t*    fArray;
    uint16_t    fReserve, fCount;
    void growBy(U16CPU extra);
};

#ifdef SK_DEBUG
    #define SYNC() fTData = (T (*)[kDebugArraySize]) fArray
#else
    #define SYNC()
#endif

template <typename T> class SkTDS32Array : public SkDS32Array {
public:
    SkTDS32Array() { SkDEBUGCODE(fTData=nullptr); SkASSERT(sizeof(T) == sizeof(int32_t)); }
    SkTDS32Array(const SkTDS32Array<T>& src) : SkDS32Array(src) {}
    ~SkTDS32Array() { sk_free(fArray); }
    T&  operator[](int index) const { SYNC(); SkASSERT((unsigned)index < fCount); return ((T*) fArray)[index]; }
    SkTDS32Array<T>& operator=(const SkTDS32Array<T>& src) {
        return (SkTDS32Array<T>&) SkDS32Array::operator=(src); }
    friend int operator==(const SkTDS32Array<T>& a, const SkTDS32Array<T>& b) {
        return operator==((const SkDS32Array&) a, (const SkDS32Array&) b); }
    T* append() { return (T*) SkDS32Array::append(); }
    T* appendClear() { return (T*) SkDS32Array::appendClear(); }
    T* append(U16CPU count, const T* src = nullptr) { return (T*) SkDS32Array::append(count, (const int32_t*) src); }
    T*  begin() const { SYNC(); return (T*) fArray; }
    T*  end() const { return (T*) (fArray ? fArray + fCount : nullptr); }
    int find(const T& elem) const { return SkDS32Array::find((const int32_t&) elem); }
    T* insert(U16CPU index) { return this->insert(index, 1, nullptr); }
    T* insert(U16CPU index, U16CPU count, const T* src = nullptr) {
        return (T*) SkDS32Array::insert(index, count, (const int32_t*) src); }
    int rfind(const T& elem) const { return SkDS32Array::rfind((const int32_t&) elem); }
    T*          push() { return this->append(); }
    void        push(T& elem) { *this->append() = elem; }
    const T&    top() const { return (*this)[fCount - 1]; }
    T&          top() { return (*this)[fCount - 1]; }
    void        pop(T* elem) { if (elem) *elem = (*this)[fCount - 1]; --fCount; }
    void        pop() { --fCount; }
private:
#ifdef SK_DEBUG
    mutable T(* fTData)[kDebugArraySize];
#endif
};

#define SkIntArray(type) SkTDS32Array<type> // holds 32 bit data types
#define SkLongArray(type) SkTDS32Array<type>

#endif // SK_SMALLER_ARRAY_TEMPLATE_EXPERIMENT

#endif // SkTDArray_Experimental_DEFINED
