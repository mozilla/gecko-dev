// |jit-test| --delazification-mode=concurrent-df+on-demand

function func() {}

// asm.js forces full-parse.
var asmModule = function() {
  'use asm';
  return {};
};

func();
relazifyFunctions();
func();
