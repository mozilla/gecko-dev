/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const { require } = devtools;
const Editor  = require("devtools/sourceeditor/editor");

function setup(cb) {
  const opt = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";
  const url = "data:text/xml;charset=UTF-8,<?xml version='1.0'?>" +
    "<?xml-stylesheet href='chrome://global/skin/global.css'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='Editor' width='600' height='500'><box flex='1'/></window>";

  let win = Services.ww.openWindow(null, url, "_blank", opt, null);

  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);

    waitForFocus(function () {
      let box = win.document.querySelector("box");
      let editor = new Editor({
        value: "Hello.",
        lineNumbers: true,
        foldGutter: true,
        gutters: [ "CodeMirror-linenumbers", "breakpoints", "CodeMirror-foldgutter" ]
      });

      editor.appendTo(box)
        .then(() => cb(editor, win))
        .then(null, (err) => ok(false, err.message));
    }, win);
  }, false);
}

function ch(exp, act, label) {
  is(exp.line, act.line, label + " (line)");
  is(exp.ch, act.ch, label + " (ch)");
}

function teardown(ed, win) {
  ed.destroy();
  win.close();
  finish();
}

/**
 * This method returns the portion of the input string `source` up to the
 * [line, ch] location.
 */
function limit(source, [line, ch]) {
  line++;
  let list = source.split("\n");
  if (list.length < line)
    return source;
  if (line == 1)
    return list[0].slice(0, ch);
  return [...list.slice(0, line - 1), list[line - 1].slice(0, ch)].join("\n");
}

function read(url) {
  let scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"]
    .getService(Ci.nsIScriptableInputStream);

  let channel = Services.io.newChannel(url, null, null);
  let input = channel.open();
  scriptableStream.init(input);

  let data = scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  return data;
}
