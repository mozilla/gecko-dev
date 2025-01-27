/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

document.querySelectorAll(".broken-embed-content").forEach(embedContainer => {
  // create the "real" embed content
  let contentDiv = document.createElement("div");
  contentDiv.classList.add("loaded-embed-content");

  let contentContent = document.createTextNode("This is the loaded embed");
  contentDiv.appendChild(contentContent);

  // replace the embed code with the "real" embed
  embedContainer.replaceWith(contentDiv);
});

const finishedEvent = new Event("testEmbedScriptFinished", {
  bubbles: true,
});
window.dispatchEvent(finishedEvent);
