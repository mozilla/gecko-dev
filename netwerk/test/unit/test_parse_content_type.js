/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var charset = {};
var hadCharset = {};
var type;

function reset() {
  delete charset.value;
  delete hadCharset.value;
  type = undefined;
}

function check(aType, aCharset, aHadCharset) {
  do_check_eq(type, aType);
  do_check_eq(aCharset, charset.value);
  do_check_eq(aHadCharset, hadCharset.value);
  reset();
}

function run_test() {
  var netutil = Components.classes["@mozilla.org/network/util;1"]
                          .getService(Components.interfaces.nsINetUtil);

  type = netutil.parseContentType("text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType("TEXT/HTML", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType("text/html, text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType("text/html, text/plain",
				  charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/html, ', charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType('text/html, */*', charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType('text/html, foo', charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType("text/html; charset=ISO-8859-1",
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType('text/html; charset="ISO-8859-1"',
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType("text/html; charset='ISO-8859-1'",
				  charset, hadCharset);
  check("text/html", "'ISO-8859-1'", true);

  type = netutil.parseContentType("text/html; charset=\"ISO-8859-1\", text/html",
				  charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType("text/html; charset=\"ISO-8859-1\", text/html; charset=UTF8",
				  charset, hadCharset);
  check("text/html", "UTF8", true);

  type = netutil.parseContentType("text/html; charset=ISO-8859-1, TEXT/HTML", charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType("text/html; charset=ISO-8859-1, TEXT/plain", charset, hadCharset);
  check("text/plain", "", true);

  type = netutil.parseContentType("text/plain, TEXT/HTML; charset=ISO-8859-1, text/html, TEXT/HTML", charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType('text/plain, TEXT/HTML; param="charset=UTF8"; charset="ISO-8859-1"; param2="charset=UTF16", text/html, TEXT/HTML', charset, hadCharset);
  check("text/html", "ISO-8859-1", true);

  type = netutil.parseContentType('text/plain, TEXT/HTML; param=charset=UTF8; charset="ISO-8859-1"; param2=charset=UTF16, text/html, TEXT/HTML', charset, hadCharset);
  check("text/html", "ISO-8859-1", true);  

  type = netutil.parseContentType("text/plain; param= , text/html", charset, hadCharset);
  check("text/html", "", false);

  type = netutil.parseContentType('text/plain; param=", text/html"', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/plain; param=", \\" , text/html"', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/plain; param=", \\" , text/html , "', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/plain param=", \\" , text/html , "', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/plain charset=UTF8', charset, hadCharset);
  check("text/plain", "", false);

  type = netutil.parseContentType('text/plain, TEXT/HTML; param="charset=UTF8"; ; param2="charset=UTF16", text/html, TEXT/HTML', charset, hadCharset);
  check("text/html", "", false);

  // Bug 562915 - correctness: "\x" is "x"
  type = netutil.parseContentType('text/plain; charset="UTF\\-8"', charset, hadCharset);
  check("text/plain", "UTF-8", true);

  // Bug 700589

  // check that single quote doesn't confuse parsing of subsequent parameters
  type = netutil.parseContentType("text/plain; x='; charset=\"UTF-8\"", charset, hadCharset);
  check("text/plain", "UTF-8", true);

  // check that single quotes do not get removed from extracted charset
  type = netutil.parseContentType("text/plain; charset='UTF-8'", charset, hadCharset);
  check("text/plain", "'UTF-8'", true);
}
