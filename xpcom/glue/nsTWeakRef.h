/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTWeakRef_h__
#define nsTWeakRef_h__

#ifndef nsDebug_h___
#include "nsDebug.h"
#endif

/**
 * A weak reference class for use with generic C++ objects.  NOT THREADSAFE!
 *
 * Example usage:
 *
 *   class A {
 *   public:
 *     A() : mWeakSelf(this) {
 *     }
 *     ~A() {
 *       mWeakSelf.forget();
 *     }
 *     void Bar() { printf("Bar!\n"); }
 *     const nsTWeakRef<A> &AsWeakRef() const { return mWeakSelf; }
 *   private:
 *     nsTWeakRef<A> mWeakSelf;
 *   };
 *
 *   class B {
 *   public:
 *     void SetA(const nsTWeakRef<A> &a) {
 *       mA = a;
 *     }
 *     void Foo() {
 *       if (mA)
 *         mA->Bar();
 *     }
 *   private:
 *     nsTWeakRef<A> mA;
 *   };
 *
 *   void Test() {
 *     B b;
 *     {
 *       A a;
 *       b.SetA(a.AsWeakRef());
 *       b.Foo();  // prints "Bar!"
 *     }
 *     b.Foo();  // prints nothing because |a| has already been destroyed
 *   }
 *
 * One can imagine much more complex examples, especially when asynchronous
 * event processing is involved.
 *
 * Keep in mind that you should only ever need a class like this when you have
 * multiple instances of B, such that it is not possible for A and B to simply
 * have pointers to one another.
 */
template<class Type>
class nsTWeakRef
{
public:
  ~nsTWeakRef()
  {
    if (mRef) {
      mRef->Release();
    }
  }

  /**
   * Construct from an object pointer (may be null).
   */
  explicit nsTWeakRef(Type* aObj = nullptr)
  {
    if (aObj) {
      mRef = new Inner(aObj);
    } else {
      mRef = nullptr;
    }
  }

  /**
   * Construct from another weak reference object.
   */
  explicit nsTWeakRef(const nsTWeakRef<Type>& aOther) : mRef(aOther.mRef)
  {
    if (mRef) {
      mRef->AddRef();
    }
  }

  /**
   * Assign from an object pointer.
   */
  nsTWeakRef<Type>& operator=(Type* aObj)
  {
    if (mRef) {
      mRef->Release();
    }
    if (aObj) {
      mRef = new Inner(aObj);
    } else {
      mRef = nullptr;
    }
    return *this;
  }

  /**
   * Assign from another weak reference object.
   */
  nsTWeakRef<Type>& operator=(const nsTWeakRef<Type>& aOther)
  {
    if (mRef) {
      mRef->Release();
    }
    mRef = aOther.mRef;
    if (mRef) {
      mRef->AddRef();
    }
    return *this;
  }

  /**
   * Get the referenced object.  This method may return null if the reference
   * has been cleared or if an out-of-memory error occurred at assignment.
   */
  Type* get() const { return mRef ? mRef->mObj : nullptr; }

  /**
   * Called to "null out" the weak reference.  Typically, the object referenced
   * by this weak reference calls this method when it is being destroyed.
   * @returns The former referenced object.
   */
  Type* forget()
  {
    Type* obj;
    if (mRef) {
      obj = mRef->mObj;
      mRef->mObj = nullptr;
      mRef->Release();
      mRef = nullptr;
    } else {
      obj = nullptr;
    }
    return obj;
  }

  /**
   * Allow |*this| to be treated as a |Type*| for convenience.
   */
  operator Type*() const { return get(); }

  /**
   * Allow |*this| to be treated as a |Type*| for convenience.  Use with
   * caution since this method will crash if the referenced object is null.
   */
  Type* operator->() const
  {
    NS_ASSERTION(mRef && mRef->mObj,
                 "You can't dereference a null weak reference with operator->().");
    return get();
  }

private:

  struct Inner
  {
    int     mCnt;
    Type*   mObj;

    Inner(Type* aObj)
      : mCnt(1)
      , mObj(aObj)
    {
    }
    void AddRef()
    {
      ++mCnt;
    }
    void Release()
    {
      if (--mCnt == 0) {
        delete this;
      }
    }
  };

  Inner* mRef;
};

#endif  // nsTWeakRef_h__
