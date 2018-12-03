/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2018 Mozilla Foundation
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

#ifndef wasm_cranelift_compile_h
#define wasm_cranelift_compile_h

#include "mozilla/Attributes.h"

#include "wasm/WasmGenerator.h"

namespace js {
namespace wasm {

// Generates code with Cranelift.
MOZ_MUST_USE bool CraneliftCompileFunctions(
    const ModuleEnvironment& env, LifoAlloc& lifo,
    const FuncCompileInputVector& inputs, CompiledCode* code,
    ExclusiveDeferredValidationState& dvs, UniqueChars* error);

}  // namespace wasm
}  // namespace js

#endif  // wasm_cranelift_compile_h
