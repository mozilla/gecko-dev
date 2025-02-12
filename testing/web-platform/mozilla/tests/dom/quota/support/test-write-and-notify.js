var testId;

const getNonZeroRandomValue = () => {
  return 1 + Math.floor(Math.random() * (Number.MAX_SAFE_INTEGER - 1));
};

var messageSource;
var messageSink;
var messageTarget;
try {
  messageSource = window;
  messageSink = window.parent;
  messageTarget = document.referrer;
} catch (_) {
  messageSource = self;
  messageSink = self;
  messageTarget = null;
}

messageSource.addEventListener("message", e => {
  try {
    console.log("Write script received " + JSON.stringify(e.data));
    testId = e.data.message;
    const storeName = "testObjectStore" + testId;
    const storeProps = { autoIncrement: false };

    const actual = getNonZeroRandomValue();

    if (e.data.type === "IDB") {
      new Promise((openResolve, openReject) => {
        const openReq = indexedDB.open(testId, 1);
        openReq.onerror = () => {
          openReject(openReq.error);
        };
        openReq.onupgradeneeded = ev => {
          ev.target.result.createObjectStore(storeName, storeProps);
        };
        openReq.onsuccess = ev => {
          openResolve(ev.target.result);
        };
      })
        .then(db => {
          new Promise((addResolve, addReject) => {
            const blob = new Blob([actual], {});
            const tx = db.transaction([storeName], "readwrite");
            const objectStore = tx.objectStore(storeName);
            const addReq = objectStore.add(blob, "actual");
            addReq.onerror = () => {
              addReject(addReq.error);
            };
            addReq.onsuccess = ev => {
              addResolve(ev);
            };
          })
            .then(() => {
              const doneMessage = {
                id: testId,
                message: "write done",
                expected: actual,
              };
              console.log("Sending parent " + JSON.stringify(doneMessage));
              messageSink.postMessage(doneMessage, messageTarget);
            })
            .catch(err => {
              console.log("Encountered error " + err.message);
              messageSink.postMessage(
                { id: testId, message: err.message },
                messageTarget
              );
            });
        })
        .catch(err => {
          console.log("Sending parent error " + err.message);
          messageSink.postMessage(
            { id: testId, message: err.message },
            messageTarget
          );
        });
    } else if (e.data.type === "FS") {
      navigator.storage
        .getDirectory()
        .then(root => {
          root.getFileHandle(testId, { create: true }).then(file => {
            file.createWritable({}).then(writable => {
              writable.write(actual).then(() => {
                writable.close().then(() => {
                  const doneMessage = {
                    id: testId,
                    message: "write done",
                    expected: actual,
                  };
                  console.log("Sending parent " + JSON.stringify(doneMessage));
                  messageSink.postMessage(doneMessage, messageTarget);
                });
              });
            });
          });
        })
        .catch(err => {
          console.log("Sending parent error " + err.message);
          messageSink.postMessage(
            { id: testId, message: err.message },
            messageTarget
          );
        });
    } else {
      throw Error("Unknown data type " + e.data.type);
    }
  } catch (err) {
    console.log("Caught " + err.message);
    messageSink.postMessage(
      { id: testId, message: err.message },
      messageTarget
    );
  }
});
