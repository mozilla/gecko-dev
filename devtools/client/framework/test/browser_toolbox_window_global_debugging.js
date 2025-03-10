/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Note: adding Date.now to the function name to generate a unique test URL to
// allow running the test with --repeat / --verify.
const TEST_URL = `data:text/html,<script>window.someInlineSource${Date.now()} = () => {}</script>`;

add_task(async function () {
  // Ensure debugging the content processes
  await pushPref("devtools.browsertoolbox.scope", "everything");

  // This is the one test involving enableWindowGlobalThreadActors option.
  // It instructs the Watcher Front to allow debugging sources related to Window Global targets.
  //
  // This codepath isn't used by DevTools, but by VS Code to debug all tabs via their
  // Window Global targets, that, without involving the Content Process targets.
  const commands = await CommandsFactory.forMainProcess({
    enableWindowGlobalThreadActors: true,
  });
  await commands.targetCommand.startListening();

  await addTab(TEST_URL);

  const sources = [];
  await commands.resourceCommand.watchResources(
    [commands.resourceCommand.TYPES.SOURCE],
    {
      onAvailable(resources) {
        sources.push(...resources);
      },
    }
  );

  const sourceForTab = sources
    .filter(s => s.url == TEST_URL)
    .map(s => s.targetFront.targetType);
  is(
    sourceForTab.length,
    2,
    "We should get two source matching the tab url. One for the content process target and another copy for the window global target"
  );
  ok(
    sourceForTab.includes("process"),
    "We got a source from the content process target"
  );
  ok(
    sourceForTab.includes("frame"),
    "We got a source from the window global target"
  );

  await commands.destroy();
});
