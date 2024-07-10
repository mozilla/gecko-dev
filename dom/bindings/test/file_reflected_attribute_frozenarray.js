function checkEquals(value, expected, valueCheckFn = (a, b) => a == b) {
  if (!valueCheckFn(value, expected)) {
    return `, got ${value}, expected ${expected}`;
  }
  return undefined;
}

function checkReflectedAttributeWithFrozenArrayValues(obj, values, valueCheck) {
  if (!SimpleTest.isa(obj.reflectedHTMLAttribute, "Array")) {
    return `, expected array`;
  }
  let failure = checkEquals(obj.reflectedHTMLAttribute.length, values.length);
  if (!failure) {
    for (let [i, v] of obj.reflectedHTMLAttribute.entries()) {
      failure = checkEquals(values[i], v, valueCheck);
      if (failure) {
        break;
      }
    }
  }
  return failure;
}

function checkReflectedAttributeWithFrozenArray(
  obj,
  values,
  suffix,
  valueCheck
) {
  let failure = checkReflectedAttributeWithFrozenArrayValues(
    obj,
    values,
    valueCheck
  );
  ok(
    !failure,
    `Cached value on object for HTML reflected FrozenArray attribute should contain the right values ${suffix}${
      failure || ""
    }`
  );
}

function testReflectedAttributeWithFrozenArray(win) {
  let testObject = new win.TestReflectedHTMLAttribute();
  ok(
    testObject instanceof win.TestReflectedHTMLAttribute,
    "Got a TestReflectedHTMLAttribute object"
  );

  is(
    testObject.reflectedHTMLAttribute,
    null,
    "Initial value for HTML reflected FrozenArray attribute should be null"
  );

  let values = [win.document.head];
  testObject.setReflectedHTMLAttributeValue(values);
  checkReflectedAttributeWithFrozenArray(testObject, values, "after setting");

  values = [win.document.body, win.document.body.firstElementChild];
  testObject.setReflectedHTMLAttributeValue(values);
  checkReflectedAttributeWithFrozenArray(testObject, values, "after resetting");

  // Use a loop to ensure the JITs optimize the getter access.
  let failure;
  for (let i = 0; i < 10_000; i++) {
    failure = checkReflectedAttributeWithFrozenArrayValues(testObject, values);
    if (!failure) {
      break;
    }
    if (i == 9_990) {
      values = [win.document.head];
      testObject.setReflectedHTMLAttributeValue(values);
    }
  }
  ok(
    !failure,
    `Shouldn't use the cached value for HTML reflected FrozenArray attribute directly from JITted code${
      failure || ""
    }`
  );

  is(
    testObject.reflectedHTMLAttribute,
    testObject.reflectedHTMLAttribute,
    "Getter for HTML reflected FrozenArray attribute should return the cached value"
  );

  return [testObject, values];
}
