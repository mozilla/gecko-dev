import { ASRouterStorage } from "modules/ASRouterStorage.sys.mjs";
import { GlobalOverrider } from "tests/unit/utils";

let overrider = new GlobalOverrider();

describe("ASRouterStorage", () => {
  let sandbox;
  let indexedDB;
  let storage;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    indexedDB = {
      open: sandbox.stub().resolves({}),
      deleteDatabase: sandbox.stub().resolves(),
    };
    overrider.set({ IndexedDB: indexedDB });
    storage = new ASRouterStorage({
      storeNames: ["storage_test"],
      telemetry: { handleUndesiredEvent: sandbox.stub() },
    });
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should throw if required arguments not provided", () => {
    assert.throws(() => new ASRouterStorage({ telemetry: true }));
  });
  describe(".db", () => {
    it("should not throw an error when accessing db", async () => {
      assert.ok(storage.db);
    });

    it("should delete and recreate the db if opening db fails", async () => {
      const newDb = {};
      indexedDB.open.onFirstCall().rejects(new Error("fake error"));
      indexedDB.open.onSecondCall().resolves(newDb);

      const db = await storage.db;
      assert.calledOnce(indexedDB.deleteDatabase);
      assert.calledTwice(indexedDB.open);
      assert.equal(db, newDb);
    });
  });
  describe("#getDbTable", () => {
    let testStorage;
    let storeStub;
    beforeEach(() => {
      storeStub = {
        getAll: sandbox.stub().resolves(),
        getAllKeys: sandbox.stub().resolves(),
        get: sandbox.stub().resolves(),
        put: sandbox.stub().resolves(),
      };
      sandbox.stub(storage, "_getStore").resolves(storeStub);
      testStorage = storage.getDbTable("storage_test");
    });
    it("should reverse key value parameters for put", async () => {
      await testStorage.set("key", "value");

      assert.calledOnce(storeStub.put);
      assert.calledWith(storeStub.put, "value", "key");
    });
    it("should return the correct value for get", async () => {
      storeStub.get.withArgs("foo").resolves("foo");

      const result = await testStorage.get("foo");

      assert.calledOnce(storeStub.get);
      assert.equal(result, "foo");
    });
    it("should return the correct value for getAll", async () => {
      storeStub.getAll.resolves(["bar"]);

      const result = await testStorage.getAll();

      assert.calledOnce(storeStub.getAll);
      assert.deepEqual(result, ["bar"]);
    });
    it("should return the correct value for getAllKeys", async () => {
      storeStub.getAllKeys.resolves(["key1", "key2", "key3"]);

      const result = await testStorage.getAllKeys();

      assert.calledOnce(storeStub.getAllKeys);
      assert.deepEqual(result, ["key1", "key2", "key3"]);
    });
    it("should query the correct object store", async () => {
      await testStorage.get();

      assert.calledOnce(storage._getStore);
      assert.calledWithExactly(storage._getStore, "storage_test");
    });
    it("should throw if table is not found", () => {
      assert.throws(() => storage.getDbTable("undefined_store"));
    });
  });
  it("should get the correct objectStore when calling _getStore", async () => {
    const objectStoreStub = sandbox.stub();
    indexedDB.open.resolves({ objectStore: objectStoreStub });

    await storage._getStore("foo");

    assert.calledOnce(objectStoreStub);
    assert.calledWithExactly(objectStoreStub, "foo", "readwrite");
  });
  it("should create a db with the correct store name", async () => {
    const dbStub = {
      createObjectStore: sandbox.stub(),
      objectStoreNames: { contains: sandbox.stub().returns(false) },
    };
    await storage.db;

    // call the cb with a stub
    indexedDB.open.args[0][2](dbStub);

    assert.calledOnce(dbStub.createObjectStore);
    assert.calledWithExactly(dbStub.createObjectStore, "storage_test");
  });
  it("should handle an array of object store names", async () => {
    storage = new ASRouterStorage({
      storeNames: ["store1", "store2"],
      telemetry: {},
    });
    const dbStub = {
      createObjectStore: sandbox.stub(),
      objectStoreNames: { contains: sandbox.stub().returns(false) },
    };
    await storage.db;

    // call the cb with a stub
    indexedDB.open.args[0][2](dbStub);

    assert.calledTwice(dbStub.createObjectStore);
    assert.calledWith(dbStub.createObjectStore, "store1");
    assert.calledWith(dbStub.createObjectStore, "store2");
  });
  it("should skip creating existing stores", async () => {
    storage = new ASRouterStorage({
      storeNames: ["store1", "store2"],
      telemetry: {},
    });
    const dbStub = {
      createObjectStore: sandbox.stub(),
      objectStoreNames: { contains: sandbox.stub().returns(true) },
    };
    await storage.db;

    // call the cb with a stub
    indexedDB.open.args[0][2](dbStub);

    assert.notCalled(dbStub.createObjectStore);
  });
  describe("#_requestWrapper", () => {
    it("should return a successful result", async () => {
      const result = await storage._requestWrapper(() =>
        Promise.resolve("foo")
      );

      assert.equal(result, "foo");
      assert.notCalled(storage.telemetry.handleUndesiredEvent);
    });
    it("should report failures", async () => {
      try {
        await storage._requestWrapper(() => Promise.reject(new Error()));
      } catch (e) {
        assert.calledOnce(storage.telemetry.handleUndesiredEvent);
      }
    });
  });
});

function assertMessageBlockedTransaction(mockConnection, expectedMessageId) {
  assert.callCount(mockConnection.executeCached, 2);

  const [call1, call2] = [
    mockConnection.executeCached.getCall(0),
    mockConnection.executeCached.getCall(1),
  ];

  assert.match(call1.args[0], /INSERT INTO MessagingSystemMessageBlocklist/);
  assert.match(call2.args[0], /DELETE FROM MessagingSystemMessageImpressions/);

  assert.deepEqual(call1.args[1], { messageId: expectedMessageId });
  assert.deepEqual(call2.args[1], { messageId: expectedMessageId });
}

describe("Shared database methods", () => {
  let sandbox;
  let mockConnection;
  let storage;
  let errorStub;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    errorStub = sandbox.stub();

    storage = new ASRouterStorage({
      storeNames: ["storage_test"],
      telemetry: { handleUndesiredEvent: sandbox.stub() },
    });

    mockConnection = {
      executeCached: sandbox.stub(),
      executeBeforeShutdown: sandbox.stub(),
      executeTransaction: sandbox.stub(),
    };

    mockConnection.executeBeforeShutdown.callsFake(async (label, fn) => {
      return await fn();
    });
    mockConnection.executeTransaction.callsFake(async fn => {
      return await fn();
    });

    overrider.set({
      ASRouterPreferences: {
        console: {
          error: errorStub,
        },
      },
      IndexedDB: indexedDB,
      ProfilesDatastoreService: {
        getConnection: sandbox.stub().resolves(mockConnection),
        notify: sandbox.stub(),
      },
    });
  });

  afterEach(() => {
    sandbox.restore();
    overrider.restore();
  });

  describe("#getSharedMessageImpressions", () => {
    it("should return message impressions data when records exist", async () => {
      const mockRows = [
        {
          getResultByName: columnName => {
            if (columnName === "messageId") {
              return "message1";
            }
            if (columnName === "impressions") {
              return JSON.stringify([123, 456]);
            }
            return null;
          },
        },
        {
          getResultByName: columnName => {
            if (columnName === "messageId") {
              return "message2";
            }
            if (columnName === "impressions") {
              return JSON.stringify([123, 456, 789]);
            }
            return null;
          },
        },
      ];

      mockConnection.executeCached.resolves(mockRows);

      const result = await storage.getSharedMessageImpressions();

      // Execute should be called with expected SQL
      assert.calledOnce(mockConnection.executeCached);
      assert.calledWith(
        mockConnection.executeCached,
        "SELECT messageId, json(impressions) AS impressions FROM MessagingSystemMessageImpressions;"
      );

      assert.deepEqual(result, {
        message1: [123, 456],
        message2: [123, 456, 789],
      });
    });

    it("should return null when no records exist", async () => {
      mockConnection.executeCached.resolves([]);

      const result = await storage.getSharedMessageImpressions();

      assert.ok(result === null);
    });

    it("should handle database errors and call telemetry", async () => {
      const error = new Error("Database connection failed");
      mockConnection.executeCached.rejects(error);

      const result = await storage.getSharedMessageImpressions();

      assert.equal(result, null);
      assert.calledOnce(errorStub);
      assert.calledWith(
        errorStub,
        "ASRouterStorage: Failed reading from MessagingSystemMessageImpressions",
        error
      );
      assert.calledOnce(storage.telemetry.handleUndesiredEvent);
      assert.calledWith(storage.telemetry.handleUndesiredEvent, {
        event: "SHARED_DB_READ_FAILED",
      });
    });
  });

  describe("#setSharedMessageImpressions", () => {
    it("should return a true success state when the transaction succeeds", async () => {
      mockConnection.executeCached.resolves();

      const result = await storage.setSharedMessageImpressions(
        "test_message",
        [123, 456]
      );

      assert.equal(result, true);
      assert.calledOnce(mockConnection.executeBeforeShutdown);
      assert.calledWith(
        mockConnection.executeBeforeShutdown,
        "ASRouter: setSharedMessageImpressions"
      );
    });

    it("should return a false success state when the transaction fails", async () => {
      const error = new Error("Database write failed");
      mockConnection.executeCached.rejects(error);

      const result = await storage.setSharedMessageImpressions(
        "test_message",
        [123, 456]
      );

      assert.equal(result, false);
      assert.calledOnce(errorStub);
      assert.calledWith(
        errorStub,
        "ASRouterStorage: Failed writing to MessagingSystemMessageImpressions",
        error
      );
      assert.calledOnce(storage.telemetry.handleUndesiredEvent);
      assert.calledWith(storage.telemetry.handleUndesiredEvent, {
        event: "SHARED_DB_WRITE_FAILED",
      });
    });

    it("should gracefully fail when called with no message ID", async () => {
      const result = await storage.setSharedMessageImpressions(
        null,
        [123, 456]
      );

      assert.equal(result, false);
      assert.calledOnce(errorStub);
      assert.calledOnce(storage.telemetry.handleUndesiredEvent);
      assert.calledWith(storage.telemetry.handleUndesiredEvent, {
        event: "SHARED_DB_WRITE_FAILED",
      });
    });

    it("should execute the appropriate sql query with the appropriate message ID", async () => {
      mockConnection.executeCached.resolves();

      await storage.setSharedMessageImpressions("test_message", [123, 456]);

      assert.calledOnce(mockConnection.executeCached);
      const executeCall = mockConnection.executeCached.getCall(0);

      assert.match(
        executeCall.args[0],
        /INSERT INTO MessagingSystemMessageImpressions/
      );
      assert.match(executeCall.args[0], /ON CONFLICT \(messageId\) DO UPDATE/);

      assert.deepEqual(executeCall.args[1], {
        messageId: "test_message",
        impressions: JSON.stringify([123, 456]),
      });
    });

    it("should delete the record when impressions is falsy or an empty array", async () => {
      mockConnection.executeCached.resolves();

      await storage.setSharedMessageImpressions("test_message", []);

      assert.calledOnce(mockConnection.executeCached);
      let executeCall = mockConnection.executeCached.getCall(0);
      assert.match(
        executeCall.args[0],
        /DELETE FROM MessagingSystemMessageImpressions/
      );
      assert.deepEqual(executeCall.args[1], { messageId: "test_message" });

      // Reset and test with null
      mockConnection.executeCached.resetHistory();
      mockConnection.executeBeforeShutdown.resetHistory();
      await storage.setSharedMessageImpressions("test_message", null);

      assert.calledOnce(mockConnection.executeCached);
      executeCall = mockConnection.executeCached.getCall(0);
      assert.match(
        executeCall.args[0],
        /DELETE FROM MessagingSystemMessageImpressions/
      );
    });

    it("should call ProfilesDatastoreService.notify() after successful operation", async () => {
      mockConnection.executeCached.resolves();

      const notifySpy = sandbox.spy();

      overrider.set({
        ProfilesDatastoreService: {
          getConnection: sandbox.stub().resolves(mockConnection),
          notify: notifySpy,
        },
      });

      await storage.setSharedMessageImpressions("test_message", [123]);
      assert.calledOnce(notifySpy);
    });
  });

  describe("#getSharedMessageBlocklist", () => {
    it("should return array of blocked message IDs when records exist", async () => {
      const mockRows = [
        {
          getResultByName: columnName => {
            if (columnName === "messageId") {
              return "blocked_message1";
            }
            return null;
          },
        },
        {
          getResultByName: columnName => {
            if (columnName === "messageId") {
              return "blocked_message2";
            }
            return null;
          },
        },
      ];

      mockConnection.executeCached.resolves(mockRows);

      const result = await storage.getSharedMessageBlocklist();

      // Execute should be called with expected SQL
      assert.calledOnce(mockConnection.executeCached);
      assert.calledWith(
        mockConnection.executeCached,
        "SELECT messageId FROM MessagingSystemMessageBlocklist;"
      );

      assert.deepEqual(result, ["blocked_message1", "blocked_message2"]);
    });

    it("should return empty array when no blocked messages exist", async () => {
      mockConnection.executeCached.resolves([]);

      const result = await storage.getSharedMessageBlocklist();

      assert.deepEqual(result, []);
    });

    it("should handle database errors and return null", async () => {
      const error = new Error("Database connection failed");
      mockConnection.executeCached.rejects(error);

      const result = await storage.getSharedMessageBlocklist();

      assert.equal(result, null);
      assert.calledOnce(errorStub);
      assert.calledWith(
        errorStub,
        "ASRouterStorage: Failed reading from MessagingSystemMessageBlocklist",
        error
      );

      assert.calledOnce(storage.telemetry.handleUndesiredEvent);
      assert.calledWith(storage.telemetry.handleUndesiredEvent, {
        event: "SHARED_DB_READ_FAILED",
      });
    });
  });

  describe("#setSharedMessageBlocked", () => {
    it("should return true success state when blocking a message succeeds", async () => {
      mockConnection.executeCached.resolves();

      const result = await storage.setSharedMessageBlocked(
        "test_message",
        true
      );

      assert.equal(result, true);
      assert.calledOnce(mockConnection.executeTransaction);
    });

    it("should return false success state when the transaction fails", async () => {
      const error = new Error("Database write failed");
      mockConnection.executeCached
        .withArgs(sinon.match(/INSERT/))
        .rejects(error);

      const result = await storage.setSharedMessageBlocked(
        "test_message",
        true
      );

      assert.equal(result, false);
      assert.calledOnce(errorStub);
      assert.calledWith(
        errorStub,
        "ASRouterStorage: Failed writing to MessagingSystemMessageBlocklist",
        error
      );
      assert.calledOnce(storage.telemetry.handleUndesiredEvent);
      assert.calledWith(storage.telemetry.handleUndesiredEvent, {
        event: "SHARED_DB_WRITE_FAILED",
      });
    });

    it("should execute INSERT and DELETE queries in a transaction when blocking a message", async () => {
      const messageId = "test_blocking_message";
      mockConnection.executeCached.resolves();
      await storage.setSharedMessageBlocked(messageId, true);

      assertMessageBlockedTransaction(mockConnection, messageId);
    });

    it("should execute DELETE query when unblocking a message", async () => {
      mockConnection.executeCached.resolves();

      await storage.setSharedMessageBlocked("test_unblocking_message", false);

      assert.calledOnce(mockConnection.executeCached);
      const executeCall = mockConnection.executeCached.getCall(0);
      assert.match(
        executeCall.args[0],
        /DELETE FROM MessagingSystemMessageBlocklist/
      );
      assert.deepEqual(executeCall.args[1], {
        messageId: "test_unblocking_message",
      });
    });

    it("should default to blocking when isBlocked parameter is not provided", async () => {
      const messageId = "default_blocked_message";
      mockConnection.executeCached.resolves();

      await storage.setSharedMessageBlocked(messageId);

      assertMessageBlockedTransaction(mockConnection, messageId);
    });

    it("should call ProfilesDatastoreService.notify() after successful operation", async () => {
      mockConnection.executeCached.resolves();

      const notifySpy = sandbox.spy();

      overrider.set({
        ProfilesDatastoreService: {
          getConnection: sandbox.stub().resolves(mockConnection),
          notify: notifySpy,
        },
      });

      await storage.setSharedMessageBlocked("test_message", true);
      assert.calledOnce(notifySpy);
    });
  });
});
