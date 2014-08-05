/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2014 Mozilla Foundation
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

#include "asmjs/AsmJSLink.h"

#include "mozilla/PodOperations.h"

#ifdef MOZ_VTUNE
# include "vtune/VTuneWrapper.h"
#endif

#include "jscntxt.h"
#include "jsmath.h"
#include "jsprf.h"
#include "jswrapper.h"

#include "asmjs/AsmJSModule.h"
#include "frontend/BytecodeCompiler.h"
#include "jit/Ion.h"
#include "jit/JitCommon.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "vm/StringBuffer.h"

#include "jsobjinlines.h"

using namespace js;
using namespace js::jit;

using mozilla::IsNaN;
using mozilla::PodZero;

static bool
CloneModule(JSContext *cx, MutableHandle<AsmJSModuleObject*> moduleObj)
{
    ScopedJSDeletePtr<AsmJSModule> module;
    if (!moduleObj->module().clone(cx, &module))
        return false;

    module->staticallyLink(cx);

    AsmJSModuleObject *newModuleObj = AsmJSModuleObject::create(cx, &module);
    if (!newModuleObj)
        return false;

    moduleObj.set(newModuleObj);
    return true;
}

static bool
LinkFail(JSContext *cx, const char *str)
{
    JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING, js_GetErrorMessage,
                                 nullptr, JSMSG_USE_ASM_LINK_FAIL, str);
    return false;
}

static bool
GetDataProperty(JSContext *cx, HandleValue objVal, HandlePropertyName field, MutableHandleValue v)
{
    if (!objVal.isObject())
        return LinkFail(cx, "accessing property of non-object");

    Rooted<JSPropertyDescriptor> desc(cx);
    RootedObject obj(cx, &objVal.toObject());
    RootedId id(cx, NameToId(field));
    if (!JS_GetPropertyDescriptorById(cx, obj, id, &desc))
        return false;

    if (!desc.object())
        return LinkFail(cx, "property not present on object");

    if (desc.hasGetterOrSetterObject())
        return LinkFail(cx, "property is not a data property");

    v.set(desc.value());
    return true;
}

static bool
ValidateGlobalVariable(JSContext *cx, const AsmJSModule &module, AsmJSModule::Global &global,
                       HandleValue importVal)
{
    JS_ASSERT(global.which() == AsmJSModule::Global::Variable);

    void *datum = module.globalVarIndexToGlobalDatum(global.varIndex());

    switch (global.varInitKind()) {
      case AsmJSModule::Global::InitConstant: {
        const Value &v = global.varInitConstant();
        switch (global.varInitCoercion()) {
          case AsmJS_ToInt32:
            *(int32_t *)datum = v.toInt32();
            break;
          case AsmJS_ToNumber:
            *(double *)datum = v.toDouble();
            break;
          case AsmJS_FRound:
            *(float *)datum = static_cast<float>(v.toDouble());
            break;
        }
        break;
      }
      case AsmJSModule::Global::InitImport: {
        RootedPropertyName field(cx, global.varImportField());
        RootedValue v(cx);
        if (!GetDataProperty(cx, importVal, field, &v))
            return false;

        switch (global.varInitCoercion()) {
          case AsmJS_ToInt32:
            if (!ToInt32(cx, v, (int32_t *)datum))
                return false;
            break;
          case AsmJS_ToNumber:
            if (!ToNumber(cx, v, (double *)datum))
                return false;
            break;
          case AsmJS_FRound:
            if (!RoundFloat32(cx, v, (float *)datum))
                return false;
            break;
        }
        break;
      }
    }

    return true;
}

static bool
ValidateFFI(JSContext *cx, AsmJSModule::Global &global, HandleValue importVal,
            AutoObjectVector *ffis)
{
    RootedPropertyName field(cx, global.ffiField());
    RootedValue v(cx);
    if (!GetDataProperty(cx, importVal, field, &v))
        return false;

    if (!v.isObject() || !v.toObject().is<JSFunction>())
        return LinkFail(cx, "FFI imports must be functions");

    (*ffis)[global.ffiIndex()].set(&v.toObject().as<JSFunction>());
    return true;
}

static bool
ValidateArrayView(JSContext *cx, AsmJSModule::Global &global, HandleValue globalVal,
                  HandleValue bufferVal)
{
    RootedPropertyName field(cx, global.viewName());
    RootedValue v(cx);
    if (!GetDataProperty(cx, globalVal, field, &v))
        return false;

    if (!IsTypedArrayConstructor(v, global.viewType()))
        return LinkFail(cx, "bad typed array constructor");

    return true;
}

static bool
ValidateMathBuiltinFunction(JSContext *cx, AsmJSModule::Global &global, HandleValue globalVal)
{
    RootedValue v(cx);
    if (!GetDataProperty(cx, globalVal, cx->names().Math, &v))
        return false;
    RootedPropertyName field(cx, global.mathName());
    if (!GetDataProperty(cx, v, field, &v))
        return false;

    Native native = nullptr;
    switch (global.mathBuiltinFunction()) {
      case AsmJSMathBuiltin_sin: native = math_sin; break;
      case AsmJSMathBuiltin_cos: native = math_cos; break;
      case AsmJSMathBuiltin_tan: native = math_tan; break;
      case AsmJSMathBuiltin_asin: native = math_asin; break;
      case AsmJSMathBuiltin_acos: native = math_acos; break;
      case AsmJSMathBuiltin_atan: native = math_atan; break;
      case AsmJSMathBuiltin_ceil: native = math_ceil; break;
      case AsmJSMathBuiltin_floor: native = math_floor; break;
      case AsmJSMathBuiltin_exp: native = math_exp; break;
      case AsmJSMathBuiltin_log: native = math_log; break;
      case AsmJSMathBuiltin_pow: native = math_pow; break;
      case AsmJSMathBuiltin_sqrt: native = math_sqrt; break;
      case AsmJSMathBuiltin_min: native = math_min; break;
      case AsmJSMathBuiltin_max: native = math_max; break;
      case AsmJSMathBuiltin_abs: native = math_abs; break;
      case AsmJSMathBuiltin_atan2: native = math_atan2; break;
      case AsmJSMathBuiltin_imul: native = math_imul; break;
      case AsmJSMathBuiltin_fround: native = math_fround; break;
    }

    if (!IsNativeFunction(v, native))
        return LinkFail(cx, "bad Math.* builtin function");

    return true;
}

static bool
ValidateConstant(JSContext *cx, AsmJSModule::Global &global, HandleValue globalVal)
{
    RootedPropertyName field(cx, global.constantName());
    RootedValue v(cx, globalVal);

    if (global.constantKind() == AsmJSModule::Global::MathConstant) {
        if (!GetDataProperty(cx, v, cx->names().Math, &v))
            return false;
    }

    if (!GetDataProperty(cx, v, field, &v))
        return false;
    if (!v.isNumber())
        return LinkFail(cx, "math / global constant value needs to be a number");

    // NaN != NaN
    if (IsNaN(global.constantValue())) {
        if (!IsNaN(v.toNumber()))
            return LinkFail(cx, "global constant value needs to be NaN");
    } else {
        if (v.toNumber() != global.constantValue())
            return LinkFail(cx, "global constant value mismatch");
    }

    return true;
}

static bool
LinkModuleToHeap(JSContext *cx, AsmJSModule &module, Handle<ArrayBufferObject*> heap)
{
    uint32_t heapLength = heap->byteLength();
    if (!IsValidAsmJSHeapLength(heapLength)) {
        ScopedJSFreePtr<char> msg(
            JS_smprintf("ArrayBuffer byteLength 0x%x is not a valid heap length. The next "
                        "valid length is 0x%x",
                        heapLength,
                        RoundUpToNextValidAsmJSHeapLength(heapLength)));
        return LinkFail(cx, msg.get());
    }

    // This check is sufficient without considering the size of the loaded datum because heap
    // loads and stores start on an aligned boundary and the heap byteLength has larger alignment.
    JS_ASSERT((module.minHeapLength() - 1) <= INT32_MAX);
    if (heapLength < module.minHeapLength()) {
        ScopedJSFreePtr<char> msg(
            JS_smprintf("ArrayBuffer byteLength of 0x%x is less than 0x%x (which is the"
                        "largest constant heap access offset rounded up to the next valid "
                        "heap size).",
                        heapLength,
                        module.minHeapLength()));
        return LinkFail(cx, msg.get());
    }

    // If we've generated the code with signal handlers in mind (for bounds
    // checks on x64 and for interrupt callback requesting on all platforms),
    // we need to be able to use signals at runtime. In particular, a module
    // can have been created using signals and cached, and executed without
    // signals activated.
    if (module.usesSignalHandlersForInterrupt() && !cx->canUseSignalHandlers())
        return LinkFail(cx, "Code generated with signal handlers but signals are deactivated");

    if (!ArrayBufferObject::prepareForAsmJS(cx, heap, module.usesSignalHandlersForOOB()))
        return LinkFail(cx, "Unable to prepare ArrayBuffer for asm.js use");

    module.initHeap(heap, cx);
    return true;
}

static bool
DynamicallyLinkModule(JSContext *cx, CallArgs args, AsmJSModule &module)
{
    module.setIsDynamicallyLinked();

    RootedValue globalVal(cx);
    if (args.length() > 0)
        globalVal = args[0];

    RootedValue importVal(cx);
    if (args.length() > 1)
        importVal = args[1];

    RootedValue bufferVal(cx);
    if (args.length() > 2)
        bufferVal = args[2];

    Rooted<ArrayBufferObject*> heap(cx);
    if (module.hasArrayView()) {
        if (!IsTypedArrayBuffer(bufferVal))
            return LinkFail(cx, "bad ArrayBuffer argument");

        heap = &AsTypedArrayBuffer(bufferVal);
        if (!LinkModuleToHeap(cx, module, heap))
            return false;
    }

    AutoObjectVector ffis(cx);
    if (!ffis.resize(module.numFFIs()))
        return false;

    for (unsigned i = 0; i < module.numGlobals(); i++) {
        AsmJSModule::Global &global = module.global(i);
        switch (global.which()) {
          case AsmJSModule::Global::Variable:
            if (!ValidateGlobalVariable(cx, module, global, importVal))
                return false;
            break;
          case AsmJSModule::Global::FFI:
            if (!ValidateFFI(cx, global, importVal, &ffis))
                return false;
            break;
          case AsmJSModule::Global::ArrayView:
            if (!ValidateArrayView(cx, global, globalVal, bufferVal))
                return false;
            break;
          case AsmJSModule::Global::MathBuiltinFunction:
            if (!ValidateMathBuiltinFunction(cx, global, globalVal))
                return false;
            break;
          case AsmJSModule::Global::Constant:
            if (!ValidateConstant(cx, global, globalVal))
                return false;
            break;
        }
    }

    for (unsigned i = 0; i < module.numExits(); i++)
        module.exitIndexToGlobalDatum(i).fun = &ffis[module.exit(i).ffiIndex()]->as<JSFunction>();

    return true;
}

static const unsigned ASM_MODULE_SLOT = 0;
static const unsigned ASM_EXPORT_INDEX_SLOT = 1;

static unsigned
FunctionToExportedFunctionIndex(HandleFunction fun)
{
    Value v = fun->getExtendedSlot(ASM_EXPORT_INDEX_SLOT);
    return v.toInt32();
}

static const AsmJSModule::ExportedFunction &
FunctionToExportedFunction(HandleFunction fun, AsmJSModule &module)
{
    unsigned funIndex = FunctionToExportedFunctionIndex(fun);
    return module.exportedFunction(funIndex);
}

static AsmJSModule &
FunctionToEnclosingModule(HandleFunction fun)
{
    return fun->getExtendedSlot(ASM_MODULE_SLOT).toObject().as<AsmJSModuleObject>().module();
}

// The JSNative for the functions nested in an asm.js module. Calling this
// native will trampoline into generated code.
static bool
CallAsmJS(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs callArgs = CallArgsFromVp(argc, vp);
    RootedFunction callee(cx, &callArgs.callee().as<JSFunction>());

    // An asm.js function stores, in its extended slots:
    //  - a pointer to the module from which it was returned
    //  - its index in the ordered list of exported functions
    AsmJSModule &module = FunctionToEnclosingModule(callee);

    // Enable/disable profiling in the asm.js module to match the current global
    // profiling state. Don't do this if the module is already active on the
    // stack since this would leave the module in a state where profiling is
    // enabled but the stack isn't unwindable.
    if (module.profilingEnabled() != cx->runtime()->spsProfiler.enabled() && !module.active())
        module.setProfilingEnabled(cx->runtime()->spsProfiler.enabled(), cx);

    // An exported function points to the code as well as the exported
    // function's signature, which implies the dynamic coercions performed on
    // the arguments.
    const AsmJSModule::ExportedFunction &func = FunctionToExportedFunction(callee, module);

    // The calling convention for an external call into asm.js is to pass an
    // array of 8-byte values where each value contains either a coerced int32
    // (in the low word) or double value, with the coercions specified by the
    // asm.js signature. The external entry point unpacks this array into the
    // system-ABI-specified registers and stack memory and then calls into the
    // internal entry point. The return value is stored in the first element of
    // the array (which, therefore, must have length >= 1).

    js::Vector<uint64_t, 8> coercedArgs(cx);
    if (!coercedArgs.resize(Max<size_t>(1, func.numArgs())))
        return false;

    RootedValue v(cx);
    for (unsigned i = 0; i < func.numArgs(); ++i) {
        v = i < callArgs.length() ? callArgs[i] : UndefinedValue();
        switch (func.argCoercion(i)) {
          case AsmJS_ToInt32:
            if (!ToInt32(cx, v, (int32_t*)&coercedArgs[i]))
                return false;
            break;
          case AsmJS_ToNumber:
            if (!ToNumber(cx, v, (double*)&coercedArgs[i]))
                return false;
            break;
          case AsmJS_FRound:
            if (!RoundFloat32(cx, v, (float *)&coercedArgs[i]))
                return false;
            break;
        }
    }

    // An asm.js module is specialized to its heap's base address and length
    // which is normally immutable except for the neuter operation that occurs
    // when an ArrayBuffer is transfered. Throw an internal error if we're
    // about to run with a neutered heap.
    if (module.maybeHeapBufferObject() && module.maybeHeapBufferObject()->isNeutered()) {
        js_ReportOverRecursed(cx);
        return false;
    }

    {
        // Push an AsmJSActivation to describe the asm.js frames we're about to
        // push when running this module. Additionally, push a JitActivation so
        // that the optimized asm.js-to-Ion FFI call path (which we want to be
        // very fast) can avoid doing so. The JitActivation is marked as
        // inactive so stack iteration will skip over it.
        AsmJSActivation activation(cx, module);
        JitActivation jitActivation(cx, /* firstFrameIsConstructing = */ false, /* active */ false);

        // Call the per-exported-function trampoline created by GenerateEntry.
        AsmJSModule::CodePtr enter = module.entryTrampoline(func);
        if (!CALL_GENERATED_ASMJS(enter, coercedArgs.begin(), module.globalData()))
            return false;
    }

    if (callArgs.isConstructing()) {
        // By spec, when a function is called as a constructor and this function
        // returns a primary type, which is the case for all asm.js exported
        // functions, the returned value is discarded and an empty object is
        // returned instead.
        JSObject *obj = NewBuiltinClassInstance(cx, &JSObject::class_);
        callArgs.rval().set(ObjectValue(*obj));
        return true;
    }

    switch (func.returnType()) {
      case AsmJSModule::Return_Void:
        callArgs.rval().set(UndefinedValue());
        break;
      case AsmJSModule::Return_Int32:
        callArgs.rval().set(Int32Value(*(int32_t*)&coercedArgs[0]));
        break;
      case AsmJSModule::Return_Double:
        callArgs.rval().set(NumberValue(*(double*)&coercedArgs[0]));
        break;
    }

    return true;
}

static JSFunction *
NewExportedFunction(JSContext *cx, const AsmJSModule::ExportedFunction &func,
                    HandleObject moduleObj, unsigned exportIndex)
{
    RootedPropertyName name(cx, func.name());
    JSFunction *fun = NewFunction(cx, NullPtr(), CallAsmJS, func.numArgs(),
                                  JSFunction::ASMJS_CTOR, cx->global(), name,
                                  JSFunction::ExtendedFinalizeKind);
    if (!fun)
        return nullptr;

    fun->setExtendedSlot(ASM_MODULE_SLOT, ObjectValue(*moduleObj));
    fun->setExtendedSlot(ASM_EXPORT_INDEX_SLOT, Int32Value(exportIndex));
    return fun;
}

static bool
HandleDynamicLinkFailure(JSContext *cx, CallArgs args, AsmJSModule &module, HandlePropertyName name)
{
    if (cx->isExceptionPending())
        return false;

    uint32_t begin = module.srcBodyStart();  // starts right after 'use asm'
    uint32_t end = module.srcEndBeforeCurly();
    Rooted<JSFlatString*> src(cx, module.scriptSource()->substringDontDeflate(cx, begin, end));
    if (!src)
        return false;

    RootedFunction fun(cx, NewFunction(cx, NullPtr(), nullptr, 0, JSFunction::INTERPRETED,
                                       cx->global(), name, JSFunction::FinalizeKind,
                                       TenuredObject));
    if (!fun)
        return false;

    AutoNameVector formals(cx);
    formals.reserve(3);
    if (module.globalArgumentName())
        formals.infallibleAppend(module.globalArgumentName());
    if (module.importArgumentName())
        formals.infallibleAppend(module.importArgumentName());
    if (module.bufferArgumentName())
        formals.infallibleAppend(module.bufferArgumentName());

    CompileOptions options(cx);
    options.setOriginPrincipals(module.scriptSource()->originPrincipals())
           .setFile(module.scriptSource()->filename())
           .setCompileAndGo(false)
           .setNoScriptRval(false);

    // The exported function inherits an implicit strict context if the module
    // also inherited it somehow.
    if (module.strict())
        options.strictOption = true;

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, src))
        return false;

    const jschar *chars = stableChars.twoByteRange().start().get();
    SourceBufferHolder::Ownership ownership = stableChars.maybeGiveOwnershipToCaller()
                                              ? SourceBufferHolder::GiveOwnership
                                              : SourceBufferHolder::NoOwnership;
    SourceBufferHolder srcBuf(chars, end - begin, ownership);
    if (!frontend::CompileFunctionBody(cx, &fun, options, formals, srcBuf))
        return false;

    // Call the function we just recompiled.
    args.setCallee(ObjectValue(*fun));
    return Invoke(cx, args, args.isConstructing() ? CONSTRUCT : NO_CONSTRUCT);
}

#ifdef MOZ_VTUNE
static bool
SendFunctionsToVTune(JSContext *cx, AsmJSModule &module)
{
    uint8_t *base = module.codeBase();

    for (unsigned i = 0; i < module.numProfiledFunctions(); i++) {
        const AsmJSModule::ProfiledFunction &func = module.profiledFunction(i);

        uint8_t *start = base + func.pod.startCodeOffset;
        uint8_t *end   = base + func.pod.endCodeOffset;
        JS_ASSERT(end >= start);

        unsigned method_id = iJIT_GetNewMethodID();
        if (method_id == 0)
            return false;

        JSAutoByteString bytes;
        const char *method_name = AtomToPrintableString(cx, func.name, &bytes);
        if (!method_name)
            return false;

        iJIT_Method_Load method;
        method.method_id = method_id;
        method.method_name = const_cast<char *>(method_name);
        method.method_load_address = (void *)start;
        method.method_size = unsigned(end - start);
        method.line_number_size = 0;
        method.line_number_table = nullptr;
        method.class_id = 0;
        method.class_file_name = nullptr;
        method.source_file_name = nullptr;

        iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void *)&method);
    }

    return true;
}
#endif

#ifdef JS_ION_PERF
static bool
SendFunctionsToPerf(JSContext *cx, AsmJSModule &module)
{
    if (!PerfFuncEnabled())
        return true;

    uintptr_t base = (uintptr_t) module.codeBase();
    const char *filename = module.scriptSource()->filename();

    for (unsigned i = 0; i < module.numProfiledFunctions(); i++) {
        const AsmJSModule::ProfiledFunction &func = module.profiledFunction(i);
        uintptr_t start = base + (unsigned long) func.pod.startCodeOffset;
        uintptr_t end   = base + (unsigned long) func.pod.endCodeOffset;
        JS_ASSERT(end >= start);
        size_t size = end - start;

        JSAutoByteString bytes;
        const char *name = AtomToPrintableString(cx, func.name, &bytes);
        if (!name)
            return false;

        writePerfSpewerAsmJSFunctionMap(start, size, filename, func.pod.lineno,
                                        func.pod.columnIndex, name);
    }

    return true;
}

static bool
SendBlocksToPerf(JSContext *cx, AsmJSModule &module)
{
    if (!PerfBlockEnabled())
        return true;

    unsigned long funcBaseAddress = (unsigned long) module.codeBase();
    const char *filename = module.scriptSource()->filename();

    for (unsigned i = 0; i < module.numPerfBlocksFunctions(); i++) {
        const AsmJSModule::ProfiledBlocksFunction &func = module.perfProfiledBlocksFunction(i);

        size_t size = func.pod.endCodeOffset - func.pod.startCodeOffset;

        JSAutoByteString bytes;
        const char *name = AtomToPrintableString(cx, func.name, &bytes);
        if (!name)
            return false;

        writePerfSpewerAsmJSBlocksMap(funcBaseAddress, func.pod.startCodeOffset,
                                      func.endInlineCodeOffset, size, filename, name, func.blocks);
    }

    return true;
}
#endif

static bool
SendModuleToAttachedProfiler(JSContext *cx, AsmJSModule &module)
{
#if defined(MOZ_VTUNE)
    if (IsVTuneProfilingActive() && !SendFunctionsToVTune(cx, module))
        return false;
#endif

#if defined(JS_ION_PERF)
    if (module.numExportedFunctions() > 0) {
        size_t firstEntryCode = size_t(module.codeBase() + module.functionBytes());
        writePerfSpewerAsmJSEntriesAndExits(firstEntryCode, module.codeBytes() - module.functionBytes());
    }
    if (!SendBlocksToPerf(cx, module))
        return false;
    if (!SendFunctionsToPerf(cx, module))
        return false;
#endif

    return true;
}


static JSObject *
CreateExportObject(JSContext *cx, Handle<AsmJSModuleObject*> moduleObj)
{
    AsmJSModule &module = moduleObj->module();

    if (module.numExportedFunctions() == 1) {
        const AsmJSModule::ExportedFunction &func = module.exportedFunction(0);
        if (!func.maybeFieldName())
            return NewExportedFunction(cx, func, moduleObj, 0);
    }

    gc::AllocKind allocKind = gc::GetGCObjectKind(module.numExportedFunctions());
    RootedObject obj(cx, NewBuiltinClassInstance(cx, &JSObject::class_, allocKind));
    if (!obj)
        return nullptr;

    for (unsigned i = 0; i < module.numExportedFunctions(); i++) {
        const AsmJSModule::ExportedFunction &func = module.exportedFunction(i);

        RootedFunction fun(cx, NewExportedFunction(cx, func, moduleObj, i));
        if (!fun)
            return nullptr;

        JS_ASSERT(func.maybeFieldName() != nullptr);
        RootedId id(cx, NameToId(func.maybeFieldName()));
        RootedValue val(cx, ObjectValue(*fun));
        if (!DefineNativeProperty(cx, obj, id, val, nullptr, nullptr, JSPROP_ENUMERATE))
            return nullptr;
    }

    return obj;
}

static const unsigned MODULE_FUN_SLOT = 0;

static AsmJSModuleObject &
ModuleFunctionToModuleObject(JSFunction *fun)
{
    return fun->getExtendedSlot(MODULE_FUN_SLOT).toObject().as<AsmJSModuleObject>();
}

// Implements the semantics of an asm.js module function that has been successfully validated.
static bool
LinkAsmJS(JSContext *cx, unsigned argc, JS::Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // The LinkAsmJS builtin (created by NewAsmJSModuleFunction) is an extended
    // function and stores its module in an extended slot.
    RootedFunction fun(cx, &args.callee().as<JSFunction>());
    Rooted<AsmJSModuleObject*> moduleObj(cx, &ModuleFunctionToModuleObject(fun));

    // All ICache flushing of the module being linked has been inhibited under the
    // assumption that the module is flushed after dynamic linking (when the last code
    // mutation occurs).  Thus, enter an AutoFlushICache context for the entire module
    // now.  The module range is set below.
    AutoFlushICache afc("LinkAsmJS");

    // When a module is linked, it is dynamically specialized to the given
    // arguments (buffer, ffis). Thus, if the module is linked again (it is just
    // a function so it can be called multiple times), we need to clone a new
    // module.
    if (moduleObj->module().isDynamicallyLinked()) {
        if (!CloneModule(cx, &moduleObj))
            return false;
    } else {
        // CloneModule already calls setAutoFlushICacheRange internally before patching
        // the cloned module, so avoid calling twice.
        moduleObj->module().setAutoFlushICacheRange();
    }

    AsmJSModule &module = moduleObj->module();

    // Link the module by performing the link-time validation checks in the
    // asm.js spec and then patching the generated module to associate it with
    // the given heap (ArrayBuffer) and a new global data segment (the closure
    // state shared by the inner asm.js functions).
    if (!DynamicallyLinkModule(cx, args, module)) {
        // Linking failed, so reparse the entire asm.js module from scratch to
        // get normal interpreted bytecode which we can simply Invoke. Very slow.
        RootedPropertyName name(cx, fun->name());
        return HandleDynamicLinkFailure(cx, args, module, name);
    }

    // Notify profilers so that asm.js generated code shows up with JS function
    // names and lines in native (i.e., not SPS) profilers.
    if (!SendModuleToAttachedProfiler(cx, module))
        return false;

    // Link-time validation succeeded, so wrap all the exported functions with
    // CallAsmJS builtins that trampoline into the generated code.
    JSObject *obj = CreateExportObject(cx, moduleObj);
    if (!obj)
        return false;

    args.rval().set(ObjectValue(*obj));
    return true;
}

JSFunction *
js::NewAsmJSModuleFunction(ExclusiveContext *cx, JSFunction *origFun, HandleObject moduleObj)
{
    RootedPropertyName name(cx, origFun->name());

    JSFunction::Flags flags = origFun->isLambda() ? JSFunction::ASMJS_LAMBDA_CTOR
                                                  : JSFunction::ASMJS_CTOR;
    JSFunction *moduleFun = NewFunction(cx, NullPtr(), LinkAsmJS, origFun->nargs(),
                                        flags, NullPtr(), name,
                                        JSFunction::ExtendedFinalizeKind, TenuredObject);
    if (!moduleFun)
        return nullptr;

    moduleFun->setExtendedSlot(MODULE_FUN_SLOT, ObjectValue(*moduleObj));
    return moduleFun;
}

bool
js::IsAsmJSModuleNative(js::Native native)
{
    return native == LinkAsmJS;
}

static bool
IsMaybeWrappedNativeFunction(const Value &v, Native native, JSFunction **fun = nullptr)
{
    if (!v.isObject())
        return false;

    JSObject *obj = CheckedUnwrap(&v.toObject());
    if (!obj)
        return false;

    if (!obj->is<JSFunction>())
        return false;

    if (fun)
        *fun = &obj->as<JSFunction>();

    return obj->as<JSFunction>().maybeNative() == native;
}

bool
js::IsAsmJSModule(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    bool rval = args.hasDefined(0) && IsMaybeWrappedNativeFunction(args.get(0), LinkAsmJS);
    args.rval().set(BooleanValue(rval));
    return true;
}

bool
js::IsAsmJSModule(HandleFunction fun)
{
    return fun->isNative() && fun->maybeNative() == LinkAsmJS;
}

static bool
AppendUseStrictSource(JSContext *cx, HandleFunction fun, Handle<JSFlatString*> src, StringBuffer &out)
{
    // We need to add "use strict" in the body right after the opening
    // brace.
    size_t bodyStart = 0, bodyEnd;

    // No need to test for functions created with the Function ctor as
    // these don't implicitly inherit the "use strict" context. Strict mode is
    // enabled for functions created with the Function ctor only if they begin with
    // the "use strict" directive, but these functions won't validate as asm.js
    // modules.

    if (!FindBody(cx, fun, src, &bodyStart, &bodyEnd))
        return false;

    return out.appendSubstring(src, 0, bodyStart) &&
           out.append("\n\"use strict\";\n") &&
           out.appendSubstring(src, bodyStart, src->length() - bodyStart);
}

JSString *
js::AsmJSModuleToString(JSContext *cx, HandleFunction fun, bool addParenToLambda)
{
    AsmJSModule &module = ModuleFunctionToModuleObject(fun).module();

    uint32_t begin = module.srcStart();
    uint32_t end = module.srcEndAfterCurly();
    ScriptSource *source = module.scriptSource();
    StringBuffer out(cx);

    // Whether the function has been created with a Function ctor
    bool funCtor = begin == 0 && end == source->length() && source->argumentsNotIncluded();

    if (addParenToLambda && fun->isLambda() && !out.append("("))
        return nullptr;

    if (!out.append("function "))
        return nullptr;

    if (fun->atom() && !out.append(fun->atom()))
        return nullptr;

    if (funCtor) {
        // Functions created with the function constructor don't have arguments in their source.
        if (!out.append("("))
            return nullptr;

        if (PropertyName *argName = module.globalArgumentName()) {
            if (!out.append(argName))
                return nullptr;
        }
        if (PropertyName *argName = module.importArgumentName()) {
            if (!out.append(", ") || !out.append(argName))
                return nullptr;
        }
        if (PropertyName *argName = module.bufferArgumentName()) {
            if (!out.append(", ") || !out.append(argName))
                return nullptr;
        }

        if (!out.append(") {\n"))
            return nullptr;
    }

    Rooted<JSFlatString*> src(cx, source->substring(cx, begin, end));
    if (!src)
        return nullptr;

    if (module.strict()) {
        if (!AppendUseStrictSource(cx, fun, src, out))
            return nullptr;
    } else {
        if (!out.append(src))
            return nullptr;
    }

    if (funCtor && !out.append("\n}"))
        return nullptr;

    if (addParenToLambda && fun->isLambda() && !out.append(")"))
        return nullptr;

    return out.finishString();
}

bool
js::IsAsmJSModuleLoadedFromCache(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSFunction *fun;
    if (!args.hasDefined(0) || !IsMaybeWrappedNativeFunction(args[0], LinkAsmJS, &fun)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_USE_ASM_TYPE_FAIL,
                             "argument passed to isAsmJSModuleLoadedFromCache is not a "
                             "validated asm.js module");
        return false;
    }

    bool loadedFromCache = ModuleFunctionToModuleObject(fun).module().loadedFromCache();

    args.rval().set(BooleanValue(loadedFromCache));
    return true;
}

bool
js::IsAsmJSFunction(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    bool rval = args.hasDefined(0) && IsMaybeWrappedNativeFunction(args[0], CallAsmJS);
    args.rval().set(BooleanValue(rval));
    return true;
}

bool
js::IsAsmJSFunction(HandleFunction fun)
{
    return fun->isNative() && fun->maybeNative() == CallAsmJS;
}

JSString *
js::AsmJSFunctionToString(JSContext *cx, HandleFunction fun)
{
    AsmJSModule &module = FunctionToEnclosingModule(fun);
    const AsmJSModule::ExportedFunction &f = FunctionToExportedFunction(fun, module);
    uint32_t begin = module.srcStart() + f.startOffsetInModule();
    uint32_t end = module.srcStart() + f.endOffsetInModule();

    ScriptSource *source = module.scriptSource();
    StringBuffer out(cx);

    // asm.js functions cannot have been created with a Function constructor
    // as they belong within a module.
    JS_ASSERT(!(begin == 0 && end == source->length() && source->argumentsNotIncluded()));

    if (!out.append("function "))
        return nullptr;

    if (module.strict()) {
        // AppendUseStrictSource expects its input to start right after the
        // function name, so split the source chars from the src into two parts:
        // the function name and the rest (arguments + body).

        // asm.js functions can't be anonymous
        JS_ASSERT(fun->atom());
        if (!out.append(fun->atom()))
            return nullptr;

        size_t nameEnd = begin + fun->atom()->length();
        Rooted<JSFlatString*> src(cx, source->substring(cx, nameEnd, end));
        if (!AppendUseStrictSource(cx, fun, src, out))
            return nullptr;
    } else {
        Rooted<JSFlatString*> src(cx, source->substring(cx, begin, end));
        if (!src)
            return nullptr;
        if (!out.append(src))
            return nullptr;
    }

    return out.finishString();
}
