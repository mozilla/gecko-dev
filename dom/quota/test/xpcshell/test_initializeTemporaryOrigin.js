/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that initializeTemporaryOrigin() does call
 * QuotaManager::EnsureTemporaryOriginIsInitialized() which ensures origin
 * directory existence.
 */

async function testSteps() {
  const originMetadata = {
    persistence: "default",
    principal: getPrincipal("https://foo.example.com"),
    file: getRelativeFile("storage/default/https+++foo.example.com"),
  };

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Initializing temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  info("Initializing temporary origin");

  ok(!originMetadata.file.exists(), "Origin directory does not exist");

  request = initTemporaryOrigin(
    originMetadata.persistence,
    originMetadata.principal
  );
  await requestFinished(request);

  ok(originMetadata.file.exists(), "Origin directory does exist");

  info("Verifying temporary origin initialization status");

  request = temporaryOriginInitialized(
    originMetadata.persistence,
    originMetadata.principal
  );
  await requestFinished(request);

  ok(request.result, "Temporary origin is initialized");
}
