// META: title=Origin private file system used from multiple tabs
// META: script=support/testHelpers.js
// META: timeout=long

const okResponse = "200 OK";
const firstTabContent = "Hello from the first tab!";

const expectCloseIsOk = async (t, ev, aSubDir, aName) => {
  if (ev.data != okResponse) {
    throw new Error("Expect close is ok callback failed");
  }

  const fileHandle = await aSubDir.getFileHandle(aName, { create: false });
  const file = await fileHandle.getFile();
  const text = await file.text();

  t.step(() => {
    assert_equals(firstTabContent, text, "Does the content look good?");
  });
};

const expectErrorCallback = errorMsg => {
  return async (t, ev) => {
    if (ev.data != errorMsg) {
      t.step(() => {
        assert_equals(ev.data, errorMsg, "Expect error callback failed");
      });
    }
  };
};

const expectInternalError = expectErrorCallback(
  "Internal error closing file stream"
);

const expectModificationError = expectErrorCallback("No modification allowed");

const expectNotFoundError = expectErrorCallback("Entry not found");

function addWritableClosingTest(testName, secondTabURL, expectationChecks) {
  promise_test(async t => {
    const firstTabURL =
      "support/fs-create_writables_and_close_on_trigger.sub.html";
    const channelName = crypto.randomUUID(); // Avoid cross-talk between tests
    const dirHandleName = "dusty-dir-handle-" + channelName;
    const fileHandleName = "funky-file-handle-" + channelName;

    const params = new URLSearchParams(
      secondTabURL.slice(secondTabURL.lastIndexOf("?"))
    );
    t.step(() => {
      assert_true(params.size > 0, "Missing search parameters");
    });

    const channelParams = "channel=" + channelName;
    const bc = new BroadcastChannel(channelName);
    t.add_cleanup(() => {
      bc.close();
    });

    const rootDir = await navigator.storage.getDirectory();
    const subDir = await rootDir.getDirectoryHandle(dirHandleName, {
      create: true,
    });
    await subDir.getFileHandle(fileHandleName, { create: true });

    async function firstReady(win) {
      return new Promise((resolve, reject) => {
        bc.onmessage = e => {
          if (e.data === "First window ready!") {
            resolve(win);
          } else {
            t.step(() => {
              reject(e.data);
              assert_equals(
                "First window ready!",
                e.data,
                "Is first window ready?"
              );
            });
          }
        };
      });
    }

    const firstTabLocation = firstTabURL + "?" + channelParams;
    const firstTab = await firstReady(window.open(firstTabLocation));
    t.step(() => {
      assert_true(!!firstTab, "Is the first tab fine?");
      assert_false(firstTab.closed, "Is the first tab open?");
    });

    let secondTab = null;
    async function secondReady(secondTabLocation) {
      let win = null;
      return new Promise((resolve, reject) => {
        bc.onmessage = async ev => {
          if (expectationChecks.length > 1) {
            try {
              await expectationChecks.shift()(t, ev, subDir, fileHandleName);
            } catch (err) {
              reject(err);
              return;
            }
          }

          resolve(win);
        };

        try {
          win = window.open(secondTabLocation);
        } catch (err) {
          reject(err);
        }
      });
    }
    const secondTabLocation = secondTabURL + "&" + channelParams;
    try {
      secondTab = await secondReady(secondTabLocation);
    } catch (err) {
      if (expectationChecks.length > 1) {
        await expectationChecks.shift()(t, { data: err.message });
      } else {
        t.step_func(() => {
          throw err;
        });
      }
    } finally {
      try {
        const closeHandles = async () => {
          return new Promise((resolve, reject) => {
            bc.onmessage = async ev => {
              try {
                if (expectationChecks) {
                  await expectationChecks.shift()(
                    t,
                    ev,
                    subDir,
                    fileHandleName
                  );
                } else {
                  t.step_func(() => {
                    throw err;
                  });
                }
                resolve();
              } catch (err) {
                reject(err);
              }
            };

            bc.postMessage("trigger");
          });
        };

        await closeHandles();
      } catch (err) {
        if (expectationChecks) {
          await expectationChecks.shift()(t, { data: err.message });
        }
      } finally {
        const waitForCleanup = async () => {
          return new Promise((resolve, reject) => {
            let firstDone = false;
            let secondDone = !secondTab;

            bc.onmessage = ev => {
              if (ev.data == "first done") {
                firstDone = true;
              } else if (ev.data == "done") {
                secondDone = true;
              } else {
                t.step(() => {
                  assert_false(
                    true,
                    "We got a cleanup message " + JSON.stringify(ev.data)
                  );
                });
                reject(new Error(ev.data));
              }

              if (firstDone && secondDone) {
                resolve();
              }
            };

            firstTab.postMessage("cleanup");
            if (secondTab) {
              bc.postMessage("cleanup");
            }
          });
        };

        try {
          await waitForCleanup();

          for await (let entry of rootDir.values()) {
            console.log(entry.name);
            await rootDir.removeEntry(entry.name, { recursive: true });
          }
        } catch (err) {
          t.step_func(() => {
            assert_unreached(err.message);
          });
        } finally {
          t.done();
        }
      }
    }
  }, testName);
}

addWritableClosingTest(
  `closing writable in single tab is success`,
  "support/fs-noop.sub.html?op=move",
  [expectCloseIsOk]
);

addWritableClosingTest(
  `closing writable fails silently on move of the parent directory`,
  "support/fs-relocate_dir_to_trash.sub.html?op=move",
  [expectCloseIsOk, expectNotFoundError, expectCloseIsOk, expectNotFoundError]
);

addWritableClosingTest(
  `closing writable fails silently on rename of the parent directory`,
  "support/fs-relocate_dir_to_trash.sub.html?op=rename",
  [expectCloseIsOk, expectNotFoundError, expectCloseIsOk, expectNotFoundError]
);

addWritableClosingTest(
  `closing writable succeeds after move of the parent directory is rolled back`,
  "support/fs-relocate_dir_to_trash_and_back.sub.html?op=move",
  [expectCloseIsOk]
);

addWritableClosingTest(
  `closing writable succeeds after rename of the parent directory is rolled back`,
  "support/fs-relocate_dir_to_trash_and_back.sub.html?op=rename",
  [expectCloseIsOk]
);

/**
 * Test case
 * `removeEntry() of a directory while a containing file has an open writable fails`
 * in web platform test /fs/FileSystemDirectoryHandle-removeEntry.https.any.html
 * currently requires that directory cannot be moved while it contains open
 * writable file streams, even if they are not owner by the current context.
 *
 * Without this limitation, the test should yield
 * "Internal error closing file stream" because the requested close cannot be completed
 * because the required file path no longer exists, and the request is aborted.
 */
addWritableClosingTest(
  `closing writable yields error on removal of the parent directory`,
  "support/fs-remove_dir.sub.html?op=remove",
  [expectModificationError, expectCloseIsOk]
);

addWritableClosingTest(
  `closing old writable succeeds after directory tree is moved and created again`,
  "support/fs-relocate_dir_to_trash_and_recreate.sub.html?op=move",
  [expectCloseIsOk]
);

addWritableClosingTest(
  `closing old writable succeeds after directory tree is renamed and created again`,
  "support/fs-relocate_dir_to_trash_and_recreate.sub.html?op=rename",
  [expectCloseIsOk]
);

addWritableClosingTest(
  `closing old writable succeeds after directory tree is moved, created again and the new file has shared lock open`,
  "support/fs-relocate_dir_to_trash_and_recreate.sub.html?op=move&keep_open=true",
  [expectCloseIsOk]
);

addWritableClosingTest(
  `closing old writable fails after directory tree is renamed and created again and the new file has shared lock open`,
  "support/fs-relocate_dir_to_trash_and_recreate.sub.html?op=rename&keep_open=true",
  [expectCloseIsOk]
);

addWritableClosingTest(
  `closing old writable succeeds after directory tree is overwritten by move`,
  "support/fs-overwrite_existing_dir.sub.html?op=move",
  [expectCloseIsOk]
);

addWritableClosingTest(
  `closing old writable succeeds after directory tree is overwritten by rename`,
  "support/fs-overwrite_existing_dir.sub.html?op=rename",
  [expectCloseIsOk]
);

addWritableClosingTest(
  `closing old writable succeeds after directory tree is overwritten by move with new writable open`,
  "support/fs-overwrite_existing_dir.sub.html?op=move&keep_open=true",
  [expectCloseIsOk]
);

addWritableClosingTest(
  `closing old writable succeeds after directory tree is overwritten by rename with new writable open`,
  "support/fs-overwrite_existing_dir.sub.html?op=rename&keep_open=true",
  [expectCloseIsOk]
);

addWritableClosingTest(
  `after overwriting directory with move check that no files are left behind`,
  "support/fs-overwrite_leaves_no_files_behind.sub.html?op=move",
  [expectNotFoundError, expectCloseIsOk]
);

addWritableClosingTest(
  `after overwriting directory with rename check that no files are left behind`,
  "support/fs-overwrite_leaves_no_files_behind.sub.html?op=rename",
  [expectNotFoundError, expectCloseIsOk]
);
