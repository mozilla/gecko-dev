/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS parser.
 *
 * This is a recursive-descent parser for the JavaScript language specified by
 * "The JavaScript 1.5 Language Specification".  It uses lexical and semantic
 * feedback to disambiguate non-LL(1) structures.  It generates trees of nodes
 * induced by the recursive parsing (not precise syntax trees, see Parser.h).
 * After tree construction, it rewrites trees to fold constants and evaluate
 * compile-time expressions.
 *
 * This parser attempts no error recovery.
 */

#include "frontend/Parser-inl.h"

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jstypes.h"

#include "asmjs/AsmJSValidate.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/FoldConstants.h"
#include "frontend/ParseMaps.h"
#include "frontend/TokenStream.h"
#include "vm/Shape.h"

#include "jsatominlines.h"
#include "jsscriptinlines.h"

#include "frontend/ParseNode-inl.h"
#include "vm/ScopeObject-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::Maybe;

using JS::AutoGCRooter;

namespace js {
namespace frontend {

typedef Rooted<StaticBlockObject*> RootedStaticBlockObject;
typedef Handle<StaticBlockObject*> HandleStaticBlockObject;
typedef Rooted<NestedScopeObject*> RootedNestedScopeObject;
typedef Handle<NestedScopeObject*> HandleNestedScopeObject;

/* Read a token. Report an error and return null() if that token isn't of type tt. */
#define MUST_MATCH_TOKEN(tt, errno)                                                         \
    JS_BEGIN_MACRO                                                                          \
        TokenKind token;                                                                    \
        if (!tokenStream.getToken(&token))                                                  \
            return null();                                                                  \
        if (token != tt) {                                                                  \
            report(ParseError, false, null(), errno);                                       \
            return null();                                                                  \
        }                                                                                   \
    JS_END_MACRO

static const unsigned BlockIdLimit = 1 << ParseNode::NumBlockIdBits;

template <typename ParseHandler>
bool
GenerateBlockId(TokenStream& ts, ParseContext<ParseHandler>* pc, uint32_t& blockid)
{
    if (pc->blockidGen == BlockIdLimit) {
        ts.reportError(JSMSG_NEED_DIET, "program");
        return false;
    }
    MOZ_ASSERT(pc->blockidGen < BlockIdLimit);
    blockid = pc->blockidGen++;
    return true;
}

template bool
GenerateBlockId(TokenStream& ts, ParseContext<SyntaxParseHandler>* pc, uint32_t& blockid);

template bool
GenerateBlockId(TokenStream& ts, ParseContext<FullParseHandler>* pc, uint32_t& blockid);

template <typename ParseHandler>
static void
PushStatementPC(ParseContext<ParseHandler>* pc, StmtInfoPC* stmt, StmtType type)
{
    stmt->blockid = pc->blockid();
    PushStatement(pc, stmt, type);
}

template <>
bool
ParseContext<FullParseHandler>::checkLocalsOverflow(TokenStream& ts)
{
    if (vars_.length() + bodyLevelLexicals_.length() >= LOCALNO_LIMIT) {
        ts.reportError(JSMSG_TOO_MANY_LOCALS);
        return false;
    }
    return true;
}

static void
MarkUsesAsHoistedLexical(ParseNode* pn)
{
    MOZ_ASSERT(pn->isDefn());

    Definition* dn = (Definition*)pn;
    ParseNode** pnup = &dn->dn_uses;
    ParseNode* pnu;
    unsigned start = pn->pn_blockid;

    // In ES6, lexical bindings cannot be accessed until initialized.
    // Distinguish hoisted uses as a different JSOp for easier compilation.
    while ((pnu = *pnup) != nullptr && pnu->pn_blockid >= start) {
        MOZ_ASSERT(pnu->isUsed());
        pnu->pn_dflags |= PND_LEXICAL;
        pnup = &pnu->pn_link;
    }
}

// See comment on member function declaration.
template <>
bool
ParseContext<FullParseHandler>::define(TokenStream& ts,
                                       HandlePropertyName name, ParseNode* pn, Definition::Kind kind)
{
    MOZ_ASSERT(!pn->isUsed());
    MOZ_ASSERT_IF(pn->isDefn(), pn->isPlaceholder());

    Definition* prevDef = nullptr;
    if (kind == Definition::LET || kind == Definition::CONST)
        prevDef = decls_.lookupFirst(name);
    else
        MOZ_ASSERT(!decls_.lookupFirst(name));

    if (!prevDef)
        prevDef = lexdeps.lookupDefn<FullParseHandler>(name);

    if (prevDef) {
        ParseNode** pnup = &prevDef->dn_uses;
        ParseNode* pnu;
        unsigned start = (kind == Definition::LET || kind == Definition::CONST) ? pn->pn_blockid
                                                                                : bodyid;

        while ((pnu = *pnup) != nullptr && pnu->pn_blockid >= start) {
            MOZ_ASSERT(pnu->pn_blockid >= bodyid);
            MOZ_ASSERT(pnu->isUsed());
            pnu->pn_lexdef = (Definition*) pn;
            pn->pn_dflags |= pnu->pn_dflags & PND_USE2DEF_FLAGS;
            pnup = &pnu->pn_link;
        }

        if (!pnu || pnu != prevDef->dn_uses) {
            *pnup = pn->dn_uses;
            pn->dn_uses = prevDef->dn_uses;
            prevDef->dn_uses = pnu;

            if (!pnu && prevDef->isPlaceholder())
                lexdeps->remove(name);
        }

        pn->pn_dflags |= prevDef->pn_dflags & PND_CLOSED;
    }

    MOZ_ASSERT_IF(kind != Definition::LET && kind != Definition::CONST, !lexdeps->lookup(name));
    pn->setDefn(true);
    pn->pn_dflags &= ~PND_PLACEHOLDER;
    if (kind == Definition::CONST)
        pn->pn_dflags |= PND_CONST;

    Definition* dn = (Definition*)pn;
    switch (kind) {
      case Definition::ARG:
        MOZ_ASSERT(sc->isFunctionBox());
        dn->setOp((js_CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETARG : JSOP_GETARG);
        dn->pn_blockid = bodyid;
        dn->pn_dflags |= PND_BOUND;
        if (!dn->pn_cookie.set(ts, staticLevel, args_.length()))
            return false;
        if (!args_.append(dn))
            return false;
        if (args_.length() >= ARGNO_LIMIT) {
            ts.reportError(JSMSG_TOO_MANY_FUN_ARGS);
            return false;
        }
        if (name == ts.names().empty)
            break;
        if (!decls_.addUnique(name, dn))
            return false;
        break;

      case Definition::GLOBALCONST:
      case Definition::VAR:
        if (sc->isFunctionBox()) {
            dn->setOp((js_CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETLOCAL : JSOP_GETLOCAL);
            dn->pn_blockid = bodyid;
            dn->pn_dflags |= PND_BOUND;
            if (!dn->pn_cookie.set(ts, staticLevel, vars_.length()))
                return false;
            if (!vars_.append(dn))
                return false;
            if (!checkLocalsOverflow(ts))
                return false;
        }
        if (!decls_.addUnique(name, dn))
            return false;
        break;

      case Definition::LET:
      case Definition::CONST:
        dn->setOp(JSOP_INITLEXICAL);
        dn->pn_dflags |= (PND_LEXICAL | PND_BOUND);
        MOZ_ASSERT(dn->pn_cookie.level() == staticLevel); /* see bindLet */
        if (atBodyLevel()) {
            if (!bodyLevelLexicals_.append(dn))
                return false;
            if (!checkLocalsOverflow(ts))
                return false;
        }

        // In ES6, lexical bindings cannot be accessed until initialized. If
        // the definition has existing uses, they need to be marked so that we
        // emit dead zone checks.
        MarkUsesAsHoistedLexical(pn);

        if (!decls_.addShadow(name, dn))
            return false;
        break;

      default:
        MOZ_CRASH("unexpected kind");
    }

    return true;
}

template <>
bool
ParseContext<SyntaxParseHandler>::checkLocalsOverflow(TokenStream& ts)
{
    return true;
}

template <>
bool
ParseContext<SyntaxParseHandler>::define(TokenStream& ts, HandlePropertyName name, Node pn,
                                         Definition::Kind kind)
{
    MOZ_ASSERT(!decls_.lookupFirst(name));

    if (lexdeps.lookupDefn<SyntaxParseHandler>(name))
        lexdeps->remove(name);

    // Keep track of the number of arguments in args_, for fun->nargs.
    if (kind == Definition::ARG) {
        if (!args_.append((Definition*) nullptr))
            return false;
        if (args_.length() >= ARGNO_LIMIT) {
            ts.reportError(JSMSG_TOO_MANY_FUN_ARGS);
            return false;
        }
    }

    return decls_.addUnique(name, kind);
}

template <typename ParseHandler>
void
ParseContext<ParseHandler>::prepareToAddDuplicateArg(HandlePropertyName name, DefinitionNode prevDecl)
{
    MOZ_ASSERT(decls_.lookupFirst(name) == prevDecl);
    decls_.remove(name);
}

template <typename ParseHandler>
void
ParseContext<ParseHandler>::updateDecl(JSAtom* atom, Node pn)
{
    Definition* oldDecl = decls_.lookupFirst(atom);

    pn->setDefn(true);
    Definition* newDecl = (Definition*)pn;
    decls_.updateFirst(atom, newDecl);

    if (!sc->isFunctionBox()) {
        MOZ_ASSERT(newDecl->isFreeVar());
        return;
    }

    MOZ_ASSERT(oldDecl->isBound());
    MOZ_ASSERT(!oldDecl->pn_cookie.isFree());
    newDecl->pn_cookie = oldDecl->pn_cookie;
    newDecl->pn_dflags |= PND_BOUND;
    if (IsArgOp(oldDecl->getOp())) {
        newDecl->setOp(JSOP_GETARG);
        MOZ_ASSERT(args_[oldDecl->pn_cookie.slot()] == oldDecl);
        args_[oldDecl->pn_cookie.slot()] = newDecl;
    } else {
        MOZ_ASSERT(IsLocalOp(oldDecl->getOp()));
        newDecl->setOp(JSOP_GETLOCAL);
        MOZ_ASSERT(vars_[oldDecl->pn_cookie.slot()] == oldDecl);
        vars_[oldDecl->pn_cookie.slot()] = newDecl;
    }
}

template <typename ParseHandler>
void
ParseContext<ParseHandler>::popLetDecl(JSAtom* atom)
{
    MOZ_ASSERT(ParseHandler::getDefinitionKind(decls_.lookupFirst(atom)) == Definition::LET ||
               ParseHandler::getDefinitionKind(decls_.lookupFirst(atom)) == Definition::CONST);
    decls_.remove(atom);
}

template <typename ParseHandler>
static void
AppendPackedBindings(const ParseContext<ParseHandler>* pc, const DeclVector& vec, Binding* dst,
                     uint32_t* numUnaliased = nullptr)
{
    for (size_t i = 0; i < vec.length(); ++i, ++dst) {
        Definition* dn = vec[i];
        PropertyName* name = dn->name();

        Binding::Kind kind;
        switch (dn->kind()) {
          case Definition::LET:
            // Treat body-level let declarations as var bindings by falling
            // through. The fact that the binding is in fact a let declaration
            // is reflected in the slot. All body-level lets go after the
            // vars.
          case Definition::VAR:
            kind = Binding::VARIABLE;
            break;
          case Definition::CONST:
          case Definition::GLOBALCONST:
            kind = Binding::CONSTANT;
            break;
          case Definition::ARG:
            kind = Binding::ARGUMENT;
            break;
          default:
            MOZ_CRASH("unexpected dn->kind");
        }

        /*
         * Bindings::init does not check for duplicates so we must ensure that
         * only one binding with a given name is marked aliased. pc->decls
         * maintains the canonical definition for each name, so use that.
         */
        MOZ_ASSERT_IF(dn->isClosed(), pc->decls().lookupFirst(name) == dn);
        bool aliased = dn->isClosed() ||
                       (pc->sc->allLocalsAliased() &&
                        pc->decls().lookupFirst(name) == dn);

        *dst = Binding(name, kind, aliased);
        if (!aliased && numUnaliased)
            ++*numUnaliased;
    }
}

template <typename ParseHandler>
bool
ParseContext<ParseHandler>::generateFunctionBindings(ExclusiveContext* cx, TokenStream& ts,
                                                     LifoAlloc& alloc,
                                                     InternalHandle<Bindings*> bindings) const
{
    MOZ_ASSERT(sc->isFunctionBox());
    MOZ_ASSERT(args_.length() < ARGNO_LIMIT);
    MOZ_ASSERT(vars_.length() + bodyLevelLexicals_.length() < LOCALNO_LIMIT);

    /*
     * Avoid pathological edge cases by explicitly limiting the total number of
     * bindings to what will fit in a uint32_t.
     */
    if (UINT32_MAX - args_.length() <= vars_.length() + bodyLevelLexicals_.length())
        return ts.reportError(JSMSG_TOO_MANY_LOCALS);

    // Fix up the slots of body-level lets to come after the vars now that we
    // know how many vars there are.
    for (size_t i = 0; i < bodyLevelLexicals_.length(); i++) {
        Definition* dn = bodyLevelLexicals_[i];
        if (!dn->pn_cookie.set(ts, dn->pn_cookie.level(), vars_.length() + i))
            return false;
    }

    uint32_t count = args_.length() + vars_.length() + bodyLevelLexicals_.length();
    Binding* packedBindings = alloc.newArrayUninitialized<Binding>(count);
    if (!packedBindings) {
        ReportOutOfMemory(cx);
        return false;
    }

    uint32_t numUnaliasedVars = 0;
    uint32_t numUnaliasedBodyLevelLexicals = 0;

    AppendPackedBindings(this, args_, packedBindings);
    AppendPackedBindings(this, vars_, packedBindings + args_.length(), &numUnaliasedVars);
    AppendPackedBindings(this, bodyLevelLexicals_,
                         packedBindings + args_.length() + vars_.length(), &numUnaliasedBodyLevelLexicals);

    return Bindings::initWithTemporaryStorage(cx, bindings, args_.length(), vars_.length(),
                                              bodyLevelLexicals_.length(), blockScopeDepth,
                                              numUnaliasedVars, numUnaliasedBodyLevelLexicals,
                                              packedBindings);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportHelper(ParseReportKind kind, bool strict, uint32_t offset,
                                   unsigned errorNumber, va_list args)
{
    bool result = false;
    switch (kind) {
      case ParseError:
        result = tokenStream.reportCompileErrorNumberVA(offset, JSREPORT_ERROR, errorNumber, args);
        break;
      case ParseWarning:
        result =
            tokenStream.reportCompileErrorNumberVA(offset, JSREPORT_WARNING, errorNumber, args);
        break;
      case ParseExtraWarning:
        result = tokenStream.reportStrictWarningErrorNumberVA(offset, errorNumber, args);
        break;
      case ParseStrictError:
        result = tokenStream.reportStrictModeErrorNumberVA(offset, strict, errorNumber, args);
        break;
    }
    return result;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::report(ParseReportKind kind, bool strict, Node pn, unsigned errorNumber, ...)
{
    uint32_t offset = (pn ? handler.getPosition(pn) : pos()).begin;

    va_list args;
    va_start(args, errorNumber);
    bool result = reportHelper(kind, strict, offset, errorNumber, args);
    va_end(args);
    return result;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportNoOffset(ParseReportKind kind, bool strict, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = reportHelper(kind, strict, TokenStream::NoOffset, errorNumber, args);
    va_end(args);
    return result;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportWithOffset(ParseReportKind kind, bool strict, uint32_t offset,
                                       unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = reportHelper(kind, strict, offset, errorNumber, args);
    va_end(args);
    return result;
}

template <>
bool
Parser<FullParseHandler>::abortIfSyntaxParser()
{
    handler.disableSyntaxParser();
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::abortIfSyntaxParser()
{
    abortedSyntaxParse = true;
    return false;
}

template <typename ParseHandler>
Parser<ParseHandler>::Parser(ExclusiveContext* cx, LifoAlloc* alloc,
                             const ReadOnlyCompileOptions& options,
                             const char16_t* chars, size_t length, bool foldConstants,
                             Parser<SyntaxParseHandler>* syntaxParser,
                             LazyScript* lazyOuterFunction)
  : AutoGCRooter(cx, PARSER),
    context(cx),
    alloc(*alloc),
    tokenStream(cx, options, chars, length, thisForCtor()),
    traceListHead(nullptr),
    pc(nullptr),
    sct(nullptr),
    ss(nullptr),
    keepAtoms(cx->perThreadData),
    foldConstants(foldConstants),
#ifdef DEBUG
    checkOptionsCalled(false),
#endif
    abortedSyntaxParse(false),
    isUnexpectedEOF_(false),
    handler(cx, *alloc, tokenStream, syntaxParser, lazyOuterFunction)
{
    {
        AutoLockForExclusiveAccess lock(cx);
        cx->perThreadData->addActiveCompilation();
    }

    // The Mozilla specific JSOPTION_EXTRA_WARNINGS option adds extra warnings
    // which are not generated if functions are parsed lazily. Note that the
    // standard "use strict" does not inhibit lazy parsing.
    if (options.extraWarningsOption)
        handler.disableSyntaxParser();

    tempPoolMark = alloc->mark();
}

template<typename ParseHandler>
bool
Parser<ParseHandler>::checkOptions()
{
#ifdef DEBUG
    checkOptionsCalled = true;
#endif

    if (!tokenStream.checkOptions())
        return false;

    return true;
}

template <typename ParseHandler>
Parser<ParseHandler>::~Parser()
{
    MOZ_ASSERT(checkOptionsCalled);

    alloc.release(tempPoolMark);

    /*
     * The parser can allocate enormous amounts of memory for large functions.
     * Eagerly free the memory now (which otherwise won't be freed until the
     * next GC) to avoid unnecessary OOMs.
     */
    alloc.freeAllIfHugeAndUnused();

    {
        AutoLockForExclusiveAccess lock(context);
        context->perThreadData->removeActiveCompilation();
    }
}

template <typename ParseHandler>
ObjectBox*
Parser<ParseHandler>::newObjectBox(JSObject* obj)
{
    MOZ_ASSERT(obj);

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */

    ObjectBox* objbox = alloc.new_<ObjectBox>(obj, traceListHead);
    if (!objbox) {
        ReportOutOfMemory(context);
        return nullptr;
    }

    traceListHead = objbox;

    return objbox;
}

template <typename ParseHandler>
FunctionBox::FunctionBox(ExclusiveContext* cx, ObjectBox* traceListHead, JSFunction* fun,
                         ParseContext<ParseHandler>* outerpc, Directives directives,
                         bool extraWarnings, GeneratorKind generatorKind)
  : ObjectBox(fun, traceListHead),
    SharedContext(cx, directives, extraWarnings),
    bindings(),
    bufStart(0),
    bufEnd(0),
    length(0),
    generatorKindBits_(GeneratorKindAsBits(generatorKind)),
    inWith_(false),                  // initialized below
    inGenexpLambda(false),
    hasDestructuringArgs(false),
    useAsm(false),
    insideUseAsm(outerpc && outerpc->useAsmOrInsideUseAsm()),
    usesArguments(false),
    usesApply(false),
    usesThis(false),
    funCxFlags()
{
    // Functions created at parse time may be set singleton after parsing and
    // baked into JIT code, so they must be allocated tenured. They are held by
    // the JSScript so cannot be collected during a minor GC anyway.
    MOZ_ASSERT(fun->isTenured());

    if (!outerpc) {
        inWith_ = false;

    } else if (outerpc->parsingWith) {
        // This covers cases that don't involve eval().  For example:
        //
        //   with (o) { (function() { g(); })(); }
        //
        // In this case, |outerpc| corresponds to global code, and
        // outerpc->parsingWith is true.
        inWith_ = true;

    } else if (outerpc->sc->isFunctionBox()) {
        // This is like the above case, but for more deeply nested functions.
        // For example:
        //
        //   with (o) { eval("(function() { (function() { g(); })(); })();"); } }
        //
        // In this case, the inner anonymous function needs to inherit the
        // setting of |inWith| from the outer one.
        FunctionBox* parent = outerpc->sc->asFunctionBox();
        if (parent && parent->inWith())
            inWith_ = true;
    } else {
        // This is like the above case, but when inside eval.
        //
        // For example:
        //
        //   with(o) { eval("(function() { g(); })();"); }
        //
        // In this case, the static scope chain tells us the presence of with.
        inWith_ = outerpc->sc->inWith();
    }
}

template <typename ParseHandler>
FunctionBox*
Parser<ParseHandler>::newFunctionBox(Node fn, JSFunction* fun, ParseContext<ParseHandler>* outerpc,
                                     Directives inheritedDirectives, GeneratorKind generatorKind)
{
    MOZ_ASSERT(fun);

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */
    FunctionBox* funbox =
        alloc.new_<FunctionBox>(context, traceListHead, fun, outerpc,
                                inheritedDirectives, options().extraWarningsOption,
                                generatorKind);
    if (!funbox) {
        ReportOutOfMemory(context);
        return nullptr;
    }

    traceListHead = funbox;
    if (fn)
        handler.setFunctionBox(fn, funbox);

    return funbox;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::trace(JSTracer* trc)
{
    traceListHead->trace(trc);
}

void
MarkParser(JSTracer* trc, AutoGCRooter* parser)
{
    static_cast<Parser<FullParseHandler>*>(parser)->trace(trc);
}

/*
 * Parse a top-level JS script.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::parse(JSObject* chain)
{
    MOZ_ASSERT(checkOptionsCalled);

    /*
     * Protect atoms from being collected by a GC activation, which might
     * - nest on this thread due to out of memory (the so-called "last ditch"
     *   GC attempted within js_NewGCThing), or
     * - run for any reason on another thread if this thread is suspended on
     *   an object lock before it finishes generating bytecode into a script
     *   protected from the GC by a root or a stack frame reference.
     */
    Directives directives(options().strictOption);
    GlobalSharedContext globalsc(context, directives,
                                 /* staticEvalScope = */ nullptr,
                                 options().extraWarningsOption);
    ParseContext<ParseHandler> globalpc(this, /* parent = */ nullptr, ParseHandler::null(),
                                        &globalsc, /* newDirectives = */ nullptr,
                                        /* staticLevel = */ 0, /* bodyid = */ 0,
                                        /* blockScopeDepth = */ 0);
    if (!globalpc.init(tokenStream))
        return null();

    Node pn = statements(YieldIsName);
    if (pn) {
        TokenKind tt;
        if (!tokenStream.getToken(&tt))
            return null();
        if (tt != TOK_EOF) {
            report(ParseError, false, null(), JSMSG_GARBAGE_AFTER_INPUT,
                   "script", TokenKindToDesc(tt));
            return null();
        }
        if (foldConstants) {
            if (!FoldConstants(context, &pn, this))
                return null();
        }
    }
    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportBadReturn(Node pn, ParseReportKind kind,
                                      unsigned errnum, unsigned anonerrnum)
{
    JSAutoByteString name;
    JSAtom* atom = pc->sc->asFunctionBox()->function()->atom();
    if (atom) {
        if (!AtomToPrintableString(context, atom, &name))
            return false;
    } else {
        errnum = anonerrnum;
    }
    return report(kind, pc->sc->strict(), pn, errnum, name.ptr());
}

/*
 * Check that it is permitted to introduce a binding for atom.  Strict mode
 * forbids introducing new definitions for 'eval', 'arguments', or for any
 * strict mode reserved keyword.  Use pn for reporting error locations, or use
 * pc's token stream if pn is nullptr.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::checkStrictBinding(PropertyName* name, Node pn)
{
    if (!pc->sc->needStrictChecks())
        return true;

    if (name == context->names().eval || name == context->names().arguments || IsKeyword(name)) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(context, name, &bytes))
            return false;
        return report(ParseStrictError, pc->sc->strict(), pn,
                      JSMSG_BAD_BINDING, bytes.ptr());
    }

    return true;
}

template <>
ParseNode*
Parser<FullParseHandler>::standaloneFunctionBody(HandleFunction fun, const AutoNameVector& formals,
                                                 GeneratorKind generatorKind,
                                                 Directives inheritedDirectives,
                                                 Directives* newDirectives)
{
    MOZ_ASSERT(checkOptionsCalled);

    Node fn = handler.newFunctionDefinition();
    if (!fn)
        return null();

    ParseNode* argsbody = handler.newList(PNK_ARGSBODY);
    if (!argsbody)
        return null();
    fn->pn_body = argsbody;

    FunctionBox* funbox = newFunctionBox(fn, fun, /* outerpc = */ nullptr, inheritedDirectives,
                                         generatorKind);
    if (!funbox)
        return null();
    funbox->length = fun->nargs() - fun->hasRest();
    handler.setFunctionBox(fn, funbox);

    ParseContext<FullParseHandler> funpc(this, pc, fn, funbox, newDirectives,
                                         /* staticLevel = */ 0, /* bodyid = */ 0,
                                         /* blockScopeDepth = */ 0);
    if (!funpc.init(tokenStream))
        return null();

    for (unsigned i = 0; i < formals.length(); i++) {
        if (!defineArg(fn, formals[i]))
            return null();
    }

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;
    ParseNode* pn = functionBody(InAllowed, yieldHandling, Statement, StatementListBody);
    if (!pn)
        return null();

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();
    if (tt != TOK_EOF) {
        report(ParseError, false, null(), JSMSG_GARBAGE_AFTER_INPUT,
               "function body", TokenKindToDesc(tt));
        return null();
    }

    if (!FoldConstants(context, &pn, this))
        return null();

    fn->pn_pos.end = pos().end;

    MOZ_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
    fn->pn_body->append(pn);

    /*
     * Make sure to deoptimize lexical dependencies that are polluted
     * by eval and function statements (which both flag the function as
     * having an extensible scope).
     */
    if (funbox->hasExtensibleScope() && pc->lexdeps->count()) {
        for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
            Definition* dn = r.front().value().get<FullParseHandler>();
            MOZ_ASSERT(dn->isPlaceholder());

            handler.deoptimizeUsesWithin(dn, fn->pn_pos);
        }
    }

    InternalHandle<Bindings*> funboxBindings =
        InternalHandle<Bindings*>::fromMarkedLocation(&funbox->bindings);
    if (!funpc.generateFunctionBindings(context, tokenStream, alloc, funboxBindings))
        return null();

    return fn;
}

template <>
bool
Parser<FullParseHandler>::checkFunctionArguments()
{
    /*
     * Non-top-level functions use JSOP_DEFFUN which is a dynamic scope
     * operation which means it aliases any bindings with the same name.
     */
    if (FuncStmtSet* set = pc->funcStmts) {
        for (FuncStmtSet::Range r = set->all(); !r.empty(); r.popFront()) {
            PropertyName* name = r.front()->asPropertyName();
            if (Definition* dn = pc->decls().lookupFirst(name))
                dn->pn_dflags |= PND_CLOSED;
        }
    }

    /* Time to implement the odd semantics of 'arguments'. */
    HandlePropertyName arguments = context->names().arguments;

    /*
     * As explained by the ContextFlags::funArgumentsHasLocalBinding comment,
     * create a declaration for 'arguments' if there are any unbound uses in
     * the function body.
     */
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
        if (r.front().key() == arguments) {
            Definition* dn = r.front().value().get<FullParseHandler>();
            pc->lexdeps->remove(arguments);
            dn->pn_dflags |= PND_IMPLICITARGUMENTS;
            if (!pc->define(tokenStream, arguments, dn, Definition::VAR))
                return false;
            pc->sc->asFunctionBox()->usesArguments = true;
            break;
        }
    }

    /*
     * Report error if both rest parameters and 'arguments' are used. Do this
     * check before adding artificial 'arguments' below.
     */
    Definition* maybeArgDef = pc->decls().lookupFirst(arguments);
    bool argumentsHasBinding = !!maybeArgDef;
    // ES6 9.2.13.17 says that a lexical binding of 'arguments' shadows the
    // arguments object.
    bool argumentsHasLocalBinding = maybeArgDef && (maybeArgDef->kind() != Definition::ARG &&
                                                    maybeArgDef->kind() != Definition::LET &&
                                                    maybeArgDef->kind() != Definition::CONST);
    bool hasRest = pc->sc->asFunctionBox()->function()->hasRest();
    if (hasRest && argumentsHasLocalBinding) {
        report(ParseError, false, nullptr, JSMSG_ARGUMENTS_AND_REST);
        return false;
    }

    /*
     * Even if 'arguments' isn't explicitly mentioned, dynamic name lookup
     * forces an 'arguments' binding. The exception is that functions with rest
     * parameters are free from 'arguments'.
     */
    if (!argumentsHasBinding && pc->sc->bindingsAccessedDynamically() && !hasRest) {
        ParseNode* pn = newName(arguments);
        if (!pn)
            return false;
        if (!pc->define(tokenStream, arguments, pn, Definition::VAR))
            return false;
        argumentsHasBinding = true;
        argumentsHasLocalBinding = true;
    }

    /*
     * Now that all possible 'arguments' bindings have been added, note whether
     * 'arguments' has a local binding and whether it unconditionally needs an
     * arguments object. (Also see the flags' comments in ContextFlags.)
     */
    if (argumentsHasLocalBinding) {
        FunctionBox* funbox = pc->sc->asFunctionBox();
        funbox->setArgumentsHasLocalBinding();

        /*
         * If a script has both explicit mentions of 'arguments' and dynamic
         * name lookups which could access the arguments, an arguments object
         * must be created eagerly. The SSA analysis used for lazy arguments
         * cannot cope with dynamic name accesses, so any 'arguments' accessed
         * via a NAME opcode must force construction of the arguments object.
         */
        if (pc->sc->bindingsAccessedDynamically() && maybeArgDef)
            funbox->setDefinitelyNeedsArgsObj();

        /*
         * If a script contains the debugger statement either directly or
         * within an inner function, the arguments object must be created
         * eagerly. The debugger can walk the scope chain and observe any
         * values along it.
         */
        if (pc->sc->hasDebuggerStatement())
            funbox->setDefinitelyNeedsArgsObj();

        /*
         * Check whether any parameters have been assigned within this
         * function. In strict mode parameters do not alias arguments[i], and
         * to make the arguments object reflect initial parameter values prior
         * to any mutation we create it eagerly whenever parameters are (or
         * might, in the case of calls to eval) be assigned.
         */
        if (pc->sc->needStrictChecks()) {
            for (AtomDefnListMap::Range r = pc->decls().all(); !r.empty(); r.popFront()) {
                DefinitionList& dlist = r.front().value();
                for (DefinitionList::Range dr = dlist.all(); !dr.empty(); dr.popFront()) {
                    Definition* dn = dr.front<FullParseHandler>();
                    if (dn->kind() == Definition::ARG && dn->isAssigned())
                        funbox->setDefinitelyNeedsArgsObj();
                }
            }
            /* Watch for mutation of arguments through e.g. eval(). */
            if (pc->sc->bindingsAccessedDynamically())
                funbox->setDefinitelyNeedsArgsObj();
        }
    }

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkFunctionArguments()
{
    bool hasRest = pc->sc->asFunctionBox()->function()->hasRest();

    if (pc->lexdeps->lookup(context->names().arguments)) {
        pc->sc->asFunctionBox()->usesArguments = true;
        if (hasRest) {
            report(ParseError, false, null(), JSMSG_ARGUMENTS_AND_REST);
            return false;
        }
    } else if (hasRest) {
        DefinitionNode maybeArgDef = pc->decls().lookupFirst(context->names().arguments);
        if (maybeArgDef && handler.getDefinitionKind(maybeArgDef) != Definition::ARG) {
            report(ParseError, false, null(), JSMSG_ARGUMENTS_AND_REST);
            return false;
        }
    }

    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionBody(InHandling inHandling, YieldHandling yieldHandling,
                                   FunctionSyntaxKind kind, FunctionBodyType type)
{
    MOZ_ASSERT(pc->sc->isFunctionBox());
    MOZ_ASSERT(!pc->funHasReturnExpr && !pc->funHasReturnVoid);

#ifdef DEBUG
    uint32_t startYieldOffset = pc->lastYieldOffset;
#endif

    Node pn;
    if (type == StatementListBody) {
        pn = statements(yieldHandling);
        if (!pn)
            return null();
    } else {
        MOZ_ASSERT(type == ExpressionBody);

        Node kid = assignExpr(inHandling, yieldHandling);
        if (!kid)
            return null();

        pn = handler.newReturnStatement(kid, null(), handler.getPosition(kid));
        if (!pn)
            return null();
    }

    switch (pc->generatorKind()) {
      case NotGenerator:
        MOZ_ASSERT(pc->lastYieldOffset == startYieldOffset);
        break;

      case LegacyGenerator:
        // FIXME: Catch these errors eagerly, in Parser::yieldExpression.
        MOZ_ASSERT(pc->lastYieldOffset != startYieldOffset);
        if (kind == Arrow) {
            reportWithOffset(ParseError, false, pc->lastYieldOffset,
                             JSMSG_YIELD_IN_ARROW, js_yield_str);
            return null();
        }
        if (type == ExpressionBody) {
            reportBadReturn(pn, ParseError,
                            JSMSG_BAD_GENERATOR_RETURN,
                            JSMSG_BAD_ANON_GENERATOR_RETURN);
            return null();
        }
        break;

      case StarGenerator:
        MOZ_ASSERT(kind != Arrow);
        MOZ_ASSERT(type == StatementListBody);
        break;
    }

    if (pc->isGenerator()) {
        MOZ_ASSERT(type == StatementListBody);
        Node generator = newName(context->names().dotGenerator);
        if (!generator)
            return null();
        if (!pc->define(tokenStream, context->names().dotGenerator, generator, Definition::VAR))
            return null();

        if (pc->isStarGenerator()) {
            Node genrval = newName(context->names().dotGenRVal);
            if (!genrval)
                return null();
            if (!pc->define(tokenStream, context->names().dotGenRVal, genrval, Definition::VAR))
                return null();
        }

        generator = newName(context->names().dotGenerator);
        if (!generator)
            return null();
        if (!noteNameUse(context->names().dotGenerator, generator))
            return null();
        if (!handler.prependInitialYield(pn, generator))
            return null();
    }

    /* Define the 'arguments' binding if necessary. */
    if (!checkFunctionArguments())
        return null();

    return pn;
}

/* See comment for use in Parser::functionDef. */
template <>
bool
Parser<FullParseHandler>::makeDefIntoUse(Definition* dn, ParseNode* pn, JSAtom* atom)
{
    /* Turn pn into a definition. */
    pc->updateDecl(atom, pn);

    /* Change all uses of dn to be uses of pn. */
    for (ParseNode* pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
        MOZ_ASSERT(pnu->isUsed());
        MOZ_ASSERT(!pnu->isDefn());
        pnu->pn_lexdef = (Definition*) pn;
        pn->pn_dflags |= pnu->pn_dflags & PND_USE2DEF_FLAGS;
    }
    pn->pn_dflags |= dn->pn_dflags & PND_USE2DEF_FLAGS;
    pn->dn_uses = dn;

    /*
     * A PNK_FUNCTION node must be a definition, so convert shadowed function
     * statements into nops. This is valid since all body-level function
     * statement initialization happens at the beginning of the function
     * (thus, only the last statement's effect is visible). E.g., in
     *
     *   function outer() {
     *     function g() { return 1 }
     *     assertEq(g(), 2);
     *     function g() { return 2 }
     *     assertEq(g(), 2);
     *   }
     *
     * both asserts are valid.
     */
    if (dn->getKind() == PNK_FUNCTION) {
        MOZ_ASSERT(dn->functionIsHoisted());
        pn->dn_uses = dn->pn_link;
        handler.prepareNodeForMutation(dn);
        dn->setKind(PNK_NOP);
        dn->setArity(PN_NULLARY);
        dn->setDefn(false);
        return true;
    }

    /*
     * If dn is in [var, const, let] and has an initializer, then we
     * must rewrite it to be an assignment node, whose freshly allocated
     * left-hand side becomes a use of pn.
     */
    if (dn->canHaveInitializer()) {
        if (ParseNode* rhs = dn->expr()) {
            ParseNode* lhs = handler.makeAssignment(dn, rhs);
            if (!lhs)
                return false;
            pn->dn_uses = lhs;
            dn->pn_link = nullptr;
            dn = (Definition*) lhs;
        }
    }

    /* Turn dn into a use of pn. */
    MOZ_ASSERT(dn->isKind(PNK_NAME));
    MOZ_ASSERT(dn->isArity(PN_NAME));
    MOZ_ASSERT(dn->pn_atom == atom);
    dn->setOp((js_CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETNAME : JSOP_GETNAME);
    dn->setDefn(false);
    dn->setUsed(true);
    dn->pn_lexdef = (Definition*) pn;
    dn->pn_cookie.makeFree();
    dn->pn_dflags &= ~PND_BOUND;
    return true;
}

/*
 * Parameter block types for the several Binder functions.  We use a common
 * helper function signature in order to share code among destructuring and
 * simple variable declaration parsers.  In the destructuring case, the binder
 * function is called indirectly from the variable declaration parser by way
 * of checkDestructuringPattern and its friends.
 */

template <typename ParseHandler>
struct BindData
{
    explicit BindData(ExclusiveContext* cx) : let(cx) {}

    typedef bool
    (*Binder)(BindData* data, HandlePropertyName name, Parser<ParseHandler>* parser);

    /* name node for definition processing and error source coordinates */
    typename ParseHandler::Node pn;

    JSOp            op;         /* prologue bytecode or nop */
    Binder          binder;     /* binder, discriminates u */
    bool            isConst;    /* const binding? */

    struct LetData {
        explicit LetData(ExclusiveContext* cx) : blockObj(cx) {}
        VarContext varContext;
        RootedStaticBlockObject blockObj;
        unsigned   overflow;
    } let;

    void initLexical(VarContext varContext, StaticBlockObject* blockObj, unsigned overflow,
                     bool isConst = false) {
        this->pn = ParseHandler::null();
        this->op = JSOP_INITLEXICAL;
        this->isConst = isConst;
        this->binder = Parser<ParseHandler>::bindLexical;
        this->let.varContext = varContext;
        this->let.blockObj = blockObj;
        this->let.overflow = overflow;
    }

    void initVarOrGlobalConst(JSOp op) {
        this->op = op;
        this->isConst = op == JSOP_DEFCONST;
        this->binder = Parser<ParseHandler>::bindVarOrGlobalConst;
    }
};

template <typename ParseHandler>
JSFunction*
Parser<ParseHandler>::newFunction(HandleAtom atom, FunctionSyntaxKind kind,
                                  GeneratorKind generatorKind, HandleObject proto)
{
    MOZ_ASSERT_IF(kind == Statement, atom != nullptr);

    RootedFunction fun(context);

    gc::AllocKind allocKind = gc::AllocKind::FUNCTION;
    JSFunction::Flags flags;
    switch (kind) {
      case Expression:
        flags = JSFunction::INTERPRETED_LAMBDA;
        break;
      case Arrow:
        flags = JSFunction::INTERPRETED_LAMBDA_ARROW;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case Method:
        MOZ_ASSERT(generatorKind == NotGenerator || generatorKind == StarGenerator);
        if (generatorKind == NotGenerator)
            flags = JSFunction::INTERPRETED_METHOD;
        else
            flags = JSFunction::INTERPRETED_METHOD_GENERATOR;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case ClassConstructor:
      case DerivedClassConstructor:
        flags = JSFunction::INTERPRETED_CLASS_CONSTRUCTOR;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case Getter:
        flags = JSFunction::INTERPRETED_GETTER;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case Setter:
        flags = JSFunction::INTERPRETED_SETTER;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      default:
        flags = JSFunction::INTERPRETED_NORMAL;
        break;
    }

    fun = NewFunctionWithProto(context, nullptr, 0, flags, nullptr, atom, proto,
                               allocKind, TenuredObject);
    if (!fun)
        return nullptr;
    if (options().selfHostingMode)
        fun->setIsSelfHostedBuiltin();
    return fun;
}

static bool
MatchOrInsertSemicolon(TokenStream& ts)
{
    TokenKind tt;
    if (!ts.peekTokenSameLine(&tt, TokenStream::Operand))
        return false;
    if (tt != TOK_EOF && tt != TOK_EOL && tt != TOK_SEMI && tt != TOK_RC) {
        /* Advance the scanner for proper error location reporting. */
        ts.consumeKnownToken(tt);
        ts.reportError(JSMSG_SEMI_BEFORE_STMNT);
        return false;
    }
    bool ignored;
    return ts.matchToken(&ignored, TOK_SEMI);
}

template <typename ParseHandler>
typename ParseHandler::DefinitionNode
Parser<ParseHandler>::getOrCreateLexicalDependency(ParseContext<ParseHandler>* pc, JSAtom* atom)
{
    AtomDefnAddPtr p = pc->lexdeps->lookupForAdd(atom);
    if (p)
        return p.value().get<ParseHandler>();

    DefinitionNode dn = handler.newPlaceholder(atom, pc->blockid(), pos());
    if (!dn)
        return ParseHandler::nullDefinition();
    DefinitionSingle def = DefinitionSingle::new_<ParseHandler>(dn);
    if (!pc->lexdeps->add(p, atom, def))
        return ParseHandler::nullDefinition();
    return dn;
}

static bool
ConvertDefinitionToNamedLambdaUse(TokenStream& ts, ParseContext<FullParseHandler>* pc,
                                  FunctionBox* funbox, Definition* dn)
{
    dn->setOp(JSOP_CALLEE);
    if (!dn->pn_cookie.set(ts, pc->staticLevel, 0))
        return false;
    dn->pn_dflags |= PND_BOUND;
    MOZ_ASSERT(dn->kind() == Definition::NAMED_LAMBDA);

    /*
     * Since 'dn' is a placeholder, it has not been defined in the
     * ParseContext and hence we must manually flag a closed-over
     * callee name as needing a dynamic scope (this is done for all
     * definitions in the ParseContext by generateFunctionBindings).
     *
     * If 'dn' has been assigned to, then we also flag the function
     * scope has needing a dynamic scope so that dynamic scope
     * setter can either ignore the set (in non-strict mode) or
     * produce an error (in strict mode).
     */
    if (dn->isClosed() || dn->isAssigned())
        funbox->setNeedsDeclEnvObject();
    return true;
}

static bool
IsNonDominatingInScopedSwitch(ParseContext<FullParseHandler>* pc, HandleAtom name,
                              Definition* dn)
{
    MOZ_ASSERT(dn->isLexical());
    StmtInfoPC* stmt = LexicalLookup(pc, name);
    if (stmt && stmt->type == STMT_SWITCH)
        return dn->pn_cookie.slot() < stmt->firstDominatingLexicalInCase;
    return false;
}

static void
AssociateUsesWithOuterDefinition(ParseNode* pnu, Definition* dn, Definition* outer_dn,
                                 bool markUsesAsLexical)
{
    uint32_t dflags = markUsesAsLexical ? PND_LEXICAL : 0;
    while (true) {
        pnu->pn_lexdef = outer_dn;
        pnu->pn_dflags |= dflags;
        if (!pnu->pn_link)
            break;
        pnu = pnu->pn_link;
    }
    pnu->pn_link = outer_dn->dn_uses;
    outer_dn->dn_uses = dn->dn_uses;
    dn->dn_uses = nullptr;
}

/*
 * Beware: this function is called for functions nested in other functions or
 * global scripts but not for functions compiled through the Function
 * constructor or JSAPI. To always execute code when a function has finished
 * parsing, use Parser::functionBody.
 */
template <>
bool
Parser<FullParseHandler>::leaveFunction(ParseNode* fn, ParseContext<FullParseHandler>* outerpc,
                                        FunctionSyntaxKind kind)
{
    outerpc->blockidGen = pc->blockidGen;

    bool bodyLevel = outerpc->atBodyLevel();
    FunctionBox* funbox = fn->pn_funbox;
    MOZ_ASSERT(funbox == pc->sc->asFunctionBox());

    /* Propagate unresolved lexical names up to outerpc->lexdeps. */
    if (pc->lexdeps->count()) {
        for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
            JSAtom* atom = r.front().key();
            Definition* dn = r.front().value().get<FullParseHandler>();
            MOZ_ASSERT(dn->isPlaceholder());

            if (atom == funbox->function()->name() && kind == Expression) {
                if (!ConvertDefinitionToNamedLambdaUse(tokenStream, pc, funbox, dn))
                    return false;
                continue;
            }

            Definition* outer_dn = outerpc->decls().lookupFirst(atom);

            /*
             * Make sure to deoptimize lexical dependencies that are polluted
             * by eval and function statements (which both flag the function as
             * having an extensible scope) or any enclosing 'with'.
             */
            if (funbox->hasExtensibleScope() || outerpc->parsingWith)
                handler.deoptimizeUsesWithin(dn, fn->pn_pos);

            if (!outer_dn) {
                /*
                 * Create a new placeholder for our outer lexdep. We could
                 * simply re-use the inner placeholder, but that introduces
                 * subtleties in the case where we find a later definition
                 * that captures an existing lexdep. For example:
                 *
                 *   function f() { function g() { x; } let x; }
                 *
                 * Here, g's TOK_UPVARS node lists the placeholder for x,
                 * which must be captured by the 'let' declaration later,
                 * since 'let's are hoisted.  Taking g's placeholder as our
                 * own would work fine. But consider:
                 *
                 *   function f() { x; { function g() { x; } let x; } }
                 *
                 * Here, the 'let' must not capture all the uses of f's
                 * lexdep entry for x, but it must capture the x node
                 * referred to from g's TOK_UPVARS node.  Always turning
                 * inherited lexdeps into uses of a new outer definition
                 * allows us to handle both these cases in a natural way.
                 */
                outer_dn = getOrCreateLexicalDependency(outerpc, atom);
                if (!outer_dn)
                    return false;
            }

            /*
             * Insert dn's uses list at the front of outer_dn's list.
             *
             * Without loss of generality or correctness, we allow a dn to
             * be in inner and outer lexdeps, since the purpose of lexdeps
             * is one-pass coordination of name use and definition across
             * functions, and if different dn's are used we'll merge lists
             * when leaving the inner function.
             *
             * The dn == outer_dn case arises with generator expressions
             * (see LegacyCompExprTransplanter::transplant, the PN_CODE/PN_NAME
             * case), and nowhere else, currently.
             */
            if (dn != outer_dn) {
                if (ParseNode* pnu = dn->dn_uses) {
                    // In ES6, lexical bindings cannot be accessed until
                    // initialized. If we are parsing a body-level function,
                    // it is hoisted to the top, so we conservatively mark all
                    // uses linked to an outer lexical binding as needing TDZ
                    // checks. e.g.,
                    //
                    // function outer() {
                    //   inner2();
                    //   function inner() { use(x); }
                    //   function inner2() { inner(); }
                    //   let x;
                    // }
                    //
                    // The use of 'x' inside 'inner' needs to be marked.
                    //
                    // Note that to not be fully conservative requires a call
                    // graph analysis of all body-level functions to compute
                    // the transitive closure of which hoisted body level use
                    // of which function forces TDZ checks on which uses. This
                    // is unreasonably difficult to do in a single pass parser
                    // like ours.
                    //
                    // Similarly, if we are closing over a lexical binding
                    // from another case in a switch, those uses also need to
                    // be marked as needing dead zone checks.
                    RootedAtom name(context, atom);
                    bool markUsesAsLexical = outer_dn->isLexical() &&
                                             (bodyLevel ||
                                              IsNonDominatingInScopedSwitch(outerpc, name, outer_dn));
                    AssociateUsesWithOuterDefinition(pnu, dn, outer_dn, markUsesAsLexical);
                }

                outer_dn->pn_dflags |= dn->pn_dflags & ~PND_PLACEHOLDER;
            }

            /* Mark the outer dn as escaping. */
            outer_dn->pn_dflags |= PND_CLOSED;
        }
    }

    InternalHandle<Bindings*> bindings =
        InternalHandle<Bindings*>::fromMarkedLocation(&funbox->bindings);
    return pc->generateFunctionBindings(context, tokenStream, alloc, bindings);
}

template <>
bool
Parser<SyntaxParseHandler>::leaveFunction(Node fn, ParseContext<SyntaxParseHandler>* outerpc,
                                          FunctionSyntaxKind kind)
{
    outerpc->blockidGen = pc->blockidGen;

    FunctionBox* funbox = pc->sc->asFunctionBox();
    return addFreeVariablesFromLazyFunction(funbox->function(), outerpc);
}

/*
 * defineArg is called for both the arguments of a regular function definition
 * and the arguments specified by the Function constructor.
 *
 * The 'disallowDuplicateArgs' bool indicates whether the use of another
 * feature (destructuring or default arguments) disables duplicate arguments.
 * (ECMA-262 requires us to support duplicate parameter names, but, for newer
 * features, we consider the code to have "opted in" to higher standards and
 * forbid duplicates.)
 *
 * If 'duplicatedArg' is non-null, then DefineArg assigns to it any previous
 * argument with the same name. The caller may use this to report an error when
 * one of the abovementioned features occurs after a duplicate.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::defineArg(Node funcpn, HandlePropertyName name,
                                bool disallowDuplicateArgs, Node* duplicatedArg)
{
    SharedContext* sc = pc->sc;

    /* Handle duplicate argument names. */
    if (DefinitionNode prevDecl = pc->decls().lookupFirst(name)) {
        Node pn = handler.getDefinitionNode(prevDecl);

        /*
         * Strict-mode disallows duplicate args. We may not know whether we are
         * in strict mode or not (since the function body hasn't been parsed).
         * In such cases, report will queue up the potential error and return
         * 'true'.
         */
        if (sc->needStrictChecks()) {
            JSAutoByteString bytes;
            if (!AtomToPrintableString(context, name, &bytes))
                return false;
            if (!report(ParseStrictError, pc->sc->strict(), pn,
                        JSMSG_DUPLICATE_FORMAL, bytes.ptr()))
            {
                return false;
            }
        }

        if (disallowDuplicateArgs) {
            report(ParseError, false, pn, JSMSG_BAD_DUP_ARGS);
            return false;
        }

        if (duplicatedArg)
            *duplicatedArg = pn;

        /* ParseContext::define assumes and asserts prevDecl is not in decls. */
        MOZ_ASSERT(handler.getDefinitionKind(prevDecl) == Definition::ARG);
        pc->prepareToAddDuplicateArg(name, prevDecl);
    }

    Node argpn = newName(name);
    if (!argpn)
        return false;

    if (!checkStrictBinding(name, argpn))
        return false;

    handler.addFunctionArgument(funcpn, argpn);
    return pc->define(tokenStream, name, argpn, Definition::ARG);
}

template <typename ParseHandler>
/* static */ bool
Parser<ParseHandler>::bindDestructuringArg(BindData<ParseHandler>* data,
                                           HandlePropertyName name, Parser<ParseHandler>* parser)
{
    ParseContext<ParseHandler>* pc = parser->pc;
    MOZ_ASSERT(pc->sc->isFunctionBox());

    if (pc->decls().lookupFirst(name)) {
        parser->report(ParseError, false, null(), JSMSG_BAD_DUP_ARGS);
        return false;
    }

    if (!parser->checkStrictBinding(name, data->pn))
        return false;

    return pc->define(parser->tokenStream, name, data->pn, Definition::VAR);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::functionArguments(YieldHandling yieldHandling, FunctionSyntaxKind kind,
                                        Node funcpn, bool* hasRest)
{
    FunctionBox* funbox = pc->sc->asFunctionBox();

    *hasRest = false;

    bool parenFreeArrow = false;
    if (kind == Arrow) {
        TokenKind tt;
        if (!tokenStream.peekToken(&tt))
            return false;
        if (tt == TOK_NAME)
            parenFreeArrow = true;
    }
    if (!parenFreeArrow) {
        TokenKind tt;
        if (!tokenStream.getToken(&tt))
            return false;
        if (tt != TOK_LP) {
            report(ParseError, false, null(),
                   kind == Arrow ? JSMSG_BAD_ARROW_ARGS : JSMSG_PAREN_BEFORE_FORMAL);
            return false;
        }

        // Record the start of function source (for FunctionToString). If we
        // are parenFreeArrow, we will set this below, after consuming the NAME.
        funbox->setStart(tokenStream);
    }

    Node argsbody = handler.newList(PNK_ARGSBODY);
    if (!argsbody)
        return false;
    handler.setFunctionBody(funcpn, argsbody);

    bool hasArguments = false;
    if (parenFreeArrow) {
        hasArguments = true;
    } else {
        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_RP))
            return false;
        if (!matched)
            hasArguments = true;
    }
    if (hasArguments) {
        bool hasDefaults = false;
        Node duplicatedArg = null();
        bool disallowDuplicateArgs = kind == Arrow || kind == Method || kind == ClassConstructor;

        if (kind == Getter) {
            report(ParseError, false, null(), JSMSG_ACCESSOR_WRONG_ARGS, "getter", "no", "s");
            return false;
        }

        while (true) {
            if (*hasRest) {
                report(ParseError, false, null(), JSMSG_PARAMETER_AFTER_REST);
                return false;
            }

            TokenKind tt;
            if (!tokenStream.getToken(&tt))
                return false;
            MOZ_ASSERT_IF(parenFreeArrow, tt == TOK_NAME);
            switch (tt) {
              case TOK_LB:
              case TOK_LC:
              {
                /* See comment below in the TOK_NAME case. */
                disallowDuplicateArgs = true;
                if (duplicatedArg) {
                    report(ParseError, false, duplicatedArg, JSMSG_BAD_DUP_ARGS);
                    return false;
                }

                funbox->hasDestructuringArgs = true;

                /*
                 * A destructuring formal parameter turns into one or more
                 * local variables initialized from properties of a single
                 * anonymous positional parameter, so here we must tweak our
                 * binder and its data.
                 */
                BindData<ParseHandler> data(context);
                data.pn = ParseHandler::null();
                data.op = JSOP_DEFVAR;
                data.binder = bindDestructuringArg;
                Node destruct = destructuringExprWithoutYield(yieldHandling, &data, tt,
                                                              JSMSG_YIELD_IN_DEFAULT);
                if (!destruct)
                    return false;

                /*
                 * Make a single anonymous positional parameter, and store
                 * destructuring expression into the node.
                 */
                HandlePropertyName name = context->names().empty;
                Node arg = newName(name);
                if (!arg)
                    return false;

                handler.addFunctionArgument(funcpn, arg);
                if (!pc->define(tokenStream, name, arg, Definition::ARG))
                    return false;

                handler.setLastFunctionArgumentDestructuring(funcpn, destruct);
                break;
              }

              case TOK_YIELD:
                if (!checkYieldNameValidity())
                    return false;
                MOZ_ASSERT(yieldHandling == YieldIsName);
                goto TOK_NAME;

              case TOK_TRIPLEDOT:
              {
                if (kind == Setter) {
                    report(ParseError, false, null(),
                           JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
                    return false;
                }
                *hasRest = true;
                if (!tokenStream.getToken(&tt))
                    return false;
                // FIXME: This fails to handle a rest parameter named |yield|
                //        correctly outside of generators: that is,
                //        |var f = (...yield) => 42;| should be valid code!
                //        When this is fixed, make sure to consult both
                //        |yieldHandling| and |checkYieldNameValidity| for
                //        correctness until legacy generator syntax is removed.
                if (tt != TOK_NAME) {
                    report(ParseError, false, null(), JSMSG_NO_REST_NAME);
                    return false;
                }
                disallowDuplicateArgs = true;
                if (duplicatedArg) {
                    // Has duplicated args before the rest parameter.
                    report(ParseError, false, duplicatedArg, JSMSG_BAD_DUP_ARGS);
                    return false;
                }
                goto TOK_NAME;
              }

              TOK_NAME:
              case TOK_NAME:
              {
                if (parenFreeArrow)
                    funbox->setStart(tokenStream);

                RootedPropertyName name(context, tokenStream.currentName());
                if (!defineArg(funcpn, name, disallowDuplicateArgs, &duplicatedArg))
                    return false;
                break;
              }

              default:
                report(ParseError, false, null(), JSMSG_MISSING_FORMAL);
                return false;
            }

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_ASSIGN))
                return false;
            if (matched) {
                // A default argument without parentheses would look like:
                // a = expr => body, but both operators are right-associative, so
                // that would have been parsed as a = (expr => body) instead.
                // Therefore it's impossible to get here with parenFreeArrow.
                MOZ_ASSERT(!parenFreeArrow);

                if (*hasRest) {
                    report(ParseError, false, null(), JSMSG_REST_WITH_DEFAULT);
                    return false;
                }
                disallowDuplicateArgs = true;
                if (duplicatedArg) {
                    report(ParseError, false, duplicatedArg, JSMSG_BAD_DUP_ARGS);
                    return false;
                }
                if (!hasDefaults) {
                    hasDefaults = true;

                    // The Function.length property is the number of formals
                    // before the first default argument.
                    funbox->length = pc->numArgs() - 1;
                }
                Node def_expr = assignExprWithoutYield(yieldHandling, JSMSG_YIELD_IN_DEFAULT);
                if (!def_expr)
                    return false;
                if (!handler.setLastFunctionArgumentDefault(funcpn, def_expr))
                    return false;
            }

            if (parenFreeArrow || kind == Setter)
                break;

            if (!tokenStream.matchToken(&matched, TOK_COMMA))
                return false;
            if (!matched)
                break;
        }

        if (!parenFreeArrow) {
            TokenKind tt;
            if (!tokenStream.getToken(&tt))
                return false;
            if (tt != TOK_RP) {
                if (kind == Setter) {
                    report(ParseError, false, null(),
                           JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
                    return false;
                }

                report(ParseError, false, null(), JSMSG_PAREN_AFTER_FORMAL);
                return false;
            }
        }

        if (!hasDefaults)
            funbox->length = pc->numArgs() - *hasRest;
    } else if (kind == Setter) {
        report(ParseError, false, null(), JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
        return false;
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::checkFunctionDefinition(HandlePropertyName funName,
                                                  ParseNode** pn_, FunctionSyntaxKind kind,
                                                  bool* pbodyProcessed)
{
    ParseNode*& pn = *pn_;
    *pbodyProcessed = false;

    /* Function statements add a binding to the enclosing scope. */
    bool bodyLevel = pc->atBodyLevel();

    if (kind == Statement) {
        /*
         * Handle redeclaration and optimize cases where we can statically bind the
         * function (thereby avoiding JSOP_DEFFUN and dynamic name lookup).
         */
        if (Definition* dn = pc->decls().lookupFirst(funName)) {
            MOZ_ASSERT(!dn->isUsed());
            MOZ_ASSERT(dn->isDefn());

            bool throwRedeclarationError = dn->kind() == Definition::GLOBALCONST ||
                                           dn->kind() == Definition::CONST ||
                                           dn->kind() == Definition::LET;
            if (options().extraWarningsOption || throwRedeclarationError) {
                JSAutoByteString name;
                ParseReportKind reporter = throwRedeclarationError
                                           ? ParseError
                                           : ParseExtraWarning;
                if (!AtomToPrintableString(context, funName, &name) ||
                    !report(reporter, false, nullptr, JSMSG_REDECLARED_VAR,
                            Definition::kindString(dn->kind()), name.ptr()))
                {
                    return false;
                }
            }

            /*
             * Body-level function statements are effectively variable
             * declarations where the initialization is hoisted to the
             * beginning of the block. This means that any other variable
             * declaration with the same name is really just an assignment to
             * the function's binding (which is mutable), so turn any existing
             * declaration into a use.
             */
            if (bodyLevel) {
                if (dn->kind() == Definition::ARG) {
                    // The exception to the above comment is when the function
                    // has the same name as an argument. Then the argument node
                    // remains a definition. But change the function node pn so
                    // that it knows where the argument is located.
                    pn->setOp(JSOP_GETARG);
                    pn->setDefn(true);
                    pn->pn_cookie = dn->pn_cookie;
                    pn->pn_dflags |= PND_BOUND;
                    dn->markAsAssigned();
                } else {
                    if (!makeDefIntoUse(dn, pn, funName))
                        return false;
                }
            }
        } else if (bodyLevel) {
            /*
             * If this function was used before it was defined, claim the
             * pre-created definition node for this function that primaryExpr
             * put in pc->lexdeps on first forward reference, and recycle pn.
             */
            if (Definition* fn = pc->lexdeps.lookupDefn<FullParseHandler>(funName)) {
                MOZ_ASSERT(fn->isDefn());
                fn->setKind(PNK_FUNCTION);
                fn->setArity(PN_CODE);
                fn->pn_pos.begin = pn->pn_pos.begin;
                fn->pn_pos.end = pn->pn_pos.end;

                fn->pn_body = nullptr;
                fn->pn_cookie.makeFree();

                pc->lexdeps->remove(funName);
                handler.freeTree(pn);
                pn = fn;
            }

            if (!pc->define(tokenStream, funName, pn, Definition::VAR))
                return false;
        }

        if (bodyLevel) {
            MOZ_ASSERT(pn->functionIsHoisted());
            MOZ_ASSERT_IF(pc->sc->isFunctionBox(), !pn->pn_cookie.isFree());
            MOZ_ASSERT_IF(!pc->sc->isFunctionBox(), pn->pn_cookie.isFree());
        } else {
            /*
             * As a SpiderMonkey-specific extension, non-body-level function
             * statements (e.g., functions in an "if" or "while" block) are
             * dynamically bound when control flow reaches the statement.
             */
            MOZ_ASSERT(!pc->sc->strict());
            MOZ_ASSERT(pn->pn_cookie.isFree());
            if (pc->sc->isFunctionBox()) {
                FunctionBox* funbox = pc->sc->asFunctionBox();
                funbox->setMightAliasLocals();
                funbox->setHasExtensibleScope();
            }
            pn->setOp(JSOP_DEFFUN);

            /*
             * Instead of setting bindingsAccessedDynamically, which would be
             * overly conservative, remember the names of all function
             * statements and mark any bindings with the same as aliased at the
             * end of functionBody.
             */
            if (!pc->funcStmts) {
                pc->funcStmts = alloc.new_<FuncStmtSet>(alloc);
                if (!pc->funcStmts || !pc->funcStmts->init())
                    return false;
            }
            if (!pc->funcStmts->put(funName))
                return false;

            /*
             * Due to the implicit declaration mechanism, 'arguments' will not
             * have decls and, even if it did, they will not be noted as closed
             * in the emitter. Thus, in the corner case of function statements
             * overridding arguments, flag the whole scope as dynamic.
             */
            if (funName == context->names().arguments)
                pc->sc->setBindingsAccessedDynamically();
        }

        /* No further binding (in BindNameToSlot) is needed for functions. */
        pn->pn_dflags |= PND_BOUND;
    } else {
        /* A function expression does not introduce any binding. */
        pn->setOp(kind == Arrow ? JSOP_LAMBDA_ARROW : JSOP_LAMBDA);
    }

    // When a lazily-parsed function is called, we only fully parse (and emit)
    // that function, not any of its nested children. The initial syntax-only
    // parse recorded the free variables of nested functions and their extents,
    // so we can skip over them after accounting for their free variables.
    if (LazyScript* lazyOuter = handler.lazyOuterFunction()) {
        JSFunction* fun = handler.nextLazyInnerFunction();
        MOZ_ASSERT(!fun->isLegacyGenerator());
        FunctionBox* funbox = newFunctionBox(pn, fun, pc, Directives(/* strict = */ false),
                                             fun->generatorKind());
        if (!funbox)
            return false;

        if (!addFreeVariablesFromLazyFunction(fun, pc))
            return false;

        // The position passed to tokenStream.advance() is an offset of the sort
        // returned by userbuf.offset() and expected by userbuf.rawCharPtrAt(),
        // while LazyScript::{begin,end} offsets are relative to the outermost
        // script source.
        uint32_t userbufBase = lazyOuter->begin() - lazyOuter->column();
        if (!tokenStream.advance(fun->lazyScript()->end() - userbufBase))
            return false;

        *pbodyProcessed = true;
        return true;
    }

    return true;
}

template <class T, class U>
static inline void
PropagateTransitiveParseFlags(const T* inner, U* outer)
{
    if (inner->bindingsAccessedDynamically())
        outer->setBindingsAccessedDynamically();
    if (inner->hasDebuggerStatement())
        outer->setHasDebuggerStatement();
    if (inner->hasDirectEval())
        outer->setHasDirectEval();
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::addFreeVariablesFromLazyFunction(JSFunction* fun,
                                                       ParseContext<ParseHandler>* pc)
{
    // Update any definition nodes in this context according to free variables
    // in a lazily parsed inner function.

    bool bodyLevel = pc->atBodyLevel();
    LazyScript* lazy = fun->lazyScript();
    LazyScript::FreeVariable* freeVariables = lazy->freeVariables();
    for (size_t i = 0; i < lazy->numFreeVariables(); i++) {
        JSAtom* atom = freeVariables[i].atom();

        // 'arguments' will be implicitly bound within the inner function.
        if (atom == context->names().arguments)
            continue;

        DefinitionNode dn = pc->decls().lookupFirst(atom);

        if (!dn) {
            dn = getOrCreateLexicalDependency(pc, atom);
            if (!dn)
                return false;
        }

        // In ES6, lexical bindings are unaccessible before initialization. If
        // the inner function closes over a placeholder definition, we need to
        // mark the variable as maybe needing a dead zone check when we emit
        // bytecode.
        //
        // Note that body-level function declaration statements are always
        // hoisted to the top, so all accesses to free let variables need the
        // dead zone check.
        //
        // Subtlety: we don't need to check for closing over a non-dominating
        // lexical binding in a switch, as lexical declarations currently
        // disable syntax parsing. So a non-dominating but textually preceding
        // lexical declaration would have aborted syntax parsing, and a
        // textually following declaration would return true for
        // handler.isPlaceholderDefinition(dn) below.
        if (handler.isPlaceholderDefinition(dn) || bodyLevel)
            freeVariables[i].setIsHoistedUse();

        /* Mark the outer dn as escaping. */
        handler.setFlag(handler.getDefinitionNode(dn), PND_CLOSED);
    }

    PropagateTransitiveParseFlags(lazy, pc->sc);
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkFunctionDefinition(HandlePropertyName funName,
                                                    Node* pn, FunctionSyntaxKind kind,
                                                    bool* pbodyProcessed)
{
    *pbodyProcessed = false;

    /* Function statements add a binding to the enclosing scope. */
    bool bodyLevel = pc->atBodyLevel();

    if (kind == Statement) {
        /*
         * Handle redeclaration and optimize cases where we can statically bind the
         * function (thereby avoiding JSOP_DEFFUN and dynamic name lookup).
         */
        if (DefinitionNode dn = pc->decls().lookupFirst(funName)) {
            if (dn == Definition::GLOBALCONST ||
                dn == Definition::CONST       ||
                dn == Definition::LET)
            {
                JSAutoByteString name;
                if (!AtomToPrintableString(context, funName, &name) ||
                    !report(ParseError, false, null(), JSMSG_REDECLARED_VAR,
                            Definition::kindString(dn), name.ptr()))
                {
                    return false;
                }
            }
        } else if (bodyLevel) {
            if (pc->lexdeps.lookupDefn<SyntaxParseHandler>(funName))
                pc->lexdeps->remove(funName);

            if (!pc->define(tokenStream, funName, *pn, Definition::VAR))
                return false;
        }

        if (!bodyLevel && funName == context->names().arguments)
            pc->sc->setBindingsAccessedDynamically();
    }

    if (kind == Arrow) {
        /* Arrow functions cannot yet be parsed lazily. */
        return abortIfSyntaxParser();
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::addExprAndGetNextTemplStrToken(YieldHandling yieldHandling, Node nodeList,
                                                     TokenKind* ttp)
{
    Node pn = expr(InAllowed, yieldHandling);
    if (!pn)
        return false;
    handler.addList(nodeList, pn);

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return false;
    if (tt != TOK_RC) {
        report(ParseError, false, null(), JSMSG_TEMPLSTR_UNTERM_EXPR);
        return false;
    }

    return tokenStream.getToken(ttp, TokenStream::TemplateTail);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::taggedTemplate(YieldHandling yieldHandling, Node nodeList, TokenKind tt)
{
    Node callSiteObjNode = handler.newCallSiteObject(pos().begin, pc->blockidGen);
    if (!callSiteObjNode)
        return false;
    handler.addList(nodeList, callSiteObjNode);

    while (true) {
        if (!appendToCallSiteObj(callSiteObjNode))
            return false;
        if (tt != TOK_TEMPLATE_HEAD)
            break;

        if (!addExprAndGetNextTemplStrToken(yieldHandling, nodeList, &tt))
            return false;
    }
    handler.setEndPosition(nodeList, callSiteObjNode);
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::templateLiteral(YieldHandling yieldHandling)
{
    Node pn = noSubstitutionTemplate();
    if (!pn)
        return null();
    Node nodeList = handler.newList(PNK_TEMPLATE_STRING_LIST, pn);

    TokenKind tt;
    do {
        if (!addExprAndGetNextTemplStrToken(yieldHandling, nodeList, &tt))
            return null();

        pn = noSubstitutionTemplate();
        if (!pn)
            return null();

        handler.addList(nodeList, pn);
    } while (tt == TOK_TEMPLATE_HEAD);
    return nodeList;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionDef(InHandling inHandling, YieldHandling yieldHandling,
                                  HandlePropertyName funName, FunctionSyntaxKind kind,
                                  GeneratorKind generatorKind, InvokedPrediction invoked)
{
    MOZ_ASSERT_IF(kind == Statement, funName);

    /* Make a TOK_FUNCTION node. */
    Node pn = handler.newFunctionDefinition();
    if (!pn)
        return null();

    if (invoked)
        pn = handler.setLikelyIIFE(pn);

    bool bodyProcessed;
    if (!checkFunctionDefinition(funName, &pn, kind, &bodyProcessed))
        return null();

    if (bodyProcessed)
        return pn;

    RootedObject proto(context);
    if (generatorKind == StarGenerator) {
        // If we are off the main thread, the generator meta-objects have
        // already been created by js::StartOffThreadParseScript, so cx will not
        // be necessary.
        JSContext* cx = context->maybeJSContext();
        proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, context->global());
        if (!proto)
            return null();
    }
    RootedFunction fun(context, newFunction(funName, kind, generatorKind, proto));
    if (!fun)
        return null();

    // Speculatively parse using the directives of the parent parsing context.
    // If a directive is encountered (e.g., "use strict") that changes how the
    // function should have been parsed, we backup and reparse with the new set
    // of directives.
    Directives directives(pc);
    Directives newDirectives = directives;

    TokenStream::Position start(keepAtoms);
    tokenStream.tell(&start);

    while (true) {
        if (functionArgsAndBody(inHandling, pn, fun, kind, generatorKind, directives,
                                &newDirectives))
        {
            break;
        }
        if (tokenStream.hadError() || directives == newDirectives)
            return null();

        // Assignment must be monotonic to prevent reparsing iloops
        MOZ_ASSERT_IF(directives.strict(), newDirectives.strict());
        MOZ_ASSERT_IF(directives.asmJS(), newDirectives.asmJS());
        directives = newDirectives;

        tokenStream.seek(start);

        // functionArgsAndBody may have already set pn->pn_body before failing.
        handler.setFunctionBody(pn, null());
    }

    return pn;
}

template <>
bool
Parser<FullParseHandler>::finishFunctionDefinition(ParseNode* pn, FunctionBox* funbox,
                                                   ParseNode* body)
{
    pn->pn_pos.end = pos().end;

    MOZ_ASSERT(pn->pn_funbox == funbox);
    MOZ_ASSERT(pn->pn_body->isKind(PNK_ARGSBODY));
    pn->pn_body->append(body);

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::finishFunctionDefinition(Node pn, FunctionBox* funbox,
                                                     Node body)
{
    // The LazyScript for a lazily parsed function needs to be constructed
    // while its ParseContext and associated lexdeps and inner functions are
    // still available.

    if (funbox->inWith())
        return abortIfSyntaxParser();

    size_t numFreeVariables = pc->lexdeps->count();
    size_t numInnerFunctions = pc->innerFunctions.length();

    RootedFunction fun(context, funbox->function());
    LazyScript* lazy = LazyScript::CreateRaw(context, fun, numFreeVariables, numInnerFunctions,
                                             versionNumber(), funbox->bufStart, funbox->bufEnd,
                                             funbox->startLine, funbox->startColumn);
    if (!lazy)
        return false;

    LazyScript::FreeVariable* freeVariables = lazy->freeVariables();
    size_t i = 0;
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront())
        freeVariables[i++] = LazyScript::FreeVariable(r.front().key());
    MOZ_ASSERT(i == numFreeVariables);

    HeapPtrFunction* innerFunctions = lazy->innerFunctions();
    for (size_t i = 0; i < numInnerFunctions; i++)
        innerFunctions[i].init(pc->innerFunctions[i]);

    if (pc->sc->strict())
        lazy->setStrict();
    lazy->setGeneratorKind(funbox->generatorKind());
    if (funbox->usesArguments && funbox->usesApply && funbox->usesThis)
        lazy->setUsesArgumentsApplyAndThis();
    if (funbox->isDerivedClassConstructor())
        lazy->setIsDerivedClassConstructor();
    PropagateTransitiveParseFlags(funbox, lazy);

    fun->initLazyScript(lazy);
    return true;
}

template <>
bool
Parser<FullParseHandler>::functionArgsAndBody(InHandling inHandling, ParseNode* pn,
                                              HandleFunction fun, FunctionSyntaxKind kind,
                                              GeneratorKind generatorKind,
                                              Directives inheritedDirectives,
                                              Directives* newDirectives)
{
    ParseContext<FullParseHandler>* outerpc = pc;

    // Create box for fun->object early to protect against last-ditch GC.
    FunctionBox* funbox = newFunctionBox(pn, fun, pc, inheritedDirectives, generatorKind);
    if (!funbox)
        return false;

    if (kind == DerivedClassConstructor)
        funbox->setDerivedClassConstructor();

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;

    // Try a syntax parse for this inner function.
    do {
        // If we're assuming this function is an IIFE, always perform a full
        // parse to avoid the overhead of a lazy syntax-only parse. Although
        // the prediction may be incorrect, IIFEs are common enough that it
        // pays off for lots of code.
        if (pn->isLikelyIIFE() && !funbox->isGenerator())
            break;

        Parser<SyntaxParseHandler>* parser = handler.syntaxParser;
        if (!parser)
            break;

        {
            // Move the syntax parser to the current position in the stream.
            TokenStream::Position position(keepAtoms);
            tokenStream.tell(&position);
            if (!parser->tokenStream.seek(position, tokenStream))
                return false;

            ParseContext<SyntaxParseHandler> funpc(parser, outerpc, SyntaxParseHandler::null(), funbox,
                                                   newDirectives, outerpc->staticLevel + 1,
                                                   outerpc->blockidGen, /* blockScopeDepth = */ 0);
            if (!funpc.init(tokenStream))
                return false;

            if (!parser->functionArgsAndBodyGeneric(inHandling, yieldHandling,
                                                    SyntaxParseHandler::NodeGeneric, fun, kind))
            {
                if (parser->hadAbortedSyntaxParse()) {
                    // Try again with a full parse.
                    parser->clearAbortedSyntaxParse();
                    MOZ_ASSERT_IF(parser->context->isJSContext(),
                                  !parser->context->asJSContext()->isExceptionPending());
                    break;
                }
                return false;
            }

            outerpc->blockidGen = funpc.blockidGen;

            // Advance this parser over tokens processed by the syntax parser.
            parser->tokenStream.tell(&position);
            if (!tokenStream.seek(position, parser->tokenStream))
                return false;

            // Update the end position of the parse node.
            pn->pn_pos.end = tokenStream.currentToken().pos.end;
        }

        if (!addFreeVariablesFromLazyFunction(fun, pc))
            return false;

        pn->pn_blockid = outerpc->blockid();
        PropagateTransitiveParseFlags(funbox, outerpc->sc);
        return true;
    } while (false);

    // Continue doing a full parse for this inner function.
    ParseContext<FullParseHandler> funpc(this, pc, pn, funbox, newDirectives,
                                         outerpc->staticLevel + 1, outerpc->blockidGen,
                                         /* blockScopeDepth = */ 0);
    if (!funpc.init(tokenStream))
        return false;

    if (!functionArgsAndBodyGeneric(inHandling, yieldHandling, pn, fun, kind))
        return false;

    if (!leaveFunction(pn, outerpc, kind))
        return false;

    pn->pn_blockid = outerpc->blockid();

    /*
     * Fruit of the poisonous tree: if a closure contains a dynamic name access
     * (eval, with, etc), we consider the parent to do the same. The reason is
     * that the deoptimizing effects of dynamic name access apply equally to
     * parents: any local can be read at runtime.
     */
    PropagateTransitiveParseFlags(funbox, outerpc->sc);
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::functionArgsAndBody(InHandling inHandling, Node pn, HandleFunction fun,
                                                FunctionSyntaxKind kind,
                                                GeneratorKind generatorKind,
                                                Directives inheritedDirectives,
                                                Directives* newDirectives)
{
    ParseContext<SyntaxParseHandler>* outerpc = pc;

    // Create box for fun->object early to protect against last-ditch GC.
    FunctionBox* funbox = newFunctionBox(pn, fun, pc, inheritedDirectives, generatorKind);
    if (!funbox)
        return false;

    // Initialize early for possible flags mutation via destructuringExpr.
    ParseContext<SyntaxParseHandler> funpc(this, pc, handler.null(), funbox, newDirectives,
                                           outerpc->staticLevel + 1, outerpc->blockidGen,
                                           /* blockScopeDepth = */ 0);
    if (!funpc.init(tokenStream))
        return false;

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;
    if (!functionArgsAndBodyGeneric(inHandling, yieldHandling, pn, fun, kind))
        return false;

    if (!leaveFunction(pn, outerpc, kind))
        return false;

    // This is a lazy function inner to another lazy function. Remember the
    // inner function so that if the outer function is eventually parsed we do
    // not need any further parsing or processing of the inner function.
    MOZ_ASSERT(fun->lazyScript());
    return outerpc->innerFunctions.append(fun);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::appendToCallSiteObj(Node callSiteObj)
{
    Node cookedNode = noSubstitutionTemplate();
    if (!cookedNode)
        return false;

    JSAtom* atom = tokenStream.getRawTemplateStringAtom();
    if (!atom)
        return false;
    Node rawNode = handler.newTemplateStringLiteral(atom, pos());
    if (!rawNode)
        return false;

    return handler.addToCallSiteObject(callSiteObj, rawNode, cookedNode);
}

template <>
ParseNode*
Parser<FullParseHandler>::standaloneLazyFunction(HandleFunction fun, unsigned staticLevel,
                                                 bool strict, GeneratorKind generatorKind)
{
    MOZ_ASSERT(checkOptionsCalled);

    Node pn = handler.newFunctionDefinition();
    if (!pn)
        return null();

    // Our tokenStream has no current token, so pn's position is garbage.
    // Substitute the position of the first token in our source.
    if (!tokenStream.peekTokenPos(&pn->pn_pos))
        return null();

    Directives directives(/* strict = */ strict);
    FunctionBox* funbox = newFunctionBox(pn, fun, /* outerpc = */ nullptr, directives,
                                         generatorKind);
    if (!funbox)
        return null();
    funbox->length = fun->nargs() - fun->hasRest();

    if (fun->lazyScript()->isDerivedClassConstructor())
        funbox->setDerivedClassConstructor();

    Directives newDirectives = directives;
    ParseContext<FullParseHandler> funpc(this, /* parent = */ nullptr, pn, funbox,
                                         &newDirectives, staticLevel, /* bodyid = */ 0,
                                         /* blockScopeDepth = */ 0);
    if (!funpc.init(tokenStream))
        return null();

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;
    FunctionSyntaxKind syntaxKind = Statement;
    if (fun->isClassConstructor())
        syntaxKind = ClassConstructor;
    else if (fun->isMethod())
        syntaxKind = Method;
    else if (fun->isGetter())
        syntaxKind = Getter;
    else if (fun->isSetter())
        syntaxKind = Setter;
    if (!functionArgsAndBodyGeneric(InAllowed, yieldHandling, pn, fun, syntaxKind)) {
        MOZ_ASSERT(directives == newDirectives);
        return null();
    }

    if (fun->isNamedLambda()) {
        if (AtomDefnPtr p = pc->lexdeps->lookup(fun->name())) {
            Definition* dn = p.value().get<FullParseHandler>();
            if (!ConvertDefinitionToNamedLambdaUse(tokenStream, pc, funbox, dn))
                return nullptr;
        }
    }

    InternalHandle<Bindings*> bindings =
        InternalHandle<Bindings*>::fromMarkedLocation(&funbox->bindings);
    if (!pc->generateFunctionBindings(context, tokenStream, alloc, bindings))
        return null();

    if (!FoldConstants(context, &pn, this))
        return null();

    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::functionArgsAndBodyGeneric(InHandling inHandling,
                                                 YieldHandling yieldHandling, Node pn,
                                                 HandleFunction fun, FunctionSyntaxKind kind)
{
    // Given a properly initialized parse context, try to parse an actual
    // function without concern for conversion to strict mode, use of lazy
    // parsing and such.

    bool hasRest;
    if (!functionArguments(yieldHandling, kind, pn, &hasRest))
        return false;

    FunctionBox* funbox = pc->sc->asFunctionBox();

    fun->setArgCount(pc->numArgs());
    if (hasRest)
        fun->setHasRest();

    if (kind == Arrow) {
        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_ARROW))
            return false;
        if (!matched) {
            report(ParseError, false, null(), JSMSG_BAD_ARROW_ARGS);
            return false;
        }
    }

    // Parse the function body.
    FunctionBodyType bodyType = StatementListBody;
    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return false;
    if (tt != TOK_LC) {
        if (funbox->isStarGenerator() || kind == Method || kind == ClassConstructor) {
            report(ParseError, false, null(), JSMSG_CURLY_BEFORE_BODY);
            return false;
        }

        if (kind != Arrow) {
#if JS_HAS_EXPR_CLOSURES
            addTelemetry(JSCompartment::DeprecatedExpressionClosure);
#else
            report(ParseError, false, null(), JSMSG_CURLY_BEFORE_BODY);
            return false;
#endif
        }

        tokenStream.ungetToken();
        bodyType = ExpressionBody;
#if JS_HAS_EXPR_CLOSURES
        fun->setIsExprBody();
#endif
    }

    Node body = functionBody(inHandling, yieldHandling, kind, bodyType);
    if (!body)
        return false;

    if ((kind != Method && kind != ClassConstructor) && fun->name() &&
        !checkStrictBinding(fun->name(), pn))
    {
        return false;
    }

    if (bodyType == StatementListBody) {
        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_RC))
            return false;
        if (!matched) {
            report(ParseError, false, null(), JSMSG_CURLY_AFTER_BODY);
            return false;
        }
        funbox->bufEnd = pos().begin + 1;
    } else {
#if !JS_HAS_EXPR_CLOSURES
        MOZ_ASSERT(kind == Arrow);
#endif
        if (tokenStream.hadError())
            return false;
        funbox->bufEnd = pos().end;
        if (kind == Statement && !MatchOrInsertSemicolon(tokenStream))
            return false;
    }

    return finishFunctionDefinition(pn, funbox, body);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkYieldNameValidity()
{
    // In star generators and in JS >= 1.7, yield is a keyword.  Otherwise in
    // strict mode, yield is a future reserved word.
    if (pc->isStarGenerator() || versionNumber() >= JSVERSION_1_7 || pc->sc->strict()) {
        report(ParseError, false, null(), JSMSG_RESERVED_ID, "yield");
        return false;
    }
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionStmt(YieldHandling yieldHandling, DefaultHandling defaultHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FUNCTION));

    RootedPropertyName name(context);
    GeneratorKind generatorKind = NotGenerator;
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    if (tt == TOK_MUL) {
        generatorKind = StarGenerator;
        if (!tokenStream.getToken(&tt))
            return null();
    }

    if (tt == TOK_NAME) {
        name = tokenStream.currentName();
    } else if (tt == TOK_YIELD) {
        if (!checkYieldNameValidity())
            return null();
        name = tokenStream.currentName();
    } else if (defaultHandling == AllowDefaultName) {
        name = context->names().starDefaultStar;
        tokenStream.ungetToken();
    } else {
        /* Unnamed function expressions are forbidden in statement context. */
        report(ParseError, false, null(), JSMSG_UNNAMED_FUNCTION_STMT);
        return null();
    }

    /* We forbid function statements in strict mode code. */
    if (!pc->atBodyLevel() && pc->sc->needStrictChecks() &&
        !report(ParseStrictError, pc->sc->strict(), null(), JSMSG_STRICT_FUNCTION_STATEMENT))
        return null();

    return functionDef(InAllowed, yieldHandling, name, Statement, generatorKind);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionExpr(InvokedPrediction invoked)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FUNCTION));

    GeneratorKind generatorKind = NotGenerator;
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    if (tt == TOK_MUL) {
        generatorKind = StarGenerator;
        if (!tokenStream.getToken(&tt))
            return null();
    }

    RootedPropertyName name(context);
    if (tt == TOK_NAME) {
        name = tokenStream.currentName();
    } else if (tt == TOK_YIELD) {
        if (!checkYieldNameValidity())
            return null();
        name = tokenStream.currentName();
    } else {
        tokenStream.ungetToken();
    }

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;
    return functionDef(InAllowed, yieldHandling, name, Expression, generatorKind, invoked);
}

/*
 * Return true if this node, known to be an unparenthesized string literal,
 * could be the string of a directive in a Directive Prologue. Directive
 * strings never contain escape sequences or line continuations.
 * isEscapeFreeStringLiteral, below, checks whether the node itself could be
 * a directive.
 */
static inline bool
IsEscapeFreeStringLiteral(const TokenPos& pos, JSAtom* str)
{
    /*
     * If the string's length in the source code is its length as a value,
     * accounting for the quotes, then it must not contain any escape
     * sequences or line continuations.
     */
    return pos.begin + str->length() + 2 == pos.end;
}

template <>
bool
Parser<SyntaxParseHandler>::asmJS(Node list)
{
    // While asm.js could technically be validated and compiled during syntax
    // parsing, we have no guarantee that some later JS wouldn't abort the
    // syntax parse and cause us to re-parse (and re-compile) the asm.js module.
    // For simplicity, unconditionally abort the syntax parse when "use asm" is
    // encountered so that asm.js is always validated/compiled exactly once
    // during a full parse.
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template <>
bool
Parser<FullParseHandler>::asmJS(Node list)
{
    // Disable syntax parsing in anything nested inside the asm.js module.
    handler.disableSyntaxParser();

    // We should be encountering the "use asm" directive for the first time; if
    // the directive is already, we must have failed asm.js validation and we're
    // reparsing. In that case, don't try to validate again. A non-null
    // newDirectives means we're not in a normal function.
    if (!pc->newDirectives || pc->newDirectives->asmJS())
        return true;

    // If there is no ScriptSource, then we are doing a non-compiling parse and
    // so we shouldn't (and can't, without a ScriptSource) compile.
    if (ss == nullptr)
        return true;

    pc->sc->asFunctionBox()->useAsm = true;

    // Attempt to validate and compile this asm.js module. On success, the
    // tokenStream has been advanced to the closing }. On failure, the
    // tokenStream is in an indeterminate state and we must reparse the
    // function from the beginning. Reparsing is triggered by marking that a
    // new directive has been encountered and returning 'false'.
    bool validated;
    if (!ValidateAsmJS(context, *this, list, &validated))
        return false;
    if (!validated) {
        pc->newDirectives->setAsmJS();
        return false;
    }

    return true;
}

/*
 * Recognize Directive Prologue members and directives. Assuming |pn| is a
 * candidate for membership in a directive prologue, recognize directives and
 * set |pc|'s flags accordingly. If |pn| is indeed part of a prologue, set its
 * |pn_prologue| flag.
 *
 * Note that the following is a strict mode function:
 *
 * function foo() {
 *   "blah" // inserted semi colon
 *        "blurgh"
 *   "use\x20loose"
 *   "use strict"
 * }
 *
 * That is, even though "use\x20loose" can never be a directive, now or in the
 * future (because of the hex escape), the Directive Prologue extends through it
 * to the "use strict" statement, which is indeed a directive.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::maybeParseDirective(Node list, Node pn, bool* cont)
{
    TokenPos directivePos;
    JSAtom* directive = handler.isStringExprStatement(pn, &directivePos);

    *cont = !!directive;
    if (!*cont)
        return true;

    if (IsEscapeFreeStringLiteral(directivePos, directive)) {
        // Mark this statement as being a possibly legitimate part of a
        // directive prologue, so the bytecode emitter won't warn about it being
        // useless code. (We mustn't just omit the statement entirely yet, as it
        // could be producing the value of an eval or JSScript execution.)
        //
        // Note that even if the string isn't one we recognize as a directive,
        // the emitter still shouldn't flag it as useless, as it could become a
        // directive in the future. We don't want to interfere with people
        // taking advantage of directive-prologue-enabled features that appear
        // in other browsers first.
        handler.setPrologue(pn);

        if (directive == context->names().useStrict) {
            // We're going to be in strict mode. Note that this scope explicitly
            // had "use strict";
            pc->sc->setExplicitUseStrict();
            if (!pc->sc->strict()) {
                if (pc->sc->isFunctionBox()) {
                    // Request that this function be reparsed as strict.
                    pc->newDirectives->setStrict();
                    return false;
                } else {
                    // We don't reparse global scopes, so we keep track of the
                    // one possible strict violation that could occur in the
                    // directive prologue -- octal escapes -- and complain now.
                    if (tokenStream.sawOctalEscape()) {
                        report(ParseError, false, null(), JSMSG_DEPRECATED_OCTAL);
                        return false;
                    }
                    pc->sc->strictScript = true;
                }
            }
        } else if (directive == context->names().useAsm) {
            if (pc->sc->isFunctionBox())
                return asmJS(list);
            return report(ParseWarning, false, pn, JSMSG_USE_ASM_DIRECTIVE_FAIL);
        }
    }
    return true;
}

/*
 * Parse the statements in a block, creating a StatementList node that lists
 * the statements.  If called from block-parsing code, the caller must match
 * '{' before and '}' after.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::statements(YieldHandling yieldHandling)
{
    JS_CHECK_RECURSION(context, return null());

    Node pn = handler.newStatementList(pc->blockid(), pos());
    if (!pn)
        return null();

    Node saveBlock = pc->blockNode;
    pc->blockNode = pn;

    bool canHaveDirectives = pc->atBodyLevel();
    bool afterReturn = false;
    bool warnedAboutStatementsAfterReturn = false;
    uint32_t statementBegin;
    for (;;) {
        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand)) {
            if (tokenStream.isEOF())
                isUnexpectedEOF_ = true;
            return null();
        }
        if (tt == TOK_EOF || tt == TOK_RC)
            break;
        if (afterReturn) {
            TokenPos pos(0, 0);
            if (!tokenStream.peekTokenPos(&pos, TokenStream::Operand))
                return null();
            statementBegin = pos.begin;
        }
        Node next = statement(yieldHandling, canHaveDirectives);
        if (!next) {
            if (tokenStream.isEOF())
                isUnexpectedEOF_ = true;
            return null();
        }
        if (!warnedAboutStatementsAfterReturn) {
            if (afterReturn) {
                if (!handler.isStatementPermittedAfterReturnStatement(next)) {
                    if (!reportWithOffset(ParseWarning, false, statementBegin,
                                          JSMSG_STMT_AFTER_RETURN))
                    {
                        return null();
                    }
                    warnedAboutStatementsAfterReturn = true;
                }
            } else if (handler.isReturnStatement(next)) {
                afterReturn = true;
            }
        }

        if (canHaveDirectives) {
            if (!maybeParseDirective(pn, next, &canHaveDirectives))
                return null();
        }

        handler.addStatementToList(pn, next, pc);
    }

    /*
     * Handle the case where there was a let declaration under this block.  If
     * it replaced pc->blockNode with a new block node then we must refresh pn
     * and then restore pc->blockNode.
     */
    if (pc->blockNode != pn)
        pn = pc->blockNode;
    pc->blockNode = saveBlock;
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::condition(InHandling inHandling, YieldHandling yieldHandling)
{
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_COND);
    Node pn = exprInParens(inHandling, yieldHandling);
    if (!pn)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_COND);

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    if (handler.isUnparenthesizedAssignment(pn)) {
        if (!report(ParseExtraWarning, false, null(), JSMSG_EQUAL_AS_ASSIGN))
            return null();
    }
    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchLabel(YieldHandling yieldHandling, MutableHandle<PropertyName*> label)
{
    TokenKind tt;
    if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
        return false;

    if (tt == TOK_NAME) {
        tokenStream.consumeKnownToken(TOK_NAME);
        MOZ_ASSERT_IF(tokenStream.currentName() == context->names().yield,
                      yieldHandling == YieldIsName);
        label.set(tokenStream.currentName());
    } else if (tt == TOK_YIELD) {
        // We might still consider |yield| to be valid here, contrary to ES6.
        // Fix bug 1104014, then stop shipping legacy generators in chrome
        // code, then remove this check!
        tokenStream.consumeKnownToken(TOK_YIELD);
        if (!checkYieldNameValidity())
            return false;
        label.set(tokenStream.currentName());
    } else {
        label.set(nullptr);
    }
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportRedeclaration(Node pn, Definition::Kind redeclKind, HandlePropertyName name)
{
    JSAutoByteString printable;
    if (!AtomToPrintableString(context, name, &printable))
        return false;

    StmtInfoPC* stmt = LexicalLookup(pc, name);
    if (stmt && stmt->type == STMT_CATCH) {
        report(ParseError, false, pn, JSMSG_REDECLARED_CATCH_IDENTIFIER, printable.ptr());
    } else {
        if (redeclKind == Definition::ARG) {
            report(ParseError, false, pn, JSMSG_REDECLARED_PARAM, printable.ptr());
        } else {
            report(ParseError, false, pn, JSMSG_REDECLARED_VAR, Definition::kindString(redeclKind),
                   printable.ptr());
        }
    }
    return false;
}

/*
 * Define a lexical binding in a block, or comprehension scope. pc must
 * already be in such a scope.
 *
 * Throw a SyntaxError if 'atom' is an invalid name. Otherwise create a
 * property for the new variable on the block object, pc->staticScope;
 * populate data->pn->pn_{op,cookie,defn,dflags}; and stash a pointer to
 * data->pn in a slot of the block object.
 */
template <>
/* static */ bool
Parser<FullParseHandler>::bindLexical(BindData<FullParseHandler>* data,
                                      HandlePropertyName name, Parser<FullParseHandler>* parser)
{
    ParseContext<FullParseHandler>* pc = parser->pc;
    ParseNode* pn = data->pn;
    if (!parser->checkStrictBinding(name, pn))
        return false;

    ExclusiveContext* cx = parser->context;
    Rooted<StaticBlockObject*> blockObj(cx, data->let.blockObj);

    unsigned index;
    if (blockObj) {
        index = blockObj->numVariables();
        if (index >= StaticBlockObject::LOCAL_INDEX_LIMIT) {
            parser->report(ParseError, false, pn, data->let.overflow);
            return false;
        }
    } else {
        // If we don't have a block object, we are parsing a body-level let,
        // in which case we use a bogus index. See comment block below in
        // setting the pn_cookie for explanation on how it gets adjusted.
        index = 0;
    }

    // For block-level lets, assign block-local index to pn->pn_cookie right
    // away, encoding it as an upvar cookie whose skip tells the current
    // static level. The emitter will adjust the node's slot based on its
    // stack depth model -- and, for global and eval code,
    // js::frontend::CompileScript will adjust the slot again to include
    // script->nfixed and body-level lets.
    //
    // For body-level lets, the index is bogus at this point and is adjusted
    // when creating Bindings. See ParseContext::generateFunctionBindings and
    // AppendPackedBindings.
    if (!pn->pn_cookie.set(parser->tokenStream, pc->staticLevel, index))
        return false;

    Definition* dn = pc->decls().lookupFirst(name);
    Definition::Kind bindingKind = data->isConst ? Definition::CONST : Definition::LET;

    /*
     * For bindings that are hoisted to the beginning of the block/function,
     * define() right now. Otherwise, delay define until PushLetScope.
     */
    if (data->let.varContext == HoistVars) {
        if (dn && dn->pn_blockid == pc->blockid())
            return parser->reportRedeclaration(pn, dn->kind(), name);
        if (!pc->define(parser->tokenStream, name, pn, bindingKind))
            return false;
    }

    if (blockObj) {
        bool redeclared;
        RootedId id(cx, NameToId(name));
        RootedShape shape(cx, StaticBlockObject::addVar(cx, blockObj, id,
                                                        data->isConst, index, &redeclared));
        if (!shape) {
            if (redeclared) {
                // The only way to be redeclared without a previous definition is if we're in a
                // comma separated list in a DontHoistVars block, so a let block of for header. In
                // that case, we must be redeclaring the same type of definition as we're trying to
                // make.
                Definition::Kind dnKind = dn ? dn->kind() : bindingKind;
                parser->reportRedeclaration(pn, dnKind, name);
            }
            return false;
        }

        /* Store pn in the static block object. */
        blockObj->setDefinitionParseNode(index, reinterpret_cast<Definition*>(pn));
    } else {
        // Body-level lets are hoisted and need to have been defined via
        // pc->define above.
        MOZ_ASSERT(data->let.varContext == HoistVars);
        MOZ_ASSERT(pc->decls().lookupFirst(name));
    }

    return true;
}

template <>
/* static */ bool
Parser<SyntaxParseHandler>::bindLexical(BindData<SyntaxParseHandler>* data,
                                        HandlePropertyName name, Parser<SyntaxParseHandler>* parser)
{
    if (!parser->checkStrictBinding(name, data->pn))
        return false;

    return true;
}

template <typename ParseHandler, class Op>
static inline bool
ForEachLetDef(TokenStream& ts, ParseContext<ParseHandler>* pc,
              HandleStaticBlockObject blockObj, Op op)
{
    for (Shape::Range<CanGC> r(ts.context(), blockObj->lastProperty()); !r.empty(); r.popFront()) {
        Shape& shape = r.front();

        /* Beware the destructuring dummy slots. */
        if (JSID_IS_INT(shape.propid()))
            continue;

        if (!op(ts, pc, blockObj, shape, JSID_TO_ATOM(shape.propid())))
            return false;
    }
    return true;
}

template <typename ParseHandler>
struct PopLetDecl {
    bool operator()(TokenStream&, ParseContext<ParseHandler>* pc, HandleStaticBlockObject,
                    const Shape&, JSAtom* atom)
    {
        pc->popLetDecl(atom);
        return true;
    }
};

// We compute the maximum block scope depth, in slots, of a compilation unit at
// parse-time.  Each nested statement has a field indicating the maximum block
// scope depth that is nested inside it.  When we leave a nested statement, we
// add the number of slots in the statement to the nested depth, and use that to
// update the maximum block scope depth of the outer statement or parse
// context.  In the end, pc->blockScopeDepth will indicate the number of slots
// to reserve in the fixed part of a stack frame.
//
template <typename ParseHandler>
static void
AccumulateBlockScopeDepth(ParseContext<ParseHandler>* pc)
{
    uint32_t innerDepth = pc->topStmt->innerBlockScopeDepth;
    StmtInfoPC* outer = pc->topStmt->down;

    if (pc->topStmt->isBlockScope)
        innerDepth += pc->topStmt->staticScope->template as<StaticBlockObject>().numVariables();

    if (outer) {
        if (outer->innerBlockScopeDepth < innerDepth)
            outer->innerBlockScopeDepth = innerDepth;
    } else {
        if (pc->blockScopeDepth < innerDepth)
            pc->blockScopeDepth = innerDepth;
    }
}

template <typename ParseHandler>
static void
PopStatementPC(TokenStream& ts, ParseContext<ParseHandler>* pc)
{
    RootedNestedScopeObject scopeObj(ts.context(), pc->topStmt->staticScope);
    MOZ_ASSERT(!!scopeObj == pc->topStmt->isNestedScope);

    AccumulateBlockScopeDepth(pc);
    FinishPopStatement(pc);

    if (scopeObj) {
        if (scopeObj->is<StaticBlockObject>()) {
            RootedStaticBlockObject blockObj(ts.context(), &scopeObj->as<StaticBlockObject>());
            MOZ_ASSERT(!blockObj->inDictionaryMode());
            ForEachLetDef(ts, pc, blockObj, PopLetDecl<ParseHandler>());
        }
        scopeObj->resetEnclosingNestedScopeFromParser();
    }
}

/*
 * The function LexicalLookup searches a static binding for the given name in
 * the stack of statements enclosing the statement currently being parsed. Each
 * statement that introduces a new scope has a corresponding scope object, on
 * which the bindings for that scope are stored. LexicalLookup either returns
 * the innermost statement which has a scope object containing a binding with
 * the given name, or nullptr.
 */
template <class ContextT>
typename ContextT::StmtInfo*
LexicalLookup(ContextT* ct, HandleAtom atom, typename ContextT::StmtInfo* stmt)
{
    RootedId id(ct->sc->context, AtomToId(atom));

    if (!stmt)
        stmt = ct->topScopeStmt;
    for (; stmt; stmt = stmt->downScope) {
        /*
         * With-statements introduce dynamic bindings. Since dynamic bindings
         * can potentially override any static bindings introduced by statements
         * further up the stack, we have to abort the search.
         */
        if (stmt->type == STMT_WITH && !ct->sc->isDotVariable(atom))
            break;

        // Skip statements that do not introduce a new scope
        if (!stmt->isBlockScope)
            continue;

        StaticBlockObject& blockObj = stmt->staticBlock();
        Shape* shape = blockObj.lookup(ct->sc->context, id);
        if (shape)
            return stmt;
    }

    return stmt;
}

template <typename ParseHandler>
static inline bool
OuterLet(ParseContext<ParseHandler>* pc, StmtInfoPC* stmt, HandleAtom atom)
{
    while (stmt->downScope) {
        stmt = LexicalLookup(pc, atom, stmt->downScope);
        if (!stmt)
            return false;
        if (stmt->type == STMT_BLOCK)
            return true;
    }
    return false;
}

template <typename ParseHandler>
/* static */ bool
Parser<ParseHandler>::bindVarOrGlobalConst(BindData<ParseHandler>* data,
                                           HandlePropertyName name, Parser<ParseHandler>* parser)
{
    ExclusiveContext* cx = parser->context;
    ParseContext<ParseHandler>* pc = parser->pc;
    Node pn = data->pn;
    bool isConstDecl = data->op == JSOP_DEFCONST;

    /* Default best op for pn is JSOP_GETNAME; we'll try to improve below. */
    parser->handler.setOp(pn, JSOP_GETNAME);

    if (!parser->checkStrictBinding(name, pn))
        return false;

    StmtInfoPC* stmt = LexicalLookup(pc, name);

    if (stmt && stmt->type == STMT_WITH) {
        parser->handler.setFlag(pn, PND_DEOPTIMIZED);
        if (pc->sc->isFunctionBox()) {
            FunctionBox* funbox = pc->sc->asFunctionBox();
            funbox->setMightAliasLocals();
        }

        /*
         * This definition isn't being added to the parse context's
         * declarations, so make sure to indicate the need to deoptimize
         * the script's arguments object. Mark the function as if it
         * contained a debugger statement, which will deoptimize arguments
         * as much as possible.
         */
        if (name == cx->names().arguments)
            pc->sc->setHasDebuggerStatement();

        return true;
    }

    DefinitionList::Range defs = pc->decls().lookupMulti(name);
    MOZ_ASSERT_IF(stmt, !defs.empty());

    if (defs.empty()) {
        return pc->define(parser->tokenStream, name, pn,
                          isConstDecl ? Definition::GLOBALCONST : Definition::VAR);
    }

    /*
     * There was a previous declaration with the same name. The standard
     * disallows several forms of redeclaration. Critically,
     *   let (x) { var x; } // error
     * is not allowed which allows us to turn any non-error redeclaration
     * into a use of the initial declaration.
     */
    DefinitionNode dn = defs.front<ParseHandler>();
    Definition::Kind dn_kind = parser->handler.getDefinitionKind(dn);
    if (dn_kind == Definition::ARG) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(cx, name, &bytes))
            return false;

        if (isConstDecl) {
            parser->report(ParseError, false, pn, JSMSG_REDECLARED_PARAM, bytes.ptr());
            return false;
        }
        if (!parser->report(ParseExtraWarning, false, pn, JSMSG_VAR_HIDES_ARG, bytes.ptr()))
            return false;
    } else {
        bool inCatchBody = (stmt && stmt->type == STMT_CATCH);
        bool error = (isConstDecl ||
                      dn_kind == Definition::CONST ||
                      dn_kind == Definition::GLOBALCONST ||
                      (dn_kind == Definition::LET &&
                       (!inCatchBody || OuterLet(pc, stmt, name))));

        if (parser->options().extraWarningsOption
            ? data->op != JSOP_DEFVAR || dn_kind != Definition::VAR
            : error)
        {
            JSAutoByteString bytes;
            if (!AtomToPrintableString(cx, name, &bytes))
                return false;

            ParseReportKind reporter = error ? ParseError : ParseExtraWarning;
            if (!(inCatchBody
                  ? parser->report(reporter, false, pn,
                                   JSMSG_REDECLARED_CATCH_IDENTIFIER, bytes.ptr())
                  : parser->report(reporter, false, pn, JSMSG_REDECLARED_VAR,
                                   Definition::kindString(dn_kind), bytes.ptr())))
            {
                return false;
            }
        }
    }

    parser->handler.linkUseToDef(pn, dn);
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::makeSetCall(Node target, unsigned msg)
{
    MOZ_ASSERT(handler.isFunctionCall(target));

    // Assignment to function calls is forbidden in ES6.  We're still somewhat
    // concerned about sites using this in dead code, so forbid it only in
    // strict mode code (or if the werror option has been set), and otherwise
    // warn.
    if (!report(ParseStrictError, pc->sc->strict(), target, msg))
        return false;

    handler.markAsSetCall(target);
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::noteNameUse(HandlePropertyName name, Node pn)
{
    /*
     * The asm.js validator does all its own symbol-table management so, as an
     * optimization, avoid doing any work here. Use-def links are only necessary
     * for emitting bytecode and successfully-validated asm.js does not emit
     * bytecode. (On validation failure, the asm.js module is reparsed.)
     */
    if (pc->useAsmOrInsideUseAsm())
        return true;

    StmtInfoPC* stmt = LexicalLookup(pc, name);

    DefinitionList::Range defs = pc->decls().lookupMulti(name);

    DefinitionNode dn;
    if (!defs.empty()) {
        dn = defs.front<ParseHandler>();
    } else {
        /*
         * No definition before this use in any lexical scope.
         * Create a placeholder definition node to either:
         * - Be adopted when we parse the real defining
         *   declaration, or
         * - Be left as a free variable definition if we never
         *   see the real definition.
         */
        dn = getOrCreateLexicalDependency(pc, name);
        if (!dn)
            return false;
    }

    handler.linkUseToDef(pn, dn);

    if (stmt) {
        if (stmt->type == STMT_WITH) {
            handler.setFlag(pn, PND_DEOPTIMIZED);
        } else if (stmt->type == STMT_SWITCH && stmt->isBlockScope) {
            // See comments above StmtInfoPC and switchStatement for how
            // firstDominatingLexicalInCase is computed.
            MOZ_ASSERT(stmt->firstDominatingLexicalInCase <= stmt->staticBlock().numVariables());
            handler.markMaybeUninitializedLexicalUseInSwitch(pn, dn,
                                                             stmt->firstDominatingLexicalInCase);
        }
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::bindInitialized(BindData<FullParseHandler>* data, ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_NAME));

    RootedPropertyName name(context, pn->pn_atom->asPropertyName());

    data->pn = pn;
    if (!data->binder(data, name, this))
        return false;

    /*
     * Select the appropriate name-setting opcode, respecting eager selection
     * done by the data->binder function.
     */
    if (data->op == JSOP_INITLEXICAL)
        pn->setOp(JSOP_INITLEXICAL);
    else if (pn->pn_dflags & PND_BOUND)
        pn->setOp(JSOP_SETLOCAL);
    else if (data->op == JSOP_DEFCONST)
        pn->setOp(JSOP_SETCONST);
    else
        pn->setOp(JSOP_SETNAME);

    if (data->op == JSOP_DEFCONST)
        pn->pn_dflags |= PND_CONST;

    pn->markAsAssigned();
    return true;
}

template <>
bool
Parser<FullParseHandler>::checkDestructuringName(BindData<FullParseHandler>* data, ParseNode* expr)
{
    MOZ_ASSERT(!handler.isUnparenthesizedDestructuringPattern(expr));

    // Parentheses are forbidden around destructuring *patterns* (but allowed
    // around names).  Use our nicer error message for parenthesized, nested
    // patterns.
    if (handler.isParenthesizedDestructuringPattern(expr)) {
        report(ParseError, false, expr, JSMSG_BAD_DESTRUCT_PARENS);
        return false;
    }

    // This expression might be in a variable-binding pattern where only plain,
    // unparenthesized names are permitted.
    if (data) {
        // Destructuring patterns in declarations must only contain
        // unparenthesized names.
        if (!handler.maybeUnparenthesizedName(expr)) {
            report(ParseError, false, expr, JSMSG_NO_VARIABLE_NAME);
            return false;
        }

        return bindInitialized(data, expr);
    }

    // Otherwise this is an expression in destructuring outside a declaration.
    if (!reportIfNotValidSimpleAssignmentTarget(expr, KeyedDestructuringAssignment))
        return false;

    MOZ_ASSERT(!handler.isFunctionCall(expr),
               "function calls shouldn't be considered valid targets in "
               "destructuring patterns");

    if (handler.maybeNameAnyParentheses(expr)) {
        // The arguments/eval identifiers are simple in non-strict mode code.
        // Warn to discourage their use nonetheless.
        if (!reportIfArgumentsEvalTarget(expr))
            return false;

        // We may be called on a name node that has already been
        // specialized, in the very weird "for (var [x] = i in o) ..."
        // case. See bug 558633.
        //
        // XXX Is this necessary with the changes in bug 1164741?  This is
        //     likely removable now.
        handler.maybeDespecializeSet(expr);

        handler.markAsAssigned(expr);
        return true;
    }

    // Nothing further to do for property accesses.
    MOZ_ASSERT(handler.isPropertyAccess(expr));
    return true;
}

template <>
bool
Parser<FullParseHandler>::checkDestructuringPattern(BindData<FullParseHandler>* data, ParseNode* pattern);

template <>
bool
Parser<FullParseHandler>::checkDestructuringObject(BindData<FullParseHandler>* data,
                                                   ParseNode* objectPattern)
{
    MOZ_ASSERT(objectPattern->isKind(PNK_OBJECT));

    for (ParseNode* member = objectPattern->pn_head; member; member = member->pn_next) {
        ParseNode* target;
        if (member->isKind(PNK_MUTATEPROTO)) {
            target = member->pn_kid;
        } else {
            MOZ_ASSERT(member->isKind(PNK_COLON) || member->isKind(PNK_SHORTHAND));
            MOZ_ASSERT_IF(member->isKind(PNK_SHORTHAND),
                          member->pn_left->isKind(PNK_OBJECT_PROPERTY_NAME) &&
                          member->pn_right->isKind(PNK_NAME) &&
                          member->pn_left->pn_atom == member->pn_right->pn_atom);

            target = member->pn_right;
        }
        if (target->isKind(PNK_ASSIGN))
            target = target->pn_left;

        if (handler.isUnparenthesizedDestructuringPattern(target)) {
            if (!checkDestructuringPattern(data, target))
                return false;
        } else {
            if (!checkDestructuringName(data, target))
                return false;
        }
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::checkDestructuringArray(BindData<FullParseHandler>* data,
                                                  ParseNode* arrayPattern)
{
    MOZ_ASSERT(arrayPattern->isKind(PNK_ARRAY));

    for (ParseNode* element = arrayPattern->pn_head; element; element = element->pn_next) {
        if (element->isKind(PNK_ELISION))
            continue;

        ParseNode* target;
        if (element->isKind(PNK_SPREAD)) {
            if (element->pn_next) {
                report(ParseError, false, element->pn_next, JSMSG_PARAMETER_AFTER_REST);
                return false;
            }
            target = element->pn_kid;

            if (handler.isUnparenthesizedDestructuringPattern(target)) {
                report(ParseError, false, target, JSMSG_BAD_DESTRUCT_TARGET);
                return false;
            }
        } else if (element->isKind(PNK_ASSIGN)) {
            target = element->pn_left;
        } else {
            target = element;
        }

        if (handler.isUnparenthesizedDestructuringPattern(target)) {
            if (!checkDestructuringPattern(data, target))
                return false;
        } else {
            if (!checkDestructuringName(data, target))
                return false;
        }
    }

    return true;
}

/*
 * Destructuring patterns can appear in two kinds of contexts:
 *
 * - assignment-like: assignment expressions and |for| loop heads.  In
 *   these cases, the patterns' property value positions can be
 *   arbitrary lvalue expressions; the destructuring is just a fancy
 *   assignment.
 *
 * - binding-like: |var| and |let| declarations, functions' formal
 *   parameter lists, |catch| clauses, and comprehension tails.  In
 *   these cases, the patterns' property value positions must be
 *   simple names; the destructuring defines them as new variables.
 *
 * In both cases, other code parses the pattern as an arbitrary
 * primaryExpr, and then, here in checkDestructuringPattern, verify
 * that the tree is a valid AssignmentPattern or BindingPattern.
 *
 * In assignment-like contexts, we parse the pattern with
 * pc->inDeclDestructuring clear, so the lvalue expressions in the
 * pattern are parsed normally.  primaryExpr links variable references
 * into the appropriate use chains; creates placeholder definitions;
 * and so on.  checkDestructuringPattern is called with |data| nullptr
 * (since we won't be binding any new names), and we specialize lvalues
 * as appropriate.
 *
 * In declaration-like contexts, the normal variable reference
 * processing would just be an obstruction, because we're going to
 * define the names that appear in the property value positions as new
 * variables anyway.  In this case, we parse the pattern with
 * pc->inDeclDestructuring set, which directs primaryExpr to leave
 * whatever name nodes it creates unconnected.  Then, here in
 * checkDestructuringPattern, we require the pattern's property value
 * positions to be simple names, and define them as appropriate to the
 * context.  For these calls, |data| points to the right sort of
 * BindData.
 */
template <>
bool
Parser<FullParseHandler>::checkDestructuringPattern(BindData<FullParseHandler>* data, ParseNode* pattern)
{
    if (pattern->isKind(PNK_ARRAYCOMP)) {
        report(ParseError, false, pattern, JSMSG_ARRAY_COMP_LEFTSIDE);
        return false;
    }

    if (pattern->isKind(PNK_ARRAY))
        return checkDestructuringArray(data, pattern);
    return checkDestructuringObject(data, pattern);
}

template <>
bool
Parser<SyntaxParseHandler>::checkDestructuringPattern(BindData<SyntaxParseHandler>* data, Node pattern)
{
    return abortIfSyntaxParser();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::destructuringExpr(YieldHandling yieldHandling, BindData<ParseHandler>* data,
                                        TokenKind tt)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(tt));

    pc->inDeclDestructuring = true;
    Node pn = primaryExpr(yieldHandling, tt);
    pc->inDeclDestructuring = false;
    if (!pn)
        return null();
    if (!checkDestructuringPattern(data, pn))
        return null();
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::destructuringExprWithoutYield(YieldHandling yieldHandling,
                                                    BindData<ParseHandler>* data, TokenKind tt,
                                                    unsigned msg)
{
    uint32_t startYieldOffset = pc->lastYieldOffset;
    Node res = destructuringExpr(yieldHandling, data, tt);
    if (res && pc->lastYieldOffset != startYieldOffset) {
        reportWithOffset(ParseError, false, pc->lastYieldOffset,
                         msg, js_yield_str);
        return null();
    }
    return res;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::pushLexicalScope(HandleStaticBlockObject blockObj, StmtInfoPC* stmt)
{
    MOZ_ASSERT(blockObj);

    ObjectBox* blockbox = newObjectBox(blockObj);
    if (!blockbox)
        return null();

    PushStatementPC(pc, stmt, STMT_BLOCK);
    blockObj->initEnclosingNestedScopeFromParser(pc->staticScope);
    FinishPushNestedScope(pc, stmt, *blockObj.get());
    stmt->isBlockScope = true;

    Node pn = handler.newLexicalScope(blockbox);
    if (!pn)
        return null();

    if (!GenerateBlockId(tokenStream, pc, stmt->blockid))
        return null();
    handler.setBlockId(pn, stmt->blockid);
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::pushLexicalScope(StmtInfoPC* stmt)
{
    RootedStaticBlockObject blockObj(context, StaticBlockObject::create(context));
    if (!blockObj)
        return null();

    return pushLexicalScope(blockObj, stmt);
}

struct AddLetDecl
{
    uint32_t blockid;

    explicit AddLetDecl(uint32_t blockid) : blockid(blockid) {}

    bool operator()(TokenStream& ts, ParseContext<FullParseHandler>* pc,
                    HandleStaticBlockObject blockObj, const Shape& shape, JSAtom*)
    {
        ParseNode* def = (ParseNode*) blockObj->getSlot(shape.slot()).toPrivate();
        def->pn_blockid = blockid;
        RootedPropertyName name(ts.context(), def->name());
        return pc->define(ts, name, def, Definition::LET);
    }
};

template <>
ParseNode*
Parser<FullParseHandler>::pushLetScope(HandleStaticBlockObject blockObj, StmtInfoPC* stmt)
{
    MOZ_ASSERT(blockObj);
    ParseNode* pn = pushLexicalScope(blockObj, stmt);
    if (!pn)
        return null();

    pn->pn_dflags |= PND_LEXICAL;

    /* Populate the new scope with decls found in the head with updated blockid. */
    if (!ForEachLetDef(tokenStream, pc, blockObj, AddLetDecl(stmt->blockid)))
        return null();

    return pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::pushLetScope(HandleStaticBlockObject blockObj, StmtInfoPC* stmt)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

/*
 * Parse a let block statement.
 * In both cases, bindings are not hoisted to the top of the enclosing block
 * and thus must be carefully injected between variables() and the let body.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::deprecatedLetBlock(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LET));

    RootedStaticBlockObject blockObj(context, StaticBlockObject::create(context));
    if (!blockObj)
        return null();

    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_LET);

    Node vars = variables(yieldHandling, PNK_LET, NotInForInit, nullptr, blockObj, DontHoistVars);
    if (!vars)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_LET);

    StmtInfoPC stmtInfo(context);
    Node block = pushLetScope(blockObj, &stmtInfo);
    if (!block)
        return null();

    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_LET);

    Node expr = statements(yieldHandling);
    if (!expr)
        return null();
    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_LET);

    addTelemetry(JSCompartment::DeprecatedLetBlock);
    if (!report(ParseWarning, pc->sc->strict(), expr, JSMSG_DEPRECATED_LET_BLOCK))
        return null();

    handler.setLexicalScopeBody(block, expr);
    PopStatementPC(tokenStream, pc);

    TokenPos letPos(begin, pos().end);

    return handler.newLetBlock(vars, block, letPos);
}

template <typename ParseHandler>
static bool
PushBlocklikeStatement(TokenStream& ts, StmtInfoPC* stmt, StmtType type,
                       ParseContext<ParseHandler>* pc)
{
    PushStatementPC(pc, stmt, type);
    return GenerateBlockId(ts, pc, stmt->blockid);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::blockStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));

    StmtInfoPC stmtInfo(context);
    if (!PushBlocklikeStatement(tokenStream, &stmtInfo, STMT_BLOCK, pc))
        return null();

    Node list = statements(yieldHandling);
    if (!list)
        return null();

    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_IN_COMPOUND);
    PopStatementPC(tokenStream, pc);
    return list;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newBindingNode(PropertyName* name, bool functionScope, VarContext varContext)
{
    /*
     * If this name is being injected into an existing block/function, see if
     * it has already been declared or if it resolves an outstanding lexdep.
     * Otherwise, this is a let block/expr that introduces a new scope and thus
     * shadows existing decls and doesn't resolve existing lexdeps. Duplicate
     * names are caught by bindLet.
     */
    if (varContext == HoistVars) {
        if (AtomDefnPtr p = pc->lexdeps->lookup(name)) {
            DefinitionNode lexdep = p.value().get<ParseHandler>();
            MOZ_ASSERT(handler.getDefinitionKind(lexdep) == Definition::PLACEHOLDER);

            Node pn = handler.getDefinitionNode(lexdep);
            if (handler.dependencyCovered(pn, pc->blockid(), functionScope)) {
                handler.setBlockId(pn, pc->blockid());
                pc->lexdeps->remove(p);
                handler.setPosition(pn, pos());
                return pn;
            }
        }
    }

    /* Make a new node for this declarator name (or destructuring pattern). */
    return newName(name);
}

/*
 * The 'blockObj' parameter is non-null when parsing the 'vars' in a let
 * expression, block statement, non-top-level let declaration in statement
 * context, and the let-initializer of a for-statement.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::variables(YieldHandling yieldHandling,
                                ParseNodeKind kind,
                                ForInitLocation location,
                                bool* psimple, StaticBlockObject* blockObj, VarContext varContext)
{
    /*
     * The four options here are:
     * - PNK_VAR:   We're parsing var declarations.
     * - PNK_CONST: We're parsing const declarations.
     * - PNK_GLOBALCONST: We're parsing const declarations at toplevel (see bug 589119).
     * - PNK_LET:   We are parsing a let declaration.
     */
    MOZ_ASSERT(kind == PNK_VAR || kind == PNK_CONST || kind == PNK_LET || kind == PNK_GLOBALCONST);

    /*
     * The simple flag is set if the declaration has the form 'var x', with
     * only one variable declared and no initializer expression.
     */
    MOZ_ASSERT_IF(psimple, *psimple);

    JSOp op = JSOP_NOP;
    if (kind == PNK_VAR)
        op = JSOP_DEFVAR;
    else if (kind == PNK_GLOBALCONST)
        op = JSOP_DEFCONST;

    Node pn = handler.newDeclarationList(kind, op);
    if (!pn)
        return null();

    /*
     * SpiderMonkey const is really "write once per initialization evaluation"
     * var, whereas let is block scoped. ES-Harmony wants block-scoped const so
     * this code will change soon.
     */
    BindData<ParseHandler> data(context);
    if (kind == PNK_VAR || kind == PNK_GLOBALCONST) {
        data.initVarOrGlobalConst(op);
    } else {
        data.initLexical(varContext, blockObj, JSMSG_TOO_MANY_LOCALS,
                         /* isConst = */ kind == PNK_CONST);
    }

    bool first = true;
    Node pn2;
    while (true) {
        do {
            if (psimple && !first)
                *psimple = false;
            first = false;

            TokenKind tt;
            if (!tokenStream.getToken(&tt))
                return null();
            if (tt == TOK_LB || tt == TOK_LC) {
                if (psimple)
                    *psimple = false;

                pc->inDeclDestructuring = true;
                pn2 = primaryExpr(yieldHandling, tt);
                pc->inDeclDestructuring = false;
                if (!pn2)
                    return null();

                bool parsingForInOrOfInit = false;
                if (location == InForInit) {
                    bool isForIn, isForOf;
                    if (!matchInOrOf(&isForIn, &isForOf))
                        return null();
                    parsingForInOrOfInit = isForIn || isForOf;
                }

                // See comment below for bindBeforeInitializer in the code that
                // handles the non-destructuring case.
                bool bindBeforeInitializer = (kind != PNK_LET && kind != PNK_CONST) ||
                                             parsingForInOrOfInit;
                if (bindBeforeInitializer && !checkDestructuringPattern(&data, pn2))
                    return null();

                if (parsingForInOrOfInit) {
                    tokenStream.ungetToken();
                    handler.addList(pn, pn2);
                    break;
                }

                MUST_MATCH_TOKEN(TOK_ASSIGN, JSMSG_BAD_DESTRUCT_DECL);

                Node init = assignExpr(location == InForInit ? InProhibited : InAllowed,
                                       yieldHandling);
                if (!init)
                    return null();

                // Ban the nonsensical |for (var V = E1 in E2);| where V is a
                // destructuring pattern.  See bug 1164741 for background.
                if (location == InForInit && kind == PNK_VAR) {
                    TokenKind afterInit;
                    if (!tokenStream.peekToken(&afterInit))
                        return null();
                    if (afterInit == TOK_IN) {
                        report(ParseError, false, init, JSMSG_INVALID_FOR_INOF_DECL_WITH_INIT,
                               "in");
                        return null();
                    }
                }

                if (!bindBeforeInitializer && !checkDestructuringPattern(&data, pn2))
                    return null();

                pn2 = handler.newBinary(PNK_ASSIGN, pn2, init);
                if (!pn2)
                    return null();

                handler.addList(pn, pn2);
                break;
            }

            if (tt != TOK_NAME) {
                if (tt == TOK_YIELD) {
                    if (!checkYieldNameValidity())
                        return null();
                } else {
                    report(ParseError, false, null(), JSMSG_NO_VARIABLE_NAME);
                    return null();
                }
            }

            RootedPropertyName name(context, tokenStream.currentName());
            pn2 = newBindingNode(name, kind == PNK_VAR || kind == PNK_GLOBALCONST, varContext);
            if (!pn2)
                return null();
            if (data.isConst)
                handler.setFlag(pn2, PND_CONST);
            data.pn = pn2;

            handler.addList(pn, pn2);

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_ASSIGN))
                return null();
            if (matched) {
                if (psimple)
                    *psimple = false;

                // In ES6, lexical bindings may not be accessed until
                // initialized. So a declaration of the form |let x = x| results
                // in a ReferenceError, as the 'x' on the RHS is accessing the let
                // binding before it is initialized.
                //
                // If we are not parsing a let declaration, bind the name
                // now. Otherwise we must wait until after parsing the initializing
                // assignment.
                bool bindBeforeInitializer = kind != PNK_LET && kind != PNK_CONST;
                if (bindBeforeInitializer && !data.binder(&data, name, this))
                    return null();

                Node init = assignExpr(location == InForInit ? InProhibited : InAllowed,
                                       yieldHandling);
                if (!init)
                    return null();

                // Ignore an initializer if we have a for-in loop declaring a
                // |var| with an initializer: |for (var v = ... in ...);|.
                // Warn that this syntax is invalid so that developers looking
                // in the console know to fix this.  ES<6 permitted the
                // initializer while ES6 doesn't; ignoring it seems the best
                // way to incrementally move to ES6 semantics.
                bool performAssignment = true;
                if (location == InForInit && kind == PNK_VAR) {
                    TokenKind afterInit;
                    if (!tokenStream.peekToken(&afterInit))
                        return null();
                    if (afterInit == TOK_IN) {
                        performAssignment = false;
                        if (!report(ParseWarning, pc->sc->strict(), init,
                                    JSMSG_INVALID_FOR_INOF_DECL_WITH_INIT, "in"))
                        {
                            return null();
                        }
                    }
                }

                if (performAssignment) {
                    if (!bindBeforeInitializer && !data.binder(&data, name, this))
                        return null();

                    if (!handler.finishInitializerAssignment(pn2, init, data.op))
                        return null();
                }
            } else {
                if (data.isConst && location == NotInForInit) {
                    report(ParseError, false, null(), JSMSG_BAD_CONST_DECL);
                    return null();
                }

                if (!data.binder(&data, name, this))
                    return null();
            }

            handler.setEndPosition(pn, pn2);
        } while (false);

        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_COMMA))
            return null();
        if (!matched)
            break;
    }

    return pn;
}

template <>
bool
Parser<FullParseHandler>::checkAndPrepareLexical(bool isConst, const TokenPos& errorPos)
{
    /*
     * This is a lexical declaration. We must be directly under a block per the
     * proposed ES4 specs, but not an implicit block created due to
     * 'for (let ...)'. If we pass this error test, make the enclosing
     * StmtInfoPC be our scope. Further let declarations in this block will
     * find this scope statement and use the same block object.
     *
     * If we are the first let declaration in this block (i.e., when the
     * enclosing maybe-scope StmtInfoPC isn't yet a scope statement) then
     * we also need to set pc->blockNode to be our PNK_LEXICALSCOPE.
     */
    StmtInfoPC* stmt = pc->topStmt;
    if (stmt && (!stmt->maybeScope() || stmt->isForLetBlock)) {
        reportWithOffset(ParseError, false, errorPos.begin, JSMSG_LEXICAL_DECL_NOT_IN_BLOCK,
                         isConst ? "const" : "lexical");
        return false;
    }

    if (stmt && stmt->isBlockScope) {
        MOZ_ASSERT(pc->staticScope == stmt->staticScope);
    } else {
        if (pc->atBodyLevel()) {
            /*
             * When bug 589199 is fixed, let variables will be stored in
             * the slots of a new scope chain object, encountered just
             * before the global object in the overall chain.  This extra
             * object is present in the scope chain for all code in that
             * global, including self-hosted code.  But self-hosted code
             * must be usable against *any* global object, including ones
             * with other let variables -- variables possibly placed in
             * conflicting slots.  Forbid top-level let declarations to
             * prevent such conflicts from ever occurring.
             */
            bool isGlobal = !pc->sc->isFunctionBox() && stmt == pc->topScopeStmt;
            if (options().selfHostingMode && isGlobal) {
                report(ParseError, false, null(), JSMSG_SELFHOSTED_TOP_LEVEL_LEXICAL,
                        isConst ? "'const'" : "'let'");
                return false;
            }
            return true;
        }

        /*
         * Some obvious assertions here, but they may help clarify the
         * situation. This stmt is not yet a scope, so it must not be a
         * catch block (catch is a lexical scope by definition).
         */
        MOZ_ASSERT(!stmt->isBlockScope);
        MOZ_ASSERT(stmt != pc->topScopeStmt);
        MOZ_ASSERT(stmt->type == STMT_BLOCK ||
                    stmt->type == STMT_SWITCH ||
                    stmt->type == STMT_TRY ||
                    stmt->type == STMT_FINALLY);
        MOZ_ASSERT(!stmt->downScope);

        /* Convert the block statement into a scope statement. */
        StaticBlockObject* blockObj = StaticBlockObject::create(context);
        if (!blockObj)
            return false;

        ObjectBox* blockbox = newObjectBox(blockObj);
        if (!blockbox)
            return false;

        /*
         * Insert stmt on the pc->topScopeStmt/stmtInfo.downScope linked
         * list stack, if it isn't already there.  If it is there, but it
         * lacks the SIF_SCOPE flag, it must be a try, catch, or finally
         * block.
         */
        stmt->isBlockScope = stmt->isNestedScope = true;
        stmt->downScope = pc->topScopeStmt;
        pc->topScopeStmt = stmt;

        blockObj->initEnclosingNestedScopeFromParser(pc->staticScope);
        pc->staticScope = blockObj;
        stmt->staticScope = blockObj;

#ifdef DEBUG
        ParseNode* tmp = pc->blockNode;
        MOZ_ASSERT(!tmp || !tmp->isKind(PNK_LEXICALSCOPE));
#endif

        /* Create a new lexical scope node for these statements. */
        ParseNode* pn1 = handler.new_<LexicalScopeNode>(blockbox, pc->blockNode);
        if (!pn1)
            return false;;
        pc->blockNode = pn1;
    }
    return true;
}

static StaticBlockObject*
CurrentLexicalStaticBlock(ParseContext<FullParseHandler>* pc)
{
    return pc->atBodyLevel() ? nullptr :
           &pc->staticScope->as<StaticBlockObject>();
}

template <>
ParseNode*
Parser<FullParseHandler>::makeInitializedLexicalBinding(HandlePropertyName name, bool isConst,
                                                        const TokenPos& pos)
{
    // Handle the silliness of global and body level lexical decls.
    BindData<FullParseHandler> data(context);
    if (pc->atGlobalLevel()) {
        data.initVarOrGlobalConst(isConst ? JSOP_DEFCONST : JSOP_DEFVAR);
    } else {
        if (!checkAndPrepareLexical(isConst, pos))
            return null();
        data.initLexical(HoistVars, CurrentLexicalStaticBlock(pc), JSMSG_TOO_MANY_LOCALS, isConst);
    }
    ParseNode* dn = newBindingNode(name, pc->atGlobalLevel());
    if (!dn)
        return null();
    handler.setPosition(dn, pos);

    if (!bindInitialized(&data, dn))
        return null();

    return dn;
}

template <>
ParseNode*
Parser<FullParseHandler>::lexicalDeclaration(YieldHandling yieldHandling, bool isConst)
{
    handler.disableSyntaxParser();

    if (!checkAndPrepareLexical(isConst, pos()))
        return null();

    /*
     * Parse body-level lets without a new block object. ES6 specs
     * that an execution environment's initial lexical environment
     * is the VariableEnvironment, i.e., body-level lets are in
     * the same environment record as vars.
     *
     * However, they cannot be parsed exactly as vars, as ES6
     * requires that uninitialized lets throw ReferenceError on use.
     *
     * See 8.1.1.1.6 and the note in 13.2.1.
     *
     * FIXME global-level lets are still considered vars until
     * other bugs are fixed.
     */
    ParseNodeKind kind = PNK_LET;
    if (pc->atGlobalLevel())
        kind = isConst ? PNK_GLOBALCONST : PNK_VAR;
    else if (isConst)
        kind = PNK_CONST;

    ParseNode* pn = variables(yieldHandling, kind, NotInForInit,
                              nullptr, CurrentLexicalStaticBlock(pc), HoistVars);
    if (!pn)
        return null();
    pn->pn_xflags = PNX_POPVAR;
    return MatchOrInsertSemicolon(tokenStream) ? pn : nullptr;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::lexicalDeclaration(YieldHandling, bool)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <>
ParseNode*
Parser<FullParseHandler>::letDeclarationOrBlock(YieldHandling yieldHandling)
{
    handler.disableSyntaxParser();

    /* Check for a let statement. */
    TokenKind tt;
    if (!tokenStream.peekToken(&tt))
        return null();
    if (tt == TOK_LP) {
        ParseNode* node = deprecatedLetBlock(yieldHandling);
        if (!node)
            return nullptr;

        MOZ_ASSERT(node->isKind(PNK_LETBLOCK));
        MOZ_ASSERT(node->isArity(PN_BINARY));
        return node;
    }

    ParseNode* decl = lexicalDeclaration(yieldHandling, /* isConst = */ false);
    if (!decl)
        return nullptr;

    // let-declarations at global scope are currently treated as plain old var.
    // See bug 589199.
    MOZ_ASSERT(decl->isKind(PNK_LET) || decl->isKind(PNK_VAR));
    MOZ_ASSERT(decl->isArity(PN_LIST));
    return decl;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::letDeclarationOrBlock(YieldHandling yieldHandling)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template<>
bool
Parser<FullParseHandler>::namedImportsOrNamespaceImport(TokenKind tt, Node importSpecSet)
{
    if (tt == TOK_LC) {
        while (true) {
            // Handle the forms |import {} from 'a'| and
            // |import { ..., } from 'a'| (where ... is non empty), by
            // escaping the loop early if the next token is }.
            if (!tokenStream.peekToken(&tt, TokenStream::KeywordIsName))
                return false;

            if (tt == TOK_RC)
                break;

            // If the next token is a keyword, the previous call to
            // peekToken matched it as a TOK_NAME, and put it in the
            // lookahead buffer, so this call will match keywords as well.
            MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_IMPORT_NAME);
            Node importName = newName(tokenStream.currentName());
            if (!importName)
                return false;

            if (!tokenStream.getToken(&tt))
                return false;

            if (tt == TOK_NAME && tokenStream.currentName() == context->names().as) {
                MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_BINDING_NAME);
            } else {
                // Keywords cannot be bound to themselves, so an import name
                // that is a keyword is a syntax error if it is not followed
                // by the keyword 'as'.
                // See the ImportSpecifier production in ES6 section 15.2.2.
                if (IsKeyword(importName->name())) {
                    JSAutoByteString bytes;
                    if (!AtomToPrintableString(context, importName->name(), &bytes))
                        return false;
                    report(ParseError, false, null(), JSMSG_AS_AFTER_RESERVED_WORD, bytes.ptr());
                    return false;
                }
                tokenStream.ungetToken();
            }
            Node bindingName = newName(tokenStream.currentName());
            if (!bindingName)
                return false;

            Node importSpec = handler.newBinary(PNK_IMPORT_SPEC, importName, bindingName);
            if (!importSpec)
                return false;

            handler.addList(importSpecSet, importSpec);

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_COMMA))
                return false;

            if (!matched)
                break;
        }

        MUST_MATCH_TOKEN(TOK_RC, JSMSG_RC_AFTER_IMPORT_SPEC_LIST);
    } else {
        MOZ_ASSERT(tt == TOK_MUL);
        if (!tokenStream.getToken(&tt))
            return false;

        if (tt != TOK_NAME || tokenStream.currentName() != context->names().as) {
            report(ParseError, false, null(), JSMSG_AS_AFTER_IMPORT_STAR);
            return false;
        }

        MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_BINDING_NAME);

        Node importName = newName(context->names().star);
        if (!importName)
            return null();

        Node bindingName = newName(tokenStream.currentName());
        if (!bindingName)
            return false;

        Node importSpec = handler.newBinary(PNK_IMPORT_SPEC, importName, bindingName);
        if (!importSpec)
            return false;

        handler.addList(importSpecSet, importSpec);
    }

    return true;
}

template<>
bool
Parser<SyntaxParseHandler>::namedImportsOrNamespaceImport(TokenKind tt, Node importSpecSet)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::importDeclaration()
{
    MOZ_ASSERT(tokenStream.currentToken().type == TOK_IMPORT);

    if (pc->sc->isFunctionBox() || !pc->atBodyLevel()) {
        report(ParseError, false, null(), JSMSG_IMPORT_DECL_AT_TOP_LEVEL);
        return null();
    }

    uint32_t begin = pos().begin;
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    Node importSpecSet = handler.newList(PNK_IMPORT_SPEC_LIST);
    if (!importSpecSet)
        return null();

    if (tt == TOK_NAME || tt == TOK_LC || tt == TOK_MUL) {
        if (tt == TOK_NAME) {
            // Handle the form |import a from 'b'|, by adding a single import
            // specifier to the list, with 'default' as the import name and
            // 'a' as the binding name. This is equivalent to
            // |import { default as a } from 'b'|.
            Node importName = newName(context->names().default_);
            if (!importName)
                return null();

            Node bindingName = newName(tokenStream.currentName());
            if (!bindingName)
                return null();

            Node importSpec = handler.newBinary(PNK_IMPORT_SPEC, importName, bindingName);
            if (!importSpec)
                return null();

            handler.addList(importSpecSet, importSpec);

            if (!tokenStream.peekToken(&tt))
                return null();

            if (tt == TOK_COMMA) {
                if (!tokenStream.getToken(&tt) || !tokenStream.getToken(&tt))
                    return null();

                if (tt != TOK_LC && tt != TOK_MUL) {
                    report(ParseError, false, null(), JSMSG_NAMED_IMPORTS_OR_NAMESPACE_IMPORT);
                    return null();
                }

                if (!namedImportsOrNamespaceImport(tt, importSpecSet))
                    return null();
            }
        } else {
            if (!namedImportsOrNamespaceImport(tt, importSpecSet))
                return null();
        }

        if (!tokenStream.getToken(&tt))
            return null();

        if (tt != TOK_NAME || tokenStream.currentName() != context->names().from) {
            report(ParseError, false, null(), JSMSG_FROM_AFTER_IMPORT_CLAUSE);
            return null();
        }

        MUST_MATCH_TOKEN(TOK_STRING, JSMSG_MODULE_SPEC_AFTER_FROM);
    } else if (tt == TOK_STRING) {
        // Handle the form |import 'a'| by leaving the list empty. This is
        // equivalent to |import {} from 'a'|.
        importSpecSet->pn_pos.end = importSpecSet->pn_pos.begin;
    } else {
        report(ParseError, false, null(), JSMSG_DECLARATION_AFTER_IMPORT);
        return null();
    }

    Node moduleSpec = stringLiteral();
    if (!moduleSpec)
        return null();

    if (!MatchOrInsertSemicolon(tokenStream))
        return null();

    return handler.newImportDeclaration(importSpecSet, moduleSpec, TokenPos(begin, pos().end));
}

template<>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::importDeclaration()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <>
ParseNode*
Parser<FullParseHandler>::classDefinition(YieldHandling yieldHandling,
                                          ClassContext classContext,
                                          DefaultHandling defaultHandling);

template<>
ParseNode*
Parser<FullParseHandler>::exportDeclaration()
{
    MOZ_ASSERT(tokenStream.currentToken().type == TOK_EXPORT);

    if (pc->sc->isFunctionBox() || !pc->atBodyLevel()) {
        report(ParseError, false, null(), JSMSG_EXPORT_DECL_AT_TOP_LEVEL);
        return null();
    }

    uint32_t begin = pos().begin;

    Node kid;
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();
    bool isExportStar = tt == TOK_MUL;
    switch (tt) {
      case TOK_LC:
      case TOK_MUL:
        kid = handler.newList(PNK_EXPORT_SPEC_LIST);
        if (!kid)
            return null();

        if (tt == TOK_LC) {
            while (true) {
                // Handle the forms |export {}| and |export { ..., }| (where ...
                // is non empty), by escaping the loop early if the next token
                // is }.
                if (!tokenStream.peekToken(&tt))
                    return null();
                if (tt == TOK_RC)
                    break;

                MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_BINDING_NAME);
                Node bindingName = newName(tokenStream.currentName());
                if (!bindingName)
                    return null();

                if (!tokenStream.getToken(&tt))
                    return null();
                if (tt == TOK_NAME && tokenStream.currentName() == context->names().as) {
                    if (!tokenStream.getToken(&tt, TokenStream::KeywordIsName))
                        return null();
                    if (tt != TOK_NAME) {
                        report(ParseError, false, null(), JSMSG_NO_EXPORT_NAME);
                        return null();
                    }
                } else {
                    tokenStream.ungetToken();
                }
                Node exportName = newName(tokenStream.currentName());
                if (!exportName)
                    return null();

                Node exportSpec = handler.newBinary(PNK_EXPORT_SPEC, bindingName, exportName);
                if (!exportSpec)
                    return null();

                handler.addList(kid, exportSpec);

                bool matched;
                if (!tokenStream.matchToken(&matched, TOK_COMMA))
                    return null();
                if (!matched)
                    break;
            }

            MUST_MATCH_TOKEN(TOK_RC, JSMSG_RC_AFTER_EXPORT_SPEC_LIST);
        } else {
            // Handle the form |export *| by adding a special export batch
            // specifier to the list.
            Node exportSpec = handler.newNullary(PNK_EXPORT_BATCH_SPEC, JSOP_NOP, pos());
            if (!exportSpec)
                return null();

            handler.addList(kid, exportSpec);
        }
        if (!tokenStream.getToken(&tt))
            return null();
        if (tt == TOK_NAME && tokenStream.currentName() == context->names().from) {
            MUST_MATCH_TOKEN(TOK_STRING, JSMSG_MODULE_SPEC_AFTER_FROM);

            Node moduleSpec = stringLiteral();
            if (!moduleSpec)
                return null();

            if (!MatchOrInsertSemicolon(tokenStream))
                return null();

            return handler.newExportFromDeclaration(begin, kid, moduleSpec);
        } else if (isExportStar) {
            report(ParseError, false, null(), JSMSG_FROM_AFTER_EXPORT_STAR);
            return null();
        } else {
            tokenStream.ungetToken();
        }

        if (!MatchOrInsertSemicolon(tokenStream))
            return null();
        break;

      case TOK_FUNCTION:
        kid = functionStmt(YieldIsKeyword, NameRequired);
        if (!kid)
            return null();
        break;

      case TOK_CLASS:
        kid = classDefinition(YieldIsKeyword, ClassStatement, NameRequired);
        if (!kid)
            return null();
        break;

      case TOK_VAR:
        kid = variables(YieldIsName, PNK_VAR, NotInForInit);
        if (!kid)
            return null();
        kid->pn_xflags = PNX_POPVAR;

        kid = MatchOrInsertSemicolon(tokenStream) ? kid : nullptr;
        if (!kid)
            return null();
        break;

      case TOK_DEFAULT: {
        if (!tokenStream.getToken(&tt))
            return null();

        switch (tt) {
          case TOK_FUNCTION:
            kid = functionStmt(YieldIsKeyword, AllowDefaultName);
            break;
          case TOK_CLASS:
            kid = classDefinition(YieldIsKeyword, ClassStatement, AllowDefaultName);
            break;
          default:
            tokenStream.ungetToken();
            kid = assignExpr(InAllowed, YieldIsKeyword);
            if (kid) {
                if (!MatchOrInsertSemicolon(tokenStream))
                    return null();
            }
            break;
        }
        if (!kid)
            return null();

        return handler.newExportDefaultDeclaration(kid, TokenPos(begin, pos().end));
      }

      case TOK_LET:
      case TOK_CONST:
        kid = lexicalDeclaration(YieldIsName, tt == TOK_CONST);
        if (!kid)
            return null();
        break;

      default:
        report(ParseError, false, null(), JSMSG_DECLARATION_AFTER_EXPORT);
        return null();
    }

    return handler.newExportDeclaration(kid, TokenPos(begin, pos().end));
}

template<>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::exportDeclaration()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::expressionStatement(YieldHandling yieldHandling, InvokedPrediction invoked)
{
    tokenStream.ungetToken();
    Node pnexpr = expr(InAllowed, yieldHandling, invoked);
    if (!pnexpr)
        return null();
    if (!MatchOrInsertSemicolon(tokenStream))
        return null();
    return handler.newExprStatement(pnexpr, pos().end);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::ifStatement(YieldHandling yieldHandling)
{
    uint32_t begin = pos().begin;

    /* An IF node has three kids: condition, then, and optional else. */
    Node cond = condition(InAllowed, yieldHandling);
    if (!cond)
        return null();

    TokenKind tt;
    if (!tokenStream.peekToken(&tt, TokenStream::Operand))
        return null();
    if (tt == TOK_SEMI) {
        if (!report(ParseExtraWarning, false, null(), JSMSG_EMPTY_CONSEQUENT))
            return null();
    }

    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_IF);
    Node thenBranch = statement(yieldHandling);
    if (!thenBranch)
        return null();

    Node elseBranch;
    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_ELSE, TokenStream::Operand))
        return null();
    if (matched) {
        stmtInfo.type = STMT_ELSE;
        elseBranch = statement(yieldHandling);
        if (!elseBranch)
            return null();
    } else {
        elseBranch = null();
    }

    PopStatementPC(tokenStream, pc);
    return handler.newIfStatement(begin, cond, thenBranch, elseBranch);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::doWhileStatement(YieldHandling yieldHandling)
{
    uint32_t begin = pos().begin;
    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_DO_LOOP);
    Node body = statement(yieldHandling);
    if (!body)
        return null();
    MUST_MATCH_TOKEN(TOK_WHILE, JSMSG_WHILE_AFTER_DO);
    Node cond = condition(InAllowed, yieldHandling);
    if (!cond)
        return null();
    PopStatementPC(tokenStream, pc);

    // The semicolon after do-while is even more optional than most
    // semicolons in JS.  Web compat required this by 2004:
    //   http://bugzilla.mozilla.org/show_bug.cgi?id=238945
    // ES3 and ES5 disagreed, but ES6 conforms to Web reality:
    //   https://bugs.ecmascript.org/show_bug.cgi?id=157
    bool ignored;
    if (!tokenStream.matchToken(&ignored, TOK_SEMI))
        return null();
    return handler.newDoWhileStatement(body, cond, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::whileStatement(YieldHandling yieldHandling)
{
    uint32_t begin = pos().begin;
    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_WHILE_LOOP);
    Node cond = condition(InAllowed, yieldHandling);
    if (!cond)
        return null();
    Node body = statement(yieldHandling);
    if (!body)
        return null();
    PopStatementPC(tokenStream, pc);
    return handler.newWhileStatement(begin, cond, body);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchInOrOf(bool* isForInp, bool* isForOfp)
{
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return false;
    *isForInp = tt == TOK_IN;
    *isForOfp = tt == TOK_NAME && tokenStream.currentToken().name() == context->names().of;
    if (!*isForInp && !*isForOfp)
        tokenStream.ungetToken();
    return true;
}

template <>
bool
Parser<FullParseHandler>::isValidForStatementLHS(ParseNode* pn1, JSVersion version,
                                                 bool isForDecl, bool isForEach,
                                                 ParseNodeKind headKind)
{
    if (isForDecl)
        return pn1->pn_count < 2 && !pn1->isKind(PNK_CONST);

    switch (pn1->getKind()) {
      case PNK_ARRAY:
      case PNK_CALL:
      case PNK_DOT:
      case PNK_SUPERPROP:
      case PNK_ELEM:
      case PNK_SUPERELEM:
      case PNK_NAME:
      case PNK_OBJECT:
        return true;

      default:
        return false;
    }
}

template <>
bool
Parser<FullParseHandler>::checkForHeadConstInitializers(ParseNode* pn1)
{
    if (!pn1->isKind(PNK_CONST))
        return true;

    for (ParseNode* assign = pn1->pn_head; assign; assign = assign->pn_next) {
        MOZ_ASSERT(assign->isKind(PNK_ASSIGN) || assign->isKind(PNK_NAME));
        if (assign->isKind(PNK_NAME) && !assign->isAssigned())
            return false;
        // PNK_ASSIGN nodes (destructuring assignment) are always assignments.
    }
    return true;
}

template <>
ParseNode*
Parser<FullParseHandler>::forStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));
    uint32_t begin = pos().begin;

    StmtInfoPC forStmt(context);
    PushStatementPC(pc, &forStmt, STMT_FOR_LOOP);

    bool isForEach = false;
    unsigned iflags = 0;

    if (allowsForEachIn()) {
        bool matched;
        if (!tokenStream.matchContextualKeyword(&matched, context->names().each))
            return null();
        if (matched) {
            iflags = JSITER_FOREACH;
            isForEach = true;
            addTelemetry(JSCompartment::DeprecatedForEach);
            if (versionNumber() < JSVERSION_LATEST) {
                if (!report(ParseWarning, pc->sc->strict(), null(), JSMSG_DEPRECATED_FOR_EACH))
                    return null();
            }
        }
    }

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    /*
     * True if we have 'for (var/let/const ...)'.
     */
    bool isForDecl = false;

    /* Non-null when isForDecl is true for a 'for (let ...)' statement. */
    RootedStaticBlockObject blockObj(context);

    /* Set to 'x' in 'for (x ;... ;...)' or 'for (x in ...)'. */
    ParseNode* pn1;

    {
        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();
        if (tt == TOK_SEMI) {
            pn1 = nullptr;
        } else {
            // Set pn1 to a variable list or an initializing expression.
            //
            // Pass |InForInit| to Parser::variables when parsing declarations
            // to trigger |for|-specific parsing for that one position.  In a
            // normal variable declaration, any initializer may be an |in|
            // expression.  But for declarations at the start of a for-loop
            // head, initializers can't contain |in|.  (Such syntax conflicts
            // with ES5's |for (var i = 0 in foo)| syntax, removed in ES6, that
            // we "support" by ignoring the |= 0|.)
            if (tt == TOK_VAR) {
                isForDecl = true;
                tokenStream.consumeKnownToken(tt);
                pn1 = variables(yieldHandling, PNK_VAR, InForInit);
            } else if (tt == TOK_LET || tt == TOK_CONST) {
                handler.disableSyntaxParser();
                bool constDecl = tt == TOK_CONST;
                tokenStream.consumeKnownToken(tt);
                isForDecl = true;
                blockObj = StaticBlockObject::create(context);
                if (!blockObj)
                    return null();
                pn1 = variables(yieldHandling, constDecl ? PNK_CONST : PNK_LET, InForInit,
                                nullptr, blockObj, DontHoistVars);
            } else {
                // Pass |InProhibited| when parsing an expression so that |in|
                // isn't parsed in a RelationalExpression as a binary operator.
                // In this context, |in| is part of a for-in loop -- *not* part
                // of a binary expression.
                pn1 = expr(InProhibited, yieldHandling);
            }
            if (!pn1)
                return null();
        }
    }

    MOZ_ASSERT_IF(isForDecl, pn1->isArity(PN_LIST));
    MOZ_ASSERT(!!blockObj == (isForDecl && pn1->isOp(JSOP_NOP)));

    // All forms of for-loop (for(;;), for-in, for-of) generate an implicit
    // block to store any lexical variables declared by the loop-head.  We
    // implement this by desugaring such loops.  These:
    //
    //   for (let/const <pattern-and-assigns>; <test>; <update>) <stmt>
    //   for (let <pattern> in <expr>) <stmt>
    //   for (let <pattern> of <expr>) <stmt>
    //
    // transform into almost these desugarings:
    //
    //   let (<pattern-and-assigns>) { for (; <test>; <update>) <stmt> }
    //   let (<pattern>) { for (<pattern> in <expr>) <stmt> }
    //   let (<pattern>) { for (<pattern> of <expr>) <stmt> }
    //
    // This desugaring is not *quite* correct.  Assignments in the head of a
    // let-block are evaluated *outside* the scope of the variables declared by
    // the let-block-head.  But ES6 mandates that they be evaluated in the same
    // scope, triggering used-before-initialization temporal dead zone errors
    // as necessary.  Bug 1069480 will fix this.
    //
    // Additionally, ES6 mandates that *each iteration* of a for-loop create a
    // fresh binding of loop variables.  For example:
    //
    //   var funcs = [];
    //   for (let i = 0; i < 2; i++)
    //     funcs.push(function() { return i; });
    //   assertEq(funcs[0](), 0);
    //   assertEq(funcs[1](), 1);
    //
    // These semantics are implemented by "freshening" the implicit block --
    // changing the scope chain to a fresh clone of the instantaneous block
    // object -- each iteration, just before evaluating the "update" in
    // for(;;) loops.  (We don't implement this freshening for for-in/of loops,
    // but soon: bug 449811.)  No freshening occurs in for (const ...;;) as
    // there's no point (you can't reassign consts), and moreover the spec
    // requires it (which fact isn't exposed in-language but can be observed
    // through the Debugger API).
    //
    // If the for-loop head includes a lexical declaration, then we create an
    // implicit block scope, and:
    //
    //   * forLetImpliedBlock is the node for the implicit block scope.
    //   * forLetDecl is the node for the decl 'let/const <pattern>'.
    //
    // Otherwise both are null.
    ParseNode* forLetImpliedBlock = nullptr;
    ParseNode* forLetDecl = nullptr;

    // If there's an |in| keyword here, it's a for-in loop, by dint of careful
    // parsing of |pn1|.
    StmtInfoPC letStmt(context); /* used if blockObj != nullptr. */
    ParseNode* pn2;      /* forHead->pn_kid2 */
    ParseNode* pn3;      /* forHead->pn_kid3 */
    ParseNodeKind headKind = PNK_FORHEAD;
    if (pn1) {
        bool isForIn, isForOf;
        if (!matchInOrOf(&isForIn, &isForOf))
            return null();
        if (isForIn)
            headKind = PNK_FORIN;
        else if (isForOf)
            headKind = PNK_FOROF;
    }

    if (headKind == PNK_FOROF || headKind == PNK_FORIN) {
        /*
         * Parse the rest of the for/in or for/of head.
         *
         * Here pn1 is everything to the left of 'in' or 'of'. At the end of
         * this block, pn1 is a decl or nullptr, pn2 is the assignment target
         * that receives the enumeration value each iteration, and pn3 is the
         * rhs of 'in'.
         */
        if (headKind == PNK_FOROF) {
            forStmt.type = (headKind == PNK_FOROF) ? STMT_FOR_OF_LOOP : STMT_FOR_IN_LOOP;
            if (isForEach) {
                report(ParseError, false, null(), JSMSG_BAD_FOR_EACH_LOOP);
                return null();
            }
        } else {
            forStmt.type = STMT_FOR_IN_LOOP;
            iflags |= JSITER_ENUMERATE;
        }

        /* Check that the left side of the 'in' or 'of' is valid. */
        if (!isValidForStatementLHS(pn1, versionNumber(), isForDecl, isForEach, headKind)) {
            report(ParseError, false, pn1, JSMSG_BAD_FOR_LEFTSIDE);
            return null();
        }

        /*
         * After the following if-else, pn2 will point to the name or
         * destructuring pattern on in's left. pn1 will point to the decl, if
         * any, else nullptr. Note that the "declaration with initializer" case
         * rewrites the loop-head, moving the decl and setting pn1 to nullptr.
         */
        if (isForDecl) {
            pn2 = pn1->pn_head;
            if ((pn2->isKind(PNK_NAME) && pn2->maybeExpr()) || pn2->isKind(PNK_ASSIGN)) {
                MOZ_ASSERT(!(headKind == PNK_FORIN && pn1->isKind(PNK_VAR)),
                           "Parser::variables should have ignored the "
                           "initializer in the ES5-sanctioned, ES6-prohibited "
                           "|for (var ... = ... in ...)| syntax");

                // Otherwise, this bizarre |for (const/let x = ... in/of ...)|
                // loop isn't valid ES6 and has never been permitted in
                // SpiderMonkey.
                report(ParseError, false, pn2, JSMSG_INVALID_FOR_INOF_DECL_WITH_INIT,
                       headKind == PNK_FOROF ? "of" : "in");
                return null();
            }
        } else {
            /* Not a declaration. */
            MOZ_ASSERT(!blockObj);
            pn2 = pn1;
            pn1 = nullptr;

            if (!checkAndMarkAsAssignmentLhs(pn2, PlainAssignment))
                return null();
        }

        pn3 = (headKind == PNK_FOROF)
              ? assignExpr(InAllowed, yieldHandling)
              : expr(InAllowed, yieldHandling);
        if (!pn3)
            return null();

        if (blockObj) {
            /*
             * Now that the pn3 has been parsed, push the let scope. To hold
             * the blockObj for the emitter, wrap the PNK_LEXICALSCOPE node
             * created by PushLetScope around the for's initializer. This also
             * serves to indicate the let-decl to the emitter.
             */
            ParseNode* block = pushLetScope(blockObj, &letStmt);
            if (!block)
                return null();
            letStmt.isForLetBlock = true;
            block->pn_expr = pn1;
            block->pn_pos = pn1->pn_pos;
            pn1 = block;
        }

        if (isForDecl) {
            /*
             * pn2 is part of a declaration. Make a copy that can be passed to
             * EmitAssignment. Take care to do this after PushLetScope.
             */
            pn2 = cloneLeftHandSide(pn2);
            if (!pn2)
                return null();
        }

        ParseNodeKind kind2 = pn2->getKind();
        MOZ_ASSERT(kind2 != PNK_ASSIGN, "forStatement TOK_ASSIGN");

        if (kind2 == PNK_NAME) {
            /* Beware 'for (arguments in ...)' with or without a 'var'. */
            pn2->markAsAssigned();
        }
    } else {
        if (isForEach) {
            reportWithOffset(ParseError, false, begin, JSMSG_BAD_FOR_EACH_LOOP);
            return null();
        }

        headKind = PNK_FORHEAD;

        if (blockObj) {
            // Ensure here that the previously-unchecked assignment mandate for
            // const declarations holds.
            if (!checkForHeadConstInitializers(pn1)) {
                report(ParseError, false, nullptr, JSMSG_BAD_CONST_DECL);
                return null();
            }

            // Desugar
            //
            //   for (let INIT; TEST; UPDATE) STMT
            //
            // into
            //
            //   let (INIT) { for (; TEST; UPDATE) STMT }
            //
            // to provide a block scope for INIT.
            forLetImpliedBlock = pushLetScope(blockObj, &letStmt);
            if (!forLetImpliedBlock)
                return null();
            letStmt.isForLetBlock = true;

            forLetDecl = pn1;

            // The above transformation isn't enough to implement |INIT|
            // scoping, because each loop iteration must see separate bindings
            // of |INIT|.  We handle this by replacing the block on the scope
            // chain with a new block, copying the old one's contents, each
            // iteration.  We supply a special PNK_FRESHENBLOCK node as the
            // |let INIT| node for |for(let INIT;;)| loop heads to distinguish
            // such nodes from *actual*, non-desugared use of the above syntax.
            // (We don't do this for PNK_CONST nodes because the spec says no
            // freshening happens -- observable with the Debugger API.)
            if (pn1->isKind(PNK_CONST)) {
                pn1 = nullptr;
            } else {
                pn1 = handler.newFreshenBlock(pn1->pn_pos);
                if (!pn1)
                    return null();
            }
        }

        /* Parse the loop condition or null into pn2. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_INIT);
        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();
        if (tt == TOK_SEMI) {
            pn2 = nullptr;
        } else {
            pn2 = expr(InAllowed, yieldHandling);
            if (!pn2)
                return null();
        }

        /* Parse the update expression or null into pn3. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_COND);
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();
        if (tt == TOK_RP) {
            pn3 = nullptr;
        } else {
            pn3 = expr(InAllowed, yieldHandling);
            if (!pn3)
                return null();
        }
    }

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

    TokenPos headPos(begin, pos().end);
    ParseNode* forHead = handler.newForHead(headKind, pn1, pn2, pn3, headPos);
    if (!forHead)
        return null();

    /* Parse the loop body. */
    ParseNode* body = statement(yieldHandling);
    if (!body)
        return null();

    if (blockObj)
        PopStatementPC(tokenStream, pc);
    PopStatementPC(tokenStream, pc);

    ParseNode* forLoop = handler.newForStatement(begin, forHead, body, iflags);
    if (!forLoop)
        return null();

    if (forLetImpliedBlock) {
        forLetImpliedBlock->pn_expr = forLoop;
        forLetImpliedBlock->pn_pos = forLoop->pn_pos;
        return handler.newLetBlock(forLetDecl, forLetImpliedBlock, forLoop->pn_pos);
    }
    return forLoop;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::forStatement(YieldHandling yieldHandling)
{
    /*
     * 'for' statement parsing is fantastically complicated and requires being
     * able to inspect the parse tree for previous parts of the 'for'. Syntax
     * parsing of 'for' statements is thus done separately, and only handles
     * the types of 'for' statements likely to be seen in web content.
     */
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    StmtInfoPC forStmt(context);
    PushStatementPC(pc, &forStmt, STMT_FOR_LOOP);

    /* Don't parse 'for each' loops. */
    if (allowsForEachIn()) {
        TokenKind tt;
        if (!tokenStream.peekToken(&tt))
            return null();
        // Not all "yield" tokens are names, but the ones that aren't names are
        // invalid in this context anyway.
        if (tt == TOK_NAME || tt == TOK_YIELD) {
            JS_ALWAYS_FALSE(abortIfSyntaxParser());
            return null();
        }
    }

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    /* True if we have 'for (var ...)'. */
    bool isForDecl = false;
    bool simpleForDecl = true;

    /* Set to 'x' in 'for (x ;... ;...)' or 'for (x in ...)'. */
    Node lhsNode;

    {
        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();
        if (tt == TOK_SEMI) {
            lhsNode = null();
        } else {
            /* Set lhsNode to a var list or an initializing expression. */
            if (tt == TOK_VAR) {
                isForDecl = true;
                tokenStream.consumeKnownToken(tt);
                lhsNode = variables(yieldHandling, PNK_VAR, InForInit, &simpleForDecl);
            }
            else if (tt == TOK_CONST || tt == TOK_LET) {
                JS_ALWAYS_FALSE(abortIfSyntaxParser());
                return null();
            }
            else {
                lhsNode = expr(InProhibited, yieldHandling);
            }
            if (!lhsNode)
                return null();
        }
    }

    // If there's an |in| keyword here, it's a for-in loop, by dint of careful
    // parsing of |pn1|.
    bool isForIn = false, isForOf = false;
    if (lhsNode) {
        if (!matchInOrOf(&isForIn, &isForOf))
            return null();
    }
    if (isForIn || isForOf) {
        /* Parse the rest of the for/in or for/of head. */
        forStmt.type = isForOf ? STMT_FOR_OF_LOOP : STMT_FOR_IN_LOOP;

        /* Check that the left side of the 'in' or 'of' is valid. */
        if (!isForDecl &&
            !handler.maybeNameAnyParentheses(lhsNode) &&
            !handler.isPropertyAccess(lhsNode))
        {
            JS_ALWAYS_FALSE(abortIfSyntaxParser());
            return null();
        }

        if (!simpleForDecl) {
            JS_ALWAYS_FALSE(abortIfSyntaxParser());
            return null();
        }

        if (!isForDecl && !checkAndMarkAsAssignmentLhs(lhsNode, PlainAssignment))
            return null();

        if (!(isForIn ? expr(InAllowed, yieldHandling) : assignExpr(InAllowed, yieldHandling)))
            return null();
    } else {
        /* Parse the loop condition or null. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_INIT);
        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();
        if (tt != TOK_SEMI) {
            if (!expr(InAllowed, yieldHandling))
                return null();
        }

        /* Parse the update expression or null. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_COND);
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();
        if (tt != TOK_RP) {
            if (!expr(InAllowed, yieldHandling))
                return null();
        }
    }

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

    /* Parse the loop body. */
    if (!statement(yieldHandling))
        return null();

    PopStatementPC(tokenStream, pc);
    return SyntaxParseHandler::NodeGeneric;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::switchStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_SWITCH));
    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_SWITCH);

    Node discriminant = exprInParens(InAllowed, yieldHandling);
    if (!discriminant)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_SWITCH);
    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_SWITCH);

    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_SWITCH);

    if (!GenerateBlockId(tokenStream, pc, pc->topStmt->blockid))
        return null();

    Node caseList = handler.newStatementList(pc->blockid(), pos());
    if (!caseList)
        return null();

    Node saveBlock = pc->blockNode;
    pc->blockNode = caseList;

    bool seenDefault = false;
    TokenKind tt;
    while (true) {
        if (!tokenStream.getToken(&tt))
            return null();
        if (tt == TOK_RC)
            break;
        uint32_t caseBegin = pos().begin;

        Node caseExpr;
        switch (tt) {
          case TOK_DEFAULT:
            if (seenDefault) {
                report(ParseError, false, null(), JSMSG_TOO_MANY_DEFAULTS);
                return null();
            }
            seenDefault = true;
            caseExpr = null();  // The default case has pn_left == nullptr.
            break;

          case TOK_CASE:
            caseExpr = expr(InAllowed, yieldHandling);
            if (!caseExpr)
                return null();
            break;

          default:
            report(ParseError, false, null(), JSMSG_BAD_SWITCH);
            return null();
        }

        MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_AFTER_CASE);

        Node body = handler.newStatementList(pc->blockid(), pos());
        if (!body)
            return null();

        bool afterReturn = false;
        bool warnedAboutStatementsAfterReturn = false;
        uint32_t statementBegin;
        while (true) {
            if (!tokenStream.peekToken(&tt, TokenStream::Operand))
                return null();
            if (tt == TOK_RC || tt == TOK_CASE || tt == TOK_DEFAULT)
                break;
            if (afterReturn) {
                TokenPos pos(0, 0);
                if (!tokenStream.peekTokenPos(&pos, TokenStream::Operand))
                    return null();
                statementBegin = pos.begin;
            }
            Node stmt = statement(yieldHandling);
            if (!stmt)
                return null();
            if (!warnedAboutStatementsAfterReturn) {
                if (afterReturn) {
                    if (!handler.isStatementPermittedAfterReturnStatement(stmt)) {
                        if (!reportWithOffset(ParseWarning, false, statementBegin,
                                              JSMSG_STMT_AFTER_RETURN))
                        {
                            return null();
                        }
                        warnedAboutStatementsAfterReturn = true;
                    }
                } else if (handler.isReturnStatement(stmt)) {
                    afterReturn = true;
                }
            }
            handler.addList(body, stmt);
        }

        // In ES6, lexical bindings canot be accessed until initialized. If
        // there was a 'let' declaration in the case we just parsed, remember
        // the slot starting at which new lexical bindings will be
        // assigned. Since lexical bindings from previous cases will not
        // dominate uses in the current case, any such uses will require a
        // dead zone check.
        //
        // Currently this is overly conservative; we could do better, but
        // declaring lexical bindings within switch cases without introducing
        // a new block is poor form and should be avoided.
        if (stmtInfo.isBlockScope)
            stmtInfo.firstDominatingLexicalInCase = stmtInfo.staticBlock().numVariables();

        Node casepn = handler.newCaseOrDefault(caseBegin, caseExpr, body);
        if (!casepn)
            return null();
        handler.addList(caseList, casepn);
    }

    /*
     * Handle the case where there was a let declaration in any case in
     * the switch body, but not within an inner block.  If it replaced
     * pc->blockNode with a new block node then we must refresh caseList and
     * then restore pc->blockNode.
     */
    if (pc->blockNode != caseList)
        caseList = pc->blockNode;
    pc->blockNode = saveBlock;

    PopStatementPC(tokenStream, pc);

    handler.setEndPosition(caseList, pos().end);

    return handler.newSwitchStatement(begin, discriminant, caseList);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::continueStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_CONTINUE));
    uint32_t begin = pos().begin;

    RootedPropertyName label(context);
    if (!matchLabel(yieldHandling, &label))
        return null();

    StmtInfoPC* stmt = pc->topStmt;
    if (label) {
        for (StmtInfoPC* stmt2 = nullptr; ; stmt = stmt->down) {
            if (!stmt) {
                report(ParseError, false, null(), JSMSG_LABEL_NOT_FOUND);
                return null();
            }
            if (stmt->type == STMT_LABEL) {
                if (stmt->label == label) {
                    if (!stmt2 || !stmt2->isLoop()) {
                        report(ParseError, false, null(), JSMSG_BAD_CONTINUE);
                        return null();
                    }
                    break;
                }
            } else {
                stmt2 = stmt;
            }
        }
    } else {
        for (; ; stmt = stmt->down) {
            if (!stmt) {
                report(ParseError, false, null(), JSMSG_BAD_CONTINUE);
                return null();
            }
            if (stmt->isLoop())
                break;
        }
    }

    if (!MatchOrInsertSemicolon(tokenStream))
        return null();

    return handler.newContinueStatement(label, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::breakStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_BREAK));
    uint32_t begin = pos().begin;

    RootedPropertyName label(context);
    if (!matchLabel(yieldHandling, &label))
        return null();
    StmtInfoPC* stmt = pc->topStmt;
    if (label) {
        for (; ; stmt = stmt->down) {
            if (!stmt) {
                report(ParseError, false, null(), JSMSG_LABEL_NOT_FOUND);
                return null();
            }
            if (stmt->type == STMT_LABEL && stmt->label == label)
                break;
        }
    } else {
        for (; ; stmt = stmt->down) {
            if (!stmt) {
                report(ParseError, false, null(), JSMSG_TOUGH_BREAK);
                return null();
            }
            if (stmt->isLoop() || stmt->type == STMT_SWITCH)
                break;
        }
    }

    if (!MatchOrInsertSemicolon(tokenStream))
        return null();

    return handler.newBreakStatement(label, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::returnStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_RETURN));
    uint32_t begin = pos().begin;

    MOZ_ASSERT(pc->sc->isFunctionBox());

    // Parse an optional operand.
    //
    // This is ugly, but we don't want to require a semicolon.
    Node exprNode;
    TokenKind tt;
    if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
        return null();
    switch (tt) {
      case TOK_EOL:
      case TOK_EOF:
      case TOK_SEMI:
      case TOK_RC:
        exprNode = null();
        pc->funHasReturnVoid = true;
        break;
      default: {
        exprNode = expr(InAllowed, yieldHandling);
        if (!exprNode)
            return null();
        pc->funHasReturnExpr = true;
      }
    }

    if (!MatchOrInsertSemicolon(tokenStream))
        return null();

    Node genrval = null();
    if (pc->isStarGenerator()) {
        genrval = newName(context->names().dotGenRVal);
        if (!genrval)
            return null();
        if (!noteNameUse(context->names().dotGenRVal, genrval))
            return null();
        if (!checkAndMarkAsAssignmentLhs(genrval, PlainAssignment))
            return null();
    }

    Node pn = handler.newReturnStatement(exprNode, genrval, TokenPos(begin, pos().end));
    if (!pn)
        return null();

    if (pc->isLegacyGenerator() && exprNode) {
        /* Disallow "return v;" in legacy generators. */
        reportBadReturn(pn, ParseError, JSMSG_BAD_GENERATOR_RETURN,
                        JSMSG_BAD_ANON_GENERATOR_RETURN);
        return null();
    }

    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newYieldExpression(uint32_t begin, typename ParseHandler::Node expr,
                                         bool isYieldStar)
{
    Node generator = newName(context->names().dotGenerator);
    if (!generator)
        return null();
    if (!noteNameUse(context->names().dotGenerator, generator))
        return null();
    if (isYieldStar)
        return handler.newYieldStarExpression(begin, expr, generator);
    return handler.newYieldExpression(begin, expr, generator);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::yieldExpression(InHandling inHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_YIELD));
    uint32_t begin = pos().begin;

    switch (pc->generatorKind()) {
      case StarGenerator:
      {
        MOZ_ASSERT(pc->sc->isFunctionBox());

        pc->lastYieldOffset = begin;

        Node exprNode;
        ParseNodeKind kind = PNK_YIELD;
        TokenKind tt;
        if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
            return null();
        switch (tt) {
          // TOK_EOL is special; it implements the [no LineTerminator here]
          // quirk in the grammar.
          case TOK_EOL:
          // The rest of these make up the complete set of tokens that can
          // appear after any of the places where AssignmentExpression is used
          // throughout the grammar.  Conveniently, none of them can also be the
          // start an expression.
          case TOK_EOF:
          case TOK_SEMI:
          case TOK_RC:
          case TOK_RB:
          case TOK_RP:
          case TOK_COLON:
          case TOK_COMMA:
            // No value.
            exprNode = null();
            break;
          case TOK_MUL:
            kind = PNK_YIELD_STAR;
            tokenStream.consumeKnownToken(TOK_MUL);
            // Fall through.
          default:
            exprNode = assignExpr(inHandling, YieldIsKeyword);
            if (!exprNode)
                return null();
        }
        return newYieldExpression(begin, exprNode, kind == PNK_YIELD_STAR);
      }

      case NotGenerator:
        // We are in code that has not seen a yield, but we are in JS 1.7 or
        // later.  Try to transition to being a legacy generator.
        MOZ_ASSERT(tokenStream.versionNumber() >= JSVERSION_1_7);
        MOZ_ASSERT(pc->lastYieldOffset == ParseContext<ParseHandler>::NoYieldOffset);

        if (!abortIfSyntaxParser())
            return null();

        if (!pc->sc->isFunctionBox()) {
            report(ParseError, false, null(), JSMSG_BAD_RETURN_OR_YIELD, js_yield_str);
            return null();
        }

        pc->sc->asFunctionBox()->setGeneratorKind(LegacyGenerator);
        addTelemetry(JSCompartment::DeprecatedLegacyGenerator);

        if (pc->funHasReturnExpr) {
            /* As in Python (see PEP-255), disallow return v; in generators. */
            reportBadReturn(null(), ParseError, JSMSG_BAD_GENERATOR_RETURN,
                            JSMSG_BAD_ANON_GENERATOR_RETURN);
            return null();
        }
        // Fall through.

      case LegacyGenerator:
      {
        // We are in a legacy generator: a function that has already seen a
        // yield, or in a legacy generator comprehension.
        MOZ_ASSERT(pc->sc->isFunctionBox());

        pc->lastYieldOffset = begin;

        // Legacy generators do not require a value.
        Node exprNode;
        TokenKind tt;
        if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
            return null();
        switch (tt) {
          case TOK_EOF:
          case TOK_EOL:
          case TOK_SEMI:
          case TOK_RC:
          case TOK_RB:
          case TOK_RP:
          case TOK_COLON:
          case TOK_COMMA:
            // No value.
            exprNode = null();
            break;
          default:
            exprNode = assignExpr(inHandling, YieldIsKeyword);
            if (!exprNode)
                return null();
        }

        return newYieldExpression(begin, exprNode);
      }
    }

    MOZ_CRASH("yieldExpr");
}

template <>
ParseNode*
Parser<FullParseHandler>::withStatement(YieldHandling yieldHandling)
{
    // test262/ch12/12.10/12.10-0-1.js fails if we try to parse with-statements
    // in syntax-parse mode. See bug 892583.
    if (handler.syntaxParser) {
        handler.disableSyntaxParser();
        abortedSyntaxParse = true;
        return null();
    }

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_WITH));
    uint32_t begin = pos().begin;

    // In most cases, we want the constructs forbidden in strict mode code to be
    // a subset of those that JSOPTION_EXTRA_WARNINGS warns about, and we should
    // use reportStrictModeError.  However, 'with' is the sole instance of a
    // construct that is forbidden in strict mode code, but doesn't even merit a
    // warning under JSOPTION_EXTRA_WARNINGS.  See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=514576#c1.
    if (pc->sc->strict() && !report(ParseStrictError, true, null(), JSMSG_STRICT_CODE_WITH))
        return null();

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_WITH);
    Node objectExpr = exprInParens(InAllowed, yieldHandling);
    if (!objectExpr)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_WITH);

    bool oldParsingWith = pc->parsingWith;
    pc->parsingWith = true;

    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_WITH);
    Rooted<StaticWithObject*> staticWith(context, StaticWithObject::create(context));
    if (!staticWith)
        return null();
    staticWith->initEnclosingNestedScopeFromParser(pc->staticScope);
    FinishPushNestedScope(pc, &stmtInfo, *staticWith);

    Node innerBlock = statement(yieldHandling);
    if (!innerBlock)
        return null();

    PopStatementPC(tokenStream, pc);

    pc->sc->setBindingsAccessedDynamically();
    pc->parsingWith = oldParsingWith;

    /*
     * Make sure to deoptimize lexical dependencies inside the |with|
     * to safely optimize binding globals (see bug 561923).
     */
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
        DefinitionNode defn = r.front().value().get<FullParseHandler>();
        DefinitionNode lexdep = handler.resolve(defn);
        if (!pc->sc->isDotVariable(lexdep->name()))
            handler.deoptimizeUsesWithin(lexdep, TokenPos(begin, pos().begin));
    }

    ObjectBox* staticWithBox = newObjectBox(staticWith);
    if (!staticWithBox)
        return null();
    return handler.newWithStatement(begin, objectExpr, innerBlock, staticWithBox);
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::withStatement(YieldHandling yieldHandling)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return null();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::labeledStatement(YieldHandling yieldHandling)
{
    uint32_t begin = pos().begin;
    RootedPropertyName label(context, tokenStream.currentName());
    for (StmtInfoPC* stmt = pc->topStmt; stmt; stmt = stmt->down) {
        if (stmt->type == STMT_LABEL && stmt->label == label) {
            report(ParseError, false, null(), JSMSG_DUPLICATE_LABEL);
            return null();
        }
    }

    tokenStream.consumeKnownToken(TOK_COLON);

    /* Push a label struct and parse the statement. */
    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_LABEL);
    stmtInfo.label = label;
    Node pn = statement(yieldHandling);
    if (!pn)
        return null();

    /* Pop the label, set pn_expr, and return early. */
    PopStatementPC(tokenStream, pc);

    return handler.newLabeledStatement(label, pn, begin);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::throwStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_THROW));
    uint32_t begin = pos().begin;

    /* ECMA-262 Edition 3 says 'throw [no LineTerminator here] Expr'. */
    TokenKind tt;
    if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
        return null();
    if (tt == TOK_EOF || tt == TOK_SEMI || tt == TOK_RC) {
        report(ParseError, false, null(), JSMSG_MISSING_EXPR_AFTER_THROW);
        return null();
    }
    if (tt == TOK_EOL) {
        report(ParseError, false, null(), JSMSG_LINE_BREAK_AFTER_THROW);
        return null();
    }

    Node throwExpr = expr(InAllowed, yieldHandling);
    if (!throwExpr)
        return null();

    if (!MatchOrInsertSemicolon(tokenStream))
        return null();

    return handler.newThrowStatement(throwExpr, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::tryStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_TRY));
    uint32_t begin = pos().begin;

    /*
     * try nodes are ternary.
     * kid1 is the try statement
     * kid2 is the catch node list or null
     * kid3 is the finally statement
     *
     * catch nodes are ternary.
     * kid1 is the lvalue (TOK_NAME, TOK_LB, or TOK_LC)
     * kid2 is the catch guard or null if no guard
     * kid3 is the catch block
     *
     * catch lvalue nodes are either:
     *   TOK_NAME for a single identifier
     *   TOK_RB or TOK_RC for a destructuring left-hand side
     *
     * finally nodes are TOK_LC statement lists.
     */

    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_TRY);
    StmtInfoPC stmtInfo(context);
    if (!PushBlocklikeStatement(tokenStream, &stmtInfo, STMT_TRY, pc))
        return null();
    Node innerBlock = statements(yieldHandling);
    if (!innerBlock)
        return null();
    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_TRY);
    PopStatementPC(tokenStream, pc);

    bool hasUnconditionalCatch = false;
    Node catchList = null();
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();
    if (tt == TOK_CATCH) {
        catchList = handler.newCatchList();
        if (!catchList)
            return null();

        do {
            Node pnblock;
            BindData<ParseHandler> data(context);

            /* Check for another catch after unconditional catch. */
            if (hasUnconditionalCatch) {
                report(ParseError, false, null(), JSMSG_CATCH_AFTER_GENERAL);
                return null();
            }

            /*
             * Create a lexical scope node around the whole catch clause,
             * including the head.
             */
            pnblock = pushLexicalScope(&stmtInfo);
            if (!pnblock)
                return null();
            stmtInfo.type = STMT_CATCH;

            /*
             * Legal catch forms are:
             *   catch (lhs)
             *   catch (lhs if <boolean_expression>)
             * where lhs is a name or a destructuring left-hand side.
             * (the latter is legal only #ifdef JS_HAS_CATCH_GUARD)
             */
            MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_CATCH);

            /*
             * Contrary to ECMA Ed. 3, the catch variable is lexically
             * scoped, not a property of a new Object instance.  This is
             * an intentional change that anticipates ECMA Ed. 4.
             */
            data.initLexical(HoistVars, &pc->staticScope->template as<StaticBlockObject>(),
                             JSMSG_TOO_MANY_CATCH_VARS);
            MOZ_ASSERT(data.let.blockObj);

            if (!tokenStream.getToken(&tt))
                return null();
            Node catchName;
            switch (tt) {
              case TOK_LB:
              case TOK_LC:
                catchName = destructuringExpr(yieldHandling, &data, tt);
                if (!catchName)
                    return null();
                break;

              case TOK_YIELD:
                if (yieldHandling == YieldIsKeyword) {
                    report(ParseError, false, null(), JSMSG_RESERVED_ID, "yield");
                    return null();
                }

                // Even if yield is *not* necessarily a keyword, we still must
                // check its validity for legacy generators.
                if (!checkYieldNameValidity())
                    return null();
                // Fall through.
              case TOK_NAME:
              {
                RootedPropertyName label(context, tokenStream.currentName());
                catchName = newBindingNode(label, false);
                if (!catchName)
                    return null();
                data.pn = catchName;
                if (!data.binder(&data, label, this))
                    return null();
                break;
              }

              default:
                report(ParseError, false, null(), JSMSG_CATCH_IDENTIFIER);
                return null();
            }

            Node catchGuard = null();
#if JS_HAS_CATCH_GUARD
            /*
             * We use 'catch (x if x === 5)' (not 'catch (x : x === 5)')
             * to avoid conflicting with the JS2/ECMAv4 type annotation
             * catchguard syntax.
             */
            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_IF))
                return null();
            if (matched) {
                catchGuard = expr(InAllowed, yieldHandling);
                if (!catchGuard)
                    return null();
            }
#endif
            MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_CATCH);

            MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_CATCH);
            Node catchBody = statements(yieldHandling);
            if (!catchBody)
                return null();
            MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_CATCH);
            PopStatementPC(tokenStream, pc);

            if (!catchGuard)
                hasUnconditionalCatch = true;

            if (!handler.addCatchBlock(catchList, pnblock, catchName, catchGuard, catchBody))
                return null();
            handler.setEndPosition(catchList, pos().end);
            handler.setEndPosition(pnblock, pos().end);

            if (!tokenStream.getToken(&tt, TokenStream::Operand))
                return null();
        } while (tt == TOK_CATCH);
    }

    Node finallyBlock = null();

    if (tt == TOK_FINALLY) {
        MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_FINALLY);
        if (!PushBlocklikeStatement(tokenStream, &stmtInfo, STMT_FINALLY, pc))
            return null();
        finallyBlock = statements(yieldHandling);
        if (!finallyBlock)
            return null();
        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_FINALLY);
        PopStatementPC(tokenStream, pc);
    } else {
        tokenStream.ungetToken();
    }
    if (!catchList && !finallyBlock) {
        report(ParseError, false, null(), JSMSG_CATCH_OR_FINALLY);
        return null();
    }

    return handler.newTryStatement(begin, innerBlock, catchList, finallyBlock);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::debuggerStatement()
{
    TokenPos p;
    p.begin = pos().begin;
    if (!MatchOrInsertSemicolon(tokenStream))
        return null();
    p.end = pos().end;

    pc->sc->setBindingsAccessedDynamically();
    pc->sc->setHasDebuggerStatement();

    return handler.newDebuggerStatement(p);
}

template <>
ParseNode*
Parser<FullParseHandler>::classDefinition(YieldHandling yieldHandling,
                                          ClassContext classContext,
                                          DefaultHandling defaultHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_CLASS));

    bool savedStrictness = setLocalStrictMode(true);

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    RootedPropertyName name(context);
    if (tt == TOK_NAME) {
        name = tokenStream.currentName();
    } else if (tt == TOK_YIELD) {
        if (!checkYieldNameValidity())
            return null();
        MOZ_ASSERT(yieldHandling != YieldIsKeyword);
        name = tokenStream.currentName();
    } else if (classContext == ClassStatement) {
        if (defaultHandling == AllowDefaultName) {
            name = context->names().starDefaultStar;
            tokenStream.ungetToken();
        } else {
            // Class statements must have a bound name
            report(ParseError, false, null(), JSMSG_UNNAMED_CLASS_STMT);
            return null();
        }
    } else {
        // Make sure to put it back, whatever it was
        tokenStream.ungetToken();
    }

    if (name == context->names().let) {
        report(ParseError, false, null(), JSMSG_LET_CLASS_BINDING);
        return null();
    }

    ParseNode* classBlock = null();
    StmtInfoPC classStmt(context);
    if (name) {
        classBlock = pushLexicalScope(&classStmt);
        if (!classBlock)
            return null();
    }

    // Because the binding definitions keep track of their blockId, we need to
    // create at least the inner binding later. Keep track of the name's position
    // in order to provide it for the nodes created later.
    TokenPos namePos = pos();

    ParseNode* classHeritage = null();
    bool hasHeritage;
    if (!tokenStream.matchToken(&hasHeritage, TOK_EXTENDS))
        return null();
    if (hasHeritage) {
        if (!tokenStream.getToken(&tt))
            return null();
        classHeritage = memberExpr(yieldHandling, tt, true);
        if (!classHeritage)
            return null();
    }

    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_CLASS);

    ParseNode* classMethods = propertyList(yieldHandling,
                                           hasHeritage ? DerivedClassBody : ClassBody);
    if (!classMethods)
        return null();

    ParseNode* nameNode = null();
    ParseNode* methodsOrBlock = classMethods;
    if (name) {
        ParseNode* innerBinding = makeInitializedLexicalBinding(name, true, namePos);
        if (!innerBinding)
            return null();

        MOZ_ASSERT(classBlock);
        handler.setLexicalScopeBody(classBlock, classMethods);
        methodsOrBlock = classBlock;

        PopStatementPC(tokenStream, pc);

        ParseNode* outerBinding = null();
        if (classContext == ClassStatement) {
            outerBinding = makeInitializedLexicalBinding(name, false, namePos);
            if (!outerBinding)
                return null();
        }

        nameNode = handler.newClassNames(outerBinding, innerBinding, namePos);
        if (!nameNode)
            return null();
    }

    MOZ_ALWAYS_TRUE(setLocalStrictMode(savedStrictness));

    return handler.newClass(nameNode, classHeritage, methodsOrBlock);
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::classDefinition(YieldHandling yieldHandling,
                                            ClassContext classContext,
                                            DefaultHandling defaultHandling)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::statement(YieldHandling yieldHandling, bool canHaveDirectives)
{
    MOZ_ASSERT(checkOptionsCalled);

    JS_CHECK_RECURSION(context, return null());

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    switch (tt) {
      // BlockStatement[?Yield, ?Return]
      case TOK_LC:
        return blockStatement(yieldHandling);

      // VariableStatement[?Yield]
      case TOK_VAR: {
        Node pn = variables(yieldHandling, PNK_VAR, NotInForInit);
        if (!pn)
            return null();

        // Tell js_EmitTree to generate a final POP.
        handler.setListFlag(pn, PNX_POPVAR);

        if (!MatchOrInsertSemicolon(tokenStream))
            return null();
        return pn;
      }

      // EmptyStatement
      case TOK_SEMI:
        return handler.newEmptyStatement(pos());

      // ExpressionStatement[?Yield].
      //
      // These should probably be handled by a single ExpressionStatement
      // function in a default, not split up this way.
      case TOK_STRING:
        if (!canHaveDirectives && tokenStream.currentToken().atom() == context->names().useAsm) {
            if (!abortIfSyntaxParser())
                return null();
            if (!report(ParseWarning, false, null(), JSMSG_USE_ASM_DIRECTIVE_FAIL))
                return null();
        }
        return expressionStatement(yieldHandling);

      case TOK_YIELD: {
        TokenKind next;
        TokenStream::Modifier modifier = yieldExpressionsSupported()
                                         ? TokenStream::Operand
                                         : TokenStream::None;
        if (!tokenStream.peekToken(&next, modifier))
            return null();
        if (next == TOK_COLON) {
            if (!checkYieldNameValidity())
                return null();
            return labeledStatement(yieldHandling);
        }
        return expressionStatement(yieldHandling);
      }

      case TOK_NAME: {
        TokenKind next;
        if (!tokenStream.peekToken(&next))
            return null();
        if (next == TOK_COLON)
            return labeledStatement(yieldHandling);
        return expressionStatement(yieldHandling);
      }

      case TOK_NEW:
        return expressionStatement(yieldHandling, PredictInvoked);

      default:
        return expressionStatement(yieldHandling);

      // IfStatement[?Yield, ?Return]
      case TOK_IF:
        return ifStatement(yieldHandling);

      // BreakableStatement[?Yield, ?Return]
      //
      // BreakableStatement[Yield, Return]:
      //   IterationStatement[?Yield, ?Return]
      //   SwitchStatement[?Yield, ?Return]
      case TOK_DO:
        return doWhileStatement(yieldHandling);

      case TOK_WHILE:
        return whileStatement(yieldHandling);

      case TOK_FOR:
        return forStatement(yieldHandling);

      case TOK_SWITCH:
        return switchStatement(yieldHandling);

      // ContinueStatement[?Yield]
      case TOK_CONTINUE:
        return continueStatement(yieldHandling);

      // BreakStatement[?Yield]
      case TOK_BREAK:
        return breakStatement(yieldHandling);

      // [+Return] ReturnStatement[?Yield]
      case TOK_RETURN:
        // The Return parameter is only used here, and the effect is easily
        // detected this way, so don't bother passing around an extra parameter
        // everywhere.
        if (!pc->sc->isFunctionBox()) {
            report(ParseError, false, null(), JSMSG_BAD_RETURN_OR_YIELD, js_return_str);
            return null();
        }
        return returnStatement(yieldHandling);

      // WithStatement[?Yield, ?Return]
      case TOK_WITH:
        return withStatement(yieldHandling);

      // LabelledStatement[?Yield, ?Return]
      // This is really handled by TOK_NAME and TOK_YIELD cases above.

      // ThrowStatement[?Yield]
      case TOK_THROW:
        return throwStatement(yieldHandling);

      // TryStatement[?Yield, ?Return]
      case TOK_TRY:
        return tryStatement(yieldHandling);

      // DebuggerStatement
      case TOK_DEBUGGER:
        return debuggerStatement();

      // HoistableDeclaration[?Yield]
      case TOK_FUNCTION:
        return functionStmt(yieldHandling, NameRequired);

      // ClassDeclaration[?Yield]
      case TOK_CLASS:
        if (!abortIfSyntaxParser())
            return null();
        return classDefinition(yieldHandling, ClassStatement, NameRequired);

      // LexicalDeclaration[In, ?Yield]
      case TOK_LET:
        // [In] is the default behavior, because for-loops currently specially
        // parse their heads to handle |in| in this situation.
        return letDeclarationOrBlock(yieldHandling);
      case TOK_CONST:
        if (!abortIfSyntaxParser())
            return null();
        return lexicalDeclaration(yieldHandling, /* isConst = */ true);

      // ImportDeclaration (only inside modules)
      case TOK_IMPORT:
        return importDeclaration();

      // ExportDeclaration (only inside modules)
      case TOK_EXPORT:
        return exportDeclaration();

      // Miscellaneous error cases arguably better caught here than elsewhere.

      case TOK_CATCH:
        report(ParseError, false, null(), JSMSG_CATCH_WITHOUT_TRY);
        return null();

      case TOK_FINALLY:
        report(ParseError, false, null(), JSMSG_FINALLY_WITHOUT_TRY);
        return null();

      // NOTE: default case handled in the ExpressionStatement section.
    }
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::expr(InHandling inHandling, YieldHandling yieldHandling,
                           InvokedPrediction invoked)
{
    Node pn = assignExpr(inHandling, yieldHandling, invoked);
    if (!pn)
        return null();

    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_COMMA))
        return null();
    if (matched) {
        Node seq = handler.newCommaExpressionList(pn);
        if (!seq)
            return null();
        while (true) {
            if (handler.isUnparenthesizedYieldExpression(pn)) {
                report(ParseError, false, pn, JSMSG_BAD_GENERATOR_SYNTAX, js_yield_str);
                return null();
            }

            pn = assignExpr(inHandling, yieldHandling);
            if (!pn)
                return null();
            handler.addList(seq, pn);

            if (!tokenStream.matchToken(&matched, TOK_COMMA))
                return null();
            if (!matched)
                break;
        }
        return seq;
    }
    return pn;
}

static const JSOp ParseNodeKindToJSOp[] = {
    JSOP_OR,
    JSOP_AND,
    JSOP_BITOR,
    JSOP_BITXOR,
    JSOP_BITAND,
    JSOP_STRICTEQ,
    JSOP_EQ,
    JSOP_STRICTNE,
    JSOP_NE,
    JSOP_LT,
    JSOP_LE,
    JSOP_GT,
    JSOP_GE,
    JSOP_INSTANCEOF,
    JSOP_IN,
    JSOP_LSH,
    JSOP_RSH,
    JSOP_URSH,
    JSOP_ADD,
    JSOP_SUB,
    JSOP_MUL,
    JSOP_DIV,
    JSOP_MOD
};

static inline JSOp
BinaryOpParseNodeKindToJSOp(ParseNodeKind pnk)
{
    MOZ_ASSERT(pnk >= PNK_BINOP_FIRST);
    MOZ_ASSERT(pnk <= PNK_BINOP_LAST);
    return ParseNodeKindToJSOp[pnk - PNK_BINOP_FIRST];
}

static ParseNodeKind
BinaryOpTokenKindToParseNodeKind(TokenKind tok)
{
    MOZ_ASSERT(TokenKindIsBinaryOp(tok));
    return ParseNodeKind(PNK_BINOP_FIRST + (tok - TOK_BINOP_FIRST));
}

static const int PrecedenceTable[] = {
    1, /* PNK_OR */
    2, /* PNK_AND */
    3, /* PNK_BITOR */
    4, /* PNK_BITXOR */
    5, /* PNK_BITAND */
    6, /* PNK_STRICTEQ */
    6, /* PNK_EQ */
    6, /* PNK_STRICTNE */
    6, /* PNK_NE */
    7, /* PNK_LT */
    7, /* PNK_LE */
    7, /* PNK_GT */
    7, /* PNK_GE */
    7, /* PNK_INSTANCEOF */
    7, /* PNK_IN */
    8, /* PNK_LSH */
    8, /* PNK_RSH */
    8, /* PNK_URSH */
    9, /* PNK_ADD */
    9, /* PNK_SUB */
    10, /* PNK_STAR */
    10, /* PNK_DIV */
    10  /* PNK_MOD */
};

static const int PRECEDENCE_CLASSES = 10;

static int
Precedence(ParseNodeKind pnk) {
    // Everything binds tighter than PNK_LIMIT, because we want to reduce all
    // nodes to a single node when we reach a token that is not another binary
    // operator.
    if (pnk == PNK_LIMIT)
        return 0;

    MOZ_ASSERT(pnk >= PNK_BINOP_FIRST);
    MOZ_ASSERT(pnk <= PNK_BINOP_LAST);
    return PrecedenceTable[pnk - PNK_BINOP_FIRST];
}

template <typename ParseHandler>
MOZ_ALWAYS_INLINE typename ParseHandler::Node
Parser<ParseHandler>::orExpr1(InHandling inHandling, YieldHandling yieldHandling,
                              InvokedPrediction invoked)
{
    // Shift-reduce parser for the left-associative binary operator part of
    // the JS syntax.

    // Conceptually there's just one stack, a stack of pairs (lhs, op).
    // It's implemented using two separate arrays, though.
    Node nodeStack[PRECEDENCE_CLASSES];
    ParseNodeKind kindStack[PRECEDENCE_CLASSES];
    int depth = 0;

    Node pn;
    for (;;) {
        pn = unaryExpr(yieldHandling, invoked);
        if (!pn)
            return pn;

        // If a binary operator follows, consume it and compute the
        // corresponding operator.
        TokenKind tok;
        if (!tokenStream.getToken(&tok))
            return null();

        ParseNodeKind pnk;
        if (tok == TOK_IN ? inHandling == InAllowed : TokenKindIsBinaryOp(tok)) {
            pnk = BinaryOpTokenKindToParseNodeKind(tok);
        } else {
            tok = TOK_EOF;
            pnk = PNK_LIMIT;
        }

        // If pnk has precedence less than or equal to another operator on the
        // stack, reduce. This combines nodes on the stack until we form the
        // actual lhs of pnk.
        //
        // The >= in this condition works because all the operators in question
        // are left-associative; if any were not, the case where two operators
        // have equal precedence would need to be handled specially, and the
        // stack would need to be a Vector.
        while (depth > 0 && Precedence(kindStack[depth - 1]) >= Precedence(pnk)) {
            depth--;
            ParseNodeKind combiningPnk = kindStack[depth];
            JSOp combiningOp = BinaryOpParseNodeKindToJSOp(combiningPnk);
            pn = handler.appendOrCreateList(combiningPnk, nodeStack[depth], pn, pc, combiningOp);
            if (!pn)
                return pn;
        }

        if (pnk == PNK_LIMIT)
            break;

        nodeStack[depth] = pn;
        kindStack[depth] = pnk;
        depth++;
        MOZ_ASSERT(depth <= PRECEDENCE_CLASSES);
    }

    MOZ_ASSERT(depth == 0);
    return pn;
}

template <typename ParseHandler>
MOZ_ALWAYS_INLINE typename ParseHandler::Node
Parser<ParseHandler>::condExpr1(InHandling inHandling, YieldHandling yieldHandling,
                                InvokedPrediction invoked)
{
    Node condition = orExpr1(inHandling, yieldHandling, invoked);
    if (!condition || !tokenStream.isCurrentTokenType(TOK_HOOK))
        return condition;

    Node thenExpr = assignExpr(InAllowed, yieldHandling);
    if (!thenExpr)
        return null();

    MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_IN_COND);

    Node elseExpr = assignExpr(inHandling, yieldHandling);
    if (!elseExpr)
        return null();

    // Advance to the next token; the caller is responsible for interpreting it.
    TokenKind ignored;
    if (!tokenStream.getToken(&ignored))
        return null();
    return handler.newConditional(condition, thenExpr, elseExpr);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkAndMarkAsAssignmentLhs(Node target, AssignmentFlavor flavor)
{
    MOZ_ASSERT(flavor != KeyedDestructuringAssignment,
               "destructuring must use special checking/marking code, not "
               "this method");

    if (handler.isUnparenthesizedDestructuringPattern(target)) {
        if (flavor == CompoundAssignment) {
            report(ParseError, false, null(), JSMSG_BAD_DESTRUCT_ASS);
            return false;
        }

        return checkDestructuringPattern(nullptr, target);
    }

    // All other permitted targets are simple.
    if (!reportIfNotValidSimpleAssignmentTarget(target, flavor))
        return false;

    if (handler.isPropertyAccess(target))
        return true;

    if (handler.maybeNameAnyParentheses(target)) {
        // The arguments/eval identifiers are simple in non-strict mode code,
        // but warn to discourage use nonetheless.
        if (!reportIfArgumentsEvalTarget(target))
            return false;

        handler.adjustGetToSet(target);
        handler.markAsAssigned(target);
        return true;
    }

    MOZ_ASSERT(handler.isFunctionCall(target));
    return makeSetCall(target, JSMSG_BAD_LEFTSIDE_OF_ASS);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::assignExpr(InHandling inHandling, YieldHandling yieldHandling,
                                 InvokedPrediction invoked)
{
    JS_CHECK_RECURSION(context, return null());

    // It's very common at this point to have a "detectably simple" expression,
    // i.e. a name/number/string token followed by one of the following tokens
    // that obviously isn't part of an expression: , ; : ) ] }
    //
    // (In Parsemark this happens 81.4% of the time;  in code with large
    // numeric arrays, such as some Kraken benchmarks, it happens more often.)
    //
    // In such cases, we can avoid the full expression parsing route through
    // assignExpr(), condExpr1(), orExpr1(), unaryExpr(), memberExpr(), and
    // primaryExpr().

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    bool endsExpr;

    if (tt == TOK_NAME) {
        if (!tokenStream.nextTokenEndsExpr(&endsExpr))
            return null();
        if (endsExpr)
            return identifierName(yieldHandling);
    }

    if (tt == TOK_NUMBER) {
        if (!tokenStream.nextTokenEndsExpr(&endsExpr))
            return null();
        if (endsExpr)
            return newNumber(tokenStream.currentToken());
    }

    if (tt == TOK_STRING) {
        if (!tokenStream.nextTokenEndsExpr(&endsExpr))
            return null();
        if (endsExpr)
            return stringLiteral();
    }

    if (tt == TOK_YIELD && yieldExpressionsSupported())
        return yieldExpression(inHandling);

    tokenStream.ungetToken();

    // Save the tokenizer state in case we find an arrow function and have to
    // rewind.
    TokenStream::Position start(keepAtoms);
    tokenStream.tell(&start);

    Node lhs = condExpr1(inHandling, yieldHandling, invoked);
    if (!lhs)
        return null();

    ParseNodeKind kind;
    JSOp op;
    switch (tokenStream.currentToken().type) {
      case TOK_ASSIGN:       kind = PNK_ASSIGN;       op = JSOP_NOP;    break;
      case TOK_ADDASSIGN:    kind = PNK_ADDASSIGN;    op = JSOP_ADD;    break;
      case TOK_SUBASSIGN:    kind = PNK_SUBASSIGN;    op = JSOP_SUB;    break;
      case TOK_BITORASSIGN:  kind = PNK_BITORASSIGN;  op = JSOP_BITOR;  break;
      case TOK_BITXORASSIGN: kind = PNK_BITXORASSIGN; op = JSOP_BITXOR; break;
      case TOK_BITANDASSIGN: kind = PNK_BITANDASSIGN; op = JSOP_BITAND; break;
      case TOK_LSHASSIGN:    kind = PNK_LSHASSIGN;    op = JSOP_LSH;    break;
      case TOK_RSHASSIGN:    kind = PNK_RSHASSIGN;    op = JSOP_RSH;    break;
      case TOK_URSHASSIGN:   kind = PNK_URSHASSIGN;   op = JSOP_URSH;   break;
      case TOK_MULASSIGN:    kind = PNK_MULASSIGN;    op = JSOP_MUL;    break;
      case TOK_DIVASSIGN:    kind = PNK_DIVASSIGN;    op = JSOP_DIV;    break;
      case TOK_MODASSIGN:    kind = PNK_MODASSIGN;    op = JSOP_MOD;    break;

      case TOK_ARROW: {
        // A line terminator between ArrowParameters and the => should trigger a SyntaxError.
        tokenStream.ungetToken();
        TokenKind next;
        if (!tokenStream.peekTokenSameLine(&next) || next != TOK_ARROW) {
            report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                   "expression", TokenKindToDesc(TOK_ARROW));
            return null();
        }

        tokenStream.seek(start);
        if (!abortIfSyntaxParser())
            return null();

        TokenKind ignored;
        if (!tokenStream.peekToken(&ignored))
            return null();

        if (pc->sc->isFunctionBox() && pc->sc->asFunctionBox()->isDerivedClassConstructor()) {
            report(ParseError, false, null(), JSMSG_DISABLED_DERIVED_CLASS, "arrow functions");
            return null();
        }

        return functionDef(inHandling, yieldHandling, nullptr, Arrow, NotGenerator);
      }

      default:
        MOZ_ASSERT(!tokenStream.isCurrentTokenAssignment());
        tokenStream.ungetToken();
        return lhs;
    }

    AssignmentFlavor flavor = kind == PNK_ASSIGN ? PlainAssignment : CompoundAssignment;
    if (!checkAndMarkAsAssignmentLhs(lhs, flavor))
        return null();

    bool saved = pc->inDeclDestructuring;
    pc->inDeclDestructuring = false;
    Node rhs = assignExpr(inHandling, yieldHandling);
    pc->inDeclDestructuring = saved;
    if (!rhs)
        return null();

    return handler.newAssignment(kind, lhs, rhs, pc, op);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::isValidSimpleAssignmentTarget(Node node,
                                                    FunctionCallBehavior behavior /* = ForbidAssignmentToFunctionCalls */)
{
    // Note that this method implements *only* a boolean test.  Reporting an
    // error for the various syntaxes that fail this, and warning for the
    // various syntaxes that "pass" this but should not, occurs elsewhere.

    if (PropertyName* name = handler.maybeNameAnyParentheses(node)) {
        if (!pc->sc->strict())
            return true;

        return name != context->names().arguments && name != context->names().eval;
    }

    if (handler.isPropertyAccess(node))
        return true;

    if (behavior == PermitAssignmentToFunctionCalls) {
        if (handler.isFunctionCall(node))
            return true;
    }

    return false;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportIfArgumentsEvalTarget(Node nameNode)
{
    PropertyName* name = handler.maybeNameAnyParentheses(nameNode);
    MOZ_ASSERT(name, "must only call this function on known names");

    const char* chars = (name == context->names().arguments)
                        ? js_arguments_str
                        : (name == context->names().eval)
                        ? js_eval_str
                        : nullptr;
    if (!chars)
        return true;

    if (!report(ParseStrictError, pc->sc->strict(), nameNode, JSMSG_BAD_STRICT_ASSIGN, chars))
        return false;

    MOZ_ASSERT(!pc->sc->strict(), "in strict mode an error should have been reported");
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportIfNotValidSimpleAssignmentTarget(Node target, AssignmentFlavor flavor)
{
    FunctionCallBehavior behavior = flavor == KeyedDestructuringAssignment
                                    ? ForbidAssignmentToFunctionCalls
                                    : PermitAssignmentToFunctionCalls;
    if (isValidSimpleAssignmentTarget(target, behavior))
        return true;

    if (handler.maybeNameAnyParentheses(target)) {
        // Use a special error if the target is arguments/eval.  This ensures
        // targeting these names is consistently a SyntaxError (which error numbers
        // below don't guarantee) while giving us a nicer error message.
        if (!reportIfArgumentsEvalTarget(target))
            return false;
    }

    unsigned errnum;
    const char* extra = nullptr;

    switch (flavor) {
      case IncrementAssignment:
        errnum = JSMSG_BAD_OPERAND;
        extra = "increment";
        break;

      case DecrementAssignment:
        errnum = JSMSG_BAD_OPERAND;
        extra = "decrement";
        break;

      case KeyedDestructuringAssignment:
        errnum = JSMSG_BAD_DESTRUCT_TARGET;
        break;

      case PlainAssignment:
      case CompoundAssignment:
        errnum = JSMSG_BAD_LEFTSIDE_OF_ASS;
        break;
    }

    report(ParseError, pc->sc->strict(), target, errnum, extra);
    return false;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkAndMarkAsIncOperand(Node target, AssignmentFlavor flavor)
{
    MOZ_ASSERT(flavor == IncrementAssignment || flavor == DecrementAssignment);

    // Check.
    if (!reportIfNotValidSimpleAssignmentTarget(target, flavor))
        return false;

    // Mark.
    if (handler.maybeNameAnyParentheses(target)) {
        // Assignment to arguments/eval is allowed outside strict mode code,
        // but it's dodgy.  Report a strict warning (error, if werror was set).
        if (!reportIfArgumentsEvalTarget(target))
            return false;

        handler.markAsAssigned(target);
    } else if (handler.isFunctionCall(target)) {
        if (!makeSetCall(target, JSMSG_BAD_INCOP_OPERAND))
            return false;
    }
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::unaryOpExpr(YieldHandling yieldHandling, ParseNodeKind kind, JSOp op,
                                  uint32_t begin)
{
    Node kid = unaryExpr(yieldHandling);
    if (!kid)
        return null();
    return handler.newUnary(kind, op, begin, kid);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::unaryExpr(YieldHandling yieldHandling, InvokedPrediction invoked)
{
    JS_CHECK_RECURSION(context, return null());

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();
    uint32_t begin = pos().begin;
    switch (tt) {
      case TOK_VOID:
        return unaryOpExpr(yieldHandling, PNK_VOID, JSOP_VOID, begin);
      case TOK_NOT:
        return unaryOpExpr(yieldHandling, PNK_NOT, JSOP_NOT, begin);
      case TOK_BITNOT:
        return unaryOpExpr(yieldHandling, PNK_BITNOT, JSOP_BITNOT, begin);
      case TOK_ADD:
        return unaryOpExpr(yieldHandling, PNK_POS, JSOP_POS, begin);
      case TOK_SUB:
        return unaryOpExpr(yieldHandling, PNK_NEG, JSOP_NEG, begin);

      case TOK_TYPEOF: {
        // The |typeof| operator is specially parsed to distinguish its
        // application to a name, from its application to a non-name
        // expression:
        //
        //   // Looks up the name, doesn't find it and so evaluates to
        //   // "undefined".
        //   assertEq(typeof nonExistentName, "undefined");
        //
        //   // Evaluates expression, triggering a runtime ReferenceError for
        //   // the undefined name.
        //   typeof (1, nonExistentName);
        Node kid = unaryExpr(yieldHandling);
        if (!kid)
            return null();

        return handler.newTypeof(begin, kid);
      }

      case TOK_INC:
      case TOK_DEC:
      {
        TokenKind tt2;
        if (!tokenStream.getToken(&tt2, TokenStream::Operand))
            return null();
        Node pn2 = memberExpr(yieldHandling, tt2, true);
        if (!pn2)
            return null();
        AssignmentFlavor flavor = (tt == TOK_INC) ? IncrementAssignment : DecrementAssignment;
        if (!checkAndMarkAsIncOperand(pn2, flavor))
            return null();
        return handler.newUnary((tt == TOK_INC) ? PNK_PREINCREMENT : PNK_PREDECREMENT,
                                JSOP_NOP,
                                begin,
                                pn2);
      }

      case TOK_DELETE: {
        Node expr = unaryExpr(yieldHandling);
        if (!expr)
            return null();

        // Per spec, deleting any unary expression is valid -- it simply
        // returns true -- except for one case that is illegal in strict mode.
        if (handler.maybeNameAnyParentheses(expr)) {
            if (!report(ParseStrictError, pc->sc->strict(), expr, JSMSG_DEPRECATED_DELETE_OPERAND))
                return null();
            pc->sc->setBindingsAccessedDynamically();
        }

        return handler.newDelete(begin, expr);
      }

      default: {
        Node pn = memberExpr(yieldHandling, tt, /* allowCallSyntax = */ true, invoked);
        if (!pn)
            return null();

        /* Don't look across a newline boundary for a postfix incop. */
        if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
            return null();
        if (tt == TOK_INC || tt == TOK_DEC) {
            tokenStream.consumeKnownToken(tt);
            AssignmentFlavor flavor = (tt == TOK_INC) ? IncrementAssignment : DecrementAssignment;
            if (!checkAndMarkAsIncOperand(pn, flavor))
                return null();
            return handler.newUnary((tt == TOK_INC) ? PNK_POSTINCREMENT : PNK_POSTDECREMENT,
                                    JSOP_NOP,
                                    begin,
                                    pn);
        }
        return pn;
      }
    }
}

/*
 * A dedicated helper for transplanting the legacy comprehension expression E in
 *
 *   [E for (V in I)]   // legacy array comprehension
 *   (E for (V in I))   // legacy generator expression
 *
 * from its initial location in the AST, on the left of the 'for', to its final
 * position on the right. To avoid a separate pass we do this by adjusting the
 * blockids and name binding links that were established when E was parsed.
 *
 * A legacy generator expression desugars like so:
 *
 *   (E for (V in I)) => (function () { for (var V in I) yield E; })()
 *
 * so the transplanter must adjust static level as well as blockid. E's source
 * coordinates in root->pn_pos are critical to deciding which binding links to
 * preserve and which to cut.
 *
 * NB: This is not a general tree transplanter -- it knows in particular that
 * the one or more bindings induced by V have not yet been created.
 */
class LegacyCompExprTransplanter
{
    ParseNode*      root;
    Parser<FullParseHandler>* parser;
    ParseContext<FullParseHandler>* outerpc;
    GeneratorKind   comprehensionKind;
    unsigned        adjust;
    HashSet<Definition*> visitedImplicitArguments;

  public:
    LegacyCompExprTransplanter(ParseNode* pn, Parser<FullParseHandler>* parser,
                               ParseContext<FullParseHandler>* outerpc,
                               GeneratorKind kind, unsigned adj)
      : root(pn), parser(parser), outerpc(outerpc), comprehensionKind(kind), adjust(adj),
        visitedImplicitArguments(parser->context)
    {}

    bool init() {
        return visitedImplicitArguments.init();
    }

    bool transplant(ParseNode* pn);
};

/*
 * Any definitions nested within the legacy comprehension expression of a
 * generator expression must move "down" one static level, which of course
 * increases the upvar-frame-skip count.
 */
template <typename ParseHandler>
static bool
BumpStaticLevel(TokenStream& ts, ParseNode* pn, ParseContext<ParseHandler>* pc)
{
    if (pn->pn_cookie.isFree())
        return true;

    unsigned level = unsigned(pn->pn_cookie.level()) + 1;
    MOZ_ASSERT(level >= pc->staticLevel);
    return pn->pn_cookie.set(ts, level, pn->pn_cookie.slot());
}

template <typename ParseHandler>
static bool
AdjustBlockId(TokenStream& ts, ParseNode* pn, unsigned adjust, ParseContext<ParseHandler>* pc)
{
    MOZ_ASSERT(pn->isArity(PN_LIST) || pn->isArity(PN_CODE) || pn->isArity(PN_NAME));
    if (BlockIdLimit - pn->pn_blockid <= adjust + 1) {
        ts.reportError(JSMSG_NEED_DIET, "program");
        return false;
    }
    pn->pn_blockid += adjust;
    if (pn->pn_blockid >= pc->blockidGen)
        pc->blockidGen = pn->pn_blockid + 1;
    return true;
}

bool
LegacyCompExprTransplanter::transplant(ParseNode* pn)
{
    ParseContext<FullParseHandler>* pc = parser->pc;

    bool isGenexp = comprehensionKind != NotGenerator;

    if (!pn)
        return true;

    switch (pn->getArity()) {
      case PN_LIST:
        for (ParseNode* pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (!transplant(pn2))
                return false;
        }
        if (pn->pn_pos >= root->pn_pos) {
            if (!AdjustBlockId(parser->tokenStream, pn, adjust, pc))
                return false;
        }
        break;

      case PN_TERNARY:
        if (!transplant(pn->pn_kid1) ||
            !transplant(pn->pn_kid2) ||
            !transplant(pn->pn_kid3))
            return false;
        break;

      case PN_BINARY:
      case PN_BINARY_OBJ:
        if (!transplant(pn->pn_left))
            return false;

        /* Binary TOK_COLON nodes can have left == right. See bug 492714. */
        if (pn->pn_right != pn->pn_left) {
            if (!transplant(pn->pn_right))
                return false;
        }
        break;

      case PN_UNARY:
        if (!transplant(pn->pn_kid))
            return false;
        break;

      case PN_CODE:
      case PN_NAME:
        if (!transplant(pn->maybeExpr()))
            return false;

        if (pn->isDefn()) {
            if (isGenexp && !BumpStaticLevel(parser->tokenStream, pn, pc))
                return false;
        } else if (pn->isUsed()) {
            MOZ_ASSERT(pn->pn_cookie.isFree());

            Definition* dn = pn->pn_lexdef;
            MOZ_ASSERT(dn->isDefn());

            /*
             * Adjust the definition's block id only if it is a placeholder not
             * to the left of the root node, and if pn is the last use visited
             * in the legacy comprehension expression (to avoid adjusting the
             * blockid multiple times).
             *
             * Non-placeholder definitions within the legacy comprehension
             * expression will be visited further below.
             */
            if (dn->isPlaceholder() && dn->pn_pos >= root->pn_pos && dn->dn_uses == pn) {
                if (isGenexp && !BumpStaticLevel(parser->tokenStream, dn, pc))
                    return false;
                if (!AdjustBlockId(parser->tokenStream, dn, adjust, pc))
                    return false;
            }

            RootedAtom atom(parser->context, pn->pn_atom);
#ifdef DEBUG
            StmtInfoPC* stmt = LexicalLookup(pc, atom);
            MOZ_ASSERT(!stmt || stmt != pc->topStmt);
#endif
            if (isGenexp && !dn->isOp(JSOP_CALLEE)) {
                MOZ_ASSERT_IF(!pc->sc->isDotVariable(atom), !pc->decls().lookupFirst(atom));

                if (pc->sc->isDotVariable(atom)) {
                    if (dn->dn_uses == pn) {
                        if (!BumpStaticLevel(parser->tokenStream, dn, pc))
                            return false;
                        if (!AdjustBlockId(parser->tokenStream, dn, adjust, pc))
                            return false;
                    }
                } else if (dn->pn_pos < root->pn_pos) {
                    /*
                     * The variable originally appeared to be a use of a
                     * definition or placeholder outside the generator, but now
                     * we know it is scoped within the legacy comprehension
                     * tail's clauses. Make it (along with any other uses within
                     * the generator) a use of a new placeholder in the
                     * generator's lexdeps.
                     */
                    Definition* dn2 = parser->handler.newPlaceholder(atom, parser->pc->blockid(),
                                                                     parser->pos());
                    if (!dn2)
                        return false;
                    dn2->pn_pos = root->pn_pos;

                    /*
                     * Change all uses of |dn| that lie within the generator's
                     * |yield| expression into uses of dn2.
                     */
                    ParseNode** pnup = &dn->dn_uses;
                    ParseNode* pnu;
                    while ((pnu = *pnup) != nullptr && pnu->pn_pos >= root->pn_pos) {
                        pnu->pn_lexdef = dn2;
                        dn2->pn_dflags |= pnu->pn_dflags & PND_USE2DEF_FLAGS;
                        pnup = &pnu->pn_link;
                    }
                    dn2->dn_uses = dn->dn_uses;
                    dn->dn_uses = *pnup;
                    *pnup = nullptr;
                    DefinitionSingle def = DefinitionSingle::new_<FullParseHandler>(dn2);
                    if (!pc->lexdeps->put(atom, def))
                        return false;
                    if (dn->isClosed())
                        dn2->pn_dflags |= PND_CLOSED;
                } else if (dn->isPlaceholder()) {
                    /*
                     * The variable first occurs free in the 'yield' expression;
                     * move the existing placeholder node (and all its uses)
                     * from the parent's lexdeps into the generator's lexdeps.
                     */
                    outerpc->lexdeps->remove(atom);
                    DefinitionSingle def = DefinitionSingle::new_<FullParseHandler>(dn);
                    if (!pc->lexdeps->put(atom, def))
                        return false;
                } else if (dn->isImplicitArguments()) {
                    /*
                     * Implicit 'arguments' Definition nodes (see
                     * PND_IMPLICITARGUMENTS in Parser::functionBody) are only
                     * reachable via the lexdefs of their uses. Unfortunately,
                     * there may be multiple uses, so we need to maintain a set
                     * to only bump the definition once.
                     */
                    if (isGenexp && !visitedImplicitArguments.has(dn)) {
                        if (!BumpStaticLevel(parser->tokenStream, dn, pc))
                            return false;
                        if (!AdjustBlockId(parser->tokenStream, dn, adjust, pc))
                            return false;
                        if (!visitedImplicitArguments.put(dn))
                            return false;
                    }
                }
            }
        }

        if (pn->pn_pos >= root->pn_pos) {
            if (!AdjustBlockId(parser->tokenStream, pn, adjust, pc))
                return false;
        }
        break;

      case PN_NULLARY:
        /* Nothing. */
        break;
    }
    return true;
}

// Parsing legacy (JS1.7-style) comprehensions is terrible: we parse the head
// expression as if it's part of a comma expression, then when we see the "for"
// we transplant the parsed expression into the inside of a constructed
// for-of/for-in/for-each tail.  Transplanting an already-parsed expression is
// tricky, but the LegacyCompExprTransplanter handles most of that.
//
// The one remaining thing to patch up is the block scope depth.  We need to
// compute the maximum block scope depth of a function, so we know how much
// space to reserve in the fixed part of a stack frame.  Normally this is done
// whenever we leave a statement, via AccumulateBlockScopeDepth.
//
// Thing is, we don't actually know what that depth is, because the only
// information we keep is the maximum nested depth within a statement, so we
// just conservatively propagate the maximum nested depth from the top statement
// to the comprehension tail.
//
template <typename ParseHandler>
static unsigned
LegacyComprehensionHeadBlockScopeDepth(ParseContext<ParseHandler>* pc)
{
    return pc->topStmt ? pc->topStmt->innerBlockScopeDepth : pc->blockScopeDepth;
}

/*
 * Starting from a |for| keyword after the first array initialiser element or
 * an expression in an open parenthesis, parse the tail of the comprehension
 * or generator expression signified by this |for| keyword in context.
 *
 * Return null on failure, else return the top-most parse node for the array
 * comprehension or generator expression, with a unary node as the body of the
 * (possibly nested) for-loop, initialized by |kind, op, kid|.
 */
template <>
ParseNode*
Parser<FullParseHandler>::legacyComprehensionTail(ParseNode* bodyExpr, unsigned blockid,
                                                  GeneratorKind comprehensionKind,
                                                  ParseContext<FullParseHandler>* outerpc,
                                                  unsigned innerBlockScopeDepth)
{
    /*
     * If we saw any inner functions while processing the generator expression
     * then they may have upvars referring to the let vars in this generator
     * which were not correctly processed. Bail out and start over without
     * allowing lazy parsing.
     */
    if (handler.syntaxParser) {
        handler.disableSyntaxParser();
        abortedSyntaxParse = true;
        return nullptr;
    }

    unsigned adjust;
    ParseNode* pn;
    ParseNode* pn3;
    ParseNode** pnp;
    StmtInfoPC stmtInfo(context);
    BindData<FullParseHandler> data(context);
    TokenKind tt;

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    bool isGenexp = comprehensionKind != NotGenerator;

    if (isGenexp) {
        MOZ_ASSERT(comprehensionKind == LegacyGenerator);
        /*
         * Generator expression desugars to an immediately applied lambda that
         * yields the next value from a for-in loop (possibly nested, and with
         * optional if guard). Make pn be the TOK_LC body node.
         */
        pn = pushLexicalScope(&stmtInfo);
        if (!pn)
            return null();
        adjust = pn->pn_blockid - blockid;
    } else {
        /*
         * Make a parse-node and literal object representing the block scope of
         * this array comprehension. Our caller in primaryExpr, the TOK_LB case
         * aka the array initialiser case, has passed the blockid to claim for
         * the comprehension's block scope. We allocate that id or one above it
         * here, by calling PushLexicalScope.
         *
         * In the case of a comprehension expression that has nested blocks,
         * we will allocate a higher blockid but then slide all blocks "to the
         * right" to make room for the comprehension's block scope.
         */
        adjust = pc->blockid();
        pn = pushLexicalScope(&stmtInfo);
        if (!pn)
            return null();

        MOZ_ASSERT(blockid <= pn->pn_blockid);
        MOZ_ASSERT(blockid < pc->blockidGen);
        MOZ_ASSERT(pc->bodyid < blockid);
        pn->pn_blockid = stmtInfo.blockid = blockid;
        MOZ_ASSERT(adjust < blockid);
        adjust = blockid - adjust;
    }

    handler.setBeginPosition(pn, bodyExpr);

    pnp = &pn->pn_expr;

    LegacyCompExprTransplanter transplanter(bodyExpr, this, outerpc, comprehensionKind, adjust);
    if (!transplanter.init())
        return null();

    if (!transplanter.transplant(bodyExpr))
        return null();

    MOZ_ASSERT(pc->staticScope && pc->staticScope == pn->pn_objbox->object);
    data.initLexical(HoistVars, &pc->staticScope->as<StaticBlockObject>(),
                     JSMSG_ARRAY_INIT_TOO_BIG);

    while (true) {
        /*
         * FOR node is binary, left is loop control and right is body.  Use
         * index to count each block-local let-variable on the left-hand side
         * of the in/of.
         */
        ParseNode* pn2 = handler.new_<BinaryNode>(PNK_FOR, JSOP_ITER, pos(),
                                                  nullptr, nullptr);
        if (!pn2)
            return null();

        pn2->pn_iflags = JSITER_ENUMERATE;
        if (allowsForEachIn()) {
            bool matched;
            if (!tokenStream.matchContextualKeyword(&matched, context->names().each))
                return null();
            if (matched) {
                pn2->pn_iflags |= JSITER_FOREACH;
                addTelemetry(JSCompartment::DeprecatedForEach);
                if (versionNumber() < JSVERSION_LATEST) {
                    if (!report(ParseWarning, pc->sc->strict(), pn2, JSMSG_DEPRECATED_FOR_EACH))
                        return null();
                }
            }
        }
        MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

        uint32_t startYieldOffset = pc->lastYieldOffset;

        RootedPropertyName name(context);
        if (!tokenStream.getToken(&tt))
            return null();
        switch (tt) {
          case TOK_LB:
          case TOK_LC:
            pc->inDeclDestructuring = true;
            pn3 = primaryExpr(YieldIsKeyword, tt);
            pc->inDeclDestructuring = false;
            if (!pn3)
                return null();
            break;

          case TOK_NAME:
            name = tokenStream.currentName();

            /*
             * Create a name node with pn_op JSOP_GETNAME.  We can't set pn_op to
             * JSOP_GETLOCAL here, because we don't yet know the block's depth
             * in the operand stack frame.  The code generator computes that,
             * and it tries to bind all names to slots, so we must let it do
             * the deed.
             */
            pn3 = newBindingNode(name, false);
            if (!pn3)
                return null();
            break;

          default:
            report(ParseError, false, null(), JSMSG_NO_VARIABLE_NAME);
            return null();
        }

        bool isForIn, isForOf;
        if (!matchInOrOf(&isForIn, &isForOf))
            return null();
        if (!isForIn && !isForOf) {
            report(ParseError, false, null(), JSMSG_IN_AFTER_FOR_NAME);
            return null();
        }
        ParseNodeKind headKind = PNK_FORIN;
        if (isForOf) {
            if (pn2->pn_iflags != JSITER_ENUMERATE) {
                MOZ_ASSERT(pn2->pn_iflags == (JSITER_FOREACH | JSITER_ENUMERATE));
                report(ParseError, false, null(), JSMSG_BAD_FOR_EACH_LOOP);
                return null();
            }
            pn2->pn_iflags = 0;
            headKind = PNK_FOROF;
        }

        ParseNode* pn4 = expr(InAllowed, YieldIsKeyword);
        if (!pn4)
            return null();
        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

        if (isGenexp && pc->lastYieldOffset != startYieldOffset) {
            reportWithOffset(ParseError, false, pc->lastYieldOffset,
                             JSMSG_BAD_GENEXP_BODY, js_yield_str);
            return null();
        }

        switch (tt) {
          case TOK_LB:
          case TOK_LC:
            if (!checkDestructuringPattern(&data, pn3))
                return null();
            break;

          case TOK_NAME:
            data.pn = pn3;
            if (!data.binder(&data, name, this))
                return null();
            break;

          default:;
        }

        /*
         * Synthesize a declaration. Every definition must appear in the parse
         * tree in order for ComprehensionTranslator to work.
         *
         * These are lets to tell the bytecode emitter to emit initialization
         * code for the temporal dead zone.
         */
        ParseNode* lets = handler.newList(PNK_LET, pn3);
        if (!lets)
            return null();
        lets->pn_xflags |= PNX_POPVAR;

        /* Definitions can't be passed directly to EmitAssignment as lhs. */
        pn3 = cloneLeftHandSide(pn3);
        if (!pn3)
            return null();

        pn2->pn_left = handler.newTernary(headKind, lets, pn3, pn4);
        if (!pn2->pn_left)
            return null();
        *pnp = pn2;
        pnp = &pn2->pn_right;

        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_FOR))
            return null();
        if (!matched)
            break;
    }

    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_IF))
        return null();
    if (matched) {
        ParseNode* cond = condition(InAllowed, YieldIsKeyword);
        if (!cond)
            return null();
        ParseNode* ifNode = handler.new_<TernaryNode>(PNK_IF, JSOP_NOP, cond, nullptr, nullptr,
                                                      cond->pn_pos);
        if (!ifNode)
            return null();
        *pnp = ifNode;
        pnp = &ifNode->pn_kid2;
    }

    ParseNode* bodyStmt;
    if (isGenexp) {
        ParseNode* yieldExpr = newYieldExpression(bodyExpr->pn_pos.begin, bodyExpr);
        if (!yieldExpr)
            return null();
        yieldExpr->setInParens(true);

        bodyStmt = handler.newExprStatement(yieldExpr, bodyExpr->pn_pos.end);
        if (!bodyStmt)
            return null();
    } else {
        bodyStmt = handler.newUnary(PNK_ARRAYPUSH, JSOP_ARRAYPUSH,
                                    bodyExpr->pn_pos.begin, bodyExpr);
        if (!bodyStmt)
            return null();
    }

    *pnp = bodyStmt;

    pc->topStmt->innerBlockScopeDepth += innerBlockScopeDepth;
    PopStatementPC(tokenStream, pc);

    handler.setEndPosition(pn, pos().end);

    return pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::legacyComprehensionTail(SyntaxParseHandler::Node bodyStmt,
                                                    unsigned blockid,
                                                    GeneratorKind comprehensionKind,
                                                    ParseContext<SyntaxParseHandler>* outerpc,
                                                    unsigned innerBlockScopeDepth)
{
    abortIfSyntaxParser();
    return null();
}

template <>
ParseNode*
Parser<FullParseHandler>::legacyArrayComprehension(ParseNode* array)
{
    // Discard our presumed array literal containing only a single element, and
    // instead return an array comprehension node.  Extract the few bits of
    // information needed from the array literal, then free it.
    MOZ_ASSERT(array->isKind(PNK_ARRAY));
    MOZ_ASSERT(array->pn_count == 1);

    uint32_t arrayBegin = handler.getPosition(array).begin;
    uint32_t blockid = array->pn_blockid;

    ParseNode* bodyExpr = array->pn_head;
    array->pn_count = 0;
    array->pn_tail = &array->pn_head;
    *array->pn_tail = nullptr;

    handler.freeTree(array);

    ParseNode* comp = legacyComprehensionTail(bodyExpr, blockid, NotGenerator, nullptr,
                                              LegacyComprehensionHeadBlockScopeDepth(pc));
    if (!comp)
        return null();

    MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_ARRAY_COMPREHENSION);

    return handler.newArrayComprehension(comp, blockid, TokenPos(arrayBegin, pos().end));
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::legacyArrayComprehension(Node array)
{
    abortIfSyntaxParser();
    return null();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::generatorComprehensionLambda(GeneratorKind comprehensionKind,
                                                   unsigned begin, Node innerExpr)
{
    MOZ_ASSERT(comprehensionKind == LegacyGenerator || comprehensionKind == StarGenerator);
    MOZ_ASSERT(!!innerExpr == (comprehensionKind == LegacyGenerator));

    Node genfn = handler.newFunctionDefinition();
    if (!genfn)
        return null();
    handler.setOp(genfn, JSOP_LAMBDA);

    ParseContext<ParseHandler>* outerpc = pc;

    // If we are off the main thread, the generator meta-objects have
    // already been created by js::StartOffThreadParseScript, so cx will not
    // be necessary.
    RootedObject proto(context);
    if (comprehensionKind == StarGenerator) {
        JSContext* cx = context->maybeJSContext();
        proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, context->global());
        if (!proto)
            return null();
    }

    RootedFunction fun(context, newFunction(/* atom = */ nullptr, Expression, comprehensionKind, proto));
    if (!fun)
        return null();

    // Create box for fun->object early to root it.
    Directives directives(/* strict = */ outerpc->sc->strict());
    FunctionBox* genFunbox = newFunctionBox(genfn, fun, outerpc, directives, comprehensionKind);
    if (!genFunbox)
        return null();

    ParseContext<ParseHandler> genpc(this, outerpc, genfn, genFunbox,
                                     /* newDirectives = */ nullptr,
                                     outerpc->staticLevel + 1, outerpc->blockidGen,
                                     /* blockScopeDepth = */ 0);
    if (!genpc.init(tokenStream))
        return null();

    /*
     * We assume conservatively that any deoptimization flags in pc->sc
     * come from the kid. So we propagate these flags into genfn. For code
     * simplicity we also do not detect if the flags were only set in the
     * kid and could be removed from pc->sc.
     */
    genFunbox->anyCxFlags = outerpc->sc->anyCxFlags;
    if (outerpc->sc->isFunctionBox())
        genFunbox->funCxFlags = outerpc->sc->asFunctionBox()->funCxFlags;

    MOZ_ASSERT(genFunbox->generatorKind() == comprehensionKind);
    genFunbox->inGenexpLambda = true;
    handler.setBlockId(genfn, genpc.bodyid);

    Node generator = newName(context->names().dotGenerator);
    if (!generator)
        return null();
    if (!pc->define(tokenStream, context->names().dotGenerator, generator, Definition::VAR))
        return null();

    Node body = handler.newStatementList(pc->blockid(), TokenPos(begin, pos().end));
    if (!body)
        return null();

    Node comp;
    if (comprehensionKind == StarGenerator) {
        comp = comprehension(StarGenerator);
        if (!comp)
            return null();
    } else {
        MOZ_ASSERT(comprehensionKind == LegacyGenerator);
        comp = legacyComprehensionTail(innerExpr, outerpc->blockid(), LegacyGenerator,
                                       outerpc, LegacyComprehensionHeadBlockScopeDepth(outerpc));
        if (!comp)
            return null();
    }

    if (comprehensionKind == StarGenerator)
        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);

    handler.setBeginPosition(comp, begin);
    handler.setEndPosition(comp, pos().end);
    handler.addStatementToList(body, comp, pc);
    handler.setEndPosition(body, pos().end);
    handler.setBeginPosition(genfn, begin);
    handler.setEndPosition(genfn, pos().end);

    generator = newName(context->names().dotGenerator);
    if (!generator)
        return null();
    if (!noteNameUse(context->names().dotGenerator, generator))
        return null();
    if (!handler.prependInitialYield(body, generator))
        return null();

    // Note that if we ever start syntax-parsing generators, we will also
    // need to propagate the closed-over variable set to the inner
    // lazyscript, as in finishFunctionDefinition.
    handler.setFunctionBody(genfn, body);

    PropagateTransitiveParseFlags(genFunbox, outerpc->sc);

    if (!leaveFunction(genfn, outerpc))
        return null();

    return genfn;
}

#if JS_HAS_GENERATOR_EXPRS

/*
 * Starting from a |for| keyword after an expression, parse the comprehension
 * tail completing this generator expression. Wrap the expression at kid in a
 * generator function that is immediately called to evaluate to the generator
 * iterator that is the value of this legacy generator expression.
 *
 * |kid| must be the expression before the |for| keyword; we return an
 * application of a generator function that includes the |for| loops and
 * |if| guards, with |kid| as the operand of a |yield| expression as the
 * innermost loop body.
 *
 * Note how unlike Python, we do not evaluate the expression to the right of
 * the first |in| in the chain of |for| heads. Instead, a generator expression
 * is merely sugar for a generator function expression and its application.
 */
template <>
ParseNode*
Parser<FullParseHandler>::legacyGeneratorExpr(ParseNode* expr)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    // Make a new node for the desugared generator function.
    ParseNode* genfn = generatorComprehensionLambda(LegacyGenerator, expr->pn_pos.begin, expr);
    if (!genfn)
        return null();

    // Our result is a call expression that invokes the anonymous generator
    // function object.
    return handler.newList(PNK_GENEXP, genfn, JSOP_CALL);
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::legacyGeneratorExpr(Node kid)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

static const char js_generator_str[] = "generator";

#endif /* JS_HAS_GENERATOR_EXPRS */

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehensionFor(GeneratorKind comprehensionKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    // FIXME: Destructuring binding (bug 980828).

    MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_VARIABLE_NAME);
    RootedPropertyName name(context, tokenStream.currentName());
    if (name == context->names().let) {
        report(ParseError, false, null(), JSMSG_LET_COMP_BINDING);
        return null();
    }
    Node assignLhs = newName(name);
    if (!assignLhs)
        return null();
    Node lhs = newName(name);
    if (!lhs)
        return null();
    bool matched;
    if (!tokenStream.matchContextualKeyword(&matched, context->names().of))
        return null();
    if (!matched) {
        report(ParseError, false, null(), JSMSG_OF_AFTER_FOR_NAME);
        return null();
    }

    Node rhs = assignExpr(InAllowed, YieldIsKeyword);
    if (!rhs)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_OF_ITERABLE);

    TokenPos headPos(begin, pos().end);

    StmtInfoPC stmtInfo(context);
    BindData<ParseHandler> data(context);
    RootedStaticBlockObject blockObj(context, StaticBlockObject::create(context));
    if (!blockObj)
        return null();
    data.initLexical(DontHoistVars, blockObj, JSMSG_TOO_MANY_LOCALS);
    Node decls = handler.newList(PNK_LET, lhs);
    if (!decls)
        return null();
    data.pn = lhs;
    if (!data.binder(&data, name, this))
        return null();
    Node letScope = pushLetScope(blockObj, &stmtInfo);
    if (!letScope)
        return null();
    handler.setLexicalScopeBody(letScope, decls);

    if (!noteNameUse(name, assignLhs))
        return null();
    handler.setOp(assignLhs, JSOP_SETNAME);

    Node head = handler.newForHead(PNK_FOROF, letScope, assignLhs, rhs, headPos);
    if (!head)
        return null();

    Node tail = comprehensionTail(comprehensionKind);
    if (!tail)
        return null();

    PopStatementPC(tokenStream, pc);

    return handler.newForStatement(begin, head, tail, JSOP_ITER);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehensionIf(GeneratorKind comprehensionKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_IF));

    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_COND);
    Node cond = assignExpr(InAllowed, YieldIsKeyword);
    if (!cond)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_COND);

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    if (handler.isUnparenthesizedAssignment(cond)) {
        if (!report(ParseExtraWarning, false, null(), JSMSG_EQUAL_AS_ASSIGN))
            return null();
    }

    Node then = comprehensionTail(comprehensionKind);
    if (!then)
        return null();

    return handler.newIfStatement(begin, cond, then, null());
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehensionTail(GeneratorKind comprehensionKind)
{
    JS_CHECK_RECURSION(context, return null());

    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_FOR, TokenStream::Operand))
        return null();
    if (matched)
        return comprehensionFor(comprehensionKind);

    if (!tokenStream.matchToken(&matched, TOK_IF, TokenStream::Operand))
        return null();
    if (matched)
        return comprehensionIf(comprehensionKind);

    uint32_t begin = pos().begin;

    Node bodyExpr = assignExpr(InAllowed, YieldIsKeyword);
    if (!bodyExpr)
        return null();

    if (comprehensionKind == NotGenerator)
        return handler.newUnary(PNK_ARRAYPUSH, JSOP_ARRAYPUSH, begin, bodyExpr);

    MOZ_ASSERT(comprehensionKind == StarGenerator);
    Node yieldExpr = newYieldExpression(begin, bodyExpr);
    if (!yieldExpr)
        return null();
    yieldExpr = handler.parenthesize(yieldExpr);

    return handler.newExprStatement(yieldExpr, pos().end);
}

// Parse an ES6 generator or array comprehension, starting at the first 'for'.
// The caller is responsible for matching the ending TOK_RP or TOK_RB.
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehension(GeneratorKind comprehensionKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    uint32_t startYieldOffset = pc->lastYieldOffset;

    Node body = comprehensionFor(comprehensionKind);
    if (!body)
        return null();

    if (comprehensionKind != NotGenerator && pc->lastYieldOffset != startYieldOffset) {
        reportWithOffset(ParseError, false, pc->lastYieldOffset,
                         JSMSG_BAD_GENEXP_BODY, js_yield_str);
        return null();
    }

    return body;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::arrayComprehension(uint32_t begin)
{
    Node inner = comprehension(NotGenerator);
    if (!inner)
        return null();

    MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_ARRAY_COMPREHENSION);

    Node comp = handler.newList(PNK_ARRAYCOMP, inner);
    if (!comp)
        return null();

    handler.setBeginPosition(comp, begin);
    handler.setEndPosition(comp, pos().end);

    return comp;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::generatorComprehension(uint32_t begin)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    // We have no problem parsing generator comprehensions inside lazy
    // functions, but the bytecode emitter currently can't handle them that way,
    // because when it goes to emit the code for the inner generator function,
    // it expects outer functions to have non-lazy scripts.
    if (!abortIfSyntaxParser())
        return null();

    Node genfn = generatorComprehensionLambda(StarGenerator, begin, null());
    if (!genfn)
        return null();

    Node result = handler.newList(PNK_GENEXP, genfn, JSOP_CALL);
    if (!result)
        return null();
    handler.setBeginPosition(result, begin);
    handler.setEndPosition(result, pos().end);

    return result;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::assignExprWithoutYield(YieldHandling yieldHandling, unsigned msg)
{
    uint32_t startYieldOffset = pc->lastYieldOffset;
    Node res = assignExpr(InAllowed, yieldHandling);
    if (res && pc->lastYieldOffset != startYieldOffset) {
        reportWithOffset(ParseError, false, pc->lastYieldOffset,
                         msg, js_yield_str);
        return null();
    }
    return res;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::argumentList(YieldHandling yieldHandling, Node listNode, bool* isSpread)
{
    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_RP, TokenStream::Operand))
        return false;
    if (matched) {
        handler.setEndPosition(listNode, pos().end);
        return true;
    }

    uint32_t startYieldOffset = pc->lastYieldOffset;
    bool arg0 = true;

    while (true) {
        bool spread = false;
        uint32_t begin = 0;
        if (!tokenStream.matchToken(&matched, TOK_TRIPLEDOT, TokenStream::Operand))
            return false;
        if (matched) {
            spread = true;
            begin = pos().begin;
            *isSpread = true;
        }

        Node argNode = assignExpr(InAllowed, yieldHandling);
        if (!argNode)
            return false;
        if (spread) {
            argNode = handler.newUnary(PNK_SPREAD, JSOP_NOP, begin, argNode);
            if (!argNode)
                return false;
        }

        if (handler.isUnparenthesizedYieldExpression(argNode)) {
            TokenKind tt;
            if (!tokenStream.peekToken(&tt))
                return false;
            if (tt == TOK_COMMA) {
                report(ParseError, false, argNode, JSMSG_BAD_GENERATOR_SYNTAX, js_yield_str);
                return false;
            }
        }
#if JS_HAS_GENERATOR_EXPRS
        if (!spread) {
            if (!tokenStream.matchToken(&matched, TOK_FOR))
                return false;
            if (matched) {
                if (pc->lastYieldOffset != startYieldOffset) {
                    reportWithOffset(ParseError, false, pc->lastYieldOffset,
                                     JSMSG_BAD_GENEXP_BODY, js_yield_str);
                    return false;
                }
                argNode = legacyGeneratorExpr(argNode);
                if (!argNode)
                    return false;
                if (!arg0) {
                    report(ParseError, false, argNode, JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
                    return false;
                }
                TokenKind tt;
                if (!tokenStream.peekToken(&tt))
                    return false;
                if (tt == TOK_COMMA) {
                    report(ParseError, false, argNode, JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
                    return false;
                }
            }
        }
#endif
        arg0 = false;

        handler.addList(listNode, argNode);

        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_COMMA))
            return false;
        if (!matched)
            break;
    }

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return false;
    if (tt != TOK_RP) {
        report(ParseError, false, null(), JSMSG_PAREN_AFTER_ARGS);
        return false;
    }
    handler.setEndPosition(listNode, pos().end);
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkAllowedNestedSyntax(SharedContext::AllowedSyntax allowed,
                                               SharedContext** allowingContext)
{
    for (GenericParseContext* gpc = pc; gpc; gpc = gpc->parent) {
        SharedContext* sc = gpc->sc;

        // Arrow functions don't help decide whether we should allow nested
        // syntax, as they don't store any of the necessary state for themselves.
        if (sc->isFunctionBox() && sc->asFunctionBox()->function()->isArrow())
            continue;

        if (!sc->allowSyntax(allowed))
            return false;
        if (allowingContext)
            *allowingContext = sc;
        return true;
    }
    return false;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkAndMarkSuperScope()
{
    SharedContext* foundContext = nullptr;
    if (checkAllowedNestedSyntax(SharedContext::AllowedSyntax::SuperProperty, &foundContext)) {
        if (foundContext->isFunctionBox())
            foundContext->asFunctionBox()->setNeedsHomeObject();
        return true;
    }
    return false;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::memberExpr(YieldHandling yieldHandling, TokenKind tt, bool allowCallSyntax,
                                 InvokedPrediction invoked)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(tt));

    Node lhs;

    JS_CHECK_RECURSION(context, return null());

    bool isSuper = false;
    uint32_t superBegin = pos().begin;

    /* Check for new expression first. */
    if (tt == TOK_NEW) {
        uint32_t newBegin = pos().begin;
        // Make sure this wasn't a |new.target| in disguise.
        Node newTarget;
        if (!tryNewTarget(newTarget))
            return null();
        if (newTarget) {
            lhs = newTarget;
        } else {
            lhs = handler.newList(PNK_NEW, newBegin, JSOP_NEW);
            if (!lhs)
                return null();

            // Gotten by tryNewTarget
            tt = tokenStream.currentToken().type;
            Node ctorExpr = memberExpr(yieldHandling, tt, false, PredictInvoked);
            if (!ctorExpr)
                return null();

            handler.addList(lhs, ctorExpr);

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_LP))
                return null();
            if (matched) {
                bool isSpread = false;
                if (!argumentList(yieldHandling, lhs, &isSpread))
                    return null();
                if (isSpread)
                    handler.setOp(lhs, JSOP_SPREADNEW);
            }
        }
    } else if (tt == TOK_SUPER) {
        lhs = null();
        isSuper = true;
    } else {
        lhs = primaryExpr(yieldHandling, tt, invoked);
        if (!lhs)
            return null();
    }

    while (true) {
        if (!tokenStream.getToken(&tt))
            return null();
        if (tt == TOK_EOF)
            break;

        Node nextMember;
        if (tt == TOK_DOT) {
            if (!tokenStream.getToken(&tt, TokenStream::KeywordIsName))
                return null();
            if (tt == TOK_NAME) {
                PropertyName* field = tokenStream.currentName();
                if (isSuper) {
                    isSuper = false;
                    if (!checkAndMarkSuperScope()) {
                        report(ParseError, false, null(), JSMSG_BAD_SUPERPROP, "property");
                        return null();
                    }
                    nextMember = handler.newSuperProperty(field, TokenPos(superBegin, pos().end));
                } else {
                    nextMember = handler.newPropertyAccess(lhs, field, pos().end);
                }
                if (!nextMember)
                    return null();
            } else {
                report(ParseError, false, null(), JSMSG_NAME_AFTER_DOT);
                return null();
            }
        } else if (tt == TOK_LB) {
            Node propExpr = expr(InAllowed, yieldHandling);
            if (!propExpr)
                return null();

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_IN_INDEX);

            if (isSuper) {
                isSuper = false;
                if (!checkAndMarkSuperScope()) {
                    report(ParseError, false, null(), JSMSG_BAD_SUPERPROP, "member");
                    return null();
                }
                nextMember = handler.newSuperElement(propExpr, TokenPos(superBegin, pos().end));
            } else {
                nextMember = handler.newPropertyByValue(lhs, propExpr, pos().end);
            }
            if (!nextMember)
                return null();
        } else if ((allowCallSyntax && tt == TOK_LP) ||
                   tt == TOK_TEMPLATE_HEAD ||
                   tt == TOK_NO_SUBS_TEMPLATE)
        {
            if (isSuper) {
                // For now...
                report(ParseError, false, null(), JSMSG_BAD_SUPER);
                return null();
            }

            nextMember = tt == TOK_LP ? handler.newCall() : handler.newTaggedTemplate();
            if (!nextMember)
                return null();

            JSOp op = JSOP_CALL;
            if (PropertyName* name = handler.maybeNameAnyParentheses(lhs)) {
                if (tt == TOK_LP && name == context->names().eval) {
                    /* Select JSOP_EVAL and flag pc as heavyweight. */
                    op = pc->sc->strict() ? JSOP_STRICTEVAL : JSOP_EVAL;
                    pc->sc->setBindingsAccessedDynamically();
                    pc->sc->setHasDirectEval();

                    /*
                     * In non-strict mode code, direct calls to eval can add
                     * variables to the call object.
                     */
                    if (pc->sc->isFunctionBox() && !pc->sc->strict())
                        pc->sc->asFunctionBox()->setHasExtensibleScope();

                    // If we're in a method, mark the method as requiring
                    // support for 'super', since direct eval code can use it.
                    // (If we're not in a method, that's fine, so ignore the
                    // return value.)
                    checkAndMarkSuperScope();
                }
            } else if (PropertyName* prop = handler.maybeDottedProperty(lhs)) {
                // Use the JSOP_FUN{APPLY,CALL} optimizations given the right
                // syntax.
                if (prop == context->names().apply) {
                    op = JSOP_FUNAPPLY;
                    if (pc->sc->isFunctionBox())
                        pc->sc->asFunctionBox()->usesApply = true;
                } else if (prop == context->names().call) {
                    op = JSOP_FUNCALL;
                }
            }

            handler.setBeginPosition(nextMember, lhs);
            handler.addList(nextMember, lhs);

            if (tt == TOK_LP) {
                bool isSpread = false;
                if (!argumentList(yieldHandling, nextMember, &isSpread))
                    return null();
                if (isSpread) {
                    if (op == JSOP_EVAL)
                        op = JSOP_SPREADEVAL;
                    else if (op == JSOP_STRICTEVAL)
                        op = JSOP_STRICTSPREADEVAL;
                    else
                        op = JSOP_SPREADCALL;
                }
            } else {
                if (!taggedTemplate(yieldHandling, nextMember, tt))
                    return null();
            }
            handler.setOp(nextMember, op);
        } else {
            if (isSuper) {
                report(ParseError, false, null(), JSMSG_BAD_SUPER);
                return null();
            }
            tokenStream.ungetToken();
            return lhs;
        }

        lhs = nextMember;
    }

    if (isSuper) {
        report(ParseError, false, null(), JSMSG_BAD_SUPER);
        return null();
    }

    return lhs;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newName(PropertyName* name)
{
    return handler.newName(name, pc->blockid(), pos(), context);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::identifierName(YieldHandling yieldHandling)
{
    RootedPropertyName name(context, tokenStream.currentName());
    if (yieldHandling == YieldIsKeyword && name == context->names().yield) {
        report(ParseError, false, null(), JSMSG_RESERVED_ID, "yield");
        return null();
    }

    // If we're inside a function that later becomes a legacy generator, then
    // a |yield| identifier name here will be detected by a subsequent
    // |checkYieldNameValidity| call.
    Node pn = newName(name);
    if (!pn)
        return null();

    if (!pc->inDeclDestructuring && !noteNameUse(name, pn))
        return null();

    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::stringLiteral()
{
    return handler.newStringLiteral(stopStringCompression(), pos());
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::noSubstitutionTemplate()
{
    return handler.newTemplateStringLiteral(stopStringCompression(), pos());
}

template <typename ParseHandler>
JSAtom * Parser<ParseHandler>::stopStringCompression() {
    JSAtom* atom = tokenStream.currentToken().atom();

    // Large strings are fast to parse but slow to compress. Stop compression on
    // them, so we don't wait for a long time for compression to finish at the
    // end of compilation.
    const size_t HUGE_STRING = 50000;
    if (sct && sct->active() && atom->length() >= HUGE_STRING)
        sct->abort();
    return atom;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newRegExp()
{
    MOZ_ASSERT(!options().selfHostingMode);
    // Create the regexp even when doing a syntax parse, to check the regexp's syntax.
    const char16_t* chars = tokenStream.getTokenbuf().begin();
    size_t length = tokenStream.getTokenbuf().length();
    RegExpFlag flags = tokenStream.currentToken().regExpFlags();

    Rooted<RegExpObject*> reobj(context);
    RegExpStatics* res = context->global()->getRegExpStatics(context);
    if (!res)
        return null();

    reobj = RegExpObject::create(context, res, chars, length, flags, &tokenStream, alloc);
    if (!reobj)
        return null();

    return handler.newRegExp(reobj, pos(), *this);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::arrayInitializer(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LB));

    uint32_t begin = pos().begin;
    Node literal = handler.newArrayLiteral(begin, pc->blockidGen);
    if (!literal)
        return null();

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    // Handle an ES7 array comprehension first.
    if (tt == TOK_FOR)
        return arrayComprehension(begin);

    if (tt == TOK_RB) {
        /*
         * Mark empty arrays as non-constant, since we cannot easily
         * determine their type.
         */
        handler.setListFlag(literal, PNX_NONCONST);
    } else {
        tokenStream.ungetToken();

        bool spread = false, missingTrailingComma = false;
        uint32_t index = 0;
        for (; ; index++) {
            if (index == NativeObject::NELEMENTS_LIMIT) {
                report(ParseError, false, null(), JSMSG_ARRAY_INIT_TOO_BIG);
                return null();
            }

            TokenKind tt;
            if (!tokenStream.peekToken(&tt, TokenStream::Operand))
                return null();
            if (tt == TOK_RB)
                break;

            if (tt == TOK_COMMA) {
                tokenStream.consumeKnownToken(TOK_COMMA);
                if (!handler.addElision(literal, pos()))
                    return null();
            } else if (tt == TOK_TRIPLEDOT) {
                spread = true;
                tokenStream.consumeKnownToken(TOK_TRIPLEDOT);
                uint32_t begin = pos().begin;
                Node inner = assignExpr(InAllowed, yieldHandling);
                if (!inner)
                    return null();
                if (!handler.addSpreadElement(literal, begin, inner))
                    return null();
            } else {
                Node element = assignExpr(InAllowed, yieldHandling);
                if (!element)
                    return null();
                if (foldConstants && !FoldConstants(context, &element, this))
                    return null();
                handler.addArrayElement(literal, element);
            }

            if (tt != TOK_COMMA) {
                /* If we didn't already match TOK_COMMA in above case. */
                bool matched;
                if (!tokenStream.matchToken(&matched, TOK_COMMA))
                    return null();
                if (!matched) {
                    missingTrailingComma = true;
                    break;
                }
            }
        }

        /*
         * At this point, (index == 0 && missingTrailingComma) implies one
         * element initialiser was parsed.
         *
         * A legacy array comprehension of the form:
         *
         *   [i * j for (i in o) for (j in p) if (i != j)]
         *
         * translates to roughly the following code:
         *
         *   {
         *     let array = new Array, i, j;
         *     for (i in o) let {
         *       for (j in p)
         *         if (i != j)
         *           array.push(i * j)
         *     }
         *     array
         *   }
         *
         * where array is a nameless block-local variable. The "roughly" means
         * that an implementation may optimize away the array.push.  A legacy
         * array comprehension opens exactly one block scope, no matter how many
         * for heads it contains.
         *
         * Each let () {...} or for (let ...) ... compiles to:
         *
         *   JSOP_PUSHN <N>            // Push space for block-scoped locals.
         *   (JSOP_PUSHBLOCKSCOPE <O>) // If a local is aliased, push on scope
         *                             // chain.
         *   ...
         *   JSOP_DEBUGLEAVEBLOCK      // Invalidate any DebugScope proxies.
         *   JSOP_POPBLOCKSCOPE?       // Pop off scope chain, if needed.
         *   JSOP_POPN <N>             // Pop space for block-scoped locals.
         *
         * where <o> is a literal object representing the block scope,
         * with <n> properties, naming each var declared in the block.
         *
         * Each var declaration in a let-block binds a name in <o> at compile
         * time. A block-local var is accessed by the JSOP_GETLOCAL and
         * JSOP_SETLOCAL ops. These ops have an immediate operand, the local
         * slot's stack index from fp->spbase.
         *
         * The legacy array comprehension iteration step, array.push(i * j) in
         * the example above, is done by <i * j>; JSOP_ARRAYPUSH <array>, where
         * <array> is the index of array's stack slot.
         */
        if (index == 0 && !spread) {
            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_FOR))
                return null();
            if (matched && missingTrailingComma)
                return legacyArrayComprehension(literal);
        }

        MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_LIST);
    }
    handler.setEndPosition(literal, pos().end);
    return literal;
}

static JSAtom*
DoubleToAtom(ExclusiveContext* cx, double value)
{
    // This is safe because doubles can not be moved.
    Value tmp = DoubleValue(value);
    return ToAtom<CanGC>(cx, HandleValue::fromMarkedLocation(&tmp));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::computedPropertyName(YieldHandling yieldHandling, Node literal)
{
    uint32_t begin = pos().begin;

    // Turn off the inDeclDestructuring flag when parsing computed property
    // names. In short, when parsing 'let {[x + y]: z} = obj;', noteNameUse()
    // should be called on x and y, but not on z. See the comment on
    // Parser<>::checkDestructuringPattern() for details.
    bool saved = pc->inDeclDestructuring;
    pc->inDeclDestructuring = false;
    Node assignNode = assignExpr(InAllowed, yieldHandling);
    pc->inDeclDestructuring = saved;
    if (!assignNode)
        return null();

    MUST_MATCH_TOKEN(TOK_RB, JSMSG_COMP_PROP_UNTERM_EXPR);
    Node propname = handler.newComputedName(assignNode, begin, pos().end);
    if (!propname)
        return null();
    handler.setListFlag(literal, PNX_NONCONST);
    return propname;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newPropertyListNode(PropListType type)
{
    if (IsClassBody(type))
        return handler.newClassMethodList(pos().begin);

    MOZ_ASSERT(type == ObjectLiteral);
    return handler.newObjectLiteral(pos().begin);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::propertyList(YieldHandling yieldHandling, PropListType type)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));

    Node propList = newPropertyListNode(type);
    if (!propList)
        return null();

    bool seenPrototypeMutation = false;
    bool seenConstructor = false;
    RootedAtom atom(context);
    for (;;) {
        TokenKind ltok;
        if (!tokenStream.getToken(&ltok, TokenStream::KeywordIsName))
            return null();
        if (ltok == TOK_RC)
            break;

        bool isStatic = false;
        if (IsClassBody(type)) {
            if (ltok == TOK_SEMI)
                continue;

            if (ltok == TOK_NAME &&
                tokenStream.currentName() == context->names().static_)
            {
                isStatic = true;
                if (!tokenStream.getToken(&ltok, TokenStream::KeywordIsName))
                    return null();
            }
        }

        bool isGenerator = false;
        if (ltok == TOK_MUL) {
            isGenerator = true;
            if (!tokenStream.getToken(&ltok, TokenStream::KeywordIsName))
                return null();
        }

        atom = nullptr;

        JSOp op = JSOP_INITPROP;
        Node propname;
        bool isConstructor = false;
        switch (ltok) {
          case TOK_NUMBER:
            atom = DoubleToAtom(context, tokenStream.currentToken().number());
            if (!atom)
                return null();
            propname = newNumber(tokenStream.currentToken());
            if (!propname)
                return null();
            break;

          case TOK_LB: {
              propname = computedPropertyName(yieldHandling, propList);
              if (!propname)
                  return null();
              break;
          }

          case TOK_NAME: {
            atom = tokenStream.currentName();
            // Do not look for accessor syntax on generators
            if (!isGenerator &&
                (atom == context->names().get ||
                 atom == context->names().set))
            {
                op = atom == context->names().get ? JSOP_INITPROP_GETTER
                                                  : JSOP_INITPROP_SETTER;
            } else {
                propname = handler.newObjectLiteralPropertyName(atom, pos());
                if (!propname)
                    return null();
                break;
            }

            // We have parsed |get| or |set|. Look for an accessor property
            // name next.
            TokenKind tt;
            if (!tokenStream.getToken(&tt, TokenStream::KeywordIsName))
                return null();
            if (tt == TOK_NAME) {
                atom = tokenStream.currentName();
                propname = handler.newObjectLiteralPropertyName(atom, pos());
                if (!propname)
                    return null();
            } else if (tt == TOK_STRING) {
                atom = tokenStream.currentToken().atom();

                uint32_t index;
                if (atom->isIndex(&index)) {
                    propname = handler.newNumber(index, NoDecimal, pos());
                    if (!propname)
                        return null();
                    atom = DoubleToAtom(context, index);
                    if (!atom)
                        return null();
                } else {
                    propname = stringLiteral();
                    if (!propname)
                        return null();
                }
            } else if (tt == TOK_NUMBER) {
                atom = DoubleToAtom(context, tokenStream.currentToken().number());
                if (!atom)
                    return null();
                propname = newNumber(tokenStream.currentToken());
                if (!propname)
                    return null();
            } else if (tt == TOK_LB) {
                propname = computedPropertyName(yieldHandling, propList);
                if (!propname)
                    return null();
            } else {
                // Not an accessor property after all.
                tokenStream.ungetToken();
                propname = handler.newObjectLiteralPropertyName(atom, pos());
                if (!propname)
                    return null();
                op = JSOP_INITPROP;
                break;
            }

            MOZ_ASSERT(op == JSOP_INITPROP_GETTER || op == JSOP_INITPROP_SETTER);
            break;
          }

          case TOK_STRING: {
            atom = tokenStream.currentToken().atom();
            uint32_t index;
            if (atom->isIndex(&index)) {
                propname = handler.newNumber(index, NoDecimal, pos());
                if (!propname)
                    return null();
            } else {
                propname = stringLiteral();
                if (!propname)
                    return null();
            }
            break;
          }

          default:
            // There is never a case in which |static *(| can make a meaningful method definition.
            if (isStatic && !isGenerator) {
                // Turns out it wasn't static. Put it back and pretend it was a name all along.
                isStatic = false;
                tokenStream.ungetToken();
                atom = tokenStream.currentName();
                propname = handler.newObjectLiteralPropertyName(atom->asPropertyName(), pos());
                if (!propname)
                    return null();
            } else {
                report(ParseError, false, null(), JSMSG_BAD_PROP_ID);
                return null();
            }
        }

        if (IsClassBody(type)) {
            if (!isStatic && atom == context->names().constructor) {
                if (isGenerator || op != JSOP_INITPROP) {
                    report(ParseError, false, propname, JSMSG_BAD_METHOD_DEF);
                    return null();
                }
                if (seenConstructor) {
                    report(ParseError, false, propname, JSMSG_DUPLICATE_PROPERTY, "constructor");
                    return null();
                }
                seenConstructor = true;
                isConstructor = true;
            } else if (isStatic && atom == context->names().prototype) {
                report(ParseError, false, propname, JSMSG_BAD_METHOD_DEF);
                return null();
            }
        }

        if (op == JSOP_INITPROP) {
            TokenKind tt;
            if (!tokenStream.getToken(&tt))
                return null();

            if (tt == TOK_COLON) {
                if (IsClassBody(type)) {
                    report(ParseError, false, null(), JSMSG_BAD_METHOD_DEF);
                    return null();
                }
                if (isGenerator) {
                    report(ParseError, false, null(), JSMSG_BAD_PROP_ID);
                    return null();
                }

                Node propexpr = assignExpr(InAllowed, yieldHandling);
                if (!propexpr)
                    return null();

                if (foldConstants && !FoldConstants(context, &propexpr, this))
                    return null();

                if (atom == context->names().proto) {
                    if (seenPrototypeMutation) {
                        report(ParseError, false, propname, JSMSG_DUPLICATE_PROPERTY, "__proto__");
                        return null();
                    }
                    seenPrototypeMutation = true;

                    // Note: this occurs *only* if we observe TOK_COLON!  Only
                    // __proto__: v mutates [[Prototype]].  Getters, setters,
                    // method/generator definitions, computed property name
                    // versions of all of these, and shorthands do not.
                    uint32_t begin = handler.getPosition(propname).begin;
                    if (!handler.addPrototypeMutation(propList, begin, propexpr))
                        return null();
                } else {
                    if (!handler.isConstant(propexpr))
                        handler.setListFlag(propList, PNX_NONCONST);

                    if (!handler.addPropertyDefinition(propList, propname, propexpr))
                        return null();
                }
            } else if (ltok == TOK_NAME && (tt == TOK_COMMA || tt == TOK_RC)) {
                /*
                 * Support, e.g., |var {x, y} = o| as destructuring shorthand
                 * for |var {x: x, y: y} = o|, per proposed JS2/ES4 for JS1.8.
                 */
                if (IsClassBody(type)) {
                    report(ParseError, false, null(), JSMSG_BAD_METHOD_DEF);
                    return null();
                }
                if (isGenerator) {
                    report(ParseError, false, null(), JSMSG_BAD_PROP_ID);
                    return null();
                }

                tokenStream.ungetToken();
                if (!tokenStream.checkForKeyword(atom, nullptr))
                    return null();

                Node nameExpr = identifierName(yieldHandling);
                if (!nameExpr)
                    return null();

                if (!handler.addShorthand(propList, propname, nameExpr))
                    return null();
            } else if (tt == TOK_LP) {
                tokenStream.ungetToken();
                if (!methodDefinition(yieldHandling, type, propList, propname,
                                      isConstructor ? type == DerivedClassBody ? DerivedClassConstructor
                                                                               : ClassConstructor
                                                    : Method,
                                      isGenerator ? StarGenerator : NotGenerator, isStatic, op))
                {
                    return null();
                }
            } else {
                report(ParseError, false, null(), JSMSG_COLON_AFTER_ID);
                return null();
            }
        } else {
            if (!methodDefinition(yieldHandling, type, propList, propname,
                                  op == JSOP_INITPROP_GETTER ? Getter : Setter, NotGenerator,
                                  isStatic, op))
            {
                return null();
            }
        }

        if (type == ObjectLiteral) {
            TokenKind tt;
            if (!tokenStream.getToken(&tt))
                return null();
            if (tt == TOK_RC)
                break;
            if (tt != TOK_COMMA) {
                report(ParseError, false, null(), JSMSG_CURLY_AFTER_LIST);
                return null();
            }
        }
    }

    // Default constructors not yet implemented. See bug 1105463
    if (IsClassBody(type) && !seenConstructor) {
        report(ParseError, false, null(), JSMSG_NO_CLASS_CONSTRUCTOR);
        return null();
    }

    handler.setEndPosition(propList, pos().end);
    return propList;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::methodDefinition(YieldHandling yieldHandling, PropListType listType,
                                       Node propList, Node propname, FunctionSyntaxKind kind,
                                       GeneratorKind generatorKind, bool isStatic, JSOp op)
{
    MOZ_ASSERT(kind == Method || kind == ClassConstructor || kind == DerivedClassConstructor ||
               kind == Getter || kind == Setter);
    /* NB: Getter function in { get x(){} } is unnamed. */
    RootedPropertyName funName(context);
    if ((kind == Method || kind == ClassConstructor || kind == DerivedClassConstructor) &&
        tokenStream.isCurrentTokenType(TOK_NAME))
    {
        funName = tokenStream.currentName();
    } else {
        funName = nullptr;
    }

    Node fn = functionDef(InAllowed, yieldHandling, funName, kind, generatorKind);
    if (!fn)
        return false;

    if (IsClassBody(listType))
        return handler.addClassMethodDefinition(propList, propname, fn, op, isStatic);

    MOZ_ASSERT(listType == ObjectLiteral);
    return handler.addObjectMethodDefinition(propList, propname, fn, op);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::tryNewTarget(Node &newTarget)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_NEW));

    uint32_t begin = pos().begin;
    newTarget = null();

    // |new| expects to look for an operand, so we will honor that.
    TokenKind next;
    if (!tokenStream.getToken(&next, TokenStream::Operand))
        return false;

    // Don't unget the token, since lookahead cannot handle someone calling
    // getToken() with a different modifier. Callers should inspect currentToken().
    if (next != TOK_DOT)
        return true;

    if (!tokenStream.getToken(&next))
        return false;
    if (next != TOK_NAME || tokenStream.currentName() != context->names().target) {
        report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
               "target", TokenKindToDesc(next));
        return false;
    }

    if (!checkAllowedNestedSyntax(SharedContext::AllowedSyntax::NewTarget)) {
        reportWithOffset(ParseError, false, begin, JSMSG_BAD_NEWTARGET);
        return false;
    }

    newTarget = handler.newNewTarget(TokenPos(begin, pos().end));
    return !!newTarget;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::primaryExpr(YieldHandling yieldHandling, TokenKind tt,
                                  InvokedPrediction invoked)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(tt));
    JS_CHECK_RECURSION(context, return null());

    switch (tt) {
      case TOK_FUNCTION:
        return functionExpr(invoked);

      case TOK_CLASS:
        return classDefinition(yieldHandling, ClassExpression, NameRequired);

      case TOK_LB:
        return arrayInitializer(yieldHandling);

      case TOK_LC:
        return propertyList(yieldHandling, ObjectLiteral);

      case TOK_LP: {
        TokenKind next;
        if (!tokenStream.peekToken(&next, TokenStream::Operand))
            return null();
        if (next != TOK_RP)
            return parenExprOrGeneratorComprehension(yieldHandling);

        // Not valid expression syntax, but this is valid in an arrow function
        // with no params: `() => body`.
        tokenStream.consumeKnownToken(next);

        if (!tokenStream.peekToken(&next))
            return null();
        if (next != TOK_ARROW) {
            report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                   "expression", TokenKindToDesc(TOK_RP));
            return null();
        }

        // Now just return something that will allow parsing to continue.
        // It doesn't matter what; when we reach the =>, we will rewind and
        // reparse the whole arrow function. See Parser::assignExpr.
        return handler.newNullLiteral(pos());
      }

      case TOK_TEMPLATE_HEAD:
        return templateLiteral(yieldHandling);

      case TOK_NO_SUBS_TEMPLATE:
        return noSubstitutionTemplate();

      case TOK_STRING:
        return stringLiteral();

      case TOK_YIELD:
        if (!checkYieldNameValidity())
            return null();
        // Fall through.
      case TOK_NAME:
        return identifierName(yieldHandling);

      case TOK_REGEXP:
        return newRegExp();

      case TOK_NUMBER:
        return newNumber(tokenStream.currentToken());

      case TOK_TRUE:
        return handler.newBooleanLiteral(true, pos());
      case TOK_FALSE:
        return handler.newBooleanLiteral(false, pos());
      case TOK_THIS:
        if (pc->sc->isFunctionBox())
            pc->sc->asFunctionBox()->usesThis = true;
        return handler.newThisLiteral(pos());
      case TOK_NULL:
        return handler.newNullLiteral(pos());

      case TOK_TRIPLEDOT: {
        TokenKind next;

        // This isn't valid expression syntax, but it's valid in an arrow
        // function as a trailing rest param: `(a, b, ...rest) => body`.  Check
        // for a name, closing parenthesis, and arrow, and allow it only if all
        // are present.
        if (!tokenStream.getToken(&next))
            return null();
        // FIXME: This fails to handle a rest parameter named |yield| correctly
        //        outside of generators: |var f = (...yield) => 42;| should be
        //        valid code!  When this is fixed, make sure to consult both
        //        |yieldHandling| and |checkYieldNameValidity| for correctness
        //        until legacy generator syntax is removed.
        if (next != TOK_NAME) {
            report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                   "rest argument name", TokenKindToDesc(next));
            return null();
        }

        if (!tokenStream.getToken(&next))
            return null();
        if (next != TOK_RP) {
            report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                   "closing parenthesis", TokenKindToDesc(next));
            return null();
        }

        if (!tokenStream.peekTokenSameLine(&next))
            return null();
        if (next != TOK_ARROW) {
            report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                   "'=>' after argument list", TokenKindToDesc(next));
            return null();
        }

        tokenStream.ungetToken();  // put back right paren

        // Return an arbitrary expression node. See case TOK_RP above.
        return handler.newNullLiteral(pos());
      }

      default:
        report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
               "expression", TokenKindToDesc(tt));
        return null();
    }
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::parenExprOrGeneratorComprehension(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LP));
    uint32_t begin = pos().begin;
    uint32_t startYieldOffset = pc->lastYieldOffset;

    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_FOR, TokenStream::Operand))
        return null();
    if (matched)
        return generatorComprehension(begin);

    Node pn = expr(InAllowed, yieldHandling, PredictInvoked);
    if (!pn)
        return null();

#if JS_HAS_GENERATOR_EXPRS
    if (!tokenStream.matchToken(&matched, TOK_FOR))
        return null();
    if (matched) {
        if (pc->lastYieldOffset != startYieldOffset) {
            reportWithOffset(ParseError, false, pc->lastYieldOffset,
                             JSMSG_BAD_GENEXP_BODY, js_yield_str);
            return null();
        }
        if (handler.isUnparenthesizedCommaExpression(pn)) {
            report(ParseError, false, null(),
                   JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
            return null();
        }
        pn = legacyGeneratorExpr(pn);
        if (!pn)
            return null();
        handler.setBeginPosition(pn, begin);
        TokenKind tt;
        if (!tokenStream.getToken(&tt))
            return null();
        if (tt != TOK_RP) {
            report(ParseError, false, null(),
                   JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
            return null();
        }
        handler.setEndPosition(pn, pos().end);
        return handler.parenthesize(pn);
    }
#endif /* JS_HAS_GENERATOR_EXPRS */

    pn = handler.parenthesize(pn);

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);

    return pn;
}

// Legacy generator comprehensions can sometimes appear without parentheses.
// For example:
//
//   foo(x for (x in bar))
//
// In this case the parens are part of the call, and not part of the generator
// comprehension.  This can happen in these contexts:
//
//   if (_)
//   while (_) {}
//   do {} while (_)
//   switch (_) {}
//   with (_) {}
//   foo(_) // must be first and only argument
//
// This is not the case for ES6 generator comprehensions; they must always be in
// parentheses.

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::exprInParens(InHandling inHandling, YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LP));
    uint32_t begin = pos().begin;
    uint32_t startYieldOffset = pc->lastYieldOffset;

    Node pn = expr(inHandling, yieldHandling, PredictInvoked);
    if (!pn)
        return null();

#if JS_HAS_GENERATOR_EXPRS
    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_FOR))
        return null();
    if (matched) {
        if (pc->lastYieldOffset != startYieldOffset) {
            reportWithOffset(ParseError, false, pc->lastYieldOffset,
                             JSMSG_BAD_GENEXP_BODY, js_yield_str);
            return null();
        }
        if (handler.isUnparenthesizedCommaExpression(pn)) {
            report(ParseError, false, null(),
                   JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
            return null();
        }
        pn = legacyGeneratorExpr(pn);
        if (!pn)
            return null();
        handler.setBeginPosition(pn, begin);
    }
#endif /* JS_HAS_GENERATOR_EXPRS */

    return pn;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::addTelemetry(JSCompartment::DeprecatedLanguageExtension e)
{
    JSContext* cx = context->maybeJSContext();
    if (!cx)
        return;
    cx->compartment()->addTelemetry(getFilename(), e);
}

template class Parser<FullParseHandler>;
template class Parser<SyntaxParseHandler>;

} /* namespace frontend */
} /* namespace js */
