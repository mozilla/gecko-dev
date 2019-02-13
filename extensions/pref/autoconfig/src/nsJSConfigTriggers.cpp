/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "nsIXPConnect.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsIPrefService.h"
#include "nspr.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsJSPrincipals.h"
#include "jswrapper.h"

extern PRLogModuleInfo *MCD;
using mozilla::AutoSafeJSContext;

//*****************************************************************************

static JS::PersistentRooted<JSObject *> autoconfigSb;

nsresult CentralizedAdminPrefManagerInit()
{
    nsresult rv;

    // If the sandbox is already created, no need to create it again.
    if (autoconfigSb.initialized())
        return NS_OK;

    // Grab XPConnect.
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);
    if (NS_FAILED(rv)) {
        return rv;
    }

    // Grab the system principal.
    nsCOMPtr<nsIPrincipal> principal;
    nsContentUtils::GetSecurityManager()->GetSystemPrincipal(getter_AddRefs(principal));


    // Create a sandbox.
    AutoSafeJSContext cx;
    nsCOMPtr<nsIXPConnectJSObjectHolder> sandbox;
    rv = xpc->CreateSandbox(cx, principal, getter_AddRefs(sandbox));
    NS_ENSURE_SUCCESS(rv, rv);

    // Unwrap, store and root the sandbox.
    NS_ENSURE_STATE(sandbox->GetJSObject());
    autoconfigSb.init(cx, js::UncheckedUnwrap(sandbox->GetJSObject()));

    return NS_OK;
}

nsresult CentralizedAdminPrefManagerFinish()
{
    if (autoconfigSb.initialized()) {
        AutoSafeJSContext cx;
        autoconfigSb.reset();
        JS_MaybeGC(cx);
    }
    return NS_OK;
}

nsresult EvaluateAdminConfigScript(const char *js_buffer, size_t length,
                                   const char *filename, bool bGlobalContext,
                                   bool bCallbacks, bool skipFirstLine)
{
    nsresult rv = NS_OK;

    if (skipFirstLine) {
        /* In order to protect the privacy of the JavaScript preferences file
         * from loading by the browser, we make the first line unparseable
         * by JavaScript. We must skip that line here before executing
         * the JavaScript code.
         */
        unsigned int i = 0;
        while (i < length) {
            char c = js_buffer[i++];
            if (c == '\r') {
                if (js_buffer[i] == '\n')
                    i++;
                break;
            }
            if (c == '\n')
                break;
        }

        length -= i;
        js_buffer += i;
    }

    // Grab XPConnect.
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);
    if (NS_FAILED(rv)) {
        return rv;
    }

    AutoSafeJSContext cx;
    JSAutoCompartment ac(cx, autoconfigSb);

    nsAutoCString script(js_buffer, length);
    JS::RootedValue v(cx);
    rv = xpc->EvalInSandboxObject(NS_ConvertUTF8toUTF16(script), filename, cx,
                                  autoconfigSb, &v);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

