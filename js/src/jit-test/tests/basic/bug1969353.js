for (var i of []) {}
oomTest(
  function () {
    eval("(async function*(){})");
  },
  { keepFailing: true },
);
