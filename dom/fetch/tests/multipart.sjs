/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function handleRequest(request, response) {
  response.seizePower();

  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Access-Control-Allow-Origin: *\r\n");
  // See bug 1752761. The extra "\r\n" was the reason why FormDataParser
  // could not parse this correctly.
  response.write(
    "Content-type: multipart/form-data; boundary=boundary\r\n\r\n"
  );
  response.write("\r\n");
  response.write("--boundary\r\n");
  response.write(
    'Content-Disposition: form-data; name="file1"; filename="file1.txt"\r\n'
  );
  response.write("Content-Type: text/plain\r\n\r\n");
  response.write("Content of file1\r\n");
  response.write("--boundary\r\n");
  response.write(
    'Content-Disposition: form-data; name="file2"; filename="file2.txt"\r\n'
  );
  response.write("Content-Type: text/plain\r\n\r\n");
  response.write("Content of file2\r\n");
  response.write("--boundary--\r\n");
  response.write("");
  response.finish();
}
