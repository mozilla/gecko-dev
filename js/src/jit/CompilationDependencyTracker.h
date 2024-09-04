/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CompilationDependencyTracker_h
#define jit_CompilationDependencyTracker_h

#include "mozilla/Vector.h"

#include "jstypes.h"
#include "NamespaceImports.h"

struct JSContext;

namespace js::jit {

struct CompilationDependency {
  enum class Type { GetIterator, Limit };

  Type type;

  CompilationDependency(Type type) : type(type) {}
  virtual bool operator==(CompilationDependency& other) = 0;

  // Return true iff this dependency still holds.
  virtual bool checkDependency() = 0;
  [[nodiscard]] virtual bool registerDependency(JSContext* cx,
                                                HandleScript script) = 0;

  virtual UniquePtr<CompilationDependency> clone() = 0;
  virtual ~CompilationDependency() = default;
};

// For a given Warp compilation keep track of the dependencies this compilation
// is depending on. These dependencies will be checked on main thread during
// link time, causing abandonment of a compilation if they no longer hold.
struct CompilationDependencyTracker {
  mozilla::Vector<UniquePtr<CompilationDependency>, 8, SystemAllocPolicy>
      dependencies;

  [[nodiscard]] bool addDependency(CompilationDependency& dep) {
    // Ensure we don't add duplicates. We expect this list to be short,
    // and so iteration is preferred over a more costly hashset.
    MOZ_ASSERT(dependencies.length() <= 32);
    for (auto& existingDep : dependencies) {
      if (dep == *existingDep) {
        return true;
      }
    }
    return dependencies.append(dep.clone());
  }

  bool checkDependencies() {
    for (auto& dep : dependencies) {
      if (!dep->checkDependency()) {
        return false;
      }
    }
    return true;
  }

  void reset() { dependencies.clear(); }
};

}  // namespace js::jit
#endif /* jit_CompilationDependencyTracker_h */
