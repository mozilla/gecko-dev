/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("double register", {
  async test(_browser, _window, fileExt) {
    Assert.throws(
      () =>
        ChromeUtils.registerProcessActor(
          "TestProcessActor",
          processActorOptions[fileExt]
        ),
      /'TestProcessActor' actor is already registered./,
      "Throw if registering a process actor with the same name twice."
    );

    Assert.throws(
      () =>
        ChromeUtils.registerWindowActor(
          "TestProcessActor",
          processActorOptions[fileExt]
        ),
      /'TestProcessActor' actor is already registered as a process actor./,
      "Throw if registering a window actor with the same name as a process actor."
    );
  },
});
