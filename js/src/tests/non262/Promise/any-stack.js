// |reftest| skip-if(!xulRuntime.shell) -- needs async stack capture
// SKIP test262 export
// File name

function toMessage(stack) {
  // Provide the stack string in the error message for debugging.
  return `[stack: ${stack.replace(/\n/g, "\\n")}]`;
}

// Test when AggregateError isn't created from a Promise Job.
{
  let p = Promise.any([]); // line 12

  p.then(v => {
    reportCompare(0, 1, "expected error");
  }, e => {
    assertEq(e.name, "AggregateError");
    var {stack} = e;

    assertEq(/^@.+any-stack.js:12/m.test(stack), true, toMessage(stack));
  });
}

// Same as above, but now with surrounding function context.
function testNoJobQueue() {
  let p = Promise.any([]); // line 26

  p.then(v => {
    reportCompare(0, 1, "expected error");
  }, e => {
    assertEq(e.name, "AggregateError");
    var {stack} = e;

    assertEq(/^testNoJobQueue@.+any-stack.js:26/m.test(stack), true, toMessage(stack));
  });
}
testNoJobQueue();

// Test when AggregateError is created from a Promise Job.
{
  let rejected = Promise.reject(0);
  let p = Promise.any([rejected]); // line 42

  p.then(v => {
    reportCompare(0, 1, "expected error");
  }, e => {
    assertEq(e.name, "AggregateError");
    var {stack} = e;

    assertEq(/^Promise.any\*@.+any-stack.js:42/m.test(stack), true, toMessage(stack));
  });
}

// Same as above, but now with surrounding function context.
function testFromJobQueue() {
  let rejected = Promise.reject(0);
  let p = Promise.any([rejected]); // line 57

  p.then(v => {
    reportCompare(0, 1, "expected error");
  }, e => {
    assertEq(e.name, "AggregateError");
    var {stack} = e;

    assertEq(/^Promise.any\*testFromJobQueue@.+any-stack.js:57/m.test(stack), true, toMessage(stack));
  });
}
testFromJobQueue();

if (typeof reportCompare === "function")
  reportCompare(0, 0);
