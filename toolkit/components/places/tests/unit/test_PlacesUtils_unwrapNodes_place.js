/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that unwrapNodes properly filters out place: uris from text flavors.

add_task(function () {
  let tests = [
    // Single url.
    {
      blob: "place:type=0&sort=1:",
      type: PlacesUtils.TYPE_X_MOZ_URL,
      invalidCount: 1,
    },
    // Multiple urls.
    {
      blob: "place:type=0&sort=1:\nfirst\nplace:type=0&sort=1\nsecond",
      type: PlacesUtils.TYPE_X_MOZ_URL,
      invalidCount: 2,
    },
    // Url == title.
    {
      blob: "place:type=0&sort=1:\nplace:type=0&sort=1",
      type: PlacesUtils.TYPE_X_MOZ_URL,
      invalidCount: 1,
    },
    // Malformed.
    {
      blob: "place:type=0&sort=1:\nplace:type=0&sort=1\nmalformed",
      type: PlacesUtils.TYPE_X_MOZ_URL,
      invalidCount: 0,
    },
    // Single url.
    {
      blob: "place:type=0&sort=1:",
      type: PlacesUtils.TYPE_PLAINTEXT,
      invalidCount: 1,
    },
    // Multiple urls.
    {
      blob: "place:type=0&sort=1:\nplace:type=0&sort=1",
      type: PlacesUtils.TYPE_PLAINTEXT,
      invalidCount: 2,
    },
  ];
  for (let { blob, type, invalidCount } of tests) {
    Assert.deepEqual(
      PlacesUtils.unwrapNodes(blob, type).validNodes,
      [],
      "No valid entries should be found"
    );

    Assert.equal(
      PlacesUtils.unwrapNodes(blob, type).invalidNodes.length,
      invalidCount,
      "Should correctly mark all invalid entries"
    );
  }
});
