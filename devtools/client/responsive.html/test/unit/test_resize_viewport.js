/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test resizing the viewport.

const { addViewport, resizeViewport } =
  require("devtools/client/responsive.html/actions/viewports");

add_task(function* () {
  let store = Store();
  const { getState, dispatch } = store;

  dispatch(addViewport());
  dispatch(resizeViewport(0, 500, 500));

  let viewport = getState().viewports[0];
  equal(viewport.width, 500, "Resized width of 500");
  equal(viewport.height, 500, "Resized height of 500");
});
