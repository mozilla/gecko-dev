/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FxAccounts } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccounts.sys.mjs"
);
const { CLIENT_IS_THUNDERBIRD } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsCommon.sys.mjs"
);

add_task(
  async function test_non_https_remote_server_uri_with_requireHttps_false() {
    ensureOauthNotConfigured();
    Services.prefs.setStringPref("identity.fxaccounts.autoconfig.uri", "");
    Services.prefs.setBoolPref("identity.fxaccounts.allowHttp", true);
    Services.prefs.setStringPref(
      "identity.fxaccounts.remote.root",
      "http://example.com/"
    );
    Assert.equal(
      await FxAccounts.config.promiseConnectAccountURI("test"),
      "http://example.com/?context=fx_desktop_v3&entrypoint=test&action=email&service=sync"
    );

    ensureOauthConfigured();
    let url = new URL(await FxAccounts.config.promiseConnectAccountURI("test"));
    Assert.equal(url.host, "example.com");
    Assert.equal(url.searchParams.get("context"), "oauth_webchannel_v1");
    Assert.equal(url.searchParams.get("service"), "sync");
    Assert.equal(url.searchParams.get("entrypoint"), "test");
    Assert.equal(url.searchParams.get("action"), "email");
    Assert.equal(
      url.searchParams.get("client_id"),
      CLIENT_IS_THUNDERBIRD ? "8269bacd7bbc7f80" : "5882386c6d801776"
    );
    Assert.equal(url.searchParams.get("response_type"), "code");

    Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
    Services.prefs.clearUserPref("identity.fxaccounts.allowHttp");
    resetOauthConfig();
  }
);

add_task(async function test_non_https_remote_server_uri() {
  Services.prefs.setStringPref(
    "identity.fxaccounts.remote.root",
    "http://example.com/"
  );
  await Assert.rejects(
    FxAccounts.config.promiseConnectAccountURI(),
    /Firefox Accounts server must use HTTPS/
  );
  Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
});

add_task(async function test_is_production_config() {
  // should start with no auto-config URL.
  Assert.ok(!FxAccounts.config.getAutoConfigURL());
  // which means we are using prod.
  Assert.ok(FxAccounts.config.isProductionConfig());

  // Set an auto-config URL.
  Services.prefs.setStringPref(
    "identity.fxaccounts.autoconfig.uri",
    "http://x"
  );
  Assert.equal(FxAccounts.config.getAutoConfigURL(), "http://x");
  Assert.ok(!FxAccounts.config.isProductionConfig());

  // Clear the auto-config URL, but set one of the other config params.
  Services.prefs.clearUserPref("identity.fxaccounts.autoconfig.uri");
  Services.prefs.setStringPref("identity.sync.tokenserver.uri", "http://t");
  Assert.ok(!FxAccounts.config.isProductionConfig());
  Services.prefs.clearUserPref("identity.sync.tokenserver.uri");
});

add_task(async function test_promise_account_service_param() {
  ensureOauthNotConfigured();
  Services.prefs.setStringPref("identity.fxaccounts.autoconfig.uri", "");
  Services.prefs.setStringPref(
    "identity.fxaccounts.remote.root",
    "https://accounts.firefox.com/"
  );
  Assert.equal(
    await FxAccounts.config.promiseConnectAccountURI("test", {
      service: "custom-service",
    }),
    "https://accounts.firefox.com/?context=fx_desktop_v3&entrypoint=test&action=email&service=custom-service"
  );
  ensureOauthConfigured();
  let url = new URL(
    await FxAccounts.config.promiseConnectAccountURI("test", {
      service: "custom-service",
    })
  );
  Assert.equal(url.searchParams.get("context"), "oauth_webchannel_v1");
  Assert.equal(
    url.searchParams.get("client_id"),
    CLIENT_IS_THUNDERBIRD ? "8269bacd7bbc7f80" : "5882386c6d801776"
  );
  Assert.equal(url.searchParams.get("service"), "custom-service");
  resetOauthConfig();
});
