/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const setup = async () => {
  await initGroupDatabase();
  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  return profile;
};

async function createAvatarFile(width, height) {
  const mockAvatarFilePath = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "avatar.png"
  );

  // We need to create actual image data here because our circular avatar
  // processing code in profile-avatar-selector.mjs expects valid image data.
  // If we pass an empty file, the img.decode() call will fail with an EncodingError
  const canvas = new OffscreenCanvas(width, height);
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "red";
  ctx.fillRect(0, 0, width, height);

  const blob = await canvas.convertToBlob({ type: "image/png" });
  const buffer = await blob.arrayBuffer();
  await IOUtils.write(mockAvatarFilePath, new Uint8Array(buffer));

  const mockAvatarFile = Cc["@mozilla.org/file/local;1"].createInstance(
    Ci.nsIFile
  );
  mockAvatarFile.initWithPath(mockAvatarFilePath);

  return mockAvatarFile;
}

async function verifyCustomAvatarImage(
  avatarPath,
  expectedWidth,
  expectedHeight
) {
  // Verify the saved avatar is circular and corners are transparent
  const avatarData = await IOUtils.read(avatarPath);
  const avatarBlob = new Blob([avatarData], { type: "image/png" });

  const img = new Image();
  const loaded = new Promise(resolve => {
    img.addEventListener("load", resolve, { once: true });
  });
  img.src = URL.createObjectURL(avatarBlob);
  await loaded;

  const verifyCanvas = new OffscreenCanvas(img.width, img.height);
  const verifyCtx = verifyCanvas.getContext("2d");
  verifyCtx.drawImage(img, 0, 0);

  const topLeft = verifyCtx.getImageData(0, 0, 1, 1).data;
  const topRight = verifyCtx.getImageData(img.width - 1, 0, 1, 1).data;
  const bottomLeft = verifyCtx.getImageData(0, img.height - 1, 1, 1).data;
  const bottomRight = verifyCtx.getImageData(
    img.width - 1,
    img.height - 1,
    1,
    1
  ).data;
  const center = verifyCtx.getImageData(
    Math.floor(img.width / 2),
    Math.floor(img.height / 2),
    1,
    1
  ).data;

  Assert.equal(img.width, expectedWidth, "The image is the expected width.");
  Assert.equal(img.height, expectedHeight, "The image is the expected height.");

  Assert.equal(topLeft[3], 0, "Top left corner should be transparent");
  Assert.equal(topRight[3], 0, "Top right corner should be transparent");
  Assert.equal(bottomLeft[3], 0, "Bottom left corner should be transparent");
  Assert.equal(bottomRight[3], 0, "Bottom right corner should be transparent");

  Assert.equal(center[0], 255, "Center pixel should have red channel = 255");
  Assert.equal(center[1], 0, "Center pixel should have green channel = 0");
  Assert.equal(center[2], 0, "Center pixel should have blue channel = 0");
  Assert.equal(center[3], 255, "Center pixel should be opaque");

  URL.revokeObjectURL(img.src);
}

add_task(async function test_edit_profile_custom_avatar() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  await setup();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.updated-avatar-selector", true]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );

        await editProfileCard.updateComplete;

        EventUtils.synthesizeMouseAtCenter(
          editProfileCard.avatarSelectorLink,
          {},
          content
        );

        Assert.ok(
          ContentTaskUtils.isVisible(editProfileCard.avatarSelector),
          "Should be showing the profile avatar selector"
        );
      });
    }
  );

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_edit_profile_custom_avatar_upload() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  const profile = await setup();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.updated-avatar-selector", true]],
  });

  const avatarWidth = 100;
  const avatarHeight = 100;
  const mockAvatarFile = await createAvatarFile(avatarWidth, avatarHeight);

  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window.browsingContext);
  MockFilePicker.setFiles([mockAvatarFile]);
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  let curProfile = await SelectableProfileService.getProfile(profile.id);
  await curProfile.setAvatar("star");
  Assert.ok(
    !curProfile.hasCustomAvatar,
    "Current profile does not have a custom avatar"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      const customAvatarData = await SpecialPowers.spawn(
        browser,
        [],
        async () => {
          let editProfileCard =
            content.document.querySelector("edit-profile-card").wrappedJSObject;

          await ContentTaskUtils.waitForCondition(
            () => editProfileCard.initialized,
            "Waiting for edit-profile-card to be initialized"
          );

          await editProfileCard.updateComplete;

          const avatarSelector = editProfileCard.avatarSelector;

          EventUtils.synthesizeMouseAtCenter(
            editProfileCard.avatarSelectorLink,
            {},
            content
          );

          Assert.ok(
            ContentTaskUtils.isVisible(avatarSelector),
            "Should be showing the profile avatar selector"
          );

          EventUtils.synthesizeMouseAtCenter(
            avatarSelector.customTabButton,
            {},
            content
          );
          await avatarSelector.updateComplete;

          await ContentTaskUtils.waitForCondition(
            () => ContentTaskUtils.isVisible(avatarSelector.input),
            "Waiting for avatar selector input to be visible"
          );

          const inputReceived = new Promise(resolve =>
            avatarSelector.input.addEventListener(
              "input",
              event => {
                resolve(event.target.files[0].name);
              },
              { once: true }
            )
          );

          EventUtils.synthesizeMouseAtCenter(avatarSelector.input, {}, content);

          await inputReceived;

          await ContentTaskUtils.waitForCondition(
            () => ContentTaskUtils.isVisible(avatarSelector.saveButton),
            "Waiting for avatar selector save button to be visible"
          );

          let region = avatarSelector.avatarRegion.dimensions;
          let cropClientHeight =
            avatarSelector.customAvatarCropArea.clientHeight;
          let cropClientWidth = avatarSelector.customAvatarCropArea.clientWidth;

          EventUtils.synthesizeMouseAtCenter(
            avatarSelector.saveButton,
            {},
            content
          );

          // Sometimes the async message takes a bit longer to arrive.
          await new Promise(resolve => content.setTimeout(resolve, 100));

          return { region, cropClientHeight, cropClientWidth };
        }
      );

      curProfile = await SelectableProfileService.getProfile(profile.id);
      Assert.ok(
        curProfile.hasCustomAvatar,
        "Current profile has a custom avatar image"
      );

      let { region, cropClientHeight, cropClientWidth } = customAvatarData;
      const scale =
        avatarWidth <= avatarHeight
          ? avatarWidth / cropClientWidth
          : avatarHeight / cropClientHeight;

      const expectedWidth =
        2 * Math.round(region.radius * scale * window.devicePixelRatio);

      await verifyCustomAvatarImage(
        curProfile.getAvatarPath(),
        expectedWidth,
        expectedWidth
      );
    }
  );

  MockFilePicker.cleanup();

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_avatar_selector_tabs() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  await setup();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.updated-avatar-selector", true]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );

        await editProfileCard.updateComplete;

        EventUtils.synthesizeMouseAtCenter(
          editProfileCard.avatarSelectorLink,
          {},
          content
        );

        const avatarSelector = editProfileCard.avatarSelector;
        await avatarSelector.updateComplete;

        // Verify Icon is default
        Assert.equal(
          avatarSelector.view,
          "icon",
          "Icon should be the default tab"
        );
        Assert.equal(
          avatarSelector.iconTabButton.type,
          "primary",
          "Icon tab should be active by default"
        );
        Assert.equal(
          avatarSelector.customTabButton.type,
          "default",
          "Custom tab should be inactive by default"
        );

        EventUtils.synthesizeMouseAtCenter(
          avatarSelector.customTabButton,
          {},
          content
        );
        await avatarSelector.updateComplete;
        Assert.equal(
          avatarSelector.view,
          "custom",
          "Should switch to custom tab"
        );
        Assert.equal(
          avatarSelector.customTabButton.type,
          "primary",
          "Custom tab should be active"
        );
        Assert.equal(
          avatarSelector.iconTabButton.type,
          "default",
          "Icon tab should be inactive"
        );

        EventUtils.synthesizeMouseAtCenter(
          avatarSelector.iconTabButton,
          {},
          content
        );
        await avatarSelector.updateComplete;
        Assert.equal(
          avatarSelector.view,
          "icon",
          "Should switch back to icon tab"
        );
        Assert.equal(
          avatarSelector.iconTabButton.type,
          "primary",
          "Icon tab should be active"
        );
        Assert.equal(
          avatarSelector.customTabButton.type,
          "default",
          "Custom tab should be inactive"
        );
      });
    }
  );

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_edit_profile_custom_avatar_crop() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  const profile = await setup();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.updated-avatar-selector", true]],
  });

  const avatarWidth = 1000;
  const avatarHeight = 2000;

  const mockAvatarFile = await createAvatarFile(avatarWidth, avatarHeight);

  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window.browsingContext);
  MockFilePicker.setFiles([mockAvatarFile]);
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  let curProfile = await SelectableProfileService.getProfile(profile.id);
  await curProfile.setAvatar("star");
  Assert.ok(
    !curProfile.hasCustomAvatar,
    "Current profile does not have a custom avatar"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      const customAvatarData = await SpecialPowers.spawn(
        browser,
        [],
        async () => {
          let editProfileCard =
            content.document.querySelector("edit-profile-card").wrappedJSObject;

          await ContentTaskUtils.waitForCondition(
            () => editProfileCard.initialized,
            "Waiting for edit-profile-card to be initialized"
          );

          await editProfileCard.updateComplete;

          const avatarSelector = editProfileCard.avatarSelector;

          EventUtils.synthesizeMouseAtCenter(
            editProfileCard.avatarSelectorLink,
            {},
            content
          );

          Assert.ok(
            ContentTaskUtils.isVisible(avatarSelector),
            "Should be showing the profile avatar selector"
          );

          EventUtils.synthesizeMouseAtCenter(
            avatarSelector.customTabButton,
            {},
            content
          );
          await avatarSelector.updateComplete;

          await ContentTaskUtils.waitForCondition(
            () => ContentTaskUtils.isVisible(avatarSelector.input),
            "Waiting for avatar selector input to be visible"
          );

          const inputReceived = new Promise(resolve =>
            avatarSelector.input.addEventListener(
              "input",
              event => {
                resolve(event.target.files[0].name);
              },
              { once: true }
            )
          );

          EventUtils.synthesizeMouseAtCenter(avatarSelector.input, {}, content);

          await inputReceived;

          await ContentTaskUtils.waitForCondition(
            () => ContentTaskUtils.isVisible(avatarSelector.saveButton),
            "Waiting for avatar selector save button to be visible"
          );

          // do cropping here
          for (let [target, diffX, diffY] of [
            [avatarSelector.topLeftMover, 20, 20],
            [avatarSelector.topRightMover, -20, 20],
            [avatarSelector.bottomLeftMover, 20, -20],
            [avatarSelector.bottomRightMover, -20, -20],
          ]) {
            let start = target.getBoundingClientRect();

            let startCenterX = start.x + (start.width + diffX) / 2;
            let startCenterY = start.y + (start.height + diffY) / 2;
            // Start dragging region
            EventUtils.synthesizeMouseAtPoint(
              startCenterX,
              startCenterY,
              { type: "mousedown" },
              content
            );

            await ContentTaskUtils.waitForCondition(
              () => avatarSelector.state === "resizing",
              "Waiting for avatar selector state to be resizing"
            );

            // Drag the mover element
            EventUtils.synthesizeMouseAtPoint(
              startCenterX + diffX,
              startCenterY + diffY,
              { type: "mousemove" },
              content
            );

            // Stop dragging
            EventUtils.synthesizeMouseAtPoint(
              startCenterX + diffX,
              startCenterY + diffY,
              { type: "mouseup" },
              content
            );

            await ContentTaskUtils.waitForCondition(
              () => avatarSelector.state === "selected",
              "Waiting for avatar selector state to be selected"
            );
          }

          let region = avatarSelector.avatarRegion.dimensions;
          let cropClientHeight =
            avatarSelector.customAvatarCropArea.clientHeight;
          let cropClientWidth = avatarSelector.customAvatarCropArea.clientWidth;

          EventUtils.synthesizeMouseAtCenter(
            avatarSelector.saveButton,
            {},
            content
          );

          // Sometimes the async message takes a bit longer to arrive.
          await new Promise(resolve => content.setTimeout(resolve, 100));

          return { region, cropClientHeight, cropClientWidth };
        }
      );

      curProfile = await SelectableProfileService.getProfile(profile.id);
      Assert.ok(
        curProfile.hasCustomAvatar,
        "Current profile has a custom avatar image"
      );

      let { region, cropClientHeight, cropClientWidth } = customAvatarData;
      const scale =
        avatarWidth <= avatarHeight
          ? avatarWidth / cropClientWidth
          : avatarHeight / cropClientHeight;

      const expectedWidth =
        2 * Math.round(region.radius * scale * window.devicePixelRatio);

      await verifyCustomAvatarImage(
        curProfile.getAvatarPath(),
        expectedWidth,
        expectedWidth
      );
    }
  );

  MockFilePicker.cleanup();

  await SpecialPowers.popPrefEnv();
});
