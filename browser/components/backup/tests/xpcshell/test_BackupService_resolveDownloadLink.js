/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that valid links are produced for each update channel.
 */
add_task(async function test_supported_update_channel_links() {
  const SUPPORTED_CHANNELS = ["nightly", "aurora", "beta", "release", "esr"];
  for (let channel of SUPPORTED_CHANNELS) {
    let bs = new BackupService();
    let linkHref = await bs.resolveDownloadLink(channel);
    let url = URL.parse(linkHref);
    Assert.ok(url, `Got a valid URL back for channel ${channel}`);
  }
});

/**
 * Tests that a fallback links is produced for an unsupported update channel.
 */
add_task(async function test_unsupported_update_channel() {
  const UNSUPPORTED_CHANNEL = "test123";

  let bs = new BackupService();
  let linkHref = await bs.resolveDownloadLink(UNSUPPORTED_CHANNEL);
  let url = URL.parse(linkHref);
  Assert.ok(url, "Got a valid URL back for unsupported channel");
});
