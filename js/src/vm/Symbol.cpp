/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Symbol.h"

#include "jscntxt.h"
#include "jscompartment.h"

#include "gc/Rooting.h"

#include "jscompartmentinlines.h"
#include "jsgcinlines.h"

using JS::Symbol;
using namespace js;

Symbol *
Symbol::newInternal(ExclusiveContext *cx, JS::SymbolCode code, JSAtom *description)
{
    MOZ_ASSERT(cx->compartment() == cx->atomsCompartment());
    MOZ_ASSERT(cx->atomsCompartment()->runtimeFromAnyThread()->currentThreadHasExclusiveAccess());

    // Following js::AtomizeString, we grudgingly forgo last-ditch GC here.
    Symbol *p = gc::AllocateNonObject<Symbol, NoGC>(cx);
    if (!p) {
        js_ReportOutOfMemory(cx);
        return nullptr;
    }
    return new (p) Symbol(code, description);
}

Symbol *
Symbol::new_(ExclusiveContext *cx, JS::SymbolCode code, JSString *description)
{
    RootedAtom atom(cx);
    if (description) {
        atom = AtomizeString(cx, description);
        if (!atom)
            return nullptr;
    }

    // Lock to allocate. If symbol allocation becomes a bottleneck, this can
    // probably be replaced with an assertion that we're on the main thread.
    AutoLockForExclusiveAccess lock(cx);
    AutoCompartment ac(cx, cx->atomsCompartment());
    return newInternal(cx, code, atom);
}

Symbol *
Symbol::for_(js::ExclusiveContext *cx, HandleString description)
{
    JSAtom *atom = AtomizeString(cx, description);
    if (!atom)
        return nullptr;

    AutoLockForExclusiveAccess lock(cx);

    SymbolRegistry &registry = cx->symbolRegistry();
    SymbolRegistry::AddPtr p = registry.lookupForAdd(atom);
    if (p)
        return *p;

    AutoCompartment ac(cx, cx->atomsCompartment());
    Symbol *sym = newInternal(cx, SymbolCode::InSymbolRegistry, atom);
    if (!sym)
        return nullptr;

    // p is still valid here because we have held the lock since the
    // lookupForAdd call, and newInternal can't GC.
    if (!registry.add(p, sym)) {
        // SystemAllocPolicy does not report OOM.
        js_ReportOutOfMemory(cx);
        return nullptr;
    }
    return sym;
}

#ifdef DEBUG
void
Symbol::dump(FILE *fp)
{
    if (isWellKnownSymbol()) {
        // All the well-known symbol names are ASCII.
        const jschar *desc = description_->chars();
        size_t len = description_->length();
        for (size_t i = 0; i < len; i++)
            fputc(char(desc[i]), fp);
    } else if (code_ == SymbolCode::InSymbolRegistry || code_ == SymbolCode::UniqueSymbol) {
        fputs(code_ == SymbolCode::InSymbolRegistry ? "Symbol.for(" : "Symbol(", fp);

        if (description_)
            JSString::dumpChars(description_->chars(), description_->length(), fp);
        else
            fputs("undefined", fp);

        fputc(')', fp);

        if (code_ == SymbolCode::UniqueSymbol)
            fprintf(fp, "@%p", (void *) this);
    } else {
        fprintf(fp, "<Invalid Symbol code=%u>", unsigned(code_));
    }
}
#endif  // DEBUG

void
SymbolRegistry::sweep()
{
    for (Enum e(*this); !e.empty(); e.popFront()) {
        Symbol *sym = e.front();
        if (IsSymbolAboutToBeFinalized(&sym))
            e.removeFront();
    }
}
