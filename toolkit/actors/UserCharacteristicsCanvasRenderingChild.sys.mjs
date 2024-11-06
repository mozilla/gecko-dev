/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "UserCharacteristicsPage",
    maxLogLevelPref: "toolkit.telemetry.user_characteristics_ping.logLevel",
  });
});

/* This actor is responsible for rendering the canvas elements defined in
 * recipes. It renders with both hardware and software rendering.
 * It also provides debug information about the canvas rendering
 * capabilities of its window (not all windows get HW rendering).
 *
 * See the recipes object for the list of canvases to render.
 * WebGL is still being rendered in toolkit/components/resistfingerprinting/content/usercharacteristics.js
 *
 */
export class UserCharacteristicsCanvasRenderingChild extends JSWindowActorChild {
  constructor() {
    super();

    this.destroyed = false;
  }

  async render() {
    // I couldn't think of a good name. Recipes as in instructions to render.
    const runRecipe = async (isAccelerated, recipe) => {
      const canvas = this.document.createElement("canvas");
      canvas.width = recipe.size[0];
      canvas.height = recipe.size[1];

      const ctx = canvas.getContext("2d", {
        forceSoftwareRendering: !isAccelerated,
      });

      if (!ctx) {
        return null;
      }

      let debugInfo = null;
      try {
        debugInfo = ctx.getDebugInfo(true /* ensureTarget */);
      } catch (e) {
        lazy.console.error(
          "Error getting canvas debug info during render: ",
          await stringifyError(e)
        );
        return null;
      }

      if (debugInfo.isAccelerated !== isAccelerated) {
        lazy.console.error(
          `Canvas is not rendered with expected mode. Expected: ${isAccelerated}, got: ${debugInfo.isAccelerated}`
        );
        return null;
      }

      try {
        await recipe.func(this.contentWindow, canvas, ctx);
      } catch (e) {
        lazy.console.error("Error rendering canvas: ", await stringifyError(e));
        return null;
      }

      return sha1(canvas.toDataURL("image/png", 1));
    };

    const data = {};

    // Run HW renderings
    for (const [name, recipe] of Object.entries(recipes)) {
      lazy.console.debug("[HW] Rendering ", name);
      const result = await runRecipe(true, recipe);
      if (!result) {
        continue;
      }

      data[name] = result;
    }

    // Run SW renderings
    for (const [name, recipe] of Object.entries(recipes)) {
      lazy.console.debug("[SW] Rendering ", name);
      const result = await runRecipe(false, recipe);
      if (!result) {
        continue;
      }

      data[name + "software"] = result;
    }

    this.sendMessage("CanvasRendering:Rendered", data);
  }

  async getDebugInfo() {
    const canvas = this.document.createElement("canvas");
    const ctx = canvas.getContext("2d");

    if (!ctx) {
      return;
    }

    let debugInfo = null;
    try {
      debugInfo = ctx.getDebugInfo(true /* ensureTarget */);
    } catch (e) {
      lazy.console.error(
        "Error getting canvas debug info: ",
        await stringifyError(e)
      );
      return;
    }

    this.sendMessage("CanvasRendering:GotDebugInfo", debugInfo);
  }

  sendMessage(name, obj, transferables) {
    if (this.destroyed) {
      return;
    }

    this.sendAsyncMessage(name, obj, transferables);
  }

  didDestroy() {
    this.destroyed = true;
  }

  async receiveMessage(msg) {
    lazy.console.debug("Actor Child: Got ", msg.name);
    switch (msg.name) {
      case "CanvasRendering:GetDebugInfo":
        this.getDebugInfo();
        break;
      case "CanvasRendering:Render":
        this.render();
        break;
    }

    return null;
  }
}

const recipes = {
  // Metric name => (optionally async) function to render
};

async function sha1(message) {
  const msgUint8 = new TextEncoder().encode(message);
  const hashBuffer = await crypto.subtle.digest("SHA-1", msgUint8);
  const hashArray = Array.from(new Uint8Array(hashBuffer));
  const hashHex = hashArray.map(b => b.toString(16).padStart(2, "0")).join("");
  return hashHex;
}

async function stringifyError(error) {
  if (error instanceof Error) {
    const stack = error.stack ?? "";
    return `${error.toString()} ${stack}`;
  }
  // A hacky attempt to extract as much as info from error
  const errStr = await (async () => {
    const asStr = await (async () => error.toString())().catch(() => "");
    const asJson = await (async () => JSON.stringify(error))().catch(() => "");
    return asStr.length > asJson.len ? asStr : asJson;
  })();
  return errStr;
}
