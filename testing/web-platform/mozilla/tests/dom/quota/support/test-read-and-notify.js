let testId;

let messageSource;
let messageSink;
let messageTarget;
try {
  messageSource = window;
  messageSink = window.parent;
  messageTarget = document.referrer;
} catch (_) {
  messageSource = self;
  messageSink = self;
  messageTarget = null;
}

messageSource.addEventListener("message", async e => {
  try {
    console.log("Read script received " + e.data.message);
    testId = e.data.message;
    const storeName = "testObjectStore" + testId;
    let actual;

    if (e.data.type === "IDB") {
      const openReq = indexedDB.open(testId);
      const db = await new Promise((openResolve, openReject) => {
        openReq.onerror = err => {
          openReject(err);
        };
        openReq.onsuccess = ev => {
          openResolve(ev.target.result);
        };
      });

      const objectStore = db
        .transaction([storeName], "readonly")
        .objectStore(storeName);
      const item = await new Promise((getResolve, getReject) => {
        const getReq = objectStore.get("actual");
        getReq.onerror = err => {
          getReject(err);
        };
        getReq.onsuccess = ev => {
          getResolve(ev.target.result);
        };
      });

      actual = await item.text();
    } else if (e.data.type === "FS") {
      const root = await navigator.storage.getDirectory();
      const fileHandle = await root.getFileHandle(testId, { create: false });
      const file = await fileHandle.getFile();
      actual = await file.text();
    } else {
      throw Error("Unknown data type " + e.data.type);
    }

    if (actual.toString() === e.data.expected.toString()) {
      messageSink.postMessage({ id: testId, message: testId }, messageTarget);
    } else {
      const description = "Found " + actual + ", expected " + e.data.expected;
      messageSink.postMessage(
        { id: testId, message: description },
        messageTarget
      );
    }
  } catch (err) {
    messageSink.postMessage(
      { id: testId, message: err.message },
      messageTarget
    );
  }
});
