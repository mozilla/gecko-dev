function getLogString(obj) {
  let log = getWatchtowerLog();
  return log.map(item => {
      assertEq(item.object, obj);
      if (typeof item.extra === "symbol") {
          item.extra = "<symbol>";
      }
      return item.kind + (item.extra ? ": " + item.extra : "");
  }).join("|");
}

function testDefineProperty() {
  let o = {};
  addWatchtowerTarget(o);

  Object.defineProperty(o, "a", {value: 1, configurable: true, writable: true, enumerable: true});
  assertEq(getLogString(o), "add-prop: a");

  // This doesn't change the property's flags so is just a change-prop-value.
  Object.defineProperty(o, "a", {value: 2});
  assertEq(getLogString(o), "change-prop-value: a");

  // This changes the property's flags but not its value so is just a change-prop-flags.
  Object.defineProperty(o, "a", {value: 2, enumerable: false});
  assertEq(getLogString(o), "change-prop-flags: a");

  // This defineProperty is a no-op.
  Object.defineProperty(o, "a", {value: 2, enumerable: false});
  assertEq(getLogString(o), "");

  // This changes both the property's value and its flags.
  Object.defineProperty(o, "a", {value: 1, enumerable: true});
  assertEq(getLogString(o), "change-prop-flags: a|change-prop-value: a");

  // Turning the data property into a getter changes both its (slot) value and its flags.
  let getter = () => 1;
  Object.defineProperty(o, "a", {get: getter});
  assertEq(getLogString(o), "change-prop-flags: a|change-prop-value: a");

  // This defineProperty is a no-op.
  Object.defineProperty(o, "a", {get: getter, enumerable: true});
  assertEq(getLogString(o), "");

  // Changing just the accessor property's flags.
  Object.defineProperty(o, "a", {get: getter, enumerable: false});
  assertEq(getLogString(o), "change-prop-flags: a");

  // Changing the getter function counts as a property modification.
  let getter2 = () => 2;
  Object.defineProperty(o, "a", {get: getter2});
  assertEq(getLogString(o), "change-prop-value: a");

  // Changing both the property's accessors and its flags.
  Object.defineProperty(o, "a", {set: getter, enumerable: true});
  assertEq(getLogString(o), "change-prop-flags: a|change-prop-value: a");

  // Change back to a data property.
  Object.defineProperty(o, "a", {value: 1});
  assertEq(getLogString(o), "change-prop-flags: a|change-prop-value: a");
}

for (var i = 0; i < 20; i++) {
  testDefineProperty();
}
