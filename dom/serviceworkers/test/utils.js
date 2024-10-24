function waitForState(worker, state, context) {
  return new Promise(resolve => {
    function onStateChange() {
      if (worker.state === state) {
        worker.removeEventListener("statechange", onStateChange);
        resolve(context);
      }
    }

    // First add an event listener, so we won't miss any change that happens
    // before we check the current state.
    worker.addEventListener("statechange", onStateChange);

    // Now check if the worker is already in the desired state.
    onStateChange();
  });
}

/**
 * Helper for browser tests to issue register calls from the content global and
 * wait for the SW to progress to the active state, as most tests desire.
 * From the ContentTask.spawn, use via
 * `content.wrappedJSObject.registerAndWaitForActive`.
 */
async function registerAndWaitForActive(script, maybeScope) {
  console.log("...calling register");
  let opts = undefined;
  if (maybeScope) {
    opts = { scope: maybeScope };
  }
  const reg = await navigator.serviceWorker.register(script, opts);
  // Unless registration resurrection happens, the SW should be in the
  // installing slot.
  console.log("...waiting for activation");
  await waitForState(reg.installing, "activated", reg);
  console.log("...activated!");
  return reg;
}

/**
 * Helper to create an iframe with the given URL and return the first
 * postMessage payload received.  This is intended to be used when creating
 * cross-origin iframes.
 *
 * A promise will be returned that resolves with the payload of the postMessage
 * call.
 */
function createIframeAndWaitForMessage(url) {
  const iframe = document.createElement("iframe");
  document.body.appendChild(iframe);
  return new Promise(resolve => {
    window.addEventListener(
      "message",
      event => {
        resolve(event.data);
      },
      { once: true }
    );
    iframe.src = url;
  });
}

/**
 * Helper to create a nested iframe into the iframe created by
 * createIframeAndWaitForMessage().
 *
 * A promise will be returned that resolves with the payload of the postMessage
 * call.
 */
function createNestedIframeAndWaitForMessage(url) {
  const iframe = document.getElementsByTagName("iframe")[0];
  iframe.contentWindow.postMessage("create nested iframe", "*");
  return new Promise(resolve => {
    window.addEventListener(
      "message",
      event => {
        resolve(event.data);
      },
      { once: true }
    );
  });
}

async function unregisterAll() {
  const registrations = await navigator.serviceWorker.getRegistrations();
  for (const reg of registrations) {
    await reg.unregister();
  }
}

/**
 * Make a blob that contains random data and therefore shouldn't compress all
 * that well.
 */
function makeRandomBlob(size) {
  const arr = new Uint8Array(size);
  let offset = 0;
  /**
   * getRandomValues will only provide a maximum of 64k of data at a time and
   * will error if we ask for more, so using a while loop for get a random value
   * which much larger than 64k.
   * https://developer.mozilla.org/en-US/docs/Web/API/Crypto/getRandomValues#exceptions
   */
  while (offset < size) {
    const nextSize = Math.min(size - offset, 65536);
    window.crypto.getRandomValues(new Uint8Array(arr.buffer, offset, nextSize));
    offset += nextSize;
  }
  return new Blob([arr], { type: "application/octet-stream" });
}

async function fillStorage(cacheBytes, idbBytes) {
  // ## Fill Cache API Storage
  const cache = await caches.open("filler");
  await cache.put("fill", new Response(makeRandomBlob(cacheBytes)));

  // ## Fill IDB
  const storeName = "filler";
  let db = await new Promise((resolve, reject) => {
    let openReq = indexedDB.open("filler", 1);
    openReq.onerror = event => {
      reject(event.target.error);
    };
    openReq.onsuccess = event => {
      resolve(event.target.result);
    };
    openReq.onupgradeneeded = event => {
      const useDB = event.target.result;
      useDB.onerror = error => {
        reject(error);
      };
      const store = useDB.createObjectStore(storeName);
      store.put({ blob: makeRandomBlob(idbBytes) }, "filler-blob");
    };
  });
}

const messagingChannels = {};

// This method should ideally be called during a setup phase of the test to make
// sure our BroadcastChannel is fully connected before anything that could cause
// something to send a message to the channel can happen.  Because IPC ordering
// is more predictable these days (single channel per process pair), this is
// primarily an issue of:
// - Helping you not have to worry about there being a race here at all.
// - Potentially be able to refactor this to run on the WPT infrastructure in
//   the future which likely cannot provide the same ordering guarantees.
function setupMessagingChannel(name) {
  if (messagingChannels[name]) {
    return;
  }

  messagingChannels[name] = new BroadcastChannel(name);
}

function waitForBroadcastMessage(channelName, messageToWaitFor) {
  if (!messagingChannels[channelName]) {
    throw new Error(`You forgot to call setupMessagingChannel(${channelName})`);
  }
  return new Promise((resolve, reject) => {
    const channel = messagingChannels[channelName];
    const listener = evt => {
      // Add `--setpref="devtools.console.stdout.content=true"` to your mach
      // invocation to get this to stdout for extra debugging.
      console.log("Helper seeing message", evt.data, "on channel", channelName);
      if (evt.data === messageToWaitFor) {
        resolve();
        channel.removeEventListener("message", listener);
      } else if (evt.data?.error) {
        // Anything reporting an error means we should fail fast.
        reject(evt.data);
        channel.removeEventListener("message", listener);
      }
    };
    channel.addEventListener("message", listener);
  });
}

async function postMessageScopeAndWaitFor(
  channelName,
  scope,
  messageToSend,
  messageToWaitFor
) {
  // This will throw for us if the channel does not exist.
  const waitPromise = waitForBroadcastMessage(channelName, messageToWaitFor);
  const channel = messagingChannels[channelName];

  const reg = await navigator.serviceWorker.getRegistration(scope);
  if (!reg) {
    throw new Error(`Unable to find registration for scope: ${scope}`);
  }
  if (!reg.active) {
    throw new Error(`There is no active SW on the reg for scope: ${scope}`);
  }
  reg.active.postMessage(messageToSend);

  await waitPromise;
}

async function broadcastAndWaitFor(
  channelName,
  messageToBroadcast,
  messageToWaitFor
) {
  // This will throw for us if the channel does not exist.
  const waitPromise = waitForBroadcastMessage(channelName, messageToWaitFor);
  const channel = messagingChannels[channelName];

  channel.postMessage(messageToBroadcast);

  await waitPromise;
}

async function updateScopeAndWaitFor(channelName, scope, messageToWaitFor) {
  // This will throw for us if the channel does not exist.
  const waitPromise = waitForBroadcastMessage(channelName, messageToWaitFor);
  const channel = messagingChannels[channelName];

  const reg = await navigator.serviceWorker.getRegistration(scope);
  if (!reg) {
    throw new Error(`Unable to find registration for scope: ${scope}`);
  }
  reg.update();

  await waitPromise;
}
