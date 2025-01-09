/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = URL_ROOT_COM_SSL + "doc_broken_xml_frame.html";

add_task(async function testOpenToolboxOnBrokenXMLFrame() {
  await addTab(TEST_URI);
  const tab = gBrowser.selectedTab;

  info("Open the toolbox on page with an iframe containing a broken XML doc");
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "webconsole",
  });

  info("Check that the console opened and has the message from the page");
  const { hud } = toolbox.getPanel("webconsole");
  await waitFor(() =>
    Array.from(hud.ui.window.document.querySelectorAll(".message-body")).some(
      el => el.innerText.includes("foo after iframe")
    )
  );
  ok(true, "The console opened with the expected content");
});
