/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AsmJSLink.h"

#ifdef MOZ_VTUNE
# include "vtune/VTuneWrapper.h"
#endif

#include "jscntxt.h"
#include "jsmath.h"
#include "jsprf.h"
#include "jswrapper.h"

#include "frontend/BytecodeCompiler.h"
#include "jit/AsmJSModule.h"
#include "jit/Ion.h"
#include "jit/JitCommon.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif

#include "jsobjinlines.h"

using namespace js;
using namespace js::jit;

using mozilla::IsNaN;

static const unsigned MODULE_FUN_SLOT = 0;

static bool
LinkFail(JSContext *cx, const char *str)
{
    JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING, js_GetErrorMessage,
                                 nullptr, JSMSG_USE_ASM_LINK_FAIL, str);
    return false;
}

static bool
GetDataProperty(JSContext *cx, const Value &objVal, HandlePropertyName field, MutableHandleValue v)
{
    if (!objVal.isObject())
        return LinkFail(cx, "accessing property of non-object");

    Rooted<JSPropertyDescriptor> desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, &objVal.toObject(), NameToId(field), 0, &desc))
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

    (*ffis)[global.ffiIndex()] = &v.toObject().as<JSFunction>();
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
ValidateMathBuiltin(JSContext *cx, AsmJSModule::Global &global, HandleValue globalVal)
{
    RootedValue v(cx);
    if (!GetDataProperty(cx, globalVal, cx->names().Math, &v))
        return false;
    RootedPropertyName field(cx, global.mathName());
    if (!GetDataProperty(cx, v, field, &v))
        return false;

    Native native = nullptr;
    switch (global.mathBuiltin()) {
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
      case AsmJSMathBuiltin_pow: native = js_math_pow; break;
      case AsmJSMathBuiltin_sqrt: native = js_math_sqrt; break;
      case AsmJSMathBuiltin_abs: native = js_math_abs; break;
      case AsmJSMathBuiltin_atan2: native = math_atan2; break;
      case AsmJSMathBuiltin_imul: native = math_imul; break;
      case AsmJSMathBuiltin_fround: native = math_fround; break;
    }

    if (!IsNativeFunction(v, native))
        return LinkFail(cx, "bad Math.* builtin");

    return true;
}

static bool
ValidateGlobalConstant(JSContext *cx, AsmJSModule::Global &global, HandleValue globalVal)
{
    RootedPropertyName field(cx, global.constantName());
    RootedValue v(cx);
    if (!GetDataProperty(cx, globalVal, field, &v))
        return false;

    if (!v.isNumber())
        return LinkFail(cx, "global constant value needs to be a number");

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
DynamicallyLinkModule(JSContext *cx, CallArgs args, AsmJSModule &module)
{
    if (module.isLinked())
        return LinkFail(cx, "As a temporary limitation, modules cannot be linked more than "
                            "once. This limitation should be removed in a future release. To "
                            "work around this, compile a second module (e.g., using the "
                            "Function constructor).");
    module.setIsLinked();

    RootedValue globalVal(cx, UndefinedValue());
    if (args.length() > 0)
        globalVal = args[0];

    RootedValue importVal(cx, UndefinedValue());
    if (args.length() > 1)
        importVal = args[1];

    RootedValue bufferVal(cx, UndefinedValue());
    if (args.length() > 2)
        bufferVal = args[2];

    Rooted<ArrayBufferObject*> heap(cx);
    if (module.hasArrayView()) {
        if (!IsTypedArrayBuffer(bufferVal))
            return LinkFail(cx, "bad ArrayBuffer argument");

        heap = &bufferVal.toObject().as<ArrayBufferObject>();

        if (!IsValidAsmJSHeapLength(heap->byteLength())) {
            return LinkFail(cx, JS_smprintf("ArrayBuffer byteLength 0x%x is not a valid heap length. The next valid length is 0x%x",
                                            heap->byteLength(),
                                            RoundUpToNextValidAsmJSHeapLength(heap->byteLength())));
        }

        // This check is sufficient without considering the size of the loaded datum because heap
        // loads and stores start on an aligned boundary and the heap byteLength has larger alignment.
        JS_ASSERT((module.minHeapLength() - 1) <= INT32_MAX);
        if (heap->byteLength() < module.minHeapLength()) {
            return LinkFail(cx, JS_smprintf("ArrayBuffer byteLength of 0x%x is less than 0x%x (which is the largest constant heap access offset rounded up to the next valid heap size).",
                                            heap->byteLength(), module.minHeapLength()));
        }

        if (!ArrayBufferObject::prepareForAsmJS(cx, heap))
            return LinkFail(cx, "Unable to prepare ArrayBuffer for asm.js use");

        module.initHeap(heap, cx);
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
          case AsmJSModule::Global::MathBuiltin:
            if (!ValidateMathBuiltin(cx, global, globalVal))
                return false;
            break;
          case AsmJSModule::Global::Constant:
            if (!ValidateGlobalConstant(cx, global, globalVal))
                return false;
            break;
        }
    }

    for (unsigned i = 0; i < module.numExits(); i++)
        module.exitIndexToGlobalDatum(i).fun = &ffis[module.exit(i).ffiIndex()]->as<JSFunction>();

    return true;
}

AsmJSActivation::AsmJSActivation(JSContext *cx, AsmJSModule &module)
  : cx_(cx),
    module_(module),
    errorRejoinSP_(nullptr),
    profiler_(nullptr),
    resumePC_(nullptr)
{
    if (cx->runtime()->spsProfiler.enabled()) {
        // Use a profiler string that matches jsMatch regex in
        // browser/devtools/profiler/cleopatra/js/parserWorker.js.
        // (For now use a single static string to avoid further slowing down
        // calls into asm.js.)
        profiler_ = &cx->runtime()->spsProfiler;
        profiler_->enterNative("asm.js code :0", this);
    }

    prev_ = cx_->runtime()->mainThread.asmJSActivationStack_;

    JSRuntime::AutoLockForOperationCallback lock(cx_->runtime());
    cx_->runtime()->mainThread.asmJSActivationStack_ = this;

    (void) errorRejoinSP_;  // squelch GCC warning
}

AsmJSActivation::~AsmJSActivation()
{
    if (profiler_)
        profiler_->exitNative();

    JS_ASSERT(cx_->runtime()->mainThread.asmJSActivationStack_ == this);

    JSRuntime::AutoLockForOperationCallback lock(cx_->runtime());
    cx_->runtime()->mainThread.asmJSActivationStack_ = prev_;
}

static const unsigned ASM_MODULE_SLOT = 0;
static const unsigned ASM_EXPORT_INDEX_SLOT = 1;

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
    RootedObject moduleObj(cx, &callee->getExtendedSlot(ASM_MODULE_SLOT).toObject());
    AsmJSModule &module = moduleObj->as<AsmJSModuleObject>().module();

    // An exported function points to the code as well as the exported
    // function's signature, which implies the dynamic coercions performed on
    // the arguments.
    unsigned exportIndex = callee->getExtendedSlot(ASM_EXPORT_INDEX_SLOT).toInt32();
    const AsmJSModule::ExportedFunction &func = module.exportedFunction(exportIndex);

    // An asm.js module is specialized to its heap's base address and length
    // which is normally immutable except for the neuter operation that occurs
    // when an ArrayBuffer is transfered. Throw an internal error if we try to
    // run with a neutered heap.
    if (module.maybeHeapBufferObject() && module.maybeHeapBufferObject()->isNeutered()) {
        js_ReportOverRecursed(cx);
        return false;
    }

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

    {
        // Each call into an asm.js module requires an AsmJSActivation record
        // pushed on a stack maintained by the runtime. This record is used for
        // to handle a variety of exceptional things that can happen in asm.js
        // code.
        AsmJSActivation activation(cx, module);

        // Eagerly push an IonContext+JitActivation so that the optimized
        // asm.js-to-Ion FFI call path (which we want to be very fast) can
        // avoid doing so.
        jit::IonContext ictx(cx, nullptr);
        JitActivation jitActivation(cx, /* firstFrameIsConstructing = */ false, /* active */ false);

        // Call the per-exported-function trampoline created by GenerateEntry.
        AsmJSModule::CodePtr enter = module.entryTrampoline(func);
        if (!CALL_GENERATED_ASMJS(enter, coercedArgs.begin(), module.globalData()))
            return false;
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
                                  JSFunction::NATIVE_FUN, cx->global(), name,
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

    uint32_t begin = module.charsBegin();
    uint32_t end = module.charsEnd();
    Rooted<JSFlatString*> src(cx, module.scriptSource()->substring(cx, begin, end));
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
    options.setPrincipals(cx->compartment()->principals)
           .setOriginPrincipals(module.scriptSource()->originPrincipals())
           .setCompileAndGo(false)
           .setNoScriptRval(false);

    if (!frontend::CompileFunctionBody(cx, &fun, options, formals, src->chars(), end - begin))
        return false;

    // Call the function we just recompiled.

    unsigned argc = args.length();

    InvokeArgs args2(cx);
    if (!args2.init(argc))
        return false;

    args2.setCallee(ObjectValue(*fun));
    args2.setThis(args.thisv());
    for (unsigned i = 0; i < argc; i++)
        args2[i].set(args[i]);

    if (!Invoke(cx, args2))
        return false;

    args.rval().set(args2.rval());

    return true;
}

#ifdef MOZ_VTUNE
static bool
SendFunctionsToVTune(JSContext *cx, AsmJSModule &module)
{
    uint8_t *base = module.codeBase();

    for (unsigned i = 0; i < module.numProfiledFunctions(); i++) {
        const AsmJSModule::ProfiledFunction &func = module.profiledFunction(i);

        uint8_t *start = base + func.startCodeOffset;
        uint8_t *end   = base + func.endCodeOffset;
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

    for (unsigned i = 0; i < module.numPerfFunctions(); i++) {
        const AsmJSModule::ProfiledFunction &func = module.perfProfiledFunction(i);
        uintptr_t start = base + (unsigned long) func.startCodeOffset;
        uintptr_t end   = base + (unsigned long) func.endCodeOffset;
        JS_ASSERT(end >= start);
        size_t size = end - start;

        JSAutoByteString bytes;
        const char *name = AtomToPrintableString(cx, func.name, &bytes);
        if (!name)
            return false;

        writePerfSpewerAsmJSFunctionMap(start, size, filename, func.lineno, func.columnIndex, name);
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

        size_t size = func.endCodeOffset - func.startCodeOffset;

        JSAutoByteString bytes;
        const char *name = AtomToPrintableString(cx, func.name, &bytes);
        if (!name)
            return false;

        writePerfSpewerAsmJSBlocksMap(funcBaseAddress, func.startCodeOffset,
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
        size_t firstEntryCode = (size_t) module.entryTrampoline(module.exportedFunction(0));
        writePerfSpewerAsmJSEntriesAndExits(firstEntryCode, (size_t) module.globalData() - firstEntryCode);
    }
    if (!SendBlocksToPerf(cx, module))
        return false;
    if (!SendFunctionsToPerf(cx, module))
        return false;
#endif

    return true;
}


static JSObject *
CreateExportObject(JSContext *cx, HandleObject moduleObj)
{
    AsmJSModule &module = moduleObj->as<AsmJSModuleObject>().module();

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
        if (!DefineNativeProperty(cx, obj, id, val, nullptr, nullptr, JSPROP_ENUMERATE, 0, 0))
            return nullptr;
    }

    return obj;
}

// Implements the semantics of an asm.js module function that has been successfully validated.
static bool
LinkAsmJS(JSContext *cx, unsigned argc, JS::Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // The LinkAsmJS builtin (created by NewAsmJSModuleFunction) is an extended
    // function and stores its module in an extended slot.
    RootedFunction fun(cx, &args.callee().as<JSFunction>());
    RootedObject moduleObj(cx,  &fun->getExtendedSlot(MODULE_FUN_SLOT).toObject());
    AsmJSModule &module = moduleObj->as<AsmJSModuleObject>().module();

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
    JSFunction *moduleFun = NewFunction(cx, NullPtr(), LinkAsmJS, origFun->nargs(),
                                        JSFunction::NATIVE_FUN, NullPtr(), name,
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
    bool rval = args.hasDefined(0) && IsMaybeWrappedNativeFunction(args[0], LinkAsmJS);
    args.rval().set(BooleanValue(rval));
    return true;
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

    JSObject &moduleObj = fun->getExtendedSlot(MODULE_FUN_SLOT).toObject();
    bool loadedFromCache = moduleObj.as<AsmJSModuleObject>().module().loadedFromCache();

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
