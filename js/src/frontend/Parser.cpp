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

#include "frontend/BytecodeCompiler.h"
#include "frontend/FoldConstants.h"
#include "frontend/ParseMaps.h"
#include "frontend/TokenStream.h"
#include "jit/AsmJS.h"
#include "vm/Shape.h"

#include "jsatominlines.h"
#include "jsscriptinlines.h"

#include "frontend/ParseNode-inl.h"

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


/*
 * Insist that the next token be of type tt, or report errno and return null.
 * NB: this macro uses cx and ts from its lexical environment.
 */
#define MUST_MATCH_TOKEN(tt, errno)                                                         \
    JS_BEGIN_MACRO                                                                          \
        if (tokenStream.getToken() != tt) {                                                 \
            report(ParseError, false, null(), errno);                                       \
            return null();                                                                  \
        }                                                                                   \
    JS_END_MACRO

static const unsigned BlockIdLimit = 1 << ParseNode::NumBlockIdBits;

template <typename ParseHandler>
bool
GenerateBlockId(TokenStream &ts, ParseContext<ParseHandler> *pc, uint32_t &blockid)
{
    if (pc->blockidGen == BlockIdLimit) {
        ts.reportError(JSMSG_NEED_DIET, "program");
        return false;
    }
    JS_ASSERT(pc->blockidGen < BlockIdLimit);
    blockid = pc->blockidGen++;
    return true;
}

template bool
GenerateBlockId(TokenStream &ts, ParseContext<SyntaxParseHandler> *pc, uint32_t &blockid);

template bool
GenerateBlockId(TokenStream &ts, ParseContext<FullParseHandler> *pc, uint32_t &blockid);

template <typename ParseHandler>
static void
PushStatementPC(ParseContext<ParseHandler> *pc, StmtInfoPC *stmt, StmtType type)
{
    stmt->blockid = pc->blockid();
    PushStatement(pc, stmt, type);
}

// See comment on member function declaration.
template <>
bool
ParseContext<FullParseHandler>::define(TokenStream &ts,
                                       HandlePropertyName name, ParseNode *pn, Definition::Kind kind)
{
    JS_ASSERT(!pn->isUsed());
    JS_ASSERT_IF(pn->isDefn(), pn->isPlaceholder());

    Definition *prevDef = nullptr;
    if (kind == Definition::LET)
        prevDef = decls_.lookupFirst(name);
    else
        JS_ASSERT(!decls_.lookupFirst(name));

    if (!prevDef)
        prevDef = lexdeps.lookupDefn<FullParseHandler>(name);

    if (prevDef) {
        ParseNode **pnup = &prevDef->dn_uses;
        ParseNode *pnu;
        unsigned start = (kind == Definition::LET) ? pn->pn_blockid : bodyid;

        while ((pnu = *pnup) != nullptr && pnu->pn_blockid >= start) {
            JS_ASSERT(pnu->pn_blockid >= bodyid);
            JS_ASSERT(pnu->isUsed());
            pnu->pn_lexdef = (Definition *) pn;
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

    JS_ASSERT_IF(kind != Definition::LET, !lexdeps->lookup(name));
    pn->setDefn(true);
    pn->pn_dflags &= ~PND_PLACEHOLDER;
    if (kind == Definition::CONST)
        pn->pn_dflags |= PND_CONST;

    Definition *dn = (Definition *)pn;
    switch (kind) {
      case Definition::ARG:
        JS_ASSERT(sc->isFunctionBox());
        dn->setOp((js_CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETARG : JSOP_GETARG);
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

      case Definition::CONST:
      case Definition::VAR:
        if (sc->isFunctionBox()) {
            dn->setOp((js_CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETLOCAL : JSOP_GETLOCAL);
            dn->pn_dflags |= PND_BOUND;
            if (!dn->pn_cookie.set(ts, staticLevel, vars_.length()))
                return false;
            if (!vars_.append(dn))
                return false;
            if (vars_.length() >= LOCALNO_LIMIT) {
                ts.reportError(JSMSG_TOO_MANY_LOCALS);
                return false;
            }
        }
        if (!decls_.addUnique(name, dn))
            return false;
        break;

      case Definition::LET:
        dn->setOp((js_CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETLOCAL : JSOP_GETLOCAL);
        dn->pn_dflags |= (PND_LET | PND_BOUND);
        JS_ASSERT(dn->pn_cookie.level() == staticLevel); /* see bindLet */
        if (!decls_.addShadow(name, dn))
            return false;
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("unexpected kind");
    }

    return true;
}

template <>
bool
ParseContext<SyntaxParseHandler>::define(TokenStream &ts, HandlePropertyName name, Node pn,
                                         Definition::Kind kind)
{
    JS_ASSERT(!decls_.lookupFirst(name));

    if (lexdeps.lookupDefn<SyntaxParseHandler>(name))
        lexdeps->remove(name);

    // Keep track of the number of arguments in args_, for fun->nargs.
    if (kind == Definition::ARG) {
        if (!args_.append((Definition *) nullptr))
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
    JS_ASSERT(decls_.lookupFirst(name) == prevDecl);
    decls_.remove(name);
}

template <typename ParseHandler>
void
ParseContext<ParseHandler>::updateDecl(JSAtom *atom, Node pn)
{
    Definition *oldDecl = decls_.lookupFirst(atom);

    pn->setDefn(true);
    Definition *newDecl = (Definition *)pn;
    decls_.updateFirst(atom, newDecl);

    if (!sc->isFunctionBox()) {
        JS_ASSERT(newDecl->isFreeVar());
        return;
    }

    JS_ASSERT(oldDecl->isBound());
    JS_ASSERT(!oldDecl->pn_cookie.isFree());
    newDecl->pn_cookie = oldDecl->pn_cookie;
    newDecl->pn_dflags |= PND_BOUND;
    if (IsArgOp(oldDecl->getOp())) {
        newDecl->setOp(JSOP_GETARG);
        JS_ASSERT(args_[oldDecl->pn_cookie.slot()] == oldDecl);
        args_[oldDecl->pn_cookie.slot()] = newDecl;
    } else {
        JS_ASSERT(IsLocalOp(oldDecl->getOp()));
        newDecl->setOp(JSOP_GETLOCAL);
        JS_ASSERT(vars_[oldDecl->pn_cookie.slot()] == oldDecl);
        vars_[oldDecl->pn_cookie.slot()] = newDecl;
    }
}

template <typename ParseHandler>
void
ParseContext<ParseHandler>::popLetDecl(JSAtom *atom)
{
    JS_ASSERT(ParseHandler::getDefinitionKind(decls_.lookupFirst(atom)) == Definition::LET);
    decls_.remove(atom);
}

template <typename ParseHandler>
static void
AppendPackedBindings(const ParseContext<ParseHandler> *pc, const DeclVector &vec, Binding *dst)
{
    for (size_t i = 0; i < vec.length(); ++i, ++dst) {
        Definition *dn = vec[i];
        PropertyName *name = dn->name();

        Binding::Kind kind;
        switch (dn->kind()) {
          case Definition::VAR:
            kind = Binding::VARIABLE;
            break;
          case Definition::CONST:
            kind = Binding::CONSTANT;
            break;
          case Definition::ARG:
            kind = Binding::ARGUMENT;
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected dn->kind");
        }

        /*
         * Bindings::init does not check for duplicates so we must ensure that
         * only one binding with a given name is marked aliased. pc->decls
         * maintains the canonical definition for each name, so use that.
         */
        JS_ASSERT_IF(dn->isClosed(), pc->decls().lookupFirst(name) == dn);
        bool aliased = dn->isClosed() ||
                       (pc->sc->allLocalsAliased() &&
                        pc->decls().lookupFirst(name) == dn);

        *dst = Binding(name, kind, aliased);
    }
}

template <typename ParseHandler>
bool
ParseContext<ParseHandler>::generateFunctionBindings(ExclusiveContext *cx, TokenStream &ts,
                                                     LifoAlloc &alloc,
                                                     InternalHandle<Bindings*> bindings) const
{
    JS_ASSERT(sc->isFunctionBox());
    JS_ASSERT(args_.length() < ARGNO_LIMIT);
    JS_ASSERT(vars_.length() < LOCALNO_LIMIT);

    /*
     * Avoid pathological edge cases by explicitly limiting the total number of
     * bindings to what will fit in a uint32_t.
     */
    if (UINT32_MAX - args_.length() <= vars_.length())
        return ts.reportError(JSMSG_TOO_MANY_LOCALS);

    uint32_t count = args_.length() + vars_.length();
    Binding *packedBindings = alloc.newArrayUninitialized<Binding>(count);
    if (!packedBindings) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    AppendPackedBindings(this, args_, packedBindings);
    AppendPackedBindings(this, vars_, packedBindings + args_.length());

    return Bindings::initWithTemporaryStorage(cx, bindings, args_.length(), vars_.length(),
                                              packedBindings, blockScopeDepth);
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
Parser<ParseHandler>::Parser(ExclusiveContext *cx, LifoAlloc *alloc,
                             const ReadOnlyCompileOptions &options,
                             const jschar *chars, size_t length, bool foldConstants,
                             Parser<SyntaxParseHandler> *syntaxParser,
                             LazyScript *lazyOuterFunction)
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
    abortedSyntaxParse(false),
    isUnexpectedEOF_(false),
    handler(cx, *alloc, tokenStream, foldConstants, syntaxParser, lazyOuterFunction)
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

template <typename ParseHandler>
Parser<ParseHandler>::~Parser()
{
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
ObjectBox *
Parser<ParseHandler>::newObjectBox(JSObject *obj)
{
    JS_ASSERT(obj && !IsPoisonedPtr(obj));

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */

    ObjectBox *objbox = alloc.new_<ObjectBox>(obj, traceListHead);
    if (!objbox) {
        js_ReportOutOfMemory(context);
        return nullptr;
    }

    traceListHead = objbox;

    return objbox;
}

template <typename ParseHandler>
FunctionBox::FunctionBox(ExclusiveContext *cx, ObjectBox* traceListHead, JSFunction *fun,
                         ParseContext<ParseHandler> *outerpc, Directives directives,
                         bool extraWarnings, GeneratorKind generatorKind)
  : ObjectBox(fun, traceListHead),
    SharedContext(cx, directives, extraWarnings),
    bindings(),
    bufStart(0),
    bufEnd(0),
    length(0),
    generatorKindBits_(GeneratorKindAsBits(generatorKind)),
    inWith(false),                  // initialized below
    inGenexpLambda(false),
    hasDestructuringArgs(false),
    useAsm(directives.asmJS()),
    insideUseAsm(outerpc && outerpc->useAsmOrInsideUseAsm()),
    usesArguments(false),
    usesApply(false),
    funCxFlags()
{
    // Functions created at parse time may be set singleton after parsing and
    // baked into JIT code, so they must be allocated tenured. They are held by
    // the JSScript so cannot be collected during a minor GC anyway.
    JS_ASSERT(fun->isTenured());

    if (!outerpc) {
        inWith = false;

    } else if (outerpc->parsingWith) {
        // This covers cases that don't involve eval().  For example:
        //
        //   with (o) { (function() { g(); })(); }
        //
        // In this case, |outerpc| corresponds to global code, and
        // outerpc->parsingWith is true.
        inWith = true;

    } else if (outerpc->sc->isGlobalSharedContext()) {
        // This covers the case where a function is nested within an eval()
        // within a |with| statement.
        //
        //   with (o) { eval("(function() { g(); })();"); }
        //
        // In this case, |outerpc| corresponds to the eval(),
        // outerpc->parsingWith is false because the eval() breaks the
        // ParseContext chain, and |parent| is nullptr (again because of the
        // eval(), so we have to look at |outerpc|'s scopeChain.
        //
        JSObject *scope = outerpc->sc->asGlobalSharedContext()->scopeChain();
        while (scope) {
            if (scope->is<DynamicWithObject>())
                inWith = true;
            scope = scope->enclosingScope();
        }
    } else if (outerpc->sc->isFunctionBox()) {
        // This is like the above case, but for more deeply nested functions.
        // For example:
        //
        //   with (o) { eval("(function() { (function() { g(); })(); })();"); } }
        //
        // In this case, the inner anonymous function needs to inherit the
        // setting of |inWith| from the outer one.
        FunctionBox *parent = outerpc->sc->asFunctionBox();
        if (parent && parent->inWith)
            inWith = true;
    }
}

template <typename ParseHandler>
FunctionBox *
Parser<ParseHandler>::newFunctionBox(Node fn, JSFunction *fun, ParseContext<ParseHandler> *outerpc,
                                     Directives inheritedDirectives, GeneratorKind generatorKind)
{
    JS_ASSERT(fun && !IsPoisonedPtr(fun));

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */
    FunctionBox *funbox =
        alloc.new_<FunctionBox>(context, traceListHead, fun, outerpc,
                                inheritedDirectives, options().extraWarningsOption,
                                generatorKind);
    if (!funbox) {
        js_ReportOutOfMemory(context);
        return nullptr;
    }

    traceListHead = funbox;
    if (fn)
        handler.setFunctionBox(fn, funbox);

    return funbox;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::trace(JSTracer *trc)
{
    traceListHead->trace(trc);
}

void
MarkParser(JSTracer *trc, AutoGCRooter *parser)
{
    static_cast<Parser<FullParseHandler> *>(parser)->trace(trc);
}

/*
 * Parse a top-level JS script.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::parse(JSObject *chain)
{
    /*
     * Protect atoms from being collected by a GC activation, which might
     * - nest on this thread due to out of memory (the so-called "last ditch"
     *   GC attempted within js_NewGCThing), or
     * - run for any reason on another thread if this thread is suspended on
     *   an object lock before it finishes generating bytecode into a script
     *   protected from the GC by a root or a stack frame reference.
     */
    Directives directives(options().strictOption);
    GlobalSharedContext globalsc(context, chain, directives, options().extraWarningsOption);
    ParseContext<ParseHandler> globalpc(this, /* parent = */ nullptr, ParseHandler::null(),
                                        &globalsc, /* newDirectives = */ nullptr,
                                        /* staticLevel = */ 0, /* bodyid = */ 0,
                                        /* blockScopeDepth = */ 0);
    if (!globalpc.init(tokenStream))
        return null();

    Node pn = statements();
    if (pn) {
        if (!tokenStream.matchToken(TOK_EOF)) {
            report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
            return null();
        }
        if (foldConstants) {
            if (!FoldConstants(context, &pn, this))
                return null();
        }
    }
    return pn;
}

/*
 * Insist on a final return before control flows out of pn.  Try to be a bit
 * smart about loops: do {...; return e2;} while(0) at the end of a function
 * that contains an early return e1 will get a strict warning.  Similarly for
 * iloops: while (true){...} is treated as though ... returns.
 */
enum {
    ENDS_IN_OTHER = 0,
    ENDS_IN_RETURN = 1,
    ENDS_IN_BREAK = 2
};

static int
HasFinalReturn(ParseNode *pn)
{
    ParseNode *pn2, *pn3;
    unsigned rv, rv2, hasDefault;

    switch (pn->getKind()) {
      case PNK_STATEMENTLIST:
        if (!pn->pn_head)
            return ENDS_IN_OTHER;
        return HasFinalReturn(pn->last());

      case PNK_IF:
        if (!pn->pn_kid3)
            return ENDS_IN_OTHER;
        return HasFinalReturn(pn->pn_kid2) & HasFinalReturn(pn->pn_kid3);

      case PNK_WHILE:
        pn2 = pn->pn_left;
        if (pn2->isKind(PNK_TRUE))
            return ENDS_IN_RETURN;
        if (pn2->isKind(PNK_NUMBER) && pn2->pn_dval)
            return ENDS_IN_RETURN;
        return ENDS_IN_OTHER;

      case PNK_DOWHILE:
        pn2 = pn->pn_right;
        if (pn2->isKind(PNK_FALSE))
            return HasFinalReturn(pn->pn_left);
        if (pn2->isKind(PNK_TRUE))
            return ENDS_IN_RETURN;
        if (pn2->isKind(PNK_NUMBER)) {
            if (pn2->pn_dval == 0)
                return HasFinalReturn(pn->pn_left);
            return ENDS_IN_RETURN;
        }
        return ENDS_IN_OTHER;

      case PNK_FOR:
        pn2 = pn->pn_left;
        if (pn2->isArity(PN_TERNARY) && !pn2->pn_kid2)
            return ENDS_IN_RETURN;
        return ENDS_IN_OTHER;

      case PNK_SWITCH:
        rv = ENDS_IN_RETURN;
        hasDefault = ENDS_IN_OTHER;
        pn2 = pn->pn_right;
        if (pn2->isKind(PNK_LEXICALSCOPE))
            pn2 = pn2->expr();
        for (pn2 = pn2->pn_head; rv && pn2; pn2 = pn2->pn_next) {
            if (pn2->isKind(PNK_DEFAULT))
                hasDefault = ENDS_IN_RETURN;
            pn3 = pn2->pn_right;
            JS_ASSERT(pn3->isKind(PNK_STATEMENTLIST));
            if (pn3->pn_head) {
                rv2 = HasFinalReturn(pn3->last());
                if (rv2 == ENDS_IN_OTHER && pn2->pn_next)
                    /* Falling through to next case or default. */;
                else
                    rv &= rv2;
            }
        }
        /* If a final switch has no default case, we judge it harshly. */
        rv &= hasDefault;
        return rv;

      case PNK_BREAK:
        return ENDS_IN_BREAK;

      case PNK_WITH:
        return HasFinalReturn(pn->pn_right);

      case PNK_RETURN:
        return ENDS_IN_RETURN;

      case PNK_COLON:
      case PNK_LEXICALSCOPE:
        return HasFinalReturn(pn->expr());

      case PNK_THROW:
        return ENDS_IN_RETURN;

      case PNK_TRY:
        /* If we have a finally block that returns, we are done. */
        if (pn->pn_kid3) {
            rv = HasFinalReturn(pn->pn_kid3);
            if (rv == ENDS_IN_RETURN)
                return rv;
        }

        /* Else check the try block and any and all catch statements. */
        rv = HasFinalReturn(pn->pn_kid1);
        if (pn->pn_kid2) {
            JS_ASSERT(pn->pn_kid2->isArity(PN_LIST));
            for (pn2 = pn->pn_kid2->pn_head; pn2; pn2 = pn2->pn_next)
                rv &= HasFinalReturn(pn2);
        }
        return rv;

      case PNK_CATCH:
        /* Check this catch block's body. */
        return HasFinalReturn(pn->pn_kid3);

      case PNK_LET:
        /* Non-binary let statements are let declarations. */
        if (!pn->isArity(PN_BINARY))
            return ENDS_IN_OTHER;
        return HasFinalReturn(pn->pn_right);

      default:
        return ENDS_IN_OTHER;
    }
}

static int
HasFinalReturn(SyntaxParseHandler::Node pn)
{
    return ENDS_IN_RETURN;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportBadReturn(Node pn, ParseReportKind kind,
                                      unsigned errnum, unsigned anonerrnum)
{
    JSAutoByteString name;
    JSAtom *atom = pc->sc->asFunctionBox()->function()->atom();
    if (atom) {
        if (!AtomToPrintableString(context, atom, &name))
            return false;
    } else {
        errnum = anonerrnum;
    }
    return report(kind, pc->sc->strict, pn, errnum, name.ptr());
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkFinalReturn(Node pn)
{
    JS_ASSERT(pc->sc->isFunctionBox());
    return HasFinalReturn(pn) == ENDS_IN_RETURN ||
           reportBadReturn(pn, ParseExtraWarning,
                           JSMSG_NO_RETURN_VALUE, JSMSG_ANON_NO_RETURN_VALUE);
}

/*
 * Check that assigning to lhs is permitted.  Assigning to 'eval' or
 * 'arguments' is banned in strict mode and in destructuring assignment.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::checkStrictAssignment(Node lhs, AssignmentFlavor flavor)
{
    if (!pc->sc->needStrictChecks() && flavor != KeyedDestructuringAssignment)
        return true;

    JSAtom *atom = handler.isName(lhs);
    if (!atom)
        return true;

    if (atom == context->names().eval || atom == context->names().arguments) {
        JSAutoByteString name;
        if (!AtomToPrintableString(context, atom, &name))
            return false;

        ParseReportKind kind;
        unsigned errnum;
        if (pc->sc->strict || flavor != KeyedDestructuringAssignment) {
            kind = ParseStrictError;
            errnum = JSMSG_BAD_STRICT_ASSIGN;
        } else {
            kind = ParseError;
            errnum = JSMSG_BAD_DESTRUCT_ASSIGN;
        }
        if (!report(kind, pc->sc->strict, lhs, errnum, name.ptr()))
            return false;
    }
    return true;
}

/*
 * Check that it is permitted to introduce a binding for atom.  Strict mode
 * forbids introducing new definitions for 'eval', 'arguments', or for any
 * strict mode reserved keyword.  Use pn for reporting error locations, or use
 * pc's token stream if pn is nullptr.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::checkStrictBinding(PropertyName *name, Node pn)
{
    if (!pc->sc->needStrictChecks())
        return true;

    if (name == context->names().eval || name == context->names().arguments || IsKeyword(name)) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(context, name, &bytes))
            return false;
        return report(ParseStrictError, pc->sc->strict, pn,
                      JSMSG_BAD_BINDING, bytes.ptr());
    }

    return true;
}

template <>
ParseNode *
Parser<FullParseHandler>::standaloneFunctionBody(HandleFunction fun, const AutoNameVector &formals,
                                                 GeneratorKind generatorKind,
                                                 Directives inheritedDirectives,
                                                 Directives *newDirectives)
{
    Node fn = handler.newFunctionDefinition();
    if (!fn)
        return null();

    ParseNode *argsbody = ListNode::create(PNK_ARGSBODY, &handler);
    if (!argsbody)
        return null();
    argsbody->setOp(JSOP_NOP);
    argsbody->makeEmpty();
    fn->pn_body = argsbody;

    FunctionBox *funbox = newFunctionBox(fn, fun, /* outerpc = */ nullptr, inheritedDirectives,
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

    ParseNode *pn = functionBody(Statement, StatementListBody);
    if (!pn)
        return null();

    if (!tokenStream.matchToken(TOK_EOF)) {
        report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
        return null();
    }

    if (!FoldConstants(context, &pn, this))
        return null();

    InternalHandle<Bindings*> funboxBindings =
        InternalHandle<Bindings*>::fromMarkedLocation(&funbox->bindings);
    if (!funpc.generateFunctionBindings(context, tokenStream, alloc, funboxBindings))
        return null();

    JS_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
    fn->pn_body->append(pn);
    fn->pn_body->pn_pos = pn->pn_pos;
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
    if (FuncStmtSet *set = pc->funcStmts) {
        for (FuncStmtSet::Range r = set->all(); !r.empty(); r.popFront()) {
            PropertyName *name = r.front()->asPropertyName();
            if (Definition *dn = pc->decls().lookupFirst(name))
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
            Definition *dn = r.front().value().get<FullParseHandler>();
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
    Definition *maybeArgDef = pc->decls().lookupFirst(arguments);
    bool argumentsHasBinding = !!maybeArgDef;
    bool argumentsHasLocalBinding = maybeArgDef && maybeArgDef->kind() != Definition::ARG;
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
        ParseNode *pn = newName(arguments);
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
        FunctionBox *funbox = pc->sc->asFunctionBox();
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
                DefinitionList &dlist = r.front().value();
                for (DefinitionList::Range dr = dlist.all(); !dr.empty(); dr.popFront()) {
                    Definition *dn = dr.front<FullParseHandler>();
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
Parser<ParseHandler>::functionBody(FunctionSyntaxKind kind, FunctionBodyType type)
{
    JS_ASSERT(pc->sc->isFunctionBox());
    JS_ASSERT(!pc->funHasReturnExpr && !pc->funHasReturnVoid);

#ifdef DEBUG
    uint32_t startYieldOffset = pc->lastYieldOffset;
#endif

    Node pn;
    if (type == StatementListBody) {
        pn = statements();
        if (!pn)
            return null();
    } else {
        JS_ASSERT(type == ExpressionBody);
        JS_ASSERT(JS_HAS_EXPR_CLOSURES);

        Node kid = assignExpr();
        if (!kid)
            return null();

        pn = handler.newReturnStatement(kid, handler.getPosition(kid));
        if (!pn)
            return null();
    }

    switch (pc->generatorKind()) {
      case NotGenerator:
        JS_ASSERT(pc->lastYieldOffset == startYieldOffset);
        break;

      case LegacyGenerator:
        // FIXME: Catch these errors eagerly, in yieldExpression().
        JS_ASSERT(pc->lastYieldOffset != startYieldOffset);
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
        JS_ASSERT(kind != Arrow);
        JS_ASSERT(type == StatementListBody);
        break;
    }

    /* Check for falling off the end of a function that returns a value. */
    if (options().extraWarningsOption && pc->funHasReturnExpr && !checkFinalReturn(pn))
        return null();

    /* Define the 'arguments' binding if necessary. */
    if (!checkFunctionArguments())
        return null();

    return pn;
}

/* See comment for use in Parser::functionDef. */
template <>
bool
Parser<FullParseHandler>::makeDefIntoUse(Definition *dn, ParseNode *pn, JSAtom *atom)
{
    /* Turn pn into a definition. */
    pc->updateDecl(atom, pn);

    /* Change all uses of dn to be uses of pn. */
    for (ParseNode *pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
        JS_ASSERT(pnu->isUsed());
        JS_ASSERT(!pnu->isDefn());
        pnu->pn_lexdef = (Definition *) pn;
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
        JS_ASSERT(dn->functionIsHoisted());
        pn->dn_uses = dn->pn_link;
        handler.prepareNodeForMutation(dn);
        dn->setKind(PNK_NOP);
        dn->setArity(PN_NULLARY);
        return true;
    }

    /*
     * If dn is arg, or in [var, const, let] and has an initializer, then we
     * must rewrite it to be an assignment node, whose freshly allocated
     * left-hand side becomes a use of pn.
     */
    if (dn->canHaveInitializer()) {
        if (ParseNode *rhs = dn->expr()) {
            ParseNode *lhs = handler.makeAssignment(dn, rhs);
            if (!lhs)
                return false;
            pn->dn_uses = lhs;
            dn->pn_link = nullptr;
            dn = (Definition *) lhs;
        }
    }

    /* Turn dn into a use of pn. */
    JS_ASSERT(dn->isKind(PNK_NAME));
    JS_ASSERT(dn->isArity(PN_NAME));
    JS_ASSERT(dn->pn_atom == atom);
    dn->setOp((js_CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETNAME : JSOP_NAME);
    dn->setDefn(false);
    dn->setUsed(true);
    dn->pn_lexdef = (Definition *) pn;
    dn->pn_cookie.makeFree();
    dn->pn_dflags &= ~PND_BOUND;
    return true;
}

/*
 * Parameter block types for the several Binder functions.  We use a common
 * helper function signature in order to share code among destructuring and
 * simple variable declaration parsers.  In the destructuring case, the binder
 * function is called indirectly from the variable declaration parser by way
 * of CheckDestructuring and its friends.
 */

template <typename ParseHandler>
struct BindData
{
    explicit BindData(ExclusiveContext *cx) : let(cx) {}

    typedef bool
    (*Binder)(BindData *data, HandlePropertyName name, Parser<ParseHandler> *parser);

    /* name node for definition processing and error source coordinates */
    typename ParseHandler::Node pn;

    JSOp            op;         /* prolog bytecode or nop */
    Binder          binder;     /* binder, discriminates u */

    struct LetData {
        explicit LetData(ExclusiveContext *cx) : blockObj(cx) {}
        VarContext varContext;
        RootedStaticBlockObject blockObj;
        unsigned   overflow;
    } let;

    void initLet(VarContext varContext, StaticBlockObject &blockObj, unsigned overflow) {
        this->pn = ParseHandler::null();
        this->op = JSOP_NOP;
        this->binder = Parser<ParseHandler>::bindLet;
        this->let.varContext = varContext;
        this->let.blockObj = &blockObj;
        this->let.overflow = overflow;
    }

    void initVarOrConst(JSOp op) {
        this->op = op;
        this->binder = Parser<ParseHandler>::bindVarOrConst;
    }
};

template <typename ParseHandler>
JSFunction *
Parser<ParseHandler>::newFunction(GenericParseContext *pc, HandleAtom atom,
                                  FunctionSyntaxKind kind, JSObject *proto)
{
    JS_ASSERT_IF(kind == Statement, atom != nullptr);

    /*
     * Find the global compilation context in order to pre-set the newborn
     * function's parent slot to pc->sc->as<GlobalObject>()->scopeChain. If the
     * global context is a compile-and-go one, we leave the pre-set parent
     * intact; otherwise we clear parent and proto.
     */
    while (pc->parent)
        pc = pc->parent;

    RootedFunction fun(context);
    JSFunction::Flags flags = (kind == Expression)
                              ? JSFunction::INTERPRETED_LAMBDA
                              : (kind == Arrow)
                                ? JSFunction::INTERPRETED_LAMBDA_ARROW
                                : JSFunction::INTERPRETED;
    gc::AllocKind allocKind = JSFunction::FinalizeKind;
    if (kind == Arrow)
        allocKind = JSFunction::ExtendedFinalizeKind;
    fun = NewFunctionWithProto(context, NullPtr(), nullptr, 0, flags, NullPtr(), atom, proto,
                               allocKind, MaybeSingletonObject);
    if (!fun)
        return nullptr;
    if (options().selfHostingMode)
        fun->setIsSelfHostedBuiltin();
    return fun;
}

static bool
MatchOrInsertSemicolon(TokenStream &ts)
{
    TokenKind tt = ts.peekTokenSameLine(TokenStream::Operand);
    if (tt == TOK_ERROR)
        return false;
    if (tt != TOK_EOF && tt != TOK_EOL && tt != TOK_SEMI && tt != TOK_RC) {
        /* Advance the scanner for proper error location reporting. */
        ts.getToken(TokenStream::Operand);
        ts.reportError(JSMSG_SEMI_BEFORE_STMNT);
        return false;
    }
    (void) ts.matchToken(TOK_SEMI);
    return true;
}

template <typename ParseHandler>
typename ParseHandler::DefinitionNode
Parser<ParseHandler>::getOrCreateLexicalDependency(ParseContext<ParseHandler> *pc, JSAtom *atom)
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
ConvertDefinitionToNamedLambdaUse(TokenStream &ts, ParseContext<FullParseHandler> *pc,
                                  FunctionBox *funbox, Definition *dn)
{
    dn->setOp(JSOP_CALLEE);
    if (!dn->pn_cookie.set(ts, pc->staticLevel, 0))
        return false;
    dn->pn_dflags |= PND_BOUND;
    JS_ASSERT(dn->kind() == Definition::NAMED_LAMBDA);

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

/*
 * Beware: this function is called for functions nested in other functions or
 * global scripts but not for functions compiled through the Function
 * constructor or JSAPI. To always execute code when a function has finished
 * parsing, use Parser::functionBody.
 */
template <>
bool
Parser<FullParseHandler>::leaveFunction(ParseNode *fn, ParseContext<FullParseHandler> *outerpc,
                                        FunctionSyntaxKind kind)
{
    outerpc->blockidGen = pc->blockidGen;

    FunctionBox *funbox = fn->pn_funbox;
    JS_ASSERT(funbox == pc->sc->asFunctionBox());

    /* Propagate unresolved lexical names up to outerpc->lexdeps. */
    if (pc->lexdeps->count()) {
        for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
            JSAtom *atom = r.front().key();
            Definition *dn = r.front().value().get<FullParseHandler>();
            JS_ASSERT(dn->isPlaceholder());

            if (atom == funbox->function()->name() && kind == Expression) {
                if (!ConvertDefinitionToNamedLambdaUse(tokenStream, pc, funbox, dn))
                    return false;
                continue;
            }

            Definition *outer_dn = outerpc->decls().lookupFirst(atom);

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
                if (ParseNode *pnu = dn->dn_uses) {
                    while (true) {
                        pnu->pn_lexdef = outer_dn;
                        if (!pnu->pn_link)
                            break;
                        pnu = pnu->pn_link;
                    }
                    pnu->pn_link = outer_dn->dn_uses;
                    outer_dn->dn_uses = dn->dn_uses;
                    dn->dn_uses = nullptr;
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
Parser<SyntaxParseHandler>::leaveFunction(Node fn, ParseContext<SyntaxParseHandler> *outerpc,
                                          FunctionSyntaxKind kind)
{
    outerpc->blockidGen = pc->blockidGen;

    FunctionBox *funbox = pc->sc->asFunctionBox();
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
                                bool disallowDuplicateArgs, Node *duplicatedArg)
{
    SharedContext *sc = pc->sc;

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
            if (!report(ParseStrictError, pc->sc->strict, pn,
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
        JS_ASSERT(handler.getDefinitionKind(prevDecl) == Definition::ARG);
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
Parser<ParseHandler>::bindDestructuringArg(BindData<ParseHandler> *data,
                                           HandlePropertyName name, Parser<ParseHandler> *parser)
{
    ParseContext<ParseHandler> *pc = parser->pc;
    JS_ASSERT(pc->sc->isFunctionBox());

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
Parser<ParseHandler>::functionArguments(FunctionSyntaxKind kind, Node *listp, Node funcpn,
                                        bool *hasRest)
{
    FunctionBox *funbox = pc->sc->asFunctionBox();

    *hasRest = false;

    bool parenFreeArrow = false;
    if (kind == Arrow && tokenStream.peekToken() == TOK_NAME) {
        parenFreeArrow = true;
    } else {
        if (tokenStream.getToken() != TOK_LP) {
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

    if (parenFreeArrow || !tokenStream.matchToken(TOK_RP)) {
        bool hasDefaults = false;
        Node duplicatedArg = null();
        Node list = null();

        do {
            if (*hasRest) {
                report(ParseError, false, null(), JSMSG_PARAMETER_AFTER_REST);
                return false;
            }

            TokenKind tt = tokenStream.getToken();
            JS_ASSERT_IF(parenFreeArrow, tt == TOK_NAME);
            switch (tt) {
              case TOK_LB:
              case TOK_LC:
              {
                /* See comment below in the TOK_NAME case. */
                if (duplicatedArg) {
                    report(ParseError, false, duplicatedArg, JSMSG_BAD_DUP_ARGS);
                    return false;
                }

                if (hasDefaults) {
                    report(ParseError, false, null(), JSMSG_NONDEFAULT_FORMAL_AFTER_DEFAULT);
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
                Node lhs = destructuringExpr(&data, tt);
                if (!lhs)
                    return false;

                /*
                 * Synthesize a destructuring assignment from the single
                 * anonymous positional parameter into the destructuring
                 * left-hand-side expression and accumulate it in list.
                 */
                HandlePropertyName name = context->names().empty;
                Node rhs = newName(name);
                if (!rhs)
                    return false;

                if (!pc->define(tokenStream, name, rhs, Definition::ARG))
                    return false;

                Node item = handler.newBinary(PNK_ASSIGN, lhs, rhs);
                if (!item)
                    return false;
                if (list) {
                    handler.addList(list, item);
                } else {
                    list = handler.newList(PNK_VAR, item);
                    if (!list)
                        return false;
                    *listp = list;
                }
                break;
              }

              case TOK_YIELD:
                if (!checkYieldNameValidity())
                    return false;
                goto TOK_NAME;

              case TOK_TRIPLEDOT:
              {
                *hasRest = true;
                tt = tokenStream.getToken();
                if (tt != TOK_NAME) {
                    if (tt != TOK_ERROR)
                        report(ParseError, false, null(), JSMSG_NO_REST_NAME);
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
                bool disallowDuplicateArgs = funbox->hasDestructuringArgs || hasDefaults;
                if (!defineArg(funcpn, name, disallowDuplicateArgs, &duplicatedArg))
                    return false;

                if (tokenStream.matchToken(TOK_ASSIGN)) {
                    // A default argument without parentheses would look like:
                    // a = expr => body, but both operators are right-associative, so
                    // that would have been parsed as a = (expr => body) instead.
                    // Therefore it's impossible to get here with parenFreeArrow.
                    JS_ASSERT(!parenFreeArrow);

                    if (*hasRest) {
                        report(ParseError, false, null(), JSMSG_REST_WITH_DEFAULT);
                        return false;
                    }
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
                    Node def_expr = assignExprWithoutYield(JSMSG_YIELD_IN_DEFAULT);
                    if (!def_expr)
                        return false;
                    handler.setLastFunctionArgumentDefault(funcpn, def_expr);
                }

                break;
              }

              default:
                report(ParseError, false, null(), JSMSG_MISSING_FORMAL);
                /* FALL THROUGH */
              case TOK_ERROR:
                return false;
            }
        } while (!parenFreeArrow && tokenStream.matchToken(TOK_COMMA));

        if (!parenFreeArrow && tokenStream.getToken() != TOK_RP) {
            report(ParseError, false, null(), JSMSG_PAREN_AFTER_FORMAL);
            return false;
        }

        if (!hasDefaults)
            funbox->length = pc->numArgs() - *hasRest;
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::checkFunctionDefinition(HandlePropertyName funName,
                                                  ParseNode **pn_, FunctionSyntaxKind kind,
                                                  bool *pbodyProcessed)
{
    ParseNode *&pn = *pn_;
    *pbodyProcessed = false;

    /* Function statements add a binding to the enclosing scope. */
    bool bodyLevel = pc->atBodyLevel();

    if (kind == Statement) {
        /*
         * Handle redeclaration and optimize cases where we can statically bind the
         * function (thereby avoiding JSOP_DEFFUN and dynamic name lookup).
         */
        if (Definition *dn = pc->decls().lookupFirst(funName)) {
            JS_ASSERT(!dn->isUsed());
            JS_ASSERT(dn->isDefn());

            if (options().extraWarningsOption || dn->kind() == Definition::CONST) {
                JSAutoByteString name;
                ParseReportKind reporter = (dn->kind() != Definition::CONST)
                                           ? ParseExtraWarning
                                           : ParseError;
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
            if (bodyLevel && !makeDefIntoUse(dn, pn, funName))
                return false;
        } else if (bodyLevel) {
            /*
             * If this function was used before it was defined, claim the
             * pre-created definition node for this function that primaryExpr
             * put in pc->lexdeps on first forward reference, and recycle pn.
             */
            if (Definition *fn = pc->lexdeps.lookupDefn<FullParseHandler>(funName)) {
                JS_ASSERT(fn->isDefn());
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
            JS_ASSERT(pn->functionIsHoisted());
            JS_ASSERT_IF(pc->sc->isFunctionBox(), !pn->pn_cookie.isFree());
            JS_ASSERT_IF(!pc->sc->isFunctionBox(), pn->pn_cookie.isFree());
        } else {
            /*
             * As a SpiderMonkey-specific extension, non-body-level function
             * statements (e.g., functions in an "if" or "while" block) are
             * dynamically bound when control flow reaches the statement.
             */
            JS_ASSERT(!pc->sc->strict);
            JS_ASSERT(pn->pn_cookie.isFree());
            if (pc->sc->isFunctionBox()) {
                FunctionBox *funbox = pc->sc->asFunctionBox();
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
                pc->funcStmts = context->new_<FuncStmtSet>(context);
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
    if (LazyScript *lazyOuter = handler.lazyOuterFunction()) {
        JSFunction *fun = handler.nextLazyInnerFunction();
        JS_ASSERT(!fun->isLegacyGenerator());
        FunctionBox *funbox = newFunctionBox(pn, fun, pc, Directives(/* strict = */ false),
                                             fun->generatorKind());
        if (!funbox)
            return false;

        if (!addFreeVariablesFromLazyFunction(fun, pc))
            return false;

        // The position passed to tokenStream.advance() is relative to
        // userbuf.base() while LazyScript::{begin,end} offsets are relative to
        // the outermost script source. N.B: userbuf.base() is initialized
        // (in TokenStream()) to begin() - column() so that column numbers in
        // the lazily parsed script are correct.
        uint32_t userbufBase = lazyOuter->begin() - lazyOuter->column();
        tokenStream.advance(fun->lazyScript()->end() - userbufBase);

        *pbodyProcessed = true;
        return true;
    }

    return true;
}

template <class T, class U>
static inline void
PropagateTransitiveParseFlags(const T *inner, U *outer)
{
   if (inner->bindingsAccessedDynamically())
     outer->setBindingsAccessedDynamically();
   if (inner->hasDebuggerStatement())
     outer->setHasDebuggerStatement();
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::addFreeVariablesFromLazyFunction(JSFunction *fun,
                                                       ParseContext<ParseHandler> *pc)
{
    // Update any definition nodes in this context according to free variables
    // in a lazily parsed inner function.

    LazyScript *lazy = fun->lazyScript();
    HeapPtrAtom *freeVariables = lazy->freeVariables();
    for (size_t i = 0; i < lazy->numFreeVariables(); i++) {
        JSAtom *atom = freeVariables[i];

        // 'arguments' will be implicitly bound within the inner function.
        if (atom == context->names().arguments)
            continue;

        DefinitionNode dn = pc->decls().lookupFirst(atom);

        if (!dn) {
            dn = getOrCreateLexicalDependency(pc, atom);
            if (!dn)
                return false;
        }

        /* Mark the outer dn as escaping. */
        handler.setFlag(handler.getDefinitionNode(dn), PND_CLOSED);
    }

    PropagateTransitiveParseFlags(lazy, pc->sc);
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkFunctionDefinition(HandlePropertyName funName,
                                                    Node *pn, FunctionSyntaxKind kind,
                                                    bool *pbodyProcessed)
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
            if (dn == Definition::CONST) {
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

#ifdef JS_HAS_TEMPLATE_STRINGS
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::templateLiteral()
{
    Node pn = noSubstitutionTemplate();
    if (!pn) {
        report(ParseError, false, null(),
                JSMSG_SYNTAX_ERROR);
        return null();
    }
    Node nodeList = handler.newList(PNK_TEMPLATE_STRING_LIST, pn);
    TokenKind tt;
    do {
        pn = expr();
        if (!pn) {
            report(ParseError, false, null(),
                    JSMSG_SYNTAX_ERROR);
            return null();
        }
        handler.addList(nodeList, pn);
        tt = tokenStream.getToken();
        if (tt != TOK_RC) {
            report(ParseError, false, null(),
                    JSMSG_SYNTAX_ERROR);
            return null();
        }
        tt = tokenStream.getToken(TokenStream::TemplateTail);
        if (tt == TOK_ERROR) {
            report(ParseError, false, null(),
                    JSMSG_SYNTAX_ERROR);
            return null();
        }

        pn = noSubstitutionTemplate();
        if (!pn) {
            report(ParseError, false, null(),
                    JSMSG_SYNTAX_ERROR);
            return null();
        }

        handler.addList(nodeList, pn);
    } while (tt == TOK_TEMPLATE_HEAD);
    return nodeList;
}
#endif

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionDef(HandlePropertyName funName, const TokenStream::Position &start,
                                  FunctionType type, FunctionSyntaxKind kind,
                                  GeneratorKind generatorKind)
{
    JS_ASSERT_IF(kind == Statement, funName);

    /* Make a TOK_FUNCTION node. */
    Node pn = handler.newFunctionDefinition();
    if (!pn)
        return null();

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
        JSContext *cx = context->maybeJSContext();
        proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, context->global());
        if (!proto)
            return null();
    }
    RootedFunction fun(context, newFunction(pc, funName, kind, proto));
    if (!fun)
        return null();

    // Speculatively parse using the directives of the parent parsing context.
    // If a directive is encountered (e.g., "use strict") that changes how the
    // function should have been parsed, we backup and reparse with the new set
    // of directives.
    Directives directives(pc);
    Directives newDirectives = directives;

    while (true) {
        if (functionArgsAndBody(pn, fun, type, kind, generatorKind, directives, &newDirectives))
            break;
        if (tokenStream.hadError() || directives == newDirectives)
            return null();

        // Assignment must be monotonic to prevent reparsing iloops
        JS_ASSERT_IF(directives.strict(), newDirectives.strict());
        JS_ASSERT_IF(directives.asmJS(), newDirectives.asmJS());
        directives = newDirectives;

        tokenStream.seek(start);
        if (funName && tokenStream.getToken() == TOK_ERROR)
            return null();

        // functionArgsAndBody may have already set pn->pn_body before failing.
        handler.setFunctionBody(pn, null());
    }

    return pn;
}

template <>
bool
Parser<FullParseHandler>::finishFunctionDefinition(ParseNode *pn, FunctionBox *funbox,
                                                   ParseNode *prelude, ParseNode *body)
{
    pn->pn_pos.end = pos().end;

    /*
     * If there were destructuring formal parameters, prepend the initializing
     * comma expression that we synthesized to body. If the body is a return
     * node, we must make a special PNK_SEQ node, to prepend the destructuring
     * code without bracing the decompilation of the function body.
     */
    if (prelude) {
        if (!body->isArity(PN_LIST)) {
            ParseNode *block;

            block = ListNode::create(PNK_SEQ, &handler);
            if (!block)
                return false;
            block->pn_pos = body->pn_pos;
            block->initList(body);

            body = block;
        }

        ParseNode *item = UnaryNode::create(PNK_SEMI, &handler);
        if (!item)
            return false;

        item->pn_pos.begin = item->pn_pos.end = body->pn_pos.begin;
        item->pn_kid = prelude;
        item->pn_next = body->pn_head;
        body->pn_head = item;
        if (body->pn_tail == &body->pn_head)
            body->pn_tail = &item->pn_next;
        ++body->pn_count;
        body->pn_xflags |= PNX_DESTRUCT;
    }

    JS_ASSERT(pn->pn_funbox == funbox);
    JS_ASSERT(pn->pn_body->isKind(PNK_ARGSBODY));
    pn->pn_body->append(body);
    pn->pn_body->pn_pos = body->pn_pos;

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::finishFunctionDefinition(Node pn, FunctionBox *funbox,
                                                     Node prelude, Node body)
{
    // The LazyScript for a lazily parsed function needs to be constructed
    // while its ParseContext and associated lexdeps and inner functions are
    // still available.

    if (funbox->inWith)
        return abortIfSyntaxParser();

    size_t numFreeVariables = pc->lexdeps->count();
    size_t numInnerFunctions = pc->innerFunctions.length();

    RootedFunction fun(context, funbox->function());
    LazyScript *lazy = LazyScript::CreateRaw(context, fun, numFreeVariables, numInnerFunctions,
                                             versionNumber(), funbox->bufStart, funbox->bufEnd,
                                             funbox->startLine, funbox->startColumn);
    if (!lazy)
        return false;

    HeapPtrAtom *freeVariables = lazy->freeVariables();
    size_t i = 0;
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront())
        freeVariables[i++].init(r.front().key());
    JS_ASSERT(i == numFreeVariables);

    HeapPtrFunction *innerFunctions = lazy->innerFunctions();
    for (size_t i = 0; i < numInnerFunctions; i++)
        innerFunctions[i].init(pc->innerFunctions[i]);

    if (pc->sc->strict)
        lazy->setStrict();
    lazy->setGeneratorKind(funbox->generatorKind());
    if (funbox->usesArguments && funbox->usesApply)
        lazy->setUsesArgumentsAndApply();
    PropagateTransitiveParseFlags(funbox, lazy);

    fun->initLazyScript(lazy);
    return true;
}

template <>
bool
Parser<FullParseHandler>::functionArgsAndBody(ParseNode *pn, HandleFunction fun,
                                              FunctionType type, FunctionSyntaxKind kind,
                                              GeneratorKind generatorKind,
                                              Directives inheritedDirectives,
                                              Directives *newDirectives)
{
    ParseContext<FullParseHandler> *outerpc = pc;

    // Create box for fun->object early to protect against last-ditch GC.
    FunctionBox *funbox = newFunctionBox(pn, fun, pc, inheritedDirectives, generatorKind);
    if (!funbox)
        return false;

    // Try a syntax parse for this inner function.
    do {
        Parser<SyntaxParseHandler> *parser = handler.syntaxParser;
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
                                                   outerpc->blockidGen,
                                                   /* blockScopeDepth = */ 0);
            if (!funpc.init(tokenStream))
                return false;

            if (!parser->functionArgsAndBodyGeneric(SyntaxParseHandler::NodeGeneric,
                                                    fun, type, kind, newDirectives))
            {
                if (parser->hadAbortedSyntaxParse()) {
                    // Try again with a full parse.
                    parser->clearAbortedSyntaxParse();
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

    if (!functionArgsAndBodyGeneric(pn, fun, type, kind, newDirectives))
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
Parser<SyntaxParseHandler>::functionArgsAndBody(Node pn, HandleFunction fun,
                                                FunctionType type, FunctionSyntaxKind kind,
                                                GeneratorKind generatorKind,
                                                Directives inheritedDirectives,
                                                Directives *newDirectives)
{
    ParseContext<SyntaxParseHandler> *outerpc = pc;

    // Create box for fun->object early to protect against last-ditch GC.
    FunctionBox *funbox = newFunctionBox(pn, fun, pc, inheritedDirectives, generatorKind);
    if (!funbox)
        return false;

    // Initialize early for possible flags mutation via destructuringExpr.
    ParseContext<SyntaxParseHandler> funpc(this, pc, handler.null(), funbox, newDirectives,
                                           outerpc->staticLevel + 1, outerpc->blockidGen,
                                           /* blockScopeDepth = */ 0);
    if (!funpc.init(tokenStream))
        return false;

    if (!functionArgsAndBodyGeneric(pn, fun, type, kind, newDirectives))
        return false;

    if (!leaveFunction(pn, outerpc, kind))
        return false;

    // This is a lazy function inner to another lazy function. Remember the
    // inner function so that if the outer function is eventually parsed we do
    // not need any further parsing or processing of the inner function.
    JS_ASSERT(fun->lazyScript());
    return outerpc->innerFunctions.append(fun);
}

template <>
ParseNode *
Parser<FullParseHandler>::standaloneLazyFunction(HandleFunction fun, unsigned staticLevel,
                                                 bool strict, GeneratorKind generatorKind)
{
    Node pn = handler.newFunctionDefinition();
    if (!pn)
        return null();

    Directives directives(/* strict = */ strict);
    FunctionBox *funbox = newFunctionBox(pn, fun, /* outerpc = */ nullptr, directives,
                                         generatorKind);
    if (!funbox)
        return null();
    funbox->length = fun->nargs() - fun->hasRest();

    Directives newDirectives = directives;
    ParseContext<FullParseHandler> funpc(this, /* parent = */ nullptr, pn, funbox,
                                         &newDirectives, staticLevel, /* bodyid = */ 0,
                                         /* blockScopeDepth = */ 0);
    if (!funpc.init(tokenStream))
        return null();

    if (!functionArgsAndBodyGeneric(pn, fun, Normal, Statement, &newDirectives)) {
        JS_ASSERT(directives == newDirectives);
        return null();
    }

    if (fun->isNamedLambda()) {
        if (AtomDefnPtr p = pc->lexdeps->lookup(fun->name())) {
            Definition *dn = p.value().get<FullParseHandler>();
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
Parser<ParseHandler>::functionArgsAndBodyGeneric(Node pn, HandleFunction fun, FunctionType type,
                                                 FunctionSyntaxKind kind,
                                                 Directives *newDirectives)
{
    // Given a properly initialized parse context, try to parse an actual
    // function without concern for conversion to strict mode, use of lazy
    // parsing and such.

    Node prelude = null();
    bool hasRest;
    if (!functionArguments(kind, &prelude, pn, &hasRest))
        return false;

    FunctionBox *funbox = pc->sc->asFunctionBox();

    fun->setArgCount(pc->numArgs());
    if (hasRest)
        fun->setHasRest();

    if (type == Getter && fun->nargs() > 0) {
        report(ParseError, false, null(), JSMSG_ACCESSOR_WRONG_ARGS, "getter", "no", "s");
        return false;
    }
    if (type == Setter && fun->nargs() != 1) {
        report(ParseError, false, null(), JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
        return false;
    }

    if (kind == Arrow && !tokenStream.matchToken(TOK_ARROW)) {
        report(ParseError, false, null(), JSMSG_BAD_ARROW_ARGS);
        return false;
    }

    // Parse the function body.
    FunctionBodyType bodyType = StatementListBody;
    if (tokenStream.getToken(TokenStream::Operand) != TOK_LC) {
        if (funbox->isStarGenerator()) {
            report(ParseError, false, null(), JSMSG_CURLY_BEFORE_BODY);
            return false;
        }
        tokenStream.ungetToken();
        bodyType = ExpressionBody;
        fun->setIsExprClosure();
    }

    Node body = functionBody(kind, bodyType);
    if (!body)
        return false;

    if (fun->name() && !checkStrictBinding(fun->name(), pn))
        return false;

#if JS_HAS_EXPR_CLOSURES
    if (bodyType == StatementListBody) {
#endif
        if (!tokenStream.matchToken(TOK_RC)) {
            report(ParseError, false, null(), JSMSG_CURLY_AFTER_BODY);
            return false;
        }
        funbox->bufEnd = pos().begin + 1;
#if JS_HAS_EXPR_CLOSURES
    } else {
        if (tokenStream.hadError())
            return false;
        funbox->bufEnd = pos().end;
        if (kind == Statement && !MatchOrInsertSemicolon(tokenStream))
            return false;
    }
#endif

    return finishFunctionDefinition(pn, funbox, prelude, body);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkYieldNameValidity()
{
    // In star generators and in JS >= 1.7, yield is a keyword.  Otherwise in
    // strict mode, yield is a future reserved word.
    if (pc->isStarGenerator() || versionNumber() >= JSVERSION_1_7 || pc->sc->strict) {
        report(ParseError, false, null(), JSMSG_RESERVED_ID, "yield");
        return false;
    }
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionStmt()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FUNCTION));

    TokenStream::Position start(keepAtoms);
    tokenStream.tell(&start);

    RootedPropertyName name(context);
    GeneratorKind generatorKind = NotGenerator;
    TokenKind tt = tokenStream.getToken();

    if (tt == TOK_MUL) {
        tokenStream.tell(&start);
        tt = tokenStream.getToken();
        generatorKind = StarGenerator;
    }

    if (tt == TOK_NAME) {
        name = tokenStream.currentName();
    } else if (tt == TOK_YIELD) {
        if (!checkYieldNameValidity())
            return null();
        name = tokenStream.currentName();
    } else {
        /* Unnamed function expressions are forbidden in statement context. */
        report(ParseError, false, null(), JSMSG_UNNAMED_FUNCTION_STMT);
        return null();
    }

    /* We forbid function statements in strict mode code. */
    if (!pc->atBodyLevel() && pc->sc->needStrictChecks() &&
        !report(ParseStrictError, pc->sc->strict, null(), JSMSG_STRICT_FUNCTION_STATEMENT))
        return null();

    return functionDef(name, start, Normal, Statement, generatorKind);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionExpr()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FUNCTION));

    TokenStream::Position start(keepAtoms);
    tokenStream.tell(&start);

    GeneratorKind generatorKind = NotGenerator;
    TokenKind tt = tokenStream.getToken();

    if (tt == TOK_MUL) {
        tokenStream.tell(&start);
        tt = tokenStream.getToken();
        generatorKind = StarGenerator;
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

    return functionDef(name, start, Normal, Expression, generatorKind);
}

/*
 * Return true if this node, known to be an unparenthesized string literal,
 * could be the string of a directive in a Directive Prologue. Directive
 * strings never contain escape sequences or line continuations.
 * isEscapeFreeStringLiteral, below, checks whether the node itself could be
 * a directive.
 */
static inline bool
IsEscapeFreeStringLiteral(const TokenPos &pos, JSAtom *str)
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
    // If we are already inside "use asm" that means we are either actively
    // compiling or we are reparsing after asm.js validation failure. In either
    // case, nothing to do here.
    if (pc->useAsmOrInsideUseAsm())
        return true;

    // If there is no ScriptSource, then we are doing a non-compiling parse and
    // so we shouldn't (and can't, without a ScriptSource) compile.
    if (ss == nullptr)
        return true;

    pc->sc->asFunctionBox()->useAsm = true;

#ifdef JS_ION
    // Attempt to validate and compile this asm.js module. On success, the
    // tokenStream has been advanced to the closing }. On failure, the
    // tokenStream is in an indeterminate state and we must reparse the
    // function from the beginning. Reparsing is triggered by marking that a
    // new directive has been encountered and returning 'false'.
    bool validated;
    if (!CompileAsmJS(context, *this, list, &validated))
        return false;
    if (!validated) {
        pc->newDirectives->setAsmJS();
        return false;
    }
#endif

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
Parser<ParseHandler>::maybeParseDirective(Node list, Node pn, bool *cont)
{
    TokenPos directivePos;
    JSAtom *directive = handler.isStringExprStatement(pn, &directivePos);

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
            if (!pc->sc->strict) {
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
                    pc->sc->strict = true;
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
Parser<ParseHandler>::statements()
{
    JS_CHECK_RECURSION(context, return null());

    Node pn = handler.newStatementList(pc->blockid(), pos());
    if (!pn)
        return null();

    Node saveBlock = pc->blockNode;
    pc->blockNode = pn;

    bool canHaveDirectives = pc->atBodyLevel();
    for (;;) {
        TokenKind tt = tokenStream.peekToken(TokenStream::Operand);
        if (tt <= TOK_EOF || tt == TOK_RC) {
            if (tt == TOK_ERROR) {
                if (tokenStream.isEOF())
                    isUnexpectedEOF_ = true;
                return null();
            }
            break;
        }
        Node next = statement(canHaveDirectives);
        if (!next) {
            if (tokenStream.isEOF())
                isUnexpectedEOF_ = true;
            return null();
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
Parser<ParseHandler>::condition()
{
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_COND);
    Node pn = exprInParens();
    if (!pn)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_COND);

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    if (handler.isOperationWithoutParens(pn, PNK_ASSIGN) &&
        !report(ParseExtraWarning, false, null(), JSMSG_EQUAL_AS_ASSIGN))
    {
        return null();
    }
    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchLabel(MutableHandle<PropertyName*> label)
{
    TokenKind tt = tokenStream.peekTokenSameLine(TokenStream::Operand);
    if (tt == TOK_ERROR)
        return false;
    if (tt == TOK_NAME) {
        tokenStream.consumeKnownToken(TOK_NAME);
        label.set(tokenStream.currentName());
    } else if (tt == TOK_YIELD) {
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
Parser<ParseHandler>::reportRedeclaration(Node pn, bool isConst, JSAtom *atom)
{
    JSAutoByteString name;
    if (AtomToPrintableString(context, atom, &name))
        report(ParseError, false, pn, JSMSG_REDECLARED_VAR, isConst ? "const" : "variable", name.ptr());
    return false;
}

/*
 * Define a let-variable in a block, let-expression, or comprehension scope. pc
 * must already be in such a scope.
 *
 * Throw a SyntaxError if 'atom' is an invalid name. Otherwise create a
 * property for the new variable on the block object, pc->staticScope;
 * populate data->pn->pn_{op,cookie,defn,dflags}; and stash a pointer to
 * data->pn in a slot of the block object.
 */
template <>
/* static */ bool
Parser<FullParseHandler>::bindLet(BindData<FullParseHandler> *data,
                                  HandlePropertyName name, Parser<FullParseHandler> *parser)
{
    ParseContext<FullParseHandler> *pc = parser->pc;
    ParseNode *pn = data->pn;
    if (!parser->checkStrictBinding(name, pn))
        return false;

    ExclusiveContext *cx = parser->context;

    Rooted<StaticBlockObject *> blockObj(cx, data->let.blockObj);
    unsigned index = blockObj->numVariables();
    if (index >= StaticBlockObject::LOCAL_INDEX_LIMIT) {
        parser->report(ParseError, false, pn, data->let.overflow);
        return false;
    }

    /*
     * Assign block-local index to pn->pn_cookie right away, encoding it as an
     * upvar cookie whose skip tells the current static level. The emitter will
     * adjust the node's slot based on its stack depth model -- and, for global
     * and eval code, js::frontend::CompileScript will adjust the slot
     * again to include script->nfixed.
     */
    if (!pn->pn_cookie.set(parser->tokenStream, pc->staticLevel, index))
        return false;

    /*
     * For bindings that are hoisted to the beginning of the block/function,
     * define() right now. Otherwise, delay define until PushLetScope.
     */
    if (data->let.varContext == HoistVars) {
        JS_ASSERT(!pc->atBodyLevel());
        Definition *dn = pc->decls().lookupFirst(name);
        if (dn && dn->pn_blockid == pc->blockid())
            return parser->reportRedeclaration(pn, dn->isConst(), name);
        if (!pc->define(parser->tokenStream, name, pn, Definition::LET))
            return false;
    }

    bool redeclared;
    RootedId id(cx, NameToId(name));
    RootedShape shape(cx, StaticBlockObject::addVar(cx, blockObj, id, index, &redeclared));
    if (!shape) {
        if (redeclared)
            parser->reportRedeclaration(pn, false, name);
        return false;
    }

    /* Store pn in the static block object. */
    blockObj->setDefinitionParseNode(index, reinterpret_cast<Definition *>(pn));
    return true;
}

template <>
/* static */ bool
Parser<SyntaxParseHandler>::bindLet(BindData<SyntaxParseHandler> *data,
                                    HandlePropertyName name, Parser<SyntaxParseHandler> *parser)
{
    if (!parser->checkStrictBinding(name, data->pn))
        return false;

    return true;
}

template <typename ParseHandler, class Op>
static inline bool
ForEachLetDef(TokenStream &ts, ParseContext<ParseHandler> *pc,
              HandleStaticBlockObject blockObj, Op op)
{
    for (Shape::Range<CanGC> r(ts.context(), blockObj->lastProperty()); !r.empty(); r.popFront()) {
        Shape &shape = r.front();

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
    bool operator()(TokenStream &, ParseContext<ParseHandler> *pc, HandleStaticBlockObject,
                    const Shape &, JSAtom *atom)
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
AccumulateBlockScopeDepth(ParseContext<ParseHandler> *pc)
{
    uint32_t innerDepth = pc->topStmt->innerBlockScopeDepth;
    StmtInfoPC *outer = pc->topStmt->down;

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
PopStatementPC(TokenStream &ts, ParseContext<ParseHandler> *pc)
{
    RootedNestedScopeObject scopeObj(ts.context(), pc->topStmt->staticScope);
    JS_ASSERT(!!scopeObj == pc->topStmt->isNestedScope);

    AccumulateBlockScopeDepth(pc);
    FinishPopStatement(pc);

    if (scopeObj) {
        if (scopeObj->is<StaticBlockObject>()) {
            RootedStaticBlockObject blockObj(ts.context(), &scopeObj->as<StaticBlockObject>());
            JS_ASSERT(!blockObj->inDictionaryMode());
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
typename ContextT::StmtInfo *
LexicalLookup(ContextT *ct, HandleAtom atom, int *slotp, typename ContextT::StmtInfo *stmt)
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
        if (stmt->type == STMT_WITH)
            break;

        // Skip statements that do not introduce a new scope
        if (!stmt->isBlockScope)
            continue;

        StaticBlockObject &blockObj = stmt->staticBlock();
        Shape *shape = blockObj.nativeLookup(ct->sc->context, id);
        if (shape) {
            if (slotp)
                *slotp = blockObj.shapeToIndex(*shape);
            return stmt;
        }
    }

    if (slotp)
        *slotp = -1;
    return stmt;
}

template <typename ParseHandler>
static inline bool
OuterLet(ParseContext<ParseHandler> *pc, StmtInfoPC *stmt, HandleAtom atom)
{
    while (stmt->downScope) {
        stmt = LexicalLookup(pc, atom, nullptr, stmt->downScope);
        if (!stmt)
            return false;
        if (stmt->type == STMT_BLOCK)
            return true;
    }
    return false;
}

template <typename ParseHandler>
/* static */ bool
Parser<ParseHandler>::bindVarOrConst(BindData<ParseHandler> *data,
                                     HandlePropertyName name, Parser<ParseHandler> *parser)
{
    ExclusiveContext *cx = parser->context;
    ParseContext<ParseHandler> *pc = parser->pc;
    Node pn = data->pn;
    bool isConstDecl = data->op == JSOP_DEFCONST;

    /* Default best op for pn is JSOP_NAME; we'll try to improve below. */
    parser->handler.setOp(pn, JSOP_NAME);

    if (!parser->checkStrictBinding(name, pn))
        return false;

    StmtInfoPC *stmt = LexicalLookup(pc, name, nullptr, (StmtInfoPC *)nullptr);

    if (stmt && stmt->type == STMT_WITH) {
        parser->handler.setFlag(pn, PND_DEOPTIMIZED);
        if (pc->sc->isFunctionBox()) {
            FunctionBox *funbox = pc->sc->asFunctionBox();
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
    JS_ASSERT_IF(stmt, !defs.empty());

    if (defs.empty()) {
        return pc->define(parser->tokenStream, name, pn,
                          isConstDecl ? Definition::CONST : Definition::VAR);
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
        bool error = (isConstDecl ||
                      dn_kind == Definition::CONST ||
                      (dn_kind == Definition::LET &&
                       (stmt->type != STMT_CATCH || OuterLet(pc, stmt, name))));

        if (parser->options().extraWarningsOption
            ? data->op != JSOP_DEFVAR || dn_kind != Definition::VAR
            : error)
        {
            JSAutoByteString bytes;
            ParseReportKind reporter = error ? ParseError : ParseExtraWarning;
            if (!AtomToPrintableString(cx, name, &bytes) ||
                !parser->report(reporter, false, pn, JSMSG_REDECLARED_VAR,
                                Definition::kindString(dn_kind), bytes.ptr()))
            {
                return false;
            }
        }
    }

    parser->handler.linkUseToDef(pn, dn);
    return true;
}

template <>
bool
Parser<FullParseHandler>::makeSetCall(ParseNode *pn, unsigned msg)
{
    JS_ASSERT(pn->isKind(PNK_CALL));
    JS_ASSERT(pn->isArity(PN_LIST));
    JS_ASSERT(pn->isOp(JSOP_CALL) || pn->isOp(JSOP_SPREADCALL) ||
              pn->isOp(JSOP_EVAL) || pn->isOp(JSOP_SPREADEVAL) ||
              pn->isOp(JSOP_FUNCALL) || pn->isOp(JSOP_FUNAPPLY));

    if (!report(ParseStrictError, pc->sc->strict, pn, msg))
        return false;
    handler.markAsSetCall(pn);
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::noteNameUse(HandlePropertyName name, Node pn)
{
    StmtInfoPC *stmt = LexicalLookup(pc, name, nullptr, (StmtInfoPC *)nullptr);

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

    if (stmt && stmt->type == STMT_WITH)
        handler.setFlag(pn, PND_DEOPTIMIZED);

    return true;
}

template <>
bool
Parser<FullParseHandler>::bindDestructuringVar(BindData<FullParseHandler> *data, ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_NAME));

    RootedPropertyName name(context, pn->pn_atom->asPropertyName());

    data->pn = pn;
    if (!data->binder(data, name, this))
        return false;

    /*
     * Select the appropriate name-setting opcode, respecting eager selection
     * done by the data->binder function.
     */
    if (pn->pn_dflags & PND_BOUND)
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

/*
 * Destructuring patterns can appear in two kinds of contexts:
 *
 * - assignment-like: assignment expressions and |for| loop heads.  In
 *   these cases, the patterns' property value positions can be
 *   arbitrary lvalue expressions; the destructuring is just a fancy
 *   assignment.
 *
 * - declaration-like: |var| and |let| declarations, functions' formal
 *   parameter lists, |catch| clauses, and comprehension tails.  In
 *   these cases, the patterns' property value positions must be
 *   simple names; the destructuring defines them as new variables.
 *
 * In both cases, other code parses the pattern as an arbitrary
 * primaryExpr, and then, here in CheckDestructuring, verify that the
 * tree is a valid destructuring expression.
 *
 * In assignment-like contexts, we parse the pattern with
 * pc->inDeclDestructuring clear, so the lvalue expressions in the
 * pattern are parsed normally.  primaryExpr links variable references
 * into the appropriate use chains; creates placeholder definitions;
 * and so on.  CheckDestructuring is called with |data| nullptr (since
 * we won't be binding any new names), and we specialize lvalues as
 * appropriate.
 *
 * In declaration-like contexts, the normal variable reference
 * processing would just be an obstruction, because we're going to
 * define the names that appear in the property value positions as new
 * variables anyway.  In this case, we parse the pattern with
 * pc->inDeclDestructuring set, which directs primaryExpr to leave
 * whatever name nodes it creates unconnected.  Then, here in
 * CheckDestructuring, we require the pattern's property value
 * positions to be simple names, and define them as appropriate to the
 * context.  For these calls, |data| points to the right sort of
 * BindData.
 *
 * The 'toplevel' is a private detail of the recursive strategy used by
 * CheckDestructuring and callers should use the default value.
 */
template <>
bool
Parser<FullParseHandler>::checkDestructuring(BindData<FullParseHandler> *data,
                                             ParseNode *left, bool toplevel)
{
    bool ok;

    if (left->isKind(PNK_ARRAYCOMP)) {
        report(ParseError, false, left, JSMSG_ARRAY_COMP_LEFTSIDE);
        return false;
    }

    Rooted<StaticBlockObject *> blockObj(context);
    blockObj = data && data->binder == bindLet ? data->let.blockObj.get() : nullptr;

    if (left->isKind(PNK_ARRAY)) {
        for (ParseNode *pn = left->pn_head; pn; pn = pn->pn_next) {
            if (!pn->isKind(PNK_ELISION)) {
                if (pn->isKind(PNK_ARRAY) || pn->isKind(PNK_OBJECT)) {
                    ok = checkDestructuring(data, pn, false);
                } else {
                    if (data) {
                        if (!pn->isKind(PNK_NAME)) {
                            report(ParseError, false, pn, JSMSG_NO_VARIABLE_NAME);
                            return false;
                        }
                        ok = bindDestructuringVar(data, pn);
                    } else {
                        ok = checkAndMarkAsAssignmentLhs(pn, KeyedDestructuringAssignment);
                    }
                }
                if (!ok)
                    return false;
            }
        }
    } else {
        JS_ASSERT(left->isKind(PNK_OBJECT));
        for (ParseNode *member = left->pn_head; member; member = member->pn_next) {
            MOZ_ASSERT(member->isKind(PNK_COLON));
            ParseNode *expr = member->pn_right;

            if (expr->isKind(PNK_ARRAY) || expr->isKind(PNK_OBJECT)) {
                ok = checkDestructuring(data, expr, false);
            } else if (data) {
                if (!expr->isKind(PNK_NAME)) {
                    report(ParseError, false, expr, JSMSG_NO_VARIABLE_NAME);
                    return false;
                }
                ok = bindDestructuringVar(data, expr);
            } else {
                /*
                 * If this is a destructuring shorthand ({x} = ...), then
                 * identifierName wasn't used to parse |x|.  As a result, |x|
                 * hasn't been officially linked to its def or registered in
                 * lexdeps.  Do that now.
                 */
                if (member->pn_right == member->pn_left) {
                    RootedPropertyName name(context, expr->pn_atom->asPropertyName());
                    if (!noteNameUse(name, expr))
                        return false;
                }
                ok = checkAndMarkAsAssignmentLhs(expr, KeyedDestructuringAssignment);
            }
            if (!ok)
                return false;
        }
    }

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkDestructuring(BindData<SyntaxParseHandler> *data,
                                               Node left, bool toplevel)
{
    return abortIfSyntaxParser();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::destructuringExpr(BindData<ParseHandler> *data, TokenKind tt)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(tt));

    pc->inDeclDestructuring = true;
    Node pn = primaryExpr(tt);
    pc->inDeclDestructuring = false;
    if (!pn)
        return null();
    if (!checkDestructuring(data, pn))
        return null();
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::pushLexicalScope(HandleStaticBlockObject blockObj, StmtInfoPC *stmt)
{
    JS_ASSERT(blockObj);

    ObjectBox *blockbox = newObjectBox(blockObj);
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
Parser<ParseHandler>::pushLexicalScope(StmtInfoPC *stmt)
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

    bool operator()(TokenStream &ts, ParseContext<FullParseHandler> *pc,
                    HandleStaticBlockObject blockObj, const Shape &shape, JSAtom *)
    {
        ParseNode *def = (ParseNode *) blockObj->getSlot(shape.slot()).toPrivate();
        def->pn_blockid = blockid;
        RootedPropertyName name(ts.context(), def->name());
        return pc->define(ts, name, def, Definition::LET);
    }
};

template <>
ParseNode *
Parser<FullParseHandler>::pushLetScope(HandleStaticBlockObject blockObj, StmtInfoPC *stmt)
{
    JS_ASSERT(blockObj);
    ParseNode *pn = pushLexicalScope(blockObj, stmt);
    if (!pn)
        return null();

    pn->pn_dflags |= PND_LET;

    /* Populate the new scope with decls found in the head with updated blockid. */
    if (!ForEachLetDef(tokenStream, pc, blockObj, AddLetDecl(stmt->blockid)))
        return null();

    return pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::pushLetScope(HandleStaticBlockObject blockObj, StmtInfoPC *stmt)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

/*
 * Parse a let block statement or let expression (determined by 'letContext').
 * In both cases, bindings are not hoisted to the top of the enclosing block
 * and thus must be carefully injected between variables() and the let body.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::letBlock(LetContext letContext)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_LET));

    RootedStaticBlockObject blockObj(context, StaticBlockObject::create(context));
    if (!blockObj)
        return null();

    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_LET);

    Node vars = variables(PNK_LET, nullptr, blockObj, DontHoistVars);
    if (!vars)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_LET);

    StmtInfoPC stmtInfo(context);
    Node block = pushLetScope(blockObj, &stmtInfo);
    if (!block)
        return null();

    Node pnlet = handler.newBinary(PNK_LET, vars, block);
    if (!pnlet)
        return null();
    handler.setBeginPosition(pnlet, begin);

    bool needExprStmt = false;
    if (letContext == LetStatement && !tokenStream.matchToken(TOK_LC, TokenStream::Operand)) {
        /*
         * Strict mode eliminates a grammar ambiguity with unparenthesized
         * LetExpressions in an ExpressionStatement. If followed immediately
         * by an arguments list, it's ambiguous whether the let expression
         * is the callee or the call is inside the let expression body.
         *
         * See bug 569464.
         */
        if (!report(ParseStrictError, pc->sc->strict, pnlet,
                    JSMSG_STRICT_CODE_LET_EXPR_STMT))
        {
            return null();
        }

        /*
         * If this is really an expression in let statement guise, then we
         * need to wrap the PNK_LET node in a PNK_SEMI node so that we pop
         * the return value of the expression.
         */
        needExprStmt = true;
        letContext = LetExpresion;
    }

    Node expr;
    if (letContext == LetStatement) {
        expr = statements();
        if (!expr)
            return null();
        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_LET);
    } else {
        JS_ASSERT(letContext == LetExpresion);
        expr = assignExpr();
        if (!expr)
            return null();
    }
    handler.setLexicalScopeBody(block, expr);
    PopStatementPC(tokenStream, pc);

    handler.setEndPosition(pnlet, pos().end);

    if (needExprStmt) {
        if (!MatchOrInsertSemicolon(tokenStream))
            return null();
        return handler.newExprStatement(pnlet, pos().end);
    }
    return pnlet;
}

template <typename ParseHandler>
static bool
PushBlocklikeStatement(TokenStream &ts, StmtInfoPC *stmt, StmtType type,
                       ParseContext<ParseHandler> *pc)
{
    PushStatementPC(pc, stmt, type);
    return GenerateBlockId(ts, pc, stmt->blockid);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::blockStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));

    StmtInfoPC stmtInfo(context);
    if (!PushBlocklikeStatement(tokenStream, &stmtInfo, STMT_BLOCK, pc))
        return null();

    Node list = statements();
    if (!list)
        return null();

    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_IN_COMPOUND);
    PopStatementPC(tokenStream, pc);
    return list;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newBindingNode(PropertyName *name, bool functionScope, VarContext varContext)
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
            JS_ASSERT(handler.getDefinitionKind(lexdep) == Definition::PLACEHOLDER);

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
Parser<ParseHandler>::variables(ParseNodeKind kind, bool *psimple,
                                StaticBlockObject *blockObj, VarContext varContext)
{
    /*
     * The four options here are:
     * - PNK_VAR:   We're parsing var declarations.
     * - PNK_CONST: We're parsing const declarations.
     * - PNK_LET:   We are parsing a let declaration.
     * - PNK_CALL:  We are parsing the head of a let block.
     */
    JS_ASSERT(kind == PNK_VAR || kind == PNK_CONST || kind == PNK_LET || kind == PNK_CALL);

    /*
     * The simple flag is set if the declaration has the form 'var x', with
     * only one variable declared and no initializer expression.
     */
    JS_ASSERT_IF(psimple, *psimple);

    JSOp op = blockObj ? JSOP_NOP : kind == PNK_VAR ? JSOP_DEFVAR : JSOP_DEFCONST;

    Node pn = handler.newList(kind, null(), op);
    if (!pn)
        return null();

    /*
     * SpiderMonkey const is really "write once per initialization evaluation"
     * var, whereas let is block scoped. ES-Harmony wants block-scoped const so
     * this code will change soon.
     */
    BindData<ParseHandler> data(context);
    if (blockObj)
        data.initLet(varContext, *blockObj, JSMSG_TOO_MANY_LOCALS);
    else
        data.initVarOrConst(op);

    bool first = true;
    Node pn2;
    do {
        if (psimple && !first)
            *psimple = false;
        first = false;

        TokenKind tt = tokenStream.getToken();
        if (tt == TOK_LB || tt == TOK_LC) {
            if (psimple)
                *psimple = false;

            pc->inDeclDestructuring = true;
            pn2 = primaryExpr(tt);
            pc->inDeclDestructuring = false;
            if (!pn2)
                return null();

            if (!checkDestructuring(&data, pn2))
                return null();
            bool ignored;
            if (pc->parsingForInit && matchInOrOf(&ignored)) {
                tokenStream.ungetToken();
                handler.addList(pn, pn2);
                continue;
            }

            MUST_MATCH_TOKEN(TOK_ASSIGN, JSMSG_BAD_DESTRUCT_DECL);

            Node init = assignExpr();
            if (!init)
                return null();

            pn2 = handler.newBinaryOrAppend(PNK_ASSIGN, pn2, init, pc);
            if (!pn2)
                return null();
            handler.addList(pn, pn2);
            continue;
        }

        if (tt != TOK_NAME) {
            if (tt == TOK_YIELD) {
                if (!checkYieldNameValidity())
                    return null();
            } else {
                if (tt != TOK_ERROR)
                    report(ParseError, false, null(), JSMSG_NO_VARIABLE_NAME);
                return null();
            }
        }

        RootedPropertyName name(context, tokenStream.currentName());
        pn2 = newBindingNode(name, kind == PNK_VAR || kind == PNK_CONST, varContext);
        if (!pn2)
            return null();
        if (data.op == JSOP_DEFCONST)
            handler.setFlag(pn2, PND_CONST);
        data.pn = pn2;
        if (!data.binder(&data, name, this))
            return null();
        handler.addList(pn, pn2);

        if (tokenStream.matchToken(TOK_ASSIGN)) {
            if (psimple)
                *psimple = false;

            Node init = assignExpr();
            if (!init)
                return null();

            if (!handler.finishInitializerAssignment(pn2, init, data.op))
                return null();
        }
    } while (tokenStream.matchToken(TOK_COMMA));

    return pn;
}

template <>
ParseNode *
Parser<FullParseHandler>::letDeclaration()
{
    handler.disableSyntaxParser();

    ParseNode *pn;

    do {
        /*
         * This is a let declaration. We must be directly under a block per the
         * proposed ES4 specs, but not an implicit block created due to
         * 'for (let ...)'. If we pass this error test, make the enclosing
         * StmtInfoPC be our scope. Further let declarations in this block will
         * find this scope statement and use the same block object.
         *
         * If we are the first let declaration in this block (i.e., when the
         * enclosing maybe-scope StmtInfoPC isn't yet a scope statement) then
         * we also need to set pc->blockNode to be our PNK_LEXICALSCOPE.
         */
        StmtInfoPC *stmt = pc->topStmt;
        if (stmt && (!stmt->maybeScope() || stmt->isForLetBlock)) {
            report(ParseError, false, null(), JSMSG_LET_DECL_NOT_IN_BLOCK);
            return null();
        }

        if (stmt && stmt->isBlockScope) {
            JS_ASSERT(pc->staticScope == stmt->staticScope);
        } else {
            if (pc->atBodyLevel()) {
                /*
                 * ES4 specifies that let at top level and at body-block scope
                 * does not shadow var, so convert back to var.
                 */
                pn = variables(PNK_VAR);
                if (!pn)
                    return null();
                pn->pn_xflags |= PNX_POPVAR;
                break;
            }

            /*
             * Some obvious assertions here, but they may help clarify the
             * situation. This stmt is not yet a scope, so it must not be a
             * catch block (catch is a lexical scope by definition).
             */
            JS_ASSERT(!stmt->isBlockScope);
            JS_ASSERT(stmt != pc->topScopeStmt);
            JS_ASSERT(stmt->type == STMT_BLOCK ||
                      stmt->type == STMT_SWITCH ||
                      stmt->type == STMT_TRY ||
                      stmt->type == STMT_FINALLY);
            JS_ASSERT(!stmt->downScope);

            /* Convert the block statement into a scope statement. */
            StaticBlockObject *blockObj = StaticBlockObject::create(context);
            if (!blockObj)
                return null();

            ObjectBox *blockbox = newObjectBox(blockObj);
            if (!blockbox)
                return null();

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
            ParseNode *tmp = pc->blockNode;
            JS_ASSERT(!tmp || !tmp->isKind(PNK_LEXICALSCOPE));
#endif

            /* Create a new lexical scope node for these statements. */
            ParseNode *pn1 = LexicalScopeNode::create(PNK_LEXICALSCOPE, &handler);
            if (!pn1)
                return null();

            pn1->pn_pos = pc->blockNode->pn_pos;
            pn1->pn_objbox = blockbox;
            pn1->pn_expr = pc->blockNode;
            pn1->pn_blockid = pc->blockNode->pn_blockid;
            pc->blockNode = pn1;
        }

        pn = variables(PNK_LET, nullptr, &pc->staticScope->as<StaticBlockObject>(), HoistVars);
        if (!pn)
            return null();
        pn->pn_xflags = PNX_POPVAR;
    } while (0);

    return MatchOrInsertSemicolon(tokenStream) ? pn : nullptr;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::letDeclaration()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <>
ParseNode *
Parser<FullParseHandler>::letStatement()
{
    handler.disableSyntaxParser();

    /* Check for a let statement or let expression. */
    ParseNode *pn;
    if (tokenStream.peekToken() == TOK_LP) {
        pn = letBlock(LetStatement);
        JS_ASSERT_IF(pn, pn->isKind(PNK_LET) || pn->isKind(PNK_SEMI));
    } else {
        pn = letDeclaration();
    }
    return pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::letStatement()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template<typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::importDeclaration()
{
    JS_ASSERT(tokenStream.currentToken().type == TOK_IMPORT);

    if (pc->sc->isFunctionBox() || !pc->atBodyLevel()) {
        report(ParseError, false, null(), JSMSG_IMPORT_DECL_AT_TOP_LEVEL);
        return null();
    }

    uint32_t begin = pos().begin;
    TokenKind tt = tokenStream.getToken();

    Node importSpecSet = handler.newList(PNK_IMPORT_SPEC_LIST);
    if (!importSpecSet)
        return null();

    if (tt == TOK_NAME || tt == TOK_LC) {
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
        } else {
            do {
                // Handle the forms |import {} from 'a'| and
                // |import { ..., } from 'a'| (where ... is non empty), by
                // escaping the loop early if the next token is }.
                tt = tokenStream.peekToken(TokenStream::KeywordIsName);
                if (tt == TOK_ERROR)
                    return null();
                if (tt == TOK_RC)
                    break;

                // If the next token is a keyword, the previous call to
                // peekToken matched it as a TOK_NAME, and put it in the
                // lookahead buffer, so this call will match keywords as well.
                MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_IMPORT_NAME);
                Node importName = newName(tokenStream.currentName());
                if (!importName)
                    return null();

                if (tokenStream.getToken() == TOK_NAME &&
                    tokenStream.currentName() == context->names().as)
                {
                    if (tokenStream.getToken() != TOK_NAME) {
                        report(ParseError, false, null(), JSMSG_NO_BINDING_NAME);
                        return null();
                    }
                } else {
                    // Keywords cannot be bound to themselves, so an import name
                    // that is a keyword is a syntax error if it is not followed
                    // by the keyword 'as'.
                    if (IsKeyword(importName->name())) {
                        JSAutoByteString bytes;
                        if (!AtomToPrintableString(context, importName->name(), &bytes))
                            return null();
                        report(ParseError, false, null(), JSMSG_AS_AFTER_RESERVED_WORD, bytes.ptr());
                        return null();
                    }
                    tokenStream.ungetToken();
                }
                Node bindingName = newName(tokenStream.currentName());
                if (!bindingName)
                    return null();

                Node importSpec = handler.newBinary(PNK_IMPORT_SPEC, importName, bindingName);
                if (!importSpec)
                    return null();

                handler.addList(importSpecSet, importSpec);
            } while (tokenStream.matchToken(TOK_COMMA));

            MUST_MATCH_TOKEN(TOK_RC, JSMSG_RC_AFTER_IMPORT_SPEC_LIST);
        }

        if (tokenStream.getToken() != TOK_NAME ||
            tokenStream.currentName() != context->names().from)
        {
            report(ParseError, false, null(), JSMSG_FROM_AFTER_IMPORT_SPEC_SET);
            return null();
        }

        MUST_MATCH_TOKEN(TOK_STRING, JSMSG_MODULE_SPEC_AFTER_FROM);
    } else {
        if (tt != TOK_STRING) {
            report(ParseError, false, null(), JSMSG_DECLARATION_AFTER_IMPORT);
            return null();
        }

        // Handle the form |import 'a'| by leaving the list empty. This is
        // equivalent to |import {} from 'a'|.
        importSpecSet->pn_pos.end = importSpecSet->pn_pos.begin;
    }

    Node moduleSpec = stringLiteral();
    if (!moduleSpec)
        return null();

    if (!MatchOrInsertSemicolon(tokenStream))
        return null();

    return handler.newImportDeclaration(importSpecSet, moduleSpec,
                                        TokenPos(begin, pos().end));
}

template<>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::importDeclaration()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template<typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::exportDeclaration()
{
    JS_ASSERT(tokenStream.currentToken().type == TOK_EXPORT);

    if (pc->sc->isFunctionBox() || !pc->atBodyLevel()) {
        report(ParseError, false, null(), JSMSG_EXPORT_DECL_AT_TOP_LEVEL);
        return null();
    }

    uint32_t begin = pos().begin;

    Node kid;
    switch (TokenKind tt = tokenStream.getToken()) {
      case TOK_LC:
      case TOK_MUL:
        kid = handler.newList(PNK_EXPORT_SPEC_LIST);
        if (!kid)
            return null();

        if (tt == TOK_LC) {
            do {
                // Handle the forms |export {}| and |export { ..., }| (where ...
                // is non empty), by escaping the loop early if the next token
                // is }.
                tt = tokenStream.peekToken();
                if (tt == TOK_ERROR)
                    return null();
                if (tt == TOK_RC)
                    break;

                MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_BINDING_NAME);
                Node bindingName = newName(tokenStream.currentName());
                if (!bindingName)
                    return null();

                if (tokenStream.getToken() == TOK_NAME &&
                    tokenStream.currentName() == context->names().as)
                {
                    if (tokenStream.getToken(TokenStream::KeywordIsName) != TOK_NAME) {
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
            } while (tokenStream.matchToken(TOK_COMMA));

            MUST_MATCH_TOKEN(TOK_RC, JSMSG_RC_AFTER_EXPORT_SPEC_LIST);
        } else {
            // Handle the form |export *| by adding a special export batch
            // specifier to the list.
            Node exportSpec = handler.newNullary(PNK_EXPORT_BATCH_SPEC, JSOP_NOP, pos());
            if (!kid)
                return null();

            handler.addList(kid, exportSpec);
        }
        if (tokenStream.getToken() == TOK_NAME &&
            tokenStream.currentName() == context->names().from)
        {
            MUST_MATCH_TOKEN(TOK_STRING, JSMSG_MODULE_SPEC_AFTER_FROM);

            Node moduleSpec = stringLiteral();
            if (!moduleSpec)
                return null();

            if (!MatchOrInsertSemicolon(tokenStream))
                return null();

            return handler.newExportFromDeclaration(begin, kid, moduleSpec);
        } else {
            tokenStream.ungetToken();
        }

        kid = MatchOrInsertSemicolon(tokenStream) ? kid : nullptr;
        if (!kid)
            return null();
        break;

      case TOK_FUNCTION:
        kid = functionStmt();
        if (!kid)
            return null();
        break;

      case TOK_VAR:
      case TOK_CONST:
        kid = variables(tt == TOK_VAR ? PNK_VAR : PNK_CONST);
        if (!kid)
            return null();
        kid->pn_xflags = PNX_POPVAR;

        kid = MatchOrInsertSemicolon(tokenStream) ? kid : nullptr;
        if (!kid)
            return null();
        break;

      case TOK_NAME:
        // Handle the form |export a} in the same way as |export let a|, by
        // acting as if we've just seen the let keyword. Simply unget the token
        // and fall through.
        tokenStream.ungetToken();
      case TOK_LET:
        kid = letDeclaration();
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
Parser<ParseHandler>::expressionStatement()
{
    tokenStream.ungetToken();
    Node pnexpr = expr();
    if (!pnexpr)
        return null();
    if (!MatchOrInsertSemicolon(tokenStream))
        return null();
    return handler.newExprStatement(pnexpr, pos().end);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::ifStatement()
{
    uint32_t begin = pos().begin;

    /* An IF node has three kids: condition, then, and optional else. */
    Node cond = condition();
    if (!cond)
        return null();

    if (tokenStream.peekToken(TokenStream::Operand) == TOK_SEMI &&
        !report(ParseExtraWarning, false, null(), JSMSG_EMPTY_CONSEQUENT))
    {
        return null();
    }

    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_IF);
    Node thenBranch = statement();
    if (!thenBranch)
        return null();

    Node elseBranch;
    if (tokenStream.matchToken(TOK_ELSE, TokenStream::Operand)) {
        stmtInfo.type = STMT_ELSE;
        elseBranch = statement();
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
Parser<ParseHandler>::doWhileStatement()
{
    uint32_t begin = pos().begin;
    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_DO_LOOP);
    Node body = statement();
    if (!body)
        return null();
    MUST_MATCH_TOKEN(TOK_WHILE, JSMSG_WHILE_AFTER_DO);
    Node cond = condition();
    if (!cond)
        return null();
    PopStatementPC(tokenStream, pc);

    // The semicolon after do-while is even more optional than most
    // semicolons in JS.  Web compat required this by 2004:
    //   http://bugzilla.mozilla.org/show_bug.cgi?id=238945
    // ES3 and ES5 disagreed, but ES6 conforms to Web reality:
    //   https://bugs.ecmascript.org/show_bug.cgi?id=157
    tokenStream.matchToken(TOK_SEMI);
    return handler.newDoWhileStatement(body, cond, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::whileStatement()
{
    uint32_t begin = pos().begin;
    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_WHILE_LOOP);
    Node cond = condition();
    if (!cond)
        return null();
    Node body = statement();
    if (!body)
        return null();
    PopStatementPC(tokenStream, pc);
    return handler.newWhileStatement(begin, cond, body);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchInOrOf(bool *isForOfp)
{
    if (tokenStream.matchToken(TOK_IN)) {
        *isForOfp = false;
        return true;
    }
    if (tokenStream.matchContextualKeyword(context->names().of)) {
        *isForOfp = true;
        return true;
    }
    return false;
}

template <>
bool
Parser<FullParseHandler>::isValidForStatementLHS(ParseNode *pn1, JSVersion version,
                                                 bool isForDecl, bool isForEach,
                                                 ParseNodeKind headKind)
{
    if (isForDecl) {
        if (pn1->pn_count > 1)
            return false;
        if (pn1->isOp(JSOP_DEFCONST))
            return false;

        // In JS 1.7 only, for (var [K, V] in EXPR) has a special meaning.
        // Hence all other destructuring decls are banned there.
        if (version == JSVERSION_1_7 && !isForEach && headKind == PNK_FORIN) {
            ParseNode *lhs = pn1->pn_head;
            if (lhs->isKind(PNK_ASSIGN))
                lhs = lhs->pn_left;

            if (lhs->isKind(PNK_OBJECT))
                return false;
            if (lhs->isKind(PNK_ARRAY) && lhs->pn_count != 2)
                return false;
        }
        return true;
    }

    switch (pn1->getKind()) {
      case PNK_NAME:
      case PNK_DOT:
      case PNK_CALL:
      case PNK_ELEM:
        return true;

      case PNK_ARRAY:
      case PNK_OBJECT:
        // In JS 1.7 only, for ([K, V] in EXPR) has a special meaning.
        // Hence all other destructuring left-hand sides are banned there.
        if (version == JSVERSION_1_7 && !isForEach && headKind == PNK_FORIN)
            return pn1->isKind(PNK_ARRAY) && pn1->pn_count == 2;
        return true;

      default:
        return false;
    }
}

template <>
ParseNode *
Parser<FullParseHandler>::forStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));
    uint32_t begin = pos().begin;

    StmtInfoPC forStmt(context);
    PushStatementPC(pc, &forStmt, STMT_FOR_LOOP);

    bool isForEach = false;
    unsigned iflags = 0;

    if (allowsForEachIn() && tokenStream.matchContextualKeyword(context->names().each)) {
        iflags = JSITER_FOREACH;
        isForEach = true;
    }

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    /*
     * True if we have 'for (var/let/const ...)', except in the oddball case
     * where 'let' begins a let-expression in 'for (let (...) ...)'.
     */
    bool isForDecl = false;

    /* Non-null when isForDecl is true for a 'for (let ...)' statement. */
    RootedStaticBlockObject blockObj(context);

    /* Set to 'x' in 'for (x ;... ;...)' or 'for (x in ...)'. */
    ParseNode *pn1;

    {
        TokenKind tt = tokenStream.peekToken(TokenStream::Operand);
        if (tt == TOK_SEMI) {
            pn1 = nullptr;
        } else {
            /*
             * Set pn1 to a var list or an initializing expression.
             *
             * Set the parsingForInit flag during parsing of the first clause
             * of the for statement.  This flag will be used by the RelExpr
             * production; if it is set, then the 'in' keyword will not be
             * recognized as an operator, leaving it available to be parsed as
             * part of a for/in loop.
             *
             * A side effect of this restriction is that (unparenthesized)
             * expressions involving an 'in' operator are illegal in the init
             * clause of an ordinary for loop.
             */
            pc->parsingForInit = true;
            if (tt == TOK_VAR || tt == TOK_CONST) {
                isForDecl = true;
                tokenStream.consumeKnownToken(tt);
                pn1 = variables(tt == TOK_VAR ? PNK_VAR : PNK_CONST);
            }
            else if (tt == TOK_LET) {
                handler.disableSyntaxParser();
                (void) tokenStream.getToken();
                if (tokenStream.peekToken() == TOK_LP) {
                    pn1 = letBlock(LetExpresion);
                } else {
                    isForDecl = true;
                    blockObj = StaticBlockObject::create(context);
                    if (!blockObj)
                        return null();
                    pn1 = variables(PNK_LET, nullptr, blockObj, DontHoistVars);
                }
            }
            else {
                pn1 = expr();
            }
            pc->parsingForInit = false;
            if (!pn1)
                return null();
        }
    }

    JS_ASSERT_IF(isForDecl, pn1->isArity(PN_LIST));
    JS_ASSERT(!!blockObj == (isForDecl && pn1->isOp(JSOP_NOP)));

    // The form 'for (let <vars>; <expr2>; <expr3>) <stmt>' generates an
    // implicit block even if stmt is not a BlockStatement.
    // If the loop has that exact form, then:
    // - forLetImpliedBlock is the node for the implicit block scope.
    // - forLetDecl is the node for the decl 'let <vars>'.
    // Otherwise both are null.
    ParseNode *forLetImpliedBlock = nullptr;
    ParseNode *forLetDecl = nullptr;

    // If non-null, the node for the decl 'var v = expr1' in the weirdo form
    // 'for (var v = expr1 in expr2) stmt'.
    ParseNode *hoistedVar = nullptr;

    /*
     * We can be sure that it's a for/in loop if there's still an 'in'
     * keyword here, even if JavaScript recognizes 'in' as an operator,
     * as we've excluded 'in' from being parsed in RelExpr by setting
     * pc->parsingForInit.
     */
    StmtInfoPC letStmt(context); /* used if blockObj != nullptr. */
    ParseNode *pn2, *pn3;      /* forHead->pn_kid2 and pn_kid3. */
    ParseNodeKind headKind = PNK_FORHEAD;
    if (pn1) {
        bool isForOf;
        if (matchInOrOf(&isForOf))
            headKind = isForOf ? PNK_FOROF : PNK_FORIN;
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
            forStmt.type = STMT_FOR_OF_LOOP;
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
                /*
                 * Declaration with initializer.
                 *
                 * Rewrite 'for (<decl> x = i in o)' where <decl> is 'var' or
                 * 'const' to hoist the initializer or the entire decl out of
                 * the loop head.
                 */
                if (headKind == PNK_FOROF) {
                    report(ParseError, false, pn2, JSMSG_INVALID_FOR_OF_INIT);
                    return null();
                }
                if (blockObj) {
                    report(ParseError, false, pn2, JSMSG_INVALID_FOR_IN_INIT);
                    return null();
                }

                hoistedVar = pn1;

                /*
                 * All of 'var x = i' is hoisted above 'for (x in o)'.
                 *
                 * Request JSOP_POP here since the var is for a simple
                 * name (it is not a destructuring binding's left-hand
                 * side) and it has an initializer.
                 */
                pn1->pn_xflags |= PNX_POPVAR;
                pn1 = nullptr;

                if (pn2->isKind(PNK_ASSIGN)) {
                    pn2 = pn2->pn_left;
                    JS_ASSERT(pn2->isKind(PNK_ARRAY) || pn2->isKind(PNK_OBJECT) ||
                              pn2->isKind(PNK_NAME));
                }
            }
        } else {
            /* Not a declaration. */
            JS_ASSERT(!blockObj);
            pn2 = pn1;
            pn1 = nullptr;

            if (!checkAndMarkAsAssignmentLhs(pn2, PlainAssignment))
                return null();
        }

        pn3 = (headKind == PNK_FOROF) ? assignExpr() : expr();
        if (!pn3)
            return null();

        if (blockObj) {
            /*
             * Now that the pn3 has been parsed, push the let scope. To hold
             * the blockObj for the emitter, wrap the PNK_LEXICALSCOPE node
             * created by PushLetScope around the for's initializer. This also
             * serves to indicate the let-decl to the emitter.
             */
            ParseNode *block = pushLetScope(blockObj, &letStmt);
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

        switch (pn2->getKind()) {
          case PNK_NAME:
            /* Beware 'for (arguments in ...)' with or without a 'var'. */
            pn2->markAsAssigned();
            break;

          case PNK_ASSIGN:
            MOZ_ASSUME_UNREACHABLE("forStatement TOK_ASSIGN");

          case PNK_ARRAY:
          case PNK_OBJECT:
            if (versionNumber() == JSVERSION_1_7) {
                /*
                 * Destructuring for-in requires [key, value] enumeration
                 * in JS1.7.
                 */
                if (!isForEach && headKind == PNK_FORIN)
                    iflags |= JSITER_FOREACH | JSITER_KEYVALUE;
            }
            break;

          default:;
        }
    } else {
        if (isForEach) {
            reportWithOffset(ParseError, false, begin, JSMSG_BAD_FOR_EACH_LOOP);
            return null();
        }

        headKind = PNK_FORHEAD;

        if (blockObj) {
            /*
             * Desugar 'for (let A; B; C) D' into 'let (A) { for (; B; C) D }'
             * to induce the correct scoping for A.
             */
            forLetImpliedBlock = pushLetScope(blockObj, &letStmt);
            if (!forLetImpliedBlock)
                return null();
            letStmt.isForLetBlock = true;

            forLetDecl = pn1;
            pn1 = nullptr;
        }

        /* Parse the loop condition or null into pn2. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_INIT);
        if (tokenStream.peekToken(TokenStream::Operand) == TOK_SEMI) {
            pn2 = nullptr;
        } else {
            pn2 = expr();
            if (!pn2)
                return null();
        }

        /* Parse the update expression or null into pn3. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_COND);
        if (tokenStream.peekToken(TokenStream::Operand) == TOK_RP) {
            pn3 = nullptr;
        } else {
            pn3 = expr();
            if (!pn3)
                return null();
        }
    }

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

    TokenPos headPos(begin, pos().end);
    ParseNode *forHead = handler.newForHead(headKind, pn1, pn2, pn3, headPos);
    if (!forHead)
        return null();

    /* Parse the loop body. */
    ParseNode *body = statement();
    if (!body)
        return null();

    if (blockObj)
        PopStatementPC(tokenStream, pc);
    PopStatementPC(tokenStream, pc);

    ParseNode *forLoop = handler.newForStatement(begin, forHead, body, iflags);
    if (!forLoop)
        return null();

    if (hoistedVar) {
        ParseNode *pnseq = handler.newList(PNK_SEQ, hoistedVar);
        if (!pnseq)
            return null();
        pnseq->pn_pos = forLoop->pn_pos;
        pnseq->append(forLoop);
        return pnseq;
    }
    if (forLetImpliedBlock) {
        forLetImpliedBlock->pn_expr = forLoop;
        forLetImpliedBlock->pn_pos = forLoop->pn_pos;
        ParseNode *let = handler.newBinary(PNK_LET, forLetDecl, forLetImpliedBlock);
        if (!let)
            return null();
        let->pn_pos = forLoop->pn_pos;
        return let;
    }
    return forLoop;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::forStatement()
{
    /*
     * 'for' statement parsing is fantastically complicated and requires being
     * able to inspect the parse tree for previous parts of the 'for'. Syntax
     * parsing of 'for' statements is thus done separately, and only handles
     * the types of 'for' statements likely to be seen in web content.
     */
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    StmtInfoPC forStmt(context);
    PushStatementPC(pc, &forStmt, STMT_FOR_LOOP);

    /* Don't parse 'for each' loops. */
    if (allowsForEachIn()) {
        TokenKind tt = tokenStream.peekToken();
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
        TokenKind tt = tokenStream.peekToken(TokenStream::Operand);
        if (tt == TOK_SEMI) {
            lhsNode = null();
        } else {
            /* Set lhsNode to a var list or an initializing expression. */
            pc->parsingForInit = true;
            if (tt == TOK_VAR) {
                isForDecl = true;
                tokenStream.consumeKnownToken(tt);
                lhsNode = variables(tt == TOK_VAR ? PNK_VAR : PNK_CONST, &simpleForDecl);
            }
            else if (tt == TOK_CONST || tt == TOK_LET) {
                JS_ALWAYS_FALSE(abortIfSyntaxParser());
                return null();
            }
            else {
                lhsNode = expr();
            }
            if (!lhsNode)
                return null();
            pc->parsingForInit = false;
        }
    }

    /*
     * We can be sure that it's a for/in loop if there's still an 'in'
     * keyword here, even if JavaScript recognizes 'in' as an operator,
     * as we've excluded 'in' from being parsed in RelExpr by setting
     * pc->parsingForInit.
     */
    bool isForOf;
    if (lhsNode && matchInOrOf(&isForOf)) {
        /* Parse the rest of the for/in or for/of head. */
        forStmt.type = isForOf ? STMT_FOR_OF_LOOP : STMT_FOR_IN_LOOP;

        /* Check that the left side of the 'in' or 'of' is valid. */
        if (!isForDecl &&
            lhsNode != SyntaxParseHandler::NodeName &&
            lhsNode != SyntaxParseHandler::NodeGetProp &&
            lhsNode != SyntaxParseHandler::NodeLValue)
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

        if (!expr())
            return null();
    } else {
        /* Parse the loop condition or null. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_INIT);
        if (tokenStream.peekToken(TokenStream::Operand) != TOK_SEMI) {
            if (!expr())
                return null();
        }

        /* Parse the update expression or null. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_COND);
        if (tokenStream.peekToken(TokenStream::Operand) != TOK_RP) {
            if (!expr())
                return null();
        }
    }

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

    /* Parse the loop body. */
    if (!statement())
        return null();

    PopStatementPC(tokenStream, pc);
    return SyntaxParseHandler::NodeGeneric;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::switchStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_SWITCH));
    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_SWITCH);

    Node discriminant = exprInParens();
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
    while ((tt = tokenStream.getToken()) != TOK_RC) {
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
            caseExpr = expr();
            if (!caseExpr)
                return null();
            break;

          case TOK_ERROR:
            return null();

          default:
            report(ParseError, false, null(), JSMSG_BAD_SWITCH);
            return null();
        }

        MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_AFTER_CASE);

        Node body = handler.newStatementList(pc->blockid(), pos());
        if (!body)
            return null();

        while ((tt = tokenStream.peekToken(TokenStream::Operand)) != TOK_RC &&
               tt != TOK_CASE && tt != TOK_DEFAULT) {
            if (tt == TOK_ERROR)
                return null();
            Node stmt = statement();
            if (!stmt)
                return null();
            handler.addList(body, stmt);
        }

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
Parser<ParseHandler>::continueStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_CONTINUE));
    uint32_t begin = pos().begin;

    RootedPropertyName label(context);
    if (!matchLabel(&label))
        return null();

    StmtInfoPC *stmt = pc->topStmt;
    if (label) {
        for (StmtInfoPC *stmt2 = nullptr; ; stmt = stmt->down) {
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
Parser<ParseHandler>::breakStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_BREAK));
    uint32_t begin = pos().begin;

    RootedPropertyName label(context);
    if (!matchLabel(&label))
        return null();
    StmtInfoPC *stmt = pc->topStmt;
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
Parser<ParseHandler>::returnStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_RETURN));
    uint32_t begin = pos().begin;

    if (!pc->sc->isFunctionBox()) {
        report(ParseError, false, null(), JSMSG_BAD_RETURN_OR_YIELD, js_return_str);
        return null();
    }

    // Parse an optional operand.
    //
    // This is ugly, but we don't want to require a semicolon.
    Node exprNode;
    switch (tokenStream.peekTokenSameLine(TokenStream::Operand)) {
      case TOK_ERROR:
        return null();
      case TOK_EOF:
      case TOK_EOL:
      case TOK_SEMI:
      case TOK_RC:
        exprNode = null();
        pc->funHasReturnVoid = true;
        break;
      default: {
        exprNode = expr();
        if (!exprNode)
            return null();
        pc->funHasReturnExpr = true;
      }
    }

    if (!MatchOrInsertSemicolon(tokenStream))
        return null();

    Node pn = handler.newReturnStatement(exprNode, TokenPos(begin, pos().end));
    if (!pn)
        return null();

    if (options().extraWarningsOption && pc->funHasReturnExpr && pc->funHasReturnVoid &&
        !reportBadReturn(pn, ParseExtraWarning,
                         JSMSG_NO_RETURN_VALUE, JSMSG_ANON_NO_RETURN_VALUE))
    {
        return null();
    }

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
Parser<ParseHandler>::yieldExpression()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_YIELD));
    uint32_t begin = pos().begin;

    switch (pc->generatorKind()) {
      case StarGenerator:
      {
        JS_ASSERT(pc->sc->isFunctionBox());

        pc->lastYieldOffset = begin;

        Node exprNode;
        ParseNodeKind kind = PNK_YIELD;
        switch (tokenStream.peekTokenSameLine(TokenStream::Operand)) {
          case TOK_ERROR:
            return null();
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
            exprNode = assignExpr();
            if (!exprNode)
                return null();
        }
        return handler.newUnary(kind, JSOP_NOP, begin, exprNode);
      }

      case NotGenerator:
        // We are in code that has not seen a yield, but we are in JS 1.7 or
        // later.  Try to transition to being a legacy generator.
        JS_ASSERT(tokenStream.versionNumber() >= JSVERSION_1_7);
        JS_ASSERT(pc->lastYieldOffset == ParseContext<ParseHandler>::NoYieldOffset);

        if (!abortIfSyntaxParser())
            return null();

        if (!pc->sc->isFunctionBox()) {
            report(ParseError, false, null(), JSMSG_BAD_RETURN_OR_YIELD, js_yield_str);
            return null();
        }

        pc->sc->asFunctionBox()->setGeneratorKind(LegacyGenerator);

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
        JS_ASSERT(pc->sc->isFunctionBox());

        pc->lastYieldOffset = begin;

        // Legacy generators do not require a value.
        Node exprNode;
        switch (tokenStream.peekTokenSameLine(TokenStream::Operand)) {
          case TOK_ERROR:
            return null();
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
            exprNode = assignExpr();
            if (!exprNode)
                return null();
        }

        return handler.newUnary(PNK_YIELD, JSOP_NOP, begin, exprNode);
      }
    }

    MOZ_ASSUME_UNREACHABLE("yieldExpr");
}

template <>
ParseNode *
Parser<FullParseHandler>::withStatement()
{
    // test262/ch12/12.10/12.10-0-1.js fails if we try to parse with-statements
    // in syntax-parse mode. See bug 892583.
    if (handler.syntaxParser) {
        handler.disableSyntaxParser();
        abortedSyntaxParse = true;
        return null();
    }

    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_WITH));
    uint32_t begin = pos().begin;

    // In most cases, we want the constructs forbidden in strict mode code to be
    // a subset of those that JSOPTION_EXTRA_WARNINGS warns about, and we should
    // use reportStrictModeError.  However, 'with' is the sole instance of a
    // construct that is forbidden in strict mode code, but doesn't even merit a
    // warning under JSOPTION_EXTRA_WARNINGS.  See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=514576#c1.
    if (pc->sc->strict && !report(ParseStrictError, true, null(), JSMSG_STRICT_CODE_WITH))
        return null();

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_WITH);
    Node objectExpr = exprInParens();
    if (!objectExpr)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_WITH);

    bool oldParsingWith = pc->parsingWith;
    pc->parsingWith = true;

    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_WITH);
    Rooted<StaticWithObject *> staticWith(context, StaticWithObject::create(context));
    if (!staticWith)
        return null();
    staticWith->initEnclosingNestedScopeFromParser(pc->staticScope);
    FinishPushNestedScope(pc, &stmtInfo, *staticWith);

    Node innerBlock = statement();
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
        handler.deoptimizeUsesWithin(lexdep, TokenPos(begin, pos().begin));
    }

    ObjectBox *staticWithBox = newObjectBox(staticWith);
    if (!staticWithBox)
        return null();
    return handler.newWithStatement(begin, objectExpr, innerBlock, staticWithBox);
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::withStatement()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return null();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::labeledStatement()
{
    uint32_t begin = pos().begin;
    RootedPropertyName label(context, tokenStream.currentName());
    for (StmtInfoPC *stmt = pc->topStmt; stmt; stmt = stmt->down) {
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
    Node pn = statement();
    if (!pn)
        return null();

    /* Pop the label, set pn_expr, and return early. */
    PopStatementPC(tokenStream, pc);

    return handler.newLabeledStatement(label, pn, begin);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::throwStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_THROW));
    uint32_t begin = pos().begin;

    /* ECMA-262 Edition 3 says 'throw [no LineTerminator here] Expr'. */
    TokenKind tt = tokenStream.peekTokenSameLine(TokenStream::Operand);
    if (tt == TOK_ERROR)
        return null();
    if (tt == TOK_EOF || tt == TOK_EOL || tt == TOK_SEMI || tt == TOK_RC) {
        report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
        return null();
    }

    Node throwExpr = expr();
    if (!throwExpr)
        return null();

    if (!MatchOrInsertSemicolon(tokenStream))
        return null();

    return handler.newThrowStatement(throwExpr, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::tryStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_TRY));
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
    Node innerBlock = statements();
    if (!innerBlock)
        return null();
    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_TRY);
    PopStatementPC(tokenStream, pc);

    bool hasUnconditionalCatch = false;
    Node catchList = null();
    TokenKind tt = tokenStream.getToken();
    if (tt == TOK_CATCH) {
        catchList = handler.newList(PNK_CATCH);
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
            data.initLet(HoistVars, pc->staticScope->template as<StaticBlockObject>(),
                         JSMSG_TOO_MANY_CATCH_VARS);
            JS_ASSERT(data.let.blockObj);

            tt = tokenStream.getToken();
            Node catchName;
            switch (tt) {
              case TOK_LB:
              case TOK_LC:
                catchName = destructuringExpr(&data, tt);
                if (!catchName)
                    return null();
                break;

              case TOK_YIELD:
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
            if (tokenStream.matchToken(TOK_IF)) {
                catchGuard = expr();
                if (!catchGuard)
                    return null();
            }
#endif
            MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_CATCH);

            MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_CATCH);
            Node catchBody = statements();
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

            tt = tokenStream.getToken(TokenStream::Operand);
        } while (tt == TOK_CATCH);
    }

    Node finallyBlock = null();

    if (tt == TOK_FINALLY) {
        MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_FINALLY);
        if (!PushBlocklikeStatement(tokenStream, &stmtInfo, STMT_FINALLY, pc))
            return null();
        finallyBlock = statements();
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

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::statement(bool canHaveDirectives)
{
    JS_CHECK_RECURSION(context, return null());

    switch (TokenKind tt = tokenStream.getToken(TokenStream::Operand)) {
      case TOK_LC:
        return blockStatement();

      case TOK_CONST:
        if (!abortIfSyntaxParser())
            return null();
        // FALL THROUGH
      case TOK_VAR: {
        Node pn = variables(tt == TOK_CONST ? PNK_CONST : PNK_VAR);
        if (!pn)
            return null();

        // Tell js_EmitTree to generate a final POP.
        handler.setListFlag(pn, PNX_POPVAR);

        if (!MatchOrInsertSemicolon(tokenStream))
            return null();
        return pn;
      }

      case TOK_LET:
        return letStatement();
      case TOK_IMPORT:
        return importDeclaration();
      case TOK_EXPORT:
        return exportDeclaration();
      case TOK_SEMI:
        return handler.newEmptyStatement(pos());
      case TOK_IF:
        return ifStatement();
      case TOK_DO:
        return doWhileStatement();
      case TOK_WHILE:
        return whileStatement();
      case TOK_FOR:
        return forStatement();
      case TOK_SWITCH:
        return switchStatement();
      case TOK_CONTINUE:
        return continueStatement();
      case TOK_BREAK:
        return breakStatement();
      case TOK_RETURN:
        return returnStatement();
      case TOK_WITH:
        return withStatement();
      case TOK_THROW:
        return throwStatement();
      case TOK_TRY:
        return tryStatement();
      case TOK_FUNCTION:
        return functionStmt();
      case TOK_DEBUGGER:
        return debuggerStatement();

      /* TOK_CATCH and TOK_FINALLY are both handled in the TOK_TRY case */
      case TOK_CATCH:
        report(ParseError, false, null(), JSMSG_CATCH_WITHOUT_TRY);
        return null();

      case TOK_FINALLY:
        report(ParseError, false, null(), JSMSG_FINALLY_WITHOUT_TRY);
        return null();

      case TOK_ERROR:
        return null();

      case TOK_STRING:
        if (!canHaveDirectives && tokenStream.currentToken().atom() == context->names().useAsm) {
            if (!abortIfSyntaxParser())
                return null();
            if (!report(ParseWarning, false, null(), JSMSG_USE_ASM_DIRECTIVE_FAIL))
                return null();
        }
        return expressionStatement();

      case TOK_YIELD:
        if (tokenStream.peekToken() == TOK_COLON) {
            if (!checkYieldNameValidity())
                return null();
            return labeledStatement();
        }
        return expressionStatement();

      case TOK_NAME:
        if (tokenStream.peekToken() == TOK_COLON)
            return labeledStatement();
        return expressionStatement();

      default:
        return expressionStatement();
    }
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::expr()
{
    Node pn = assignExpr();
    if (pn && tokenStream.matchToken(TOK_COMMA)) {
        Node seq = handler.newList(PNK_COMMA, pn);
        if (!seq)
            return null();
        do {
            if (handler.isUnparenthesizedYield(pn)) {
                report(ParseError, false, pn, JSMSG_BAD_GENERATOR_SYNTAX, js_yield_str);
                return null();
            }

            pn = assignExpr();
            if (!pn)
                return null();
            handler.addList(seq, pn);
        } while (tokenStream.matchToken(TOK_COMMA));
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
    JS_ASSERT(pnk >= PNK_BINOP_FIRST);
    JS_ASSERT(pnk <= PNK_BINOP_LAST);
    return ParseNodeKindToJSOp[pnk - PNK_BINOP_FIRST];
}

static bool
IsBinaryOpToken(TokenKind tok, bool parsingForInit)
{
    return tok == TOK_IN ? !parsingForInit : TokenKindIsBinaryOp(tok);
}

static ParseNodeKind
BinaryOpTokenKindToParseNodeKind(TokenKind tok)
{
    JS_ASSERT(TokenKindIsBinaryOp(tok));
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

    JS_ASSERT(pnk >= PNK_BINOP_FIRST);
    JS_ASSERT(pnk <= PNK_BINOP_LAST);
    return PrecedenceTable[pnk - PNK_BINOP_FIRST];
}

template <typename ParseHandler>
MOZ_ALWAYS_INLINE typename ParseHandler::Node
Parser<ParseHandler>::orExpr1()
{
    // Shift-reduce parser for the left-associative binary operator part of
    // the JS syntax.

    // Conceptually there's just one stack, a stack of pairs (lhs, op).
    // It's implemented using two separate arrays, though.
    Node nodeStack[PRECEDENCE_CLASSES];
    ParseNodeKind kindStack[PRECEDENCE_CLASSES];
    int depth = 0;

    bool oldParsingForInit = pc->parsingForInit;
    pc->parsingForInit = false;

    Node pn;
    for (;;) {
        pn = unaryExpr();
        if (!pn)
            return pn;

        // If a binary operator follows, consume it and compute the
        // corresponding operator.
        TokenKind tok = tokenStream.getToken();
        if (tok == TOK_ERROR)
            return null();
        ParseNodeKind pnk;
        if (IsBinaryOpToken(tok, oldParsingForInit)) {
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
            pn = handler.newBinaryOrAppend(combiningPnk, nodeStack[depth], pn, pc, combiningOp);
            if (!pn)
                return pn;
        }

        if (pnk == PNK_LIMIT)
            break;

        nodeStack[depth] = pn;
        kindStack[depth] = pnk;
        depth++;
        JS_ASSERT(depth <= PRECEDENCE_CLASSES);
    }

    JS_ASSERT(depth == 0);
    pc->parsingForInit = oldParsingForInit;
    return pn;
}

template <typename ParseHandler>
MOZ_ALWAYS_INLINE typename ParseHandler::Node
Parser<ParseHandler>::condExpr1()
{
    Node condition = orExpr1();
    if (!condition || !tokenStream.isCurrentTokenType(TOK_HOOK))
        return condition;

    /*
     * Always accept the 'in' operator in the middle clause of a ternary,
     * where it's unambiguous, even if we might be parsing the init of a
     * for statement.
     */
    bool oldParsingForInit = pc->parsingForInit;
    pc->parsingForInit = false;
    Node thenExpr = assignExpr();
    pc->parsingForInit = oldParsingForInit;
    if (!thenExpr)
        return null();

    MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_IN_COND);

    Node elseExpr = assignExpr();
    if (!elseExpr)
        return null();

    tokenStream.getToken(); /* read one token past the end */
    return handler.newConditional(condition, thenExpr, elseExpr);
}

template <>
bool
Parser<FullParseHandler>::checkAndMarkAsAssignmentLhs(ParseNode *pn, AssignmentFlavor flavor)
{
    switch (pn->getKind()) {
      case PNK_NAME:
        if (!checkStrictAssignment(pn, flavor))
            return false;
        if (flavor == KeyedDestructuringAssignment) {
            /*
             * We may be called on a name node that has already been
             * specialized, in the very weird "for (var [x] = i in o) ..."
             * case. See bug 558633.
             */
            if (!(js_CodeSpec[pn->getOp()].format & JOF_SET))
                pn->setOp(JSOP_SETNAME);
        } else {
            pn->setOp(pn->isOp(JSOP_GETLOCAL) ? JSOP_SETLOCAL : JSOP_SETNAME);
        }
        pn->markAsAssigned();
        break;

      case PNK_DOT:
      case PNK_ELEM:
        break;

      case PNK_ARRAY:
      case PNK_OBJECT:
        if (flavor == CompoundAssignment) {
            report(ParseError, false, null(), JSMSG_BAD_DESTRUCT_ASS);
            return false;
        }
        if (!checkDestructuring(nullptr, pn))
            return false;
        break;

      case PNK_CALL:
        if (!makeSetCall(pn, JSMSG_BAD_LEFTSIDE_OF_ASS))
            return false;
        break;

      default:
        report(ParseError, false, pn, JSMSG_BAD_LEFTSIDE_OF_ASS);
        return false;
    }
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkAndMarkAsAssignmentLhs(Node pn, AssignmentFlavor flavor)
{
    /* Full syntax checking of valid assignment LHS terms requires a parse tree. */
    if (pn != SyntaxParseHandler::NodeName &&
        pn != SyntaxParseHandler::NodeGetProp &&
        pn != SyntaxParseHandler::NodeLValue)
    {
        return abortIfSyntaxParser();
    }
    return checkStrictAssignment(pn, flavor);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::assignExpr()
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

    TokenKind tt = tokenStream.getToken(TokenStream::Operand);

    if (tt == TOK_NAME && tokenStream.nextTokenEndsExpr())
        return identifierName();

    if (tt == TOK_NUMBER && tokenStream.nextTokenEndsExpr())
        return newNumber(tokenStream.currentToken());

    if (tt == TOK_STRING && tokenStream.nextTokenEndsExpr())
        return stringLiteral();

    if (tt == TOK_YIELD && (versionNumber() >= JSVERSION_1_7 || pc->isGenerator()))
        return yieldExpression();

    tokenStream.ungetToken();

    // Save the tokenizer state in case we find an arrow function and have to
    // rewind.
    TokenStream::Position start(keepAtoms);
    tokenStream.tell(&start);

    Node lhs = condExpr1();
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
        tokenStream.seek(start);
        if (!abortIfSyntaxParser())
            return null();

        if (tokenStream.getToken() == TOK_ERROR)
            return null();
        tokenStream.ungetToken();

        return functionDef(NullPtr(), start, Normal, Arrow, NotGenerator);
      }

      default:
        JS_ASSERT(!tokenStream.isCurrentTokenAssignment());
        tokenStream.ungetToken();
        return lhs;
    }

    AssignmentFlavor flavor = kind == PNK_ASSIGN ? PlainAssignment : CompoundAssignment;
    if (!checkAndMarkAsAssignmentLhs(lhs, flavor))
        return null();

    Node rhs = assignExpr();
    if (!rhs)
        return null();

    return handler.newBinaryOrAppend(kind, lhs, rhs, pc, op);
}

static const char incop_name_str[][10] = {"increment", "decrement"};

template <>
bool
Parser<FullParseHandler>::checkAndMarkAsIncOperand(ParseNode *kid, TokenKind tt, bool preorder)
{
    // Check.
    if (!kid->isKind(PNK_NAME) &&
        !kid->isKind(PNK_DOT) &&
        !kid->isKind(PNK_ELEM) &&
        !(kid->isKind(PNK_CALL) &&
          (kid->isOp(JSOP_CALL) || kid->isOp(JSOP_SPREADCALL) ||
           kid->isOp(JSOP_EVAL) || kid->isOp(JSOP_SPREADEVAL) ||
           kid->isOp(JSOP_FUNCALL) ||
           kid->isOp(JSOP_FUNAPPLY))))
    {
        report(ParseError, false, null(), JSMSG_BAD_OPERAND, incop_name_str[tt == TOK_DEC]);
        return false;
    }

    if (!checkStrictAssignment(kid, IncDecAssignment))
        return false;

    // Mark.
    if (kid->isKind(PNK_NAME)) {
        kid->markAsAssigned();
    } else if (kid->isKind(PNK_CALL)) {
        if (!makeSetCall(kid, JSMSG_BAD_INCOP_OPERAND))
            return false;
    }
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkAndMarkAsIncOperand(Node kid, TokenKind tt, bool preorder)
{
    // To the extent of what we support in syntax-parse mode, the rules for
    // inc/dec operands are the same as for assignment. There are differences,
    // such as destructuring; but if we hit any of those cases, we'll abort and
    // reparse in full mode.
    return checkAndMarkAsAssignmentLhs(kid, IncDecAssignment);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::unaryOpExpr(ParseNodeKind kind, JSOp op, uint32_t begin)
{
    Node kid = unaryExpr();
    if (!kid)
        return null();
    return handler.newUnary(kind, op, begin, kid);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::unaryExpr()
{
    Node pn, pn2;

    JS_CHECK_RECURSION(context, return null());

    TokenKind tt = tokenStream.getToken(TokenStream::Operand);
    uint32_t begin = pos().begin;
    switch (tt) {
      case TOK_TYPEOF:
        return unaryOpExpr(PNK_TYPEOF, JSOP_TYPEOF, begin);
      case TOK_VOID:
        return unaryOpExpr(PNK_VOID, JSOP_VOID, begin);
      case TOK_NOT:
        return unaryOpExpr(PNK_NOT, JSOP_NOT, begin);
      case TOK_BITNOT:
        return unaryOpExpr(PNK_BITNOT, JSOP_BITNOT, begin);
      case TOK_ADD:
        return unaryOpExpr(PNK_POS, JSOP_POS, begin);
      case TOK_SUB:
        return unaryOpExpr(PNK_NEG, JSOP_NEG, begin);

      case TOK_INC:
      case TOK_DEC:
      {
        TokenKind tt2 = tokenStream.getToken(TokenStream::Operand);
        pn2 = memberExpr(tt2, true);
        if (!pn2)
            return null();
        if (!checkAndMarkAsIncOperand(pn2, tt, true))
            return null();
        return handler.newUnary((tt == TOK_INC) ? PNK_PREINCREMENT : PNK_PREDECREMENT,
                                JSOP_NOP,
                                begin,
                                pn2);
      }

      case TOK_DELETE: {
        Node expr = unaryExpr();
        if (!expr)
            return null();

        // Per spec, deleting any unary expression is valid -- it simply
        // returns true -- except for one case that is illegal in strict mode.
        if (handler.isName(expr)) {
            if (!report(ParseStrictError, pc->sc->strict, expr, JSMSG_DEPRECATED_DELETE_OPERAND))
                return null();
            pc->sc->setBindingsAccessedDynamically();
        }

        return handler.newDelete(begin, expr);
      }

      case TOK_ERROR:
        return null();

      default:
        pn = memberExpr(tt, true);
        if (!pn)
            return null();

        /* Don't look across a newline boundary for a postfix incop. */
        tt = tokenStream.peekTokenSameLine(TokenStream::Operand);
        if (tt == TOK_INC || tt == TOK_DEC) {
            tokenStream.consumeKnownToken(tt);
            if (!checkAndMarkAsIncOperand(pn, tt, false))
                return null();
            return handler.newUnary((tt == TOK_INC) ? PNK_POSTINCREMENT : PNK_POSTDECREMENT,
                                    JSOP_NOP,
                                    begin,
                                    pn);
        }
        return pn;
    }
    MOZ_ASSUME_UNREACHABLE("unaryExpr");
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
    ParseNode       *root;
    Parser<FullParseHandler> *parser;
    ParseContext<FullParseHandler> *outerpc;
    GeneratorKind   comprehensionKind;
    unsigned        adjust;
    HashSet<Definition *> visitedImplicitArguments;

  public:
    LegacyCompExprTransplanter(ParseNode *pn, Parser<FullParseHandler> *parser,
                               ParseContext<FullParseHandler> *outerpc,
                               GeneratorKind kind, unsigned adj)
      : root(pn), parser(parser), outerpc(outerpc), comprehensionKind(kind), adjust(adj),
        visitedImplicitArguments(parser->context)
    {}

    bool init() {
        return visitedImplicitArguments.init();
    }

    bool transplant(ParseNode *pn);
};

/*
 * Any definitions nested within the legacy comprehension expression of a
 * generator expression must move "down" one static level, which of course
 * increases the upvar-frame-skip count.
 */
template <typename ParseHandler>
static bool
BumpStaticLevel(TokenStream &ts, ParseNode *pn, ParseContext<ParseHandler> *pc)
{
    if (pn->pn_cookie.isFree())
        return true;

    unsigned level = unsigned(pn->pn_cookie.level()) + 1;
    JS_ASSERT(level >= pc->staticLevel);
    return pn->pn_cookie.set(ts, level, pn->pn_cookie.slot());
}

template <typename ParseHandler>
static bool
AdjustBlockId(TokenStream &ts, ParseNode *pn, unsigned adjust, ParseContext<ParseHandler> *pc)
{
    JS_ASSERT(pn->isArity(PN_LIST) || pn->isArity(PN_CODE) || pn->isArity(PN_NAME));
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
LegacyCompExprTransplanter::transplant(ParseNode *pn)
{
    ParseContext<FullParseHandler> *pc = parser->pc;

    bool isGenexp = comprehensionKind != NotGenerator;

    if (!pn)
        return true;

    switch (pn->getArity()) {
      case PN_LIST:
        for (ParseNode *pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
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
            JS_ASSERT(pn->pn_cookie.isFree());

            Definition *dn = pn->pn_lexdef;
            JS_ASSERT(dn->isDefn());

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
            StmtInfoPC *stmt = LexicalLookup(pc, atom, nullptr, (StmtInfoPC *)nullptr);
            JS_ASSERT(!stmt || stmt != pc->topStmt);
#endif
            if (isGenexp && !dn->isOp(JSOP_CALLEE)) {
                JS_ASSERT(!pc->decls().lookupFirst(atom));

                if (dn->pn_pos < root->pn_pos) {
                    /*
                     * The variable originally appeared to be a use of a
                     * definition or placeholder outside the generator, but now
                     * we know it is scoped within the legacy comprehension
                     * tail's clauses. Make it (along with any other uses within
                     * the generator) a use of a new placeholder in the
                     * generator's lexdeps.
                     */
                    Definition *dn2 = parser->handler.newPlaceholder(atom, parser->pc->blockid(),
                                                                     parser->pos());
                    if (!dn2)
                        return false;
                    dn2->pn_pos = root->pn_pos;

                    /*
                     * Change all uses of |dn| that lie within the generator's
                     * |yield| expression into uses of dn2.
                     */
                    ParseNode **pnup = &dn->dn_uses;
                    ParseNode *pnu;
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
// whenever we leave a statement, via AccumulateBlockScopeDepth.  However if the
// head has a let expression, we need to re-assign that depth to the tail of the
// comprehension.
//
// Thing is, we don't actually know what that depth is, because the only
// information we keep is the maximum nested depth within a statement, so we
// just conservatively propagate the maximum nested depth from the top statement
// to the comprehension tail.
//
template <typename ParseHandler>
static unsigned
LegacyComprehensionHeadBlockScopeDepth(ParseContext<ParseHandler> *pc)
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
ParseNode *
Parser<FullParseHandler>::legacyComprehensionTail(ParseNode *bodyStmt, unsigned blockid,
                                                  GeneratorKind comprehensionKind,
                                                  ParseContext<FullParseHandler> *outerpc,
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
    ParseNode *pn, *pn2, *pn3, **pnp;
    StmtInfoPC stmtInfo(context);
    BindData<FullParseHandler> data(context);
    TokenKind tt;

    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    bool isGenexp = comprehensionKind != NotGenerator;

    if (isGenexp) {
        JS_ASSERT(comprehensionKind == LegacyGenerator);
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
         * In the case of a comprehension expression that has nested blocks
         * (e.g., let expressions), we will allocate a higher blockid but then
         * slide all blocks "to the right" to make room for the comprehension's
         * block scope.
         */
        adjust = pc->blockid();
        pn = pushLexicalScope(&stmtInfo);
        if (!pn)
            return null();

        JS_ASSERT(blockid <= pn->pn_blockid);
        JS_ASSERT(blockid < pc->blockidGen);
        JS_ASSERT(pc->bodyid < blockid);
        pn->pn_blockid = stmtInfo.blockid = blockid;
        JS_ASSERT(adjust < blockid);
        adjust = blockid - adjust;
    }

    handler.setBeginPosition(pn, bodyStmt);

    pnp = &pn->pn_expr;

    LegacyCompExprTransplanter transplanter(bodyStmt, this, outerpc, comprehensionKind, adjust);
    if (!transplanter.init())
        return null();

    if (!transplanter.transplant(bodyStmt))
        return null();

    JS_ASSERT(pc->staticScope && pc->staticScope == pn->pn_objbox->object);
    data.initLet(HoistVars, pc->staticScope->as<StaticBlockObject>(), JSMSG_ARRAY_INIT_TOO_BIG);

    do {
        /*
         * FOR node is binary, left is loop control and right is body.  Use
         * index to count each block-local let-variable on the left-hand side
         * of the in/of.
         */
        pn2 = BinaryNode::create(PNK_FOR, &handler);
        if (!pn2)
            return null();

        pn2->setOp(JSOP_ITER);
        pn2->pn_iflags = JSITER_ENUMERATE;
        if (allowsForEachIn() && tokenStream.matchContextualKeyword(context->names().each))
            pn2->pn_iflags |= JSITER_FOREACH;
        MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

        uint32_t startYieldOffset = pc->lastYieldOffset;

        RootedPropertyName name(context);
        tt = tokenStream.getToken();
        switch (tt) {
          case TOK_LB:
          case TOK_LC:
            pc->inDeclDestructuring = true;
            pn3 = primaryExpr(tt);
            pc->inDeclDestructuring = false;
            if (!pn3)
                return null();
            break;

          case TOK_NAME:
            name = tokenStream.currentName();

            /*
             * Create a name node with pn_op JSOP_NAME.  We can't set pn_op to
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

          case TOK_ERROR:
            return null();
        }

        bool isForOf;
        if (!matchInOrOf(&isForOf)) {
            report(ParseError, false, null(), JSMSG_IN_AFTER_FOR_NAME);
            return null();
        }
        ParseNodeKind headKind = PNK_FORIN;
        if (isForOf) {
            if (pn2->pn_iflags != JSITER_ENUMERATE) {
                JS_ASSERT(pn2->pn_iflags == (JSITER_FOREACH | JSITER_ENUMERATE));
                report(ParseError, false, null(), JSMSG_BAD_FOR_EACH_LOOP);
                return null();
            }
            pn2->pn_iflags = 0;
            headKind = PNK_FOROF;
        }

        ParseNode *pn4 = expr();
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
            if (!checkDestructuring(&data, pn3))
                return null();

            if (versionNumber() == JSVERSION_1_7 &&
                !(pn2->pn_iflags & JSITER_FOREACH) &&
                !isForOf)
            {
                /* Destructuring requires [key, value] enumeration in JS1.7. */
                if (!pn3->isKind(PNK_ARRAY) || pn3->pn_count != 2) {
                    report(ParseError, false, null(), JSMSG_BAD_FOR_LEFTSIDE);
                    return null();
                }

                JS_ASSERT(pn2->isOp(JSOP_ITER));
                JS_ASSERT(pn2->pn_iflags & JSITER_ENUMERATE);
                JS_ASSERT(headKind == PNK_FORIN);
                pn2->pn_iflags |= JSITER_FOREACH | JSITER_KEYVALUE;
            }
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
         */
        ParseNode *vars = ListNode::create(PNK_VAR, &handler);
        if (!vars)
            return null();
        vars->setOp(JSOP_NOP);
        vars->pn_pos = pn3->pn_pos;
        vars->makeEmpty();
        vars->append(pn3);

        /* Definitions can't be passed directly to EmitAssignment as lhs. */
        pn3 = cloneLeftHandSide(pn3);
        if (!pn3)
            return null();

        pn2->pn_left = handler.newTernary(headKind, vars, pn3, pn4);
        if (!pn2->pn_left)
            return null();
        *pnp = pn2;
        pnp = &pn2->pn_right;
    } while (tokenStream.matchToken(TOK_FOR));

    if (tokenStream.matchToken(TOK_IF)) {
        pn2 = TernaryNode::create(PNK_IF, &handler);
        if (!pn2)
            return null();
        pn2->pn_kid1 = condition();
        if (!pn2->pn_kid1)
            return null();
        *pnp = pn2;
        pnp = &pn2->pn_kid2;
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
                                                    ParseContext<SyntaxParseHandler> *outerpc,
                                                    unsigned innerBlockScopeDepth)
{
    abortIfSyntaxParser();
    return null();
}

template <>
ParseNode*
Parser<FullParseHandler>::legacyArrayComprehension(ParseNode *array)
{
    array->setKind(PNK_ARRAYCOMP);

    // Remove the single element from array's linked list, leaving us with an
    // empty array literal and a comprehension expression.
    JS_ASSERT(array->pn_count == 1);
    ParseNode *bodyExpr = array->last();
    array->pn_count = 0;
    array->pn_tail = &array->pn_head;
    *array->pn_tail = nullptr;

    ParseNode *arrayPush = handler.newUnary(PNK_ARRAYPUSH, JSOP_ARRAYPUSH,
                                            bodyExpr->pn_pos.begin, bodyExpr);
    if (!arrayPush)
        return null();

    ParseNode *comp = legacyComprehensionTail(arrayPush, array->pn_blockid, NotGenerator,
                                              nullptr, LegacyComprehensionHeadBlockScopeDepth(pc));
    if (!comp)
        return null();

    MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_ARRAY_COMPREHENSION);

    TokenPos p = handler.getPosition(array);
    p.end = pos().end;
    return handler.newArrayComprehension(comp, array->pn_blockid, p);
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
                                                   unsigned begin, Node innerStmt)
{
    JS_ASSERT(comprehensionKind == LegacyGenerator || comprehensionKind == StarGenerator);
    JS_ASSERT(!!innerStmt == (comprehensionKind == LegacyGenerator));

    Node genfn = handler.newFunctionDefinition();
    if (!genfn)
        return null();
    handler.setOp(genfn, JSOP_LAMBDA);

    ParseContext<ParseHandler> *outerpc = pc;

    // If we are off the main thread, the generator meta-objects have
    // already been created by js::StartOffThreadParseScript, so cx will not
    // be necessary.
    RootedObject proto(context);
    if (comprehensionKind == StarGenerator) {
        JSContext *cx = context->maybeJSContext();
        proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, context->global());
        if (!proto)
            return null();
    }

    RootedFunction fun(context, newFunction(outerpc, /* atom = */ NullPtr(), Expression, proto));
    if (!fun)
        return null();

    // Create box for fun->object early to root it.
    Directives directives(/* strict = */ outerpc->sc->strict);
    FunctionBox *genFunbox = newFunctionBox(genfn, fun, outerpc, directives, comprehensionKind);
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

    JS_ASSERT(genFunbox->generatorKind() == comprehensionKind);
    genFunbox->inGenexpLambda = true;
    handler.setBlockId(genfn, genpc.bodyid);

    Node body;

    if (comprehensionKind == StarGenerator) {
        body = comprehension(StarGenerator);
        if (!body)
            return null();
    } else {
        JS_ASSERT(comprehensionKind == LegacyGenerator);
        body = legacyComprehensionTail(innerStmt, outerpc->blockid(), LegacyGenerator,
                                       outerpc, LegacyComprehensionHeadBlockScopeDepth(outerpc));
        if (!body)
            return null();
    }

    if (comprehensionKind == StarGenerator)
        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);

    handler.setBeginPosition(body, begin);
    handler.setEndPosition(body, pos().end);

    handler.setBeginPosition(genfn, begin);
    handler.setEndPosition(genfn, pos().end);

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
ParseNode *
Parser<FullParseHandler>::legacyGeneratorExpr(ParseNode *expr)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    /* Create a |yield| node for |kid|. */
    ParseNode *yieldExpr = handler.newUnary(PNK_YIELD, JSOP_NOP, expr->pn_pos.begin, expr);
    if (!yieldExpr)
        return null();
    yieldExpr->setInParens(true);

    // A statement to wrap the yield expression.
    ParseNode *yieldStmt = handler.newExprStatement(yieldExpr, expr->pn_pos.end);
    if (!yieldStmt)
        return null();

    /* Make a new node for the desugared generator function. */
    ParseNode *genfn = generatorComprehensionLambda(LegacyGenerator, expr->pn_pos.begin, yieldStmt);
    if (!genfn)
        return null();

    /*
     * Our result is a call expression that invokes the anonymous generator
     * function object.
     */
    ParseNode *result = ListNode::create(PNK_GENEXP, &handler);
    if (!result)
        return null();
    result->setOp(JSOP_CALL);
    result->pn_pos.begin = genfn->pn_pos.begin;
    result->initList(genfn);
    return result;
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
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    // FIXME: Destructuring binding (bug 980828).

    MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_VARIABLE_NAME);
    RootedPropertyName name(context, tokenStream.currentName());
    if (name == context->names().let) {
        report(ParseError, false, null(), JSMSG_LET_COMP_BINDING);
        return null();
    }
    if (!tokenStream.matchContextualKeyword(context->names().of)) {
        report(ParseError, false, null(), JSMSG_OF_AFTER_FOR_NAME);
        return null();
    }

    Node rhs = assignExpr();
    if (!rhs)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_OF_ITERABLE);

    TokenPos headPos(begin, pos().end);

    StmtInfoPC stmtInfo(context);
    BindData<ParseHandler> data(context);
    RootedStaticBlockObject blockObj(context, StaticBlockObject::create(context));
    if (!blockObj)
        return null();
    data.initLet(DontHoistVars, *blockObj, JSMSG_TOO_MANY_LOCALS);
    Node lhs = newName(name);
    if (!lhs)
        return null();
    Node decls = handler.newList(PNK_LET, lhs, JSOP_NOP);
    if (!decls)
        return null();
    data.pn = lhs;
    if (!data.binder(&data, name, this))
        return null();
    Node letScope = pushLetScope(blockObj, &stmtInfo);
    if (!letScope)
        return null();
    handler.setLexicalScopeBody(letScope, decls);

    Node assignLhs = newName(name);
    if (!assignLhs)
        return null();
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
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_IF));

    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_COND);
    Node cond = assignExpr();
    if (!cond)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_COND);

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    if (handler.isOperationWithoutParens(cond, PNK_ASSIGN) &&
        !report(ParseExtraWarning, false, null(), JSMSG_EQUAL_AS_ASSIGN))
    {
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

    if (tokenStream.matchToken(TOK_FOR, TokenStream::Operand))
        return comprehensionFor(comprehensionKind);

    if (tokenStream.matchToken(TOK_IF, TokenStream::Operand))
        return comprehensionIf(comprehensionKind);

    uint32_t begin = pos().begin;

    Node bodyExpr = assignExpr();
    if (!bodyExpr)
        return null();

    if (comprehensionKind == NotGenerator)
        return handler.newUnary(PNK_ARRAYPUSH, JSOP_ARRAYPUSH, begin, bodyExpr);

    JS_ASSERT(comprehensionKind == StarGenerator);
    Node yieldExpr = handler.newUnary(PNK_YIELD, JSOP_NOP, begin, bodyExpr);
    if (!yieldExpr)
        return null();
    handler.setInParens(yieldExpr);

    return handler.newExprStatement(yieldExpr, pos().end);
}

// Parse an ES6 generator or array comprehension, starting at the first 'for'.
// The caller is responsible for matching the ending TOK_RP or TOK_RB.
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehension(GeneratorKind comprehensionKind)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

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
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

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
Parser<ParseHandler>::assignExprWithoutYield(unsigned msg)
{
    uint32_t startYieldOffset = pc->lastYieldOffset;
    Node res = assignExpr();
    if (res && pc->lastYieldOffset != startYieldOffset) {
        reportWithOffset(ParseError, false, pc->lastYieldOffset,
                         msg, js_yield_str);
        return null();
    }
    return res;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::argumentList(Node listNode, bool *isSpread)
{
    if (tokenStream.matchToken(TOK_RP, TokenStream::Operand)) {
        handler.setEndPosition(listNode, pos().end);
        return true;
    }

    uint32_t startYieldOffset = pc->lastYieldOffset;
    bool arg0 = true;

    do {
        bool spread = false;
        uint32_t begin = 0;
        if (tokenStream.matchToken(TOK_TRIPLEDOT, TokenStream::Operand)) {
            spread = true;
            begin = pos().begin;
            *isSpread = true;
        }

        Node argNode = assignExpr();
        if (!argNode)
            return false;
        if (spread) {
            argNode = handler.newUnary(PNK_SPREAD, JSOP_NOP, begin, argNode);
            if (!argNode)
                return null();
        }

        if (handler.isOperationWithoutParens(argNode, PNK_YIELD) &&
            tokenStream.peekToken() == TOK_COMMA) {
            report(ParseError, false, argNode, JSMSG_BAD_GENERATOR_SYNTAX, js_yield_str);
            return false;
        }
#if JS_HAS_GENERATOR_EXPRS
        if (!spread && tokenStream.matchToken(TOK_FOR)) {
            if (pc->lastYieldOffset != startYieldOffset) {
                reportWithOffset(ParseError, false, pc->lastYieldOffset,
                                 JSMSG_BAD_GENEXP_BODY, js_yield_str);
                return false;
            }
            argNode = legacyGeneratorExpr(argNode);
            if (!argNode)
                return false;
            if (!arg0 || tokenStream.peekToken() == TOK_COMMA) {
                report(ParseError, false, argNode, JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
                return false;
            }
        }
#endif
        arg0 = false;

        handler.addList(listNode, argNode);
    } while (tokenStream.matchToken(TOK_COMMA));

    if (tokenStream.getToken() != TOK_RP) {
        report(ParseError, false, null(), JSMSG_PAREN_AFTER_ARGS);
        return false;
    }
    handler.setEndPosition(listNode, pos().end);
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::memberExpr(TokenKind tt, bool allowCallSyntax)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(tt));

    Node lhs;

    JS_CHECK_RECURSION(context, return null());

    /* Check for new expression first. */
    if (tt == TOK_NEW) {
        lhs = handler.newList(PNK_NEW, null(), JSOP_NEW);
        if (!lhs)
            return null();

        tt = tokenStream.getToken(TokenStream::Operand);
        Node ctorExpr = memberExpr(tt, false);
        if (!ctorExpr)
            return null();

        handler.addList(lhs, ctorExpr);

        if (tokenStream.matchToken(TOK_LP)) {
            bool isSpread = false;
            if (!argumentList(lhs, &isSpread))
                return null();
            if (isSpread)
                handler.setOp(lhs, JSOP_SPREADNEW);
        }
    } else {
        lhs = primaryExpr(tt);
        if (!lhs)
            return null();
    }

    while ((tt = tokenStream.getToken()) > TOK_EOF) {
        Node nextMember;
        if (tt == TOK_DOT) {
            tt = tokenStream.getToken(TokenStream::KeywordIsName);
            if (tt == TOK_ERROR)
                return null();
            if (tt == TOK_NAME) {
                PropertyName *field = tokenStream.currentName();
                nextMember = handler.newPropertyAccess(lhs, field, pos().end);
                if (!nextMember)
                    return null();
            } else {
                report(ParseError, false, null(), JSMSG_NAME_AFTER_DOT);
                return null();
            }
        } else if (tt == TOK_LB) {
            Node propExpr = expr();
            if (!propExpr)
                return null();

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_IN_INDEX);

            nextMember = handler.newPropertyByValue(lhs, propExpr, pos().end);
            if (!nextMember)
                return null();
        } else if (allowCallSyntax && tt == TOK_LP) {
            JSOp op = JSOP_CALL;
            nextMember = handler.newList(PNK_CALL, null(), JSOP_CALL);
            if (!nextMember)
                return null();

            if (JSAtom *atom = handler.isName(lhs)) {
                if (atom == context->names().eval) {
                    /* Select JSOP_EVAL and flag pc as heavyweight. */
                    op = JSOP_EVAL;
                    pc->sc->setBindingsAccessedDynamically();

                    /*
                     * In non-strict mode code, direct calls to eval can add
                     * variables to the call object.
                     */
                    if (pc->sc->isFunctionBox() && !pc->sc->strict)
                        pc->sc->asFunctionBox()->setHasExtensibleScope();
                }
            } else if (JSAtom *atom = handler.isGetProp(lhs)) {
                /* Select JSOP_FUNAPPLY given foo.apply(...). */
                if (atom == context->names().apply) {
                    op = JSOP_FUNAPPLY;
                    if (pc->sc->isFunctionBox())
                        pc->sc->asFunctionBox()->usesApply = true;
                } else if (atom == context->names().call) {
                    op = JSOP_FUNCALL;
                }
            }

            handler.setBeginPosition(nextMember, lhs);
            handler.addList(nextMember, lhs);

            bool isSpread = false;
            if (!argumentList(nextMember, &isSpread))
                return null();
            if (isSpread)
                op = (op == JSOP_EVAL ? JSOP_SPREADEVAL : JSOP_SPREADCALL);
            handler.setOp(nextMember, op);
        } else {
            tokenStream.ungetToken();
            return lhs;
        }

        lhs = nextMember;
    }
    if (tt == TOK_ERROR)
        return null();
    return lhs;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newName(PropertyName *name)
{
    return handler.newName(name, pc->blockid(), pos());
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::identifierName()
{
    RootedPropertyName name(context, tokenStream.currentName());
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

#ifdef JS_HAS_TEMPLATE_STRINGS
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::noSubstitutionTemplate()
{
    return handler.newTemplateStringLiteral(stopStringCompression(), pos());
}
#endif

template <typename ParseHandler>
JSAtom * Parser<ParseHandler>::stopStringCompression() {
    JSAtom *atom = tokenStream.currentToken().atom();

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
    // Create the regexp even when doing a syntax parse, to check the regexp's syntax.
    const jschar *chars = tokenStream.getTokenbuf().begin();
    size_t length = tokenStream.getTokenbuf().length();
    RegExpFlag flags = tokenStream.currentToken().regExpFlags();

    Rooted<RegExpObject*> reobj(context);
    RegExpStatics *res = context->global()->getRegExpStatics(context);
    if (!res)
        return null();

    reobj = RegExpObject::create(context, res, chars, length, flags, &tokenStream, alloc);
    if (!reobj)
        return null();

    return handler.newRegExp(reobj, pos(), *this);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::arrayInitializer()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_LB));

    uint32_t begin = pos().begin;
    Node literal = handler.newArrayLiteral(begin, pc->blockidGen);
    if (!literal)
        return null();

    if (tokenStream.matchToken(TOK_RB, TokenStream::Operand)) {
        /*
         * Mark empty arrays as non-constant, since we cannot easily
         * determine their type.
         */
        handler.setListFlag(literal, PNX_NONCONST);
    } else if (tokenStream.matchToken(TOK_FOR, TokenStream::Operand)) {
        // ES6 array comprehension.
        return arrayComprehension(begin);
    } else {
        bool spread = false, missingTrailingComma = false;
        uint32_t index = 0;
        for (; ; index++) {
            if (index == JSObject::NELEMENTS_LIMIT) {
                report(ParseError, false, null(), JSMSG_ARRAY_INIT_TOO_BIG);
                return null();
            }

            TokenKind tt = tokenStream.peekToken(TokenStream::Operand);
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
                Node inner = assignExpr();
                if (!inner)
                    return null();
                if (!handler.addSpreadElement(literal, begin, inner))
                    return null();
            } else {
                Node element = assignExpr();
                if (!element)
                    return null();
                if (foldConstants && !FoldConstants(context, &element, this))
                    return null();
                if (!handler.addArrayElement(literal, element))
                    return null();
            }

            if (tt != TOK_COMMA) {
                /* If we didn't already match TOK_COMMA in above case. */
                if (!tokenStream.matchToken(TOK_COMMA)) {
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
         * translates to roughly the following let expression:
         *
         *   let (array = new Array, i, j) {
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
        if (index == 0 && !spread && tokenStream.matchToken(TOK_FOR) && missingTrailingComma)
            return legacyArrayComprehension(literal);

        MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_LIST);
    }
    handler.setEndPosition(literal, pos().end);
    return literal;
}

static JSAtom*
DoubleToAtom(ExclusiveContext *cx, double value)
{
    // This is safe because doubles can not be moved.
    Value tmp = DoubleValue(value);
    return ToAtom<CanGC>(cx, HandleValue::fromMarkedLocation(&tmp));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::objectLiteral()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));

    /*
     * A map from property names we've seen thus far to a mask of property
     * assignment types.
     */
    AtomIndexMap seen;

    enum AssignmentType {
        GET     = 0x1,
        SET     = 0x2,
        VALUE   = 0x4 | GET | SET
    };

    Node literal = handler.newObjectLiteral(pos().begin);
    if (!literal)
        return null();

    RootedAtom atom(context);
    for (;;) {
        TokenKind ltok = tokenStream.getToken(TokenStream::KeywordIsName);
        if (ltok == TOK_RC)
            break;

        JSOp op = JSOP_INITPROP;
        Node propname;
        switch (ltok) {
          case TOK_NUMBER:
            atom = DoubleToAtom(context, tokenStream.currentToken().number());
            if (!atom)
                return null();
            propname = newNumber(tokenStream.currentToken());
            break;

          case TOK_NAME: {
            atom = tokenStream.currentName();
            if (atom == context->names().get) {
                op = JSOP_INITPROP_GETTER;
            } else if (atom == context->names().set) {
                op = JSOP_INITPROP_SETTER;
            } else {
                propname = handler.newIdentifier(atom, pos());
                if (!propname)
                    return null();
                break;
            }

            // We have parsed |get| or |set|. Look for an accessor property
            // name next.
            TokenKind tt = tokenStream.getToken(TokenStream::KeywordIsName);
            if (tt == TOK_NAME) {
                atom = tokenStream.currentName();
                propname = newName(atom->asPropertyName());
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
            } else {
                // Not an accessor property after all.
                tokenStream.ungetToken();
                propname = handler.newIdentifier(atom, pos());
                if (!propname)
                    return null();
                op = JSOP_INITPROP;
                break;
            }

            JS_ASSERT(op == JSOP_INITPROP_GETTER || op == JSOP_INITPROP_SETTER);
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
            report(ParseError, false, null(), JSMSG_BAD_PROP_ID);
            return null();
        }

        if (op == JSOP_INITPROP) {
            TokenKind tt = tokenStream.getToken();
            Node propexpr;
            if (tt == TOK_COLON) {
                propexpr = assignExpr();
                if (!propexpr)
                    return null();

                if (foldConstants && !FoldConstants(context, &propexpr, this))
                    return null();

                /*
                 * Treat initializers which mutate __proto__ as non-constant,
                 * so that we can later assume singleton objects delegate to
                 * the default Object.prototype.
                 */
                if (!handler.isConstant(propexpr) || atom == context->names().proto)
                    handler.setListFlag(literal, PNX_NONCONST);

                if (!handler.addPropertyDefinition(literal, propname, propexpr))
                    return null();
            }
            else if (ltok == TOK_NAME && (tt == TOK_COMMA || tt == TOK_RC)) {
                /*
                 * Support, e.g., |var {x, y} = o| as destructuring shorthand
                 * for |var {x: x, y: y} = o|, per proposed JS2/ES4 for JS1.8.
                 */
                if (!abortIfSyntaxParser())
                    return null();
                tokenStream.ungetToken();
                if (!tokenStream.checkForKeyword(atom, nullptr))
                    return null();
                PropertyName *name = handler.isName(propname);
                JS_ASSERT(atom);
                propname = newName(name);
                if (!propname)
                    return null();
                if (!handler.addShorthandPropertyDefinition(literal, propname))
                    return null();
            }
            else {
                report(ParseError, false, null(), JSMSG_COLON_AFTER_ID);
                return null();
            }
        } else {
            /* NB: Getter function in { get x(){} } is unnamed. */
            Rooted<PropertyName*> funName(context, nullptr);
            TokenStream::Position start(keepAtoms);
            tokenStream.tell(&start);
            Node accessor = functionDef(funName, start, op == JSOP_INITPROP_GETTER ? Getter : Setter,
                                        Expression, NotGenerator);
            if (!accessor)
                return null();
            if (!handler.addAccessorPropertyDefinition(literal, propname, accessor, op))
                return null();
        }

        /*
         * Check for duplicate property names.  Duplicate data properties
         * only conflict in strict mode.  Duplicate getter or duplicate
         * setter halves always conflict.  A data property conflicts with
         * any part of an accessor property.
         */
        AssignmentType assignType;
        if (op == JSOP_INITPROP)
            assignType = VALUE;
        else if (op == JSOP_INITPROP_GETTER)
            assignType = GET;
        else if (op == JSOP_INITPROP_SETTER)
            assignType = SET;
        else
            MOZ_ASSUME_UNREACHABLE("bad opcode in object initializer");

        AtomIndexAddPtr p = seen.lookupForAdd(atom);
        if (p) {
            jsatomid index = p.value();
            AssignmentType oldAssignType = AssignmentType(index);
            if ((oldAssignType & assignType) &&
                (oldAssignType != VALUE || assignType != VALUE || pc->sc->needStrictChecks()))
            {
                JSAutoByteString name;
                if (!AtomToPrintableString(context, atom, &name))
                    return null();

                ParseReportKind reportKind =
                    (oldAssignType == VALUE && assignType == VALUE && !pc->sc->needStrictChecks())
                    ? ParseWarning
                    : (pc->sc->needStrictChecks() ? ParseStrictError : ParseError);
                if (!report(reportKind, pc->sc->strict, null(),
                            JSMSG_DUPLICATE_PROPERTY, name.ptr()))
                {
                    return null();
                }
            }
            p.value() = assignType | oldAssignType;
        } else {
            if (!seen.add(p, atom, assignType))
                return null();
        }

        TokenKind tt = tokenStream.getToken();
        if (tt == TOK_RC)
            break;
        if (tt != TOK_COMMA) {
            report(ParseError, false, null(), JSMSG_CURLY_AFTER_LIST);
            return null();
        }
    }

    handler.setEndPosition(literal, pos().end);
    return literal;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::primaryExpr(TokenKind tt)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(tt));
    JS_CHECK_RECURSION(context, return null());

    switch (tt) {
      case TOK_FUNCTION:
        return functionExpr();

      case TOK_LB:
        return arrayInitializer();

      case TOK_LC:
        return objectLiteral();

      case TOK_LET:
        return letBlock(LetExpresion);

      case TOK_LP:
        return parenExprOrGeneratorComprehension();

#ifdef JS_HAS_TEMPLATE_STRINGS
      case TOK_TEMPLATE_HEAD:
        return templateLiteral();
      case TOK_NO_SUBS_TEMPLATE:
        return noSubstitutionTemplate();
#endif
      case TOK_STRING:
        return stringLiteral();

      case TOK_YIELD:
        if (!checkYieldNameValidity())
            return null();
        // Fall through.
      case TOK_NAME:
        return identifierName();

      case TOK_REGEXP:
        return newRegExp();

      case TOK_NUMBER:
        return newNumber(tokenStream.currentToken());

      case TOK_TRUE:
        return handler.newBooleanLiteral(true, pos());
      case TOK_FALSE:
        return handler.newBooleanLiteral(false, pos());
      case TOK_THIS:
        return handler.newThisLiteral(pos());
      case TOK_NULL:
        return handler.newNullLiteral(pos());

      case TOK_RP:
        // Not valid expression syntax, but this is valid in an arrow function
        // with no params: `() => body`.
        if (tokenStream.peekToken() == TOK_ARROW) {
            tokenStream.ungetToken();  // put back right paren

            // Now just return something that will allow parsing to continue.
            // It doesn't matter what; when we reach the =>, we will rewind and
            // reparse the whole arrow function. See Parser::assignExpr.
            return handler.newNullLiteral(pos());
        }
        report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
        return null();

      case TOK_TRIPLEDOT:
        // Not valid expression syntax, but this is valid in an arrow function
        // with a rest param: `(a, b, ...rest) => body`.
        if (tokenStream.matchToken(TOK_NAME) &&
            tokenStream.matchToken(TOK_RP) &&
            tokenStream.peekToken() == TOK_ARROW)
        {
            tokenStream.ungetToken();  // put back right paren

            // Return an arbitrary expression node. See case TOK_RP above.
            return handler.newNullLiteral(pos());
        }
        report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
        return null();

      case TOK_ERROR:
        /* The scanner or one of its subroutines reported the error. */
        return null();

      default:
        report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
        return null();
    }
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::parenExprOrGeneratorComprehension()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_LP));
    uint32_t begin = pos().begin;
    uint32_t startYieldOffset = pc->lastYieldOffset;

    if (tokenStream.matchToken(TOK_FOR, TokenStream::Operand))
        return generatorComprehension(begin);

    /*
     * Always accept the 'in' operator in a parenthesized expression,
     * where it's unambiguous, even if we might be parsing the init of a
     * for statement.
     */
    bool oldParsingForInit = pc->parsingForInit;
    pc->parsingForInit = false;
    Node pn = expr();
    pc->parsingForInit = oldParsingForInit;

    if (!pn)
        return null();

#if JS_HAS_GENERATOR_EXPRS
    if (tokenStream.matchToken(TOK_FOR)) {
        if (pc->lastYieldOffset != startYieldOffset) {
            reportWithOffset(ParseError, false, pc->lastYieldOffset,
                             JSMSG_BAD_GENEXP_BODY, js_yield_str);
            return null();
        }
        if (handler.isOperationWithoutParens(pn, PNK_COMMA)) {
            report(ParseError, false, null(),
                   JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
            return null();
        }
        pn = legacyGeneratorExpr(pn);
        if (!pn)
            return null();
        handler.setBeginPosition(pn, begin);
        if (tokenStream.getToken() != TOK_RP) {
            report(ParseError, false, null(),
                   JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
            return null();
        }
        handler.setEndPosition(pn, pos().end);
        handler.setInParens(pn);
        return pn;
    }
#endif /* JS_HAS_GENERATOR_EXPRS */

    pn = handler.setInParens(pn);

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
Parser<ParseHandler>::exprInParens()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_LP));
    uint32_t begin = pos().begin;
    uint32_t startYieldOffset = pc->lastYieldOffset;

    /*
     * Always accept the 'in' operator in a parenthesized expression,
     * where it's unambiguous, even if we might be parsing the init of a
     * for statement.
     */
    bool oldParsingForInit = pc->parsingForInit;
    pc->parsingForInit = false;
    Node pn = expr();
    pc->parsingForInit = oldParsingForInit;

    if (!pn)
        return null();

#if JS_HAS_GENERATOR_EXPRS
    if (tokenStream.matchToken(TOK_FOR)) {
        if (pc->lastYieldOffset != startYieldOffset) {
            reportWithOffset(ParseError, false, pc->lastYieldOffset,
                             JSMSG_BAD_GENEXP_BODY, js_yield_str);
            return null();
        }
        if (handler.isOperationWithoutParens(pn, PNK_COMMA)) {
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

template class Parser<FullParseHandler>;
template class Parser<SyntaxParseHandler>;

} /* namespace frontend */
} /* namespace js */
