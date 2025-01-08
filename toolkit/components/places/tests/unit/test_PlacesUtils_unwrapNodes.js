/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that unwrapNodes properly tracks valid and invalid nodes.

add_task(function () {
  let tests = [
    {
      uri: "https://example.com",
      type: PlacesUtils.TYPE_X_MOZ_URL,
      invalidCount: 0,
      validCount: 1,
    },
    {
      uri: `https://exa:mple.com\ninvalid-uri`,
      type: PlacesUtils.TYPE_PLAINTEXT,
      invalidCount: 2,
      validCount: 0,
    },
    {
      uri: `https://exa:mple.com\nhttps://example.com`,
      type: PlacesUtils.TYPE_PLAINTEXT,
      invalidCount: 1,
      validCount: 1,
    },
    {
      uri: "https://example.com\nhttps://example.com",
      type: PlacesUtils.TYPE_PLAINTEXT,
      invalidCount: 0,
      validCount: 2,
    },
    {
      uri: "https://broken:url",
      type: PlacesUtils.TYPE_X_MOZ_URL,
      invalidCount: 1,
      validCount: 0,
    },
    {
      uri: "invalid-uri",
      type: PlacesUtils.TYPE_PLAINTEXT,
      invalidCount: 1,
      validCount: 0,
    },
  ];

  for (let test of tests) {
    let { validNodes, invalidNodes } = PlacesUtils.unwrapNodes(
      test.uri,
      test.type
    );

    Assert.equal(
      invalidNodes.length,
      test.invalidCount,
      "Should correctly mark all invalid entries"
    );

    Assert.equal(
      validNodes.length,
      test.validCount,
      "Should correctly mark all valid entries"
    );
  }
});
