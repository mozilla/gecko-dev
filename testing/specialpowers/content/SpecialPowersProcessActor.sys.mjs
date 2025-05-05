/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SpecialPowersSandbox:
    "resource://testing-common/SpecialPowersSandbox.sys.mjs",
});

let nextSpfpId = 1;

/**
 * SpecialPowersForProcess wraps a content process, and allows the caller to
 * spawn() tasks like SpecialPowers.spawn() and contentPage.spawn(), including
 * Assert functionality. Assertion messages are passed back to the test scope,
 * which must be passed along with the process to the constructor.
 */
export class SpecialPowersForProcess {
  static instances = new Map();

  /**
   * Create a new SpecialPowersForProcess that enables callers to spawn tasks
   * in the given content process.
   *
   * @param {any} scope
   *        The test scope to receive assertion messages.
   *        In test files this is often equivalent to globalThis.
   * @param {nsIDOMProcessParent} domProcess
   *        The content process where the spawned code should run.
   */
  constructor(testScope, domProcess) {
    this.isXpcshellScope = !!testScope.do_get_profile;
    this.isSimpleTestScope = !this.isXpcshellScope && !!testScope.SimpleTest;
    if (!this.isXpcshellScope && !this.isSimpleTestScope) {
      // Must be global of xpcshell test, or global of browser chrome mochitest.
      throw new Error("testScope cannot receive assertion messages!");
    }

    if (!(domProcess instanceof Ci.nsIDOMProcessParent)) {
      throw new Error("domProcess is not a nsIDOMProcessParent!");
    }

    // This actor is registered via SpecialPowersParent.registerActor().
    // In mochitests that is part of the SpecialPowers add-on initialization.
    // Xpcshell tests can initialize it with XPCShellContentUtils.init(), via
    // XPCShellContentUtils.ensureInitialized().
    this.actor = domProcess.getActor("SpecialPowersProcessActor");

    this.testScope = testScope;

    this.spfpId = nextSpfpId++;
    SpecialPowersForProcess.instances.set(this.spfpId, this);

    testScope.registerCleanupFunction(() => this.destroy());
  }

  destroy() {
    if (!this.testScope) {
      // Already destroyed.
      return;
    }
    SpecialPowersForProcess.instances.delete(this.spfpId);
    this.actor = null;
    this.testScope = null;
  }

  /**
   * Like `SpecialPowers.spawn`, but spawns a task in a child process instead.
   * The task has access to Assert.
   *
   * @param {Array<any>} args
   *        An array of arguments to pass to the task. All arguments
   *        must be structured clone compatible, and will be cloned
   *        before being passed to the task.
   * @param {function} task
   *        The function to run in the context of the target process. The
   *        function will be stringified and re-evaluated in the context
   *        of the target's content process. It may return any structured
   *        clone compatible value, or a Promise which resolves to the
   *        same, which will be returned to the caller.
   * @returns {Promise<any>}
   *        A promise which resolves to the return value of the task, or
   *        which rejects if the task raises an exception. As this is
   *        being written, the rejection value will always be undefined
   *        in the cases where the task throws an error, though that may
   *        change in the future.
   */
  spawn(args, task) {
    return this.actor.sendQuery("Spawn", {
      args,
      task: String(task),
      caller: Cu.getFunctionSourceLocation(task),
      spfpId: this.spfpId,
    });
  }

  // Called when ProxiedAssert is received; this data is sent from
  // SpecialPowersSandbox's reportCallback in response to assertion messages.
  reportCallback(data) {
    if ("info" in data) {
      if (this.isXpcshellScope) {
        this.testScope.info(data.info);
      } else if (this.isSimpleTestScope) {
        this.testScope.SimpleTest.info(data.info);
      } else {
        // We checked this in the constructor, so this is unexpected.
        throw new Error(`testScope cannot receive assertion messages!?!`);
      }
      return;
    }

    const { name, diag, passed, stack, expectFail } = data;
    if (this.isXpcshellScope) {
      this.testScope.do_report_result(passed, name, stack);
    } else if (this.isSimpleTestScope) {
      // browser chrome mochitest
      let expected = expectFail ? "fail" : "pass";
      this.testScope.SimpleTest.record(passed, name, diag, stack, expected);
    } else {
      // We checked this in the constructor, so this is unexpected.
      throw new Error(`testScope cannot receive assertion messages!?!`);
    }
  }
}

// A minimal process actor that allows spawn() to run in the given process.
export class SpecialPowersProcessActorParent extends JSProcessActorParent {
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "ProxiedAssert": {
        const { spfpId, data } = aMessage.data;
        const spfp = SpecialPowersForProcess.instances.get(spfpId);
        if (!spfp) {
          dump(`Unexpected ProxiedAssert: ${uneval(data)}\n`);
          throw new Error(`Unexpected message for ${spfpId} `);
        }
        spfp.reportCallback(data);
        return undefined;
      }
      default:
        throw new Error(
          `Unknown SpecialPowersProcessActorParent action: ${aMessage.name}`
        );
    }
  }
}

export class SpecialPowersProcessActorChild extends JSProcessActorChild {
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "Spawn": {
        return this._spawnTaskInChild(aMessage.data);
      }
      default:
        throw new Error(
          `Unknown SpecialPowersProcessActorChild action: ${aMessage.name}`
        );
    }
  }

  _spawnTaskInChild({ task, args, caller, spfpId }) {
    let sb = new lazy.SpecialPowersSandbox(null, data => {
      this.sendAsyncMessage("ProxiedAssert", { spfpId, data });
    });

    return sb.execute(task, args, caller);
  }
}
