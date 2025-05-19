/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const gMIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

const gExternalProtocolService = Cc[
  "@mozilla.org/uriloader/external-protocol-service;1"
].getService(Ci.nsIExternalProtocolService);

const gHandlerService = Cc[
  "@mozilla.org/uriloader/handler-service;1"
].getService(Ci.nsIHandlerService);

// This seems odd, but for test purposes, this just has to be a file that we know exists,
// and by using this file, we don't have to worry about different platforms.
let exeFile = Services.dirsvc.get("XREExeF", Ci.nsIFile);

add_task(async function test_disable_builtin_pdf() {
  await setupPolicyEngineWithJson({
    policies: {
      DisableBuiltinPDFViewer: true,
    },
  });

  let handlerInfo = gMIMEService.getFromTypeAndExtension("application/pdf", "");
  is(handlerInfo.preferredAction, handlerInfo.useSystemDefault);
  is(handlerInfo.alwaysAskBeforeHandling, false);
});

add_task(async function test_disable_builtin_pdf() {
  await setupPolicyEngineWithJson({
    policies: {
      DisableBuiltinPDFViewer: false,
    },
  });

  let handlerInfo = gMIMEService.getFromTypeAndExtension("application/pdf", "");
  is(handlerInfo.preferredAction, handlerInfo.handleInternally);
  is(handlerInfo.alwaysAskBeforeHandling, false);
});

add_task(async function test_handler_unchanged() {
  await setupPolicyEngineWithJson({
    policies: {
      DisableBuiltinPDFViewer: true,
      Handlers: {
        mimeTypes: {
          "application/pdf": {
            action: "useHelperApp",
            ask: true,
            handlers: [
              {
                name: "Launch",
                path: exeFile.path,
              },
            ],
          },
        },
      },
    },
  });

  let handlerInfo = gMIMEService.getFromTypeAndExtension("application/pdf", "");
  is(handlerInfo.preferredAction, handlerInfo.useHelperApp);
  is(handlerInfo.alwaysAskBeforeHandling, true);
  is(handlerInfo.preferredApplicationHandler.name, "Launch");
  is(handlerInfo.preferredApplicationHandler.executable.path, exeFile.path);

  gHandlerService.remove(handlerInfo);
});
