/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SCHEDULED_BACKUPS_ENABLED_PREF_NAME = "browser.backup.scheduled.enabled";
const IDLE_THRESHOLD_SECONDS_PREF_NAME =
  "browser.backup.scheduled.idle-threshold-seconds";
const LAST_BACKUP_TIMESTAMP_PREF_NAME =
  "browser.backup.scheduled.last-backup-timestamp";
const MINIMUM_TIME_BETWEEN_BACKUPS_SECONDS_PREF_NAME =
  "browser.backup.scheduled.minimum-time-between-backups-seconds";

/**
 * This is a very thin nsIUserIdleService implementation that doesn't do much,
 * but with sinon we can stub out some parts of it to make sure that the
 * BackupService uses it in the way we expect.
 */
let idleService = {
  QueryInterface: ChromeUtils.generateQI(["nsIUserIdleService"]),
  idleTime: 19999,
  disabled: true,
  addIdleObserver() {},
  removeIdleObserver() {},
};

add_setup(() => {
  let fakeIdleServiceCID = MockRegistrar.register(
    "@mozilla.org/widget/useridleservice;1",
    idleService
  );

  Services.prefs.setBoolPref(SCHEDULED_BACKUPS_ENABLED_PREF_NAME, true);

  // We'll pretend that our threshold between backups is 20 seconds.
  Services.prefs.setIntPref(MINIMUM_TIME_BETWEEN_BACKUPS_SECONDS_PREF_NAME, 20);

  registerCleanupFunction(() => {
    MockRegistrar.unregister(fakeIdleServiceCID);
    Services.prefs.clearUserPref(SCHEDULED_BACKUPS_ENABLED_PREF_NAME);
    Services.prefs.clearUserPref(
      MINIMUM_TIME_BETWEEN_BACKUPS_SECONDS_PREF_NAME
    );
  });
});

/**
 * Tests that calling initBackupScheduler registers a callback with the
 * nsIUserIdleService.
 */
add_task(async function test_init_uninitBackupScheduler() {
  let bs = new BackupService();
  let sandbox = sinon.createSandbox();
  sandbox.stub(idleService, "addIdleObserver");
  sandbox.stub(idleService, "removeIdleObserver");

  await bs.initBackupScheduler();
  Assert.ok(
    idleService.addIdleObserver.calledOnce,
    "addIdleObserver was called"
  );
  Assert.ok(
    idleService.addIdleObserver.firstCall.args[0] instanceof Ci.nsIObserver,
    "The first argument to addIdleObserver was an nsIObserver"
  );
  const THRESHOLD_SECONDS = Services.prefs.getIntPref(
    IDLE_THRESHOLD_SECONDS_PREF_NAME
  );
  Assert.equal(
    idleService.addIdleObserver.firstCall.args[1],
    THRESHOLD_SECONDS,
    "The idle threshold preference value was passed as the second argument."
  );
  Assert.ok(
    idleService.removeIdleObserver.notCalled,
    "removeIdleObserver has not been called yet."
  );

  // Hold a reference to what addIdleObserver was called with as its first
  // argument, so we can compare it against what's passed to removeIdleObserver.
  let addObserverArg = idleService.addIdleObserver.firstCall.args[0];

  // We want to make sure that uninitBackupScheduler doesn't call this again,
  // so reset its call history.
  idleService.addIdleObserver.resetHistory();

  // Now, let's pretend that the preference for the idle threshold changed
  // before we could uninit the backup scheduler. We should ensure that this
  // change is _not_ reflected whenever deregistration of the idle callback
  // occurs, since it wouldn't match the registration arguments.
  Services.prefs.setIntPref(
    IDLE_THRESHOLD_SECONDS_PREF_NAME,
    THRESHOLD_SECONDS + 5
  );

  bs.uninitBackupScheduler();
  Assert.ok(
    idleService.addIdleObserver.notCalled,
    "addIdleObserver was not called again."
  );
  Assert.ok(
    idleService.removeIdleObserver.calledOnce,
    "removeIdleObserver was called once."
  );
  Assert.ok(
    idleService.removeIdleObserver.firstCall.args[0] instanceof Ci.nsIObserver,
    "The first argument to addIdleObserver was an nsIObserver"
  );
  Assert.equal(
    idleService.removeIdleObserver.firstCall.args[0],
    addObserverArg,
    "The first argument to addIdleObserver matches the first argument to removeIdleObserver"
  );
  Assert.equal(
    idleService.removeIdleObserver.firstCall.args[1],
    THRESHOLD_SECONDS,
    "The original idle threshold preference value was passed as the second argument."
  );

  sandbox.restore();
  Services.prefs.clearUserPref(IDLE_THRESHOLD_SECONDS_PREF_NAME);
});

/**
 * Tests that calling BackupService.onObserve with the "idle" notification
 * causes the BackupService.onIdle method to be called.
 */
add_task(async function test_BackupService_onObserve_idle() {
  let bs = new BackupService();
  let sandbox = sinon.createSandbox();
  sandbox.stub(bs, "onIdle");

  // The subject for the idle notification is always the idle service itself.
  bs.onObserve(idleService, "idle");
  Assert.ok(bs.onIdle.calledOnce, "BackupService.onIdle was called.");

  sandbox.restore();
});

/**
 * Tests that calling BackupService.onObserve with the
 * "quit-application-granted" notification causes the
 * BackupService.uninitBackupScheduler method to be called.
 */
add_task(
  async function test_BackupService_onObserve_quit_application_granted() {
    let bs = new BackupService();
    let sandbox = sinon.createSandbox();
    sandbox.stub(bs, "uninitBackupScheduler");

    // The subject for the quit-application-granted notification is null.
    bs.onObserve(null, "quit-application-granted");
    Assert.ok(
      bs.uninitBackupScheduler.calledOnce,
      "BackupService.uninitBackupScheduler was called."
    );

    sandbox.restore();
  }
);

/**
 * Tests that calling onIdle when a backup has never occurred causes a backup to
 * get scheduled.
 */
add_task(async function test_BackupService_idle_no_backup_exists() {
  // Make sure no last backup timestamp is recorded.
  Services.prefs.clearUserPref(LAST_BACKUP_TIMESTAMP_PREF_NAME);

  let bs = new BackupService();
  let sandbox = sinon.createSandbox();
  sandbox.stub(bs, "createBackupOnIdleDispatch");

  bs.initBackupScheduler();
  Assert.equal(
    bs.state.lastBackupDate,
    null,
    "State should have null for lastBackupDate"
  );

  bs.onIdle();
  Assert.ok(
    bs.createBackupOnIdleDispatch.calledOnce,
    "BackupService.createBackupOnIdleDispatch was called."
  );

  sandbox.restore();
});

/**
 * Tests that calling onIdle when a backup has occurred recently does not cause
 * a backup to get scheduled.
 */
add_task(async function test_BackupService_idle_not_expired_backup() {
  // Let's calculate a Date that's five seconds ago.
  let fiveSecondsAgo = Date.now() - 5000; /* 5 seconds in milliseconds */
  let lastBackupPrefValue = Math.floor(fiveSecondsAgo / 1000);
  Services.prefs.setIntPref(
    LAST_BACKUP_TIMESTAMP_PREF_NAME,
    lastBackupPrefValue
  );

  let bs = new BackupService();
  let sandbox = sinon.createSandbox();
  bs.initBackupScheduler();
  Assert.equal(
    bs.state.lastBackupDate,
    lastBackupPrefValue,
    "State should have cached lastBackupDate"
  );

  sandbox.stub(bs, "createBackupOnIdleDispatch");

  bs.onIdle();
  Assert.ok(
    bs.createBackupOnIdleDispatch.notCalled,
    "BackupService.createBackupOnIdleDispatch was not called."
  );

  sandbox.restore();
});

/**
 * Tests that calling onIdle when a backup has occurred, but after the threshold
 * does cause a backup to get scheduled
 */
add_task(async function test_BackupService_idle_expired_backup() {
  // Let's calculate a Date that's twenty five seconds ago.
  let twentyFiveSecondsAgo =
    Date.now() - 25000; /* 25 seconds in milliseconds */
  let lastBackupPrefValue = Math.floor(twentyFiveSecondsAgo / 1000);

  Services.prefs.setIntPref(
    LAST_BACKUP_TIMESTAMP_PREF_NAME,
    lastBackupPrefValue
  );

  let bs = new BackupService();
  let sandbox = sinon.createSandbox();
  bs.initBackupScheduler();
  Assert.equal(
    bs.state.lastBackupDate,
    lastBackupPrefValue,
    "State should have cached lastBackupDate"
  );

  sandbox.stub(bs, "createBackupOnIdleDispatch");

  bs.onIdle();
  Assert.ok(
    bs.createBackupOnIdleDispatch.calledOnce,
    "BackupService.createBackupOnIdleDispatch was called."
  );

  sandbox.restore();
});

/**
 * Tests that calling onIdle when a backup occurred in the future somehow causes
 * a backup to get scheduled.
 */
add_task(async function test_BackupService_idle_time_travel() {
  // Let's calculate a Date that's twenty-five seconds in the future.
  let twentyFiveSecondsFromNow =
    Date.now() + 25000; /* 25 seconds in milliseconds */
  let lastBackupPrefValue = Math.floor(twentyFiveSecondsFromNow / 1000);

  Services.prefs.setIntPref(
    LAST_BACKUP_TIMESTAMP_PREF_NAME,
    lastBackupPrefValue
  );

  let bs = new BackupService();
  let sandbox = sinon.createSandbox();
  bs.initBackupScheduler();
  Assert.equal(
    bs.state.lastBackupDate,
    lastBackupPrefValue,
    "State should have cached lastBackupDate"
  );

  sandbox.stub(bs, "createBackupOnIdleDispatch");

  bs.onIdle();
  Assert.ok(
    bs.createBackupOnIdleDispatch.calledOnce,
    "BackupService.createBackupOnIdleDispatch was called."
  );
  Assert.equal(
    bs.state.lastBackupDate,
    null,
    "Should have cleared the last backup date."
  );

  sandbox.restore();
});
