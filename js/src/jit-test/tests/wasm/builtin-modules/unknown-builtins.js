// Can import a builtin that's not recognized by the system, falling back to
// the imports object.
wasmEvalText(
  `(module (import "wasm:unknown" "unknown" (func)))`,
  { "wasm:unknown": { unknown: () => {} } },
  { builtins: ["unknown"] }
);
