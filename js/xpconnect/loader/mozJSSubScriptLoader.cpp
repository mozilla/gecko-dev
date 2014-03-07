/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozJSSubScriptLoader.h"
#include "mozJSComponentLoader.h"
#include "mozJSLoaderUtils.h"

#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIFileURL.h"
#include "nsScriptLoader.h"
#include "nsIScriptSecurityManager.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/OldDebugAPI.h"
#include "nsJSPrincipals.h"
#include "xpcpublic.h" // For xpc::SystemErrorReporter
#include "xpcprivate.h" // For xpc::OptionsBase

#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"

using namespace mozilla::scache;
using namespace JS;
using namespace xpc;

class MOZ_STACK_CLASS LoadSubScriptOptions : public OptionsBase {
public:
    LoadSubScriptOptions(JSContext *cx = xpc_GetSafeJSContext(),
                         JSObject *options = nullptr)
        : OptionsBase(cx, options)
        , target(cx)
        , charset(NullString())
        , ignoreCache(false)
    { }

    virtual bool Parse() {
      return ParseObject("target", &target) &&
             ParseString("charset", charset) &&
             ParseBoolean("ignoreCache", &ignoreCache);
    }

    RootedObject target;
    nsString charset;
    bool ignoreCache;
};


/* load() error msgs, XXX localize? */
#define LOAD_ERROR_NOSERVICE "Error creating IO Service."
#define LOAD_ERROR_NOURI "Error creating URI (invalid URL scheme?)"
#define LOAD_ERROR_NOSCHEME "Failed to get URI scheme.  This is bad."
#define LOAD_ERROR_URI_NOT_LOCAL "Trying to load a non-local URI."
#define LOAD_ERROR_NOSTREAM  "Error opening input stream (invalid filename?)"
#define LOAD_ERROR_NOCONTENT "ContentLength not available (not a local URL?)"
#define LOAD_ERROR_BADCHARSET "Error converting to specified charset"
#define LOAD_ERROR_BADREAD   "File Read Error."
#define LOAD_ERROR_READUNDERFLOW "File Read Error (underflow.)"
#define LOAD_ERROR_NOPRINCIPALS "Failed to get principals."
#define LOAD_ERROR_NOSPEC "Failed to get URI spec.  This is bad."
#define LOAD_ERROR_CONTENTTOOBIG "ContentLength is too large"

mozJSSubScriptLoader::mozJSSubScriptLoader() : mSystemPrincipal(nullptr)
{
    // Force construction of the JS component loader.  We may need it later.
    nsCOMPtr<xpcIJSModuleLoader> componentLoader =
        do_GetService(MOZJSCOMPONENTLOADER_CONTRACTID);
}

mozJSSubScriptLoader::~mozJSSubScriptLoader()
{
    /* empty */
}

NS_IMPL_ISUPPORTS1(mozJSSubScriptLoader, mozIJSSubScriptLoader)

static nsresult
ReportError(JSContext *cx, const char *msg)
{
    RootedValue exn(cx, JS::StringValue(JS_NewStringCopyZ(cx, msg)));
    JS_SetPendingException(cx, exn);
    return NS_OK;
}

nsresult
mozJSSubScriptLoader::ReadScript(nsIURI *uri, JSContext *cx, JSObject *targetObjArg,
                                 const nsAString &charset, const char *uriStr,
                                 nsIIOService *serv, nsIPrincipal *principal,
                                 bool reuseGlobal, JSScript **scriptp,
                                 JSFunction **functionp)
{
    RootedObject target_obj(cx, targetObjArg);

    *scriptp = nullptr;
    *functionp = nullptr;

    // Instead of calling NS_OpenURI, we create the channel ourselves and call
    // SetContentType, to avoid expensive MIME type lookups (bug 632490).
    nsCOMPtr<nsIChannel> chan;
    nsCOMPtr<nsIInputStream> instream;
    nsresult rv = NS_NewChannel(getter_AddRefs(chan), uri, serv,
                                nullptr, nullptr, nsIRequest::LOAD_NORMAL);
    if (NS_SUCCEEDED(rv)) {
        chan->SetContentType(NS_LITERAL_CSTRING("application/javascript"));
        rv = chan->Open(getter_AddRefs(instream));
    }

    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOSTREAM);
    }

    int64_t len = -1;

    rv = chan->GetContentLength(&len);
    if (NS_FAILED(rv) || len == -1) {
        return ReportError(cx, LOAD_ERROR_NOCONTENT);
    }

    if (len > INT32_MAX) {
        return ReportError(cx, LOAD_ERROR_CONTENTTOOBIG);
    }

    nsCString buf;
    rv = NS_ReadInputStreamToString(instream, buf, len);
    if (NS_FAILED(rv))
        return rv;

    /* set our own error reporter so we can report any bad things as catchable
     * exceptions, including the source/line number */
    JSErrorReporter er = JS_SetErrorReporter(cx, xpc::SystemErrorReporter);

    JS::CompileOptions options(cx);
    options.setFileAndLine(uriStr, 1);
    if (!charset.IsVoid()) {
        nsString script;
        rv = nsScriptLoader::ConvertToUTF16(nullptr, reinterpret_cast<const uint8_t*>(buf.get()), len,
                                            charset, nullptr, script);

        if (NS_FAILED(rv)) {
            return ReportError(cx, LOAD_ERROR_BADCHARSET);
        }

        if (!reuseGlobal) {
            *scriptp = JS::Compile(cx, target_obj, options,
                                   script.get(),
                                   script.Length());
        } else {
            *functionp = JS::CompileFunction(cx, target_obj, options,
                                             nullptr, 0, nullptr,
                                             script.get(),
                                             script.Length());
        }
    } else {
        // We only use LAZY_SOURCE when no special encoding is specified because
        // the lazy source loader doesn't know the encoding.
        if (!reuseGlobal) {
            options.setSourcePolicy(JS::CompileOptions::LAZY_SOURCE);
            *scriptp = JS::Compile(cx, target_obj, options, buf.get(), len);
        } else {
            *functionp = JS::CompileFunction(cx, target_obj, options,
                                             nullptr, 0, nullptr, buf.get(),
                                             len);
        }
    }

    /* repent for our evil deeds */
    JS_SetErrorReporter(cx, er);

    return NS_OK;
}

NS_IMETHODIMP
mozJSSubScriptLoader::LoadSubScript(const nsAString &url,
                                    HandleValue target,
                                    const nsAString &charset,
                                    JSContext *cx,
                                    MutableHandleValue retval)
{
    /*
     * Loads a local url and evals it into the current cx
     * Synchronous (an async version would be cool too.)
     *   url: The url to load.  Must be local so that it can be loaded
     *        synchronously.
     *   target_obj: Optional object to eval the script onto (defaults to context
     *               global)
     *   charset: Optional character set to use for reading
     *   returns: Whatever jsval the script pointed to by the url returns.
     * Should ONLY (O N L Y !) be called from JavaScript code.
     */
    LoadSubScriptOptions options(cx);
    options.charset = charset;
    options.target = target.isObject() ? &target.toObject() : nullptr;
    return DoLoadSubScriptWithOptions(url, options, cx, retval);
}


NS_IMETHODIMP
mozJSSubScriptLoader::LoadSubScriptWithOptions(const nsAString &url,
                                               HandleValue optionsVal,
                                               JSContext *cx,
                                               MutableHandleValue retval)
{
    if (!optionsVal.isObject())
        return NS_ERROR_INVALID_ARG;
    LoadSubScriptOptions options(cx, &optionsVal.toObject());
    if (!options.Parse())
        return NS_ERROR_INVALID_ARG;
    return DoLoadSubScriptWithOptions(url, options, cx, retval);
}

nsresult
mozJSSubScriptLoader::DoLoadSubScriptWithOptions(const nsAString &url,
                                                 LoadSubScriptOptions &options,
                                                 JSContext *cx,
                                                 MutableHandleValue retval)
{
    nsresult rv = NS_OK;

    /* set the system principal if it's not here already */
    if (!mSystemPrincipal) {
        nsCOMPtr<nsIScriptSecurityManager> secman =
            do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
        if (!secman)
            return NS_OK;

        rv = secman->GetSystemPrincipal(getter_AddRefs(mSystemPrincipal));
        if (NS_FAILED(rv) || !mSystemPrincipal)
            return rv;
    }

    RootedObject targetObj(cx);
    mozJSComponentLoader *loader = mozJSComponentLoader::Get();
    rv = loader->FindTargetObject(cx, &targetObj);
    NS_ENSURE_SUCCESS(rv, rv);

    // We base reusingGlobal off of what the loader told us, but we may not
    // actually be using that object.
    bool reusingGlobal = !JS_IsGlobalObject(targetObj);

    if (options.target)
        targetObj = options.target;

    // Remember an object out of the calling compartment so that we
    // can properly wrap the result later.
    nsCOMPtr<nsIPrincipal> principal = mSystemPrincipal;
    RootedObject result_obj(cx, targetObj);
    targetObj = JS_FindCompilationScope(cx, targetObj);
    if (!targetObj)
        return NS_ERROR_FAILURE;

    if (targetObj != result_obj)
        principal = GetObjectPrincipal(targetObj);

    JSAutoCompartment ac(cx, targetObj);

    /* load up the url.  From here on, failures are reflected as ``custom''
     * js exceptions */
    nsCOMPtr<nsIURI> uri;
    nsAutoCString uriStr;
    nsAutoCString scheme;

    // Figure out who's calling us
    JS::AutoFilename filename;
    if (!JS::DescribeScriptedCaller(cx, &filename)) {
        // No scripted frame means we don't know who's calling, bail.
        return NS_ERROR_FAILURE;
    }

    // Suppress caching if we're compiling as content.
    StartupCache* cache = (principal == mSystemPrincipal)
                          ? StartupCache::GetSingleton()
                          : nullptr;
    nsCOMPtr<nsIIOService> serv = do_GetService(NS_IOSERVICE_CONTRACTID);
    if (!serv) {
        return ReportError(cx, LOAD_ERROR_NOSERVICE);
    }

    // Make sure to explicitly create the URI, since we'll need the
    // canonicalized spec.
    rv = NS_NewURI(getter_AddRefs(uri), NS_LossyConvertUTF16toASCII(url).get(), nullptr, serv);
    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOURI);
    }

    rv = uri->GetSpec(uriStr);
    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOSPEC);
    }

    rv = uri->GetScheme(scheme);
    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOSCHEME);
    }

    if (!scheme.EqualsLiteral("chrome")) {
        // This might be a URI to a local file, though!
        nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
        nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(innerURI);
        if (!fileURL) {
            return ReportError(cx, LOAD_ERROR_URI_NOT_LOCAL);
        }

        // For file URIs prepend the filename with the filename of the
        // calling script, and " -> ". See bug 418356.
        nsAutoCString tmp(filename.get());
        tmp.AppendLiteral(" -> ");
        tmp.Append(uriStr);

        uriStr = tmp;
    }

    bool writeScript = false;
    JSVersion version = JS_GetVersion(cx);
    nsAutoCString cachePath;
    cachePath.AppendPrintf("jssubloader/%d", version);
    PathifyURI(uri, cachePath);

    RootedFunction function(cx);
    RootedScript script(cx);
    if (cache && !options.ignoreCache)
        rv = ReadCachedScript(cache, cachePath, cx, mSystemPrincipal, &script);
    if (!script) {
        rv = ReadScript(uri, cx, targetObj, options.charset,
                        static_cast<const char*>(uriStr.get()), serv,
                        principal, reusingGlobal, script.address(), function.address());
        writeScript = !!script;
    }

    if (NS_FAILED(rv) || (!script && !function))
        return rv;

    if (function) {
        script = JS_GetFunctionScript(cx, function);
    }

    loader->NoteSubScript(script, targetObj);


    bool ok = false;
    if (function) {
        ok = JS_CallFunction(cx, targetObj, function, JS::HandleValueArray::empty(),
                             retval);
    } else {
        ok = JS_ExecuteScriptVersion(cx, targetObj, script, retval.address(), version);
    }

    if (ok) {
        JSAutoCompartment rac(cx, result_obj);
        if (!JS_WrapValue(cx, retval))
            return NS_ERROR_UNEXPECTED;
    }

    if (cache && ok && writeScript) {
        WriteCachedScript(cache, cachePath, cx, mSystemPrincipal, script);
    }

    return NS_OK;
}
