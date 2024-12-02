// |reftest| skip-if(!xulRuntime.shell)

source = `class A {
  // Ensure this name parses.
  #℘;
}`;

Function(source);

if (typeof reportCompare === 'function') reportCompare(0, 0);