this.a = 0;

function f(y) {
  // Direct eval to make an extensible environment. Variables lookups within
  // nested environments are now dynamic.
  eval("");

  let w = y;

  // Function with NamedLambdaObject environment whose shape guard we want to omit.
  let g = function lambda() {
    // Eval to introduce a NamedLambdaObject environment.
    eval("");

    // BindName "a"                # ENV                                                                                                                                                                                         
    // Dup                         # ENV ENV                                                                                                                                                                                     
    // GetBoundName "a"            # ENV ENV.a                                                                                                                                                                                   
    // GetName "w"                 # ENV ENV.a w                                                                                                                                                                                 
    // Add                         # ENV (ENV.a += w)                                                                                                                                                                            
    // NopIsAssignOp               # ENV (ENV.a += w)                                                                                                                                                                            
    // SetName "a"                 # (ENV.a += w)                                                                                                                                                                                
    // Pop                         #
    a += w;
  };

  for (var i = 0; i < 150; ++i) {
    // Introduce a new binding in the global lexical environment which
    // shadows the global property "a".
    if (i === 100) {
      evaluate("let a = 1000");
    }
    g();
  }

  assertEq(a, 1050);
  assertEq(globalThis.a, 100);
}
f(1);
