add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.rememberSignons.visibilityToggle", true]],
  });
});

/**
 * Test that the doorhanger password field shows plain or * text
 * when the checkbox is checked.
 */
add_task(async function test_toggle_password() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        "https://example.com/browser/toolkit/components/passwordmgr/test/browser/form_basic.html",
    },
    async function(browser) {
      // Submit the form in the content page with the credentials from the test
      // case. This will cause the doorhanger notification to be displayed.
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown",
        event => event.target == PopupNotifications.panel
      );
      await ContentTask.spawn(browser, null, async function() {
        let doc = content.document;
        doc.getElementById("form-basic-username").value = "username";
        doc.getElementById("form-basic-password").value = "pw";
        doc.getElementById("form-basic").submit();
      });
      await promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector(
        "#password-notification-password"
      );
      let toggleCheckbox = notificationElement.querySelector(
        "#password-notification-visibilityToggle"
      );

      await EventUtils.synthesizeMouseAtCenter(toggleCheckbox, {});
      Assert.ok(toggleCheckbox.checked);
      Assert.equal(
        passwordTextbox.type,
        "text",
        "Password textbox changed to plain text"
      );

      await EventUtils.synthesizeMouseAtCenter(toggleCheckbox, {});
      Assert.ok(!toggleCheckbox.checked);
      Assert.equal(
        passwordTextbox.type,
        "password",
        "Password textbox changed to * text"
      );
    }
  );
});

/**
 * Test that the doorhanger password toggle checkbox is disabled
 * when the master password is set.
 */
add_task(async function test_checkbox_disabled_if_has_master_password() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        "https://example.com/browser/toolkit/components/passwordmgr/test/browser/form_basic.html",
    },
    async function(browser) {
      // Submit the form in the content page with the credentials from the test
      // case. This will cause the doorhanger notification to be displayed.
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown",
        event => event.target == PopupNotifications.panel
      );

      LoginTestUtils.masterPassword.enable();

      await ContentTask.spawn(browser, null, async function() {
        let doc = content.document;
        doc.getElementById("form-basic-username").value = "username";
        doc.getElementById("form-basic-password").value = "pass";
        doc.getElementById("form-basic").submit();
      });
      await promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector(
        "#password-notification-password"
      );
      let toggleCheckbox = notificationElement.querySelector(
        "#password-notification-visibilityToggle"
      );

      Assert.equal(
        passwordTextbox.type,
        "password",
        "Password textbox should show * text"
      );
      Assert.ok(
        toggleCheckbox.getAttribute("hidden"),
        "checkbox is hidden when master password is set"
      );
    }
  );

  LoginTestUtils.masterPassword.disable();
});
