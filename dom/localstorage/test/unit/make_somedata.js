/*
  Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/
*/

add_task(async function testSteps() {
  const principal = getPrincipal("https://example.com");

  const data = {
    key: "foo",
    value: "bar",
  };

  info("Getting storage");

  const storage = getLocalStorage(principal);

  info("Adding data");

  storage.setItem(data.key, data.value);

  info("Resetting origin");

  const request = resetOrigin(principal);
  await requestFinished(request);
});
