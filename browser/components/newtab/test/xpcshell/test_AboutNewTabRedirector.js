/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AboutNewTabRedirectorParent, AboutNewTabRedirectorChild } =
  ChromeUtils.importESModule(
    "resource:///modules/AboutNewTabRedirector.sys.mjs"
  );

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

const BUILTIN_NEWTAB_ENABLED_PREF = "browser.newtabpage.enabled";
const ABOUT_HOME_URI = Services.io.newURI("about:home");
const ABOUT_NEWTAB_URI = Services.io.newURI("about:newtab");
const BLANK_TAB_URI = Services.io.newURI(
  "chrome://browser/content/blanktab.html"
);

const PARENT_INSTANCE = new AboutNewTabRedirectorParent();
const CHILD_INSTANCE = new AboutNewTabRedirectorChild();

/**
 * Tests that both the parent and child implementations return the blank tab
 * document at chrome://browser/content/blanktab.html via getChromeURI.
 */
add_task(async function test_chromeURI() {
  const INSTANCES = [PARENT_INSTANCE, CHILD_INSTANCE];

  for (let instance of INSTANCES) {
    info("Testing " + instance.constructor.name);
    Assert.ok(
      instance.getChromeURI(ABOUT_HOME_URI).equals(BLANK_TAB_URI),
      "Got back the blank tab URI for getChromeURI(about:home)"
    );
    Assert.ok(
      instance.getChromeURI(ABOUT_NEWTAB_URI).equals(BLANK_TAB_URI),
      "Got back the blank tab URI for getChromeURI(about:newtab)"
    );
  }
});

/**
 * Test that AboutNewTabRedirectorParent returns the right values when
 * constructing a new channel, with and without the newtab enabled pref set
 * to true.
 */
add_task(async function test_parent_newChannel() {
  Services.prefs.setBoolPref(BUILTIN_NEWTAB_ENABLED_PREF, true);

  // The dummy channel lets us create an nsILoadInfo, which newChannel expects.
  const DUMMY_CHANNEL = NetUtil.newChannel({
    uri: "http://localhost",
    loadUsingSystemPrincipal: true,
  });

  const DEFAULT_URL_CHANNEL = Services.io.newChannelFromURIWithLoadInfo(
    Services.io.newURI(PARENT_INSTANCE.defaultURL),
    DUMMY_CHANNEL.loadInfo
  );

  // We expect the parent instance to return the defaultURL for about:home
  // and about:newtab (if enabled).
  let resultAboutHomeChannel = PARENT_INSTANCE.newChannel(
    ABOUT_HOME_URI,
    DUMMY_CHANNEL.loadInfo
  );

  Assert.ok(
    resultAboutHomeChannel.URI.equals(DEFAULT_URL_CHANNEL.URI),
    "about:home got the defaultURL back."
  );

  let resultAboutNewTabChannel = PARENT_INSTANCE.newChannel(
    ABOUT_NEWTAB_URI,
    DUMMY_CHANNEL.loadInfo
  );

  Assert.ok(
    resultAboutNewTabChannel.URI.equals(DEFAULT_URL_CHANNEL.URI),
    "about:newtab got the defaultURL back."
  );

  // With about:newtab disabled, we expect the blanktab fallback for
  // about:newtab, but about:home should still return the defaultURL.
  Services.prefs.setBoolPref(BUILTIN_NEWTAB_ENABLED_PREF, false);

  let blankTabChannel = Services.io.newChannelFromURIWithLoadInfo(
    BLANK_TAB_URI,
    DUMMY_CHANNEL.loadInfo
  );

  resultAboutHomeChannel = PARENT_INSTANCE.newChannel(
    ABOUT_HOME_URI,
    DUMMY_CHANNEL.loadInfo
  );

  Assert.ok(
    resultAboutHomeChannel.URI.equals(DEFAULT_URL_CHANNEL.URI),
    "about:home got the defaultURL back with about:newtab disabled."
  );

  resultAboutNewTabChannel = PARENT_INSTANCE.newChannel(
    ABOUT_NEWTAB_URI,
    DUMMY_CHANNEL.loadInfo
  );

  Assert.ok(
    resultAboutNewTabChannel.URI.equals(blankTabChannel.URI),
    "about:newtab got the blanktab URI back with about:newtab disabled."
  );

  Services.prefs.clearUserPref(BUILTIN_NEWTAB_ENABLED_PREF);
});
