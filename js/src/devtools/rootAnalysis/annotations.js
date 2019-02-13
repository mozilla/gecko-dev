/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */

"use strict";

// Ignore calls made through these function pointers
var ignoreIndirectCalls = {
    "mallocSizeOf" : true,
    "aMallocSizeOf" : true,
    "_malloc_message" : true,
    "je_malloc_message" : true,
    "chunk_dalloc" : true,
    "chunk_alloc" : true,
    "__conv" : true,
    "__convf" : true,
    "prerrortable.c:callback_newtable" : true,
    "mozalloc_oom.cpp:void (* gAbortHandler)(size_t)" : true,
};

function indirectCallCannotGC(fullCaller, fullVariable)
{
    var caller = readable(fullCaller);

    // This is usually a simple variable name, but sometimes a full name gets
    // passed through. And sometimes that name is truncated. Examples:
    //   _ZL13gAbortHandler|mozalloc_oom.cpp:void (* gAbortHandler)(size_t)
    //   _ZL14pMutexUnlockFn|umutex.cpp:void (* pMutexUnlockFn)(const void*
    var name = readable(fullVariable);

    if (name in ignoreIndirectCalls)
        return true;

    if (name == "mapper" && caller == "ptio.c:pt_MapError")
        return true;

    if (name == "params" && caller == "PR_ExplodeTime")
        return true;

    if (name == "op" && /GetWeakmapKeyDelegate/.test(caller))
        return true;

    var CheckCallArgs = "AsmJSValidate.cpp:uint8 CheckCallArgs(FunctionCompiler*, js::frontend::ParseNode*, (uint8)(FunctionCompiler*,js::frontend::ParseNode*,Type)*, FunctionCompiler::Call*)";
    if (name == "checkArg" && caller == CheckCallArgs)
        return true;

    // hook called during script finalization which cannot GC.
    if (/CallDestroyScriptHook/.test(caller))
        return true;

    // template method called during marking and hence cannot GC
    if (name == "op" && caller.indexOf("bool js::WeakMap<Key, Value, HashPolicy>::keyNeedsMark(JSObject*)") != -1)
    {
        return true;
    }

    return false;
}

// Ignore calls through functions pointers with these types
var ignoreClasses = {
    "JSStringFinalizer" : true,
    "SprintfState" : true,
    "SprintfStateStr" : true,
    "JSLocaleCallbacks" : true,
    "JSC::ExecutableAllocator" : true,
    "PRIOMethods": true,
    "XPCOMFunctions" : true, // I'm a little unsure of this one
    "_MD_IOVector" : true,
    "malloc_table_t": true, // replace_malloc
    "malloc_hook_table_t": true, // replace_malloc
};

// Ignore calls through TYPE.FIELD, where TYPE is the class or struct name containing
// a function pointer field named FIELD.
var ignoreCallees = {
    "js::Class.trace" : true,
    "js::Class.finalize" : true,
    "JSRuntime.destroyPrincipals" : true,
    "icu_50::UObject.__deleting_dtor" : true, // destructors in ICU code can't cause GC
    "mozilla::CycleCollectedJSRuntime.DescribeCustomObjects" : true, // During tracing, cannot GC.
    "mozilla::CycleCollectedJSRuntime.NoteCustomGCThingXPCOMChildren" : true, // During tracing, cannot GC.
    "PLDHashTableOps.hashKey" : true,
    "z_stream_s.zfree" : true,
    "GrGLInterface.fCallback" : true,
    "std::strstreambuf._M_alloc_fun" : true,
    "std::strstreambuf._M_free_fun" : true
};

function fieldCallCannotGC(csu, fullfield)
{
    if (csu in ignoreClasses)
        return true;
    if (fullfield in ignoreCallees)
        return true;
    return false;
}

function ignoreEdgeUse(edge, variable)
{
    // Functions which should not be treated as using variable.
    if (edge.Kind == "Call") {
        var callee = edge.Exp[0];
        if (callee.Kind == "Var") {
            var name = callee.Variable.Name[0];
            if (/~DebugOnly/.test(name))
                return true;
            if (/~ScopedThreadSafeStringInspector/.test(name))
                return true;
        }
    }

    return false;
}

function ignoreEdgeAddressTaken(edge)
{
    // Functions which may take indirect pointers to unrooted GC things,
    // but will copy them into rooted locations before calling anything
    // that can GC. These parameters should usually be replaced with
    // handles or mutable handles.
    if (edge.Kind == "Call") {
        var callee = edge.Exp[0];
        if (callee.Kind == "Var") {
            var name = callee.Variable.Name[0];
            if (/js::Invoke\(/.test(name))
                return true;
        }
    }

    return false;
}

// Ignore calls of these functions (so ignore any stack containing these)
var ignoreFunctions = {
    "ptio.c:pt_MapError" : true,
    "je_malloc_printf" : true,
    "PR_ExplodeTime" : true,
    "PR_ErrorInstallTable" : true,
    "PR_SetThreadPrivate" : true,
    "JSObject* js::GetWeakmapKeyDelegate(JSObject*)" : true, // FIXME: mark with AutoSuppressGCAnalysis instead
    "uint8 NS_IsMainThread()" : true,

    // Has an indirect call under it by the name "__f", which seemed too
    // generic to ignore by itself.
    "void* std::_Locale_impl::~_Locale_impl(int32)" : true,

    // Bug 1056410 - devirtualization prevents the standard nsISupports::Release heuristic from working
    "uint32 nsXPConnect::Release()" : true,

    // FIXME!
    "NS_LogInit": true,
    "NS_LogTerm": true,
    "NS_LogAddRef": true,
    "NS_LogRelease": true,
    "NS_LogCtor": true,
    "NS_LogDtor": true,
    "NS_LogCOMPtrAddRef": true,
    "NS_LogCOMPtrRelease": true,

    // FIXME!
    "NS_DebugBreak": true,

    // These are a little overzealous -- these destructors *can* GC if they end
    // up wrapping a pending exception. See bug 898815 for the heavyweight fix.
    "void js::AutoCompartment::~AutoCompartment(int32)" : true,
    "void JSAutoCompartment::~JSAutoCompartment(int32)" : true,

    // Bug 948646 - the only thing AutoJSContext's constructor calls
    // is an Init() routine whose entire body is covered with an
    // AutoSuppressGCAnalysis. AutoSafeJSContext is the same thing, just with
    // a different value for the 'aSafe' parameter.
    "void mozilla::AutoJSContext::AutoJSContext(mozilla::detail::GuardObjectNotifier*)" : true,
    "void mozilla::AutoSafeJSContext::~AutoSafeJSContext(int32)" : true,

    // And these are workarounds to avoid even more analysis work,
    // which would sadly still be needed even with bug 898815.
    "void js::AutoCompartment::AutoCompartment(js::ExclusiveContext*, JSCompartment*)": true,

    // The nsScriptNameSpaceManager functions can't actually GC.  They
    // just use a pldhash which has function pointers, which makes the
    // analysis think maybe they can.
    "nsGlobalNameStruct* nsScriptNameSpaceManager::LookupNavigatorName(nsAString_internal*)": true,
    "nsGlobalNameStruct* nsScriptNameSpaceManager::LookupName(nsAString_internal*, uint16**)": true,

    // Similar to heap snapshot mock classes, and GTests below. This posts a
    // synchronous runnable when a GTest fails, and we are pretty sure that the
    // particular runnable it posts can't even GC, but the analysis isn't
    // currently smart enough to determine that. In either case, this is (a)
    // only in GTests, and (b) only when the Gtest has already failed. We have
    // static and dynamic checks for no GC in the non-test code, and in the test
    // code we fall back to only the dynamic checks.
    "void test::RingbufferDumper::OnTestPartResult(testing::TestPartResult*)" : true,
};

function isProtobuf(name)
{
    return name.match(/\bgoogle::protobuf\b/) ||
           name.match(/\bmozilla::devtools::protobuf\b/);
}

function isHeapSnapshotMockClass(name)
{
    return name.match(/\bMockWriter\b/) ||
           name.match(/\bMockDeserializedNode\b/);
}

function isGTest(name)
{
    return name.match(/\btesting::/);
}

function ignoreGCFunction(mangled)
{
    assert(mangled in readableNames);
    var fun = readableNames[mangled][0];

    if (fun in ignoreFunctions)
        return true;

    // The protobuf library, and [de]serialization code generated by the
    // protobuf compiler, uses a _ton_ of function pointers but they are all
    // internal. Easiest to just ignore that mess here.
    if (isProtobuf(fun))
        return true;

    // Ignore anything that goes through heap snapshot GTests or mocked classes
    // used in heap snapshot GTests. GTest and GMock expose a lot of virtual
    // methods and function pointers that could potentially GC after an
    // assertion has already failed (depending on user-provided code), but don't
    // exhibit that behavior currently. For non-test code, we have dynamic and
    // static checks that ensure we don't GC. However, for test code we opt out
    // of static checks here, because of the above stated GMock/GTest issues,
    // and rely on only the dynamic checks provided by AutoAssertCannotGC.
    if (isHeapSnapshotMockClass(fun) || isGTest(fun))
        return true;

    // Templatized function
    if (fun.indexOf("void nsCOMPtr<T>::Assert_NoQueryNeeded()") >= 0)
        return true;

    // XXX modify refillFreeList<NoGC> to not need data flow analysis to understand it cannot GC.
    if (/refillFreeList/.test(fun) && /\(js::AllowGC\)0u/.test(fun))
        return true;
    return false;
}

function isRootedTypeName(name)
{
    if (name == "mozilla::ErrorResult" ||
        name == "JSErrorResult" ||
        name == "WrappableJSErrorResult" ||
        name == "js::frontend::TokenStream" ||
        name == "js::frontend::TokenStream::Position" ||
        name == "ModuleCompiler" ||
        name == "JSAddonId")
    {
        return true;
    }
    return false;
}

function stripUCSAndNamespace(name)
{
    if (name.startsWith('struct '))
        name = name.substr(7);
    if (name.startsWith('class '))
        name = name.substr(6);
    if (name.startsWith('const '))
        name = name.substr(6);
    if (name.startsWith('js::ctypes::'))
        name = name.substr(12);
    if (name.startsWith('js::'))
        name = name.substr(4);
    if (name.startsWith('JS::'))
        name = name.substr(4);
    if (name.startsWith('mozilla::dom::'))
        name = name.substr(14);
    if (name.startsWith('mozilla::'))
        name = name.substr(9);

    return name;
}

function isRootedPointerTypeName(name)
{
    name = stripUCSAndNamespace(name);

    if (name.startsWith('MaybeRooted<'))
        return /\(js::AllowGC\)1u>::RootType/.test(name);

    return name.startsWith('Rooted') || name.startsWith('PersistentRooted');
}

function isUnsafeStorage(typeName)
{
    typeName = stripUCSAndNamespace(typeName);
    return typeName.startsWith('UniquePtr<');
}

function isSuppressConstructor(name)
{
    return name.indexOf("::AutoSuppressGC") != -1
        || name.indexOf("::AutoAssertGCCallback") != -1
        || name.indexOf("::AutoEnterAnalysis") != -1
        || name.indexOf("::AutoSuppressGCAnalysis") != -1
        || name.indexOf("::AutoIgnoreRootingHazards") != -1;
}

// nsISupports subclasses' methods may be scriptable (or overridden
// via binary XPCOM), and so may GC. But some fields just aren't going
// to get overridden with something that can GC.
function isOverridableField(initialCSU, csu, field)
{
    if (csu != 'nsISupports')
        return false;
    if (field == 'GetCurrentJSContext')
        return false;
    if (field == 'IsOnCurrentThread')
        return false;
    if (field == 'GetNativeContext')
        return false;
    if (field == "GetGlobalJSObject")
        return false;
    if (field == "GetIsMainThread")
        return false;
    if (initialCSU == 'nsIXPConnectJSObjectHolder' && field == 'GetJSObject')
        return false;
    if (initialCSU == 'nsIXPConnect' && field == 'GetSafeJSContext')
        return false;
    if (initialCSU == 'nsIScriptContext') {
        if (field == 'GetWindowProxy' || field == 'GetWindowProxyPreserveColor')
            return false;
    }
    return true;
}

function listGCTypes() {
    return [
        'JSObject',
        'JSString',
        'JSFatInlineString',
        'JSExternalString',
        'js::Shape',
        'js::AccessorShape',
        'js::BaseShape',
        'JSScript',
        'js::ObjectGroup',
        'js::LazyScript',
        'js::jit::JitCode',
        'JS::Symbol',
    ];
}

function listGCPointers() {
    return [
        'JS::Value',
        'jsid',

        // AutoCheckCannotGC should also not be held live across a GC function.
        'JS::AutoCheckCannotGC',
    ];
}

function listNonGCTypes() {
    return [
    ];
}

function listNonGCPointers() {
    return [
        // Both of these are safe only because jsids are currently only made
        // from "interned" (pinned) strings. Once that changes, both should be
        // removed from the list.
        'NPIdentifier',
        'XPCNativeMember',
    ];
}

// Flexible mechanism for deciding an arbitrary type is a GCPointer. Its one
// use turned out to be unnecessary due to another change, but the mechanism
// seems useful for something like /Vector.*Something/.
function isGCPointer(typeName)
{
    return false;
}
