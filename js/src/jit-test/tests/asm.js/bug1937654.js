load(libdir + "asm.js");

let template = `
  'use asm';
  var imported = foreign.imported;
  function main() {
    imported(ARGS);
  }
  return main;
  `;
let args = new Array(100000).fill('0').join(', ');
assertErrorMessage(() => new Function('stdlib', 'foreign', template.replace('ARGS', args)),
  SyntaxError, /too many function arguments/);
