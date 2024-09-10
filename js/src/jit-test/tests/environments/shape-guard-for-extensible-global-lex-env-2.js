this.a = 0;

function f(y) {
  // Direct eval to make an extensible environment. Variables lookups within
  // nested environments are now dynamic.
  eval("");

  let w = y;

  // Class with class-body lexical environment whose shape guard we want to omit.
  class C {
    // Add a private brand to generate a class-body lexical environment.
    #private;

    static m() {
      // BindName "a"                # ENV                                                                                                                                                                                         
      // Dup                         # ENV ENV                                                                                                                                                                                     
      // GetBoundName "a"            # ENV ENV.a                                                                                                                                                                                   
      // GetAliasedVar "w" (hops = 2, slot = 2) # ENV ENV.a w                                                                                                                                                                      
      // CheckAliasedLexical "w" (hops = 2, slot = 2) # ENV ENV.a w                                                                                                                                                                
      // Add                         # ENV (ENV.a += w)
      // NopIsAssignOp               # ENV (ENV.a += w)
      // StrictSetName "a"           # (ENV.a += w)
      // Pop                         #
      a += w;
    }
  }

  let g = C.m;

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
