/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS script operations.
 */

#include "jsscriptinlines.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"

#include <string.h>

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsprf.h"
#include "jstypes.h"
#include "jsutil.h"
#include "jswrapper.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/SharedContext.h"
#include "gc/Marking.h"
#include "jit/BaselineJIT.h"
#include "jit/IonCode.h"
#include "js/OldDebugAPI.h"
#include "vm/ArgumentsObject.h"
#include "vm/Compression.h"
#include "vm/Debugger.h"
#include "vm/Opcodes.h"
#include "vm/Shape.h"
#include "vm/Xdr.h"

#include "jsfuninlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/ScopeObject-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::frontend;

using mozilla::PodCopy;
using mozilla::PodZero;
using mozilla::RotateLeft;

typedef Rooted<GlobalObject *> RootedGlobalObject;

/* static */ uint32_t
Bindings::argumentsVarIndex(ExclusiveContext *cx, InternalBindingsHandle bindings)
{
    HandlePropertyName arguments = cx->names().arguments;
    BindingIter bi(bindings);
    while (bi->name() != arguments)
        bi++;
    return bi.frameIndex();
}

bool
Bindings::initWithTemporaryStorage(ExclusiveContext *cx, InternalBindingsHandle self,
                                   unsigned numArgs, uint32_t numVars,
                                   Binding *bindingArray)
{
    JS_ASSERT(!self->callObjShape_);
    JS_ASSERT(self->bindingArrayAndFlag_ == TEMPORARY_STORAGE_BIT);
    JS_ASSERT(!(uintptr_t(bindingArray) & TEMPORARY_STORAGE_BIT));
    JS_ASSERT(numArgs <= ARGC_LIMIT);
    JS_ASSERT(numVars <= LOCALNO_LIMIT);
    JS_ASSERT(UINT32_MAX - numArgs >= numVars);

    self->bindingArrayAndFlag_ = uintptr_t(bindingArray) | TEMPORARY_STORAGE_BIT;
    self->numArgs_ = numArgs;
    self->numVars_ = numVars;

    // Get the initial shape to use when creating CallObjects for this script.
    // After creation, a CallObject's shape may change completely (via direct eval() or
    // other operations that mutate the lexical scope). However, since the
    // lexical bindings added to the initial shape are permanent and the
    // allocKind/nfixed of a CallObject cannot change, one may assume that the
    // slot location (whether in the fixed or dynamic slots) of a variable is
    // the same as in the initial shape. (This is assumed by the interpreter and
    // JITs when interpreting/compiling aliasedvar ops.)

    // Since unaliased variables are, by definition, only accessed by local
    // operations and never through the scope chain, only give shapes to
    // aliased variables. While the debugger may observe any scope object at
    // any time, such accesses are mediated by DebugScopeProxy (see
    // DebugScopeProxy::handleUnaliasedAccess).
    uint32_t nslots = CallObject::RESERVED_SLOTS;
    for (BindingIter bi(self); bi; bi++) {
        if (bi->aliased())
            nslots++;
    }

    // Put as many of nslots inline into the object header as possible.
    uint32_t nfixed = gc::GetGCKindSlots(gc::GetGCObjectKind(nslots));

    // Start with the empty shape and then append one shape per aliased binding.
    RootedShape shape(cx,
        EmptyShape::getInitialShape(cx, &CallObject::class_, nullptr, nullptr, nullptr,
                                    nfixed, BaseShape::VAROBJ | BaseShape::DELEGATE));
    if (!shape)
        return false;

#ifdef DEBUG
    HashSet<PropertyName *> added(cx);
    if (!added.init())
        return false;
#endif

    uint32_t slot = CallObject::RESERVED_SLOTS;
    for (BindingIter bi(self); bi; bi++) {
        if (!bi->aliased())
            continue;

#ifdef DEBUG
        // The caller ensures no duplicate aliased names.
        JS_ASSERT(!added.has(bi->name()));
        if (!added.put(bi->name()))
            return false;
#endif

        StackBaseShape stackBase(cx, &CallObject::class_, nullptr, nullptr,
                                 BaseShape::VAROBJ | BaseShape::DELEGATE);

        UnownedBaseShape *base = BaseShape::getUnowned(cx, stackBase);
        if (!base)
            return false;

        unsigned attrs = JSPROP_PERMANENT |
                         JSPROP_ENUMERATE |
                         (bi->kind() == CONSTANT ? JSPROP_READONLY : 0);
        StackShape child(base, NameToId(bi->name()), slot, attrs, 0, 0);

        shape = cx->compartment()->propertyTree.getChild(cx, shape, child);
        if (!shape)
            return false;

        JS_ASSERT(slot < nslots);
        slot++;
    }
    JS_ASSERT(slot == nslots);

    JS_ASSERT(!shape->inDictionary());
    self->callObjShape_.init(shape);
    return true;
}

uint8_t *
Bindings::switchToScriptStorage(Binding *newBindingArray)
{
    JS_ASSERT(bindingArrayUsingTemporaryStorage());
    JS_ASSERT(!(uintptr_t(newBindingArray) & TEMPORARY_STORAGE_BIT));

    if (count() > 0)
        PodCopy(newBindingArray, bindingArray(), count());
    bindingArrayAndFlag_ = uintptr_t(newBindingArray);
    return reinterpret_cast<uint8_t *>(newBindingArray + count());
}

bool
Bindings::clone(JSContext *cx, InternalBindingsHandle self,
                uint8_t *dstScriptData, HandleScript srcScript)
{
    /* The clone has the same bindingArray_ offset as 'src'. */
    Bindings &src = srcScript->bindings;
    ptrdiff_t off = (uint8_t *)src.bindingArray() - srcScript->data;
    JS_ASSERT(off >= 0);
    JS_ASSERT(size_t(off) <= srcScript->dataSize());
    Binding *dstPackedBindings = (Binding *)(dstScriptData + off);

    /*
     * Since atoms are shareable throughout the runtime, we can simply copy
     * the source's bindingArray directly.
     */
    if (!initWithTemporaryStorage(cx, self, src.numArgs(), src.numVars(), src.bindingArray()))
        return false;
    self->switchToScriptStorage(dstPackedBindings);
    return true;
}

/* static */ Bindings
GCMethods<Bindings>::initial()
{
    return Bindings();
}

template<XDRMode mode>
static bool
XDRScriptBindings(XDRState<mode> *xdr, LifoAllocScope &las, unsigned numArgs, uint32_t numVars,
                  HandleScript script)
{
    JSContext *cx = xdr->cx();

    if (mode == XDR_ENCODE) {
        for (BindingIter bi(script); bi; bi++) {
            RootedAtom atom(cx, bi->name());
            if (!XDRAtom(xdr, &atom))
                return false;
        }

        for (BindingIter bi(script); bi; bi++) {
            uint8_t u8 = (uint8_t(bi->kind()) << 1) | uint8_t(bi->aliased());
            if (!xdr->codeUint8(&u8))
                return false;
        }
    } else {
        uint32_t nameCount = numArgs + numVars;

        AutoValueVector atoms(cx);
        if (!atoms.resize(nameCount))
            return false;
        for (uint32_t i = 0; i < nameCount; i++) {
            RootedAtom atom(cx);
            if (!XDRAtom(xdr, &atom))
                return false;
            atoms[i] = StringValue(atom);
        }

        Binding *bindingArray = las.alloc().newArrayUninitialized<Binding>(nameCount);
        if (!bindingArray)
            return false;
        for (uint32_t i = 0; i < nameCount; i++) {
            uint8_t u8;
            if (!xdr->codeUint8(&u8))
                return false;

            PropertyName *name = atoms[i].toString()->asAtom().asPropertyName();
            BindingKind kind = BindingKind(u8 >> 1);
            bool aliased = bool(u8 & 1);

            bindingArray[i] = Binding(name, kind, aliased);
        }

        InternalBindingsHandle bindings(script, &script->bindings);
        if (!Bindings::initWithTemporaryStorage(cx, bindings, numArgs, numVars, bindingArray))
            return false;
    }

    return true;
}

bool
Bindings::bindingIsAliased(uint32_t bindingIndex)
{
    JS_ASSERT(bindingIndex < count());
    return bindingArray()[bindingIndex].aliased();
}

void
Bindings::trace(JSTracer *trc)
{
    if (callObjShape_)
        MarkShape(trc, &callObjShape_, "callObjShape");

    /*
     * As the comment in Bindings explains, bindingsArray may point into freed
     * storage when bindingArrayUsingTemporaryStorage so we don't mark it.
     * Note: during compilation, atoms are already kept alive by gcKeepAtoms.
     */
    if (bindingArrayUsingTemporaryStorage())
        return;

    for (Binding *b = bindingArray(), *end = b + count(); b != end; b++) {
        PropertyName *name = b->name();
        MarkStringUnbarriered(trc, &name, "bindingArray");
    }
}

bool
js::FillBindingVector(HandleScript fromScript, BindingVector *vec)
{
    for (BindingIter bi(fromScript); bi; bi++) {
        if (!vec->append(*bi))
            return false;
    }

    return true;
}

template<XDRMode mode>
bool
js::XDRScriptConst(XDRState<mode> *xdr, MutableHandleValue vp)
{
    JSContext *cx = xdr->cx();

    /*
     * A script constant can be an arbitrary primitive value as they are used
     * to implement JSOP_LOOKUPSWITCH. But they cannot be objects, see
     * bug 407186.
     */
    enum ConstTag {
        SCRIPT_INT     = 0,
        SCRIPT_DOUBLE  = 1,
        SCRIPT_ATOM    = 2,
        SCRIPT_TRUE    = 3,
        SCRIPT_FALSE   = 4,
        SCRIPT_NULL    = 5,
        SCRIPT_OBJECT  = 6,
        SCRIPT_VOID    = 7,
        SCRIPT_HOLE    = 8
    };

    uint32_t tag;
    if (mode == XDR_ENCODE) {
        if (vp.isInt32()) {
            tag = SCRIPT_INT;
        } else if (vp.isDouble()) {
            tag = SCRIPT_DOUBLE;
        } else if (vp.isString()) {
            tag = SCRIPT_ATOM;
        } else if (vp.isTrue()) {
            tag = SCRIPT_TRUE;
        } else if (vp.isFalse()) {
            tag = SCRIPT_FALSE;
        } else if (vp.isNull()) {
            tag = SCRIPT_NULL;
        } else if (vp.isObject()) {
            tag = SCRIPT_OBJECT;
        } else if (vp.isMagic(JS_ELEMENTS_HOLE)) {
            tag = SCRIPT_HOLE;
        } else {
            JS_ASSERT(vp.isUndefined());
            tag = SCRIPT_VOID;
        }
    }

    if (!xdr->codeUint32(&tag))
        return false;

    switch (tag) {
      case SCRIPT_INT: {
        uint32_t i;
        if (mode == XDR_ENCODE)
            i = uint32_t(vp.toInt32());
        if (!xdr->codeUint32(&i))
            return false;
        if (mode == XDR_DECODE)
            vp.set(Int32Value(int32_t(i)));
        break;
      }
      case SCRIPT_DOUBLE: {
        double d;
        if (mode == XDR_ENCODE)
            d = vp.toDouble();
        if (!xdr->codeDouble(&d))
            return false;
        if (mode == XDR_DECODE)
            vp.set(DoubleValue(d));
        break;
      }
      case SCRIPT_ATOM: {
        RootedAtom atom(cx);
        if (mode == XDR_ENCODE)
            atom = &vp.toString()->asAtom();
        if (!XDRAtom(xdr, &atom))
            return false;
        if (mode == XDR_DECODE)
            vp.set(StringValue(atom));
        break;
      }
      case SCRIPT_TRUE:
        if (mode == XDR_DECODE)
            vp.set(BooleanValue(true));
        break;
      case SCRIPT_FALSE:
        if (mode == XDR_DECODE)
            vp.set(BooleanValue(false));
        break;
      case SCRIPT_NULL:
        if (mode == XDR_DECODE)
            vp.set(NullValue());
        break;
      case SCRIPT_OBJECT: {
        RootedObject obj(cx);
        if (mode == XDR_ENCODE)
            obj = &vp.toObject();

        if (!XDRObjectLiteral(xdr, &obj))
            return false;

        if (mode == XDR_DECODE)
            vp.setObject(*obj);
        break;
      }
      case SCRIPT_VOID:
        if (mode == XDR_DECODE)
            vp.set(UndefinedValue());
        break;
      case SCRIPT_HOLE:
        if (mode == XDR_DECODE)
            vp.setMagic(JS_ELEMENTS_HOLE);
        break;
    }
    return true;
}

template bool
js::XDRScriptConst(XDRState<XDR_ENCODE> *, MutableHandleValue);

template bool
js::XDRScriptConst(XDRState<XDR_DECODE> *, MutableHandleValue);

static inline uint32_t
FindScopeObjectIndex(JSScript *script, NestedScopeObject &scope)
{
    ObjectArray *objects = script->objects();
    HeapPtrObject *vector = objects->vector;
    unsigned length = objects->length;
    for (unsigned i = 0; i < length; ++i) {
        if (vector[i] == &scope)
            return i;
    }

    MOZ_ASSUME_UNREACHABLE("Scope not found");
}

static bool
SaveSharedScriptData(ExclusiveContext *, Handle<JSScript *>, SharedScriptData *, uint32_t);

enum XDRClassKind {
    CK_BlockObject = 0,
    CK_JSFunction  = 1,
    CK_JSObject    = 2
};

template<XDRMode mode>
bool
js::XDRScript(XDRState<mode> *xdr, HandleObject enclosingScope, HandleScript enclosingScript,
              HandleFunction fun, MutableHandleScript scriptp)
{
    /* NB: Keep this in sync with CloneScript. */

    enum ScriptBits {
        NoScriptRval,
        SavedCallerFun,
        Strict,
        ContainsDynamicNameAccess,
        FunHasExtensibleScope,
        FunNeedsDeclEnvObject,
        FunHasAnyAliasedFormal,
        ArgumentsHasVarBinding,
        NeedsArgsObj,
        IsGeneratorExp,
        IsLegacyGenerator,
        IsStarGenerator,
        OwnSource,
        ExplicitUseStrict,
        SelfHosted,
        IsCompileAndGo,
        HasSingleton,
        TreatAsRunOnce
    };

    uint32_t length, lineno, column, nslots;
    uint32_t natoms, nsrcnotes, i;
    uint32_t nconsts, nobjects, nregexps, ntrynotes, nblockscopes;
    uint32_t prologLength, version;
    uint32_t funLength = 0;
    uint32_t nTypeSets = 0;
    uint32_t scriptBits = 0;

    JSContext *cx = xdr->cx();
    RootedScript script(cx);
    natoms = nsrcnotes = 0;
    nconsts = nobjects = nregexps = ntrynotes = nblockscopes = 0;

    /* XDR arguments and vars. */
    uint16_t nargs = 0;
    uint32_t nvars = 0;
    if (mode == XDR_ENCODE) {
        script = scriptp.get();
        JS_ASSERT_IF(enclosingScript, enclosingScript->compartment() == script->compartment());

        nargs = script->bindings.numArgs();
        nvars = script->bindings.numVars();
    }
    if (!xdr->codeUint16(&nargs))
        return false;
    if (!xdr->codeUint32(&nvars))
        return false;

    if (mode == XDR_ENCODE)
        length = script->length();
    if (!xdr->codeUint32(&length))
        return false;

    if (mode == XDR_ENCODE) {
        prologLength = script->mainOffset();
        JS_ASSERT(script->getVersion() != JSVERSION_UNKNOWN);
        version = script->getVersion();
        lineno = script->lineno();
        column = script->column();
        nslots = (uint32_t)script->nslots();
        nslots = (uint32_t)((script->staticLevel() << 16) | script->nslots());
        natoms = script->natoms();

        nsrcnotes = script->numNotes();

        if (script->hasConsts())
            nconsts = script->consts()->length;
        if (script->hasObjects())
            nobjects = script->objects()->length;
        if (script->hasRegexps())
            nregexps = script->regexps()->length;
        if (script->hasTrynotes())
            ntrynotes = script->trynotes()->length;
        if (script->hasBlockScopes())
            nblockscopes = script->blockScopes()->length;

        nTypeSets = script->nTypeSets();
        funLength = script->funLength();

        if (script->noScriptRval())
            scriptBits |= (1 << NoScriptRval);
        if (script->savedCallerFun())
            scriptBits |= (1 << SavedCallerFun);
        if (script->strict())
            scriptBits |= (1 << Strict);
        if (script->explicitUseStrict())
            scriptBits |= (1 << ExplicitUseStrict);
        if (script->selfHosted())
            scriptBits |= (1 << SelfHosted);
        if (script->bindingsAccessedDynamically())
            scriptBits |= (1 << ContainsDynamicNameAccess);
        if (script->funHasExtensibleScope())
            scriptBits |= (1 << FunHasExtensibleScope);
        if (script->funNeedsDeclEnvObject())
            scriptBits |= (1 << FunNeedsDeclEnvObject);
        if (script->funHasAnyAliasedFormal())
            scriptBits |= (1 << FunHasAnyAliasedFormal);
        if (script->argumentsHasVarBinding())
            scriptBits |= (1 << ArgumentsHasVarBinding);
        if (script->analyzedArgsUsage() && script->needsArgsObj())
            scriptBits |= (1 << NeedsArgsObj);
        if (!enclosingScript || enclosingScript->scriptSource() != script->scriptSource())
            scriptBits |= (1 << OwnSource);
        if (script->isGeneratorExp())
            scriptBits |= (1 << IsGeneratorExp);
        if (script->isLegacyGenerator())
            scriptBits |= (1 << IsLegacyGenerator);
        if (script->isStarGenerator())
            scriptBits |= (1 << IsStarGenerator);
        if (script->compileAndGo())
            scriptBits |= (1 << IsCompileAndGo);
        if (script->hasSingletons())
            scriptBits |= (1 << HasSingleton);
        if (script->treatAsRunOnce())
            scriptBits |= (1 << TreatAsRunOnce);
    }

    if (!xdr->codeUint32(&prologLength))
        return false;
    if (!xdr->codeUint32(&version))
        return false;

    // To fuse allocations, we need lengths of all embedded arrays early.
    if (!xdr->codeUint32(&natoms))
        return false;
    if (!xdr->codeUint32(&nsrcnotes))
        return false;
    if (!xdr->codeUint32(&nconsts))
        return false;
    if (!xdr->codeUint32(&nobjects))
        return false;
    if (!xdr->codeUint32(&nregexps))
        return false;
    if (!xdr->codeUint32(&ntrynotes))
        return false;
    if (!xdr->codeUint32(&nblockscopes))
        return false;
    if (!xdr->codeUint32(&nTypeSets))
        return false;
    if (!xdr->codeUint32(&funLength))
        return false;
    if (!xdr->codeUint32(&scriptBits))
        return false;

    if (mode == XDR_DECODE) {
        JSVersion version_ = JSVersion(version);
        JS_ASSERT((version_ & VersionFlags::MASK) == unsigned(version_));

        // staticLevel is set below.
        CompileOptions options(cx);
        options.setVersion(version_)
               .setNoScriptRval(!!(scriptBits & (1 << NoScriptRval)))
               .setSelfHostingMode(!!(scriptBits & (1 << SelfHosted)));
        RootedScriptSource sourceObject(cx);
        if (scriptBits & (1 << OwnSource)) {
            ScriptSource *ss = cx->new_<ScriptSource>(xdr->originPrincipals());
            if (!ss)
                return false;
            /*
             * We use this CompileOptions only to initialize the
             * ScriptSourceObject. Most CompileOptions fields aren't used by
             * ScriptSourceObject, and those that are (element; elementAttributeName)
             * aren't preserved by XDR. So this can be simple.
             */
            CompileOptions options(cx);
            sourceObject = ScriptSourceObject::create(cx, ss, options);
            if (!sourceObject)
                return false;
        } else {
            JS_ASSERT(enclosingScript);
            // When decoding, all the scripts and the script source object
            // are in the same compartment, so the script's source object
            // should never be a cross-compartment wrapper.
            JS_ASSERT(enclosingScript->sourceObject()->is<ScriptSourceObject>());
            sourceObject = &enclosingScript->sourceObject()->as<ScriptSourceObject>();
        }
        script = JSScript::Create(cx, enclosingScope, !!(scriptBits & (1 << SavedCallerFun)),
                                  options, /* staticLevel = */ 0, sourceObject, 0, 0);
        if (!script)
            return false;
    }

    /* JSScript::partiallyInit assumes script->bindings is fully initialized. */
    LifoAllocScope las(&cx->tempLifoAlloc());
    if (!XDRScriptBindings(xdr, las, nargs, nvars, script))
        return false;

    if (mode == XDR_DECODE) {
        if (!JSScript::partiallyInit(cx, script, nconsts, nobjects, nregexps, ntrynotes,
                                     nblockscopes, nTypeSets))
        {
            return false;
        }

        JS_ASSERT(!script->mainOffset());
        script->mainOffset_ = prologLength;
        script->setLength(length);
        script->funLength_ = funLength;

        scriptp.set(script);

        if (scriptBits & (1 << Strict))
            script->strict_ = true;
        if (scriptBits & (1 << ExplicitUseStrict))
            script->explicitUseStrict_ = true;
        if (scriptBits & (1 << ContainsDynamicNameAccess))
            script->bindingsAccessedDynamically_ = true;
        if (scriptBits & (1 << FunHasExtensibleScope))
            script->funHasExtensibleScope_ = true;
        if (scriptBits & (1 << FunNeedsDeclEnvObject))
            script->funNeedsDeclEnvObject_ = true;
        if (scriptBits & (1 << FunHasAnyAliasedFormal))
            script->funHasAnyAliasedFormal_ = true;
        if (scriptBits & (1 << ArgumentsHasVarBinding))
            script->setArgumentsHasVarBinding();
        if (scriptBits & (1 << NeedsArgsObj))
            script->setNeedsArgsObj(true);
        if (scriptBits & (1 << IsGeneratorExp))
            script->isGeneratorExp_ = true;
        if (scriptBits & (1 << IsCompileAndGo))
            script->compileAndGo_ = true;
        if (scriptBits & (1 << HasSingleton))
            script->hasSingletons_ = true;
        if (scriptBits & (1 << TreatAsRunOnce))
            script->treatAsRunOnce_ = true;

        if (scriptBits & (1 << IsLegacyGenerator)) {
            JS_ASSERT(!(scriptBits & (1 << IsStarGenerator)));
            script->setGeneratorKind(LegacyGenerator);
        } else if (scriptBits & (1 << IsStarGenerator))
            script->setGeneratorKind(StarGenerator);
    }

    JS_STATIC_ASSERT(sizeof(jsbytecode) == 1);
    JS_STATIC_ASSERT(sizeof(jssrcnote) == 1);

    if (scriptBits & (1 << OwnSource)) {
        if (!script->scriptSource()->performXDR<mode>(xdr))
            return false;
    }
    if (!xdr->codeUint32(&script->sourceStart_))
        return false;
    if (!xdr->codeUint32(&script->sourceEnd_))
        return false;

    if (!xdr->codeUint32(&lineno) || !xdr->codeUint32(&column) ||
        !xdr->codeUint32(&nslots))
    {
        return false;
    }

    if (mode == XDR_DECODE) {
        script->lineno_ = lineno;
        script->column_ = column;
        script->nslots_ = uint16_t(nslots);
        script->staticLevel_ = uint16_t(nslots >> 16);
    }

    jsbytecode *code = script->code();
    SharedScriptData *ssd;
    if (mode == XDR_DECODE) {
        ssd = SharedScriptData::new_(cx, length, nsrcnotes, natoms);
        if (!ssd)
            return false;
        code = ssd->data;
        if (natoms != 0) {
            script->natoms_ = natoms;
            script->atoms = ssd->atoms();
        }
    }

    if (!xdr->codeBytes(code, length) || !xdr->codeBytes(code + length, nsrcnotes)) {
        if (mode == XDR_DECODE)
            js_free(ssd);
        return false;
    }

    for (i = 0; i != natoms; ++i) {
        if (mode == XDR_DECODE) {
            RootedAtom tmp(cx);
            if (!XDRAtom(xdr, &tmp))
                return false;
            script->atoms[i].init(tmp);
        } else {
            RootedAtom tmp(cx, script->atoms[i]);
            if (!XDRAtom(xdr, &tmp))
                return false;
        }
    }

    if (mode == XDR_DECODE) {
        if (!SaveSharedScriptData(cx, script, ssd, nsrcnotes))
            return false;
    }

    if (nconsts) {
        HeapValue *vector = script->consts()->vector;
        RootedValue val(cx);
        for (i = 0; i != nconsts; ++i) {
            if (mode == XDR_ENCODE)
                val = vector[i];
            if (!XDRScriptConst(xdr, &val))
                return false;
            if (mode == XDR_DECODE)
                vector[i].init(val);
        }
    }

    /*
     * Here looping from 0-to-length to xdr objects is essential to ensure that
     * all references to enclosing blocks (via FindScopeObjectIndex below) happen
     * after the enclosing block has been XDR'd.
     */
    for (i = 0; i != nobjects; ++i) {
        HeapPtr<JSObject> *objp = &script->objects()->vector[i];
        XDRClassKind classk;

        if (mode == XDR_ENCODE) {
            JSObject *obj = *objp;
            if (obj->is<BlockObject>())
                classk = CK_BlockObject;
            else if (obj->is<JSFunction>())
                classk = CK_JSFunction;
            else if (obj->is<JSObject>() || obj->is<ArrayObject>())
                classk = CK_JSObject;
            else
                MOZ_ASSUME_UNREACHABLE("Cannot encode this class of object.");
        }

        if (!xdr->codeEnum32(&classk))
            return false;

        switch (classk) {
          case CK_BlockObject: {
            /* Code the nested block's enclosing scope. */
            uint32_t blockEnclosingScopeIndex = 0;
            if (mode == XDR_ENCODE) {
                NestedScopeObject &scope = (*objp)->as<NestedScopeObject>();
                if (NestedScopeObject *enclosing = scope.enclosingNestedScope())
                    blockEnclosingScopeIndex = FindScopeObjectIndex(script, *enclosing);
                else
                    blockEnclosingScopeIndex = UINT32_MAX;
            }
            if (!xdr->codeUint32(&blockEnclosingScopeIndex))
                return false;
            Rooted<JSObject*> blockEnclosingScope(cx);
            if (mode == XDR_DECODE) {
                if (blockEnclosingScopeIndex != UINT32_MAX) {
                    JS_ASSERT(blockEnclosingScopeIndex < i);
                    blockEnclosingScope = script->objects()->vector[blockEnclosingScopeIndex];
                } else {
                    blockEnclosingScope = fun;
                }
            }

            Rooted<StaticBlockObject*> tmp(cx, static_cast<StaticBlockObject *>(objp->get()));
            if (!XDRStaticBlockObject(xdr, blockEnclosingScope, tmp.address()))
                return false;
            *objp = tmp;
            break;
          }

          case CK_JSFunction: {
            /* Code the nested function's enclosing scope. */
            uint32_t funEnclosingScopeIndex = 0;
            if (mode == XDR_ENCODE) {
                JSScript *innerScript = (*objp)->as<JSFunction>().getOrCreateScript(cx);
                if (!innerScript)
                    return false;
                RootedObject staticScope(cx, innerScript->enclosingStaticScope());
                StaticScopeIter<NoGC> ssi(staticScope);
                if (ssi.done() || ssi.type() == StaticScopeIter<NoGC>::FUNCTION) {
                    JS_ASSERT(ssi.done() == !fun);
                    funEnclosingScopeIndex = UINT32_MAX;
                } else {
                    funEnclosingScopeIndex = FindScopeObjectIndex(script, ssi.block());
                    JS_ASSERT(funEnclosingScopeIndex < i);
                }
            }
            if (!xdr->codeUint32(&funEnclosingScopeIndex))
                return false;
            Rooted<JSObject*> funEnclosingScope(cx);
            if (mode == XDR_DECODE) {
                if (funEnclosingScopeIndex == UINT32_MAX) {
                    funEnclosingScope = fun;
                } else {
                    JS_ASSERT(funEnclosingScopeIndex < i);
                    funEnclosingScope = script->objects()->vector[funEnclosingScopeIndex];
                }
            }

            RootedObject tmp(cx, *objp);
            if (!XDRInterpretedFunction(xdr, funEnclosingScope, script, &tmp))
                return false;
            *objp = tmp;
            break;
          }

          case CK_JSObject: {
            /* Code object literal. */
            RootedObject tmp(cx, *objp);
            if (!XDRObjectLiteral(xdr, &tmp))
                return false;
            *objp = tmp;
            break;
          }

          default: {
            MOZ_ASSUME_UNREACHABLE("Unknown class kind.");
            return false;
          }
        }
    }

    for (i = 0; i != nregexps; ++i) {
        if (!XDRScriptRegExpObject(xdr, &script->regexps()->vector[i]))
            return false;
    }

    if (ntrynotes != 0) {
        JSTryNote *tnfirst = script->trynotes()->vector;
        JS_ASSERT(script->trynotes()->length == ntrynotes);
        JSTryNote *tn = tnfirst + ntrynotes;
        do {
            --tn;
            if (!xdr->codeUint8(&tn->kind) ||
                !xdr->codeUint32(&tn->stackDepth) ||
                !xdr->codeUint32(&tn->start) ||
                !xdr->codeUint32(&tn->length)) {
                return false;
            }
        } while (tn != tnfirst);
    }

    for (i = 0; i < nblockscopes; ++i) {
        BlockScopeNote *note = &script->blockScopes()->vector[i];
        if (!xdr->codeUint32(&note->index) ||
            !xdr->codeUint32(&note->start) ||
            !xdr->codeUint32(&note->length) ||
            !xdr->codeUint32(&note->parent))
        {
            return false;
        }
    }

    if (mode == XDR_DECODE) {
        scriptp.set(script);

        /* see BytecodeEmitter::tellDebuggerAboutCompiledScript */
        CallNewScriptHook(cx, script, fun);
        if (!fun) {
            RootedGlobalObject global(cx, script->compileAndGo() ? &script->global() : nullptr);
            Debugger::onNewScript(cx, script, global);
        }
    }

    return true;
}

template bool
js::XDRScript(XDRState<XDR_ENCODE> *, HandleObject, HandleScript, HandleFunction,
              MutableHandleScript);

template bool
js::XDRScript(XDRState<XDR_DECODE> *, HandleObject, HandleScript, HandleFunction,
              MutableHandleScript);

void
JSScript::setSourceObject(JSObject *object)
{
    JS_ASSERT(compartment() == object->compartment());
    sourceObject_ = object;
}

js::ScriptSource *
JSScript::scriptSource() const {
    return UncheckedUnwrap(sourceObject())->as<ScriptSourceObject>().source();
}

bool
JSScript::initScriptCounts(JSContext *cx)
{
    JS_ASSERT(!hasScriptCounts());

    size_t n = 0;

    for (jsbytecode *pc = code(); pc < codeEnd(); pc += GetBytecodeLength(pc))
        n += PCCounts::numCounts(JSOp(*pc));

    size_t bytes = (length() * sizeof(PCCounts)) + (n * sizeof(double));
    char *base = (char *) cx->calloc_(bytes);
    if (!base)
        return false;

    /* Create compartment's scriptCountsMap if necessary. */
    ScriptCountsMap *map = compartment()->scriptCountsMap;
    if (!map) {
        map = cx->new_<ScriptCountsMap>();
        if (!map || !map->init()) {
            js_free(base);
            js_delete(map);
            return false;
        }
        compartment()->scriptCountsMap = map;
    }

    char *cursor = base;

    ScriptCounts scriptCounts;
    scriptCounts.pcCountsVector = (PCCounts *) cursor;
    cursor += length() * sizeof(PCCounts);

    for (jsbytecode *pc = code(); pc < codeEnd(); pc += GetBytecodeLength(pc)) {
        JS_ASSERT(uintptr_t(cursor) % sizeof(double) == 0);
        scriptCounts.pcCountsVector[pcToOffset(pc)].counts = (double *) cursor;
        size_t capacity = PCCounts::numCounts(JSOp(*pc));
#ifdef DEBUG
        scriptCounts.pcCountsVector[pcToOffset(pc)].capacity = capacity;
#endif
        cursor += capacity * sizeof(double);
    }

    if (!map->putNew(this, scriptCounts)) {
        js_free(base);
        return false;
    }
    hasScriptCounts_ = true; // safe to set this;  we can't fail after this point

    JS_ASSERT(size_t(cursor - base) == bytes);

    /* Enable interrupts in any interpreter frames running on this script. */
    for (ActivationIterator iter(cx->runtime()); !iter.done(); ++iter) {
        if (iter.activation()->isInterpreter())
            iter.activation()->asInterpreter()->enableInterruptsIfRunning(this);
    }

    return true;
}

static inline ScriptCountsMap::Ptr GetScriptCountsMapEntry(JSScript *script)
{
    JS_ASSERT(script->hasScriptCounts());
    ScriptCountsMap *map = script->compartment()->scriptCountsMap;
    ScriptCountsMap::Ptr p = map->lookup(script);
    JS_ASSERT(p);
    return p;
}

js::PCCounts
JSScript::getPCCounts(jsbytecode *pc) {
    JS_ASSERT(containsPC(pc));
    ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
    return p->value().pcCountsVector[pcToOffset(pc)];
}

void
JSScript::addIonCounts(jit::IonScriptCounts *ionCounts)
{
    ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
    if (p->value().ionCounts)
        ionCounts->setPrevious(p->value().ionCounts);
    p->value().ionCounts = ionCounts;
}

jit::IonScriptCounts *
JSScript::getIonCounts()
{
    ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
    return p->value().ionCounts;
}

ScriptCounts
JSScript::releaseScriptCounts()
{
    ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
    ScriptCounts counts = p->value();
    compartment()->scriptCountsMap->remove(p);
    hasScriptCounts_ = false;
    return counts;
}

void
JSScript::destroyScriptCounts(FreeOp *fop)
{
    if (hasScriptCounts()) {
        ScriptCounts scriptCounts = releaseScriptCounts();
        scriptCounts.destroy(fop);
    }
}

void
ScriptSourceObject::setSource(ScriptSource *source)
{
    if (source)
        source->incref();
    if (this->source())
        this->source()->decref();
    setReservedSlot(SOURCE_SLOT, PrivateValue(source));
}

JSObject *
ScriptSourceObject::element() const
{
    return getReservedSlot(ELEMENT_SLOT).toObjectOrNull();
}

void
ScriptSourceObject::initElement(HandleObject element)
{
    JS_ASSERT(getReservedSlot(ELEMENT_SLOT).isNull());
    setReservedSlot(ELEMENT_SLOT, ObjectOrNullValue(element));
}

const Value &
ScriptSourceObject::elementAttributeName() const
{
    const Value &prop = getReservedSlot(ELEMENT_PROPERTY_SLOT);
    JS_ASSERT(prop.isUndefined() || prop.isString());
    return prop;
}

void
ScriptSourceObject::finalize(FreeOp *fop, JSObject *obj)
{
    // ScriptSource::setSource automatically takes care of the refcount
    obj->as<ScriptSourceObject>().setSource(nullptr);
}

const Class ScriptSourceObject::class_ = {
    "ScriptSource",
    JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) |
    JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,        /* addProperty */
    JS_DeletePropertyStub,  /* delProperty */
    JS_PropertyStub,        /* getProperty */
    JS_StrictPropertyStub,  /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    ScriptSourceObject::finalize
};

ScriptSourceObject *
ScriptSourceObject::create(ExclusiveContext *cx, ScriptSource *source,
                           const ReadOnlyCompileOptions &options)
{
    RootedObject object(cx, NewObjectWithGivenProto(cx, &class_, nullptr, cx->global()));
    if (!object)
        return nullptr;
    RootedScriptSource sourceObject(cx, &object->as<ScriptSourceObject>());

    source->incref();
    sourceObject->initSlot(SOURCE_SLOT, PrivateValue(source));
    sourceObject->initSlot(ELEMENT_SLOT, ObjectOrNullValue(options.element()));
    if (options.elementAttributeName())
        sourceObject->initSlot(ELEMENT_PROPERTY_SLOT, StringValue(options.elementAttributeName()));
    else
        sourceObject->initSlot(ELEMENT_PROPERTY_SLOT, UndefinedValue());

    return sourceObject;
}

static const unsigned char emptySource[] = "";

/* Adjust the amount of memory this script source uses for source data,
   reallocating if needed. */
bool
ScriptSource::adjustDataSize(size_t nbytes)
{
    // Allocating 0 bytes has undefined behavior, so special-case it.
    if (nbytes == 0) {
        if (data.compressed != emptySource)
            js_free(data.compressed);
        data.compressed = const_cast<unsigned char *>(emptySource);
        return true;
    }

    // |data.compressed| can be nullptr.
    void *buf = js_realloc(data.compressed, nbytes);
    if (!buf && data.compressed != emptySource)
        js_free(data.compressed);
    data.compressed = static_cast<unsigned char *>(buf);
    return !!data.compressed;
}

/* static */ bool
JSScript::loadSource(JSContext *cx, ScriptSource *ss, bool *worked)
{
    JS_ASSERT(!ss->hasSourceData());
    *worked = false;
    if (!cx->runtime()->sourceHook || !ss->sourceRetrievable())
        return true;
    jschar *src = nullptr;
    size_t length;
    if (!cx->runtime()->sourceHook->load(cx, ss->filename(), &src, &length))
        return false;
    if (!src)
        return true;
    ss->setSource(src, length);
    *worked = true;
    return true;
}

JSFlatString *
JSScript::sourceData(JSContext *cx)
{
    JS_ASSERT(scriptSource()->hasSourceData());
    return scriptSource()->substring(cx, sourceStart(), sourceEnd());
}

SourceDataCache::AutoSuppressPurge::AutoSuppressPurge(JSContext *cx)
 : cache_(cx->runtime()->sourceDataCache)
{
    oldValue_ = cache_.numSuppressPurges_++;
}

SourceDataCache::AutoSuppressPurge::~AutoSuppressPurge()
{
    cache_.numSuppressPurges_--;
    JS_ASSERT(cache_.numSuppressPurges_ == oldValue_);
}

const jschar *
SourceDataCache::lookup(ScriptSource *ss, const AutoSuppressPurge &asp)
{
    JS_ASSERT(this == &asp.cache());
    if (!map_)
        return nullptr;
    if (Map::Ptr p = map_->lookup(ss))
        return p->value();
    return nullptr;
}

bool
SourceDataCache::put(ScriptSource *ss, const jschar *str, const AutoSuppressPurge &asp)
{
    JS_ASSERT(this == &asp.cache());

    if (!map_) {
        map_ = js_new<Map>();
        if (!map_)
            return false;

        if (!map_->init()) {
            js_delete(map_);
            map_ = nullptr;
            return false;
        }
    }

    return map_->put(ss, str);
}

void
SourceDataCache::purge()
{
    if (!map_ || numSuppressPurges_ > 0)
        return;

    for (Map::Range r = map_->all(); !r.empty(); r.popFront())
        js_delete(const_cast<jschar*>(r.front().value()));

    js_delete(map_);
    map_ = nullptr;
}

size_t
SourceDataCache::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    size_t n = 0;
    if (map_ && !map_->empty()) {
        n += map_->sizeOfIncludingThis(mallocSizeOf);
        for (Map::Range r = map_->all(); !r.empty(); r.popFront()) {
            const jschar *v = r.front().value();
            n += mallocSizeOf(v);
        }
    }
    return n;
}

const jschar *
ScriptSource::chars(JSContext *cx, const SourceDataCache::AutoSuppressPurge &asp)
{
    if (const jschar *chars = getOffThreadCompressionChars(cx))
        return chars;
    JS_ASSERT(ready());

#ifdef USE_ZLIB
    if (compressed()) {
        if (const jschar *decompressed = cx->runtime()->sourceDataCache.lookup(this, asp))
            return decompressed;
      
        const size_t nbytes = sizeof(jschar) * (length_ + 1);
        jschar *decompressed = static_cast<jschar *>(js_malloc(nbytes));
        if (!decompressed)
            return nullptr;

        if (!DecompressString(data.compressed, compressedLength_,
                              reinterpret_cast<unsigned char *>(decompressed), nbytes)) {
            JS_ReportOutOfMemory(cx);
            js_free(decompressed);
            return nullptr;
        }

        decompressed[length_] = 0;

        if (!cx->runtime()->sourceDataCache.put(this, decompressed, asp)) {
            JS_ReportOutOfMemory(cx);
            js_free(decompressed);
            return nullptr;
        }

        return decompressed;
    }
#endif
    return data.source;
}

JSFlatString *
ScriptSource::substring(JSContext *cx, uint32_t start, uint32_t stop)
{
    JS_ASSERT(start <= stop);
    SourceDataCache::AutoSuppressPurge asp(cx);
    const jschar *chars = this->chars(cx, asp);
    if (!chars)
        return nullptr;
    return js_NewStringCopyN<CanGC>(cx, chars + start, stop - start);
}

bool
ScriptSource::setSourceCopy(ExclusiveContext *cx, const jschar *src, uint32_t length,
                            bool argumentsNotIncluded, SourceCompressionTask *task)
{
    JS_ASSERT(!hasSourceData());
    length_ = length;
    argumentsNotIncluded_ = argumentsNotIncluded;

    // There are several cases where source compression is not a good idea:
    //  - If the script is enormous, then decompression can take seconds. With
    //    lazy parsing, decompression is not uncommon, so this can significantly
    //    increase latency.
    //  - If there is only one core, then compression will contend with JS
    //    execution (which hurts benchmarketing).
    //  - If the source contains a giant string, then parsing will finish much
    //    faster than compression which increases latency (this case is handled
    //    in Parser::stringLiteral).
    //
    // Lastly, since the parsing thread will eventually perform a blocking wait
    // on the compresion task's worker thread, require that there are at least 2
    // worker threads:
    //  - If we are on a worker thread, there must be another worker thread to
    //    execute our compression task.
    //  - If we are on the main thread, there must be at least two worker
    //    threads since at most one worker thread can be blocking on the main
    //    thread (see WorkerThreadState::canStartParseTask) which would cause a
    //    deadlock if there wasn't a second worker thread that could make
    //    progress on our compression task.
    const size_t HUGE_SCRIPT = 5 * 1024 * 1024;
    if (length < HUGE_SCRIPT &&
        cx->cpuCount() > 1 &&
        cx->workerThreadCount() >= 2)
    {
        task->ss = this;
        task->chars = src;
        ready_ = false;
        if (!StartOffThreadCompression(cx, task))
            return false;
    } else {
        if (!adjustDataSize(sizeof(jschar) * length))
            return false;
        PodCopy(data.source, src, length_);
    }

    return true;
}

void
ScriptSource::setSource(const jschar *src, size_t length)
{
    JS_ASSERT(!hasSourceData());
    length_ = length;
    JS_ASSERT(!argumentsNotIncluded_);
    data.source = const_cast<jschar *>(src);
}

bool
SourceCompressionTask::work()
{
    // A given compression token can be compressed on any thread, and the ss
    // not being ready indicates to other threads that its fields might change
    // with no lock held.
    JS_ASSERT(!ss->ready());

    size_t compressedLength = 0;
    size_t nbytes = sizeof(jschar) * ss->length_;

    // Memory allocation functions on JSRuntime and JSContext are not
    // threadsafe. We have to use the js_* variants.

#ifdef USE_ZLIB
    const size_t COMPRESS_THRESHOLD = 512;
    if (nbytes >= COMPRESS_THRESHOLD) {
        // Try to keep the maximum memory usage down by only allocating half the
        // size of the string, first.
        size_t firstSize = nbytes / 2;
        if (!ss->adjustDataSize(firstSize))
            return false;
        Compressor comp(reinterpret_cast<const unsigned char *>(chars), nbytes);
        if (!comp.init())
            return false;
        comp.setOutput(ss->data.compressed, firstSize);
        bool cont = !abort_;
        while (cont) {
            switch (comp.compressMore()) {
              case Compressor::CONTINUE:
                break;
              case Compressor::MOREOUTPUT: {
                if (comp.outWritten() == nbytes) {
                    cont = false;
                    break;
                }

                // The compressed output is greater than half the size of the
                // original string. Reallocate to the full size.
                if (!ss->adjustDataSize(nbytes))
                    return false;
                comp.setOutput(ss->data.compressed, nbytes);
                break;
              }
              case Compressor::DONE:
                cont = false;
                break;
              case Compressor::OOM:
                return false;
            }
            cont = cont && !abort_;
        }
        compressedLength = comp.outWritten();
        if (abort_ || compressedLength == nbytes)
            compressedLength = 0;
    }
#endif
    if (compressedLength == 0) {
        if (!ss->adjustDataSize(nbytes))
            return false;
        PodCopy(ss->data.source, chars, ss->length());
    } else {
        // Shrink the buffer to the size of the compressed data. Shouldn't fail.
        JS_ALWAYS_TRUE(ss->adjustDataSize(compressedLength));
    }
    ss->compressedLength_ = compressedLength;
    return true;
}

void
ScriptSource::destroy()
{
    JS_ASSERT(ready());
    adjustDataSize(0);
    if (introducerFilename_ != filename_)
        js_free(introducerFilename_);
    js_free(filename_);
    js_free(displayURL_);
    js_free(sourceMapURL_);
    if (originPrincipals_)
        JS_DropPrincipals(TlsPerThreadData.get()->runtimeFromMainThread(), originPrincipals_);
    ready_ = false;
    js_free(this);
}

size_t
ScriptSource::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    // |data| is a union, but both members are pointers to allocated memory,
    // |emptySource|, or nullptr, so just using |data.compressed| will work.
    size_t n = mallocSizeOf(this);
    n += (ready() && data.compressed != emptySource)
       ? mallocSizeOf(data.compressed)
       : 0;
    n += mallocSizeOf(filename_);
    return n;
}

template<XDRMode mode>
bool
ScriptSource::performXDR(XDRState<mode> *xdr)
{
    uint8_t hasSource = hasSourceData();
    if (!xdr->codeUint8(&hasSource))
        return false;

    uint8_t retrievable = sourceRetrievable_;
    if (!xdr->codeUint8(&retrievable))
        return false;
    sourceRetrievable_ = retrievable;

    if (hasSource && !sourceRetrievable_) {
        // Only set members when we know decoding cannot fail. This prevents the
        // script source from being partially initialized.
        uint32_t length = length_;
        if (!xdr->codeUint32(&length))
            return false;

        uint32_t compressedLength = compressedLength_;
        if (!xdr->codeUint32(&compressedLength))
            return false;

        uint8_t argumentsNotIncluded = argumentsNotIncluded_;
        if (!xdr->codeUint8(&argumentsNotIncluded))
            return false;

        size_t byteLen = compressedLength ? compressedLength : (length * sizeof(jschar));
        if (mode == XDR_DECODE) {
            if (!adjustDataSize(byteLen))
                return false;
        }
        if (!xdr->codeBytes(data.compressed, byteLen)) {
            if (mode == XDR_DECODE) {
                js_free(data.compressed);
                data.compressed = nullptr;
            }
            return false;
        }
        length_ = length;
        compressedLength_ = compressedLength;
        argumentsNotIncluded_ = argumentsNotIncluded;
    }

    uint8_t haveSourceMap = hasSourceMapURL();
    if (!xdr->codeUint8(&haveSourceMap))
        return false;

    if (haveSourceMap) {
        uint32_t sourceMapURLLen = (mode == XDR_DECODE) ? 0 : js_strlen(sourceMapURL_);
        if (!xdr->codeUint32(&sourceMapURLLen))
            return false;

        if (mode == XDR_DECODE) {
            size_t byteLen = (sourceMapURLLen + 1) * sizeof(jschar);
            sourceMapURL_ = static_cast<jschar *>(xdr->cx()->malloc_(byteLen));
            if (!sourceMapURL_)
                return false;
        }
        if (!xdr->codeChars(sourceMapURL_, sourceMapURLLen)) {
            if (mode == XDR_DECODE) {
                js_free(sourceMapURL_);
                sourceMapURL_ = nullptr;
            }
            return false;
        }
        sourceMapURL_[sourceMapURLLen] = '\0';
    }

    uint8_t haveDisplayURL = hasDisplayURL();
    if (!xdr->codeUint8(&haveDisplayURL))
        return false;

    if (haveDisplayURL) {
        uint32_t displayURLLen = (mode == XDR_DECODE) ? 0 : js_strlen(displayURL_);
        if (!xdr->codeUint32(&displayURLLen))
            return false;

        if (mode == XDR_DECODE) {
            size_t byteLen = (displayURLLen + 1) * sizeof(jschar);
            displayURL_ = static_cast<jschar *>(xdr->cx()->malloc_(byteLen));
            if (!displayURL_)
                return false;
        }
        if (!xdr->codeChars(displayURL_, displayURLLen)) {
            if (mode == XDR_DECODE) {
                js_free(displayURL_);
                displayURL_ = nullptr;
            }
            return false;
        }
        displayURL_[displayURLLen] = '\0';
    }

    uint8_t haveFilename = !!filename_;
    if (!xdr->codeUint8(&haveFilename))
        return false;

    if (haveFilename) {
        const char *fn = filename();
        if (!xdr->codeCString(&fn))
            return false;
        if (mode == XDR_DECODE && !setFilename(xdr->cx(), fn))
            return false;
    }

    if (mode == XDR_DECODE)
        ready_ = true;

    return true;
}

bool
ScriptSource::setFilename(ExclusiveContext *cx, const char *filename)
{
    JS_ASSERT(!filename_);
    filename_ = js_strdup(cx, filename);
    if (!filename_)
        return false;
    return true;
}

bool
ScriptSource::setDisplayURL(ExclusiveContext *cx, const jschar *displayURL)
{
    JS_ASSERT(displayURL);
    if (hasDisplayURL()) {
        if (cx->isJSContext() &&
            !JS_ReportErrorFlagsAndNumber(cx->asJSContext(), JSREPORT_WARNING,
                                          js_GetErrorMessage, nullptr,
                                          JSMSG_ALREADY_HAS_PRAGMA, filename_,
                                          "//# sourceURL"))
        {
            return false;
        }
    }
    size_t len = js_strlen(displayURL) + 1;
    if (len == 1)
        return true;
    displayURL_ = js_strdup(cx, displayURL);
    if (!displayURL_)
        return false;
    return true;
}

const jschar *
ScriptSource::displayURL()
{
    JS_ASSERT(hasDisplayURL());
    return displayURL_;
}

bool
ScriptSource::setIntroducedFilename(ExclusiveContext *cx,
                                    const char *callerFilename, unsigned callerLineno,
                                    const char *introducer, const char *introducerFilename)
{
    JS_ASSERT(!filename_);
    JS_ASSERT(!introducerFilename_);

    introducerType_ = introducer;

    if (introducerFilename) {
        introducerFilename_ = js_strdup(cx, introducerFilename);
        if (!introducerFilename_)
            return false;
    }

    // Final format:  "{callerFilename} line {callerLineno} > {introducer}"
    // Len = strlen(callerFilename) + strlen(" line ") +
    //       strlen(toStr(callerLineno)) + strlen(" > ") + strlen(introducer);
    char linenoBuf[15];
    size_t filenameLen = strlen(callerFilename);
    size_t linenoLen = JS_snprintf(linenoBuf, 15, "%u", callerLineno);
    size_t introducerLen = strlen(introducer);
    size_t len = filenameLen                    +
                 6 /* == strlen(" line ") */    +
                 linenoLen                      +
                 3 /* == strlen(" > ") */       +
                 introducerLen                  +
                 1 /* \0 */;
    filename_ = cx->pod_malloc<char>(len);
    if (!filename_)
        return false;
    mozilla::DebugOnly<int> checkLen = JS_snprintf(filename_, len, "%s line %s > %s",
                                                   callerFilename, linenoBuf, introducer);
    JS_ASSERT(checkLen == len - 1);

    if (!introducerFilename_)
        introducerFilename_ = filename_;

    return true;
}

bool
ScriptSource::setSourceMapURL(ExclusiveContext *cx, const jschar *sourceMapURL)
{
    JS_ASSERT(sourceMapURL);
    if (hasSourceMapURL()) {
        if (cx->isJSContext() &&
            !JS_ReportErrorFlagsAndNumber(cx->asJSContext(), JSREPORT_WARNING,
                                          js_GetErrorMessage, nullptr,
                                          JSMSG_ALREADY_HAS_PRAGMA, filename_,
                                          "//# sourceMappingURL"))
        {
            return false;
        }
    }

    size_t len = js_strlen(sourceMapURL) + 1;
    if (len == 1)
        return true;
    sourceMapURL_ = js_strdup(cx, sourceMapURL);
    if (!sourceMapURL_)
        return false;
    return true;
}

const jschar *
ScriptSource::sourceMapURL()
{
    JS_ASSERT(hasSourceMapURL());
    return sourceMapURL_;
}

/*
 * Shared script data management.
 */

SharedScriptData *
js::SharedScriptData::new_(ExclusiveContext *cx, uint32_t codeLength,
                           uint32_t srcnotesLength, uint32_t natoms)
{
    /*
     * Ensure the atoms are aligned, as some architectures don't allow unaligned
     * access.
     */
    const uint32_t pointerSize = sizeof(JSAtom *);
    const uint32_t pointerMask = pointerSize - 1;
    const uint32_t dataOffset = offsetof(SharedScriptData, data);
    uint32_t baseLength = codeLength + srcnotesLength;
    uint32_t padding = (pointerSize - ((baseLength + dataOffset) & pointerMask)) & pointerMask;
    uint32_t length = baseLength + padding + pointerSize * natoms;

    SharedScriptData *entry = (SharedScriptData *)cx->malloc_(length + dataOffset);
    if (!entry)
        return nullptr;

    entry->length = length;
    entry->natoms = natoms;
    entry->marked = false;
    memset(entry->data + baseLength, 0, padding);

    /*
     * Call constructors to initialize the storage that will be accessed as a
     * HeapPtrAtom array via atoms().
     */
    HeapPtrAtom *atoms = entry->atoms();
    JS_ASSERT(reinterpret_cast<uintptr_t>(atoms) % sizeof(JSAtom *) == 0);
    for (unsigned i = 0; i < natoms; ++i)
        new (&atoms[i]) HeapPtrAtom();

    return entry;
}

/*
 * Takes ownership of its *ssd parameter and either adds it into the runtime's
 * ScriptDataTable or frees it if a matching entry already exists.
 *
 * Sets the |code| and |atoms| fields on the given JSScript.
 */
static bool
SaveSharedScriptData(ExclusiveContext *cx, Handle<JSScript *> script, SharedScriptData *ssd,
                     uint32_t nsrcnotes)
{
    ASSERT(script != nullptr);
    ASSERT(ssd != nullptr);

    AutoLockForExclusiveAccess lock(cx);

    ScriptBytecodeHasher::Lookup l(ssd);

    ScriptDataTable::AddPtr p = cx->scriptDataTable().lookupForAdd(l);
    if (p) {
        js_free(ssd);
        ssd = *p;
    } else {
        if (!cx->scriptDataTable().add(p, ssd)) {
            script->setCode(nullptr);
            script->atoms = nullptr;
            js_free(ssd);
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

#ifdef JSGC_INCREMENTAL
    /*
     * During the IGC we need to ensure that bytecode is marked whenever it is
     * accessed even if the bytecode was already in the table: at this point
     * old scripts or exceptions pointing to the bytecode may no longer be
     * reachable. This is effectively a read barrier.
     */
    if (cx->isJSContext()) {
        JSRuntime *rt = cx->asJSContext()->runtime();
        if (JS::IsIncrementalGCInProgress(rt) && rt->gcIsFull)
            ssd->marked = true;
    }
#endif

    script->setCode(ssd->data);
    script->atoms = ssd->atoms();
    return true;
}

static inline void
MarkScriptData(JSRuntime *rt, const jsbytecode *bytecode)
{
    /*
     * As an invariant, a ScriptBytecodeEntry should not be 'marked' outside of
     * a GC. Since SweepScriptBytecodes is only called during a full gc,
     * to preserve this invariant, only mark during a full gc.
     */
    if (rt->gcIsFull)
        SharedScriptData::fromBytecode(bytecode)->marked = true;
}

void
js::UnmarkScriptData(JSRuntime *rt)
{
    JS_ASSERT(rt->gcIsFull);
    ScriptDataTable &table = rt->scriptDataTable();
    for (ScriptDataTable::Enum e(table); !e.empty(); e.popFront()) {
        SharedScriptData *entry = e.front();
        entry->marked = false;
    }
}

void
js::SweepScriptData(JSRuntime *rt)
{
    JS_ASSERT(rt->gcIsFull);
    ScriptDataTable &table = rt->scriptDataTable();

    if (rt->keepAtoms())
        return;

    for (ScriptDataTable::Enum e(table); !e.empty(); e.popFront()) {
        SharedScriptData *entry = e.front();
        if (!entry->marked) {
            js_free(entry);
            e.removeFront();
        }
    }
}

void
js::FreeScriptData(JSRuntime *rt)
{
    ScriptDataTable &table = rt->scriptDataTable();
    if (!table.initialized())
        return;

    for (ScriptDataTable::Enum e(table); !e.empty(); e.popFront())
        js_free(e.front());

    table.clear();
}

/*
 * JSScript::data and SharedScriptData::data have complex,
 * manually-controlled, memory layouts.
 *
 * JSScript::data begins with some optional array headers. They are optional
 * because they often aren't needed, i.e. the corresponding arrays often have
 * zero elements. Each header has a bit in JSScript::hasArrayBits that
 * indicates if it's present within |data|; from this the offset of each
 * present array header can be computed. Each header has an accessor function
 * in JSScript that encapsulates this offset computation.
 *
 * Array type       Array elements  Accessor
 * ----------       --------------  --------
 * ConstArray       Consts          consts()
 * ObjectArray      Objects         objects()
 * ObjectArray      Regexps         regexps()
 * TryNoteArray     Try notes       trynotes()
 * BlockScopeArray  Scope notes     blockScopes()
 *
 * Then are the elements of several arrays.
 * - Most of these arrays have headers listed above (if present). For each of
 *   these, the array pointer and the array length is stored in the header.
 * - The remaining arrays have pointers and lengths that are stored directly in
 *   JSScript. This is because, unlike the others, they are nearly always
 *   non-zero length and so the optional-header space optimization isn't
 *   worthwhile.
 *
 * Array elements   Pointed to by         Length
 * --------------   -------------         ------
 * Consts           consts()->vector      consts()->length
 * Objects          objects()->vector     objects()->length
 * Regexps          regexps()->vector     regexps()->length
 * Try notes        trynotes()->vector    trynotes()->length
 * Scope notes      blockScopes()->vector blockScopes()->length
 *
 * IMPORTANT: This layout has two key properties.
 * - It ensures that everything has sufficient alignment; in particular, the
 *   consts() elements need jsval alignment.
 * - It ensures there are no gaps between elements, which saves space and makes
 *   manual layout easy. In particular, in the second part, arrays with larger
 *   elements precede arrays with smaller elements.
 *
 * SharedScriptData::data contains data that can be shared within a
 * runtime. These items' layout is manually controlled to make it easier to
 * manage both during (temporary) allocation and during matching against
 * existing entries in the runtime. As the jsbytecode has to come first to
 * enable lookup by bytecode identity, SharedScriptData::data, the atoms part
 * has to manually be aligned sufficiently by adding padding after the notes
 * part.
 *
 * Array elements   Pointed to by         Length
 * --------------   -------------         ------
 * jsbytecode       code                  length
 * jsscrnote        notes()               numNotes()
 * Atoms            atoms                 natoms
 *
 * The following static assertions check JSScript::data's alignment properties.
 */

#define KEEPS_JSVAL_ALIGNMENT(T) \
    (JS_ALIGNMENT_OF(jsval) % JS_ALIGNMENT_OF(T) == 0 && \
     sizeof(T) % sizeof(jsval) == 0)

#define HAS_JSVAL_ALIGNMENT(T) \
    (JS_ALIGNMENT_OF(jsval) == JS_ALIGNMENT_OF(T) && \
     sizeof(T) == sizeof(jsval))

#define NO_PADDING_BETWEEN_ENTRIES(T1, T2) \
    (JS_ALIGNMENT_OF(T1) % JS_ALIGNMENT_OF(T2) == 0)

/*
 * These assertions ensure that there is no padding between the array headers,
 * and also that the consts() elements (which follow immediately afterward) are
 * jsval-aligned.  (There is an assumption that |data| itself is jsval-aligned;
 * we check this below).
 */
JS_STATIC_ASSERT(KEEPS_JSVAL_ALIGNMENT(ConstArray));
JS_STATIC_ASSERT(KEEPS_JSVAL_ALIGNMENT(ObjectArray));       /* there are two of these */
JS_STATIC_ASSERT(KEEPS_JSVAL_ALIGNMENT(TryNoteArray));
JS_STATIC_ASSERT(KEEPS_JSVAL_ALIGNMENT(BlockScopeArray));

/* These assertions ensure there is no padding required between array elements. */
JS_STATIC_ASSERT(HAS_JSVAL_ALIGNMENT(HeapValue));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapValue, HeapPtrObject));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapPtrObject, HeapPtrObject));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapPtrObject, JSTryNote));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(JSTryNote, uint32_t));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(uint32_t, uint32_t));

JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapValue, BlockScopeNote));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(BlockScopeNote, BlockScopeNote));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(JSTryNote, BlockScopeNote));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapPtrObject, BlockScopeNote));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(BlockScopeNote, uint32_t));

static inline size_t
ScriptDataSize(uint32_t nbindings, uint32_t nconsts, uint32_t nobjects, uint32_t nregexps,
               uint32_t ntrynotes, uint32_t nblockscopes)
{
    size_t size = 0;

    if (nconsts != 0)
        size += sizeof(ConstArray) + nconsts * sizeof(Value);
    if (nobjects != 0)
        size += sizeof(ObjectArray) + nobjects * sizeof(JSObject *);
    if (nregexps != 0)
        size += sizeof(ObjectArray) + nregexps * sizeof(JSObject *);
    if (ntrynotes != 0)
        size += sizeof(TryNoteArray) + ntrynotes * sizeof(JSTryNote);
    if (nblockscopes != 0)
        size += sizeof(BlockScopeArray) + nblockscopes * sizeof(BlockScopeNote);

    if (nbindings != 0) {
	// Make sure bindings are sufficiently aligned.
        size = JS_ROUNDUP(size, JS_ALIGNMENT_OF(Binding)) + nbindings * sizeof(Binding);
    }

    return size;
}

void
JSScript::initCompartment(ExclusiveContext *cx)
{
    compartment_ = cx->compartment_;
}

JSScript *
JSScript::Create(ExclusiveContext *cx, HandleObject enclosingScope, bool savedCallerFun,
                 const ReadOnlyCompileOptions &options, unsigned staticLevel,
                 HandleObject sourceObject, uint32_t bufStart, uint32_t bufEnd)
{
    JS_ASSERT(bufStart <= bufEnd);

    RootedScript script(cx, js_NewGCScript(cx));
    if (!script)
        return nullptr;

    PodZero(script.get());
    new (&script->bindings) Bindings;

    script->enclosingScopeOrOriginalFunction_ = enclosingScope;
    script->savedCallerFun_ = savedCallerFun;
    script->initCompartment(cx);

    script->compileAndGo_ = options.compileAndGo;
    script->selfHosted_ = options.selfHostingMode;
    script->noScriptRval_ = options.noScriptRval;

    script->version = options.version;
    JS_ASSERT(script->getVersion() == options.version);     // assert that no overflow occurred

    // This is an unsigned-to-uint16_t conversion, test for too-high values.
    // In practice, recursion in Parser and/or BytecodeEmitter will blow the
    // stack if we nest functions more than a few hundred deep, so this will
    // never trigger.  Oh well.
    if (staticLevel > UINT16_MAX) {
        if (cx->isJSContext()) {
            JS_ReportErrorNumber(cx->asJSContext(),
                                 js_GetErrorMessage, nullptr, JSMSG_TOO_DEEP, js_function_str);
        }
        return nullptr;
    }
    script->staticLevel_ = uint16_t(staticLevel);

    script->setSourceObject(sourceObject);
    script->sourceStart_ = bufStart;
    script->sourceEnd_ = bufEnd;

    return script;
}

static inline uint8_t *
AllocScriptData(ExclusiveContext *cx, size_t size)
{
    uint8_t *data = static_cast<uint8_t *>(cx->calloc_(JS_ROUNDUP(size, sizeof(Value))));
    if (!data)
        return nullptr;

    // All script data is optional, so size might be 0. In that case, we don't care about alignment.
    JS_ASSERT(size == 0 || size_t(data) % sizeof(Value) == 0);
    return data;
}

/* static */ bool
JSScript::partiallyInit(ExclusiveContext *cx, HandleScript script, uint32_t nconsts,
                        uint32_t nobjects, uint32_t nregexps, uint32_t ntrynotes,
                        uint32_t nblockscopes, uint32_t nTypeSets)
{
    size_t size = ScriptDataSize(script->bindings.count(), nconsts, nobjects, nregexps, ntrynotes,
                                 nblockscopes);
    if (size > 0) {
        script->data = AllocScriptData(cx, size);
        if (!script->data)
            return false;
    } else {
        script->data = nullptr;
    }
    script->dataSize_ = size;

    JS_ASSERT(nTypeSets <= UINT16_MAX);
    script->nTypeSets_ = uint16_t(nTypeSets);

    uint8_t *cursor = script->data;
    if (nconsts != 0) {
        script->setHasArray(CONSTS);
        cursor += sizeof(ConstArray);
    }
    if (nobjects != 0) {
        script->setHasArray(OBJECTS);
        cursor += sizeof(ObjectArray);
    }
    if (nregexps != 0) {
        script->setHasArray(REGEXPS);
        cursor += sizeof(ObjectArray);
    }
    if (ntrynotes != 0) {
        script->setHasArray(TRYNOTES);
        cursor += sizeof(TryNoteArray);
    }
    if (nblockscopes != 0) {
        script->setHasArray(BLOCK_SCOPES);
        cursor += sizeof(BlockScopeArray);
    }

    if (nconsts != 0) {
        JS_ASSERT(reinterpret_cast<uintptr_t>(cursor) % sizeof(jsval) == 0);
        script->consts()->length = nconsts;
        script->consts()->vector = (HeapValue *)cursor;
        cursor += nconsts * sizeof(script->consts()->vector[0]);
    }

    if (nobjects != 0) {
        script->objects()->length = nobjects;
        script->objects()->vector = (HeapPtr<JSObject> *)cursor;
        cursor += nobjects * sizeof(script->objects()->vector[0]);
    }

    if (nregexps != 0) {
        script->regexps()->length = nregexps;
        script->regexps()->vector = (HeapPtr<JSObject> *)cursor;
        cursor += nregexps * sizeof(script->regexps()->vector[0]);
    }

    if (ntrynotes != 0) {
        script->trynotes()->length = ntrynotes;
        script->trynotes()->vector = reinterpret_cast<JSTryNote *>(cursor);
        size_t vectorSize = ntrynotes * sizeof(script->trynotes()->vector[0]);
#ifdef DEBUG
        memset(cursor, 0, vectorSize);
#endif
        cursor += vectorSize;
    }

    if (nblockscopes != 0) {
        script->blockScopes()->length = nblockscopes;
        script->blockScopes()->vector = reinterpret_cast<BlockScopeNote *>(cursor);
        size_t vectorSize = nblockscopes * sizeof(script->blockScopes()->vector[0]);
#ifdef DEBUG
        memset(cursor, 0, vectorSize);
#endif
        cursor += vectorSize;
    }

    if (script->bindings.count() != 0) {
	// Make sure bindings are sufficiently aligned.
	cursor = reinterpret_cast<uint8_t*>
	    (JS_ROUNDUP(reinterpret_cast<uintptr_t>(cursor), JS_ALIGNMENT_OF(Binding)));
    }
    cursor = script->bindings.switchToScriptStorage(reinterpret_cast<Binding *>(cursor));

    JS_ASSERT(cursor == script->data + size);
    return true;
}

/* static */ bool
JSScript::fullyInitTrivial(ExclusiveContext *cx, Handle<JSScript*> script)
{
    if (!partiallyInit(cx, script, 0, 0, 0, 0, 0, 0))
        return false;

    SharedScriptData *ssd = SharedScriptData::new_(cx, 1, 1, 0);
    if (!ssd)
        return false;

    ssd->data[0] = JSOP_RETRVAL;
    ssd->data[1] = SRC_NULL;
    script->setLength(1);
    return SaveSharedScriptData(cx, script, ssd, 1);
}

/* static */ bool
JSScript::fullyInitFromEmitter(ExclusiveContext *cx, HandleScript script, BytecodeEmitter *bce)
{
    /* The counts of indexed things must be checked during code generation. */
    JS_ASSERT(bce->atomIndices->count() <= INDEX_LIMIT);
    JS_ASSERT(bce->objectList.length <= INDEX_LIMIT);
    JS_ASSERT(bce->regexpList.length <= INDEX_LIMIT);

    uint32_t mainLength = bce->offset();
    uint32_t prologLength = bce->prologOffset();
    uint32_t nsrcnotes = uint32_t(bce->countFinalSourceNotes());
    uint32_t natoms = bce->atomIndices->count();
    if (!partiallyInit(cx, script,
                       bce->constList.length(), bce->objectList.length, bce->regexpList.length,
                       bce->tryNoteList.length(), bce->blockScopeList.length(), bce->typesetCount))
    {
        return false;
    }

    JS_ASSERT(script->mainOffset() == 0);
    script->mainOffset_ = prologLength;

    script->lineno_ = bce->firstLine;

    script->setLength(prologLength + mainLength);
    script->natoms_ = natoms;
    SharedScriptData *ssd = SharedScriptData::new_(cx, script->length(), nsrcnotes, natoms);
    if (!ssd)
        return false;

    jsbytecode *code = ssd->data;
    PodCopy<jsbytecode>(code, bce->prolog.code.begin(), prologLength);
    PodCopy<jsbytecode>(code + prologLength, bce->code().begin(), mainLength);
    if (!FinishTakingSrcNotes(cx, bce, (jssrcnote *)(code + script->length())))
        return false;
    InitAtomMap(bce->atomIndices.getMap(), ssd->atoms());

    if (!SaveSharedScriptData(cx, script, ssd, nsrcnotes))
        return false;

    FunctionBox *funbox = bce->sc->isFunctionBox() ? bce->sc->asFunctionBox() : nullptr;

    if (bce->constList.length() != 0)
        bce->constList.finish(script->consts());
    if (bce->objectList.length != 0)
        bce->objectList.finish(script->objects());
    if (bce->regexpList.length != 0)
        bce->regexpList.finish(script->regexps());
    if (bce->tryNoteList.length() != 0)
        bce->tryNoteList.finish(script->trynotes());
    if (bce->blockScopeList.length() != 0)
        bce->blockScopeList.finish(script->blockScopes());
    script->strict_ = bce->sc->strict;
    script->explicitUseStrict_ = bce->sc->hasExplicitUseStrict();
    script->bindingsAccessedDynamically_ = bce->sc->bindingsAccessedDynamically();
    script->funHasExtensibleScope_ = funbox ? funbox->hasExtensibleScope() : false;
    script->funNeedsDeclEnvObject_ = funbox ? funbox->needsDeclEnvObject() : false;
    script->hasSingletons_ = bce->hasSingletons;

    if (funbox) {
        if (funbox->argumentsHasLocalBinding()) {
            // This must precede the script->bindings.transfer() call below
            script->setArgumentsHasVarBinding();
            if (funbox->definitelyNeedsArgsObj())
                script->setNeedsArgsObj(true);
        } else {
            JS_ASSERT(!funbox->definitelyNeedsArgsObj());
        }

        script->funLength_ = funbox->length;
    }

    RootedFunction fun(cx, nullptr);
    if (funbox) {
        JS_ASSERT(!bce->script->noScriptRval());
        script->isGeneratorExp_ = funbox->inGenexpLambda;
        script->setGeneratorKind(funbox->generatorKind());
        script->setFunction(funbox->function());
    }

    // The call to nfixed() depends on the above setFunction() call.
    if (UINT32_MAX - script->nfixed() < bce->maxStackDepth) {
        bce->reportError(nullptr, JSMSG_NEED_DIET, "script");
        return false;
    }
    script->nslots_ = script->nfixed() + bce->maxStackDepth;

    for (unsigned i = 0, n = script->bindings.numArgs(); i < n; ++i) {
        if (script->formalIsAliased(i)) {
            script->funHasAnyAliasedFormal_ = true;
            break;
        }
    }

    return true;
}

size_t
JSScript::computedSizeOfData() const
{
    return dataSize();
}

size_t
JSScript::sizeOfData(mozilla::MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(data);
}

size_t
JSScript::sizeOfTypeScript(mozilla::MallocSizeOf mallocSizeOf) const
{
    return types->sizeOfIncludingThis(mallocSizeOf);
}

/*
 * Nb: srcnotes are variable-length.  This function computes the number of
 * srcnote *slots*, which may be greater than the number of srcnotes.
 */
uint32_t
JSScript::numNotes()
{
    jssrcnote *sn;
    jssrcnote *notes_ = notes();
    for (sn = notes_; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn))
        continue;
    return sn - notes_ + 1;    /* +1 for the terminator */
}

js::GlobalObject&
JSScript::uninlinedGlobal() const
{
    return global();
}

void
js::CallNewScriptHook(JSContext *cx, HandleScript script, HandleFunction fun)
{
    if (script->selfHosted())
        return;

    JS_ASSERT(!script->isActiveEval());
    if (JSNewScriptHook hook = cx->runtime()->debugHooks.newScriptHook) {
        AutoKeepAtoms keepAtoms(cx->perThreadData);
        hook(cx, script->filename(), script->lineno(), script, fun,
             cx->runtime()->debugHooks.newScriptHookData);
    }
}

void
js::CallDestroyScriptHook(FreeOp *fop, JSScript *script)
{
    if (script->selfHosted())
        return;

    // The hook will only call into JS if a GC is not running.
    if (JSDestroyScriptHook hook = fop->runtime()->debugHooks.destroyScriptHook)
        hook(fop, script, fop->runtime()->debugHooks.destroyScriptHookData);
    script->clearTraps(fop);
}

void
JSScript::finalize(FreeOp *fop)
{
    // NOTE: this JSScript may be partially initialized at this point.  E.g. we
    // may have created it and partially initialized it with
    // JSScript::Create(), but not yet finished initializing it with
    // fullyInitFromEmitter() or fullyInitTrivial().

    CallDestroyScriptHook(fop, this);
    fop->runtime()->spsProfiler.onScriptFinalized(this);

    if (types)
        types->destroy();

#ifdef JS_ION
    jit::DestroyIonScripts(fop, this);
#endif

    destroyScriptCounts(fop);
    destroyDebugScript(fop);

    if (data) {
        JS_POISON(data, 0xdb, computedSizeOfData());
        fop->free_(data);
    }

    fop->runtime()->lazyScriptCache.remove(this);
}

static const uint32_t GSN_CACHE_THRESHOLD = 100;

void
GSNCache::purge()
{
    code = nullptr;
    if (map.initialized())
        map.finish();
}

jssrcnote *
js::GetSrcNote(GSNCache &cache, JSScript *script, jsbytecode *pc)
{
    size_t target = pc - script->code();
    if (target >= script->length())
        return nullptr;

    if (cache.code == script->code()) {
        JS_ASSERT(cache.map.initialized());
        GSNCache::Map::Ptr p = cache.map.lookup(pc);
        return p ? p->value() : nullptr;
    }

    size_t offset = 0;
    jssrcnote *result;
    for (jssrcnote *sn = script->notes(); ; sn = SN_NEXT(sn)) {
        if (SN_IS_TERMINATOR(sn)) {
            result = nullptr;
            break;
        }
        offset += SN_DELTA(sn);
        if (offset == target && SN_IS_GETTABLE(sn)) {
            result = sn;
            break;
        }
    }

    if (cache.code != script->code() && script->length() >= GSN_CACHE_THRESHOLD) {
        unsigned nsrcnotes = 0;
        for (jssrcnote *sn = script->notes(); !SN_IS_TERMINATOR(sn);
             sn = SN_NEXT(sn)) {
            if (SN_IS_GETTABLE(sn))
                ++nsrcnotes;
        }
        if (cache.code) {
            JS_ASSERT(cache.map.initialized());
            cache.map.finish();
            cache.code = nullptr;
        }
        if (cache.map.init(nsrcnotes)) {
            pc = script->code();
            for (jssrcnote *sn = script->notes(); !SN_IS_TERMINATOR(sn);
                 sn = SN_NEXT(sn)) {
                pc += SN_DELTA(sn);
                if (SN_IS_GETTABLE(sn))
                    JS_ALWAYS_TRUE(cache.map.put(pc, sn));
            }
            cache.code = script->code();
        }
    }

    return result;
}

jssrcnote *
js_GetSrcNote(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    return GetSrcNote(cx->runtime()->gsnCache, script, pc);
}

unsigned
js::PCToLineNumber(unsigned startLine, jssrcnote *notes, jsbytecode *code, jsbytecode *pc,
                   unsigned *columnp)
{
    unsigned lineno = startLine;
    unsigned column = 0;

    /*
     * Walk through source notes accumulating their deltas, keeping track of
     * line-number notes, until we pass the note for pc's offset within
     * script->code.
     */
    ptrdiff_t offset = 0;
    ptrdiff_t target = pc - code;
    for (jssrcnote *sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        offset += SN_DELTA(sn);
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            if (offset <= target)
                lineno = (unsigned) js_GetSrcNoteOffset(sn, 0);
            column = 0;
        } else if (type == SRC_NEWLINE) {
            if (offset <= target)
                lineno++;
            column = 0;
        }

        if (offset > target)
            break;

        if (type == SRC_COLSPAN) {
            ptrdiff_t colspan = js_GetSrcNoteOffset(sn, 0);

            if (colspan >= SN_COLSPAN_DOMAIN / 2)
                colspan -= SN_COLSPAN_DOMAIN;
            JS_ASSERT(ptrdiff_t(column) + colspan >= 0);
            column += colspan;
        }
    }

    if (columnp)
        *columnp = column;

    return lineno;
}

unsigned
js::PCToLineNumber(JSScript *script, jsbytecode *pc, unsigned *columnp)
{
    /* Cope with StackFrame.pc value prior to entering js_Interpret. */
    if (!pc)
        return 0;

    return PCToLineNumber(script->lineno(), script->notes(), script->code(), pc, columnp);
}

jsbytecode *
js_LineNumberToPC(JSScript *script, unsigned target)
{
    ptrdiff_t offset = 0;
    ptrdiff_t best = -1;
    unsigned lineno = script->lineno();
    unsigned bestdiff = SN_MAX_OFFSET;
    for (jssrcnote *sn = script->notes(); !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        /*
         * Exact-match only if offset is not in the prolog; otherwise use
         * nearest greater-or-equal line number match.
         */
        if (lineno == target && offset >= ptrdiff_t(script->mainOffset()))
            goto out;
        if (lineno >= target) {
            unsigned diff = lineno - target;
            if (diff < bestdiff) {
                bestdiff = diff;
                best = offset;
            }
        }
        offset += SN_DELTA(sn);
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            lineno = (unsigned) js_GetSrcNoteOffset(sn, 0);
        } else if (type == SRC_NEWLINE) {
            lineno++;
        }
    }
    if (best >= 0)
        offset = best;
out:
    return script->offsetToPC(offset);
}

JS_FRIEND_API(unsigned)
js_GetScriptLineExtent(JSScript *script)
{
    unsigned lineno = script->lineno();
    unsigned maxLineNo = lineno;
    for (jssrcnote *sn = script->notes(); !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE)
            lineno = (unsigned) js_GetSrcNoteOffset(sn, 0);
        else if (type == SRC_NEWLINE)
            lineno++;

        if (maxLineNo < lineno)
            maxLineNo = lineno;
    }

    return 1 + maxLineNo - script->lineno();
}

void
js::CurrentScriptFileLineOrigin(JSContext *cx, JSScript **script,
                                const char **file, unsigned *linenop,
                                uint32_t *pcOffset, JSPrincipals **origin, LineOption opt)
{
    if (opt == CALLED_FROM_JSOP_EVAL) {
        jsbytecode *pc = nullptr;
        *script = cx->currentScript(&pc);
        JS_ASSERT(JSOp(*pc) == JSOP_EVAL || JSOp(*pc) == JSOP_SPREADEVAL);
        JS_ASSERT(*(pc + (JSOp(*pc) == JSOP_EVAL ? JSOP_EVAL_LENGTH
                                                 : JSOP_SPREADEVAL_LENGTH)) == JSOP_LINENO);
        *file = (*script)->filename();
        *linenop = GET_UINT16(pc + (JSOp(*pc) == JSOP_EVAL ? JSOP_EVAL_LENGTH
                                                           : JSOP_SPREADEVAL_LENGTH));
        *pcOffset = pc - (*script)->code();
        *origin = (*script)->originPrincipals();
        return;
    }

    NonBuiltinScriptFrameIter iter(cx);

    if (iter.done()) {
        *script = nullptr;
        *file = nullptr;
        *linenop = 0;
        *pcOffset = 0;
        *origin = cx->compartment()->principals;
        return;
    }

    *script = iter.script();
    *file = (*script)->filename();
    *linenop = PCToLineNumber(*script, iter.pc());
    *pcOffset = iter.pc() - (*script)->code();
    *origin = (*script)->originPrincipals();
}

template <class T>
static inline T *
Rebase(JSScript *dst, JSScript *src, T *srcp)
{
    size_t off = reinterpret_cast<uint8_t *>(srcp) - src->data;
    return reinterpret_cast<T *>(dst->data + off);
}

JSScript *
js::CloneScript(JSContext *cx, HandleObject enclosingScope, HandleFunction fun, HandleScript src,
                NewObjectKind newKind /* = GenericObject */)
{
    /* NB: Keep this in sync with XDRScript. */

    /* Some embeddings are not careful to use ExposeObjectToActiveJS as needed. */
    JS_ASSERT(!src->sourceObject()->isMarked(gc::GRAY));

    uint32_t nconsts   = src->hasConsts()   ? src->consts()->length   : 0;
    uint32_t nobjects  = src->hasObjects()  ? src->objects()->length  : 0;
    uint32_t nregexps  = src->hasRegexps()  ? src->regexps()->length  : 0;
    uint32_t ntrynotes = src->hasTrynotes() ? src->trynotes()->length : 0;
    uint32_t nblockscopes = src->hasBlockScopes() ? src->blockScopes()->length : 0;

    /* Script data */

    size_t size = src->dataSize();
    uint8_t *data = AllocScriptData(cx, size);
    if (!data)
        return nullptr;

    /* Bindings */

    Rooted<Bindings> bindings(cx);
    InternalHandle<Bindings*> bindingsHandle =
        InternalHandle<Bindings*>::fromMarkedLocation(bindings.address());
    if (!Bindings::clone(cx, bindingsHandle, data, src))
        return nullptr;

    /* Objects */

    AutoObjectVector objects(cx);
    if (nobjects != 0) {
        HeapPtrObject *vector = src->objects()->vector;
        for (unsigned i = 0; i < nobjects; i++) {
            RootedObject obj(cx, vector[i]);
            RootedObject clone(cx);
            if (obj->is<NestedScopeObject>()) {
                Rooted<NestedScopeObject*> innerBlock(cx, &obj->as<NestedScopeObject>());

                RootedObject enclosingScope(cx);
                if (NestedScopeObject *enclosingBlock = innerBlock->enclosingNestedScope())
                    enclosingScope = objects[FindScopeObjectIndex(src, *enclosingBlock)];
                else
                    enclosingScope = fun;

                clone = CloneNestedScopeObject(cx, enclosingScope, innerBlock);
            } else if (obj->is<JSFunction>()) {
                RootedFunction innerFun(cx, &obj->as<JSFunction>());
                if (innerFun->isNative()) {
                    assertSameCompartment(cx, innerFun);
                    clone = innerFun;
                } else {
                    if (innerFun->isInterpretedLazy()) {
                        AutoCompartment ac(cx, innerFun);
                        if (!innerFun->getOrCreateScript(cx))
                            return nullptr;
                    }
                    RootedObject staticScope(cx, innerFun->nonLazyScript()->enclosingStaticScope());
                    StaticScopeIter<CanGC> ssi(cx, staticScope);
                    RootedObject enclosingScope(cx);
                    if (!ssi.done() && ssi.type() == StaticScopeIter<CanGC>::BLOCK)
                        enclosingScope = objects[FindScopeObjectIndex(src, ssi.block())];
                    else
                        enclosingScope = fun;

                    clone = CloneFunctionAndScript(cx, enclosingScope, innerFun);
                }
            } else {
                /*
                 * Clone object literals emitted for the JSOP_NEWOBJECT opcode. We only emit that
                 * instead of the less-optimized JSOP_NEWINIT for self-hosted code or code compiled
                 * with JSOPTION_COMPILE_N_GO set. As we don't clone the latter type of code, this
                 * case should only ever be hit when cloning objects from self-hosted code.
                 */
                clone = CloneObjectLiteral(cx, cx->global(), obj);
            }
            if (!clone || !objects.append(clone))
                return nullptr;
        }
    }

    /* RegExps */

    AutoObjectVector regexps(cx);
    for (unsigned i = 0; i < nregexps; i++) {
        HeapPtrObject *vector = src->regexps()->vector;
        for (unsigned i = 0; i < nregexps; i++) {
            JSObject *clone = CloneScriptRegExpObject(cx, vector[i]->as<RegExpObject>());
            if (!clone || !regexps.append(clone))
                return nullptr;
        }
    }

    /* Wrap the script source object as needed. */
    RootedObject sourceObject(cx, src->sourceObject());
    if (!cx->compartment()->wrap(cx, &sourceObject))
        return nullptr;

    /* Now that all fallible allocation is complete, create the GC thing. */

    CompileOptions options(cx);
    options.setPrincipals(cx->compartment()->principals)
           .setOriginPrincipals(src->originPrincipals())
           .setCompileAndGo(src->compileAndGo())
           .setSelfHostingMode(src->selfHosted())
           .setNoScriptRval(src->noScriptRval())
           .setVersion(src->getVersion());

    RootedScript dst(cx, JSScript::Create(cx, enclosingScope, src->savedCallerFun(),
                                          options, src->staticLevel(),
                                          sourceObject, src->sourceStart(), src->sourceEnd()));
    if (!dst) {
        js_free(data);
        return nullptr;
    }

    dst->bindings = bindings;

    /* This assignment must occur before all the Rebase calls. */
    dst->data = data;
    dst->dataSize_ = size;
    memcpy(data, src->data, size);

    /* Script filenames, bytecodes and atoms are runtime-wide. */
    dst->setCode(src->code());
    dst->atoms = src->atoms;

    dst->setLength(src->length());
    dst->lineno_ = src->lineno();
    dst->mainOffset_ = src->mainOffset();
    dst->natoms_ = src->natoms();
    dst->funLength_ = src->funLength();
    dst->nTypeSets_ = src->nTypeSets();
    dst->nslots_ = src->nslots();
    if (src->argumentsHasVarBinding()) {
        dst->setArgumentsHasVarBinding();
        if (src->analyzedArgsUsage())
            dst->setNeedsArgsObj(src->needsArgsObj());
    }
    dst->cloneHasArray(src);
    dst->strict_ = src->strict();
    dst->explicitUseStrict_ = src->explicitUseStrict();
    dst->bindingsAccessedDynamically_ = src->bindingsAccessedDynamically();
    dst->funHasExtensibleScope_ = src->funHasExtensibleScope();
    dst->funNeedsDeclEnvObject_ = src->funNeedsDeclEnvObject();
    dst->funHasAnyAliasedFormal_ = src->funHasAnyAliasedFormal();
    dst->hasSingletons_ = src->hasSingletons();
    dst->treatAsRunOnce_ = src->treatAsRunOnce();
    dst->isGeneratorExp_ = src->isGeneratorExp();
    dst->setGeneratorKind(src->generatorKind());

    /* Copy over hints. */
    dst->shouldInline_ = src->shouldInline();
    dst->shouldCloneAtCallsite_ = src->shouldCloneAtCallsite();
    dst->isCallsiteClone_ = src->isCallsiteClone();

    if (nconsts != 0) {
        HeapValue *vector = Rebase<HeapValue>(dst, src, src->consts()->vector);
        dst->consts()->vector = vector;
        for (unsigned i = 0; i < nconsts; ++i)
            JS_ASSERT_IF(vector[i].isMarkable(), vector[i].toString()->isAtom());
    }
    if (nobjects != 0) {
        HeapPtrObject *vector = Rebase<HeapPtr<JSObject> >(dst, src, src->objects()->vector);
        dst->objects()->vector = vector;
        for (unsigned i = 0; i < nobjects; ++i)
            vector[i].init(objects[i]);
    }
    if (nregexps != 0) {
        HeapPtrObject *vector = Rebase<HeapPtr<JSObject> >(dst, src, src->regexps()->vector);
        dst->regexps()->vector = vector;
        for (unsigned i = 0; i < nregexps; ++i)
            vector[i].init(regexps[i]);
    }
    if (ntrynotes != 0)
        dst->trynotes()->vector = Rebase<JSTryNote>(dst, src, src->trynotes()->vector);
    if (nblockscopes != 0)
        dst->blockScopes()->vector = Rebase<BlockScopeNote>(dst, src, src->blockScopes()->vector);

    return dst;
}

bool
js::CloneFunctionScript(JSContext *cx, HandleFunction original, HandleFunction clone,
                        NewObjectKind newKind /* = GenericObject */)
{
    JS_ASSERT(clone->isInterpreted());

    RootedScript script(cx, clone->nonLazyScript());
    JS_ASSERT(script);
    JS_ASSERT(script->compartment() == original->compartment());
    JS_ASSERT_IF(script->compartment() != cx->compartment(),
                 !script->enclosingStaticScope());

    RootedObject scope(cx, script->enclosingStaticScope());

    clone->mutableScript().init(nullptr);

    JSScript *cscript = CloneScript(cx, scope, clone, script, newKind);
    if (!cscript)
        return false;

    clone->setScript(cscript);
    cscript->setFunction(clone);

    script = clone->nonLazyScript();
    CallNewScriptHook(cx, script, clone);
    RootedGlobalObject global(cx, script->compileAndGo() ? &script->global() : nullptr);
    Debugger::onNewScript(cx, script, global);

    return true;
}

DebugScript *
JSScript::debugScript()
{
    JS_ASSERT(hasDebugScript_);
    DebugScriptMap *map = compartment()->debugScriptMap;
    JS_ASSERT(map);
    DebugScriptMap::Ptr p = map->lookup(this);
    JS_ASSERT(p);
    return p->value();
}

DebugScript *
JSScript::releaseDebugScript()
{
    JS_ASSERT(hasDebugScript_);
    DebugScriptMap *map = compartment()->debugScriptMap;
    JS_ASSERT(map);
    DebugScriptMap::Ptr p = map->lookup(this);
    JS_ASSERT(p);
    DebugScript *debug = p->value();
    map->remove(p);
    hasDebugScript_ = false;
    return debug;
}

void
JSScript::destroyDebugScript(FreeOp *fop)
{
    if (hasDebugScript_) {
        for (jsbytecode *pc = code(); pc < codeEnd(); pc++) {
            if (BreakpointSite *site = getBreakpointSite(pc)) {
                /* Breakpoints are swept before finalization. */
                JS_ASSERT(site->firstBreakpoint() == nullptr);
                site->clearTrap(fop, nullptr, nullptr);
                JS_ASSERT(getBreakpointSite(pc) == nullptr);
            }
        }
        fop->free_(releaseDebugScript());
    }
}

bool
JSScript::ensureHasDebugScript(JSContext *cx)
{
    if (hasDebugScript_)
        return true;

    size_t nbytes = offsetof(DebugScript, breakpoints) + length() * sizeof(BreakpointSite*);
    DebugScript *debug = (DebugScript *) cx->calloc_(nbytes);
    if (!debug)
        return false;

    /* Create compartment's debugScriptMap if necessary. */
    DebugScriptMap *map = compartment()->debugScriptMap;
    if (!map) {
        map = cx->new_<DebugScriptMap>();
        if (!map || !map->init()) {
            js_free(debug);
            js_delete(map);
            return false;
        }
        compartment()->debugScriptMap = map;
    }

    if (!map->putNew(this, debug)) {
        js_free(debug);
        return false;
    }
    hasDebugScript_ = true; // safe to set this;  we can't fail after this point

    /*
     * Ensure that any Interpret() instances running on this script have
     * interrupts enabled. The interrupts must stay enabled until the
     * debug state is destroyed.
     */
    for (ActivationIterator iter(cx->runtime()); !iter.done(); ++iter) {
        if (iter.activation()->isInterpreter())
            iter.activation()->asInterpreter()->enableInterruptsIfRunning(this);
    }

    return true;
}

void
JSScript::recompileForStepMode(FreeOp *fop)
{
#ifdef JS_ION
    if (hasBaselineScript())
        baseline->toggleDebugTraps(this, nullptr);
#endif
}

bool
JSScript::tryNewStepMode(JSContext *cx, uint32_t newValue)
{
    JS_ASSERT(hasDebugScript_);

    DebugScript *debug = debugScript();
    uint32_t prior = debug->stepMode;
    debug->stepMode = newValue;

    if (!prior != !newValue) {
        /* Step mode has been enabled or disabled. Alert the methodjit. */
        recompileForStepMode(cx->runtime()->defaultFreeOp());

        if (!stepModeEnabled() && !debug->numSites)
            js_free(releaseDebugScript());
    }

    return true;
}

bool
JSScript::setStepModeFlag(JSContext *cx, bool step)
{
    if (!ensureHasDebugScript(cx))
        return false;

    return tryNewStepMode(cx, (debugScript()->stepMode & stepCountMask) |
                               (step ? stepFlagMask : 0));
}

bool
JSScript::changeStepModeCount(JSContext *cx, int delta)
{
    if (!ensureHasDebugScript(cx))
        return false;

    assertSameCompartment(cx, this);
    JS_ASSERT_IF(delta > 0, cx->compartment()->debugMode());

    DebugScript *debug = debugScript();
    uint32_t count = debug->stepMode & stepCountMask;
    JS_ASSERT(((count + delta) & stepCountMask) == count + delta);
    return tryNewStepMode(cx,
                          (debug->stepMode & stepFlagMask) |
                          ((count + delta) & stepCountMask));
}

BreakpointSite *
JSScript::getOrCreateBreakpointSite(JSContext *cx, jsbytecode *pc)
{
    if (!ensureHasDebugScript(cx))
        return nullptr;

    DebugScript *debug = debugScript();
    BreakpointSite *&site = debug->breakpoints[pcToOffset(pc)];

    if (!site) {
        site = cx->runtime()->new_<BreakpointSite>(this, pc);
        if (!site) {
            js_ReportOutOfMemory(cx);
            return nullptr;
        }
        debug->numSites++;
    }

    return site;
}

void
JSScript::destroyBreakpointSite(FreeOp *fop, jsbytecode *pc)
{
    DebugScript *debug = debugScript();
    BreakpointSite *&site = debug->breakpoints[pcToOffset(pc)];
    JS_ASSERT(site);

    fop->delete_(site);
    site = nullptr;

    if (--debug->numSites == 0 && !stepModeEnabled())
        fop->free_(releaseDebugScript());
}

void
JSScript::clearBreakpointsIn(FreeOp *fop, js::Debugger *dbg, JSObject *handler)
{
    if (!hasAnyBreakpointsOrStepMode())
        return;

    for (jsbytecode *pc = code(); pc < codeEnd(); pc++) {
        BreakpointSite *site = getBreakpointSite(pc);
        if (site) {
            Breakpoint *nextbp;
            for (Breakpoint *bp = site->firstBreakpoint(); bp; bp = nextbp) {
                nextbp = bp->nextInSite();
                if ((!dbg || bp->debugger == dbg) && (!handler || bp->getHandler() == handler))
                    bp->destroy(fop);
            }
        }
    }
}

bool
JSScript::hasBreakpointsAt(jsbytecode *pc)
{
    BreakpointSite *site = getBreakpointSite(pc);
    if (!site)
        return false;

    return site->enabledCount > 0 || site->trapHandler;
}

void
JSScript::clearTraps(FreeOp *fop)
{
    if (!hasAnyBreakpointsOrStepMode())
        return;

    for (jsbytecode *pc = code(); pc < codeEnd(); pc++) {
        BreakpointSite *site = getBreakpointSite(pc);
        if (site)
            site->clearTrap(fop);
    }
}

void
JSScript::markChildren(JSTracer *trc)
{
    // NOTE: this JSScript may be partially initialized at this point.  E.g. we
    // may have created it and partially initialized it with
    // JSScript::Create(), but not yet finished initializing it with
    // fullyInitFromEmitter() or fullyInitTrivial().

    JS_ASSERT_IF(trc->runtime->gcStrictCompartmentChecking, zone()->isCollecting());

    for (uint32_t i = 0; i < natoms(); ++i) {
        if (atoms[i])
            MarkString(trc, &atoms[i], "atom");
    }

    if (hasObjects()) {
        ObjectArray *objarray = objects();
        MarkObjectRange(trc, objarray->length, objarray->vector, "objects");
    }

    if (hasRegexps()) {
        ObjectArray *objarray = regexps();
        MarkObjectRange(trc, objarray->length, objarray->vector, "objects");
    }

    if (hasConsts()) {
        ConstArray *constarray = consts();
        MarkValueRange(trc, constarray->length, constarray->vector, "consts");
    }

    if (sourceObject()) {
        JS_ASSERT(sourceObject()->compartment() == compartment());
        MarkObject(trc, &sourceObject_, "sourceObject");
    }

    if (functionNonDelazifying())
        MarkObject(trc, &function_, "function");

    if (enclosingScopeOrOriginalFunction_)
        MarkObject(trc, &enclosingScopeOrOriginalFunction_, "enclosing");

    if (maybeLazyScript())
        MarkLazyScriptUnbarriered(trc, &lazyScript, "lazyScript");

    if (IS_GC_MARKING_TRACER(trc)) {
        compartment()->mark();

        if (code())
            MarkScriptData(trc->runtime, code());
    }

    bindings.trace(trc);

    if (hasAnyBreakpointsOrStepMode()) {
        for (unsigned i = 0; i < length(); i++) {
            BreakpointSite *site = debugScript()->breakpoints[i];
            if (site && site->trapHandler)
                MarkValue(trc, &site->trapClosure, "trap closure");
        }
    }

#ifdef JS_ION
    jit::TraceIonScripts(trc, this);
#endif
}

void
LazyScript::markChildren(JSTracer *trc)
{
    if (function_)
        MarkObject(trc, &function_, "function");

    if (sourceObject_)
        MarkObject(trc, &sourceObject_, "sourceObject");

    if (enclosingScope_)
        MarkObject(trc, &enclosingScope_, "enclosingScope");

    if (script_)
        MarkScript(trc, &script_, "realScript");

    HeapPtrAtom *freeVariables = this->freeVariables();
    for (size_t i = 0; i < numFreeVariables(); i++)
        MarkString(trc, &freeVariables[i], "lazyScriptFreeVariable");

    HeapPtrFunction *innerFunctions = this->innerFunctions();
    for (size_t i = 0; i < numInnerFunctions(); i++)
        MarkObject(trc, &innerFunctions[i], "lazyScriptInnerFunction");
}

void
LazyScript::finalize(FreeOp *fop)
{
    if (table_)
        fop->free_(table_);
}

NestedScopeObject *
JSScript::getStaticScope(jsbytecode *pc)
{
    JS_ASSERT(containsPC(pc));

    if (!hasBlockScopes())
        return nullptr;

    ptrdiff_t offset = pc - main();

    if (offset < 0)
        return nullptr;

    BlockScopeArray *scopes = blockScopes();
    NestedScopeObject *blockChain = nullptr;

    // Find the innermost block chain using a binary search.
    size_t bottom = 0;
    size_t top = scopes->length;

    while (bottom < top) {
        size_t mid = bottom + (top - bottom) / 2;
        const BlockScopeNote *note = &scopes->vector[mid];
        if (note->start <= offset) {
            // Block scopes are ordered in the list by their starting offset, and since
            // blocks form a tree ones earlier in the list may cover the pc even if
            // later blocks end before the pc. This only happens when the earlier block
            // is a parent of the later block, so we need to check parents of |mid| in
            // the searched range for coverage.
            size_t check = mid;
            while (check >= bottom) {
                const BlockScopeNote *checkNote = &scopes->vector[check];
                JS_ASSERT(checkNote->start <= offset);
                if (offset < checkNote->start + checkNote->length) {
                    // We found a matching block chain but there may be inner ones
                    // at a higher block chain index than mid. Continue the binary search.
                    if (checkNote->index == BlockScopeNote::NoBlockScopeIndex)
                        blockChain = nullptr;
                    else
                        blockChain = &getObject(checkNote->index)->as<NestedScopeObject>();
                    break;
                }
                if (checkNote->parent == UINT32_MAX)
                    break;
                check = checkNote->parent;
            }
            bottom = mid + 1;
        } else {
            top = mid;
        }
    }

    return blockChain;
}

void
JSScript::setArgumentsHasVarBinding()
{
    argsHasVarBinding_ = true;
    needsArgsAnalysis_ = true;
}

void
JSScript::setNeedsArgsObj(bool needsArgsObj)
{
    JS_ASSERT(!analyzedArgsUsage());
    JS_ASSERT_IF(needsArgsObj, argumentsHasVarBinding());
    needsArgsAnalysis_ = false;
    needsArgsObj_ = needsArgsObj;
}

void
js::SetFrameArgumentsObject(JSContext *cx, AbstractFramePtr frame,
                            HandleScript script, JSObject *argsobj)
{
    /*
     * Replace any optimized arguments in the frame with an explicit arguments
     * object. Note that 'arguments' may have already been overwritten.
     */

    InternalBindingsHandle bindings(script, &script->bindings);
    const uint32_t var = Bindings::argumentsVarIndex(cx, bindings);

    if (script->varIsAliased(var)) {
        /*
         * Scan the script to find the slot in the call object that 'arguments'
         * is assigned to.
         */
        jsbytecode *pc = script->code();
        while (*pc != JSOP_ARGUMENTS)
            pc += GetBytecodeLength(pc);
        pc += JSOP_ARGUMENTS_LENGTH;
        JS_ASSERT(*pc == JSOP_SETALIASEDVAR);

        if (frame.callObj().as<ScopeObject>().aliasedVar(pc).isMagic(JS_OPTIMIZED_ARGUMENTS))
            frame.callObj().as<ScopeObject>().setAliasedVar(cx, pc, cx->names().arguments, ObjectValue(*argsobj));
    } else {
        if (frame.unaliasedLocal(var).isMagic(JS_OPTIMIZED_ARGUMENTS))
            frame.unaliasedLocal(var) = ObjectValue(*argsobj);
    }
}

/* static */ bool
JSScript::argumentsOptimizationFailed(JSContext *cx, HandleScript script)
{
    JS_ASSERT(script->functionNonDelazifying());
    JS_ASSERT(script->analyzedArgsUsage());
    JS_ASSERT(script->argumentsHasVarBinding());

    /*
     * It is possible that the arguments optimization has already failed,
     * everything has been fixed up, but there was an outstanding magic value
     * on the stack that has just now flowed into an apply. In this case, there
     * is nothing to do; GuardFunApplySpeculation will patch in the real
     * argsobj.
     */
    if (script->needsArgsObj())
        return true;

    JS_ASSERT(!script->isGenerator());

    {
        AutoLockForCompilation lock(cx);
        script->needsArgsObj_ = true;
    }

#ifdef JS_ION
    /*
     * Since we can't invalidate baseline scripts, set a flag that's checked from
     * JIT code to indicate the arguments optimization failed and JSOP_ARGUMENTS
     * should create an arguments object next time.
     */
    if (script->hasBaselineScript())
        script->baselineScript()->setNeedsArgsObj();
#endif

    /*
     * By design, the arguments optimization is only made when there are no
     * outstanding cases of MagicValue(JS_OPTIMIZED_ARGUMENTS) at any points
     * where the optimization could fail, other than an active invocation of
     * 'f.apply(x, arguments)'. Thus, there are no outstanding values of
     * MagicValue(JS_OPTIMIZED_ARGUMENTS) on the stack. However, there are
     * three things that need fixup:
     *  - there may be any number of activations of this script that don't have
     *    an argsObj that now need one.
     *  - jit code compiled (and possible active on the stack) with the static
     *    assumption of !script->needsArgsObj();
     *  - type inference data for the script assuming script->needsArgsObj
     */
    for (AllFramesIter i(cx); !i.done(); ++i) {
        /*
         * We cannot reliably create an arguments object for Ion activations of
         * this script.  To maintain the invariant that "script->needsArgsObj
         * implies fp->hasArgsObj", the Ion bail mechanism will create an
         * arguments object right after restoring the StackFrame and before
         * entering the interpreter (in jit::ThunkToInterpreter).  This delay is
         * safe since the engine avoids any observation of a StackFrame when it's
         * runningInJit (see ScriptFrameIter::interpFrame comment).
         */
        if (i.isIon())
            continue;
        AbstractFramePtr frame = i.abstractFramePtr();
        if (frame.isFunctionFrame() && frame.script() == script) {
            ArgumentsObject *argsobj = ArgumentsObject::createExpected(cx, frame);
            if (!argsobj) {
                /*
                 * We can't leave stack frames with script->needsArgsObj but no
                 * arguments object. It is, however, safe to leave frames with
                 * an arguments object but !script->needsArgsObj.
                 */
                script->needsArgsObj_ = false;
                return false;
            }

            SetFrameArgumentsObject(cx, frame, script, argsobj);
        }
    }

    return true;
}

bool
JSScript::varIsAliased(uint32_t varSlot)
{
    AutoThreadSafeAccess ts(this);
    return bindings.bindingIsAliased(bindings.numArgs() + varSlot);
}

bool
JSScript::formalIsAliased(unsigned argSlot)
{
    AutoThreadSafeAccess ts(this);
    return bindings.bindingIsAliased(argSlot);
}

bool
JSScript::formalLivesInArgumentsObject(unsigned argSlot)
{
    return argsObjAliasesFormals() && !formalIsAliased(argSlot);
}

LazyScript::LazyScript(JSFunction *fun, void *table, uint32_t numFreeVariables, uint32_t numInnerFunctions,
                       JSVersion version, uint32_t begin, uint32_t end, uint32_t lineno, uint32_t column)
  : script_(nullptr),
    function_(fun),
    enclosingScope_(nullptr),
    sourceObject_(nullptr),
    table_(table),
    version_(version),
    numFreeVariables_(numFreeVariables),
    numInnerFunctions_(numInnerFunctions),
    generatorKindBits_(GeneratorKindAsBits(NotGenerator)),
    strict_(false),
    bindingsAccessedDynamically_(false),
    hasDebuggerStatement_(false),
    directlyInsideEval_(false),
    usesArgumentsAndApply_(false),
    hasBeenCloned_(false),
    treatAsRunOnce_(false),
    begin_(begin),
    end_(end),
    lineno_(lineno),
    column_(column)
{
    JS_ASSERT(this->version() == version);
    JS_ASSERT(begin <= end);
}

void
LazyScript::initScript(JSScript *script)
{
    JS_ASSERT(script && !script_);
    script_ = script;
}

void
LazyScript::resetScript()
{
    JS_ASSERT(script_);
    script_ = nullptr;
}

void
LazyScript::setParent(JSObject *enclosingScope, ScriptSourceObject *sourceObject)
{
    JS_ASSERT(!sourceObject_ && !enclosingScope_);
    JS_ASSERT_IF(enclosingScope, function_->compartment() == enclosingScope->compartment());
    JS_ASSERT(function_->compartment() == sourceObject->compartment());

    enclosingScope_ = enclosingScope;
    sourceObject_ = sourceObject;
}

ScriptSourceObject *
LazyScript::sourceObject() const
{
    return sourceObject_ ? &sourceObject_->as<ScriptSourceObject>() : nullptr;
}

/* static */ LazyScript *
LazyScript::Create(ExclusiveContext *cx, HandleFunction fun,
                   uint32_t numFreeVariables, uint32_t numInnerFunctions, JSVersion version,
                   uint32_t begin, uint32_t end, uint32_t lineno, uint32_t column)
{
    JS_ASSERT(begin <= end);

    size_t bytes = (numFreeVariables * sizeof(HeapPtrAtom))
                 + (numInnerFunctions * sizeof(HeapPtrFunction));

    void *table = nullptr;
    if (bytes) {
        table = cx->malloc_(bytes);
        if (!table)
            return nullptr;
    }

    LazyScript *res = js_NewGCLazyScript(cx);
    if (!res)
        return nullptr;

    cx->compartment()->scheduleDelazificationForDebugMode();

    return new (res) LazyScript(fun, table, numFreeVariables, numInnerFunctions, version,
                                begin, end, lineno, column);
}

uint32_t
LazyScript::staticLevel(JSContext *cx) const
{
    for (StaticScopeIter<NoGC> ssi(enclosingScope()); !ssi.done(); ssi++) {
        if (ssi.type() == StaticScopeIter<NoGC>::FUNCTION)
            return ssi.funScript()->staticLevel() + 1;
    }
    return 1;
}

void
JSScript::updateBaselineOrIonRaw()
{
#ifdef JS_ION
    if (hasIonScript()) {
        baselineOrIonRaw = ion->method()->raw();
        baselineOrIonSkipArgCheck = ion->method()->raw() + ion->getSkipArgCheckEntryOffset();
    } else if (hasBaselineScript()) {
        baselineOrIonRaw = baseline->method()->raw();
        baselineOrIonSkipArgCheck = baseline->method()->raw();
    } else {
        baselineOrIonRaw = nullptr;
        baselineOrIonSkipArgCheck = nullptr;
    }
#endif
}

bool
JSScript::hasLoops()
{
    if (!hasTrynotes())
        return false;
    JSTryNote *tn = trynotes()->vector;
    JSTryNote *tnlimit = tn + trynotes()->length;
    for (; tn < tnlimit; tn++) {
        if (tn->kind == JSTRY_ITER || tn->kind == JSTRY_LOOP)
            return true;
    }
    return false;
}

static inline void
LazyScriptHash(uint32_t lineno, uint32_t column, uint32_t begin, uint32_t end,
               HashNumber hashes[3])
{
    HashNumber hash = lineno;
    hash = RotateLeft(hash, 4) ^ column;
    hash = RotateLeft(hash, 4) ^ begin;
    hash = RotateLeft(hash, 4) ^ end;

    hashes[0] = hash;
    hashes[1] = RotateLeft(hashes[0], 4) ^ begin;
    hashes[2] = RotateLeft(hashes[1], 4) ^ end;
}

void
LazyScriptHashPolicy::hash(const Lookup &lookup, HashNumber hashes[3])
{
    LazyScript *lazy = lookup.lazy;
    LazyScriptHash(lazy->lineno(), lazy->column(), lazy->begin(), lazy->end(), hashes);
}

void
LazyScriptHashPolicy::hash(JSScript *script, HashNumber hashes[3])
{
    LazyScriptHash(script->lineno(), script->column(), script->sourceStart(), script->sourceEnd(), hashes);
}

bool
LazyScriptHashPolicy::match(JSScript *script, const Lookup &lookup)
{
    JSContext *cx = lookup.cx;
    LazyScript *lazy = lookup.lazy;

    // To be a match, the script and lazy script need to have the same line
    // and column and to be at the same position within their respective
    // source blobs, and to have the same source contents and version.
    //
    // While the surrounding code in the source may differ, this is
    // sufficient to ensure that compiling the lazy script will yield an
    // identical result to compiling the original script.
    //
    // Note that the filenames and origin principals of the lazy script and
    // original script can differ. If there is a match, these will be fixed
    // up in the resulting clone by the caller.

    if (script->lineno() != lazy->lineno() ||
        script->column() != lazy->column() ||
        script->getVersion() != lazy->version() ||
        script->sourceStart() != lazy->begin() ||
        script->sourceEnd() != lazy->end())
    {
        return false;
    }

    SourceDataCache::AutoSuppressPurge asp(cx);

    const jschar *scriptChars = script->scriptSource()->chars(cx, asp);
    if (!scriptChars)
        return false;

    const jschar *lazyChars = lazy->source()->chars(cx, asp);
    if (!lazyChars)
        return false;

    size_t begin = script->sourceStart();
    size_t length = script->sourceEnd() - begin;
    return !memcmp(scriptChars + begin, lazyChars + begin, length);
}
