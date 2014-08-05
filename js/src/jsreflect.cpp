/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS reflection package. */

#include "jsreflect.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"

#include <stdlib.h>

#include "jsarray.h"
#include "jsatom.h"
#include "jsobj.h"
#include "jspubtd.h"

#include "frontend/Parser.h"
#include "frontend/TokenStream.h"
#include "js/CharacterEncoding.h"
#include "vm/RegExpObject.h"

#include "jsobjinlines.h"

#include "frontend/ParseNode-inl.h"

using namespace js;
using namespace js::frontend;

using JS::AutoValueArray;
using mozilla::ArrayLength;
using mozilla::DebugOnly;

char const * const js::aopNames[] = {
    "=",    /* AOP_ASSIGN */
    "+=",   /* AOP_PLUS */
    "-=",   /* AOP_MINUS */
    "*=",   /* AOP_STAR */
    "/=",   /* AOP_DIV */
    "%=",   /* AOP_MOD */
    "<<=",  /* AOP_LSH */
    ">>=",  /* AOP_RSH */
    ">>>=", /* AOP_URSH */
    "|=",   /* AOP_BITOR */
    "^=",   /* AOP_BITXOR */
    "&="    /* AOP_BITAND */
};

char const * const js::binopNames[] = {
    "==",         /* BINOP_EQ */
    "!=",         /* BINOP_NE */
    "===",        /* BINOP_STRICTEQ */
    "!==",        /* BINOP_STRICTNE */
    "<",          /* BINOP_LT */
    "<=",         /* BINOP_LE */
    ">",          /* BINOP_GT */
    ">=",         /* BINOP_GE */
    "<<",         /* BINOP_LSH */
    ">>",         /* BINOP_RSH */
    ">>>",        /* BINOP_URSH */
    "+",          /* BINOP_PLUS */
    "-",          /* BINOP_MINUS */
    "*",          /* BINOP_STAR */
    "/",          /* BINOP_DIV */
    "%",          /* BINOP_MOD */
    "|",          /* BINOP_BITOR */
    "^",          /* BINOP_BITXOR */
    "&",          /* BINOP_BITAND */
    "in",         /* BINOP_IN */
    "instanceof", /* BINOP_INSTANCEOF */
};

char const * const js::unopNames[] = {
    "delete",  /* UNOP_DELETE */
    "-",       /* UNOP_NEG */
    "+",       /* UNOP_POS */
    "!",       /* UNOP_NOT */
    "~",       /* UNOP_BITNOT */
    "typeof",  /* UNOP_TYPEOF */
    "void"     /* UNOP_VOID */
};

char const * const js::nodeTypeNames[] = {
#define ASTDEF(ast, str, method) str,
#include "jsast.tbl"
#undef ASTDEF
    nullptr
};

static char const * const callbackNames[] = {
#define ASTDEF(ast, str, method) method,
#include "jsast.tbl"
#undef ASTDEF
    nullptr
};

enum YieldKind { Delegating, NotDelegating };

typedef AutoValueVector NodeVector;

/*
 * ParseNode is a somewhat intricate data structure, and its invariants have
 * evolved, making it more likely that there could be a disconnect between the
 * parser and the AST serializer. We use these macros to check invariants on a
 * parse node and raise a dynamic error on failure.
 */
#define LOCAL_ASSERT(expr)                                                                \
    JS_BEGIN_MACRO                                                                        \
        JS_ASSERT(expr);                                                                  \
        if (!(expr)) {                                                                    \
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_PARSE_NODE);  \
            return false;                                                                 \
        }                                                                                 \
    JS_END_MACRO

#define LOCAL_NOT_REACHED(expr)                                                           \
    JS_BEGIN_MACRO                                                                        \
        MOZ_ASSERT(false);                                                                \
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_PARSE_NODE);      \
        return false;                                                                     \
    JS_END_MACRO

namespace {

/* Set 'result' to obj[id] if any such property exists, else defaultValue. */
static bool
GetPropertyDefault(JSContext *cx, HandleObject obj, HandleId id, HandleValue defaultValue,
                   MutableHandleValue result)
{
    bool found;
    if (!JSObject::hasProperty(cx, obj, id, &found))
        return false;
    if (!found) {
        result.set(defaultValue);
        return true;
    }
    return JSObject::getGeneric(cx, obj, obj, id, result);
}

/*
 * Builder class that constructs JavaScript AST node objects. See:
 *
 *     https://developer.mozilla.org/en/SpiderMonkey/Parser_API
 *
 * Bug 569487: generalize builder interface
 */
class NodeBuilder
{
    typedef AutoValueArray<AST_LIMIT> CallbackArray;

    JSContext   *cx;
    TokenStream *tokenStream;
    bool        saveLoc;               /* save source location information?     */
    char const  *src;                  /* source filename or null               */
    RootedValue srcval;                /* source filename JS value or null      */
    CallbackArray callbacks;           /* user-specified callbacks              */
    RootedValue userv;                 /* user-specified builder object or null */

  public:
    NodeBuilder(JSContext *c, bool l, char const *s)
      : cx(c), tokenStream(nullptr), saveLoc(l), src(s), srcval(c), callbacks(cx),
          userv(c)
    {}

    bool init(HandleObject userobj = js::NullPtr()) {
        if (src) {
            if (!atomValue(src, &srcval))
                return false;
        } else {
            srcval.setNull();
        }

        if (!userobj) {
            userv.setNull();
            for (unsigned i = 0; i < AST_LIMIT; i++) {
                callbacks[i].setNull();
            }
            return true;
        }

        userv.setObject(*userobj);

        RootedValue nullVal(cx, NullValue());
        RootedValue funv(cx);
        for (unsigned i = 0; i < AST_LIMIT; i++) {
            const char *name = callbackNames[i];
            RootedAtom atom(cx, Atomize(cx, name, strlen(name)));
            if (!atom)
                return false;
            RootedId id(cx, AtomToId(atom));
            if (!GetPropertyDefault(cx, userobj, id, nullVal, &funv))
                return false;

            if (funv.isNullOrUndefined()) {
                callbacks[i].setNull();
                continue;
            }

            if (!funv.isObject() || !funv.toObject().is<JSFunction>()) {
                js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_NOT_FUNCTION,
                                         JSDVG_SEARCH_STACK, funv, js::NullPtr(), nullptr, nullptr);
                return false;
            }

            callbacks[i].set(funv);
        }

        return true;
    }

    void setTokenStream(TokenStream *ts) {
        tokenStream = ts;
    }

  private:
    bool callback(HandleValue fun, TokenPos *pos, MutableHandleValue dst) {
        if (saveLoc) {
            RootedValue loc(cx);
            if (!newNodeLoc(pos, &loc))
                return false;
            AutoValueArray<1> argv(cx);
            argv[0].set(loc);
            return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
        }

        AutoValueArray<1> argv(cx);
        argv[0].setNull(); /* no zero-length arrays allowed! */
        return Invoke(cx, userv, fun, 0, argv.begin(), dst);
    }

    bool callback(HandleValue fun, HandleValue v1, TokenPos *pos, MutableHandleValue dst) {
        if (saveLoc) {
            RootedValue loc(cx);
            if (!newNodeLoc(pos, &loc))
                return false;
            AutoValueArray<2> argv(cx);
            argv[0].set(v1);
            argv[1].set(loc);
            return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
        }

        AutoValueArray<1> argv(cx);
        argv[0].set(v1);
        return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
    }

    bool callback(HandleValue fun, HandleValue v1, HandleValue v2, TokenPos *pos,
                  MutableHandleValue dst) {
        if (saveLoc) {
            RootedValue loc(cx);
            if (!newNodeLoc(pos, &loc))
                return false;
            AutoValueArray<3> argv(cx);
            argv[0].set(v1);
            argv[1].set(v2);
            argv[2].set(loc);
            return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
        }

        AutoValueArray<2> argv(cx);
        argv[0].set(v1);
        argv[1].set(v2);
        return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
    }

    bool callback(HandleValue fun, HandleValue v1, HandleValue v2, HandleValue v3, TokenPos *pos,
                  MutableHandleValue dst) {
        if (saveLoc) {
            RootedValue loc(cx);
            if (!newNodeLoc(pos, &loc))
                return false;
            AutoValueArray<4> argv(cx);
            argv[0].set(v1);
            argv[1].set(v2);
            argv[2].set(v3);
            argv[3].set(loc);
            return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
        }

        AutoValueArray<3> argv(cx);
        argv[0].set(v1);
        argv[1].set(v2);
        argv[2].set(v3);
        return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
    }

    bool callback(HandleValue fun, HandleValue v1, HandleValue v2, HandleValue v3, HandleValue v4,
                  TokenPos *pos, MutableHandleValue dst) {
        if (saveLoc) {
            RootedValue loc(cx);
            if (!newNodeLoc(pos, &loc))
                return false;
            AutoValueArray<5> argv(cx);
            argv[0].set(v1);
            argv[1].set(v2);
            argv[2].set(v3);
            argv[3].set(v4);
            argv[4].set(loc);
            return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
        }

        AutoValueArray<4> argv(cx);
        argv[0].set(v1);
        argv[1].set(v2);
        argv[2].set(v3);
        argv[3].set(v4);
        return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
    }

    bool callback(HandleValue fun, HandleValue v1, HandleValue v2, HandleValue v3, HandleValue v4,
                  HandleValue v5, TokenPos *pos, MutableHandleValue dst) {
        if (saveLoc) {
            RootedValue loc(cx);
            if (!newNodeLoc(pos, &loc))
                return false;
            AutoValueArray<6> argv(cx);
            argv[0].set(v1);
            argv[1].set(v2);
            argv[2].set(v3);
            argv[3].set(v4);
            argv[4].set(v5);
            argv[5].set(loc);
            return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
        }

        AutoValueArray<5> argv(cx);
        argv[0].set(v1);
        argv[1].set(v2);
        argv[2].set(v3);
        argv[3].set(v4);
        argv[4].set(v5);
        return Invoke(cx, userv, fun, argv.length(), argv.begin(), dst);
    }

    // WARNING: Returning a Handle is non-standard, but it works in this case
    // because both |v| and |UndefinedHandleValue| are definitely rooted on a
    // previous stack frame (i.e. we're just choosing between two
    // already-rooted values).
    HandleValue opt(HandleValue v) {
        JS_ASSERT_IF(v.isMagic(), v.whyMagic() == JS_SERIALIZE_NO_NODE);
        return v.isMagic(JS_SERIALIZE_NO_NODE) ? JS::UndefinedHandleValue : v;
    }

    bool atomValue(const char *s, MutableHandleValue dst) {
        /*
         * Bug 575416: instead of Atomize, lookup constant atoms in tbl file
         */
        RootedAtom atom(cx, Atomize(cx, s, strlen(s)));
        if (!atom)
            return false;

        dst.setString(atom);
        return true;
    }

    bool newObject(MutableHandleObject dst) {
        RootedObject nobj(cx, NewBuiltinClassInstance(cx, &JSObject::class_));
        if (!nobj)
            return false;

        dst.set(nobj);
        return true;
    }

    bool newArray(NodeVector &elts, MutableHandleValue dst);

    bool newNode(ASTType type, TokenPos *pos, MutableHandleObject dst);

    bool newNode(ASTType type, TokenPos *pos, MutableHandleValue dst) {
        RootedObject node(cx);
        return newNode(type, pos, &node) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos,
                 const char *childName, HandleValue child,
                 MutableHandleValue dst) {
        RootedObject node(cx);
        return newNode(type, pos, &node) &&
               setProperty(node, childName, child) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos,
                 const char *childName1, HandleValue child1,
                 const char *childName2, HandleValue child2,
                 MutableHandleValue dst) {
        RootedObject node(cx);
        return newNode(type, pos, &node) &&
               setProperty(node, childName1, child1) &&
               setProperty(node, childName2, child2) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos,
                 const char *childName1, HandleValue child1,
                 const char *childName2, HandleValue child2,
                 const char *childName3, HandleValue child3,
                 MutableHandleValue dst) {
        RootedObject node(cx);
        return newNode(type, pos, &node) &&
               setProperty(node, childName1, child1) &&
               setProperty(node, childName2, child2) &&
               setProperty(node, childName3, child3) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos,
                 const char *childName1, HandleValue child1,
                 const char *childName2, HandleValue child2,
                 const char *childName3, HandleValue child3,
                 const char *childName4, HandleValue child4,
                 MutableHandleValue dst) {
        RootedObject node(cx);
        return newNode(type, pos, &node) &&
               setProperty(node, childName1, child1) &&
               setProperty(node, childName2, child2) &&
               setProperty(node, childName3, child3) &&
               setProperty(node, childName4, child4) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos,
                 const char *childName1, HandleValue child1,
                 const char *childName2, HandleValue child2,
                 const char *childName3, HandleValue child3,
                 const char *childName4, HandleValue child4,
                 const char *childName5, HandleValue child5,
                 MutableHandleValue dst) {
        RootedObject node(cx);
        return newNode(type, pos, &node) &&
               setProperty(node, childName1, child1) &&
               setProperty(node, childName2, child2) &&
               setProperty(node, childName3, child3) &&
               setProperty(node, childName4, child4) &&
               setProperty(node, childName5, child5) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos,
                 const char *childName1, HandleValue child1,
                 const char *childName2, HandleValue child2,
                 const char *childName3, HandleValue child3,
                 const char *childName4, HandleValue child4,
                 const char *childName5, HandleValue child5,
                 const char *childName6, HandleValue child6,
                 const char *childName7, HandleValue child7,
                 MutableHandleValue dst) {
        RootedObject node(cx);
        return newNode(type, pos, &node) &&
               setProperty(node, childName1, child1) &&
               setProperty(node, childName2, child2) &&
               setProperty(node, childName3, child3) &&
               setProperty(node, childName4, child4) &&
               setProperty(node, childName5, child5) &&
               setProperty(node, childName6, child6) &&
               setProperty(node, childName7, child7) &&
               setResult(node, dst);
    }

    bool listNode(ASTType type, const char *propName, NodeVector &elts, TokenPos *pos,
                  MutableHandleValue dst) {
        RootedValue array(cx);
        if (!newArray(elts, &array))
            return false;

        RootedValue cb(cx, callbacks[type]);
        if (!cb.isNull())
            return callback(cb, array, pos, dst);

        return newNode(type, pos, propName, array, dst);
    }

    bool setProperty(HandleObject obj, const char *name, HandleValue val) {
        JS_ASSERT_IF(val.isMagic(), val.whyMagic() == JS_SERIALIZE_NO_NODE);

        /*
         * Bug 575416: instead of Atomize, lookup constant atoms in tbl file
         */
        RootedAtom atom(cx, Atomize(cx, name, strlen(name)));
        if (!atom)
            return false;

        /* Represent "no node" as null and ensure users are not exposed to magic values. */
        RootedValue optVal(cx, val.isMagic(JS_SERIALIZE_NO_NODE) ? NullValue() : val);
        return JSObject::defineProperty(cx, obj, atom->asPropertyName(), optVal);
    }

    bool newNodeLoc(TokenPos *pos, MutableHandleValue dst);

    bool setNodeLoc(HandleObject node, TokenPos *pos);

    bool setResult(HandleObject obj, MutableHandleValue dst) {
        JS_ASSERT(obj);
        dst.setObject(*obj);
        return true;
    }

  public:
    /*
     * All of the public builder methods take as their last two
     * arguments a nullable token position and a non-nullable, rooted
     * outparam.
     *
     * Any Value arguments representing optional subnodes may be a
     * JS_SERIALIZE_NO_NODE magic value.
     */

    /*
     * misc nodes
     */

    bool program(NodeVector &elts, TokenPos *pos, MutableHandleValue dst);

    bool literal(HandleValue val, TokenPos *pos, MutableHandleValue dst);

    bool identifier(HandleValue name, TokenPos *pos, MutableHandleValue dst);

    bool function(ASTType type, TokenPos *pos,
                  HandleValue id, NodeVector &args, NodeVector &defaults,
                  HandleValue body, HandleValue rest, bool isGenerator, bool isExpression,
                  MutableHandleValue dst);

    bool variableDeclarator(HandleValue id, HandleValue init, TokenPos *pos,
                            MutableHandleValue dst);

    bool switchCase(HandleValue expr, NodeVector &elts, TokenPos *pos, MutableHandleValue dst);

    bool catchClause(HandleValue var, HandleValue guard, HandleValue body, TokenPos *pos,
                     MutableHandleValue dst);

    bool propertyInitializer(HandleValue key, HandleValue val, PropKind kind, bool isShorthand,
                             TokenPos *pos, MutableHandleValue dst);


    /*
     * statements
     */

    bool blockStatement(NodeVector &elts, TokenPos *pos, MutableHandleValue dst);

    bool expressionStatement(HandleValue expr, TokenPos *pos, MutableHandleValue dst);

    bool emptyStatement(TokenPos *pos, MutableHandleValue dst);

    bool ifStatement(HandleValue test, HandleValue cons, HandleValue alt, TokenPos *pos,
                     MutableHandleValue dst);

    bool breakStatement(HandleValue label, TokenPos *pos, MutableHandleValue dst);

    bool continueStatement(HandleValue label, TokenPos *pos, MutableHandleValue dst);

    bool labeledStatement(HandleValue label, HandleValue stmt, TokenPos *pos,
                          MutableHandleValue dst);

    bool throwStatement(HandleValue arg, TokenPos *pos, MutableHandleValue dst);

    bool returnStatement(HandleValue arg, TokenPos *pos, MutableHandleValue dst);

    bool forStatement(HandleValue init, HandleValue test, HandleValue update, HandleValue stmt,
                      TokenPos *pos, MutableHandleValue dst);

    bool forInStatement(HandleValue var, HandleValue expr, HandleValue stmt,
                        bool isForEach, TokenPos *pos, MutableHandleValue dst);

    bool forOfStatement(HandleValue var, HandleValue expr, HandleValue stmt, TokenPos *pos,
                        MutableHandleValue dst);

    bool withStatement(HandleValue expr, HandleValue stmt, TokenPos *pos, MutableHandleValue dst);

    bool whileStatement(HandleValue test, HandleValue stmt, TokenPos *pos, MutableHandleValue dst);

    bool doWhileStatement(HandleValue stmt, HandleValue test, TokenPos *pos,
                          MutableHandleValue dst);

    bool switchStatement(HandleValue disc, NodeVector &elts, bool lexical, TokenPos *pos,
                         MutableHandleValue dst);

    bool tryStatement(HandleValue body, NodeVector &guarded, HandleValue unguarded,
                      HandleValue finally, TokenPos *pos, MutableHandleValue dst);

    bool debuggerStatement(TokenPos *pos, MutableHandleValue dst);

    bool letStatement(NodeVector &head, HandleValue stmt, TokenPos *pos, MutableHandleValue dst);

    bool importDeclaration(NodeVector &elts, HandleValue moduleSpec, TokenPos *pos, MutableHandleValue dst);

    bool importSpecifier(HandleValue importName, HandleValue bindingName, TokenPos *pos, MutableHandleValue dst);

    bool exportDeclaration(HandleValue decl, NodeVector &elts, HandleValue moduleSpec, TokenPos *pos, MutableHandleValue dst);

    bool exportSpecifier(HandleValue bindingName, HandleValue exportName, TokenPos *pos, MutableHandleValue dst);

    bool exportBatchSpecifier(TokenPos *pos, MutableHandleValue dst);

    /*
     * expressions
     */

    bool binaryExpression(BinaryOperator op, HandleValue left, HandleValue right, TokenPos *pos,
                          MutableHandleValue dst);

    bool unaryExpression(UnaryOperator op, HandleValue expr, TokenPos *pos, MutableHandleValue dst);

    bool assignmentExpression(AssignmentOperator op, HandleValue lhs, HandleValue rhs,
                              TokenPos *pos, MutableHandleValue dst);

    bool updateExpression(HandleValue expr, bool incr, bool prefix, TokenPos *pos,
                          MutableHandleValue dst);

    bool logicalExpression(bool lor, HandleValue left, HandleValue right, TokenPos *pos,
                           MutableHandleValue dst);

    bool conditionalExpression(HandleValue test, HandleValue cons, HandleValue alt, TokenPos *pos,
                               MutableHandleValue dst);

    bool sequenceExpression(NodeVector &elts, TokenPos *pos, MutableHandleValue dst);

    bool newExpression(HandleValue callee, NodeVector &args, TokenPos *pos, MutableHandleValue dst);

    bool callExpression(HandleValue callee, NodeVector &args, TokenPos *pos,
                        MutableHandleValue dst);

    bool memberExpression(bool computed, HandleValue expr, HandleValue member, TokenPos *pos,
                          MutableHandleValue dst);

    bool arrayExpression(NodeVector &elts, TokenPos *pos, MutableHandleValue dst);

    bool templateLiteral(NodeVector &elts, TokenPos *pos, MutableHandleValue dst);

    bool taggedTemplate(HandleValue callee, NodeVector &args, TokenPos *pos,
                        MutableHandleValue dst);

    bool callSiteObj(NodeVector &raw, NodeVector &cooked, TokenPos *pos, MutableHandleValue dst);

    bool spreadExpression(HandleValue expr, TokenPos *pos, MutableHandleValue dst);

    bool objectExpression(NodeVector &elts, TokenPos *pos, MutableHandleValue dst);

    bool thisExpression(TokenPos *pos, MutableHandleValue dst);

    bool yieldExpression(HandleValue arg, YieldKind kind, TokenPos *pos, MutableHandleValue dst);

    bool comprehensionBlock(HandleValue patt, HandleValue src, bool isForEach, bool isForOf, TokenPos *pos,
                            MutableHandleValue dst);

    bool comprehensionExpression(HandleValue body, NodeVector &blocks, HandleValue filter,
                                 TokenPos *pos, MutableHandleValue dst);

    bool generatorExpression(HandleValue body, NodeVector &blocks, HandleValue filter,
                             TokenPos *pos, MutableHandleValue dst);

    bool letExpression(NodeVector &head, HandleValue expr, TokenPos *pos, MutableHandleValue dst);

    /*
     * declarations
     */

    bool variableDeclaration(NodeVector &elts, VarDeclKind kind, TokenPos *pos,
                             MutableHandleValue dst);

    /*
     * patterns
     */

    bool arrayPattern(NodeVector &elts, TokenPos *pos, MutableHandleValue dst);

    bool objectPattern(NodeVector &elts, TokenPos *pos, MutableHandleValue dst);

    bool propertyPattern(HandleValue key, HandleValue patt, bool isShorthand, TokenPos *pos,
                         MutableHandleValue dst);
};

} /* anonymous namespace */

bool
NodeBuilder::newNode(ASTType type, TokenPos *pos, MutableHandleObject dst)
{
    JS_ASSERT(type > AST_ERROR && type < AST_LIMIT);

    RootedValue tv(cx);
    RootedObject node(cx, NewBuiltinClassInstance(cx, &JSObject::class_));
    if (!node ||
        !setNodeLoc(node, pos) ||
        !atomValue(nodeTypeNames[type], &tv) ||
        !setProperty(node, "type", tv)) {
        return false;
    }

    dst.set(node);
    return true;
}

bool
NodeBuilder::newArray(NodeVector &elts, MutableHandleValue dst)
{
    const size_t len = elts.length();
    if (len > UINT32_MAX) {
        js_ReportAllocationOverflow(cx);
        return false;
    }
    RootedObject array(cx, NewDenseAllocatedArray(cx, uint32_t(len)));
    if (!array)
        return false;

    for (size_t i = 0; i < len; i++) {
        RootedValue val(cx, elts[i]);

        JS_ASSERT_IF(val.isMagic(), val.whyMagic() == JS_SERIALIZE_NO_NODE);

        /* Represent "no node" as an array hole by not adding the value. */
        if (val.isMagic(JS_SERIALIZE_NO_NODE))
            continue;

        if (!JSObject::setElement(cx, array, array, i, &val, false))
            return false;
    }

    dst.setObject(*array);
    return true;
}

bool
NodeBuilder::newNodeLoc(TokenPos *pos, MutableHandleValue dst)
{
    if (!pos) {
        dst.setNull();
        return true;
    }

    RootedObject loc(cx);
    RootedObject to(cx);
    RootedValue val(cx);

    if (!newObject(&loc))
        return false;

    dst.setObject(*loc);

    uint32_t startLineNum, startColumnIndex;
    uint32_t endLineNum, endColumnIndex;
    tokenStream->srcCoords.lineNumAndColumnIndex(pos->begin, &startLineNum, &startColumnIndex);
    tokenStream->srcCoords.lineNumAndColumnIndex(pos->end, &endLineNum, &endColumnIndex);

    if (!newObject(&to))
        return false;
    val.setObject(*to);
    if (!setProperty(loc, "start", val))
        return false;
    val.setNumber(startLineNum);
    if (!setProperty(to, "line", val))
        return false;
    val.setNumber(startColumnIndex);
    if (!setProperty(to, "column", val))
        return false;

    if (!newObject(&to))
        return false;
    val.setObject(*to);
    if (!setProperty(loc, "end", val))
        return false;
    val.setNumber(endLineNum);
    if (!setProperty(to, "line", val))
        return false;
    val.setNumber(endColumnIndex);
    if (!setProperty(to, "column", val))
        return false;

    if (!setProperty(loc, "source", srcval))
        return false;

    return true;
}

bool
NodeBuilder::setNodeLoc(HandleObject node, TokenPos *pos)
{
    if (!saveLoc) {
        RootedValue nullVal(cx, NullValue());
        setProperty(node, "loc", nullVal);
        return true;
    }

    RootedValue loc(cx);
    return newNodeLoc(pos, &loc) &&
           setProperty(node, "loc", loc);
}

bool
NodeBuilder::program(NodeVector &elts, TokenPos *pos, MutableHandleValue dst)
{
    return listNode(AST_PROGRAM, "body", elts, pos, dst);
}

bool
NodeBuilder::blockStatement(NodeVector &elts, TokenPos *pos, MutableHandleValue dst)
{
    return listNode(AST_BLOCK_STMT, "body", elts, pos, dst);
}

bool
NodeBuilder::expressionStatement(HandleValue expr, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_EXPR_STMT]);
    if (!cb.isNull())
        return callback(cb, expr, pos, dst);

    return newNode(AST_EXPR_STMT, pos, "expression", expr, dst);
}

bool
NodeBuilder::emptyStatement(TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_EMPTY_STMT]);
    if (!cb.isNull())
        return callback(cb, pos, dst);

    return newNode(AST_EMPTY_STMT, pos, dst);
}

bool
NodeBuilder::ifStatement(HandleValue test, HandleValue cons, HandleValue alt, TokenPos *pos,
                         MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_IF_STMT]);
    if (!cb.isNull())
        return callback(cb, test, cons, opt(alt), pos, dst);

    return newNode(AST_IF_STMT, pos,
                   "test", test,
                   "consequent", cons,
                   "alternate", alt,
                   dst);
}

bool
NodeBuilder::breakStatement(HandleValue label, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_BREAK_STMT]);
    if (!cb.isNull())
        return callback(cb, opt(label), pos, dst);

    return newNode(AST_BREAK_STMT, pos, "label", label, dst);
}

bool
NodeBuilder::continueStatement(HandleValue label, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_CONTINUE_STMT]);
    if (!cb.isNull())
        return callback(cb, opt(label), pos, dst);

    return newNode(AST_CONTINUE_STMT, pos, "label", label, dst);
}

bool
NodeBuilder::labeledStatement(HandleValue label, HandleValue stmt, TokenPos *pos,
                              MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_LAB_STMT]);
    if (!cb.isNull())
        return callback(cb, label, stmt, pos, dst);

    return newNode(AST_LAB_STMT, pos,
                   "label", label,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::throwStatement(HandleValue arg, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_THROW_STMT]);
    if (!cb.isNull())
        return callback(cb, arg, pos, dst);

    return newNode(AST_THROW_STMT, pos, "argument", arg, dst);
}

bool
NodeBuilder::returnStatement(HandleValue arg, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_RETURN_STMT]);
    if (!cb.isNull())
        return callback(cb, opt(arg), pos, dst);

    return newNode(AST_RETURN_STMT, pos, "argument", arg, dst);
}

bool
NodeBuilder::forStatement(HandleValue init, HandleValue test, HandleValue update, HandleValue stmt,
                          TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_FOR_STMT]);
    if (!cb.isNull())
        return callback(cb, opt(init), opt(test), opt(update), stmt, pos, dst);

    return newNode(AST_FOR_STMT, pos,
                   "init", init,
                   "test", test,
                   "update", update,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::forInStatement(HandleValue var, HandleValue expr, HandleValue stmt, bool isForEach,
                            TokenPos *pos, MutableHandleValue dst)
{
    RootedValue isForEachVal(cx, BooleanValue(isForEach));

    RootedValue cb(cx, callbacks[AST_FOR_IN_STMT]);
    if (!cb.isNull())
        return callback(cb, var, expr, stmt, isForEachVal, pos, dst);

    return newNode(AST_FOR_IN_STMT, pos,
                   "left", var,
                   "right", expr,
                   "body", stmt,
                   "each", isForEachVal,
                   dst);
}

bool
NodeBuilder::forOfStatement(HandleValue var, HandleValue expr, HandleValue stmt, TokenPos *pos,
                            MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_FOR_OF_STMT]);
    if (!cb.isNull())
        return callback(cb, var, expr, stmt, pos, dst);

    return newNode(AST_FOR_OF_STMT, pos,
                   "left", var,
                   "right", expr,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::withStatement(HandleValue expr, HandleValue stmt, TokenPos *pos,
                           MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_WITH_STMT]);
    if (!cb.isNull())
        return callback(cb, expr, stmt, pos, dst);

    return newNode(AST_WITH_STMT, pos,
                   "object", expr,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::whileStatement(HandleValue test, HandleValue stmt, TokenPos *pos,
                            MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_WHILE_STMT]);
    if (!cb.isNull())
        return callback(cb, test, stmt, pos, dst);

    return newNode(AST_WHILE_STMT, pos,
                   "test", test,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::doWhileStatement(HandleValue stmt, HandleValue test, TokenPos *pos,
                              MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_DO_STMT]);
    if (!cb.isNull())
        return callback(cb, stmt, test, pos, dst);

    return newNode(AST_DO_STMT, pos,
                   "body", stmt,
                   "test", test,
                   dst);
}

bool
NodeBuilder::switchStatement(HandleValue disc, NodeVector &elts, bool lexical, TokenPos *pos,
                             MutableHandleValue dst)
{
    RootedValue array(cx);
    if (!newArray(elts, &array))
        return false;

    RootedValue lexicalVal(cx, BooleanValue(lexical));

    RootedValue cb(cx, callbacks[AST_SWITCH_STMT]);
    if (!cb.isNull())
        return callback(cb, disc, array, lexicalVal, pos, dst);

    return newNode(AST_SWITCH_STMT, pos,
                   "discriminant", disc,
                   "cases", array,
                   "lexical", lexicalVal,
                   dst);
}

bool
NodeBuilder::tryStatement(HandleValue body, NodeVector &guarded, HandleValue unguarded,
                          HandleValue finally, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue guardedHandlers(cx);
    if (!newArray(guarded, &guardedHandlers))
        return false;

    RootedValue cb(cx, callbacks[AST_TRY_STMT]);
    if (!cb.isNull())
        return callback(cb, body, guardedHandlers, unguarded, opt(finally), pos, dst);

    return newNode(AST_TRY_STMT, pos,
                   "block", body,
                   "guardedHandlers", guardedHandlers,
                   "handler", unguarded,
                   "finalizer", finally,
                   dst);
}

bool
NodeBuilder::debuggerStatement(TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_DEBUGGER_STMT]);
    if (!cb.isNull())
        return callback(cb, pos, dst);

    return newNode(AST_DEBUGGER_STMT, pos, dst);
}

bool
NodeBuilder::binaryExpression(BinaryOperator op, HandleValue left, HandleValue right, TokenPos *pos,
                              MutableHandleValue dst)
{
    JS_ASSERT(op > BINOP_ERR && op < BINOP_LIMIT);

    RootedValue opName(cx);
    if (!atomValue(binopNames[op], &opName))
        return false;

    RootedValue cb(cx, callbacks[AST_BINARY_EXPR]);
    if (!cb.isNull())
        return callback(cb, opName, left, right, pos, dst);

    return newNode(AST_BINARY_EXPR, pos,
                   "operator", opName,
                   "left", left,
                   "right", right,
                   dst);
}

bool
NodeBuilder::unaryExpression(UnaryOperator unop, HandleValue expr, TokenPos *pos,
                             MutableHandleValue dst)
{
    JS_ASSERT(unop > UNOP_ERR && unop < UNOP_LIMIT);

    RootedValue opName(cx);
    if (!atomValue(unopNames[unop], &opName))
        return false;

    RootedValue cb(cx, callbacks[AST_UNARY_EXPR]);
    if (!cb.isNull())
        return callback(cb, opName, expr, pos, dst);

    RootedValue trueVal(cx, BooleanValue(true));
    return newNode(AST_UNARY_EXPR, pos,
                   "operator", opName,
                   "argument", expr,
                   "prefix", trueVal,
                   dst);
}

bool
NodeBuilder::assignmentExpression(AssignmentOperator aop, HandleValue lhs, HandleValue rhs,
                                  TokenPos *pos, MutableHandleValue dst)
{
    JS_ASSERT(aop > AOP_ERR && aop < AOP_LIMIT);

    RootedValue opName(cx);
    if (!atomValue(aopNames[aop], &opName))
        return false;

    RootedValue cb(cx, callbacks[AST_ASSIGN_EXPR]);
    if (!cb.isNull())
        return callback(cb, opName, lhs, rhs, pos, dst);

    return newNode(AST_ASSIGN_EXPR, pos,
                   "operator", opName,
                   "left", lhs,
                   "right", rhs,
                   dst);
}

bool
NodeBuilder::updateExpression(HandleValue expr, bool incr, bool prefix, TokenPos *pos,
                              MutableHandleValue dst)
{
    RootedValue opName(cx);
    if (!atomValue(incr ? "++" : "--", &opName))
        return false;

    RootedValue prefixVal(cx, BooleanValue(prefix));

    RootedValue cb(cx, callbacks[AST_UPDATE_EXPR]);
    if (!cb.isNull())
        return callback(cb, expr, opName, prefixVal, pos, dst);

    return newNode(AST_UPDATE_EXPR, pos,
                   "operator", opName,
                   "argument", expr,
                   "prefix", prefixVal,
                   dst);
}

bool
NodeBuilder::logicalExpression(bool lor, HandleValue left, HandleValue right, TokenPos *pos,
                               MutableHandleValue dst)
{
    RootedValue opName(cx);
    if (!atomValue(lor ? "||" : "&&", &opName))
        return false;

    RootedValue cb(cx, callbacks[AST_LOGICAL_EXPR]);
    if (!cb.isNull())
        return callback(cb, opName, left, right, pos, dst);

    return newNode(AST_LOGICAL_EXPR, pos,
                   "operator", opName,
                   "left", left,
                   "right", right,
                   dst);
}

bool
NodeBuilder::conditionalExpression(HandleValue test, HandleValue cons, HandleValue alt,
                                   TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_COND_EXPR]);
    if (!cb.isNull())
        return callback(cb, test, cons, alt, pos, dst);

    return newNode(AST_COND_EXPR, pos,
                   "test", test,
                   "consequent", cons,
                   "alternate", alt,
                   dst);
}

bool
NodeBuilder::sequenceExpression(NodeVector &elts, TokenPos *pos, MutableHandleValue dst)
{
    return listNode(AST_LIST_EXPR, "expressions", elts, pos, dst);
}

bool
NodeBuilder::callExpression(HandleValue callee, NodeVector &args, TokenPos *pos,
                            MutableHandleValue dst)
{
    RootedValue array(cx);
    if (!newArray(args, &array))
        return false;

    RootedValue cb(cx, callbacks[AST_CALL_EXPR]);
    if (!cb.isNull())
        return callback(cb, callee, array, pos, dst);

    return newNode(AST_CALL_EXPR, pos,
                   "callee", callee,
                   "arguments", array,
                   dst);
}

bool
NodeBuilder::newExpression(HandleValue callee, NodeVector &args, TokenPos *pos,
                           MutableHandleValue dst)
{
    RootedValue array(cx);
    if (!newArray(args, &array))
        return false;

    RootedValue cb(cx, callbacks[AST_NEW_EXPR]);
    if (!cb.isNull())
        return callback(cb, callee, array, pos, dst);

    return newNode(AST_NEW_EXPR, pos,
                   "callee", callee,
                   "arguments", array,
                   dst);
}

bool
NodeBuilder::memberExpression(bool computed, HandleValue expr, HandleValue member, TokenPos *pos,
                              MutableHandleValue dst)
{
    RootedValue computedVal(cx, BooleanValue(computed));

    RootedValue cb(cx, callbacks[AST_MEMBER_EXPR]);
    if (!cb.isNull())
        return callback(cb, computedVal, expr, member, pos, dst);

    return newNode(AST_MEMBER_EXPR, pos,
                   "object", expr,
                   "property", member,
                   "computed", computedVal,
                   dst);
}

bool
NodeBuilder::arrayExpression(NodeVector &elts, TokenPos *pos, MutableHandleValue dst)
{
    return listNode(AST_ARRAY_EXPR, "elements", elts, pos, dst);
}

bool
NodeBuilder::callSiteObj(NodeVector &raw, NodeVector &cooked, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue rawVal(cx);
    if (!newArray(raw, &rawVal))
        return false;

    RootedValue cookedVal(cx);
    if (!newArray(cooked, &cookedVal))
        return false;

    return newNode(AST_CALL_SITE_OBJ, pos,
                   "raw", rawVal,
                   "cooked", cookedVal,
                    dst);
}

bool
NodeBuilder::taggedTemplate(HandleValue callee, NodeVector &args, TokenPos *pos,
                            MutableHandleValue dst)
{
    RootedValue array(cx);
    if (!newArray(args, &array))
        return false;

    return newNode(AST_TAGGED_TEMPLATE, pos,
                   "callee", callee,
                   "arguments", array,
                   dst);
}

bool
NodeBuilder::templateLiteral(NodeVector &elts, TokenPos *pos, MutableHandleValue dst)
{
    return listNode(AST_TEMPLATE_LITERAL, "elements", elts, pos, dst);
}

bool
NodeBuilder::spreadExpression(HandleValue expr, TokenPos *pos, MutableHandleValue dst)
{
    return newNode(AST_SPREAD_EXPR, pos,
                   "expression", expr,
                   dst);
}

bool
NodeBuilder::propertyPattern(HandleValue key, HandleValue patt, bool isShorthand, TokenPos *pos,
                             MutableHandleValue dst)
{
    RootedValue kindName(cx);
    if (!atomValue("init", &kindName))
        return false;

    RootedValue isShorthandVal(cx, BooleanValue(isShorthand));

    RootedValue cb(cx, callbacks[AST_PROP_PATT]);
    if (!cb.isNull())
        return callback(cb, key, patt, pos, dst);

    return newNode(AST_PROP_PATT, pos,
                   "key", key,
                   "value", patt,
                   "kind", kindName,
                   "shorthand", isShorthandVal,
                   dst);
}

bool
NodeBuilder::propertyInitializer(HandleValue key, HandleValue val, PropKind kind, bool isShorthand,
                                 TokenPos *pos, MutableHandleValue dst)
{
    RootedValue kindName(cx);
    if (!atomValue(kind == PROP_INIT
                   ? "init"
                   : kind == PROP_GETTER
                   ? "get"
                   : "set", &kindName)) {
        return false;
    }

    RootedValue isShorthandVal(cx, BooleanValue(isShorthand));

    RootedValue cb(cx, callbacks[AST_PROPERTY]);
    if (!cb.isNull())
        return callback(cb, kindName, key, val, pos, dst);

    return newNode(AST_PROPERTY, pos,
                   "key", key,
                   "value", val,
                   "kind", kindName,
                   "shorthand", isShorthandVal,
                   dst);
}

bool
NodeBuilder::objectExpression(NodeVector &elts, TokenPos *pos, MutableHandleValue dst)
{
    return listNode(AST_OBJECT_EXPR, "properties", elts, pos, dst);
}

bool
NodeBuilder::thisExpression(TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_THIS_EXPR]);
    if (!cb.isNull())
        return callback(cb, pos, dst);

    return newNode(AST_THIS_EXPR, pos, dst);
}

bool
NodeBuilder::yieldExpression(HandleValue arg, YieldKind kind, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_YIELD_EXPR]);
    RootedValue delegateVal(cx);

    switch (kind) {
      case Delegating:
        delegateVal = BooleanValue(true);
        break;
      case NotDelegating:
        delegateVal = BooleanValue(false);
        break;
    }

    if (!cb.isNull())
        return callback(cb, opt(arg), delegateVal, pos, dst);
    return newNode(AST_YIELD_EXPR, pos, "argument", arg, "delegate", delegateVal, dst);
}

bool
NodeBuilder::comprehensionBlock(HandleValue patt, HandleValue src, bool isForEach, bool isForOf, TokenPos *pos,
                                MutableHandleValue dst)
{
    RootedValue isForEachVal(cx, BooleanValue(isForEach));
    RootedValue isForOfVal(cx, BooleanValue(isForOf));

    RootedValue cb(cx, callbacks[AST_COMP_BLOCK]);
    if (!cb.isNull())
        return callback(cb, patt, src, isForEachVal, isForOfVal, pos, dst);

    return newNode(AST_COMP_BLOCK, pos,
                   "left", patt,
                   "right", src,
                   "each", isForEachVal,
                   "of", isForOfVal,
                   dst);
}

bool
NodeBuilder::comprehensionExpression(HandleValue body, NodeVector &blocks, HandleValue filter,
                                     TokenPos *pos, MutableHandleValue dst)
{
    RootedValue array(cx);
    if (!newArray(blocks, &array))
        return false;

    RootedValue cb(cx, callbacks[AST_COMP_EXPR]);
    if (!cb.isNull())
        return callback(cb, body, array, opt(filter), pos, dst);

    return newNode(AST_COMP_EXPR, pos,
                   "body", body,
                   "blocks", array,
                   "filter", filter,
                   dst);
}

bool
NodeBuilder::generatorExpression(HandleValue body, NodeVector &blocks, HandleValue filter,
                                 TokenPos *pos, MutableHandleValue dst)
{
    RootedValue array(cx);
    if (!newArray(blocks, &array))
        return false;

    RootedValue cb(cx, callbacks[AST_GENERATOR_EXPR]);
    if (!cb.isNull())
        return callback(cb, body, array, opt(filter), pos, dst);

    return newNode(AST_GENERATOR_EXPR, pos,
                   "body", body,
                   "blocks", array,
                   "filter", filter,
                   dst);
}

bool
NodeBuilder::letExpression(NodeVector &head, HandleValue expr, TokenPos *pos,
                           MutableHandleValue dst)
{
    RootedValue array(cx);
    if (!newArray(head, &array))
        return false;

    RootedValue cb(cx, callbacks[AST_LET_EXPR]);
    if (!cb.isNull())
        return callback(cb, array, expr, pos, dst);

    return newNode(AST_LET_EXPR, pos,
                   "head", array,
                   "body", expr,
                   dst);
}

bool
NodeBuilder::letStatement(NodeVector &head, HandleValue stmt, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue array(cx);
    if (!newArray(head, &array))
        return false;

    RootedValue cb(cx, callbacks[AST_LET_STMT]);
    if (!cb.isNull())
        return callback(cb, array, stmt, pos, dst);

    return newNode(AST_LET_STMT, pos,
                   "head", array,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::importDeclaration(NodeVector &elts, HandleValue moduleSpec, TokenPos *pos,
                               MutableHandleValue dst)
{
    RootedValue array(cx);
    if (!newArray(elts, &array))
        return false;

    RootedValue cb(cx, callbacks[AST_IMPORT_DECL]);
    if (!cb.isNull())
        return callback(cb, array, moduleSpec, pos, dst);

    return newNode(AST_IMPORT_DECL, pos,
                   "specifiers", array,
                   "source", moduleSpec,
                   dst);
}

bool
NodeBuilder::importSpecifier(HandleValue importName, HandleValue bindingName, TokenPos *pos,
                             MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_IMPORT_SPEC]);
    if (!cb.isNull())
        return callback(cb, importName, bindingName, pos, dst);

    return newNode(AST_IMPORT_SPEC, pos,
                   "id", importName,
                   "name", bindingName,
                   dst);
}

bool
NodeBuilder::exportDeclaration(HandleValue decl, NodeVector &elts, HandleValue moduleSpec,
                               TokenPos *pos, MutableHandleValue dst)
{
    RootedValue array(cx, NullValue());
    if (decl.isNull() && !newArray(elts, &array))
        return false;

    RootedValue cb(cx, callbacks[AST_IMPORT_DECL]);

    if (!cb.isNull())
        return callback(cb, decl, array, moduleSpec, pos, dst);

    return newNode(AST_EXPORT_DECL, pos,
                   "declaration", decl,
                   "specifiers", array,
                   "source", moduleSpec,
                   dst);
}

bool
NodeBuilder::exportSpecifier(HandleValue bindingName, HandleValue exportName, TokenPos *pos,
                             MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_EXPORT_SPEC]);
    if (!cb.isNull())
        return callback(cb, bindingName, exportName, pos, dst);

    return newNode(AST_EXPORT_SPEC, pos,
                   "id", bindingName,
                   "name", exportName,
                   dst);
}

bool
NodeBuilder::exportBatchSpecifier(TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_EXPORT_BATCH_SPEC]);
    if (!cb.isNull())
        return callback(cb, pos, dst);

    return newNode(AST_EXPORT_BATCH_SPEC, pos, dst);
}

bool
NodeBuilder::variableDeclaration(NodeVector &elts, VarDeclKind kind, TokenPos *pos,
                                 MutableHandleValue dst)
{
    JS_ASSERT(kind > VARDECL_ERR && kind < VARDECL_LIMIT);

    RootedValue array(cx), kindName(cx);
    if (!newArray(elts, &array) ||
        !atomValue(kind == VARDECL_CONST
                   ? "const"
                   : kind == VARDECL_LET
                   ? "let"
                   : "var", &kindName)) {
        return false;
    }

    RootedValue cb(cx, callbacks[AST_VAR_DECL]);
    if (!cb.isNull())
        return callback(cb, kindName, array, pos, dst);

    return newNode(AST_VAR_DECL, pos,
                   "kind", kindName,
                   "declarations", array,
                   dst);
}

bool
NodeBuilder::variableDeclarator(HandleValue id, HandleValue init, TokenPos *pos,
                                MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_VAR_DTOR]);
    if (!cb.isNull())
        return callback(cb, id, opt(init), pos, dst);

    return newNode(AST_VAR_DTOR, pos, "id", id, "init", init, dst);
}

bool
NodeBuilder::switchCase(HandleValue expr, NodeVector &elts, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue array(cx);
    if (!newArray(elts, &array))
        return false;

    RootedValue cb(cx, callbacks[AST_CASE]);
    if (!cb.isNull())
        return callback(cb, opt(expr), array, pos, dst);

    return newNode(AST_CASE, pos,
                   "test", expr,
                   "consequent", array,
                   dst);
}

bool
NodeBuilder::catchClause(HandleValue var, HandleValue guard, HandleValue body, TokenPos *pos,
                         MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_CATCH]);
    if (!cb.isNull())
        return callback(cb, var, opt(guard), body, pos, dst);

    return newNode(AST_CATCH, pos,
                   "param", var,
                   "guard", guard,
                   "body", body,
                   dst);
}

bool
NodeBuilder::literal(HandleValue val, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_LITERAL]);
    if (!cb.isNull())
        return callback(cb, val, pos, dst);

    return newNode(AST_LITERAL, pos, "value", val, dst);
}

bool
NodeBuilder::identifier(HandleValue name, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue cb(cx, callbacks[AST_IDENTIFIER]);
    if (!cb.isNull())
        return callback(cb, name, pos, dst);

    return newNode(AST_IDENTIFIER, pos, "name", name, dst);
}

bool
NodeBuilder::objectPattern(NodeVector &elts, TokenPos *pos, MutableHandleValue dst)
{
    return listNode(AST_OBJECT_PATT, "properties", elts, pos, dst);
}

bool
NodeBuilder::arrayPattern(NodeVector &elts, TokenPos *pos, MutableHandleValue dst)
{
    return listNode(AST_ARRAY_PATT, "elements", elts, pos, dst);
}

bool
NodeBuilder::function(ASTType type, TokenPos *pos,
                      HandleValue id, NodeVector &args, NodeVector &defaults,
                      HandleValue body, HandleValue rest,
                      bool isGenerator, bool isExpression,
                      MutableHandleValue dst)
{
    RootedValue array(cx), defarray(cx);
    if (!newArray(args, &array))
        return false;
    if (!newArray(defaults, &defarray))
        return false;

    RootedValue isGeneratorVal(cx, BooleanValue(isGenerator));
    RootedValue isExpressionVal(cx, BooleanValue(isExpression));

    RootedValue cb(cx, callbacks[type]);
    if (!cb.isNull()) {
        return callback(cb, opt(id), array, body, isGeneratorVal, isExpressionVal, pos, dst);
    }

    return newNode(type, pos,
                   "id", id,
                   "params", array,
                   "defaults", defarray,
                   "body", body,
                   "rest", rest,
                   "generator", isGeneratorVal,
                   "expression", isExpressionVal,
                   dst);
}

namespace {

/*
 * Serialization of parse nodes to JavaScript objects.
 *
 * All serialization methods take a non-nullable ParseNode pointer.
 */
class ASTSerializer
{
    JSContext           *cx;
    Parser<FullParseHandler> *parser;
    NodeBuilder         builder;
    DebugOnly<uint32_t> lineno;

    Value unrootedAtomContents(JSAtom *atom) {
        return StringValue(atom ? atom : cx->names().empty);
    }

    BinaryOperator binop(ParseNodeKind kind, JSOp op);
    UnaryOperator unop(ParseNodeKind kind, JSOp op);
    AssignmentOperator aop(JSOp op);

    bool statements(ParseNode *pn, NodeVector &elts);
    bool expressions(ParseNode *pn, NodeVector &elts);
    bool leftAssociate(ParseNode *pn, MutableHandleValue dst);
    bool functionArgs(ParseNode *pn, ParseNode *pnargs, ParseNode *pndestruct, ParseNode *pnbody,
                      NodeVector &args, NodeVector &defaults, MutableHandleValue rest);

    bool sourceElement(ParseNode *pn, MutableHandleValue dst);

    bool declaration(ParseNode *pn, MutableHandleValue dst);
    bool variableDeclaration(ParseNode *pn, bool let, MutableHandleValue dst);
    bool variableDeclarator(ParseNode *pn, VarDeclKind *pkind, MutableHandleValue dst);
    bool let(ParseNode *pn, bool expr, MutableHandleValue dst);
    bool importDeclaration(ParseNode *pn, MutableHandleValue dst);
    bool importSpecifier(ParseNode *pn, MutableHandleValue dst);
    bool exportDeclaration(ParseNode *pn, MutableHandleValue dst);
    bool exportSpecifier(ParseNode *pn, MutableHandleValue dst);

    bool optStatement(ParseNode *pn, MutableHandleValue dst) {
        if (!pn) {
            dst.setMagic(JS_SERIALIZE_NO_NODE);
            return true;
        }
        return statement(pn, dst);
    }

    bool forInit(ParseNode *pn, MutableHandleValue dst);
    bool forIn(ParseNode *loop, ParseNode *head, HandleValue var, HandleValue stmt,
               MutableHandleValue dst);
    bool forOf(ParseNode *loop, ParseNode *head, HandleValue var, HandleValue stmt,
               MutableHandleValue dst);
    bool statement(ParseNode *pn, MutableHandleValue dst);
    bool blockStatement(ParseNode *pn, MutableHandleValue dst);
    bool switchStatement(ParseNode *pn, MutableHandleValue dst);
    bool switchCase(ParseNode *pn, MutableHandleValue dst);
    bool tryStatement(ParseNode *pn, MutableHandleValue dst);
    bool catchClause(ParseNode *pn, bool *isGuarded, MutableHandleValue dst);

    bool optExpression(ParseNode *pn, MutableHandleValue dst) {
        if (!pn) {
            dst.setMagic(JS_SERIALIZE_NO_NODE);
            return true;
        }
        return expression(pn, dst);
    }

    bool expression(ParseNode *pn, MutableHandleValue dst);

    bool propertyName(ParseNode *pn, MutableHandleValue dst);
    bool property(ParseNode *pn, MutableHandleValue dst);

    bool optIdentifier(HandleAtom atom, TokenPos *pos, MutableHandleValue dst) {
        if (!atom) {
            dst.setMagic(JS_SERIALIZE_NO_NODE);
            return true;
        }
        return identifier(atom, pos, dst);
    }

    bool identifier(HandleAtom atom, TokenPos *pos, MutableHandleValue dst);
    bool identifier(ParseNode *pn, MutableHandleValue dst);
    bool literal(ParseNode *pn, MutableHandleValue dst);

    bool pattern(ParseNode *pn, VarDeclKind *pkind, MutableHandleValue dst);
    bool arrayPattern(ParseNode *pn, VarDeclKind *pkind, MutableHandleValue dst);
    bool objectPattern(ParseNode *pn, VarDeclKind *pkind, MutableHandleValue dst);

    bool function(ParseNode *pn, ASTType type, MutableHandleValue dst);
    bool functionArgsAndBody(ParseNode *pn, NodeVector &args, NodeVector &defaults,
                             MutableHandleValue body, MutableHandleValue rest);
    bool functionBody(ParseNode *pn, TokenPos *pos, MutableHandleValue dst);

    bool comprehensionBlock(ParseNode *pn, MutableHandleValue dst);
    bool comprehension(ParseNode *pn, MutableHandleValue dst);
    bool generatorExpression(ParseNode *pn, MutableHandleValue dst);

  public:
    ASTSerializer(JSContext *c, bool l, char const *src, uint32_t ln)
        : cx(c)
        , builder(c, l, src)
#ifdef DEBUG
        , lineno(ln)
#endif
    {}

    bool init(HandleObject userobj) {
        return builder.init(userobj);
    }

    void setParser(Parser<FullParseHandler> *p) {
        parser = p;
        builder.setTokenStream(&p->tokenStream);
    }

    bool program(ParseNode *pn, MutableHandleValue dst);
};

} /* anonymous namespace */

AssignmentOperator
ASTSerializer::aop(JSOp op)
{
    switch (op) {
      case JSOP_NOP:
        return AOP_ASSIGN;
      case JSOP_ADD:
        return AOP_PLUS;
      case JSOP_SUB:
        return AOP_MINUS;
      case JSOP_MUL:
        return AOP_STAR;
      case JSOP_DIV:
        return AOP_DIV;
      case JSOP_MOD:
        return AOP_MOD;
      case JSOP_LSH:
        return AOP_LSH;
      case JSOP_RSH:
        return AOP_RSH;
      case JSOP_URSH:
        return AOP_URSH;
      case JSOP_BITOR:
        return AOP_BITOR;
      case JSOP_BITXOR:
        return AOP_BITXOR;
      case JSOP_BITAND:
        return AOP_BITAND;
      default:
        return AOP_ERR;
    }
}

UnaryOperator
ASTSerializer::unop(ParseNodeKind kind, JSOp op)
{
    if (kind == PNK_DELETE)
        return UNOP_DELETE;

    switch (op) {
      case JSOP_NEG:
        return UNOP_NEG;
      case JSOP_POS:
        return UNOP_POS;
      case JSOP_NOT:
        return UNOP_NOT;
      case JSOP_BITNOT:
        return UNOP_BITNOT;
      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR:
        return UNOP_TYPEOF;
      case JSOP_VOID:
        return UNOP_VOID;
      default:
        return UNOP_ERR;
    }
}

BinaryOperator
ASTSerializer::binop(ParseNodeKind kind, JSOp op)
{
    switch (kind) {
      case PNK_LSH:
        return BINOP_LSH;
      case PNK_RSH:
        return BINOP_RSH;
      case PNK_URSH:
        return BINOP_URSH;
      case PNK_LT:
        return BINOP_LT;
      case PNK_LE:
        return BINOP_LE;
      case PNK_GT:
        return BINOP_GT;
      case PNK_GE:
        return BINOP_GE;
      case PNK_EQ:
        return BINOP_EQ;
      case PNK_NE:
        return BINOP_NE;
      case PNK_STRICTEQ:
        return BINOP_STRICTEQ;
      case PNK_STRICTNE:
        return BINOP_STRICTNE;
      case PNK_ADD:
        return BINOP_ADD;
      case PNK_SUB:
        return BINOP_SUB;
      case PNK_STAR:
        return BINOP_STAR;
      case PNK_DIV:
        return BINOP_DIV;
      case PNK_MOD:
        return BINOP_MOD;
      case PNK_BITOR:
        return BINOP_BITOR;
      case PNK_BITXOR:
        return BINOP_BITXOR;
      case PNK_BITAND:
        return BINOP_BITAND;
      case PNK_IN:
        return BINOP_IN;
      case PNK_INSTANCEOF:
        return BINOP_INSTANCEOF;
      default:
        return BINOP_ERR;
    }
}

bool
ASTSerializer::statements(ParseNode *pn, NodeVector &elts)
{
    JS_ASSERT(pn->isKind(PNK_STATEMENTLIST));
    JS_ASSERT(pn->isArity(PN_LIST));

    if (!elts.reserve(pn->pn_count))
        return false;

    for (ParseNode *next = pn->pn_head; next; next = next->pn_next) {
        JS_ASSERT(pn->pn_pos.encloses(next->pn_pos));

        RootedValue elt(cx);
        if (!sourceElement(next, &elt))
            return false;
        elts.infallibleAppend(elt);
    }

    return true;
}

bool
ASTSerializer::expressions(ParseNode *pn, NodeVector &elts)
{
    if (!elts.reserve(pn->pn_count))
        return false;

    for (ParseNode *next = pn->pn_head; next; next = next->pn_next) {
        JS_ASSERT(pn->pn_pos.encloses(next->pn_pos));

        RootedValue elt(cx);
        if (!expression(next, &elt))
            return false;
        elts.infallibleAppend(elt);
    }

    return true;
}

bool
ASTSerializer::blockStatement(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT(pn->isKind(PNK_STATEMENTLIST));

    NodeVector stmts(cx);
    return statements(pn, stmts) &&
           builder.blockStatement(stmts, &pn->pn_pos, dst);
}

bool
ASTSerializer::program(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT(parser->tokenStream.srcCoords.lineNum(pn->pn_pos.begin) == lineno);

    NodeVector stmts(cx);
    return statements(pn, stmts) &&
           builder.program(stmts, &pn->pn_pos, dst);
}

bool
ASTSerializer::sourceElement(ParseNode *pn, MutableHandleValue dst)
{
    /* SpiderMonkey allows declarations even in pure statement contexts. */
    return statement(pn, dst);
}

bool
ASTSerializer::declaration(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT(pn->isKind(PNK_FUNCTION) ||
              pn->isKind(PNK_VAR) ||
              pn->isKind(PNK_LET) ||
              pn->isKind(PNK_CONST));

    switch (pn->getKind()) {
      case PNK_FUNCTION:
        return function(pn, AST_FUNC_DECL, dst);

      case PNK_VAR:
      case PNK_CONST:
        return variableDeclaration(pn, false, dst);

      default:
        JS_ASSERT(pn->isKind(PNK_LET));
        return variableDeclaration(pn, true, dst);
    }
}

bool
ASTSerializer::variableDeclaration(ParseNode *pn, bool let, MutableHandleValue dst)
{
    JS_ASSERT(let ? pn->isKind(PNK_LET) : (pn->isKind(PNK_VAR) || pn->isKind(PNK_CONST)));

    /* Later updated to VARDECL_CONST if we find a PND_CONST declarator. */
    VarDeclKind kind = let ? VARDECL_LET : VARDECL_VAR;

    NodeVector dtors(cx);
    if (!dtors.reserve(pn->pn_count))
        return false;
    for (ParseNode *next = pn->pn_head; next; next = next->pn_next) {
        RootedValue child(cx);
        if (!variableDeclarator(next, &kind, &child))
            return false;
        dtors.infallibleAppend(child);
    }
    return builder.variableDeclaration(dtors, kind, &pn->pn_pos, dst);
}

bool
ASTSerializer::variableDeclarator(ParseNode *pn, VarDeclKind *pkind, MutableHandleValue dst)
{
    ParseNode *pnleft;
    ParseNode *pnright;

    if (pn->isKind(PNK_NAME)) {
        pnleft = pn;
        pnright = pn->isUsed() ? nullptr : pn->pn_expr;
        JS_ASSERT_IF(pnright, pn->pn_pos.encloses(pnright->pn_pos));
    } else if (pn->isKind(PNK_ASSIGN)) {
        pnleft = pn->pn_left;
        pnright = pn->pn_right;
        JS_ASSERT(pn->pn_pos.encloses(pnleft->pn_pos));
        JS_ASSERT(pn->pn_pos.encloses(pnright->pn_pos));
    } else {
        /* This happens for a destructuring declarator in a for-in/of loop. */
        pnleft = pn;
        pnright = nullptr;
    }

    RootedValue left(cx), right(cx);
    return pattern(pnleft, pkind, &left) &&
           optExpression(pnright, &right) &&
           builder.variableDeclarator(left, right, &pn->pn_pos, dst);
}

bool
ASTSerializer::let(ParseNode *pn, bool expr, MutableHandleValue dst)
{
    JS_ASSERT(pn->pn_pos.encloses(pn->pn_left->pn_pos));
    JS_ASSERT(pn->pn_pos.encloses(pn->pn_right->pn_pos));

    ParseNode *letHead = pn->pn_left;
    LOCAL_ASSERT(letHead->isArity(PN_LIST));

    ParseNode *letBody = pn->pn_right;
    LOCAL_ASSERT(letBody->isKind(PNK_LEXICALSCOPE));

    NodeVector dtors(cx);
    if (!dtors.reserve(letHead->pn_count))
        return false;

    VarDeclKind kind = VARDECL_LET_HEAD;

    for (ParseNode *next = letHead->pn_head; next; next = next->pn_next) {
        RootedValue child(cx);
        /*
         * Unlike in |variableDeclaration|, this does not update |kind|; since let-heads do
         * not contain const declarations, declarators should never have PND_CONST set.
         */
        if (!variableDeclarator(next, &kind, &child))
            return false;
        dtors.infallibleAppend(child);
    }

    RootedValue v(cx);
    return expr
           ? expression(letBody->pn_expr, &v) &&
             builder.letExpression(dtors, v, &pn->pn_pos, dst)
           : statement(letBody->pn_expr, &v) &&
             builder.letStatement(dtors, v, &pn->pn_pos, dst);
}

bool
ASTSerializer::importDeclaration(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT(pn->isKind(PNK_IMPORT));
    JS_ASSERT(pn->pn_left->isKind(PNK_IMPORT_SPEC_LIST));
    JS_ASSERT(pn->pn_right->isKind(PNK_STRING));

    NodeVector elts(cx);
    if (!elts.reserve(pn->pn_count))
        return false;

    for (ParseNode *next = pn->pn_left->pn_head; next; next = next->pn_next) {
        RootedValue elt(cx);
        if (!importSpecifier(next, &elt))
            return false;
        elts.infallibleAppend(elt);
    }

    RootedValue moduleSpec(cx);
    return literal(pn->pn_right, &moduleSpec) &&
           builder.importDeclaration(elts, moduleSpec, &pn->pn_pos, dst);
}

bool
ASTSerializer::importSpecifier(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT(pn->isKind(PNK_IMPORT_SPEC));

    RootedValue importName(cx);
    RootedValue bindingName(cx);
    return identifier(pn->pn_left, &importName) &&
           identifier(pn->pn_right, &bindingName) &&
           builder.importSpecifier(importName, bindingName, &pn->pn_pos, dst);
}

bool
ASTSerializer::exportDeclaration(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT(pn->isKind(PNK_EXPORT) || pn->isKind(PNK_EXPORT_FROM));
    JS_ASSERT_IF(pn->isKind(PNK_EXPORT_FROM), pn->pn_right->isKind(PNK_STRING));

    RootedValue decl(cx, NullValue());
    NodeVector elts(cx);

    ParseNode *kid = pn->isKind(PNK_EXPORT) ? pn->pn_kid : pn->pn_left;
    switch (ParseNodeKind kind = kid->getKind()) {
      case PNK_EXPORT_SPEC_LIST:
        if (!elts.reserve(pn->pn_count))
            return false;

        for (ParseNode *next = pn->pn_left->pn_head; next; next = next->pn_next) {
            RootedValue elt(cx);
            if (next->isKind(PNK_EXPORT_SPEC)) {
                if (!exportSpecifier(next, &elt))
                    return false;
            } else {
                if (!builder.exportBatchSpecifier(&pn->pn_pos, &elt))
                    return false;
            }
            elts.infallibleAppend(elt);
        }
        break;

      case PNK_FUNCTION:
        if (!function(kid, AST_FUNC_DECL, &decl))
            return false;
        break;

      case PNK_VAR:
      case PNK_CONST:
      case PNK_LET:
        if (!variableDeclaration(kid, kind == PNK_LET, &decl))
            return false;
        break;

      default:
        LOCAL_NOT_REACHED("unexpected statement type");
    }

    RootedValue moduleSpec(cx, NullValue());
    if (pn->isKind(PNK_EXPORT_FROM) && !literal(pn->pn_right, &moduleSpec))
        return false;

    return builder.exportDeclaration(decl, elts, moduleSpec, &pn->pn_pos, dst);
}

bool
ASTSerializer::exportSpecifier(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT(pn->isKind(PNK_EXPORT_SPEC));

    RootedValue bindingName(cx);
    RootedValue exportName(cx);
    return identifier(pn->pn_left, &bindingName) &&
           identifier(pn->pn_right, &exportName) &&
           builder.exportSpecifier(bindingName, exportName, &pn->pn_pos, dst);
}

bool
ASTSerializer::switchCase(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT_IF(pn->pn_left, pn->pn_pos.encloses(pn->pn_left->pn_pos));
    JS_ASSERT(pn->pn_pos.encloses(pn->pn_right->pn_pos));

    NodeVector stmts(cx);

    RootedValue expr(cx);

    return optExpression(pn->pn_left, &expr) &&
           statements(pn->pn_right, stmts) &&
           builder.switchCase(expr, stmts, &pn->pn_pos, dst);
}

bool
ASTSerializer::switchStatement(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT(pn->pn_pos.encloses(pn->pn_left->pn_pos));
    JS_ASSERT(pn->pn_pos.encloses(pn->pn_right->pn_pos));

    RootedValue disc(cx);

    if (!expression(pn->pn_left, &disc))
        return false;

    ParseNode *listNode;
    bool lexical;

    if (pn->pn_right->isKind(PNK_LEXICALSCOPE)) {
        listNode = pn->pn_right->pn_expr;
        lexical = true;
    } else {
        listNode = pn->pn_right;
        lexical = false;
    }

    NodeVector cases(cx);
    if (!cases.reserve(listNode->pn_count))
        return false;

    for (ParseNode *next = listNode->pn_head; next; next = next->pn_next) {
        RootedValue child(cx);
        if (!switchCase(next, &child))
            return false;
        cases.infallibleAppend(child);
    }

    return builder.switchStatement(disc, cases, lexical, &pn->pn_pos, dst);
}

bool
ASTSerializer::catchClause(ParseNode *pn, bool *isGuarded, MutableHandleValue dst)
{
    JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid1->pn_pos));
    JS_ASSERT_IF(pn->pn_kid2, pn->pn_pos.encloses(pn->pn_kid2->pn_pos));
    JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid3->pn_pos));

    RootedValue var(cx), guard(cx), body(cx);

    if (!pattern(pn->pn_kid1, nullptr, &var) ||
        !optExpression(pn->pn_kid2, &guard)) {
        return false;
    }

    *isGuarded = !guard.isMagic(JS_SERIALIZE_NO_NODE);

    return statement(pn->pn_kid3, &body) &&
           builder.catchClause(var, guard, body, &pn->pn_pos, dst);
}

bool
ASTSerializer::tryStatement(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid1->pn_pos));
    JS_ASSERT_IF(pn->pn_kid2, pn->pn_pos.encloses(pn->pn_kid2->pn_pos));
    JS_ASSERT_IF(pn->pn_kid3, pn->pn_pos.encloses(pn->pn_kid3->pn_pos));

    RootedValue body(cx);
    if (!statement(pn->pn_kid1, &body))
        return false;

    NodeVector guarded(cx);
    RootedValue unguarded(cx, NullValue());

    if (pn->pn_kid2) {
        if (!guarded.reserve(pn->pn_kid2->pn_count))
            return false;

        for (ParseNode *next = pn->pn_kid2->pn_head; next; next = next->pn_next) {
            RootedValue clause(cx);
            bool isGuarded;
            if (!catchClause(next->pn_expr, &isGuarded, &clause))
                return false;
            if (isGuarded)
                guarded.infallibleAppend(clause);
            else
                unguarded = clause;
        }
    }

    RootedValue finally(cx);
    return optStatement(pn->pn_kid3, &finally) &&
           builder.tryStatement(body, guarded, unguarded, finally, &pn->pn_pos, dst);
}

bool
ASTSerializer::forInit(ParseNode *pn, MutableHandleValue dst)
{
    if (!pn) {
        dst.setMagic(JS_SERIALIZE_NO_NODE);
        return true;
    }

    return (pn->isKind(PNK_VAR) || pn->isKind(PNK_CONST))
           ? variableDeclaration(pn, false, dst)
           : expression(pn, dst);
}

bool
ASTSerializer::forOf(ParseNode *loop, ParseNode *head, HandleValue var, HandleValue stmt,
                         MutableHandleValue dst)
{
    RootedValue expr(cx);

    return expression(head->pn_kid3, &expr) &&
        builder.forOfStatement(var, expr, stmt, &loop->pn_pos, dst);
}

bool
ASTSerializer::forIn(ParseNode *loop, ParseNode *head, HandleValue var, HandleValue stmt,
                         MutableHandleValue dst)
{
    RootedValue expr(cx);
    bool isForEach = loop->pn_iflags & JSITER_FOREACH;

    return expression(head->pn_kid3, &expr) &&
        builder.forInStatement(var, expr, stmt, isForEach, &loop->pn_pos, dst);
}

bool
ASTSerializer::statement(ParseNode *pn, MutableHandleValue dst)
{
    JS_CHECK_RECURSION(cx, return false);
    switch (pn->getKind()) {
      case PNK_FUNCTION:
      case PNK_VAR:
      case PNK_CONST:
        return declaration(pn, dst);

      case PNK_LET:
        return pn->isArity(PN_BINARY)
               ? let(pn, false, dst)
               : declaration(pn, dst);

      case PNK_IMPORT:
        return importDeclaration(pn, dst);

      case PNK_EXPORT:
      case PNK_EXPORT_FROM:
        return exportDeclaration(pn, dst);

      case PNK_NAME:
        LOCAL_ASSERT(pn->isUsed());
        return statement(pn->pn_lexdef, dst);

      case PNK_SEMI:
        if (pn->pn_kid) {
            RootedValue expr(cx);
            return expression(pn->pn_kid, &expr) &&
                   builder.expressionStatement(expr, &pn->pn_pos, dst);
        }
        return builder.emptyStatement(&pn->pn_pos, dst);

      case PNK_LEXICALSCOPE:
        pn = pn->pn_expr;
        if (!pn->isKind(PNK_STATEMENTLIST))
            return statement(pn, dst);
        /* FALL THROUGH */

      case PNK_STATEMENTLIST:
        return blockStatement(pn, dst);

      case PNK_IF:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid1->pn_pos));
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid2->pn_pos));
        JS_ASSERT_IF(pn->pn_kid3, pn->pn_pos.encloses(pn->pn_kid3->pn_pos));

        RootedValue test(cx), cons(cx), alt(cx);

        return expression(pn->pn_kid1, &test) &&
               statement(pn->pn_kid2, &cons) &&
               optStatement(pn->pn_kid3, &alt) &&
               builder.ifStatement(test, cons, alt, &pn->pn_pos, dst);
      }

      case PNK_SWITCH:
        return switchStatement(pn, dst);

      case PNK_TRY:
        return tryStatement(pn, dst);

      case PNK_WITH:
      case PNK_WHILE:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_left->pn_pos));
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_right->pn_pos));

        RootedValue expr(cx), stmt(cx);

        return expression(pn->pn_left, &expr) &&
               statement(pn->pn_right, &stmt) &&
               (pn->isKind(PNK_WITH)
                ? builder.withStatement(expr, stmt, &pn->pn_pos, dst)
                : builder.whileStatement(expr, stmt, &pn->pn_pos, dst));
      }

      case PNK_DOWHILE:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_left->pn_pos));
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_right->pn_pos));

        RootedValue stmt(cx), test(cx);

        return statement(pn->pn_left, &stmt) &&
               expression(pn->pn_right, &test) &&
               builder.doWhileStatement(stmt, test, &pn->pn_pos, dst);
      }

      case PNK_FOR:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_left->pn_pos));
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_right->pn_pos));

        ParseNode *head = pn->pn_left;

        JS_ASSERT_IF(head->pn_kid1, head->pn_pos.encloses(head->pn_kid1->pn_pos));
        JS_ASSERT_IF(head->pn_kid2, head->pn_pos.encloses(head->pn_kid2->pn_pos));
        JS_ASSERT_IF(head->pn_kid3, head->pn_pos.encloses(head->pn_kid3->pn_pos));

        RootedValue stmt(cx);
        if (!statement(pn->pn_right, &stmt))
            return false;

        if (head->isKind(PNK_FORIN)) {
            RootedValue var(cx);
            return (!head->pn_kid1
                    ? pattern(head->pn_kid2, nullptr, &var)
                    : head->pn_kid1->isKind(PNK_LEXICALSCOPE)
                    ? variableDeclaration(head->pn_kid1->pn_expr, true, &var)
                    : variableDeclaration(head->pn_kid1, false, &var)) &&
                forIn(pn, head, var, stmt, dst);
        }

        if (head->isKind(PNK_FOROF)) {
            RootedValue var(cx);
            return (!head->pn_kid1
                    ? pattern(head->pn_kid2, nullptr, &var)
                    : head->pn_kid1->isKind(PNK_LEXICALSCOPE)
                    ? variableDeclaration(head->pn_kid1->pn_expr, true, &var)
                    : variableDeclaration(head->pn_kid1, false, &var)) &&
                forOf(pn, head, var, stmt, dst);
        }

        RootedValue init(cx), test(cx), update(cx);

        return forInit(head->pn_kid1, &init) &&
               optExpression(head->pn_kid2, &test) &&
               optExpression(head->pn_kid3, &update) &&
               builder.forStatement(init, test, update, stmt, &pn->pn_pos, dst);
      }

      /* Synthesized by the parser when a for-in loop contains a variable initializer. */
      case PNK_SEQ:
      {
        LOCAL_ASSERT(pn->pn_count == 2);

        ParseNode *prelude = pn->pn_head;
        ParseNode *loop = prelude->pn_next;

        LOCAL_ASSERT(prelude->isKind(PNK_VAR) && loop->isKind(PNK_FOR));

        RootedValue var(cx);
        if (!variableDeclaration(prelude, false, &var))
            return false;

        ParseNode *head = loop->pn_left;
        JS_ASSERT(head->isKind(PNK_FORIN));

        RootedValue stmt(cx);

        return statement(loop->pn_right, &stmt) && forIn(loop, head, var, stmt, dst);
      }

      case PNK_BREAK:
      case PNK_CONTINUE:
      {
        RootedValue label(cx);
        RootedAtom pnAtom(cx, pn->pn_atom);
        return optIdentifier(pnAtom, nullptr, &label) &&
               (pn->isKind(PNK_BREAK)
                ? builder.breakStatement(label, &pn->pn_pos, dst)
                : builder.continueStatement(label, &pn->pn_pos, dst));
      }

      case PNK_LABEL:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_expr->pn_pos));

        RootedValue label(cx), stmt(cx);
        RootedAtom pnAtom(cx, pn->as<LabeledStatement>().label());
        return identifier(pnAtom, nullptr, &label) &&
               statement(pn->pn_expr, &stmt) &&
               builder.labeledStatement(label, stmt, &pn->pn_pos, dst);
      }

      case PNK_THROW:
      case PNK_RETURN:
      {
        JS_ASSERT_IF(pn->pn_kid, pn->pn_pos.encloses(pn->pn_kid->pn_pos));

        RootedValue arg(cx);

        return optExpression(pn->pn_kid, &arg) &&
               (pn->isKind(PNK_THROW)
                ? builder.throwStatement(arg, &pn->pn_pos, dst)
                : builder.returnStatement(arg, &pn->pn_pos, dst));
      }

      case PNK_DEBUGGER:
        return builder.debuggerStatement(&pn->pn_pos, dst);

      case PNK_NOP:
        return builder.emptyStatement(&pn->pn_pos, dst);

      default:
        LOCAL_NOT_REACHED("unexpected statement type");
    }
}

bool
ASTSerializer::leftAssociate(ParseNode *pn, MutableHandleValue dst)
{
    JS_ASSERT(pn->isArity(PN_LIST));
    JS_ASSERT(pn->pn_count >= 1);

    ParseNodeKind kind = pn->getKind();
    bool lor = kind == PNK_OR;
    bool logop = lor || (kind == PNK_AND);

    ParseNode *head = pn->pn_head;
    RootedValue left(cx);
    if (!expression(head, &left))
        return false;
    for (ParseNode *next = head->pn_next; next; next = next->pn_next) {
        RootedValue right(cx);
        if (!expression(next, &right))
            return false;

        TokenPos subpos(pn->pn_pos.begin, next->pn_pos.end);

        if (logop) {
            if (!builder.logicalExpression(lor, left, right, &subpos, &left))
                return false;
        } else {
            BinaryOperator op = binop(pn->getKind(), pn->getOp());
            LOCAL_ASSERT(op > BINOP_ERR && op < BINOP_LIMIT);

            if (!builder.binaryExpression(op, left, right, &subpos, &left))
                return false;
        }
    }

    dst.set(left);
    return true;
}

bool
ASTSerializer::comprehensionBlock(ParseNode *pn, MutableHandleValue dst)
{
    LOCAL_ASSERT(pn->isArity(PN_BINARY));

    ParseNode *in = pn->pn_left;

    LOCAL_ASSERT(in && (in->isKind(PNK_FORIN) || in->isKind(PNK_FOROF)));

    bool isForEach = pn->pn_iflags & JSITER_FOREACH;
    bool isForOf = in->isKind(PNK_FOROF);

    RootedValue patt(cx), src(cx);
    return pattern(in->pn_kid2, nullptr, &patt) &&
           expression(in->pn_kid3, &src) &&
           builder.comprehensionBlock(patt, src, isForEach, isForOf, &in->pn_pos, dst);
}

bool
ASTSerializer::comprehension(ParseNode *pn, MutableHandleValue dst)
{
    LOCAL_ASSERT(pn->isKind(PNK_FOR));

    NodeVector blocks(cx);

    ParseNode *next = pn;
    while (next->isKind(PNK_FOR)) {
        RootedValue block(cx);
        if (!comprehensionBlock(next, &block) || !blocks.append(block))
            return false;
        next = next->pn_right;
    }

    RootedValue filter(cx, MagicValue(JS_SERIALIZE_NO_NODE));

    if (next->isKind(PNK_IF)) {
        if (!optExpression(next->pn_kid1, &filter))
            return false;
        next = next->pn_kid2;
    } else if (next->isKind(PNK_STATEMENTLIST) && next->pn_count == 0) {
        /* FoldConstants optimized away the push. */
        NodeVector empty(cx);
        return builder.arrayExpression(empty, &pn->pn_pos, dst);
    }

    LOCAL_ASSERT(next->isKind(PNK_ARRAYPUSH));

    RootedValue body(cx);

    return expression(next->pn_kid, &body) &&
           builder.comprehensionExpression(body, blocks, filter, &pn->pn_pos, dst);
}

bool
ASTSerializer::generatorExpression(ParseNode *pn, MutableHandleValue dst)
{
    LOCAL_ASSERT(pn->isKind(PNK_FOR));

    NodeVector blocks(cx);

    ParseNode *next = pn;
    while (next->isKind(PNK_FOR)) {
        RootedValue block(cx);
        if (!comprehensionBlock(next, &block) || !blocks.append(block))
            return false;
        next = next->pn_right;
    }

    RootedValue filter(cx, MagicValue(JS_SERIALIZE_NO_NODE));

    if (next->isKind(PNK_IF)) {
        if (!optExpression(next->pn_kid1, &filter))
            return false;
        next = next->pn_kid2;
    }

    LOCAL_ASSERT(next->isKind(PNK_SEMI) &&
                 next->pn_kid->isKind(PNK_YIELD) &&
                 next->pn_kid->pn_kid);

    RootedValue body(cx);

    return expression(next->pn_kid->pn_kid, &body) &&
           builder.generatorExpression(body, blocks, filter, &pn->pn_pos, dst);
}

bool
ASTSerializer::expression(ParseNode *pn, MutableHandleValue dst)
{
    JS_CHECK_RECURSION(cx, return false);
    switch (pn->getKind()) {
      case PNK_FUNCTION:
      {
        ASTType type = pn->pn_funbox->function()->isArrow() ? AST_ARROW_EXPR : AST_FUNC_EXPR;
        return function(pn, type, dst);
      }

      case PNK_COMMA:
      {
        NodeVector exprs(cx);
        return expressions(pn, exprs) &&
               builder.sequenceExpression(exprs, &pn->pn_pos, dst);
      }

      case PNK_CONDITIONAL:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid1->pn_pos));
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid2->pn_pos));
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid3->pn_pos));

        RootedValue test(cx), cons(cx), alt(cx);

        return expression(pn->pn_kid1, &test) &&
               expression(pn->pn_kid2, &cons) &&
               expression(pn->pn_kid3, &alt) &&
               builder.conditionalExpression(test, cons, alt, &pn->pn_pos, dst);
      }

      case PNK_OR:
      case PNK_AND:
      {
        if (pn->isArity(PN_BINARY)) {
            JS_ASSERT(pn->pn_pos.encloses(pn->pn_left->pn_pos));
            JS_ASSERT(pn->pn_pos.encloses(pn->pn_right->pn_pos));

            RootedValue left(cx), right(cx);
            return expression(pn->pn_left, &left) &&
                   expression(pn->pn_right, &right) &&
                   builder.logicalExpression(pn->isKind(PNK_OR), left, right, &pn->pn_pos, dst);
        }
        return leftAssociate(pn, dst);
      }

      case PNK_PREINCREMENT:
      case PNK_PREDECREMENT:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid->pn_pos));

        bool inc = pn->isKind(PNK_PREINCREMENT);
        RootedValue expr(cx);
        return expression(pn->pn_kid, &expr) &&
               builder.updateExpression(expr, inc, true, &pn->pn_pos, dst);
      }

      case PNK_POSTINCREMENT:
      case PNK_POSTDECREMENT:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid->pn_pos));

        bool inc = pn->isKind(PNK_POSTINCREMENT);
        RootedValue expr(cx);
        return expression(pn->pn_kid, &expr) &&
               builder.updateExpression(expr, inc, false, &pn->pn_pos, dst);
      }

      case PNK_ASSIGN:
      case PNK_ADDASSIGN:
      case PNK_SUBASSIGN:
      case PNK_BITORASSIGN:
      case PNK_BITXORASSIGN:
      case PNK_BITANDASSIGN:
      case PNK_LSHASSIGN:
      case PNK_RSHASSIGN:
      case PNK_URSHASSIGN:
      case PNK_MULASSIGN:
      case PNK_DIVASSIGN:
      case PNK_MODASSIGN:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_left->pn_pos));
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_right->pn_pos));

        AssignmentOperator op = aop(pn->getOp());
        LOCAL_ASSERT(op > AOP_ERR && op < AOP_LIMIT);

        RootedValue lhs(cx), rhs(cx);
        return pattern(pn->pn_left, nullptr, &lhs) &&
               expression(pn->pn_right, &rhs) &&
               builder.assignmentExpression(op, lhs, rhs, &pn->pn_pos, dst);
      }

      case PNK_ADD:
      case PNK_SUB:
      case PNK_STRICTEQ:
      case PNK_EQ:
      case PNK_STRICTNE:
      case PNK_NE:
      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_STAR:
      case PNK_DIV:
      case PNK_MOD:
      case PNK_BITOR:
      case PNK_BITXOR:
      case PNK_BITAND:
      case PNK_IN:
      case PNK_INSTANCEOF:
        if (pn->isArity(PN_BINARY)) {
            JS_ASSERT(pn->pn_pos.encloses(pn->pn_left->pn_pos));
            JS_ASSERT(pn->pn_pos.encloses(pn->pn_right->pn_pos));

            BinaryOperator op = binop(pn->getKind(), pn->getOp());
            LOCAL_ASSERT(op > BINOP_ERR && op < BINOP_LIMIT);

            RootedValue left(cx), right(cx);
            return expression(pn->pn_left, &left) &&
                   expression(pn->pn_right, &right) &&
                   builder.binaryExpression(op, left, right, &pn->pn_pos, dst);
        }
        return leftAssociate(pn, dst);

      case PNK_DELETE:
      case PNK_TYPEOF:
      case PNK_VOID:
      case PNK_NOT:
      case PNK_BITNOT:
      case PNK_POS:
      case PNK_NEG: {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid->pn_pos));

        UnaryOperator op = unop(pn->getKind(), pn->getOp());
        LOCAL_ASSERT(op > UNOP_ERR && op < UNOP_LIMIT);

        RootedValue expr(cx);
        return expression(pn->pn_kid, &expr) &&
               builder.unaryExpression(op, expr, &pn->pn_pos, dst);
      }

#if JS_HAS_GENERATOR_EXPRS
      case PNK_GENEXP:
        return generatorExpression(pn->generatorExpr(), dst);
#endif

      case PNK_NEW:
      case PNK_TAGGED_TEMPLATE:
      case PNK_CALL:
      {
        ParseNode *next = pn->pn_head;
        JS_ASSERT(pn->pn_pos.encloses(next->pn_pos));

        RootedValue callee(cx);
        if (!expression(next, &callee))
            return false;

        NodeVector args(cx);
        if (!args.reserve(pn->pn_count - 1))
            return false;

        for (next = next->pn_next; next; next = next->pn_next) {
            JS_ASSERT(pn->pn_pos.encloses(next->pn_pos));

            RootedValue arg(cx);
            if (!expression(next, &arg))
                return false;
            args.infallibleAppend(arg);
        }

        if (pn->getKind() == PNK_TAGGED_TEMPLATE)
            return builder.taggedTemplate(callee, args, &pn->pn_pos, dst);

        return pn->isKind(PNK_NEW)
               ? builder.newExpression(callee, args, &pn->pn_pos, dst)

            : builder.callExpression(callee, args, &pn->pn_pos, dst);
      }

      case PNK_DOT:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_expr->pn_pos));

        RootedValue expr(cx), id(cx);
        RootedAtom pnAtom(cx, pn->pn_atom);
        return expression(pn->pn_expr, &expr) &&
               identifier(pnAtom, nullptr, &id) &&
               builder.memberExpression(false, expr, id, &pn->pn_pos, dst);
      }

      case PNK_ELEM:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_left->pn_pos));
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_right->pn_pos));

        RootedValue left(cx), right(cx);
        return expression(pn->pn_left, &left) &&
               expression(pn->pn_right, &right) &&
               builder.memberExpression(true, left, right, &pn->pn_pos, dst);
      }

      case PNK_CALLSITEOBJ:
      {
        NodeVector raw(cx);
        if (!raw.reserve(pn->pn_head->pn_count))
            return false;
        for (ParseNode *next = pn->pn_head->pn_head; next; next = next->pn_next) {
            JS_ASSERT(pn->pn_pos.encloses(next->pn_pos));

            RootedValue expr(cx);
            expr.setString(next->pn_atom);
            raw.infallibleAppend(expr);
        }

        NodeVector cooked(cx);
        if (!cooked.reserve(pn->pn_count - 1))
            return false;

        for (ParseNode *next = pn->pn_head->pn_next; next; next = next->pn_next) {
            JS_ASSERT(pn->pn_pos.encloses(next->pn_pos));

            RootedValue expr(cx);
            expr.setString(next->pn_atom);
            cooked.infallibleAppend(expr);
        }

        return builder.callSiteObj(raw, cooked, &pn->pn_pos, dst);
      }

      case PNK_ARRAY:
      {
        NodeVector elts(cx);
        if (!elts.reserve(pn->pn_count))
            return false;

        for (ParseNode *next = pn->pn_head; next; next = next->pn_next) {
            JS_ASSERT(pn->pn_pos.encloses(next->pn_pos));

            if (next->isKind(PNK_ELISION)) {
                elts.infallibleAppend(NullValue());
            } else {
                RootedValue expr(cx);
                if (!expression(next, &expr))
                    return false;
                elts.infallibleAppend(expr);
            }
        }

        return builder.arrayExpression(elts, &pn->pn_pos, dst);
      }

      case PNK_SPREAD:
      {
          RootedValue expr(cx);
          return expression(pn->pn_kid, &expr) &&
                 builder.spreadExpression(expr, &pn->pn_pos, dst);
      }

      case PNK_OBJECT:
      {
        NodeVector elts(cx);
        if (!elts.reserve(pn->pn_count))
            return false;

        for (ParseNode *next = pn->pn_head; next; next = next->pn_next) {
            JS_ASSERT(pn->pn_pos.encloses(next->pn_pos));

            RootedValue prop(cx);
            if (!property(next, &prop))
                return false;
            elts.infallibleAppend(prop);
        }

        return builder.objectExpression(elts, &pn->pn_pos, dst);
      }

      case PNK_NAME:
        return identifier(pn, dst);

      case PNK_THIS:
        return builder.thisExpression(&pn->pn_pos, dst);

      case PNK_TEMPLATE_STRING_LIST:
      {
        NodeVector elts(cx);
        if (!elts.reserve(pn->pn_count))
            return false;

        for (ParseNode *next = pn->pn_head; next; next = next->pn_next) {
            JS_ASSERT(pn->pn_pos.encloses(next->pn_pos));

            RootedValue expr(cx);
            if (!expression(next, &expr))
                return false;
            elts.infallibleAppend(expr);
        }

        return builder.templateLiteral(elts, &pn->pn_pos, dst);
      }

      case PNK_TEMPLATE_STRING:
      case PNK_STRING:
      case PNK_REGEXP:
      case PNK_NUMBER:
      case PNK_TRUE:
      case PNK_FALSE:
      case PNK_NULL:
        return literal(pn, dst);

      case PNK_YIELD_STAR:
      {
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_kid->pn_pos));

        RootedValue arg(cx);
        return expression(pn->pn_kid, &arg) &&
               builder.yieldExpression(arg, Delegating, &pn->pn_pos, dst);
      }

      case PNK_YIELD:
      {
        JS_ASSERT_IF(pn->pn_kid, pn->pn_pos.encloses(pn->pn_kid->pn_pos));

        RootedValue arg(cx);
        return optExpression(pn->pn_kid, &arg) &&
               builder.yieldExpression(arg, NotDelegating, &pn->pn_pos, dst);
      }

      case PNK_ARRAYCOMP:
        JS_ASSERT(pn->pn_pos.encloses(pn->pn_head->pn_pos));

        /* NB: it's no longer the case that pn_count could be 2. */
        LOCAL_ASSERT(pn->pn_count == 1);
        LOCAL_ASSERT(pn->pn_head->isKind(PNK_LEXICALSCOPE));

        return comprehension(pn->pn_head->pn_expr, dst);

      case PNK_LET:
        return let(pn, true, dst);

      default:
        LOCAL_NOT_REACHED("unexpected expression type");
    }
}

bool
ASTSerializer::propertyName(ParseNode *pn, MutableHandleValue dst)
{
    if (pn->isKind(PNK_NAME))
        return identifier(pn, dst);

    LOCAL_ASSERT(pn->isKind(PNK_STRING) || pn->isKind(PNK_NUMBER));

    return literal(pn, dst);
}

bool
ASTSerializer::property(ParseNode *pn, MutableHandleValue dst)
{
    PropKind kind;
    switch (pn->getOp()) {
      case JSOP_INITPROP:
        kind = PROP_INIT;
        break;

      case JSOP_INITPROP_GETTER:
        kind = PROP_GETTER;
        break;

      case JSOP_INITPROP_SETTER:
        kind = PROP_SETTER;
        break;

      default:
        LOCAL_NOT_REACHED("unexpected object-literal property");
    }

    bool isShorthand = pn->isKind(PNK_SHORTHAND);
    RootedValue key(cx), val(cx);
    return propertyName(pn->pn_left, &key) &&
           expression(pn->pn_right, &val) &&
           builder.propertyInitializer(key, val, kind, isShorthand, &pn->pn_pos, dst);
}

bool
ASTSerializer::literal(ParseNode *pn, MutableHandleValue dst)
{
    RootedValue val(cx);
    switch (pn->getKind()) {
      case PNK_TEMPLATE_STRING:
      case PNK_STRING:
        val.setString(pn->pn_atom);
        break;

      case PNK_REGEXP:
      {
        RootedObject re1(cx, pn->as<RegExpLiteral>().objbox()->object);
        LOCAL_ASSERT(re1 && re1->is<RegExpObject>());

        RootedObject re2(cx, CloneRegExpObject(cx, re1));
        if (!re2)
            return false;

        val.setObject(*re2);
        break;
      }

      case PNK_NUMBER:
        val.setNumber(pn->pn_dval);
        break;

      case PNK_NULL:
        val.setNull();
        break;

      case PNK_TRUE:
        val.setBoolean(true);
        break;

      case PNK_FALSE:
        val.setBoolean(false);
        break;

      default:
        LOCAL_NOT_REACHED("unexpected literal type");
    }

    return builder.literal(val, &pn->pn_pos, dst);
}

bool
ASTSerializer::arrayPattern(ParseNode *pn, VarDeclKind *pkind, MutableHandleValue dst)
{
    JS_ASSERT(pn->isKind(PNK_ARRAY));

    NodeVector elts(cx);
    if (!elts.reserve(pn->pn_count))
        return false;

    for (ParseNode *next = pn->pn_head; next; next = next->pn_next) {
        if (next->isKind(PNK_ELISION)) {
            elts.infallibleAppend(NullValue());
        } else {
            RootedValue patt(cx);
            if (!pattern(next, pkind, &patt))
                return false;
            elts.infallibleAppend(patt);
        }
    }

    return builder.arrayPattern(elts, &pn->pn_pos, dst);
}

bool
ASTSerializer::objectPattern(ParseNode *pn, VarDeclKind *pkind, MutableHandleValue dst)
{
    JS_ASSERT(pn->isKind(PNK_OBJECT));

    NodeVector elts(cx);
    if (!elts.reserve(pn->pn_count))
        return false;

    for (ParseNode *next = pn->pn_head; next; next = next->pn_next) {
        LOCAL_ASSERT(next->isOp(JSOP_INITPROP));

        RootedValue key(cx), patt(cx), prop(cx);
        if (!propertyName(next->pn_left, &key) ||
            !pattern(next->pn_right, pkind, &patt) ||
            !builder.propertyPattern(key, patt, next->isKind(PNK_SHORTHAND), &next->pn_pos,
                                     &prop)) {
            return false;
        }

        elts.infallibleAppend(prop);
    }

    return builder.objectPattern(elts, &pn->pn_pos, dst);
}

bool
ASTSerializer::pattern(ParseNode *pn, VarDeclKind *pkind, MutableHandleValue dst)
{
    JS_CHECK_RECURSION(cx, return false);
    switch (pn->getKind()) {
      case PNK_OBJECT:
        return objectPattern(pn, pkind, dst);

      case PNK_ARRAY:
        return arrayPattern(pn, pkind, dst);

      case PNK_NAME:
        if (pkind && (pn->pn_dflags & PND_CONST))
            *pkind = VARDECL_CONST;
        /* FALL THROUGH */

      default:
        return expression(pn, dst);
    }
}

bool
ASTSerializer::identifier(HandleAtom atom, TokenPos *pos, MutableHandleValue dst)
{
    RootedValue atomContentsVal(cx, unrootedAtomContents(atom));
    return builder.identifier(atomContentsVal, pos, dst);
}

bool
ASTSerializer::identifier(ParseNode *pn, MutableHandleValue dst)
{
    LOCAL_ASSERT(pn->isArity(PN_NAME) || pn->isArity(PN_NULLARY));
    LOCAL_ASSERT(pn->pn_atom);

    RootedAtom pnAtom(cx, pn->pn_atom);
    return identifier(pnAtom, &pn->pn_pos, dst);
}

bool
ASTSerializer::function(ParseNode *pn, ASTType type, MutableHandleValue dst)
{
    RootedFunction func(cx, pn->pn_funbox->function());

    // FIXME: Provide more information (legacy generator vs star generator).
    bool isGenerator = pn->pn_funbox->isGenerator();

    bool isExpression =
#if JS_HAS_EXPR_CLOSURES
        func->isExprClosure();
#else
        false;
#endif

    RootedValue id(cx);
    RootedAtom funcAtom(cx, func->atom());
    if (!optIdentifier(funcAtom, nullptr, &id))
        return false;

    NodeVector args(cx);
    NodeVector defaults(cx);

    RootedValue body(cx), rest(cx);
    if (func->hasRest())
        rest.setUndefined();
    else
        rest.setNull();
    return functionArgsAndBody(pn->pn_body, args, defaults, &body, &rest) &&
        builder.function(type, &pn->pn_pos, id, args, defaults, body,
                         rest, isGenerator, isExpression, dst);
}

bool
ASTSerializer::functionArgsAndBody(ParseNode *pn, NodeVector &args, NodeVector &defaults,
                                   MutableHandleValue body, MutableHandleValue rest)
{
    ParseNode *pnargs;
    ParseNode *pnbody;

    /* Extract the args and body separately. */
    if (pn->isKind(PNK_ARGSBODY)) {
        pnargs = pn;
        pnbody = pn->last();
    } else {
        pnargs = nullptr;
        pnbody = pn;
    }

    ParseNode *pndestruct;

    /* Extract the destructuring assignments. */
    if (pnbody->isArity(PN_LIST) && (pnbody->pn_xflags & PNX_DESTRUCT)) {
        ParseNode *head = pnbody->pn_head;
        LOCAL_ASSERT(head && head->isKind(PNK_SEMI));

        pndestruct = head->pn_kid;
        LOCAL_ASSERT(pndestruct);
        LOCAL_ASSERT(pndestruct->isKind(PNK_VAR));
    } else {
        pndestruct = nullptr;
    }

    /* Serialize the arguments and body. */
    switch (pnbody->getKind()) {
      case PNK_RETURN: /* expression closure, no destructured args */
        return functionArgs(pn, pnargs, nullptr, pnbody, args, defaults, rest) &&
               expression(pnbody->pn_kid, body);

      case PNK_SEQ:    /* expression closure with destructured args */
      {
        ParseNode *pnstart = pnbody->pn_head->pn_next;
        LOCAL_ASSERT(pnstart && pnstart->isKind(PNK_RETURN));

        return functionArgs(pn, pnargs, pndestruct, pnbody, args, defaults, rest) &&
               expression(pnstart->pn_kid, body);
      }

      case PNK_STATEMENTLIST:     /* statement closure */
      {
        ParseNode *pnstart = (pnbody->pn_xflags & PNX_DESTRUCT)
                               ? pnbody->pn_head->pn_next
                               : pnbody->pn_head;

        return functionArgs(pn, pnargs, pndestruct, pnbody, args, defaults, rest) &&
               functionBody(pnstart, &pnbody->pn_pos, body);
      }

      default:
        LOCAL_NOT_REACHED("unexpected function contents");
    }
}

bool
ASTSerializer::functionArgs(ParseNode *pn, ParseNode *pnargs, ParseNode *pndestruct,
                            ParseNode *pnbody, NodeVector &args, NodeVector &defaults,
                            MutableHandleValue rest)
{
    uint32_t i = 0;
    ParseNode *arg = pnargs ? pnargs->pn_head : nullptr;
    ParseNode *destruct = pndestruct ? pndestruct->pn_head : nullptr;
    RootedValue node(cx);

    /*
     * Arguments are found in potentially two different places: 1) the
     * argsbody sequence (which ends with the body node), or 2) a
     * destructuring initialization at the beginning of the body. Loop
     * |arg| through the argsbody and |destruct| through the initial
     * destructuring assignments, stopping only when we've exhausted
     * both.
     */
    while ((arg && arg != pnbody) || destruct) {
        if (destruct && destruct->pn_right->frameSlot() == i) {
            if (!pattern(destruct->pn_left, nullptr, &node) || !args.append(node))
                return false;
            destruct = destruct->pn_next;
        } else if (arg && arg != pnbody) {
            /*
             * We don't check that arg->frameSlot() == i since we
             * can't call that method if the arg def has been turned
             * into a use, e.g.:
             *
             *     function(a) { function a() { } }
             *
             * There's no other way to ask a non-destructuring arg its
             * index in the formals list, so we rely on the ability to
             * ask destructuring args their index above.
             */
            JS_ASSERT(arg->isKind(PNK_NAME) || arg->isKind(PNK_ASSIGN));
            ParseNode *argName = arg->isKind(PNK_NAME) ? arg : arg->pn_left;
            if (!identifier(argName, &node))
                return false;
            if (rest.isUndefined() && arg->pn_next == pnbody)
                rest.setObject(node.toObject());
            else if (!args.append(node))
                return false;
            if (arg->pn_dflags & PND_DEFAULT) {
                ParseNode *expr = arg->expr();
                RootedValue def(cx);
                if (!expression(expr, &def) || !defaults.append(def))
                    return false;
            }
            arg = arg->pn_next;
        } else {
            LOCAL_NOT_REACHED("missing function argument");
        }
        ++i;
    }
    JS_ASSERT(!rest.isUndefined());

    return true;
}

bool
ASTSerializer::functionBody(ParseNode *pn, TokenPos *pos, MutableHandleValue dst)
{
    NodeVector elts(cx);

    /* We aren't sure how many elements there are up front, so we'll check each append. */
    for (ParseNode *next = pn; next; next = next->pn_next) {
        RootedValue child(cx);
        if (!sourceElement(next, &child) || !elts.append(child))
            return false;
    }

    return builder.blockStatement(elts, pos, dst);
}

static bool
reflect_parse(JSContext *cx, uint32_t argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "Reflect.parse", "0", "s");
        return false;
    }

    RootedString src(cx, ToString<CanGC>(cx, args[0]));
    if (!src)
        return false;

    ScopedJSFreePtr<char> filename;
    uint32_t lineno = 1;
    bool loc = true;

    RootedObject builder(cx);

    RootedValue arg(cx, args.get(1));

    if (!arg.isNullOrUndefined()) {
        if (!arg.isObject()) {
            js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_UNEXPECTED_TYPE,
                                     JSDVG_SEARCH_STACK, arg, js::NullPtr(),
                                     "not an object", nullptr);
            return false;
        }

        RootedObject config(cx, &arg.toObject());

        RootedValue prop(cx);

        /* config.loc */
        RootedId locId(cx, NameToId(cx->names().loc));
        RootedValue trueVal(cx, BooleanValue(true));
        if (!GetPropertyDefault(cx, config, locId, trueVal, &prop))
            return false;

        loc = ToBoolean(prop);

        if (loc) {
            /* config.source */
            RootedId sourceId(cx, NameToId(cx->names().source));
            RootedValue nullVal(cx, NullValue());
            if (!GetPropertyDefault(cx, config, sourceId, nullVal, &prop))
                return false;

            if (!prop.isNullOrUndefined()) {
                RootedString str(cx, ToString<CanGC>(cx, prop));
                if (!str)
                    return false;

                filename = JS_EncodeString(cx, str);
                if (!filename)
                    return false;
            }

            /* config.line */
            RootedId lineId(cx, NameToId(cx->names().line));
            RootedValue oneValue(cx, Int32Value(1));
            if (!GetPropertyDefault(cx, config, lineId, oneValue, &prop) ||
                !ToUint32(cx, prop, &lineno)) {
                return false;
            }
        }

        /* config.builder */
        RootedId builderId(cx, NameToId(cx->names().builder));
        RootedValue nullVal(cx, NullValue());
        if (!GetPropertyDefault(cx, config, builderId, nullVal, &prop))
            return false;

        if (!prop.isNullOrUndefined()) {
            if (!prop.isObject()) {
                js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_UNEXPECTED_TYPE,
                                         JSDVG_SEARCH_STACK, prop, js::NullPtr(),
                                         "not an object", nullptr);
                return false;
            }
            builder = &prop.toObject();
        }
    }

    /* Extract the builder methods first to report errors before parsing. */
    ASTSerializer serialize(cx, loc, filename, lineno);
    if (!serialize.init(builder))
        return false;

    JSFlatString *flat = src->ensureFlat(cx);
    if (!flat)
        return false;

    AutoStableStringChars flatChars(cx);
    if (!flatChars.initTwoByte(cx, flat))
        return false;

    CompileOptions options(cx);
    options.setFileAndLine(filename, lineno);
    options.setCanLazilyParse(false);
    mozilla::Range<const jschar> chars = flatChars.twoByteRange();
    Parser<FullParseHandler> parser(cx, &cx->tempLifoAlloc(), options, chars.start().get(),
                                    chars.length(), /* foldConstants = */ false, nullptr, nullptr);

    serialize.setParser(&parser);

    ParseNode *pn = parser.parse(nullptr);
    if (!pn)
        return false;

    RootedValue val(cx);
    if (!serialize.program(pn, &val)) {
        args.rval().setNull();
        return false;
    }

    args.rval().set(val);
    return true;
}

JS_PUBLIC_API(JSObject *)
JS_InitReflect(JSContext *cx, HandleObject obj)
{
    static const JSFunctionSpec static_methods[] = {
        JS_FN("parse", reflect_parse, 1, 0),
        JS_FS_END
    };

    RootedObject proto(cx, obj->as<GlobalObject>().getOrCreateObjectPrototype(cx));
    if (!proto)
        return nullptr;
    RootedObject Reflect(cx, NewObjectWithGivenProto(cx, &JSObject::class_, proto,
                                                     obj, SingletonObject));
    if (!Reflect)
        return nullptr;

    if (!JS_DefineProperty(cx, obj, "Reflect", Reflect, 0,
                           JS_PropertyStub, JS_StrictPropertyStub)) {
        return nullptr;
    }

    if (!JS_DefineFunctions(cx, Reflect, static_methods))
        return nullptr;

    return Reflect;
}
