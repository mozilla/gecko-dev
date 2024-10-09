// Test module fields related to asynchronous evaluation.

{
  let m = parseModule('');
  assertEq(m.status, "Unlinked");

  moduleLink(m);
  assertEq(m.isAsyncEvaluating, false);
  assertEq(m.status, "Linked");

  moduleEvaluate(m);
  assertEq(m.isAsyncEvaluating, false);
  assertEq(m.status, "Evaluated");
}

{
  let m = parseModule('await 1;');

  moduleLink(m);
  assertEq(m.isAsyncEvaluating, false);

  moduleEvaluate(m);
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, "EvaluatingAsync");
  assertEq(m.asyncEvaluatingPostOrder, 1);

  drainJobQueue();
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, "Evaluated");
  assertEq(m.asyncEvaluatingPostOrder, undefined);
}

{
  let m = parseModule('await 1; throw 2;');

  moduleLink(m);
  moduleEvaluate(m).catch(() => 0);
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, "EvaluatingAsync");
  assertEq(m.asyncEvaluatingPostOrder, 1);

  drainJobQueue();
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, "Evaluated");
  assertEq(m.evaluationError, 2);
  assertEq(m.asyncEvaluatingPostOrder, undefined);
}

{
  let m = parseModule('throw 1; await 2;');
  moduleLink(m);
  moduleEvaluate(m).catch(() => 0);
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, "EvaluatingAsync");
  assertEq(m.asyncEvaluatingPostOrder, 1);

  drainJobQueue();
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, "Evaluated");
  assertEq(m.evaluationError, 1);
  assertEq(m.asyncEvaluatingPostOrder, undefined);
}

{
  clearModules();
  let a = registerModule('a', parseModule(''));
  let b = registerModule('b', parseModule('import {} from "a"; await 1;'));

  moduleLink(b);
  moduleEvaluate(b);
  assertEq(a.isAsyncEvaluating, false);
  assertEq(a.status, "Evaluated");
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, "EvaluatingAsync");
  assertEq(b.asyncEvaluatingPostOrder, 1);

  drainJobQueue();
  assertEq(a.isAsyncEvaluating, false);
  assertEq(a.status, "Evaluated");
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, "Evaluated");
  assertEq(b.asyncEvaluatingPostOrder, undefined);
}

{
  clearModules();
  let a = registerModule('a', parseModule('await 1;'));
  let b = registerModule('b', parseModule('import {} from "a";'));

  moduleLink(b);
  moduleEvaluate(b);
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, "EvaluatingAsync");
  assertEq(a.asyncEvaluatingPostOrder, 1);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, "EvaluatingAsync");
  assertEq(b.asyncEvaluatingPostOrder, 2);

  drainJobQueue();
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, "Evaluated");
  assertEq(a.asyncEvaluatingPostOrder, undefined);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, "Evaluated");
  assertEq(b.asyncEvaluatingPostOrder, undefined);
}

{
  clearModules();
  let resolve;
  var promise = new Promise(r => { resolve = r; });
  let a = registerModule('a', parseModule('await promise;'));
  let b = registerModule('b', parseModule('await 2;'));
  let c = registerModule('c', parseModule('import {} from "a"; import {} from "b";'));

  moduleLink(c);
  moduleEvaluate(c);
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, "EvaluatingAsync");
  assertEq(a.asyncEvaluatingPostOrder, 1);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, "EvaluatingAsync");
  assertEq(b.asyncEvaluatingPostOrder, 2);
  assertEq(c.isAsyncEvaluating, true);
  assertEq(c.status, "EvaluatingAsync");
  assertEq(c.asyncEvaluatingPostOrder, 3);

  resolve(1);
  drainJobQueue();
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, "Evaluated");
  assertEq(a.asyncEvaluatingPostOrder, undefined);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, "Evaluated");
  assertEq(b.asyncEvaluatingPostOrder, undefined);
  assertEq(c.isAsyncEvaluating, true);
  assertEq(c.status, "Evaluated");
  assertEq(c.asyncEvaluatingPostOrder, undefined);
}

{
  clearModules();
  let a = registerModule('a', parseModule('throw 1;'));
  let b = registerModule('b', parseModule('import {} from "a"; await 2;'));

  moduleLink(b);
  moduleEvaluate(b).catch(() => 0);
  assertEq(a.status, "Evaluated");
  assertEq(a.isAsyncEvaluating, false);
  assertEq(a.evaluationError, 1);
  assertEq(b.status, "Evaluated");
  assertEq(b.isAsyncEvaluating, false);
  assertEq(b.evaluationError, 1);
}

{
  clearModules();
  let a = registerModule('a', parseModule('throw 1; await 2;'));
  let b = registerModule('b', parseModule('import {} from "a";'));

  moduleLink(b);
  moduleEvaluate(b).catch(() => 0);
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, "EvaluatingAsync");
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, "EvaluatingAsync");

  drainJobQueue();
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, "Evaluated");
  assertEq(a.evaluationError, 1);
  assertEq(a.asyncEvaluatingPostOrder, undefined);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, "Evaluated");
  assertEq(b.evaluationError, 1);
  assertEq(b.asyncEvaluatingPostOrder, undefined);
}

{
  clearModules();
  let a = registerModule('a', parseModule('await 1; throw 2;'));
  let b = registerModule('b', parseModule('import {} from "a";'));

  moduleLink(b);
  moduleEvaluate(b).catch(() => 0);
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, "EvaluatingAsync");
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, "EvaluatingAsync");

  drainJobQueue();
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, "Evaluated");
  assertEq(a.evaluationError, 2);
  assertEq(a.asyncEvaluatingPostOrder, undefined);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, "Evaluated");
  assertEq(b.evaluationError, 2);
  assertEq(b.asyncEvaluatingPostOrder, undefined);
}
