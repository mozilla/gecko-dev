/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain", false);

  // Echo back cookies and scheme
  let scheme = request.scheme;
  let cookies = request.hasHeader("Cookie") ? request.getHeader("Cookie") : "";
  response.write(`scheme=${scheme};cookies=${cookies}`);
}
