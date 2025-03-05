/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function setupServiceWorkerNavigationTest() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
}

async function watchServiceWorkerTargets(tab) {
  info("Create a target list for a tab target");
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;

  // Enable Service Worker listening.
  targetCommand.listenForServiceWorkers = true;
  await targetCommand.startListening();

  // Setup onAvailable & onDestroyed callbacks so that we can check how many
  // times they are called and with which targetFront.
  const hooks = {
    availableCount: 0,
    destroyedCount: 0,
    targets: [],
  };

  const onAvailable = async ({ targetFront }) => {
    info(` + Service worker target available for ${targetFront.url}\n`);
    hooks.availableCount++;
    hooks.targets.push(targetFront);
  };

  const onDestroyed = ({ targetFront }) => {
    info(` - Service worker target destroy for ${targetFront.url}\n`);
    hooks.destroyedCount++;
    hooks.targets.splice(hooks.targets.indexOf(targetFront), 1);
  };

  await targetCommand.watchTargets({
    types: [targetCommand.TYPES.SERVICE_WORKER],
    onAvailable,
    onDestroyed,
  });

  return { hooks, commands, targetCommand };
}

/**
 * Wait until the expected URL is loaded and win.registration has resolved.
 */
async function waitForRegistrationReady(tab, expectedPageUrl, workerUrl) {
  await asyncWaitUntil(() =>
    SpecialPowers.spawn(tab.linkedBrowser, [expectedPageUrl], function (_url) {
      try {
        const win = content.wrappedJSObject;
        const isExpectedUrl = win.location.href === _url;
        const hasRegistration = !!win.registrationPromise;
        return isExpectedUrl && hasRegistration;
      } catch (e) {
        return false;
      }
    })
  );
  // On debug builds, the registration may not be yet ready in the parent process
  // so we also need to ensure it is ready.
  const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );
  await waitFor(() => {
    // Unfortunately we can't use swm.getRegistrationByPrincipal, as it requires a "scope", which doesn't seem to be the worker URL.
    const registrations = swm.getAllRegistrations();
    for (let i = 0; i < registrations.length; i++) {
      const info = registrations.queryElementAt(
        i,
        Ci.nsIServiceWorkerRegistrationInfo
      );
      // Lookup for an exact URL match.
      if (info.scriptSpec === workerUrl) {
        return true;
      }
    }
    return false;
  });
}

/**
 * Assert helper for the `hooks` object, updated by the onAvailable and
 * onDestroyed callbacks. Assert that the callbacks have been called the
 * expected number of times, with the expected targets.
 */
async function checkHooks(hooks, { available, destroyed, targets }) {
  await waitUntil(
    () => hooks.availableCount == available && hooks.destroyedCount == destroyed
  );
  is(hooks.availableCount, available, "onAvailable was called as expected");
  is(hooks.destroyedCount, destroyed, "onDestroyed was called as expected");

  is(hooks.targets.length, targets.length, "Expected number of targets");
  targets.forEach((url, i) => {
    is(hooks.targets[i].url, url, `SW target ${i} has the expected url`);
  });
}
