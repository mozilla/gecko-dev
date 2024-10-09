/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify clearing by client type.
 */

async function testSteps() {
  const packages = [
    "clearStoragesForClient_profile",
    "defaultStorageDirectory_shared",
  ];

  const testData = [
    {
      origin: "http://example.com",
      client: "sdb",
      persistence: null,
      key: "afterClearByClient",
    },
    {
      origin: "http://example.com",
      client: "sdb",
      persistence: "default",
      key: "afterClearByClient_default",
    },
    {
      origin: "http://example.com",
      client: "sdb",
      persistence: "persistent",
      key: "afterClearByClient_persistent",
    },
    {
      origin: "http://example.com",
      client: "sdb",
      persistence: "temporary",
      key: "afterClearByClient_temporary",
    },
  ];

  for (const item of testData) {
    info("Clearing");

    let request = clear();
    await requestFinished(request);

    info("Verifying storage");

    verifyStorage(packages, "beforeInstall");

    info("Installing package");

    installPackages(packages);

    info("Verifying storage");

    verifyStorage(packages, "afterInstall");

    // TODO: Remove this block once origin clearing is able to ignore unknown
    //       directories.
    getRelativeFile("storage/default/invalid+++example.com").remove(false);
    getRelativeFile("storage/permanent/invalid+++example.com").remove(false);
    getRelativeFile("storage/temporary/invalid+++example.com").remove(false);

    info("Clearing by client type");

    request = clearClient(
      getPrincipal(item.origin),
      item.client,
      item.persistence
    );
    await requestFinished(request);

    info("Verifying storage");

    verifyStorage(packages, item.key, "afterClearByClient");
  }
}
