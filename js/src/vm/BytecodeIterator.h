/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_BytecodeIterator_h
#define vm_BytecodeIterator_h

#include "vm/BytecodeLocation.h"

namespace js {

class BytecodeIterator {
  BytecodeLocation current_;

 public:
  explicit BytecodeIterator(const JSScript* script);

  explicit BytecodeIterator(BytecodeLocation loc) : current_(loc) {}

  bool operator==(const BytecodeIterator& other) const {
    return other.current_ == current_;
  }

  bool operator!=(const BytecodeIterator& other) const {
    return !(other.current_ == current_);
  }

  const BytecodeLocation& operator*() const { return current_; }

  const BytecodeLocation* operator->() const { return &current_; }

  // Pre-increment
  BytecodeIterator& operator++() {
    current_ = current_.next();
    return *this;
  }

  // Post-increment
  BytecodeIterator operator++(int) {
    current_ = current_.next();
    return *this;
  }
};

// Given a JSScript, allow the construction of a range based for-loop
// that will visit all script locations in that script.
class AllBytecodesIterable {
  const JSScript* script_;

 public:
  explicit AllBytecodesIterable(const JSScript* script) : script_(script) {}

  BytecodeIterator begin();
  BytecodeIterator end();
};

}  // namespace js

#endif
