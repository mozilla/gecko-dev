let { MemoryNotificationDB } = ChromeUtils.importESModule(
  "resource://gre/modules/MemoryNotificationDB.sys.mjs"
);

add_task(async function test_delete_all() {
  const db = new MemoryNotificationDB();

  // Save three notifications
  const origin = "https://example.com";
  const scope = "https://example.com/";
  await db.taskSave({
    origin,
    notification: {
      id: "foo",
      tag: "foo",
      serviceWorkerRegistrationScope: scope,
    },
  });
  await db.taskSave({
    origin,
    notification: {
      id: "bar",
      tag: "bar",
      serviceWorkerRegistrationScope: scope,
    },
  });
  await db.taskSave({
    origin,
    notification: {
      id: "baz",
      tag: "baz",
      serviceWorkerRegistrationScope: scope,
    },
  });

  // And also one with different origin
  const origin2 = "https://example.org";
  const scope2 = "https://example.org/";
  await db.taskSave({
    origin: origin2,
    notification: {
      id: "foo2",
      tag: "foo",
      serviceWorkerRegistrationScope: scope2,
    },
  });

  // We should have three notifications
  let notifications = await db.taskGetAll({ origin, scope });
  Assert.equal(notifications.length, 3, "Should get three notifications");
  Assert.deepEqual(
    notifications.map(n => n.id),
    ["foo", "bar", "baz"],
    "Should have three different notifications"
  );

  // By tag
  notifications = await db.taskGetAll({ origin, scope, tag: "foo" });
  Assert.equal(notifications.length, 1, "Should get one notification");
  Assert.equal(notifications[0].id, "foo", "Should get the expected ID");

  // By tag for different origin
  notifications = await db.taskGetAll({
    origin: origin2,
    scope: scope2,
    tag: "foo",
  });
  Assert.equal(notifications.length, 1, "Should get one notification");
  Assert.equal(notifications[0].id, "foo2", "Should get the expected ID");

  // Now remove all except bar/baz, meaning foo should be removed
  await db.taskDeleteAllExcept({ ids: ["bar", "baz"] });
  notifications = await db.taskGetAll({ origin, scope });
  Assert.equal(notifications.length, 2, "Should get two notifications");
  Assert.deepEqual(
    notifications.map(n => n.id),
    ["bar", "baz"],
    "Should have two different notifications"
  );

  // Bar should be removed from tag too
  notifications = await db.taskGetAll({ origin, scope, tag: "foo" });
  Assert.equal(notifications.length, 0, "Should get zero notification");

  // Now remove everything
  await db.taskDeleteAllExcept({ ids: [] });
  notifications = await db.taskGetAll({ origin, scope });
  Assert.equal(notifications.length, 0, "Should get zero notification");
  notifications = await db.taskGetAll({ origin, scope, tag: "bar" });
  Assert.equal(notifications.length, 0, "Should get zero notification");
  notifications = await db.taskGetAll({ origin: origin2, scope: scope2 });
  Assert.equal(notifications.length, 0, "Should get zero notification");
  notifications = await db.taskGetAll({
    origin: origin2,
    scope: scope2,
    tag: "foo",
  });
  Assert.equal(notifications.length, 0, "Should get zero notification");
});
