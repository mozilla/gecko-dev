"use strict";

function isEvalAllowed(sandbox) {
  try {
    Cu.evalInSandbox("eval('1234')", sandbox);
    return true;
  } catch (e) {
    Assert.equal(e.message, "call to eval() blocked by CSP", "Eval error msg");
    return false;
  }
}

add_task(function test_empty_csp() {
  let sand = Cu.Sandbox(["http://example.com/"], {
    sandboxContentSecurityPolicy: "",
  });
  Assert.ok(isEvalAllowed(sand), "eval() not blocked with empty CSP string");
});

add_task(function test_undefined_csp() {
  let sand = Cu.Sandbox(["http://example.com/"], {
    sandboxContentSecurityPolicy: undefined,
  });
  Assert.ok(isEvalAllowed(sand), "eval() not blocked with undefined CSP");
});

add_task(function test_malformed_csp() {
  let sand = Cu.Sandbox(["http://example.com/"], {
    sandboxContentSecurityPolicy: "This is not a valid CSP value",
  });
  Assert.ok(isEvalAllowed(sand), "eval() not blocked with undefined CSP");
});

add_task(function test_allowed_by_sandboxContentSecurityPolicy() {
  let sand = Cu.Sandbox(["http://example.com/"], {
    sandboxContentSecurityPolicy: "script-src 'unsafe-eval';",
  });
  Assert.ok(isEvalAllowed(sand), "eval() allowed by 'unsafe-eval' CSP");
});

add_task(function test_blocked_by_sandboxContentSecurityPolicy() {
  let sand = Cu.Sandbox(["http://example.com/"], {
    sandboxContentSecurityPolicy: "script-src 'none';",
  });

  // Until bug 1548468 is fixed, CSP only works with an ExpandedPrincipal.
  Assert.ok(Cu.getObjectPrincipal(sand).isExpandedPrincipal, "Exp principal");

  Assert.ok(!isEvalAllowed(sand), "eval() should be blocked by CSP");
  // sandbox.eval is also blocked: callers should use Cu.evalInSandbox instead.
  Assert.throws(
    () => sand.eval("123"),
    /EvalError: call to eval\(\) blocked by CSP/,
    "sandbox.eval() is also blocked by CSP"
  );
});

add_task(function test_sandboxContentSecurityPolicy_on_content_principal() {
  Assert.throws(
    () => {
      Cu.Sandbox("http://example.com", {
        sandboxContentSecurityPolicy: "script-src 'none';",
      });
    },
    /Error: sandboxContentSecurityPolicy is currently only supported with ExpandedPrincipals/,
    // Until bug 1548468 is fixed, CSP only works with an ExpandedPrincipal.
    "sandboxContentSecurityPolicy does not work with content principal"
  );
});

add_task(function test_sandboxContentSecurityPolicy_on_null_principal() {
  Assert.throws(
    () => {
      Cu.Sandbox(null, { sandboxContentSecurityPolicy: "script-src 'none';" });
    },
    /Error: sandboxContentSecurityPolicy is currently only supported with ExpandedPrincipals/,
    // Until bug 1548468 is fixed, CSP only works with an ExpandedPrincipal.
    "sandboxContentSecurityPolicy does not work with content principal"
  );
});

add_task(function test_sandboxContentSecurityPolicy_on_content_principal() {
  Assert.throws(
    () => {
      Cu.Sandbox("http://example.com", {
        sandboxContentSecurityPolicy: "script-src 'none';",
      });
    },
    /Error: sandboxContentSecurityPolicy is currently only supported with ExpandedPrincipals/,
    // Until bug 1548468 is fixed, CSP only works with an ExpandedPrincipal.
    "sandboxContentSecurityPolicy does not work with content principal"
  );
});

add_task(function test_sandboxContentSecurityPolicy_on_system_principal() {
  const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  // Note: if we ever introduce support for CSP in non-Expanded principals,
  // then the test should set security.allow_eval_with_system_principal=true
  // to make sure that eval() is blocked because of CSP and not another reason.
  Assert.throws(
    () => {
      Cu.Sandbox(systemPrincipal, {
        sandboxContentSecurityPolicy: "script-src 'none';",
      });
    },
    /Error: sandboxContentSecurityPolicy is currently only supported with ExpandedPrincipals/,
    // Until bug 1548468 is fixed, CSP only works with an ExpandedPrincipal.
    "sandboxContentSecurityPolicy does not work with system principal"
  );
});
