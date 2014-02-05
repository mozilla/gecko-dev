/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2013 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef insanity_pkix__ScopedPtr_h
#define insanity_pkix__ScopedPtr_h

// GCC does not understand nullptr until 4.6
#ifdef __GNUC__
#if __GNUC__ * 100 + __GNUC_MINOR__ < 406
  #define nullptr __null
#endif
#endif

namespace insanity { namespace pkix {

// Similar to boost::scoped_ptr and std::unique_ptr. Does not support copying
// or assignment.
template <typename T, void (*Destroyer)(T*)>
class ScopedPtr
{
public:
  explicit ScopedPtr(T* value = nullptr) : mValue(value) { }
  ~ScopedPtr()
  {
    if (mValue) {
      Destroyer(mValue);
    }
  }

  void operator=(T* newValue)
  {
    if (mValue) {
      Destroyer(mValue);
    }
    mValue = newValue;
  }

  T& operator*() const { return *mValue; }
  T* operator->() const { return mValue; }
  operator bool() const { return mValue; }

  T* get() const { return mValue; }

  T* release()
  {
    T* result = mValue;
    mValue = nullptr;
    return result;
  }

  void reset() { *this = nullptr; }

protected:
  T* mValue;

  ScopedPtr(const ScopedPtr&) /* = delete */;
  void operator=(const ScopedPtr&) /* = delete */;
};

template <typename T, void(*Destroyer)(T*)>
inline bool
operator==(T* a, const ScopedPtr<T, Destroyer>& b)
{
  return a == b.get();
}

template <typename T, void(*Destroyer)(T*)>
inline bool
operator==(const ScopedPtr<T, Destroyer>& a, T* b)
{
  return a.get() == b;
}

template <typename T, void(*Destroyer)(T*)>
inline bool
operator!=(T* a, const ScopedPtr<T, Destroyer>& b)
{
  return a != b.get();
}

template <typename T, void(*Destroyer)(T*)>
inline bool
operator!=(const ScopedPtr<T, Destroyer>& a, T* b)
{
  return a.get() != b;
}

} } // namespace insanity::pkix

#endif // insanity_pkix__ScopedPtr_h
