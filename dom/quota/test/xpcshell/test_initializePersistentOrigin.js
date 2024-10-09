/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that initializePersistentOrigin() does call
 * QuotaManager::EnsurePersistentOriginIsInitialized() which ensures origin
 * directory existence.
 */

async function testSteps() {
  const originMetadata = {
    principal: getPrincipal("https://foo.example.com"),
    file: getRelativeFile("storage/permanent/https+++foo.example.com"),
  };

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Initializing persistent origin");

  ok(!originMetadata.file.exists(), "Origin directory does not exist");

  request = initPersistentOrigin(originMetadata.principal);
  await requestFinished(request);

  ok(originMetadata.file.exists(), "Origin directory does exist");

  info("Verifying persistent origin initialization status");

  request = persistentOriginInitialized(originMetadata.principal);
  await requestFinished(request);

  ok(request.result, "Persistent origin is initialized");
}
