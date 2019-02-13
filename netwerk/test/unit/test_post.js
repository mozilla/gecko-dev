//
// POST test
//

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/simple";

var testfile = do_get_file("../unit/data/test_readline6.txt");

const BOUNDARY = "AaB03x";
var teststring1 = "--" + BOUNDARY + "\r\n"
                + "Content-Disposition: form-data; name=\"body\"\r\n\r\n"
                + "0123456789\r\n"
                + "--" + BOUNDARY + "\r\n"
                + "Content-Disposition: form-data; name=\"files\"; filename=\"" + testfile.leafName + "\"\r\n"
                + "Content-Type: application/octet-stream\r\n"
                + "Content-Length: " + testfile.fileSize + "\r\n\r\n";
var teststring2 = "--" + BOUNDARY + "--\r\n";

const BUFFERSIZE = 4096;

function run_test() {
  var sstream1 = Cc["@mozilla.org/io/string-input-stream;1"].
                   createInstance(Ci.nsIStringInputStream);
  sstream1.data = teststring1;

  var fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                  createInstance(Ci.nsIFileInputStream);
  fstream.init(testfile, -1, -1, 0);

  var buffered = Cc["@mozilla.org/network/buffered-input-stream;1"].
                   createInstance(Ci.nsIBufferedInputStream);
  buffered.init(fstream, BUFFERSIZE);

  var sstream2 = Cc["@mozilla.org/io/string-input-stream;1"].
                   createInstance(Ci.nsIStringInputStream);
  sstream2.data = teststring2;

  var multi = Cc["@mozilla.org/io/multiplex-input-stream;1"].
                createInstance(Ci.nsIMultiplexInputStream);
  multi.appendStream(sstream1);
  multi.appendStream(buffered);
  multi.appendStream(sstream2);

  var mime = Cc["@mozilla.org/network/mime-input-stream;1"].
               createInstance(Ci.nsIMIMEInputStream);
  mime.addHeader("Content-Type", "multipart/form-data; boundary="+BOUNDARY);
  mime.setData(multi);
  mime.addContentLength = true;

  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);

  var channel = setupChannel(testpath);

  channel.QueryInterface(Ci.nsIUploadChannel)
         .setUploadStream(mime, "", mime.available());
  channel.requestMethod = "POST";

  channel.asyncOpen(new ChannelListener(checkRequest, channel), null);

  do_test_pending();
}

function setupChannel(path) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  return chan = ios.newChannel2(URL + path,
                               "",
                               null,
                               null,      // aLoadingNode
                               Services.scriptSecurityManager.getSystemPrincipal(),
                               null,      // aTriggeringPrincipal
                               Ci.nsILoadInfo.SEC_NORMAL,
                               Ci.nsIContentPolicy.TYPE_OTHER)
                   .QueryInterface(Ci.nsIHttpChannel);
}

function serverHandler(metadata, response) {
  do_check_eq(metadata.method, "POST");

  var data = read_stream(metadata.bodyInputStream,
			 metadata.bodyInputStream.available());

  var testfile_stream = Cc["@mozilla.org/network/file-input-stream;1"].
                   createInstance(Ci.nsIFileInputStream);
  testfile_stream.init(testfile, -1, -1, 0);

  do_check_eq(teststring1 +
	      read_stream(testfile_stream, testfile_stream.available()) +
	      teststring2,
	      data);
}

function checkRequest(request, data, context) {
  httpserver.stop(do_test_finished);
}
