/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  let p = new Promise(resolve => {
    function consoleListener() {
      addConsoleStorageListener(this);
    }

    consoleListener.prototype = {
      observe(aSubject) {
        let obj = aSubject.wrappedJSObject;
        Assert.strictEqual(
          obj.arguments[0],
          "Hello world!",
          "Message received!"
        );
        Assert.strictEqual(obj.ID, "scope", "The ID is the scope");
        Assert.strictEqual(
          obj.innerID,
          "ServiceWorker",
          "The innerID is ServiceWorker"
        );
        Assert.strictEqual(obj.filename, "filename", "The filename matches");
        Assert.strictEqual(obj.lineNumber, 42, "The lineNumber matches");
        Assert.strictEqual(obj.columnNumber, 24, "The columnNumber matches");
        Assert.strictEqual(obj.level, "error", "The level is correct");

        removeConsoleStorageListener(this);
        resolve();
      },
    };

    new consoleListener();
  });

  let ci = console.createInstance();
  ci.reportForServiceWorkerScope(
    "scope",
    "Hello world!",
    "filename",
    42,
    24,
    "error"
  );
  await p;
});
