/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html,<!DOCTYPE html>
  <main>
    <div id="1" class="x">1</div>
    <div id="2" class="x">
      <template shadowrootmode="open">
        2
        <div id="2-1" class="x">2-1</div>
        <div id="2-2" class="x">2-2</div>
        <section>
          <div id="2-3" class="x">2-3</div>
        </section>
        <div id="2-4" class="x">
          <template shadowrootmode="open">
            2-4
            <div id="2-4-1" class="x y">2-4-1</div>
          </template>
        </div>
      </template>
    </div>
    <div id="3">3</div>
  </main>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  // Place the mouse on the top left corner to avoid triggering an highlighter request
  // to the server. See Bug 1535082.
  EventUtils.synthesizeMouse(
    hud.ui.outputNode,
    0,
    0,
    { type: "mousemove" },
    hud.iframeWindow
  );

  let message = await executeAndWaitForResultMessage(
    hud,
    "$$$('.x')",
    "Array(7) [ div#1.x, div#2.x, div#2-1.x, div#2-2.x, div#2-3.x, div#2-4.x, div#2-4-1.x.y ]"
  );
  ok(message, "`$$$('.x')` worked");

  message = await executeAndWaitForResultMessage(
    hud,
    "$$$('.y').at(0).id",
    "2-4-1"
  );
  ok(message, "`$$$` result can be used right away");

  message = await executeAndWaitForResultMessage(
    hud,
    "$$$('main .x')",
    "Array [ div#1.x, div#2.x ]"
  );
  ok(
    message,
    "`$$$` doesn't handle descendant selectors across the shadow dom boundaries"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    "$$$('header')",
    "Array []"
  );
  ok(message, "`$$$('header')` returns an empty array");

  message = await executeAndWaitForErrorMessage(
    hud,
    "$$$(':foo')",
    "':foo' is not a valid selector"
  );
  ok(message, "`$$$(':foo')` returns an error message");

  message = await executeAndWaitForResultMessage(
    hud,
    "$$$('.x', document.getElementById('2'))",
    "Array(5) [ div#2-1.x, div#2-2.x, div#2-3.x, div#2-4.x, div#2-4-1.x.y ]"
  );
  ok(message, "`$$$('.x', document.getElementById('2'))` worked");

  message = await executeAndWaitForErrorMessage(
    hud,
    "$$$('li', $(':foo'))",
    "':foo' is not a valid selector"
  );
  ok(message, "`$$$('li', $(':foo'))` returns an error message");

  message = await executeAndWaitForResultMessage(
    hud,
    `$$$('.x', $('[id="2"]'))`,
    "Array(5) [ div#2-1.x, div#2-2.x, div#2-3.x, div#2-4.x, div#2-4-1.x.y ]"
  );
  ok(message, "`$$$('.x', $('[id=\"2\"]'))` worked");

  message = await executeAndWaitForResultMessage(
    hud,
    `$$$('.x', document.getElementById("2").shadowRoot.querySelector("section"))`,
    "Array [ div#2-3.x ]"
  );
  ok(message, "works when passed a scope inside the shadow DOM");
});
