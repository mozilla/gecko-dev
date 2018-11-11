/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const objectStoreName = "foo";
  const indexName = "bar", keyPath = "bar";

  info("Opening database");

  let request = indexedDB.open(name);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;

  let event = yield undefined;
  let db = event.target.result;

  info("Creating objectStore");

  let objectStore = db.createObjectStore(objectStoreName);

  info("Creating a duplicated object store to get an error");

  try {
    db.createObjectStore(objectStoreName);
    ok(false,
       "ConstraintError should be thrown if object store already exists");
  } catch (e) {
    ok(true, "ConstraintError should be thrown if object store already exists");
    is(e.message,
       "Object store named '" + objectStoreName +
       "' already exists at index '0'",
       "Threw with correct error message");
  }

  info("Creating an index");

  objectStore.createIndex(indexName, keyPath);

  info("Creating a duplicated indexes to verify the error message");

  try {
    objectStore.createIndex(indexName, keyPath);

    ok(false, "ConstraintError should be thrown if index already exists");
  } catch (e) {
    ok(true, "ConstraintError should be thrown if index already exists");
    is(e.message,
       "Index named '" + indexName + "' already exists at index '0'",
       "Threw with correct error message");
  }

  request.onsuccess = grabEventAndContinueHandler;
  yield undefined;
  db.close();

  finishTest();
}
