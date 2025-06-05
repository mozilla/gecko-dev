/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ds_Nestable_h
#define ds_Nestable_h

#include "mozilla/Assertions.h"

namespace js {

// A base class for nestable structures.
//
// This subclasses of this class must follow a LIFO destruction order, The
// easiest way to ensure that is adding MOZ_STACK_CLASS to the subclasses.
// This class doesn't have the MOZ_STACK_CLASS annotation in order to allow
// specific cases where the subclasses can be allocated on heap for reduced
// stack usage and other implementation specific reasons, but even in such
// cases the subclasses must follow a LIFO destruction order.
template <typename Concrete>
class Nestable {
  Concrete** stack_;
  Concrete* enclosing_;

 protected:
  explicit Nestable(Concrete** stack) : stack_(stack), enclosing_(*stack) {
    *stack_ = static_cast<Concrete*>(this);
  }

  // These method are protected. Some derived classes, such as ParseContext,
  // do not expose the ability to walk the stack.
  Concrete* enclosing() const { return enclosing_; }

  template <typename Predicate /* (Concrete*) -> bool */>
  static Concrete* findNearest(Concrete* it, Predicate predicate) {
    while (it && !predicate(it)) {
      it = it->enclosing();
    }
    return it;
  }

  template <typename T>
  static T* findNearest(Concrete* it) {
    while (it && !it->template is<T>()) {
      it = it->enclosing();
    }
    return it ? &it->template as<T>() : nullptr;
  }

  template <typename T, typename Predicate /* (T*) -> bool */>
  static T* findNearest(Concrete* it, Predicate predicate) {
    while (it && (!it->template is<T>() || !predicate(&it->template as<T>()))) {
      it = it->enclosing();
    }
    return it ? &it->template as<T>() : nullptr;
  }

 public:
  ~Nestable() {
    MOZ_ASSERT(*stack_ == static_cast<Concrete*>(this));
    *stack_ = enclosing_;
  }
};

}  // namespace js

#endif /* ds_Nestable_h */
