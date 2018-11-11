/* import-globals-from ../../../common/tests/unit/head_helpers.js */

const { Constructor: CC } = Components;

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://testing-common/httpd.js");

const { RemoteSettings } = ChromeUtils.import("resource://services-settings/remote-settings.js", {});
const { UptakeTelemetry } = ChromeUtils.import("resource://services-common/uptake-telemetry.js", {});

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream", "setInputStream");

let server;
let client;

async function clear_state() {
  // Clear local DB.
  const collection = await client.openCollection();
  await collection.clear();
  // Reset event listeners.
  client._listeners.set("sync", []);
  Services.prefs.clearUserPref("services.settings.default_bucket");
}


function run_test() {
  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  // Point the blocklist clients to use this local HTTP server.
  Services.prefs.setCharPref("services.settings.server",
                             `http://localhost:${server.identity.primaryPort}/v1`);
  // Ensure that signature verification is disabled to prevent interference
  // with basic certificate sync tests
  Services.prefs.setBoolPref("services.settings.verify_signature", false);

  client = RemoteSettings("password-fields");

  // Setup server fake responses.
  function handleResponse(request, response) {
    try {
      const sample = getSampleResponse(request, server.identity.primaryPort);
      if (!sample) {
        do_throw(`unexpected ${request.method} request for ${request.path}?${request.queryString}`);
      }

      response.setStatusLine(null, sample.status.status,
                             sample.status.statusText);
      // send the headers
      for (let headerLine of sample.sampleHeaders) {
        let headerElements = headerLine.split(":");
        response.setHeader(headerElements[0], headerElements[1].trimLeft());
      }
      response.setHeader("Date", (new Date()).toUTCString());

      response.write(JSON.stringify(sample.responseBody));
      response.finish();
    } catch (e) {
      info(e);
    }
  }
  const configPath = "/v1/";
  const changesPath = "/v1/buckets/monitor/collections/changes/records";
  const recordsPath  = "/v1/buckets/main/collections/password-fields/records";
  server.registerPathHandler(configPath, handleResponse);
  server.registerPathHandler(changesPath, handleResponse);
  server.registerPathHandler(recordsPath, handleResponse);

  run_next_test();

  registerCleanupFunction(function() {
    server.stop(() => { });
  });
}

add_task(async function test_records_obtained_from_server_are_stored_in_db() {
  // Test an empty db populates
  await client.maybeSync(2000, Date.now());

  // Open the collection, verify it's been populated:
  // Our test data has a single record; it should be in the local collection
  const list = await client.get();
  equal(list.length, 1);
});
add_task(clear_state);

add_task(async function test_records_can_have_local_fields() {
  const c = RemoteSettings("password-fields", { localFields: ["accepted"] });
  await c.maybeSync(2000, Date.now());

  const col = await c.openCollection();
  await col.update({ id: "9d500963-d80e-3a91-6e74-66f3811b99cc", accepted: true });

  await c.maybeSync(2000, Date.now()); // Does not fail.
});
add_task(clear_state);

add_task(async function test_current_server_time_is_saved_in_pref() {
  const serverTime = Date.now();
  await client.maybeSync(2000, serverTime);
  equal(client.lastCheckTimePref, "services.settings.main.password-fields.last_check");
  const after = Services.prefs.getIntPref(client.lastCheckTimePref);
  equal(after, Math.round(serverTime / 1000));
});
add_task(clear_state);

add_task(async function test_records_changes_are_overwritten_by_server_changes() {
  // Create some local conflicting data, and make sure it syncs without error.
  const collection = await client.openCollection();
  await collection.create({
    "website": "",
    "id": "9d500963-d80e-3a91-6e74-66f3811b99cc",
  }, { useRecordId: true });

  await client.maybeSync(2000, Date.now());

  const data = await client.get();
  equal(data[0].website, "https://some-website.com");
});
add_task(clear_state);

add_task(async function test_default_records_come_from_a_local_dump_when_database_is_empty() {
  // When collection is unknown, no dump is loaded, and there is no error.
  let data = await RemoteSettings("some-unknown-key").get();
  equal(data.length, 0);

  // When collection has a dump in services/settings/dumps/{bucket}/{collection}.json
  data = await RemoteSettings("certificates", { bucketNamePref: "services.blocklist.bucket" }).get();
  notEqual(data.length, 0);
});
add_task(clear_state);

add_task(async function test_sync_event_provides_information_about_records() {
  const serverTime = Date.now();

  let eventData;
  client.on("sync", ({ data }) => eventData = data);

  await client.maybeSync(2000, serverTime - 1000);
  equal(eventData.current.length, 1);

  await client.maybeSync(3001, serverTime);
  equal(eventData.current.length, 2);
  equal(eventData.created.length, 1);
  equal(eventData.created[0].website, "https://www.other.org/signin");
  equal(eventData.updated.length, 1);
  equal(eventData.updated[0].old.website, "https://some-website.com");
  equal(eventData.updated[0].new.website, "https://some-website.com/login");
  equal(eventData.deleted.length, 0);

  await client.maybeSync(4001, serverTime);
  equal(eventData.current.length, 1);
  equal(eventData.created.length, 0);
  equal(eventData.updated.length, 0);
  equal(eventData.deleted.length, 1);
  equal(eventData.deleted[0].website, "https://www.other.org/signin");
});
add_task(clear_state);

add_task(async function test_inspect_method() {
  const serverTime = Date.now();

  // Synchronize the `password-fields` collection.
  await client.maybeSync(2000, serverTime);

  const inspected = await RemoteSettings.inspect();

  const { mainBucket, serverURL, defaultSigner, collections } = inspected;
  const rsSigner = "remote-settings.content-signature.mozilla.org";
  equal(mainBucket, "main");
  equal(serverURL, `http://localhost:${server.identity.primaryPort}/v1`);
  equal(defaultSigner, rsSigner);

  equal(inspected.serverTimestamp, '"4000"');
  equal(collections.length, 1);
  // password-fields was synchronized and has local data.
  equal(collections[0].collection, "password-fields");
  equal(collections[0].serverTimestamp, 3000);
  equal(collections[0].localTimestamp, 3000);
});
add_task(clear_state);

add_task(async function test_listeners_are_not_deduplicated() {
  const serverTime = Date.now();

  let count = 0;
  const plus1 = () => { count += 1; };

  client.on("sync", plus1);
  client.on("sync", plus1);
  client.on("sync", plus1);

  await client.maybeSync(2000, serverTime);

  equal(count, 3);
});
add_task(clear_state);

add_task(async function test_listeners_can_be_removed() {
  const serverTime = Date.now();

  let count = 0;
  const onSync = () => { count += 1; };

  client.on("sync", onSync);
  client.off("sync", onSync);

  await client.maybeSync(2000, serverTime);

  equal(count, 0);
});
add_task(clear_state);

add_task(async function test_all_listeners_are_executed_if_one_fails() {
  const serverTime = Date.now();

  let count = 0;
  client.on("sync", () => { count += 1; });
  client.on("sync", () => { throw new Error("boom"); });
  client.on("sync", () => { count += 2; });

  let error;
  try {
    await client.maybeSync(2000, serverTime);
  } catch (e) {
    error = e;
  }

  equal(count, 3);
  equal(error.message, "boom");
});
add_task(clear_state);

add_task(async function test_telemetry_reports_up_to_date() {
  await client.maybeSync(2000, Date.now() - 1000);
  const serverTime = Date.now();
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  await client.maybeSync(3000, serverTime);

  // No Telemetry was sent.
  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.UP_TO_DATE]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_telemetry_if_sync_succeeds() {
  // We test each client because Telemetry requires preleminary declarations.
  const serverTime = Date.now();
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  await client.maybeSync(2000, serverTime);

  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.SUCCESS]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_telemetry_reports_if_application_fails() {
  const serverTime = Date.now();
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);
  client.on("sync", () => { throw new Error("boom"); });

  try {
    await client.maybeSync(2000, serverTime);
  } catch (e) {}

  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.APPLY_ERROR]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_telemetry_reports_if_sync_fails() {
  const serverTime = Date.now();

  const collection = await client.openCollection();
  await collection.db.saveLastModified(9999);

  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  try {
    await client.maybeSync(10000, serverTime);
  } catch (e) {}

  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.SYNC_ERROR]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_telemetry_reports_unknown_errors() {
  const serverTime = Date.now();
  const backup = client.openCollection;
  client.openCollection = () => { throw new Error("Internal"); };
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  try {
    await client.maybeSync(2000, serverTime);
  } catch (e) {}

  client.openCollection = backup;
  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.UNKNOWN_ERROR]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_bucketname_changes_when_bucket_pref_changes() {
  equal(client.bucketName, "main");

  Services.prefs.setCharPref("services.settings.default_bucket", "main-preview");

  equal(client.bucketName, "main-preview");
});
add_task(clear_state);

add_task(async function test_inspect_changes_the_list_when_bucket_pref_is_changed() {
  // Register a client only listed in -preview...
  RemoteSettings("crash-rate");

  const { collections: before } = await RemoteSettings.inspect();
  deepEqual(before.map(c => c.collection).sort(), ["password-fields"]);

  Services.prefs.setCharPref("services.settings.default_bucket", "main-preview");

  const { collections: after, mainBucket } = await RemoteSettings.inspect();
  deepEqual(after.map(c => c.collection).sort(), ["crash-rate", "password-fields"]);
  equal(mainBucket, "main-preview");
});
add_task(clear_state);

// get a response for a given request from sample data
function getSampleResponse(req, port) {
  const responses = {
    "OPTIONS": {
      "sampleHeaders": [
        "Access-Control-Allow-Headers: Content-Length,Expires,Backoff,Retry-After,Last-Modified,Total-Records,ETag,Pragma,Cache-Control,authorization,content-type,if-none-match,Alert,Next-Page",
        "Access-Control-Allow-Methods: GET,HEAD,OPTIONS,POST,DELETE,OPTIONS",
        "Access-Control-Allow-Origin: *",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": null,
    },
    "GET:/v1/": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": {
        "settings": {
          "batch_max_requests": 25,
        },
        "url": `http://localhost:${port}/v1/`,
        "documentation": "https://kinto.readthedocs.org/",
        "version": "1.5.1",
        "commit": "cbc6f58",
        "hello": "kinto",
      },
    },
    "GET:/v1/buckets/monitor/collections/changes/records": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        `Date: ${new Date().toUTCString()}`,
        "Etag: \"4000\"",
      ],
      "status": { status: 200, statusText: "OK" },
      "responseBody": {
        "data": [{
          "id": "4676f0c7-9757-4796-a0e8-b40a5a37a9c9",
          "bucket": "main",
          "collection": "unknown",
          "last_modified": 4000,
        }, {
          "id": "0af8da0b-3e03-48fb-8d0d-2d8e4cb7514d",
          "bucket": "main",
          "collection": "password-fields",
          "last_modified": 3000,
        }, {
          "id": "4acda969-3bd3-4074-a678-ff311eeb076e",
          "bucket": "main-preview",
          "collection": "password-fields",
          "last_modified": 2000,
        }, {
          "id": "58697bd1-315f-4185-9bee-3371befc2585",
          "bucket": "main-preview",
          "collection": "crash-rate",
          "last_modified": 1000,
        }],
      },
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=2000&_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": {
        "data": [{
          "id": "9d500963-d80e-3a91-6e74-66f3811b99cc",
          "last_modified": 3000,
          "website": "https://some-website.com",
          "selector": "#user[password]",
        }],
      },
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=3001&_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": {
        "data": [{
          "id": "aabad965-e556-ffe7-4191-074f5dee3df3",
          "last_modified": 4000,
          "website": "https://www.other.org/signin",
          "selector": "#signinpassword",
        }, {
          "id": "9d500963-d80e-3a91-6e74-66f3811b99cc",
          "last_modified": 3500,
          "website": "https://some-website.com/login",
          "selector": "input#user[password]",
        }],
      },
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=4001&_sort=-last_modified&_since=4000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"5000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": {
        "data": [{
          "id": "aabad965-e556-ffe7-4191-074f5dee3df3",
          "deleted": true,
        }],
      },
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=10000&_sort=-last_modified&_since=9999": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 503, statusText: "Service Unavailable"},
      "responseBody": {
        code: 503,
        errno: 999,
        error: "Service Unavailable",
      },
    },
  };
  return responses[`${req.method}:${req.path}?${req.queryString}`] ||
         responses[`${req.method}:${req.path}`] ||
         responses[req.method];

}
