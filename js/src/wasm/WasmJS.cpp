/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2016 Mozilla Foundation
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

#include "wasm/WasmJS.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/RangedPtr.h"

#include "builtin/Promise.h"
#include "builtin/TypedObject.h"
#include "gc/FreeOp.h"
#include "jit/AtomicOperations.h"
#include "jit/JitOptions.h"
#include "js/Printf.h"
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "vm/Interpreter.h"
#include "vm/StringType.h"
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmCompile.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmIonCompile.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmStubs.h"
#include "wasm/WasmValidate.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::CheckedInt;
using mozilla::Nothing;
using mozilla::RangedPtr;

extern mozilla::Atomic<bool> fuzzingSafe;

bool wasm::HasCompilerSupport(JSContext* cx) {
#if !MOZ_LITTLE_ENDIAN || defined(JS_CODEGEN_NONE)
  return false;
#endif

  if (gc::SystemPageSize() > wasm::PageSize) {
    return false;
  }

  if (!cx->jitSupportsFloatingPoint()) {
    return false;
  }

  if (!cx->jitSupportsUnalignedAccesses()) {
    return false;
  }

  if (!wasm::EnsureFullSignalHandlers(cx)) {
    return false;
  }

  // Wasm threads require 8-byte lock-free atomics.
  if (!jit::AtomicOperations::isLockfree8()) {
    return false;
  }

#ifdef JS_SIMULATOR
  if (!Simulator::supportsAtomics()) {
    return false;
  }
#endif

  return BaselineCanCompile() || IonCanCompile();
}

// Return whether wasm compilation is allowed by prefs.  This check
// only makes sense if HasCompilerSupport() is true.
static bool HasAvailableCompilerTier(JSContext* cx) {
  return (cx->options().wasmBaseline() && BaselineCanCompile()) ||
         (cx->options().wasmIon() && IonCanCompile());
}

bool wasm::HasSupport(JSContext* cx) {
  return cx->options().wasm() && HasCompilerSupport(cx) &&
         HasAvailableCompilerTier(cx);
}

bool wasm::HasStreamingSupport(JSContext* cx) {
  // This should match EnsureStreamSupport().

  return HasSupport(cx) &&
         cx->runtime()->offThreadPromiseState.ref().initialized() &&
         CanUseExtraThreads() && cx->runtime()->consumeStreamCallback &&
         cx->runtime()->reportStreamErrorCallback;
}

bool wasm::HasCachingSupport(JSContext* cx) {
  return HasStreamingSupport(cx) && cx->options().wasmIon() && IonCanCompile();
}

static bool ToWebAssemblyValue(JSContext* cx, ValType targetType, HandleValue v,
                               MutableHandleVal val) {
  switch (targetType.code()) {
    case ValType::I32: {
      int32_t i32;
      if (!ToInt32(cx, v, &i32)) {
        return false;
      }
      val.set(Val(uint32_t(i32)));
      return true;
    }
    case ValType::F32: {
      double d;
      if (!ToNumber(cx, v, &d)) {
        return false;
      }
      val.set(Val(float(d)));
      return true;
    }
    case ValType::F64: {
      double d;
      if (!ToNumber(cx, v, &d)) {
        return false;
      }
      val.set(Val(d));
      return true;
    }
    case ValType::AnyRef: {
      if (v.isNull()) {
        val.set(Val(targetType, nullptr));
      } else {
        JSObject* obj = ToObject(cx, v);
        if (!obj) {
          return false;
        }
        MOZ_ASSERT(obj->compartment() == cx->compartment());
        val.set(Val(targetType, obj));
      }
      return true;
    }
    case ValType::Ref:
    case ValType::NullRef:
    case ValType::I64: {
      break;
    }
  }
  MOZ_CRASH("unexpected import value type, caller must guard");
}

static Value ToJSValue(const Val& val) {
  switch (val.type().code()) {
    case ValType::I32:
      return Int32Value(val.i32());
    case ValType::F32:
      return DoubleValue(JS::CanonicalizeNaN(double(val.f32())));
    case ValType::F64:
      return DoubleValue(JS::CanonicalizeNaN(val.f64()));
    case ValType::AnyRef:
      if (!val.ptr()) {
        return NullValue();
      }
      return ObjectValue(*(JSObject*)val.ptr());
    case ValType::Ref:
    case ValType::NullRef:
    case ValType::I64:
      break;
  }
  MOZ_CRASH("unexpected type when translating to a JS value");
}

// ============================================================================
// Imports

static bool ThrowBadImportArg(JSContext* cx) {
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_WASM_BAD_IMPORT_ARG);
  return false;
}

static bool ThrowBadImportType(JSContext* cx, const char* field,
                               const char* str) {
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_WASM_BAD_IMPORT_TYPE, field, str);
  return false;
}

static bool GetProperty(JSContext* cx, HandleObject obj, const char* chars,
                        MutableHandleValue v) {
  JSAtom* atom = AtomizeUTF8Chars(cx, chars, strlen(chars));
  if (!atom) {
    return false;
  }

  RootedId id(cx, AtomToId(atom));
  return GetProperty(cx, obj, obj, id, v);
}

static bool GetImports(JSContext* cx, const Module& module,
                       HandleObject importObj,
                       MutableHandle<FunctionVector> funcImports,
                       WasmTableObjectVector& tableImports,
                       MutableHandleWasmMemoryObject memoryImport,
                       WasmGlobalObjectVector& globalObjs,
                       MutableHandleValVector globalImportValues) {
  const ImportVector& imports = module.imports();
  if (!imports.empty() && !importObj) {
    return ThrowBadImportArg(cx);
  }

  const Metadata& metadata = module.metadata();

  uint32_t globalIndex = 0;
  const GlobalDescVector& globals = metadata.globals;
  uint32_t tableIndex = 0;
  const TableDescVector& tables = metadata.tables;
  for (const Import& import : imports) {
    RootedValue v(cx);
    if (!GetProperty(cx, importObj, import.module.get(), &v)) {
      return false;
    }

    if (!v.isObject()) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_BAD_IMPORT_FIELD,
                               import.module.get());
      return false;
    }

    RootedObject obj(cx, &v.toObject());
    if (!GetProperty(cx, obj, import.field.get(), &v)) {
      return false;
    }

    switch (import.kind) {
      case DefinitionKind::Function: {
        if (!IsFunctionObject(v)) {
          return ThrowBadImportType(cx, import.field.get(), "Function");
        }

        if (!funcImports.append(&v.toObject().as<JSFunction>())) {
          return false;
        }

        break;
      }
      case DefinitionKind::Table: {
        const uint32_t index = tableIndex++;
        if (!v.isObject() || !v.toObject().is<WasmTableObject>()) {
          return ThrowBadImportType(cx, import.field.get(), "Table");
        }

        RootedWasmTableObject obj(cx, &v.toObject().as<WasmTableObject>());
        if (obj->table().kind() != tables[index].kind) {
          JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                   JSMSG_WASM_BAD_TBL_TYPE_LINK);
          return false;
        }

        if (!tableImports.append(obj)) {
          return false;
        }
        break;
      }
      case DefinitionKind::Memory: {
        if (!v.isObject() || !v.toObject().is<WasmMemoryObject>()) {
          return ThrowBadImportType(cx, import.field.get(), "Memory");
        }

        MOZ_ASSERT(!memoryImport);
        memoryImport.set(&v.toObject().as<WasmMemoryObject>());
        break;
      }
      case DefinitionKind::Global: {
        const uint32_t index = globalIndex++;
        const GlobalDesc& global = globals[index];
        MOZ_ASSERT(global.importIndex() == index);

        RootedVal val(cx);
        if (v.isObject() && v.toObject().is<WasmGlobalObject>()) {
          RootedWasmGlobalObject obj(cx, &v.toObject().as<WasmGlobalObject>());

          if (obj->isMutable() != global.isMutable()) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                     JSMSG_WASM_BAD_GLOB_MUT_LINK);
            return false;
          }
          if (obj->type() != global.type()) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                     JSMSG_WASM_BAD_GLOB_TYPE_LINK);
            return false;
          }

          if (globalObjs.length() <= index && !globalObjs.resize(index + 1)) {
            ReportOutOfMemory(cx);
            return false;
          }
          globalObjs[index] = obj;
          obj->val(&val);
        } else {
          if (IsNumberType(global.type())) {
            if (!v.isNumber()) {
              return ThrowBadImportType(cx, import.field.get(), "Number");
            }
          } else {
            MOZ_ASSERT(global.type().isReference());
            if (!v.isNull() && !v.isObject()) {
              return ThrowBadImportType(cx, import.field.get(),
                                        "Object-or-null");
            }
          }

          if (global.type() == ValType::I64) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                     JSMSG_WASM_BAD_I64_LINK);
            return false;
          }

          if (global.isMutable()) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                     JSMSG_WASM_BAD_GLOB_MUT_LINK);
            return false;
          }

          if (!ToWebAssemblyValue(cx, global.type(), v, &val)) {
            return false;
          }
        }

        if (!globalImportValues.append(val)) {
          return false;
        }

        break;
      }
    }
  }

  MOZ_ASSERT(globalIndex == globals.length() ||
             !globals[globalIndex].isImport());

  return true;
}

static bool DescribeScriptedCaller(JSContext* cx, ScriptedCaller* caller,
                                   const char* introducer) {
  // Note: JS::DescribeScriptedCaller returns whether a scripted caller was
  // found, not whether an error was thrown. This wrapper function converts
  // back to the more ordinary false-if-error form.

  JS::AutoFilename af;
  if (JS::DescribeScriptedCaller(cx, &af, &caller->line)) {
    caller->filename.reset(
        FormatIntroducedFilename(cx, af.get(), caller->line, introducer));
    if (!caller->filename) {
      return false;
    }
  }

  return true;
}

// ============================================================================
// Testing / Fuzzing support

bool wasm::Eval(JSContext* cx, Handle<TypedArrayObject*> code,
                HandleObject importObj,
                MutableHandleWasmInstanceObject instanceObj) {
  if (!GlobalObject::ensureConstructor(cx, cx->global(), JSProto_WebAssembly)) {
    return false;
  }

  MutableBytes bytecode = cx->new_<ShareableBytes>();
  if (!bytecode) {
    return false;
  }

  if (!bytecode->append((uint8_t*)code->dataPointerEither().unwrap(),
                        code->byteLength())) {
    ReportOutOfMemory(cx);
    return false;
  }

  ScriptedCaller scriptedCaller;
  if (!DescribeScriptedCaller(cx, &scriptedCaller, "wasm_eval")) {
    return false;
  }

  MutableCompileArgs compileArgs =
      cx->new_<CompileArgs>(cx, std::move(scriptedCaller));
  if (!compileArgs) {
    return false;
  }

  UniqueChars error;
  UniqueCharsVector warnings;
  SharedModule module =
      CompileBuffer(*compileArgs, *bytecode, &error, &warnings);
  if (!module) {
    if (error) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_COMPILE_ERROR, error.get());
      return false;
    }
    ReportOutOfMemory(cx);
    return false;
  }

  Rooted<FunctionVector> funcs(cx, FunctionVector(cx));
  Rooted<WasmTableObjectVector> tables(cx);
  RootedWasmMemoryObject memory(cx);
  Rooted<WasmGlobalObjectVector> globalObjs(cx);

  RootedValVector globals(cx);
  if (!GetImports(cx, *module, importObj, &funcs, tables.get(), &memory,
                  globalObjs.get(), &globals)) {
    return false;
  }

  return module->instantiate(cx, funcs, tables.get(), memory, globals,
                             globalObjs.get(), nullptr, instanceObj);
}

bool wasm::CompileAndSerialize(const ShareableBytes& bytecode,
                               Bytes* serialized) {
  MutableCompileArgs compileArgs = js_new<CompileArgs>(ScriptedCaller());
  if (!compileArgs) {
    return false;
  }

  // The caller has ensured HasCachingSupport().
  compileArgs->ionEnabled = true;

  UniqueChars error;
  UniqueCharsVector warnings;
  UniqueLinkData linkData;
  SharedModule module =
      CompileBuffer(*compileArgs, bytecode, &error, &warnings, &linkData);
  if (!module) {
    fprintf(stderr, "Compilation error: %s\n", error ? error.get() : "oom");
    return false;
  }

  MOZ_ASSERT(module->code().hasTier(Tier::Serialized));

  size_t serializedSize = module->serializedSize(*linkData);
  if (!serialized->resize(serializedSize)) {
    return false;
  }

  module->serialize(*linkData, serialized->begin(), serialized->length());
  return true;
}

bool wasm::DeserializeModule(JSContext* cx, const Bytes& serialized,
                             MutableHandleObject moduleObj) {
  MutableModule module =
      Module::deserialize(serialized.begin(), serialized.length());
  if (!module) {
    ReportOutOfMemory(cx);
    return false;
  }

  moduleObj.set(module->createObject(cx));
  return !!moduleObj;
}

// ============================================================================
// Common functions

// '[EnforceRange] unsigned long' types are coerced with
//    ConvertToInt(v, 32, 'unsigned')
// defined in Web IDL Section 3.2.4.9.
static bool EnforceRangeU32(JSContext* cx, HandleValue v, const char* kind,
                            const char* noun, uint32_t* u32) {
  // Step 4.
  double x;
  if (!ToNumber(cx, v, &x)) {
    return false;
  }

  // Step 5.
  if (mozilla::IsNegativeZero(x)) {
    x = 0.0;
  }

  // Step 6.1.
  if (!mozilla::IsFinite(x)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_UINT32, kind, noun);
    return false;
  }

  // Step 6.2.
  x = JS::ToInteger(x);

  // Step 6.3.
  if (x < 0 || x > double(UINT32_MAX)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_UINT32, kind, noun);
    return false;
  }

  *u32 = uint32_t(x);
  MOZ_ASSERT(double(*u32) == x);
  return true;
}

static bool GetLimits(JSContext* cx, HandleObject obj, uint32_t maxInitial,
                      uint32_t maxMaximum, const char* kind, Limits* limits,
                      Shareable allowShared) {
  JSAtom* initialAtom = Atomize(cx, "initial", strlen("initial"));
  if (!initialAtom) {
    return false;
  }
  RootedId initialId(cx, AtomToId(initialAtom));

  RootedValue initialVal(cx);
  if (!GetProperty(cx, obj, obj, initialId, &initialVal)) {
    return false;
  }

  if (!EnforceRangeU32(cx, initialVal, kind, "initial size",
                       &limits->initial)) {
    return false;
  }

  if (limits->initial > maxInitial) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_RANGE,
                             kind, "initial size");
    return false;
  }

  JSAtom* maximumAtom = Atomize(cx, "maximum", strlen("maximum"));
  if (!maximumAtom) {
    return false;
  }
  RootedId maximumId(cx, AtomToId(maximumAtom));

  RootedValue maxVal(cx);
  if (!GetProperty(cx, obj, obj, maximumId, &maxVal)) {
    return false;
  }

  // maxVal does not have a default value.
  if (!maxVal.isUndefined()) {
    limits->maximum.emplace();
    if (!EnforceRangeU32(cx, maxVal, kind, "maximum size",
                         limits->maximum.ptr())) {
      return false;
    }

    if (*limits->maximum > maxMaximum || limits->initial > *limits->maximum) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_BAD_RANGE, kind, "maximum size");
      return false;
    }
  }

  limits->shared = Shareable::False;

  if (allowShared == Shareable::True) {
    JSAtom* sharedAtom = Atomize(cx, "shared", strlen("shared"));
    if (!sharedAtom) {
      return false;
    }
    RootedId sharedId(cx, AtomToId(sharedAtom));

    RootedValue sharedVal(cx);
    if (!GetProperty(cx, obj, obj, sharedId, &sharedVal)) {
      return false;
    }

    // shared's default value is false, which is already the value set above.
    if (!sharedVal.isUndefined()) {
      limits->shared =
          ToBoolean(sharedVal) ? Shareable::True : Shareable::False;

      if (limits->shared == Shareable::True) {
        if (maxVal.isUndefined()) {
          JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                    JSMSG_WASM_MISSING_MAXIMUM, kind);
          return false;
        }

        if (!cx->realm()
                 ->creationOptions()
                 .getSharedMemoryAndAtomicsEnabled()) {
          JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                    JSMSG_WASM_NO_SHMEM_LINK);
          return false;
        }
      }
    }
  }

  return true;
}

// ============================================================================
// WebAssembly.Module class and methods

const ClassOps WasmModuleObject::classOps_ = {nullptr, /* addProperty */
                                              nullptr, /* delProperty */
                                              nullptr, /* enumerate */
                                              nullptr, /* newEnumerate */
                                              nullptr, /* resolve */
                                              nullptr, /* mayResolve */
                                              WasmModuleObject::finalize};

const Class WasmModuleObject::class_ = {
    "WebAssembly.Module",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(WasmModuleObject::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &WasmModuleObject::classOps_,
};

const JSPropertySpec WasmModuleObject::properties[] = {JS_PS_END};

const JSFunctionSpec WasmModuleObject::methods[] = {JS_FS_END};

const JSFunctionSpec WasmModuleObject::static_methods[] = {
    JS_FN("imports", WasmModuleObject::imports, 1, JSPROP_ENUMERATE),
    JS_FN("exports", WasmModuleObject::exports, 1, JSPROP_ENUMERATE),
    JS_FN("customSections", WasmModuleObject::customSections, 2,
          JSPROP_ENUMERATE),
    JS_FS_END};

/* static */ void WasmModuleObject::finalize(FreeOp* fop, JSObject* obj) {
  obj->as<WasmModuleObject>().module().Release();
}

static bool IsModuleObject(JSObject* obj, const Module** module) {
  JSObject* unwrapped = CheckedUnwrap(obj);
  if (!unwrapped || !unwrapped->is<WasmModuleObject>()) {
    return false;
  }

  *module = &unwrapped->as<WasmModuleObject>().module();
  return true;
}

static bool GetModuleArg(JSContext* cx, CallArgs args, uint32_t numRequired,
                         const char* name, const Module** module) {
  if (!args.requireAtLeast(cx, name, numRequired)) {
    return false;
  }

  if (!args[0].isObject() || !IsModuleObject(&args[0].toObject(), module)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_MOD_ARG);
    return false;
  }

  return true;
}

struct KindNames {
  RootedPropertyName kind;
  RootedPropertyName table;
  RootedPropertyName memory;
  RootedPropertyName signature;

  explicit KindNames(JSContext* cx)
      : kind(cx), table(cx), memory(cx), signature(cx) {}
};

static bool InitKindNames(JSContext* cx, KindNames* names) {
  JSAtom* kind = Atomize(cx, "kind", strlen("kind"));
  if (!kind) {
    return false;
  }
  names->kind = kind->asPropertyName();

  JSAtom* table = Atomize(cx, "table", strlen("table"));
  if (!table) {
    return false;
  }
  names->table = table->asPropertyName();

  JSAtom* memory = Atomize(cx, "memory", strlen("memory"));
  if (!memory) {
    return false;
  }
  names->memory = memory->asPropertyName();

  JSAtom* signature = Atomize(cx, "signature", strlen("signature"));
  if (!signature) {
    return false;
  }
  names->signature = signature->asPropertyName();

  return true;
}

static JSString* KindToString(JSContext* cx, const KindNames& names,
                              DefinitionKind kind) {
  switch (kind) {
    case DefinitionKind::Function:
      return cx->names().function;
    case DefinitionKind::Table:
      return names.table;
    case DefinitionKind::Memory:
      return names.memory;
    case DefinitionKind::Global:
      return cx->names().global;
  }

  MOZ_CRASH("invalid kind");
}

static JSString* FuncTypeToString(JSContext* cx, const FuncType& funcType) {
  StringBuffer buf(cx);
  if (!buf.append('(')) {
    return nullptr;
  }

  bool first = true;
  for (ValType arg : funcType.args()) {
    if (!first && !buf.append(", ", strlen(", "))) {
      return nullptr;
    }

    const char* argStr = ToCString(arg);
    if (!buf.append(argStr, strlen(argStr))) {
      return nullptr;
    }

    first = false;
  }

  if (!buf.append(") -> (", strlen(") -> ("))) {
    return nullptr;
  }

  if (funcType.ret() != ExprType::Void) {
    const char* retStr = ToCString(funcType.ret());
    if (!buf.append(retStr, strlen(retStr))) {
      return nullptr;
    }
  }

  if (!buf.append(')')) {
    return nullptr;
  }

  return buf.finishString();
}

static JSString* UTF8CharsToString(JSContext* cx, const char* chars) {
  return NewStringCopyUTF8Z<CanGC>(cx,
                                   JS::ConstUTF8CharsZ(chars, strlen(chars)));
}

/* static */ bool WasmModuleObject::imports(JSContext* cx, unsigned argc,
                                            Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  const Module* module;
  if (!GetModuleArg(cx, args, 1, "WebAssembly.Module.imports", &module)) {
    return false;
  }

  KindNames names(cx);
  if (!InitKindNames(cx, &names)) {
    return false;
  }

  AutoValueVector elems(cx);
  if (!elems.reserve(module->imports().length())) {
    return false;
  }

  const FuncImportVector& funcImports =
      module->metadata(module->code().stableTier()).funcImports;

  size_t numFuncImport = 0;
  for (const Import& import : module->imports()) {
    Rooted<IdValueVector> props(cx, IdValueVector(cx));
    if (!props.reserve(3)) {
      return false;
    }

    JSString* moduleStr = UTF8CharsToString(cx, import.module.get());
    if (!moduleStr) {
      return false;
    }
    props.infallibleAppend(
        IdValuePair(NameToId(cx->names().module), StringValue(moduleStr)));

    JSString* nameStr = UTF8CharsToString(cx, import.field.get());
    if (!nameStr) {
      return false;
    }
    props.infallibleAppend(
        IdValuePair(NameToId(cx->names().name), StringValue(nameStr)));

    JSString* kindStr = KindToString(cx, names, import.kind);
    if (!kindStr) {
      return false;
    }
    props.infallibleAppend(
        IdValuePair(NameToId(names.kind), StringValue(kindStr)));

    if (fuzzingSafe && import.kind == DefinitionKind::Function) {
      JSString* ftStr =
          FuncTypeToString(cx, funcImports[numFuncImport++].funcType());
      if (!ftStr) {
        return false;
      }
      if (!props.append(
              IdValuePair(NameToId(names.signature), StringValue(ftStr)))) {
        return false;
      }
    }

    JSObject* obj = ObjectGroup::newPlainObject(cx, props.begin(),
                                                props.length(), GenericObject);
    if (!obj) {
      return false;
    }

    elems.infallibleAppend(ObjectValue(*obj));
  }

  JSObject* arr = NewDenseCopiedArray(cx, elems.length(), elems.begin());
  if (!arr) {
    return false;
  }

  args.rval().setObject(*arr);
  return true;
}

/* static */ bool WasmModuleObject::exports(JSContext* cx, unsigned argc,
                                            Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  const Module* module;
  if (!GetModuleArg(cx, args, 1, "WebAssembly.Module.exports", &module)) {
    return false;
  }

  KindNames names(cx);
  if (!InitKindNames(cx, &names)) {
    return false;
  }

  AutoValueVector elems(cx);
  if (!elems.reserve(module->exports().length())) {
    return false;
  }

  const FuncExportVector& funcExports =
      module->metadata(module->code().stableTier()).funcExports;

  size_t numFuncExport = 0;
  for (const Export& exp : module->exports()) {
    Rooted<IdValueVector> props(cx, IdValueVector(cx));
    if (!props.reserve(2)) {
      return false;
    }

    JSString* nameStr = UTF8CharsToString(cx, exp.fieldName());
    if (!nameStr) {
      return false;
    }
    props.infallibleAppend(
        IdValuePair(NameToId(cx->names().name), StringValue(nameStr)));

    JSString* kindStr = KindToString(cx, names, exp.kind());
    if (!kindStr) {
      return false;
    }
    props.infallibleAppend(
        IdValuePair(NameToId(names.kind), StringValue(kindStr)));

    if (fuzzingSafe && exp.kind() == DefinitionKind::Function) {
      JSString* ftStr =
          FuncTypeToString(cx, funcExports[numFuncExport++].funcType());
      if (!ftStr) {
        return false;
      }
      if (!props.append(
              IdValuePair(NameToId(names.signature), StringValue(ftStr)))) {
        return false;
      }
    }

    JSObject* obj = ObjectGroup::newPlainObject(cx, props.begin(),
                                                props.length(), GenericObject);
    if (!obj) {
      return false;
    }

    elems.infallibleAppend(ObjectValue(*obj));
  }

  JSObject* arr = NewDenseCopiedArray(cx, elems.length(), elems.begin());
  if (!arr) {
    return false;
  }

  args.rval().setObject(*arr);
  return true;
}

/* static */ bool WasmModuleObject::customSections(JSContext* cx, unsigned argc,
                                                   Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  const Module* module;
  if (!GetModuleArg(cx, args, 2, "WebAssembly.Module.customSections",
                    &module)) {
    return false;
  }

  Vector<char, 8> name(cx);
  {
    RootedString str(cx, ToString(cx, args.get(1)));
    if (!str) {
      return false;
    }

    Rooted<JSFlatString*> flat(cx, str->ensureFlat(cx));
    if (!flat) {
      return false;
    }

    if (!name.initLengthUninitialized(JS::GetDeflatedUTF8StringLength(flat))) {
      return false;
    }

    JS::DeflateStringToUTF8Buffer(flat,
                                  RangedPtr<char>(name.begin(), name.length()));
  }

  AutoValueVector elems(cx);
  RootedArrayBufferObject buf(cx);
  for (const CustomSection& cs : module->customSections()) {
    if (name.length() != cs.name.length()) {
      continue;
    }
    if (memcmp(name.begin(), cs.name.begin(), name.length())) {
      continue;
    }

    buf = ArrayBufferObject::create(cx, cs.payload->length());
    if (!buf) {
      return false;
    }

    memcpy(buf->dataPointer(), cs.payload->begin(), cs.payload->length());
    if (!elems.append(ObjectValue(*buf))) {
      return false;
    }
  }

  JSObject* arr = NewDenseCopiedArray(cx, elems.length(), elems.begin());
  if (!arr) {
    return false;
  }

  args.rval().setObject(*arr);
  return true;
}

/* static */ WasmModuleObject* WasmModuleObject::create(JSContext* cx,
                                                        const Module& module,
                                                        HandleObject proto) {
  AutoSetNewObjectMetadata metadata(cx);
  auto* obj = NewObjectWithGivenProto<WasmModuleObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  obj->initReservedSlot(MODULE_SLOT,
                        PrivateValue(const_cast<Module*>(&module)));
  module.AddRef();
  // We account for the first tier here; the second tier, if different, will be
  // accounted for separately when it's been compiled.
  cx->zone()->updateJitCodeMallocBytes(
      module.codeLength(module.code().stableTier()));
  return obj;
}

static bool GetBufferSource(JSContext* cx, JSObject* obj, unsigned errorNumber,
                            MutableBytes* bytecode) {
  *bytecode = cx->new_<ShareableBytes>();
  if (!*bytecode) {
    return false;
  }

  JSObject* unwrapped = CheckedUnwrap(obj);

  SharedMem<uint8_t*> dataPointer;
  size_t byteLength;
  if (!unwrapped || !IsBufferSource(unwrapped, &dataPointer, &byteLength)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, errorNumber);
    return false;
  }

  if (!(*bytecode)->append(dataPointer.unwrap(), byteLength)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

static MutableCompileArgs InitCompileArgs(JSContext* cx,
                                          const char* introducer) {
  ScriptedCaller scriptedCaller;
  if (!DescribeScriptedCaller(cx, &scriptedCaller, introducer)) {
    return nullptr;
  }

  return cx->new_<CompileArgs>(cx, std::move(scriptedCaller));
}

static bool ReportCompileWarnings(JSContext* cx,
                                  const UniqueCharsVector& warnings) {
  // Avoid spamming the console.
  size_t numWarnings = Min<size_t>(warnings.length(), 3);

  for (size_t i = 0; i < numWarnings; i++) {
    if (!JS_ReportErrorFlagsAndNumberASCII(
            cx, JSREPORT_WARNING, GetErrorMessage, nullptr,
            JSMSG_WASM_COMPILE_WARNING, warnings[i].get()))
      return false;
  }

  if (warnings.length() > numWarnings) {
    if (!JS_ReportErrorFlagsAndNumberASCII(
            cx, JSREPORT_WARNING, GetErrorMessage, nullptr,
            JSMSG_WASM_COMPILE_WARNING, "other warnings suppressed"))
      return false;
  }

  return true;
}

/* static */ bool WasmModuleObject::construct(JSContext* cx, unsigned argc,
                                              Value* vp) {
  CallArgs callArgs = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, callArgs, "Module")) {
    return false;
  }

  if (!callArgs.requireAtLeast(cx, "WebAssembly.Module", 1)) {
    return false;
  }

  if (!callArgs[0].isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_BUF_ARG);
    return false;
  }

  MutableBytes bytecode;
  if (!GetBufferSource(cx, &callArgs[0].toObject(), JSMSG_WASM_BAD_BUF_ARG,
                       &bytecode)) {
    return false;
  }

  SharedCompileArgs compileArgs = InitCompileArgs(cx, "WebAssembly.Module");
  if (!compileArgs) {
    return false;
  }

  UniqueChars error;
  UniqueCharsVector warnings;
  SharedModule module =
      CompileBuffer(*compileArgs, *bytecode, &error, &warnings);
  if (!module) {
    if (error) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_COMPILE_ERROR, error.get());
      return false;
    }
    ReportOutOfMemory(cx);
    return false;
  }

  if (!ReportCompileWarnings(cx, warnings)) {
    return false;
  }

  RootedObject proto(
      cx, &cx->global()->getPrototype(JSProto_WasmModule).toObject());
  RootedObject moduleObj(cx, WasmModuleObject::create(cx, *module, proto));
  if (!moduleObj) {
    return false;
  }

  callArgs.rval().setObject(*moduleObj);
  return true;
}

const Module& WasmModuleObject::module() const {
  MOZ_ASSERT(is<WasmModuleObject>());
  return *(const Module*)getReservedSlot(MODULE_SLOT).toPrivate();
}

// ============================================================================
// WebAssembly.Instance class and methods

const ClassOps WasmInstanceObject::classOps_ = {nullptr, /* addProperty */
                                                nullptr, /* delProperty */
                                                nullptr, /* enumerate */
                                                nullptr, /* newEnumerate */
                                                nullptr, /* resolve */
                                                nullptr, /* mayResolve */
                                                WasmInstanceObject::finalize,
                                                nullptr, /* call */
                                                nullptr, /* hasInstance */
                                                nullptr, /* construct */
                                                WasmInstanceObject::trace};

const Class WasmInstanceObject::class_ = {
    "WebAssembly.Instance",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(WasmInstanceObject::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &WasmInstanceObject::classOps_,
};

static bool IsInstance(HandleValue v) {
  return v.isObject() && v.toObject().is<WasmInstanceObject>();
}

/* static */ bool WasmInstanceObject::exportsGetterImpl(JSContext* cx,
                                                        const CallArgs& args) {
  args.rval().setObject(
      args.thisv().toObject().as<WasmInstanceObject>().exportsObj());
  return true;
}

/* static */ bool WasmInstanceObject::exportsGetter(JSContext* cx,
                                                    unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsInstance, exportsGetterImpl>(cx, args);
}

const JSPropertySpec WasmInstanceObject::properties[] = {
    JS_PSG("exports", WasmInstanceObject::exportsGetter, JSPROP_ENUMERATE),
    JS_PS_END};

const JSFunctionSpec WasmInstanceObject::methods[] = {JS_FS_END};

const JSFunctionSpec WasmInstanceObject::static_methods[] = {JS_FS_END};

bool WasmInstanceObject::isNewborn() const {
  MOZ_ASSERT(is<WasmInstanceObject>());
  return getReservedSlot(INSTANCE_SLOT).isUndefined();
}

/* static */ void WasmInstanceObject::finalize(FreeOp* fop, JSObject* obj) {
  fop->delete_(&obj->as<WasmInstanceObject>().exports());
  fop->delete_(&obj->as<WasmInstanceObject>().scopes());
  fop->delete_(&obj->as<WasmInstanceObject>().indirectGlobals());
  if (!obj->as<WasmInstanceObject>().isNewborn()) {
    fop->delete_(&obj->as<WasmInstanceObject>().instance());
  }
}

/* static */ void WasmInstanceObject::trace(JSTracer* trc, JSObject* obj) {
  WasmInstanceObject& instanceObj = obj->as<WasmInstanceObject>();
  instanceObj.exports().trace(trc);
  instanceObj.indirectGlobals().trace(trc);
  if (!instanceObj.isNewborn()) {
    instanceObj.instance().tracePrivate(trc);
  }
}

/* static */ WasmInstanceObject* WasmInstanceObject::create(
    JSContext* cx, SharedCode code, const DataSegmentVector& dataSegments,
    const ElemSegmentVector& elemSegments, UniqueTlsData tlsData,
    HandleWasmMemoryObject memory, SharedTableVector&& tables,
    StructTypeDescrVector&& structTypeDescrs,
    Handle<FunctionVector> funcImports, const GlobalDescVector& globals,
    HandleValVector globalImportValues,
    const WasmGlobalObjectVector& globalObjs, HandleObject proto,
    UniqueDebugState maybeDebug) {
  UniquePtr<ExportMap> exports = js::MakeUnique<ExportMap>();
  if (!exports) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  UniquePtr<ScopeMap> scopes = js::MakeUnique<ScopeMap>(cx->zone());
  if (!scopes) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  uint32_t indirectGlobals = 0;

  for (uint32_t i = 0; i < globalObjs.length(); i++) {
    if (globalObjs[i] && globals[i].isIndirect()) {
      indirectGlobals++;
    }
  }

  Rooted<UniquePtr<WasmGlobalObjectVector>> indirectGlobalObjs(
      cx, js::MakeUnique<WasmGlobalObjectVector>());
  if (!indirectGlobalObjs || !indirectGlobalObjs->resize(indirectGlobals)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  {
    uint32_t next = 0;
    for (uint32_t i = 0; i < globalObjs.length(); i++) {
      if (globalObjs[i] && globals[i].isIndirect()) {
        (*indirectGlobalObjs)[next++] = globalObjs[i];
      }
    }
  }

  AutoSetNewObjectMetadata metadata(cx);
  RootedWasmInstanceObject obj(
      cx, NewObjectWithGivenProto<WasmInstanceObject>(cx, proto));
  if (!obj) {
    return nullptr;
  }

  MOZ_ASSERT(obj->isTenured(), "assumed by WasmTableObject write barriers");

  // Finalization assumes these slots are always initialized:
  obj->initReservedSlot(EXPORTS_SLOT, PrivateValue(exports.release()));
  obj->initReservedSlot(SCOPES_SLOT, PrivateValue(scopes.release()));
  obj->initReservedSlot(GLOBALS_SLOT,
                        PrivateValue(indirectGlobalObjs.release()));
  obj->initReservedSlot(INSTANCE_SCOPE_SLOT, UndefinedValue());

  // The INSTANCE_SLOT may not be initialized if Instance allocation fails,
  // leading to an observable "newborn" state in tracing/finalization.
  MOZ_ASSERT(obj->isNewborn());

  // Root the Instance via WasmInstanceObject before any possible GC.
  auto* instance = cx->new_<Instance>(
      cx, obj, code, std::move(tlsData), memory, std::move(tables),
      std::move(structTypeDescrs), funcImports, globalImportValues, globalObjs,
      std::move(maybeDebug));
  if (!instance) {
    return nullptr;
  }

  obj->initReservedSlot(INSTANCE_SLOT, PrivateValue(instance));
  MOZ_ASSERT(!obj->isNewborn());

  if (!instance->init(cx, dataSegments, elemSegments)) {
    return nullptr;
  }

  return obj;
}

void WasmInstanceObject::initExportsObj(JSObject& exportsObj) {
  MOZ_ASSERT(getReservedSlot(EXPORTS_OBJ_SLOT).isUndefined());
  setReservedSlot(EXPORTS_OBJ_SLOT, ObjectValue(exportsObj));
}

static bool GetImportArg(JSContext* cx, CallArgs callArgs,
                         MutableHandleObject importObj) {
  if (!callArgs.get(1).isUndefined()) {
    if (!callArgs[1].isObject()) {
      return ThrowBadImportArg(cx);
    }
    importObj.set(&callArgs[1].toObject());
  }
  return true;
}

static bool Instantiate(JSContext* cx, const Module& module,
                        HandleObject importObj,
                        MutableHandleWasmInstanceObject instanceObj) {
  RootedObject instanceProto(
      cx, &cx->global()->getPrototype(JSProto_WasmInstance).toObject());

  Rooted<FunctionVector> funcs(cx, FunctionVector(cx));
  Rooted<WasmTableObjectVector> tables(cx);
  RootedWasmMemoryObject memory(cx);
  Rooted<WasmGlobalObjectVector> globalObjs(cx);

  RootedValVector globals(cx);
  if (!GetImports(cx, module, importObj, &funcs, tables.get(), &memory,
                  globalObjs.get(), &globals)) {
    return false;
  }

  return module.instantiate(cx, funcs, tables.get(), memory, globals,
                            globalObjs.get(), instanceProto, instanceObj);
}

/* static */ bool WasmInstanceObject::construct(JSContext* cx, unsigned argc,
                                                Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Instance")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Instance", 1)) {
    return false;
  }

  const Module* module;
  if (!args[0].isObject() || !IsModuleObject(&args[0].toObject(), &module)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_MOD_ARG);
    return false;
  }

  RootedObject importObj(cx);
  if (!GetImportArg(cx, args, &importObj)) {
    return false;
  }

  RootedWasmInstanceObject instanceObj(cx);
  if (!Instantiate(cx, *module, importObj, &instanceObj)) {
    return false;
  }

  args.rval().setObject(*instanceObj);
  return true;
}

Instance& WasmInstanceObject::instance() const {
  MOZ_ASSERT(!isNewborn());
  return *(Instance*)getReservedSlot(INSTANCE_SLOT).toPrivate();
}

JSObject& WasmInstanceObject::exportsObj() const {
  return getReservedSlot(EXPORTS_OBJ_SLOT).toObject();
}

WasmInstanceObject::ExportMap& WasmInstanceObject::exports() const {
  return *(ExportMap*)getReservedSlot(EXPORTS_SLOT).toPrivate();
}

WasmInstanceObject::ScopeMap& WasmInstanceObject::scopes() const {
  return *(ScopeMap*)getReservedSlot(SCOPES_SLOT).toPrivate();
}

WasmGlobalObjectVector& WasmInstanceObject::indirectGlobals() const {
  return *(WasmGlobalObjectVector*)getReservedSlot(GLOBALS_SLOT).toPrivate();
}

static bool WasmCall(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedFunction callee(cx, &args.callee().as<JSFunction>());

  Instance& instance = ExportedFunctionToInstance(callee);
  uint32_t funcIndex = ExportedFunctionToFuncIndex(callee);
  return instance.callExport(cx, funcIndex, args);
}

static bool EnsureLazyEntryStub(const Instance& instance,
                                size_t funcExportIndex, const FuncExport& fe) {
  if (fe.hasEagerStubs()) {
    return true;
  }

  MOZ_ASSERT(!instance.isAsmJS(), "only wasm can lazily export functions");

  // If the best tier is Ion, life is simple: background compilation has
  // already completed and has been committed, so there's no risk of race
  // conditions here.
  //
  // If the best tier is Baseline, there could be a background compilation
  // happening at the same time. The background compilation will lock the
  // first tier lazy stubs first to stop new baseline stubs from being
  // generated, then the second tier stubs to generate them.
  //
  // - either we take the tier1 lazy stub lock before the background
  // compilation gets it, then we generate the lazy stub for tier1. When the
  // background thread gets the tier1 lazy stub lock, it will see it has a
  // lazy stub and will recompile it for tier2.
  // - or we don't take the lock here first. Background compilation won't
  // find a lazy stub for this function, thus won't generate it. So we'll do
  // it ourselves after taking the tier2 lock.

  Tier prevTier = instance.code().bestTier();

  auto stubs = instance.code(prevTier).lazyStubs().lock();
  if (stubs->hasStub(fe.funcIndex())) {
    return true;
  }

  // The best tier might have changed after we've taken the lock.
  Tier tier = instance.code().bestTier();
  const CodeTier& codeTier = instance.code(tier);
  if (tier == prevTier) {
    return stubs->createOne(funcExportIndex, codeTier);
  }

  MOZ_ASSERT(prevTier == Tier::Baseline && tier == Tier::Optimized);

  auto stubs2 = instance.code(tier).lazyStubs().lock();

  // If it didn't have a stub in the first tier, background compilation
  // shouldn't have made one in the second tier.
  MOZ_ASSERT(!stubs2->hasStub(fe.funcIndex()));

  return stubs2->createOne(funcExportIndex, codeTier);
}

/* static */ bool WasmInstanceObject::getExportedFunction(
    JSContext* cx, HandleWasmInstanceObject instanceObj, uint32_t funcIndex,
    MutableHandleFunction fun) {
  if (ExportMap::Ptr p = instanceObj->exports().lookup(funcIndex)) {
    fun.set(p->value());
    return true;
  }

  const Instance& instance = instanceObj->instance();
  const MetadataTier& metadata = instance.metadata(instance.code().bestTier());

  size_t funcExportIndex;
  const FuncExport& funcExport =
      metadata.lookupFuncExport(funcIndex, &funcExportIndex);

  if (!EnsureLazyEntryStub(instance, funcExportIndex, funcExport)) {
    return false;
  }

  const FuncType& funcType = funcExport.funcType();
  unsigned numArgs = funcType.args().length();

  if (instance.isAsmJS()) {
    // asm.js needs to act like a normal JS function which means having the
    // name from the original source and being callable as a constructor.
    RootedAtom name(cx, instance.getFuncDisplayAtom(cx, funcIndex));
    if (!name) {
      return false;
    }
    fun.set(NewNativeConstructor(cx, WasmCall, numArgs, name,
                                 gc::AllocKind::FUNCTION_EXTENDED,
                                 SingletonObject, JSFunction::ASMJS_CTOR));
    if (!fun) {
      return false;
    }
    fun->setAsmJSIndex(funcIndex);
  } else {
    RootedAtom name(cx, NumberToAtom(cx, funcIndex));
    if (!name) {
      return false;
    }

    // Functions with anyref don't have jit entries yet, so they should
    // mostly behave like asm.js functions. Pretend it's the case, until
    // jit entries are implemented.
    JSFunction::Flags flags = funcType.temporarilyUnsupportedAnyRef()
                                  ? JSFunction::ASMJS_NATIVE
                                  : JSFunction::WASM_FUN;

    fun.set(NewNativeFunction(cx, WasmCall, numArgs, name,
                              gc::AllocKind::FUNCTION_EXTENDED, SingletonObject,
                              flags));
    if (!fun) {
      return false;
    }

    if (funcType.temporarilyUnsupportedAnyRef()) {
      fun->setAsmJSIndex(funcIndex);
    } else {
      fun->setWasmJitEntry(instance.code().getAddressOfJitEntry(funcIndex));
    }
  }

  fun->setExtendedSlot(FunctionExtended::WASM_INSTANCE_SLOT,
                       ObjectValue(*instanceObj));

  void* tlsData = instanceObj->instance().tlsData();
  fun->setExtendedSlot(FunctionExtended::WASM_TLSDATA_SLOT,
                       PrivateValue(tlsData));

  if (!instanceObj->exports().putNew(funcIndex, fun)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

const CodeRange& WasmInstanceObject::getExportedFunctionCodeRange(
    JSFunction* fun, Tier tier) {
  uint32_t funcIndex = ExportedFunctionToFuncIndex(fun);
  MOZ_ASSERT(exports().lookup(funcIndex)->value() == fun);
  const MetadataTier& metadata = instance().metadata(tier);
  return metadata.codeRange(metadata.lookupFuncExport(funcIndex));
}

/* static */ WasmInstanceScope* WasmInstanceObject::getScope(
    JSContext* cx, HandleWasmInstanceObject instanceObj) {
  if (!instanceObj->getReservedSlot(INSTANCE_SCOPE_SLOT).isUndefined()) {
    return (WasmInstanceScope*)instanceObj->getReservedSlot(INSTANCE_SCOPE_SLOT)
        .toGCThing();
  }

  Rooted<WasmInstanceScope*> instanceScope(
      cx, WasmInstanceScope::create(cx, instanceObj));
  if (!instanceScope) {
    return nullptr;
  }

  instanceObj->setReservedSlot(INSTANCE_SCOPE_SLOT,
                               PrivateGCThingValue(instanceScope));

  return instanceScope;
}

/* static */ WasmFunctionScope* WasmInstanceObject::getFunctionScope(
    JSContext* cx, HandleWasmInstanceObject instanceObj, uint32_t funcIndex) {
  if (ScopeMap::Ptr p = instanceObj->scopes().lookup(funcIndex)) {
    return p->value();
  }

  Rooted<WasmInstanceScope*> instanceScope(
      cx, WasmInstanceObject::getScope(cx, instanceObj));
  if (!instanceScope) {
    return nullptr;
  }

  Rooted<WasmFunctionScope*> funcScope(
      cx, WasmFunctionScope::create(cx, instanceScope, funcIndex));
  if (!funcScope) {
    return nullptr;
  }

  if (!instanceObj->scopes().putNew(funcIndex, funcScope)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return funcScope;
}

bool wasm::IsExportedFunction(JSFunction* fun) {
  return fun->maybeNative() == WasmCall;
}

bool wasm::IsExportedWasmFunction(JSFunction* fun) {
  return IsExportedFunction(fun) && !ExportedFunctionToInstance(fun).isAsmJS();
}

bool wasm::IsExportedFunction(const Value& v, MutableHandleFunction f) {
  if (!v.isObject()) {
    return false;
  }

  JSObject& obj = v.toObject();
  if (!obj.is<JSFunction>() || !IsExportedFunction(&obj.as<JSFunction>())) {
    return false;
  }

  f.set(&obj.as<JSFunction>());
  return true;
}

Instance& wasm::ExportedFunctionToInstance(JSFunction* fun) {
  return ExportedFunctionToInstanceObject(fun)->instance();
}

WasmInstanceObject* wasm::ExportedFunctionToInstanceObject(JSFunction* fun) {
  MOZ_ASSERT(IsExportedFunction(fun));
  const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_INSTANCE_SLOT);
  return &v.toObject().as<WasmInstanceObject>();
}

uint32_t wasm::ExportedFunctionToFuncIndex(JSFunction* fun) {
  MOZ_ASSERT(IsExportedFunction(fun));
  Instance& instance = ExportedFunctionToInstanceObject(fun)->instance();
  return instance.code().getFuncIndex(fun);
}

// ============================================================================
// WebAssembly.Memory class and methods

const ClassOps WasmMemoryObject::classOps_ = {nullptr, /* addProperty */
                                              nullptr, /* delProperty */
                                              nullptr, /* enumerate */
                                              nullptr, /* newEnumerate */
                                              nullptr, /* resolve */
                                              nullptr, /* mayResolve */
                                              WasmMemoryObject::finalize};

const Class WasmMemoryObject::class_ = {
    "WebAssembly.Memory",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(WasmMemoryObject::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &WasmMemoryObject::classOps_};

/* static */ void WasmMemoryObject::finalize(FreeOp* fop, JSObject* obj) {
  WasmMemoryObject& memory = obj->as<WasmMemoryObject>();
  if (memory.hasObservers()) {
    fop->delete_(&memory.observers());
  }
}

/* static */ WasmMemoryObject* WasmMemoryObject::create(
    JSContext* cx, HandleArrayBufferObjectMaybeShared buffer,
    HandleObject proto) {
  AutoSetNewObjectMetadata metadata(cx);
  auto* obj = NewObjectWithGivenProto<WasmMemoryObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  obj->initReservedSlot(BUFFER_SLOT, ObjectValue(*buffer));
  MOZ_ASSERT(!obj->hasObservers());
  return obj;
}

/* static */ bool WasmMemoryObject::construct(JSContext* cx, unsigned argc,
                                              Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Memory")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Memory", 1)) {
    return false;
  }

  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_DESC_ARG, "memory");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());
  Limits limits;
  if (!GetLimits(cx, obj, MaxMemoryInitialPages, MaxMemoryMaximumPages,
                 "Memory", &limits, Shareable::True)) {
    return false;
  }

  ConvertMemoryPagesToBytes(&limits);

  RootedArrayBufferObjectMaybeShared buffer(cx);
  if (!CreateWasmBuffer(cx, limits, &buffer)) {
    return false;
  }

  RootedObject proto(
      cx, &cx->global()->getPrototype(JSProto_WasmMemory).toObject());
  RootedWasmMemoryObject memoryObj(cx,
                                   WasmMemoryObject::create(cx, buffer, proto));
  if (!memoryObj) {
    return false;
  }

  args.rval().setObject(*memoryObj);
  return true;
}

static bool IsMemory(HandleValue v) {
  return v.isObject() && v.toObject().is<WasmMemoryObject>();
}

/* static */ bool WasmMemoryObject::bufferGetterImpl(JSContext* cx,
                                                     const CallArgs& args) {
  RootedWasmMemoryObject memoryObj(
      cx, &args.thisv().toObject().as<WasmMemoryObject>());
  RootedArrayBufferObjectMaybeShared buffer(cx, &memoryObj->buffer());

  if (memoryObj->isShared()) {
    uint32_t memoryLength = memoryObj->volatileMemoryLength();
    MOZ_ASSERT(memoryLength >= buffer->byteLength());

    if (memoryLength > buffer->byteLength()) {
      RootedSharedArrayBufferObject newBuffer(
          cx, SharedArrayBufferObject::New(
                  cx, memoryObj->sharedArrayRawBuffer(), memoryLength));
      if (!newBuffer) {
        return false;
      }
      // OK to addReference after we try to allocate because the memoryObj
      // keeps the rawBuffer alive.
      if (!memoryObj->sharedArrayRawBuffer()->addReference()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_SC_SAB_REFCNT_OFLO);
        return false;
      }
      buffer = newBuffer;
      memoryObj->setReservedSlot(BUFFER_SLOT, ObjectValue(*newBuffer));
    }
  }

  args.rval().setObject(*buffer);
  return true;
}

/* static */ bool WasmMemoryObject::bufferGetter(JSContext* cx, unsigned argc,
                                                 Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsMemory, bufferGetterImpl>(cx, args);
}

const JSPropertySpec WasmMemoryObject::properties[] = {
    JS_PSG("buffer", WasmMemoryObject::bufferGetter, JSPROP_ENUMERATE),
    JS_PS_END};

/* static */ bool WasmMemoryObject::growImpl(JSContext* cx,
                                             const CallArgs& args) {
  RootedWasmMemoryObject memory(
      cx, &args.thisv().toObject().as<WasmMemoryObject>());

  if (!args.requireAtLeast(cx, "WebAssembly.Memory.grow", 1)) {
    return false;
  }

  uint32_t delta;
  if (!EnforceRangeU32(cx, args.get(0), "Memory", "grow delta", &delta)) {
    return false;
  }

  uint32_t ret = grow(memory, delta, cx);

  if (ret == uint32_t(-1)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_GROW,
                             "memory");
    return false;
  }

  args.rval().setInt32(ret);
  return true;
}

/* static */ bool WasmMemoryObject::grow(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsMemory, growImpl>(cx, args);
}

const JSFunctionSpec WasmMemoryObject::methods[] = {
    JS_FN("grow", WasmMemoryObject::grow, 1, JSPROP_ENUMERATE), JS_FS_END};

const JSFunctionSpec WasmMemoryObject::static_methods[] = {JS_FS_END};

ArrayBufferObjectMaybeShared& WasmMemoryObject::buffer() const {
  return getReservedSlot(BUFFER_SLOT)
      .toObject()
      .as<ArrayBufferObjectMaybeShared>();
}

SharedArrayRawBuffer* WasmMemoryObject::sharedArrayRawBuffer() const {
  MOZ_ASSERT(isShared());
  return buffer().as<SharedArrayBufferObject>().rawBufferObject();
}

uint32_t WasmMemoryObject::volatileMemoryLength() const {
  if (isShared()) {
    SharedArrayRawBuffer::Lock lock(sharedArrayRawBuffer());
    return sharedArrayRawBuffer()->byteLength(lock);
  }
  return buffer().byteLength();
}

bool WasmMemoryObject::isShared() const {
  return buffer().is<SharedArrayBufferObject>();
}

bool WasmMemoryObject::hasObservers() const {
  return !getReservedSlot(OBSERVERS_SLOT).isUndefined();
}

WasmMemoryObject::InstanceSet& WasmMemoryObject::observers() const {
  MOZ_ASSERT(hasObservers());
  return *reinterpret_cast<InstanceSet*>(
      getReservedSlot(OBSERVERS_SLOT).toPrivate());
}

WasmMemoryObject::InstanceSet* WasmMemoryObject::getOrCreateObservers(
    JSContext* cx) {
  if (!hasObservers()) {
    auto observers = MakeUnique<InstanceSet>(cx->zone());
    if (!observers) {
      ReportOutOfMemory(cx);
      return nullptr;
    }

    setReservedSlot(OBSERVERS_SLOT, PrivateValue(observers.release()));
  }

  return &observers();
}

bool WasmMemoryObject::movingGrowable() const {
#ifdef WASM_HUGE_MEMORY
  return false;
#else
  return !buffer().wasmMaxSize();
#endif
}

bool WasmMemoryObject::addMovingGrowObserver(JSContext* cx,
                                             WasmInstanceObject* instance) {
  MOZ_ASSERT(movingGrowable());

  InstanceSet* observers = getOrCreateObservers(cx);
  if (!observers) {
    return false;
  }

  if (!observers->putNew(instance)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

/* static */ uint32_t WasmMemoryObject::growShared(
    HandleWasmMemoryObject memory, uint32_t delta) {
  SharedArrayRawBuffer* rawBuf = memory->sharedArrayRawBuffer();
  SharedArrayRawBuffer::Lock lock(rawBuf);

  MOZ_ASSERT(rawBuf->byteLength(lock) % PageSize == 0);
  uint32_t oldNumPages = rawBuf->byteLength(lock) / PageSize;

  CheckedInt<uint32_t> newSize = oldNumPages;
  newSize += delta;
  newSize *= PageSize;
  if (!newSize.isValid()) {
    return -1;
  }

  if (newSize.value() > rawBuf->maxSize()) {
    return -1;
  }

  if (!rawBuf->wasmGrowToSizeInPlace(lock, newSize.value())) {
    return -1;
  }

  // New buffer objects will be created lazily in all agents (including in
  // this agent) by bufferGetterImpl, above, so no more work to do here.

  return oldNumPages;
}

/* static */ uint32_t WasmMemoryObject::grow(HandleWasmMemoryObject memory,
                                             uint32_t delta, JSContext* cx) {
  if (memory->isShared()) {
    return growShared(memory, delta);
  }

  RootedArrayBufferObject oldBuf(cx, &memory->buffer().as<ArrayBufferObject>());

  MOZ_ASSERT(oldBuf->byteLength() % PageSize == 0);
  uint32_t oldNumPages = oldBuf->byteLength() / PageSize;

  CheckedInt<uint32_t> newSize = oldNumPages;
  newSize += delta;
  newSize *= PageSize;
  if (!newSize.isValid()) {
    return -1;
  }

  RootedArrayBufferObject newBuf(cx);
  uint8_t* prevMemoryBase = nullptr;

  if (Maybe<uint32_t> maxSize = oldBuf->wasmMaxSize()) {
    if (newSize.value() > maxSize.value()) {
      return -1;
    }

    if (!ArrayBufferObject::wasmGrowToSizeInPlace(newSize.value(), oldBuf,
                                                  &newBuf, cx)) {
      return -1;
    }
  } else {
#ifdef WASM_HUGE_MEMORY
    if (!ArrayBufferObject::wasmGrowToSizeInPlace(newSize.value(), oldBuf,
                                                  &newBuf, cx)) {
      return -1;
    }
#else
    MOZ_ASSERT(memory->movingGrowable());
    prevMemoryBase = oldBuf->dataPointer();
    if (!ArrayBufferObject::wasmMovingGrowToSize(newSize.value(), oldBuf,
                                                 &newBuf, cx)) {
      return -1;
    }
#endif
  }

  memory->setReservedSlot(BUFFER_SLOT, ObjectValue(*newBuf));

  // Only notify moving-grow-observers after the BUFFER_SLOT has been updated
  // since observers will call buffer().
  if (memory->hasObservers()) {
    MOZ_ASSERT(prevMemoryBase);
    for (InstanceSet::Range r = memory->observers().all(); !r.empty();
         r.popFront()) {
      r.front()->instance().onMovingGrowMemory(prevMemoryBase);
    }
  }

  return oldNumPages;
}

bool js::wasm::IsSharedWasmMemoryObject(JSObject* obj) {
  obj = CheckedUnwrap(obj);
  return obj && obj->is<WasmMemoryObject>() &&
         obj->as<WasmMemoryObject>().isShared();
}

// ============================================================================
// WebAssembly.Table class and methods

const ClassOps WasmTableObject::classOps_ = {nullptr, /* addProperty */
                                             nullptr, /* delProperty */
                                             nullptr, /* enumerate */
                                             nullptr, /* newEnumerate */
                                             nullptr, /* resolve */
                                             nullptr, /* mayResolve */
                                             WasmTableObject::finalize,
                                             nullptr, /* call */
                                             nullptr, /* hasInstance */
                                             nullptr, /* construct */
                                             WasmTableObject::trace};

const Class WasmTableObject::class_ = {
    "WebAssembly.Table",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(WasmTableObject::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &WasmTableObject::classOps_};

bool WasmTableObject::isNewborn() const {
  MOZ_ASSERT(is<WasmTableObject>());
  return getReservedSlot(TABLE_SLOT).isUndefined();
}

/* static */ void WasmTableObject::finalize(FreeOp* fop, JSObject* obj) {
  WasmTableObject& tableObj = obj->as<WasmTableObject>();
  if (!tableObj.isNewborn()) {
    tableObj.table().Release();
  }
}

/* static */ void WasmTableObject::trace(JSTracer* trc, JSObject* obj) {
  WasmTableObject& tableObj = obj->as<WasmTableObject>();
  if (!tableObj.isNewborn()) {
    tableObj.table().tracePrivate(trc);
  }
}

/* static */ WasmTableObject* WasmTableObject::create(JSContext* cx,
                                                      const Limits& limits,
                                                      TableKind tableKind) {
  RootedObject proto(cx,
                     &cx->global()->getPrototype(JSProto_WasmTable).toObject());

  AutoSetNewObjectMetadata metadata(cx);
  RootedWasmTableObject obj(
      cx, NewObjectWithGivenProto<WasmTableObject>(cx, proto));
  if (!obj) {
    return nullptr;
  }

  MOZ_ASSERT(obj->isNewborn());

  TableDesc td(tableKind, limits, /*importedOrExported=*/true);

  SharedTable table = Table::create(cx, td, obj);
  if (!table) {
    return nullptr;
  }

  obj->initReservedSlot(TABLE_SLOT, PrivateValue(table.forget().take()));

  MOZ_ASSERT(!obj->isNewborn());
  return obj;
}

/* static */ bool WasmTableObject::construct(JSContext* cx, unsigned argc,
                                             Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Table")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Table", 1)) {
    return false;
  }

  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_DESC_ARG, "table");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());

  JSAtom* elementAtom = Atomize(cx, "element", strlen("element"));
  if (!elementAtom) {
    return false;
  }
  RootedId elementId(cx, AtomToId(elementAtom));

  RootedValue elementVal(cx);
  if (!GetProperty(cx, obj, obj, elementId, &elementVal)) {
    return false;
  }

  RootedString elementStr(cx, ToString(cx, elementVal));
  if (!elementStr) {
    return false;
  }

  RootedLinearString elementLinearStr(cx, elementStr->ensureLinear(cx));
  if (!elementLinearStr) {
    return false;
  }

  TableKind tableKind;
  if (StringEqualsAscii(elementLinearStr, "anyfunc")) {
    tableKind = TableKind::AnyFunction;
#ifdef ENABLE_WASM_GENERALIZED_TABLES
  } else if (StringEqualsAscii(elementLinearStr, "anyref")) {
    if (!cx->options().wasmGc()) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_BAD_ELEMENT);
      return false;
    }
    tableKind = TableKind::AnyRef;
#endif
  } else {
#ifdef ENABLE_WASM_GENERALIZED_TABLES
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_ELEMENT_GENERALIZED);
#else
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_ELEMENT);
#endif
    return false;
  }

  Limits limits;
  if (!GetLimits(cx, obj, MaxTableInitialLength, MaxTableMaximumLength, "Table",
                 &limits, Shareable::False)) {
    return false;
  }

  RootedWasmTableObject table(cx,
                              WasmTableObject::create(cx, limits, tableKind));
  if (!table) {
    return false;
  }

  args.rval().setObject(*table);
  return true;
}

static bool IsTable(HandleValue v) {
  return v.isObject() && v.toObject().is<WasmTableObject>();
}

/* static */ bool WasmTableObject::lengthGetterImpl(JSContext* cx,
                                                    const CallArgs& args) {
  args.rval().setNumber(
      args.thisv().toObject().as<WasmTableObject>().table().length());
  return true;
}

/* static */ bool WasmTableObject::lengthGetter(JSContext* cx, unsigned argc,
                                                Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTable, lengthGetterImpl>(cx, args);
}

const JSPropertySpec WasmTableObject::properties[] = {
    JS_PSG("length", WasmTableObject::lengthGetter, JSPROP_ENUMERATE),
    JS_PS_END};

static bool ToTableIndex(JSContext* cx, HandleValue v, const Table& table,
                         const char* noun, uint32_t* index) {
  if (!EnforceRangeU32(cx, v, "Table", noun, index)) {
    return false;
  }

  if (*index >= table.length()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_BAD_RANGE, "Table", noun);
    return false;
  }

  return true;
}

/* static */ bool WasmTableObject::getImpl(JSContext* cx,
                                           const CallArgs& args) {
  RootedWasmTableObject tableObj(
      cx, &args.thisv().toObject().as<WasmTableObject>());
  const Table& table = tableObj->table();

  if (!args.requireAtLeast(cx, "WebAssembly.Table.get", 1)) {
    return false;
  }

  uint32_t index;
  if (!ToTableIndex(cx, args.get(0), table, "get index", &index)) {
    return false;
  }

  switch (table.kind()) {
    case TableKind::AnyFunction: {
      const FunctionTableElem& elem = table.getAnyFunc(index);
      if (!elem.code) {
        args.rval().setNull();
        return true;
      }

      Instance& instance = *elem.tls->instance;
      const CodeRange& codeRange = *instance.code().lookupFuncRange(elem.code);

      RootedWasmInstanceObject instanceObj(cx, instance.object());
      RootedFunction fun(cx);
      if (!instanceObj->getExportedFunction(cx, instanceObj,
                                            codeRange.funcIndex(), &fun)) {
        return false;
      }

      args.rval().setObject(*fun);
      break;
    }
    case TableKind::AnyRef: {
      args.rval().setObjectOrNull(table.getAnyRef(index));
      break;
    }
    default: { MOZ_CRASH("Unexpected table kind"); }
  }
  return true;
}

/* static */ bool WasmTableObject::get(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTable, getImpl>(cx, args);
}

/* static */ bool WasmTableObject::setImpl(JSContext* cx,
                                           const CallArgs& args) {
  RootedWasmTableObject tableObj(
      cx, &args.thisv().toObject().as<WasmTableObject>());
  Table& table = tableObj->table();

  if (!args.requireAtLeast(cx, "WebAssembly.Table.set", 2)) {
    return false;
  }

  uint32_t index;
  if (!ToTableIndex(cx, args.get(0), table, "set index", &index)) {
    return false;
  }

  switch (table.kind()) {
    case TableKind::AnyFunction: {
      RootedFunction value(cx);
      if (!IsExportedFunction(args[1], &value) && !args[1].isNull()) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_WASM_BAD_TABLE_VALUE);
        return false;
      }

      if (value) {
        RootedWasmInstanceObject instanceObj(
            cx, ExportedFunctionToInstanceObject(value));
        uint32_t funcIndex = ExportedFunctionToFuncIndex(value);

#ifdef DEBUG
        RootedFunction f(cx);
        MOZ_ASSERT(
            instanceObj->getExportedFunction(cx, instanceObj, funcIndex, &f));
        MOZ_ASSERT(value == f);
#endif

        Instance& instance = instanceObj->instance();
        Tier tier = instance.code().bestTier();
        const MetadataTier& metadata = instance.metadata(tier);
        const CodeRange& codeRange =
            metadata.codeRange(metadata.lookupFuncExport(funcIndex));
        void* code = instance.codeBase(tier) + codeRange.funcTableEntry();
        table.setAnyFunc(index, code, &instance);
      } else {
        table.setNull(index);
      }
      break;
    }
    case TableKind::AnyRef: {
      if (args[1].isNull()) {
        table.setNull(index);
      } else {
        RootedObject value(cx, ToObject(cx, args[1]));
        if (!value) {
          return false;
        }
        table.setAnyRef(index, value.get());
      }
      break;
    }
    default: { MOZ_CRASH("Unexpected table kind"); }
  }

  args.rval().setUndefined();
  return true;
}

/* static */ bool WasmTableObject::set(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTable, setImpl>(cx, args);
}

/* static */ bool WasmTableObject::growImpl(JSContext* cx,
                                            const CallArgs& args) {
  RootedWasmTableObject table(cx,
                              &args.thisv().toObject().as<WasmTableObject>());

  if (!args.requireAtLeast(cx, "WebAssembly.Table.grow", 1)) {
    return false;
  }

  uint32_t delta;
  if (!EnforceRangeU32(cx, args.get(0), "Table", "grow delta", &delta)) {
    return false;
  }

  uint32_t ret = table->table().grow(delta, cx);

  if (ret == uint32_t(-1)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_GROW,
                             "table");
    return false;
  }

  args.rval().setInt32(ret);
  return true;
}

/* static */ bool WasmTableObject::grow(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTable, growImpl>(cx, args);
}

const JSFunctionSpec WasmTableObject::methods[] = {
    JS_FN("get", WasmTableObject::get, 1, JSPROP_ENUMERATE),
    JS_FN("set", WasmTableObject::set, 2, JSPROP_ENUMERATE),
    JS_FN("grow", WasmTableObject::grow, 1, JSPROP_ENUMERATE), JS_FS_END};

const JSFunctionSpec WasmTableObject::static_methods[] = {JS_FS_END};

Table& WasmTableObject::table() const {
  return *(Table*)getReservedSlot(TABLE_SLOT).toPrivate();
}

// ============================================================================
// WebAssembly.global class and methods

const ClassOps WasmGlobalObject::classOps_ = {nullptr, /* addProperty */
                                              nullptr, /* delProperty */
                                              nullptr, /* enumerate */
                                              nullptr, /* newEnumerate */
                                              nullptr, /* resolve */
                                              nullptr, /* mayResolve */
                                              WasmGlobalObject::finalize,
                                              nullptr, /* call */
                                              nullptr, /* hasInstance */
                                              nullptr, /* construct */
                                              WasmGlobalObject::trace};

const Class WasmGlobalObject::class_ = {
    "WebAssembly.Global",
    JSCLASS_HAS_RESERVED_SLOTS(WasmGlobalObject::RESERVED_SLOTS) |
        JSCLASS_BACKGROUND_FINALIZE,
    &WasmGlobalObject::classOps_};

/* static */ void WasmGlobalObject::trace(JSTracer* trc, JSObject* obj) {
  WasmGlobalObject* global = reinterpret_cast<WasmGlobalObject*>(obj);
  if (global->isNewborn()) {
    // This can happen while we're allocating the object, in which case
    // every single slot of the object is not defined yet. In particular,
    // there's nothing to trace yet.
    return;
  }
  switch (global->type().code()) {
    case ValType::AnyRef:
      if (global->cell()->ptr) {
        TraceManuallyBarrieredEdge(trc, &global->cell()->ptr,
                                   "wasm anyref global");
      }
      break;
    case ValType::I32:
    case ValType::F32:
    case ValType::I64:
    case ValType::F64:
      break;
    case ValType::Ref:
      MOZ_CRASH("Ref NYI");
    case ValType::NullRef:
      MOZ_CRASH("NullRef not expressible");
  }
}

/* static */ void WasmGlobalObject::finalize(FreeOp*, JSObject* obj) {
  WasmGlobalObject* global = reinterpret_cast<WasmGlobalObject*>(obj);
  if (!global->isNewborn()) {
    js_delete(global->cell());
  }
}

/* static */ WasmGlobalObject* WasmGlobalObject::create(JSContext* cx,
                                                        HandleVal hval,
                                                        bool isMutable) {
  RootedObject proto(
      cx, &cx->global()->getPrototype(JSProto_WasmGlobal).toObject());

  AutoSetNewObjectMetadata metadata(cx);
  RootedWasmGlobalObject obj(
      cx, NewObjectWithGivenProto<WasmGlobalObject>(cx, proto));
  if (!obj) {
    return nullptr;
  }

  MOZ_ASSERT(obj->isNewborn());
  MOZ_ASSERT(obj->isTenured(), "assumed by set_global post barriers");

  // It's simpler to initialize the cell after the object has been created,
  // to avoid needing to root the cell before the object creation.

  Cell* cell = js_new<Cell>();
  if (!cell) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  const Val& val = hval.get();
  switch (val.type().code()) {
    case ValType::I32:
      cell->i32 = val.i32();
      break;
    case ValType::I64:
      cell->i64 = val.i64();
      break;
    case ValType::F32:
      cell->f32 = val.f32();
      break;
    case ValType::F64:
      cell->f64 = val.f64();
      break;
    case ValType::NullRef:
      MOZ_ASSERT(!cell->ptr, "value should be null already");
      break;
    case ValType::AnyRef:
      MOZ_ASSERT(!cell->ptr, "no prebarriers needed");
      cell->ptr = val.ptr();
      if (cell->ptr) {
        JSObject::writeBarrierPost(&cell->ptr, nullptr, cell->ptr);
      }
      break;
    case ValType::Ref:
      MOZ_CRASH("Ref NYI");
  }

  obj->initReservedSlot(TYPE_SLOT,
                        Int32Value(int32_t(val.type().bitsUnsafe())));
  obj->initReservedSlot(MUTABLE_SLOT, JS::BooleanValue(isMutable));
  obj->initReservedSlot(CELL_SLOT, PrivateValue(cell));

  MOZ_ASSERT(!obj->isNewborn());

  return obj;
}

/* static */ bool WasmGlobalObject::construct(JSContext* cx, unsigned argc,
                                              Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Global")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Global", 1)) {
    return false;
  }

  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_DESC_ARG, "global");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());

  // Extract properties in lexicographic order per spec.

  RootedValue mutableVal(cx);
  if (!JS_GetProperty(cx, obj, "mutable", &mutableVal)) {
    return false;
  }

  RootedValue typeVal(cx);
  if (!JS_GetProperty(cx, obj, "value", &typeVal)) {
    return false;
  }

  RootedString typeStr(cx, ToString(cx, typeVal));
  if (!typeStr) {
    return false;
  }

  RootedLinearString typeLinearStr(cx, typeStr->ensureLinear(cx));
  if (!typeLinearStr) {
    return false;
  }

  ValType globalType;
  if (StringEqualsAscii(typeLinearStr, "i32")) {
    globalType = ValType::I32;
  } else if (args.length() == 1 && StringEqualsAscii(typeLinearStr, "i64")) {
    // For the time being, i64 is allowed only if there is not an
    // initializing value.
    globalType = ValType::I64;
  } else if (StringEqualsAscii(typeLinearStr, "f32")) {
    globalType = ValType::F32;
  } else if (StringEqualsAscii(typeLinearStr, "f64")) {
    globalType = ValType::F64;
#ifdef ENABLE_WASM_GC
  } else if (cx->options().wasmGc() &&
             StringEqualsAscii(typeLinearStr, "anyref")) {
    globalType = ValType::AnyRef;
#endif
  } else {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_GLOBAL_TYPE);
    return false;
  }

  bool isMutable = ToBoolean(mutableVal);

  // Extract the initial value, or provide a suitable default.
  RootedVal globalVal(cx);

  // Initialize with default value.
  switch (globalType.code()) {
    case ValType::I32:
      globalVal = Val(uint32_t(0));
      break;
    case ValType::I64:
      globalVal = Val(uint64_t(0));
      break;
    case ValType::F32:
      globalVal = Val(float(0.0));
      break;
    case ValType::F64:
      globalVal = Val(double(0.0));
      break;
    case ValType::AnyRef:
      globalVal = Val(ValType::AnyRef, nullptr);
      break;
    case ValType::Ref:
      MOZ_CRASH("Ref NYI");
    case ValType::NullRef:
      MOZ_CRASH("NullRef not expressible");
  }

  // Override with non-undefined value, if provided.
  RootedValue valueVal(cx, args.get(1));
  if (!valueVal.isUndefined()) {
    if (!ToWebAssemblyValue(cx, globalType, valueVal, &globalVal)) {
      return false;
    }
  }

  WasmGlobalObject* global = WasmGlobalObject::create(cx, globalVal, isMutable);
  if (!global) {
    return false;
  }

  args.rval().setObject(*global);
  return true;
}

static bool IsGlobal(HandleValue v) {
  return v.isObject() && v.toObject().is<WasmGlobalObject>();
}

/* static */ bool WasmGlobalObject::valueGetterImpl(JSContext* cx,
                                                    const CallArgs& args) {
  switch (args.thisv().toObject().as<WasmGlobalObject>().type().code()) {
    case ValType::I32:
    case ValType::F32:
    case ValType::F64:
    case ValType::AnyRef:
      args.rval().set(args.thisv().toObject().as<WasmGlobalObject>().value(cx));
      return true;
    case ValType::I64:
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_BAD_I64_TYPE);
      return false;
    case ValType::Ref:
      MOZ_CRASH("Ref NYI");
    case ValType::NullRef:
      MOZ_CRASH("NullRef not expressible");
  }
  MOZ_CRASH();
}

/* static */ bool WasmGlobalObject::valueGetter(JSContext* cx, unsigned argc,
                                                Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsGlobal, valueGetterImpl>(cx, args);
}

/* static */ bool WasmGlobalObject::valueSetterImpl(JSContext* cx,
                                                    const CallArgs& args) {
  if (!args.requireAtLeast(cx, "WebAssembly.Global setter", 1)) {
    return false;
  }

  RootedWasmGlobalObject global(
      cx, &args.thisv().toObject().as<WasmGlobalObject>());
  if (!global->isMutable()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_GLOBAL_IMMUTABLE);
    return false;
  }

  if (global->type() == ValType::I64) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_I64_TYPE);
    return false;
  }

  RootedVal val(cx);
  if (!ToWebAssemblyValue(cx, global->type(), args.get(0), &val)) {
    return false;
  }

  Cell* cell = global->cell();
  switch (global->type().code()) {
    case ValType::I32:
      cell->i32 = val.get().i32();
      break;
    case ValType::F32:
      cell->f32 = val.get().f32();
      break;
    case ValType::F64:
      cell->f64 = val.get().f64();
      break;
    case ValType::AnyRef: {
      JSObject* prevPtr = cell->ptr;
      JSObject::writeBarrierPre(prevPtr);
      cell->ptr = val.get().ptr();
      if (cell->ptr) {
        JSObject::writeBarrierPost(&cell->ptr, prevPtr, cell->ptr);
      }
      break;
    }
    case ValType::I64:
      MOZ_CRASH("unexpected i64 when setting global's value");
    case ValType::Ref:
      MOZ_CRASH("Ref NYI");
    case ValType::NullRef:
      MOZ_CRASH("NullRef not expressible");
  }

  args.rval().setUndefined();
  return true;
}

/* static */ bool WasmGlobalObject::valueSetter(JSContext* cx, unsigned argc,
                                                Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsGlobal, valueSetterImpl>(cx, args);
}

const JSPropertySpec WasmGlobalObject::properties[] = {
    JS_PSGS("value", WasmGlobalObject::valueGetter,
            WasmGlobalObject::valueSetter, JSPROP_ENUMERATE),
    JS_PS_END};

const JSFunctionSpec WasmGlobalObject::methods[] = {
    JS_FN(js_valueOf_str, WasmGlobalObject::valueGetter, 0, JSPROP_ENUMERATE),
    JS_FS_END};

const JSFunctionSpec WasmGlobalObject::static_methods[] = {JS_FS_END};

ValType WasmGlobalObject::type() const {
  return ValType::fromBitsUnsafe(getReservedSlot(TYPE_SLOT).toInt32());
}

bool WasmGlobalObject::isMutable() const {
  return getReservedSlot(MUTABLE_SLOT).toBoolean();
}

void WasmGlobalObject::val(MutableHandleVal outval) const {
  Cell* cell = this->cell();
  switch (type().code()) {
    case ValType::I32:
      outval.set(Val(uint32_t(cell->i32)));
      return;
    case ValType::I64:
      outval.set(Val(uint64_t(cell->i64)));
      return;
    case ValType::F32:
      outval.set(Val(cell->f32));
      return;
    case ValType::F64:
      outval.set(Val(cell->f64));
      return;
    case ValType::AnyRef:
      outval.set(Val(ValType::AnyRef, cell->ptr));
      return;
    case ValType::Ref:
      MOZ_CRASH("Ref NYI");
    case ValType::NullRef:
      MOZ_CRASH("NullRef not expressible");
  }
  MOZ_CRASH("unexpected Global type");
}

Value WasmGlobalObject::value(JSContext* cx) const {
  // ToJSValue crashes on I64; this is desirable.
  RootedVal result(cx);
  val(&result);
  return ToJSValue(result.get());
}

WasmGlobalObject::Cell* WasmGlobalObject::cell() const {
  return reinterpret_cast<Cell*>(getReservedSlot(CELL_SLOT).toPrivate());
}

// ============================================================================
// WebAssembly class and static methods

static bool WebAssembly_toSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(cx->names().WebAssembly);
  return true;
}

static bool RejectWithPendingException(JSContext* cx,
                                       Handle<PromiseObject*> promise) {
  if (!cx->isExceptionPending()) {
    return false;
  }

  RootedValue rejectionValue(cx);
  if (!GetAndClearException(cx, &rejectionValue)) {
    return false;
  }

  return PromiseObject::reject(cx, promise, rejectionValue);
}

static bool Reject(JSContext* cx, const CompileArgs& args,
                   Handle<PromiseObject*> promise, const UniqueChars& error) {
  if (!error) {
    ReportOutOfMemory(cx);
    return RejectWithPendingException(cx, promise);
  }

  RootedObject stack(cx, promise->allocationSite());
  RootedString filename(
      cx, JS_NewStringCopyZ(cx, args.scriptedCaller.filename.get()));
  if (!filename) {
    return false;
  }

  unsigned line = args.scriptedCaller.line;

  // Ideally we'd report a JSMSG_WASM_COMPILE_ERROR here, but there's no easy
  // way to create an ErrorObject for an arbitrary error code with multiple
  // replacements.
  UniqueChars str(JS_smprintf("wasm validation error: %s", error.get()));
  if (!str) {
    return false;
  }

  RootedString message(cx, NewLatin1StringZ(cx, std::move(str)));
  if (!message) {
    return false;
  }

  RootedObject errorObj(
      cx, ErrorObject::create(cx, JSEXN_WASMCOMPILEERROR, stack, filename, line,
                              0, nullptr, message));
  if (!errorObj) {
    return false;
  }

  RootedValue rejectionValue(cx, ObjectValue(*errorObj));
  return PromiseObject::reject(cx, promise, rejectionValue);
}

static bool Resolve(JSContext* cx, const Module& module,
                    Handle<PromiseObject*> promise, bool instantiate,
                    HandleObject importObj, const UniqueCharsVector& warnings) {
  if (!ReportCompileWarnings(cx, warnings)) {
    return false;
  }

  RootedObject proto(
      cx, &cx->global()->getPrototype(JSProto_WasmModule).toObject());
  RootedObject moduleObj(cx, WasmModuleObject::create(cx, module, proto));
  if (!moduleObj) {
    return RejectWithPendingException(cx, promise);
  }

  RootedValue resolutionValue(cx);
  if (instantiate) {
    RootedWasmInstanceObject instanceObj(cx);
    if (!Instantiate(cx, module, importObj, &instanceObj)) {
      return RejectWithPendingException(cx, promise);
    }

    RootedObject resultObj(cx, JS_NewPlainObject(cx));
    if (!resultObj) {
      return RejectWithPendingException(cx, promise);
    }

    RootedValue val(cx, ObjectValue(*moduleObj));
    if (!JS_DefineProperty(cx, resultObj, "module", val, JSPROP_ENUMERATE)) {
      return RejectWithPendingException(cx, promise);
    }

    val = ObjectValue(*instanceObj);
    if (!JS_DefineProperty(cx, resultObj, "instance", val, JSPROP_ENUMERATE)) {
      return RejectWithPendingException(cx, promise);
    }

    resolutionValue = ObjectValue(*resultObj);
  } else {
    MOZ_ASSERT(!importObj);
    resolutionValue = ObjectValue(*moduleObj);
  }

  if (!PromiseObject::resolve(cx, promise, resolutionValue)) {
    return RejectWithPendingException(cx, promise);
  }

  return true;
}

struct CompileBufferTask : PromiseHelperTask {
  MutableBytes bytecode;
  SharedCompileArgs compileArgs;
  UniqueChars error;
  UniqueCharsVector warnings;
  SharedModule module;
  bool instantiate;
  PersistentRootedObject importObj;

  CompileBufferTask(JSContext* cx, Handle<PromiseObject*> promise,
                    HandleObject importObj)
      : PromiseHelperTask(cx, promise),
        instantiate(true),
        importObj(cx, importObj) {}

  CompileBufferTask(JSContext* cx, Handle<PromiseObject*> promise)
      : PromiseHelperTask(cx, promise), instantiate(false) {}

  bool init(JSContext* cx, const char* introducer) {
    compileArgs = InitCompileArgs(cx, introducer);
    if (!compileArgs) {
      return false;
    }
    return PromiseHelperTask::init(cx);
  }

  void execute() override {
    module = CompileBuffer(*compileArgs, *bytecode, &error, &warnings);
  }

  bool resolve(JSContext* cx, Handle<PromiseObject*> promise) override {
    return module
               ? Resolve(cx, *module, promise, instantiate, importObj, warnings)
               : Reject(cx, *compileArgs, promise, error);
  }
};

static bool RejectWithPendingException(JSContext* cx,
                                       Handle<PromiseObject*> promise,
                                       CallArgs& callArgs) {
  if (!RejectWithPendingException(cx, promise)) {
    return false;
  }

  callArgs.rval().setObject(*promise);
  return true;
}

static bool EnsurePromiseSupport(JSContext* cx) {
  if (!cx->runtime()->offThreadPromiseState.ref().initialized()) {
    JS_ReportErrorASCII(
        cx, "WebAssembly Promise APIs not supported in this runtime.");
    return false;
  }
  return true;
}

static bool GetBufferSource(JSContext* cx, CallArgs callArgs, const char* name,
                            MutableBytes* bytecode) {
  if (!callArgs.requireAtLeast(cx, name, 1)) {
    return false;
  }

  if (!callArgs[0].isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_BUF_ARG);
    return false;
  }

  return GetBufferSource(cx, &callArgs[0].toObject(), JSMSG_WASM_BAD_BUF_ARG,
                         bytecode);
}

static bool WebAssembly_compile(JSContext* cx, unsigned argc, Value* vp) {
  if (!EnsurePromiseSupport(cx)) {
    return false;
  }

  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return false;
  }

  auto task = cx->make_unique<CompileBufferTask>(cx, promise);
  if (!task || !task->init(cx, "WebAssembly.compile")) {
    return false;
  }

  CallArgs callArgs = CallArgsFromVp(argc, vp);

  if (!GetBufferSource(cx, callArgs, "WebAssembly.compile", &task->bytecode)) {
    return RejectWithPendingException(cx, promise, callArgs);
  }

  if (!StartOffThreadPromiseHelperTask(cx, std::move(task))) {
    return false;
  }

  callArgs.rval().setObject(*promise);
  return true;
}

static bool GetInstantiateArgs(JSContext* cx, CallArgs callArgs,
                               MutableHandleObject firstArg,
                               MutableHandleObject importObj) {
  if (!callArgs.requireAtLeast(cx, "WebAssembly.instantiate", 1)) {
    return false;
  }

  if (!callArgs[0].isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_BUF_MOD_ARG);
    return false;
  }

  firstArg.set(&callArgs[0].toObject());

  return GetImportArg(cx, callArgs, importObj);
}

static bool WebAssembly_instantiate(JSContext* cx, unsigned argc, Value* vp) {
  if (!EnsurePromiseSupport(cx)) {
    return false;
  }

  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return false;
  }

  CallArgs callArgs = CallArgsFromVp(argc, vp);

  RootedObject firstArg(cx);
  RootedObject importObj(cx);
  if (!GetInstantiateArgs(cx, callArgs, &firstArg, &importObj)) {
    return RejectWithPendingException(cx, promise, callArgs);
  }

  const Module* module;
  if (IsModuleObject(firstArg, &module)) {
    RootedWasmInstanceObject instanceObj(cx);
    if (!Instantiate(cx, *module, importObj, &instanceObj)) {
      return RejectWithPendingException(cx, promise, callArgs);
    }

    RootedValue resolutionValue(cx, ObjectValue(*instanceObj));
    if (!PromiseObject::resolve(cx, promise, resolutionValue)) {
      return false;
    }
  } else {
    auto task = cx->make_unique<CompileBufferTask>(cx, promise, importObj);
    if (!task || !task->init(cx, "WebAssembly.instantiate")) {
      return false;
    }

    if (!GetBufferSource(cx, firstArg, JSMSG_WASM_BAD_BUF_MOD_ARG,
                         &task->bytecode)) {
      return RejectWithPendingException(cx, promise, callArgs);
    }

    if (!StartOffThreadPromiseHelperTask(cx, std::move(task))) {
      return false;
    }
  }

  callArgs.rval().setObject(*promise);
  return true;
}

static bool WebAssembly_validate(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs callArgs = CallArgsFromVp(argc, vp);

  MutableBytes bytecode;
  if (!GetBufferSource(cx, callArgs, "WebAssembly.validate", &bytecode)) {
    return false;
  }

  UniqueChars error;
  bool validated = Validate(cx, *bytecode, &error);

  // If the reason for validation failure was OOM (signalled by null error
  // message), report out-of-memory so that validate's return is always
  // correct.
  if (!validated && !error) {
    ReportOutOfMemory(cx);
    return false;
  }

  callArgs.rval().setBoolean(validated);
  return true;
}

static bool EnsureStreamSupport(JSContext* cx) {
  // This should match wasm::HasStreamingSupport().

  if (!EnsurePromiseSupport(cx)) {
    return false;
  }

  if (!CanUseExtraThreads()) {
    JS_ReportErrorASCII(
        cx, "WebAssembly.compileStreaming not supported with --no-threads");
    return false;
  }

  if (!cx->runtime()->consumeStreamCallback) {
    JS_ReportErrorASCII(cx,
                        "WebAssembly streaming not supported in this runtime");
    return false;
  }

  return true;
}

// This value is chosen and asserted to be disjoint from any host error code.
static const size_t StreamOOMCode = 0;

static bool RejectWithStreamErrorNumber(JSContext* cx, size_t errorCode,
                                        Handle<PromiseObject*> promise) {
  if (errorCode == StreamOOMCode) {
    ReportOutOfMemory(cx);
    return false;
  }

  cx->runtime()->reportStreamErrorCallback(cx, errorCode);
  return RejectWithPendingException(cx, promise);
}

class CompileStreamTask : public PromiseHelperTask, public JS::StreamConsumer {
  enum StreamState { Env, Code, Tail, Closed };
  typedef ExclusiveWaitableData<StreamState> ExclusiveStreamState;

  // Immutable:
  const MutableCompileArgs compileArgs_;  // immutable during streaming
  const bool instantiate_;
  const PersistentRootedObject importObj_;

  // Mutated on a stream thread (consumeChunk(), streamEnd(), streamError()):
  ExclusiveStreamState streamState_;
  Bytes envBytes_;            // immutable after Env state
  SectionRange codeSection_;  // immutable after Env state
  Bytes codeBytes_;           // not resized after Env state
  uint8_t* codeBytesEnd_;
  ExclusiveBytesPtr exclusiveCodeBytesEnd_;
  Bytes tailBytes_;  // immutable after Tail state
  ExclusiveStreamEndData exclusiveStreamEnd_;
  Maybe<size_t> streamError_;
  Atomic<bool> streamFailed_;
  Tier2Listener tier2Listener_;

  // Mutated on helper thread (execute()):
  SharedModule module_;
  UniqueChars compileError_;
  UniqueCharsVector warnings_;

  // Called on some thread before consumeChunk(), streamEnd(), streamError()):

  void noteResponseURLs(const char* url, const char* sourceMapUrl) override {
    if (url) {
      compileArgs_->scriptedCaller.filename = DuplicateString(url);
      compileArgs_->scriptedCaller.filenameIsURL = true;
    }
    if (sourceMapUrl) {
      compileArgs_->sourceMapURL = DuplicateString(sourceMapUrl);
    }
  }

  // Called on a stream thread:

  // Until StartOffThreadPromiseHelperTask succeeds, we are responsible for
  // dispatching ourselves back to the JS thread.
  //
  // Warning: After this function returns, 'this' can be deleted at any time, so
  // the caller must immediately return from the stream callback.
  void setClosedAndDestroyBeforeHelperThreadStarted() {
    streamState_.lock().get() = Closed;
    dispatchResolveAndDestroy();
  }

  // See setClosedAndDestroyBeforeHelperThreadStarted() comment.
  bool rejectAndDestroyBeforeHelperThreadStarted(size_t errorNumber) {
    MOZ_ASSERT(streamState_.lock() == Env);
    MOZ_ASSERT(!streamError_);
    streamError_ = Some(errorNumber);
    setClosedAndDestroyBeforeHelperThreadStarted();
    return false;
  }

  // Once StartOffThreadPromiseHelperTask succeeds, the helper thread will
  // dispatchResolveAndDestroy() after execute() returns, but execute()
  // wait()s for state to be Closed.
  //
  // Warning: After this function returns, 'this' can be deleted at any time, so
  // the caller must immediately return from the stream callback.
  void setClosedAndDestroyAfterHelperThreadStarted() {
    auto streamState = streamState_.lock();
    MOZ_ASSERT(streamState != Closed);
    streamState.get() = Closed;
    streamState.notify_one(/* stream closed */);
  }

  // See setClosedAndDestroyAfterHelperThreadStarted() comment.
  bool rejectAndDestroyAfterHelperThreadStarted(size_t errorNumber) {
    MOZ_ASSERT(!streamError_);
    streamError_ = Some(errorNumber);
    streamFailed_ = true;
    exclusiveCodeBytesEnd_.lock().notify_one();
    exclusiveStreamEnd_.lock().notify_one();
    setClosedAndDestroyAfterHelperThreadStarted();
    return false;
  }

  bool consumeChunk(const uint8_t* begin, size_t length) override {
    switch (streamState_.lock().get()) {
      case Env: {
        if (!envBytes_.append(begin, length)) {
          return rejectAndDestroyBeforeHelperThreadStarted(StreamOOMCode);
        }

        if (!StartsCodeSection(envBytes_.begin(), envBytes_.end(),
                               &codeSection_)) {
          return true;
        }

        uint32_t extraBytes = envBytes_.length() - codeSection_.start;
        if (extraBytes) {
          envBytes_.shrinkTo(codeSection_.start);
        }

        if (codeSection_.size > MaxCodeSectionBytes) {
          return rejectAndDestroyBeforeHelperThreadStarted(StreamOOMCode);
        }

        if (!codeBytes_.resize(codeSection_.size)) {
          return rejectAndDestroyBeforeHelperThreadStarted(StreamOOMCode);
        }

        codeBytesEnd_ = codeBytes_.begin();
        exclusiveCodeBytesEnd_.lock().get() = codeBytesEnd_;

        if (!StartOffThreadPromiseHelperTask(this)) {
          return rejectAndDestroyBeforeHelperThreadStarted(StreamOOMCode);
        }

        // Set the state to Code iff StartOffThreadPromiseHelperTask()
        // succeeds so that the state tells us whether we are before or
        // after the helper thread started.
        streamState_.lock().get() = Code;

        if (extraBytes) {
          return consumeChunk(begin + length - extraBytes, extraBytes);
        }

        return true;
      }
      case Code: {
        size_t copyLength =
            Min<size_t>(length, codeBytes_.end() - codeBytesEnd_);
        memcpy(codeBytesEnd_, begin, copyLength);
        codeBytesEnd_ += copyLength;

        {
          auto codeStreamEnd = exclusiveCodeBytesEnd_.lock();
          codeStreamEnd.get() = codeBytesEnd_;
          codeStreamEnd.notify_one();
        }

        if (codeBytesEnd_ != codeBytes_.end()) {
          return true;
        }

        streamState_.lock().get() = Tail;

        if (uint32_t extraBytes = length - copyLength) {
          return consumeChunk(begin + copyLength, extraBytes);
        }

        return true;
      }
      case Tail: {
        if (!tailBytes_.append(begin, length)) {
          return rejectAndDestroyAfterHelperThreadStarted(StreamOOMCode);
        }

        return true;
      }
      case Closed:
        MOZ_CRASH("consumeChunk() in Closed state");
    }
    MOZ_CRASH("unreachable");
  }

  void streamEnd(JS::OptimizedEncodingListener* tier2Listener) override {
    switch (streamState_.lock().get()) {
      case Env: {
        SharedBytes bytecode = js_new<ShareableBytes>(std::move(envBytes_));
        if (!bytecode) {
          rejectAndDestroyBeforeHelperThreadStarted(StreamOOMCode);
          return;
        }
        module_ =
            CompileBuffer(*compileArgs_, *bytecode, &compileError_, &warnings_);
        setClosedAndDestroyBeforeHelperThreadStarted();
        return;
      }
      case Code:
      case Tail: {
        auto streamEnd = exclusiveStreamEnd_.lock();
        MOZ_ASSERT(!streamEnd->reached);
        streamEnd->reached = true;
        streamEnd->tailBytes = &tailBytes_;
        streamEnd->tier2Listener = tier2Listener;
        streamEnd.notify_one();
      }
        setClosedAndDestroyAfterHelperThreadStarted();
        return;
      case Closed:
        MOZ_CRASH("streamEnd() in Closed state");
    }
  }

  void streamError(size_t errorCode) override {
    MOZ_ASSERT(errorCode != StreamOOMCode);
    switch (streamState_.lock().get()) {
      case Env:
        rejectAndDestroyBeforeHelperThreadStarted(errorCode);
        return;
      case Tail:
      case Code:
        rejectAndDestroyAfterHelperThreadStarted(errorCode);
        return;
      case Closed:
        MOZ_CRASH("streamError() in Closed state");
    }
  }

  void consumeOptimizedEncoding(const uint8_t* begin, size_t length) override {
    module_ = Module::deserialize(begin, length);

    MOZ_ASSERT(streamState_.lock().get() == Env);
    setClosedAndDestroyBeforeHelperThreadStarted();
  }

  // Called on a helper thread:

  void execute() override {
    module_ = CompileStreaming(*compileArgs_, envBytes_, codeBytes_,
                               exclusiveCodeBytesEnd_, exclusiveStreamEnd_,
                               streamFailed_, &compileError_, &warnings_);

    // When execute() returns, the CompileStreamTask will be dispatched
    // back to its JS thread to call resolve() and then be destroyed. We
    // can't let this happen until the stream has been closed lest
    // consumeChunk() or streamEnd() be called on a dead object.
    auto streamState = streamState_.lock();
    while (streamState != Closed) {
      streamState.wait(/* stream closed */);
    }
  }

  // Called on a JS thread after streaming compilation completes/errors:

  bool resolve(JSContext* cx, Handle<PromiseObject*> promise) override {
    MOZ_ASSERT(streamState_.lock() == Closed);
    MOZ_ASSERT_IF(module_, !streamFailed_ && !streamError_ && !compileError_);
    return module_
               ? Resolve(cx, *module_, promise, instantiate_, importObj_,
                         warnings_)
               : streamError_
                     ? RejectWithStreamErrorNumber(cx, *streamError_, promise)
                     : Reject(cx, *compileArgs_, promise, compileError_);
  }

 public:
  CompileStreamTask(JSContext* cx, Handle<PromiseObject*> promise,
                    CompileArgs& compileArgs, bool instantiate,
                    HandleObject importObj)
      : PromiseHelperTask(cx, promise),
        compileArgs_(&compileArgs),
        instantiate_(instantiate),
        importObj_(cx, importObj),
        streamState_(mutexid::WasmStreamStatus, Env),
        codeSection_{},
        codeBytesEnd_(nullptr),
        exclusiveCodeBytesEnd_(mutexid::WasmCodeBytesEnd, nullptr),
        exclusiveStreamEnd_(mutexid::WasmStreamEnd),
        streamFailed_(false) {
    MOZ_ASSERT_IF(importObj_, instantiate_);
  }
};

// A short-lived object that captures the arguments of a
// WebAssembly.{compileStreaming,instantiateStreaming} while waiting for
// the Promise<Response> to resolve to a (hopefully) Promise.
class ResolveResponseClosure : public NativeObject {
  static const unsigned COMPILE_ARGS_SLOT = 0;
  static const unsigned PROMISE_OBJ_SLOT = 1;
  static const unsigned INSTANTIATE_SLOT = 2;
  static const unsigned IMPORT_OBJ_SLOT = 3;
  static const ClassOps classOps_;

  static void finalize(FreeOp* fop, JSObject* obj) {
    obj->as<ResolveResponseClosure>().compileArgs().Release();
  }

 public:
  static const unsigned RESERVED_SLOTS = 4;
  static const Class class_;

  static ResolveResponseClosure* create(JSContext* cx, CompileArgs& args,
                                        HandleObject promise, bool instantiate,
                                        HandleObject importObj) {
    MOZ_ASSERT_IF(importObj, instantiate);

    AutoSetNewObjectMetadata metadata(cx);
    auto* obj = NewObjectWithGivenProto<ResolveResponseClosure>(cx, nullptr);
    if (!obj) {
      return nullptr;
    }

    args.AddRef();
    obj->setReservedSlot(COMPILE_ARGS_SLOT, PrivateValue(&args));
    obj->setReservedSlot(PROMISE_OBJ_SLOT, ObjectValue(*promise));
    obj->setReservedSlot(INSTANTIATE_SLOT, BooleanValue(instantiate));
    obj->setReservedSlot(IMPORT_OBJ_SLOT, ObjectOrNullValue(importObj));
    return obj;
  }

  CompileArgs& compileArgs() const {
    return *(CompileArgs*)getReservedSlot(COMPILE_ARGS_SLOT).toPrivate();
  }
  PromiseObject& promise() const {
    return getReservedSlot(PROMISE_OBJ_SLOT).toObject().as<PromiseObject>();
  }
  bool instantiate() const {
    return getReservedSlot(INSTANTIATE_SLOT).toBoolean();
  }
  JSObject* importObj() const {
    return getReservedSlot(IMPORT_OBJ_SLOT).toObjectOrNull();
  }
};

const ClassOps ResolveResponseClosure::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate */
    nullptr, /* newEnumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    ResolveResponseClosure::finalize};

const Class ResolveResponseClosure::class_ = {
    "WebAssembly ResolveResponseClosure",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(ResolveResponseClosure::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &ResolveResponseClosure::classOps_,
};

static ResolveResponseClosure* ToResolveResponseClosure(CallArgs args) {
  return &args.callee()
              .as<JSFunction>()
              .getExtendedSlot(0)
              .toObject()
              .as<ResolveResponseClosure>();
}

static bool RejectWithErrorNumber(JSContext* cx, uint32_t errorNumber,
                                  Handle<PromiseObject*> promise) {
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, errorNumber);
  return RejectWithPendingException(cx, promise);
}

static bool ResolveResponse_OnFulfilled(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs callArgs = CallArgsFromVp(argc, vp);

  Rooted<ResolveResponseClosure*> closure(cx,
                                          ToResolveResponseClosure(callArgs));
  Rooted<PromiseObject*> promise(cx, &closure->promise());
  CompileArgs& compileArgs = closure->compileArgs();
  bool instantiate = closure->instantiate();
  Rooted<JSObject*> importObj(cx, closure->importObj());

  auto task = cx->make_unique<CompileStreamTask>(cx, promise, compileArgs,
                                                 instantiate, importObj);
  if (!task || !task->init(cx)) {
    return false;
  }

  if (!callArgs.get(0).isObject()) {
    return RejectWithErrorNumber(cx, JSMSG_BAD_RESPONSE_VALUE, promise);
  }

  RootedObject response(cx, &callArgs.get(0).toObject());
  if (!cx->runtime()->consumeStreamCallback(cx, response, JS::MimeType::Wasm,
                                            task.get())) {
    return RejectWithPendingException(cx, promise);
  }

  Unused << task.release();

  callArgs.rval().setUndefined();
  return true;
}

static bool ResolveResponse_OnRejected(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<ResolveResponseClosure*> closure(cx, ToResolveResponseClosure(args));
  Rooted<PromiseObject*> promise(cx, &closure->promise());

  if (!PromiseObject::reject(cx, promise, args.get(0))) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool ResolveResponse(JSContext* cx, CallArgs callArgs,
                            Handle<PromiseObject*> promise,
                            bool instantiate = false,
                            HandleObject importObj = nullptr) {
  MOZ_ASSERT_IF(importObj, instantiate);

  const char* introducer = instantiate ? "WebAssembly.instantiateStreaming"
                                       : "WebAssembly.compileStreaming";

  MutableCompileArgs compileArgs = InitCompileArgs(cx, introducer);
  if (!compileArgs) {
    return false;
  }

  RootedObject closure(
      cx, ResolveResponseClosure::create(cx, *compileArgs, promise, instantiate,
                                         importObj));
  if (!closure) {
    return false;
  }

  RootedFunction onResolved(
      cx, NewNativeFunction(cx, ResolveResponse_OnFulfilled, 1, nullptr,
                            gc::AllocKind::FUNCTION_EXTENDED));
  if (!onResolved) {
    return false;
  }

  RootedFunction onRejected(
      cx, NewNativeFunction(cx, ResolveResponse_OnRejected, 1, nullptr,
                            gc::AllocKind::FUNCTION_EXTENDED));
  if (!onRejected) {
    return false;
  }

  onResolved->setExtendedSlot(0, ObjectValue(*closure));
  onRejected->setExtendedSlot(0, ObjectValue(*closure));

  RootedObject resolve(cx,
                       PromiseObject::unforgeableResolve(cx, callArgs.get(0)));
  if (!resolve) {
    return false;
  }

  return JS::AddPromiseReactions(cx, resolve, onResolved, onRejected);
}

static bool WebAssembly_compileStreaming(JSContext* cx, unsigned argc,
                                         Value* vp) {
  if (!EnsureStreamSupport(cx)) {
    return false;
  }

  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return false;
  }

  CallArgs callArgs = CallArgsFromVp(argc, vp);

  if (!ResolveResponse(cx, callArgs, promise)) {
    return RejectWithPendingException(cx, promise, callArgs);
  }

  callArgs.rval().setObject(*promise);
  return true;
}

static bool WebAssembly_instantiateStreaming(JSContext* cx, unsigned argc,
                                             Value* vp) {
  if (!EnsureStreamSupport(cx)) {
    return false;
  }

  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return false;
  }

  CallArgs callArgs = CallArgsFromVp(argc, vp);

  RootedObject firstArg(cx);
  RootedObject importObj(cx);
  if (!GetInstantiateArgs(cx, callArgs, &firstArg, &importObj)) {
    return RejectWithPendingException(cx, promise, callArgs);
  }

  if (!ResolveResponse(cx, callArgs, promise, true, importObj)) {
    return RejectWithPendingException(cx, promise, callArgs);
  }

  callArgs.rval().setObject(*promise);
  return true;
}

static const JSFunctionSpec WebAssembly_static_methods[] = {
    JS_FN(js_toSource_str, WebAssembly_toSource, 0, 0),
    JS_FN("compile", WebAssembly_compile, 1, JSPROP_ENUMERATE),
    JS_FN("instantiate", WebAssembly_instantiate, 1, JSPROP_ENUMERATE),
    JS_FN("validate", WebAssembly_validate, 1, JSPROP_ENUMERATE),
    JS_FN("compileStreaming", WebAssembly_compileStreaming, 1,
          JSPROP_ENUMERATE),
    JS_FN("instantiateStreaming", WebAssembly_instantiateStreaming, 1,
          JSPROP_ENUMERATE),
    JS_FS_END};

const Class js::WebAssemblyClass = {
    js_WebAssembly_str, JSCLASS_HAS_CACHED_PROTO(JSProto_WebAssembly)};

template <class Class>
static bool InitConstructor(JSContext* cx, HandleObject wasm, const char* name,
                            MutableHandleObject proto) {
  proto.set(NewBuiltinClassInstance<PlainObject>(cx, SingletonObject));
  if (!proto) {
    return false;
  }

  if (!DefinePropertiesAndFunctions(cx, proto, Class::properties,
                                    Class::methods)) {
    return false;
  }

  RootedAtom className(cx, Atomize(cx, name, strlen(name)));
  if (!className) {
    return false;
  }

  RootedFunction ctor(cx,
                      NewNativeConstructor(cx, Class::construct, 1, className));
  if (!ctor) {
    return false;
  }

  if (!DefinePropertiesAndFunctions(cx, ctor, nullptr, Class::static_methods)) {
    return false;
  }

  if (!LinkConstructorAndPrototype(cx, ctor, proto)) {
    return false;
  }

  UniqueChars tagStr(JS_smprintf("WebAssembly.%s", name));
  if (!tagStr) {
    ReportOutOfMemory(cx);
    return false;
  }

  RootedAtom tag(cx, Atomize(cx, tagStr.get(), strlen(tagStr.get())));
  if (!tag) {
    return false;
  }
  if (!DefineToStringTag(cx, proto, tag)) {
    return false;
  }

  RootedId id(cx, AtomToId(className));
  RootedValue ctorValue(cx, ObjectValue(*ctor));
  return DefineDataProperty(cx, wasm, id, ctorValue, 0);
}

static bool InitErrorClass(JSContext* cx, HandleObject wasm, const char* name,
                           JSExnType exn) {
  Handle<GlobalObject*> global = cx->global();
  RootedObject proto(
      cx, GlobalObject::getOrCreateCustomErrorPrototype(cx, global, exn));
  if (!proto) {
    return false;
  }

  RootedAtom className(cx, Atomize(cx, name, strlen(name)));
  if (!className) {
    return false;
  }

  RootedId id(cx, AtomToId(className));
  RootedValue ctorValue(cx, global->getConstructor(GetExceptionProtoKey(exn)));
  return DefineDataProperty(cx, wasm, id, ctorValue, 0);
}

JSObject* js::InitWebAssemblyClass(JSContext* cx,
                                   Handle<GlobalObject*> global) {
  MOZ_RELEASE_ASSERT(HasSupport(cx));

  MOZ_ASSERT(!global->isStandardClassResolved(JSProto_WebAssembly));

  RootedObject proto(cx, GlobalObject::getOrCreateObjectPrototype(cx, global));
  if (!proto) {
    return nullptr;
  }

  RootedObject wasm(cx, NewObjectWithGivenProto(cx, &WebAssemblyClass, proto,
                                                SingletonObject));
  if (!wasm) {
    return nullptr;
  }

  if (!JS_DefineFunctions(cx, wasm, WebAssembly_static_methods)) {
    return nullptr;
  }

  RootedObject moduleProto(cx), instanceProto(cx), memoryProto(cx),
      tableProto(cx);
  RootedObject globalProto(cx);
  if (!InitConstructor<WasmModuleObject>(cx, wasm, "Module", &moduleProto)) {
    return nullptr;
  }
  if (!InitConstructor<WasmInstanceObject>(cx, wasm, "Instance",
                                           &instanceProto)) {
    return nullptr;
  }
  if (!InitConstructor<WasmMemoryObject>(cx, wasm, "Memory", &memoryProto)) {
    return nullptr;
  }
  if (!InitConstructor<WasmTableObject>(cx, wasm, "Table", &tableProto)) {
    return nullptr;
  }
  if (!InitConstructor<WasmGlobalObject>(cx, wasm, "Global", &globalProto)) {
    return nullptr;
  }
  if (!InitErrorClass(cx, wasm, "CompileError", JSEXN_WASMCOMPILEERROR)) {
    return nullptr;
  }
  if (!InitErrorClass(cx, wasm, "LinkError", JSEXN_WASMLINKERROR)) {
    return nullptr;
  }
  if (!InitErrorClass(cx, wasm, "RuntimeError", JSEXN_WASMRUNTIMEERROR)) {
    return nullptr;
  }

  // Perform the final fallible write of the WebAssembly object to a global
  // object property at the end. Only after that succeeds write all the
  // constructor and prototypes to the JSProto slots. This ensures that
  // initialization is atomic since a failed initialization can be retried.

  if (!JS_DefineProperty(cx, global, js_WebAssembly_str, wasm,
                         JSPROP_RESOLVING)) {
    return nullptr;
  }

  global->setPrototype(JSProto_WasmModule, ObjectValue(*moduleProto));
  global->setPrototype(JSProto_WasmInstance, ObjectValue(*instanceProto));
  global->setPrototype(JSProto_WasmMemory, ObjectValue(*memoryProto));
  global->setPrototype(JSProto_WasmTable, ObjectValue(*tableProto));
  global->setPrototype(JSProto_WasmGlobal, ObjectValue(*globalProto));
  global->setConstructor(JSProto_WebAssembly, ObjectValue(*wasm));

  MOZ_ASSERT(global->isStandardClassResolved(JSProto_WebAssembly));
  return wasm;
}
