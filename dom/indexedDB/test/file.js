/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from helpers.js */

var bufferCache = [];
var utils = SpecialPowers.getDOMWindowUtils(window);

function getBuffer(size) {
  let buffer = new ArrayBuffer(size);
  is(buffer.byteLength, size, "Correct byte length");
  return buffer;
}

function getRandomBuffer(size) {
  let buffer = getBuffer(size);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < size; i++) {
    view[i] = parseInt(Math.random() * 255);
  }
  return buffer;
}

function getView(size) {
  let buffer = new ArrayBuffer(size);
  let view = new Uint8Array(buffer);
  is(buffer.byteLength, size, "Correct byte length");
  return view;
}

function getRandomView(size) {
  let view = getView(size);
  for (let i = 0; i < size; i++) {
    view[i] = parseInt(Math.random() * 255);
  }
  return view;
}

function compareBuffers(buffer1, buffer2) {
  if (buffer1.byteLength != buffer2.byteLength) {
    return false;
  }
  let view1 = buffer1 instanceof Uint8Array ? buffer1 : new Uint8Array(buffer1);
  let view2 = buffer2 instanceof Uint8Array ? buffer2 : new Uint8Array(buffer2);
  for (let i = 0; i < buffer1.byteLength; i++) {
    if (view1[i] != view2[i]) {
      return false;
    }
  }
  return true;
}

function getBlob(type, view) {
  return new Blob([view], { type });
}

function getFile(name, type, view) {
  return new File([view], name, { type });
}

function getRandomBlob(size) {
  return getBlob("binary/random", getRandomView(size));
}

function getRandomFile(name, size) {
  return getFile(name, "binary/random", getRandomView(size));
}

function getNullBlob(size) {
  return getBlob("binary/null", getView(size));
}

function getNullFile(name, size) {
  return getFile(name, "binary/null", getView(size));
}

function getWasmModule(binary) {
  let module = new WebAssembly.Module(binary);
  return module;
}

function verifyBuffers(buffer1, buffer2) {
  ok(compareBuffers(buffer1, buffer2), "Correct buffer data");
}

function verifyBlobProperties(blob1, blob2, fileId) {
  // eslint-disable-next-line mozilla/use-cc-etc
  is(SpecialPowers.wrap(Blob).isInstance(blob1), true, "Instance of Blob");
  is(blob1 instanceof File, blob2 instanceof File, "Instance of DOM File");
  is(blob1.size, blob2.size, "Correct size");
  is(blob1.type, blob2.type, "Correct type");
  if (blob2 instanceof File) {
    is(blob1.name, blob2.name, "Correct name");
  }
  is(utils.getFileId(blob1), fileId, "Correct file id");
}

/**
 * verifyBlobAsync checks that blob1 has the same size, type and buffer as
 * blob2, and the given fileId. Additionally, if blob2 is a File, the names
 * of two blobs must be equal.
 *
 * Note: Unlike the generator based verifyBlob routine, verifyBlobAsync uses
 * bufferCache for both blob1 and blob2 arguments.
 * @param {Blob} blob1 actual Blob value
 * @param {Blob} blob2 Blob with expected properties
 * @param {Number} fileId expected id
 */
async function verifyBlobAsync(blob1, blob2, fileId) {
  verifyBlobProperties(blob1, blob2, fileId);

  let buffer1;
  let buffer2;

  for (let i = 0; i < bufferCache.length; i++) {
    if (!buffer1 && bufferCache[i].blob == blob1) {
      buffer1 = bufferCache[i].buffer;
    }

    if (!buffer2 && bufferCache[i].blob == blob2) {
      buffer2 = bufferCache[i].buffer;
    }

    if (!!buffer1 && !!buffer2) {
      break;
    }
  }

  const getBuffer = blobItem => {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.addEventListener("loadend", () => {
        if (reader.error) {
          reject(reader.error);
        }

        resolve(reader.result);
      });
      reader.readAsArrayBuffer(blobItem);
    });
  };

  if (!buffer1) {
    buffer1 = await getBuffer(blob1);
    bufferCache.push({ blob: blob1, buffer: buffer1 });
  }

  if (!buffer2) {
    buffer2 = await getBuffer(blob2);
    bufferCache.push({ blob: blob2, buffer: buffer2 });
  }

  verifyBuffers(buffer1, buffer2);
}

function verifyBlob(blob1, blob2, fileId, blobReadHandler) {
  verifyBlobProperties(blob1, blob2, fileId);

  let buffer1;
  let buffer2;

  for (let i = 0; i < bufferCache.length; i++) {
    if (bufferCache[i].blob == blob2) {
      buffer2 = bufferCache[i].buffer;
      break;
    }
  }

  if (!buffer2) {
    let reader = new FileReader();
    reader.readAsArrayBuffer(blob2);
    reader.onload = function (event) {
      buffer2 = event.target.result;
      bufferCache.push({ blob: blob2, buffer: buffer2 });
      if (buffer1) {
        verifyBuffers(buffer1, buffer2);
        if (blobReadHandler) {
          blobReadHandler();
        } else {
          testGenerator.next();
        }
      }
    };
  }

  let reader = new FileReader();
  reader.readAsArrayBuffer(blob1);
  reader.onload = function (event) {
    buffer1 = event.target.result;
    if (buffer2) {
      verifyBuffers(buffer1, buffer2);
      if (blobReadHandler) {
        blobReadHandler();
      } else {
        testGenerator.next();
      }
    }
  };
}

function verifyBlobArray(blobs1, blobs2, expectedFileIds) {
  is(blobs1 instanceof Array, true, "Got an array object");
  is(blobs1.length, blobs2.length, "Correct length");

  if (!blobs1.length) {
    return;
  }

  let verifiedCount = 0;

  function blobReadHandler() {
    if (++verifiedCount == blobs1.length) {
      testGenerator.next();
    } else {
      verifyBlob(
        blobs1[verifiedCount],
        blobs2[verifiedCount],
        expectedFileIds[verifiedCount],
        blobReadHandler
      );
    }
  }

  verifyBlob(
    blobs1[verifiedCount],
    blobs2[verifiedCount],
    expectedFileIds[verifiedCount],
    blobReadHandler
  );
}

function verifyView(view1, view2) {
  is(view1.byteLength, view2.byteLength, "Correct byteLength");
  verifyBuffers(view1, view2);
  continueToNextStep();
}

function verifyWasmModule(module1, module2) {
  // We assume the given modules have no imports and export a single function
  // named 'run'.
  var instance1 = new WebAssembly.Instance(module1);
  var instance2 = new WebAssembly.Instance(module2);
  is(instance1.exports.run(), instance2.exports.run(), "same run() result");

  continueToNextStep();
}

function grabFileUsageAndContinueHandler(request) {
  testGenerator.next(request.result.fileUsage);
}

function getCurrentUsage(usageHandler) {
  let qms = SpecialPowers.Services.qms;
  let principal = SpecialPowers.wrap(document).nodePrincipal;
  let cb = SpecialPowers.wrapCallback(usageHandler);
  qms.getUsageForPrincipal(principal, cb);
}

function getFileId(file) {
  return utils.getFileId(file);
}

function getFilePath(file) {
  return utils.getFilePath(file);
}

function* assertEventuallyHasFileInfo(name, id) {
  yield* assertEventuallyWithGC(
    () => utils.getFileReferences(name, id),
    `Expect existing DatabaseFileInfo for ${name}/${id}`
  );
}

function* assertEventuallyHasNoFileInfo(name, id) {
  yield* assertEventuallyWithGC(
    () => !utils.getFileReferences(name, id),
    `Expect no existing DatabaseFileInfo for ${name}/${id}`
  );
}

function* assertEventuallyFileRefCount(name, id, expectedCount) {
  yield* assertEventuallyWithGC(() => {
    let count = {};
    utils.getFileReferences(name, id, count);
    return count.value == expectedCount;
  }, `Expect ${expectedCount} existing references for ${name}/${id}`);
}

function getFileDBRefCount(name, id) {
  let count = {};
  utils.getFileReferences(name, id, {}, count);
  return count.value;
}

function flushPendingFileDeletions() {
  utils.flushPendingFileDeletions();
}
