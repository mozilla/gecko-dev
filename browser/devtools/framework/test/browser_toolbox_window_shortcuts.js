/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let Toolbox = devtools.Toolbox;

let toolbox, toolIDs, idIndex, modifiedPrefs = [];

function test() {
  addTab("about:blank").then(function() {
    toolIDs = [];
    for (let [id, definition] of gDevTools._tools) {
      if (definition.key) {
        toolIDs.push(id);

        // Enable disabled tools
        let pref = definition.visibilityswitch, prefValue;
        try {
          prefValue = Services.prefs.getBoolPref(pref);
        } catch (e) {
          continue;
        }
        if (!prefValue) {
          modifiedPrefs.push(pref);
          Services.prefs.setBoolPref(pref, true);
        }
      }
    }
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    idIndex = 0;
    gDevTools.showToolbox(target, toolIDs[0], Toolbox.HostType.WINDOW)
             .then(testShortcuts);
  });
}

function testShortcuts(aToolbox, aIndex) {
  if (aIndex === undefined) {
    aIndex = 1;
  } else if (aIndex == toolIDs.length) {
    tidyUp();
    return;
  }

  toolbox = aToolbox;
  info("Toolbox fired a `ready` event");

  // FIXME Bug 1175850 - Enable storage inspector tests after upgrading for E10S
  if (toolIDs[aIndex] === "storage") {
    testShortcuts(toolbox, aIndex + 1);
    return;
  }

  toolbox.once("select", selectCB);

  let key = gDevTools._tools.get(toolIDs[aIndex]).key;
  let toolModifiers = gDevTools._tools.get(toolIDs[aIndex]).modifiers;
  let modifiers = {
    accelKey: toolModifiers.includes("accel"),
    altKey: toolModifiers.includes("alt"),
    shiftKey: toolModifiers.includes("shift"),
  };
  idIndex = aIndex;
  info("Testing shortcut for tool " + aIndex + ":" + toolIDs[aIndex] +
       " using key " + key);
  EventUtils.synthesizeKey(key, modifiers, toolbox.doc.defaultView.parent);
}

function selectCB(event, id) {
  info("toolbox-select event from " + id);

  is(toolIDs.indexOf(id), idIndex,
     "Correct tool is selected on pressing the shortcut for " + id);

  testShortcuts(toolbox, idIndex + 1);
}

function tidyUp() {
  toolbox.destroy().then(function() {
    gBrowser.removeCurrentTab();

    for (let pref of modifiedPrefs) {
      Services.prefs.clearUserPref(pref);
    }
    toolbox = toolIDs = idIndex = modifiedPrefs = Toolbox = null;
    finish();
  });
}
