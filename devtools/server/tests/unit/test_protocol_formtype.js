"use strict";

var protocol = require("devtools/shared/protocol");
var {RetVal} = protocol;

protocol.types.addActorType("child");
protocol.types.addActorType("root");

const childSpec = protocol.generateActorSpec({
  typeName: "child",

  methods: {
    getChild: {
      response: RetVal("child"),
    },
  },
});

// The child actor doesn't provide a form description
var ChildActor = protocol.ActorClassWithSpec(childSpec, {
  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
  },

  form(detail) {
    return {
      actor: this.actorID,
      extra: "extra",
    };
  },

  getChild: function() {
    return this;
  },
});

var ChildFront = protocol.FrontClassWithSpec(childSpec, {
  initialize(client) {
    protocol.Front.prototype.initialize.call(this, client);
  },

  form(v, ctx, detail) {
    this.extra = v.extra;
  },
});

const rootSpec = protocol.generateActorSpec({
  typeName: "root",

  // Basic form type, relies on implicit DictType creation
  formType: {
    childActor: "child",
  },

  // This detail uses explicit DictType creation
  "formType#detail1": protocol.types.addDictType("RootActorFormTypeDetail1", {
    detailItem: "child",
  }),

  // This detail a string type.
  "formType#actorid": "string",

  methods: {
    getDefault: {
      response: RetVal("root"),
    },
    getDetail1: {
      response: RetVal("root#detail1"),
    },
    getDetail2: {
      response: {
        item: RetVal("root#actorid"),
      },
    },
    getUnknownDetail: {
      response: RetVal("root#unknownDetail"),
    },
  },
});

// The root actor does provide a form description.
var RootActor = protocol.ActorClassWithSpec(rootSpec, {
  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.manage(this);
    this.child = new ChildActor();
  },

  sayHello() {
    return {
      from: "root",
      applicationType: "xpcshell-tests",
      traits: [],
    };
  },

  form(detail) {
    if (detail === "detail1") {
      return {
        actor: this.actorID,
        detailItem: this.child,
      };
    } else if (detail === "actorid") {
      return this.actorID;
    }

    return {
      actor: this.actorID,
      childActor: this.child,
    };
  },

  getDefault: function() {
    return this;
  },

  getDetail1: function() {
    return this;
  },

  getDetail2: function() {
    return this;
  },

  getUnknownDetail: function() {
    return this;
  },
});

var RootFront = protocol.FrontClassWithSpec(rootSpec, {
  initialize(client) {
    this.actorID = "root";
    protocol.Front.prototype.initialize.call(this, client);

    // Root owns itself.
    this.manage(this);
  },

  form(v, ctx, detail) {
    this.lastForm = v;
  },
});

const run_test = Test(async function() {
  DebuggerServer.createRootActor = (conn => {
    return RootActor(conn);
  });
  DebuggerServer.init();

  const connection = DebuggerServer.connectPipe();
  const conn = new DebuggerClient(connection);
  const client = Async(conn);

  await client.connect();

  // DebuggerClient.connect() will instantiate the regular RootFront
  // which will override the one we just registered and mess up with this test.
  // So override it again with test one before asserting.
  protocol.types.registeredTypes.get("root").actorSpec = rootSpec;

  const rootFront = RootFront(conn);

  // Trigger some methods that return forms.
  let retval = await rootFront.getDefault();
  Assert.ok(retval instanceof RootFront);
  Assert.ok(rootFront.lastForm.childActor instanceof ChildFront);

  retval = await rootFront.getDetail1();
  Assert.ok(retval instanceof RootFront);
  Assert.ok(rootFront.lastForm.detailItem instanceof ChildFront);

  retval = await rootFront.getDetail2();
  Assert.ok(retval instanceof RootFront);
  Assert.ok(typeof (rootFront.lastForm) === "string");

  // getUnknownDetail should fail, since no typeName is specified.
  try {
    await rootFront.getUnknownDetail();
    Assert.ok(false);
  } catch (ex) {
    // empty
  }

  await client.close();
});
