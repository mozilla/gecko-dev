// Wait for a blob URL to be posted to this worker.
// Obtain the blob, and read the string contained in it.
// Post back the string.

var postStringInBlob = function (blobObject) {
  var fileReader = new FileReaderSync();
  var result = fileReader.readAsText(blobObject);
  postMessage(result);
};

self.addEventListener("message", function (e) {
  if ("error" in e.data) {
    postMessage(e.data);
    return;
  }
  var blobURL = e.data.blobURL,
      xhr = new XMLHttpRequest();
  try {
    xhr.open("GET", blobURL, true);
    xhr.onload = function () {
      postStringInBlob(xhr.response);
    };
    xhr.onerror = function () {
      postMessage({ error: "xhr error" });
    };
    xhr.responseType = "blob";
    xhr.send();
  } catch (e) {
    postMessage({ error: e.message });
  }
}, false);
