/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that projecteditor can be destroyed in various states of loading
// without causing any leaks or exceptions.

let test = asyncTest(function* () {

  info ("Testing tab closure when projecteditor is in various states");

  yield addTab("chrome://browser/content/devtools/projecteditor-test.html").then(() => {
    let iframe = content.document.getElementById("projecteditor-iframe");
    ok (iframe, "Tab has placeholder iframe for projecteditor");

    info ("Closing the tab without doing anything");
    gBrowser.removeCurrentTab();
  });

  yield addTab("chrome://browser/content/devtools/projecteditor-test.html").then(() => {
    let iframe = content.document.getElementById("projecteditor-iframe");
    ok (iframe, "Tab has placeholder iframe for projecteditor");

    let projecteditor = ProjectEditor.ProjectEditor();
    ok (projecteditor, "ProjectEditor has been initialized");

    info ("Closing the tab before attempting to load");
    gBrowser.removeCurrentTab();
  });

  yield addTab("chrome://browser/content/devtools/projecteditor-test.html").then(() => {
    let iframe = content.document.getElementById("projecteditor-iframe");
    ok (iframe, "Tab has placeholder iframe for projecteditor");

    let projecteditor = ProjectEditor.ProjectEditor();
    ok (projecteditor, "ProjectEditor has been initialized");

    projecteditor.load(iframe);

    info ("Closing the tab after a load is requested, but before load is finished");
    gBrowser.removeCurrentTab();
  });

  yield addTab("chrome://browser/content/devtools/projecteditor-test.html").then(() => {
    let iframe = content.document.getElementById("projecteditor-iframe");
    ok (iframe, "Tab has placeholder iframe for projecteditor");

    let projecteditor = ProjectEditor.ProjectEditor();
    ok (projecteditor, "ProjectEditor has been initialized");

    return projecteditor.load(iframe).then(() => {
      info ("Closing the tab after a load has been requested and finished");
      gBrowser.removeCurrentTab();
    });
  });

  finish();
});


