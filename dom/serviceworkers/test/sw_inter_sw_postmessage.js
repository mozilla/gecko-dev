/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let bc = new BroadcastChannel("inter-sw-postmessage");
let myId = /\/sw-(.)+$/.exec(registration.scope)[1];
// If we are being imported by the generated script from
// `sw_always_updating_inter_sw_postmessage.sjs`, there will be a "version"
// global and it starts counting from 1.
let myVersion = "version" in globalThis ? globalThis.version : 0;
let myFullId = `${myId}#${myVersion}`;

onactivate = function () {
  bc.postMessage(`${myId}:version-activated:${myVersion}`);
};

function extractId(urlStr) {
  if (!urlStr) {
    return urlStr;
  }
  const qIndex = urlStr.indexOf("?");
  if (qIndex >= 0) {
    return urlStr.substring(qIndex + 1);
  }
  if (urlStr.endsWith("/empty_with_utils.html")) {
    return "helper";
  }
  return urlStr;
}

function describeSource(source) {
  // Note that WindowProxy is impossible here, so we don't check it.
  if (source === null) {
    return "null";
  } else if (source instanceof MessagePort) {
    return "port";
  } else if (source instanceof WindowClient) {
    return `wc-${extractId(source.url)}`;
  } else if (source instanceof Client) {
    return `c-${extractId(source.url)}`;
  } else if (source instanceof ServiceWorker) {
    return `sw-${extractId(source.scriptURL)}`;
  } else {
    return "unexpected";
  }
}

let lastPostMessageSource = null;
globalThis.onmessage = async function handle_message(evt) {
  console.log(myId, "received postMessage");
  lastPostMessageSource = evt.source;
  bc.postMessage(
    `${myId}:received-post-message-from:${describeSource(evt.source)}`
  );
};

/**
 * Map a target descriptor onto something we can postMessage.  Possible options
 * and the resulting target:
 * - `last-source`: The `.source` property of the most recent event received via
 *   `globalThis.onmessage`.
 * - `reg-sw-ID`: The active ServiceWorker found on a registration whose
 *   scriptURL ends with `?ID`.  This allows us to distinguish between multiple
 *   (non-self-updating) ServiceWorkers on the same registration because each SW
 *   can be given a distinct script path via the `?ID` suffix.  But it does not
 *   work for self-updating ServiceWorkers where the only difference is the
 *   version identifier embedded in the script itself.
 */
async function resolveTarget(descriptor) {
  if (descriptor === "last-source") {
    return lastPostMessageSource;
  } else if (descriptor.startsWith("reg-")) {
    const registrations = await navigator.serviceWorker.getRegistrations();
    let filterFunc;
    if (descriptor.startsWith("reg-sw-")) {
      const descriptorId = /^reg-sw-(.+)$/.exec(descriptor)[1];
      console.log(
        "Looking for registration with id",
        descriptorId,
        "across",
        registrations.length,
        "registrations"
      );
      filterFunc = sw => {
        if (sw) {
          console.log("checking SW", sw.scriptURL);
        }
        return extractId(sw?.scriptURL) === descriptorId;
      };
    } else {
      throw new Error(`Target selector '${descriptor}' not understood`);
    }

    for (const reg of registrations) {
      console.log("Reg scriptURL", reg.active?.scriptURL);
      if (filterFunc(reg.active)) {
        return reg.active;
      } else if (filterFunc(reg.waiting)) {
        return reg.waiting;
      } else if (filterFunc(reg.installing)) {
        return reg.installing;
      }
    }
    throw new Error("No registration matches found!");
  }
  throw new Error(`Target selector '${descriptor}' not understood`);
}

/**
 * Map a registration descriptor onto a registration.  Options:
 * - `scope-ID`: The registration with a scope ending with `/sw-ID`.
 */
async function resolveRegistration(descriptor) {
  if (descriptor.startsWith("sw-")) {
    const registrations = await navigator.serviceWorker.getRegistrations();

    const scopeSuffix = `/${descriptor}`;
    for (const reg of registrations) {
      if (reg.scope.endsWith(scopeSuffix)) {
        return reg;
      }
    }

    throw new Error("No registration matches found!");
  }
  throw new Error(`Registration selector '${descriptor}' not understood`);
}

bc.onmessage = async function handle_bc(evt) {
  // Split the message into colon-delimited commands of the form:
  // <who should do the thing>:<the command>:<the target of the command>
  if (typeof evt?.data !== "string") {
    return;
  }
  const pieces = evt?.data?.split(":");
  if (
    !pieces ||
    pieces.length < 2 ||
    (pieces[0] !== myId && pieces[0] !== myFullId)
  ) {
    return;
  }

  const cmd = pieces[1];
  try {
    if (cmd === "post-message-to") {
      const target = await resolveTarget(pieces[2]);
      target.postMessage("yo!");
    } else if (cmd === "update-reg") {
      const reg = await resolveRegistration(pieces[2]);
      reg.update();
    } else if (cmd === "install-reg") {
      const installId = pieces[2];
      const scope = `sw-${installId}`;
      const script = `sw_inter_sw_postmessage.js?${installId}`;
      await navigator.serviceWorker.register(script, {
        scope,
      });
      bc.postMessage(`${myId}:registered:${installId}`);
    }
  } catch (ex) {
    console.error(ex);
    bc.postMessage({
      error: ex + "",
      myId,
      processing: evt?.data,
    });
  }
};
