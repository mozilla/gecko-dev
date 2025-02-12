// META: script=/resources/testharness.js

/**
 * Awaitable which returns when error occurs or the test sends back its name.
 * @param {*} testId - name to distinguish the test
 * @returns promise which resolves if test sends back its name, otherwise rejects.
 */
const waitOutcomesForNames = async nameList => {
  return Promise.allSettled(
    nameList.map(
      name =>
        new Promise((resolve, reject) => {
          const bc = new BroadcastChannel(name);
          bc.onmessage = e => {
            try {
              if (e.data.message.startsWith(name)) {
                resolve("ok");
              } else {
                reject(JSON.stringify(e.data));
              }
            } catch (err) {
              reject(err.message);
            }
          };
        })
    )
  );
};

const expectNamesForTestWindow = (names, windowPath) => {
  return () => {
    return new Promise((resolve, reject) => {
      try {
        waitOutcomesForNames(names).then(res => {
          try {
            if (
              res.every(
                elem => elem.status === "fulfilled" && elem.value == "ok"
              )
            ) {
              resolve();
            } else {
              reject(res.find(elem => elem.status === "rejected").reason);
            }
          } catch (err) {
            reject(err.message);
          }
        });
        window.open(windowPath);
      } catch (err) {
        reject(err.message);
      }
    });
  };
};

/**
 * Tests whether the event is an automated summary from a known window.
 *
 * @param {*} e - event
 * @returns true if event seems to be an automated summary with a recognized name, otherwise false
 */
const isSummary = e => {
  const wrappers = ["Write wrapper", "Read wrapper"];
  const hasName =
    !!e.data.tests && 1 === e.data.tests.length && !!e.data.tests[0].name;
  if (!hasName) {
    return false;
  }
  return wrappers.includes(e.data.tests[0].name);
};

/**
 * Returns expected error message when access to a storage API is denied.
 *
 * @param {*} api Shorthand for the storage API type to be tested
 * @returns Expected error message when access is denied.
 */
const getAccessErrorForAPI = (api, testId) => {
  if (api === "IDB") {
    const storeName = "testObjectStore" + testId;
    return (
      "IDBDatabase.transaction: '" +
      storeName +
      "' is not a known object store name"
    );
  } else if (api === "FS") {
    return "Entry not found"; //"Security error when calling GetDirectory";
  }
  throw Error("Unknown API!");
};

const childListeners = new Map();
const readPromises = new Map();
const readListeners = new Map();

function createMotherListener(
  defaultHandler = e => {
    childListeners.values().next().value(e);
    const msg =
      "Unexpectedly, default handler called: " + JSON.stringify(e.data);
    console.log(msg);
  }
) {
  // The core 'messageHub' listener that checks for a matching child ID
  function messageHubListener(event) {
    console.log("We got message with data " + JSON.stringify(event.data));
    const id = event.data.id;
    if (id) {
      if (event.data.message == "read loaded") {
        if (readListeners.has(id)) {
          if (!readPromises.has(id)) {
            throw new Error("Read window lifecycle issue");
          }
          readListeners.get(id)(event);
        } else {
          readPromises[id] = new Promise(resolve => resolve());
        }
        return;
      } else if (childListeners.has(id)) {
        childListeners.get(id)(event);
        return;
      }
    }
    defaultHandler(event);
  }

  // Start listening to window messages right away
  window.addEventListener("message", messageHubListener);

  // Return an API to register new child listeners
  return {
    registerWindow(t, testId, testAPI, expectation, setup) {
      if (childListeners.has(testId)) {
        throw new Error(`Window ID "${testId}" is already registered.`);
      }
      const handler = getWindowTestListener(
        t,
        testAPI,
        testId,
        expectation,
        "window",
        setup
      );
      childListeners.set(testId, handler);
      console.log("Registered window id " + testId);
    },
    registerWorker(t, testId, testAPI, expectation, setup) {
      if (childListeners.has(testId)) {
        throw new Error(`Worker ID "${testId}" is already registered.`);
      }
      const handler = getWindowTestListener(
        t,
        testAPI,
        testId,
        expectation,
        "worker",
        setup
      );
      childListeners.set(testId, handler);
      console.log("Registered worker id " + testId);
    },
    registerReadWindow(testId) {
      if (readPromises.has(testId)) {
        return;
      }
      readPromises.set(
        testId,
        new Promise((resolve, reject) => {
          readListeners.set(testId, e => {
            if (e.data.id != testId) {
              reject("Expected read id " + testId + ", actual " + e.data.id);
            }
            resolve();
          });
        })
      );
      console.log("Registered read window id " + testId);
    },
    async getReadWindow(testId) {
      if (!readPromises.has(testId)) {
        throw new Error("Read window lifecycle issue");
      }

      return readPromises.get(testId);
    },
  };
}

/**
 * Requires that writeWindows and readWindows are defined in the calling context.
 * @param {*} t - test object provided by the wpt test harness
 * @param {*} testId - name to distinguish the test
 * @param {*} expectation - final message from the tested iframe
 * @returns test step function to be used as a window listener
 */
const getWindowTestListener = (
  t,
  testAPI,
  testId,
  expectation,
  contextType,
  setup
) => {
  const bc = new BroadcastChannel(testId);

  assert_true(["window", "worker"].includes(contextType));

  const readFrame = "read-frame-" + contextType;

  assert_true(["allow", "deny"].includes(expectation));

  const expectedMessage =
    expectation == "allow" ? testId : getAccessErrorForAPI(testAPI, testId);

  const ownedReadWindow = (s => {
    if (s.readWindows) {
      return s.readWindows.get(testId);
    }
    return null;
  })(setup);

  const ownedWriteWindows = setup.writeWindows;

  return t.step_func(e => {
    const here = {};

    try {
      console.log("Test listener received " + JSON.stringify(e.data));
      here.ownedReadWindow = ownedReadWindow;
      here.writeWindows = ownedWriteWindows;
      here.bc = bc;
      here.api = testAPI;
      here.id = testId;
      here.expected = expectedMessage;
      here.readFrame = readFrame;

      // Test summary is automatically sent to parent window
      if (isSummary(e)) {
        const maybeError = e.data.tests[0].message;
        if (maybeError) {
          here.bc.postMessage({ id: here.id, message: maybeError });
          throw new Error(maybeError);
        }
      } else {
        // Otherwise it should follow this protocol.
        if (e.data.id !== here.id) {
          const msg = "id " + here.id + " ignores message for id " + e.data.id;
          console.log(msg);
          here.bc.postMessage({ id: here.id, message: msg });
          throw new Error(msg);
        }

        assert_true(!!e.data.message);
        if (e.data.message === "write loaded") {
          const msg = { id: here.id, message: here.id, type: here.api };
          here.writeWindows.get(here.id).postMessage(msg, "*");
        } else if (e.data.message === "write done") {
          assert_true(!!e.data.expected); // What we wrote to the database
          const msg = {
            id: here.id,
            message: here.id,
            expected: e.data.expected, // What was written to storage
            outcome: here.expected, // What the iframe should send to its parent
            frame: here.readFrame, // Should it go to the worker or window iframe?
            type: here.api, // Which storage API should be tested?
          };

          here.ownedReadWindow.postMessage(msg, "*");
        } else {
          assert_equals(e.data.message, here.expected);
          here.bc.postMessage({ id: here.id, message: here.id });
          t.done();
        }
      }
    } catch (err) {
      here.bc.postMessage({ id: here.id, message: err.message });
      throw err;
    }
  });
};
