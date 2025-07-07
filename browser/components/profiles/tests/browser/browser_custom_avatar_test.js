/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const setup = async () => {
  await initGroupDatabase();
  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  return profile;
};

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
        await new Promise(resolve => content.requestAnimationFrame(resolve));

        EventUtils.synthesizeMouseAtCenter(
          editProfileCard.avatarSelectorLink,
          {},
          content
        );

        const avatarSelector = editProfileCard.avatarSelector;

        Assert.ok(
          ContentTaskUtils.isVisible(avatarSelector),
          "Should be showing the profile avatar selector"
        );

        avatarSelector.state = "custom";
        await avatarSelector.updateComplete;
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

  const mockAvatarFilePath = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "avatar.png"
  );

  // We need to create actual image data here because our circular avatar
  // processing code in profile-avatar-selector.mjs expects valid image data.
  // If we pass an empty file, the img.decode() call will fail with an EncodingError
  const canvas = new OffscreenCanvas(100, 100);
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "red";
  ctx.fillRect(0, 0, 100, 100);

  const blob = await canvas.convertToBlob({ type: "image/png" });
  const buffer = await blob.arrayBuffer();
  await IOUtils.write(mockAvatarFilePath, new Uint8Array(buffer));

  const mockAvatarFile = Cc["@mozilla.org/file/local;1"].createInstance(
    Ci.nsIFile
  );
  mockAvatarFile.initWithPath(mockAvatarFilePath);

  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window.browsingContext);
  MockFilePicker.setFiles([mockAvatarFile]);
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  let curProfile = await SelectableProfileService.getProfile(profile.id);
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
      await SpecialPowers.spawn(browser, [], async () => {
        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );

        await editProfileCard.updateComplete;
        await new Promise(resolve => content.requestAnimationFrame(resolve));

        EventUtils.synthesizeMouseAtCenter(
          editProfileCard.avatarSelectorLink,
          {},
          content
        );

        const avatarSelector = editProfileCard.avatarSelector;

        Assert.ok(
          ContentTaskUtils.isVisible(avatarSelector),
          "Should be showing the profile avatar selector"
        );

        avatarSelector.state = "custom";
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

        EventUtils.synthesizeMouseAtCenter(
          avatarSelector.saveButton,
          {},
          content
        );

        // Sometimes the async message takes a bit longer to arrive.
        await new Promise(resolve => content.setTimeout(resolve, 100));
      });
    }
  );

  curProfile = await SelectableProfileService.getProfile(profile.id);
  Assert.ok(
    curProfile.hasCustomAvatar,
    "Current profile has a custom avatar image"
  );

  // Verify the saved avatar is circular and corners are transparent
  const avatarPath = curProfile.getAvatarPath();
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

  Assert.equal(topLeft[3], 0, "Top left corner should be transparent");
  Assert.equal(topRight[3], 0, "Top right corner should be transparent");
  Assert.equal(bottomLeft[3], 0, "Bottom left corner should be transparent");
  Assert.equal(bottomRight[3], 0, "Bottom right corner should be transparent");

  Assert.equal(center[0], 255, "Center pixel should have red channel = 255");
  Assert.equal(center[1], 0, "Center pixel should have green channel = 0");
  Assert.equal(center[2], 0, "Center pixel should have blue channel = 0");
  Assert.equal(center[3], 255, "Center pixel should be opaque");

  URL.revokeObjectURL(img.src);

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

        await editProfileCard.updateComplete;
        await new Promise(resolve => content.requestAnimationFrame(resolve));

        EventUtils.synthesizeMouseAtCenter(
          editProfileCard.avatarSelectorLink,
          {},
          content
        );

        const avatarSelector = editProfileCard.avatarSelector;

        await ContentTaskUtils.waitForCondition(
          () => ContentTaskUtils.isVisible(avatarSelector),
          "Waiting for avatar selector to become visible"
        );
        await avatarSelector.updateComplete;

        // Wait for all Lit microtask queue updates to complete so we don't run into race condition issues
        await new Promise(resolve => content.requestAnimationFrame(resolve));

        const buttonGroup =
          avatarSelector.shadowRoot.querySelector(".button-group");
        Assert.ok(buttonGroup, "Tab section should exist");

        const iconTab = buttonGroup.querySelector(
          '[data-l10n-id="avatar-selector-icon-tab"]'
        );
        const customTab = buttonGroup.querySelector(
          '[data-l10n-id="avatar-selector-custom-tab"]'
        );
        Assert.ok(iconTab, "Icon tab should exist");
        Assert.ok(customTab, "Custom tab should exist");

        // Verify Icon is default
        Assert.equal(
          avatarSelector.state,
          "icon",
          "Icon should be the default tab"
        );
        Assert.equal(
          iconTab.type,
          "primary",
          "Icon tab should be active by default"
        );
        Assert.equal(
          customTab.type,
          "default",
          "Custom tab should be inactive by default"
        );

        info("Clicking custom tab");
        EventUtils.synthesizeMouseAtCenter(customTab, {}, content);
        await avatarSelector.updateComplete;

        Assert.equal(
          avatarSelector.state,
          "custom",
          "Should switch to custom tab"
        );
        Assert.equal(customTab.type, "primary", "Custom tab should be active");
        Assert.equal(iconTab.type, "default", "Icon tab should be inactive");

        info("Clicking icon tab");
        EventUtils.synthesizeMouseAtCenter(iconTab, {}, content);
        await avatarSelector.updateComplete;

        Assert.equal(
          avatarSelector.state,
          "icon",
          "Should switch back to icon tab"
        );
        Assert.equal(iconTab.type, "primary", "Icon tab should be active");
        Assert.equal(
          customTab.type,
          "default",
          "Custom tab should be inactive"
        );
      });
    }
  );

  await SpecialPowers.popPrefEnv();
});
