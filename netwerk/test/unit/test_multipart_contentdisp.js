"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpserver = null;

ChromeUtils.defineLazyGetter(this, "uri", function () {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

function make_channel(url) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

var multipartBody =
  "--boundary\r\nContent-Type: text/html\r\nContent-Disposition: inline\r\n\r\n<script>alert(document.domain)</script>\r\n--boundary--";

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", 'multipart/mixed; boundary="boundary"');
  response.bodyOutputStream.write(multipartBody, multipartBody.length);
}

function contentHandler2(metadata, response) {
  response.setHeader("Content-Type", 'multipart/mixed; boundary="boundary"');
  response.setHeader("Content-Disposition", "attachment");

  response.bodyOutputStream.write(multipartBody, multipartBody.length);
}

class MultipartListener {
  QueryInterface = ChromeUtils.generateQI([
    "nsIStreamListener",
    "nsIRequestObserver",
    "nsIMultiPartChannelListener",
  ]);

  constructor(callback) {
    this.entries = [];
    this.entry = null;
    this.callback = callback;
  }

  onStartRequest(request) {
    this.entry = {
      request,
      data: "",
    };
  }

  onDataAvailable(request, stream, offset, count) {
    try {
      this.entry.data = this.entry.data.concat(read_stream(stream, count));
      dump("BUFFEEE: " + this.entry.data + "\n\n");
    } catch (ex) {
      do_throw("Error in onDataAvailable: " + ex);
    }
  }

  onStopRequest(_request) {
    this.entries.push(this.entry);
    this.entry = null;
  }

  onAfterLastPart(status) {
    this.callback(this.entries, status);
  }
}

add_setup(async () => {
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/multipart", contentHandler);
  httpserver.registerPathHandler("/multipart2", contentHandler2);
  httpserver.start(-1);
});

add_task(async function test_contentDisp() {
  var streamConv = Cc["@mozilla.org/streamConverters;1"].getService(
    Ci.nsIStreamConverterService
  );
  let req = await new Promise(resolve => {
    let multipartListener = new MultipartListener(resolve);
    var conv = streamConv.asyncConvertData(
      "multipart/mixed",
      "*/*",
      multipartListener,
      null
    );
    var chan = make_channel(`${uri}/multipart`);
    chan.asyncOpen(conv, null);
  });
  Assert.ok(req.length);
  req[0].request.QueryInterface(Ci.nsIChannel);
  Assert.equal(req[0].request.contentType, "text/html");
  Assert.equal(
    req[0].request.contentDisposition,
    Ci.nsIChannel.DISPOSITION_INLINE
  );
});

add_task(async function test_contentDisp() {
  var streamConv = Cc["@mozilla.org/streamConverters;1"].getService(
    Ci.nsIStreamConverterService
  );
  let req = await new Promise(resolve => {
    let multipartListener = new MultipartListener(resolve);
    var conv = streamConv.asyncConvertData(
      "multipart/mixed",
      "*/*",
      multipartListener,
      null
    );
    var chan = make_channel(`${uri}/multipart2`);
    chan.asyncOpen(conv, null);
  });
  Assert.ok(req.length);
  req[0].request.QueryInterface(Ci.nsIChannel);
  Assert.equal(req[0].request.contentType, "text/html");
  Assert.equal(
    req[0].request.contentDisposition,
    Ci.nsIChannel.DISPOSITION_ATTACHMENT
  );
});
