// |jit-test| allow-overrecursed

const arr = [1,2,3,4,5,6,7,8];

function f() {
  let import_str = 'import {} from "module2"; import {} from "module3";';
  let await_str = "await 1;"
  const mod = registerModule("module1", parseModule(import_str));
  registerModule("module2", parseModule(await_str));
  registerModule("module3", parseModule(import_str + await_str));
  moduleLink(mod);
  moduleEvaluate(mod);
  function recurse(a, b) {
    try {
      a(a);
    } catch {
      drainJobQueue();
    }
  }
  const wrapper = wrapWithProto(recurse, {});
  recurse(wrapper);
}
arr.sort(f)
